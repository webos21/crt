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


# Mach-O magic numbers (32/64-bit, either endianness) and the fat
# (universal) binary magic -- see rewrite_stale_macho_rpaths()'s own
# comment (this file's copy is intentionally identical to tools/
# build_stage_03_gfx_simple.py's/build_stage_04_gfx_media.py's -- each
# staged/packaged stage source bundles its own standalone entrypoint).
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
    build in place" case) -- so any RPATH-bearing binary built while
    old_root was still "where this SDK lives" gets old_root's own (about-
    to-be-deleted) path baked in, not new_root's. A real, confirmed bug
    (2026-09-11), first found on the sibling 03-gfx-simple/04-gfx-media
    stages' own published executables ("Library not loaded: @rpath/
    libcrtgfx_skia.dylib ... tried: '<old_root>/lib/libcrtgfx_skia.dylib'
    (no such file)" once old_root, already deleted, was gone) -- this
    stage's own imported libc++.dylib/libc++abi.dylib currently carry no
    LC_RPATH at all (confirmed directly against a real built 02-cxx dist,
    2026-09-11), so this is a no-op here today, but is added for the same
    reason and kept identical to the sibling stages' copies in case that
    ever changes (e.g. a future libc++ build config that does embed one).
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

    # verify_dist.py requires a packaged 02-cxx SDK to carry the pinned
    # recipe that advances it one stage further, the same cumulative-chain
    # contract 01-c's own dist already satisfies for this stage -- see
    # tools/create_stage_source.py's own --successor-recipe comment for the
    # full "why" and the real, confirmed bug (2026-09-11) this closes.
    # create_stage_source.py already embedded it inside this stage's own
    # source asset (CMakeLists.txt's crt-stage-02-source target), at
    # exactly this same relative path, so it only needs copying forward.
    successor_recipe = asset / "stages" / "recipes" / "03-gfx-simple.json"
    if successor_recipe.is_file():
        recipe_dest = staged / "stages" / "recipes" / "03-gfx-simple.json"
        recipe_dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(successor_recipe, recipe_dest)

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
    if sys.platform == "darwin":
        # See rewrite_stale_macho_rpaths()'s own comment. Rewritten after
        # the copy (unlike the sibling 03/04 stages' own publish_tree(),
        # this stage has no atomic publish_root staging step to rewrite
        # under first) -- a pre-existing gap in this stage's own publish
        # atomicity, not something this fix changes.
        rewrite_stale_macho_rpaths(output, staged, output)
    shutil.rmtree(temp_root)
    print(f"CRT stage ready: {output}")


if __name__ == "__main__":
    main()
