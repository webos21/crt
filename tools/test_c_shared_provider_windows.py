#!/usr/bin/env python3
"""Run the installed native Windows C runtime policies; retain PE imports and the loaded libc.dll.

The Linux counterpart (test_c_shared_provider.py) also covers SDK zlib and an explicit same-SONAME provider.
Windows resolves import libraries by name and loads DLLs from the application directory and PATH, so those two
cases have no equivalent here and are not claimed. This checks only what the exported C policy decides:
static vs shared libc, the exact libc.dll image, and one libc for an executable plus a project DLL.
The public policy argv is expanded without wrappers; a pre-existing output directory is refused.
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
    parser.add_argument("--os-sdk", required=True, type=Path, help="Windows SDK um/<arch> import-library directory")
    parser.add_argument("--allow-dirty-sdk", action="store_true")
    args = parser.parse_args()
    sdk, output = args.sdk_root.resolve(), args.output.resolve()
    if output.exists():
        parser.error("output already exists; preserve prior evidence")
    if digest(args.contract) != args.contract_sha256:
        parser.error("contract SHA-256 mismatch")
    contract = json.loads(args.contract.read_text())
    arch = {"amd64": "x86_64", "arm64": "aarch64"}.get(platform.machine().lower(), platform.machine().lower())
    if platform.system() != "Windows" or contract["target"] != {"os": "windows", "arch": arch}:
        parser.error("this is native Windows acceptance, not cross execution")
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
    sdk_bin = sdk / "bin"
    system = Path(os.environ["SystemRoot"]) / "System32"

    def invoke(label, argv, env=None, expect_failure=False):
        result = subprocess.run(argv, capture_output=True, text=True, timeout=60, env=env)
        (output / (label + ".stdout")).write_text(result.stdout)
        (output / (label + ".stderr")).write_text(result.stderr)
        commands.append({"case": label, "argv": argv, "exit": result.returncode})
        if bool(result.returncode) != expect_failure:
            raise ValueError(f"{label}: exit {result.returncode}: {result.stderr[-1000:]}")
        return result.stdout

    def expand(mode, operation, source, destination, objects=(), extra=()):
        result = []
        for arg in contract["modes"][mode][operation + "_c"]:
            if arg in ("@OBJECTS@", "@LIBRARIES@", "@EXTRA@"):
                result.extend({"@OBJECTS@": objects, "@LIBRARIES@": (), "@EXTRA@": extra}[arg])
                continue
            for key, value in {"@SDK@": str(sdk), "@RESOURCE@": resource, "@SOURCE@": str(source),
                               "@OUTPUT@": str(destination), "@OS_SDK@": str(args.os_sdk),
                               "@OUTPUT_NAME@": destination.name}.items():
                arg = arg.replace(key, value)
            if "@" in arg:
                raise ValueError("unresolved policy token: " + arg)
            result.append(arg)
        return [cc, *result]

    def imports(label, binary):
        text = invoke(label, [str(Path(cc).with_name("llvm-readobj.exe")), "--coff-imports", str(binary)])
        return sorted(set(m.lower() for m in re.findall(r"Name:\s+(\S+\.dll)", text, re.IGNORECASE)))

    library = output / "crt_provider.dll"
    dll_source = ROOT / "libc/tests/sdk_c_provider_dso.c"
    obj = output / "provider.o"
    dll_imports = []
    case = {"name": "project-dll-build", "passed": False}
    cases.append(case)
    try:
        invoke("dll-compile", expand("shared-library", "compile", dll_source, obj))
        invoke("dll-link", expand("shared-library", "link", dll_source, library, [str(obj)]))
        dll_imports = imports("dll-imports", library)
        if "libc.dll" not in dll_imports:
            raise ValueError("project DLL does not import libc.dll: " + str(dll_imports))
        case["passed"] = True
    except (ValueError, subprocess.SubprocessError) as error:
        case["error"] = str(error)
    source = ROOT / "libc/tests/sdk_c_provider_windows_test.c"
    environment = {"SystemRoot": os.environ["SystemRoot"], "PATH": str(sdk_bin) + os.pathsep + str(system)}
    for name, mode, project_dll in (
        ("static-runtime", "executable-static", False),
        ("shared-runtime", "executable-shared", False),
        ("shared-project-dll", "executable-shared", True),
    ):
        case = {"name": name, "passed": False}
        cases.append(case)
        try:
            if project_dll and "error" in cases[0]:
                raise ValueError("project DLL did not build")
            exe, obj = output / (name + ".exe"), output / (name + ".o")
            extra = ["-std=c99"] + (["-DCRT_TEST_PROJECT_DLL=1"] if project_dll else [])
            invoke(name + "-compile", expand(mode, "compile", source, obj, extra=extra))
            objects = [str(obj)] + ([str(library.with_suffix(".dll.a"))] if project_dll and library.with_suffix(".dll.a").exists() else [])
            if project_dll and len(objects) == 1:
                objects.append(str(library))
            invoke(name + "-link", expand(mode, "link", source, exe, objects))
            found = imports(name + "-imports", exe)
            case["imports"] = found
            shared = mode == "executable-shared"
            if shared != ("libc.dll" in found):
                raise ValueError("incorrect direct CRT runtime import: " + str(found))
            text = invoke(name + "-run", [str(exe)], environment)
            if "sdk_c_provider: ok" not in text:
                raise ValueError("missing runtime success marker")
            loaded = [line.split(":", 1)[1] for line in text.splitlines() if line.startswith("LIBC_DLL:")]
            expected = [str((sdk_bin / "libc.dll").resolve())] if shared else ["none"]
            if [str(Path(p).resolve()) if p != "none" else p for p in loaded] != expected:
                raise ValueError(f"wrong loaded libc: {loaded}, expected {expected}")
            case["loaded"] = loaded
            if shared:
                # Without the SDK bin directory nothing may stand in for libc.dll.
                bare = {"SystemRoot": os.environ["SystemRoot"], "PATH": str(system)}
                invoke(name + "-no-provider", [str(exe)], bare, expect_failure=True)
            case["passed"] = True
        except (ValueError, subprocess.SubprocessError) as error:
            case["error"] = str(error)
    unchanged = inventory(sdk) == before
    result = {"passed": unchanged and all(c["passed"] for c in cases), "target": contract["target"],
              "sdk_content_sha256": contract["sdk_content_sha256"], "contract_sha256": args.contract_sha256,
              "sdk_unchanged": unchanged, "dll_imports": dll_imports, "cases": cases, "commands": commands}
    (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print("Windows C provider:", ", ".join(c["name"] + "=" + str(c["passed"]) for c in cases))
    for case in cases:
        if "error" in case:
            print(case["name"] + ": " + case["error"])
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
