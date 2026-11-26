import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).parent))
import compare_dev_sdk_elf_abi as tool

STATE = {"target": {"os": "linux", "arch": "aarch64"}, "target_triple": "aarch64-linux-gnu", "binaries": {}, "aliases": {}}


class BaselineVerification(unittest.TestCase):
    def run_main(self, *extra):
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            (root / "old").mkdir(); (root / "new").mkdir()
            out = root / "evidence"
            argv = ["compare", "--baseline", str(root / "old"), "--candidate", str(root / "new"), "--output", str(out), *extra]
            with mock.patch.object(sys, "argv", argv), mock.patch.object(tool.shutil, "which", lambda name: "/usr/bin/" + name), \
                    mock.patch.object(tool.subprocess, "run") as run, mock.patch.object(tool, "capture", lambda *args: json.loads(json.dumps(STATE))):
                tool.main()
            verified = [Path(call.args[0][3]).name for call in run.call_args_list]
            return verified, json.loads((out / "verification.json").read_text())

    def test_both_distributions_are_verified_by_default(self):
        verified, record = self.run_main()
        self.assertEqual(["old", "new"], verified)
        self.assertEqual({"baseline": {"profile_verified": True}, "candidate": {"profile_verified": True}}, record)

    def test_a_baseline_that_predates_the_profile_is_recorded_not_hidden_and_the_candidate_is_still_verified(self):
        verified, record = self.run_main("--baseline-predates-profile")
        self.assertEqual(["new"], verified)
        self.assertFalse(record["baseline"]["profile_verified"])
        self.assertIn("predates", record["baseline"]["reason"])
        self.assertTrue(record["candidate"]["profile_verified"])


if __name__ == "__main__":
    unittest.main()
