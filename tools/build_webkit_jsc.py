#!/usr/bin/env python3
"""Build JavaScriptCore (WebKit's JSCOnly port) with the CRT toolchain and run the CRT
acceptance (Web Tranche 1, docs/crtweb_acceptance.md).

The build is driven the way an external consumer would drive it: the pinned WPE WebKit
tarball (libcrtweb/third_party/webkit/recipe.json, verified by tools/fetch_webkit.py) is
configured with an installed SDK's crt-toolchain.cmake -- the CRT compiler wrappers and
sysroot -- never with host clang and host headers. The target-side
dependency, ICU, comes from a CRT port recipe (porting/recipes/icu.json) installed under
--deps-prefix. The BUILD-host tools -- CMake, Ninja, Perl (with English, FindBin, JSON::PP),
Python and Ruby >= 2.5 -- run on the build machine and are not CRT artifacts: JavaScriptCore
generates its interpreter with Ruby and Perl scripts. gperf is not needed here: upstream asks
for it only when WebCore is enabled (Source/cmake/WebKitCommon.cmake), so it is a later
prerequisite, not a JSCOnly one.

Steps: check host tools -> fetch and verify -> extract -> configure -> build `jsc` -> build
the lifecycle test program -> run the acceptance (a JavaScript script through the `jsc`
shell, and a C API program that creates, uses and releases JS contexts 150 times and on
several threads while watching resident memory).

Two build modes, one new runtime assumption each (Web Tranche 1B and 1C):

  interpreter   the C_LOOP interpreter; JIT, DFG, FTL, WebAssembly and the sampling profiler off
  baseline-jit  the assembly interpreter plus the JIT tiers compiled in (a smaller JIT build does
                not compile, see MODE_OPTIONS); the DFG, FTL and WebAssembly tiers are disabled at
                run time, so only the Baseline JIT generates code. The sampling profiler is off

both with the Generic event loop (no GLib). In baseline-jit mode the same binary is run twice:
with JSC_useJIT=false (the assembly interpreter alone: the whole 1B acceptance must still pass and
no code may be compiled) and with the Baseline JIT on, where the compile report must prove that
machine code was generated.
Nothing in the WebKit tree is patched; every deviation is a CMake option or a compiler
flag, listed in CONFIGURE_OPTIONS below with the reason.
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
import time
from contextlib import contextmanager
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TESTS = ROOT / "libcrtweb" / "tests" / "jsc"

# Peak resident memory the acceptance script may reach (KiB). The script allocates about
# 400 MB of short-lived objects; a collector that did not reclaim would far exceed this.
ACCEPTANCE_MAX_RSS_KB = 450 * 1024
CYCLE_THREAD_COUNTS = (0, 1, 4, 8)


def run(command, env=None, cwd=None, check=True, capture=False):
    print("+", " ".join(str(part) for part in command), flush=True)
    return subprocess.run(
        [str(part) for part in command], env=env, cwd=cwd, check=check,
        capture_output=capture, text=True)


class Phases:
    def __init__(self):
        self.records = []

    @contextmanager
    def measure(self, name):
        print(f"==> {name}", flush=True)
        started = time.monotonic()
        status = "ok"
        try:
            yield
        except BaseException:
            status = "failed"
            raise
        finally:
            elapsed = time.monotonic() - started
            self.records.append((name, elapsed, status))
            print(f"<== {name}: {status} ({elapsed:.1f}s)", flush=True)

    def report(self):
        print("JSC bring-up phase timing summary:")
        for name, elapsed, status in self.records:
            print(f"  {elapsed:9.1f}s  {status:6s}  {name}")


def sdk_target(sdk: Path) -> tuple[str, str]:
    manifest = json.loads((sdk / "manifest.json").read_text(encoding="utf-8"))
    target = manifest.get("target", {})
    return target.get("os", ""), target.get("arch", "")


def tool_version(command) -> str:
    completed = run(command, capture=True, check=False)
    return (completed.stdout or completed.stderr).strip().splitlines()[0] if (completed.stdout or completed.stderr) else "?"


def require_host_tools() -> dict:
    """The build-host tools (they run on the build machine, whatever the target) and
    their versions, which also go into the configure fingerprint."""
    # Windows has no `python3` command; the interpreter running this script is the Python.
    missing = [tool for tool in ("cmake", "ninja", "perl") if shutil.which(tool) is None]
    ruby = shutil.which("ruby")
    if ruby is None:
        missing.append("ruby (>= 2.5: JavaScriptCore generates its interpreter with Ruby scripts)")
    if missing:
        raise SystemExit("missing host build tools: " + ", ".join(missing))
    ruby_version = run([ruby, "-e", "print RUBY_VERSION"], capture=True).stdout.strip()
    major, minor = (int(part) for part in ruby_version.split(".")[:2])
    if (major, minor) < (2, 5):
        raise SystemExit(f"ruby {ruby_version} found, 2.5 or newer is required")
    modules = run(["perl", "-MEnglish", "-MFindBin", "-MJSON::PP", "-e", "print 1"], capture=True, check=False)
    if modules.stdout.strip() != "1":
        raise SystemExit("perl needs the English, FindBin and JSON::PP modules (WebKitCommon.cmake requires them)")
    return {
        "cmake": tool_version(["cmake", "--version"]),
        "ninja": tool_version(["ninja", "--version"]),
        "perl": tool_version(["perl", "-e", "print $^V"]),
        "python": tool_version([sys.executable, "--version"]),
        "ruby": ruby_version,
    }


def fetch_and_extract(recipe: Path, cache: Path, work: Path) -> Path:
    recipe_data = json.loads(recipe.read_text(encoding="utf-8"))
    version = recipe_data["version"]
    source = work / "src"
    tree = source / f"wpewebkit-{version}"
    marker = source / f".extracted-{recipe_data['source']['archive_sha256']}"
    run([sys.executable, ROOT / "tools" / "fetch_webkit.py", "--recipe", recipe, "--cache", cache])
    if marker.is_file() and tree.is_dir():
        print(f"WebKit {version} already extracted at {tree}")
        return tree
    if source.exists():
        shutil.rmtree(source)
    source.mkdir(parents=True)
    archive = cache / recipe_data["source"]["archive_name"]
    print(f"extracting {archive.name} ...", flush=True)
    with tarfile.open(archive) as handle:
        handle.extractall(source, filter="data")
    marker.write_text("ok\n", encoding="utf-8")
    return tree


PATCHES = ROOT / "libcrtweb" / "patches"


def sha256_file(path: Path) -> str:
    import hashlib
    return hashlib.sha256(path.read_bytes()).hexdigest()


def apply_patches(tree: Path, target_os: str) -> list:
    """Apply the carried patches that target this host (libcrtweb/patches/manifest.json).

    Each patch lists the files it changes with their SHA-256 before and after. Every file must hash
    to sha256_before (then the patch is applied) or already to sha256_after (a rerun); anything else
    means the tree is not the pinned one and the run stops. Returns the applied entries for the
    configure fingerprint."""
    manifest = json.loads((PATCHES / "manifest.json").read_text(encoding="utf-8"))
    applied = []
    for entry in manifest["patches"]:
        if target_os not in entry["targets"]:
            continue
        states = []
        for item in entry["files"]:
            current = sha256_file(tree / item["file"])
            if current == item["sha256_after"]:
                states.append("applied")
            elif current == item["sha256_before"]:
                states.append("pristine")
            else:
                raise SystemExit(f"patch {entry['id']}: {item['file']} hashes to {current}, expected the "
                                 f"pinned {item['sha256_before']}")
        if len(set(states)) != 1:
            raise SystemExit(f"patch {entry['id']}: its files are in mixed states {states}")
        if states[0] == "pristine":
            run(["patch", "-p1", "-i", PATCHES / entry["patch"]], cwd=tree)
            for item in entry["files"]:
                if sha256_file(tree / item["file"]) != item["sha256_after"]:
                    raise SystemExit(f"patch {entry['id']}: result of {item['file']} does not hash to sha256_after")
        else:
            print(f"patch {entry['id']}: already applied")
        applied.append({"id": entry["id"], "files": [item["file"] for item in entry["files"]]})
    return applied


def build_environment(sdk: Path, deps: Path, target_os: str) -> dict:
    env = dict(os.environ)
    # Shared runtime linkage: ONE libc/libc++ in the process. The shared libraries
    # (libJavaScriptCore.so, ICU) use libc.so; an executable linking libc.a would carry a
    # second libc with its own thread/key/TLS registry and allocator.
    env["CRT_CXX_RUNTIME_LINKAGE"] = "shared"
    # WebKit's `jsc` and its tools define a plain `int main`; see tools/crt-c++.
    env["CRT_CXX_HOSTED"] = "1"
    if target_os == "windows":
        # What the SDK's activate.cmd sets for a consumer: the wrappers (crt-cc.cmd) need the CRT
        # shell and rootfs, and PATH finds the SDK's tools and runtime DLLs before the host's.
        root = str(sdk).replace("\\", "/")
        env["CRT_SYSROOT"] = root
        env["CRT_ROOTFS"] = root
        env["CRT_TARGET_OS"] = "windows"
        env["CRT_MKSH_EXE"] = f"{root}/system/bin/mksh.exe"
        # Read by patched offlineasm/asm.rb (libcrtweb/patches, 0003): no ELF .size/.type directives.
        env["WTF_CRT_COFF_ASM"] = "1"
        for variable, command in (("CRT_HOST_CC", "clang"), ("CRT_HOST_CXX", "clang++")):
            if not env.get(variable):
                found = shutil.which(command)
                if found:
                    env[variable] = found.replace("\\", "/")
        env["PATH"] = os.pathsep.join(
            [str(sdk / "tools"), str(sdk / "system" / "bin"), str(sdk / "bin"), env.get("PATH", "")])
    if target_os == "macos":
        # Read by patched offlineasm/asm.rb (libcrtweb/patches, 0002): no ELF .size/.type debug
        # directives although CMake is told the system is Linux.
        env["WTF_CRT_MACHO_ASM"] = "1"
    return env


COMMON_OPTIONS = [
    ("-DPORT=JSCOnly", "JavaScriptCore alone (no WebCore/WebKit)"),
    ("-DCMAKE_BUILD_TYPE=Release", ""),
    ("-DUSE_HEADER_MAPS=OFF", "the release tarball omits Tools/Scripts/hmaptool, which "
                              "header maps need; an upstream option, no source change"),
    ("-DENABLE_API_TESTS=OFF", "upstream disables them on Windows; CRT has its own acceptance"),
    ("-DDEVELOPER_MODE=OFF", ""),
    ("-DENABLE_SAMPLING_PROFILER=OFF", "needs signal-based thread suspension (and conflicts with C_LOOP)"),
]

MODE_OPTIONS = {
    "interpreter": [
        ("-DENABLE_JIT=OFF", "Tranche 1B interpreter first-green"),
        ("-DENABLE_C_LOOP=ON", "the portable C++ interpreter (no assembly LLInt, no JIT)"),
        ("-DENABLE_DFG_JIT=OFF", ""),
        ("-DENABLE_FTL_JIT=OFF", ""),
        ("-DENABLE_WEBASSEMBLY=OFF", "conflicts with C_LOOP"),
    ],
    "baseline-jit": [
        ("-DENABLE_JIT=ON", "Tranche 1C: generated code"),
        ("-DENABLE_C_LOOP=OFF", "the JIT conflicts with C_LOOP; this is the offlineasm assembly LLInt"),
        ("-DENABLE_DFG_JIT=ON", "the tiers are compiled in and the Baseline JIT is isolated at run time "
                                "(JSC_useDFGJIT/useFTLJIT/useWasm=false). A smaller JIT build does not compile "
                                "(upstream builds only the full default set): with DFG or WebAssembly off, "
                                "bytecode/InlineCacheCompiler.h uses CCallHelpers::Jump but only the DFG/"
                                "WebAssembly headers happen to include CCallHelpers.h before it, and the WebAssembly "
                                "sources need B3, which exists only with FTL"),
        ("-DENABLE_FTL_JIT=ON", "see above"),
        ("-DENABLE_WEBASSEMBLY=ON", "see above"),
    ],
}

# Run-time JSC options (the JSC_<name> environment variables) for each acceptance run.
JIT_OFF_OPTIONS = {"JSC_useJIT": "false", "JSC_reportBaselineCompileTimes": "true",
                   "JSC_validateOptions": "true"}
JIT_ON_OPTIONS = {"JSC_useJIT": "true", "JSC_useBaselineJIT": "true", "JSC_useDFGJIT": "false",
                  "JSC_useFTLJIT": "false", "JSC_useConcurrentJIT": "false", "JSC_useWasm": "false",
                  "JSC_jitPolicyScale": "0.01", "JSC_usePollingTraps": "false", "JSC_crashIfCantAllocateJITMemory": "true",
                  "JSC_reportBaselineCompileTimes": "true", "JSC_validateOptions": "true"}


def configure_options(mode: str):
    return COMMON_OPTIONS + MODE_OPTIONS[mode]


def configure(tree: Path, build: Path, sdk: Path, deps: Path, env: dict, target_os: str, arch: str, mode: str):
    toolchain = sdk / "crt-toolchain.cmake"
    if not toolchain.is_file():
        raise SystemExit(f"{toolchain} not found: --sdk-root must be an installed CRT SDK")
    c_flags = []
    if arch in ("x86_64", "amd64"):
        # libpas/bmalloc use 16-byte atomics; without cmpxchg16b the compiler emits calls to
        # __atomic_*_16 (libatomic), which the CRT toolchain does not provide.
        c_flags.append("-mcx16")
    link_flags = []
    macos_options = []
    if target_os == "macos":
        # CRT presents macOS to WebKit as a Bionic/Linux-shaped platform (libcrtweb/cmake/
        # crt_webkit_platform.cmake and the CRT wrapper's -U__APPLE__), because WebKit's Darwin
        # branches are Apple SDK code. __linux__ makes WTF/bmalloc take OS(LINUX); the int64
        # define selects the RawHex overloads for Darwin's `long long` int64_t (patch 0001).
        # The compilers are given explicitly: on a macOS host WebKitXcodeSDK.cmake otherwise
        # pins Xcode's own clang before the toolchain file is read.
        c_flags += ["-D__linux__=1", "-DWTF_CRT_INT64_IS_LONG_LONG=1", "-DWTF_CRT_MACHO_ASM=1",
                    # Mach-O: a global label inside a function is a new linker atom unless marked
                    # .alt_entry, and the linker may then move the continuation elsewhere (the
                    # LLInt's vmEntryToJavaScript lost the rest of its body). WTF turns this on only
                    # for PLATFORM(COCOA); the header does not guard it, so a -D is enough.
                    "-DENABLE_OFFLINE_ASM_ALT_ENTRY=1"]
        macos_options = [f"-DCMAKE_C_COMPILER={sdk / 'tools' / 'crt-cc'}",
                         f"-DCMAKE_CXX_COMPILER={sdk / 'tools' / 'crt-c++'}",
                         f"-DCMAKE_PROJECT_INCLUDE={ROOT / 'libcrtweb' / 'cmake' / 'crt_webkit_platform.cmake'}"]
    if target_os == "windows":
        # Linux-shaped presentation, as on macOS (libcrtweb/cmake/crt_webkit_platform.cmake): the pinned
        # tarball has no WIN32 sources, and WebKit's WIN32 branches need windows.h and the Microsoft C
        # runtime, which the CRT sysroot does not have. __linux__ makes WTF/bmalloc take OS(LINUX).
        # Every Windows/MinGW macro clang predefines for *-w64-mingw32 (WebKit tests WIN32, _WIN32,
        # WINNT and __MINGW32__ in different places), then the Linux one.
        c_flags += [f"-U{name}" for name in (
            "WIN32", "WIN64", "WINNT", "_WIN32", "_WIN64", "__WIN32", "__WIN32__", "__WIN64", "__WIN64__",
            "__WINNT", "__WINNT__", "__MINGW32__", "__MINGW64__")] + ["-D__linux__=1", "-DWTF_CRT_COFF_ASM=1",
                                                                         # GNU bit-field layout, as on Linux: WebKit packs bit-fields of different
                                                                         # underlying types (RegisterAtOffset: unsigned and ptrdiff_t, 8 bytes), which
                                                                         # the Microsoft layout MinGW targets default to does not.
                                                                         "-mno-ms-bitfields"]
        macos_options = [f"-DCMAKE_PROJECT_INCLUDE={ROOT / 'libcrtweb' / 'cmake' / 'crt_webkit_platform.cmake'}"]
        # thread_local is lowered to compiler-rt's emulated TLS (the wrapper's -femulated-tls), whose
        # emutls.c names two UCRT entry points that only its fatal-error path reaches; the stubs and
        # --allow-multiple-definition are the accommodation libcrtgfx documents for Skia's thread_local.
        # Data exported by one CRT DLL and read by another (environ, libc++'s vtables) is auto-imported.
        stubs = build.parent / "emutls_link_stubs.o"
        stubs.parent.mkdir(parents=True, exist_ok=True)
        run([sdk / "tools" / "crt-cc.cmd", "-c", ROOT / "libc" / "src" / "arch" / "windows" / "common" /
             "emutls_link_stubs.c", "-o", stubs], env=env)
        link_flags += [str(stubs).replace("\\", "/"), "-Wl,--allow-multiple-definition", "-Wl,--enable-auto-import"]
    if target_os == "linux":
        # Native TLS in a shared library: the initial-exec model avoids __tls_get_addr, which
        # only the (host) dynamic loader defines.
        c_flags.append("-ftls-model=initial-exec")
        # The shared libraries reference loader-provided symbols (__tls_get_addr from ICU and
        # libc++abi); the executable link must not insist on resolving them at link time.
        link_flags.append("-Wl,--allow-shlib-undefined")
    cxx_flags = list(c_flags)
    if target_os == "macos" and mode == "baseline-jit":
        # Apple Silicon W^X through JavaScriptCore's own extension point (no patch): the CRT toggle
        # behind OS_THREAD_SELF_RESTRICT. JavaScriptCore only uses these macros from C++.
        cxx_flags += ["-include", str(ROOT / "libcrtweb" / "cmake" / "crt_webkit_jit_permissions.h")]
    if build.exists():
        shutil.rmtree(build)
    command = ["cmake", "-S", tree, "-B", build, "-G", "Ninja",
               f"-DCMAKE_TOOLCHAIN_FILE={toolchain}", f"-DICU_ROOT={deps}"]
    command += macos_options
    command += [option for option, _ in configure_options(mode)]
    command += [f"-DCMAKE_C_FLAGS={' '.join(c_flags)}", f"-DCMAKE_CXX_FLAGS={' '.join(cxx_flags)}"]
    if link_flags:
        command += [f"-DCMAKE_EXE_LINKER_FLAGS={' '.join(link_flags)}",
                    f"-DCMAKE_SHARED_LINKER_FLAGS={' '.join(link_flags)}"]
    run(command, env=env)


def runtime_library_path(sdk: Path, deps: Path, build: Path, target_os: str, env: dict) -> dict:
    run_env = dict(env)
    paths = [build / "lib", deps / "lib", sdk / "lib"]
    if target_os == "windows":
        # DLLs are found through PATH: the build's own (JavaScriptCore.dll sits next to jsc.exe),
        # the CRT ICU port's, and the SDK's runtime (libc.dll, libc++.dll, ...) in bin/.
        paths = [build / "bin", build / "lib", deps / "bin", deps / "lib", sdk / "bin", sdk / "lib"]
    variable = ("DYLD_LIBRARY_PATH" if target_os == "macos" else "PATH" if target_os == "windows"
                else "LD_LIBRARY_PATH")
    existing = run_env.get(variable)
    run_env[variable] = os.pathsep.join(str(path) for path in paths) + (os.pathsep + existing if existing else "")
    return run_env


def build_test_programs(sdk: Path, build: Path, env: dict, target_os: str, mode: str = "interpreter") -> dict:
    """The C API test programs (libcrtweb/tests/jsc/*.cpp), linked against the built library."""
    programs = {}
    compile_env = dict(env)
    compile_env["CRT_SYSROOT"] = str(sdk)
    if target_os == "linux":
        compile_env["CRT_TARGET_OS"] = "linux"
    # The watchdog test interrupts compiled code with a signal and reads the forwarded machine
    # context, so it belongs to the JIT step only.
    for name in (("jsc_context_cycle", "jsc_watchdog_test") if mode == "baseline-jit" else ("jsc_context_cycle",)):
        output = build / "bin" / (name + (".exe" if target_os == "windows" else ""))
        command = [sdk / "tools" / ("crt-c++.cmd" if target_os == "windows" else "crt-c++"),
                   f"-I{build / 'JavaScriptCore' / 'Headers'}",
                   TESTS / f"{name}.cpp", "-o", output, f"-L{build / 'lib'}", f"-L{build / 'bin'}",
                   "-lJavaScriptCore"]
        if target_os != "windows":  # no rpath in PE: the DLL is found through PATH
            command.append(f"-Wl,-rpath,{build / 'lib'}")
        if target_os == "linux":
            command.append("-Wl,--allow-shlib-undefined")
        if target_os == "macos":
            # Same presentation as the JavaScriptCore build: <ucontext.h> then gives the
            # Linux-layout ucontext_t the CRT signal backend forwards to handlers.
            command.insert(1, "-D__linux__=1")
        run(command, env=compile_env)
        programs[name] = output
    return programs


# Peak resident memory of a run is measured by a fresh Python process that runs the program as
# its only child and reports RUSAGE_CHILDREN (in this process it would be the largest of every
# child ever waited for, the build's compilers included). Windows has no RUSAGE_CHILDREN and
# Python has no `resource` module there: the measuring process polls the child's peak working
# set instead (monotonic, so the last sample before exit is within one poll interval of the peak).
MEASURE_WINDOWS = r"""
import ctypes, subprocess, sys, time
from ctypes import wintypes

class Counters(ctypes.Structure):
    _fields_ = [("cb", wintypes.DWORD), ("PageFaultCount", wintypes.DWORD),
                ("PeakWorkingSetSize", ctypes.c_size_t), ("WorkingSetSize", ctypes.c_size_t),
                ("QuotaPeakPagedPoolUsage", ctypes.c_size_t), ("QuotaPagedPoolUsage", ctypes.c_size_t),
                ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t), ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
                ("PagefileUsage", ctypes.c_size_t), ("PeakPagefileUsage", ctypes.c_size_t)]

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
kernel32.K32GetProcessMemoryInfo.argtypes = [wintypes.HANDLE, ctypes.POINTER(Counters), wintypes.DWORD]
process = subprocess.Popen(sys.argv[1:])
peak = 0
while True:
    counters = Counters()
    counters.cb = ctypes.sizeof(counters)
    if kernel32.K32GetProcessMemoryInfo(int(process._handle), ctypes.byref(counters), counters.cb):
        peak = max(peak, counters.PeakWorkingSetSize)
    if process.poll() is not None:
        break
    time.sleep(0.02)
print("PEAK_RSS_KB=%d" % (peak // 1024))
sys.exit(process.returncode)
"""

MEASURE = ("import resource, subprocess, sys;"
           "code = subprocess.run(sys.argv[1:]).returncode;"
           "peak = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss;"
           # ru_maxrss is in bytes on macOS and in KiB on Linux.
           "print('PEAK_RSS_KB=%d' % (peak // 1024 if sys.platform == 'darwin' else peak));"
           "sys.exit(code)")

# The line JavaScriptCore prints for each function the Baseline JIT compiles when
# JSC_reportBaselineCompileTimes=true.
BASELINE_COMPILE_REPORT = re.compile(r"Baseline", re.IGNORECASE)


def measured(command, env) -> dict:
    script = MEASURE_WINDOWS if sys.platform == "win32" else MEASURE
    completed = run([sys.executable, "-c", script] + [str(part) for part in command],
                    env=env, check=False, capture=True)
    print(completed.stdout, end="")
    print(completed.stderr, end="", file=sys.stderr)
    peak = 0
    for line in completed.stdout.splitlines():
        if line.startswith("PEAK_RSS_KB="):
            peak = int(line.split("=", 1)[1])
    return {"returncode": completed.returncode, "stdout": completed.stdout,
            "stderr": completed.stderr, "peak_rss_kb": peak,
            "compile_reports": sum(1 for line in (completed.stdout + completed.stderr).splitlines()
                                   if BASELINE_COMPILE_REPORT.search(line) and "jsc_" not in line)}


def script_result(run_result: dict, marker: str) -> dict:
    return {"ok": run_result["returncode"] == 0 and marker in run_result["stdout"],
            "peak_rss_kb": run_result["peak_rss_kb"],
            "within_rss_bound": 0 < run_result["peak_rss_kb"] <= ACCEPTANCE_MAX_RSS_KB,
            "baseline_compile_reports": run_result["compile_reports"]}


def program_ok(env, command, marker) -> dict:
    completed = run(command, env=env, check=False, capture=True)
    print(completed.stdout, end="")
    print(completed.stderr, end="", file=sys.stderr)
    return {"ok": completed.returncode == 0 and marker in completed.stdout}


def run_acceptance(mode: str, build: Path, run_env: dict, programs: dict) -> dict:
    results = {}
    jsc = build / "bin" / "jsc"
    cycle = programs["jsc_context_cycle"]
    if mode == "interpreter":
        results["script"] = script_result(measured([jsc, TESTS / "jsc_acceptance.js"], run_env),
                                          "jsc_acceptance: ok")
        for threads in CYCLE_THREAD_COUNTS:
            results[f"context_cycle_threads_{threads}"] = program_ok(
                run_env, [cycle, str(threads)], "jsc_context_cycle: ok")
    else:
        off_env = {**run_env, **JIT_OFF_OPTIONS}
        on_env = {**run_env, **JIT_ON_OPTIONS}
        # Step 1: the assembly interpreter alone. The whole 1B acceptance must pass and nothing
        # may be compiled.
        off = script_result(measured([jsc, TESTS / "jsc_acceptance.js"], off_env), "jsc_acceptance: ok")
        off["no_code_compiled"] = off["baseline_compile_reports"] == 0
        results["step1_assembly_interpreter_script"] = off
        # Step 2: the Baseline JIT. The compile report is the proof that generated code ran.
        jit = script_result(measured([jsc, TESTS / "jsc_jit_acceptance.js"], on_env), "jsc_jit_acceptance: ok")
        jit["compiled_code_proven"] = jit["baseline_compile_reports"] > 0
        results["step2_jit_script"] = jit
        again = script_result(measured([jsc, TESTS / "jsc_acceptance.js"], on_env), "jsc_acceptance: ok")
        results["step2_1b_script_with_jit"] = again
        for threads in CYCLE_THREAD_COUNTS:
            results[f"step2_context_cycle_threads_{threads}"] = program_ok(
                on_env, [cycle, str(threads)], "jsc_context_cycle: ok")
        # The watchdog interrupts an infinite loop with a signal; the loop must have been
        # compiled (the report names `spin`, the function the loop runs in), or the test would
        # only be exercising the interpreter's trap.
        watchdog = measured([programs["jsc_watchdog_test"]], on_env)
        compiled_spin = any("spin" in line and BASELINE_COMPILE_REPORT.search(line)
                            for line in (watchdog["stdout"] + watchdog["stderr"]).splitlines())
        results["step2_watchdog_signal_vm_traps"] = {
            "ok": watchdog["returncode"] == 0 and "jsc_watchdog_test: ok" in watchdog["stdout"],
            "spin_function_compiled": compiled_spin}
    results["passed"] = all(
        value["ok"] and value.get("within_rss_bound", True) and value.get("no_code_compiled", True)
        and value.get("compiled_code_proven", True) and value.get("spin_function_compiled", True)
        for value in results.values() if isinstance(value, dict))
    return results


# A shared library the process may load that is NOT a CRT artifact. Each entry is a known gap,
# listed so the audit stays honest instead of being loosened (see docs/crtweb_acceptance.md).
KNOWN_HOST_LIBRARIES: dict[str, str] = {}


def jsc_library(build: Path, target_os: str) -> Path:
    """The built JavaScriptCore shared library (its file name differs per host)."""
    if target_os == "windows":
        for directory in (build / "bin", build / "lib"):
            found = sorted(directory.glob("*JavaScriptCore*.dll"))
            if found:
                return found[0]
        raise SystemExit(f"JavaScriptCore.dll not found under {build / 'bin'} or {build / 'lib'}")
    patterns = ("libJavaScriptCore*.dylib*",) if target_os == "macos" else ("libJavaScriptCore.so.*", "libJavaScriptCore.so")
    for pattern in patterns:
        found = sorted(path for path in (build / "lib").glob(pattern) if not path.is_symlink())
        if found:
            return found[0]
    raise SystemExit(f"libJavaScriptCore not found under {build / 'lib'}")


# macOS: the only system library the CRT runtime itself loads. libc.dylib sits on top of
# libSystem (the Mach kernel boundary), so a CRT process always has it; anything else under
# /usr/lib or /System (frameworks, libc++, libobjc, ICU) is a host ABI leak.
MACOS_ALLOWED_SYSTEM_LIBRARIES = {"/usr/lib/libSystem.B.dylib"}


def host_abi_audit_macos(sdk: Path, deps: Path, build: Path, binaries) -> dict:
    """Mach-O twin of the Linux audit, from `otool -L` on every binary and the libraries they
    load: each dependency must be an @rpath/@loader_path name that resolves inside the CRT SDK,
    the CRT-built ICU or this build tree, or libSystem."""
    allowed = [sdk.resolve(), deps.resolve(), build.resolve()]
    search = [build / "lib", deps / "lib", sdk / "lib"]
    violations, inventory, pending, seen = [], {}, [Path(b) for b in binaries], set()
    while pending:
        binary = pending.pop()
        if binary in seen:
            continue
        seen.add(binary)
        completed = run(["otool", "-L", binary], capture=True, check=False)
        libraries = []
        for line in completed.stdout.splitlines()[1:]:
            name = line.strip().split(" (")[0]
            if not name:
                continue
            libraries.append(Path(name).name)
            if name in MACOS_ALLOWED_SYSTEM_LIBRARIES:
                continue
            if name.startswith(("@rpath/", "@loader_path/", "@executable_path/")):
                leaf = Path(name).name
                resolved = next((directory / leaf for directory in search if (directory / leaf).exists()), None)
                if resolved is None:
                    violations.append(f"{binary.name}: {name} not found in the CRT SDK, ICU or build tree")
                else:
                    pending.append(resolved.resolve())
                continue
            path = Path(name).resolve()
            if any(path.is_relative_to(prefix) for prefix in allowed):
                pending.append(path)
                continue
            violations.append(f"{binary.name}: {name} is a host library")
        inventory[binary.name] = sorted(set(libraries))
    return {"ok": not violations, "violations": violations, "known_host_libraries": {},
            "libraries": inventory}


def windows_system_dlls() -> set:
    """The OS DLLs a CRT process may import: the canonical Windows runtime contract of the
    distribution (tools/crt_dist_prerequisites.py), lower-cased; the api-ms-win-* API-set
    family is allowed by prefix at the call site."""
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from crt_dist_prerequisites import external_prerequisites_for
    names = set()
    for item in external_prerequisites_for("windows", "02-cxx"):
        if item.get("kind") == "os-runtime":
            names.update(name.lower() for name in item["components"])
    return names


def host_abi_audit_windows(sdk: Path, deps: Path, build: Path, binaries) -> dict:
    """PE twin of the Linux audit, from `llvm-objdump -p` on every binary and the DLLs they
    import: each import must resolve to a DLL inside the CRT SDK, the CRT-built ICU or this
    build tree, or be an OS runtime DLL of the distribution's own Windows contract (KERNEL32 and
    the api-ms-win-core API sets). The Microsoft C runtime (ucrtbase, vcruntime, msvcp) or any
    other host DLL would mean a native build that merely ran."""
    import shutil
    objdump = shutil.which("llvm-objdump") or shutil.which("llvm-objdump.exe")
    if objdump is None:
        raise SystemExit("llvm-objdump is required for the Windows host ABI audit")
    system = windows_system_dlls()
    search = [build / "bin", build / "lib", deps / "bin", deps / "lib", sdk / "bin", sdk / "lib"]
    violations, inventory, pending, seen = [], {}, [Path(b) for b in binaries], set()
    while pending:
        binary = pending.pop()
        if binary in seen:
            continue
        seen.add(binary)
        completed = run([objdump, "-p", binary], capture=True, check=False)
        imports = [line.split("DLL Name:", 1)[1].strip() for line in completed.stdout.splitlines()
                   if "DLL Name:" in line]
        for name in imports:
            lowered = name.lower()
            resolved = next((directory / name for directory in search if (directory / name).is_file()), None)
            if resolved is not None:
                pending.append(resolved.resolve())
            elif lowered in system or lowered.startswith("api-ms-win-"):
                continue
            else:
                violations.append(f"{binary.name}: imports {name}, which is neither a CRT artifact nor "
                                  f"an OS runtime DLL of the distribution contract")
        inventory[binary.name] = sorted(set(imports))
    return {"ok": not violations, "violations": violations, "known_host_libraries": {},
            "libraries": inventory}


def host_abi_audit(sdk: Path, deps: Path, build: Path, binaries, run_env: dict, target_os: str = "linux") -> dict:
    """The Host ABI firewall for the JavaScriptCore bring-up (Linux): every shared object the
    binaries load must come from the CRT SDK, the CRT-built ICU or this build tree. A native
    libc, a host ICU, libstdc++ or any other host library would mean the result is a native
    JavaScriptCore port that merely ran, not one running on the CRT runtime. The dynamic
    loader itself (the host's, by design until CRT owns one) and the vDSO are allowed."""
    if target_os == "macos":
        return host_abi_audit_macos(sdk, deps, build, binaries)
    if target_os == "windows":
        return host_abi_audit_windows(sdk, deps, build, binaries)
    allowed = [sdk.resolve(), deps.resolve(), build.resolve()]
    violations, known, inventory = [], {}, {}
    for binary in binaries:
        completed = run(["ldd", binary], env=run_env, capture=True, check=False)
        libraries = []
        for line in completed.stdout.splitlines():
            line = line.strip()
            if not line or line.startswith("linux-vdso"):
                continue
            name, _, rest = line.partition(" => ")
            resolved = rest.split(" (")[0].strip() if rest else name.split(" (")[0].strip()
            if resolved == "not found":
                violations.append(f"{Path(binary).name}: {name} not found")
                continue
            libraries.append(Path(resolved).name)
            path = Path(resolved).resolve()
            if "ld-linux" in path.name or path.name.startswith("ld-"):
                continue
            if any(path.is_relative_to(prefix) for prefix in allowed):
                continue
            if path.name in KNOWN_HOST_LIBRARIES:
                known[path.name] = KNOWN_HOST_LIBRARIES[path.name]
                continue
            violations.append(f"{Path(binary).name}: {name} resolves to {path}")
        inventory[Path(binary).name] = sorted(set(libraries))
    return {"ok": not violations, "violations": violations, "known_host_libraries": known,
            "libraries": inventory}


def write_fingerprint(work: Path, recipe: Path, sdk: Path, deps: Path, tools: dict, arch: str, mode: str):
    """What this configuration was built from, so a later success or failure can be tied to
    exactly these inputs."""
    import hashlib
    recipe_data = json.loads(recipe.read_text(encoding="utf-8"))
    manifest = sdk / "manifest.json"
    fingerprint = {
        "webkit": {"version": recipe_data["version"],
                   "archive_sha256": recipe_data["source"]["archive_sha256"],
                   "expected_commit": recipe_data["source"]["expected_commit"]},
        "sdk": {"root": str(sdk), "manifest_sha256": hashlib.sha256(manifest.read_bytes()).hexdigest(),
                "arch": arch},
        "icu_prefix": str(deps),
        "icu_recipe_sha256": hashlib.sha256((ROOT / "porting/recipes/icu.json").read_bytes()).hexdigest(),
        "host_tools": tools,
        "configuration": {"port": "JSCOnly", "mode": mode, "jit": mode == "baseline-jit",
                          "c_loop": mode == "interpreter", "event_loop": "Generic",
                          "options": [option for option, _ in configure_options(mode)],
                          "jit_run_options": JIT_ON_OPTIONS if mode == "baseline-jit" else {}},
    }
    (work / f"webkit-jsc-config-{mode}.json").write_text(json.dumps(fingerprint, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--recipe", type=Path, default=ROOT / "libcrtweb/third_party/webkit/recipe.json")
    parser.add_argument("--sdk-root", type=Path, required=True,
                        help="installed CRT SDK (02-cxx or later: libc++ is required)")
    parser.add_argument("--deps-prefix", type=Path, required=True,
                        help="install prefix holding the CRT ICU port")
    parser.add_argument("--work-root", type=Path, required=True)
    parser.add_argument("--cache", type=Path, help="download cache (default: <work-root>/cache)")
    parser.add_argument("--mode", choices=sorted(MODE_OPTIONS), default="interpreter",
                        help="interpreter (1B, default) or baseline-jit (1C); each mode has its own build tree")
    parser.add_argument("--skip-build", action="store_true",
                        help="reuse an existing build tree and only run the acceptance")
    args = parser.parse_args()

    sdk = args.sdk_root.resolve()
    deps = args.deps_prefix.resolve()
    work = args.work_root.resolve()
    cache = (args.cache or work / "cache").resolve()
    work.mkdir(parents=True, exist_ok=True)
    cache.mkdir(parents=True, exist_ok=True)
    mode = args.mode
    build = work / f"build-{mode}"

    target_os, arch = sdk_target(sdk)
    if target_os not in ("linux", "macos", "windows"):
        raise SystemExit(f"JSC bring-up supports Linux, macOS and Windows SDKs (SDK target is {target_os!r})")
    if not (sdk / "include" / "c++" / "v1").is_dir():
        raise SystemExit(f"{sdk} has no libc++ headers: use a 02-cxx or later SDK")
    needed = deps / "include" / "unicode" / "utypes.h"
    if not needed.is_file():
        raise SystemExit(f"{needed} not found: build the icu port recipe into --deps-prefix")

    phases = Phases()
    try:
        with phases.measure("check host build tools"):
            host_tools = require_host_tools()
            print(json.dumps(host_tools))
            write_fingerprint(work, args.recipe, sdk, deps, host_tools, arch, mode)
        env = build_environment(sdk, deps, target_os)
        if not args.skip_build:
            with phases.measure("fetch, verify and extract WebKit"):
                tree = fetch_and_extract(args.recipe, cache, work)
            with phases.measure("apply carried patches"):
                carried_patches = apply_patches(tree, target_os)
                (work / "carried-patches.json").write_text(json.dumps(carried_patches, indent=2) + "\n",
                                                          encoding="utf-8")
            with phases.measure(f"configure JavaScriptCore (JSCOnly, {mode})"):
                configure(tree, build, sdk, deps, env, target_os, arch, mode)
            with phases.measure("build jsc"):
                run(["ninja", "-C", build, "jsc"], env=env)
        run_env = runtime_library_path(sdk, deps, build, target_os, env)
        with phases.measure("build the C API test programs"):
            programs = build_test_programs(sdk, build, env, target_os, mode)
        with phases.measure(f"run the {mode} acceptance"):
            results = run_acceptance(mode, build, run_env, programs)
        with phases.measure("host ABI audit"):
            audit = host_abi_audit(
                sdk, deps, build,
                [build / "bin" / "jsc", jsc_library(build, target_os), *programs.values()], run_env, target_os)
            print(json.dumps(audit, indent=2))
            results["host_abi"] = {"ok": audit["ok"], **audit}
        (work / f"acceptance-{mode}.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
        results["passed"] = results["passed"] and results["host_abi"]["ok"]
        if not results["passed"]:
            raise SystemExit("JSC acceptance FAILED: " + json.dumps(results))
    finally:
        phases.report()
    print("JSC acceptance passed:", json.dumps(results))
    return 0


if __name__ == "__main__":
    sys.exit(main())
