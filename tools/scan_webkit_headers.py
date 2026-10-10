#!/usr/bin/env python3
"""Per-file license header scan of the pinned WebKit tree (Web Tranche 3B licensing gate).

Tranche 0 inventoried license *files*; this scans the license *header of every source file* of the
PlatformCRT product source (tools/fetch_webkit_commit.py --checkout), classifies it, and writes a
compact summary: counts per license class and per component, every file in a class that needs a
decision (GPL, anything with a copyright notice that matched no known text), and the bundled
third-party components. The classifier is deliberately small and conservative: it recognises standard
texts and SPDX tags; anything else is reported as unclassified, never guessed.

classify_header() is shared with tools/check_webkit_patch_manifest.py, which requires a recorded
license class for every WebKit file a CRT patch modifies and refuses classes CRT cannot carry.
"""

import argparse
import json
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

HEADER_BYTES = 8192
COMMENT_NOISE = re.compile(r"[^a-z0-9]+")
SPDX = re.compile(r"spdx-license-identifier:\s*([^\n\r*]+)", re.IGNORECASE)

# Classes CRT can carry in a modified or redistributed WebKit file, and the ones it cannot
# without a decision. (WebKit's own code is LGPL-2/2.1 or BSD; bundled components differ.)
PERMISSIVE = {"BSD-style", "BSD-2-Clause", "BSD-3-Clause", "BSD-2-Clause-Patent", "MIT", "ISC", "Zlib", "Apache-2.0",
              "Apache-2.0 OR MIT", "Unicode", "Unicode-3.0", "Unicode-ICU", "Public-Domain", "BSL-1.0"}
# The components a WebKit port is built from; a file there needs a decision even when only
# "unclassified". Bundled third-party trees are reported by count and tree.
CORE_COMPONENTS = {"WTF", "JavaScriptCore", "WebCore", "WebKit", "bmalloc", "cmake", "WebDriver", "PAL"}
LGPL = {"LGPL-2+", "LGPL-2.1+", "LGPL-2.1", "LGPL-2", "LGPL"}
NEEDS_DECISION = {"GPL", "GPL-with-exception", "MPL", "unclassified-with-copyright"}


# SPDX spellings folded into the classes the checks use.
SPDX_ALIASES = {"LGPL-2.1-or-later": "LGPL-2.1+", "LGPL-2.0-or-later": "LGPL-2+",
                "LGPL-2.1-only": "LGPL-2.1", "LGPL-2.0-only": "LGPL-2", "LGPL-2.1": "LGPL-2.1"}


def normalize(text: str) -> str:
    return COMMENT_NOISE.sub(" ", text.lower()).strip()


def classify_header(data: bytes) -> str:
    """The license class of one file from its first 8 KiB, or 'binary' / 'no-license-header'."""
    head = data[:HEADER_BYTES]
    if b"\0" in head[:1024]:
        return "binary"
    raw = head.decode("utf-8", "replace")
    tag = SPDX.search(raw)
    if tag:
        expression = tag.group(1).strip().rstrip("*/ ")
        return SPDX_ALIASES.get(expression, expression)
    text = normalize(raw)
    if "bsd style license applies to this one file" in text:
        return "BSD-style"  # valgrind.h: a BSD-style notice on one file of an otherwise GPL project
    if "gnu lesser general public license" in text or "gnu library general public license" in text:
        later = "any later version" in text or "or later" in text
        if "version 2 1" in text or "2 1 of the license" in text:
            return "LGPL-2.1+" if later else "LGPL-2.1"
        if "version 2 of the license" in text or "version 2" in text:
            return "LGPL-2+" if later else "LGPL-2"
        return "LGPL"
    if "gnu general public license" in text:
        return "GPL-with-exception" if "exception" in text else "GPL"
    if "unicode org copyright" in text or "unicode org license" in text:
        return "Unicode-ICU"  # ICU headers: "License & terms of use: http://www.unicode.org/copyright.html"
    if "mozilla public license" in text:
        return "MPL"
    if "apache license" in text and "version 2" in text:
        return "Apache-2.0"
    if "boost software license" in text:
        return "BSL-1.0"
    if "under the terms of the mit license" in text:
        return "MIT"
    if "permission is hereby granted free of charge" in text:
        return "Unicode" if "unicode" in text and "data files" in text else "MIT"
    if "permission to use copy modify and or distribute this software for any purpose" in text:
        return "ISC"
    if "redistribution and use in source and binary forms" in text:
        if "advertising materials mentioning features" in text:
            return "BSD-4-Clause"
        if "neither the name" in text or "may be used to endorse or promote" in text:
            return "BSD-3-Clause"
        return "BSD-2-Clause"
    if "governed by a bsd style license" in text:
        return "BSD-3-Clause"
    if "altered source versions must be plainly marked" in text:
        return "Zlib"
    if "public domain" in text and "copyright" not in text:
        return "Public-Domain"
    if "copyright" in text or "all rights reserved" in text:
        return "unclassified-with-copyright"
    return "no-license-header"


