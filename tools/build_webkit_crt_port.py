#!/usr/bin/env python3
"""Configure and build the PlatformCRT port with WebCore (Web Tranche 3B, milestone B3).

Where tools/build_webkit_jsc.py --port CRT builds JavaScriptCore alone (B1), this driver composes the
same tree (the pinned full WebKit commit plus libcrtweb/port and the carried patches), configures it
with -DCRT_ENABLE_WEBCORE=ON, and builds the requested targets: the bundled Skia, PAL, WebCore and
the test program. The target-side dependencies are the CRT ports of milestone B2 installed under one
--deps-prefix (porting/recipes, group "web"), found by WebKit's own Find modules; the BUILD-host tools
are CMake, Ninja, Perl, Ruby, Python and gperf (WebCore needs it to generate its hash tables).

Nothing in the WebKit tree is patched beyond libcrtweb/patches/manifest.json; every deviation is a
CMake option or a compiler flag.
"""

import argparse
import json
import os
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import build_webkit_jsc as jsc  # noqa: E402  (shared compose/patch/environment helpers)

DEFAULT_TARGETS = ("PAL", "WebCore")


def port_environment(sdk: Path, prefix: Path, gperf: Path | None, target_os: str = "linux") -> dict:
    env = jsc.build_environment(sdk, prefix, target_os)
    env.pop("LD_LIBRARY_PATH", None)
    env.pop("DYLD_LIBRARY_PATH", None)
    # The OpenSSL-backed libcurl's libcurl.pc first: the prefix also holds the mbedTLS curl (the
    # media stack's), whose libcurl.pc would otherwise satisfy FindCURL's pkg-config hint and put a
    # second image of the same name on the link line. Linux's loader merges them by soname; Mach-O
    # keeps both, so the program loaded two libcurls.
    dirs = [prefix / "curl-openssl" / "lib" / "pkgconfig", prefix / "lib" / "pkgconfig",
            prefix / "share" / "pkgconfig"]
    env["PKG_CONFIG_PATH"] = os.pathsep.join(str(d) for d in dirs)
    env["PKG_CONFIG_LIBDIR"] = env["PKG_CONFIG_PATH"]
    if gperf is not None:
        env["PATH"] = str(gperf.parent) + os.pathsep + env.get("PATH", "")
    if target_os == "windows":
        # WebKit's generators are Python and Perl programs that read their input with the process's
        # default encoding (cp949 here), not the UTF-8 the sources are in.
        env["PYTHONUTF8"] = "1"
        # A native Windows Perl: the bindings generator compares paths the build hands it with paths
        # it computes, which MSYS's Perl (Git for Windows) spells /c/... against C:/...
        for candidate in (Path("C:/Strawberry/perl/bin"),):
            if (candidate / "perl.exe").exists():
                env["PATH"] = str(candidate) + os.pathsep + env.get("PATH", "")
        # The pkg-config of the build host (pkgconf, which winget installs outside the PATH of a
        # running session), for WebKit's Find modules and the ports' .pc files.
        for base in (os.environ.get("ProgramFiles"), os.environ.get("ProgramFiles(x86)")):
            if base:
                for candidate in sorted(Path(base).glob("pkgconf*")):
                    if (candidate / "pkg-config.exe").exists() or (candidate / "pkgconf.exe").exists():
                        env["PATH"] = env.get("PATH", "") + os.pathsep + str(candidate)
        env["PKG_CONFIG_PATH"] = os.pathsep.join(d.as_posix() for d in dirs)
        env["PKG_CONFIG_LIBDIR"] = env["PKG_CONFIG_PATH"]
    return env


