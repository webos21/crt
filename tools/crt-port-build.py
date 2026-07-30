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


def is_native_windows_configure(target_os):
    return target_os == "windows" and os.name == "nt"


def path_for_posix_shell(path):
    value = str(path).replace("\\", "/")
    if len(value) >= 2 and value[1] == ":":
        drive = value[0].lower()
        rest = value[2:]
        if rest.startswith("/"):
            rest = rest[1:]
        return f"/{drive}/{rest}"
    return value


def find_posix_shell(env):
    for name in ("CRT_PORT_SHELL", "CONFIG_SHELL"):
        value = env.get(name)
        if value:
            return value

    for name in ("bash", "sh"):
        value = shutil.which(name)
        if value:
            return value

    raise SystemExit(
        "configure recipes on native Windows require a POSIX shell. "
        "Install MSYS2 or Git Bash, put bash.exe/sh.exe in PATH, or set "
        "CRT_PORT_SHELL to the shell executable path."
    )


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
    use_posix_paths = is_native_windows_configure(target_os)
    root_env = path_for_posix_shell(root) if use_posix_paths else str(root)
    build_dir_env = path_for_posix_shell(build_dir) if use_posix_paths else str(build_dir)
    sysroot_env = path_for_posix_shell(sysroot) if use_posix_paths else str(sysroot)
    port_prefix_env = path_for_posix_shell(port_prefix) if use_posix_paths else str(port_prefix)
    tools_dir_env = path_for_posix_shell(root / "tools") if use_posix_paths else str(root / "tools")

    env["CRT_SYSROOT"] = sysroot_env
    env["CRT_TARGET_OS"] = target_os
    env["CC"] = f"{root_env}/tools/crt-cc" if use_posix_paths else str(root / "tools" / "crt-cc")
    env["CXX"] = f"{root_env}/tools/crt-c++" if use_posix_paths else str(root / "tools" / "crt-c++")
    env["AR"] = env.get("AR") or shutil.which("llvm-ar") or shutil.which("ar") or "ar"
    env["RANLIB"] = env.get("RANLIB") or shutil.which("llvm-ranlib") or shutil.which("ranlib") or "ranlib"
    env["STRIP"] = env.get("STRIP") or shutil.which("llvm-strip") or shutil.which("strip") or "strip"
    env["PKG_CONFIG_LIBDIR"] = f"{port_prefix_env}/lib/pkgconfig"
    env["PKG_CONFIG_PATH"] = env["PKG_CONFIG_LIBDIR"]
    include_flags = f"-I{port_prefix_env}/include"
    lib_flags = f"-L{port_prefix_env}/lib"
    env["CPPFLAGS"] = join_flags(include_flags, env.get("CRT_EXTRA_CPPFLAGS", ""))
    env["CFLAGS"] = join_flags(env.get("CRT_PORT_CFLAGS", "-O2"), env.get("CRT_EXTRA_CFLAGS", ""))
    env["CXXFLAGS"] = join_flags(env.get("CRT_PORT_CXXFLAGS", "-O2"), env.get("CRT_EXTRA_CXXFLAGS", ""))
    env["LDFLAGS"] = join_flags(lib_flags, env.get("CRT_EXTRA_LDFLAGS", ""))
    env["LIBS"] = env.get("CRT_EXTRA_LIBS", "")
    env["PATH"] = f"{tools_dir_env}{os.pathsep}{env.get('PATH', '')}"
    env["DESTDIR"] = ""
    env["CRT_PORT_BUILD_DIR"] = build_dir_env
    return env


def apply_recipe_env(env, recipe):
    for name, value in recipe["build"].get("env", {}).items():
        env[name] = str(value)


def build_configure_port(root, work, port_prefix, recipe, env, target_os):
    build = recipe["build"]
    configure = ["./configure"]
    configure.extend(build["configure_args"])
    prefix = path_for_posix_shell(port_prefix) if is_native_windows_configure(target_os) else str(port_prefix)
    configure.append(f"--prefix={prefix}")
    if is_native_windows_configure(target_os):
        shell = find_posix_shell(env)
        env["CONFIG_SHELL"] = path_for_posix_shell(shell)
        configure = [shell] + configure
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
        build_configure_port(root, work, port_prefix, recipe, env, target_os)
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