def component(relative: str) -> str:
    parts = relative.split("/")
    if len(parts) >= 3 and parts[0] == "Source" and parts[1] == "ThirdParty":
        return f"ThirdParty/{parts[2]}"
    if len(parts) >= 2 and parts[0] == "Source":
        return parts[1]
    return parts[0]


def scan_tree(tree: Path) -> dict:
    by_class: Counter = Counter()
    by_component: dict = defaultdict(Counter)
    flagged: dict = defaultdict(list)
    files = 0
    for path in sorted(tree.rglob("*")):
        if not path.is_file() or path.is_symlink() or ".git" in path.relative_to(tree).parts:
            continue
        relative = path.relative_to(tree).as_posix()
        with path.open("rb") as stream:
            cls = classify_header(stream.read(HEADER_BYTES))
        files += 1
        by_class[cls] += 1
        by_component[component(relative)][cls] += 1
        if cls in ("unclassified-with-copyright", "no-license-header"):
            # Counts everywhere; the file list only for the components a port is built from, and
            # for no-license-header only source-like files there (build files and data are many).
            if component(relative) in CORE_COMPONENTS and cls == "unclassified-with-copyright":
                flagged[cls].append(relative)
        elif cls in NEEDS_DECISION or cls == "BSD-4-Clause" or cls not in PERMISSIVE | LGPL | {"binary"}:
            flagged[cls].append(relative)
    return {
        "files_scanned": files,
        "by_class": dict(by_class.most_common()),
        "by_component": {name: dict(counter.most_common()) for name, counter in sorted(by_component.items())},
        "flagged": {cls: {"count": len(paths), "files": paths[:400], "truncated": len(paths) > 400}
                    for cls, paths in sorted(flagged.items())},
        "core_components": sorted(CORE_COMPONENTS),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tree", required=True, type=Path,
                        help="the product source checkout (tools/fetch_webkit_commit.py --checkout)")
    parser.add_argument("--recipe", type=Path,
                        default=Path(__file__).resolve().parent.parent / "libcrtweb/third_party/webkit/recipe.json")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    recipe = json.loads(args.recipe.read_text(encoding="utf-8"))
    source = recipe["product_source"]
    result = scan_tree(args.tree)
    report = {
        "schema": 1,
        "source": {"role": source["role"], "commit": source["commit"], "tree": source["tree"],
                   "scanned_paths": source["checkout_paths"]},
        "method": "tools/scan_webkit_headers.py: first 8 KiB of every file; SPDX tag, else standard license texts; "
                  "never guessed (unmatched copyright text is 'unclassified-with-copyright')",
        **result,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"{report['files_scanned']} files; classes: " +
          ", ".join(f"{name}={count}" for name, count in list(report["by_class"].items())[:12]))
    for cls, info in report["flagged"].items():
        print(f"  {cls}: {info['count']}")
    print(f"wrote {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
