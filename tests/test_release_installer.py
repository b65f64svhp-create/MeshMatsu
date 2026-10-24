"""Exercise release verification and failure paths without USB access."""
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import zlib

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('installer', ROOT / 'tools/install_release.py')
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)


class ReleaseInstallerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        (self.root / 'launcher').mkdir()
        (self.root / 'application.bin').write_bytes(b'new-app')
        for name in installer.ASSETS:
            (self.root / 'launcher' / name).write_bytes(
                b'{"version":"62"}' if name == 'metadata.json' else b'icon')
        self.manifest = {'schema': 1, 'slug': installer.SLUG, 'target': 'tanmatsu',
                         'version': 62, 'binary_bytes': 7,
                         'binary_crc32': f'0x{zlib.crc32(b"new-app"):08x}',
                         'files': {str(p.relative_to(self.root)): installer.digest(p)
                                   for p in self.root.rglob('*') if p.is_file()}}
        (self.root / 'manifest.json').write_text(json.dumps(self.manifest))
        self.commands = []
        self.uploaded = False
        self.fail = None
        self.old_version = 61

    def command(self, cmd, **kwargs):
        args = cmd[6:]
        self.commands.append(args)
        out = 'Negotiated protocol version 3 (server supports v3)\n'
        rc = 0
        if args == ['appfs', 'list']:
            if self.fail == 'connection':
                return subprocess.CompletedProcess(cmd, 1, 'Badge not found')
            out += f'slug | title | version | size\nmeshdeck-mui | MeshMatsu | {self.old_version} | 1K\n'
        elif args[:2] == ['appfs', 'stat']:
            version, data = (62, b'new-app') if self.uploaded else (self.old_version, b'old-app')
            out += f'Slug:    meshdeck-mui\nVersion: {version}\nSize:    {len(data)}\n'
        elif args[:2] == ['appfs', 'crc32']:
            data = b'new-app' if self.uploaded else b'old-app'
            out += f'0x{zlib.crc32(data):08x}\n' if self.fail != 'crc' or not self.uploaded else '0x00000000\n'
        elif args[:2] == ['appfs', 'download']:
            Path(args[-1]).write_bytes(b'bad' if self.fail == 'backup' else b'old-app')
        elif args[:2] == ['appfs', 'upload']:
            self.uploaded = True
        elif args[:2] == ['fs', 'list']:
            out += 'type | path\ndir | meshdeck-mui\n'
        elif args[:2] == ['fs', 'stat']:
            out += 'Type:     directory\n'
        elif args[:2] == ['fs', 'download']:
            data = (self.root / 'launcher' / Path(args[2]).name).read_bytes()
            Path(args[-1]).write_bytes(b'corrupt' if self.fail == 'icon' else data)
        return subprocess.CompletedProcess(cmd, rc, out)

    def install(self):
        with patch.object(installer.subprocess, 'run', self.command):
            installer.install(self.root, self.manifest, Path('badgelink.py'), Path('python'),
                              self.root / 'rollback', True)

    def test_verify_without_transport(self):
        with patch.object(installer.subprocess, 'run', side_effect=AssertionError('USB access')):
            self.assertEqual(installer.verify(self.root)['version'], 62)

    def test_corrupt_package(self):
        (self.root / 'application.bin').write_bytes(b'tampered')
        with self.assertRaisesRegex(ValueError, 'Checksum mismatch'):
            installer.verify(self.root)

    def test_virtualenv_interpreter_path_is_preserved(self):
        python = self.root / 'venv/bin/python'
        python.parent.mkdir(parents=True)
        python.symlink_to(sys.executable)
        cli = self.root / 'cli.py'
        cli.write_text('pass\n')
        argv = ['install.py', '--package', str(self.root), '--install',
                '--python', str(python), '--badgelink', str(cli)]
        with patch.object(sys, 'argv', argv), patch.object(installer, 'install') as run:
            self.assertEqual(installer.main(), 0)
            self.assertEqual(run.call_args.args[3], python)

    def test_path_traversal_and_symlinks(self):
        for name in ['../escape', '/absolute', 'launcher\\escape']:
            with self.assertRaises(ValueError):
                installer.package_file(self.root, name)
        (self.root / 'linked').symlink_to(self.root / 'application.bin')
        with self.assertRaises(ValueError):
            installer.package_file(self.root, 'linked')

    def test_success_with_verified_rollback(self):
        self.install()
        self.assertTrue(self.uploaded)
        self.assertEqual(self.commands[-1], ['start', 'meshdeck-mui'])
        saved = list((self.root / 'rollback').glob('*/application.bin'))
        self.assertEqual(len(saved), 1)
        self.assertEqual(saved[0].read_bytes(), b'old-app')

    def test_connection_failure_stops_before_write(self):
        self.fail = 'connection'
        with self.assertRaises(RuntimeError):
            self.install()
        self.assertFalse(self.uploaded)

    def test_downgrade_stops_before_write(self):
        self.old_version = 63
        with self.assertRaisesRegex(ValueError, 'downgrade'):
            self.install()
        self.assertFalse(self.uploaded)

    def test_bad_backup_stops_before_write(self):
        self.fail = 'backup'
        with self.assertRaisesRegex(ValueError, 'Rollback'):
            self.install()
        self.assertFalse(self.uploaded)

    def test_bad_app_crc_or_icon_prevents_start(self):
        for failure in ['crc', 'icon']:
            with self.subTest(failure=failure):
                self.commands = []
                self.uploaded = False
                self.fail = failure
                with self.assertRaises(ValueError):
                    self.install()
                self.assertFalse(any(c[0] == 'start' for c in self.commands))


if __name__ == '__main__':
    unittest.main()
