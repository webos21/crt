"""Publisher gate tests with real OpenSSL signatures and a local GitHub double."""

import copy
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import release_signing as signing


ROOT = Path(__file__).resolve().parents[1]
COMMIT = "b" * 40
TAG = "v0.0.1-preview.20261124.linux.x64.1"


class WorkflowRules(unittest.TestCase):
    def test_only_separate_approved_job_may_finalize(self):
        is_sdk = (ROOT / ".github/workflows/sdk-release.yml").exists()
        stage = (ROOT / ".github/workflows" / ("sdk-release.yml" if is_sdk else "release.yml")).read_text()
        final = (ROOT / ".github/workflows/finalize-release.yml").read_text()
        self.assertIn("if: github.ref == 'refs/heads/main'", stage)
        self.assertIn("if: github.ref == 'refs/heads/main'", final)
        self.assertIn("if: ${{ !inputs.dry_run }}", stage)
        self.assertIn("release-stage-receipt-${{ github.run_attempt }}", stage)
        self.assertNotIn("--draft=false", stage)
        self.assertNotIn("publish \"", stage)
        plan, finalize = final.split("\n  finalize:\n")
        self.assertNotIn("contents: write", plan)
        self.assertIn("needs: plan", finalize)
        self.assertIn("environment: release", finalize)
        self.assertIn("contents: write", finalize)
        self.assertIn("group: release-publication", stage)
        self.assertIn("group: release-publication", finalize)
        self.assertIn("--key " + ("docs/security/" if is_sdk else ".github/ci/") + "crt-release-publisher.pub", finalize)

    @unittest.skipUnless(shutil.which("openssl"), "openssl required")
    def test_the_committed_key_matches_the_verifier_pin(self):
        key = ROOT / "docs/security/crt-release-publisher.pub"
        if not key.exists():
            key = ROOT / ".github/ci/crt-release-publisher.pub"
        der = signing.run("openssl", "pkey", "-pubin", "-in", key, "-outform", "DER")
        self.assertEqual(hashlib.sha256(der).hexdigest(), signing.PUBLISHER_SPKI)


