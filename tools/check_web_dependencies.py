#!/usr/bin/env python3
"""Check that WebKit's own dependency detection accepts the CRT web-stage ports (Tranche 3B, B2).

Configures libcrtweb/cmake/web-dependencies (WebKit's find_package calls for the non-GLib ports,
using WebKit's own Find modules from the pinned product source) with a generated CRT toolchain
file whose find roots are the port prefix and the SDK only, and pkg-config limited to the port
prefix, so a host library can never satisfy a lookup. Exit status is non-zero when any dependency
is missing or too old.

  python3 tools/check_web_dependencies.py --prefix out/<preset>/port-tests/install \\
      --sdk-root out/<preset>/dist/02-cxx --webkit-source out/web-product/src
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PROJECT = ROOT / "libcrtweb" / "cmake" / "web-dependencies"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--prefix", type=Path, required=True, help="CRT port install prefix (port-tests/install)")
    parser.add_argument("--sdk-root", type=Path, required=True, help="CRT SDK with libc++ (02-cxx or later)")
    parser.add_argument("--webkit-source", type=Path, required=True, help="PlatformCRT product source (Source/cmake)")
    parser.add_argument("--keep", type=Path, help="keep the build directory here")
    args = parser.parse_args()
    prefix = args.prefix.resolve()
    sdk = args.sdk_root.resolve()
    source = args.webkit_source.resolve()
    if not (source / "Source" / "cmake" / "FindHarfBuzz.cmake").is_file():
        raise SystemExit(f"{source} is not a WebKit source tree")

    pkg_dirs = [prefix / "lib" / "pkgconfig", prefix / "curl-openssl" / "lib" / "pkgconfig",
                prefix / "share" / "pkgconfig"]
    roots = [prefix, prefix / "curl-openssl", sdk]
    work = args.keep.resolve() if args.keep else Path(tempfile.mkdtemp(prefix="crt-web-deps-"))
    work.mkdir(parents=True, exist_ok=True)
    toolchain = work / "toolchain.cmake"
    windows = sys.platform == "win32"
    suffix = ".cmd" if windows else ""
    # Windows is left as CMake detects it, like the SDK's crt-toolchain.cmake; the wrappers are
    # the .cmd files and every path in a CMake file uses forward slashes.
    system_line = "" if windows else f"set(CMAKE_SYSTEM_NAME {'Darwin' if sys.platform == 'darwin' else 'Linux'})\n"
    toolchain.write_text(
        system_line +
        f'set(CMAKE_C_COMPILER "{(sdk / "tools" / ("crt-cc" + suffix)).as_posix()}")\n'
        f'set(CMAKE_CXX_COMPILER "{(sdk / "tools" / ("crt-c++" + suffix)).as_posix()}")\n'
        "set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)\n"
        f'set(CMAKE_FIND_ROOT_PATH "{";".join(r.as_posix() for r in roots)}")\n'
        "set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)\n"
        "set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)\n"
        "set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)\n"
        "set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)\n"
        "set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH OFF)\n"
        "set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH OFF)\n",
        encoding="utf-8")
    archiver = shutil.which("llvm-ar") or shutil.which("llvm-ar-21") or shutil.which("ar")
    ranlib = shutil.which("llvm-ranlib") or shutil.which("llvm-ranlib-21") or shutil.which("ranlib")
    with toolchain.open("a", encoding="utf-8") as handle:
        handle.write(f'set(CMAKE_AR "{Path(archiver).as_posix()}" CACHE FILEPATH "")\n'
                     f'set(CMAKE_RANLIB "{Path(ranlib).as_posix()}" CACHE FILEPATH "")\n')
    env = dict(os.environ)
    env["CRT_SYSROOT"] = sdk.as_posix() if windows else str(sdk)
    env["CRT_TARGET_OS"] = "windows" if windows else ("macos" if sys.platform == "darwin" else "linux")
    if windows:
        # What the SDK's activate script sets for a consumer (see tools/build_webkit_jsc.py).
        env["CRT_ROOTFS"] = sdk.as_posix()
        env["CRT_MKSH_EXE"] = f"{sdk.as_posix()}/system/bin/mksh.exe"
        env["PATH"] = os.pathsep.join([str(sdk / "tools"), str(sdk / "system" / "bin"), str(sdk / "bin"),
                                       env.get("PATH", "")])
        for variable, command in (("CRT_HOST_CC", "clang"), ("CRT_HOST_CXX", "clang++")):
            found = shutil.which(command)
            if found and not env.get(variable):
                env[variable] = found.replace("\\", "/")
        sys.path.insert(0, str(ROOT / "tools"))
        import build_webkit_jsc as jsc  # noqa: E402
        libpath = jsc.windows_sdk_libpath(env)
        if libpath:
            env["CRT_WINDOWS_SDK_LIBPATH"] = libpath
    env["PKG_CONFIG_PATH"] = os.pathsep.join(str(d) for d in pkg_dirs)
    env["PKG_CONFIG_LIBDIR"] = env["PKG_CONFIG_PATH"]
    env.pop("LD_LIBRARY_PATH", None)
    build = work / "build"
    if build.exists():
        shutil.rmtree(build)
    command = ["cmake", "-S", str(PROJECT), "-B", str(build), "-G", "Ninja",
               f"-DCMAKE_TOOLCHAIN_FILE={toolchain.as_posix()}", f"-DWEBKIT_SOURCE_DIR={source.as_posix()}",
               f"-DCMAKE_PREFIX_PATH={prefix.as_posix()}", f"-DCURL_ROOT={(prefix / 'curl-openssl').as_posix()}",
               f"-DICU_ROOT={prefix.as_posix()}",
               f"-DCMAKE_MAKE_PROGRAM={shutil.which('ninja')}"]
    completed = subprocess.run(command, env=env, text=True, capture_output=True)
    print(completed.stdout, end="")
    if completed.returncode != 0:
        print(completed.stderr, end="", file=sys.stderr)
        print("web dependency check: FAILED")
        return 1
    results = [line for line in completed.stdout.splitlines() if "RESULT " in line]
    leaks = [line for line in results if " /usr/" in line or "/lib/x86_64-linux-gnu" in line]
    if leaks:
        print("web dependency check: a dependency resolved to a host path:", *leaks, sep="\n  ")
        return 1
    print(f"web dependency check: ok ({len(results)} dependencies resolved under {prefix})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
