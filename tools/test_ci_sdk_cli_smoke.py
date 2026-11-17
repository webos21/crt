"""Tests of the S2 consumer-smoke driver's trust checks and of its pin file (stdlib + the documented `openssl` verifier)."""
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

import ci_sdk_cli_smoke as smoke

ROOT = Path(__file__).resolve().parent.parent
PINS = json.loads((ROOT / '.github/ci/sdk-pins.json').read_text(encoding='utf-8'))
OPENSSL = shutil.which('openssl')


class PinFileTest(unittest.TestCase):
    def test_every_supported_host_pins_a_signed_published_cli(self):
        self.assertEqual(sorted(PINS['hosts']), ['linux-aarch64', 'linux-x86_64', 'macos-aarch64', 'windows-x86_64'])
        for host, pin in PINS['hosts'].items():
            self.assertRegex(pin['sha256'], r'^[0-9a-f]{64}$', host)
            self.assertRegex(pin['signature']['sha256'], r'^[0-9a-f]{64}$', host)
            self.assertEqual(pin['signature']['size'], 256, host)
            self.assertEqual(pin['signature']['name'], pin['asset'] + '.sig', host)
            self.assertRegex(pin['release'], r'^v[0-9]+\.[0-9]+\.[0-9]+-preview\.[0-9.]+[A-Za-z0-9._-]*$', host)
            self.assertNotIn('latest', pin['release'])

    def test_the_pinned_key_fingerprint_belongs_to_the_committed_key(self):
        if not OPENSSL:
            self.skipTest('openssl not available')
        self.assertEqual(smoke.spki_fingerprint(OPENSSL, ROOT / PINS['publisher']['key_file']), PINS['publisher']['key_spki_sha256'])

    def test_downloads_come_from_the_cli_release_pages_only(self):
        self.assertEqual(PINS['cli_release_base'], 'https://github.com/webos21/crt-cli/releases/download/')


@unittest.skipUnless(OPENSSL, 'openssl not available')
class SignatureTest(unittest.TestCase):
    def setUp(self):
        self.dir = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.dir, True)
        for argv in (['genpkey', '-algorithm', 'RSA', '-pkeyopt', 'rsa_keygen_bits:2048', '-out', 'k.pem'], ['pkey', '-in', 'k.pem', '-pubout', '-out', 'k.pub']):
            subprocess.run([OPENSSL] + argv, cwd=self.dir, check=True, capture_output=True)
        (self.dir / 'a.bin').write_bytes(b'archive bytes')
        subprocess.run([OPENSSL, 'dgst', '-sha256', '-sign', 'k.pem', '-sigopt', 'rsa_padding_mode:pss', '-sigopt', 'rsa_pss_saltlen:32', '-sigopt', 'rsa_mgf1_md:sha256',
                        '-out', 'a.sig', 'a.bin'], cwd=self.dir, check=True, capture_output=True)

    def test_the_signed_bytes_verify(self):
        smoke.verify_signature(OPENSSL, self.dir / 'k.pub', self.dir / 'a.sig', self.dir / 'a.bin')

    def test_one_changed_byte_fails(self):
        (self.dir / 'a.bin').write_bytes(b'archive byteS')
        with self.assertRaises(smoke.SmokeError):
            smoke.verify_signature(OPENSSL, self.dir / 'k.pub', self.dir / 'a.sig', self.dir / 'a.bin')

    def test_another_key_fails(self):
        subprocess.run([OPENSSL, 'genpkey', '-algorithm', 'RSA', '-pkeyopt', 'rsa_keygen_bits:2048', '-out', 'o.pem'], cwd=self.dir, check=True, capture_output=True)
        subprocess.run([OPENSSL, 'pkey', '-in', 'o.pem', '-pubout', '-out', 'o.pub'], cwd=self.dir, check=True, capture_output=True)
        with self.assertRaises(smoke.SmokeError):
            smoke.verify_signature(OPENSSL, self.dir / 'o.pub', self.dir / 'a.sig', self.dir / 'a.bin')

    def test_a_pkcs1_signature_is_not_accepted_as_pss(self):
        subprocess.run([OPENSSL, 'dgst', '-sha256', '-sign', 'k.pem', '-out', 'p1.sig', 'a.bin'], cwd=self.dir, check=True, capture_output=True)
        with self.assertRaises(smoke.SmokeError):
            smoke.verify_signature(OPENSSL, self.dir / 'k.pub', self.dir / 'p1.sig', self.dir / 'a.bin')


class PinnedBytesTest(unittest.TestCase):
    def test_size_and_digest_must_both_match(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'x'
            path.write_bytes(b'abc')
            digest = smoke.sha256_file(path)
            smoke.check_pinned('x', path, 3, digest)
            with self.assertRaises(smoke.SmokeError):
                smoke.check_pinned('x', path, 4, digest)
            with self.assertRaises(smoke.SmokeError):
                smoke.check_pinned('x', path, 3, '0' * 64)


if __name__ == '__main__':
    unittest.main()
