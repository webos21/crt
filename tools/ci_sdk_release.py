#!/usr/bin/env python3
"""CI verifier of a produced Dev SDK release asset set. Stdlib only; nothing is uploaded or published.

`verify` re-derives every claim of the asset set from the bytes with its own small implementation, deliberately not importing the
producer (`crt_sdk_release.py`, `crt_sdk_install_metadata.py`): a producer that agrees with itself proves nothing. It checks that

* the directory holds exactly the files the release manifest lists plus the manifest and the `SHA256SUMS-dev-<os>-<arch>` file;
* sizes and SHA-256 agree between the files, the manifest and the sums file, and the names follow the release naming;
* the manifest identity (profile, version, target, producer commit) is the one the caller expects;
* the archive extracts (safely, bounded) to the single `dev/` root and its install metadata (counts, sizes, content identity) is
  true of what actually extracted, and its `VERSION`/`manifest.json` agree with the release;
* the portable toolchain contract is bound to that content identity and commit and names no machine path.

The result is a JSON document with the manifest pin (SHA-256 and size of the release manifest) that reviewers and the release notes
quote. Exit status is non-zero on any problem; every problem found is listed, not only the first.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import sys
import tarfile
import tempfile
import zipfile

VERSION_RE = re.compile(r'^v[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z][0-9A-Za-z.]*)?$')
HEX64 = re.compile(r'^[0-9a-f]{64}$')
COMMIT_RE = re.compile(r'^[0-9a-f]{40}$')
# Bounds on what the verifier is willing to extract; the packaged SDK is about 2.8k entries and 105 MB.
MAX_ENTRIES = 20000
MAX_BYTES = 1 << 30
MAX_FILE_BYTES = 256 << 20
SUPPORTED = {('linux', 'x86_64'), ('linux', 'aarch64'), ('macos', 'aarch64'), ('windows', 'x86_64'), ('windows', 'aarch64')}


class Problems(list):
    def add(self, text):
        self.append(text)


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, 'rb') as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b''):
            digest.update(chunk)
    return digest.hexdigest()


def load_json(path, problems, label):
    try:
        value = json.loads(Path(path).read_text(encoding='utf-8'))
    except (OSError, ValueError) as error:
        problems.add('%s: cannot read as JSON: %s' % (label, error))
        return None
    if not isinstance(value, dict):
        problems.add('%s: not a JSON object' % label)
        return None
    return value


def expected_names(version, os_name, arch):
    suffix = 'zip' if os_name == 'windows' else 'tar.xz'
    return {
        'archive': 'crt-%s-%s-%s-dev.%s' % (version, os_name, arch, suffix),
        'contract': 'crt-%s-%s-%s-dev-toolchain.json' % (version, os_name, arch),
        'sources': ['crt-%s-%s-02-cxx-source.tar.xz' % (version, os_name), 'crt-%s-%s-03-gfx-simple-source.tar.xz' % (version, os_name)],
        'sums': 'SHA256SUMS-dev-%s-%s' % (os_name, arch),
        'manifest': 'release-manifest-dev-%s-%s.json' % (os_name, arch)}


def parse_sums(path, problems):
    entries = {}
    try:
        text = Path(path).read_text(encoding='utf-8')
    except (OSError, UnicodeDecodeError) as error:
        problems.add('sums file unreadable: %s' % error)
        return entries
    for number, line in enumerate(text.splitlines(), 1):
        match = re.fullmatch(r'([0-9a-f]{64})  ([^/\\\s][^/\\]*)', line)
        if match is None:
            problems.add('sums file line %d is not "<sha256>  <name>"' % number)
            continue
        if match.group(2) in entries:
            problems.add('sums file lists %s twice' % match.group(2))
            continue
        entries[match.group(2)] = match.group(1)
    return entries


def safe_member(name, problems, label):
    """A relative, normalized path below the archive's single root; anything else is refused before extraction."""
    path = PurePosixPath(name)
    if path.is_absolute() or '\\' in name or ':' in name.split('/')[0] or any(part in ('', '.', '..') for part in name.rstrip('/').split('/')):
        problems.add('%s: unsafe member path %r' % (label, name))
        return False
    return True


