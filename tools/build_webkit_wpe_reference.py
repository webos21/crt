#!/usr/bin/env python3
"""Build and run the native Linux WPE reference (Web Tranche 2).

This deliberately does not use a CRT SDK. It builds the verified WPE WebKit
tarball with the host toolchain, enables only WPEPlatform's built-in headless
backend, and runs a small native harness that loads local HTML and captures a
640x480 snapshot. No file in the extracted WebKit tree is patched.

The host needs WPE's normal development dependencies. ``--native-sysroot`` is
provided for build machines where those packages have been extracted into a
private prefix instead of installed system-wide; it is a dependency lookup
root, not a CRT sysroot.
"""

import argparse
import hashlib
import json
import os
import platform
import shlex
import shutil
import subprocess
import sys
import tarfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RECIPE = ROOT / "libcrtweb" / "third_party" / "webkit" / "recipe.json"
TESTS = ROOT / "libcrtweb" / "tests" / "wpe-reference"
PATCH_MANIFEST = ROOT / "libcrtweb" / "patches" / "manifest.json"

OPTIONS = [
    "-DPORT=WPE",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DDEVELOPER_MODE=OFF",
    "-DUSE_HEADER_MAPS=OFF",
    "-DENABLE_MINIBROWSER=ON",
    "-DENABLE_API_TESTS=OFF",
    "-DENABLE_LAYOUT_TESTS=OFF",
    "-DENABLE_DOCUMENTATION=OFF",
    "-DENABLE_INTROSPECTION=OFF",
    "-DENABLE_JOURNALD_LOG=OFF",
    "-DENABLE_BUBBLEWRAP_SANDBOX=OFF",
    "-DENABLE_WPE_LEGACY_API=OFF",
    "-DENABLE_WPE_PLATFORM=ON",
    "-DENABLE_WPE_PLATFORM_HEADLESS=ON",
    "-DENABLE_WPE_PLATFORM_DRM=OFF",
    "-DENABLE_WPE_PLATFORM_WAYLAND=OFF",
    "-DENABLE_VIDEO=ON",
    "-DENABLE_WEB_AUDIO=OFF",
    "-DENABLE_WEB_CODECS=ON",
    "-DENABLE_MEDIA_STREAM=OFF",
    "-DENABLE_MEDIA_CAPTURE=OFF",
    "-DENABLE_SPEECH_SYNTHESIS=OFF",
    "-DENABLE_GAMEPAD=OFF",
    "-DENABLE_WEBXR=OFF",
    "-DENABLE_WEBGL=OFF",
    "-DENABLE_WEBDRIVER=OFF",
    "-DENABLE_XSLT=OFF",
    "-DENABLE_SPELLCHECK=OFF",
    "-DUSE_ATK=OFF",
    "-DUSE_AVIF=OFF",
    "-DUSE_FLITE=OFF",
    "-DUSE_GBM=OFF",
    "-DUSE_JPEGXL=OFF",
    "-DUSE_LCMS=OFF",
    "-DUSE_LIBBACKTRACE=OFF",
    "-DUSE_LIBDRM=ON",
    "-DUSE_LIBHYPHEN=OFF",
    "-DUSE_SYSPROF_CAPTURE=OFF",
    "-DUSE_SYSTEM_SYSPROF_CAPTURE=OFF",
    "-DUSE_SYSTEM_UNIFDEF=OFF",
]


def run(command, *, env=None, cwd=None, capture=False):
    print("+", " ".join(shlex.quote(str(part)) for part in command), flush=True)
    try:
        return subprocess.run([str(part) for part in command], env=env, cwd=cwd,
                              check=True, text=True, capture_output=capture)
    except subprocess.CalledProcessError as error:
        if error.stdout:
            print(error.stdout, end="")
        if error.stderr:
            print(error.stderr, end="", file=sys.stderr)
        raise


def tool_version(command, env=None):
    result = run(command, capture=True, env=env)
    text = (result.stdout or result.stderr).strip()
    return text.splitlines()[0] if text else "?"


def require_host(env):
    if sys.platform != "linux" or platform.machine().lower() not in ("x86_64", "amd64"):
        raise SystemExit("the Tranche 2 reference is currently defined for native Linux/x86_64")
    search_path = env.get("PATH")
    missing = [name for name in ("cmake", "ninja", "pkg-config", "gperf", "perl", "ruby", "cc")
               if shutil.which(name, path=search_path) is None]
    if missing:
        raise SystemExit("missing native build tools: " + ", ".join(missing))
    return {name: tool_version([name, "--version"], env)
            for name in ("cmake", "ninja", "pkg-config", "gperf", "cc")}