def configure_command(tree: Path, build: Path, sdk: Path, prefix: Path, arch: str, extra: list,
                      target_os: str = "linux") -> list:
    flags = jsc.target_c_flags(target_os, arch)
    cxx_flags = list(flags)
    link_flags = ["-Wl,--allow-shlib-undefined"] if target_os == "linux" else []
    platform_options = []
    if target_os == "windows":
        # As in build_webkit_jsc.configure() for Windows: the CRT wrappers come from the SDK's
        # toolchain file; the Linux-shaped presentation (flags above) selects WebKit's portable
        # source surface. __unix__ completes the persona for eglplatform.h (real Linux defines both).
        flags = flags + ["-D__unix__=1"]
        cxx_flags = list(flags)
        link_flags = ["-Wl,--enable-auto-import"]
        platform_options = [f"-DCMAKE_PROJECT_INCLUDE={ROOT / 'libcrtweb' / 'cmake' / 'crt_webkit_platform.cmake'}",
                            # A WebCore compile line (several hundred -I and -D, and the precompiled-header
                            # one) is longer than the 8,191 characters cmd.exe passes to the .cmd compiler
                            # wrappers: defines, includes and flags go through a response file. Some
                            # WebCore sources rely on the precompiled header for their includes, so it stays on.
                            "-DCMAKE_NINJA_FORCE_RESPONSE_FILE=ON"]
    if target_os == "macos":
        # The Linux-shaped presentation of macOS to WebKit, as in build_webkit_jsc.configure():
        # the CRT compilers are named (WebKitXcodeSDK.cmake would otherwise pin Xcode's), APPLE is
        # off, and the Baseline JIT gets Apple Silicon W^X through JavaScriptCore's own extension
        # point. __unix__ completes the persona for eglplatform.h (real Linux defines both).
        flags = flags + ["-D__unix__=1"]
        cxx_flags = flags + ["-include", str(ROOT / "libcrtweb" / "cmake" / "crt_webkit_jit_permissions.h")]
        platform_options = [f"-DCMAKE_C_COMPILER={sdk / 'tools' / 'crt-cc'}",
                            f"-DCMAKE_CXX_COMPILER={sdk / 'tools' / 'crt-c++'}",
                            f"-DCMAKE_PROJECT_INCLUDE={ROOT / 'libcrtweb' / 'cmake' / 'crt_webkit_platform.cmake'}"]
    options = [
        "-DPORT=CRT", "-DCRT_ENABLE_WEBCORE=ON", "-DCMAKE_BUILD_TYPE=Release",
        "-DUSE_HEADER_MAPS=OFF", "-DENABLE_API_TESTS=OFF", "-DDEVELOPER_MODE=OFF",
        "-DENABLE_STATIC_JSC=ON",
        # Same JIT configuration as the accepted B1 build.
        "-DENABLE_JIT=ON", "-DENABLE_C_LOOP=OFF", "-DENABLE_DFG_JIT=ON", "-DENABLE_FTL_JIT=ON",
        "-DENABLE_WEBASSEMBLY=ON", "-DENABLE_SAMPLING_PROFILER=OFF",
        f"-DCRT_WEBCORE_TESTS={ROOT / 'libcrtweb' / 'tests' / 'webcore'}",
    ]
    command = ["cmake", "-S", str(tree), "-B", str(build), "-G", "Ninja",
               f"-DCMAKE_TOOLCHAIN_FILE={sdk / 'crt-toolchain.cmake'}",
               f"-DCMAKE_PREFIX_PATH={prefix}", f"-DICU_ROOT={prefix}",
               f"-DEGL_INCLUDE_DIRS={tree / 'Source' / 'ThirdParty' / 'ANGLE' / 'include'}",
               f"-DOpenGLES2_INCLUDE_DIR={tree / 'Source' / 'ThirdParty' / 'ANGLE' / 'include'}",
               f"-DCURL_ROOT={prefix / 'curl-openssl'}",
               f"-DCMAKE_C_FLAGS={' '.join(flags)}", f"-DCMAKE_CXX_FLAGS={' '.join(cxx_flags)}"]
    if link_flags:
        command += [f"-DCMAKE_EXE_LINKER_FLAGS={' '.join(link_flags)}",
                    f"-DCMAKE_SHARED_LINKER_FLAGS={' '.join(link_flags)}"]
    return command + platform_options + options + extra


ALLOWED_NEEDED_PREFIXES = ("lib", "ld-linux")
# Host objects a CRT executable may name: the loader only. Everything else is a CRT SDK or CRT port
# library found by file name under the SDK or the port prefix.
HOST_LOADER_SONAMES = {"ld-linux-x86-64.so.2", "ld-linux-aarch64.so.1"}


