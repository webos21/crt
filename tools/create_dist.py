#!/usr/bin/env python3
"""Create one cumulative, externally consumable CRT distribution stage."""

import argparse
import json
import os
import shutil
import subprocess
from datetime import datetime, timezone
from pathlib import Path

from create_rootfs import TOYBOX_APPLETS


# What "ships in every packaged distribution" is, on purpose, a single
# definition. verify_dist.py imports these same three names instead of
# re-declaring its own copy of this list -- before this, the set of files
# copied here and the set of files verify_dist.py required existed as two
# independently maintained tuples in two different files, which is a real
# drift risk (add a file to one, forget the other, and verify_dist.py
# either false-fails a good distribution or silently stops checking a
# real one).
DIST_PORTING_TOOLS = (
    "crt-port-build.py", "fetch_ports.py", "crt-native-tool", "crt-stage-build.py",
    "crt_stage_recipe.py",
)
DIST_PORTING_DIRS = ("recipes", "tests", "shims")
DIST_WRAPPER_TOOLS = ("crt-cc", "crt-c++", "crt-cc.cmd", "crt-c++.cmd")


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


def copy_xkbcommon(install: Path | None, source: Path | None, root: Path,
                   destination: Path) -> dict | None:
    """Copy the Linux Simple Graphics source port and its provenance."""
    if not install:
        return None
    headers = install / "include" / "xkbcommon"
    library = install / "lib" / "libxkbcommon.a"
    license_file = source / "LICENSE" if source else None
    if not headers.is_dir() or not library.is_file() or not license_file or not license_file.is_file():
        raise SystemExit("xkbcommon install/source is incomplete for Simple Graphics packaging")
    header_dest = destination / "include" / "xkbcommon"
    if header_dest.exists():
        shutil.rmtree(header_dest)
    shutil.copytree(headers, header_dest)
    (destination / "lib").mkdir(parents=True, exist_ok=True)
    shutil.copy2(library, destination / "lib" / library.name)
    notice = destination / "share" / "licenses" / "xkbcommon" / "LICENSE"
    notice.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(license_file, notice)
    provenance = destination / "share" / "crt" / "dependencies" / "xkbcommon"
    provenance.mkdir(parents=True, exist_ok=True)
    shutil.copy2(root / "libcrtgfx" / "third_party" / "xkbcommon" / "recipe.json",
                 provenance / "recipe.json")
    return {
        "name": "xkbcommon",
        "kind": "private-static-port",
        "headers": ["include/xkbcommon"],
        "link_artifacts": ["lib/libxkbcommon.a"],
        "runtime_artifacts": [],
        "notices": ["share/licenses/xkbcommon/LICENSE"],
        "provenance": "share/crt/dependencies/xkbcommon/recipe.json",
    }


def copy_porting_sdk(root: Path, destination: Path) -> None:
    """Install the optional source-porting workflow, but not upstream sources."""
    tools_dest = destination / "tools"
    tools_dest.mkdir(parents=True, exist_ok=True)
    for name in DIST_PORTING_TOOLS:
        shutil.copy2(root / "tools" / name, tools_dest / name)

    porting_dest = destination / "porting"
    for directory in DIST_PORTING_DIRS:
        target = porting_dest / directory
        if target.exists():
            shutil.rmtree(target)
        shutil.copytree(root / "porting" / directory, target)
    shutil.copy2(root / "porting" / "DISTRIBUTION_README.md",
                 porting_dest / "README.md")


def copy_stage_recipes(recipes: list[Path], destination: Path) -> None:
    if not recipes:
        return
    recipe_dest = destination / "stages" / "recipes"
    recipe_dest.mkdir(parents=True, exist_ok=True)
    for recipe in recipes:
        shutil.copy2(recipe, recipe_dest / recipe.name)


