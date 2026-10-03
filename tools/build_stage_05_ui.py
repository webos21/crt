#!/usr/bin/env python3
"""Build a cumulative 05-ui SDK from an isolated 04-gfx-media SDK and a source asset.

crtui, its private LVGL renderer, the optional Skia/MediaView companions, their
tests and the installed ui-basic sample are built from the source asset against
the predecessor SDK alone. Only LVGL is fetched live (pinned by
libcrtui/third_party/lvgl/recipe.json); crtgfx, crtmedia, Skia, FreeType, FFmpeg
and the curl chain are the 04 SDK's own installed headers and archives.
"""

import argparse
from contextlib import contextmanager
import json
import os
import shutil
import stat
import subprocess
import sys
import tempfile
import time
from pathlib import Path

from crt_dist_prerequisites import external_prerequisites_for
from crt_elf import relocate_absolute_runtime_paths, remove_absolute_runtime_paths


def run(command: list[str], env: dict[str, str] | None = None,
        cwd: Path | None = None) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, check=True, env=env, cwd=cwd)


class PhaseTimings:
    """Print and retain coarse stage timings without changing build behavior."""

    def __init__(self) -> None:
        self.started = time.monotonic()
        self.records: list[tuple[str, float, str]] = []

    @contextmanager
    def measure(self, name: str):
        print(f"==> {name}", flush=True)
        started = time.monotonic()
        status = "ok"
        try:
            yield
        except BaseException:
            status = "failed"
            raise
        finally:
            elapsed = time.monotonic() - started
            self.records.append((name, elapsed, status))
            print(f"<== {name}: {status} ({elapsed:.1f}s)", flush=True)

    def report(self) -> None:
        if not self.records:
            return
        print("Stage phase timing summary:", flush=True)
        for name, elapsed, status in self.records:
            print(f"  {elapsed:9.1f}s  {status:6s}  {name}", flush=True)
        print(f"  {time.monotonic() - self.started:9.1f}s  total", flush=True)


def cmake_path(path: Path) -> str:
    return path.resolve().as_posix()


def remove_readonly_and_retry(function, path: str, _exc_info) -> None:
    """Permit removal of read-only files left by Windows source checkouts."""
    os.chmod(path, stat.S_IWRITE)
    function(path)


def remove_tree(path: Path, ignore_errors: bool = False) -> None:
    shutil.rmtree(
        path, ignore_errors=ignore_errors,
        onerror=None if ignore_errors else remove_readonly_and_retry)


def touch_tree(root: Path) -> None:
    """Stamp every file under root with the current time.

    The extracted stage-source tarball carries normalized epoch-0 mtimes
    (reproducible packaging), which defeats CMake's install(FILES ...) staleness
    check against an older installed copy; see build_stage_04_gfx_media.py.
    """
    for path in root.rglob("*"):
        if path.is_file():
            os.utime(path, None)


# The Mach-O / ELF / publish helpers below are intentionally the same as the
# 03/04 entrypoints': each stage asset bundles its own standalone entrypoint.
# Their long rationale lives in build_stage_03_gfx_simple.py and
# build_stage_04_gfx_media.py. macOS and Linux behavior is UNVERIFIED in this
# stage until the replays on those hosts.
_MACHO_MAGICS = frozenset((
    b"\xfe\xed\xfa\xce", b"\xce\xfa\xed\xfe",
    b"\xfe\xed\xfa\xcf", b"\xcf\xfa\xed\xfe",
    b"\xca\xfe\xba\xbe", b"\xbe\xba\xfe\xca",
))


def rewrite_stale_macho_rpaths(root: Path, old_root: Path, new_root: Path) -> None:
    """install_name_tool -rpath every Mach-O under root whose LC_RPATH still
    points under old_root (this stage is built in a throwaway temp dir and then
    moved to --output), to the equivalent path under new_root."""
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
                 if value == old_str or value.startswith(old_str + os.sep)), None)
            if matched_old is None:
                continue
            new_value = new_str + value[len(matched_old):]
            if new_value in existing_rpaths:
                # dyld rejects two byte-identical LC_RPATH commands.
                subprocess.run(
                    ["install_name_tool", "-delete_rpath", value, str(path)], check=True)
            else:
                subprocess.run(
                    ["install_name_tool", "-rpath", value, new_value, str(path)], check=True)
                existing_rpaths.add(new_value)


