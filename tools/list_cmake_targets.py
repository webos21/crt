#!/usr/bin/env python3
"""List the CMake targets of a configured CRT build tree, split into user-facing and internal ones.

User-facing targets are the ones the README documents (docs/guides/build_targets.json owns the list, so the README,
this tool and the CI check cannot drift apart). Everything else is internal. The output is stable text so the lists
of two hosts can be compared (--diff) to see which targets exist only on some operating systems.

  list_cmake_targets.py <build dir>                       print both groups
  list_cmake_targets.py <build dir> --json FILE           write the classification
  list_cmake_targets.py --diff A.json B.json              targets present on one host only
  list_cmake_targets.py <build dir> --check               fail if a documented target is missing
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CATALOG = ROOT / "docs" / "guides" / "build_targets.json"

INTERNAL_RULES = (
    ("stage source assets", r"^crt-stage-0\d-source$"),
    ("stage cache", r"^crt-stage-cache-reset$"),
    ("port recipes", r"^(port-(build|fetch|rebuild|test)-.+|port-build-(recipes|configure|web)|port-test-recipes)$"),
    ("feature preparation", r"^(crt-libcxx-(fetch|configure|sysroot|smoke)|crtgfx-(skia|wayland|xkbcommon|mingw-w64).*|crtui-lvgl-fetch|crtmedia-require-libva)$"),
    ("stage aggregates", r"^crt-(gfx|media)-(build|test)$"),
    ("acceptance gates", r"^crtgfx-boundary-acceptance$"),
    ("cmake defaults", r"^(all|clean|install.*|test|edit_cache|rebuild_cache|list_install_components|package.*|ZERO_CHECK|CMakeCache\.txt)$"),
    ("test programs", r"(_test|_smoke|_demo|_runs|_victim)$|^(android_log_test)$"),
)


def target_names(build_dir: Path) -> list:
    out = subprocess.run(["ninja", "-C", str(build_dir), "-t", "targets", "all"], capture_output=True, text=True,
                         check=True).stdout
    names = set()
    for line in out.splitlines():
        name, _, rule = line.partition(": ")
        if name.startswith("cmake_object_order_depends_") or name in ("build.ninja", "cmake_order_depends"):
            continue  # Ninja bookkeeping, not CMake targets
        if "/" in name or "\\" in name or (name.endswith((".o", ".a", ".so", ".dylib", ".dll", ".exe", ".txt", ".cmake"))
                                          and name != "CMakeCache.txt"):
            continue
        names.add(name)
    return sorted(names)


def load_catalog() -> dict:
    return json.loads(CATALOG.read_text(encoding="utf-8"))


def classify(names: list, catalog: dict, target_os: str | None = None) -> dict:
    user = {}
    for group, entries in catalog["groups"].items():
        for entry in entries:
            user[entry["target"]] = group
    result = {"user": {}, "internal": {}, "missing": [], "unlisted_stage_like": []}
    for name in names:
        if name in user:
            result["user"][name] = user[name]
            continue
        for label, pattern in INTERNAL_RULES:
            if re.search(pattern, name):
                result["internal"][name] = label
                break
        else:
            result["internal"][name] = "libraries and tools"
    result["missing"] = sorted(
        t for t in user if t not in names and
        (target_os is None or not catalog["os_specific"].get(t) or target_os in catalog["os_specific"][t]))
    parts = catalog.get("internal_stage_parts", {})
    result["unlisted_stage_like"] = sorted(
        n for n in result["internal"]
        if re.match(r"^crt-[a-z0-9-]+-(config|build|test|dist|release)$", n) and n not in parts)
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("build_dir", nargs="?", type=Path)
    parser.add_argument("--json", type=Path)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--diff", nargs=2, type=Path)
    args = parser.parse_args()
    if args.diff:
        a, b = (json.loads(path.read_text(encoding="utf-8")) for path in args.diff)
        for label, left, right in (("only in " + args.diff[0].name, a, b), ("only in " + args.diff[1].name, b, a)):
            names = [n for n in sorted(set(left["user"]) | set(left["internal"]))
                     if n not in right["user"] and n not in right["internal"]]
            print(f"{label}: {len(names)}")
            for n in names:
                print("  ", n)
        return 0
    if args.build_dir is None:
        parser.error("a build directory is required")
    catalog = load_catalog()
    cache = (args.build_dir / "CMakeCache.txt").read_text(encoding="utf-8")
    selected_os = re.search(r"^CRT_TARGET_OS:[^=]+=([^\r\n]+)$", cache, re.M)
    if selected_os is None:
        parser.error("build directory has no CRT_TARGET_OS in CMakeCache.txt")
    result = classify(target_names(args.build_dir), catalog, selected_os.group(1))
    if args.json:
        args.json.write_text(json.dumps(result, indent=1), encoding="utf-8")
    if args.check:
        for name in result["missing"]:
            print(f"documented target missing: {name}", file=sys.stderr)
        for name in result["unlisted_stage_like"]:
            print(f"stage-looking target not in docs/guides/build_targets.json: {name}", file=sys.stderr)
        return 1 if result["missing"] or result["unlisted_stage_like"] else 0
    print(f"user-facing targets: {len(result['user'])}")
    current = None
    for name, group in sorted(result["user"].items(), key=lambda kv: (kv[1], kv[0])):
        if group != current:
            current = group
            print(f"\n[{group}]")
        print(f"  {name}")
    groups = {}
    for name, label in result["internal"].items():
        groups.setdefault(label, []).append(name)
    print(f"\ninternal targets: {len(result['internal'])}")
    for label in sorted(groups):
        items = groups[label]
        print(f"  {label}: {len(items)}" + ("" if len(items) > 12 else "  " + ", ".join(items)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
