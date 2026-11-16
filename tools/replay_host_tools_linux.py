#!/usr/bin/env python3
"""Native Linux replay of an extracted host-tools release bundle (producer evidence; the CLI runs no Python).

Runs, on the real host and without the source SDK or any CRT checkout: bundle verification, the tools in a minimal bubblewrap root
that exposes only the OS loader and a copy of the bundle (space/Unicode path, empty environment), optionally the same smoke in a
second glibc root (--glibc-root, an extracted ubuntu-base), and optionally the CLI's own acceptance of the bundle together with
fail-closed controls (--cli): changed alias, extra/missing file, escaping symlink, lost execute bit, wrong pin, edited manifest.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).parent))
import create_host_tools as producer


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


SMOKE = [
    ('sh', ['-c', 'echo shell-ok; exit 3'], 3, 'shell-ok'),
    ('awk', ['BEGIN{print 6*7}'], 0, '42'),
    ('make', ['--version'], 0, 'GNU Make'),
    ('printf', ['%s-%s\\n', 'a', 'b'], 0, 'a-b'),
    ('echo', ['x'], 0, 'x'),
    ('cat', ['/proc/self/status'], 0, 'Name:'),
    ('ls', ['/'], 0, 'proc'),
    ('sort', ['/proc/self/status'], 0, 'Name:'),
    ('wc', ['-c', '/proc/self/status'], 0, ''),
    ('expr', ['6', '*', '7'], 0, '42'),
    ('basename', ['/a/b/c.txt'], 0, 'c.txt'),
    ('uname', ['-s'], 0, 'Linux'),
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundle', type=Path, required=True, help='extracted bundle directory')
    parser.add_argument('--output', type=Path, required=True, help='new evidence directory')
    parser.add_argument('--glibc-root', type=Path, help='extracted ubuntu-base root (an older glibc) for the loader smoke')
    parser.add_argument('--cli', type=Path, help='crt-cli executable that accepts the bundle')
    args = parser.parse_args()
    out = args.output.resolve(); out.mkdir(parents=True)
    bundle = args.bundle.resolve()
    manifest = producer.verify_bundle(bundle)
    pin = sha(bundle / 'manifest.json')
    results = []

    def record(label, passed, detail=''):
        results.append({'case': label, 'passed': bool(passed), 'detail': str(detail)[:600]})
        print(('PASS ' if passed else 'FAIL ') + label + ('' if passed or not detail else ' -- ' + str(detail)[:300]))

    record('bundle inventory, executables and native images verify', True)
    record('manifest names a preview bundle with a clean or declared assembler tree', manifest['promotion'] in ('preview', 'local-only'),
           manifest['promotion'])

    copy = out / 'space 한글 bundle'
    shutil.copytree(bundle, copy, symlinks=True)
    bwrap = shutil.which('bwrap')
    if bwrap:
        def sandbox(root_args, tool, argv):
            cmd = [bwrap, '--unshare-all', '--die-with-parent', '--new-session', *root_args, '--ro-bind', str(copy), str(copy),
                   '--proc', '/proc', '--dev', '/dev', '--tmpfs', '/tmp', '--clearenv', '--setenv', 'PATH', '/nonexistent',
                   str(copy / 'system/bin' / tool), *argv]
            return subprocess.run(cmd, capture_output=True, text=True, timeout=60, stdin=subprocess.DEVNULL)

        minimal = ['--ro-bind', '/usr', '/usr', '--symlink', 'usr/lib', '/lib', '--symlink', 'usr/lib64', '/lib64']
        roots = [('minimal root (only /usr loader + bundle copy)', minimal)]
        if args.glibc_root:
            roots.append(('glibc root ' + args.glibc_root.parent.name + '/' + args.glibc_root.name, ['--overlay-src', str(args.glibc_root.resolve()), '--tmp-overlay', '/']))
        for label, root_args in roots:
            for tool, argv, code, needle in SMOKE:
                p = sandbox(root_args, tool, argv)
                record('{}: {}'.format(label, tool), p.returncode == code and needle in p.stdout, p.stdout + p.stderr)
            missing = [name for name in manifest['executable_files'] if not (copy / name).exists()]
            record(label + ': every declared alias exists', not missing, missing[:3])
            failures = []
            for relative in sorted(manifest['executable_files']):
                alias = Path(relative).name
                if relative.startswith('system/bin/') or relative.startswith('bin/') or relative.startswith('usr/bin/'):
                    cmd = [bwrap, '--unshare-all', '--die-with-parent', *root_args, '--ro-bind', str(copy), str(copy), '--proc', '/proc',
                           '--dev', '/dev', '--tmpfs', '/tmp', '--clearenv', '--setenv', 'PATH', '/nonexistent', str(copy / relative),
                           '--help']
                    p = subprocess.run(cmd, capture_output=True, text=True, timeout=60, stdin=subprocess.DEVNULL)
                    if p.returncode in (126, 127) or p.returncode < 0:
                        failures.append((relative, p.returncode, p.stderr[:80]))
            record(label + ': every executable alias starts (no loader failure or signal)', not failures, failures[:3])
    else:
        record('bubblewrap available', False, 'install bubblewrap to run the isolated smoke')

    if args.cli:
        cli = args.cli.resolve()
        home = out / 'cli-home'; home.mkdir(); tmp = out / 'cli-tmp'; tmp.mkdir()
        env = {'HOME': str(home), 'PATH': '/nonexistent', 'TMPDIR': str(tmp)}
        jobs = [0]

        def cli_run(root, pin_value, script='echo cli-ok'):
            jobs[0] += 1
            (out / 'probe.sh').write_text(script + '\n')
            return subprocess.run([str(cli), '--json', 'shell', '--host-tools', str(root), '--host-tools-sha256', pin_value,
                                   '--work-root', str(out / ('job%d' % jobs[0])), '--script', str(out / 'probe.sh')],
                                  capture_output=True, text=True, env=env, timeout=120, stdin=subprocess.DEVNULL)

        p = cli_run(copy, pin, 'echo cli-ok; awk "BEGIN{print 1+1}"; make --version >/dev/null')
        record('CLI accepts the bundle and runs shell/awk/make', p.returncode == 0 and 'cli-ok' in p.stdout, p.stdout[-300:] + p.stderr[-300:])
        record('CLI reports the manifest promotion unchanged', json.loads(p.stdout.splitlines()[-1])['data']['host_tools_identity']['promotion'] == manifest['promotion']
               if p.returncode == 0 else False)

        def mutated(label, change, pin_value=None, repin=False):
            target = out / ('mutant-' + label.replace(' ', '-'))
            shutil.copytree(bundle, target, symlinks=True)
            change(target)
            p = cli_run(target, sha(target / 'manifest.json') if repin else (pin_value or pin))
            record('CLI rejects ' + label, p.returncode != 0 and 'cli-ok' not in p.stdout, (p.stdout + p.stderr)[-200:])

        def edit_manifest(target, **fields):
            m = json.loads((target / 'manifest.json').read_text()); m.update(fields)
            (target / 'manifest.json').write_text(json.dumps(m, ensure_ascii=False, indent=2) + '\n')

        mutated('a changed alias', lambda t: (t / 'system/bin/printf').write_bytes(b'\x7fELF changed'))
        mutated('an extra file', lambda t: (t / 'system/bin/extra').write_text('x'))
        mutated('a missing alias', lambda t: (t / 'system/bin/printf').unlink())
        mutated('an escaping symlink', lambda t: ((t / 'usr/bin/sh').unlink(), (t / 'usr/bin/sh').symlink_to('../../../../../etc/passwd')))
        mutated('a lost execute bit', lambda t: os.chmod(t / 'system/bin/make', 0o644))
        mutated('a wrong pin', lambda t: None, '0' * 64)
        mutated('an unknown promotion with its own pin', lambda t: edit_manifest(t, promotion='official'), repin=True)
        wrong_host = dict(manifest['host'], arch='aarch64' if manifest['host']['arch'] == 'x86_64' else 'x86_64')
        mutated('a wrong host with its own pin', lambda t: edit_manifest(t, host=wrong_host), repin=True)

    ok = all(r['passed'] for r in results)
    (out / 'host-tools-replay.json').write_text(json.dumps({'passed': ok, 'bundle_manifest_sha256': pin, 'cases': results}, indent=2, ensure_ascii=False) + '\n')
    print('{}/{} cases passed'.format(sum(r['passed'] for r in results), len(results)))
    return 0 if ok else 1


if __name__ == '__main__':
    raise SystemExit(main())