def fetch_and_extract(cache, work, clean):
    recipe = json.loads(RECIPE.read_text(encoding="utf-8"))
    tree = work / "src" / f"wpewebkit-{recipe['version']}"
    marker = tree.parent / (".extracted-" + recipe["source"]["archive_sha256"])
    run([sys.executable, ROOT / "tools" / "fetch_webkit.py", "--recipe", RECIPE, "--cache", cache])
    if clean and tree.parent.exists():
        shutil.rmtree(tree.parent)
    if marker.is_file() and tree.is_dir():
        return tree, recipe
    if tree.parent.exists():
        shutil.rmtree(tree.parent)
    tree.parent.mkdir(parents=True)
    archive = cache / recipe["source"]["archive_name"]
    with tarfile.open(archive) as handle:
        handle.extractall(tree.parent, filter="data")
    marker.write_text("unmodified archive extraction\n", encoding="utf-8")
    return tree, recipe


def verify_no_crt_patches(tree):
    """The reference must not accidentally reuse a PlatformCRT/JSC patched tree."""
    manifest = json.loads(PATCH_MANIFEST.read_text(encoding="utf-8"))
    checked = set()
    for patch in manifest["patches"]:
        for item in patch["files"]:
            if item["file"] in checked:
                continue
            checked.add(item["file"])
            digest = hashlib.sha256((tree / item["file"]).read_bytes()).hexdigest()
            if digest != item["sha256_before"]:
                raise SystemExit(f"native WPE reference requires pristine upstream file: {item['file']}")


def native_environment(native_sysroot):
    env = dict(os.environ)
    if native_sysroot:
        prefix = native_sysroot.resolve()
        lib = prefix / "usr" / "lib" / "x86_64-linux-gnu"
        include = prefix / "usr" / "include"
        pc_paths = [lib / "pkgconfig", prefix / "usr" / "lib" / "pkgconfig",
                    prefix / "usr" / "share" / "pkgconfig"]
        env["PKG_CONFIG_SYSROOT_DIR"] = str(prefix)
        env["PKG_CONFIG_PATH"] = os.pathsep.join(str(path) for path in pc_paths)
        env["CMAKE_PREFIX_PATH"] = str(prefix / "usr")
        env["CMAKE_INCLUDE_PATH"] = str(include)
        env["CMAKE_LIBRARY_PATH"] = str(lib)
        env["PATH"] = os.pathsep.join((str(prefix / "usr" / "bin"), env.get("PATH", "")))
    return env


def configure(tree, build, env, native_sysroot):
    # Release builds deliberately ignore WEBKIT_EXEC_PATH.  Give the unmodified
    # build a private install prefix so its compiled-in Web/Network process paths
    # never point at (or require writes to) the host's /usr/local tree.
    install = build.parent / "install"
    command = ["cmake", "-S", tree, "-B", build, "-G", "Ninja",
               f"-DCMAKE_INSTALL_PREFIX={install}",
               f"-DEXEC_INSTALL_DIR={install / 'bin'}",
               f"-DLIB_INSTALL_DIR={install / 'lib'}",
               f"-DLIBEXEC_INSTALL_DIR={install / 'libexec' / 'wpe-webkit-2.0'}",
               *OPTIONS]
    if native_sysroot:
        multiarch = native_sysroot.resolve() / "usr" / "include" / "x86_64-linux-gnu"
        command.extend((f"-DCMAKE_C_FLAGS=-isystem {multiarch}",
                        f"-DCMAKE_CXX_FLAGS=-isystem {multiarch}"))
    run(command, env=env)


def compile_harness(build, env, output):
    pc_env = dict(env)
    old_path = pc_env.get("PKG_CONFIG_PATH", "")
    pc_env["PKG_CONFIG_PATH"] = os.pathsep.join(filter(None, (str(build), old_path)))
    flags = run(["pkg-config", "--cflags", "--libs", "wpe-webkit-2.0-uninstalled",
                 "wpe-platform-headless-2.0-uninstalled"], env=pc_env, capture=True).stdout.split()
    # pkg-config correctly prefixes dependency /usr paths with the private native root,
    # but it also prefixes absolute paths emitted by WebKit's uninstalled .pc files.
    # Those are build-tree paths, not paths inside the native dependency root.
    if pc_env.get("PKG_CONFIG_SYSROOT_DIR"):
        bad_prefix = pc_env["PKG_CONFIG_SYSROOT_DIR"] + str(build)
        flags = [flag.replace(bad_prefix, str(build)) for flag in flags]
    run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", TESTS / "wpe_reference.c",
         "-o", output, *flags], env=pc_env)


