#!/usr/bin/env python3
"""Native Windows D1 replay. Python/OpenSSL are test-host tools, never product inputs.

Restricted tokens deny original repositories/SDK/provider access. Job objects
account for launched images; this is not executable denial or a hostile sandbox.
Only newly created fixture directories receive private/read-only ACLs.
"""
import argparse
import ctypes
from ctypes import wintypes as w
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

import crt_windows_external as sandbox
import crt_windows_trace as trace
from create_host_tools import verify_bundle


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def readonly_fixture(path, sid):
    """Protect a fixture created by this run, preserving orchestrator mutation."""
    kernel, advapi = sandbox.api()
    descriptor, dacl = ctypes.c_void_p(), ctypes.c_void_p()
    sddl = 'D:P' + ''.join(f'(A;OICI;FA;;;{i})' for i in (sandbox.caller_sid(kernel, advapi), 'SY', 'BA'))
    sddl += f'(A;OICI;GRGX;;;{sid})'
    sandbox.checked(advapi.ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, 1, ctypes.byref(descriptor), None))
    try:
        present, defaulted = w.BOOL(), w.BOOL()
        sandbox.checked(advapi.GetSecurityDescriptorDacl(descriptor, ctypes.byref(present), ctypes.byref(dacl), ctypes.byref(defaulted)))
        error = advapi.SetNamedSecurityInfoW(str(path), 1, 4 | 0x80000000, None, None, dacl, None)
        if error:
            raise ctypes.WinError(error)
    finally:
        kernel.LocalFree(descriptor)