def portable_macho_dependency_paths(root: Path) -> None:
    """Make Mach-O dylib ids/dependencies installed under `root` relocatable."""
    root_strs = {str(root), str(root.resolve())}

    def under_root(value: str) -> bool:
        return any(value == r or value.startswith(r + os.sep) for r in root_strs)

    for path in root.rglob("*"):
        if path.is_symlink() or not path.is_file():
            continue
        try:
            with path.open("rb") as handle:
                magic = handle.read(4)
        except OSError:
            continue
        if magic not in _MACHO_MAGICS or path.suffix == ".a":
            continue
        ident = subprocess.run(
            ["otool", "-D", str(path)], capture_output=True, text=True, check=False)
        if ident.returncode != 0:
            continue
        id_lines = ident.stdout.splitlines()[1:]
        if id_lines and under_root(id_lines[0].strip()):
            subprocess.run(
                ["install_name_tool", "-id",
                 "@rpath/" + Path(id_lines[0].strip()).name, str(path)], check=True)
        deps = subprocess.run(
            ["otool", "-L", str(path)], capture_output=True, text=True, check=False)
        for line in deps.stdout.splitlines()[1:]:
            dep = line.strip().split(" (compatibility", 1)[0]
            if under_root(dep):
                subprocess.run(
                    ["install_name_tool", "-change", dep,
                     "@rpath/" + Path(dep).name, str(path)], check=True)


def remove_staged_absolute_elf_rpaths(root: Path) -> None:
    """Keep packaged ELF runtime paths relocatable."""
    bin_dir = root / "bin"
    if bin_dir.is_dir():
        for path in bin_dir.iterdir():
            if path.is_symlink() or not path.is_file():
                continue
            relocate_absolute_runtime_paths(path, "$ORIGIN/../lib")
    for path in root.rglob("*"):
        if path.is_symlink() or not path.is_file():
            continue
        remove_absolute_runtime_paths(path, require_origin=True)


def publish_tree(staged: Path, output: Path) -> None:
    """Copy beside the destination, then expose the final name atomically."""
    publish_root = Path(tempfile.mkdtemp(
        prefix=f".{output.name}-publish-", dir=output.parent))
    candidate = publish_root / output.name
    try:
        shutil.copytree(staged, candidate)
        if sys.platform == "darwin":
            rewrite_stale_macho_rpaths(candidate, staged, output)
        os.replace(candidate, output)
    finally:
        remove_tree(publish_root, ignore_errors=True)


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
        "CRT_CXX_STANDARD_INCLUDE_FLAGS":
            f"-isystem{sdk / 'include' / 'c++' / 'v1'}",
    })
    # Forward-slash paths: the Windows wrapper runs under mksh, which only treats
    # a command as a literal path when it contains a forward slash.
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


def llvm_bin_directory() -> Path:
    """Directory holding llvm-nm/llvm-readobj (the privacy check needs both).

    The host compiler is an LLVM clang, so its own directory is the first place
    to look; fall back to PATH.
    """
    compiler = os.environ.get("CRT_CC")
    if compiler:
        candidate = Path(compiler).resolve().parent
        if any(candidate.glob("llvm-nm*")):
            return candidate
    found = shutil.which("llvm-nm")
    if found:
        return Path(found).resolve().parent
    raise SystemExit("llvm-nm/llvm-readobj were not found next to CRT_CC or on PATH; "
                     "the crtui privacy check needs them")


def require_isolated_gfx_media(sdk: Path, manifest: dict) -> None:
    """05-ui consumes Skia/FFmpeg/curl archives: only the option-ON isolated 04
    SDK has them. The ordinary cumulative 04 package is default-OFF and lacks
    them, so say so up front instead of failing deep inside CMake."""
    if manifest.get("stage") != "04-gfx-media":
        raise SystemExit("05-ui requires a 04-gfx-media predecessor SDK")
    if (manifest.get("built_from") or {}).get("stage") != "03-gfx-simple":
        raise SystemExit(
            "05-ui requires the isolated, option-ON 04-gfx-media SDK (built by "
            "tools/crt-stage-build.py from 03-gfx-simple); the ordinary cumulative "
            "04 package has no Skia/FFmpeg")
    for relative in ("lib/libskia.a", "lib/libcrtgfx_skia.a", "lib/libcrtmedia.a",
                     "include/include/core/SkSurface.h", "include/crtmedia/gpu_frame.h"):
        if not (sdk / relative).is_file():
            raise SystemExit(f"predecessor SDK is missing {relative}")


