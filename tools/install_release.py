#!/usr/bin/env python3
"""Verify or install an extracted MeshMatsu app-only release using BadgeLink USB."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
import tempfile
import zlib


ASSETS = ('icon16.png', 'icon32.png', 'icon64.png', 'metadata.json')
SLUG = 'meshdeck-mui'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def package_file(folder, name):
    parts = PurePosixPath(name)
    if not name or parts.is_absolute() or '..' in parts.parts or '\\' in name:
        raise ValueError('Invalid package path: ' + name)
    p = folder / name
    if not p.resolve().is_relative_to(folder) or any(x.is_symlink() for x in [p, *p.parents]):
        raise ValueError('Package links are not supported: ' + name)
    if not p.is_file():
        raise ValueError('Missing package file: ' + name)
    return p


def verify(folder):
    manifest = json.loads(package_file(folder, 'manifest.json').read_text())
    if manifest.get('schema') != 1 or manifest.get('slug') != SLUG or manifest.get('target') != 'tanmatsu':
        raise ValueError('Unsupported release manifest')
    if type(manifest.get('version')) is not int or manifest['version'] < 1:
        raise ValueError('Invalid release version')
    files = manifest['files']
    required = {'application.bin', *(f'launcher/{n}' for n in ASSETS)}
    if not required.issubset(files):
        raise ValueError('Incomplete release manifest')
    for name, expected in files.items():
        if digest(package_file(folder, name)) != expected:
            raise ValueError('Checksum mismatch: ' + name)
    binary = (folder / 'application.bin').read_bytes()
    if len(binary) != manifest['binary_bytes'] or f'0x{zlib.crc32(binary):08x}' != manifest['binary_crc32']:
        raise ValueError('App size/CRC32 mismatch')
    metadata = json.loads((folder / 'launcher/metadata.json').read_text())
    if str(metadata['version']) != str(manifest['version']):
        raise ValueError('Launcher version does not match app')
    return manifest


def stat_fields(text):
    values = dict(re.findall(r'^(Slug|Version|Size):\s*(\S+)\s*$', text, re.MULTILINE))
    if values.get('Slug') != SLUG or not values.get('Version', '').isdigit() or not values.get('Size', '').isdigit():
        raise ValueError('Unrecognized installed app metadata')
    return int(values['Version']), int(values['Size'])


def install(folder, manifest, cli, python, backup_dir, start):
    base = [str(python), str(cli), '--timeout', '15', '--xfer-timeout', '60']

    def run(*args, check=True):
        result = subprocess.run(base + list(args), text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=180)
        if check and result.returncode:
            raise RuntimeError(result.stdout.strip() or 'BadgeLink command failed')
        return result

    # Listing distinguishes a new installation from transport/protocol failures.
    listing = run('appfs', 'list').stdout
    exists = any(line.split() and line.split()[0] == SLUG for line in listing.splitlines())
    saved = None
    if exists:
        version, size = stat_fields(run('appfs', 'stat', SLUG).stdout)
        if version > manifest['version']:
            raise ValueError('A newer MeshMatsu is installed; refusing an automatic downgrade')
        crc = run('appfs', 'crc32', SLUG).stdout
        match = re.search(r'^0x([0-9a-fA-F]{8})\s*$', crc, re.MULTILINE)
        if not match:
            raise ValueError('Could not read existing app checksum')
        backup_dir.mkdir(parents=True, exist_ok=True)
        saved = Path(tempfile.mkdtemp(prefix=f'MeshMatsu-v{version}-', dir=backup_dir))
        run('appfs', 'download', SLUG, str(saved / 'application.bin'))
        old = (saved / 'application.bin').read_bytes()
        if len(old) != size or zlib.crc32(old) != int(match[1], 16):
            raise ValueError('Rollback app verification failed; installation stopped')
        old_manifest = {'slug': SLUG, 'version': version, 'binary_bytes': size,
                        'binary_crc32': f'0x{zlib.crc32(old):08x}',
                        'binary_sha256': hashlib.sha256(old).hexdigest()}
        (saved / 'rollback.json').write_text(json.dumps(old_manifest, indent=2) + '\n')
        print('Verified previous app saved in ' + str(saved), flush=True)

    destination = '/int/apps/' + SLUG
    # Check the parent before any app write. Never treat a USB error as absence.
    parent = run('fs', 'list', '/int/apps').stdout
    asset_dir_exists = any(line.split() and line.split()[-1].rstrip('/').split('/')[-1] == SLUG
                           for line in parent.splitlines())
    if asset_dir_exists and not re.search(r'^Type:\s+directory\s*$', run('fs', 'stat', destination).stdout, re.MULTILINE):
        raise ValueError('Launcher destination is not a directory')
    if saved is not None and asset_dir_exists:
        old_assets = saved / 'launcher'
        old_assets.mkdir()
        for name in ASSETS:
            run('fs', 'download', destination + '/' + name, str(old_assets / name))
        old_manifest['launcher_assets'] = {name: digest(old_assets / name) for name in ASSETS}
        (saved / 'rollback.json').write_text(json.dumps(old_manifest, indent=2) + '\n')
    print('Uploading MeshMatsu app…', flush=True)
    run('appfs', 'upload', SLUG, 'MeshMatsu', str(manifest['version']), str(folder / 'application.bin'))
    version, size = stat_fields(run('appfs', 'stat', SLUG).stdout)
    if version != manifest['version'] or size != manifest['binary_bytes']:
        raise ValueError('Installed app version/size mismatch; app was not started')
    crc = run('appfs', 'crc32', SLUG).stdout
    if not re.search(r'^' + re.escape(manifest['binary_crc32']) + r'\s*$', crc, re.MULTILINE):
        raise ValueError('Installed checksum mismatch; app was not started')
    if not asset_dir_exists:
        run('fs', 'mkdir', destination)
    with tempfile.TemporaryDirectory(prefix='meshmatsu-launcher-check-') as tmp:
        for name in ASSETS:
            run('fs', 'upload', destination + '/' + name, str(folder / 'launcher' / name))
            downloaded = Path(tmp) / name
            run('fs', 'download', destination + '/' + name, str(downloaded))
            if digest(downloaded) != manifest['files']['launcher/' + name]:
                raise ValueError('Launcher verification failed: ' + name + '; app was not started')
    print('Installed app and launcher files verified.', flush=True)
    if start:
        run('start', SLUG)
        print('MeshMatsu start request acknowledged.', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument('--install', action='store_true', help='upload after local verification; default only verifies files')
    parser.add_argument('--badgelink', type=Path, help='BadgeLink CLI; defaults to bundled tools')
    parser.add_argument('--python', type=Path, default=Path(sys.executable), help='Python with BadgeLink dependencies installed')
    parser.add_argument('--backup-dir', type=Path, help='where to retain the previous app binary')
    parser.add_argument('--no-start', action='store_true')
    args = parser.parse_args()
    try:
        folder = args.package.resolve()
        manifest = verify(folder)
        print(f"Package v0.{manifest['version']}.beta verified locally.", flush=True)
        if args.install:
            cli = args.badgelink.resolve() if args.badgelink else package_file(folder, 'badgelink/badgelink.py')
            if not cli.is_file():
                raise ValueError('BadgeLink CLI not found')
            backup = (args.backup_dir or folder.parent / 'MeshMatsu-rollback').resolve()
            # Keep the venv entry-point path: resolving its symlink loses pyvenv.cfg.
            install(folder, manifest, cli, args.python.absolute(), backup, not args.no_start)
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.TimeoutExpired) as error:
        print('Stopped: ' + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
