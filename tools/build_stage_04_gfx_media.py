#!/usr/bin/env python3
"""Build an option-ON cumulative 04-gfx-media SDK from 03-gfx-simple."""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def run(command: list[str], env: dict[str, str] | None = None,
        cwd: Path | None = None) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, check=True, env=env, cwd=cwd)


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
    target = manifest["target"]
    required = manifest.get("external_toolchain_environment", [])
    missing = [name for name in required if not os.environ.get(name)]
    if missing:
        raise SystemExit(
            "external toolchain environment is incomplete; set before "
            "starting the stage build: " + ", ".join(missing))
    env.update({
        "CRT_SYSROOT": str(sdk),
        "CRT_ROOTFS": str(sdk),
        "CRT_TARGET_OS": target["os"],
        "CRT_TARGET_ARCH": target["arch"],
        # The predecessor may have been packaged before crt-c++ learned to
        # infer the cumulative SDK's canonical libc++ include directory.
        # Preserve the wrapper's required C++-before-C include order while
        # bootstrapping the next stage from any otherwise-valid 03 SDK.
        "CRT_CXX_STANDARD_INCLUDE_FLAGS":
            f"-isystem{sdk / 'include' / 'c++' / 'v1'}",
    })
    if os.environ.get("CRT_CC"):
        env["CRT_HOST_CC"] = Path(os.environ["CRT_CC"]).as_posix()
    if os.environ.get("CRT_CXX"):
        env["CRT_HOST_CXX"] = Path(os.environ["CRT_CXX"]).as_posix()
    if os.environ.get("CRT_AR"):
        env["CRT_HOST_AR"] = Path(os.environ["CRT_AR"]).as_posix()
    if target["os"] == "windows":
        env["CRT_MKSH_EXE"] = str(sdk / "system" / "bin" / "mksh.exe")
        env["CRT_HOST_PYTHON"] = sys.executable
        env["PATH"] = str(sdk / "bin") + os.pathsep + env.get("PATH", "")
    return env


def first_existing(root: Path, relatives: tuple[str, ...]) -> Path:
    for relative in relatives:
        candidate = root / relative
        if candidate.is_file():
            return candidate
    raise SystemExit(f"none of the required notice files exist under {root}: {relatives}")


def copy_dependency_record(staged: Path, name: str, recipe: Path,
                           notice: Path, headers: list[str],
                           links: list[str], runtimes: list[str]) -> dict:
    notice_relative = Path("share") / "licenses" / name / notice.name
    notice_dest = staged / notice_relative
    notice_dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(notice, notice_dest)

    provenance_relative = Path("share") / "crt" / "dependencies" / name / "recipe.json"
    provenance_dest = staged / provenance_relative
    provenance_dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(recipe, provenance_dest)
    return {
        "name": name,
        "kind": "private-static-port",
        "headers": headers,
        "link_artifacts": links,
        "runtime_artifacts": runtimes,
        "notices": [notice_relative.as_posix()],
        "provenance": provenance_relative.as_posix(),
    }


def existing_relative_files(root: Path, patterns: tuple[str, ...]) -> list[str]:
    found: list[str] = []
    for pattern in patterns:
        found.extend(path.relative_to(root).as_posix()
                     for path in root.glob(pattern) if path.is_file())
    return sorted(set(found))


def build_ports(asset: Path, staged: Path, temp_root: Path,
                manifest: dict, env: dict[str, str]) -> None:
    source_root = temp_root / "port-sources"
    # crt-port-build.py is intentionally build-only. Repository-mode CMake
    # normally gives it sources via port-fetch-* dependencies; an extracted
    # SDK has no such target graph, so reproduce that edge explicitly with
    # the packaged, checksum-verifying fetch driver.
    run([
        sys.executable, str(staged / "tools" / "fetch_ports.py"),
        "--dest", str(source_root),
        "--cache", str(Path(asset).parent / "port-downloads"),
        "--port", "freetype",
        "--port", "ffmpeg",
    ], env, asset)
    driver = staged / "tools" / "crt-port-build.py"
    command = [
        sys.executable, str(driver),
        "--sdk-root", str(staged),
        "--target-os", manifest["target"]["os"],
        "--target-arch", manifest["target"]["arch"],
        "--source-root", str(source_root),
        "--work-root", str(temp_root / "port-build"),
        "--install-prefix", str(staged),
        # NOT a fix for the "can't fork - try again" failure this stage hit
        # here -- confirmed directly (2026-09-10) by reproducing the exact
        # same failure with --jobs 1 already in effect. Root-caused instead:
        # a real Windows-PAL handle leak in __crt_sys_posix_spawn()'s fd-
        # snapshot export/child-duplicate path (libc/src/arch/windows/
        # common/syscall.c), proportional to *pipeline* (`cmd1 | cmd2`)
        # execution specifically -- confirmed with a minimal, deterministic
        # repro (`echo hi | sed ... >/dev/null` in a loop leaks exactly +1
        # real Windows handle per iteration; the same loop with a plain
        # external command or a redirected subshell, no pipe, leaks
        # nothing). FreeType's real autoconf `configure` is pipeline-heavy
        # (the AS_LINENO self-test's `sed | sed` chain, repeated `` `expr
        # ...` `` substitutions) and is the first real port in this stage
        # chain to run enough of that to exhaust CRT_FD_TABLE_SIZE (64).
        # See TODO.md's in-progress note for the full repro and the current
        # best lead on where the leak actually is; --jobs 1 is kept anyway
        # since stage acceptance values a deterministic build over port-
        # level throughput, not because it addresses this failure.
        "--jobs", "1",
        "--port", "freetype",
        "--port", "ffmpeg",
    ]
    run(command, env, asset)