def run_acceptance(build, env, harness, result_dir, recipe, started):
    result_dir.mkdir(parents=True, exist_ok=True)
    snapshot = result_dir / "wpe-reference.ppm"
    run_env = dict(env)
    library_paths = [build / "lib"]
    if env.get("CMAKE_LIBRARY_PATH"):
        library_paths.append(Path(env["CMAKE_LIBRARY_PATH"]))
    if run_env.get("LD_LIBRARY_PATH"):
        library_paths.append(Path(run_env["LD_LIBRARY_PATH"]))
    run_env.update({
        "LD_LIBRARY_PATH": os.pathsep.join(str(path) for path in library_paths),
        "WEBKIT_EXEC_PATH": str(build / "bin"),
        "WEBKIT_INJECTED_BUNDLE_PATH": str(build / "lib"),
        "WEBKIT_INSPECTOR_RESOURCES_PATH": str(build / "share"),
        "WEBKIT_DISABLE_SANDBOX_THIS_IS_DANGEROUS": "1",
        "GIO_USE_VFS": "local",
        "GSETTINGS_BACKEND": "memory",
        "LIBGL_ALWAYS_SOFTWARE": "1",
        "MESA_SHADER_CACHE_DISABLE": "true",
        "LC_ALL": "C.UTF-8",
        "WPE_PLATFORM": "headless",
    })
    completed = run([harness, TESTS / "reference.html", snapshot], env=run_env, capture=True)
    print(completed.stdout, end="")
    if completed.stderr:
        print(completed.stderr, end="", file=sys.stderr)
    digest = hashlib.sha256(snapshot.read_bytes()).hexdigest()
    result = {
        "schema": 1,
        "status": "pass",
        "purpose": "native Linux WPE reference; no CRT integration",
        "webkit_version": recipe["version"],
        "source_archive_sha256": recipe["source"]["archive_sha256"],
        "backend": "WPEPlatform built-in headless",
        "viewport": {"width": 640, "height": 480},
        "fixture": str(TESTS / "reference.html"),
        "snapshot": snapshot.name,
        "snapshot_sha256": digest,
        "elapsed_seconds": round(time.monotonic() - started, 1),
        "configure_options": OPTIONS,
    }
    (result_dir / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"WPE reference PASS: {snapshot} sha256={digest}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work", type=Path, default=ROOT / "out" / "web-reference")
    parser.add_argument("--cache", type=Path, default=ROOT / "out" / "web" / "cache")
    parser.add_argument("--source-work", type=Path, default=ROOT / "out" / "web",
                        help="shared verified WebKit extraction root (default: out/web)")
    parser.add_argument("--native-sysroot", type=Path,
                        help="optional private root containing native Linux development packages")
    parser.add_argument("--jobs", type=int, default=min(16, max(1, os.cpu_count() or 1)))
    parser.add_argument("--clean", action="store_true", help="re-extract and reconfigure from scratch")
    parser.add_argument("--skip-build", action="store_true", help="reuse an existing completed build")
    args = parser.parse_args()
    started = time.monotonic()
    args.work.mkdir(parents=True, exist_ok=True)
    args.cache.mkdir(parents=True, exist_ok=True)
    env = native_environment(args.native_sysroot)
    tools = require_host(env)
    tree, recipe = fetch_and_extract(args.cache, args.source_work, args.clean)
    verify_no_crt_patches(tree)
    build = args.work / "build"
    if args.clean and build.exists():
        shutil.rmtree(build)
    if args.clean and (args.work / "install").exists():
        shutil.rmtree(args.work / "install")
    if not args.skip_build:
        configure(tree, build, env, args.native_sysroot)
        run(["cmake", "--build", build, "--target", "MiniBrowser",
             "InspectorResources", "WPEInjectedBundle", "-j", str(args.jobs)], env=env)
    # The Release process launcher uses CMake's compiled-in install paths.  This
    # is also needed for --skip-build when resuming after a completed build.
    run(["cmake", "--install", build], env=env)
    harness = args.work / "wpe-reference"
    compile_harness(build, env, harness)
    fingerprint = {
        "host": {"system": platform.system(), "machine": platform.machine()},
        "tools": tools,
        "native_sysroot": str(args.native_sysroot.resolve()) if args.native_sysroot else None,
        "webkit_version": recipe["version"],
        "source_archive_sha256": recipe["source"]["archive_sha256"],
        "source_policy": "verified tarball, unmodified",
        "configure_options": OPTIONS,
    }
    (args.work / "configure-fingerprint.json").write_text(
        json.dumps(fingerprint, indent=2) + "\n", encoding="utf-8")
    run_acceptance(build, env, harness, args.work / "result", recipe, started)


if __name__ == "__main__":
    main()
