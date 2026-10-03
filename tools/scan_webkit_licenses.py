#!/usr/bin/env python3
"""Inventory the license files and bundled components of the pinned WPE WebKit archive.

Reads the verified archive once in streaming mode (nothing is extracted to disk)
and writes a JSON inventory: member counts, the bundled third-party trees under
Source/ThirdParty, and every license/notice file with its size and SHA-256. This
is the file-level scan Web Tranche 0 requires before the pin is relied on; it
feeds the per-SDK notices and SBOM later. It does not interpret license texts: it
records what is there so a reviewer, and later tooling, can.

The archive is first verified against libcrtweb/third_party/webkit/recipe.json, so an
inventory can only describe the pinned bytes.
"""

import argparse
import hashlib
import json
import re
import sys
import tarfile
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import fetch_webkit

LICENSE_NAME = re.compile(r"^(LICENSE|LICENCE|COPYING|NOTICE|COPYRIGHT)", re.IGNORECASE)
# Small text files whose content identifies the tree; stored verbatim in the inventory.
IDENTITY_FILES = (
    "Source/cmake/WebKitVersion.cmake",
    "Source/ThirdParty/skia/include/core/SkMilestone.h",
    "Source/ThirdParty/skia/LICENSE",
    "Source/WebCore/LICENSE-APPLE",
)


def scan(archive: Path) -> dict:
    top: Counter = Counter()
    third_party: Counter = Counter()
    licenses = []
    identity: dict = {}
    members = 0
    root = None
    with tarfile.open(archive, "r:xz") as tar:
        for member in tar:
            members += 1
            parts = member.name.split("/")
            if root is None:
                root = parts[0]
            relative = "/".join(parts[1:])
            top[parts[1] if len(parts) > 1 else "."] += 1
            if len(parts) > 3 and parts[1] == "Source" and parts[2] == "ThirdParty":
                third_party[parts[3]] += 1
            if not member.isfile():
                continue
            wanted_identity = relative in IDENTITY_FILES
            if LICENSE_NAME.match(parts[-1]) or wanted_identity:
                data = tar.extractfile(member).read()
                if LICENSE_NAME.match(parts[-1]):
                    licenses.append({
                        "path": relative, "size": member.size,
                        "sha256": hashlib.sha256(data).hexdigest(),
                    })
                if wanted_identity:
                    identity[relative] = data.decode("utf-8", "replace")
    licenses.sort(key=lambda item: item["path"])
    return {
        "root": root, "members": members,
        "top_level_member_counts": dict(sorted(top.items())),
        "bundled_third_party_member_counts": dict(sorted(third_party.items())),
        "license_files": licenses,
        "identity_files": identity,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recipe", required=True, type=Path)
    parser.add_argument("--archive", required=True, type=Path,
                        help="the downloaded archive (verified against the recipe first)")
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    recipe = json.loads(args.recipe.read_text(encoding="utf-8"))
    source = fetch_webkit.validate_recipe(recipe)
    digest = fetch_webkit.verify(args.archive, source)
    inventory = scan(args.archive)
    inventory = {
        "recipe_version": recipe["version"], "archive": source["archive_name"],
        "archive_sha256": digest, **inventory,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    # newline="\n": the inventory is a committed file and must be byte-identical on
    # every host, not CRLF on Windows.
    args.output.write_text(json.dumps(inventory, indent=2, sort_keys=False) + "\n",
                           encoding="utf-8", newline="\n")
    print(f"{inventory['members']} archive members, "
          f"{len(inventory['license_files'])} license/notice files, "
          f"{len(inventory['bundled_third_party_member_counts'])} bundled third-party trees")
    print(f"wrote {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