def extract(archive, target, root, problems):
    """Extract with explicit member validation (no absolute/parent paths, no links leaving the root, no devices) and bounds."""
    label = 'archive'
    count = total = 0
    if archive.name.endswith('.zip'):
        with zipfile.ZipFile(archive) as bundle:
            infos = bundle.infolist()
            if len(infos) > MAX_ENTRIES:
                problems.add('archive has %d entries (limit %d)' % (len(infos), MAX_ENTRIES))
                return False
            for info in infos:
                count += 1
                total += info.file_size
                if not safe_member(info.filename, problems, label) or not (info.filename == root + '/' or info.filename.startswith(root + '/')):
                    problems.add('archive member outside the %s/ root: %r' % (root, info.filename))
                    return False
                if info.file_size > MAX_FILE_BYTES or total > MAX_BYTES:
                    problems.add('archive exceeds the extraction bounds')
                    return False
                mode = info.external_attr >> 16
                if mode and stat.S_ISLNK(mode):
                    problems.add('zip symlink member %r is not supported by the verifier' % info.filename)
                    return False
            bundle.extractall(target)
        return True
    with tarfile.open(archive, 'r:xz') as bundle:
        members = bundle.getmembers()
        if len(members) > MAX_ENTRIES:
            problems.add('archive has %d entries (limit %d)' % (len(members), MAX_ENTRIES))
            return False
        for member in members:
            count += 1
            total += member.size if member.isfile() else 0
            if not safe_member(member.name, problems, label) or not (member.name == root or member.name.startswith(root + '/')):
                problems.add('archive member outside the %s/ root: %r' % (root, member.name))
                return False
            if not (member.isfile() or member.isdir() or member.issym()):
                problems.add('archive member %r is a %s, only files, directories and symlinks are allowed' % (member.name, 'hard link' if member.islnk() else 'special file'))
                return False
            if member.issym():
                resolved = os.path.normpath(os.path.join(os.path.dirname(member.name), member.linkname))
                if os.path.isabs(member.linkname) or not (resolved == root or resolved.startswith(root + '/')):
                    problems.add('symlink %r points outside the SDK: %r' % (member.name, member.linkname))
                    return False
            if member.size > MAX_FILE_BYTES or total > MAX_BYTES:
                problems.add('archive exceeds the extraction bounds')
                return False
        bundle.extractall(target, members=members, filter='data')
    return True


def inventory(sdk):
    """Own implementation of the content identity: files by SHA-256 and symlinks by link text, component-wise case-sensitive order."""
    items, files, directories, symlinks, total, largest, longest, deepest = [], 0, 1, 0, 0, 0, 0, 0
    for path in sorted(sdk.rglob('*'), key=lambda item: item.relative_to(sdk).parts):
        relative = path.relative_to(sdk).as_posix()
        archive_path = sdk.name + '/' + relative
        longest = max(longest, len(archive_path.encode('utf-8')))
        deepest = max(deepest, len(archive_path.split('/')))
        if path.is_symlink():
            symlinks += 1
            items.append([relative, 'symlink', os.readlink(path)])
        elif path.is_dir():
            directories += 1
        elif path.is_file():
            files += 1
            size = path.stat().st_size
            total += size
            largest = max(largest, size)
            items.append([relative, 'file', sha256_file(path)])
    identity = hashlib.sha256(json.dumps(items, ensure_ascii=False, separators=(',', ':')).encode()).hexdigest()
    return {'content_sha256': identity, 'entries': files + directories + symlinks, 'files': files, 'directories': directories,
            'symlinks': symlinks, 'uncompressed_bytes': total, 'max_file_bytes': largest, 'max_path_bytes': longest, 'max_depth': deepest}


def check_install(install, archive_name, version, os_name, arch, extracted, problems):
    if not isinstance(install, dict):
        problems.add('manifest: the archive asset has no install metadata')
        return None
    expect = {'format': 1, 'archive_format': 'zip' if archive_name.endswith('.zip') else 'tar.xz', 'root': 'dev', 'version': version,
              'stage': '02-cxx', 'profile': 'dev', 'target': {'os': os_name, 'arch': arch}}
    for key, value in expect.items():
        if install.get(key) != value:
            problems.add('install.%s is %r, expected %r' % (key, install.get(key), value))
    actual = inventory(extracted)
    for key, value in actual.items():
        if install.get(key) != value:
            problems.add('install.%s is %r but the extracted tree has %r' % (key, install.get(key), value))
    return actual


