#!/usr/bin/env python3
"""Build a cumulative 02-cxx SDK from a 01-c SDK and a source asset."""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def run(command: list[str]) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, check=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-root", required=True, type=Path)
    parser.add_argument("--asset-root", required=True, type=Path)
    parser.add_argument("--work-root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--recipe-location", required=True)
    parser.add_argument("--source-sha256", required=True)
    args = parser.parse_args()

    sdk = args.sdk_root.resolve()
    asset = args.asset_root.resolve()
    work = args.work_root.resolve()
    output = args.output.resolve()
    if output.exists():
        raise SystemExit(f"output already exists: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    # Keep partial products away from the requested output. The imported
    # runtime is first built and tested in a short host-temporary path; only a
    # completely verified cumulative SDK is copied into place. The verified
    # asset is allowed to live in a path containing spaces, but the current
    # POSIX-shell Windows wrappers still flatten compiler argv internally.
    # Copy build inputs to a short temporary path until that general wrapper
    # limitation is fixed and exercised separately by consumer acceptance.
    temp_root = Path(tempfile.mkdtemp(prefix="crt-stage-02-cxx-"))
    staged = temp_root / "sdk"
    shutil.copytree(sdk, staged)
    integration = temp_root / "integration"
    shutil.copytree(asset / "tools", integration / "tools")
    shutil.copytree(asset / "libstdc++", integration / "libstdc++")
    source_root = temp_root / "sources"
    shutil.copytree(asset / "sources", source_root)

    manifest_path = staged / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    target_os = manifest["target"]["os"]
    target_arch = manifest["target"]["arch"]
    build_root = temp_root / "runtime-build"
    install_prefix = temp_root / "runtime-install"
    driver = integration / "tools" / "crt-libcxx-build.py"
    common = [
        sys.executable, str(driver),
        "--root", str(integration),
        "--recipe-root", str(integration / "libstdc++" / "third_party"),
        "--source-root", str(source_root),
        "--build-root", str(build_root),
        "--install-prefix", str(install_prefix),
        "--sysroot", str(staged),
        "--target-os", target_os,
        "--target-arch", target_arch,
    ]
    if target_os == "windows":
        common += ["--rootfs", str(staged)]
        sdk_libpath = os.environ.get("CRT_WINDOWS_SDK_LIBPATH")
        if sdk_libpath:
            common += ["--windows-sdk-libpath", sdk_libpath]
    if os.environ.get("CRT_CC"):
        common += ["--host-cc", os.environ["CRT_CC"]]
    if os.environ.get("CRT_CXX"):
        common += ["--host-cxx", os.environ["CRT_CXX"]]
    run(common + ["--phase", "configure"])
    run(common + ["--phase", "build"])
    run([
        sys.executable, str(integration / "tools" / "install_libcxx_runtimes.py"),
        "--install-prefix", str(install_prefix), "--sysroot", str(staged),
    ])

    manifest["stage"] = "02-cxx"
    manifest["built_from"] = {
        "stage": "01-c",
        "source_recipe": args.recipe_location,
        "source_sha256": args.source_sha256,
    }
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

    # The smoke executable also passes through the current shell-based
    # compiler wrapper. Keep its path in the same short temporary tree as the
    # runtime build; --work-root remains the runner-owned location for future
    # diagnostics and stages whose tools can preserve arbitrary argv.
    work.mkdir(parents=True, exist_ok=True)
    smoke = temp_root / ("imported_libcxx_test.exe" if target_os == "windows" else "imported_libcxx_test")
    test_command = [
        sys.executable, str(integration / "tools" / "test_libcxx_runtime.py"),
        "--root", str(integration), "--sysroot", str(staged),
        "--target-os", target_os, "--output", str(smoke),
    ]
    if target_os == "windows":
        test_command += ["--rootfs", str(staged)]
        if os.environ.get("CRT_WINDOWS_SDK_LIBPATH"):
            test_command += ["--windows-sdk-libpath", os.environ["CRT_WINDOWS_SDK_LIBPATH"]]
    if os.environ.get("CRT_CC"):
        test_command += ["--host-cc", os.environ["CRT_CC"]]
    if os.environ.get("CRT_CXX"):
        test_command += ["--host-cxx", os.environ["CRT_CXX"]]
    run(test_command)
    run([sys.executable, str(integration / "tools" / "verify_dist.py"),
         "--dist", str(staged), "--stage", "02-cxx"])
    shutil.copytree(staged, output)
    shutil.rmtree(temp_root)
    print(f"CRT stage ready: {output}")


if __name__ == "__main__":
    main()