def audit_program_native(program: Path, sdk: Path, prefix: Path, build: Path, target_os: str) -> dict:
    """Mach-O / PE B3 audit: no GLib/GIO/GObject symbol is referenced, and every load command (or
    import) is a CRT SDK library, a port-prefix library, this build tree or the host's own system
    library (the same closure the JavaScriptCore audit uses)."""
    if target_os == "windows":
        nm = shutil.which("llvm-nm") or shutil.which("nm") or "nm"
    else:
        nm = shutil.which("nm") or "nm"
    symbols = jsc.run([nm, "-u", str(program)], capture=True).stdout.split()
    glib = sorted({s for s in symbols if s.lstrip("_").startswith(("g_", "G_", "gdk_", "gio_", "gobject_"))
                   or "glib" in s.lower()})
    audit = jsc.host_abi_audit(sdk, prefix, build, [program], {}, target_os)
    return {"glib_symbols": glib, "needed": sorted(audit.get("libraries", {}).get(program.name, [])),
            "host_objects": audit.get("violations", []), "ok": not glib and audit.get("ok", False)}


def audit_program(program: Path, sdk: Path, prefix: Path, env: dict) -> dict:
    """The B3 link audit: no GLib/GIO/GObject symbol is referenced, and every DT_NEEDED resolves
    under the SDK, the port prefix, or is the loader. Returns a result dictionary."""
    nm = shutil.which("llvm-nm") or shutil.which("nm")
    symbols = jsc.run([nm, "-D", "--undefined-only", str(program)], capture=True).stdout.split()
    glib = sorted({s for s in symbols if s.startswith(("g_", "G_", "gdk_", "gio_", "gobject_"))
                   or "glib" in s.lower()})
    readelf = shutil.which("llvm-readelf") or shutil.which("readelf")
    dynamic = jsc.run([readelf, "-d", str(program)], capture=True).stdout
    needed = [line.split("[", 1)[1].rstrip("]") for line in dynamic.splitlines() if "(NEEDED)" in line]
    unresolved = []
    for name in needed:
        if name in HOST_LOADER_SONAMES:
            continue
        found = False
        for root in (sdk / "lib", prefix / "lib", prefix / "curl-openssl" / "lib"):
            if (root / Path(name).name).exists():
                found = True
                break
        if not found:
            unresolved.append(name)
    return {"glib_symbols": glib, "needed": needed, "host_objects": unresolved,
            "ok": not glib and not unresolved}


