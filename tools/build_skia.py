#!/usr/bin/env python3
import argparse
import os
import shutil
import subprocess
from pathlib import Path


def run(args, cwd=None, env=None):
    print("+", " ".join(str(arg) for arg in args), flush=True)
    subprocess.run(args, cwd=cwd, env=env, check=True)


def gn_string(value):
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    return f'"{escaped}"'


def gn_list(values):
    return "[" + ", ".join(gn_string(value) for value in values) + "]"


def default_gn_args(root, sysroot, target_os, target_arch):
    crt_cc = root / "tools" / "crt-cc"
    crt_cxx = root / "tools" / "crt-c++"
    ar = shutil.which("llvm-ar") or shutil.which("ar") or "ar"

    args = {
        "is_official_build": "true",
        "is_component_build": "false",
        "skia_enable_gpu": "false",
        "skia_enable_tools": "false",
        "skia_use_dawn": "false",
        "skia_use_direct3d": "false",
        "skia_use_egl": "false",
        "skia_use_gl": "false",
        "skia_use_metal": "false",
        "skia_use_vulkan": "false",
        "skia_use_fontconfig": "false",
        "skia_use_freetype": "false",
        "skia_use_harfbuzz": "false",
        "skia_use_icu": "false",
        "skia_use_libheif": "false",
        "skia_use_libjpeg_turbo": "false",
        "skia_use_libpng": "false",
        "skia_use_libwebp": "false",
        "skia_use_zlib": "false",
        "cc": gn_string(str(crt_cc)),
        "cxx": gn_string(str(crt_cxx)),
        "ar": gn_string(ar),
        "extra_cflags": gn_list([f"-isystem{sysroot / 'include'}"]),
        "extra_cflags_cc": gn_list(["-fno-exceptions", "-fno-rtti"]),
        "extra_ldflags": gn_list([f"-L{sysroot / 'lib'}"]),
    }

    if target_os == "windows":
        args["target_os"] = gn_string("win")
        if target_arch in ("aarch64", "arm64"):
            args["target_cpu"] = gn_string("arm64")
        elif target_arch in ("x86_64", "amd64", "x64"):
            args["target_cpu"] = gn_string("x64")
    elif target_os == "macos":
        args["target_os"] = gn_string("mac")
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
    gn = args.gn or str(source / "bin" / "gn")
    if not Path(gn).exists():
        gn = shutil.which("gn") or gn
    ninja = args.ninja or shutil.which("ninja") or "ninja"

    env = os.environ.copy()
    env["CRT_SYSROOT"] = str(sysroot)
    env["CRT_TARGET_OS"] = args.target_os
    if args.target_arch != "host":
        env["CRT_TARGET_ARCH"] = args.target_arch

    gn_args = default_gn_args(root, sysroot, args.target_os, args.target_arch)
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
