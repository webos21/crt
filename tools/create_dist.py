#!/usr/bin/env python3
"""Create one cumulative, externally consumable CRT distribution stage."""

import argparse
import json
import os
import shutil
import subprocess
from datetime import datetime, timezone
from pathlib import Path


def copy_base(source: Path | None, destination: Path) -> None:
    if destination.exists():
        shutil.rmtree(destination)
    if source:
        if not source.is_dir():
            raise SystemExit(f"base distribution does not exist: {source}")
        shutil.copytree(source, destination, symlinks=True)
    else:
        destination.mkdir(parents=True)


def install_component(cmake: str, build_dir: Path, destination: Path, component: str) -> None:
    subprocess.run(
        [cmake, "--install", str(build_dir), "--prefix", str(destination), "--component", component],
        check=True,
    )


def copy_libcxx(install: Path, destination: Path) -> None:
    headers = install / "include" / "c++" / "v1"
    libraries = install / "lib"
    if not headers.is_dir() or not libraries.is_dir():
        raise SystemExit("libc++ install is incomplete; run crt-libcxx-build first")
    header_dest = destination / "include" / "c++" / "v1"
    if header_dest.exists():
        shutil.rmtree(header_dest)
    shutil.copytree(headers, header_dest)
    (destination / "lib").mkdir(parents=True, exist_ok=True)
    copied = 0
    for item in libraries.iterdir():
        if item.is_file() and item.name.startswith(("libc++", "libunwind", "unwind")):
            shutil.copy2(item, destination / "lib" / item.name)
            copied += 1
    runtime_bin = install / "bin"
    if runtime_bin.is_dir():
        (destination / "bin").mkdir(parents=True, exist_ok=True)
        for item in runtime_bin.iterdir():
            if item.is_file() and ("c++" in item.name or item.name.startswith(("libunwind", "unwind"))):
                shutil.copy2(item, destination / "bin" / item.name)
                copied += 1
    if copied == 0:
        raise SystemExit("libc++ install contains no runtime libraries")


def copy_port_tools(port_prefix: Path | None, destination: Path) -> None:
    if not port_prefix:
        return
    source_bin = port_prefix / "bin"
    if not source_bin.is_dir():
        raise SystemExit(f"port tool prefix has no bin directory: {port_prefix}")
    candidates = [item for item in source_bin.iterdir() if item.name in ("make", "make.exe")]
    if not candidates:
        raise SystemExit(f"make was not installed under: {source_bin}")
    target_bin = destination / "system" / "bin"
    target_bin.mkdir(parents=True, exist_ok=True)
    for item in candidates:
        shutil.copy2(item, target_bin / item.name)


def install_wrapper_applets(destination: Path, target_os: str) -> None:
    """Install the Toybox command names used internally by CRT wrappers."""
    suffix = ".exe" if target_os == "windows" else ""
    toybox = destination / "system" / "bin" / f"toybox{suffix}"
    if not toybox.is_file():
        raise SystemExit(f"distribution does not contain Toybox: {toybox}")
    for name in ("printf", "sed", "uname"):
        alias = toybox.with_name(f"{name}{suffix}")
        shutil.copy2(toybox, alias)
        if suffix:
            # The CRT mksh/PAL PATH search does not append PATHEXT. Keep an
            # extensionless PE alias as rootfs creation already does.
            shutil.copy2(toybox, toybox.with_name(name))


