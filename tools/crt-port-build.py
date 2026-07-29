#!/usr/bin/env python3
import argparse
import os
import shutil
import subprocess
from pathlib import Path


PORTS = {
    "zlib": {
        "source_glob": "zlib-*",
        "configure": ["./configure", "--static"],
        "build_subdir": ".",
        "needs": [],
    },
    "libpng": {
        "source_glob": "libpng-*",
        "configure": ["./configure", "--disable-shared", "--enable-static"],
        "build_subdir": ".",
        "needs": ["zlib"],
    },
}


def run(args, cwd, env):
    print("+", " ".join(str(a) for a in args), flush=True)
    subprocess.run(args, cwd=cwd, env=env, check=True)


def find_source(source_root, pattern):
    candidates = sorted(source_root.glob(pattern), reverse=True)
    if not candidates:
        raise SystemExit(f"source not found: {source_root}/{pattern}")
    return candidates[0]


def copy_source(src, dst):
    if dst.exists():
        shutil.rmtree(dst)
    ignore = shutil.ignore_patterns(".git", "autom4te.cache", "*.o", "*.a", "*.so", "*.dylib", "*.dll")
    shutil.copytree(src, dst, ignore=ignore)


def make_env(root, build_dir, sysroot, port_prefix, target_os):
    env = os.environ.copy()
    env["CRT_SYSROOT"] = str(sysroot)
    env["CRT_TARGET_OS"] = target_os
    env["CC"] = str(root / "tools" / "crt-cc")
    env["CXX"] = str(root / "tools" / "crt-c++")
    env["AR"] = env.get("AR") or shutil.which("llvm-ar") or shutil.which("ar") or "ar"
    env["RANLIB"] = env.get("RANLIB") or shutil.which("llvm-ranlib") or shutil.which("ranlib") or "ranlib"
    env["STRIP"] = env.get("STRIP") or shutil.which("llvm-strip") or shutil.which("strip") or "strip"
    env["PKG_CONFIG_LIBDIR"] = str(port_prefix / "lib" / "pkgconfig")
    env["PKG_CONFIG_PATH"] = env["PKG_CONFIG_LIBDIR"]
    include_flags = f"-I{port_prefix / 'include'}"
    lib_flags = f"-L{port_prefix / 'lib'}"
    env["CPPFLAGS"] = f"{include_flags} {env.get('CPPFLAGS', '')}".strip()
    env["CFLAGS"] = env.get("CFLAGS", "-O2")
    env["CXXFLAGS"] = env.get("CXXFLAGS", "-O2")
    env["LDFLAGS"] = f"{lib_flags} {env.get('LDFLAGS', '')}".strip()
    env["PATH"] = f"{root / 'tools'}{os.pathsep}{env.get('PATH', '')}"
    env["DESTDIR"] = ""
    env["CRT_PORT_BUILD_DIR"] = str(build_dir)
    return env


def build_port(root, build_dir, source_root, sysroot, port_prefix, port, target_os):
    spec = PORTS[port]
    for dep in spec["needs"]:
        build_port(root, build_dir, source_root, sysroot, port_prefix, dep, target_os)

    stamp = build_dir / "stamps" / f"{port}.installed"
    if stamp.exists():
        return

    src = find_source(source_root, spec["source_glob"])
    work = build_dir / "work" / port
    copy_source(src, work)

    env = make_env(root, build_dir, sysroot, port_prefix, target_os)
    configure = list(spec["configure"])
    configure.append(f"--prefix={port_prefix}")
    run(configure, work / spec["build_subdir"], env)
    run(["make", "-j", str(os.cpu_count() or 2)], work / spec["build_subdir"], env)
    run(["make", "install"], work / spec["build_subdir"], env)
    stamp.parent.mkdir(parents=True, exist_ok=True)
    stamp.write_text("ok\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--preset", required=True)
    parser.add_argument("--target-os", default=None)
    parser.add_argument("--source-root", default=None)
    parser.add_argument("--work-root", default=None)
    parser.add_argument("--install-prefix", default=None)
    parser.add_argument("--port", action="append", choices=sorted(PORTS), required=True)
    parser.add_argument("--rebuild", action="store_true", help="remove port install stamps before building")
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    build_dir = root / "out" / args.preset
    port_test_root = build_dir / "port-tests"
    source_root = Path(args.source_root) if args.source_root else port_test_root / "src"
    work_root = Path(args.work_root) if args.work_root else port_test_root / "build"
    sysroot = build_dir / "sysroot"
    port_prefix = Path(args.install_prefix) if args.install_prefix else port_test_root / "install"
    target_os = args.target_os or args.preset.split("-host-", 1)[0]

    run(["cmake", "--build", "--preset", args.preset, "--target", "sysroot"], root, os.environ.copy())
    port_prefix.mkdir(parents=True, exist_ok=True)

    if args.rebuild:
        for port in args.port:
            stamp = work_root / "stamps" / f"{port}.installed"
            if stamp.exists():
                stamp.unlink()

    for port in args.port:
        build_port(root, work_root, source_root, sysroot, port_prefix, port, target_os)

    print(f"ports installed: {port_prefix}")


if __name__ == "__main__":
    main()
