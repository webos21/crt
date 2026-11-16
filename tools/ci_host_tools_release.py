#!/usr/bin/env python3
"""CI driver: assemble, pack, verify and replay the independent host-tools bundle on the native Linux host.

Downloads the pinned public Dev SDK Preview (`.github/ci/host-tools-sdk-pins.json`) with `gh`, checks the release manifest against
its pinned SHA-256 and the archive against that manifest, extracts the SDK with the stdlib `data` filter, and runs
create_host_tools.py (--promotion preview), create_host_tools_release.py pack/verify and replay_host_tools_linux.py. Nothing is
published: the outputs are workflow artifacts for a maintainer to review before a deliberate release.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
import tarfile

HERE = Path(__file__).resolve().parent
PINS = HERE.parent / '.github/ci/host-tools-sdk-pins.json'
VERSION_RE = re.compile(r'^v0\.1\.0-hosttools\.\d{8}\.linux-(x86_64|aarch64)\.\d+$')


def host_key():
    arch = {'x86_64': 'x86_64', 'amd64': 'x86_64', 'aarch64': 'aarch64', 'arm64': 'aarch64'}.get(platform.machine().lower())
    if platform.system() != 'Linux' or arch is None:
        raise SystemExit('this driver runs on native Linux x86_64 or aarch64 only')
    return arch


def version_for(arch, date, revision):
    version = 'v0.1.0-hosttools.{}.linux-{}.{}'.format(date, arch, revision)
    if not VERSION_RE.match(version):
        raise ValueError('invalid host-tools version: ' + version)
    return version


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def pin_for(arch, pins_path=PINS):
    pins = json.loads(Path(pins_path).read_text(encoding='utf-8'))
    entry = pins['hosts'].get('linux-' + arch)
    if not entry or not re.fullmatch(r'[0-9a-f]{64}', entry.get('release_manifest_sha256', '')):
        raise ValueError('no pinned Dev SDK for linux-' + arch)
    return entry


def run(argv, **kwargs):
    print('+', ' '.join(map(str, argv)), flush=True)
    subprocess.run(argv, check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--date', required=True, help='yyyymmdd of the release version')
    parser.add_argument('--revision', required=True, help='revision number of the release version')
    parser.add_argument('--work', type=Path, required=True, help='new work directory')
    parser.add_argument('--allow-dirty', action='store_true', help='rehearsal only')
    args = parser.parse_args()
    arch = host_key()
    entry = pin_for(arch)
    version = version_for(arch, args.date, args.revision)
    work = args.work.resolve(); work.mkdir(parents=True)
    download = work / 'sdk-download'; download.mkdir()
    run(['gh', 'release', 'download', entry['sdk_tag'], '-R', entry['repository'], '-D', str(download), '-p', entry['release_manifest'], '-p', '*-dev.tar.xz'])
    manifest_path = download / entry['release_manifest']
    if sha256(manifest_path) != entry['release_manifest_sha256']:
        raise SystemExit('the SDK release manifest differs from its pin')
    release = json.loads(manifest_path.read_text(encoding='utf-8'))
    package = [a for a in release['assets'] if 'install' in a]
    if len(package) != 1 or package[0]['install'].get('target') != {'os': 'linux', 'arch': arch}:
        raise SystemExit('the pinned release does not describe exactly one native Dev SDK archive')
    archive = download / package[0]['name']
    if archive.stat().st_size != package[0]['size'] or sha256(archive) != package[0]['sha256']:
        raise SystemExit('the SDK archive differs from its release manifest')
    extracted = work / 'sdk'; extracted.mkdir()
    with tarfile.open(archive, 'r:xz') as tar:
        tar.extractall(extracted, filter='data')
    sdk = extracted / package[0]['install']['root']
    bundle, release_out, unpacked, replay = work / 'bundle', work / 'release', work / 'verified', work / 'replay'
    tools = [sys.executable, '-E', '-s']
    assemble = [*tools, str(HERE / 'create_host_tools.py'), '--sdk-root', str(sdk), '--output', str(bundle), '--version', version, '--promotion', 'preview']
    run(assemble)
    pack = [*tools, str(HERE / 'create_host_tools_release.py'), 'pack', '--bundle', str(bundle), '--sdk-release-manifest', str(manifest_path),
            '--output', str(release_out), '--tag', version]
    if args.allow_dirty:
        pack.append('--allow-dirty')
    run(pack)
    record = release_out / 'host-tools-release-record-linux-{}.json'.format(arch)
    run([*tools, str(HERE / 'create_host_tools_release.py'), 'verify', '--archive', str(release_out / 'crt-{}.tar.xz'.format(version)),
         '--record', str(record), '--extract-to', str(unpacked)])
    run([*tools, str(HERE / 'replay_host_tools_linux.py'), '--bundle', str(unpacked / 'crt-host-tools-linux-{}'.format(arch)), '--output', str(replay)])
    print(json.dumps(json.loads(record.read_text(encoding='utf-8'))['consumer_pin'], indent=2))


if __name__ == '__main__':
    main()
