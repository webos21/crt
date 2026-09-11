#!/usr/bin/env python3
"""Create a reproducible CRT source-stage asset and its pinned recipe."""

import argparse
import hashlib
import io
import json
import lzma
import stat
import subprocess
import tarfile
from pathlib import Path, PurePosixPath

from crt_stage_recipe import STAGE_SUCCESSORS


STAGES = {
    "02-cxx": {
        "input_stage": "01-c",
        "entrypoint": "tools/build_stage_02_cxx.py",
        "project_paths": (
            "LICENSE.md",
            "tools/build_stage_02_cxx.py",
            "tools/crt-libcxx-build.py",
            "tools/install_libcxx_runtimes.py",
            "tools/test_libcxx_runtime.py",
            # verify_dist.py imports DIST_PORTING_TOOLS/DIST_PORTING_DIRS/
            # DIST_WRAPPER_TOOLS from create_dist.py (added the same day as
            # this stage-source packaging itself, e400ad9 "chained sdk") --
            # a real, confirmed bug (2026-09-09) when this list didn't also
            # carry create_dist.py: the packaged 02-cxx stage's own
            # standalone verify_dist.py run failed outright with
            # "ModuleNotFoundError: No module named 'create_dist'", since
            # nothing else in a real end-user SDK (no full repo checkout)
            # would ever put create_dist.py next to it on sys.path.
            # create_dist.py itself imports TOYBOX_APPLETS from
            # create_rootfs.py, one more transitive hop -- confirmed by
            # actually reading create_dist.py's own imports rather than
            # fixing this one file at a time and hitting the next
            # ModuleNotFoundError on the next run.
            "tools/create_dist.py",
            "tools/create_rootfs.py",
            "tools/verify_dist.py",
            "tools/crt_stage_recipe.py",
            "tools/crt-cc",
            "tools/crt-c++",
            "tools/crt-cc.cmd",
            "tools/crt-c++.cmd",
            "libstdc++/third_party",
            "libstdc++/tests/imported_libcxx_test.cc",
            "libstdc++/tests/imported_libcxx_string_sdk.cc",
            "libstdc++/tests/imported_libcxx_string_abi_test.cc",
        ),
        "source_paths": ("libcxx", "libcxxabi", "libunwind", "cmake", "runtimes", "libc"),
    },
    "03-gfx-simple": {
        "input_stage": "02-cxx",
        "entrypoint": "tools/build_stage_03_gfx_simple.py",
        "project_paths": (
            "LICENSE.md",
            "distribution/stages/03-gfx-simple/CMakeLists.txt",
            "tools/build_stage_03_gfx_simple.py",
            "tools/build_xkbcommon.py",
            "tools/create_dist.py",
            "tools/create_rootfs.py",
            "tools/verify_dist.py",
            "tools/crt_stage_recipe.py",
            "tools/crt-cc",
            "tools/crt-c++",
            "tools/crt-cc.cmd",
            "tools/crt-c++.cmd",
            "libcrtgfx/cmake/crtgfx_window_sources.cmake",
            "libcrtgfx/include/crtgfx/window.h",
            "libcrtgfx/src/window.c",
            "libcrtgfx/src/wayland_weston.c",
            "libcrtgfx/src/wayland_weston_internal.h",
            "libcrtgfx/tests/window_smoke_test.c",
            "libcrtgfx/tests/synthetic_event_test.c",
            "libcrtgfx/tools/window_demo.c",
            "examples/README.md",
            "examples/gfx-simple/CMakeLists.txt",
        ),
        "project_paths_by_os": {
            "windows": (
                "libcrtgfx/src/arch/windows/window_win32.c",
                "libcrtgfx/src/arch/windows/window_win32_gpu.h",
            ),
            "macos": (
                "libcrtgfx/src/arch/macos/window_cocoa.c",
                "libcrtgfx/src/arch/macos/window_cocoa_gpu.h",
            ),
            "linux": (
                "libcrtgfx/src/arch/linux/window_wayland.c",
                # The legacy backend dispatches through this shared interface
                # even when the native Vulkan/Wayland implementation itself
                # is intentionally excluded from the Simple Graphics stage.
                "libcrtgfx/src/arch/linux/window_wayland_native.h",
                "libcrtgfx/third_party/xkbcommon/recipe.json",
                "libcrtgfx/third_party/xkbcommon/generated",
            ),
        },
        "source_paths": (),
        "source_paths_by_os": {"linux": ("xkbcommon/src",)},
    },
    "04-gfx-media": {
        "input_stage": "03-gfx-simple",
        "entrypoint": "tools/build_stage_04_gfx_media.py",
        "project_paths": (
            "LICENSE.md",
            "distribution/stages/04-gfx-media/CMakeLists.txt",
            "tools/build_stage_04_gfx_media.py",
            "tools/build_skia.py",
            "tools/fetch_skia.py",
            "tools/fetch_mingw_w64_headers.py",
            "tools/create_dist.py",
            "tools/create_rootfs.py",
            "tools/verify_dist.py",
            "tools/crt_stage_recipe.py",
            # build_skia.py's GN arguments invoke these from --root. They
            # must come from the verified stage asset, not a full checkout.
            "tools/crt-cc",
            "tools/crt-c++",
            "tools/crt-ar",
            "tools/crt-cc.cmd",
            "tools/crt-c++.cmd",
            "tools/crt-ar.cmd",
            "tools/crt-native-tool",
            "libcrtgfx/cmake/crtgfx_gpu_sources.cmake",
            "libcrtgfx/cmake/crtgfx_gpu_targets.cmake",
            "libcrtgfx/cmake/crtgfx_skia_targets.cmake",
            "libcrtgfx/include",
            "libcrtgfx/src/gpu.c",
            "libcrtgfx/src/gpu_internal.h",
            "libcrtgfx/src/skia_bridge.cc",
            "libcrtgfx/tests/gpu_test.c",
            "libcrtgfx/tests/skia_raster_smoke.cc",
            "libcrtgfx/tests/skia_reference_scene.h",
            "libcrtgfx/tools/gpu_window_demo.c",
            "libcrtgfx/tools/skia_gpu_window_demo.cc",
            "libcrtgfx/assets/fonts",
            "libcrtgfx/third_party/skia",
            "libcrtmedia/cmake/crtmedia_targets.cmake",
            "libcrtmedia/include",
            "libcrtmedia/src/runtime.c",
            "libcrtmedia/src/frame.c",
            "libcrtmedia/src/frame_convert.c",
            "libcrtmedia/src/audio.c",
            "libcrtmedia/src/format.c",
            "libcrtmedia/src/player.c",
            "libcrtmedia/src/gpu_frame.c",
            "libcrtmedia/src/demux.c",
            "libcrtmedia/src/extractor.c",
            "libcrtmedia/src/codec.c",
            "libcrtmedia/tests/frame_test.c",
            "libcrtmedia/tests/gpu_frame_test.c",
            "libcrtmedia/tests/format_test.c",
            "libcrtmedia/tests/player_test.c",
            "libcrtmedia/tests/audio_sink_test.c",
            "libcrtmedia/tests/demux_decode_test.c",
            "libcrtmedia/assets/test_tone.wav",
            "porting/recipes/freetype.json",
            "porting/recipes/ffmpeg.json",
            "examples/README.md",
            "examples/gfx-gpu/CMakeLists.txt",
            "examples/gfx-skia/CMakeLists.txt",
        ),
        "project_paths_by_os": {
            "windows": (
                "libcrtgfx/src/arch/windows/gpu_win32.c",
                "libcrtmedia/src/arch/windows/audio_sink_wasapi.c",
                "libc/src/arch/windows/common/emutls_link_stubs.c",
                # The whole shim is required: Skia's D3D include chain uses
                # its forwarding windows.h plus exclusive excpt/malloc/etc.
                "libstdc++/third_party/win32_shim",
            ),
            "macos": (
                "libcrtgfx/src/arch/macos/gpu_metal.c",
                "libcrtmedia/src/arch/macos/audio_sink_coreaudio.c",
                "libstdc++/third_party/libcxx/libc++.unexported.exp",
            ),
            "linux": (
                "libcrtgfx/src/arch/linux/gpu_vulkan.c",
                "libcrtmedia/src/arch/linux/audio_sink_linux.c",
            ),
        },
        # Skia, FreeType, FFmpeg, and the Windows D3D headers are fetched by
        # pinned live mechanisms in the entrypoint; no large upstream source
        # tree is duplicated into this release asset.
        "source_paths": (),
    },
}


