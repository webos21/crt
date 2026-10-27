#!/usr/bin/env python3
"""Native S5 package, negative and candidate-identity evidence; never promote."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

import crt_dev_profile as profile
import crt_sdk_release as collector
from crt_file_ops import copy_tree
from prepare_release_assets import archive_directory
import test_dev_sdk_external as external

ROOT = Path(__file__).resolve().parent.parent


def require_runtime_correspondence(dev, full):
    observed = profile.runtime_digests(dev)
    if not observed or observed != profile.runtime_digests(full):
        raise ValueError('Full reference runtime bytes differ from Dev; supply its actual matching predecessor/Full SDK')
    return len(observed)


def package_equivalence(source, extracted, target_os):
    before, after = external.inventory(source), external.inventory(extracted)
    if before.keys() != after.keys():
        raise ValueError('archive payload paths differ from original SDK')
    aliases = []
    for name in before:
        if before[name] == after[name]:
            continue
        path = source / name
        if (target_os == 'windows' and name in ('lib/libz.so', 'lib/libz.so.1')
                and path.is_symlink() and path.resolve().is_relative_to(source.resolve())
                and path.resolve().name == 'libz.so.1.3.1'
                and after[name].startswith('file:')
                and profile.digest(path) == profile.digest(extracted / name)):
            aliases.append(name)
        else:
            raise ValueError(f'archive changed payload type/content: {name}')
    return {'source_inventory_sha256': external.inventory_digest(before),
            'extracted_inventory_sha256': external.inventory_digest(after),
            'payload_paths': len(before), 'materialized_zip_aliases': sorted(aliases)}


def mutate_negative(sdk, case):
    manifest_path = sdk / 'manifest.json'
    manifest = json.loads(manifest_path.read_text())
    curl = next(item for item in manifest['redistributed_dependencies'] if item['name'] == 'curl')
    if case == 'missing-static-port':
        (sdk / 'lib/libcurl.a').unlink()
    elif case == 'missing-shared-port':
        for name in curl['runtime_artifacts']:
            path = sdk / name
            if profile.is_shared_runtime(path, manifest['target']['os']) or path.is_symlink():
                path.unlink()
    elif case == 'runtime-digest-mismatch':
        runtime = 'bin/libc.dll' if manifest['target']['os'] == 'windows' else (
            'lib/libc.dylib' if manifest['target']['os'] == 'macos' else 'lib/libc.so')
        path = sdk / runtime
        path.write_bytes(path.read_bytes() + b'S5-negative-fixture')
    elif case == 'upper-stage-content':
        path = sdk / 'include/crtweb/s5-negative.h'
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('/* forbidden upper stage */\n')
    elif case == 'missing-port-notice':
        (sdk / curl['notices'][0]).unlink()
    elif case == 'missing-port-declaration':
        manifest['redistributed_dependencies'].remove(curl)
        manifest_path.write_text(json.dumps(manifest))
    elif case == 'ambient-trust-policy':
        path = sdk / profile.SIDECAR
        data = json.loads(path.read_text())
        data['tls_trust']['bundled'] = True
        path.write_text(json.dumps(data))
    else:
        raise ValueError(f'unknown S5 negative: {case}')
    return {'missing-static-port': 'Development SDK missing curl static archive',
            'missing-shared-port': 'Development SDK missing curl shared runtime',
            'runtime-digest-mismatch': 'Development SDK inherited runtime digest mismatch',
            'upper-stage-content': 'Development SDK contains upper-stage content',
            'missing-port-notice': curl['notices'][0],
            'missing-port-declaration': 'Development SDK missing static/shared curl dependency',
            'ambient-trust-policy': 'Development SDK must require consumer-supplied explicit CA trust'}[case]


def stale_full_negative(dev, full, version):
    """Mutate only a disposable Full source identity, preserving Web binding."""
    metadata = full / 'manifest.json'
    data = json.loads(metadata.read_text())
    dev_identity = json.loads((dev / profile.SIDECAR).read_text())
    stale = '0' * 40 if dev_identity.get('producer_source_commit') != '0' * 40 else '1' * 40
    data['source_commit'] = stale
    metadata.write_text(json.dumps(data))
    if data['stage'] == '06-web':
        path = full / 'stages/recipes/06-web.json'
        recipe = json.loads(path.read_text())
        recipe['source']['source_commit'] = stale
        path.write_text(json.dumps(recipe))
    try:
        collector.check_pair(dev, full, version)
    except collector.release.ReleaseError as error:
        if 'same known clean source' not in str(error):
            raise ValueError(f'stale Full rejected at an unrelated gate: {error}') from error
        return {'rejected': True, 'mutation': 'Full source commit only (and matching Web overlay identity)',
                'full_fixture_source_commit': stale, 'diagnostic': str(error)}
    raise ValueError('collector accepted stale Full source identity')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk-root', required=True, type=Path)
    parser.add_argument('--full-sdk', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path, help='fresh evidence directory')
    args = parser.parse_args(argv)
    sdk, full, output = args.sdk_root.resolve(), args.full_sdk.resolve(), args.output.resolve()
    for source in (sdk, full):
        external.require_disjoint(output, [source])
    manifest = json.loads((sdk / 'manifest.json').read_text())
    external.native_target(manifest)
    full_manifest = json.loads((full / 'manifest.json').read_text())
    if (full_manifest.get('stage') not in ('02-cxx', '03-gfx-simple', '04-gfx-media', '05-ui', '06-web')
            or (full / profile.SIDECAR).exists() or full_manifest.get('target') != manifest['target']):
        raise ValueError('matching-target Full cumulative SDK required')
    require_runtime_correspondence(sdk, full)
    output.mkdir(parents=True, exist_ok=False)
    before, full_before = external.inventory(sdk), external.inventory(full)

    def verify(path, log_name, expected_rejection=None):
        command = [sys.executable, str(ROOT / 'tools/verify_dist.py'), '--dist', str(path),
                   '--stage', json.loads((path / 'manifest.json').read_text())['stage']]
        if (path / profile.SIDECAR).exists():
            command += ['--profile', 'dev']
        with (output / log_name).open('w') as log:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=300)
        if (result.returncode == 0) == (expected_rejection is not None):
            raise ValueError(f'expected {"rejection" if expected_rejection else "verification"}: {log_name}')
        if expected_rejection and expected_rejection not in (output / log_name).read_text():
            raise ValueError(f'negative failed at an unrelated gate: {log_name}')
        return {'exit_code': result.returncode, 'log': log_name, 'expected_diagnostic': expected_rejection}

    verify(sdk, 'dev-verify.log')
    verify(full, 'full-verify.log')
    version = (sdk / 'VERSION').read_text().strip()
    if version != 'development':
        collector.release.validate_version(version)
    canonical = output / 'package/dev'
    canonical.parent.mkdir()
    copy_tree(sdk, canonical, symlinks=True)
    archives = output / 'archives'
    archives.mkdir()
    archive = archive_directory(canonical, 'dev', version, manifest['target']['os'], manifest['target']['arch'], archives)
    extracted = output / 'extracted SDK with spaces'
    shutil.unpack_archive(str(archive), extracted)
    extracted /= 'dev'
    package = package_equivalence(sdk, extracted, manifest['target']['os'])
    verify(extracted, 'extracted-verify.log')
    external.main(['--sdk-root', str(extracted), '--output', str(output / 'external')])
    consumer = json.loads((output / 'external/result.json').read_text())
    cases = {}
    for case in ('missing-static-port', 'missing-shared-port', 'runtime-digest-mismatch',
                 'upper-stage-content', 'missing-port-notice', 'missing-port-declaration', 'ambient-trust-policy'):
        with tempfile.TemporaryDirectory(prefix='s5-negative-', dir=output) as raw:
            damaged = Path(raw) / 'dev'
            copy_tree(extracted, damaged, symlinks=True)
            diagnostic = mutate_negative(damaged, case)
            cases[case] = verify(damaged, case + '.log', expected_rejection=diagnostic)
    with tempfile.TemporaryDirectory(prefix='s5-stale-full-', dir=output) as raw:
        stale = Path(raw) / 'full'
        copy_tree(full, stale, symlinks=True)
        cases['stale-full-source'] = stale_full_negative(extracted, stale, version)
    if external.inventory(sdk) != before or external.inventory(full) != full_before:
        raise ValueError('S5 replay changed original Dev/Full payload')
    if external.inventory(extracted) != external.inventory(canonical):
        # Windows ZIP may materialize the two approved zlib aliases.
        package_equivalence(canonical, extracted, manifest['target']['os'])
    data = json.loads((sdk / profile.SIDECAR).read_text())
    result = {'format': 1, 'kind': 'crt-sdk-s5-evidence', 'passed': True, 'target': manifest['target'],
              'validation_source_commit': subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip(),
              'archive': {'path': str(archive), 'bytes': archive.stat().st_size, 'sha256': profile.digest(archive)},
              'dev_manifest_sha256': profile.digest(sdk / 'manifest.json'),
              'dev_profile_sha256': profile.digest(sdk / profile.SIDECAR), 'producer_identity': data,
              'package': package, 'consumer_receipt_sha256': profile.digest(output / 'external/result.json'),
              'link_modes': consumer['consumer']['link_modes'], 'negative_cases': cases,
              'full_reference': {'stage': full_manifest['stage'], 'source_commit': full_manifest.get('source_commit'),
                                 'manifest_sha256': profile.digest(full / 'manifest.json'),
                                 'runtime_bytes_equal': profile.runtime_digests(sdk) == profile.runtime_digests(full)},
              'original_payloads_unchanged': True, 'promotion': 'not-promoted',
              'release_qualified': False, 'cli_adoption': 'not-run'}
    (output / 'handoff.json').write_text(json.dumps(result, indent=2) + '\n')
    print('S5 native package/consumer and eight negatives pass; candidate identity retained, no promotion/adoption')


if __name__ == '__main__':
    main()
