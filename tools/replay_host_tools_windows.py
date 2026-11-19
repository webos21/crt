#!/usr/bin/env python3
"""Native Windows replay of an extracted host-tools release bundle (producer evidence; the CLI runs no Python).

Runs on the real host and without the source SDK or any CRT checkout: bundle verification, then every tool from a copy of the bundle in
a path with a space and Hangul, with an almost empty environment (no PATH, only the system root every Windows process needs) and a
private scratch directory as the working directory. Windows has no unprivileged sandbox here, so isolation is the empty environment and
the relocated copy; no network or file-system denial is claimed.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).parent))
import create_host_tools as producer
import host_tools_replay_common as common


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundle', type=Path, required=True, help='extracted bundle directory')
    parser.add_argument('--output', type=Path, required=True, help='new evidence directory')
    args = parser.parse_args()
    if os.name != 'nt':
        raise SystemExit('this replay runs on Windows')
    out = args.output.resolve(); out.mkdir(parents=True)
    bundle = args.bundle.resolve()
    manifest = producer.verify_bundle(bundle)
    pin = common.sha(bundle / 'manifest.json')
    results = []

    def record(label, passed, detail=''):
        results.append({'case': label, 'passed': bool(passed), 'detail': str(detail)[:600]})
        print(('PASS ' if passed else 'FAIL ') + label + ('' if passed or not detail else ' -- ' + str(detail)[:300]))

    record('bundle inventory, executables and native images verify', True)
    record('manifest names a preview or local bundle', manifest['promotion'] in ('preview', 'local-only'), manifest['promotion'])
    record('the bundle is for this host', manifest['host'] == producer.native_host(), manifest['host'])

    copy = out / 'space 한글 bundle'
    shutil.copytree(bundle, copy, symlinks=True)
    scratch = Path(tempfile.mkdtemp(prefix='host-tools-scratch-', dir=out)).resolve()
    environment = {'SystemRoot': os.environ.get('SystemRoot', r'C:\Windows'), 'PATH': '', 'TEMP': str(scratch), 'TMP': str(scratch)}

    def launch(tool, argv):
        proc = subprocess.run([str(tool), *argv], capture_output=True, text=True, timeout=60, stdin=subprocess.DEVNULL,
                              env=environment, cwd=scratch, encoding='utf-8', errors='replace')
        return proc.returncode, proc.stdout + proc.stderr

    common.run_cases(copy, manifest, launch, record, suffix='.exe')
    return common.write_result(out, pin, results)


if __name__ == '__main__':
    raise SystemExit(main())
