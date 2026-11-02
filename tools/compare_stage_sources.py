#!/usr/bin/env python3
"""Compare two pinned source-stage archives without extracting or modifying them.

Differences are evidence, not a release approval. A completed comparison exits
zero even when inputs differ; --require-identical additionally gates exact bytes.
"""
import argparse
import hashlib
import json
import lzma
from pathlib import Path, PurePosixPath
import sys
import tarfile

MAX_MEMBERS = 250000
MAX_UNPACKED_BYTES = 2 * 1024 * 1024 * 1024
MAX_JSON_BYTES = 1024 * 1024


def digest_stream(stream, limit=None):
    digest = hashlib.sha256()
    size = 0
    while block := stream.read(1024 * 1024):
        size += len(block)
        if limit is not None and size > limit:
            raise ValueError('source archive exceeds the uncompressed byte limit')
        digest.update(block)
    return digest.hexdigest(), size


def digest_json(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':'),
                                     ensure_ascii=False).encode('utf-8')).hexdigest()


def read_archive(path):
    path = Path(path).resolve(strict=True)
    with path.open('rb') as stream:
        archive_hash, archive_size = digest_stream(stream)
    with lzma.open(path, 'rb') as stream:
        tar_hash, tar_size = digest_stream(stream, MAX_UNPACKED_BYTES)
    entries, order, declarations = {}, [], {}
    total = 0
    with tarfile.open(path, mode='r|xz') as archive:
        for member in archive:
            if len(order) >= MAX_MEMBERS:
                raise ValueError('source archive exceeds the member limit')
            name = member.name
            parts = PurePosixPath(name).parts
            if not parts or name.startswith('/') or '\\' in name or '..' in parts or ':' in parts[0]:
                raise ValueError('unsafe source member: ' + name)
            normalized = PurePosixPath(name).as_posix()
            if normalized != name.rstrip('/') or normalized in entries:
                raise ValueError('noncanonical or duplicate source member: ' + name)
            if not (member.isfile() or member.isdir()):
                raise ValueError('source archive contains a link or special member: ' + name)
            total += member.size
            if total > MAX_UNPACKED_BYTES:
                raise ValueError('source archive exceeds the payload byte limit')
            entry = {'type': 'file' if member.isfile() else 'directory', 'mode': member.mode,
                     'size': member.size, 'uid': member.uid, 'gid': member.gid,
                     'uname': member.uname, 'gname': member.gname, 'mtime': member.mtime,
                     'pax_headers': member.pax_headers}
            if member.isfile():
                with archive.extractfile(member) as stream:
                    entry['sha256'], actual_size = digest_stream(stream, MAX_UNPACKED_BYTES)
                if actual_size != member.size:
                    raise ValueError('truncated source member: ' + name)
                is_declaration = name == 'stage.json' or name.startswith('stages/recipes/') and name.endswith('.json')
                if is_declaration:
                    # Streaming tar cannot seek backwards; declaration bytes
                    # are read in a separate bounded pass below.
                    if member.size > MAX_JSON_BYTES:
                        raise ValueError('oversized stage declaration: ' + name)
                    declarations[name] = None
            entries[normalized] = entry
            order.append(normalized)
    if declarations:
        with tarfile.open(path, mode='r|xz') as archive:
            for member in archive:
                if member.name in declarations:
                    declarations[member.name] = json.loads(archive.extractfile(member).read(MAX_JSON_BYTES + 1))
    payload = [{key: entry[key] for key in ('type', 'size', 'sha256') if key in entry} | {'path': name}
               for name, entry in sorted(entries.items())]
    portable = [entry | {'mode': entries[entry['path']]['mode']} for entry in payload]
    return {'path': str(path), 'archive_sha256': archive_hash, 'archive_bytes': archive_size,
            'tar_sha256': tar_hash, 'tar_bytes': tar_size, 'payload_sha256': digest_json(payload),
            'portable_sha256': digest_json(portable), 'member_count': len(entries), 'payload_bytes': total,
            'entries': entries, 'member_order': order, 'declarations': declarations}


