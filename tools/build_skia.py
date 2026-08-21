#!/usr/bin/env python3
import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def run(args, cwd=None, env=None):
    print("+", " ".join(str(arg) for arg in args), flush=True)
    subprocess.run(args, cwd=cwd, env=env, check=True)


def ensure_python3_shim(env, host_python):
    """Skia's own .gn dotfile hardcodes `script_executable = "python3"` --
    a real, unconditional upstream requirement `gn gen` needs to resolve
    on PATH, independent of anything this project's own build controls.
    Most Linux/macOS Python installs already provide a "python3"-named
    executable by distro/package-manager convention; a stock Windows
    Python install typically does not (only "python.exe"). Confirmed for
    real: `gn gen` fails outright with `ERROR Could not find "python3"
    from dotfile in PATH` without this on Windows.

    A tiny, project-owned .bat shim (not a copy of the whole interpreter)
    resolves it without touching any real system PATH -- prepended only
    to this subprocess's own env, matching this project's "wrap what's
    needed, don't require host changes" discipline already used
    throughout tools/crt-cc and friends. .bat rather than .exe: Windows'
    own PATHEXT-based bare-name resolution (which is exactly how `gn`
    itself, and CreateProcess more generally, looks up "python3" with no
    extension) tries .BAT/.CMD the same way it tries .EXE, so a batch
    shim works identically here without needing to copy python.exe's own
    multi-MB binary into a throwaway temp directory on every build.
    """
    if os.name != "nt":
        return
    if shutil.which("python3", path=env.get("PATH")):
        return
    shim_dir = Path(tempfile.mkdtemp(prefix="crt-skia-python3-shim-"))
    shim_path = shim_dir / "python3.bat"
    shim_path.write_text(f'@echo off\r\n"{host_python}" %*\r\n', encoding="utf-8")
    env["PATH"] = str(shim_dir) + os.pathsep + env.get("PATH", "")


def gn_string(value):
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    return f'"{escaped}"'


def gn_list(values):
    return "[" + ", ".join(gn_string(value) for value in values) + "]"


def detect_cxx_standard_include_dirs(cxx):
    probe = subprocess.run(
        [cxx, "-x", "c++", "-E", "-v", "-"],
        input="",
        text=True,
        capture_output=True,
        check=False,
    )
    search = probe.stderr.splitlines()
    include_dirs = []
    in_search_list = False
    for line in search:
        if line.strip() == "#include <...> search starts here:":
            in_search_list = True
            continue
        if in_search_list and line.strip() == "End of search list.":
            break
        if not in_search_list:
            continue
        candidate = line.strip()
        normalized = candidate.replace("\\", "/").lower()
        is_libcxx = "include/c++" in normalized or normalized.endswith("/c++/v1")
        is_msvc_stl = "/vc/tools/msvc/" in normalized and normalized.endswith("/include")
        if not is_libcxx and not is_msvc_stl:
            continue
        if Path(candidate).is_dir():
            include_dirs.append(candidate)
    return include_dirs


