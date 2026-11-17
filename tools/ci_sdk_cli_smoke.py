#!/usr/bin/env python3
"""Consumer smoke of a freshly built Dev SDK by the pinned, publisher-signed, *published* CRT-CLI Preview (plan step S2).

Stdlib only; the signature check shells out to `openssl` (the documented independent verifier in docs/security/publisher_signing.md),
so nothing of the producer's or the CLI's own code decides whether the CLI is authentic.

1. Fetch the pinned CLI Preview archive and its detached signature (`.github/ci/sdk-pins.json`); exact size and SHA-256 must match;
   the pinned publisher key must match its SPKI fingerprint and must have signed exactly those archive bytes (RSA-PSS, SHA-256).
2. Extract it, in a private HOME so the SDK store lives inside this run.
3. `sdk install` the produced archive with the release manifest's SHA-256 computed here from the asset directory.
4. For C and C++: `new`, `build`, `test`, `run`, then a warm `build` that must reuse the cache.

Nothing is published and no host SDK/header is exposed to the project: the compiler and linker are the only external tools.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import ssl
import subprocess
import sys
import tarfile
import time
import urllib.request

ROOT = Path(__file__).resolve().parent.parent
EXE = '.exe' if os.name == 'nt' else ''


class SmokeError(Exception):
    pass


def log(message):
    print(message, flush=True)


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, 'rb') as handle:
        for block in iter(lambda: handle.read(1 << 20), b''):
            digest.update(block)
    return digest.hexdigest()


def check_pinned(label, path, size, sha256):
    if Path(path).stat().st_size != size or sha256_file(path) != sha256:
        raise SmokeError('%s does not match its pin (size %d / sha256 %s)' % (label, size, sha256))


def spki_fingerprint(openssl, key):
    pem = subprocess.run([openssl, 'pkey', '-pubin', '-in', str(key), '-outform', 'DER'], capture_output=True)
    if pem.returncode != 0:
        raise SmokeError('openssl cannot read the publisher key: ' + pem.stderr.decode(errors='replace').strip())
    return hashlib.sha256(pem.stdout).hexdigest()


def verify_signature(openssl, key, signature, archive):
    done = subprocess.run([openssl, 'dgst', '-sha256', '-verify', str(key), '-sigopt', 'rsa_padding_mode:pss', '-sigopt', 'rsa_pss_saltlen:32',
                           '-sigopt', 'rsa_mgf1_md:sha256', '-signature', str(signature), str(archive)], capture_output=True, text=True)
    if done.returncode != 0 or 'Verified OK' not in done.stdout:
        raise SmokeError('publisher signature verification failed for %s: %s' % (archive.name, (done.stdout + done.stderr).strip()))


def download(url, destination):
    for attempt in (1, 2, 3):  # bounded; the pins and the signature, not TLS, authenticate the bytes
        try:
            with urllib.request.urlopen(url, timeout=120, context=ssl.create_default_context()) as src, open(destination, 'wb') as dst:
                shutil.copyfileobj(src, dst)
            return
        except OSError as error:
            log('download attempt %d of %s failed: %s' % (attempt, url, error))
            if attempt == 3:
                raise SmokeError('cannot download ' + url)
            time.sleep(5 * attempt)


def fetch_cli(pins, host, work, from_dir):
    pin = pins['hosts'][host]
    archive, signature = work / pin['asset'], work / pin['signature']['name']
    base = '%s%s/' % (pins['cli_release_base'], pin['release'])
    for target, name in ((archive, pin['asset']), (signature, pin['signature']['name'])):
        if from_dir:
            shutil.copyfile(Path(from_dir) / name, target)
        else:
            download(base + name, target)
    check_pinned('the CLI archive', archive, pin['size'], pin['sha256'])
    check_pinned('the CLI signature', signature, pin['signature']['size'], pin['signature']['sha256'])
    openssl = shutil.which('openssl')
    if not openssl:
        raise SmokeError('openssl is required to check the publisher signature')
    key = ROOT / pins['publisher']['key_file']
    if spki_fingerprint(openssl, key) != pins['publisher']['key_spki_sha256']:
        raise SmokeError('the publisher key file does not match the pinned SPKI fingerprint')
    verify_signature(openssl, key, signature, archive)
    target = work / 'cli'
    target.mkdir()
    if not hasattr(tarfile, 'data_filter'):
        raise SmokeError('this Python lacks tarfile extraction filters')
    with tarfile.open(archive) as bundle:
        bundle.extractall(target, filter='data')
    found = [p for p in target.rglob('crt-cli' + EXE) if p.is_file()]
    if len(found) != 1:
        raise SmokeError('expected exactly one crt-cli executable in the archive, found %d' % len(found))
    exe = found[0]
    exe.chmod(exe.stat().st_mode | 0o111)
    return exe, pin


def host_tools(os_name, llvm_bin, os_sdk):
    """The only external inputs: compiler, linker, archiver (and the Windows import libraries)."""
    flags = []
    if os_name == 'macos':
        for flag, name in (('--cc', 'clang'), ('--cxx', 'clang++'), ('--linker', 'ld'), ('--ar', 'ar')):
            done = subprocess.run(['/usr/bin/xcrun', '-f', name], capture_output=True, text=True)
            path = Path(done.stdout.strip())
            if done.returncode or not path.is_absolute() or not path.is_file():
                raise SmokeError('xcrun cannot find ' + name)
            flags += [flag, str(path)]
        return flags
    base = Path(llvm_bin) if llvm_bin else None
    for flag, name in (('--cc', 'clang'), ('--cxx', 'clang++'), ('--linker', 'ld.lld'), ('--ar', 'llvm-ar')):
        path = base / (name + EXE) if base else Path(shutil.which(name) or '')
        if not path.is_file():
            raise SmokeError('missing host tool: %s' % (path or name))
        flags += [flag, str(path.absolute())]  # not resolve(): clang++ and ld.lld are symlinks that dispatch on their own name
    if os_name == 'windows':
        if not os_sdk:
            raise SmokeError('--os-sdk (the um/x64 import-library directory) is required on Windows')
        flags += ['--os-sdk', os_sdk]
    return flags


class Smoke:
    def __init__(self, args):
        self.work = Path(args.work).resolve()
        if self.work.exists() and any(self.work.iterdir()):
            raise SmokeError('work directory must be new or empty: ' + str(self.work))
        for name in ('logs', 'home', 'tmp', 'projects'):
            (self.work / name).mkdir(parents=True, exist_ok=True)
        self.env = dict(os.environ, HOME=str(self.work / 'home'), USERPROFILE=str(self.work / 'home'), TMPDIR=(self.work / 'tmp').as_posix())
        self.steps = []

    def run(self, label, argv, timeout=3600):
        argv = [str(a) for a in argv]
        log('[%s] %s' % (label, ' '.join(argv)))
        began = time.monotonic()
        try:
            done = subprocess.run(argv, env=self.env, capture_output=True, timeout=timeout)
            code, out, err = done.returncode, done.stdout, done.stderr
        except subprocess.TimeoutExpired as expired:
            code, out, err = 124, expired.stdout or b'', expired.stderr or b''
        (self.work / 'logs' / (label + '.stdout')).write_bytes(out)
        (self.work / 'logs' / (label + '.stderr')).write_bytes(err)
        self.steps.append({'label': label, 'exit': code, 'seconds': round(time.monotonic() - began, 1)})
        if code != 0:
            log('\n'.join(err.decode('utf-8', 'replace').splitlines()[-40:]))
            log('stdout tail: ' + out.decode('utf-8', 'replace')[-1500:])
            raise SmokeError('%s exited %d; logs: %s' % (label, code, self.work / 'logs'))
        return out.decode('utf-8', 'replace')

    def cli(self, label, exe, command, project=None, extra=(), timeout=3600):
        argv = [exe] + command.split() + (['--project', project] if project else []) + list(extra) + ['--json']
        reply = json.loads(self.run(label, argv, timeout))
        if not reply.get('ok'):
            raise SmokeError('%s reported failure: %s' % (label, reply.get('data')))
        return reply['data']


def find_assets(assets, os_name, arch):
    manifest = assets / ('release-manifest-dev-%s-%s.json' % (os_name, arch))
    if not manifest.is_file():
        raise SmokeError('no release manifest in ' + str(assets))
    listing = json.loads(manifest.read_text(encoding='utf-8'))
    archives = [a for a in listing['assets'] if 'install' in a]
    if len(archives) != 1 or not (assets / archives[0]['name']).is_file():
        raise SmokeError('the manifest must name exactly one installable archive present in the asset directory')
    return manifest, assets / archives[0]['name'], listing


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--assets', required=True, help='directory with the Dev SDK release assets (release/dev)')
    parser.add_argument('--host', required=True, help='pin key, e.g. linux-x86_64')
    parser.add_argument('--work', required=True)
    parser.add_argument('--pins', default=str(ROOT / '.github/ci/sdk-pins.json'))
    parser.add_argument('--llvm-bin')
    parser.add_argument('--os-sdk')
    parser.add_argument('--cli-dir', help='offline: directory holding the pinned CLI archive and .sig instead of downloading them')
    parser.add_argument('--json', help='write the receipt here')
    args = parser.parse_args(argv)
    pins = json.loads(Path(args.pins).read_text(encoding='utf-8'))
    if pins.get('schema') != 'crt.sdk-ci-pins' or pins.get('format') != 0 or args.host not in pins['hosts']:
        raise SmokeError('unsupported or incomplete pin file for ' + args.host)
    os_name, arch = args.host.split('-', 1)
    manifest, archive, listing = find_assets(Path(args.assets).resolve(), os_name, arch)
    smoke = Smoke(args)
    exe, pin = fetch_cli(pins, args.host, smoke.work, args.cli_dir)
    version = smoke.run('cli-version', [exe, '--version'])
    if pin['version'] not in version:
        raise SmokeError('the pinned CLI reports an unexpected version: ' + version.strip())
    tools = host_tools(os_name, args.llvm_bin, args.os_sdk)
    manifest_sha = sha256_file(manifest)
    install = smoke.cli('sdk-install', exe, 'sdk install', extra=['--archive', archive, '--release-manifest', manifest, '--release-manifest-sha256', manifest_sha])
    content = [a for a in listing['assets'] if 'install' in a][0]['install']['content_sha256']
    if install['identity']['content_sha256'] != content or install['already_installed'] is not False:
        raise SmokeError('the CLI installed content %s, the manifest says %s' % (install['identity']['content_sha256'], content))
    listed = smoke.cli('sdk-list', exe, 'sdk list')
    if [sdk['content12'] for sdk in listed['sdks']] != [content[:12]]:
        raise SmokeError('the SDK store does not hold exactly the installed SDK: %s' % listed['sdks'])
    receipt = {'host': args.host, 'cli': {'release': pin['release'], 'asset': pin['asset'], 'sha256': pin['sha256'], 'signature': 'publisher-verified'},
               'sdk': {'version': listing['version'], 'manifest_sha256': manifest_sha, 'content_sha256': content, 'source_commit': listing['source_commit']},
               'projects': {}}
    for language in ('c', 'cxx'):
        name = 'hello-' + language
        project = smoke.work / 'projects' / name
        smoke.run(name + '-new', [exe, 'new', project, '--language', language])
        cold = smoke.cli(name + '-build', exe, 'build', str(project), ['--sdk', content] + tools)
        smoke.cli(name + '-test', exe, 'test', str(project))
        ran = smoke.run(name + '-run', [exe, 'run', '--project', project])
        warm = smoke.cli(name + '-rebuild', exe, 'build', str(project))
        if cold['cache']['executed'] < 1:
            raise SmokeError('%s: the first build executed nothing' % name)
        if warm['cache']['executed'] != 0 or warm['cache']['reused'] < 1:
            raise SmokeError('%s: the warm rebuild did not reuse the cache: %s' % (name, warm['cache']))
        receipt['projects'][language] = {'cold_cache': cold['cache'], 'warm_cache': warm['cache'], 'run_output': ran.strip()[-200:]}
    receipt['steps'] = smoke.steps
    if args.json:
        Path(args.json).write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    log('OK %s: pinned CLI %s installed the produced SDK %s and built/tested/ran C and C++ (cold and warm)' % (args.host, pin['release'], listing['version']))
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except SmokeError as error:
        print('FAIL: %s' % error, file=sys.stderr)
        sys.exit(1)