def fetch_lvgl(asset: Path, temp_root: Path, download_cache: Path,
               env: dict[str, str]) -> tuple[Path, Path]:
    """Fetch and verify the pinned LVGL release; return (source root, recipe).

    The ~111 MB archive is cached in download_cache (kept between runs, still
    re-verified against the recipe's size and SHA-256 every time); only the
    extracted tree lives in the disposable temp_root.
    """
    recipe = asset / "libcrtui" / "third_party" / "lvgl" / "recipe.json"
    destination = temp_root / "lvgl"
    run([sys.executable, str(asset / "tools" / "fetch_lvgl.py"),
         "--recipe", str(recipe), "--dest", str(destination),
         "--cache", str(download_cache)], env)
    source_dir = json.loads(recipe.read_text(encoding="utf-8"))["source"]["source_dir"]
    root = destination / source_dir
    if not (root / "include" / "lvgl" / "lvgl.h").is_file():
        raise SystemExit(f"LVGL fetch did not produce {root / 'include/lvgl/lvgl.h'}")
    return root, recipe


def add_lvgl_dependency(staged: Path, lvgl_root: Path, recipe: Path, manifest: dict) -> None:
    """Declare LVGL as a private static dependency (MIT notice + pinned recipe).

    LVGL is linked inside libcrtui and installs no header or library of its own,
    so only the notice and provenance are packaged.
    """
    notice = lvgl_root / "LICENCE.txt"
    if not notice.is_file():
        raise SystemExit(f"LVGL license notice is missing: {notice}")
    notice_relative = Path("share") / "licenses" / "lvgl" / notice.name
    (staged / notice_relative).parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(notice, staged / notice_relative)
    provenance_relative = Path("share") / "crt" / "dependencies" / "lvgl" / "recipe.json"
    (staged / provenance_relative).parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(recipe, staged / provenance_relative)
    retained = [item for item in manifest.get("redistributed_dependencies", [])
                if item.get("name") != "lvgl"]
    retained.append({
        "name": "lvgl",
        "kind": "private-static-port",
        "headers": [],
        "link_artifacts": [],
        "runtime_artifacts": [],
        "notices": [notice_relative.as_posix()],
        "provenance": provenance_relative.as_posix(),
    })
    manifest["redistributed_dependencies"] = retained