def default_gn_args(root, sysroot, target_os, target_arch, cxx_include_flags):
    crt_cc = root / "tools" / "crt-cc"
    crt_cxx = root / "tools" / "crt-c++"
    crt_ar = root / "tools" / ("crt-ar.cmd" if target_os == "windows" else "crt-ar")

    args = {
        "is_official_build": "true",
        "is_component_build": "false",
        "skia_enable_fontmgr_custom_empty": "true",
        "skia_enable_ganesh": "false",
        "skia_enable_graphite": "false",
        "skia_enable_pdf": "false",
        "skia_enable_skottie": "false",
        "skia_enable_svg": "false",
        "skia_use_fonthost_mac": "false",
        "skia_use_perfetto": "false",
        "skia_enable_tools": "false",
        "skia_use_dawn": "false",
        "skia_use_direct3d": "false",
        "skia_use_egl": "false",
        "skia_use_expat": "false",
        "skia_use_gl": "false",
        "skia_use_metal": "false",
        "skia_use_vulkan": "false",
        "skia_use_fontconfig": "false",
        "skia_use_freetype": "false",
        "skia_use_harfbuzz": "false",
        "skia_use_icu": "false",
        "skia_use_libavif": "false",
        "skia_use_libjpeg_turbo_decode": "false",
        "skia_use_libjpeg_turbo_encode": "false",
        "skia_use_libpng_decode": "false",
        "skia_use_libpng_encode": "false",
        "skia_use_libwebp_decode": "false",
        "skia_use_libwebp_encode": "false",
        "skia_use_zlib": "false",
        # skia_use_wuffs (GIF decode support, via a small vendored
        # third_party/externals/wuffs checkout) defaults to true in
        # Skia's own gn/skia.gni, unlike every other optional codec
        # above -- confirmed for real via `ninja -t inputs skia`: with
        # every other codec already off, third_party/externals/wuffs is
        # the ONLY third-party external source this minimal CPU-raster
        # config still pulls in. Disabled for the same reason as every
        # sibling codec flag above: nothing in this project's current
        # Skia integration needs GIF decoding yet, and this keeps the
        # build needing zero third_party/externals content at all.
        "skia_use_wuffs": "false",
        "cc": gn_string(str(crt_cc)),
        "cxx": gn_string(str(crt_cxx)),
        # Skia's GN archives use @response files. Apple ar does not expand
        # those files itself, so keep that detail in the project-owned build
        # wrapper rather than depending on a particular host LLVM install.
        "ar": gn_string(str(crt_ar)),
        "extra_cflags": gn_list(
            [f"-isystem{sysroot / 'include'}"] +
            (["-DSK_BUILD_FOR_UNIX"] if target_os == "macos" else [])
        ),
        "extra_cflags_cc": gn_list(cxx_include_flags + ["-fno-exceptions", "-fno-rtti"]),
        "extra_ldflags": gn_list([f"-L{sysroot / 'lib'}"]),
    }

    if not cxx_include_flags:
        print(
            "warning: no C++ standard include directory was detected; "
            "Skia may fail on <cstddef>/<utility>/...",
            file=sys.stderr,
        )

    if target_os == "windows":
        args["target_os"] = gn_string("win")
        if target_arch in ("aarch64", "arm64"):
            args["target_cpu"] = gn_string("arm64")
        elif target_arch in ("x86_64", "amd64", "x64"):
            args["target_cpu"] = gn_string("x64")
    elif target_os == "macos":
        # This is a CRT/PAL build of Skia, not Skia's native Cocoa/CoreGraphics
        # port. Select Skia's generic POSIX source set while the wrapper
        # compiler still emits a macOS binary via CRT_TARGET_OS=macos.
        args["target_os"] = gn_string("linux")
        if target_arch in ("aarch64", "arm64"):
            args["target_cpu"] = gn_string("arm64")
    elif target_os == "linux":
        args["target_os"] = gn_string("linux")
        if target_arch in ("aarch64", "arm64"):
            args["target_cpu"] = gn_string("arm64")

    return args


def write_args_file(path, args, extra_args):
    lines = [f"{key} = {value}" for key, value in sorted(args.items())]
    lines.extend(extra_args)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def copy_tree(src, dst):
    if dst.exists():
        shutil.rmtree(dst)
    shutil.copytree(src, dst)


def install_artifacts(source, build_dir, install_prefix):
    include_src = source / "include"
    include_dst = install_prefix / "include"
    lib_dst = install_prefix / "lib"
    lib_dst.mkdir(parents=True, exist_ok=True)
    copy_tree(include_src, include_dst / "include")

    candidates = [
        build_dir / "libskia.a",
        build_dir / "skia.lib",
        build_dir / "obj" / "libskia.a",
    ]
    for candidate in candidates:
        if candidate.exists():
            shutil.copy2(candidate, lib_dst / candidate.name)
            if candidate.name != "libskia.a":
                shutil.copy2(candidate, lib_dst / "libskia.a")
            return
    raise SystemExit(f"Skia library artifact was not found under {build_dir}")


