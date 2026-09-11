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


# Mach-O magic numbers (32/64-bit, either endianness) and the fat
# (universal) binary magic -- see rewrite_stale_macho_rpaths()'s own
# comment (this file's copy is intentionally identical to tools/
# build_stage_04_gfx_media.py's -- each staged/packaged stage source
# bundles its own standalone entrypoint, matching this project's own
# already-established per-stage duplication for publish_tree() itself,
# just above).
_MACHO_MAGICS = frozenset((
    b"\xfe\xed\xfa\xce", b"\xce\xfa\xed\xfe",
    b"\xfe\xed\xfa\xcf", b"\xcf\xfa\xed\xfe",
    b"\xca\xfe\xba\xbe", b"\xbe\xba\xfe\xca",
))


def rewrite_stale_macho_rpaths(root: Path, old_root: Path, new_root: Path) -> None:
    """install_name_tool -rpath's every Mach-O file under root whose own
    LC_RPATH still points somewhere under old_root, rewriting it to the
    equivalent path under new_root.

    macOS-only, and needed for real: this stage is built entirely inside a
    throwaway tempfile.mkdtemp() (old_root, see main()'s own temp_root),
    then copied to the real, requested --output (new_root) only once
    finished. The predecessor SDK's own crt-toolchain.cmake computes
    CRT_DISTRIBUTION_ROOT as `get_filename_component(... "${CMAKE_CURRENT_
    LIST_DIR}" ABSOLUTE)` -- deliberately self-relocating ("wherever this
    SDK happens to be unpacked", by design, for the normal "download once,
    build in place" case) -- and both CMAKE_BUILD_RPATH/CMAKE_INSTALL_
    RPATH and tools/crt-c++'s own `-Wl,-rpath,${CRT_SYSROOT}/lib` derive
    from it. So every RPATH-bearing binary built while old_root was still
    "where this SDK lives" gets old_root's own (about-to-be-deleted) path
    baked in, not new_root's -- a real, confirmed bug (2026-09-11), first
    found on the sibling 04-gfx-media stage's own crtgfx_skia_gpu_window_
    demo ("Library not loaded: @rpath/libcrtgfx_skia.dylib ... tried:
    '<old_root>/lib/libcrtgfx_skia.dylib' (no such file)" once old_root,
    already deleted by main()'s own `finally: shutil.rmtree(temp_root)`,
    was gone) -- this stage's own crtgfx_window_shared.dylib-linked
    consumers share the identical exposure. Note DYLD_LIBRARY_PATH is NOT
    a workaround here -- pointing it at the real lib/ "fixes" that error
    but immediately reintroduces the leaf-name dyld-hijack bug this
    file's own runtime_env() comment (further down this file) already
    documents for a different call site: real Apple frameworks' own
    libc++.1.dylib gets
    silently swapped for this project's ABI-incompatible one.
    dyld's own @rpath resolution consults every already-loaded image's own
    RPATH entries, so fixing only the top-level executable would usually
    be enough in practice -- every Mach-O file in the tree is rewritten
    here anyway regardless, since it costs little and does not depend on
    that aggregation behavior for a dylib some future consumer might
    dlopen() directly."""
    # Two candidate spellings of old_root, both checked below: ld64 bakes
    # the *real*, symlink-resolved path (/private/var/folders/... on
    # macOS) into some LC_RPATH commands (e.g. CMake's own automatic
    # RPATH) but the raw, unresolved tempfile.mkdtemp() spelling (/var/
    # folders/...) into others (tools/crt-c++'s own -Wl,-rpath uses
    # $CRT_SYSROOT verbatim) -- confirmed for real, 2026-09-11: matching
    # only one form left the other spelling's own stale entry (a second,
    # separate LC_RPATH command in the same file) silently unrewritten,
    # dangling at an already-deleted temp path forever, even though the
    # file also carried a second, correctly-rewritten entry that happened
    # to make the binary still run.
    old_strs = {str(old_root), str(old_root.resolve())}
    new_str = str(new_root.resolve())
    for path in root.rglob("*"):
        if path.is_symlink() or not path.is_file():
            continue
        try:
            with path.open("rb") as handle:
                magic = handle.read(4)
        except OSError:
            continue
        if magic not in _MACHO_MAGICS:
            continue
        listing = subprocess.run(
            ["otool", "-l", str(path)], capture_output=True, text=True, check=False)
        lines = listing.stdout.splitlines()
        existing_rpaths = set()
        for index, line in enumerate(lines):
            if line.strip() != "cmd LC_RPATH" or index + 2 >= len(lines):
                continue
            path_line = lines[index + 2].strip()
            if path_line.startswith("path "):
                existing_rpaths.add(path_line[len("path "):].rsplit(" (offset", 1)[0])
        for value in list(existing_rpaths):
            matched_old = next(
                (old_str for old_str in old_strs
                 if value == old_str or value.startswith(old_str + os.sep)),
                None)
            if matched_old is None:
                continue
            new_value = new_str + value[len(matched_old):]
            if new_value in existing_rpaths:
                # A second, differently-spelled RPATH entry for the exact
                # same real directory already exists in this file (e.g.
                # both the raw and the /private-resolved form of the same
                # TMPDIR path -- confirmed for real, 2026-09-11: dyld
                # tolerates two RPATH strings that happen to resolve to
                # one real directory, but rejects two byte-identical
                # LC_RPATH commands outright ("duplicate LC_RPATH") the
                # instant rewriting both stale spellings to the same new
                # path would have made them identical). Drop this one
                # instead of creating that duplicate; the other spelling
                # is either already correct or gets rewritten to the same
                # new_value by its own turn through this same loop.
                subprocess.run(
                    ["install_name_tool", "-delete_rpath", value, str(path)],
                    check=True)
            else:
                subprocess.run(
                    ["install_name_tool", "-rpath", value, new_value, str(path)],
                    check=True)
                existing_rpaths.add(new_value)


