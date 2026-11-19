"""Cases shared by the macOS and Windows native replays of an extracted host-tools bundle (producer evidence only).

Each host script supplies `launch(tool_path, argv)` -> (returncode, stdout+stderr) which runs one tool under that host's isolation with
an empty environment and the scratch directory as the working directory. The cases use only relative file names, because the CRT
spells native drive paths differently on Windows; they exercise the four roles (shell, awk, make, toybox applets) and every alias.
"""
import json
import hashlib
from pathlib import Path

#: (label, tool, argv, expected exit code, expected output fragment)
CASES = [
    ('shell runs a script and returns its status', 'sh', ['-c', 'echo shell-ok; exit 3'], 3, 'shell-ok'),
    ('awk evaluates', 'awk', ['BEGIN{print 6*7}'], 0, '42'),
    ('make reports GNU Make', 'make', ['--version'], 0, 'GNU Make'),
    ('printf formats', 'printf', ['%s-%s\\n', 'a', 'b'], 0, 'a-b'),
    ('echo', 'echo', ['x'], 0, 'x'),
    ('expr computes', 'expr', ['6', '*', '7'], 0, '42'),
    ('basename strips a directory', 'basename', ['/a/b/c.txt'], 0, 'c.txt'),
    ('uname answers', 'uname', ['-s'], 0, ''),
]

#: A small pipeline through real files: the shell writes, the applets read.
FILE_CASES = [
    # Builtins only: the shell finds external commands through PATH, which the replay empties on purpose.
    ('shell writes a file through redirection', 'sh', ['-c', 'echo b > data.txt; echo a >> data.txt'], 0, ''),
    ('cat reads it', 'cat', ['data.txt'], 0, 'b'),
    ('sort orders it', 'sort', ['data.txt'], 0, 'a'),
    ('wc counts its lines', 'wc', ['-l', 'data.txt'], 0, '2'),
    ('ls lists it', 'ls', ['.'], 0, 'data.txt'),
]


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def crashed(code):
    """A loader failure, signal or Windows fatal status (0xC0000000 and up) rather than an ordinary exit code."""
    return code in (126, 127) or code < 0 or code >= 0xC0000000


def run_cases(copy, manifest, launch, record, suffix=''):
    for group in (CASES, FILE_CASES):
        for label, tool, argv, code, needle in group:
            rc, output = launch(Path(copy) / 'system/bin' / (tool + suffix), argv)
            record(label, rc == code and needle in output, 'exit {}: {}'.format(rc, output[-200:]))
    missing = [name for name in manifest['executable_files'] if not (Path(copy) / name).exists()]
    record('every declared executable exists in the relocated copy', not missing, missing[:3])
    failures = []
    for relative in sorted(manifest['executable_files']):
        if not relative.startswith(('system/bin/', 'bin/', 'usr/bin/')) or (suffix and not relative.endswith(suffix)):
            continue
        rc, output = launch(Path(copy) / relative, ['--help'])
        if crashed(rc):
            failures.append((relative, rc, output[:80]))
    record('every executable alias starts (no loader failure, signal or fatal status)', not failures, failures[:3])


def write_result(out, manifest_sha, results):
    ok = all(r['passed'] for r in results)
    (Path(out) / 'host-tools-replay.json').write_text(
        json.dumps({'passed': ok, 'bundle_manifest_sha256': manifest_sha, 'cases': results}, indent=2, ensure_ascii=False) + '\n')
    print('{}/{} cases passed'.format(sum(r['passed'] for r in results), len(results)))
    return 0 if ok else 1
