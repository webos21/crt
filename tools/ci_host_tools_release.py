#!/usr/bin/env python3
"""CI driver: assemble, pack, verify and replay the independent host-tools bundle on the native host.

Runs on native Linux (x86_64, aarch64), macOS/arm64 and Windows (x64, arm64). Downloads the pinned public Dev SDK Preview
(`.github/ci/host-tools-sdk-pins.json`) with `gh`, checks the release manifest against its pinned SHA-256 and the archive against
that manifest, extracts the SDK with a safe extractor (tar.xz or zip, whichever the release manifest declares), and runs
create_host_tools.py (--promotion preview), create_host_tools_release.py pack/verify and the host's replay driver. Nothing is
published: the outputs are workflow artifacts for a maintainer to review before a deliberate release.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import re
import subprocess
import sys
import tarfile
import zipfile

HERE = Path(__file__).resolve().parent
PINS = HERE.parent / '.github/ci/host-tools-sdk-pins.json'
HOSTS = ('linux-x86_64', 'linux-aarch64', 'macos-aarch64', 'windows-x86_64', 'windows-aarch64')
VERSION_RE = re.compile(r'^v0\.1\.0-hosttools\.\d{8}\.(linux-x86_64|linux-aarch64|macos-aarch64|windows-x86_64|windows-aarch64)\.\d+$')
REPLAYS = {'linux': 'replay_host_tools_linux.py', 'macos': 'replay_host_tools_macos.py', 'windows': 'replay_host_tools_windows.py'}


def host_key():
    os_name = {'Linux': 'linux', 'Darwin': 'macos', 'Windows': 'windows'}.get(platform.system())
    arch = {'x86_64': 'x86_64', 'amd64': 'x86_64', 'aarch64': 'aarch64', 'arm64': 'aarch64'}.get(platform.machine().lower())
    key = '{}-{}'.format(os_name, arch)
    if key not in HOSTS:
        raise SystemExit('this driver runs on native ' + ', '.join(HOSTS) + ' only, not ' + key)
    return key


def version_for(key, date, revision):
    if key not in HOSTS:
        raise ValueError('unsupported host-tools host: ' + str(key))
    version = 'v0.1.0-hosttools.{}.{}.{}'.format(date, key, revision)
    if not VERSION_RE.match(version):
        raise ValueError('invalid host-tools version: ' + version)
    return version


def sha256(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def pin_for(key, pins_path=PINS):
    pins = json.loads(Path(pins_path).read_text(encoding='utf-8'))
    entry = pins['hosts'].get(key)
    if not entry or not re.fullmatch(r'[0-9a-f]{64}', entry.get('release_manifest_sha256', '')):
        raise ValueError('no pinned Dev SDK for ' + key)
    return entry


def safe_member(name):
    """A relative archive member name that stays inside the extraction root on every host."""
    posix = PurePosixPath(name.replace('\\', '/'))
    if not name or posix.is_absolute() or '..' in posix.parts or re.match(r'^[A-Za-z]:', name) or name.startswith(('/', '\\')):
        raise ValueError('unsafe archive member: ' + name)
    return posix


def extract_sdk(archive, archive_format, destination):
    """Extract the pinned SDK archive; refuses members that escape the root. Windows SDKs are zips, the others tar.xz."""
    destination = Path(destination)
    if archive_format == 'tar.xz':
        with tarfile.open(archive, 'r:xz') as tar:
            for member in tar.getmembers():
                safe_member(member.name)
            tar.extractall(destination, filter='data')
    elif archive_format == 'zip':
        with zipfile.ZipFile(archive) as bundle:
            for info in bundle.infolist():
                safe_member(info.filename)
                if (info.external_attr >> 16) & 0o170000 == 0o120000:
                    raise ValueError('SDK zip contains a symlink member: ' + info.filename)
            bundle.extractall(destination)
    else:
        raise ValueError('unsupported SDK archive format: ' + str(archive_format))


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
    key = host_key()
    os_name, arch = key.split('-', 1)
    entry = pin_for(key)
    version = version_for(key, args.date, args.revision)
    work = args.work.resolve(); work.mkdir(parents=True)
    download = work / 'sdk-download'; download.mkdir()
    run(['gh', 'release', 'download', entry['sdk_tag'], '-R', entry['repository'], '-D', str(download), '-p', entry['release_manifest']])
    manifest_path = download / entry['release_manifest']
    if sha256(manifest_path) != entry['release_manifest_sha256']:
        raise SystemExit('the SDK release manifest differs from its pin')
    release = json.loads(manifest_path.read_text(encoding='utf-8'))
    package = [a for a in release['assets'] if 'install' in a]
    if len(package) != 1 or package[0]['install'].get('target') != {'os': os_name, 'arch': arch}:
        raise SystemExit('the pinned release does not describe exactly one native Dev SDK archive')
    run(['gh', 'release', 'download', entry['sdk_tag'], '-R', entry['repository'], '-D', str(download), '-p', package[0]['name']])
    archive = download / package[0]['name']
    if archive.stat().st_size != package[0]['size'] or sha256(archive) != package[0]['sha256']:
        raise SystemExit('the SDK archive differs from its release manifest')
    extracted = work / 'sdk'; extracted.mkdir()
    extract_sdk(archive, package[0]['install']['archive_format'], extracted)
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
    record = release_out / 'host-tools-release-record-{}.json'.format(key)
    run([*tools, str(HERE / 'create_host_tools_release.py'), 'verify', '--archive', str(release_out / 'crt-{}.tar.xz'.format(version)),
         '--record', str(record), '--extract-to', str(unpacked)])
    run([*tools, str(HERE / REPLAYS[os_name]), '--bundle', str(unpacked / 'crt-host-tools-{}'.format(key)), '--output', str(replay)])
    print(json.dumps(json.loads(record.read_text(encoding='utf-8'))['consumer_pin'], indent=2))


if __name__ == '__main__':
    main()
