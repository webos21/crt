"""Web target wiring/transaction fixtures; not real WebKit acceptance."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))
import crt_web_stage as adapter


class WebStage(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="web targets spaces ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.sdk = self.make_sdk("existing SDK", "06-web")
        self.work = self.root / "work"

    def make_sdk(self, name, stage):
        sdk = self.root / name
        sdk.mkdir()
        (sdk / "manifest.json").write_text(json.dumps({"stage": stage,
            "target": {"os": "windows", "arch": "x86_64"}}))
        (sdk / "VERSION").write_text("development\n")
        (sdk / "payload").write_text("original")
        return sdk

    def args(self, phase):
        return ["--phase", phase, "--sdk", str(self.sdk), "--work-root", str(self.work),
                "--target-os", "windows", "--target-arch", "x86_64"]

    def test_wrong_stage_or_arch_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "expected 05-ui"):
            adapter.identity(self.sdk, "05-ui", "windows", "x86_64")
        with self.assertRaisesRegex(ValueError, "expected 06-web"):
            adapter.identity(self.sdk, "06-web", "windows", "aarch64")

    def test_test_phase_uses_copy_and_writes_receipt_only_after_success(self):
        def validate(candidate, work, source, env):
            (candidate / "payload").write_text("modified by normalization")
        with patch("build_stage_06_web.build_kind", return_value="native"), \
             patch.object(adapter, "run"), patch("build_stage_05_ui.runtime_env", return_value={}), \
             patch("build_stage_06_web.validate_candidate", side_effect=validate):
            self.assertEqual(0, adapter.main(self.args("test")))
        self.assertEqual("original", (self.sdk / "payload").read_text())
        self.assertTrue(json.loads((self.work / "test-result.json").read_text())["passed"])
        with patch("build_stage_06_web.build_kind", return_value="native"), \
             patch.object(adapter, "run"), patch("build_stage_05_ui.runtime_env", return_value={}), \
             patch("build_stage_06_web.validate_candidate", side_effect=ValueError("TLS failed")):
            with self.assertRaisesRegex(ValueError, "TLS failed"):
                adapter.main(self.args("test"))
        self.assertFalse((self.work / "test-result.json").exists())

    def test_publication_failure_restores_previous_sdk(self):
        candidate = self.make_sdk("candidate", "06-web")
        with patch("build_stage_05_ui.publish_tree", side_effect=OSError("injected failure")):
            with self.assertRaises(OSError):
                adapter.publish(candidate, self.sdk)
        self.assertEqual("original", (self.sdk / "payload").read_text())
        self.assertFalse(list(self.root.glob("existing SDK.previous-*")))

    def test_ordinary_verification_failure_removes_previous_success_receipt(self):
        self.work.mkdir()
        receipt = self.work / "test-result.json"
        receipt.write_text('{"passed": true}')
        with patch("build_stage_06_web.build_kind", return_value="native"), \
             patch.object(adapter, "run", side_effect=ValueError("invalid SDK")):
            with self.assertRaisesRegex(ValueError, "invalid SDK"):
                adapter.main(self.args("test"))
        self.assertFalse(receipt.exists())

    def test_build_failure_preserves_previous_output(self):
        predecessor = self.make_sdk("predecessor", "05-ui")
        recipe = self.root / "06-web.json"
        recipe.write_text(json.dumps({"source": {"archive": "pinned.tar.xz"}}))
        with patch("build_stage_06_web.build_kind", return_value="native"), \
             patch("build_stage_06_web.require_predecessor"), \
             patch.object(adapter, "preflight_host_inputs"), \
             patch.object(adapter, "run", side_effect=subprocess.CalledProcessError(7, ["builder"])):
            with self.assertRaises(subprocess.CalledProcessError):
                adapter.main(self.args("build") + ["--predecessor", str(predecessor), "--recipe", str(recipe)])
        self.assertEqual("original", (self.sdk / "payload").read_text())

    def test_missing_host_prerequisite_fails_before_producer_work_is_created(self):
        predecessor = self.make_sdk("predecessor", "05-ui")
        with patch("build_stage_06_web.build_kind", return_value="native"), \
             patch("build_stage_06_web.require_predecessor"), \
             patch.object(adapter, "preflight_host_inputs", side_effect=ValueError("host Perl missing")), \
             patch.object(adapter, "run") as producer:
            with self.assertRaisesRegex(ValueError, "host Perl missing"):
                adapter.main(self.args("build") + ["--predecessor", str(predecessor), "--recipe", str(self.root / "recipe.json")])
        producer.assert_not_called()
        self.assertFalse(list(self.work.glob("run-*")))

    def test_explicit_retry_is_scoped_and_forwards_existing_isolated_guards(self):
        predecessor = self.make_sdk("predecessor", "05-ui")
        recipe = self.root / "06-web.json"
        recipe.write_text(json.dumps({"source": {"archive": "pinned.tar.xz"}}))
        run_root = self.work / "run-owned"
        marker = run_root / "driver/build/06-web/inputs.json"
        marker.parent.mkdir(parents=True)
        marker.write_text('{}')
        previous = self.root / "previous.tar.xz"
        with patch("build_stage_06_web.build_kind", return_value="native"), \
             patch("build_stage_06_web.require_predecessor"), patch.object(adapter, "preflight_host_inputs"), \
             patch.object(adapter, "run", side_effect=RuntimeError("stop after command")) as run:
            with self.assertRaisesRegex(RuntimeError, "stop after command"):
                adapter.main(self.args("build") + ["--predecessor", str(predecessor), "--recipe", str(recipe),
                    "--retry-run", str(run_root), "--previous-source-asset", str(previous)])
            command = run.call_args.args[0]
            self.assertIn("--reuse-work-root", command)
            self.assertEqual(previous.resolve(), command[command.index("--previous-source-asset") + 1])
            self.assertEqual((run_root / "driver").resolve(), command[command.index("--work-root") + 1])
            with self.assertRaisesRegex(ValueError, "owned direct"):
                adapter.main(self.args("build") + ["--predecessor", str(predecessor), "--recipe", str(recipe),
                    "--retry-run", str(self.root / "outside")])

    def test_archive_has_canonical_root_and_does_not_change_input(self):
        with patch("build_stage_06_web.build_kind", return_value="native"), patch.object(adapter, "run"):
            self.assertEqual(0, adapter.main(self.args("dist") + ["--archive-dir", str(self.root / "archives")]))
        import zipfile
        archive = next((self.root / "archives").glob("*.zip"))
        with zipfile.ZipFile(archive) as opened:
            self.assertIn("06-web/payload", opened.namelist())
        self.assertEqual("original", (self.sdk / "payload").read_text())

    def test_cross_mode_is_not_runtime_acceptance(self):
        with patch("build_stage_06_web.build_kind", return_value="cross-linux-aarch64"):
            with self.assertRaisesRegex(ValueError, "native supported pair"):
                adapter.main(self.args("test"))


if __name__ == "__main__":
    unittest.main()
