#!/usr/bin/env python3
"""Release orchestration for the Dev SDK Preview workflow (`.github/workflows/sdk-release.yml`, plan step S3).

A build-host driver, never a runtime dependency. Stdlib only. Commands:

  plan     validate the request (host, date, serial, commit on main, CI success) and print the names and runner for the jobs.
  publish  the only step with write permission: re-verify the packaged bytes, stage a draft release, download it back and compare
           every byte, then (unless --draft-only) turn it into a non-latest prerelease. It never rebuilds and never overwrites.

The assets are exactly the files `crt-dev-release` produced (verified by tools/ci_sdk_release.py, which shares no code with the
producer). Publisher authentication is *not* part of publication: the owner signs the archive offline afterwards and attaches `.sig`
(docs/security/publisher_signing.md), then the consumer pin is promoted.
"""
import argparse
import datetime
import json
import os
from pathlib import Path
import re
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ci_sdk_release  # noqa: E402

BASE_VERSION = '0.0.1'
# host key -> OS, architecture, hosted runner, and the dotted host label carried by the tag (a prerelease identifier allows no `_`).
# `msvc_arch` and `llvm_asset` feed .github/actions/sdk-host-tools on Windows and must equal the sdk-integration.yml row of the host
# (tools/test_sdk_workflows.py); `bootstrap` marks a host that has no published, pinned crt-cli Preview yet (see `smoke_policy`). No host is
# marked now: Windows/arm64 was the first and lost the mark when its Preview was pinned.
HOSTS = {
    'linux-x64': {'os': 'linux', 'arch': 'x86_64', 'runner': 'ubuntu-26.04', 'label': 'linux.x64'},
    'linux-arm64': {'os': 'linux', 'arch': 'aarch64', 'runner': 'ubuntu-26.04-arm', 'label': 'linux.arm64'},
    'macos-arm64': {'os': 'macos', 'arch': 'aarch64', 'runner': 'macos-latest', 'label': 'macos.arm64'},
    'windows-x64': {'os': 'windows', 'arch': 'x86_64', 'runner': 'windows-latest', 'label': 'windows.x64', 'msvc_arch': 'x64', 'llvm_asset': 'win64'},
    'windows-arm64': {'os': 'windows', 'arch': 'aarch64', 'runner': 'windows-11-arm', 'label': 'windows.arm64', 'msvc_arch': 'arm64',
                      'llvm_asset': 'woa64'},
}
# The statement a first-host release's reviewed notes must carry verbatim: the consumer smoke by a published CLI did not run.
BOOTSTRAP_MARKER = 'Consumer smoke by the published CLI: NOT RUN (first-host bootstrap)'
PINS_FILE = Path(__file__).resolve().parent.parent / '.github' / 'ci' / 'sdk-pins.json'
COMMIT_RE = re.compile(r'[0-9a-f]{40}')
TOKEN_RE = re.compile(r'\{\{[A-Z0-9_]+\}\}')
NOTES_TOKENS = ('{{TAG}}', '{{COMMIT}}', '{{MANIFEST_SHA256}}', '{{MANIFEST_BYTES}}', '{{CONTENT_SHA256}}', '{{ASSET_TABLE}}', '{{TARGET}}')


NOTES_PATH_RE = re.compile(r'docs/releases/[A-Za-z0-9][A-Za-z0-9_.-]*\.md')


class ReleaseError(Exception):
    pass


def log(message):
    print(message, flush=True)


def require_commit(sha):
    if not COMMIT_RE.fullmatch(sha or ''):
        raise ReleaseError('the commit must be a full 40-character lowercase SHA (a branch or tag is refused)')
    return sha


