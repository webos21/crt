#!/usr/bin/env python3
"""Build JavaScriptCore (WebKit's JSCOnly port) with the CRT toolchain and run the CRT
acceptance (Web Tranche 1, docs/crtweb_acceptance.md).

The build is driven the way an external consumer would drive it: the pinned WPE WebKit
tarball (libcrtweb/third_party/webkit/recipe.json, verified by tools/fetch_webkit.py) is
configured with an installed SDK's crt-toolchain.cmake -- the CRT compiler wrappers and
sysroot -- never with host clang and host headers. ICU and gperf come from CRT port
recipes (porting/recipes/icu.json, gperf.json) installed under --deps-prefix; Ruby is the
one host build tool, because JavaScriptCore generates its interpreter with Ruby scripts.

Steps: check host tools -> fetch and verify -> extract -> configure -> build `jsc` -> build
the lifecycle test program -> run the acceptance (a JavaScript script through the `jsc`
shell, and a C API program that creates, uses and releases JS contexts 150 times and on
several threads while watching resident memory).

First-green configuration (Web Tranche 1B): interpreter only (the C_LOOP interpreter, JIT,
FTL and WebAssembly off), the Generic event loop (no GLib). Tranche 1C turns the JIT on.
Nothing in the WebKit tree is patched; every deviation is a CMake option or a compiler
flag, listed in CONFIGURE_OPTIONS below with the reason.
"""

import argparse
import json
import os
import resource
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


def require_host_tools() -> str:
    missing = [tool for tool in ("cmake", "ninja", "perl", "python3") if shutil.which(tool) is None]
    ruby = shutil.which("ruby")
    if ruby is None:
        missing.append("ruby (>= 2.5: JavaScriptCore generates its interpreter with Ruby scripts)")
    if missing:
        raise SystemExit("missing host build tools: " + ", ".join(missing))
    version = run([ruby, "-e", "print RUBY_VERSION"], capture=True).stdout.strip()
    major, minor = (int(part) for part in version.split(".")[:2])
    if (major, minor) < (2, 5):
        raise SystemExit(f"ruby {version} found, 2.5 or newer is required")
    return version


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


def build_environment(sdk: Path, deps: Path, target_os: str) -> dict:
    env = dict(os.environ)
    # Shared runtime linkage: ONE libc/libc++ in the process. The shared libraries
    # (libJavaScriptCore.so, ICU) use libc.so; an executable linking libc.a would carry a
    # second libc with its own thread/key/TLS registry and allocator.
    env["CRT_CXX_RUNTIME_LINKAGE"] = "shared"
    # WebKit's `jsc` and its tools define a plain `int main`; see tools/crt-c++.
    env["CRT_CXX_HOSTED"] = "1"
    env["PATH"] = f"{deps / 'bin'}{os.pathsep}{env['PATH']}"
    return env


CONFIGURE_OPTIONS = [
    ("-DPORT=JSCOnly", "JavaScriptCore alone (no WebCore/WebKit)"),
    ("-DCMAKE_BUILD_TYPE=Release", ""),
    ("-DUSE_HEADER_MAPS=OFF", "the release tarball omits Tools/Scripts/hmaptool, which "
                              "header maps need; an upstream option, no source change"),
    ("-DENABLE_JIT=OFF", "Tranche 1B interpreter first-green; 1C enables the JIT"),
    ("-DENABLE_C_LOOP=ON", "the portable C++ interpreter (no assembly LLInt, no JIT)"),
    ("-DENABLE_FTL_JIT=OFF", ""),
    ("-DENABLE_WEBASSEMBLY=OFF", "needs the JIT tiers"),
    ("-DENABLE_SAMPLING_PROFILER=OFF", "needs signal-based thread suspension"),
    ("-DENABLE_API_TESTS=OFF", "upstream disables them on Windows; CRT has its own acceptance"),
    ("-DDEVELOPER_MODE=OFF", ""),
]


def configure(tree: Path, build: Path, sdk: Path, deps: Path, env: dict, target_os: str, arch: str):
    toolchain = sdk / "crt-toolchain.cmake"
    if not toolchain.is_file():
        raise SystemExit(f"{toolchain} not found: --sdk-root must be an installed CRT SDK")
    c_flags = []
    if arch in ("x86_64", "amd64"):
        # libpas/bmalloc use 16-byte atomics; without cmpxchg16b the compiler emits calls to
        # __atomic_*_16 (libatomic), which the CRT toolchain does not provide.
        c_flags.append("-mcx16")
    link_flags = []
    if target_os == "linux":
        # Native TLS in a shared library: the initial-exec model avoids __tls_get_addr, which
        # only the (host) dynamic loader defines.
        c_flags.append("-ftls-model=initial-exec")
        # The shared libraries reference loader-provided symbols (__tls_get_addr from ICU and
        # libc++abi); the executable link must not insist on resolving them at link time.
        link_flags.append("-Wl,--allow-shlib-undefined")
    if build.exists():
        shutil.rmtree(build)
    command = ["cmake", "-S", tree, "-B", build, "-G", "Ninja",
               f"-DCMAKE_TOOLCHAIN_FILE={toolchain}", f"-DICU_ROOT={deps}"]
    command += [option for option, _ in CONFIGURE_OPTIONS]
    command += [f"-DCMAKE_C_FLAGS={' '.join(c_flags)}", f"-DCMAKE_CXX_FLAGS={' '.join(c_flags)}"]
    if link_flags:
        command += [f"-DCMAKE_EXE_LINKER_FLAGS={' '.join(link_flags)}",
                    f"-DCMAKE_SHARED_LINKER_FLAGS={' '.join(link_flags)}"]
    run(command, env=env)