def install_wrapper_applets(destination: Path, target_os: str) -> None:
    """Install every enabled Toybox applet needed by the packaged mksh."""
    suffix = ".exe" if target_os == "windows" else ""
    system_bin = destination / "system" / "bin"
    mksh = system_bin / f"mksh{suffix}"
    if mksh.is_file():
        # shell/CMakeLists.txt's own `install(TARGETS crt_tiny_sh ...)`
        # rule (OUTPUT_NAME "sh", part of the base crt-c component, every
        # platform) always lands directly at system/bin/sh<suffix> --
        # crt_tiny_sh only understands `-c <command>` (see
        # shell/tiny_sh/tiny_sh.c), not being run *as* a script, which is
        # exactly how a "#!/bin/sh" shebang line invokes its interpreter.
        # create_rootfs.py's own in-tree rootfs already gets this right by
        # explicitly aliasing "sh"/"sh.exe" to mksh (a full shell) and
        # keeping the tiny one under a separate "tiny-sh" name -- this
        # packaged/cumulative dist never got the equivalent treatment, so
        # every packaged SDK's own "sh" was silently the tiny, -c-only
        # shell instead, standing unnoticed until /bin/sh's own path
        # resolution got fixed below (see that comment): FreeType's
        # generated `install-sh` (invoked as a real multi-line script via
        # its own "#!/bin/sh" shebang, not as `sh -c ...`) then failed
        # instead with "sh: only -c scripts are supported by crt tiny sh"
        # -- found for real, 2026-09-10, immediately after the /bin/sh
        # path fix let that shebang resolve to a real file at all. Must
        # run before the extensionless-alias loop below, which copies
        # system/bin/sh<suffix> onward from whatever it is at that point.
        shutil.copy2(mksh, system_bin / f"sh{suffix}")
    toybox = system_bin / f"toybox{suffix}"
    if not toybox.is_file():
        raise SystemExit(f"distribution does not contain Toybox: {toybox}")
    if suffix:
        # Keep the dispatcher itself directly runnable by CRT's POSIX-shaped
        # PATH search as well as the individual applet aliases below.
        shutil.copy2(toybox, toybox.with_name("toybox"))
    for name in TOYBOX_APPLETS:
        alias = toybox.with_name(f"{name}{suffix}")
        shutil.copy2(toybox, alias)
        if suffix:
            # The CRT mksh/PAL PATH search does not append PATHEXT. Keep an
            # extensionless PE alias as rootfs creation already does.
            shutil.copy2(toybox, toybox.with_name(name))
    if suffix:
        # CRT's exec/PATH layer deliberately does not emulate PATHEXT.
        # Keep extensionless names for the other packaged shell tools too,
        # matching create_rootfs.py's runnable rootfs layout.
        for name in ("mksh", "sh", "make", "awk"):
            executable = destination / "system" / "bin" / f"{name}.exe"
            if executable.is_file():
                shutil.copy2(executable, executable.with_name(name))
    # Also alias the shell interpreter (and the other tools commonly named
    # in a hardcoded shebang line) into bin/ and usr/bin/, not just
    # system/bin/. This cumulative dist directory is used directly as
    # CRT_ROOTFS -- by activate.sh/activate.cmd above, and by
    # crt-port-build.py's packaged-SDK mode -- and CRT_ROOTFS's whole
    # premise (and its own PATH, "/system/bin:/bin:/usr/bin") is that
    # /bin and /usr/bin resolve too, not just /system/bin. Before this,
    # this dist's own top-level bin/ held only compiled native runtime
    # DLLs (libc.dll etc, from copy_libcxx/install_component in main()),
    # so any script whose shebang is a literal "#!/bin/sh" -- which is
    # how essentially every autoconf/libtool-generated script starts,
    # including libtool's own generated `install-sh` driver -- failed
    # CreateProcessA() with ERROR_FILE_NOT_FOUND every single time,
    # deterministically. Found and root-caused 2026-09-10 chasing
    # FreeType's isolated `make install`'s "can't fork - try again":
    # syscall.c's crt_createprocess_with_retry() (added first, suspecting
    # transient CreateProcessA flakiness under bursty spawn load) logs the
    # exact application_path on each retry, and it was always the same,
    # permanently-nonexistent ".../bin/sh" -- proving this is a real,
    # deterministic packaging gap, not a race. Deliberately narrow (just
    # these four names, not every Toybox applet): the concrete, reproduced
    # failure is /bin/sh specifically, and duplicating the full ~266MB
    # system/bin Toybox-applet set into two more directories per dist
    # stage isn't justified without a concrete case needing it -- add more
    # names here if/when a specific absolute shebang path is found to
    # need one. Mirrors create_rootfs.py's own ROOTFS_DIRS/shell_dir loop
    # over ("system/bin", "bin", "usr/bin") for the in-tree, non-packaged
    # rootfs -- this packaged dist was simply never given the same
    # treatment.
    for alias_dir_name in ("bin", "usr/bin"):
        alias_dir = destination / alias_dir_name
        alias_dir.mkdir(parents=True, exist_ok=True)
        for name in ("mksh", "sh", "make", "awk"):
            for candidate_name in (f"{name}{suffix}", name):
                source = destination / "system" / "bin" / candidate_name
                if source.is_file():
                    shutil.copy2(source, alias_dir / candidate_name)