def build_skia(asset: Path, staged: Path, temp_root: Path,
               manifest: dict, env: dict[str, str]) -> tuple[Path, Path | None]:
    recipe_path = asset / "libcrtgfx" / "third_party" / "skia" / "recipe.json"
    recipe = json.loads(recipe_path.read_text(encoding="utf-8"))
    source_spec = recipe["source"]
    source = temp_root / "skia-source"
    fetch = [
        sys.executable, str(asset / "tools" / "fetch_skia.py"),
        "--dest", str(source),
        "--repo", source_spec["repository"],
        "--version", source_spec["version"],
        "--ref", source_spec["ref"],
        "--expected-commit", source_spec["expected_commit"],
    ]
    for sparse_path in source_spec.get("sparse_paths", []):
        fetch.extend(["--sparse-path", sparse_path])
    if source_spec.get("sync_deps"):
        fetch.append("--sync-deps")
    run(fetch, env, asset)

    mingw_checkout: Path | None = None
    build = [
        sys.executable, str(asset / "tools" / "build_skia.py"),
        "--root", str(asset),
        "--source", str(source),
        "--build-dir", str(temp_root / "skia-build"),
        "--install-prefix", str(staged),
        "--sysroot", str(staged),
        "--target-os", manifest["target"]["os"],
        "--target-arch", manifest["target"]["arch"],
        "--freetype-prefix", str(staged),
    ]
    if manifest["target"]["os"] == "windows":
        mingw_checkout = temp_root / "mingw-w64"
        run([
            sys.executable,
            str(asset / "tools" / "fetch_mingw_w64_headers.py"),
            "--dest", str(mingw_checkout),
        ], env, asset)
        build.extend([
            "--rootfs", str(staged),
            "--mingw-w64-headers-root",
            str(mingw_checkout / "mingw-w64-headers" / "include"),
        ])
    run(build, env, asset)
    return source, mingw_checkout


def add_redistributed_dependencies(asset: Path, staged: Path,
                                   port_sources: Path, skia_source: Path,
                                   manifest: dict) -> None:
    freetype_source = next(port_sources.glob("freetype-*"), None)
    ffmpeg_source = next(port_sources.glob("ffmpeg-*"), None)
    if freetype_source is None or ffmpeg_source is None:
        raise SystemExit("the port driver did not leave verified FreeType/FFmpeg sources")

    dependencies = [
        copy_dependency_record(
            staged, "freetype", asset / "porting" / "recipes" / "freetype.json",
            first_existing(freetype_source, ("LICENSE.TXT", "LICENSE", "docs/LICENSE.TXT")),
            ["include/freetype2"], ["lib/libfreetype.a"],
            existing_relative_files(staged, (
                "lib/libfreetype.so*", "lib/libfreetype*.dylib",
                "bin/libfreetype*.dll", "lib/libfreetype*.dll.a"))),
        copy_dependency_record(
            staged, "ffmpeg", asset / "porting" / "recipes" / "ffmpeg.json",
            first_existing(ffmpeg_source, ("COPYING.LGPLv2.1", "COPYING.LGPLv3", "LICENSE.md")),
            ["include/libavformat", "include/libavcodec",
             "include/libswresample", "include/libavutil"],
            ["lib/libavformat.a", "lib/libavcodec.a",
             "lib/libswresample.a", "lib/libavutil.a"],
            existing_relative_files(staged, (
                "lib/libavformat.so*", "lib/libavcodec.so*",
                "lib/libswresample.so*", "lib/libavutil.so*",
                "lib/libavformat*.dylib", "lib/libavcodec*.dylib",
                "lib/libswresample*.dylib", "lib/libavutil*.dylib",
                "bin/avformat*.dll", "bin/avcodec*.dll",
                "bin/swresample*.dll", "bin/avutil*.dll"))),
        copy_dependency_record(
            staged, "skia", asset / "libcrtgfx" / "third_party" / "skia" / "recipe.json",
            first_existing(skia_source, ("LICENSE",)),
            ["include/include", "include/modules/skcms"],
            ["lib/libskia.a"], []),
    ]
    retained = [item for item in manifest.get("redistributed_dependencies", [])
                if item.get("name") not in {"freetype", "ffmpeg", "skia"}]
    manifest["redistributed_dependencies"] = retained + dependencies


