#!/usr/bin/env python3
"""Pin, fetch and verify the full WebKit commit behind the signed tag (Web Tranche 3B source gate).

The WPE release tarball (tools/fetch_webkit.py) is the *reference* source: the WPE baseline, the
JSCOnly bring-up and the 3A prototype. It lacks the Windows port entirely (no PlatformWin.cmake, no
Windows IPC backend) and carries no commit header. PlatformCRT is a new cross-platform port, so before
its first file is written the whole WebKit tree at the tagged commit is pinned as a second source, role
"PlatformCRT product source", so the Mac and Win ports can be read for process, IPC, font and input
work. This tool is that source's fetch-and-verify path.

What is pinned and why that is enough: a git commit id is a hash over the commit object, whose tree
id is a hash over every directory, whose entries are hashes over every file. Recording the 40-hex
commit *and* its tree id (and a digest of the complete recursive listing, so the count of files is
pinned too) pins every byte of the tree. The fetch asks GitHub for that single commit (depth 1, trees
only: about 14 MB), checks the commit and tree ids, optionally checks that the tag still peels to the
commit, and checks out only the paths the recipe lists. Nothing is ever patched.

`--correspondence ARCHIVE` closes the other half: every file of the release tarball is hashed as a git
blob and compared with the commit's tree, so "the tarball is the tagged commit's tree, except for these
files" is a measured statement, not a sampled one.
"""

import argparse
import hashlib
import json
import subprocess
import sys
import tarfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RECIPE = ROOT / "libcrtweb" / "third_party" / "webkit" / "recipe.json"
HEX40 = set("0123456789abcdef")


def is_hex40(value) -> bool:
    return isinstance(value, str) and len(value) == 40 and set(value) <= HEX40


def product_source(recipe: dict) -> dict:
    source = recipe.get("product_source")
    if not isinstance(source, dict):
        raise SystemExit("recipe has no product_source")
    for field in ("repository", "commit", "tag", "tree", "ls_tree_sha256", "tree_entries", "checkout_paths"):
        if field not in source:
            raise SystemExit(f"product_source is missing {field}")
    if not is_hex40(source["commit"]) or not is_hex40(source["tree"]):
        raise SystemExit("product_source commit and tree must be 40-character ids")
    if source["commit"] != recipe["source"]["expected_commit"]:
        raise SystemExit("product_source.commit differs from source.expected_commit: the two sources "
                         "must be cut from the same signed tag")
    return source


def git(repo: Path, *arguments: str, capture: bool = True, check: bool = True) -> str:
    completed = subprocess.run(["git", "-C", str(repo), *arguments], check=check,
                               capture_output=capture, text=True)
    return completed.stdout if capture else ""


def fetch_commit(work: Path, source: dict) -> Path:
    """A bare, tree-only (blob:none) depth-1 repository holding exactly the pinned commit."""
    repo = work / "WebKit.git"
    if not (repo / "HEAD").exists():
        repo.mkdir(parents=True, exist_ok=True)
        subprocess.run(["git", "init", "-q", "--bare", str(repo)], check=True)
        git(repo, "remote", "add", "origin", source["repository"])
    have = git(repo, "cat-file", "-t", source["commit"], check=False).strip()
    if have != "commit":
        git(repo, "fetch", "-q", "--depth=1", "--filter=blob:none", "origin", source["commit"], capture=False)
    return repo


def verify_commit(repo: Path, source: dict) -> list:
    """Checks the commit and tree ids and the recursive listing; returns [(mode, type, sha, path)]."""
    commit = git(repo, "rev-parse", f"{source['commit']}^{{commit}}").strip()
    if commit != source["commit"]:
        raise SystemExit(f"fetched commit {commit} is not the pinned {source['commit']}")
    tree = git(repo, "rev-parse", f"{source['commit']}^{{tree}}").strip()
    if tree != source["tree"]:
        raise SystemExit(f"tree {tree} is not the pinned {source['tree']}")
    listing = git(repo, "ls-tree", "-r", "-z", source["commit"])
    digest = hashlib.sha256(listing.encode("utf-8", "surrogateescape")).hexdigest()
    entries = []
    for record in listing.split("\0"):
        if not record:
            continue
        meta, path = record.split("\t", 1)
        mode, kind, sha = meta.split(" ")
        entries.append((mode, kind, sha, path))
    if digest != source["ls_tree_sha256"] or len(entries) != source["tree_entries"]:
        raise SystemExit(f"recursive listing differs from the pin: {len(entries)} entries, sha256 {digest}")
    return entries