def run_with_marker(command: list[str], env: dict[str, str],
                    markers: tuple[str, ...], what: str) -> None:
    """Run a bounded program and require its own report of success, not just exit 0."""
    print("+", " ".join(command), flush=True)
    result = subprocess.run(
        command, check=True, env=env, timeout=120,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    print(result.stdout, end="", flush=True)
    missing = [marker for marker in markers if marker not in result.stdout]
    if missing:
        raise SystemExit(f"{what} did not report success (expected {missing!r})")


# The window demo prints this when its scripted pixel and input checks pass.
DEMO_MARKERS = ("presented=30", "pixel_check=pass", "input_check=pass")


def build_example(staged: Path, temp_root: Path, env: dict[str, str]) -> None:
    build_dir = temp_root / "example-ui-basic"
    if build_dir.exists():
        remove_tree(build_dir)
    run(["cmake", "-S", str(staged / "examples" / "ui-basic"),
         "-B", str(build_dir), "-G", "Ninja",
         f"-DCMAKE_TOOLCHAIN_FILE={cmake_path(staged / 'crt-toolchain.cmake')}"], env)
    run(["cmake", "--build", str(build_dir)], env)
    suffix = ".exe" if env["CRT_TARGET_OS"] == "windows" else ""
    run_with_marker([str(build_dir / f"crtui_basic_example{suffix}"), "30"], env,
                    DEMO_MARKERS, "externally rebuilt examples/ui-basic")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-root", required=True, type=Path)
    parser.add_argument("--asset-root", required=True, type=Path)
    parser.add_argument("--work-root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--recipe-location", required=True)
    parser.add_argument("--source-sha256", required=True)
    # crt-stage-build.py forwards these only if given; accepted so a shared
    # invocation works for every stage. 05-ui has no multi-hour dependency layer.
    parser.add_argument("--reuse-work-root", action="store_true",
                        help="Keep --work-root (and the fetched LVGL) after the run.")
    parser.add_argument("--dependency-jobs", type=int,
                        help="Accepted for crt-stage-build.py compatibility; unused.")
    args = parser.parse_args()

    timings = PhaseTimings()
    sdk = args.sdk_root.resolve()
    asset = args.asset_root.resolve()
    output = args.output.resolve()
    if output.exists():
        raise SystemExit(f"output already exists: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    work_root = args.work_root.resolve()
    work_root.mkdir(parents=True, exist_ok=True)

    manifest = json.loads((sdk / "manifest.json").read_text(encoding="utf-8"))
    require_isolated_gfx_media(sdk, manifest)
    target_os = manifest["target"]["os"]
    if target_os == "windows" and not os.environ.get("CRT_WINDOWS_SDK_LIBPATH"):
        raise SystemExit("CRT_WINDOWS_SDK_LIBPATH is required on Windows")

    touch_tree(asset)
    temp_root = Path(tempfile.mkdtemp(prefix="crt-stage-05-ui-", dir=work_root))
    try:
        staged = temp_root / "sdk"
        with timings.measure("prepare clean predecessor SDK copy"):
            # work_root may be nested inside the SDK (the documented invocation
            # shape); copying the SDK into one of its own descendants would
            # recurse until the path-length limit, so skip it by resolved path.
            work_root_resolved = work_root.resolve()

            def skip_work_root(directory: str, names):
                base = Path(directory).resolve()
                return {name for name in names if (base / name).resolve() == work_root_resolved}

            shutil.copytree(sdk, staged, ignore=skip_work_root)
        manifest_path = staged / "manifest.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        env = runtime_env(staged, manifest)

        with timings.measure("fetch and verify LVGL"):
            lvgl_root, lvgl_recipe = fetch_lvgl(
                asset, temp_root, work_root / "downloads" / "lvgl", env)

        build_dir = temp_root / "ui-build"
        with timings.measure("configure crtui stage"):
            run(["cmake", "-S", str(asset / "distribution" / "stages" / "05-ui"),
                 "-B", str(build_dir), "-G", "Ninja",
                 f"-DCMAKE_TOOLCHAIN_FILE={cmake_path(staged / 'crt-toolchain.cmake')}",
                 f"-DCMAKE_INSTALL_PREFIX={cmake_path(staged)}",
                 f"-DCRT_STAGE_SOURCE_ROOT={cmake_path(asset)}",
                 f"-DCRT_STAGE_TARGET_OS={target_os}",
                 f"-DCRT_STAGE_LVGL_ROOT={cmake_path(lvgl_root)}",
                 f"-DCRT_STAGE_PYTHON={cmake_path(Path(sys.executable))}",
                 f"-DCRT_STAGE_LLVM_BIN={cmake_path(llvm_bin_directory())}"], env)
        with timings.measure("build crtui, companions and tests"):
            run(["cmake", "--build", str(build_dir)], env)
        with timings.measure("ctest"):
            run(["ctest", "--test-dir", str(build_dir), "--output-on-failure"], env)
        with timings.measure("install"):
            run(["cmake", "--install", str(build_dir)], env)

        if target_os == "macos":
            rewrite_stale_macho_rpaths(staged, staged, staged)
            portable_macho_dependency_paths(staged)
        elif target_os == "linux":
            remove_staged_absolute_elf_rpaths(staged)

        with timings.measure("write dependency provenance and manifest"):
            manifest["stage"] = "05-ui"
            manifest["external_prerequisites"] = external_prerequisites_for(target_os, "05-ui")
            manifest["built_from"] = {
                "stage": "04-gfx-media",
                "source_recipe": args.recipe_location,
                "source_sha256": args.source_sha256,
            }
            add_lvgl_dependency(staged, lvgl_root, lvgl_recipe, manifest)
            manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

        with timings.measure("rebuild and run installed ui-basic example"):
            build_example(staged, temp_root, env)
        with timings.measure("run packaged crtui_window_demo directly"):
            suffix = ".exe" if target_os == "windows" else ""
            run_with_marker(
                [str(staged / "examples" / "bin" / f"crtui_window_demo{suffix}"), "30"],
                env, DEMO_MARKERS, "packaged examples/bin/crtui_window_demo")
        with timings.measure("verify 05-ui distribution"):
            run([sys.executable, str(asset / "tools" / "verify_dist.py"),
                 "--dist", str(staged), "--stage", "05-ui"], env)
        with timings.measure("publish 05-ui atomically"):
            publish_tree(staged, output)
        print(f"CRT stage ready: {output}")
    finally:
        # CRT_STAGE_KEEP_TMP=1 keeps the build tree and staged SDK copy for
        # post-mortem inspection after a failure.
        if os.environ.get("CRT_STAGE_KEEP_TMP"):
            print(f"CRT_STAGE_KEEP_TMP set: keeping {temp_root}")
        elif not args.reuse_work_root:
            remove_tree(temp_root, ignore_errors=True)
        timings.report()


if __name__ == "__main__":
    main()
