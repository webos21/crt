#!/usr/bin/env python3
"""Offline signing gate shared by the SDK and CLI release workflows.

Keep this build-host helper in sync with its counterpart in the other repo.
Neither repository needs the other's checkout to stage or finalize.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from urllib.parse import quote

PUBLISHER_SPKI = "59218576956e493476bc36f7385d8e01a03ff9b92946a531b013ad3937d43fa2"
RECEIPT_NAME = "release-stage.json"
STAGE_WORKFLOWS = {"crt": "sdk-release.yml", "crt-cli": "release.yml"}
SHA = re.compile(r"[0-9a-f]{64}")
ASSET = re.compile(r"[A-Za-z0-9][A-Za-z0-9_.-]*")
# A tag may also carry "+" (v0.5.0+linux.x64). Asset names may not: GitHub rewrites special characters in them.
TAG = re.compile(r"[A-Za-z0-9][A-Za-z0-9_.+-]*")
PSS = ("-sigopt", "rsa_padding_mode:pss", "-sigopt", "rsa_pss_saltlen:32",
       "-sigopt", "rsa_mgf1_md:sha256")


class SigningError(Exception):
    pass


def require(condition, message):
    if not condition:
        raise SigningError(message)


def run(*args):
    result = subprocess.run(list(map(str, args)), capture_output=True)
    if result.returncode:
        error = result.stderr.decode(errors="replace").strip()[-400:]
        raise SigningError(f"{args[0]} {args[1]} failed: {error}")
    return result.stdout


def gh(*args):
    return run("gh", *args)


def api(repo, endpoint, *args):
    return json.loads(gh("api", *args, f"repos/{repo}/{endpoint}"))


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def number(value, label):
    require(re.fullmatch(r"[1-9][0-9]{0,19}", str(value)), f"invalid {label}")
    return int(value)


def require_approval(repo):
    env = api(repo, "environments/release")
    require(any(rule.get("type") == "required_reviewers" and rule.get("reviewers")
                for rule in env.get("protection_rules", [])),
            "the release environment must have required reviewers")


def release_view(repo, release_id):
    release = api(repo, f"releases/{release_id}")
    require(release["id"] == release_id and isinstance(release["prerelease"], bool),
            "release identity changed")
    assets = {}
    pages = api(repo, f"releases/{release_id}/assets?per_page=100", "--paginate", "--slurp")
    for page in pages:
        for asset in page:
            name, digest = asset["name"], asset.get("digest")
            require(isinstance(name, str) and ASSET.fullmatch(name) and name not in assets,
                    "unsafe or duplicate release asset name")
            require(asset["state"] == "uploaded" and isinstance(digest, str)
                    and digest.startswith("sha256:") and SHA.fullmatch(digest[7:]),
                    f"{name}: missing uploaded asset SHA-256")
            assets[name] = {
                "id": number(asset["id"], "asset ID"),
                "size": asset["size"], "sha256": digest[7:],
            }
    return {
        "tag": release["tag_name"], "commit": release["target_commitish"],
        "draft": release["draft"], "prerelease": release["prerelease"], "assets": assets, "title": release["name"],
        "body_sha256": hashlib.sha256((release["body"] or "").encode("utf-8")).hexdigest(),
    }


def verify_tag(repo, tag, commit, *, required=False):
    refs = api(repo, f"git/matching-refs/tags/{quote(tag, safe='')}")
    exact = [item["object"] for item in refs if item["ref"] == f"refs/tags/{tag}"]
    require((not exact and not required)
            or (len(exact) == 1 and exact[0]["type"] == "commit" and exact[0]["sha"] == commit),
            "tag does not point directly to the staged commit")


def verify_files(directory, assets):
    paths = {path.name: path for path in directory.iterdir()}
    require(paths.keys() == assets.keys(), "downloaded files do not match the exact asset set")
    for name, item in assets.items():
        path = paths[name]
        require(not path.is_symlink() and path.is_file() and path.stat().st_size == item["size"]
                and sha256(path) == item["sha256"], f"{name}: downloaded bytes differ from verified assets")


def download_release(repo, tag, directory, assets):
    directory.mkdir()
    gh("release", "download", tag, "--repo", repo, "--dir", directory)
    verify_files(directory, assets)


def stage(repo, tag, commit, title, notes, files, work, receipt, dry_run, prerelease=True):
    require(TAG.fullmatch(tag), "unsafe release tag")
    require_approval(repo)
    verify_tag(repo, tag, commit)
    expected = {path.name: {"size": path.stat().st_size, "sha256": sha256(path)} for path in files}
    require(expected and len(expected) == len(files) and
            all(ASSET.fullmatch(name) and not name.endswith(".sig") for name in expected),
            "staging requires unique unsigned payload files")
    if not dry_run:
        run_id = number(os.environ.get("GITHUB_RUN_ID"), "stage run ID")
        attempt = number(os.environ.get("GITHUB_RUN_ATTEMPT"), "stage run attempt")
        require(not receipt.exists(), "stage receipt already exists")
    gh("release", "create", tag, "--repo", repo, "--draft", *(["--prerelease"] if prerelease else []), "--latest=false",
       "--target", commit, "--title", title, "--notes-file", notes, *files)
    res = json.loads(gh("release", "view", tag, "--repo", repo, "--json", "databaseId"))
    release_id = number(res["databaseId"], "release ID")
    view = release_view(repo, release_id)
    require(view["tag"] == tag and view["commit"] == commit and view["draft"] is True
            and view["title"] == title and view["prerelease"] is prerelease, "staged draft does not match the request")
    require({name: {k: item[k] for k in ("size", "sha256")} for name, item in view["assets"].items()} == expected,
            "draft inventory differs from the verified files")
    download_release(repo, tag, work, expected)
    require(release_view(repo, release_id) == view, "draft changed during staging")
    if dry_run:
        print(f"{tag}: verified rehearsal draft; no finalization receipt was issued")
        return
    data = {"format": 1, "repo": repo, "run_id": run_id, "attempt": attempt,
            "release_id": release_id, "draft": view}
    with receipt.open("x", encoding="utf-8") as stream:
        json.dump(data, stream, indent=2, sort_keys=True)
        stream.write("\n")
    print(f"{tag}: verified draft from run {run_id}. Sign and attach exactly:")
    for name, item in sorted(expected.items()):
        print(f"  {name}.sig (payload SHA-256 {item['sha256']})")


def stage_receipt(repo, run_id, directory):
    workflow = STAGE_WORKFLOWS.get(repo.split("/")[-1])
    require(workflow is not None, "unsupported release repository")
    run_id = number(run_id, "stage run ID")
    info = api(repo, f"actions/runs/{run_id}")
    path, _, ref = info["path"].partition("@")
    require(info["id"] == run_id and info["event"] == "workflow_dispatch"
            and info["head_branch"] == "main" and info["head_repository"]["full_name"] == repo
            and path == f".github/workflows/{workflow}" and ref in ("", "main", "refs/heads/main")
            and info["status"] == "completed" and info["conclusion"] == "success",
            "stage run must be a successful manual release run from this repository's main")
    attempt = number(info["run_attempt"], "stage run attempt")
    gh("run", "download", run_id, "--repo", repo, "--name", f"release-stage-receipt-{attempt}",
       "--dir", directory)
    require({p.name for p in directory.iterdir()} == {RECEIPT_NAME},
            "stage run has no eligible finalization receipt")
    data = json.loads((directory / RECEIPT_NAME).read_text(encoding="utf-8"))
    require(data["format"] == 1 and data["repo"] == repo and data["run_id"] == run_id
            and data["attempt"] == attempt, "finalization receipt is for another run")
    return data


def verify_signatures(directory, payload, key):
    der = run("openssl", "pkey", "-pubin", "-in", key, "-outform", "DER")
    require(hashlib.sha256(der).hexdigest() == PUBLISHER_SPKI, "publisher key fingerprint differs from the pin")
    for name in sorted(payload):
        signature = directory / (name + ".sig")
        require(signature.stat().st_size == 256, f"{name}: invalid RSA-2048 signature length")
        try:
            run("openssl", "dgst", "-sha256", "-verify", key, *PSS, "-signature", signature, directory / name)
        except SigningError as error:
            raise SigningError(f"{name}: publisher signature verification failed") from error


def finalize(repo, run_id, key, work):
    require_approval(repo)
    receipt = stage_receipt(repo, run_id, work / "receipt")
    release_id = number(receipt["release_id"], "release ID")
    staged = receipt["draft"]
    tag, commit = staged["tag"], staged["commit"]
    require(TAG.fullmatch(tag) and re.fullmatch(r"[0-9a-f]{40}", commit)
            and staged["draft"] is True, "invalid staged release identity")
    payload = staged["assets"]
    require(payload and all(ASSET.fullmatch(name) and not name.endswith(".sig") for name in payload),
            "invalid staged asset names")
    view = release_view(repo, release_id)
    require(view["draft"] is True, "release is already public; no modification was made")
    require({**view, "assets": {name: view["assets"].get(name) for name in payload}} == staged,
            "staged release metadata or payload identity changed")
    require(set(view["assets"]) == set(payload) | {name + ".sig" for name in payload},
            "expected exactly the staged assets and one .sig for every asset")
    before = work / "signed"
    download_release(repo, tag, before, view["assets"])
    verify_signatures(before, payload, key)
    verify_tag(repo, tag, commit)
    require(release_view(repo, release_id) == view, "draft or asset inventory changed during verification")
    # Publish by immutable release ID, not by tag lookup. The API has no atomic
    # compare-and-publish operation for the verified inventory.
    api(repo, f"releases/{release_id}", "--method", "PATCH", "-F", "draft=false", "-f", "make_latest=false")
    try:
        verify_tag(repo, tag, commit, required=True)
        require(release_view(repo, release_id) == {**view, "draft": False},
                "published release identity differs from the verified draft")
        download_release(repo, tag, work / "published", view["assets"])
    except (SigningError, OSError, ValueError, KeyError, TypeError) as error:
        raise SigningError(f"{tag}: publication occurred but post-publication verification failed: {error}") from error
    kind = "prerelease" if view["prerelease"] else "release"
    print(f"{tag}: signed non-latest {kind} published and downloaded assets reverified")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", required=True)
    sub = parser.add_subparsers(dest="command", required=True)
    plan = sub.add_parser("plan")
    plan.add_argument("--stage-run-id", required=True)
    final = sub.add_parser("finalize")
    final.add_argument("--stage-run-id", required=True)
    final.add_argument("--key", required=True, type=Path)
    args = parser.parse_args()
    try:
        require(re.fullmatch(r"[A-Za-z0-9_.-]+/(crt|crt-cli)", args.repo), "unsupported release repository")
        with tempfile.TemporaryDirectory(prefix="crt-release-finalize-") as temp:
            if args.command == "plan":
                require_approval(args.repo)
                receipt = stage_receipt(args.repo, args.stage_run_id, Path(temp))
                print(f"Finalize {receipt['draft']['tag']} at {receipt['draft']['commit']} "
                      f"(release {receipt['release_id']}, stage run {receipt['run_id']}, attempt {receipt['attempt']})")
            else:
                finalize(args.repo, args.stage_run_id, args.key.resolve(), Path(temp))
    except (SigningError, OSError, ValueError, KeyError, TypeError) as error:
        print(f"RELEASE FAIL: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