def runtime_library_path(sdk: Path, deps: Path, build: Path, target_os: str, env: dict) -> dict:
    run_env = dict(env)
    paths = [build / "lib", deps / "lib", sdk / "lib"]
    variable = "DYLD_LIBRARY_PATH" if target_os == "macos" else "LD_LIBRARY_PATH"
    existing = run_env.get(variable)
    run_env[variable] = os.pathsep.join(str(path) for path in paths) + (os.pathsep + existing if existing else "")
    return run_env


def build_cycle_program(sdk: Path, build: Path, env: dict, target_os: str) -> Path:
    output = build / "bin" / "jsc_context_cycle"
    compile_env = dict(env)
    compile_env["CRT_SYSROOT"] = str(sdk)
    if target_os == "linux":
        compile_env["CRT_TARGET_OS"] = "linux"
    command = [sdk / "tools" / "crt-c++", f"-I{build / 'JavaScriptCore' / 'Headers'}",
               TESTS / "jsc_context_cycle.cpp", "-o", output, f"-L{build / 'lib'}",
               "-lJavaScriptCore", f"-Wl,-rpath,{build / 'lib'}"]
    if target_os == "linux":
        command.append("-Wl,--allow-shlib-undefined")
    run(command, env=compile_env)
    return output


def run_acceptance(build: Path, run_env: dict, cycle_program: Path) -> dict:
    results = {}
    jsc = build / "bin" / "jsc"
    # Peak resident memory of the script run alone: a fresh Python process runs the shell
    # as its only child and reports RUSAGE_CHILDREN (in this process it would be the
    # largest of every child ever waited for, the build's compilers included).
    measure = ("import resource, subprocess, sys;"
               "code = subprocess.run(sys.argv[1:]).returncode;"
               "print('PEAK_RSS_KB=%d' % resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss);"
               "sys.exit(code)")
    completed = run([sys.executable, "-c", measure, jsc, TESTS / "jsc_acceptance.js"],
                    env=run_env, check=False, capture=True)
    print(completed.stdout, end="")
    print(completed.stderr, end="", file=sys.stderr)
    peak = 0
    for line in completed.stdout.splitlines():
        if line.startswith("PEAK_RSS_KB="):
            peak = int(line.split("=", 1)[1])
    ok = completed.returncode == 0 and "jsc_acceptance: ok" in completed.stdout
    results["script"] = {"ok": ok, "peak_rss_kb": peak,
                         "within_rss_bound": 0 < peak <= ACCEPTANCE_MAX_RSS_KB}
    for threads in CYCLE_THREAD_COUNTS:
        completed = run([cycle_program, str(threads)], env=run_env, check=False, capture=True)
        print(completed.stdout, end="")
        print(completed.stderr, end="", file=sys.stderr)
        results[f"context_cycle_threads_{threads}"] = {
            "ok": completed.returncode == 0 and "jsc_context_cycle: ok" in completed.stdout}
    results["passed"] = all(
        (value["ok"] and value.get("within_rss_bound", True)) for value in results.values()
        if isinstance(value, dict))
    return results


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--recipe", type=Path, default=ROOT / "libcrtweb/third_party/webkit/recipe.json")
    parser.add_argument("--sdk-root", type=Path, required=True,
                        help="installed CRT SDK (02-cxx or later: libc++ is required)")
    parser.add_argument("--deps-prefix", type=Path, required=True,
                        help="install prefix holding the CRT ICU and gperf ports")
    parser.add_argument("--work-root", type=Path, required=True)
    parser.add_argument("--cache", type=Path, help="download cache (default: <work-root>/cache)")
    parser.add_argument("--skip-build", action="store_true",
                        help="reuse an existing build tree and only run the acceptance")
    args = parser.parse_args()

    sdk = args.sdk_root.resolve()
    deps = args.deps_prefix.resolve()
    work = args.work_root.resolve()
    cache = (args.cache or work / "cache").resolve()
    work.mkdir(parents=True, exist_ok=True)
    cache.mkdir(parents=True, exist_ok=True)
    build = work / "build-jsc"

    target_os, arch = sdk_target(sdk)
    if target_os != "linux":
        raise SystemExit(f"JSC bring-up is verified on Linux only so far (SDK target is {target_os!r}); "
                         "the Windows and macOS replays are separate Tranche 1 steps")
    if not (sdk / "include" / "c++" / "v1").is_dir():
        raise SystemExit(f"{sdk} has no libc++ headers: use a 02-cxx or later SDK")
    for needed in (deps / "include" / "unicode" / "utypes.h", deps / "bin" / "gperf"):
        if not needed.is_file():
            raise SystemExit(f"{needed} not found: build the icu and gperf port recipes into --deps-prefix")

    phases = Phases()
    try:
        with phases.measure("check host build tools"):
            print("ruby", require_host_tools())
        env = build_environment(sdk, deps, target_os)
        if not args.skip_build:
            with phases.measure("fetch, verify and extract WebKit"):
                tree = fetch_and_extract(args.recipe, cache, work)
            with phases.measure("configure JavaScriptCore (JSCOnly)"):
                configure(tree, build, sdk, deps, env, target_os, arch)
            with phases.measure("build jsc"):
                run(["ninja", "-C", build, "jsc"], env=env)
        run_env = runtime_library_path(sdk, deps, build, target_os, env)
        with phases.measure("build the context lifecycle program"):
            cycle_program = build_cycle_program(sdk, build, env, target_os)
        with phases.measure("run the acceptance"):
            results = run_acceptance(build, run_env, cycle_program)
        (work / "acceptance.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
        if not results["passed"]:
            raise SystemExit("JSC acceptance FAILED: " + json.dumps(results))
    finally:
        phases.report()
    print("JSC acceptance passed:", json.dumps(results))
    return 0


if __name__ == "__main__":
    sys.exit(main())