def check_extracted(extracted, version, os_name, arch, problems):
    try:
        if (extracted / 'VERSION').read_text(encoding='utf-8').strip() != version:
            problems.add('the extracted VERSION file differs from the release version')
    except OSError:
        problems.add('the extracted SDK has no VERSION file')
    manifest = load_json(extracted / 'manifest.json', problems, 'extracted manifest.json')
    if manifest is not None:
        if manifest.get('stage') != '02-cxx':
            problems.add('extracted manifest.json stage is %r, expected 02-cxx' % manifest.get('stage'))
        if manifest.get('target', {}).get('os') != os_name or manifest.get('target', {}).get('arch') != arch:
            problems.add('extracted manifest.json target differs from the release target')
    profile = load_json(extracted / 'share/crt/contracts/development.json', problems, 'development profile')
    if profile is not None and profile.get('profile') != 'dev':
        problems.add('development profile is %r, expected dev' % profile.get('profile'))


def check_contract(path, content, commit, forbidden, problems):
    contract = load_json(path, problems, 'toolchain contract')
    if contract is None:
        return
    if contract.get('schema') != 'crt.experimental.native-toolchain' or contract.get('format') != 1:
        problems.add('toolchain contract schema/format is %r/%r' % (contract.get('schema'), contract.get('format')))
    if content and contract.get('sdk_content_sha256') != content:
        problems.add('toolchain contract sdk_content_sha256 is not the content identity of the extracted archive')
    if commit and contract.get('producer_source_commit') != commit:
        problems.add('toolchain contract producer_source_commit differs from the release commit')
    if 'external_os_sdk' in contract:
        problems.add('toolchain contract names a machine path (external_os_sdk); it must be portable')
    text = Path(path).read_text(encoding='utf-8')
    for item in forbidden:
        if item and item in text:
            problems.add('toolchain contract contains the build-host path %r' % item)