def required_source_paths(root: Path, spec: dict, target_os: str) -> tuple:
    """Filters spec["source_paths"] down to the ones this target_os actually
    needs -- e.g. libunwind (tools/crt-libcxx-build.py's own libstdc++/
    third_party/libunwind/recipe.json) is deliberately never fetched on
    macOS at all (Darwin's own libSystem unwinder is used instead; see
    that recipe.json's own notes), so requiring "sources/libunwind" to
    exist there is a real, confirmed bug (2026-09-09) -- it can never
    exist on a real macOS build, only ever on Linux/Windows. A source_
    paths entry with no matching libstdc++/third_party/<name>/recipe.json
    (cmake/runtimes/libc -- plain sparse-checkout subtrees of the same
    upstream monorepo, not their own separately-gated component) is
    always required, unchanged from before this function existed."""
    required = []
    candidates = tuple(spec.get("source_paths", ())) + tuple(
        spec.get("source_paths_by_os", {}).get(target_os, ())
    )
    for name in candidates:
        recipe_path = root / "libstdc++" / "third_party" / name / "recipe.json"
        if recipe_path.exists():
            recipe = json.loads(recipe_path.read_text(encoding="utf-8"))
            allowed = recipe.get("target_os")
            if allowed is not None and target_os not in allowed:
                continue
        required.append(name)
    return tuple(required)


