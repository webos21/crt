#!/usr/bin/env python3
"""Producer-side D1-07 fetch cases for the CRT-native CLI; never a CLI runtime dependency.

Generates a throw-away PKI with OpenSSL, serves controlled HTTPS endpoints on loopback with Python's ssl module and
runs `crt-cli fetch` against them from a clean directory with an empty PATH and no SDK. The cases are one positive
pair (pinned metadata, then the artifact) and the rejections the gate lists: wrong CA, expired certificate, host name
mismatch, malformed CA file, tampered metadata, size and digest mismatch, interrupted transfer, timeout, HTTP error,
redirect, plain HTTP and an existing output. Every rejection must publish nothing and leave no staging file. The
endpoint is a controlled loopback server and the digest is pinned by the caller, so this establishes transport
verification and integrity, not publisher authentication.

Usage: test_cli_fetch_https.py --cli <crt-cli executable> --output <fresh evidence dir> [--work <fresh work dir>]
"""
import argparse
import hashlib, http.server, json, os, shutil, socketserver, ssl, subprocess, sys, tempfile, threading, time
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument('--cli', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--work', type=Path, help='fresh work directory (default: a new one under TEMP)')
arguments = parser.parse_args()
cli_source, evidence = arguments.cli.resolve(), arguments.output.resolve()
if (evidence / 'result.json').exists():
    parser.error('the evidence directory already holds a result; use a fresh one')
root = arguments.work.resolve() if arguments.work else Path(tempfile.mkdtemp(prefix='crt-cli-fetch-'))
pki = root / 'pki'
bare = root / 'bare'
out = bare / 'out'
for d in (pki, bare, evidence):
    d.mkdir(parents=True, exist_ok=True)
if out.exists():
    shutil.rmtree(out)
out.mkdir()
cli = bare / cli_source.name
shutil.copy2(cli_source, cli)
OPENSSL = shutil.which('openssl')
if not OPENSSL:
    parser.error('OpenSSL is required as a build-host test tool')


def sh(*args, **kw):
    subprocess.run([OPENSSL, *args], check=True, capture_output=True, cwd=pki, **kw)


def make_ca(name):
    sh('req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-keyout', f'{name}.key', '-out', f'{name}.pem', '-days', '30',
       '-subj', f'/CN=crt-cli test CA {name}', '-addext', 'basicConstraints=critical,CA:TRUE', '-addext', 'keyUsage=keyCertSign,cRLSign')


def make_leaf(name, ca, san, *, not_before=None, not_after=None):
    ext = pki / f'{name}.ext'
    ext.write_text(f'subjectAltName={san}\nbasicConstraints=CA:FALSE\nextendedKeyUsage=serverAuth\nkeyUsage=digitalSignature,keyEncipherment\n')
    sh('req', '-newkey', 'rsa:2048', '-nodes', '-keyout', f'{name}.key', '-out', f'{name}.csr', '-subj', f'/CN={name}')
    extra = []
    if not_before:
        extra += ['-not_before', not_before]
    if not_after:
        extra += ['-not_after', not_after]
    else:
        extra += ['-days', '30']
    sh('x509', '-req', '-in', f'{name}.csr', '-CA', f'{ca}.pem', '-CAkey', f'{ca}.key', '-CAcreateserial', '-out', f'{name}.pem',
       '-extfile', str(ext), *extra)


make_ca('ca-good')
make_ca('ca-other')
make_leaf('leaf-good', 'ca-good', 'DNS:localhost,IP:127.0.0.1')
make_leaf('leaf-expired', 'ca-good', 'DNS:localhost,IP:127.0.0.1', not_before='20200101000000Z', not_after='20200102000000Z')
make_leaf('leaf-wronghost', 'ca-good', 'DNS:other.example')
(pki / 'garbage.pem').write_text('-----BEGIN CERTIFICATE-----\nnot a certificate\n-----END CERTIFICATE-----\n')

META = json.dumps({'name': 'crt-cli test metadata', 'artifact': 'artifact.bin', 'format': 0}, separators=(',', ':')).encode()
META_TAMPERED = META.replace(b'artifact.bin', b'evil-xxx.bin')           # same length, different bytes
ARTIFACT = b''.join(hashlib.sha256(str(i).encode()).digest() for i in range(32768))  # 1 MiB, deterministic
sha = lambda b: hashlib.sha256(b).hexdigest()
assert len(META) == len(META_TAMPERED)


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'
    def log_message(self, *a): pass
    def send(self, body, status=200, length=None):
        self.send_response(status)
        self.send_header('Content-Length', str(len(body) if length is None else length))
        self.send_header('Content-Type', 'application/octet-stream')
        self.end_headers()
    def do_GET(self):
        p = self.path
        try:
            if p == '/meta.json': self.send(META); self.wfile.write(META)
            elif p == '/meta-tampered.json': self.send(META_TAMPERED); self.wfile.write(META_TAMPERED)
            elif p == '/artifact.bin': self.send(ARTIFACT); self.wfile.write(ARTIFACT)
            elif p == '/truncated.bin':
                self.send(ARTIFACT); self.wfile.write(ARTIFACT[:300000]); self.wfile.flush(); self.close_connection = True
                self.connection.close()
            elif p == '/slow.bin':
                self.send(ARTIFACT); self.wfile.write(ARTIFACT[:1000]); self.wfile.flush(); time.sleep(8)
            elif p == '/redirect':
                self.send_response(302); self.send_header('Location', 'http://127.0.0.1:9/meta.json'); self.send_header('Content-Length', '0'); self.end_headers()
            elif p == '/oversize.bin':
                body = ARTIFACT + b'x' * 4096; self.send(body); self.wfile.write(body)
            else:
                self.send(b'no', 404); self.wfile.write(b'no')
        except (ConnectionError, ssl.SSLError, OSError):
            pass


class Server(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
    def handle_error(self, request, client_address): pass


def serve(leaf):
    server = Server(('127.0.0.1', 0), Handler)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.load_cert_chain(pki / f'{leaf}.pem', pki / f'{leaf}.key')
    server.socket = context.wrap_socket(server.socket, server_side=True)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server.server_address[1]


ports = {name: serve(name) for name in ('leaf-good', 'leaf-expired', 'leaf-wronghost')}
env = {'PATH': '', 'TEMP': str(bare), 'TMP': str(bare), 'TMPDIR': str(bare)}
if os.name == 'nt':
    env['SystemRoot'] = os.environ['SystemRoot']
results = []


def case(label, args, *, expect_exit, expect_code=None, expect_published=False, output='x.bin', cwd=None):
    target = out / output
    argv = [str(cli), 'fetch', '--json', *args]
    started = time.time()
    r = subprocess.run(argv, cwd=bare, env=env, capture_output=True, timeout=120, stdin=subprocess.DEVNULL)
    text = r.stdout.decode('utf-8', 'replace')
    reply = json.loads(text) if text.strip().startswith('{') else {}
    data = reply.get('data', {})
    leftovers = sorted(p.name for p in out.iterdir() if '.part-' in p.name)
    ok = r.returncode == expect_exit
    if expect_code is not None:
        ok = ok and data.get('code') == expect_code
    published = target.exists()
    ok = ok and published == expect_published and not leftovers and data.get('published', expect_published) == expect_published
    row = dict(label=label, ok=ok, exit=r.returncode, code=data.get('code'), stage=data.get('stage'), published=published,
               leftovers=leftovers, seconds=round(time.time() - started, 1), curl_code=data.get('curl_code'),
               http_status=data.get('http_status'), message=data.get('message'), argv_tail=[a for a in args if not a.startswith('C:')])
    if published:
        row['output_sha256'] = sha(target.read_bytes())
    results.append(row)
    print(('PASS ' if ok else 'FAIL ') + label, r.returncode, data.get('code'), flush=True)
    (evidence / f'{label}.stdout').write_bytes(r.stdout)
    (evidence / f'{label}.stderr').write_bytes(r.stderr)
    return reply


def url(leaf, path):
    return f'https://localhost:{ports[leaf]}{path}'


good = str(pki / 'ca-good.pem')
common = lambda path, digest, size, ca=good, leaf='leaf-good', output='x.bin', extra=(): [
    '--url', url(leaf, path), '--ca-file', ca, '--output', str(out / output), '--sha256', digest, '--size', str(size), *extra]

# Positive: pinned metadata, then the artifact it names.
case('positive-metadata', common('/meta.json', sha(META), len(META), output='meta.json'), expect_exit=0, expect_published=True, output='meta.json')
case('positive-artifact', common('/artifact.bin', sha(ARTIFACT), len(ARTIFACT), output='artifact.bin'), expect_exit=0, expect_published=True, output='artifact.bin')
# Trust and certificate failures: fail closed, nothing published.
case('neg-wrong-ca', common('/meta.json', sha(META), len(META), ca=str(pki / 'ca-other.pem')), expect_exit=1, expect_code='tls')
case('neg-expired-certificate', common('/meta.json', sha(META), len(META), leaf='leaf-expired'), expect_exit=1, expect_code='tls')
case('neg-hostname-mismatch', common('/meta.json', sha(META), len(META), leaf='leaf-wronghost'), expect_exit=1, expect_code='tls')
case('neg-malformed-ca-file', common('/meta.json', sha(META), len(META), ca=str(pki / 'garbage.pem')), expect_exit=1, expect_code='tls')
case('neg-missing-ca-file', common('/meta.json', sha(META), len(META), ca=str(pki / 'absent.pem')), expect_exit=2)
# Metadata/artifact integrity.
case('neg-tampered-metadata', common('/meta-tampered.json', sha(META), len(META)), expect_exit=1, expect_code='digest_mismatch')
case('neg-size-larger-than-declared', common('/oversize.bin', sha(ARTIFACT), len(ARTIFACT)), expect_exit=1, expect_code='too_large')
case('neg-size-smaller-than-declared', common('/artifact.bin', sha(ARTIFACT), len(ARTIFACT) + 10), expect_exit=1, expect_code='size_mismatch')
case('neg-wrong-digest-artifact', common('/artifact.bin', '0' * 64, len(ARTIFACT)), expect_exit=1, expect_code='digest_mismatch')
# Transfer failures.
case('neg-interrupted-download', common('/truncated.bin', sha(ARTIFACT), len(ARTIFACT)), expect_exit=1, expect_code='interrupted')
case('neg-timeout', common('/slow.bin', sha(ARTIFACT), len(ARTIFACT), extra=('--timeout', '2')), expect_exit=1, expect_code='timeout')
case('neg-http-404', common('/missing', sha(META), len(META)), expect_exit=1, expect_code='http_status')
case('neg-redirect-not-followed', common('/redirect', sha(META), len(META)), expect_exit=1, expect_code='http_status')
case('neg-plain-http-url', ['--url', 'http://localhost:1/m.json', '--ca-file', good, '--output', str(out / 'x.bin'), '--sha256', sha(META), '--size', str(len(META))], expect_exit=2)
# No overwrite: the earlier artifact stays byte-identical.
before = sha((out / 'artifact.bin').read_bytes())
case('neg-existing-output-untouched', common('/artifact.bin', sha(ARTIFACT), len(ARTIFACT), output='artifact.bin'), expect_exit=2, expect_published=True, output='artifact.bin')
after = sha((out / 'artifact.bin').read_bytes())
passed = all(r['ok'] for r in results) and before == after == sha(ARTIFACT)
summary = dict(passed=passed, cli=str(cli_source), cli_sha256=sha(cli_source.read_bytes()), cases=results,
               pki=dict(ca_good_sha256=sha((pki / 'ca-good.pem').read_bytes()), openssl=subprocess.run([OPENSSL, 'version'], capture_output=True, text=True).stdout.strip()),
               metadata_sha256=sha(META), artifact_sha256=sha(ARTIFACT), host='127.0.0.1 (controlled loopback endpoint)', work=str(root),
               directory_after=sorted(p.name for p in out.iterdir()))
(evidence / 'result.json').write_text(json.dumps(summary, indent=2))
print('passed' if passed else 'FAILED', len(results), 'cases')
sys.exit(0 if passed else 1)
