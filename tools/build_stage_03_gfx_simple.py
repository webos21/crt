#!/usr/bin/env python3
"""Build a cumulative 03-gfx-simple SDK from 02-cxx and a source asset."""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def run(command: list[str], env: dict[str, str] | None = None) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, check=True, env=env)


def cmake_path(path: Path) -> str:
    return path.resolve().as_posix()


def publish_tree(staged: Path, output: Path) -> None:
    """Copy beside the destination, then expose the final name atomically."""
    publish_root = Path(tempfile.mkdtemp(
        prefix=f".{output.name}-publish-", dir=output.parent
    ))
    candidate = publish_root / output.name
    try:
        shutil.copytree(staged, candidate)
        os.replace(candidate, output)
    finally:
        shutil.rmtree(publish_root, ignore_errors=True)


def runtime_env(sdk: Path, manifest: dict) -> dict[str, str]:
    env = os.environ.copy()
    target_os = manifest["target"]["os"]
    env.update({
        "CRT_SYSROOT": str(sdk),
        "CRT_ROOTFS": str(sdk),
        "CRT_TARGET_OS": target_os,
        "CRT_TARGET_ARCH": manifest["target"]["arch"],
    })
    if os.environ.get("CRT_CC"):
        env["CRT_HOST_CC"] = os.environ["CRT_CC"]
    if os.environ.get("CRT_CXX"):
        env["CRT_HOST_CXX"] = os.environ["CRT_CXX"]
    if target_os == "windows":
        env["CRT_MKSH_EXE"] = str(sdk / "system" / "bin" / "mksh.exe")
        env["PATH"] = str(sdk / "bin") + os.pathsep + env.get("PATH", "")
    elif target_os == "macos":
        env["DYLD_LIBRARY_PATH"] = str(sdk / "lib")
    else:
        env["LD_LIBRARY_PATH"] = str(sdk / "lib")
    return env


def install_xkbcommon(asset: Path, staged: Path, temp_root: Path,
                      manifest: dict, env: dict[str, str]) -> None:
    source = asset / "sources" / "xkbcommon" / "src"
    build = temp_root / "xkbcommon-build"
    driver = asset / "tools" / "build_xkbcommon.py"
    command = [
        sys.executable, str(driver),
        "--root", str(asset),
        "--source", str(source),
        "--build-dir", str(build),
        "--install-prefix", str(staged),
        "--sysroot", str(staged),
        "--target-os", "linux",
        "--target-arch", manifest["target"]["arch"],
    ]
    run(command, env)

    license_source = source / "LICENSE"
    license_dest = staged / "share" / "licenses" / "xkbcommon" / "LICENSE"
    license_dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(license_source, license_dest)
    provenance = staged / "share" / "crt" / "dependencies" / "xkbcommon"
    provenance.mkdir(parents=True, exist_ok=True)
    shutil.copy2(asset / "libcrtgfx" / "third_party" / "xkbcommon" / "recipe.json",
                 provenance / "recipe.json")
    manifest.setdefault("redistributed_dependencies", []).append({
        "name": "xkbcommon",
        "kind": "private-static-port",
        "headers": ["include/xkbcommon"],
        "link_artifacts": ["lib/libxkbcommon.a"],
        "runtime_artifacts": [],
        "notices": ["share/licenses/xkbcommon/LICENSE"],
        "provenance": "share/crt/dependencies/xkbcommon/recipe.json",
    })


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
    output = args.output.resolve()
    if output.exists():
        raise SystemExit(f"output already exists: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    Path(args.work_root).resolve().mkdir(parents=True, exist_ok=True)

    temp_root = Path(tempfile.mkdtemp(prefix="crt-stage-03-gfx-simple-"))
    staged = temp_root / "sdk"
    shutil.copytree(sdk, staged)
    manifest_path = staged / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    target_os = manifest["target"]["os"]
    env = runtime_env(staged, manifest)

    # The verified source asset may be extracted below a path containing
    # spaces, but the current Windows crt-cc.cmd -> mksh -> crt-cc chain
    # still flattens quoted compiler argv. Keep this the same explicit,
    # Windows-only workaround used by build_stage_02_cxx.py: preserve the
    # acceptance input/output paths, but compile from a short temporary copy.
    # This does not claim that the general wrapper limitation is fixed.
    build_asset = asset
    if target_os == "windows":
        build_asset = temp_root / "source"
        shutil.copytree(asset, build_asset)

    if target_os == "linux":
        install_xkbcommon(build_asset, staged, temp_root, manifest, env)

    build_dir = temp_root / "gfx-build"
    configure = [
        "cmake", "-S", str(build_asset / "distribution" / "stages" / "03-gfx-simple"),
        "-B", str(build_dir), "-G", "Ninja",
        f"-DCMAKE_TOOLCHAIN_FILE={cmake_path(staged / 'crt-toolchain.cmake')}",
        f"-DCMAKE_INSTALL_PREFIX={cmake_path(staged)}",
        f"-DCRT_STAGE_SOURCE_ROOT={cmake_path(build_asset)}",
        f"-DCRT_STAGE_TARGET_OS={target_os}",
    ]
    if target_os == "linux":
        configure.append(f"-DCRT_STAGE_XKBCOMMON_PREFIX={cmake_path(staged)}")
    elif target_os == "windows" and not os.environ.get("CRT_WINDOWS_SDK_LIBPATH"):
        raise SystemExit("CRT_WINDOWS_SDK_LIBPATH is required on Windows")
    run(configure, env)
    run(["cmake", "--build", str(build_dir)], env)
    run(["ctest", "--test-dir", str(build_dir), "--output-on-failure"], env)
    run(["cmake", "--install", str(build_dir)], env)

    manifest["stage"] = "03-gfx-simple"
    manifest["built_from"] = {
        "stage": "02-cxx",
        "source_recipe": args.recipe_location,
        "source_sha256": args.source_sha256,
    }
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

    example_build = temp_root / "example-build"
    run([
        "cmake", "-S", str(staged / "examples" / "gfx-simple"),
        "-B", str(example_build), "-G", "Ninja",
        f"-DCMAKE_TOOLCHAIN_FILE={cmake_path(staged / 'crt-toolchain.cmake')}",
    ], env)
    run(["cmake", "--build", str(example_build)], env)
    example = example_build / ("crtgfx_window_example.exe" if target_os == "windows"
                               else "crtgfx_window_example")
    run([str(example), "1"], env)
    run([sys.executable, str(build_asset / "tools" / "verify_dist.py"),
         "--dist", str(staged), "--stage", "03-gfx-simple"], env)

    publish_tree(staged, output)
    shutil.rmtree(temp_root)
    print(f"CRT stage ready: {output}")


if __name__ == "__main__":
    main()
