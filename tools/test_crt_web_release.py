#!/usr/bin/env python3
"""Tests for tools/crt_web_release.py (the 06-web public-release decision)."""

import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import crt_web_release as web

BLOCKERS = ["native three-host and cross-target packaging acceptance required", "another open blocker"]
SECURITY = {"passed": True, "pin_is_latest_micro": True, "series_within_grace": True,
            "release_blockers": BLOCKERS}
APPROVED = {"web_public_release": {"approved": True, "approved_by": "owner", "approved_utc": "2026-11-01",
                                   "accepted_blockers": BLOCKERS}}


class Decide(unittest.TestCase):
    def test_real_default_is_not_approved(self) -> None:
        config = json.loads((Path(__file__).resolve().parents[1] / "third_party/crt-runtime/release.json").read_text())
        decision = web.decide(config, SECURITY)
        self.assertFalse(decision["public_release_allowed"])
        self.assertTrue(decision["development_candidate"])
        self.assertEqual(BLOCKERS, decision["open_blockers"])

    def test_approval_with_every_blocker_accepted_allows_it(self) -> None:
        decision = web.decide(APPROVED, SECURITY)
        self.assertTrue(decision["public_release_allowed"])
        self.assertEqual([], decision["reasons"])
        self.assertEqual("owner", decision["approval"]["approved_by"])

    def test_one_unaccepted_blocker_keeps_it_a_candidate(self) -> None:
        partial = {"web_public_release": {**APPROVED["web_public_release"], "accepted_blockers": BLOCKERS[:1]}}
        decision = web.decide(partial, SECURITY)
        self.assertFalse(decision["public_release_allowed"])
        self.assertEqual(BLOCKERS[1:], decision["open_blockers"])

    def test_failed_snapshot_or_stale_pin_blocks_even_an_approved_stage(self) -> None:
        self.assertFalse(web.decide(APPROVED, {**SECURITY, "passed": False})["public_release_allowed"])
        self.assertFalse(web.decide(APPROVED, {**SECURITY, "pin_is_latest_micro": False})["public_release_allowed"])

    def test_a_series_past_the_grace_period_blocks_even_an_approved_stage(self) -> None:
        stale = {**SECURITY, "series_within_grace": False}
        decision = web.decide(APPROVED, stale)
        self.assertFalse(decision["public_release_allowed"])
        self.assertIn("grace period", " ".join(decision["reasons"]))

    def test_inconsistent_approval_records_are_rejected(self) -> None:
        for patch in ({"approved_by": None}, {"approved_utc": "someday"}, {"accepted_blockers": "all"}):
            with self.assertRaises(ValueError, msg=str(patch)):
                web.decide({"web_public_release": {**APPROVED["web_public_release"], **patch}}, SECURITY)


class Recorded(unittest.TestCase):
    def test_coherent_candidate_and_public_records(self) -> None:
        candidate = web.decide({}, SECURITY)
        runtime = {"development_candidate": True, "release_decision": candidate}
        self.assertEqual([], web.check_recorded(runtime, {"public_release_allowed": False}, SECURITY))
        public = web.decide(APPROVED, SECURITY)
        runtime = {"development_candidate": False, "release_decision": public}
        self.assertEqual([], web.check_recorded(runtime, {"public_release_allowed": True}, SECURITY))

    def test_contradictions_are_reported(self) -> None:
        public = web.decide(APPROVED, SECURITY)
        self.assertTrue(web.check_recorded({"development_candidate": True, "release_decision": public},
                                           {"public_release_allowed": True}, SECURITY))
        self.assertTrue(web.check_recorded({"development_candidate": False, "release_decision": public},
                                           {"public_release_allowed": False}, SECURITY))
        self.assertTrue(web.check_recorded({}, {}, SECURITY))
        stripped = {**public, "approval": None}
        problems = web.check_recorded({"development_candidate": False, "release_decision": stripped},
                                      {"public_release_allowed": True}, SECURITY)
        self.assertIn("approval record", " ".join(problems))
        unaccepted = {**public, "approval": {**public["approval"], "accepted_blockers": []}}
        self.assertIn("unaccepted", " ".join(web.check_recorded(
            {"development_candidate": False, "release_decision": unaccepted}, {"public_release_allowed": True}, SECURITY)))


if __name__ == "__main__":
    unittest.main()