def names(host, date, serial):
    """Tag and title for one release, e.g. v0.0.1-preview.20261118.linux.x64.1."""
    if host not in HOSTS:
        raise ReleaseError('unknown host %r' % host)
    if not re.fullmatch(r'20[0-9]{6}', str(date)):
        raise ReleaseError('the date must be YYYYMMDD')
    try:
        datetime.datetime.strptime(str(date), '%Y%m%d')
    except ValueError:
        raise ReleaseError('the date %s is not a calendar date' % date)
    if not re.fullmatch(r'[1-9][0-9]{0,2}', str(serial)):
        raise ReleaseError('the serial must be a positive integer without leading zeros')
    info = HOSTS[host]
    tag = 'v%s-preview.%s.%s.%s' % (BASE_VERSION, date, info['label'], serial)
    return {'tag': tag, 'title': 'Development SDK Preview %s (%s/%s)' % (tag, info['os'], info['arch']), 'target': '%s/%s' % (info['os'], info['arch']), **info}


def render_notes(template, *, tag, commit, manifest, content_sha256, assets, target):
    """Fill the reviewed template's explicit tokens. The numbers only exist after the build, so the committed notes carry tokens.

    A template that does not state the manifest pin and the content identity cannot say what is being published and is refused,
    as is any other `{{TOKEN}}`: nothing is left to guess at publication time.
    """
    for required in ('{{MANIFEST_SHA256}}', '{{CONTENT_SHA256}}', '{{ASSET_TABLE}}'):
        if required not in template:
            raise ReleaseError('the notes template must contain %s' % required)
    table = ['| Asset | Bytes | SHA-256 |', '| --- | ---: | --- |']
    table += ['| `%s` | %d | `%s` |' % (a['name'], a['size'], a['sha256']) for a in sorted(assets, key=lambda item: item['name'])]
    values = {'{{TAG}}': tag, '{{COMMIT}}': commit, '{{MANIFEST_SHA256}}': manifest['sha256'], '{{MANIFEST_BYTES}}': str(manifest['size']),
              '{{CONTENT_SHA256}}': content_sha256, '{{ASSET_TABLE}}': '\n'.join(table), '{{TARGET}}': target}
    text = template
    for token, value in values.items():
        text = text.replace(token, value)
    leftover = TOKEN_RE.findall(text)
    if leftover:
        raise ReleaseError('unknown notes token(s): %s' % ', '.join(sorted(set(leftover))))
    return text


def require_notes_path(path):
    if not NOTES_PATH_RE.fullmatch(path or ''):
        raise ReleaseError('the notes file must be a reviewed docs/releases/*.md path of the repository, not %r' % path)
    return path


def has_required_reviewers(environment_json):
    """Whether the `release` environment makes a human approve every deployment (a missing environment is auto-created without one)."""
    return any(rule.get('type') == 'required_reviewers' and rule.get('reviewers') for rule in environment_json.get('protection_rules', []))


def run(argv, check=True, cwd=None):
    done = subprocess.run([str(a) for a in argv], capture_output=True, text=True, cwd=cwd)
    if check and done.returncode != 0:
        raise ReleaseError('%s failed: %s' % (' '.join(str(a) for a in argv[:3]), (done.stderr or done.stdout).strip()[-400:]))
    return done


def gh(*argv, check=True):
    return run(['gh', *argv], check=check)


def ci_succeeded(runs_json):
    """Whether the listed `CI` workflow runs of one commit contain a completed success (every matrix leg of the required gate)."""
    return any(item.get('conclusion') == 'success' and item.get('status') == 'completed' for item in runs_json.get('workflow_runs', []))


def docs_only(paths):
    """ci.yml does not run for a push that touches only these; porting/** is ignored there but ships in the SDK, so it is *not* docs here."""
    return bool(paths) and all(path.endswith('.md') or path.startswith('docs/') for path in paths)


def ci_gate(commit, runs_for, changed_files, parent_of, depth=20):
    """The commit whose successful CI run covers `commit`, or a ReleaseError.

    ci.yml ignores docs-only pushes, and the release procedure requires the reviewed notes to be committed before the release
    commit, so the release commit is usually docs-only and has no run of its own. It is then covered by its nearest ancestor that
    has a run, provided every commit passed over changed only *.md or docs/**. A run that exists but is not a success, or a
    skipped commit that changed anything else, is a refusal.
    """
    sha = commit
    for _ in range(depth):
        runs = runs_for(sha)
        if ci_succeeded(runs):
            return sha
        if runs.get('workflow_runs'):
            raise ReleaseError('the CI run(s) of %s have not succeeded' % sha)
        if not docs_only(changed_files(sha)):
            raise ReleaseError('%s changed more than docs and has no CI run' % sha)
        sha = parent_of(sha)
    raise ReleaseError('no successful CI run within %d docs-only commits of %s' % (depth, commit))