def verify(assets, os_name, arch, version, commit=None, forbidden=(), keep=None):
    problems = Problems()
    result = {'host': '%s-%s' % (os_name, arch), 'version': version, 'problems': problems}
    if (os_name, arch) not in SUPPORTED:
        problems.add('unsupported target %s-%s' % (os_name, arch))
        return result
    if not VERSION_RE.match(version):
        problems.add('version %r is not vMAJOR.MINOR.PATCH[-prerelease]' % version)
        return result
    if commit is not None and not COMMIT_RE.match(commit):
        problems.add('expected commit %r is not a full lowercase SHA-1' % commit)
        return result
    assets = Path(assets)
    names = expected_names(version, os_name, arch)
    present = sorted(entry.name for entry in assets.iterdir()) if assets.is_dir() else []
    if not present:
        problems.add('%s is empty or not a directory' % assets)
        return result
    for entry in assets.iterdir():
        if entry.is_symlink() or not entry.is_file():
            problems.add('%s is not a regular file' % entry.name)
    manifest_path = assets / names['manifest']
    manifest = load_json(manifest_path, problems, 'release manifest') if manifest_path.is_file() else None
    if manifest is None:
        problems.add('release manifest %s is missing' % names['manifest'])
        return result
    result['manifest'] = {'name': names['manifest'], 'size': manifest_path.stat().st_size, 'sha256': sha256_file(manifest_path)}
    if manifest.get('profile') != 'dev':
        problems.add('manifest profile is %r, expected dev' % manifest.get('profile'))
    if manifest.get('version') != version:
        problems.add('manifest version is %r, expected %r' % (manifest.get('version'), version))
    if manifest.get('target') != {'os': os_name, 'arch': arch}:
        problems.add('manifest target is %r' % (manifest.get('target'),))
    source = manifest.get('source_commit')
    if not isinstance(source, str) or not COMMIT_RE.match(source):
        problems.add('manifest source_commit is not a full lowercase SHA-1')
    elif commit is not None and source != commit:
        problems.add('manifest source_commit %s is not the expected commit %s' % (source, commit))
    listed = manifest.get('assets')
    if not isinstance(listed, list) or not listed:
        problems.add('manifest lists no assets')
        return result
    wanted = [names['archive'], names['contract']] + names['sources']
    manifest_names = [entry.get('name') for entry in listed if isinstance(entry, dict)]
    if manifest_names != wanted:
        problems.add('manifest assets are %r, expected %r' % (manifest_names, wanted))
    expected_files = sorted(set(wanted + [names['sums'], names['manifest']]))
    if present != expected_files:
        missing, extra = sorted(set(expected_files) - set(present)), sorted(set(present) - set(expected_files))
        problems.add('asset directory differs from the release: missing %r, unexpected %r' % (missing, extra))
    sums = parse_sums(assets / names['sums'], problems) if (assets / names['sums']).is_file() else {}
    if sorted(sums) != sorted(wanted):
        problems.add('the sums file lists %r, expected %r' % (sorted(sums), sorted(wanted)))
    table = []
    for entry in listed:
        if not isinstance(entry, dict) or entry.get('name') not in wanted:
            continue
        name, path = entry['name'], assets / entry['name']
        if not path.is_file():
            continue
        digest, size = sha256_file(path), path.stat().st_size
        if entry.get('sha256') != digest or not HEX64.match(str(entry.get('sha256'))):
            problems.add('%s: manifest SHA-256 differs from the file' % name)
        if entry.get('size') != size:
            problems.add('%s: manifest size differs from the file' % name)
        if sums.get(name) != digest:
            problems.add('%s: sums file SHA-256 differs from the file' % name)
        table.append({'name': name, 'size': size, 'sha256': digest})
    result['assets'] = table
    archive = assets / names['archive']
    if archive.is_file() and isinstance(listed[0], dict):
        with tempfile.TemporaryDirectory(prefix='crt-sdk-verify-') as scratch:
            target = Path(keep) if keep else Path(scratch) / 'x'
            target.mkdir(parents=True, exist_ok=True)
            try:
                extracted_ok = extract(archive, target, 'dev', problems)
            except (OSError, tarfile.TarError, zipfile.BadZipFile) as error:
                problems.add('archive does not extract: %s' % error)
                extracted_ok = False
            if extracted_ok:
                extracted = target / 'dev'
                if not extracted.is_dir():
                    problems.add('the archive has no dev/ directory')
                else:
                    actual = check_install(listed[0].get('install'), names['archive'], version, os_name, arch, extracted, problems)
                    check_extracted(extracted, version, os_name, arch, problems)
                    if actual is not None:
                        result['content_sha256'] = actual['content_sha256']
                        contract = assets / names['contract']
                        if contract.is_file():
                            check_contract(contract, actual['content_sha256'], source if isinstance(source, str) else None,
                                           list(forbidden) + [str(assets.resolve()), str(target.resolve())], problems)
                        if listed[0].get('contract') != names['contract']:
                            problems.add('manifest names the contract %r, expected %r' % (listed[0].get('contract'), names['contract']))
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest='command', required=True)
    check = commands.add_parser('verify', help='verify a produced asset directory')
    check.add_argument('--assets', type=Path, required=True)
    check.add_argument('--os', required=True, dest='os_name', choices=sorted({item[0] for item in SUPPORTED}))
    check.add_argument('--arch', required=True, choices=sorted({item[1] for item in SUPPORTED}))
    check.add_argument('--version', required=True)
    check.add_argument('--commit', help='the full commit the assets must have been produced from')
    check.add_argument('--forbid', action='append', default=[], help='a build-host path that must not appear in the toolchain contract')
    check.add_argument('--keep-extracted', type=Path, help='extract here (fresh directory) and keep the tree for later steps')
    check.add_argument('--json', type=Path, help='write the result document here')
    args = parser.parse_args(argv)
    result = verify(args.assets, args.os_name, args.arch, args.version, args.commit, args.forbid, args.keep_extracted)
    document = dict(result, problems=list(result['problems']), ok=not result['problems'])
    if args.json:
        args.json.write_text(json.dumps(document, indent=2, sort_keys=True) + '\n', encoding='utf-8')
    for problem in document['problems']:
        print('PROBLEM: ' + problem, file=sys.stderr)
    if document['ok']:
        pin = document['manifest']
        print('OK %s %s manifest %s %d bytes sha256 %s content %s' % (document['host'], document['version'], pin['name'], pin['size'], pin['sha256'], document.get('content_sha256')))
        return 0
    return 1


if __name__ == '__main__':
    sys.exit(main())