def build_example(staged: Path, temp_root: Path, name: str,
                  executable_name: str, env: dict[str, str]) -> None:
    build_dir = temp_root / f"example-{name}"
    run([
        "cmake", "-S", str(staged / "examples" / name),
        "-B", str(build_dir), "-G", "Ninja",
        f"-DCMAKE_TOOLCHAIN_FILE={cmake_path(staged / 'crt-toolchain.cmake')}",
    ], env)
    run(["cmake", "--build", str(build_dir)], env)
    suffix = ".exe" if env["CRT_TARGET_OS"] == "windows" else ""
    run([str(build_dir / f"{executable_name}{suffix}"), "1"], env)


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

    temp_root = Path(tempfile.mkdtemp(prefix="crt-stage-04-gfx-media-"))
    try:
        staged = temp_root / "sdk"
        shutil.copytree(sdk, staged)
        manifest_path = staged / "manifest.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        if manifest.get("stage") != "03-gfx-simple":
            raise SystemExit("04-gfx-media requires a 03-gfx-simple predecessor SDK")
        target_os = manifest["target"]["os"]
        env = runtime_env(staged, manifest)
        if target_os == "windows" and not os.environ.get("CRT_WINDOWS_SDK_LIBPATH"):
            raise SystemExit("CRT_WINDOWS_SDK_LIBPATH is required on Windows")

        # Preserve the path-with-spaces acceptance input, while retaining the
        # same explicit Windows wrapper workaround as stages 02 and 03.
        build_asset = asset
        if target_os == "windows":
            build_asset = temp_root / "source"
            shutil.copytree(asset, build_asset)

        build_ports(build_asset, staged, temp_root, manifest, env)
        skia_source, mingw_checkout = build_skia(
            build_asset, staged, temp_root, manifest, env)

        build_dir = temp_root / "gfx-media-build"
        configure = [
            "cmake", "-S", str(build_asset / "distribution" / "stages" / "04-gfx-media"),
            "-B", str(build_dir), "-G", "Ninja",
            f"-DCMAKE_TOOLCHAIN_FILE={cmake_path(staged / 'crt-toolchain.cmake')}",
            f"-DCMAKE_INSTALL_PREFIX={cmake_path(staged)}",
            f"-DCRT_STAGE_SOURCE_ROOT={cmake_path(build_asset)}",
            f"-DCRT_STAGE_TARGET_OS={target_os}",
            f"-DCRT_STAGE_SKIA_PREFIX={cmake_path(staged)}",
            f"-DCRT_STAGE_FREETYPE_PREFIX={cmake_path(staged)}",
            f"-DCRT_STAGE_FFMPEG_PREFIX={cmake_path(staged)}",
        ]
        if mingw_checkout is not None:
            configure.append(
                "-DCRT_STAGE_MINGW_W64_HEADERS_ROOT=" +
                cmake_path(mingw_checkout / "mingw-w64-headers" / "include"))
        run(configure, env)
        run(["cmake", "--build", str(build_dir)], env)
        run(["ctest", "--test-dir", str(build_dir), "--output-on-failure"], env)
        run(["cmake", "--install", str(build_dir)], env)

        manifest["stage"] = "04-gfx-media"
        manifest["built_from"] = {
            "stage": "03-gfx-simple",
            "source_recipe": args.recipe_location,
            "source_sha256": args.source_sha256,
        }
        add_redistributed_dependencies(
            build_asset, staged, temp_root / "port-sources", skia_source, manifest)
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

        build_example(staged, temp_root, "gfx-gpu", "crtgfx_gpu_example", env)
        build_example(staged, temp_root, "gfx-skia", "crtgfx_skia_example", env)
        run([sys.executable, str(build_asset / "tools" / "verify_dist.py"),
             "--dist", str(staged), "--stage", "04-gfx-media"], env)
        publish_tree(staged, output)
        print(f"CRT stage ready: {output}")
    finally:
        shutil.rmtree(temp_root, ignore_errors=True)


if __name__ == "__main__":
    main()