def required_project_paths(spec: dict, target_os: str) -> tuple:
    return tuple(spec["project_paths"]) + tuple(
        spec.get("project_paths_by_os", {}).get(target_os, ())
    )


def git(root: Path, *args: str) -> str:
    result = subprocess.run(
        ["git", *args], cwd=root, text=True, capture_output=True, check=True
    )
    return result.stdout.strip()


def add_path(tar: tarfile.TarFile, source: Path, archive: PurePosixPath) -> None:
    """Add one tree with stable ordering and metadata."""
    paths = [source]
    if source.is_dir():
        paths.extend(sorted(
            (path for path in source.rglob("*")
             if not any(part in (".git", ".hg", ".svn")
                        for part in path.relative_to(source).parts)),
            key=lambda p: p.as_posix(),
        ))
    for path in paths:
        relative = path.relative_to(source)
        name = archive if relative == Path(".") else archive / PurePosixPath(relative.as_posix())
        info = tarfile.TarInfo(name.as_posix())
        info.uid = info.gid = 0
        info.uname = info.gname = ""
        info.mtime = 0
        if path.is_symlink():
            raise SystemExit(f"stage source packages do not accept symlinks: {path}")
        if path.is_dir():
            info.type = tarfile.DIRTYPE
            info.mode = 0o755
            tar.addfile(info)
        elif path.is_file():
            info.size = path.stat().st_size
            executable = bool(path.stat().st_mode & stat.S_IXUSR) or path.suffix in (".sh",)
            info.mode = 0o755 if executable else 0o644
            with path.open("rb") as stream:
                tar.addfile(info, stream)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--stage", required=True, choices=sorted(STAGES))
    parser.add_argument("--target-os", required=True, help="linux/windows/macos -- gates which source_paths entries this build actually needs (see required_source_paths())")
    parser.add_argument("--source-root", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--release-tag", required=True)
    parser.add_argument("--repository-url", default="https://github.com/webos21/crt")
    parser.add_argument("--allow-dirty", action="store_true")
    # verify_dist.py requires a packaged 01-c/02-cxx SDK to carry the
    # pinned recipe.json that advances it one stage further (the "cumulative
    # chain" contract: each stage's own dist output can bootstrap the next
    # one with no other input than a downloaded source asset). The *main*
    # CMake-driven pipeline satisfies this via create_dist.py's own
    # --stage-recipe (copying an already-generated <successor>.json
    # straight into that dist output), entirely outside this script -- but
    # the *isolated*, standalone stage-build path (crt-stage-build.py ->
    # tools/build_stage_0N_*.py, meant to run with only a previous stage's
    # packaged SDK and a downloaded asset, no git checkout or CMake at all)
    # has no equivalent, and never did: a real, confirmed bug (2026-09-11),
    # found running the documented end-user upgrade from a real packaged
    # dist/01-c -- verify_dist.py failed outright ("distribution is
    # missing: .../stages/recipes/03-gfx-simple.json") the first time
    # 01-c -> 02-cxx was exercised this way, since build_stage_02_cxx.py's
    # own output never carried it. Fixed by optionally embedding an
    # already-generated successor recipe (produced the identical way the
    # main pipeline already does, via this same script one stage further
    # out) inside *this* stage's own source asset, at stages/recipes/
    # <successor>.json -- the isolated build_stage_0N_*.py entrypoint then
    # only needs to copy it into its own output, no git/CMake required.
    parser.add_argument("--successor-recipe", type=Path, default=None,
        help="path to an already-generated <successor-stage>.json recipe to embed at stages/recipes/<successor-stage>.json inside this stage's own asset")
    args = parser.parse_args()

    root = args.root.resolve()
    source_root = args.source_root.resolve()
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    spec = STAGES[args.stage]
    project_paths = required_project_paths(spec, args.target_os)
    source_paths = required_source_paths(root, spec, args.target_os)
    commit = git(root, "rev-parse", "HEAD")
    if not args.allow_dirty and git(root, "status", "--porcelain"):
        raise SystemExit("refusing to create a release source asset from a dirty worktree; use --allow-dirty only for local testing")

    missing = [path for path in project_paths if not (root / path).exists()]
    missing += [f"sources/{path}" for path in source_paths if not (source_root / path).exists()]
    if missing:
        raise SystemExit("stage source inputs are missing: " + ", ".join(missing))

    successor_stage = None
    if args.successor_recipe is not None:
        successor_stage = STAGE_SUCCESSORS.get(args.stage)
        if successor_stage is None:
            raise SystemExit(f"--successor-recipe given but stage {args.stage!r} has no successor")
        if not args.successor_recipe.is_file():
            raise SystemExit(f"--successor-recipe file not found: {args.successor_recipe}")

    asset_name = f"crt-{args.release_tag}-{args.target_os}-{args.stage}-source.tar.xz"
    asset = output_dir / asset_name
    metadata = {
        "format": 2,
        "stage": args.stage,
        "input_stage": spec["input_stage"],
        "target": {"os": args.target_os},
        "source_commit": commit,
        "entrypoint": spec["entrypoint"],
    }
    with lzma.open(asset, "wb", preset=9) as compressed:
        with tarfile.open(fileobj=compressed, mode="w", format=tarfile.PAX_FORMAT) as tar:
            payload = (json.dumps(metadata, indent=2, sort_keys=True) + "\n").encode("utf-8")
            info = tarfile.TarInfo("stage.json")
            info.size = len(payload)
            info.mode = 0o644
            info.mtime = 0
            tar.addfile(info, io.BytesIO(payload))
            for relative in project_paths:
                add_path(tar, root / relative, PurePosixPath(relative))
            for relative in source_paths:
                add_path(tar, source_root / relative, PurePosixPath("sources") / relative)
            if successor_stage is not None:
                add_path(tar, args.successor_recipe,
                          PurePosixPath("stages") / "recipes" / f"{successor_stage}.json")

    digest = hashlib.sha256(asset.read_bytes()).hexdigest()
    recipe = {
        "schema_version": 2,
        "input_stage": spec["input_stage"],
        "output_stage": args.stage,
        "target": {"os": args.target_os},
        "source": {
            "url": f"{args.repository_url.rstrip('/')}/releases/download/{args.release_tag}/{asset_name}",
            "archive": asset_name,
            "size": asset.stat().st_size,
            "sha256": digest,
            "source_commit": commit,
        },
        "build": {"entrypoint": spec["entrypoint"]},
    }
    recipe_path = output_dir / f"{args.stage}.json"
    recipe_path.write_text(json.dumps(recipe, indent=2) + "\n", encoding="utf-8")
    print(f"CRT stage source asset: {asset}")
    print(f"CRT stage recipe: {recipe_path}")
    print(f"SHA-256: {digest}")


if __name__ == "__main__":
    main()
