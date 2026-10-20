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
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import build_webkit_jsc as jsc  # noqa: E402  (shared compose/patch/environment helpers)
import test_crtweb_network  # noqa: E402 (build-host-only TLS fixture)

DEFAULT_TARGETS = ("PAL", "WebCore")


def port_environment(sdk: Path, prefix: Path, gperf: Path | None, target_os: str = "linux",
                     work: Path | None = None) -> dict:
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
    if target_os == "linux" and work is not None:
        # The SDK ships a shared mbedTLS libcurl.so.4 (no FILE protocol) beside this port's OpenSSL
        # libcurl with the same soname. The wrappers list the SDK's lib directory first in every
        # RPATH, and an RPATH beats LD_LIBRARY_PATH, so the Web processes would load the SDK's curl
        # and every file: load would fail with "Unsupported protocol". Name the port's own
        # directories first (tools/crt-c++, CRT_LINK_RPATH_FIRST).
        env["CRT_LINK_RPATH_FIRST"] = os.pathsep.join(
            [str(work / "build" / "lib"), str(prefix / "curl-openssl" / "lib")])
    if target_os == "windows":
        # WebKit's generators are Python and Perl programs that read their input with the process's
        # default encoding (cp949 here), not the UTF-8 the sources are in.
        env["PYTHONUTF8"] = "1"
        # WebKit enables ccache automatically when it is on PATH.  Native ccache cannot launch the
        # SDK's .cmd compiler wrappers directly, so a reconfigure on a machine that happens to have
        # ccache would otherwise turn a previously working build into a full-tree compile failure.
        env["WK_USE_CCACHE"] = "NO"
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
        platform_options = [f"-DCMAKE_PROJECT_INCLUDE={(ROOT / 'libcrtweb' / 'cmake' / 'crt_webkit_platform.cmake').as_posix()}",
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
        cxx_flags = flags + ["-include", (ROOT / "libcrtweb" / "cmake" / "crt_webkit_jit_permissions.h").as_posix()]
        platform_options = [f"-DCMAKE_C_COMPILER={(sdk / 'tools' / 'crt-cc').as_posix()}",
                            f"-DCMAKE_CXX_COMPILER={(sdk / 'tools' / 'crt-c++').as_posix()}",
                            f"-DCMAKE_PROJECT_INCLUDE={(ROOT / 'libcrtweb' / 'cmake' / 'crt_webkit_platform.cmake').as_posix()}"]
    options = [
        "-DPORT=CRT", "-DCRT_ENABLE_WEBCORE=ON", "-DCMAKE_BUILD_TYPE=Release",
        "-DUSE_HEADER_MAPS=OFF", "-DENABLE_API_TESTS=OFF", "-DDEVELOPER_MODE=OFF",
        "-DENABLE_STATIC_JSC=ON",
        # Same JIT configuration as the accepted B1 build.
        "-DENABLE_JIT=ON", "-DENABLE_C_LOOP=OFF", "-DENABLE_DFG_JIT=ON", "-DENABLE_FTL_JIT=ON",
        "-DENABLE_WEBASSEMBLY=ON", "-DENABLE_SAMPLING_PROFILER=OFF",
        # WebKit evaluates some source lists as CMake code. Native Windows
        # backslashes in an untyped -D path become escapes (notably C:\\Users).
        f"-DCRT_WEBCORE_TESTS={(ROOT / 'libcrtweb' / 'tests' / 'webcore').as_posix()}",
        f"-DCRT_WEBKIT_TESTS={(ROOT / 'libcrtweb' / 'tests' / 'webkit').as_posix()}",
    ]
    if target_os in ("linux", "macos", "windows"):
        # Tranche 4 (libcrtweb, crtweb_host, crtweb_api_test): Linux/x86_64, replayed on macOS/arm64
        # and Windows/x64.
        options.append(f"-DCRT_LIBCRTWEB_DIR={(ROOT / 'libcrtweb').as_posix()}")
    command = ["cmake", "-S", str(tree), "-B", str(build), "-G", "Ninja",
               f"-DCMAKE_TOOLCHAIN_FILE={(sdk / 'crt-toolchain.cmake').as_posix()}",
               f"-DCMAKE_PREFIX_PATH={prefix.as_posix()}", f"-DICU_ROOT={prefix.as_posix()}",
               f"-DEGL_INCLUDE_DIRS={(tree / 'Source' / 'ThirdParty' / 'ANGLE' / 'include').as_posix()}",
               f"-DOpenGLES2_INCLUDE_DIR={(tree / 'Source' / 'ThirdParty' / 'ANGLE' / 'include').as_posix()}",
               f"-DCURL_ROOT={(prefix / 'curl-openssl').as_posix()}",
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


def audit_program(program: Path, sdk: Path, prefix: Path, env: dict, build: Path | None = None) -> dict:
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
        roots = [sdk / "lib", prefix / "lib", prefix / "curl-openssl" / "lib"]
        if build is not None:
            roots.append(build / "lib")  # the port's own libraries (libWebKit)
        for root in roots:
            if (root / Path(name).name).exists():
                found = True
                break
        if not found:
            unresolved.append(name)
    return {"glib_symbols": glib, "needed": needed, "host_objects": unresolved,
            "ok": not glib and not unresolved}


# The family names WebKit asks fontconfig for: the CSS generic families, the names WebKit's default
# preferences use for them, and system-ui. The fontconfig port installs no default configuration, so
# without these aliases a name that is not an installed family matches nothing, the primary font of
# an empty form control has no metrics and the control collapses to its border (found by the B6 run:
# an <input> with font-family sans-serif was 6 px high).
GENERIC_FAMILIES = ("sans-serif", "serif", "monospace", "cursive", "fantasy", "system-ui", "ui-sans-serif",
                    "Times", "Times New Roman", "Helvetica", "Arial", "Courier", "Courier New")


def font_configuration(fonts_dir: str, cache_dir: str) -> str:
    aliases = "".join(
        f"<alias binding=\"same\"><family>{name}</family><prefer><family>DejaVu Sans Mono</family></prefer></alias>"
        for name in GENERIC_FAMILIES)
    return ('<?xml version="1.0"?>\n<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">\n'
            f"<fontconfig><dir>{fonts_dir}</dir><cachedir>{cache_dir}</cachedir>{aliases}</fontconfig>\n")


def crt_path(path: Path) -> str:
    # The CRT spells a drive path /c/Users/...: fontconfig only treats a leading '/' as absolute.
    text = path.as_posix()
    return f"/{text[0].lower()}{text[2:]}" if len(text) > 2 and text[1] == ":" else text


def gate_environment(build: Path, sdk: Path, prefix: Path, work: Path, env: dict, target_os: str) -> dict:
    """The environment of the B5-B7 gates: the port's library directories on the loader path (PATH on
    Windows) and the fontconfig file run_program wrote into the work directory."""
    run_env = dict(env)
    if target_os == "windows":
        run_env["PATH"] = os.pathsep.join(
            [str(sdk / "bin"), str(prefix / "bin"), str(prefix / "lib"), str(build / "bin"), run_env.get("PATH", "")])
        run_env["FONTCONFIG_FILE"] = crt_path(work / "fonts.conf")
    else:
        loader_path = "DYLD_LIBRARY_PATH" if target_os == "macos" else "LD_LIBRARY_PATH"
        run_env[loader_path] = os.pathsep.join(
            [str(build / "lib"), str(prefix / "curl-openssl" / "lib"), str(sdk / "lib"), str(prefix / "lib")])
        run_env["FONTCONFIG_FILE"] = str(work / "fonts.conf")
    return run_env


def run_program(name: str, build: Path, sdk: Path, prefix: Path, work: Path, env: dict,
                target_os: str = "linux") -> dict:
    """Run one gate program from <build>/bin with a fontconfig file that scans only the bundled
    fonts, the port's library directories on the loader path, then audit it. The program must exit 0
    and print "<name>: ok"."""
    program = build / "bin" / (name + (".exe" if target_os == "windows" else ""))
    fonts = ROOT / "libcrtgfx" / "assets" / "fonts"
    cache = work / "fontcache"

    if target_os == "windows":
        fonts_text, cache_text = crt_path(fonts), crt_path(cache)
    else:
        fonts_text, cache_text = str(fonts), str(cache)
    conf = work / "fonts.conf"
    conf.write_text(font_configuration(fonts_text, cache_text), encoding="utf-8")
    run_env = dict(env)
    run_env["FONTCONFIG_FILE"] = crt_path(conf) if target_os == "windows" else str(conf)
    if target_os == "windows":
        # DLLs are found through PATH: the SDK's runtime DLLs and the ports' (ICU, ...).
        run_env["PATH"] = os.pathsep.join(
            [str(sdk / "bin"), str(prefix / "bin"), str(prefix / "lib"), str(build / "bin"),
             run_env.get("PATH", "")])
    else:
        # The port's own libraries (libWebKit) first, then the curl-openssl install: both curls share
        # the soname libcurl.so.4 (porting_status.md).
        variable = "DYLD_LIBRARY_PATH" if target_os == "macos" else "LD_LIBRARY_PATH"
        run_env[variable] = os.pathsep.join(
            [str(build / "lib"), str(prefix / "curl-openssl" / "lib"), str(sdk / "lib"), str(prefix / "lib")])
    # Run in the work directory: the Network process keeps its cookie jar in the current directory.
    # A lifecycle failure must not leave the acceptance driver blocked forever.
    completed = subprocess.run([str(program)], env=run_env, cwd=work, check=False,
                               capture_output=True, text=True, timeout=900 if "lifecycle" in name else 300)
    print(completed.stdout, end="")
    print(completed.stderr, end="", file=sys.stderr)
    audit = (audit_program_native(program, sdk, prefix, build, target_os)
             if target_os in ("macos", "windows") else audit_program(program, sdk, prefix, env, build))
    ok = completed.returncode == 0 and f"{name}: ok" in completed.stdout and audit["ok"]
    return {"passed": ok, "returncode": completed.returncode, "audit": audit,
            "output": completed.stdout.splitlines()}


def ppm_pixels(path: Path):
    """(width, height, bytes) of a binary P6 file."""
    data = path.read_bytes()
    header = data.split(b"\n", 3)
    width, height = (int(v) for v in header[1].split())
    return width, height, header[3]


def compare_frames(frame: Path, reference: Path) -> dict:
    """How far the CRT frame is from the native WPE reference: the share of pixels that differ by more
    than 24 summed channel units, and the share inside the fixture's solid regions. Text is expected
    to differ (CRT's font set is the bundled fonts, WPE's the host's)."""
    w, h, a = ppm_pixels(frame)
    w2, h2, b = ppm_pixels(reference)
    result = {"size": [w, h], "reference_size": [w2, h2]}
    if (w, h) != (w2, h2):
        result["comparable"] = False
        return result
    differing = 0
    for i in range(0, len(a), 3):
        if abs(a[i] - b[i]) + abs(a[i + 1] - b[i + 1]) + abs(a[i + 2] - b[i + 2]) > 24:
            differing += 1
    result["comparable"] = True
    result["differing_pixels"] = differing
    result["differing_percent"] = round(100.0 * differing / (w * h), 2)
    # Solid regions of the fixture: background, aside, canvas fills.
    probes = {"background": (20, 300), "aside": (540, 400), "canvas_red": (100, 290), "canvas_purple": (200, 290)}
    for name, (x, y) in probes.items():
        index = (y * w + x) * 3
        result[name] = {"crt": list(a[index:index + 3]), "reference": list(b[index:index + 3])}
    return result


def run_render_gate(build: Path, sdk: Path, prefix: Path, work: Path, env: dict, target_os: str = "linux") -> dict:
    """B5: reference.html rendered by the CRT port, delivered through the web surface protocol to
    surface_probe (built here with the CRT toolchain), and diffed against the native WPE reference."""
    import subprocess
    import time
    exe = ".exe" if target_os == "windows" else ""
    probe = work / ("surface_probe" + exe)
    source = ROOT / "libcrtweb" / "tests" / "platform-crt" / "surface_probe.c"
    jsc.run([str(sdk / "tools" / ("crt-cc.cmd" if target_os == "windows" else "crt-cc")), "-O1", "-o", str(probe),
             str(source)], env=env)
    socket_path = crt_path(work / "surface.sock") if target_os == "windows" else str(work / "surface.sock")
    frame_from_consumer = work / "frame-consumer.ppm"
    frame_from_producer = work / "frame-producer.ppm"
    run_env = gate_environment(build, sdk, prefix, work, env, target_os)
    consumer = subprocess.Popen([str(probe), socket_path, "--reference", str(frame_from_consumer)],
                                stdout=subprocess.PIPE, text=True, env=run_env)
    try:
        line = consumer.stdout.readline()
        if "SURFACE_PROBE_LISTENING" not in line:
            return {"passed": False, "reason": f"consumer did not listen: {line!r}"}
        producer = subprocess.run(
            [str(build / "bin" / ("crtweb_render_test" + exe)),
             str(ROOT / "libcrtweb" / "tests" / "wpe-reference" / "reference.html"),
             str(frame_from_producer), socket_path],
            env=run_env, cwd=work, capture_output=True, text=True, timeout=180)
        print(producer.stdout, end="")
        print(producer.stderr, end="", file=sys.stderr)
        consumer_output, _ = consumer.communicate(timeout=60)
        print(consumer_output, end="")
    finally:
        if consumer.poll() is None:
            consumer.kill()
    ok = (producer.returncode == 0 and "crtweb_render_test: ok" in producer.stdout
          and consumer.returncode == 0 and "surface_probe: ok" in consumer_output)
    result = {"passed": ok, "producer_returncode": producer.returncode, "consumer_returncode": consumer.returncode,
              "producer_output": producer.stdout.splitlines(), "consumer_output": consumer_output.splitlines()}
    reference = ROOT / "out" / "web-reference" / "result" / "wpe-reference.ppm"
    if frame_from_consumer.exists() and reference.exists():
        result["diff_against_wpe_reference"] = compare_frames(frame_from_consumer, reference)
        result["frame_identical_to_producer_copy"] = frame_from_consumer.read_bytes() == frame_from_producer.read_bytes()
    else:
        result["diff_against_wpe_reference"] = "reference or consumer frame missing"
    return result


def run_input_gate(build: Path, sdk: Path, prefix: Path, work: Path, env: dict, target_os: str = "linux") -> dict:
    """B6: the 3A acceptance consumer (surface_probe, its full scripted run: pointer click -> page report
    and pixel change, key code and modifiers, wheel, committed text, resize, stalled consumer) against
    crtweb_surface_host, the CRT port's frame producer, instead of the native WPE module."""
    import subprocess
    exe = ".exe" if target_os == "windows" else ""
    probe = work / ("surface_probe" + exe)
    if not probe.exists():
        jsc.run([str(sdk / "tools" / ("crt-cc.cmd" if target_os == "windows" else "crt-cc")), "-O1", "-o",
                 str(probe), str(ROOT / "libcrtweb" / "tests" / "platform-crt" / "surface_probe.c")], env=env)
    socket_path = crt_path(work / "input.sock") if target_os == "windows" else str(work / "input.sock")
    run_env = gate_environment(build, sdk, prefix, work, env, target_os)
    consumer = subprocess.Popen([str(probe), socket_path], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, env=run_env)
    try:
        line = consumer.stdout.readline()
        if "SURFACE_PROBE_LISTENING" not in line:
            return {"passed": False, "reason": f"consumer did not listen: {line!r}"}
        producer = subprocess.run(
            [str(build / "bin" / ("crtweb_surface_host" + exe)),
             str(ROOT / "libcrtweb" / "tests" / "platform-crt" / "crt_surface.html"), socket_path],
            env=run_env, cwd=work, capture_output=True, text=True, timeout=240)
        consumer_output, _ = consumer.communicate(timeout=60)
        print(consumer_output, end="")
        if producer.stderr:
            print(producer.stderr, end="", file=sys.stderr)
    finally:
        if consumer.poll() is None:
            consumer.kill()
    ok = producer.returncode == 0 and consumer.returncode == 0 and "surface_probe: ok" in consumer_output
    return {"passed": ok, "producer_returncode": producer.returncode, "consumer_returncode": consumer.returncode,
            "consumer_output": consumer_output.splitlines()}


def macos_mapped_files(pid: int, log: Path | None = None, anonymous: list | None = None) -> list:
    completed = subprocess.run(["vmmap", str(pid)], capture_output=True, text=True,
                               check=False, timeout=15)
    if log is not None:
        log.write_text(completed.stdout + completed.stderr, encoding="utf-8")
    if completed.returncode:
        return []
    # Parse real region rows, not the masked Path: or Analysis Tool: metadata.
    # Observe ALL path roots, including foreign /opt, /Library and /Volumes images.
    pattern = re.compile(r"^.+?\s+[0-9a-fA-F]+-[0-9a-fA-F]+\s+\[[^]]+\]\s+"
                         r"([rwx-]{3})/[rwx-]{3}\s+.*?\s(/.*)$")
    files = set()
    temporary = Path(tempfile.gettempdir()).resolve()
    for line in completed.stdout.splitlines():
        match = pattern.search(line)
        if not match:
            continue
        permissions, path = match.group(1), match.group(2).rstrip()
        resolved = Path(path).resolve()
        # libc's own memfd_create (mman.c): an immediately unlinked, PID-scoped
        # temporary file used as non-executable shared data. Never exempt an
        # executable, named, foreign-PID or arbitrary temporary-file mapping.
        if (anonymous is not None and "x" not in permissions and resolved.parent == temporary
                and re.fullmatch(rf"crt_memfd_{pid}_[0-9]{{6}}\.tmp", resolved.name)
                and not resolved.exists()):
            anonymous.append(path)
        else:
            files.add(path)
    return sorted(files)


def observe_macos_processes(parent_pid: int, children: dict | None = None) -> dict:
    """The mapped files of the three live B7 processes (UI, Web, Network), from the host side. macOS has
    no /proc, and an observer started inside the process under test (popen of ps/vmmap) is a child
    that changes the lifecycle being measured, so the driver samples while the test runs."""
    children = dict(children or {})
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline and len(children) < 2:
        snapshot = subprocess.run(["ps", "-axo", "pid=,ppid=,comm="], capture_output=True, text=True,
                                  check=False).stdout
        for line in snapshot.splitlines():
            fields = line.strip().split(None, 2)
            if len(fields) != 3 or not fields[0].isdigit() or not fields[1].isdigit():
                continue
            if int(fields[1]) == parent_pid and Path(fields[2]).name in ("WebProcess", "NetworkProcess"):
                children[Path(fields[2]).name] = int(fields[0])
        if len(children) < 2:
            time.sleep(0.02)
    mapped = {}
    for role, pid in {"UIProcess": parent_pid, **children}.items():
        mapped[role] = macos_mapped_files(pid)
    return mapped


def macos_map_checkpoint(text: str, parent_pid: int) -> dict | None:
    records = {}
    for line in text.splitlines():
        fields = line.split()
        if (len(fields) != 2 or fields[0] in records or not fields[1].isascii() or not fields[1].isdigit()
                or int(fields[1]) <= 0):
            return None
        records[fields[0]] = int(fields[1])
    if set(records) != {"UIProcess", "WebProcess", "NetworkProcess"} or records["UIProcess"] != parent_pid:
        return None
    if len(set(records.values())) != 3:
        return None
    return {role: records[role] for role in ("WebProcess", "NetworkProcess")}


def run_lifecycle_program_macos(build: Path, sdk: Path, prefix: Path, work: Path, env: dict) -> tuple:
    """crtweb_lifecycle_test as run_program runs it, with the host-side sampling of the process maps."""
    program = build / "bin" / "crtweb_lifecycle_test"
    with tempfile.TemporaryDirectory(prefix="map-audit-", dir=work) as directory:
        run_env = gate_environment(build, sdk, prefix, work, env, "macos")
        run_env["CRTWEB_MAP_AUDIT_DIR"] = directory
        process = subprocess.Popen([str(program)], cwd=work, env=run_env,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        mapped = {}
        try:
            ready = Path(directory) / "ready"
            deadline = time.monotonic() + 25
            while time.monotonic() < deadline and process.poll() is None:
                if ready.exists():
                    children = macos_map_checkpoint(ready.read_text(), process.pid)
                    if children is not None:
                        mapped = observe_macos_processes(process.pid, children)
                        break
                time.sleep(0.02)
        finally:
            (Path(directory) / "release").touch()
            stdout, stderr = process.communicate(timeout=660)
    print(stdout, end="")
    print(stderr, end="", file=sys.stderr)
    audit = audit_program_native(program, sdk, prefix, build, "macos")
    passed = process.returncode == 0 and "crtweb_lifecycle_test: ok" in stdout and audit["ok"]
    return ({"passed": passed, "returncode": process.returncode, "audit": audit,
             "output": stdout.splitlines()}, mapped)


def run_lifecycle_gate(build: Path, sdk: Path, prefix: Path, work: Path, env: dict, target_os: str = "linux") -> dict:
    """B7: crtweb_lifecycle_test (100 create/load/paint/destroy cycles of a view with its Web and
    Network processes; the test itself bounds descriptor, thread and RSS growth), then the Host ABI
    audit twice over: the static one (ldd closure of every binary the port produced) and the runtime
    one, from the MAPPED lines the test prints from /proc/<pid>/maps of each child: every file-backed
    mapping of the UI, Web and Network processes must be the CRT SDK, the port prefix, this build
    tree, the bundled fonts, the fontconfig cache, or the host's dynamic loader."""
    if target_os == "macos":
        result, observed = run_lifecycle_program_macos(build, sdk, prefix, work, env)
        mapped_lines = [f"MAPPED {role} {path}" for role, paths in observed.items() for path in paths]
    else:
        result = run_program("crtweb_lifecycle_test", build, sdk, prefix, work, env, target_os)
        mapped_lines = result["output"]
    windows = target_os == "windows"
    allowed = [sdk.resolve(), prefix.resolve(), build.resolve(), (ROOT / "libcrtgfx" / "assets" / "fonts").resolve(),
               work.resolve()]
    # Windows: every process also maps the OS's own DLLs (System32): the ones its runtime contract names, and
    # what those pull in (advapi32 and RPCRT4 import msvcrt.dll and ucrtbase.dll, so every Windows process
    # maps them whatever it is). They are listed, not judged: a CRT or port binary that imports the host C
    # runtime itself is caught by the static audit of the import closure, which is the strict half.
    system_root = Path(os.environ.get("SystemRoot", r"C:\Windows")).resolve() if windows else None
    os_runtime = set()
    mapped, violations = {}, []
    for line in mapped_lines:
        if not line.startswith("MAPPED "):
            continue
        _, role, path = line.split(" ", 2)
        mapped.setdefault(role, []).append(path)
        if windows:
            resolved = Path(path).resolve()
            if any(resolved.is_relative_to(root) for root in allowed):
                continue
            if resolved.is_relative_to(system_root) and resolved.suffix.lower() == ".dll":
                os_runtime.add(resolved.name.lower())
                continue
            violations.append(f"{role}: maps {path}")
            continue
        if path.startswith(("[", "/memfd:")) or Path(path).name.startswith("ld-linux"):
            continue
        # vmmap lists the dyld shared-cache closure of every process. Direct Mach-O dependencies are
        # held to libSystem by the static audit below; what is mapped outside the OS-owned cache must be
        # the CRT SDK, the port prefix or this build.
        if target_os == "macos" and (path.startswith(("/usr/lib/", "/System/", "/usr/share/", "/Library/Apple/",
                                                      "/private/var/db/", "/usr/libexec/"))
                                     or path == "/usr/bin/vmmap" or "/*/" in path):
            continue
        if not any(Path(path).resolve().is_relative_to(root) for root in allowed):
            violations.append(f"{role}: maps {path}")
    if windows:
        binaries = sorted(p for p in (build / "bin").iterdir() if p.suffix.lower() in (".exe", ".dll"))
        binaries += sorted((build / "lib").glob("libWebKit*.dll"))
        probe = work / "surface_probe.exe"
        if probe.exists():
            binaries.append(probe)
        static_binaries = []
        static = jsc.host_abi_audit(sdk, prefix, build, binaries, {}, "windows")
    else:
        binaries = sorted(p for p in (build / "bin").iterdir() if p.is_file() and os.access(p, os.X_OK))
        binaries += sorted((build / "lib").glob("libWebKit*.dylib*" if target_os == "macos" else "libWebKit.so*"))
        probe = work / "surface_probe"
        if probe.exists():
            binaries.append(probe)
        static_binaries = []
        if target_os == "linux":
            # A CRT-linked static executable (surface_probe) has no DT_NEEDED: nothing for ldd to resolve.
            readelf = shutil.which("llvm-readelf") or shutil.which("readelf")
            static_binaries = [b for b in binaries
                               if "(NEEDED)" not in jsc.run([readelf, "-d", str(b)], capture=True, check=False).stdout]
            binaries = [b for b in binaries if b not in static_binaries]
        loader_path = "DYLD_LIBRARY_PATH" if target_os == "macos" else "LD_LIBRARY_PATH"
        static = jsc.host_abi_audit(sdk, prefix, build, binaries, {
            loader_path: os.pathsep.join([str(build / "lib"), str(prefix / "curl-openssl" / "lib"),
                                          str(sdk / "lib"), str(prefix / "lib")])}, target_os)
    result["runtime_audit"] = {"roles": sorted(mapped), "mapped_files": sum(len(v) for v in mapped.values()),
                               "violations": violations}
    if windows:
        result["runtime_audit"]["os_runtime_dlls"] = sorted(os_runtime)
    result["static_audit"] = {"binaries": [b.name for b in binaries],
                              "statically_linked": [b.name for b in static_binaries], "ok": static["ok"],
                              "violations": static["violations"], "known_host_libraries": static["known_host_libraries"]}
    result["passed"] = (result["passed"] and not violations and static["ok"]
                        and {"WebProcess", "NetworkProcess"} <= set(mapped))
    return result


def run_webview_gate(build: Path, sdk: Path, prefix: Path, work: Path, env: dict, target_os: str = "linux") -> dict:
    """Tranche 4: the crtui WebView. The in-tree crtweb_webview_test (a crtui window composing the page
    through the final compositor, built with the rest of the SDK's C++ tests) is run against
    crtweb_host, the out-of-process half built here from the public crtweb API alone. Also checks the
    public boundary: libcrtweb.so exports crtweb_* only, and neither public header names a WebKit, WPE,
    GLib or JavaScriptCore type."""
    import re
    import subprocess
    result = {"passed": False}
    include = ROOT / "libcrtweb" / "include" / "crtweb"
    forbidden = re.compile(r"\b(WK[A-Z]\w*|WPE\w*|WebKit\w*|JS[A-Z]\w*|G[A-Z][a-z]+\w*Ref|g_\w+|GObject|GLib)\b")
    leaks = sorted({f"{header.name}: {match}" for header in include.glob("*.h")
                    for match in forbidden.findall(
                        re.sub(r"/\*.*?\*/|//[^\n]*", "", header.read_text(encoding="utf-8"), flags=re.S))})
    nm = shutil.which("llvm-nm") or shutil.which("nm") or "nm"
    macos = target_os == "macos"
    windows = target_os == "windows"
    exe = ".exe" if windows else ""
    if windows:
        candidates = sorted((build / "bin").glob("*crtweb.dll")) + sorted((build / "lib").glob("*crtweb.dll"))
        library = candidates[0] if candidates else build / "bin" / "libcrtweb.dll"
    else:
        library = build / "lib" / ("libcrtweb.dylib" if macos else "libcrtweb.so")
    if windows:
        # PE: the export directory, by name.
        readobj = shutil.which("llvm-readobj") or "llvm-readobj"
        listing = jsc.run([readobj, "--coff-exports", str(library)], capture=True).stdout.splitlines()
        exported = [line.split("Name:", 1)[1].strip() for line in listing if line.strip().startswith("Name:")]
    elif macos:
        # Mach-O: every external defined symbol, with the leading underscore of the C symbol removed.
        listing = jsc.run([nm, "-gU", str(library)], capture=True).stdout.splitlines()
        exported = [line.split()[-1].lstrip("_") for line in listing if line.split()]
    else:
        exported = [line.split()[-1] for line in jsc.run([nm, "-D", "--defined-only", str(library)], capture=True).stdout.splitlines()
                    if line.split() and line.split()[-2:-1] and line.split()[-2] in ("T", "W", "D", "B", "R", "V", "i")]
    foreign = sorted(name for name in exported if not name.startswith("crtweb_"))
    result["header_leaks"] = leaks
    result["foreign_exports"] = foreign
    build_root = sdk.parent.parent  # <preset>/dist/05-ui -> <preset>
    test = build_root / "libcrtweb" / ("crtweb_webview_test" + exe)
    if (build_root / "build.ninja").exists():
        # This executable belongs to the in-tree 05-ui build rather than the composed WebKit tree.
        # Always ask Ninja to refresh it: merely checking that an old executable exists silently
        # skips newly-added WebView acceptance code after a pull.
        built = jsc.run(["ninja", "-C", str(build_root), "crtweb_webview_test"], env=env, check=False)
        if built.returncode:
            result["reason"] = f"failed to rebuild {test}"
            return result
    if not test.exists():
        result["reason"] = f"{test} was not built (ninja -C {build_root} crtweb_webview_test)"
        return result
    run_env = gate_environment(build, sdk, prefix, work, env, target_os)
    run_env["CRTWEB_HOST"] = str(build / "bin" / ("crtweb_host" + exe))
    runs = work / "webview-run"
    runs.mkdir(exist_ok=True)
    test_env = dict(run_env)
    if macos:
        # The test links the host's AppKit/Metal frameworks, which depend on /usr/lib/libc++.1.dylib:
        # DYLD_LIBRARY_PATH matches by leaf name, so the SDK's libc++ would replace the system one in
        # the frameworks (two C++ runtimes and two heaps in one process). crtweb_host finds the SDK and
        # the build tree through its own rpaths and needs no loader path.
        test_env.pop("DYLD_LIBRARY_PATH", None)
    completed = subprocess.run([str(test)], env=test_env, cwd=runs, capture_output=True, text=True, timeout=300)
    print(completed.stdout, end="")
    print(completed.stderr, end="", file=sys.stderr)
    host_audit = jsc.host_abi_audit(sdk, prefix, build, [build / "bin" / ("crtweb_host" + exe), library], run_env, target_os)
    result.update({"returncode": completed.returncode, "output": completed.stdout.splitlines(),
                   "host_abi": {"ok": host_audit["ok"], "violations": host_audit["violations"]}})
    result["passed"] = (completed.returncode == 0 and "crtweb_webview_test: ok" in completed.stdout
                        and not leaks and not foreign and host_audit["ok"])
    return result


def gpu_composition_evidence(text: str, target_os: str, window_frames: int = 0) -> dict:
    """Fail closed: a skipped browser/GPU probe is not Tranche 9A acceptance."""
    lines = text.splitlines()
    summaries = [line.split(": ok ", 1)[1] for line in lines
                 if line.startswith("crtweb_webview_gpu_test: ok ")]
    required = dict(source="cpu-shm", interop="cpu-upload", gpu_target="pass", pixels="pass",
                    clip="pass", opacity="pass", ordering="pass", input="pass", resize="pass",
                    updates="20", recovery="pass", cleanup="pass")
    fields = dict(item.split("=", 1) for item in summaries[0].split() if "=" in item) if len(summaries) == 1 else {}
    backend = {"windows": "1", "macos": "2", "linux": "3"}.get(target_os)
    presented = re.findall(r"^crtweb_webview_gpu_test: presented=(\d+) requested=(\d+)$", text, re.M)
    window_ok = (presented == [(str(window_frames), str(window_frames))]) if window_frames else not presented
    return {"verified": bool(backend and fields.get("backend") == backend
                              and all(fields.get(k) == v for k, v in required.items())
                              and window_ok and not re.search(r"FAILED|skipped", text)),
            "backend": fields.get("backend"), "source": fields.get("source"),
            "interop": fields.get("interop"), "window_frames": window_frames,
            "presented": int(presented[0][0]) if len(presented) == 1 else 0}


def gpu_browser_map_evidence(text: str, sdk: Path, prefix: Path, build: Path, work: Path,
                             target_os: str) -> dict:
    # Record browser roles separately from the application which owns the
    # native GPU driver. Other hosts need their native observer on replay;
    # do not silently present absent observations as a clean runtime audit.
    if target_os not in ("linux", "windows", "macos"):
        return {"verified": None, "scope": "native runtime observation pending", "violations": []}
    roots = [path.resolve() for path in (sdk, prefix, build, work, ROOT / "libcrtgfx/assets/fonts")]
    mapped = re.findall(r"^MAPPED (HostProcess|WebProcess|NetworkProcess) (.+)$", text, re.M)
    violations = []
    windows = target_os == "windows"
    macos = target_os == "macos"
    suffix = ".dll" if windows else ".dylib" if macos else ".so"
    stub_names = tuple(name + suffix for name in ("libEGL", "libGLESv2"))
    stub_files = {(prefix / "lib" / name).resolve() for name in stub_names}
    system_root = Path(os.environ.get("SystemRoot", r"C:\Windows")).resolve() if windows else None
    os_runtime = set()
    os_mappings = set()
    stubs = set()
    for role, path in mapped:
        resolved = Path(path).resolve()
        if windows:
            name = resolved.name.lower()
            if resolved in stub_files:
                stubs.add(resolved.name)
            elif name.startswith(("d3d12", "d3dcompiler", "vulkan-", "libvulkan", "opengl32",
                                  "libegl", "libgles", "libglx", "libgl.")):
                violations.append(f"{role}: GPU loader entered CPU browser path: {path}")
            if any(resolved.is_relative_to(root) for root in roots):
                continue
            # As in B7, Windows' transitive OS DLL closure is recorded, not a
            # host-libc waiver. Direct imports are held to the static PE audit.
            # D3D11/DXGI are already part of the desktop/media PAL contract;
            # their presence does not prove that the browser rendered on a GPU.
            if resolved.is_relative_to(system_root) and resolved.suffix.lower() == ".dll":
                os_runtime.add(name)
                continue
            violations.append(f"{role}: maps {path}")
            continue
        if resolved.name.startswith("ld-linux"):
            continue
        if macos:
            # vmmap sees Apple's transitive framework closure, including media
            # GPU helpers. As in B7, record that separately; the static Mach-O
            # audit still restricts direct imports to the owned PAL contract.
            if path.startswith(("/usr/lib/", "/System/", "/usr/share/", "/Library/Apple/",
                                "/private/var/db/", "/usr/libexec/")):
                os_mappings.add(path)
                continue
        if not any(resolved.is_relative_to(root) for root in roots):
            violations.append(f"{role}: maps {path}")
        if resolved in stub_files:
            stubs.add(resolved.name)  # existing CRT-owned no-context B3 link stubs, not native drivers
        elif (resolved.name.startswith(("libvulkan", "libEGL", "libGLES", "libGLX", "libGL.", "libMoltenVK"))
              or (macos and resolved.name in ("Metal", "MetalKit", "MetalPerformanceShaders", "MPSGraph", "OpenGL"))):
            violations.append(f"{role}: GPU loader entered CPU browser path: {path}")
    roles = sorted({role for role, _ in mapped})
    result = {"verified": roles == ["HostProcess", "NetworkProcess", "WebProcess"] and not violations,
              "roles": roles, "mapped_files": len(mapped), "violations": violations, "crt_gl_stubs": sorted(stubs)}
    if windows:
        result["os_runtime_dlls"] = sorted(os_runtime)
    if macos:
        result["os_mappings"] = sorted(os_mappings)
        result["anonymous_shared_data"] = re.findall(
            r"^ANON_SHM (HostProcess|WebProcess|NetworkProcess) (.+)$", text, re.M)
    return result


def macos_gpu_map_checkpoint(text: str, consumer_pid: int) -> list | None:
    records = []
    for line in text.splitlines():
        fields = line.split()
        if (len(fields) != 2 or fields[0] not in ("HostProcess", "WebProcess", "NetworkProcess")
                or not fields[1].isascii() or not fields[1].isdigit() or int(fields[1]) <= 0):
            return None
        records.append((fields[0], int(fields[1])))
    roles = [role for role, _ in records]
    pids = [pid for _, pid in records]
    if (roles.count("HostProcess") != 1 or roles.count("NetworkProcess") != 1 or "WebProcess" not in roles
            or len(set(pids)) != len(pids) or consumer_pid in pids):
        return None
    return records


def macos_gpu_resource_map_checkpoint(text: str, consumer_pid: int) -> tuple | None:
    lines = text.splitlines()
    worker = lines[0].split() if lines else []
    if (len(worker) != 2 or worker[0] != "GPUWorker" or not worker[1].isascii()
            or not worker[1].isdigit() or int(worker[1]) <= 0 or int(worker[1]) == consumer_pid):
        return None
    worker_pid = int(worker[1])
    records = macos_gpu_map_checkpoint("\n".join(lines[1:]), consumer_pid)
    if not records or worker_pid in [pid for _, pid in records]:
        return None
    return worker_pid, records


def run_gpu_program_macos(command: list, runs: Path, env: dict, resource_worker: bool = False,
                          checkpoint_timeout: int = 60) -> tuple:
    """Hold the browser at the post-update checkpoint while the host samples its real mappings."""
    mapped = []
    observation = {"verified": False, "reason": "missing or invalid browser map checkpoint"}
    with tempfile.TemporaryDirectory(prefix="gpu-map-audit-", dir=runs) as directory:
        run_env = dict(env, CRTWEB_GPU_MAP_AUDIT_DIR=directory)
        process = subprocess.Popen(command, cwd=runs, env=run_env,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            ready = Path(directory) / "ready"
            deadline = time.monotonic() + checkpoint_timeout
            while process.poll() is None and time.monotonic() < deadline:
                if ready.exists():
                    text = ready.read_text(encoding="utf-8")
                    (runs / "map-checkpoint.txt").write_text(text, encoding="utf-8")
                    worker_pid = None
                    if resource_worker:
                        checkpoint = macos_gpu_resource_map_checkpoint(text, process.pid)
                        worker_pid, records = checkpoint if checkpoint else (None, None)
                    else:
                        records = macos_gpu_map_checkpoint(text, process.pid)
                    if records:
                        snapshot = subprocess.run(["ps", "-axo", "pid=,ppid=,comm="],
                                                  capture_output=True, text=True, check=False, timeout=10)
                        processes = {}
                        for line in snapshot.stdout.splitlines():
                            fields = line.strip().split(None, 2)
                            if len(fields) == 3 and fields[0].isdigit() and fields[1].isdigit():
                                processes[int(fields[0])] = (int(fields[1]), Path(fields[2]).name)
                        host_pid = next(pid for role, pid in records if role == "HostProcess")
                        topology_ok = (not resource_worker or processes.get(worker_pid) == (process.pid, "crtweb_gpu_worker"))
                        topology_ok &= all(processes.get(pid) == (
                            (worker_pid if resource_worker else process.pid) if role == "HostProcess" else host_pid,
                            "crtweb_host" if role == "HostProcess" else role) for role, pid in records)
                        if topology_ok:
                            counts = []
                            for role, pid in records:
                                anonymous = []
                                paths = macos_mapped_files(pid, runs / f"vmmap-{role}-{pid}.log", anonymous)
                                counts.append(len(paths))
                                mapped += [f"MAPPED {role} {path}" for path in paths]
                                mapped += [f"ANON_SHM {role} {path}" for path in sorted(set(anonymous))]
                            observation = {"verified": all(counts),
                                           "processes": [{"role": role, "pid": pid,
                                                          "parent_pid": processes[pid][0],
                                                          "name": processes[pid][1], "mapped_files": count}
                                                         for (role, pid), count in zip(records, counts)]}
                        else:
                            observation["reason"] = "browser checkpoint does not match native parent/name observations"
                    break
                time.sleep(0.02)
        finally:
            (Path(directory) / "release").touch()
            try:
                stdout, stderr = process.communicate(timeout=300 if "--lifecycle" in command else 240)
            except subprocess.TimeoutExpired:
                process.kill()
                process.communicate()
                raise
    if mapped:
        stdout += "\n" + "\n".join(mapped) + "\n"
    return subprocess.CompletedProcess(command, process.returncode, stdout, stderr), observation


def run_gpu_gate(build: Path, sdk: Path, prefix: Path, work: Path, env: dict,
                 target_os: str = "linux", window_frames: int = 0) -> dict:
    """Tranche 9A only: CPU-shared-memory WebView -> Ganesh GPU composition/present.

    This gate does not enable accelerated WebCore, a GPUProcess or GPU-buffer IPC.
    The application's GPU PAL may load native drivers; the separate browser host
    remains subject to the existing static Host ABI audit.
    """
    result = {"passed": False}
    exe = ".exe" if target_os == "windows" else ""
    build_root = sdk.parent.parent
    program = build_root / "libcrtweb" / ("crtweb_webview_gpu_test" + exe)
    if (build_root / "build.ninja").exists():
        built = jsc.run(["ninja", "-C", str(build_root), "crtweb_webview_gpu_test"], env=env, check=False)
        if built.returncode:
            return result | {"reason": "could not rebuild GPU WebView consumer"}
    if not program.exists():
        return result | {"reason": f"missing {program}; enable Skia/GPU/LVGL in the predecessor build"}
    runs = work / "gpu-webview-run"
    runs.mkdir(exist_ok=True)
    fonts, cache = ROOT / "libcrtgfx/assets/fonts", runs / "fontcache"
    path_for_target = crt_path if target_os == "windows" else str
    (runs / "fonts.conf").write_text(font_configuration(path_for_target(fonts), path_for_target(cache)), encoding="utf-8")
    run_env = gate_environment(build, sdk, prefix, runs, env, target_os)
    host = build / "bin" / ("crtweb_host" + exe)
    run_env["CRTWEB_HOST"] = str(host)
    if target_os == "macos":
        run_env.pop("DYLD_LIBRARY_PATH", None)
    command = [str(program), "--require-gpu"]
    if window_frames:
        command += ["--window-frames", str(window_frames)]
    observation = None
    if target_os == "macos":
        completed, observation = run_gpu_program_macos(command, runs, run_env)
    else:
        completed = subprocess.run(command, cwd=runs, env=run_env, capture_output=True, text=True, timeout=240)
    print(completed.stdout, end="")
    print(completed.stderr, end="", file=sys.stderr)
    (runs / "stdout.log").write_text(completed.stdout, encoding="utf-8")
    (runs / "stderr.log").write_text(completed.stderr, encoding="utf-8")
    library = build / "lib" / ("libcrtweb.dylib" if target_os == "macos" else "libcrtweb.so")
    if target_os == "windows":
        candidates = sorted((build / "bin").glob("*crtweb.dll")) + sorted((build / "lib").glob("*crtweb.dll"))
        library = candidates[0] if candidates else build / "bin/libcrtweb.dll"
    audit = jsc.host_abi_audit(sdk, prefix, build, [host, library], run_env, target_os)
    evidence = gpu_composition_evidence(completed.stdout + completed.stderr, target_os, window_frames)
    runtime = gpu_browser_map_evidence(completed.stdout, sdk, prefix, build, runs, target_os)
    if observation is not None:
        runtime["observation"] = observation
        runtime["verified"] &= observation["verified"]
    result.update(returncode=completed.returncode, evidence=evidence, output=completed.stdout.splitlines(),
                  host_abi={"ok": audit["ok"], "violations": audit["violations"]}, browser_runtime=runtime)
    result["passed"] = (completed.returncode == 0 and evidence["verified"] and audit["ok"]
                        and runtime["verified"] is True)
    return result


def gpu_resource_evidence(text: str, target_os: str, window_frames: int = 0) -> dict:
    """9B resource evidence is independent of 9A and explicit CPU fallback."""
    summaries = re.findall(r"^crtweb_gpu_resource_test: ok (.+)$", text, re.M)
    fields = dict(re.findall(r"(\w+)=([^ ]+)", summaries[0])) if len(summaries) == 1 else {}
    # backend= is the crtgfx backend enum (Vulkan 3, D3D12 1); sync= names the native handles: Vulkan's
    # semaphore/fence pairs, shared D3D12 fences, or GPU-signaled Metal shared events.
    backends = {"linux": ("3", "native-fence-semaphore"), "windows": ("1", "native-shared-fence"),
                "macos": ("2", "native-shared-event")}
    backend, sync = backends.get(target_os, ("", ""))
    required = dict(backend=backend, source="browser-cpu", worker_upload="1", consumer_upload="0",
                    transport="shared-gpu", sync=sync, updates="20",
                    resize="pass", ownership="pass", cleanup="pass")
    pools = re.findall(r"^crtweb_gpu_resource_test: imports=(\d+) releases=(\d+) live_buffers=(\d+) generation=(\d+)$", text, re.M)
    pool_ok = len(pools) == 1 and int(pools[0][0]) == 6 and int(pools[0][1]) >= 20 \
        and 0 < int(pools[0][2]) <= 4 and int(pools[0][3]) == 2
    workers = re.findall(r"^crtweb_gpu_worker: uploads=(\d+) reused=(\d+) retired=(\d+) peak_buffers=(\d+) gpu_readbacks=(\d+)$", text, re.M)
    worker_ok = len(workers) == 1 and int(workers[0][0]) >= 22 and int(workers[0][1]) >= 20 \
        and int(workers[0][2]) == 3 and 0 < int(workers[0][3]) <= 4 and workers[0][4] == "0"
    presented = re.findall(r"^crtweb_gpu_resource_test: presented=(\d+) requested=(\d+)$", text, re.M)
    window_ok = presented == [(str(window_frames), str(window_frames))] if window_frames else not presented
    verified = (target_os in backends and len(summaries) == 1
                and all(fields.get(k) == v for k, v in required.items()) and pool_ok and worker_ok
                and window_ok and not re.search(r"FAILED|skipped|fallback", text))
    return {"verified": bool(verified), "fields": fields, "pool": pools, "worker": workers,
            "window_frames": window_frames, "shared_segment": "GPU worker -> application only"}


def gpu_lifecycle_evidence(text: str, target_os: str) -> dict:
    backends = {"linux": "3", "windows": "1", "macos": "2"}
    summaries = re.findall(r"^crtweb_gpu_lifecycle_test: ok (.+)$", text, re.M)
    fields = dict(re.findall(r"(\w+)=([^ ]+)", summaries[0])) if len(summaries) == 1 else {}
    required = dict(backend=backends.get(target_os, ""), cycles="20",
                    device_loss="controlled-context-and-owner", worker_loss="pass", browser_loss="pass",
                    disconnect="pass", held_resize="pass", recovery="pass", cleanup="pass")
    expected = {"normal": 3, "consumer-disconnect": 3, "worker-kill": 3, "host-kill": 3,
                "web-kill": 3, "context-loss": 3, "held-resize": 2}
    coverage = re.findall(r"^crtweb_gpu_lifecycle_test: coverage=([\w-]+) count=(\d+)$", text, re.M)
    coverage_ok = len(coverage) == 7 and {k: int(v) for k, v in coverage} == expected
    cases = re.findall(r"^crtweb_gpu_lifecycle_test: case=([\w-]+) result=(\w+) elapsed_ms=(\d+)$", text, re.M)
    actual = {name: sum(c[0] == name for c in cases) for name in expected}
    # Warm every exit policy once, twenty measured lifetimes, then prove fresh
    # pixels once more after the last controlled device loss.
    totals = {name: count + 1 + (name == "normal") for name, count in expected.items()}
    cases_ok = len(cases) == 28 and actual == totals and all(c[1] == "pass" and int(c[2]) <= 30000 for c in cases)
    stats = re.findall(r"^crtweb_gpu_lifecycle_test: cycles=20 rss=([+-]?\d+) KiB threads=([+-]?\d+) descriptors=([+-]?\d+)$", text, re.M)
    bounded = len(stats) == 1 and all(int(v) <= limit for v, limit in zip(stats[0], (24 * 1024, 2, 2)))
    transfers = text.count("crtweb_gpu_lifecycle_test: queued_transfers=3 imported=0 cleanup=pass\n") == 1
    # A deliberately killed Host makes its worker exit nonzero. That expected
    # worker diagnostic is not accepted as a failed lifecycle assertion.
    failed = re.search(r"crtweb_gpu_(?:resource|lifecycle)_test: FAILED|result=fail|skipped|fallback", text)
    verified = (target_os in backends and len(summaries) == 1 and all(fields.get(k) == v for k, v in required.items())
                and coverage_ok and cases_ok and bounded and transfers and not failed)
    return {"verified": bool(verified), "fields": fields, "coverage": coverage, "cases": cases,
            "resources": stats, "device_loss": "controlled context abandonment + owner teardown/recreation, not physical reset"}


def run_gpu_resource_gate(build: Path, sdk: Path, prefix: Path, work: Path, env: dict,
                          target_os: str = "linux", window_frames: int = 0, lifecycle: bool = False) -> dict:
    if target_os not in ("linux", "windows", "macos"):
        return {"passed": False, "reason": "9B native resource backend/replay pending on this host"}
    exe = ".exe" if target_os == "windows" else ""
    build_root = sdk.parent.parent
    # Current CMake orders both Skia objects and consumers after the SDK
    # publisher. One build preserves that ordering without republishing the
    # entire predecessor SDK separately for each archive.
    built = subprocess.run(["cmake", "--build", str(build_root), "--target", "crtweb_gpu_resource_test"],
                           env=dict(os.environ), check=False)
    if built.returncode:
        return {"passed": False, "reason": "could not build GPU worker/resource consumer"}
    runs = work / ("gpu-lifecycle-run" if lifecycle else "gpu-resource-run")
    runs.mkdir(exist_ok=True)
    (runs / "fonts.conf").write_text(font_configuration(str(ROOT / "libcrtgfx/assets/fonts"),
                                                       str(runs / "fontcache")), encoding="utf-8")
    if target_os == "windows":
        # font_configuration() above was given the POSIX spelling; the CRT wants /c/dir for fontconfig.
        (runs / "fonts.conf").write_text(font_configuration(
            crt_path(ROOT / "libcrtgfx/assets/fonts"), crt_path(runs / "fontcache")), encoding="utf-8")
    run_env = gate_environment(build, sdk, prefix, runs, env, target_os)
    host = build / "bin" / ("crtweb_host" + exe)
    run_env["CRTWEB_HOST"] = str(host)
    run_env["CRTWEB_GPU_WORKER"] = str(build_root / "libcrtweb" / ("crtweb_gpu_worker" + exe))
    command = [str(build_root / "libcrtweb" / ("crtweb_gpu_resource_test" + exe)), "--require-gpu"]
    if lifecycle:
        command += ["--lifecycle"]
    if window_frames:
        command += ["--window-frames", str(window_frames)]
    observation = None
    if target_os == "macos":
        run_env.pop("DYLD_LIBRARY_PATH", None)
        completed, observation = run_gpu_program_macos(command, runs, run_env, resource_worker=True,
                                                      checkpoint_timeout=180 if lifecycle else 60)
    else:
        completed = subprocess.run(command, cwd=runs, env=run_env, capture_output=True, text=True, timeout=300 if lifecycle else 240)
    print(completed.stdout, end="")
    print(completed.stderr, end="", file=sys.stderr)
    (runs / "stdout.log").write_text(completed.stdout, encoding="utf-8")
    (runs / "stderr.log").write_text(completed.stderr, encoding="utf-8")
    library = build / "lib/libcrtweb.so"
    if target_os == "macos":
        library = build / "lib/libcrtweb.dylib"
    if target_os == "windows":
        candidates = sorted((build / "bin").glob("*crtweb.dll")) + sorted((build / "lib").glob("*crtweb.dll"))
        library = candidates[0] if candidates else build / "bin/libcrtweb.dll"
    audit = jsc.host_abi_audit(sdk, prefix, build, [host, library], run_env, target_os)
    evidence = (gpu_lifecycle_evidence(completed.stdout + completed.stderr, target_os) if lifecycle else
                gpu_resource_evidence(completed.stdout + completed.stderr, target_os, window_frames))
    runtime = gpu_browser_map_evidence(completed.stdout, sdk, prefix, build, runs, target_os)
    if observation is not None:
        runtime["observation"] = observation
        runtime["verified"] = runtime["verified"] and observation["verified"]
    return {"passed": completed.returncode == 0 and evidence["verified"] and audit["ok"] and runtime["verified"] is True,
            "returncode": completed.returncode, "evidence": evidence, "browser_runtime": runtime,
            "host_abi": {"ok": audit["ok"], "violations": audit["violations"]},
            "output": completed.stdout.splitlines()}


def audio_output_evidence(text: str) -> dict:
    sinks = []
    routes = {}
    for line in text.splitlines():
        if line.startswith("CRTMEDIA_AUDIO sink_open "):
            route = dict(re.findall(r"(\w+)=([^ ]+)", line))
            if route.get("sink_id", "").isdigit():
                routes[int(route["sink_id"])] = route
        if line.startswith("CRTWEB_AUDIO sink_closed "):
            sink = {key: int(value) for key, value in re.findall(r"(\w+)=(-?\d+)", line)}
            route = routes.get(sink.get("sink_id"), {})
            sink.update({key: route[key] for key in ("backend", "device") if key in route})
            sinks.append(sink)
    def delivered(sink):
        return (sink.get("written_frames", 0) > 0 and sink.get("nonzero_samples", 0) > 0
                and sink.get("position_frames", 0) > 0 and sink.get("position_result", -1) == 0
                and sink.get("clock_advances", 0) > 0 and sink.get("write_failures", -1) == 0)
    no_failures = all(sink.get("write_failures", -1) == 0 for sink in sinks)
    verified = no_failures and any(delivered(sink) for sink in sinks)
    def alsa_playback(sink):
        return sink.get("backend") == "alsa" and bool(re.fullmatch(
            r"/dev/snd/pcmC[0-9]+D[0-9]+p", sink.get("device", "")))
    def complete_fixture(sink):
        return (delivered(sink) and sink.get("rate") == 48000 and sink.get("channels") == 2
                and sink.get("written_frames") == 14400 and sink.get("nonzero_samples", 0) > 27000
                and sink.get("position_frames", 0) >= sink["written_frames"])
    return {"verified": verified, "sinks": sinks,
            "alsa_hardware_verified": no_failures and any(
                alsa_playback(sink) and delivered(sink)
                for sink in sinks),
            # Generated 300-ms S16/stereo/48kHz clip: paused preroll must not
            # discard all but its final decoded buffer before opening output;
            # device position must also reach the accepted count before close.
            "fixture_pcm_complete": no_failures and any(complete_fixture(sink) for sink in sinks),
            "native_fixture_verified": no_failures and any(
                alsa_playback(sink) and complete_fixture(sink) for sink in sinks),
            "fallback_count": text.count("CRTWEB_AUDIO sink_open_failed "),
            "meaning": "nonzero PCM accepted, sink position advanced and used to re-anchor the clock; not a listening test or proof a Pulse sink is physical"}


def run_media_gate(build: Path, sdk: Path, prefix: Path, work: Path, env: dict, target_os: str = "linux",
                   require_audio_output: bool = False) -> dict:
    """Tranche 8B: HTML <video>/<audio> through the CRT playback engine (crtmedia). Runs
    crtweb_media_test, then audits the Web process's mapped files: the crtmedia library is in (the
    engine really ran), no host VA-API/audio/GPU library is, and exactly one C++ runtime is mapped."""
    import subprocess
    run_env = gate_environment(build, sdk, prefix, work, env, target_os)
    run_env["CRTWEB_MEDIA_AUDIO_TRACE"] = "1"
    if target_os in ("linux", "windows"):
        run_env["CRTMEDIA_AUDIO_SINK_TRACE"] = "1"
    if target_os == "macos":
        # CoreAudio brings Apple's own C++ runtime. A global loader override also rewrites the
        # framework's library lookup; the linked RPATHs preserve the CRT/OS framework boundary.
        run_env.pop("DYLD_LIBRARY_PATH", None)
    runs = work / "media-run"
    runs.mkdir(exist_ok=True)
    exe = ".exe" if target_os == "windows" else ""
    completed = subprocess.run([str(build / "bin" / ("crtweb_media_test" + exe)), str(ROOT / "libcrtmedia" / "assets")],
                               env=run_env, cwd=runs, capture_output=True, text=True, timeout=300)
    print(completed.stdout, end="")
    print(completed.stderr, end="", file=sys.stderr)
    roots = [sdk.resolve(), prefix.resolve(), build.resolve(), (ROOT / "libcrtgfx" / "assets" / "fonts").resolve(),
             work.resolve()]
    mapped = [line.split(" ", 2)[2] for line in completed.stdout.splitlines() if line.startswith("MAPPED WebProcess ")]
    violations = []
    macos = target_os == "macos"
    windows = target_os == "windows"
    system_root = Path(os.environ.get("SystemRoot", r"C:\Windows")).resolve() if windows else None
    system_runtime = []
    os_runtime = []
    for path in mapped:
        name = Path(path).name
        if name.startswith("ld-linux"):
            continue
        if macos and (path.startswith(("/usr/lib/", "/System/", "/usr/share/", "/Library/Apple/",
                                       "/private/var/db/", "/usr/libexec/")) or path == "/usr/bin/vmmap"):
            # The OS media frameworks libcrtmedia reaches (AVFoundation, AudioToolbox, VideoToolbox) and
            # what they pull in (their ICU data, their log caches, the shared cache's iOSSupport
            # volume, which vmmap lists): the OS's own, not a CRT dependency. Only a second C++ runtime
            # matters.
            if name.startswith(("libc++", "libc++abi")):
                system_runtime.append(path)
            continue
        if windows:
            resolved = Path(path).resolve()
            if resolved.is_relative_to(system_root) and resolved.suffix.lower() == ".dll":
                os_runtime.append(resolved.name.lower())
                continue
        if not any(Path(path).resolve().is_relative_to(root) for root in roots):
            violations.append(f"WebProcess maps {path}")
    names = {Path(path).name.lower() if windows else Path(path).name
             for path in mapped if not (macos and path.startswith(("/usr/lib/", "/System/")))}
    runtimes = sorted(n for n in names if n.startswith(("libc++.so", "libc++abi", "libc++.1", "libc++.dylib", "libc++.dll")))
    result = {"returncode": completed.returncode, "mapped_files": len(mapped), "violations": violations,
              "cxx_runtime_files": runtimes, "system_cxx_runtime_files": sorted(set(system_runtime)),
              "os_runtime_dlls": sorted(set(os_runtime)),
              "crtmedia_mapped": any(n.startswith("libcrtmedia") for n in names),
              "va_stub_mapped": sorted(n for n in names if n.startswith("libva")),
              "audio_output": audio_output_evidence(completed.stderr),
              "output": completed.stdout.splitlines()}
    if target_os == "linux":
        result["audio_output"]["endpoint_verified"] = result["audio_output"]["verified"]
        result["audio_output"]["verified"] &= (result["audio_output"]["alsa_hardware_verified"]
                                                and result["audio_output"]["native_fixture_verified"])
        result["audio_output"]["linux_route_policy"] = "direct ALSA device evidence; Pulse endpoint alone may be Dummy Output"
        result["audio_output"]["playback_devices"] = [
            {"path": str(device), "writable": os.access(device, os.W_OK)}
            for device in sorted(Path("/dev/snd").glob("pcm*p"))]
        # Optional build-host observation, not a host client library in the target closure and not
        # a reason to change routing/permissions. Direct ALSA evidence remains the strict criterion.
        wpctl = shutil.which("wpctl")
        if wpctl:
            try:
                observed = subprocess.run([wpctl, "inspect", "@DEFAULT_AUDIO_SINK@"],
                                          capture_output=True, text=True, timeout=5)
                result["audio_output"]["pipewire_default_sink"] = dict(re.findall(
                    r'^\s*\*?\s*(node\.name|node\.description|node\.virtual|factory\.name)\s*=\s*"([^"]*)"',
                    observed.stdout, re.M))
            except (OSError, subprocess.TimeoutExpired):
                result["audio_output"]["pipewire_default_sink"] = {"observation": "unavailable"}
    (runs / "audio-output.log").write_text(completed.stderr, encoding="utf-8")
    result["passed"] = (completed.returncode == 0 and "crtweb_media_test: ok" in completed.stdout and not violations
                        and result["crtmedia_mapped"]
                        # Linux: exactly one C++ runtime (libc++ and libc++abi). macOS: exactly one CRT
                        # runtime image pair; the OS frameworks libcrtmedia reaches bring Apple's own
                        # /usr/lib libc++ and libc++abi, which are recorded and not a CRT dependency.
                        and len(runtimes) == 2 and (macos or not system_runtime))
    if require_audio_output:
        result["passed"] &= (result["audio_output"]["verified"]
                             and result["audio_output"]["fixture_pcm_complete"])
        if target_os == "linux":
            # A Pulse null sink acknowledges nonzero PCM and advances a software clock. That is
            # endpoint delivery, NOT physical Linux output. Require an observed native ALSA sink.
            result["audio_output"]["physical_linux_output_required"] = True
    return result


def camera_stress_evidence(text: str, expected_cycles: int = 20, require_device_fds: bool = False) -> dict:
    """A fresh process per run cannot prove bounded camera lifetimes in one browser process."""
    def fields(line):
        return {key: int(value) for key, value in re.findall(r"(\w+)=(-?\d+)", line)}
    baselines = [fields(line) for line in text.splitlines() if line.startswith("CAMERA_STRESS baseline ")]
    cycles = [fields(line) for line in text.splitlines() if line.startswith("CAMERA_STRESS cycle=")]
    summaries = [fields(line) for line in text.splitlines() if line.startswith("CAMERA_STRESS completed=")]
    permissions = [fields(line) for line in text.splitlines() if line.startswith("CAMERA_STRESS permission_requests=")]
    verified = (expected_cycles >= 20 and len(baselines) == len(summaries) == len(permissions) == 1
                and len(cycles) == expected_cycles and "CAMERA_STRESS failed" not in text)
    baseline = baselines[0] if len(baselines) == 1 else {}
    summary = summaries[0] if len(summaries) == 1 else {}
    verified &= (baseline.get("pid", -1) > 0 and summary.get("completed") == expected_cycles
                 and summary.get("warmups", -1) >= 3 and summary.get("same_process") == 1
                 and summary.get("rss_limit_kib") == 24576 and summary.get("thread_limit") == 2
                 and summary.get("descriptor_limit") == 2
                 and (permissions[0].get("permission_requests") if len(permissions) == 1 else -1)
                 == 1 + summary.get("warmups", 0) + expected_cycles)
    for role in ("ui", "web"):
        verified &= all(baseline.get(role + "_" + key, -1) > 0 for key in ("rss", "threads", "descriptors"))
    for index, cycle in enumerate(cycles, 1):
        verified &= (cycle.get("cycle") == index and cycle.get("pid") == baseline.get("pid")
                     and cycle.get("samples", 0) >= 3 and cycle.get("distinct", 0) >= 2)
        verified &= cycle.get("mode") == (summary.get("warmups", -1) + index - 1) % 4
        if "settled_ms" in cycle or "settle_timeout_ms" in summary:
            verified &= (summary.get("settle_timeout_ms") == 2000
                         and 0 <= cycle.get("settled_ms", -1) <= 2000)
        for role in ("ui", "web"):
            for key, limit in (("rss", 24576), ("threads", 2), ("descriptors", 2)):
                delta = cycle.get(role + "_" + key + "_delta")
                verified &= delta is not None and delta <= limit and baseline.get(role + "_" + key, -1) + delta > 0
        if require_device_fds:
            verified &= cycle.get("camera_fds") == 0
    verified &= {cycle.get("mode") for cycle in cycles} == {0, 1, 2, 3}
    return {"verified": bool(verified), "baseline": baseline, "summary": summary, "cycles": cycles,
            "device_fd_release_required": require_device_fds,
            "meaning": "same UI/Web process; bounded post-stop RSS/threads/descriptors after warm-up; no images saved"}


def run_physical_camera_gate(build: Path, sdk: Path, prefix: Path, work: Path, env: dict,
                             target_os: str, runs: int = 3) -> dict:
    run_env = gate_environment(build, sdk, prefix, work, env, target_os)
    run_env.pop("CRTWEB_TEST_SYNTHETIC_CAMERA", None)
    if target_os == "macos":
        # Use the linked SDK RPATHs, as for the out-of-process WebView replay.
        run_env.pop("DYLD_LIBRARY_PATH", None)
    directory = work / "physical-camera-run"
    directory.mkdir(exist_ok=True)
    results = []
    exe = ".exe" if target_os == "windows" else ""
    for iteration in range(runs):
        completed = subprocess.run([str(build / "bin" / ("crtweb_camera_probe" + exe)), "--allow-physical-camera", "3"],
                                   env=run_env, cwd=directory, capture_output=True, text=True, timeout=90)
        output = completed.stdout + completed.stderr
        print(output, end="")
        (directory / f"run-{iteration + 1}.log").write_text(output, encoding="utf-8")
        results.append({"returncode": completed.returncode,
                        "passed": completed.returncode == 0 and "crtweb_camera_probe: ok" in completed.stdout,
                        "output": output.splitlines()})
    completed = subprocess.run([str(build / "bin" / ("crtweb_camera_probe" + exe)), "--allow-physical-camera",
                                "0", "--stress-cycles=20"], env=run_env, cwd=directory,
                               capture_output=True, text=True, timeout=300)
    output = completed.stdout + completed.stderr
    print(output, end="")
    (directory / "stress.log").write_text(output, encoding="utf-8")
    stress = camera_stress_evidence(output, require_device_fds=target_os == "linux")
    stress.update(returncode=completed.returncode, output=output.splitlines())
    stress["passed"] = (completed.returncode == 0 and "crtweb_camera_probe: ok" in completed.stdout
                        and stress["verified"])
    return {"passed": bool(results) and all(run["passed"] for run in results) and stress["passed"],
            "source": "physical (explicit opt-in)", "runs": results, "stress": stress,
            "physical_camera_opened": True if any(run["passed"] for run in results) else None}


def run_capture_gate(build: Path, sdk: Path, prefix: Path, work: Path, env: dict, target_os: str = "linux") -> dict:
    """Tranche 8C: camera permission lifecycle and capture. Runs crtweb_capture_test with the labelled
    synthetic camera (no physical device is opened) and records that fact; a physical-camera run is a
    separate, explicitly opt-in procedure and is never produced by this gate."""
    import subprocess
    run_env = gate_environment(build, sdk, prefix, work, env, target_os)
    runs = work / "capture-run"
    runs.mkdir(exist_ok=True)
    exe = ".exe" if target_os == "windows" else ""
    completed = subprocess.run([str(build / "bin" / ("crtweb_capture_test" + exe))], env=run_env, cwd=runs,
                               capture_output=True, text=True, timeout=300)
    (runs / "capture-output.log").write_text(completed.stdout + completed.stderr, encoding="utf-8")
    print(completed.stdout, end="")
    print(completed.stderr, end="", file=sys.stderr)
    return {"passed": completed.returncode == 0 and "crtweb_capture_test: ok" in completed.stdout,
            "returncode": completed.returncode, "source": "synthetic (test-only, labelled)",
            "physical_camera_opened": False, "output": completed.stdout.splitlines()}


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
                        help="run the WebCore gate; with --webkit also run browser/API/lifecycle, "
                             "render/input, WebView, network trust and Host ABI gates")
    parser.add_argument("--run-gpu-tests", action="store_true",
                        help="with --webkit: strict Tranche 9A GPU WebView gate (alone or with --run-tests)")
    parser.add_argument("--gpu-window-frames", type=int, default=0,
                        help="with --run-gpu-tests: also open a real GPU window for 1..300 frames")
    parser.add_argument("--run-gpu-resource-tests", action="store_true",
                        help="with --webkit: strict independent Tranche 9B GPU resource/worker gate")
    parser.add_argument("--run-gpu-lifecycle-tests", action="store_true",
                        help="with --webkit: strict Tranche 9C owned GPU worker/device lifecycle gate")
    parser.add_argument("--gpu-resource-window-frames", type=int, default=0,
                        help="with --run-gpu-resource-tests: also present 1..300 native GPU frames")
    parser.add_argument("--reuse-tree", action="store_true", help="keep the composed tree and build directory")
    parser.add_argument("--recompose", action="store_true",
                        help="with --reuse-tree: compose the source tree again (a patch or the pin changed) but keep "
                             "the build directory, so only files whose content changed are rebuilt")
    parser.add_argument("--webkit", action="store_true",
                        help="also build WebKit and its Web/Network process executables (milestone B4)")
    parser.add_argument("--require-audio-output", action="store_true",
                        help="with --webkit --run-tests: require real PCM delivery and a progressing sink clock")
    parser.add_argument("--allow-physical-camera", action="store_true",
                        help="with --webkit --run-tests: explicitly open the real camera in three probe runs "
                             "and 20 same-process capture lifetimes after warm-up")
    parser.add_argument("--define", action="append", default=[], help="extra -D option for cmake")
    parser.add_argument("-k", "--keep-going", action="store_true", help="ninja -k 0: collect every error")
    parser.add_argument("-j", "--jobs", type=int, default=0)
    args = parser.parse_args()
    if args.run_gpu_tests and not args.webkit:
        parser.error("--run-gpu-tests requires --webkit")
    if args.run_gpu_resource_tests and not args.webkit:
        parser.error("--run-gpu-resource-tests requires --webkit")
    if args.run_gpu_lifecycle_tests and not args.webkit:
        parser.error("--run-gpu-lifecycle-tests requires --webkit")
    if not 0 <= args.gpu_resource_window_frames <= 300 or (args.gpu_resource_window_frames and not args.run_gpu_resource_tests):
        parser.error("--gpu-resource-window-frames requires --run-gpu-resource-tests and a count in 0..300")
    if args.gpu_window_frames < 0 or args.gpu_window_frames > 300 or (args.gpu_window_frames and not args.run_gpu_tests):
        parser.error("--gpu-window-frames requires --run-gpu-tests and a count in 0..300")
    if (args.require_audio_output or args.allow_physical_camera) and not (args.webkit and args.run_tests):
        parser.error("device evidence options require --webkit --run-tests")

    sdk, prefix, work = args.sdk_root.resolve(), args.deps_prefix.resolve(), args.work_root.resolve()
    work.mkdir(parents=True, exist_ok=True)
    target_os, arch = jsc.sdk_target(sdk)
    if (args.require_audio_output or args.allow_physical_camera) and target_os not in ("linux", "macos", "windows"):
        parser.error("device evidence gates are implemented for Linux, macOS and Windows")
    if target_os not in ("linux", "macos", "windows"):
        raise SystemExit(f"B3 is not replayed on {target_os} (docs/acceptance/crtweb_acceptance.md)")
    candidate = prefix / "bin" / "gperf"
    if os.name == "nt" and not candidate.exists():
        candidate = candidate.with_suffix(".exe")
    gperf = args.gperf or (candidate if candidate.exists() else
                           Path(shutil.which("gperf")) if shutil.which("gperf") else None)
    if gperf is None:
        raise SystemExit("gperf not found: WebCore generates its hash tables with it (--gperf)")
    build = work / "build"
    tree = work / "src" / "webkit-crt"
    env = port_environment(sdk, prefix, gperf.resolve(), target_os, work)
    if args.reuse_tree and not args.recompose and (tree / "Source").is_dir():
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
    if args.webkit:
        args.define.append("CRT_ENABLE_WEBKIT=ON")
    reconfigure = args.recompose and (build / "build.ninja").exists()
    if reconfigure:
        # Re-run cmake in the existing build directory with the (possibly changed) options.
        # Every -D of a fresh configure (compiler flags included: a change to the persona's flags
        # must reach an existing build directory), then the explicit defines.
        fresh = configure_command(tree, build, sdk, prefix, arch, [], target_os)
        defines = [item for item in fresh if item.startswith("-D") and not item.startswith("-DCMAKE_TOOLCHAIN_FILE")]
        jsc.run(["cmake"] + defines + [f"-D{d}" for d in args.define] + ["."], cwd=build, env=env)
    elif not (build / "build.ninja").exists() or not args.reuse_tree:
        jsc.run(configure_command(tree, build, sdk, prefix, arch, [f"-D{d}" for d in args.define], target_os),
                env=env)
    if args.configure_only:
        return 0
    tests = ["crtweb_webcore_graph_test"] + (["crtweb_process_test"] if args.webkit else []) \
        + (["crtweb_api_test", "crtweb_api_lifecycle_test"] if args.webkit and target_os in ("linux", "macos", "windows") else [])
    if args.run_tests and not args.target:
        args.target = list(tests) + (["WebProcess", "NetworkProcess", "crtweb_render_test", "crtweb_surface_host", "crtweb_lifecycle_test"]
                                          if args.webkit else []) \
            + (["crtweb_host", "crtweb_network_test"] if args.webkit and target_os in ("linux", "macos", "windows") else []) \
            + (["crtweb_media_test", "crtweb_capture_test"] if args.webkit and target_os in ("linux", "macos", "windows") else [])
    if args.allow_physical_camera:
        args.target = list(args.target or DEFAULT_TARGETS)
        if "crtweb_camera_probe" not in args.target:
            args.target.append("crtweb_camera_probe")
    if args.run_gpu_tests or args.run_gpu_resource_tests or args.run_gpu_lifecycle_tests:
        args.target = list(args.target or (["crtweb_host"] if not args.run_tests else DEFAULT_TARGETS))
        if "crtweb_host" not in args.target:
            args.target.append("crtweb_host")
    command = ["ninja", "-C", str(build)]
    if args.jobs:
        command += [f"-j{args.jobs}"]
    if args.keep_going:
        command += ["-k", "0"]
    command += list(args.target or DEFAULT_TARGETS)
    status = jsc.run(command, env=env, check=False).returncode
    if status != 0 or not (args.run_tests or args.run_gpu_tests or args.run_gpu_resource_tests or args.run_gpu_lifecycle_tests):
        return status
    gpu_results = {}
    if args.run_gpu_tests:
        gpu_results["gpu_webview"] = run_gpu_gate(build, sdk, prefix, work, env, target_os, args.gpu_window_frames)
        gpu_passed = gpu_results["gpu_webview"]["passed"]
        (work / "acceptance-gpu.json").write_text(json.dumps(
            {"passed": gpu_passed, "programs": gpu_results}, indent=2) + "\n", encoding="utf-8")
    if args.run_gpu_resource_tests:
        gpu_results["gpu_resources"] = run_gpu_resource_gate(build, sdk, prefix, work, env, target_os,
                                                            args.gpu_resource_window_frames)
        (work / "acceptance-gpu-resources.json").write_text(json.dumps(
            {"passed": gpu_results["gpu_resources"]["passed"], "programs": {
                "gpu_resources": gpu_results["gpu_resources"]}}, indent=2) + "\n", encoding="utf-8")
    if args.run_gpu_lifecycle_tests:
        gpu_results["gpu_lifecycle"] = run_gpu_resource_gate(build, sdk, prefix, work, env, target_os, lifecycle=True)
        (work / "acceptance-gpu-lifecycle.json").write_text(json.dumps(
            {"passed": gpu_results["gpu_lifecycle"]["passed"], "programs": {
                "gpu_lifecycle": gpu_results["gpu_lifecycle"]}}, indent=2) + "\n", encoding="utf-8")
    if not args.run_tests:
        passed = all(result["passed"] for result in gpu_results.values())
        print("GPU gates:", "PASSED" if passed else "FAILED")
        return 0 if passed else 1
    results = {name: run_program(name, build, sdk, prefix, work, env, target_os) for name in tests}
    results.update(gpu_results)
    if args.webkit and target_os in ("linux", "macos", "windows"):
        results["render_through_surface_protocol"] = run_render_gate(build, sdk, prefix, work, env, target_os)
        print("B5 diff:", json.dumps(results["render_through_surface_protocol"].get("diff_against_wpe_reference")))
        results["input_through_surface_protocol"] = run_input_gate(build, sdk, prefix, work, env, target_os)
        results["lifecycle_and_host_abi"] = run_lifecycle_gate(build, sdk, prefix, work, env, target_os)
    if args.webkit and target_os in ("linux", "macos", "windows"):
        results["webview_through_crtui"] = run_webview_gate(build, sdk, prefix, work, env, target_os)
        if target_os in ("linux", "macos", "windows"):
            results["media_playback"] = run_media_gate(build, sdk, prefix, work, env, target_os, args.require_audio_output)
            results["camera_capture"] = run_capture_gate(build, sdk, prefix, work, env, target_os)
            if args.allow_physical_camera:
                results["physical_camera"] = run_physical_camera_gate(build, sdk, prefix, work, env, target_os)
        network_program = build / "bin" / ("crtweb_network_test.exe" if target_os == "windows" else "crtweb_network_test")
        network_env = gate_environment(build, sdk, prefix, work, env, target_os)
        webview_env = dict(network_env)
        webview_env["CRTWEB_HOST"] = str(build / "bin" / ("crtweb_host.exe" if target_os == "windows" else "crtweb_host"))
        if target_os == "macos":
            webview_env.pop("DYLD_LIBRARY_PATH", None)
        network = test_crtweb_network.run_gate(
            network_program, network_env, work, path_for_target=crt_path if target_os == "windows" else str,
            webview_program=sdk.parent.parent / "libcrtweb" / ("crtweb_webview_test.exe" if target_os == "windows" else "crtweb_webview_test"),
            webview_env=webview_env)
        network["audit"] = (audit_program_native(network_program, sdk, prefix, build, target_os)
                            if target_os in ("macos", "windows")
                            else audit_program(network_program, sdk, prefix, env, build))
        network["passed"] &= network["audit"]["ok"]
        results["crt_network_trust"] = network
    passed = all(r["passed"] for r in results.values())
    (work / "acceptance-b3.json").write_text(json.dumps(
        {"passed": passed, "programs": results}, indent=2) + "\n", encoding="utf-8")
    print("B3-C8/Tranche 5 gate:", "PASSED" if passed else "FAILED", {k: v["passed"] for k, v in results.items()})
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
