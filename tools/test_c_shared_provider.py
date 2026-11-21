#!/usr/bin/env python3
"""Run installed native Linux C policies; retain maps and actual loaded images.

This exercises public compiler-policy argv without a wrapper at compile time.
CLI adoption is a separate consumer replay. A failed baseline remains evidence,
not a supported shared-mode pass.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess

from crt_sdk_install_metadata import inventory

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-root", required=True, type=Path)
    parser.add_argument("--contract", required=True, type=Path)
    parser.add_argument("--contract-sha256", required=True)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--cc", default="clang")
    parser.add_argument("--allow-dirty-sdk", action="store_true")
    parser.add_argument("--loader", action="append", default=[], type=Path)
    args = parser.parse_args()
    sdk, output = args.sdk_root.resolve(), args.output.resolve()
    if output.exists():
        parser.error("output already exists; preserve prior evidence")
    if digest(args.contract) != args.contract_sha256:
        parser.error("contract SHA-256 mismatch")
    contract = json.loads(args.contract.read_text())
    arch = {"amd64": "x86_64", "arm64": "aarch64"}.get(platform.machine().lower(), platform.machine().lower())
    if platform.system() != "Linux" or contract["target"] != {"os": "linux", "arch": arch}:
        parser.error("this is native Linux acceptance, not cross execution")
    before = inventory(sdk)
    content = hashlib.sha256(json.dumps(before, ensure_ascii=False, separators=(",", ":")).encode()).hexdigest()
    if before != contract["sdk_inventory"] or content != contract["sdk_content_sha256"]:
        parser.error("SDK inventory differs from the contract")
    profile = json.loads((sdk / "share/crt/contracts/development.json").read_text())
    if not args.allow_dirty_sdk and (profile["producer_working_tree_dirty"] or profile["compile_provenance"]["working_tree_dirty"]):
        parser.error("dirty local candidate needs --allow-dirty-sdk")
    cc = str(Path(shutil.which(args.cc) or args.cc).resolve())
    resource = subprocess.check_output([cc, "-print-resource-dir"], text=True).strip()
    output.mkdir(parents=True)
    commands, cases = [], []

    def invoke(label, argv, env=None):
        result = subprocess.run(argv, capture_output=True, text=True, timeout=60, env=env)
        (output / (label + ".stdout")).write_text(result.stdout)
        (output / (label + ".stderr")).write_text(result.stderr)
        commands.append({"case": label, "argv": argv, "exit": result.returncode})
        if result.returncode:
            raise ValueError(f"{label}: exit {result.returncode}: {result.stderr[-1000:]}")
        return result.stdout

    def expand(mode, operation, source, destination, objects=(), libraries=(), extra=()):
        result = []
        for arg in contract["modes"][mode][operation + "_c"]:
            if arg in ("@OBJECTS@", "@LIBRARIES@", "@EXTRA@"):
                result.extend({"@OBJECTS@": objects, "@LIBRARIES@": libraries, "@EXTRA@": extra}[arg])
                continue
            for key, value in {"@SDK@": str(sdk), "@RESOURCE@": resource, "@SOURCE@": str(source),
                               "@OUTPUT@": str(destination), "@OUTPUT_NAME@": destination.name}.items():
                arg = arg.replace(key, value)
            if "@" in arg:
                raise ValueError("unresolved policy token: " + arg)
            result.append(arg)
        return [cc, *result]

    library = output / "libcrt_provider.so"
    source = ROOT / "libc/tests/sdk_c_provider_dso.c"
    obj = output / "provider.o"
    invoke("dso-compile", expand("shared-library", "compile", source, obj))
    invoke("dso-link", expand("shared-library", "link", source, library, [str(obj)]))
    dynamic = invoke("dso-dynamic", ["readelf", "-dW", str(library)])
    # A same-SONAME copy is an explicit-provider search-order control, not a
    # separately built upstream port or a binary reproducibility claim.
    explicit = output / "explicit provider/lib"
    explicit.mkdir(parents=True)
    zlib = sdk / contract["libraries"]["z"]["shared"]
    zlib_dynamic = invoke("zlib-dynamic", ["readelf", "-dW", str(zlib)])
    soname = re.search(r"\(SONAME\).*?\[(.*?)\]", zlib_dynamic).group(1)
    shutil.copy2(zlib.resolve(), explicit / soname)
    source = ROOT / "libc/tests/sdk_c_provider_test.c"
    for name, mode, shared_z, project_dso, explicit_z in (
        ("static-sdk-static", "executable-static", False, False, False),
        ("shared-sdk-static", "executable-shared", False, False, False),
        ("shared-sdk-shared", "executable-shared", True, False, False),
        ("shared-project-dso", "executable-shared", True, True, False),
        ("shared-explicit-provider", "executable-shared", True, False, True),
    ):
        case = {"name": name, "passed": False, "executions": []}
        cases.append(case)
        try:
            exe, obj = output / name, output / (name + ".o")
            extra = ["-std=c99", "-DCRT_TEST_PROJECT_DSO=1"] if project_dso else ["-std=c99"]
            invoke(name + "-compile", expand(mode, "compile", source, obj, extra=extra))
            selected = explicit / soname if explicit_z else sdk / contract["libraries"]["z"]["shared" if shared_z else "static"]
            libraries = [str(selected)]
            if project_dso:
                libraries.append(str(library))
            if explicit_z:
                libraries.append("-Wl,-rpath," + str(explicit))
            link_map = output / (name + ".map")
            libraries.append("-Wl,-Map," + str(link_map))
            invoke(name + "-link", expand(mode, "link", source, exe, [str(obj)], libraries))
            tags = invoke(name + "-dynamic", ["readelf", "-dW", str(exe)])
            needed = re.findall(r"\(NEEDED\).*?\[(.*?)\]", tags)
            case["needed"] = needed
            shared = mode == "executable-shared"
            if shared != ("libc.so" in needed) or "libc.so.6" in needed:
                raise ValueError("incorrect direct CRT runtime dependency: " + str(needed))
            map_text = link_map.read_text()
            if not shared and str(sdk / "lib/libc.a") not in map_text:
                raise ValueError("static control did not consume the SDK's C archive")
            if not shared_z and str(selected) not in map_text:
                raise ValueError("static zlib control did not consume the selected archive")
            for suffix in ("libc.a", "libm.a", "libdl.a"):
                if shared and str(sdk / "lib" / suffix) in map_text:
                    raise ValueError("shared executable embeds a static CRT: " + suffix)
            for index, loader in enumerate([None, *args.loader]):
                argv = [str(exe)] if loader is None else [str(loader.resolve()), str(exe)]
                text = invoke(name + "-run-" + str(index), argv, {"PATH": "/nonexistent"})
                if "sdk_c_provider: ok" not in text:
                    raise ValueError("missing runtime success marker")
                images = sorted(set(line.split(maxsplit=5)[5] for line in text.splitlines() if line.startswith(tuple("0123456789abcdef")) and "/" in line))
                c_images = [p for p in images if Path(p).name.startswith("libc.so")]
                z_images = [p for p in images if Path(p).name.startswith("libz.so")]
                expected_c = [str((sdk / "lib/libc.so").resolve())] if shared else []
                expected_z = [str(selected.resolve())] if shared_z else []
                if c_images != expected_c or z_images != expected_z:
                    raise ValueError(f"wrong loaded providers: libc={c_images}, libz={z_images}")
                case["executions"].append({"loader": str(loader) if loader else "PT_INTERP", "images": images})
            case["passed"] = True
        except (ValueError, subprocess.SubprocessError) as error:
            case["error"] = str(error)
    unchanged = inventory(sdk) == before
    result = {"passed": unchanged and all(c["passed"] for c in cases), "target": contract["target"],
              "sdk_content_sha256": contract["sdk_content_sha256"], "contract_sha256": args.contract_sha256,
              "sdk_unchanged": unchanged, "dso_needed": re.findall(r"\(NEEDED\).*?\[(.*?)\]", dynamic),
              "cases": cases, "commands": commands}
    (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print("C provider:", ", ".join(c["name"] + "=" + str(c["passed"]) for c in cases))
    for case in cases:
        if "error" in case:
            print(case["name"] + ": " + case["error"])
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