def smoke_policy(host, info, pins_hosts, notes_text):
    """How the consumer smoke of the packaged bytes is done: `pinned` (a published, signature-checked CLI Preview installs the archive) or
    `none` (first-host bootstrap: it is *not run*, the notes say so, and the source-hidden native consumer of the extracted archive stands in).

    The exception cannot become the default or outlive its reason. A host that is not marked `bootstrap` must have a pin; a `bootstrap` host
    that has gained a pin is refused until the mark is removed; and without a pin the reviewed notes must carry BOOTSTRAP_MARKER, so that
    "no published CLI exercised this archive" is never omitted from what the public reads.
    """
    pinned = '%s-%s' % (info['os'], info['arch']) in pins_hosts
    if info.get('bootstrap'):
        if pinned:
            raise ReleaseError('%s has a pinned published CLI Preview, so the first-host bootstrap exception has ended: remove `bootstrap` for it '
                               'in HOSTS and let the normal consumer smoke run' % host)
        if BOOTSTRAP_MARKER not in notes_text:
            raise ReleaseError('%s has no published CLI Preview: the reviewed notes must state "%s"' % (host, BOOTSTRAP_MARKER))
        return 'none'
    if not pinned:
        raise ReleaseError('%s is not a first-host bootstrap and has no pinned CLI Preview in .github/ci/sdk-pins.json' % host)
    return 'pinned'


def load_pins_hosts():
    return json.loads(PINS_FILE.read_text(encoding='utf-8'))['hosts']


def command_plan(args):
    commit = require_commit(args.commit)
    info = names(args.host, args.date, args.serial)
    done = run(['git', 'merge-base', '--is-ancestor', commit, 'origin/main'], check=False)
    if done.returncode != 0:
        raise ReleaseError('%s is not on main' % commit)
    covered_by = ci_gate(
        commit,
        lambda sha: json.loads(gh('api', 'repos/%s/actions/workflows/ci.yml/runs?head_sha=%s&per_page=30' % (args.repo, sha)).stdout),
        lambda sha: run(['git', 'diff', '--name-only', sha + '^1', sha]).stdout.split(),
        lambda sha: run(['git', 'rev-parse', sha + '^1']).stdout.strip())
    log('CI gate: %s is covered by the successful run of %s' % (commit, covered_by))
    if gh('release', 'view', info['tag'], '--repo', args.repo, check=False).returncode == 0:
        raise ReleaseError('a release for %s already exists; releases are never overwritten' % info['tag'])
    environment = gh('api', 'repos/%s/environments/release' % args.repo, check=False)
    if environment.returncode != 0 or not has_required_reviewers(json.loads(environment.stdout)):
        raise ReleaseError('the `release` environment does not exist or has no required reviewers; publication would not wait for approval')
    notes = Path(require_notes_path(args.notes_file))
    if not notes.is_file() or not notes.read_text(encoding='utf-8').strip():
        raise ReleaseError('the reviewed release notes %s are missing or empty at this commit' % args.notes_file)
    notes_text = notes.read_text(encoding='utf-8')
    render_notes(notes_text, tag=info['tag'], commit=commit, manifest={'sha256': '0' * 64, 'size': 0}, content_sha256='0' * 64,
                 assets=[{'name': 'x', 'size': 0, 'sha256': '0' * 64}], target=info['target'])
    smoke_cli = smoke_policy(args.host, info, load_pins_hosts(), notes_text)
    outputs = {'tag': info['tag'], 'runner': info['runner'], 'os': info['os'], 'arch': info['arch'], 'smoke_host': '%s-%s' % (info['os'], info['arch']),
               'preset': '%s-host-ninja-debug' % info['os'], 'msvc_arch': info.get('msvc_arch', ''), 'llvm_asset': info.get('llvm_asset', ''),
               'smoke_cli': smoke_cli}
    for key, value in outputs.items():
        log('%s=%s' % (key, value))
    if args.github_output:
        with open(args.github_output, 'a', encoding='utf-8') as handle:
            handle.writelines('%s=%s\n' % item for item in outputs.items())