def run_graph_test(build: Path, sdk: Path, prefix: Path, work: Path, env: dict, target_os: str = "linux") -> dict:
    """Run libcrtweb/tests/webcore/crtweb_webcore_graph_test with a fontconfig file that scans only
    the bundled fonts, and audit the program."""
    program = build / "bin" / ("crtweb_webcore_graph_test" + (".exe" if target_os == "windows" else ""))
    fonts = ROOT / "libcrtgfx" / "assets" / "fonts"
    cache = work / "fontcache"
    if target_os == "windows":
        # The CRT spells a drive path /c/Users/...: fontconfig only treats a leading '/' as absolute.
        def crt_path(path: Path) -> str:
            text = path.as_posix()
            return f"/{text[0].lower()}{text[2:]}" if len(text) > 2 and text[1] == ":" else text
        fonts_text, cache_text = crt_path(fonts), crt_path(cache)
    else:
        fonts_text, cache_text = str(fonts), str(cache)
    conf = work / "fonts.conf"
    conf.write_text(
        '<?xml version="1.0"?>\n<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">\n'
        f'<fontconfig><dir>{fonts_text}</dir><cachedir>{cache_text}</cachedir></fontconfig>\n',
        encoding="utf-8")
    run_env = dict(env)
    run_env["FONTCONFIG_FILE"] = crt_path(conf) if target_os == "windows" else str(conf)
    # The curl-openssl install first: both curls share the soname libcurl.so.4 (porting_status.md).
    if target_os == "windows":
        # DLLs are found through PATH: the SDK's runtime DLLs and the ports' (ICU, ...).
        run_env["PATH"] = os.pathsep.join(
            [str(sdk / "bin"), str(prefix / "bin"), str(prefix / "lib"), str(build / "bin"),
             run_env.get("PATH", "")])
    else:
        variable = "DYLD_LIBRARY_PATH" if target_os == "macos" else "LD_LIBRARY_PATH"
        run_env[variable] = os.pathsep.join(
            [str(prefix / "curl-openssl" / "lib"), str(sdk / "lib"), str(prefix / "lib")])
    completed = jsc.run([str(program)], env=run_env, check=False, capture=True)
    print(completed.stdout, end="")
    print(completed.stderr, end="", file=sys.stderr)
    audit = (audit_program_native(program, sdk, prefix, build, target_os)
             if target_os in ("macos", "windows") else audit_program(program, sdk, prefix, env))
    ok = completed.returncode == 0 and "crtweb_webcore_graph_test: ok" in completed.stdout and audit["ok"]
    return {"passed": ok, "returncode": completed.returncode, "audit": audit,
            "output": completed.stdout.splitlines()}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--recipe", type=Path, default=ROOT / "libcrtweb/third_party/webkit/recipe.json")
    parser.add_argument("--sdk-root", type=Path, required=True, help="installed CRT SDK (05-ui or later)")
    parser.add_argument("--deps-prefix", type=Path, required=True, help="prefix holding the web-group ports")
    parser.add_argument("--work-root", type=Path, required=True)
    parser.add_argument("--gperf", type=Path, help="gperf executable (default: found on PATH)")
    parser.add_argument("--target", action="append", help="ninja target (default: PAL WebCore)")
    parser.add_argument("--configure-only", action="store_true")
    parser.add_argument("--run-tests", action="store_true",
                        help="build and run crtweb_webcore_graph_test, then audit it (the B3 gate)")
    parser.add_argument("--reuse-tree", action="store_true", help="keep the composed tree and build directory")
    parser.add_argument("--define", action="append", default=[], help="extra -D option for cmake")
    parser.add_argument("-k", "--keep-going", action="store_true", help="ninja -k 0: collect every error")
    parser.add_argument("-j", "--jobs", type=int, default=0)
    args = parser.parse_args()

    sdk, prefix, work = args.sdk_root.resolve(), args.deps_prefix.resolve(), args.work_root.resolve()
    work.mkdir(parents=True, exist_ok=True)
    target_os, arch = jsc.sdk_target(sdk)
    if target_os not in ("linux", "macos", "windows"):
        raise SystemExit(f"B3 is not replayed on {target_os} (docs/acceptance/crtweb_acceptance.md)")
    candidate = prefix / "bin" / "gperf"
    gperf = args.gperf or (candidate if candidate.exists() else
                           Path(shutil.which("gperf")) if shutil.which("gperf") else None)
    if gperf is None:
        raise SystemExit("gperf not found: WebCore generates its hash tables with it (--gperf)")
    build = work / "build"
    tree = work / "src" / "webkit-crt"
    env = port_environment(sdk, prefix, gperf.resolve(), target_os)
    if args.reuse_tree and (tree / "Source").is_dir():
        # Incremental: keep the composed (and patched) tree, and copy only the CRT-owned overlay
        # files whose content changed, so unchanged files keep their timestamps. A change to the
        # carried patches needs a fresh compose (run without --reuse-tree).
        for source in jsc.PORT_OVERLAY.rglob("*"):
            if source.is_file():
                destination = tree / source.relative_to(jsc.PORT_OVERLAY)
                if not destination.exists() or destination.read_bytes() != source.read_bytes():
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(source, destination)
    else:
        tree = jsc.compose_port_source(args.recipe, work)
        applied = jsc.apply_patches(tree, target_os, ("crt-port", f"crt-port-{target_os}"))
        (work / "carried-patches.json").write_text(json.dumps(applied, indent=2) + "\n", encoding="utf-8")
    if build.exists() and not args.reuse_tree:
        shutil.rmtree(build)
    if not (build / "build.ninja").exists() or not args.reuse_tree:
        jsc.run(configure_command(tree, build, sdk, prefix, arch, [f"-D{d}" for d in args.define], target_os),
                env=env)
    if args.configure_only:
        return 0
    if args.run_tests and not args.target:
        args.target = ["crtweb_webcore_graph_test"]
    command = ["ninja", "-C", str(build)]
    if args.jobs:
        command += [f"-j{args.jobs}"]
    if args.keep_going:
        command += ["-k", "0"]
    command += list(args.target or DEFAULT_TARGETS)
    status = jsc.run(command, env=env, check=False).returncode
    if status != 0 or not args.run_tests:
        return status
    result = run_graph_test(build, sdk, prefix, work, env, target_os)
    (work / "acceptance-b3.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print("B3 gate:", "PASSED" if result["passed"] else "FAILED")
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