@unittest.skipUnless(shutil.which("openssl"), "openssl required")
class FinalizeFlow(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(temp.cleanup)
        root = Path(temp.name)
        cls.private, cls.public = root / "test.key", root / "test.pub"
        subprocess.run(["openssl", "genpkey", "-algorithm", "RSA", "-pkeyopt", "rsa_keygen_bits:2048",
                        "-out", cls.private], capture_output=True, check=True)
        signing.run("openssl", "pkey", "-in", cls.private, "-pubout", "-out", cls.public)
        der = signing.run("openssl", "pkey", "-pubin", "-in", cls.public, "-outform", "DER")
        cls.pin = hashlib.sha256(der).hexdigest()

    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.work = Path(tmp.name)
        self.repo = "owner/crt"
        self.run_meta = {"id": 123, "event": "workflow_dispatch", "head_branch": "main",
                         "head_repository": {"full_name": self.repo}, "path": ".github/workflows/sdk-release.yml@main",
                         "status": "completed", "conclusion": "success", "run_attempt": 2}
        self.release = {"id": 30, "tag_name": TAG, "target_commitish": COMMIT, "draft": True,
                        "prerelease": True, "name": "Preview", "body": "Reviewed notes"}
        self.assets = {}
        self.upload("preview.tar.xz", b"exact archive")
        self.upload("SHA256SUMS-dev", b"verified checksums")
        self.receipt = None
        self.reviewers = True
        self.refs = []
        self.patch_count = 0
        self.downloads = []
        self.during_draft_download = lambda: None
        self.addCleanup(patch.stopall)
        patch.object(signing, "gh", side_effect=self.gh).start()
        patch.object(signing, "PUBLISHER_SPKI", self.pin).start()
        self.receipt = {"format": 1, "repo": self.repo, "run_id": 123, "attempt": 2,
                        "release_id": 30, "draft": signing.release_view(self.repo, 30)}
        for name in self.receipt["draft"]["assets"]:
            self.sign(name)

    def upload(self, name, data, asset_id=None):
        old = self.assets.get(name)
        self.assets[name] = (asset_id or (old[0] if old else len(self.assets) + 1), data)

    def sign(self, name, pss=signing.PSS):
        payload, sig = self.work / "test.payload", self.work / "test.sig"
        payload.write_bytes(self.assets[name][1])
        signing.run("openssl", "dgst", "-sha256", "-sign", self.private, *pss, "-out", sig, payload)
        self.upload(name + ".sig", sig.read_bytes())

    def gh(self, *args):
        def encoded(data):
            return json.dumps(data).encode()
        if args[0] == "api":
            endpoint = args[-1]
            self.assertTrue(endpoint.startswith(f"repos/{self.repo}/"))
            if endpoint.endswith("/environments/release"):
                return encoded({"protection_rules": [{"type": "required_reviewers",
                                                       "reviewers": [{}] if self.reviewers else []}]})
            if endpoint.endswith("/actions/runs/123"):
                return encoded(self.run_meta)
            if endpoint.endswith("/releases/30/assets?per_page=100"):
                self.assertEqual(args[1:3], ("--paginate", "--slurp"))
                items = [{"id": ident, "name": name, "state": "uploaded", "size": len(data),
                          "digest": "sha256:" + hashlib.sha256(data).hexdigest()}
                         for name, (ident, data) in self.assets.items()]
                return encoded([items[:1], items[1:]])
            if "/git/matching-refs/" in endpoint:
                return encoded(self.refs)
            if endpoint.endswith("/releases/30"):
                if "PATCH" in args:
                    self.assertEqual(args[1:-1], ("--method", "PATCH", "-F", "draft=false", "-f", "make_latest=false"))
                    self.patch_count += 1
                    self.release["draft"] = False
                    self.refs = [{"ref": f"refs/tags/{TAG}", "object": {"type": "commit", "sha": COMMIT}}]
                return encoded(self.release)
        elif args[:2] == ("run", "download"):
            self.assertEqual(args[2:7], (123, "--repo", self.repo, "--name", "release-stage-receipt-2"))
            if self.receipt is None:
                raise signing.SigningError("no receipt artifact")
            directory = Path(args[-1])
            directory.mkdir(exist_ok=True)
            (directory / signing.RECEIPT_NAME).write_bytes(encoded(self.receipt))
            return b""
        elif args[:2] == ("release", "download"):
            self.assertEqual(args[2], TAG)
            directory = Path(args[-1])
            self.downloads.append(directory.name)
            for name, (_, data) in self.assets.items():
                (directory / name).write_bytes(data)
            if self.release["draft"]:
                self.during_draft_download()
            return b""
        raise AssertionError(args)

    def refuse(self, diagnostic):
        with self.assertRaisesRegex(signing.SigningError, diagnostic):
            signing.finalize(self.repo, "123", self.public, self.work)
        self.assertTrue(self.release["draft"])
        self.assertEqual(self.patch_count, 0)

    def test_an_official_release_passes_the_same_gate_and_stays_non_prerelease(self):
        self.release["prerelease"] = False
        self.receipt["draft"] = signing.release_view(self.repo, 30)
        self.receipt["draft"]["assets"] = {name: item for name, item in self.receipt["draft"]["assets"].items()
                                           if not name.endswith(".sig")}
        signing.finalize(self.repo, "123", self.public, self.work)
        self.assertFalse(self.release["draft"])
        self.assertFalse(self.release["prerelease"])
        self.assertEqual(self.patch_count, 1)

    def test_a_flag_changed_after_staging_is_refused(self):
        self.release["prerelease"] = False
        self.refuse("staged release metadata or payload identity changed")

    def test_signed_draft_published_and_downloaded_once(self):
        signing.finalize(self.repo, "123", self.public, self.work)
        self.assertFalse(self.release["draft"])
        self.assertEqual(self.patch_count, 1)
        self.assertEqual(self.downloads, ["signed", "published"])
        retry = self.work / "retry"
        retry.mkdir()
        with self.assertRaisesRegex(signing.SigningError, "already public"):
            signing.finalize(self.repo, "123", self.public, retry)
        self.assertEqual(self.patch_count, 1)

    def test_missing_signature(self):
        del self.assets["SHA256SUMS-dev.sig"]
        self.refuse("one .sig for every asset")

    def test_extra_asset(self):
        self.upload("unreviewed.zip", b"extra")
        self.refuse("exactly the staged assets")

    def test_wrong_signature(self):
        self.upload("SHA256SUMS-dev.sig", self.assets["preview.tar.xz.sig"][1])
        self.refuse("signature verification failed")

    def test_wrong_padding(self):
        self.sign("preview.tar.xz", pss=())
        self.refuse("signature verification failed")

    def test_wrong_key_pin(self):
        with patch.object(signing, "PUBLISHER_SPKI", "0" * 64):
            self.refuse("fingerprint differs")

    def test_changed_metadata_bytes_even_if_signed(self):
        self.upload("SHA256SUMS-dev", b"changed")
        self.sign("SHA256SUMS-dev")
        self.refuse("payload identity changed")

    def test_reuploaded_payload_even_if_same_bytes(self):
        self.upload("preview.tar.xz", b"exact archive", asset_id=42)
        self.refuse("payload identity changed")

    def test_changed_release_notes(self):
        self.release["body"] = "unreviewed notes"
        self.refuse("metadata or payload identity changed")

    def test_no_required_reviewer(self):
        self.reviewers = False
        self.refuse("must have required reviewers")

    def test_non_main_or_failed_or_wrong_workflow_run(self):
        original = copy.deepcopy(self.run_meta)
        for changes in ({"head_branch": "topic"}, {"conclusion": "failure"}, {"event": "push"},
                        {"head_repository": {"full_name": "fork/crt"}},
                        {"path": ".github/workflows/ci.yml@main"}):
            with self.subTest(changes=changes):
                self.run_meta = {**original, **changes}
                self.refuse("successful manual release run")

    def test_rehearsal_has_no_finalization_receipt(self):
        self.receipt = None
        self.refuse("no receipt artifact")

    def test_stale_receipt(self):
        self.receipt["attempt"] = 1
        self.refuse("receipt is for another run")

    def test_moved_tag(self):
        self.refs = [{"ref": f"refs/tags/{TAG}", "object": {"type": "commit", "sha": "9" * 40}}]
        self.refuse("tag does not point")

    def test_concurrent_edit_is_detected_at_last_read(self):
        self.during_draft_download = lambda: self.upload("extra", b"concurrent mutation")
        self.refuse("changed during verification")


if __name__ == "__main__":
    unittest.main()
