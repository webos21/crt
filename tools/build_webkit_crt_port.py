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
import time
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
        f"-DCRT_WEBKIT_TESTS={ROOT / 'libcrtweb' / 'tests' / 'webkit'}",
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
    completed = jsc.run([str(program)], env=run_env, cwd=work, check=False, capture=True)
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


def observe_macos_processes(parent_pid: int) -> dict:
    """The mapped files of the three live B7 processes (UI, Web, Network), from the host side. macOS has
    no /proc, and an observer started inside the process under test (popen of ps/vmmap) is a child
    that changes the lifecycle being measured, so the driver samples while the test runs."""
    children = {}
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
    pattern = re.compile(r"(?:^|\s)(/(?:Users|private|tmp|usr|System)/.*)$")
    for role, pid in {"UIProcess": parent_pid, **children}.items():
        output = subprocess.run(["vmmap", str(pid)], capture_output=True, text=True, check=False).stdout
        files = set()
        for line in output.splitlines():
            match = pattern.search(line)
            if match:
                files.add(match.group(1).rstrip())
        mapped[role] = sorted(files)
    return mapped


def run_lifecycle_program_macos(build: Path, sdk: Path, prefix: Path, work: Path, env: dict) -> tuple:
    """crtweb_lifecycle_test as run_program runs it, with the host-side sampling of the process maps."""
    program = build / "bin" / "crtweb_lifecycle_test"
    process = subprocess.Popen([str(program)], cwd=work, env=gate_environment(build, sdk, prefix, work, env, "macos"),
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    mapped = observe_macos_processes(process.pid)
    stdout, stderr = process.communicate()
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
        if target_os == "macos" and (path.startswith(("/usr/lib/", "/System/Library/"))
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
    parser.add_argument("--recompose", action="store_true",
                        help="with --reuse-tree: compose the source tree again (a patch or the pin changed) but keep "
                             "the build directory, so only files whose content changed are rebuilt")
    parser.add_argument("--webkit", action="store_true",
                        help="also build WebKit and its Web/Network process executables (milestone B4)")
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
    tests = ["crtweb_webcore_graph_test"] + (["crtweb_process_test"] if args.webkit else [])
    if args.run_tests and not args.target:
        args.target = list(tests) + (["WebProcess", "NetworkProcess", "crtweb_render_test", "crtweb_surface_host", "crtweb_lifecycle_test"] if args.webkit else [])
    command = ["ninja", "-C", str(build)]
    if args.jobs:
        command += [f"-j{args.jobs}"]
    if args.keep_going:
        command += ["-k", "0"]
    command += list(args.target or DEFAULT_TARGETS)
    status = jsc.run(command, env=env, check=False).returncode
    if status != 0 or not args.run_tests:
        return status
    results = {name: run_program(name, build, sdk, prefix, work, env, target_os) for name in tests}
    if args.webkit and target_os in ("linux", "macos", "windows"):
        results["render_through_surface_protocol"] = run_render_gate(build, sdk, prefix, work, env, target_os)
        print("B5 diff:", json.dumps(results["render_through_surface_protocol"].get("diff_against_wpe_reference")))
        results["input_through_surface_protocol"] = run_input_gate(build, sdk, prefix, work, env, target_os)
        results["lifecycle_and_host_abi"] = run_lifecycle_gate(build, sdk, prefix, work, env, target_os)
    passed = all(r["passed"] for r in results.values())
    (work / "acceptance-b3.json").write_text(json.dumps(
        {"passed": passed, "programs": results}, indent=2) + "\n", encoding="utf-8")
    print("B3-B7 gate:", "PASSED" if passed else "FAILED", {k: v["passed"] for k, v in results.items()})
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