def write_sdk_files(root: Path, destination: Path, target_os: str, target_arch: str,
                    target_triple: str, stage: str, tools: dict[str, str]) -> None:
    tools_dest = destination / "tools"
    tools_dest.mkdir(parents=True, exist_ok=True)
    for name in ("crt-cc", "crt-c++", "crt-cc.cmd", "crt-c++.cmd"):
        shutil.copy2(root / "tools" / name, tools_dest / name)
    shutil.copy2(root / "LICENSE.md", destination / "LICENSE.md")

    (destination / "activate.sh").write_text(
        """#!/bin/sh
_crt_dist_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
export CRT_SYSROOT="$_crt_dist_root"
export CRT_ROOTFS="$_crt_dist_root"
export CRT_TARGET_OS="__TARGET_OS__"
export CRT_TARGET_ARCH="__TARGET_ARCH__"
export CRT_HOST_CC="${CRT_CC:-${CRT_HOST_CC:-clang}}"
export CRT_HOST_CXX="${CRT_CXX:-${CRT_HOST_CXX:-clang++}}"
export CC="$_crt_dist_root/tools/crt-cc"
export CXX="$_crt_dist_root/tools/crt-c++"
export AR="${CRT_AR:-${AR:-llvm-ar}}"
export RANLIB="${CRT_RANLIB:-${RANLIB:-llvm-ranlib}}"
export PATH="$_crt_dist_root/tools:$_crt_dist_root/system/bin:$PATH"
""".replace("__TARGET_OS__", target_os).replace("__TARGET_ARCH__", target_arch),
        encoding="utf-8",
    )
    os.chmod(destination / "activate.sh", 0o755)
    (destination / "activate.cmd").write_text(
        """@echo off
set "CRT_SYSROOT=%~dp0"
set "CRT_ROOTFS=%CRT_SYSROOT%"
set "CRT_TARGET_OS=__TARGET_OS__"
set "CRT_TARGET_ARCH=__TARGET_ARCH__"
if defined CRT_CC set "CRT_HOST_CC=%CRT_CC%"
if defined CRT_CXX set "CRT_HOST_CXX=%CRT_CXX%"
if not defined CRT_HOST_CC for /f "delims=" %%I in ('where clang.exe 2^>nul') do if not defined CRT_HOST_CC set "CRT_HOST_CC=%%I"
if not defined CRT_HOST_CXX for /f "delims=" %%I in ('where clang++.exe 2^>nul') do if not defined CRT_HOST_CXX set "CRT_HOST_CXX=%%I"
if not defined CRT_HOST_CC set "CRT_HOST_CC=clang"
if not defined CRT_HOST_CXX set "CRT_HOST_CXX=clang++"
set "CRT_HOST_CC=%CRT_HOST_CC:\\=/%"
set "CRT_HOST_CXX=%CRT_HOST_CXX:\\=/%"
if not defined CRT_WINDOWS_SDK_LIBPATH if defined WindowsSdkDir if defined WindowsSDKLibVersion set "CRT_WINDOWS_SDK_LIBPATH=%WindowsSdkDir%Lib\\%WindowsSDKLibVersion%um\\__WINDOWS_SDK_ARCH__"
if not defined AR set "AR=llvm-ar"
if defined CRT_AR set "AR=%CRT_AR%"
if not defined RANLIB set "RANLIB=llvm-ranlib"
if defined CRT_RANLIB set "RANLIB=%CRT_RANLIB%"
set "CRT_MKSH_EXE=%CRT_SYSROOT%system\\bin\\mksh.exe"
set "CC=%CRT_SYSROOT%tools\\crt-cc.cmd"
set "CXX=%CRT_SYSROOT%tools\\crt-c++.cmd"
set "PATH=%CRT_SYSROOT%tools;%CRT_SYSROOT%system\\bin;%PATH%"
""".replace("__TARGET_OS__", target_os).replace("__TARGET_ARCH__", target_arch).replace(
            "__WINDOWS_SDK_ARCH__", "x64" if target_arch == "x86_64" else "arm64"),
        encoding="utf-8",
    )
    wrapper_suffix = ".cmd" if target_os == "windows" else ""
    toolchain = """# Use with: cmake -S <app> -B <build> -DCMAKE_TOOLCHAIN_FILE=<this-file>
get_filename_component(CRT_DISTRIBUTION_ROOT "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
set(CMAKE_SYSROOT "${CRT_DISTRIBUTION_ROOT}")
set(CMAKE_C_COMPILER "${CRT_DISTRIBUTION_ROOT}/tools/crt-cc__WRAPPER_SUFFIX__" CACHE FILEPATH "CRT C compiler wrapper")
set(CMAKE_CXX_COMPILER "${CRT_DISTRIBUTION_ROOT}/tools/crt-c++__WRAPPER_SUFFIX__" CACHE FILEPATH "CRT C++ compiler wrapper")
if(DEFINED ENV{CRT_AR} AND NOT "$ENV{CRT_AR}" STREQUAL "")
  set(CMAKE_AR "$ENV{CRT_AR}" CACHE FILEPATH "External archiver")
endif()
if(DEFINED ENV{CRT_RANLIB} AND NOT "$ENV{CRT_RANLIB}" STREQUAL "")
  set(CMAKE_RANLIB "$ENV{CRT_RANLIB}" CACHE FILEPATH "External ranlib")
endif()
set(CMAKE_C_FLAGS_INIT "-ffreestanding -fno-builtin -nostdinc -isystem${CRT_DISTRIBUTION_ROOT}/include")
set(CMAKE_CXX_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} -nostdinc++ -isystem${CRT_DISTRIBUTION_ROOT}/include/c++/v1")
""".replace("__WRAPPER_SUFFIX__", wrapper_suffix)
    (destination / "crt-toolchain.cmake").write_text(toolchain, encoding="utf-8")
    manifest = {
        "format": 1,
        "stage": stage,
        "target": {"os": target_os, "arch": target_arch},
        "target_triple": target_triple,
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "compiler_bundled": False,
        "external_tools_used_to_build": {
            name: Path(value).name if value else "" for name, value in tools.items()
        },
        "default_compile_options": ["-ffreestanding", "-fno-builtin", "-nostdinc"],
        "external_toolchain_environment": [
            "CRT_CC", "CRT_CXX", "CRT_AR", "CRT_RANLIB", "CRT_WINDOWS_SDK_LIBPATH"
        ],
    }
    (destination / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    (destination / "VERSION").write_text("development\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--dest", required=True, type=Path)
    parser.add_argument("--base", type=Path)
    parser.add_argument("--component", action="append", default=[])
    parser.add_argument("--libcxx-install", type=Path)
    parser.add_argument("--port-prefix", type=Path)
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--stage", required=True)
    parser.add_argument("--target-os", required=True)
    parser.add_argument("--target-arch", required=True)
    parser.add_argument("--target-triple", default="")
    parser.add_argument("--cc", default="")
    parser.add_argument("--cxx", default="")
    parser.add_argument("--ar", default="")
    parser.add_argument("--ranlib", default="")
    parser.add_argument("--archive", action="store_true")
    args = parser.parse_args()

    normalized_arch = {
        "amd64": "x86_64",
        "x64": "x86_64",
        "arm64": "aarch64",
    }.get(args.target_arch.lower(), args.target_arch.lower())

    root = args.root.resolve()
    build_dir = args.build_dir.resolve()
    destination = args.dest.resolve()
    copy_base(args.base.resolve() if args.base else None, destination)
    for component in args.component:
        install_component(args.cmake, build_dir, destination, component)
    if args.libcxx_install:
        copy_libcxx(args.libcxx_install.resolve(), destination)
    copy_port_tools(args.port_prefix.resolve() if args.port_prefix else None, destination)
    install_wrapper_applets(destination, args.target_os)
    write_sdk_files(
        root, destination, args.target_os, normalized_arch,
        args.target_triple or f"{normalized_arch}-{args.target_os}", args.stage,
        {"cc": args.cc, "cxx": args.cxx, "ar": args.ar, "ranlib": args.ranlib},
    )
    if args.archive:
        archive_format = "zip" if args.target_os == "windows" else "xztar"
        archive_name = destination.parent / (
            f"crt-development-{args.target_os}-{normalized_arch}-{args.stage}"
        )
        shutil.make_archive(str(archive_name), archive_format,
                            root_dir=destination.parent, base_dir=destination.name)
    print(f"CRT distribution ready: {destination}")


if __name__ == "__main__":
    main()