def allowed_image(image, permitted):
    if not image:
        return False
    path = Path(image).resolve()
    return path in permitted or path == Path(os.environ['SystemRoot'], 'System32/conhost.exe').resolve()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('cli', 'host-tools', 'resources', 'output', 'probe'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--work', type=Path)
    parser.add_argument('--hide', type=Path, action='append', required=True)
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('requires native Windows')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    work, sid = sandbox.create_work(args.work)
    tools, package = work / 'tools space 한글', work / 'bootstrap space 한글'
    shutil.copytree(args.host_tools, tools, symlinks=True)
    shutil.copytree(args.resources, package, symlinks=True)
    cli, probe = work / 'crt-cli.exe', work / 'probe.exe'
    shutil.copy2(args.cli, cli)
    shutil.copy2(args.probe, probe)
    assert digest(cli) == digest(package / 'crt-cli.exe'), 'package must contain the selected CLI'
    hidden = sorted({p.resolve(strict=True) for p in args.hide})
    assert hidden and all(not work.is_relative_to(p) for p in hidden), 'work must be outside hidden inputs'
    sources = work / 'scripts space 한글'
    sources.mkdir()
    (work / 'home').mkdir()
    before = (tools / 'manifest.json').read_bytes()
    verify_bundle(tools)
    manifest = json.loads(before)
    assert manifest['host'] == {'os': 'windows', 'arch': 'x86_64'}
    permitted = {p.resolve() for p in [cli, package / 'crt-cli.exe', probe, *[tools / p for p in manifest['executable_files']]]}
    readonly_fixture(tools, sid)
    readonly_fixture(package, sid)
    environment = {'HOME': str(work / 'home'), 'USERPROFILE': str(work / 'home'), 'PATH': 'C:/hostile/sentinel',
                   'SystemRoot': os.environ['SystemRoot'], 'LC_ALL': 'C', 'TEMP': str(work), 'TMP': str(work),
                   'CPATH': 'secret-poison', 'CRT_CLI_TEST_SECRET': 'secret-poison',
                   'MAKEFLAGS': 'SHELL=/hostile/sentinel', 'CRT_ROOTFS': '/hostile/rootfs'}
    commands, results = [], []

    def save(completed=False):
        (output / 'commands.json').write_text(json.dumps(commands, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        receipt = {'pass': completed and all(r['pass'] for r in results), 'completed': completed,
                   'cases': results, 'work': str(work), 'cli_sha256': digest(cli),
                   'host_tools_manifest_sha256': digest(tools / 'manifest.json'),
                   'resources_manifest_sha256': digest(package / 'manifest.json'), 'hidden': list(map(str, hidden)),
                   'permitted_executables': sorted(map(str, permitted)), 'publisher': 'disposable explicit test key',
                   'isolation': 'Windows restricted token and job process accounting; not executable denial'}
        (output / 'result.json').write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

    def launch(label, argv, expected=0):
        out, err = output / (label + '.stdout'), output / (label + '.stderr')
        result = trace.run(list(map(str, argv)), work=work, sid=sid, env=environment, cwd=work,
                           stdout=out, stderr=err, timeout=90)
        stray = [p for p in result['processes'] if not allowed_image(p['image'], permitted)]
        commands.append({'label': label, 'argv': list(map(str, argv)), 'cwd': str(work), **result})
        assert not stray, f'{label}: undeclared process images: {stray[:3]}'
        assert not result['timed_out'], f'{label}: outer fixture timeout'
        return result['exit_code'], out.read_bytes()

    boundary = [probe]
    for path in hidden:
        boundary += ['--read', path]
    status, _ = launch('boundary-probe', boundary)
    results.append({'label': 'boundary-probe', 'pass': status == 0})
    save()
    assert status == 0, 'hidden source/SDK read probe failed'

    def run(label, tail, expected=0, check=None, binary=cli):
        status, stdout = launch(label, [binary, '--json', *tail])
        reply = json.loads(stdout)
        okay = status == expected if expected is not None else status != 0
        if check:
            okay = okay and bool(check(reply))
        summary = json.loads(json.dumps(reply))
        if 'stdout' in summary.get('data', {}):
            text = summary['data'].pop('stdout')
            summary['data']['stdout_bytes'] = len(text.encode('utf-8'))
            summary['data']['stdout_sha256'] = hashlib.sha256(text.encode('utf-8')).hexdigest()
        results.append({'label': label, 'pass': okay, 'exit': status, 'reply': summary})
        print(('PASS ' if okay else 'FAIL ') + label, flush=True)
        save()
        return reply

    selection = ['--host-tools', tools, '--host-tools-sha256', digest(tools / 'manifest.json')]
    def shell(label, text, expected=0, timeout=20, check=None):
        script = sources / (label + '.sh')
        script.write_text(text, encoding='utf-8')
        return run(label, ['shell', *selection, '--work-root', work / ('job space 한글 ' + label),
                           '--script', script, '--directory', sources, '--timeout', str(timeout)], expected, check)

    run('self-doctor', ['doctor'], check=lambda r: r['data']['scope'] == 'self')
    run('environment-doctor', ['doctor', '--scope', 'environment', *selection])
    diagnostic = work / 'diagnostic only'
    report = run('environment-report', ['env', '--profile', 'port-build', *selection, '--work-root', diagnostic],
        check=lambda r: 'CRT_SYSROOT' not in r['data']['fixed'] and not r['data']['interactive'] and not diagnostic.exists())
    # std::filesystem preserves the manifest's forward slash in "system/bin"
    # after a native root. Validate the exact selected directory independently,
    # then compare the child's PATH byte spelling with the declared environment.
    tools_path = report['data']['fixed']['PATH']
    assert Path(tools_path).resolve() == (tools / 'system/bin').resolve()
    shell('immutable-payload-write-denied',
          f"if printf changed > '{(tools / 'unexpected-write').as_posix()}'; then exit 91; fi\n"
          f"if printf changed > '{(package / 'unexpected-write').as_posix()}'; then exit 92; fi\nexit 0\n")
    shell('tools-workflow', '''set -eu
test "${CRT_CLI_TEST_SECRET-unset}" = unset
test "${CPATH-unset}" = unset
test "${CRT_SYSROOT-unset}" = unset
test "${DYLD_LIBRARY_PATH-unset}" = unset
test "${DYLD_FALLBACK_LIBRARY_PATH-unset}" = unset
test "$SHELL" = /system/bin/sh.exe
test "$PATH" = '@TOOLS_PATH@'
test "$PWD" = '@SOURCE_PATH@'
printf '%s\\n' selected-tools | grep selected-tools
value=$(printf '20 22\\n' | awk '{print $1+$2}')
test "$value" = 42
cat <<'MAKEFILE' > Makefile
all:
\t@test "$(SHELL)" = /system/bin/sh.exe
\t@printf '%s\\n' crt-make-shell
\t@$(MAKE) -f nested.mk
MAKEFILE
cat <<'MAKEFILE' > nested.mk
all:
\t@printf '%s\\n' recursive-make-ok
MAKEFILE
make -f Makefile
printf '%s\\n' alias-test > input.txt
cp input.txt copied.txt
cmp input.txt copied.txt
rm copied.txt
printf '%s\\n' tools-ok
'''.replace('@TOOLS_PATH@', tools_path).replace('@SOURCE_PATH@', sources.as_posix()),
          check=lambda r: 'recursive-make-ok' in r['data']['stdout'] and 'tools-ok' in r['data']['stdout'])
    for label, ending, expected in [('normal-cleanup', 'exit 0', 0), ('timeout-cleanup', 'wait', 124)]:
        marker, started = sources / (label + '.marker'), sources / (label + '.started')
        shell(label, f"set -eu\n(printf started > '{started.as_posix()}'; sleep 3; printf escaped > '{marker.as_posix()}') &\n"
              f"while test ! -f '{started.as_posix()}'; do sleep 0.1; done\n{ending}\n", expected, 1 if expected else 20)
        time.sleep(4)
        results.append({'label': label + '-marker', 'pass': started.exists() and not marker.exists()})
    shell('child-exit', 'exit 7\n', 7)
    shell('stdout-limit', 'dd if=/dev/zero bs=65536 count=160\n', timeout=60,
          check=lambda r: r['data']['output_truncated'] and len(r['data']['stdout']) == 8 * 1024 * 1024)
    run('job-reuse-rejected', ['shell', *selection, '--work-root', work / 'job space 한글 child-exit',
                             '--script', sources / 'child-exit.sh'], expected=None)
    run('wrong-pin', ['doctor', '--scope', 'environment', '--host-tools', tools,
                      '--host-tools-sha256', '0' * 64], expected=None)
    link = work / 'tools-parent-link'
    link.symlink_to(tools, target_is_directory=True)
    run('linked-immutable-parent', ['env', '--profile', 'port-build', *selection, '--work-root', link / 'unsafe'], expected=None)
    victim = tools / 'system/bin/awk.exe'
    original = victim.read_bytes()
    victim.write_bytes(original + b'pollution')
    run('modified-tool', ['doctor', '--scope', 'environment', *selection], expected=None)
    victim.write_bytes(original)
    extra = tools / 'extra'
    extra.write_text('unexpected')
    run('unexpected-payload', ['doctor', '--scope', 'environment', *selection], expected=None)
    extra.unlink()
    missing, saved = tools / 'system/bin/make.exe', work / 'saved-make'
    missing.rename(saved)
    run('missing-make-no-fallback', ['doctor', '--scope', 'environment', *selection], expected=None)
    saved.rename(missing)
    changed = json.loads(before)
    changed['host']['arch'] = 'aarch64'
    (tools / 'manifest.json').write_text(json.dumps(changed))
    run('wrong-execution-host', ['doctor', '--scope', 'environment', '--host-tools', tools,
                               '--host-tools-sha256', digest(tools / 'manifest.json')], expected=None)
    (tools / 'manifest.json').write_bytes(before)
    openssl = shutil.which('openssl')
    assert openssl, 'OpenSSL is a test-host prerequisite'
    def crypto(*args):
        subprocess.run([openssl, *map(str, args)], cwd=work, check=True, capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW)
    crypto('genpkey', '-algorithm', 'RSA', '-pkeyopt', 'rsa_keygen_bits:2048', '-out', 'test.key')
    crypto('pkey', '-in', 'test.key', '-pubout', '-out', 'test.pub')
    crypto('dgst', '-sha256', '-sign', 'test.key', '-sigopt', 'rsa_padding_mode:pss',
           '-sigopt', 'rsa_pss_saltlen:32', '-out', 'resources.sig', package / 'manifest.json')
    resources = ['doctor', '--scope', 'self', '--resources', package, '--resources-sha256', digest(package / 'manifest.json')]
    run('resource-pin', resources, binary=package / 'crt-cli.exe', check=lambda r: r['data']['checks']['resources'] == 'pass')
    signed = [*resources, '--publisher-key', work / 'test.pub', '--signature', work / 'resources.sig']
    run('resource-signature', signed, binary=package / 'crt-cli.exe',
        check=lambda r: r['data']['resources']['publisher_authentication'] == 'signature_verified')
    signature = work / 'resources.sig'
    old_signature = signature.read_bytes()
    signature.write_bytes(b'\0' * 256)
    run('resource-signature-invalid', signed, expected=None)
    signature.write_bytes(old_signature)
    crypto('genpkey', '-algorithm', 'RSA', '-pkeyopt', 'rsa_keygen_bits:2048', '-out', 'other.key')
    crypto('pkey', '-in', 'other.key', '-pubout', '-out', 'other.pub')
    run('resource-wrong-key', [*resources, '--publisher-key', work / 'other.pub', '--signature', signature], expected=None)
    run('resource-missing-signature', [*resources, '--publisher-key', work / 'test.pub'], expected=None)
    resource_manifest = package / 'manifest.json'
    old_manifest = resource_manifest.read_bytes()
    for label, field, value in [('resource-wrong-host', 'target', {'os': 'linux', 'arch': 'x86_64'}),
                                ('resource-unknown-schema', 'format', 99)]:
        data = json.loads(old_manifest)
        data[field] = value
        resource_manifest.write_text(json.dumps(data))
        run(label, ['doctor', '--resources', package, '--resources-sha256', digest(resource_manifest)], expected=None)
    resource_manifest.write_bytes(old_manifest)
    for label, path in [('notice', package / 'LICENSE.md'), ('binary', package / 'crt-cli.exe'), ('ca', package / 'share/trust/ca.pem')]:
        original = path.read_bytes()
        path.write_bytes(original + b'changed')
        run('resource-tampered-' + label, resources, expected=None)
        path.write_bytes(original)
        if label == 'notice':
            path.unlink()
            run('resource-missing-notice', resources, expected=None)
            path.write_bytes(original)
    run('resources-restored', resources, binary=package / 'crt-cli.exe',
        check=lambda r: r['data']['checks']['resources'] == 'pass')
    verify_bundle(tools)
    results.append({'label': 'immutable-payload', 'pass': (tools / 'manifest.json').read_bytes() == before})
    assert not (tools / 'unexpected-write').exists() and not (package / 'unexpected-write').exists()
    for job in work.glob('job space 한글 *'):
        for name in ('system', 'bin', 'usr'):
            link = job / 'rootfs' / name
            if link.exists():
                assert link.is_symlink() and link.resolve() == (tools / name).resolve(), 'real directory symlink required'
    save(completed=True)
    print('PASS' if all(r['pass'] for r in results) else 'FAILED', len(results), 'cases', work, flush=True)
    raise SystemExit(0 if all(r['pass'] for r in results) else 1)


if __name__ == '__main__':
    main()
