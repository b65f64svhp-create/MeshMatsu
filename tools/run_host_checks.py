#!/usr/bin/env python3
"""Run MeshMatsu host regression checks from any working directory."""
from pathlib import Path
import argparse
import os
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
C_EXTRAS = {
    'test_backlight.c': (
        ['-DCONFIG_BSP_TARGET_TANMATSU'], ['backlight.c', 'backlight_curve.c']),
    'test_config_local.c': (
        [], ['config_local.c', 'display_settings.c', 'backlight_curve.c', 'notification_time.c']),
}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--match', default='test_*', help='test filename glob')
    args = parser.parse_args()
    env = os.environ.copy()
    if sys.platform == 'darwin' and Path('/Library/Developer/CommandLineTools').exists():
        env.setdefault('DEVELOPER_DIR', '/Library/Developer/CommandLineTools')
    passed = {'Python': 0, 'C': 0}
    failed = []
    tests = sorted(p for p in (ROOT / 'tests').glob(args.match)
                   if p.suffix in ('.py', '.c') and p.name != 'test_key_backup_crypto.py')
    if not tests:
        parser.error('no matching host checks')
    for test in tests:
        with tempfile.TemporaryDirectory(prefix='meshmatsu-check-') as folder:
            if test.suffix == '.py':
                commands = [[sys.executable, str(test)]]
                family = 'Python'
            else:
                flags, sources = C_EXTRAS.get(test.name, ([], []))
                executable = str(Path(folder) / 'test')
                commands = [[
                    'clang', '-std=c11', '-fsanitize=address,undefined',
                    '-I', str(ROOT / 'tests/stubs'), '-I', str(ROOT / 'main'),
                    *flags, str(test), *[str(ROOT / 'main' / name) for name in sources],
                    '-lm', '-o', executable], [executable]]
                family = 'C'
            output = []
            for command in commands:
                result = subprocess.run(command, cwd=folder, env=env, text=True,
                                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                output.append(result.stdout)
                if result.returncode:
                    break
            ok = result.returncode == 0
            print(f'{test.name} {"PASS" if ok else "FAIL"}', flush=True)
            if ok:
                passed[family] += 1
            else:
                failed.append(test.name)
            # Preserve diagnostics, including warnings from a successful compiler.
            combined = ''.join(output)
            if not ok or 'warning:' in combined:
                print(combined, flush=True)
    print(f'PASSED: {sum(passed.values())} ({passed["Python"]} Python, {passed["C"]} C) '
          f'FAILED: {failed}', flush=True)
    print('SDK-specific crypto harness is separate (test_key_backup_crypto.py).', flush=True)
    return bool(failed)

if __name__ == '__main__':
    raise SystemExit(main())
