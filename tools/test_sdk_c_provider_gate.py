"""The Dev collector must reject a stale/partial C-policy qualification."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))
import crt_sdk_release as release


class ProviderGate(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.contract = self.root / "policy.json"
        self.contract.write_text("{}")
        self.original_mkdtemp = tempfile.mkdtemp
        self.document = {"target": {"os": "linux", "arch": "x86_64"}, "sdk_content_sha256": "a" * 64}

    def report(self, command):
        output = Path(command[command.index("--output") + 1])
        output.mkdir()
        result = {"passed": True, "sdk_unchanged": True, "target": self.document["target"],
                  "sdk_content_sha256": "a" * 64, "contract_sha256": release.dev.digest(self.contract),
                  "cases": [{"name": name, "passed": True, "executions": [{"loader": "PT_INTERP"}]}
                            for name in ("static-sdk-static", "shared-sdk-static", "shared-sdk-shared",
                                         "shared-project-dso", "shared-explicit-provider")]}
        result.update(self.override)
        (output / "result.json").write_text(json.dumps(result))

    def test_matching_complete_report_passes(self):
        self.override = {}
        with patch.object(release, "run", side_effect=self.report) as run:
            release.check_linux_c_provider(self.root, self.contract, self.document)
        self.assertEqual(run.call_count, 1)

    def test_other_hosts_have_separate_qualification(self):
        for os_name in ("macos", "windows"):
            with patch.object(release, "run") as run:
                release.check_linux_c_provider(self.root, self.contract, {"target": {"os": os_name}})
                run.assert_not_called()
        for os_name in ("linux", "macos"):
            with patch.object(release, "run") as run:
                release.check_windows_c_provider(self.root, self.contract, {"target": {"os": os_name}})
                run.assert_not_called()

    def windows_report(self, command):
        output = Path(command[command.index("--output") + 1])
        output.mkdir()
        target = {"os": "windows", "arch": "x86_64"}
        result = {"passed": True, "sdk_unchanged": True, "target": target, "sdk_content_sha256": "a" * 64,
                  "contract_sha256": release.dev.digest(self.contract),
                  "cases": [{"name": name, "passed": True} for name in
                            ("project-dll-build", "static-runtime", "shared-runtime", "shared-project-dll")]}
        result.update(self.override)
        (output / "result.json").write_text(json.dumps(result))

    def test_windows_gate_accepts_only_a_complete_matching_report(self):
        document = {"target": {"os": "windows", "arch": "x86_64"}, "sdk_content_sha256": "a" * 64}
        with patch.dict(release.os.environ, {"CRT_WINDOWS_SDK_LIBPATH": "C:/um/x64"}):
            self.override = {}
            with patch.object(release, "run", side_effect=self.windows_report):
                release.check_windows_c_provider(self.root, self.contract, document)
            for override in ({"passed": False}, {"cases": [{"passed": True}] * 4}, {"sdk_unchanged": False},
                             {"sdk_content_sha256": "b" * 64}, {"contract_sha256": "c" * 64},
                             {"cases": [{"name": "static-runtime", "passed": True}] * 4}):
                self.override = override
                with patch.object(release.tempfile, "mkdtemp", side_effect=lambda **_: self.original_mkdtemp(dir=self.root)),                      patch.object(release, "run", side_effect=self.windows_report),                      self.assertRaises(release.release.ReleaseError):
                    release.check_windows_c_provider(self.root, self.contract, document)

    def test_windows_gate_needs_the_os_sdk(self):
        document = {"target": {"os": "windows", "arch": "x86_64"}}
        with patch.dict(release.os.environ, {}, clear=False) as env:
            env.pop("CRT_WINDOWS_SDK_LIBPATH", None)
            with self.assertRaises(release.release.ReleaseError):
                release.check_windows_c_provider(self.root, self.contract, document)

    def test_failed_partial_changed_sdk_and_wrong_input_never_pass(self):
        for override in ({"passed": False}, {"cases": [{"passed": True}] * 4},
                         {"sdk_unchanged": False}, {"sdk_content_sha256": "b" * 64},
                         {"contract_sha256": "c" * 64}, {"cases": [{"passed": False}] * 5},
                         {"target": {"os": "linux", "arch": "aarch64"}},
                         {"cases": [{"name": "static-sdk-static", "passed": True}] * 5}):
            self.override = override
            # Confine the deliberately retained failure evidence to this fixture.
            with patch.object(release.tempfile, "mkdtemp", side_effect=lambda **_: self.original_mkdtemp(dir=self.root)), \
                 patch.object(release, "run", side_effect=self.report), \
                 self.assertRaises(release.release.ReleaseError):
                release.check_linux_c_provider(self.root, self.contract, self.document)


if __name__ == "__main__":
    unittest.main()