def write_sdk_files(root: Path, destination: Path, target_os: str, target_arch: str,
                    target_triple: str, stage: str, tools: dict[str, str],
                    redistributed_dependencies: list[dict]) -> None:
    tools_dest = destination / "tools"
    tools_dest.mkdir(parents=True, exist_ok=True)
    for name in DIST_WRAPPER_TOOLS:
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
if [ -d "$_crt_dist_root/include/c++/v1" ]; then
  export CRT_CXX_STANDARD_INCLUDE_FLAGS="-isystem$_crt_dist_root/include/c++/v1"
fi
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
if exist "%CRT_SYSROOT%include\\c++\\v1" set "CRT_CXX_STANDARD_INCLUDE_FLAGS=-isystem%CRT_SYSROOT%include/c++/v1"
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
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_C_STANDARD_LIBRARIES "" CACHE STRING "CRT wrapper supplies the C runtime" FORCE)
set(CMAKE_CXX_STANDARD_LIBRARIES "" CACHE STRING "CRT wrapper supplies the C++ runtime" FORCE)
if(DEFINED ENV{CRT_AR} AND NOT "$ENV{CRT_AR}" STREQUAL "")
  set(CMAKE_AR "$ENV{CRT_AR}" CACHE FILEPATH "External archiver")
endif()
if(DEFINED ENV{CRT_RANLIB} AND NOT "$ENV{CRT_RANLIB}" STREQUAL "")
  set(CMAKE_RANLIB "$ENV{CRT_RANLIB}" CACHE FILEPATH "External ranlib")
