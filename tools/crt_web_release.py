#!/usr/bin/env python3
"""Whether the 06-web stage may be published as a final release (Tranche 10).

The WebKit advisory snapshot (tools/check_webkit_security.py) never grants approval itself: it records the
``release_blockers`` that are still open. The owner decides in ``third_party/crt-runtime/release.json``
(``web_public_release``): approve the stage and name, verbatim, the blockers that are accepted. This module turns
the two into the decision that ``build_stage_06_web.py`` writes into the SDK, ``verify_dist.py`` checks for
coherence and ``prepare_release_assets.py`` enforces (a final tag needs a public decision; a prerelease tag may
carry a development candidate).
"""

from __future__ import annotations

import re

ISO_DATE = re.compile(r"^\d{4}-\d{2}-\d{2}(T[\d:.+\-Zz]+)?$")


def decide(config: dict, security: dict) -> dict:
    """The release decision for one 06-web build; raises ValueError for an inconsistent approval record."""
    approval = config.get("web_public_release") or {}
    approved = approval.get("approved") is True
    accepted = approval.get("accepted_blockers", [])
    if not isinstance(accepted, list) or not all(isinstance(item, str) and item for item in accepted):
        raise ValueError("web_public_release.accepted_blockers must be a list of blocker texts")
    if approved:
        if not isinstance(approval.get("approved_by"), str) or not approval["approved_by"].strip():
            raise ValueError("web_public_release: approved=true needs approved_by")
        if not isinstance(approval.get("approved_utc"), str) or not ISO_DATE.match(approval["approved_utc"]):
            raise ValueError("web_public_release: approved=true needs approved_utc (an ISO date)")
    blockers = list(security.get("release_blockers", []))
    open_blockers = [item for item in blockers if item not in accepted]
    reasons = []
    if not approved:
        reasons.append("the owner has not approved a public Web release")
    if security.get("passed") is not True:
        reasons.append("the WebKit advisory snapshot did not pass")
    if security.get("pin_is_latest_micro") is not True:
        reasons.append("the WebKit pin is not the newest micro release")
    reasons += [f"open release blocker: {item}" for item in open_blockers]
    allowed = not reasons
    return {"public_release_allowed": allowed, "development_candidate": not allowed, "reasons": reasons,
            "open_blockers": open_blockers,
            "approval": ({"approved_by": approval.get("approved_by"), "approved_utc": approval.get("approved_utc"),
                          "accepted_blockers": accepted} if approved else None)}


def check_recorded(manifest_runtime: dict, correspondence: dict, security: dict) -> list:
    """Coherence problems of the decision recorded in a built SDK (empty = coherent)."""
    problems = []
    decision = manifest_runtime.get("release_decision")
    if not isinstance(decision, dict):
        return ["web_runtime.release_decision is missing"]
    allowed = decision.get("public_release_allowed")
    if manifest_runtime.get("development_candidate") is not (not allowed):
        problems.append("development_candidate contradicts the recorded release decision")
    if correspondence.get("public_release_allowed") is not allowed:
        problems.append("web-source-correspondence.json contradicts the recorded release decision")
    if allowed:
        approval = decision.get("approval") or {}
        if not approval.get("approved_by") or not approval.get("approved_utc"):
            problems.append("a public Web release carries no approval record")
        missing = [item for item in security.get("release_blockers", []) if item not in approval.get("accepted_blockers", [])]
        if missing:
            problems.append("a public Web release has unaccepted blockers: " + "; ".join(missing))
        if security.get("passed") is not True or security.get("pin_is_latest_micro") is not True:
            problems.append("a public Web release needs a passing advisory snapshot on the newest micro")
    elif not decision.get("reasons"):
        problems.append("a development candidate must say why")
    return problems