def verify_tag(repo: Path, source: dict) -> None:
    """The signed tag must still peel to the pinned commit (needs the network)."""
    out = git(repo, "ls-remote", source["repository"], f"refs/tags/{source['tag']}*")
    peeled = {line.split("\t")[1]: line.split("\t")[0] for line in out.splitlines()}
    want = peeled.get(f"refs/tags/{source['tag']}^{{}}")
    if want != source["commit"]:
        raise SystemExit(f"tag {source['tag']} peels to {want}, not the pinned commit")


def checkout(source: dict, destination: Path) -> int:
    """Materialise only the listed paths (blobs are fetched on demand, in one batch)."""
    destination.mkdir(parents=True, exist_ok=True)
    if not (destination / ".git").exists():
        subprocess.run(["git", "init", "-q", str(destination)], check=True)
        git(destination, "remote", "add", "origin", source["repository"])
        git(destination, "config", "core.autocrlf", "false")
    git(destination, "sparse-checkout", "init", "--cone", capture=False)
    git(destination, "sparse-checkout", "set", *source["checkout_paths"], capture=False)
    git(destination, "fetch", "-q", "--depth=1", "--filter=blob:none", "origin", source["commit"], capture=False)
    git(destination, "checkout", "-q", "--detach", source["commit"], capture=False)
    head = git(destination, "rev-parse", "HEAD").strip()
    if head != source["commit"]:
        raise SystemExit("checkout is not at the pinned commit")
    return sum(1 for path in destination.rglob("*") if path.is_file() and ".git" not in path.parts)


def blob_sha1(data: bytes) -> str:
    return hashlib.sha1(b"blob %d\0" % len(data) + data).hexdigest()


def correspondence(archive: Path, entries: list) -> dict:
    """Every regular file of the release tarball against the commit's tree (by git blob id)."""
    tree = {path: (mode, sha) for mode, kind, sha, path in entries if kind == "blob"}
    identical = 0
    differs, line_endings, only_archive = [], [], []
    seen = set()
    with tarfile.open(archive, "r:xz") as tar:
        for member in tar:
            if not member.isfile():
                continue
            relative = "/".join(member.name.split("/")[1:])
            if not relative:
                continue
            data = tar.extractfile(member).read()
            seen.add(relative)
            entry = tree.get(relative)
            if entry is None:
                only_archive.append(relative)
            elif blob_sha1(data) == entry[1]:
                identical += 1
            elif blob_sha1(data.replace(b"\r\n", b"\n")) == entry[1]:
                line_endings.append(relative)  # .gitattributes eol=crlf applied when the tarball was made
            else:
                differs.append(relative)
    only_commit = sorted(path for path in tree if path not in seen)
    by_top = {}
    for path in only_commit:
        top = path.split("/")[0] if "/" in path else "."
        by_top[top] = by_top.get(top, 0) + 1
    # Source/ trees that a build needs and the tarball lacks are the interesting ones.
    only_commit_source = [path for path in only_commit if path.startswith("Source/")]
    return {
        "archive_files": identical + len(differs) + len(line_endings) + len(only_archive),
        "identical_to_commit": identical,
        "same_after_crlf_to_lf": len(line_endings),
        "same_after_crlf_to_lf_top_directories": sorted({"/".join(p.split("/")[:5]) for p in line_endings}),
        "content_differs_from_commit": sorted(differs),
        "only_in_archive": sorted(only_archive),
        "only_in_commit_by_top_level": dict(sorted(by_top.items())),
        "only_in_commit_under_Source": len(only_commit_source),
        "commit_blob_entries": len(tree),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recipe", type=Path, default=RECIPE)
    parser.add_argument("--work", type=Path, default=ROOT / "out" / "web-product")
    parser.add_argument("--offline", action="store_true", help="skip the tag peel check")
    parser.add_argument("--checkout", action="store_true", help="materialise product_source.checkout_paths")
    parser.add_argument("--correspondence", type=Path, metavar="ARCHIVE",
                        help="compare the release tarball with the commit tree")
    parser.add_argument("--json", type=Path, help="write the correspondence result here")
    args = parser.parse_args()

    recipe = json.loads(args.recipe.read_text(encoding="utf-8"))
    source = product_source(recipe)
    repo = fetch_commit(args.work, source)
    entries = verify_commit(repo, source)
    print(f"commit {source['commit']} tree {source['tree']}: {len(entries)} entries match the pin")
    if not args.offline:
        verify_tag(repo, source)
        print(f"tag {source['tag']} peels to the pinned commit")
    if args.correspondence:
        result = correspondence(args.correspondence, entries)
        print(json.dumps(result, indent=2)[:6000])
        if args.json:
            args.json.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8", newline="\n")
    if args.checkout:
        files = checkout(source, args.work / "src")
        print(f"checked out {files} files under {args.work / 'src'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