endif()
set(CMAKE_C_FLAGS_INIT "-ffreestanding -fno-builtin -nostdinc -isystem${CRT_DISTRIBUTION_ROOT}/include")
set(CMAKE_CXX_FLAGS_INIT "-ffreestanding -fno-builtin -nostdinc -nostdinc++ -isystem${CRT_DISTRIBUTION_ROOT}/include/c++/v1 -isystem${CRT_DISTRIBUTION_ROOT}/include")
# CMake's own default rpath computation, once CMAKE_SYSROOT is set above,
# treats it as a real target-device filesystem root and strips it from
# any absolute library path found underneath -- appropriate for real
# cross-compilation onto a separate device filesystem, but wrong here:
# CRT_DISTRIBUTION_ROOT is just wherever this SDK happens to be unpacked
# on this same host, not a chroot the consumer executable will actually
# run inside. Confirmed for real (2026-09-09): linking examples/gfx-
# simple against this stage's own installed libcrtgfx.dylib produced a
# real, reproducible "-rpath /lib" (the sysroot prefix silently stripped
# from the real "${CRT_DISTRIBUTION_ROOT}/lib" path) and the resulting
# executable failed to even start -- "dyld: Library not loaded: @rpath/
# libcrtgfx.dylib ... tried: '/lib/libcrtgfx.dylib' (no such file)".
# Setting CMAKE_BUILD_RPATH explicitly overrides that sysroot-relative
# default with the real, absolute lib directory instead, matching how
# this SDK's own wrapper scripts (tools/crt-cc) already reference
# ${CRT_SYSROOT}/lib/... by absolute path throughout.
set(CMAKE_BUILD_RPATH "${CRT_DISTRIBUTION_ROOT}/lib")
set(CMAKE_INSTALL_RPATH "${CRT_DISTRIBUTION_ROOT}/lib")
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
        # Windows genuinely has no usable bare "clang"/"ar"/"ranlib" (a
        # fresh CMake compiler probe there needs CRT_CC/CRT_CXX explicitly,
        # and the raw Windows SDK import-library path has no other source)
        # -- confirmed for real, HISTORY.md's own "isolated 04 stage" entry,
        # where the first complete Windows 03 -> 04 run got all the way
        # through a multi-hour FreeType/FFmpeg/Skia build before failing at
        # its own standalone CMake configure for exactly this reason. macOS
        # and Linux have no equivalent gap: bare clang/clang++/ar/ranlib
        # already resolve to real, directly usable system tools there (this
        # whole distribution-stage machinery has run repeatedly on macOS
        # this same day without ever setting any of these) -- CRT_CC/
        # CRT_CXX/CRT_AR/CRT_RANLIB stay purely optional overrides on those
        # two hosts (runtime_env() in tools/build_stage_04_gfx_media.py
        # already only *applies* them when present, never requires them).
        # A real, confirmed bug fixed the same day this comment was written
        # (2026-09-11): this list was unconditional for every target_os, so
        # the isolated 03 -> 04 upgrade's very first real macOS attempt
        # failed immediately demanding CRT_WINDOWS_SDK_LIBPATH be set even
        # though the target was macOS.
        "external_toolchain_environment": (
            ["CRT_CC", "CRT_CXX", "CRT_AR", "CRT_RANLIB", "CRT_WINDOWS_SDK_LIBPATH"]
            if target_os == "windows" else []
        ),
        "redistributed_dependencies": redistributed_dependencies,
        "optional_tools": {
            "porting": {
                "included": True,
                "python": {"required": True, "minimum_version": "3.9"},
                "network_required_for_fetch": True,
                "standalone_smoke_ports": ["zlib"],
            }
        },
        "shell_environment": ({
            "provider": "crt-mksh-toybox",
            "external_posix_shell_required": False,
            "crt_shell_default": True,
        } if target_os == "windows" else {
            "provider": "host-posix-shell",
            "external_posix_shell_required": True,
            "crt_shell_default": False,
            "crt_shell_available": True,
        }),
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
    parser.add_argument("--xkbcommon-install", type=Path)
    parser.add_argument("--xkbcommon-source", type=Path)
    parser.add_argument("--stage-recipe", action="append", default=[], type=Path)
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
    inherited_dependencies = []
    inherited_manifest = destination / "manifest.json"
    if inherited_manifest.is_file():
        inherited_dependencies = json.loads(
            inherited_manifest.read_text(encoding="utf-8")
        ).get("redistributed_dependencies", [])
    # These are runtime-writable namespace directories, not build outputs.
    # Keep them in every cumulative SDK so the extracted distribution can be
    # used directly as CRT_ROOTFS.  In particular, Windows crt_mksh uses the
    # Android-compatible /data/local default for here-document temporary
    # files; $TMPDIR=/tmp does not override that internal mksh default.
    for relative in ("tmp", "data/local", "data/local/tmp"):
        (destination / relative).mkdir(parents=True, exist_ok=True)
    for component in args.component:
        install_component(args.cmake, build_dir, destination, component)
    if args.libcxx_install:
        copy_libcxx(args.libcxx_install.resolve(), destination)
    copy_port_tools(args.port_prefix.resolve() if args.port_prefix else None, destination)
    redistributed_dependencies = list(inherited_dependencies)
    xkbcommon = copy_xkbcommon(
        args.xkbcommon_install.resolve() if args.xkbcommon_install else None,
        args.xkbcommon_source.resolve() if args.xkbcommon_source else None,
        root, destination,
    )
    if xkbcommon and not any(item.get("name") == "xkbcommon"
                             for item in redistributed_dependencies):
        redistributed_dependencies.append(xkbcommon)
    copy_porting_sdk(root, destination)
    copy_stage_recipes([recipe.resolve() for recipe in args.stage_recipe], destination)
    install_wrapper_applets(destination, args.target_os)
    write_sdk_files(
        root, destination, args.target_os, normalized_arch,
        args.target_triple or f"{normalized_arch}-{args.target_os}", args.stage,
        {"cc": args.cc, "cxx": args.cxx, "ar": args.ar, "ranlib": args.ranlib},
        redistributed_dependencies,
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
