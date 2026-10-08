# crtweb Acceptance (Stage `06-web`)

**Status: in progress -- Tranche 0 (scope, version and license freeze) is closed;
Tranche 1 (JavaScriptCore bring-up) is in progress: 1A, 1B and 1C (Baseline JIT) are green on Linux/x86_64, Linux/aarch64, macOS/arm64 and Windows/x64; 1D-A (DFG + concurrent JIT) is green on all four hosts; 1D-B (FTL) and 1D-C (WebAssembly) are green on Linux/x86_64 and Windows/x64; 1D-D (sampling profiler) is green on Linux/x86_64 and Windows/x64. macOS/arm64 and Linux/aarch64 1D-B/C replays plus their 1D-D replays remain. W^X is separate hardening. No WebCore, WebKit or `PlatformCRT` code is built yet.** Detailed contract and tranche order for the
WebKit-based web runtime. It depends on `05-ui`'s External Surface contract
([`crtui_acceptance.md`](crtui_acceptance.md)), which is accepted on all three
hosts (2026-10-03). Upstream mapping lives in [`crtweb_porting.md`](crtweb_porting.md). Evidence is recorded
here as each tranche closes; completed work goes to `../HISTORY.md` and the
current tranche state to `../TODO.md`.

## Purpose

Build a **WebKit CRT Port** (`PlatformCRT`) -- not a straight port of WPE to
three operating systems. WPE WebKit is the reference implementation to compare
against and to borrow structure from; WebKit already has separate ports (Apple,
GTK/WPE, PlayStation, Windows, JSCOnly), so a CRT port fits its architecture.

```text
                 WebKit
  +--------------------------------------+
  | WTF  JavaScriptCore  WebCore  WebKit |
  | WebProcess  NetworkProcess  GPUProcess |
  +------------------+-------------------+
                     |
                PlatformCRT
                     |
          +----------+----------+
          |          |          |
       CRT PAL     crtgfx    crtmedia
          |          |          |
          +------ CRT network --+
```

Application code sees only `libcrtweb` and the `crtui` WebView; no WebKit, WPE,
or GLib type appears in a public header.

## Scope

Required for v1: HTML, CSS, DOM, JavaScript, navigation, HTTP/HTTPS, images,
fonts/text, mouse/keyboard, scrolling, Canvas 2D, basic cookies/storage.

Deferred: WebRTC, WebGPU, WebXR, EME/DRM, camera/microphone, printing,
extensions, full accessibility, downloads.

**Version and licensing.** Frozen reference: WPE WebKit 2.54.0 (the stable
series, where WPEPlatform is the stable embedding API), pinned in Tranche 0 with
its revision, source provenance, LGPL/BSD notices, and source correspondence. WebKit ports own their release and security cadence, so a
security-update policy is a deliverable, not an afterthought.

## Tranches and gates

### 0. Scope, version, and license freeze

Pin the reference release and write `crtweb_porting.md`'s upstream mapping.
Decide the SBOM/CVE-tracking policy now.

**Status (2026-10-03): closed.** The reference is pinned in
`libcrtweb/third_party/webkit/recipe.json` (provenance in `libcrtweb/third_party/webkit/README.md`) and
verified: the archive was downloaded and its SHA-256 recomputed
(`efa9bcc3...eb452`, 46,202,080 bytes, equal to the release page); the signed tag
`wpewebkit-2.54.0` verifies with `gpg` (signer Adrian Perez de Castro, key
`5AA3BC33...123B`, not web-of-trust certified) and points at commit `73f39d84...`;
six sampled source files are byte-identical to that commit; and the 64 license and
notice files of the archive are inventoried in `libcrtweb/third_party/webkit/license-inventory.json`
(`tools/scan_webkit_licenses.py`). The upstream mapping is in `crtweb_porting.md`
and the security policy below was confirmed by the owner. Still unverified, and
carried as explicit items in the recipe: per-file license headers, tarball-equals-
commit beyond the sampled files, and upstream's security support window.

**Frozen scope and version.** Reference: WPE WebKit **2.54.0** (2026-09-16), the
current stable series. CRT targets the stable WPEPlatform API and does not use the
deprecated libwpe-based API (`ENABLE_WPE_LEGACY_API=OFF`). Build system: CMake with
Ninja (upstream now requires it). 2D rendering is Skia only; threaded painting is
mandatory upstream. The v1 scope above is unchanged.