def declaration_changes(left, right, prefix=''):
    if isinstance(left, dict) and isinstance(right, dict):
        result = []
        for key in sorted(left.keys() | right.keys()):
            field = prefix + '/' + key.replace('~', '~0').replace('/', '~1')
            if key not in left:
                result.append({'field': field, 'change': 'added', 'right': right[key]})
            elif key not in right:
                result.append({'field': field, 'change': 'removed', 'left': left[key]})
            else:
                result.extend(declaration_changes(left[key], right[key], field))
        return result
    return [] if left == right else [{'field': prefix or '/', 'change': 'modified', 'left': left, 'right': right}]


def compare(left_path, right_path):
    left, right = read_archive(left_path), read_archive(right_path)
    content, permissions, metadata = [], [], []
    for name in sorted(left['entries'].keys() | right['entries'].keys()):
        a, b = left['entries'].get(name), right['entries'].get(name)
        if a is None or b is None:
            content.append({'path': name, 'change': 'added' if a is None else 'removed', 'left': a, 'right': b})
            continue
        fields = [key for key in ('type', 'size', 'sha256') if a.get(key) != b.get(key)]
        if fields:
            content.append({'path': name, 'change': 'modified', 'fields': fields, 'left': a, 'right': b})
        if a['mode'] != b['mode']:
            permissions.append({'path': name, 'left': a['mode'], 'right': b['mode']})
        fields = [key for key in ('uid', 'gid', 'uname', 'gname', 'mtime', 'pax_headers') if a[key] != b[key]]
        if fields:
            metadata.append({'path': name, 'fields': fields,
                             'left': {key: a[key] for key in fields}, 'right': {key: b[key] for key in fields}})
    declarations = []
    for name in sorted(left['declarations'].keys() | right['declarations'].keys()):
        changes = declaration_changes(left['declarations'].get(name), right['declarations'].get(name))
        if changes:
            declarations.append({'path': name, 'fields': changes})
    exact = left['archive_sha256'] == right['archive_sha256']
    tar_equal = left['tar_sha256'] == right['tar_sha256']
    left_stage = left['declarations'].get('stage.json')
    right_stage = right['declarations'].get('stage.json')
    baseline_keys = ('format', 'stage', 'input_stage', 'target', 'source_commit', 'working_tree_dirty')
    same_baseline = (all(key in left_stage and key in right_stage and left_stage[key] == right_stage[key]
                         for key in baseline_keys) if isinstance(left_stage, dict) and isinstance(right_stage, dict) else None)
    category = ('identical' if exact else 'compression-only' if tar_equal else 'payload-difference' if content
                else 'permission-difference' if permissions else 'tar-container-only')
    hidden = {'entries', 'member_order', 'declarations'}
    def summary(value):
        return {key: item for key, item in value.items() if key not in hidden} | {
            'stage_metadata': value['declarations'].get('stage.json'),
            'successor_recipes': {name: item for name, item in value['declarations'].items() if name != 'stage.json'}}
    return {'format': 1, 'comparison': 'crt-stage-source', 'category': category,
            'exact_archive_equal': exact, 'uncompressed_tar_equal': tar_equal,
            'same_declared_baseline': same_baseline,
            'payload_equal': not content, 'portable_members_equal': not content and not permissions,
            'member_order_equal': left['member_order'] == right['member_order'],
            'left': summary(left), 'right': summary(right),
            'content_changes': content, 'permission_changes': permissions, 'metadata_changes': metadata,
            'declaration_changes': declarations,
            'limits': {'max_members': MAX_MEMBERS, 'max_unpacked_bytes': MAX_UNPACKED_BYTES},
            'approval': 'not-a-release-approval'}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('left', type=Path)
    parser.add_argument('right', type=Path)
    parser.add_argument('--output', type=Path, required=True, help='new JSON evidence file; never overwritten')
    parser.add_argument('--require-identical', action='store_true')
    args = parser.parse_args(argv)
    if args.output.exists() or args.output.is_symlink():
        parser.error('output exists; choose a new evidence path')
    report = compare(args.left, args.right)
    report['tool_sha256'] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x', encoding='utf-8') as stream:
        json.dump(report, stream, indent=2, ensure_ascii=False)
        stream.write('\n')
    print(json.dumps({key: report[key] for key in ('category', 'same_declared_baseline', 'exact_archive_equal', 'payload_equal', 'portable_members_equal')}))
    return 1 if args.require_identical and not report['exact_archive_equal'] else 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, tarfile.TarError, lzma.LZMAError) as error:
        print('compare-stage-sources: ' + str(error), file=sys.stderr)
        sys.exit(2)
