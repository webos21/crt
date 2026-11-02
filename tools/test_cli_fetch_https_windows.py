#!/usr/bin/env python3
"""Run tools/test_cli_fetch_https.py with the CLI confined like the native Windows replay; never a CLI dependency.

The case definitions are the unchanged ones of test_cli_fetch_https.py. This adapter only changes how the CLI is
launched: every `crt-cli fetch` call goes through crt_windows_trace.run, i.e. a restricted token naming a per-run SID,
an empty PATH, redirected stdio and a job object that records every process the call starts. The work directory
is private to that SID, so the producer/SDK/source trees (which live under the user profile and are not readable
through it) are inaccessible. A native visibility probe, built with the SDK under test, proves that for the roots named
with --hidden, under the same SID. Python and OpenSSL only generate the throw-away identities and serve loopback
HTTPS for the fixture; the product fetch starts neither. Process records are accounting, not launch denial.

Usage: test_cli_fetch_https_windows.py --cli <crt-cli.exe> --output <fresh dir> --work <fresh dir that does not exist>
         --probe <probe.exe built with the SDK> --hidden <root> [--hidden <root> ...] [--publisher-cases]
Run with PYTHONUTF8=1 when any path contains non-ASCII characters.
"""
import argparse
import json
import os
import runpy
import shutil
import subprocess
import sys
import types
from pathlib import Path

import crt_windows_external as sandbox
import crt_windows_trace as trace

TOOLS = Path(__file__).resolve().parent
FORBIDDEN = ('cmd.exe', 'powershell.exe', 'pwsh.exe', 'python.exe', 'python3.exe', 'py.exe', 'openssl.exe', 'sh.exe',
             'bash.exe', 'mksh.exe', 'perl.exe', 'clang.exe', 'clang++.exe', 'ld.lld.exe', 'cmake.exe', 'ninja.exe')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--cli', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True, help='fresh directory; it gets an ACL naming only this run')
    parser.add_argument('--probe', type=Path, required=True, help='SDK-built executable that must fail to read every --hidden root')
    parser.add_argument('--hidden', type=Path, action='append', default=[], required=True)
    parser.add_argument('--publisher-cases', action='store_true')
    arguments = parser.parse_args()
    if os.name != 'nt':
        parser.error('this adapter requires native Windows')
    evidence = arguments.output.resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    work, sid = sandbox.create_work(arguments.work)
    logs = work / 'logs'
    logs.mkdir()
    records = []
    counter = [0]
    real_run = subprocess.run
    cli_name = arguments.cli.name

    def confined(argv, **keywords):
        argv = [str(item) for item in argv]
        if Path(argv[0]).name != cli_name:
            return real_run(argv, **keywords)
        counter[0] += 1
        out, err = logs / f'cli-{counter[0]:03d}.stdout', logs / f'cli-{counter[0]:03d}.stderr'
        result = trace.run(argv, work=work, sid=sid, env=keywords['env'], cwd=keywords['cwd'], stdout=out, stderr=err,
                           timeout=keywords.get('timeout', 600))
        records.extend(result['processes'])
        stray = [p for p in result['processes'] if p['image'] is None or Path(p['image']).name.lower() in FORBIDDEN]
        if stray:
            raise ValueError(f'undeclared or forbidden process started by {argv[1:3]}: {stray[:3]}')
        return types.SimpleNamespace(returncode=result['exit_code'], stdout=out.read_bytes(), stderr=err.read_bytes())

    subprocess.run = confined
    # The visibility probe runs under the same SID before the cases; it must be denied every hidden root.
    probe_dir = work / 'probe'
    probe_dir.mkdir()
    probe = probe_dir / arguments.probe.name
    shutil.copy2(arguments.probe, probe)
    command = [probe]
    for path in arguments.hidden:
        command.extend(['--read', path.resolve()])
    out, err = logs / 'probe.stdout', logs / 'probe.stderr'
    outcome = trace.run([str(c) for c in command], work=work, sid=sid, env={'PATH': '', 'SystemRoot': os.environ['SystemRoot']},
                        cwd=probe_dir, stdout=out, stderr=err, timeout=120)
    visibility = {'exit': outcome['exit_code'], 'roots': [str(p.resolve()) for p in arguments.hidden],
                  'stdout': out.read_text(encoding='utf-8', errors='replace').strip()}
    shutil.copy2(out, evidence / 'visibility.stdout')
    if outcome['exit_code'] != 0:
        raise SystemExit(f'visibility probe failed: {visibility}')
    sys.argv = [str(TOOLS / 'test_cli_fetch_https.py'), '--cli', str(arguments.cli), '--output', str(evidence / 'cases'),
                '--work', str(work / 'cases-work')]
    if arguments.publisher_cases:
        sys.argv.append('--publisher-cases')
    code = 0
    try:
        runpy.run_path(str(TOOLS / 'test_cli_fetch_https.py'), run_name='__main__')
    except SystemExit as exit_request:
        code = exit_request.code or 0
    seen = sorted({Path(p['image']).name.lower() for p in records if p['image']})
    summary = {'cases_exit': code, 'cli_calls': counter[0], 'process_records': len(records), 'process_images': seen,
               'sid_work': str(work), 'visibility': visibility}
    (evidence / 'adapter-result.json').write_text(json.dumps(summary, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    print(json.dumps({k: summary[k] for k in ('cases_exit', 'cli_calls', 'process_records', 'process_images')}))
    sys.exit(code)


if __name__ == '__main__':
    main()
