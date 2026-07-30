#!/usr/bin/env python3
import argparse
import json
import os
import shlex
import shutil
import subprocess
from pathlib import Path


def load_recipes(recipe_dir):
    recipes = {}
    for path in sorted(Path(recipe_dir).glob("*.json")):
        with open(path, "r", encoding="utf-8") as f:
            recipe = json.load(f)
        recipes[recipe["name"]] = recipe
    return recipes


def run(args, cwd, env):
    print("+", " ".join(str(a) for a in args), flush=True)
    subprocess.run(args, cwd=cwd, env=env, check=True)


def join_flags(*parts):
    return " ".join(part for part in parts if part).strip()


def find_source(source_root, source_dir):
    exact = source_root / source_dir
    if exact.exists():
        return exact

    pattern = source_dir.split("-", 1)[0] + "-*"
    candidates = sorted(source_root.glob(pattern), reverse=True)
    if not candidates:
        raise SystemExit(f"source not found: {exact}")
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
    env["CPPFLAGS"] = join_flags(include_flags, env.get("CRT_EXTRA_CPPFLAGS", ""))
    env["CFLAGS"] = join_flags(env.get("CRT_PORT_CFLAGS", "-O2"), env.get("CRT_EXTRA_CFLAGS", ""))
    env["CXXFLAGS"] = join_flags(env.get("CRT_PORT_CXXFLAGS", "-O2"), env.get("CRT_EXTRA_CXXFLAGS", ""))
    env["LDFLAGS"] = join_flags(lib_flags, env.get("CRT_EXTRA_LDFLAGS", ""))
    env["LIBS"] = env.get("CRT_EXTRA_LIBS", "")
    env["PATH"] = f"{root / 'tools'}{os.pathsep}{env.get('PATH', '')}"
    env["DESTDIR"] = ""
    env["CRT_PORT_BUILD_DIR"] = str(build_dir)
    return env


def apply_recipe_env(env, recipe):
    for name, value in recipe["build"].get("env", {}).items():
        env[name] = str(value)


def build_configure_port(root, work, port_prefix, recipe, env):
    build = recipe["build"]
    configure = ["./configure"]
    configure.extend(build["configure_args"])
    configure.append(f"--prefix={port_prefix}")
    run(configure, work, env)
    run(["make", "-j", str(os.cpu_count() or 2)] + build["make_args"], work, env)
    run(["make", "install"], work, env)


def build_amalgamation_port(work, port_prefix, recipe, env):
    build = recipe["build"]
    objects = []
    cflags = shlex.split(env.get("CPPFLAGS", "")) + shlex.split(env.get("CFLAGS", ""))
    cflags.extend(build.get("cflags", []))

    for source in build["sources"]:
        src = work / source
        obj = work / (Path(source).name + ".o")
        run([env["CC"]] + cflags + ["-I", str(work), "-c", str(src), "-o", str(obj)], work, env)
        objects.append(obj)

    lib_dir = port_prefix / "lib"
    include_dir = port_prefix / "include"
    lib_dir.mkdir(parents=True, exist_ok=True)
    include_dir.mkdir(parents=True, exist_ok=True)
    archive = lib_dir / build["archive"]
    run([env["AR"], "rcs", str(archive)] + [str(obj) for obj in objects], work, env)
    run([env["RANLIB"], str(archive)], work, env)
    for header in build.get("install_headers", []):
        shutil.copy2(work / header, include_dir / Path(header).name)


def build_port(root, build_dir, source_root, sysroot, port_prefix, recipes, port, target_os):
    if port not in recipes:
        raise SystemExit(f"recipe not found: {port}")

    recipe = recipes[port]
    build = recipe["build"]
    if build["system"] not in ("configure", "amalgamation"):
        raise SystemExit(f"{port}: build system '{build['system']}' is not supported by crt-port-build.py yet")

    for dep in recipe["dependencies"]:
        build_port(root, build_dir, source_root, sysroot, port_prefix, recipes, dep, target_os)

    stamp = build_dir / "stamps" / f"{port}.installed"
    if stamp.exists():
        return

    src = find_source(source_root, recipe["source"]["source_dir"])
    work = build_dir / "work" / port
    copy_source(src, work)

    env = make_env(root, build_dir, sysroot, port_prefix, target_os)
    apply_recipe_env(env, recipe)
    if build["system"] == "configure":
        build_configure_port(root, work, port_prefix, recipe, env)
    elif build["system"] == "amalgamation":
        build_amalgamation_port(work, port_prefix, recipe, env)
    stamp.parent.mkdir(parents=True, exist_ok=True)
    stamp.write_text("ok\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--preset", required=True)
    parser.add_argument("--target-os", default=None)
    parser.add_argument("--source-root", default=None)
    parser.add_argument("--work-root", default=None)
    parser.add_argument("--install-prefix", default=None)
    parser.add_argument("--recipe-dir", default="porting/recipes")
    parser.add_argument("--port", action="append", required=True)
    parser.add_argument("--rebuild", action="store_true", help="remove port install stamps before building")
    parser.add_argument("--skip-sysroot-build", action="store_true", help="assume the sysroot target has already been built")
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    recipe_dir = Path(args.recipe_dir)
    if not recipe_dir.is_absolute():
        recipe_dir = root / recipe_dir
    recipe_dir = recipe_dir.resolve()
    recipes = load_recipes(recipe_dir)
    build_dir = (root / "out" / args.preset).resolve()
    port_test_root = build_dir / "port-tests"
    source_root = Path(args.source_root) if args.source_root else port_test_root / "src"
    work_root = Path(args.work_root) if args.work_root else port_test_root / "build"
    sysroot = build_dir / "sysroot"
    port_prefix = Path(args.install_prefix) if args.install_prefix else port_test_root / "install"
    source_root = source_root.resolve()
    work_root = work_root.resolve()
    sysroot = sysroot.resolve()
    port_prefix = port_prefix.resolve()
    target_os = args.target_os or args.preset.split("-host-", 1)[0]

    if not args.skip_sysroot_build:
        run(["cmake", "--build", "--preset", args.preset, "--target", "sysroot"], root, os.environ.copy())
    port_prefix.mkdir(parents=True, exist_ok=True)
    (port_prefix / "include").mkdir(parents=True, exist_ok=True)
    (port_prefix / "lib" / "pkgconfig").mkdir(parents=True, exist_ok=True)

    if args.rebuild:
        for port in args.port:
            stamp = work_root / "stamps" / f"{port}.installed"
            if stamp.exists():
                stamp.unlink()

    for port in args.port:
        build_port(root, work_root, source_root, sysroot, port_prefix, recipes, port, target_os)

    print(f"ports installed: {port_prefix}")


if __name__ == "__main__":
    main()