def command_publish(args):
    commit = require_commit(args.commit)
    info = names(args.host, args.date, args.serial)
    release = Path(args.release_dir).resolve()
    result = ci_sdk_release.verify(release, info['os'], info['arch'], info['tag'], commit)
    if result['problems']:
        raise ReleaseError('the packaged bytes no longer verify: ' + '; '.join(result['problems']))
    manifest, content = result['manifest'], result.get('content_sha256')
    if not content:
        raise ReleaseError('the verifier did not report the installed content identity')
    files = sorted(entry for entry in release.iterdir())
    if gh('release', 'view', info['tag'], '--repo', args.repo, check=False).returncode == 0:
        raise ReleaseError('a release for %s already exists; releases are never overwritten' % info['tag'])
    template = Path(require_notes_path(args.notes_file)).resolve()
    if not template.is_file():
        raise ReleaseError('the release notes template is missing')
    smoke_policy(args.host, info, load_pins_hosts(), template.read_text(encoding='utf-8'))
    rendered = render_notes(template.read_text(encoding='utf-8'), tag=info['tag'], commit=commit, manifest=manifest, content_sha256=content,
                            assets=result['assets'], target=info['target'])
    work = Path(args.work).resolve()
    notes = work.parent / (work.name + '-notes.md')
    notes.parent.mkdir(parents=True, exist_ok=True)
    notes.write_text(rendered, encoding='utf-8')
    gh('release', 'create', info['tag'], '--repo', args.repo, '--draft', '--prerelease', '--latest=false', '--target', commit,
       '--title', info['title'], '--notes-file', notes, *files)
    log('draft %s created' % info['tag'])
    work.mkdir(parents=True, exist_ok=False)
    gh('release', 'download', info['tag'], '--repo', args.repo, '--dir', work)
    expected = {entry.name: entry.read_bytes() for entry in files}
    got = {entry.name: entry.read_bytes() for entry in work.iterdir()}
    if sorted(got) != sorted(expected) or any(got[name] != expected[name] for name in expected):
        raise ReleaseError('the draft assets differ from the verified files')
    view = json.loads(gh('release', 'view', info['tag'], '--repo', args.repo, '--json', 'isDraft,isPrerelease,targetCommitish,assets').stdout)
    if not view['isDraft'] or not view['isPrerelease'] or view['targetCommitish'] != commit or len(view['assets']) != len(files):
        raise ReleaseError('the staged draft does not match the request: ' + json.dumps(view)[:300])
    log('draft verified byte for byte (%d assets)' % len(files))
    if args.draft_only:
        log('draft-only: stopping before publication; delete the draft by hand')
        return
    gh('release', 'edit', info['tag'], '--repo', args.repo, '--draft=false', '--prerelease', '--latest=false')
    final = json.loads(gh('release', 'view', info['tag'], '--repo', args.repo, '--json', 'isDraft,isPrerelease,tagName,url').stdout)
    if final['isDraft'] or not final['isPrerelease']:
        raise ReleaseError('the published release is not a non-draft prerelease: %s' % final)
    log('published %s' % final['url'])


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    sub = parser.add_subparsers(dest='command', required=True)
    for name in ('plan', 'publish'):
        p = sub.add_parser(name)
        p.add_argument('--host', required=True, choices=sorted(HOSTS))
        p.add_argument('--commit', required=True)
        p.add_argument('--date', required=True)
        p.add_argument('--serial', required=True)
        p.add_argument('--repo', required=True)
        p.add_argument('--notes-file', required=True)
        if name == 'plan':
            p.add_argument('--github-output', default=os.environ.get('GITHUB_OUTPUT'))
        else:
            p.add_argument('--release-dir', required=True)
            p.add_argument('--work', required=True)
            p.add_argument('--draft-only', action='store_true')
    args = parser.parse_args(argv)
    try:
        {'plan': command_plan, 'publish': command_publish}[args.command](args)
    except ReleaseError as error:
        log('RELEASE FAIL: %s' % error)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
