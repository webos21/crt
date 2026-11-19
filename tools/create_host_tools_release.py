#!/usr/bin/env python3
"""Pack and verify the public release assets of one independent CRT host-tools bundle.

`pack` takes a bundle made by create_host_tools.py (promotion `preview`) plus the public release manifest of the Dev SDK it
was assembled from, and writes into a NEW directory: one deterministic `crt-<version>.tar.xz` (one top-level directory, sorted
members, owner 0, mtime 0, relative symlinks kept), `SHA256SUMS-host-tools-<os>-<arch>` and a release record naming the
producer commit, whether the assembler tree was clean, the source SDK identity and the six values a consumer pins.
`verify` extracts an archive into a new directory with the stdlib `data` filter and re-checks it against the record and the
bundle's own manifest. Nothing is uploaded; publishing is a separate, deliberate step.

The record is evidence, not authentication: a published bundle is trusted only through the digests a caller pins.
"""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tarfile

sys.path.insert(0, str(Path(__file__).parent))
import create_host_tools as producer

RECORD_SCHEMA = 'crt.host-tools-release-record'
DEFAULT_REPO = 'webos21/crt'
MAX_ARCHIVE = 1 << 30


def sha256_file(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def top_name(host):
    return 'crt-host-tools-{}-{}'.format(host['os'], host['arch'])


def archive_name(version):
    return 'crt-{}.tar.xz'.format(version)


def sums_name(host):
    return 'SHA256SUMS-host-tools-{}-{}'.format(host['os'], host['arch'])


def record_name(host):
    return 'host-tools-release-record-{}-{}.json'.format(host['os'], host['arch'])


def pack_archive(bundle, host, output):
    """Deterministic tar.xz of a verified bundle: sorted, owner 0, mtime 0, modes 0755 (dirs/executables) or 0644."""
    bundle = Path(bundle)
    top = top_name(host)
    buffer = io.BytesIO()
    with tarfile.open(fileobj=buffer, mode='w', format=tarfile.PAX_FORMAT) as archive:
        def entry(name, kind, mode, **extra):
            info = tarfile.TarInfo(name)
            info.type, info.mode, info.uid, info.gid, info.uname, info.gname, info.mtime = kind, mode, 0, 0, '', '', 0
            for key, value in extra.items():
                setattr(info, key, value)
            return info
        archive.addfile(entry(top, tarfile.DIRTYPE, 0o755))
        for path in sorted(bundle.rglob('*'), key=lambda p: p.relative_to(bundle).parts):
            relative = path.relative_to(bundle).as_posix()
            name = top + '/' + relative
            if path.is_symlink():
                archive.addfile(entry(name, tarfile.SYMTYPE, 0o777, linkname=os.readlink(path)))
            elif path.is_dir():
                archive.addfile(entry(name, tarfile.DIRTYPE, 0o755))
            elif path.is_file():
                mode = 0o755 if os.access(path, os.X_OK) else 0o644
                info = entry(name, tarfile.REGTYPE, mode, size=path.stat().st_size)
                with path.open('rb') as stream:
                    archive.addfile(info, stream)
            else:
                raise ValueError('special file in the bundle: ' + relative)
    import lzma
    data = lzma.compress(buffer.getvalue(), format=lzma.FORMAT_XZ, check=lzma.CHECK_CRC64, preset=9 | lzma.PRESET_EXTREME)
    if len(data) > MAX_ARCHIVE:
        raise ValueError('host-tools archive exceeds the size limit')
    Path(output).write_bytes(data)


def pins(release_manifest):
    manifest = json.loads(Path(release_manifest).read_text(encoding='utf-8'))
    package = [a for a in manifest.get('assets', []) if 'install' in a]
    if len(package) != 1:
        raise ValueError('source SDK release manifest must describe exactly one installable archive')
    return manifest, package[0]


def pack(bundle, sdk_release_manifest, output, tag, repo=DEFAULT_REPO, allow_dirty=False):
    bundle, output = Path(bundle).resolve(), Path(output).absolute()
    manifest = producer.verify_bundle(bundle)
    host = manifest['host']
    if manifest['promotion'] != 'preview':
        raise ValueError('only a bundle assembled with --promotion preview can be packed for release')
    if manifest['version'] != tag:
        raise ValueError('bundle version {} differs from the release tag {}'.format(manifest['version'], tag))
    if manifest['assembler']['working_tree_dirty'] and not allow_dirty:
        raise ValueError('the bundle was assembled from a dirty tree (--allow-dirty is for rehearsals only)')
    release, package = pins(sdk_release_manifest)
    install = package['install']
    if install.get('target') != host or install.get('version') != manifest['source_sdk']['version']:
        raise ValueError('source SDK release manifest does not describe the SDK the bundle was assembled from')
    if output.exists() or output.is_symlink():
        raise ValueError('release output already exists; select a new directory')
    output.mkdir(parents=True)
    name = archive_name(tag)
    pack_archive(bundle, host, output / name)
    archive_sha = sha256_file(output / name)
    manifest_sha = sha256_file(bundle / 'manifest.json')
    (output / sums_name(host)).write_text('{}  {}\n'.format(archive_sha, name), encoding='utf-8')
    make_provenance = json.loads((bundle / 'share/crt/dependencies/make/provenance.json').read_text(encoding='utf-8'))
    try:
        commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=producer.ROOT, text=True).strip()
        dirty = bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=producer.ROOT))
    except (OSError, subprocess.CalledProcessError):
        commit, dirty = None, None
    record = {
        'schema': RECORD_SCHEMA, 'format': 0, 'tag': tag, 'version': manifest['version'], 'host': host, 'promotion': manifest['promotion'],
        'producer': {'repository': repo, 'packer_commit': commit, 'packer_working_tree_dirty': dirty,
                     'packer_script_sha256': sha256_file(Path(__file__)), 'assembler': manifest['assembler']},
        'source_sdk': {'version': manifest['source_sdk']['version'], 'source_commit': manifest['source_sdk']['source_commit'],
                       'sdk_manifest_sha256': manifest['source_sdk']['manifest_sha256'],
                       'release_manifest_sha256': sha256_file(sdk_release_manifest),
                       'archive': {'name': package['name'], 'size': package['size'], 'sha256': package['sha256']},
                       'content_sha256': install['content_sha256']},
        'bundle': {'manifest_sha256': manifest_sha, 'content_sha256': manifest['content_sha256'], 'files': len(manifest['inventory']),
                   'executable_files': len(manifest['executable_files']), 'applets': len(manifest['applets']),
                   'external_prerequisites': manifest['external_prerequisites']},
        'make': {'source': make_provenance['source'], 'notice_sha256': make_provenance['notice_sha256'],
                 'source_sdk_tool_sha256': make_provenance['source_sdk_tool_sha256']},
        'archive': {'name': name, 'size': (output / name).stat().st_size, 'sha256': archive_sha, 'top_level': top_name(host)},
        'consumer_pin': {'url': 'https://github.com/{}/releases/download/{}/{}'.format(repo, tag, name), 'name': name,
                         'size': (output / name).stat().st_size, 'sha256': archive_sha, 'manifest_sha256': manifest_sha,
                         'version': manifest['version']},
        'trust': {'publisher_authentication': 'not_verified', 'basis': 'caller-pinned digests'}}
    (output / record_name(host)).write_text(json.dumps(record, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return record


def verify(archive, record_path, extract_to):
    archive, extract_to = Path(archive), Path(extract_to).absolute()
    record = json.loads(Path(record_path).read_text(encoding='utf-8'))
    if record.get('schema') != RECORD_SCHEMA or record.get('format') != 0:
        raise ValueError('unsupported host-tools release record')
    if archive.name != record['archive']['name'] or archive.stat().st_size != record['archive']['size'] \
            or sha256_file(archive) != record['archive']['sha256']:
        raise ValueError('archive differs from the release record')
    sums = archive.parent / sums_name(record['host'])
    if sums.exists() and sums.read_text(encoding='utf-8') != '{}  {}\n'.format(record['archive']['sha256'], archive.name):
        raise ValueError('checksum file differs from the release record')
    if extract_to.exists() or extract_to.is_symlink():
        raise ValueError('extraction directory already exists; select a new one')
    top = record['archive']['top_level']
    with tarfile.open(archive, 'r:xz') as tar:
        for member in tar.getmembers():
            parts = Path(member.name).parts
            if not parts or parts[0] != top or Path(member.name).is_absolute() or '..' in parts:
                raise ValueError('unsafe or foreign archive member: ' + member.name)
            if not (member.isreg() or member.isdir() or member.issym()):
                raise ValueError('special archive member: ' + member.name)
            if member.uid != 0 or member.gid != 0 or member.mtime != 0:
                raise ValueError('non-deterministic archive member metadata: ' + member.name)
        extract_to.mkdir(parents=True)
        tar.extractall(extract_to, filter='data')
    root = extract_to / top
    manifest = producer.verify_bundle(root)
    if sha256_file(root / 'manifest.json') != record['bundle']['manifest_sha256'] \
            or manifest['content_sha256'] != record['bundle']['content_sha256'] or manifest['version'] != record['version']:
        raise ValueError('extracted bundle differs from the release record')
    if os.name != 'nt':
        # Windows file modes carry no execute bit; there, executability is the native image and its .exe name, which
        # producer.verify_bundle has just re-checked for every declared executable.
        for relative in manifest['executable_files']:
            if not (root / relative).stat().st_mode & stat.S_IXUSR:
                raise ValueError('extraction lost an execute bit: ' + relative)
    return record, root


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    p = commands.add_parser('pack')
    p.add_argument('--bundle', type=Path, required=True)
    p.add_argument('--sdk-release-manifest', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--tag', required=True)
    p.add_argument('--repo', default=DEFAULT_REPO)
    p.add_argument('--allow-dirty', action='store_true')
    v = commands.add_parser('verify')
    v.add_argument('--archive', type=Path, required=True)
    v.add_argument('--record', type=Path, required=True)
    v.add_argument('--extract-to', type=Path, required=True)
    args = parser.parse_args()
    if args.command == 'pack':
        record = pack(args.bundle, args.sdk_release_manifest, args.output, args.tag, args.repo, args.allow_dirty)
        print(json.dumps(record['consumer_pin'], indent=2))
    else:
        record, root = verify(args.archive, args.record, args.extract_to)
        print(json.dumps({'verified': True, 'root': str(root), 'manifest_sha256': record['bundle']['manifest_sha256']}, indent=2))


if __name__ == '__main__':
    main()