def main():
    parser = argparse.ArgumentParser(description="Build Skia with the CRT wrapper toolchain.")
    parser.add_argument("--root", required=True, help="CRT repository root")
    parser.add_argument("--source", required=True, help="Skia source checkout")
    parser.add_argument("--build-dir", required=True, help="Skia GN output directory")
    parser.add_argument("--install-prefix", required=True, help="Skia install prefix used by libcrtgfx")
    parser.add_argument("--sysroot", required=True, help="CRT sysroot")
    parser.add_argument("--target-os", required=True, choices=["linux", "macos", "windows"])
    parser.add_argument("--target-arch", default="host")
    parser.add_argument("--gn", default="", help="path to gn; defaults to Skia bin/gn or PATH")
    parser.add_argument("--ninja", default="", help="path to ninja; defaults to PATH")
    parser.add_argument("--gn-arg", action="append", default=[], help="extra raw GN arg line")
    parser.add_argument("--configure-only", action="store_true")
    args = parser.parse_args()

    root = Path(args.root).resolve()
    source = Path(args.source).resolve()
    build_dir = Path(args.build_dir).resolve()
    install_prefix = Path(args.install_prefix).resolve()
    sysroot = Path(args.sysroot).resolve()
    gn_exe_suffix = ".exe" if args.target_os == "windows" or os.name == "nt" else ""
    gn = args.gn or str(source / "bin" / ("gn" + gn_exe_suffix))
    if not Path(gn).exists():
        # Bare "gn" (no .exe) never resolves via Path.exists() on Windows
        # even when bin/gn.exe genuinely exists -- confirmed for real: this
        # silently fell through to `shutil.which("gn")` (which also never
        # finds it, since bin/ is never on PATH) and then to the literal
        # string "gn" itself, only failing much later with a confusing
        # FileNotFoundError from CreateProcess. Bootstraps a fresh gn the
        # same way Skia's own real developers do -- bin/fetch-gn downloads
        # a prebuilt binary pinned to a specific git_revision hardcoded in
        # that script itself, so this needs no pinning of its own here.
        gn = shutil.which("gn") or gn
        if not Path(gn).exists():
            fetch_gn = source / "bin" / "fetch-gn"
            if fetch_gn.exists():
                run([sys.executable, str(fetch_gn)], cwd=source)
                gn = str(source / "bin" / ("gn" + gn_exe_suffix))
    ninja = args.ninja or shutil.which("ninja") or "ninja"

    env = os.environ.copy()
    env["CRT_SYSROOT"] = str(sysroot)
    env["CRT_TARGET_OS"] = args.target_os
    ensure_python3_shim(env, sys.executable)
    if args.target_os == "windows":
        # crt-ar.cmd needs the same interpreter CMake used to launch this
        # driver; relying on the optional py launcher would make a configured
        # Python installation look like a missing archiver.
        env["CRT_HOST_PYTHON"] = sys.executable
    if args.target_arch != "host":
        env["CRT_TARGET_ARCH"] = args.target_arch

    host_cxx = os.environ.get("CRT_HOST_CXX") or shutil.which("clang++") or "clang++"
    cxx_include_flags = [f"-isystem{path}" for path in detect_cxx_standard_include_dirs(host_cxx)]
    if cxx_include_flags:
        env["CRT_CXX_STANDARD_INCLUDE_FLAGS"] = " ".join(cxx_include_flags)

    gn_args = default_gn_args(root, sysroot, args.target_os, args.target_arch, cxx_include_flags)
    args_gn = build_dir / "args.gn"
    write_args_file(args_gn, gn_args, args.gn_arg)
    run([gn, "gen", str(build_dir)], cwd=source, env=env)
    if args.configure_only:
        return

    run([ninja, "-C", str(build_dir), "skia"], cwd=source, env=env)
    install_artifacts(source, build_dir, install_prefix)
    print(f"Skia installed into {install_prefix}")


if __name__ == "__main__":
    main()