**Licensing.** WebKit is licensed per file and bundles third-party components. The
archive carries LGPL-2 and LGPL-2.1 texts (WebCore, JavaScriptCore's `COPYING.LIB`),
an Apple BSD-style 2-clause notice, and license files for bundled Skia (BSD-3-style,
milestone 154), ANGLE, pdf.js, gtest and others (inventory above). Every `06-web` SDK
must carry the notices, the exact source revision and archive hash, and enough
information for source correspondence (the pinned recipe plus the carried-patch
list). Because parts are LGPL, the proposal is to keep WebKit as separately built,
separately replaceable libraries rather than merging it into application binaries;
that is confirmed or changed with the packaging decisions in Tranche 10.

**Security and update policy (confirmed by the owner 2026-10-03).**
1. Upstream publishes WebKitGTK/WPE advisories (`WSA-YYYY-NNNN`, the 2026 ones
   dated 2026-03-18, 03-28, 06-02, 07-10 and 09-29 at the time of writing) and
   fixes them in the current stable series. CRT pins one stable series and moves
   to the newest micro release of it.
2. Every new advisory is checked against the pin by version ("affected: before
   X"); an affected pin is bumped and re-verified (`tools/fetch_webkit.py`) before
   any further `06-web` release asset is published. A bump is a recipe change plus
   a re-run of the stage gates, never an in-place edit of a built tree.
3. The advisory feed (`https://wpewebkit.org/security/`) and the `webkit-wpe`
   mailing list are the tracked sources; a release asset records the WebKit
   version and the newest advisory it was checked against.
4. An SBOM (package, version, license, source URL, hash) is generated with each
   `06-web` SDK and declared in its manifest, like the other redistributed
   dependencies; the SBOM format is chosen at Tranche 10.
5. When the pinned series stops receiving upstream fixes, moving to the next
   stable series is a planned tranche item, not a silent bump. The length of
   upstream's support window is not documented on the pages consulted and must
   be confirmed before the first public `06-web` release.

### 1. JavaScriptCore / JSCOnly bring-up

Prerequisite bring-up, not an application scripting layer. Linux first, then a
quick replay on Windows and macOS, because it surfaces threads, TLS, virtual
and executable memory, signals/exceptions, GC, and JIT problems in the lower
runtime early. Upstream's JSCOnly port is not stand-alone: it needs Threads and
ICU >= 70.1, and its default `Generic` event loop needs no GLib. Upstream also
disables the JSCOnly API tests on Windows, so CRT keeps its own acceptance.

**Build boundary.** Tranche 1 builds against the installed SDK, never host clang
plus the WebKit tree: `tools/build_webkit_jsc.py` (fetch, extract, configure with
the CRT compiler wrappers and the installed `05-ui` sysroot, build `jsc`, run the
CRT acceptance). Tranche 10 grows it into `tools/build_stage_06_web.py` (full
WebKit, `libcrtweb`, packaging, `verify_dist.py`).

**Build-host tools versus target dependencies.** The two are different things and are
kept apart from the start.

- *Build-host tools* run on the build machine, whatever the target, and are not CRT
  artifacts: CMake, Ninja, Perl >= 5.10 with the `English`, `FindBin` and `JSON::PP`
  modules, Python, and Ruby >= 2.5 (all required by the pinned
  `Source/cmake/WebKitCommon.cmake` for every port; JavaScriptCore generates its
  interpreter with Ruby and Perl scripts). `gperf` joins them only when WebCore is
  enabled (`if (ENABLE_WEBCORE) find_package(Gperf ...)`), so it is a WebCore
  prerequisite, not a JSCOnly one: its Linux port is done and kept, and its
  Windows/macOS replay does not block 1B.
- *Target dependencies* are built against the CRT target sysroot and shipped: CRT
  pthread/libc/libc++, and ICU (`data`, `uc`, `i18n`, >= 70.1) from the CRT port
  `porting/recipes/icu.json`, never the host ICU (fast on Linux, wrong on
  Windows/macOS).

Today host and target are the same machine and architecture, so the ports are
ordinary CRT executables that run during the build. That does not survive a cross
build (an x86_64 build host producing a Linux/aarch64 SDK cannot run an aarch64
`gperf` or ICU's data generators), so before `06-web` becomes the embedded product
stage the build must separate the two: build tools built for, and run on, the build
machine; target libraries built for the target. Not a blocker for the native
three-host acceptance; recorded in `TODO.md`.

**1A -- dependency inventory and ports.** The build-host tool inventory above
(checked, with versions, by `tools/build_webkit_jsc.py`) and the ICU target port,
which passes its own recipe test before JSC is attempted. The gperf port exists for
WebCore.

**Configure fingerprint.** Each harness run writes `webkit-jsc-config.json`: the
WebKit version, archive hash and expected commit, the SDK manifest hash and
architecture, the ICU prefix and recipe hash, the build-host tool versions, and the
configuration (port, JIT, interpreter, event loop, every CMake option). A later
success or failure can then be tied to exactly the inputs that produced it.

**Host ABI firewall.** CRT's aim is that JavaScriptCore runs on the CRT runtime, not
that a native JavaScriptCore links. Every 1B acceptance therefore audits what the
binaries load: only CRT SDK libraries, the CRT-built ICU and the build tree are
allowed, plus the loader itself. Rejected: a native libc/libstdc++, a host ICU, any
undeclared host library, and (Windows/macOS) an undeclared Win32 or Cocoa dependency.
Linux checks the resolved `ldd` set (`host_abi_audit()` in `tools/build_webkit_jsc.py`,
no exceptions); the Windows replay uses `llvm-readobj`/`dumpbin` imports and the macOS
replay `otool -L`.

**1B -- interpreter first-green.** JSCOnly, `EVENT_LOOP_TYPE=Generic`, JIT off.
Acceptance: arithmetic, objects/arrays, JSON, RegExp, Promise/microtasks,
UTF-8/Unicode (ICU), exceptions, a GC stress run, and repeated runtime
create/destroy with a leak check. Linux, then Windows and macOS.

**1C -- Baseline JIT (refined 2026-10-04).** One new runtime assumption at a time, so a
failure can be attributed. 1C is the **Baseline JIT only**; DFG, FTL, concurrent compiler
threads, WebAssembly and the sampling profiler are later steps (1D and after), and W^X
policy is a hardening follow-up (below).

*Why not just `ENABLE_JIT=ON`.* Checked against the pinned `Source/cmake/WebKitFeatures.cmake`:
`ENABLE_JIT` conflicts with `ENABLE_C_LOOP`; with the JIT on, `ENABLE_DFG_JIT` defaults to ON
(`ENABLE_FTL_JIT` depends on DFG, WebAssembly's BBQ/OMG tiers on FTL); WebAssembly and the
sampling profiler conflict with `C_LOOP`. The harness therefore has `--mode
interpreter|baseline-jit`. **The planned "Baseline-only build" does not compile**, which the
first 1C build found: with DFG or WebAssembly compiled out, `bytecode/InlineCacheCompiler.h`
uses `CCallHelpers::Jump` but only the DFG/WebAssembly headers happen to include
`CCallHelpers.h` before it (`LLIntOffsetsExtractor.cpp` fails), and the WebAssembly sources
need B3, which exists only with FTL. Upstream builds only the full default tier set, and the
header-order dependency cannot be patched here. So the 1C build compiles every tier in and the
Baseline JIT is isolated **at run time**:

| | 1B `interpreter` build | 1C `baseline-jit` build |
| --- | --- | --- |
| `ENABLE_JIT` / `ENABLE_C_LOOP` | OFF / ON | ON / OFF (the offlineasm assembly LLInt) |
| `ENABLE_DFG_JIT`, `ENABLE_FTL_JIT`, `ENABLE_WEBASSEMBLY` | OFF | ON (compiled in) |
| `ENABLE_SAMPLING_PROFILER` | OFF | OFF |
| run-time isolation | -- | `JSC_useDFGJIT=false`, `useFTLJIT=false`, `useWasm=false`, `useConcurrentJIT=false` |

Turning `C_LOOP` off also switches to the assembly interpreter, a second new assumption, so
1C runs in two steps over **the same binary**, chosen with JSC options: (1) `JSC_useJIT=false`
-- the assembly LLInt with no generated code, rerunning the whole 1B acceptance and requiring
that nothing is compiled; (2) the Baseline JIT with the options above.

*Proof that machine code ran.* Passing the interpreter script on a JIT build proves
nothing. Step (2) runs `jsc_jit_acceptance.js` with `JSC_jitPolicyScale=0.01` (compile
almost at once), `JSC_crashIfCantAllocateJITMemory=true`, `JSC_validateOptions=true` and
`JSC_reportBaselineCompileTimes=true`, and the harness requires at least one Baseline
compile in that output and none in the step (1) run. The script covers a hot arithmetic
loop, hot calls, array and property access, an exception thrown through a compiled
frame, a full GC while compiled code is live, repeated compile and execute, and the
multi-thread VM cycle with the same options.

*New risk: signal-based VM traps.* With the JIT on, `ENABLE_SIGNAL_BASED_VM_TRAPS` is 1: the
watchdog and termination interrupt compiled code from another thread with a signal whose
handler edits the interrupted `ucontext_t`. CRT now forwards the kernel's real context and has
`pthread_kill`, so it can work; it is unproven until a compiled infinite loop is terminated
by `JSContextGroupSetExecutionTimeLimit` (exported by the library and declared in the source tree's `API/JSContextRefPrivate.h`, which the build's header set does not install, so the test declares it itself; a 1C acceptance item).

*CRT's own executable-memory contract first.* `libc/tests/mman_test.c` exercises RW to R to RW
only and never `PROT_EXEC`. A `jit_memory_test` (x86_64: `mov eax, 42; ret`; aarch64 with the
instruction-cache flush) maps RW, writes code, `mprotect` RX, calls it, flips to RW, patches,
flips to RX and calls again, and also maps `PROT_READ|PROT_WRITE|PROT_EXEC` in one call and
reserves `PROT_NONE`/`MAP_NORESERVE` address space that is then committed -- the shapes WTF
uses. It runs before JSC, so an executable-memory defect in CRT is told apart from one in
JSC's executable allocator. macOS needs `MAP_JIT` and the arm64 write-protect toggle, and
Windows `VirtualAlloc`/`VirtualProtect`; each is its own host replay.

*W^X is not part of the first green.* In the pinned tree `ENABLE_MPROTECT_RX_TO_RWX` defaults
to 0 and the POSIX allocator creates Linux executable memory `PROT_READ|PROT_WRITE|PROT_EXEC`
at once, with no `MAP_JIT` (that flag is Darwin's). A JIT-on build therefore does *not* test an
RW to RX transition, and the earlier wording "W^X transitions" overstated 1C. 1C verifies the
executable-memory lifecycle (allocate, generate, execute, free); a W^X policy -- whether CRT
enables upstream's mprotect path on a given host, and verifies it -- is a separate hardening
step after first green.

*Pre-JIT gate (all before the first JIT run):* build mode and the Baseline profile in the
harness; `jit_memory_test`; rerun 1B from the installed `05-ui` SDK (the stage contract's
predecessor of `06-web`; 1B was run from `02-cxx`); gperf removed from the JSCOnly checks
(done); the host `libatomic` dependency of `libc++.so` removed (done: the libc++ build now sets
`LIBCXX_HAS_ATOMIC_LIB=OFF`, and neither `libc++.so` nor `libJavaScriptCore.so` has an undefined
`__atomic_*` symbol); the stale `libcrtweb` and porting documents corrected (done).

*1C is complete when:* the Baseline-JIT acceptance, the `JSC_useJIT=false` rerun of 1B and the
watchdog test pass on Linux/x86_64; the compile proof is present; resident memory stays
bounded; and the host-ABI audit is clean. That is the *reference acceptance* (done). The *host
replays* are recorded separately and are not part of that definition: Linux/aarch64,
macOS/arm64 and Windows/x64 are all done.

*Deliberately unchanged.* The default thread stack stays 1 MiB. Correct stack-overflow detection,
1/4/8-thread VMs, concurrent DFG/FTL compiler threads and the profiler's four worker VMs all
pass, but an embedder that runs arbitrary script still owns its thread-size policy (JSC's defaults
are 5 MB per-thread usage, 64 KB reserved zone and 128 KB soft zone). `__tls_get_addr`: JSC itself avoids it with `-ftls-model=initial-exec`; ICU and
`libc++abi` still reference it. On Linux the host's dynamic loader is CRT's loader today, so
this is the *accepted current loader boundary*, and becomes a gap only when CRT owns a loader.

**1D is split by what each step adds** (the 1C binary already has every tier compiled in; each step turns
one on and isolates it with JSC run-time options):

| Step | Turns on | New assumption under test |
|---|---|---|
| 1D-A | DFG + concurrent compiler threads | compiler threads, DFG code, cross-thread VM/code lifecycle |
| 1D-B | FTL/B3 | the B3/Air backend, larger compile jobs (Linux/x86_64, Windows/x64 done) |
| 1D-C | WebAssembly | wasm tiers, their memory/trap handling (Linux/x86_64 and Windows/x64 done) |
| 1D-D | sampling profiler (`ENABLE_SAMPLING_PROFILER`, a build option) | signal-based suspension of arbitrary VM threads (Linux/x86_64 and Windows/x64 done) |

W^X is hardening, not a tier. Order: Linux/x86_64 first, so a failure is a JSC-tier problem and not a signal
emulation or persona problem, then Linux/aarch64, macOS/arm64 and Windows/x64. **1D-A gates Tranche 2 at minimum.** The pinned WebKit
enables FTL, WebAssembly and the sampling profiler by default on x86_64/arm64, so 1D-B..1D-D close JSC's default
capabilities: finish them before the unchanged WPE baseline where practical (1D-B through 1D-D are done on Linux/x86_64 and Windows/x64), but none
is a `PlatformCRT` architecture blocker.

**1D-A result, Linux/x86_64 (2026-10-05).** `tools/build_webkit_jsc.py --mode baseline-jit` gained "step 3", run on
the *same* binary as 1C with `JSC_useDFGJIT=true`, `JSC_useConcurrentJIT=true`, four compiler threads
(`JSC_numberOfDFGCompilerThreads=4`), FTL and WebAssembly still off and polling traps off:

* `libcrtweb/tests/jsc/jsc_dfg_acceptance.js` (7 groups: hot int/double code and overflow exits, OSR exit and
  re-optimisation after type/shape changes, inlining and callee replacement, exceptions through optimised frames,
  GC with live DFG code, many functions compiling concurrently while being invalidated, typed arrays/strings) passes at
  jitPolicyScale 0.01, at the default thresholds, and with `useConcurrentJIT=false`.
* The proof that DFG code ran is the `JSC_reportDFGCompileTimes` report ("using DFG"): 108-137 reports per script run; there
  must be none for FTL. With `JSC_useDFGJIT=false` the same script yields zero DFG reports (the check is not vacuous).
  The compiler threads exist (`JITWorker` threads in `/proc/<pid>/task` with concurrent JIT on, none with it off).
* The 1B and 1C scripts pass again under DFG; the context cycle passes on 0/1/4/8 threads (250 DFG reports on 4
  threads); the watchdog terminates an infinite loop whose `spin` function was compiled by DFG; host-ABI audit clean.
* Stress: the DFG script 100/100, the watchdog 100/100, 4- and 8-thread context cycles 40/40 (no failures); the
  interpreter mode is unchanged and passes. The first run was green; nothing in CRT had to change for DFG.

*Not covered by 1D-A:* FTL and WebAssembly (1D-B/1D-C, below), the sampling profiler, W^X, TSAN/ASAN builds, a long soak, and any host other
than Linux/x86_64 and macOS/arm64 (below).

**1D-A replay, macOS/arm64 (2026-10-06): green on the first run, nothing in CRT or the carried patches changed.**
Rebuilt `crt-ui-dist` from the tree after the pull (verified), then `tools/build_webkit_jsc.py --mode baseline-jit`
from an empty work root (build 133 s, acceptance 19 s). Every 1B/1C step passes again, and all of step 3:
the DFG script at jitPolicyScale 0.01 (124 `using DFG` reports), at default thresholds (106), with serial compilation
(137), the 1B script (9) and the JIT script (44) under DFG, context cycles on 0/1/4/8 threads, and the watchdog killing a
DFG-compiled `spin`; host-ABI audit clean. Beyond the harness: the DFG script 100/100, the watchdog 100/100 and
4/8-thread context cycles 40/40 with no failure; with `useDFGJIT=false` the same script gives zero DFG reports and
FTL gives none in either configuration. macOS has no `/proc/<pid>/task`, so the compiler threads were checked by
sampling a running process (`sample`): with concurrent JIT on, `JITPlan::compileInThread` runs under
`JITWorklistThread::work` on worker threads; with it off it runs inside the main thread. DFG code is written under the
same `MAP_JIT` plus per-thread write-protect toggle as Baseline, which the compiler threads' finalization and the
concurrent invalidation exercised without a fault.

**1D-A replay, Linux/aarch64 (2026-10-06): green on the first run, nothing in CRT or the carried patches changed.**
Ubuntu 26.04 aarch64 (QEMU guest, 4 CPUs, Clang 21.1.8), HEAD `8e30763`, 24 commits after the aarch64 TLS work. The old
`out/` was reconfigured with `cmake --fresh` (the commits changed shared libc, `crt-c++` and the dist prerequisites):
clean build with 0 warnings, `crt-ui-dist` and `verify_dist` pass for 03/04/05, tooling unittests 102/102, full `ctest`
140/140. `signal_vmtrap_test` reports "skipped: x86_64 Linux and Windows only" on aarch64 and is **not** evidence here;
the aarch64 evidence for signal VM traps is the JSC watchdog. `tools/build_webkit_jsc.py --mode baseline-jit` from an
empty work root (build 356 s, acceptance 20 s) passes every 1B/1C step and all of step 3: the DFG script at
jitPolicyScale 0.01 (123 `using DFG` reports), at default thresholds (108) and with serial compilation (137, the same
count as Linux/x86_64 and macOS/arm64), the 1B script (9) and the JIT script (45) under DFG, context cycles on 0/1/4/8
threads, and the watchdog killing a DFG-compiled `spin`; no FTL report anywhere; host-ABI audit clean. Beyond the
harness: the DFG script 100/100 (118-125 reports per run), the watchdog 100/100 (every run's `spin` compiled by DFG) and
4/8-thread context cycles 40/40, all with no failure; with `useDFGJIT=false` the same script passes with zero DFG
reports. Compiler threads: with concurrent JIT on, `JITWorker` threads appear in `/proc/<pid>/task` (up to 5 threads in
the process); with it off there are none (3). `--mode interpreter` is unchanged and passes (build 285 s). Not covered:
the same list as the other replays (FTL, WebAssembly, the sampling profiler, W^X, TSAN/ASAN, a long soak).

**1D-A replay, Windows/x64 (2026-10-06): green on the first run; nothing in CRT, the carried patches or the harness
changed.** Rebuilt `crt-ui-dist` from the tree, then `tools/build_webkit_jsc.py --mode baseline-jit --skip-build`
on the same Windows Baseline-JIT binary (polling traps off, the signal VM-trap gate's first consumer for DFG). Every
1B/1C step passes again and all of step 3: the DFG script at jitPolicyScale 0.01 (119 `using DFG` reports), at default
thresholds (103), with serial compilation (134), the 1B script (10) and the JIT script under DFG, context cycles on
0/1/4/8 threads, and the watchdog killing a DFG-compiled `spin`; FTL compile reports are zero everywhere and the
host-ABI audit is clean. Stress beyond the harness: the DFG script 100/100, the watchdog 100/100 and 4- and 8-thread
context cycles 40/40 with no failure or hang. Concurrent compilation is real: the process peaks at 15 threads with it on
and 12 with `JSC_useConcurrentJIT=false`.

**1D-B and 1D-C result, Linux/x86_64 (2026-10-06).** Same binary again; the harness gained "step 4" (FTL) and "step 5"
(WebAssembly) in `tools/build_webkit_jsc.py` (`run_ftl_acceptance`, `run_wasm_acceptance`), still requiring evidence of the
tier each step claims.

* *1D-B, FTL (B3/Air).* `jsc_ftl_acceptance.js` (the DFG groups sized to reach FTL at the default thresholds, plus a
  40-block function that stresses B3 lowering and Air register allocation, float and BigInt cases) passes at
  jitPolicyScale 0.01, at the default thresholds and with serial compilation: 108-252 FTL reports (`reportFTLCompileTimes`,
  "using FTL ... with FTL", also `FTLForOSREntry`); the same script with FTL off yields none (negative control). The 1B, 1C and
  1D-A scripts, the context cycle on 0/1/4/8 threads and the watchdog pass with FTL enabled (two FTL compiler threads).
* *1D-C, WebAssembly.* `jsc_wasm_acceptance.js` assembles its modules from raw bytes (no wat2wasm): validate/compile/
  instantiate, i32/i64/f32/f64 including wraparound and BigInt, loops and recursion (made hot), imports and exceptions
  through Wasm frames, tables and `call_indirect` (signature and bounds traps), memory load/store/`memory.grow`, nine kinds
  of trap that must surface as `WebAssembly.RuntimeError` (out of bounds, straddling the end, huge offset, divide by zero,
  divide overflow, `unreachable`, ...), 400 instances of one module, async compile. The tiers are separated with
  `JSC_useBBQJIT`/`JSC_useOMGJIT` and evidenced by `JSC_dumpBBQDisassembly`/`JSC_dumpOMGDisassembly` ("Generated BBQ/OMG"):
  interpreter only (0 JIT code), BBQ only (20+ BBQ, 0 OMG), BBQ and OMG (7-9 OMG), and again with
  `useWasmFastMemory=false`. *Fast memory is proven, not assumed:* `jsc_wasm_oob_probe.js` under `strace -f -c` handles 50
  signals with fast memory (each out-of-bounds access is a SIGSEGV that JSC's fault handler turns into a RuntimeError)
  and none with software bounds checks (Linux with `strace` only; recorded as unavailable elsewhere). The watchdog stops
  an infinite loop running in Wasm code on the main and on worker threads (`jsc_watchdog_test wasm`).
  *Upstream limit, not a CRT gap:* JSC delivers a VM trap to Wasm code at function entry only (the prologue stack
  check), so the loop must contain a call; a Wasm loop with none is not interruptible by the execution time limit
  (from the pinned source, `WasmIPIntSlowPaths.cpp`; not measured on a native build). SIMD and Wasm threads are not covered.
* Stress: FTL script 100/100, Wasm script 100/100, watchdog 100/100 for the JS and the Wasm loop, 4- and 8-thread
  context cycles 40/40, and the Wasm script as four concurrent processes x 200 (800 runs) without failure.

*Two CRT bugs, found only by running Wasm while the compiler threads install code.* A native glibc build of the same JSCOnly
source (ICU built from the same tarball) ran 1,200 such runs without a failure; the CRT runtime hung or crashed in
roughly 1 run in 10 (hang) and 1 in 100 (crash) under 4-way CPU contention before the fixes, and 0 in 800 after.

1. *Thread registry lock.* `libc/src/tls.c` found the current thread's context (errno, `pthread_self`, `pthread_getspecific`,
   `pthread_kill`) by walking a list under one global spin lock. WTF suspends a thread with SIGUSR1 and parks it in the
   handler; a thread parked while it held that lock left every other thread, including the one sent to resume it
   (`pthread_kill` -> `pthread_self`), spinning in `sched_yield` for ever. The Wasm tier-up path
   (`resetInstructionCacheOnAllThreads`) suspends all threads for each installed callee, which made it frequent. Fix: a fixed
   open-addressed table keyed by tid with lock-free readers (slots are never reallocated; the context pointer is published
   last and cleared first, a tombstone keeps probe chains intact); only thread start/end/fork take the lock, and nothing a
   handler or a resumer does does. `signal_threads_test` now parks and resumes a thread that is hot in errno/`pthread_self`
   3000 times (it fails on every run with the old registry, 0/40 with the new). *Not fixed:* `pthread_kill` to a thread before it
   has started running (it registers itself) finds nothing; the runtimes that use the suspend protocol signal started threads only.
2. *Small `memcpy` was a byte loop.* `libc/src/string/bcopy.c` is OpenBSD's Torek copy: a 4-byte `memcpy` is four byte stores,
   and the CRT wrapper's `-fno-builtin` keeps the compiler from turning a constant-size copy into one `mov`, as it does against
   glibc. WTF's `performJITMemcpy` patches call displacements and immediates with `memcpy`, and another thread may be executing
   that code: BBQ-to-OMG call-site patching showed a thread half of a new displacement and it crashed in JIT code (SIGSEGV or
   SIGILL). Fix: copies of up to 16 bytes are single, overlapping 2/4/8-byte loads then stores (what Bionic's and glibc's
   optimised versions do), shared by `memmove` and `bcopy`. `memcpy_atomicity_test` reads the destination while another thread
   alternates two patterns: over a million torn 4-byte values before, none after, plus a correctness sweep of sizes 0-40 and of
   forward/backward overlap. This also applies to the macOS/arm64 and Windows replays, which patch 4-byte instruction words
   the same way and passed without it (by timing, not by design); they need this commit and a rerun of their JIT steps.

**1D-B accepted and 1D-C progressed but still open, Windows/x64 (2026-10-07).** The replay
used the installed and verified `out/windows-host-ninja-debug/dist/05-ui` SDK and the existing pinned JSCOnly build, with
polling traps off. Step 4 passed the FTL script at reduced/default thresholds and with serial compilation (108-252 FTL
reports), its FTL-off negative control, the earlier scripts, 0/1/4/8-thread context cycles and the watchdog. One near-final
full run's Step 5 passed Wasm interpreter-only, BBQ-only (22 BBQ, zero OMG), BBQ+OMG (22 BBQ, 6 OMG), software-bounds mode, earlier-tier scripts,
0/1/4/8-thread cycles and termination of main- and worker-thread Wasm loops. The host-ABI audit is clean.

Linux proves the fast-memory path with `strace`, which Windows does not have. The new
`libcrtweb/tests/jsc/jsc_wasm_signal_probe.cpp` instead evaluates the same OOB script through JavaScriptCore's public C API
and reads CRT's process-wide signal-delivery generation before and after it. The result is the same proof on Windows:
**50** fault-handler invocations with Wasm fast memory and **0** with software bounds checks.

The first Windows replay exposed races in the emulated thread-directed signal backend, not WebAssembly code:

1. JSC's x86_64 VM-trap instruction is `hlt`. Windows reports it as `STATUS_PRIVILEGED_INSTRUCTION`; CRT now maps that to
   Linux-compatible `SIGSEGV`/`SI_KERNEL` with null `si_addr`, and delivers a thread signal that became pending under the
   fault handler's full mask before returning to the faulting context.
2. A bounded injection attempt may repeatedly catch a target in ntdll/kernelbase. A per-process signal pump retains the
   pending work and retries when the target returns to user code; per-target `injecting` and `delivering` state prevents a
   caller, pump and target-side delivery from duplicating one signal or leaking a handler mask.
3. The decisive exact-once bug was the sender polling `claim` in a frame below the target's old stack pointer. As soon as
   the handler restored that pointer, ordinary target execution could overwrite the frame before the sender observed the
   claim; the sender then resurrected the pending bit and delivered the same signal twice. The handler now acknowledges on
   the sender's still-live stack while the target-frame claim remains only the cancellation arbitration. Strict handler
   counts made the old bug fail 19-20 of 20 runs; after the fix `signal_vmtrap_test`, `signal_threads_test` and
   `memcpy_atomicity_test` each pass 20 consecutive runs and full Windows CTest passes 157/157.

One near-final aggregate harness run made every 1D-B/1D-C item pass, but one earlier 1D-A 4-thread context-cycle process
exited at a JSC lock assertion (`Invalid value for lock: 0`), making only the aggregate `passed` bit false. The exact same
DFG/4-thread process then passed 10/10 focused runs; the FTL and Wasm 4-thread processes in that aggregate run also passed.
A second run passed every step and the aggregate/host-ABI gates cleanly (131 s acceptance). During final code review the
sender-acknowledgement lifetime was tightened once more (if the target claims immediately before the timeout, the sender
now waits for the acknowledgement store before its stack can disappear). The first SDK rebuilt from that source still exposed
three independent defects: a BBQ SIGSEGV when a fault context had already been redirected to the signal stub, an
`instance-lifecycle` lost-acknowledgement hang, and compiler-rt emulated-TLS teardown racing a concurrent compiler thread at
process exit (`Invalid value for lock: 0` or silent exit 134). All three are fixed in the CRT/PAL: 0 SIGSEGV in 70 BBQ-only
runs, 0 hangs in 150 runs, the official acceptance passes, and about 800 bounded Wasm runs have no failures. The emulated-TLS
runtime was subsequently centralized in `libc.dll` (2026-10-08) with a two-DLL ownership regression, so JSC and graphics no
longer carry consumer-local copies. Windows 1D-B and 1D-C are closed. No temporary signal tracing instrumentation is part of
the result. macOS/arm64 and Linux/aarch64 replays can proceed independently; sampling-profiler replays, W^X, SIMD and Wasm
threads remain separate work.

**1D-D result, Linux/x86_64 (2026-10-08).** The harness has a third, separate
`--mode sampling-profiler` build tree. It configures the pinned JSCOnly source with
`ENABLE_SAMPLING_PROFILER=ON`, keeps Baseline/DFG/FTL/WebAssembly enabled, and reruns every
1B..1D-C gate before the profiler-specific gate. This avoids treating a profiler-only build as
evidence for an ordinary tier build and proves that profiler suspension coexists with concurrent
JIT compilation, Wasm fast-memory faults and watchdog VM traps.

`libcrtweb/tests/jsc/jsc_sampling_profiler_acceptance.js` first checks JSC's
`platformSupportsSamplingProfiler()`, starts profiling, and executes a named non-inlined hot loop.
It validates the returned profile interval, non-empty traces and that the expected JavaScript
function occurs in sampled frames. Four `$262.agent` workers then create independent VMs and run
the same test concurrently; each worker must return its own valid profile. The official installed-
`05-ui` SDK run passed with **5/5 profiles, 1,320 traces and 1,320 named hot frames**, and the host-
ABI audit reported no host-library violation. Twenty additional isolated repeats passed 20/20;
every run sampled all five VMs, with at least 1,288 traces and 1,288 named hot frames. The rebuilt
CRT signal/TLS regression subset passes 9/9 and the tools suite passes 104/104. This closes 1D-D on
Linux/x86_64 only. The full in-tree CTest result is 169/170; its sole failure is the documented,
pre-existing no-sound-card `crtmedia_playback_pipeline_test_runs` wall-time pacing check, unrelated
to Web/JSC. Linux/aarch64 and macOS/arm64 remain native replay gates.

**1D-D replay, Windows/x64 (2026-10-08).** The separate `sampling-profiler` build
passed the complete 1B..1D-C sequence before the profiler step, including concurrent DFG/FTL,
Wasm fast-memory faults and watchdog VM traps. The official installed-`05-ui` SDK run sampled
the main VM plus every `$262.agent` worker: **5/5 profiles, 153 traces and 153 named hot frames**;
the aggregate acceptance and PE host-ABI audit passed. Twenty focused repetitions passed 20/20,
always sampled all five VMs, and had minima of 152 traces and 152 named hot frames.

The first worker run found a Windows pthread publication race. The target wrapper obtains its
actual bounds through `GetCurrentThreadStackLimits()`, but `pthread_create()` previously could
return before those bounds were stored. WebKit queries `pthread_getattr_np()` immediately in the
creator, so it observed the initialized `{ base = NULL, size = 1 MiB }`, derived stack origin
`0x100000`, and crashed the worker at `sanitizeStackForVMImpl` before useful profiling. The
Windows start wrapper now release-publishes its bounds and wakes the creator; `pthread_create()`
does not publish the new `pthread_t` until that handshake completes. A second creator-publication
handshake prevents an immediately finishing detached worker from freeing its control block while
the creator still reads it. A libc regression queries a still-live worker immediately after
creation and requires a real non-null range. With this PAL contract fixed, no JSC/WebKit source
change was needed. The final source passes Windows CTest
164/164, the focused pthread/signal/TLS subset 7/7, the stack-bound and detached-thread regressions
100/100 each, and the JSC harness unit tests 2/2. A cumulative `05-ui` rebuild passed every
`verify_dist`; the direct profiler gate against that final SDK passed again with 5/5 VMs,
138 traces and 138 named frames.

A redundant post-package full-suite replay was stopped after its second Wasm script exceeded
the expected duration, before it reached the profiler. The official complete run, the 20/20
profiler repetitions and the final direct profiler gate remain green; this observation reopens
1D-C only if it reproduces. Linux/aarch64 and macOS/arm64 remain the native 1D-D replay gates.

**Linux/x86_64 result (2026-10-04): 1A and 1B green; 1C (JIT) and the Windows and
macOS replays remain.** `tools/build_webkit_jsc.py` builds JavaScriptCore from the
verified pin against an installed SDK (`02-cxx` here) and runs the acceptance; the
whole run from an empty work root takes about six minutes (build 297 s) and passes.

- *1A.* JSCOnly needs ICU >= 70.1 (`data`, `uc`, `i18n`) as a target dependency, and
  CMake, Ninja, Perl (with modules), Python and Ruby as build-host tools. ICU 78.3 is a
  CRT port (`porting/recipes/icu.json`); gperf 3.3 is ported too (`gperf.json`) but is a
  WebCore prerequisite, not needed here; both pass their recipe tests. The first-green configuration is the
  C_LOOP interpreter (JIT, FTL, WebAssembly and the sampling profiler off) with the
  `Generic` event loop, no GLib; every deviation is a CMake option or compiler flag
  recorded in the harness with its reason (`USE_HEADER_MAPS=OFF` because the release
  tarball omits `hmaptool`, `-mcx16`, `-ftls-model=initial-exec`).
- *1B acceptance.* `libcrtweb/tests/jsc/jsc_acceptance.js` runs eight groups through the
  `jsc` shell -- arithmetic (including BigInt), objects/arrays/classes/Proxy, JSON,
  RegExp (named groups, lookbehind, Unicode properties), Unicode through ICU
  (`toUpperCase` of sharp s, NFD/NFC, surrogate pairs, `Intl.Collator` for `sv` versus
  `en`, `Intl.NumberFormat`, Turkish dotless i, `Intl.Segmenter`), exceptions (including
  stack-overflow detection), GC stress (about 400 MB of short-lived objects across full
  collections, survivors checked, WeakRef/FinalizationRegistry, a 200,000-object live set),
  and Promise/async microtask ordering -- and passes with a peak RSS of 198 MB (bound
  450 MB). `jsc_context_cycle` creates, uses and releases a JS global context (a whole VM)
  150 times, then runs 20 create/use/release iterations on each of 1, 4 and 8 threads
  concurrently, checking results and resident memory: 0 failures in every configuration, with
  resident memory flat (growth within +/-1 MB after warm-up; 6 of 6 repeated 8-thread runs
  clean).
- *Mutation.* Making `__crt_linux_thread_tls_size()` decline reverts threads to shared TLS
  and fails `pthread_native_tls_test` (and, before the fix, every JSC thread test); restored.
- *Not covered.* The JIT (1C, planned above), any host but Linux/x86_64, aarch64 (native thread TLS was not
  implemented there when this was written, so threaded JSC misbehaved; done 2026-10-05, see the Linux/aarch64 replay below), and the sampling profiler and
  WebAssembly (they need the JIT tiers and signal-based thread suspension).

**CRT gaps this exposed (all fixed in CRT/PAL, none by changing WebKit):**

1. *`thread_local` was shared by every CRT thread on Linux* (one address, values bleeding):
   native per-thread ELF TLS blocks, control block and dtv for `pthread_create` threads
   (`docs/linux_pthread_lifecycle.md`).
2. *An executable linked its own `libc.a` while the shared libraries used `libc.so`*, so a
   process had two libcs with separate thread/key registries and allocators (a library's
   `pthread_getspecific` returned the main thread's value in every worker; heap panics).
   Under shared runtime linkage (`CRT_CXX_RUNTIME_LINKAGE=shared`) `crt-c++` now links
   `libc.so`/`libm.so`/`libdl.so` into the executable too.
3. *`siginfo_t` had no `si_addr`, `ucontext_t` was a private blob and handlers got a null
   context*: Bionic/kernel layouts and real forwarding on Linux (`docs/signal_delivery.md`).
4. *`open(..., O_CLOEXEC)` silently ignored the flag* on Linux (the descriptor leaked into
   exec'd children); now applied atomically (Linux) or with `fcntl` (elsewhere).
5. *Bionic surface JSC/WTF needs*: `pthread_kill` (`tgkill`), `pthread_getattr_np` for the
   initial thread, `sched_*` and `SCHED_BATCH/IDLE`, `memmem`, `usleep`, `mkostemp`,
   `fallocate`, `sysinfo`, `sendfile`, `getprogname`, `CLOCK_BOOTTIME`/`_COARSE`/`_RAW` and the
   CPU-time clocks, `sys/ucontext.h`, `SYS_gettid`/`__NR_*`, `<cxxabi.h>` in the C++ SDK.
   `bionic_surface_test` covers them; the macOS and Windows implementations of the clocks,
   `sched_*` and the signal record are compile-checked there but not run.
6. *GNU ld.bfd links a PIE that has `PT_TLS` with a stray `.rela.plt` entry the loader
   rejects*: the Linux CTest executables now link with LLD, the project's primary linker.

*Found by the host-ABI audit and fixed:* `libc++.so.1` recorded `NEEDED libatomic.so.1`
(the libc++ build had linked the host libatomic), and that host library in turn pulled
glibc's own `libc.so.6` into the process next to CRT's `libc.so`; the libc++ recipe now sets
`LIBCXX_HAS_ATOMIC_LIB=OFF` on every target (it was Windows-only), and the audit is clean.

*Recorded, not fixed:* the ICU shared libraries and `libc++abi.so` use the global-dynamic TLS model and so reference
the loader-provided `__tls_get_addr` (the accepted Linux dynamic-loader boundary, a gap only once
CRT owns a loader), which is why the harness links with `--allow-shlib-undefined`; the default thread stack is 1 MiB (as in Bionic), far less than
JavaScriptCore wants, so an embedder must size the stacks of threads that run script.

**Linux/x86_64 1C result (2026-10-04): the Baseline JIT is green**, built from the installed `05-ui`
SDK (the stage contract's predecessor of `06-web`; 1B was rerun from it first, from a fresh
configure and build, and also passes) by `tools/build_webkit_jsc.py --mode baseline-jit`.
The pre-JIT gate is closed: `--mode`, the profiles above, `jit_memory_test` (CRT's own
RW-to-RX-to-RW, one-call RWX and reserve-then-commit executable-memory contract, 3 shapes),
gperf out of the JSCOnly checks, `libatomic` gone from libc++, 1B on `05-ui`, stale docs.

- *Step 1* (`JSC_useJIT=false`, assembly interpreter): the whole 1B script passes, zero functions compiled.
- *Step 2* (Baseline JIT): `jsc_jit_acceptance.js`, eight groups (hot arithmetic, hot calls and
  class dispatch, arrays/typed arrays/polymorphic properties, exceptions thrown through compiled
  frames including a stack overflow in compiled code, a full GC while compiled closures are live,
  1,500 `new Function` and 400 `eval` compilations, RegExp and string work, an async loop) passes.
  **Proof that generated code ran:** the compile report lists 2,340 lines such as
  `Optimized mix#...: LLIntFunctionCall ... using Baseline with Baseline into 1360 bytes`, none
  with the JIT off. The 1B script under the JIT, the VM create/use/release cycle (150 times, and on
  1/4/8 threads; RSS flat, growth under 0.1 MB after warm-up) and the host-ABI audit also pass.
- *Signal-based VM traps:* `jsc_watchdog_test` terminates a compiled infinite loop (the report names
  `spin`) with a 250 ms execution-time limit -- 3 times on the main thread and 3 on workers, each
  at 0.25 s -- and a crash reporter built on the forwarded `siginfo`/`ucontext` names any fault.
- *Stability:* 10 of 10 harness runs, and 150 of 150 watchdog runs (8 of 100 crashed before the fixes below).

**What the JIT work found in CRT (all fixed there):**

1. *`pthread_kill` crashed on the initial thread* (my own new code): a `pthread_t` not created by
   `pthread_create` is the plain kernel tid, and it was dereferenced as a control block. Found as the
   intermittent watchdog crash (faulting address = the pid); now handles are validated against the
   registry of live CRT threads (`__crt_thread_control_is_live`). `bionic_surface_test` signals the initial
   thread from a worker (a SEGFAULT with the fix reverted).
2. *`sigsuspend` returned immediately.* WTF suspends a thread by signalling it and parking it in `sigsuspend`;
   with the stub the thread kept running while another believed it stopped, and JSC patched code under it.
   `sigsuspend` is now `rt_sigsuspend` on Linux.
3. *`sigaction` dropped `sa_mask` and the flags* (`SA_NODEFER`, `SA_ONSTACK`, `SA_RESTART`, ...): WTF's handlers
   block every other signal while they run, so a second signal could interrupt one mid-way. Both are now handed
   to the kernel (`SA_RESETHAND` is emulated), and `SA_ONSTACK`/`SA_NODEFER`/`SA_RESETHAND` exist in `<signal.h>`.
4. *The signal mask was one software value for the whole process*; it is per thread in the kernel and
   `sigprocmask`/`pthread_sigmask` now read and write the calling thread's real mask on Linux.
   `signal_threads_test` covers 2-4 (stubbing `sigsuspend` fails four of its checks).
5. *`pause()` was missing* (`jsc.cpp` needs it): polls the signal-delivery generation, then fails with EINTR.

macOS and Windows keep the software mask and the stub `sigsuspend` (their backends return "not
provided"); that is compile-checked, not run, and is the likely first problem of their JIT replays.

*Not part of this green:* W^X policy (upstream's default does not exercise an RW-to-RX transition),
the DFG, FTL and WebAssembly tiers (compiled in, disabled at run time; 1D), the sampling profiler,
concurrent compiler threads, aarch64 (replayed 2026-10-05, below), and the Windows and macOS replays. Default thread stack stays
1 MiB; the baseline JIT needed no more.

**macOS/arm64 replay, 1A and 1B (2026-10-04): the JavaScriptCore interpreter is green on the CRT
runtime. The Baseline JIT (1C) is green too, recorded after this section.** Built from the in-tree `05-ui` SDK and the CRT-built ICU by
`tools/build_webkit_jsc.py --mode interpreter` on macOS 27.0.1 / arm64 (about 2.3 minutes from a populated
cache: build 100 s).

*Decision (A): how WebKit sees macOS.* WebKit's CMake knows `APPLE`, Linux, Windows and Fuchsia, and its
Apple branches select Apple SDK code (Mach exceptions through MIG, Cocoa, libdispatch, the host `libicucore`),
which would make `libJavaScriptCore.dylib` run on libSystem instead of the CRT runtime, the thing the Host ABI
firewall rejects. So the harness presents the CRT target as a Bionic/Linux-shaped POSIX platform and changes
nothing in the WebKit tree except one carried patch:

- *CMake side.* `libcrtweb/cmake/crt_webkit_platform.cmake` (a `CMAKE_PROJECT_INCLUDE`, run right after
  `project()`) sets `APPLE` off and `CMAKE_SYSTEM_NAME` to Linux, so WTF/JSC/bmalloc use their portable POSIX and
  Linux source lists and ICU comes from `-DICU_ROOT`. The CRT compiler wrappers are passed explicitly, because
  on a macOS host `WebKitXcodeSDK.cmake` otherwise pins Xcode's own clang before the toolchain file is read.
- *Compiler side.* The CRT wrapper already compiles macOS with `-U__APPLE__`; the harness adds `-D__linux__=1`
  (WTF/bmalloc `OS(LINUX)`) and `-DWTF_CRT_INT64_IS_LONG_LONG=1`. The integer typedefs stay Darwin's: changing
  `int64_t` to `long` was tried and clang crashed on 142 translation units, because clang's own `arm_acle.h` and
  NEON builtins require `uint64_t == __UINT64_TYPE__`.
- *Carried patches* (`libcrtweb/patches/manifest.json`; this one is 0001, the Mach-O assembler flavor 0002 is described under 1C below, applied by the harness with a SHA-256 check before and
  after, recorded in `carried-patches.json`): `WTF::RawHex` gives `int64_t`/`uint64_t` their own constructors only
  for `CPU(ADDRESS32) || OS(DARWIN)` and otherwise assumes `int64_t` is `intptr_t`; with Darwin's `long long` the
  call in `JavaScriptCore/tools/Integrity.cpp` was ambiguous. The patch adds one condition keyed on the define
  above. Removal condition: WebKit selects those overloads by type identity, or CRT presents macOS as `OS(DARWIN)`
  without Apple SDK code.
- *ELF-only options.* Because WebKit now thinks the target is not Apple it adds `-fdebug-types-section` and
  `-Wl,--no-undefined`; clang rejects the first for a Darwin target and ld64 already refuses undefined symbols in
  a dylib, so `crt-cc`/`crt-c++` drop both on macOS only.

*Result (1B).* `jsc_acceptance.js` passes all eight groups (arithmetic, objects/arrays, JSON, RegExp, Unicode
through ICU, exceptions, GC stress, Promise ordering), peak RSS 69 MB (bound 450 MB). `jsc_context_cycle` creates,
uses and releases a JS context 150 times and then 20 times on each of 1, 4 and 8 threads, `failures=0` in every
configuration and growth of 48-64 KB. The Mach-O host-ABI audit (`otool -L` on every binary and the libraries
they load) is clean: only CRT SDK libraries, the CRT-built ICU, the build tree and `libSystem`.

*CRT gaps the replay exposed (all fixed in CRT/PAL or the recipes, none in WebKit):*

1. *The ports.* `memset_explicit()` (Bionic API 34) added, because gnulib otherwise took Apple's `memset_s` from
   the host libSystem; UTC `tzname`/`daylight`/`timezone` added; `crt-cc`/`crt-c++` drop upstream's
   `-lm`/`-ldl` from non-shared macOS links (ICU's `wchar_t` and endianness probes died in dyld); the ICU recipe
   links its build tools with the shared runtime, passes `--enable-rpath` (absolute install names, not the bare
   `libicudata.78.dylib` that dyld cannot resolve without `DYLD_LIBRARY_PATH`) and uses `-std=c++17` and `.dylib`
   names in its tests (see `docs/porting_status.md`). The libc++ recipe now searches the install prefix first on
   macOS: `libc++.dylib` had recorded the *host* `/usr/lib/libc++abi.dylib` (found by the new audit), so a process
   carried Apple's C++ ABI runtime next to the CRT's.
2. *Thread identity and stacks.* `syscall(SYS_gettid)` returned `ENOSYS` on macOS, so WTF did not recognise the
   main thread and its `RELEASE_ASSERT(uid == 1)` aborted: libc has `gettid()` (Bionic) and macOS now maps the
   Linux numbers WTF uses (`SYS_gettid`, `SYS_getpid`) through the PAL, with the initial thread's id equal to the
   pid as on Linux. `pthread_getattr_np()` for the initial thread had no macOS implementation (Linux reads
   `/proc/self/maps`), so `VM::setLastStackTop` failed: the stack top is recorded at startup from the end of the
   kernel's argv/envp/apple strings and the size comes from the stack limit.
3. *`getrusage()`* was a stub returning zeros on every host (the Linux harness had used `/proc`); macOS now uses
   the BSD syscall, converts `ru_maxrss` from bytes to Bionic's KiB and normalises `tv_usec`.
4. *Tests.* `pthread_native_tls_test` compared TLS addresses of threads that could already have exited (a reused
   block is legitimate); it now keeps every thread alive until all have recorded theirs. The upload receiver of
   `crtmedia_http_lifecycle_test` listened with a backlog of 1 and the test asserted its connection count at
   once; the backlog is 16 and the check waits (bounded). A macOS-only `-Werror` in `bionic_surface_test.c`.

*`jit_memory_test` on Apple Silicon.* Before 1C the test could only check RW to RX there, which was a gap in what
CRT provides, not a pass; the full test now runs, see 1C below.

**macOS/arm64 replay, 1C Baseline JIT (2026-10-04): green.** `tools/build_webkit_jsc.py --mode baseline-jit`
from an empty work root builds in 153 s (configure 9 s) and passes all of: the 1B script with the JIT off (the
assembly interpreter runs it and no code is compiled), the JIT script with `2,340` `using Baseline ... into N bytes`
reports as the compile proof, the 1B script under the JIT, 150 VM cycles plus 20 per thread on 0/1/4/8 threads
(growth under 300 KB), and `jsc_watchdog_test`, which terminates a compiled infinite loop on the main thread and on
workers three times each (about 0.25 s each) through the signal-based VM traps. The Mach-O host-ABI audit is clean.
Three more acceptance runs on the same tree, and the interpreter mode from a clean tree, all pass.

*How the JIT is allowed to run (no WebKit patch).* Apple Silicon refuses memory that is writable and executable at
once unless it was `mmap`'d with `MAP_JIT` and is switched per thread between RW and RX with
`pthread_jit_write_protect_np`. CRT provides both: `mmap()` promotes an anonymous RWX request to `MAP_JIT`
(Darwin's 0x800 collides with Bionic's `MAP_DENYWRITE`, which `mmap()` strips for everything else), and
`libc/src/arch/macos/common/jit_permissions.c` exposes the toggle as `__crt_jit_write_protect`.
JavaScriptCore's own extension point, `OS_THREAD_SELF_RESTRICT(_SUPPORTED)` in `FastJITPermissions.h`, is pointed
at it by `libcrtweb/cmake/crt_webkit_jit_permissions.h`, force-included by the harness. WTF's POSIX allocator
reserves the pool RWX with `MAP_NORESERVE` and commits with `madvise`, which is what the extended
`jit_memory_test` now does as well (RWX in one call, reserve then commit, patching in place). `MAP_JIT` plus
the per-thread toggle is the macOS W^X behaviour; hardened-runtime entitlements are an application-packaging question, not tested here.
x86_64 macOS has no JIT permissions backend (the functions report unsupported).

*Second carried patch, `0002-macho-assembler-flavor`.* The Linux persona chooses ELF assembler syntax, but the
generated and inline assembly is assembled by clang for a Mach-O target. The patch ORs a condition keyed on
`-DWTF_CRT_MACHO_ASM` into the places that choose the `_` symbol prefix, `.private_extern` versus `.hidden`, the
`L` local-label prefix, `.alt_entry`, the absence of `.cfi_startproc`/`.cfi_endproc` in the global asm blocks, the
`#if OS(DARWIN)` guard offlineasm writes into `LLIntAssembly.h`, and Mach-O versus ELF relocation specifiers
(`@PAGE`/`@GOTPAGE` versus `:lo12:`/`:got:`) in `arm64.rb`; `asm.rb` also skips ELF `.size`/`.type` when the
environment variable of the same name is set, because CMake tells offlineasm `--binary-format=ELF` for a Linux
system. The last hunk of the patch is the one that took longest to find: because the persona defines
`OS(LINUX)`, `OFFLINE_ASM_OPCODE_DEBUG_LABEL` emitted a bare, non-underscore GDB label (`op_construct_return_location:`)
next to every `.alt_entry` label. On Mach-O a non-temporary symbol starts a new atom, so the linker moved that
atom away and left zeros after the `blr` that precedes it (twelve such 92-byte gaps; the assembly interpreter died
with `udf` in the middle of `callHelper`). The patch disables those debug labels in the Mach-O flavor. Without the
debug labels the default wrapper link (`-dead_strip`) works and no linker option is needed (an earlier
`CRT_MACOS_NO_DEAD_STRIP` escape hatch, which only hid part of the symptom, was removed). Removal condition: WebKit
keys these on the object format instead of `OS(DARWIN)`, or CRT presents macOS as `OS(DARWIN)` without Apple SDK code.

*CRT gaps found and fixed (CRT/PAL, not WebKit).*

1. *Signals.* The macOS backend kept a software mask and a stub `sigsuspend`; it now forwards real
   `sigaction`/`pthread_sigmask`/`sigsuspend`/`pthread_kill` to libSystem and converts Darwin `siginfo` and the
   arm64 context to the Bionic/Linux aarch64 layouts with register write-back (`docs/signal_delivery.md`).
2. *Thread stack bounds.* A thread created by `pthread_create` with only a stack size had no recorded base on
   macOS, so `pthread_getattr_np()` reported a null stack; JavaScriptCore's soft limit then put the LLInt frame
   zero-fill at address 0x800000 and every worker-thread context cycle died with `EXC_BAD_ACCESS`. The new thread
   now records the bounds Apple reports (`docs/pthread_policy.md`).
3. *Bionic surface.* `getauxval()` (a macOS stub, `AT_HWCAP` etc. report no features), `<asm/hwcap.h>`,
   `gettid()`/`SYS_gettid` mapping (1B), real `getrusage()` (1B).
4. *Tests.* `jit_memory_test` (RWX and reserve-then-commit on Apple Silicon), `signal_threads_test` (the real macOS
   signal cases), `pthread_native_tls_test`, `bionic_surface_test`, and `jsc_watchdog_test`/`jsc_context_cycle`
   run on macOS.

*Open on macOS.* x86_64 macOS has neither the signal conversion nor JIT permissions; the DFG/FTL/WebAssembly tiers and
concurrent compiler threads are 1D; the harness is run from the in-tree `05-ui` SDK, not yet from the isolated stage chain.

**Linux verification of the macOS replay (done on Linux/x86_64 2026-10-04 at `cd954c9` and on Linux/aarch64 2026-10-05; Windows/x64 shared-code checks 2026-10-05, JSC harness not yet).** The macOS work changed shared
libc, headers, wrappers, recipes and the harness. Nothing below has been built or run on Linux (or Windows) since
Linux/x86_64 closed 1C (`c8d5c11`); the macOS-only code is guarded by `CRT_TARGET_OS_MACOS`, but these shared
pieces are not, and are what to check:

| Shared change | Where | Linux/Windows risk to look for |
|---|---|---|
| `gettid()` (+ `unistd.h`), `syscall()` mapping is macOS-only | `libc/src/process.c`, `include/unistd.h` | duplicate/conflicting definition; `__crt_sys_thread_id` is the Linux tid syscall |
| `memset_explicit()` | `libc/src/string/memset_explicit.c`, `string.h` | new exported symbol on every host; `bionic_surface_test` |
| `tzname`/`daylight`/`timezone` (UTC) | `libc/src/time.c`, `time.h` | glibc-style name clashes on Windows (`_timezone`/`_daylight` macros of the mingw headers) |
| real `getrusage()` stays Linux's; `bionic_surface_test` cases | `libc/src/resource.c`, test | the test's new `ru_maxrss`/gettid/hwcap cases on Linux |
| `<asm/hwcap.h>`, `sys/auxv.h` comment | `include/asm/hwcap.h` | collides with the cleaned kernel UAPI `asm/hwcap.h` if the Linux sysroot ships one |
| `pthread_getattr_np()`/`pthread_kill` macOS parts | `libc/src/pthread.c`, `env.c` | only the shared edits (includes, externs) compile on Linux/Windows |
| test changes | `jit_memory_test`, `signal_threads_test`, `pthread_native_tls_test`, `crtmedia_http_lifecycle_test` | the Linux paths are unchanged except the TLS test now keeps threads alive and the upload receiver backlog is 16 |
| wrappers | `tools/crt-cc`, `tools/crt-c++` | macOS-only branches (`-lm/-ldl` drop, ELF-only flag filter); Linux link lines must be byte-identical to before |
| recipes | `porting/recipes/icu.json`, `gperf.json`, `libstdc++/third_party/libcxx/recipe.json` | the macOS overrides are keyed by host; the Linux ICU/libc++ builds must be unchanged |
| harness | `tools/build_webkit_jsc.py` | `apply_patches` (patches target `macos` only, so nothing applies on Linux), `jsc_library`, `MEASURE`, `build_test_programs` |

Steps on the Linux host (x86_64 first, then aarch64 if available):

```
git pull
cmake --preset linux-host-ninja-debug && cmake --build --preset linux-host-ninja-debug
ctest --preset linux-host-ninja-debug                      # C stage; expect the previous pass count
cmake --build out/linux-host-ninja-debug --target crt-ui-dist
(cd tools && python3 test_verify_dist.py && python3 test_text_relocate.py)
python3 tools/build_webkit_jsc.py --sdk-root out/linux-host-ninja-debug/dist/05-ui \
  --deps-prefix out/linux-host-ninja-debug/port-tests/install --work-root /tmp/jscw --mode interpreter
python3 tools/build_webkit_jsc.py ... --mode baseline-jit   # same arguments; expect the 1C results above
```

Expected: the C and `crtui_`/`crtmedia_` tests pass as before, `verify_dist` passes, both JSC modes pass with the
Linux numbers recorded in 1B/1C (the watchdog test, 150 cycles on 0/1/4/8 threads, `ldd` audit). A failure in
a shared-code row above is a regression from the macOS replay and should be fixed in the shared code, not
guarded away. Windows needs the same build plus `ctest`, with particular attention to the `time.h` row.

*Result (Linux/x86_64).* Clean build with 0 warnings; C-stage ctest 114/114; full `ctest` 158/158 serial (the one
excluded test is `crtmedia_playback_pipeline_test_runs`, which needs a sound card); `crt-ui-dist` builds and
`verify_dist` passes; tooling unittests 102/102 (`test_verify_dist` 15, `test_text_relocate` 7 included). The JSC
harness passes from the installed `05-ui` SDK in both modes with the 1B/1C numbers: Baseline compile proof 2,340
reports (same as before), watchdog terminates the compiled loop, cycles on 0/1/4/8 threads, host-ABI audit clean.
No shared-code regression found in the table above. *Wrapper check:* `tools/crt-cc` and `tools/crt-c++` at
`c8d5c11` and at HEAD were run against a stub compiler that prints its argv, on the Linux SDK, for compile, exe-link
and `-shared` lines (including `-lm -ldl` and the new `-fdebug-types-section -Wl,--no-undefined`) under six
`CRT_CXX_*` environments: 54 cases, output byte-identical. *Not checked on that run:* no aarch64 Linux host was available (the aarch64 run follows).

*Result (Windows/x64, 2026-10-05, shared-code checks only).* Incremental build of the existing `out/windows-host-ninja-debug`
(Skia and LVGL ON) at HEAD after the Linux/aarch64 commit: exit 0, one linker warning (the intended duplicate `fprintf`
of `pthread_native_tls_test`, below). Full `ctest` 152/152; `crt-ui-dist` builds and `verify_dist` passes;
`test_verify_dist` 15 and `test_text_relocate` 7 (one POSIX-only skip) pass. The `time.h` row (`_timezone`/`_daylight`
clashes) compiled clean. Two Windows-only defects the checklist had not named were found and fixed first: the in-tree build
lacked `-femulated-tls` (so `pthread_native_tls_test` did not link) and `gettid()` on the initial thread, also in a fork
child, was not the pid (`bionic_surface_test`). *Not done:* the wrapper byte-comparison, and the JSC harness --
`tools/build_webkit_jsc.py` has no Windows path yet (no ICU/gperf ports for the CRT Windows target, no DLL/`jsc.exe`
library lookup, `LD_LIBRARY_PATH`/ELF audit only), so 1A/1B/1C on Windows are not started.

**Windows/x64 replay of Tranche 1 (started 2026-10-05).** *Done and evidenced:* the ICU and gperf ports build and pass
their recipe tests; the shared CRT checks above pass (full CTest 153/153); `jit_memory_test` passes in full on
Windows/x64 (RW to RX, RWX, 256 MiB reserve then commit, generated code executed), so the executable-memory contract
holds through the PAL's `VirtualAlloc`/`VirtualProtect` mapping; `getrusage(RUSAGE_SELF)` now reports the CPU times
and the peak working set on Windows (it returned zeros, and the context-cycle test read `/proc`, which does not exist
there), covered by `bionic_surface_test`. `tools/build_webkit_jsc.py` has a Windows lane (PATH-based DLL lookup,
`JavaScriptCore.dll`, no rpath, a polled peak-working-set measurement because Python has no `resource` module there, and
a PE import audit through `llvm-objdump -p` whose OS-DLL allowlist is the distribution's own Windows contract in
`tools/crt_dist_prerequisites.py`); the audit was checked on real data (it accepts the CRT-built ICU tools and their
`libc.dll`/`KERNEL32.dll` imports and rejects `python.exe`'s `python311.dll`/`VCRUNTIME140.dll`) and the measurement
reports about 214 MB for a 200 MB allocation.

*Result: Windows/x64 1B (interpreter) is green (2026-10-05).* From the installed `05-ui` SDK, with Ruby 4.0.7 on the build host
(a build tool, not a CRT artifact), the pinned WPE WebKit 2.54.0 configures (about 125 s) and `jsc` builds (about 520 s, every
JavaScriptCore source compiled); `jsc_acceptance.js` passes all 8 groups (peak 74 MB, bound 450 MB) and `jsc_context_cycle`
passes 150 cycles on 0, 1, 4 and 8 threads with 0 failures and about 60 KiB of growth; the PE import audit is clean (every
import is a CRT DLL, the CRT ICU or KERNEL32/api-ms-win-core). 1C follows below.

*Result: Windows/x64 1C (Baseline JIT) is green (2026-10-05), with polling traps.* Same pinned tarball, same SDK, the JIT
tiers compiled in and DFG/FTL/WebAssembly off at run time. `jsc_acceptance.js` passes with `JSC_useJIT=false` and compiles
nothing (the assembly interpreter alone); `jsc_jit_acceptance.js` passes with a compile report of 2,340 entries, the same
number as Linux/x86_64; 1B passes again with the JIT on; `jsc_context_cycle` passes 150 cycles on 0/1/4/8 threads (failures 0,
growth about 200 KiB); `jsc_watchdog_test` terminates a compiled `spin` loop on the main thread and on workers, 3 times each
(`spin_function_compiled`); the PE audit is clean. *Not covered:* signal-based VM traps (this run uses
`JSC_usePollingTraps=true`, the first Windows run by decision: the CRT's software signal mask and stub `sigsuspend` are a gate of
their own), the sampling profiler, DFG/FTL/WebAssembly, W^X, Windows/ARM64.

*Update (2026-10-05): the Windows signal VM-trap gate.* The same Windows/x64 Baseline-JIT binary now runs with
`JSC_usePollingTraps=false` (the harness no longer overrides it; `JIT_ON_OPTIONS` sets it explicitly): the watchdog's
VM traps are real signals on the CRT's per-thread signal implementation (`docs/signal_delivery.md`, "Windows:
thread-directed signals"). Acceptance is unchanged and green (compile proof 2,340 reports, `jsc_watchdog_test` main x3 and
worker x3 with the compiled `spin` named in the report, 1B again with the JIT, thread cycles, PE audit), and
`jsc_watchdog_test` was run 100 times with no failure or hang. The first measurement of that loop was 8 hangs in 60 runs:
WTF's thread suspension signals a thread and waits for its handler, and the target was parked in `pthread_cond_wait`,
where the CRT did not run handlers; the CRT's blocking waits were made interruptible. Sampling profiler, DFG/FTL,
concurrent compilation and WebAssembly are 1D.

*What the JIT needed, beyond 1B (carried patch 0003-coff-assembler-flavor, Windows only; nothing else in the tree changes):*

1. *The assembler flavour.* The persona makes offlineasm and the inline assembly take their ELF branches but the object format
   is COFF: `.hidden`, `.size`, `.type name, function`, `name@plt` and `.previous` are rejected. Selected by
   `-DWTF_CRT_COFF_ASM` and the `WTF_CRT_COFF_ASM` environment variable (the same mechanism as the Mach-O variant).
2. *The calling convention.* JavaScriptCore's x86_64 JIT and LLInt pass System V argument registers on every OS and, on
   Windows, call each JIT operation and host function through `__attribute__((sysv_abi))`, selected by `OS(WINDOWS)`; under the
   Linux-shaped persona those macros were empty, so the C++ helpers used the Microsoft ABI the target defaults to and
   `LLInt::initialize()` aborted in `llint_crash` on the first call. The conditions (`PlatformCallingConventions.h`,
   `FunctionTraits.h`, `FunctionPtr.h`, `CodePtr.h`, `AbstractMacroAssembler.h`, `StackPointer.cpp`) and offlineasm's
   `--platform=Windows` (`CMakeLists.txt`, through `CRT_WINDOWS_ABI` from the persona include) now include the CRT Windows
   flavour. `CallFrame.h`'s Windows branch (`_AddressOfReturnAddress`, an MSVC intrinsic mingw clang lacks) is deliberately
   not taken; the generic `__builtin_frame_address(1)` path ran correctly under all of the acceptance.
3. *GNU bit-field layout* (`-mno-ms-bitfields`): `RegisterAtOffset` packs an `unsigned` and a `ptrdiff_t` bit-field into 8 bytes,
   which the Microsoft layout MinGW defaults to does not (16 bytes, a `static_assert`).
4. *CRT:* `getrlimit(RLIMIT_STACK)` promised a fixed 8 MiB while the executable's real stack is 1 MiB; `WTF::StackBounds` sizes the
   main thread's stack from that limit, so the assembly interpreter recursed past the real stack end and crashed instead of
   throwing a RangeError. It now reports the real size (recorded at startup, `pthread_stack_bounds_test`). Raising the default
   executable stack to 8 MiB (`/stack:`) is a separate decision, not taken.
5. *Test:* `jsc_watchdog_test.cpp`'s crash reporter reads the Linux-layout `ucontext_t` and is not built on Windows.

*Decision taken: the Linux-shaped persona.* The first configure showed that the pinned tarball does not contain WebKit's WIN32
sources (`Source/WTF/wtf/text/win/StringWin.cpp` is named by CMake and absent), so a WIN32 build is not possible from this pin at
all, apart from the objections above. `libcrtweb/cmake/crt_webkit_platform.cmake` turns off WIN32/MINGW/MSVC for WebKit's CMake
and the harness undefines every Windows macro clang predefines for `*-w64-mingw32` (13 of them; undefining only four let
bmalloc pick `<process.h>`) and defines `__linux__`. Nothing in the WebKit tree is patched for 1B.

*CRT gaps this exposed, all fixed in the CRT/PAL (none by changing WebKit):*

1. `atanf` was missing from libm (a cast-wrapper over `atan`, like `atan2f`).
2. `libc.dll` did not export `environ` (CMake's export-all list omitted it, unlike `optarg` or `timezone`); it is exported
   explicitly together with `--export-all-symbols`, because one explicit export turns lld's automatic export off.
3. `_fltused` was defined only in `libc.a`, so a DLL built with `dllcrt.o` could not link; a weak definition is in `dllcrt.c`.
4. A Windows executable always linked its own static `libc.a` while DLLs used `libc.dll`: two libcs in one process, each with its
   own malloc, TLS registry and initial-thread id. `crt-c++` now links the executable against `libc.dll` when
   `CRT_CXX_RUNTIME_LINKAGE=shared`, and a new `CRT_CXX_STL_LINKAGE` picks libc++ separately (not needed by the harness now).
5. `syscall(SYS_gettid)` returned ENOSYS on Windows, so WTF never recognised the main thread
   (`getpid() == syscall(SYS_gettid)`) and aborted in `initializeMainThread`; Windows now maps `SYS_gettid` and `SYS_getpid`
   as macOS does.
6. `munmap()` ignored its length and always released a whole VirtualAlloc allocation (failing for an interior address), so
   the 'reserve more, trim the head and tail' pattern of `WTF::OSAllocator` aborted. Partial unmaps are now emulated: a range
   whose neighbours are plain reservations is released and the rest re-reserved at the same addresses; otherwise the pages
   are decommitted and the address range stays reserved (documented limit, never a data loss). An anonymous `PROT_NONE`
   mapping is now a reservation that costs no commit charge, and `mprotect()` commits on demand.
7. The memory-map entry points took `unsigned long length`, 32 bits on Windows, so any mapping of 4 GiB or more was truncated
   (JavaScriptCore reserves multi-GiB regions); they take `size_t` now.
8. `pthread_getattr_np()` reported no stack on Windows, so `VM::setLastStackTop` aborted; it now reports the real stack from
   `GetCurrentThreadStackLimits` (the initial thread, and recorded at start for threads the CRT creates).

*Explained (was open): the missing `<sstream>` exports of `libc++.dll`.* Upstream libc++ turns the additional iostream explicit
instantiations (`basic_stringbuf`, `basic_stringstream`, `basic_ostringstream`, `basic_istringstream<char>`) off on Windows
(`__configuration/availability.h`, `!defined(_WIN32)`, LLVM PR41018): the library still defines them but `.drectve` exports only
the `dllexport`-annotated symbols (an explicit export disables lld's automatic export, which is why `/OPT:NOREF` and
`-export-all-symbols` changed nothing), and a Windows consumer instantiates them itself. A consumer that hides `_WIN32`, as
the Linux-shaped persona does, took the other branch, declared them `extern template` and could not link. It was a
configuration mismatch between library and consumer, not a missing export. A recipe patch to `libcxx/recipe.json`
(`CRT_TARGET_OS_WINDOWS` joins the condition, removal condition in the patch) gives CRT Windows consumers the library's own
configuration; JavaScriptCore.dll now links against the shared `libc++.dll` and the whole 1B acceptance passes with one libc
and one libc++ in the process (peak 73 MB, 150 cycles on 0/1/4/8 threads, audit clean).

*Decisions to take before the first Windows configure* (proposals, not yet decided):

1. *Platform persona.* macOS already presents itself to WebKit as a Linux-shaped CRT target. For Windows the choice is
   between that and upstream's WIN32 branches. The WIN32 branches include `<windows.h>` and the Microsoft C runtime
   throughout WTF, bmalloc and libpas, and this sysroot has neither by design (the boundary is the CRT's own libc and
   libc++, with real Windows SDK headers only scoped per file). The proposal is the same Linux-shaped persona as macOS,
   with the COFF assembly differences (offlineasm emits ELF `.type`/`.size`, as it did for Mach-O) as a narrow carried patch,
   and the audit above as the guard against any OS-owned dependency beyond the contract. Upstream WIN32 code stays a reference
   for specific mechanics (executable memory, thread suspension), not a build mode.
2. *Watchdog.* Two questions are separate: does a compiled infinite loop terminate on Windows (the 1C gate), and does the
   CRT's POSIX signal emulation deliver it the way Linux does (a PAL gate only if JavaScriptCore needs it). Windows has a
   software signal mask and a stub `sigsuspend`, and `jsc_watchdog_test.cpp` uses `ucontext_t`, `REG_RIP`, `dladdr` and
   `syscall(SYS_gettid)`. The first Windows run can use JavaScriptCore's polling traps (`JSC_usePollingTraps=true`) to
   separate a Baseline-JIT problem from a signal problem, and the signal path is then promoted to a gate on its own.
3. *Order.* Windows `jit_memory_test` (done), then the interpreter (1B), then the Baseline JIT (1C); no DFG/FTL/concurrent
   compiler threads before that, so a failure can be attributed.

**Linux/aarch64 replay, 1A/1B/1C (2026-10-05): green, after implementing native thread TLS for aarch64.**
Host: Ubuntu 26.04 aarch64 (a QEMU guest, 4 CPUs, kernel 7.0, 4 KiB pages), Clang 21.1.8, from a deleted `out/`.
Host prerequisites that a fresh machine lacked and the checklist did not name: `libc++-21-dev`, `libc++abi-21-dev`,
`libunwind-21-dev` (the build forces `-stdlib=libc++`, so the active Clang's libc++ must be installed) and `ruby`
(JavaScriptCore's offlineasm). The harness also needs the ICU port in `--deps-prefix`, which a clean tree does not
build by default (`port-build-icu`, `port-test-icu`), and `crt-ui-dist` needs `-DCRTUI_ENABLE_LVGL=ON`, whose
`crtui-lvgl-fetch` target cannot run while configure is failing (the fetch was run through `tools/fetch_lvgl.py`
directly, then reconfigured).

*Before the TLS work.* Clean build, exit 0, no warning from CRT sources (37 unused `-L` warnings from the port
builds, one in third-party `make`, and about 2,100 deprecated-builtin warnings from the imported libc++ headers under
Clang 21). C-stage ctest 113/114: only `pthread_native_tls_test` failed, every thread reporting the same TLS address.
JSC interpreter: the eight-group script and the 0- and 1-thread context cycles passed; the 4- and 8-thread cycles
died with `SIGTRAP` (WTF's `CRASH()` on arm64), the shared-TLS symptom this document predicted.

*The fix.* `libc/src/arch/linux/common/thread_tls.c` now builds the new thread's TLS on aarch64 (variant I; details in
`docs/linux_pthread_lifecycle.md`): header and dtv at the thread pointer, static blocks at the offsets read from the
initial thread's dtv, the thread pointer aligned to the strictest `p_align`, and the window below the thread pointer
(glibc's `struct pthread`, 0x720 bytes here, a loader internal that is not exported) copied at the same
thread-pointer-relative offsets and clamped with `mincore()`. `__crt_sys_clone_thread` already passed the TLS argument
in the aarch64 `clone` order. x86_64 behaviour is unchanged; both targets syntax-check clean.

*After.* `pthread_native_tls_test` passes, and fails when the setup is made to decline (mutation, restored); full
`ctest` 136/136; tooling unittests 102/102; `crt-ui-dist` and `verify_dist` pass for 03/04/05. JSC interpreter
acceptance passes (script peak RSS 69 MB, bound 450 MB; cycles on 0/1/4/8 threads, 150 each, 0 failures; host-ABI audit
clean). Baseline JIT acceptance passes: the assembly interpreter compiles nothing, the JIT script reports 2,340 Baseline
compilations (the same count as Linux/x86_64), the 1B script passes under the JIT, cycles on 0/1/4/8 threads pass, and
the watchdog terminates a compiled loop. Repeats: 150/150 watchdog runs, and 4- and 8-thread context cycles 20/20 in
both interpreter and JIT mode. The process has three shared-library TLS modules (`libJavaScriptCore`, `libc++abi`,
`libicuuc`), so the multi-module dtv path is exercised, not only the executable's own block.

*Not covered on aarch64.* DFG/FTL/WebAssembly and W^X (1D), the wrapper byte-for-byte comparison (it was an x86_64
check), and a shared-library TLS case in `pthread_native_tls_test` itself (JSC is the only multi-module evidence).

### 2. Linux WPE reference baseline

Build upstream WPE unchanged and render local HTML with its own built-in
backend. No CRT integration; the purpose is a known-good baseline to diff
against when `PlatformCRT` misbehaves.

### 3. `PlatformCRT` graphics and input prototype

**3A -- Linux prototype.** A WPEPlatform platform implementation (the reference
boundary, see `crtweb_porting.md`) that reaches shared-memory output: WebKit
rendering -> adapter -> `crtgfx` -> `crtui` external surface. It exists to prove
the surface and input contracts below, not to be the cross-platform
architecture. **3B+ -- the product:** `PlatformCRT` as a new WebKit port
(`OptionsCRT.cmake`, `PlatformCRT.cmake`, `platform/crt`, `UIProcess/crt`, ...),
then GPU-buffer output through the same external-surface contract.

**Source gate before 3B.** The pinned WPE tarball is the *reference* source: the WPE
baseline, the JSCOnly bring-up and the Linux 3A prototype. `PlatformCRT` is a new
cross-platform port, and the tarball lacks the Windows port entirely (no
`PlatformWin.cmake`, no Windows IPC backend). Before the first `PlatformCRT` file is
written, the full WebKit commit that the signed tag points at (`73f39d84...`,
already recorded in `recipe.json`) is pinned as a second source, role
*PlatformCRT product source*, with its own fetch-and-verify path, so the Mac and Win
ports can be read as references for process, IPC, font and input work instead of
guessing. The WPE tarball pin stays as it is.

**Licensing gate.** Per-file license headers are not scanned yet (Tranche 0 recorded
that honestly). The rule from here: before the first carried WebKit patch, scan the
files per file and keep a patch manifest that distinguishes a new CRT-owned platform
file from a modified LGPL/BSD WebKit file; Tranche 10 then closes it for release.

Two contracts are frozen before 3A starts, because `crtui` deliberately owns only
scene metadata (bounds, clip, opacity, z-order, damage, hit-testing) and not the
producer's frame:

1. **Frame producer.** `crtui` is the scene, `crtweb` is the browser frame
   producer, `crtgfx` is the imported resource and compositor. The producer
   contract states: acquire/release of a frame; width, height and pixel format;
   damage region and a damage serial; frame sequence and presentation
   acknowledgement; behaviour when the producer is blocked or the consumer is slow
   (frame-drop policy); shared-memory ownership; and, for the GPU path, buffer
   ownership with acquire/release synchronisation and fence/semaphore lifetime.
   WPEBuffer lifetime semantics must not leak into the CRT API.
2. **Input.** A web view needs more than `crtui`'s 12 keys and `SHIFT`: Ctrl/Alt/
   Meta, letters, digits and function keys, native virtual key/scan code, repeat,
   mouse button and click count, pointer type/id, and (later) composition.
   **Decided 2026-10-03:** a separate path -- `crtgfx` native event -> a `crtweb`
   input adapter -> WebKit -- with `crtui` supplying only focus, bounds and
   hit-testing. The frozen `crtui` v1 key enum is not extended for WebView.
   **IME/composition is deferred past Web v1** (Latin committed text and basic key
   input only), together with selection handling and touch.

### 4. `libcrtweb` and the WebView

`crtweb_runtime_create`, `crtweb_view_create/load_url/load_html/go_back/
go_forward/reload/set_size/set_focus`, and title/load/navigation callbacks,
plus `crtui`'s web view on top. Application code sees no WebKit type.

### 5. Multi-process lifecycle

UIProcess with WebProcess, NetworkProcess, and GPUProcess (GPUProcess is on by default in the WPE port): launch through WebKit's `ProcessLauncher`/`AuxiliaryProcess` on CRT PAL primitives (not `WPEProcessManager`, which is Android-only), IPC, clean
shutdown, forced WebProcess termination and recovery/reload, 100x view
create/destroy, navigation while resizing, resource-leak audit. Closing this on
Linux is the proof that the port's architecture holds.

### 6. Windows/x64 replay

The same WebKit and `PlatformCRT` on the CRT PAL (not a WPE-for-Windows
effort). Focus on process spawning, IPC handles, shared memory, executable
pages, D3D12 surface integration, fonts, basic key/pointer input (IME is deferred past v1), TLS, DLL boundaries.

### 7. macOS/arm64 replay

Same WebKit, same `PlatformCRT`, same `libcrtweb` API. Apple's native WebKit
framework is deliberately not used as a shortcut -- that would not be a
cross-platform CRT port.

### 8. CRT subsystem substitution

Only after a working browser. Replace, one at a time, the components WPE
brought with it: WebKit's network backend -> CRT networking; WebCore's media
backend -> `crtmedia`; capture -> `crtmedia` capture. Do not remove
GLib/libsoup/GStreamer in one step; that mixes a browser port with subsystem
rewrites.

### 9. GPU integration

Start with WebKit's Skia output to an external surface consumed by CRT's Skia.
Later optimize to a shared/zero-copy GPU buffer import. Sharing one Skia
context between WebKit and CRT is explicitly not an initial acceptance item.

### 10. Distribution and security closure

Isolated `06-web` build from the installed `05-ui` SDK: JSC/WebCore/WebKit ->
`libcrtweb` -> a browser sample, `verify_dist.py`, dependency/RPATH audit, and
publication only after all three hosts pass. Include the provenance/SBOM/CVE
policy artifacts named in Tranche 0.

## Host order

Linux first (WPE is the reference), with an early three-host JavaScriptCore
replay; then Windows/x64; then macOS/arm64.