def publish_tree(staged: Path, output: Path) -> None:
    """Copy beside the destination, then expose the final name atomically."""
    publish_root = Path(tempfile.mkdtemp(
        prefix=f".{output.name}-publish-", dir=output.parent
    ))
    candidate = publish_root / output.name
    try:
        shutil.copytree(staged, candidate)
        if sys.platform == "darwin":
            # Rewrite while still under the hidden publish_root, targeting
            # the real final `output` path, so the atomic os.replace()
            # below still exposes either a fully-correct tree or nothing
            # -- never a half-patched one visible at the real output path.
            rewrite_stale_macho_rpaths(candidate, staged, output)
        os.replace(candidate, output)
    finally:
        shutil.rmtree(publish_root, ignore_errors=True)


# Deliberately never sets DYLD_LIBRARY_PATH/LD_LIBRARY_PATH pointing at
# this stage's own from-scratch lib/ -- confirmed twice, for real
# (2026-09-09), that this env reaches every subprocess this script spawns
# (configure/build/install *and* ctest, all real host tools -- cmake and
# ctest are both linked against the real macOS system libc++), so setting
# it here breaks the host tool itself before this stage's own C ever gets
# a chance to run: "Symbol not found: __ZNSt12length_errorD1Ev ... Expected
# in: .../sdk/lib/libc++.1.dylib" aborted cmake's own configure, then
# ctest itself the same way once configure/build were fixed to stop
# poisoning cmake/ninja specifically. This stage's own test binaries
# (crtgfx_window_smoke/crtgfx_synthetic_event) do not need it in the end
# anyway -- confirmed via `otool -L`, they link only real, absolute-path
# system frameworks (Foundation/AppKit/.../libobjc/libSystem), no SDK-
# relative dylib at all. A future stage whose own test binaries *do* need
# to find an SDK-relative shared library at runtime should set it as a
# per-test CTest ENVIRONMENT property in that stage's own CMakeLists.txt
# (`set_tests_properties(... PROPERTIES ENVIRONMENT "DYLD_LIBRARY_PATH=...")`)
# instead -- that reaches only the spawned test process, never ctest's own.
def runtime_env(sdk: Path, manifest: dict) -> dict[str, str]:
    env = os.environ.copy()
    target_os = manifest["target"]["os"]
    env.update({
        "CRT_SYSROOT": str(sdk),
        "CRT_ROOTFS": str(sdk),
        "CRT_TARGET_OS": target_os,
        "CRT_TARGET_ARCH": manifest["target"]["arch"],
    })
    # mksh.exe's own exec/command-lookup only recognizes a literal path
    # (vs. a bare $PATH-searched command name) when it contains a forward
    # slash -- a raw Windows backslash path (exactly what a PowerShell
    # $env:CRT_CC assignment naturally produces) makes mksh report
    # "inaccessible or not found" even though the file genuinely exists
    # (confirmed for real, 2026-09; see the project's own mksh/CMake
    # Windows-gotchas notes). crt-cc/crt-c++ run under mksh on Windows and
    # exec "$CRT_HOST_CC"/"$CRT_HOST_CXX" directly, so normalize here.
    if os.environ.get("CRT_CC"):
        env["CRT_HOST_CC"] = Path(os.environ["CRT_CC"]).as_posix()
    if os.environ.get("CRT_CXX"):
        env["CRT_HOST_CXX"] = Path(os.environ["CRT_CXX"]).as_posix()
    if target_os == "windows":
        env["CRT_MKSH_EXE"] = str(sdk / "system" / "bin" / "mksh.exe")
        env["PATH"] = str(sdk / "bin") + os.pathsep + env.get("PATH", "")
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
