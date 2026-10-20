# crtweb Acceptance (Stage `06-web`)

**Status: in progress. Tranches 0-9 are closed, including the public `crtweb`
API and `crtui` WebView plus public-API/process lifecycle acceptance on
Linux/x86_64, macOS/arm64 and Windows/x64. GPU integration is accepted (Tranche 9); a packaged
`06-web` SDK follows in Tranche 10.** Detailed contract and tranche order for the
WebKit-based web runtime. It depends on `05-ui`'s External Surface contract
([`crtui_acceptance.md`](crtui_acceptance.md)), which is accepted on all three
hosts (2026-10-03). Upstream mapping lives in [`crtweb_porting.md`](../porting/crtweb_porting.md). Evidence is recorded
here as each tranche closes; completed work goes to [`HISTORY.md`](../../HISTORY.md)
and the current tranche state to [`TODO.md`](../../TODO.md).

## Purpose

Build a **WebKit CRT Port** (`PlatformCRT`) -- not a straight port of WPE to
three operating systems. WPE WebKit is the reference implementation to compare
against and to borrow structure from; WebKit already has separate ports (Apple,
GTK/WPE, PlayStation, Windows, JSCOnly), so a CRT port fits its architecture.

```text
                 WebKit
  +--------------------------------------+
  | WTF  JavaScriptCore  WebCore  WebKit |
  |     WebProcess  NetworkProcess        |
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
The GPU-resource process is the separate CRT-owned `crtweb_gpu_worker`, not
WebKit's upstream GPUProcess; the adopted lifecycle boundary is defined in
[the 9C policy](../design/libcrtweb_api_policy.md#owned-gpu-process-lifecycle-tranche-9c).

## Scope

Required for v1: HTML, CSS, DOM, JavaScript, navigation, HTTP/HTTPS, images,
fonts/text, mouse/keyboard, scrolling, Canvas 2D, basic cookies/storage. Video camera capture with
application-owned permission decisions is an approved extension (2026-10-14), gated independently
in Tranche 8C. Synthetic and physical-camera paths, permission decisions and repeated
same-process lifetimes are accepted on Linux/x86_64, macOS/arm64 and Windows/x64,
within the device/resource scope recorded below.

Deferred: WebRTC, WebGPU, WebXR, EME/DRM, microphone/display capture, printing,
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
  prerequisite, not a JSCOnly one. ICU's data-generator executables also belong
  on this side even though the libraries they generate data for are target
  dependencies. None of these executables belongs in the SDK or target ABI audit.
- *Target dependencies* are built against the CRT target sysroot and shipped: CRT
  pthread/libc/libc++, ICU (`data`, `uc`, `i18n`, >= 70.1), JavaScriptCore and
  later WebCore/WebKit. ICU comes from the CRT port `porting/recipes/icu.json`,
  never the host ICU (fast on Linux, wrong on Windows/macOS).

For target dependencies built with `configure`/Make, record the shell and Make identities as well.
A native build-host shell/Make run can establish target-library behavior when the CRT toolchain
and ABI audits pass, but does not verify the CRT shell/Make or Windows SDK self-hosting. Those
claims require a successful CRT mksh/Make replay; an alternate-tool comparison cannot close a
failure observed in that path.

Today host and target are the same machine and architecture, so the ports are
ordinary CRT executables that run during the build. That does not survive a cross
build (an x86_64 build host producing a Linux/aarch64 SDK cannot run an aarch64
`gperf` or ICU's data generators), so before `06-web` becomes the embedded product
stage the build must separate the two: build tools built for, and run on, the build
machine; target libraries built for the target. This design boundary is fixed
here, but the complete x86_64-host -> Linux/aarch64-target proof belongs to the
isolated stage chain in Tranche 10, not to Tranche 1.

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
| 1D-B | FTL/B3 | the B3/Air backend, larger compile jobs (all four acceptance hosts done) |
| 1D-C | WebAssembly | wasm tiers, their memory/trap handling (all four acceptance hosts done) |
| 1D-D | sampling profiler (`ENABLE_SAMPLING_PROFILER`, a build option) | signal-based suspension of arbitrary VM threads (all four acceptance hosts done) |

W^X is hardening, not a tier. Order: Linux/x86_64 first, so a failure is a JSC-tier problem and not a signal
emulation or persona problem, then Linux/aarch64, macOS/arm64 and Windows/x64. **1D-A gates Tranche 2 at minimum.** The pinned WebKit
enables FTL, WebAssembly and the sampling profiler by default on x86_64/arm64, so 1D-B..1D-D close JSC's default
capabilities. They are complete on Linux/x86_64, Linux/aarch64, macOS/arm64 and Windows/x64; W^X, SIMD and Wasm threads
remain separate hardening rather than `PlatformCRT` architecture blockers.

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
the result. At that point macOS/arm64 and Linux/aarch64 replays could proceed independently;
sampling-profiler replays, W^X, SIMD and Wasm threads remained separate work.

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
to Web/JSC. At that point Linux/aarch64 and macOS/arm64 remained native replay gates.

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
1D-C only if it reproduces. At that point Linux/aarch64 remained after the
macOS/arm64 replay; its completed replay is recorded below.

**1D-B, 1D-C and 1D-D replay, macOS/arm64 (2026-10-09).** Fresh
`baseline-jit` and `sampling-profiler` trees were built from the verified WPE WebKit 2.54.0
archive against the regenerated and verified installed `05-ui` SDK. The baseline tree closed
FTL/B3 and WebAssembly. The profiler tree then reran the complete 1B..1D-C sequence before its
own gate. FTL produced 153 reports at the reduced threshold, 107 at default thresholds and 252
with serial compilation; its negative control produced zero. Wasm produced 21 BBQ and 5 OMG
code reports, passed interpreter-only and software-bounds modes, and the C API signal probe
proved 50 fast-memory fault deliveries versus zero in bounds-check mode. The 0/1/4/8-thread
context cycles, JS and Wasm watchdogs, earlier-tier scripts and Mach-O host-ABI audit all passed.

The first FTL run found an object-ABI mismatch in the Linux-shaped WebKit persona. Its Linux
ARM64 register table makes x18 allocatable, but Darwin arm64 reserves x18 as the platform
register. Under B3/Air pressure generated code preserved a value there across a runtime call
and later faulted near `operationStringFromCharCode`. Carried patch
`0004-darwin-arm64-reserved-x18` is selected only by
`WTF_CRT_DARWIN_ARM64_ABI` on macOS/arm64; it chooses WebKit's existing reserved-x18 table
without enabling Apple SDK source branches. Linux/aarch64 is unchanged. The manifest records
the exact before/after hashes and removal condition.

The first profiler worker run then exposed the same contract gap previously found on Windows:
macOS learns a kernel-allocated worker stack only in the target wrapper, but `pthread_create()`
could return before that wrapper had stored the bounds. WebKit immediately queried
`pthread_getattr_np()`, observed a null base and derived `0x100000` as the stack origin, then
faulted in `sanitizeStackForVM`. The wrapper now release-publishes the Apple-reported bounds and
wakes the creator; the creator publishes the Bionic-shaped `pthread_t` only after that point.
The detached-thread lifetime handshake covers macOS as well as Windows.

The official profiler gate sampled the main VM and four `$262.agent` worker VMs: **5/5 profiles,
1,167 traces and 1,167 named hot frames**. Twenty focused repeats passed 20/20 with all five VMs
and minima of 1,156 traces and hot frames. `pthread_stack_bounds_test`,
`pthread_thread_test` and `pthread_detach_test` each passed 100 consecutive repetitions; the
full macOS CTest passed 125/125. This closes 1D-B/C/D on macOS/arm64. The final
Linux/aarch64 replay follows below; W^X, SIMD and Wasm threads remain separate work.

**1D-B, 1D-C and 1D-D replay, Linux/aarch64 (2026-10-09).** Regenerated and
verified the cumulative `05-ui` SDK from `a8a20fa`, then built fresh
`baseline-jit` and `sampling-profiler` trees from the verified WPE WebKit 2.54.0
archive on Ubuntu 26.04/aarch64 with Clang 21.1.8. The baseline tree passed every
1B..1D-C gate. FTL produced 158 reports at the reduced threshold, 108 at default
thresholds and 252 with serial compilation; its negative control produced zero.
Wasm passed interpreter-only, BBQ, BBQ+OMG and software-bounds modes with 21 BBQ
and 7 OMG reports. The C API signal probe proved 50 fast-memory fault deliveries
versus zero in bounds-check mode. Context cycles on 0/1/4/8 threads, JS and Wasm
watchdogs, RSS bounds and the ELF host-ABI audit were green.

The separate profiler build reran the complete 1B..1D-C sequence and again
proved FTL (157/108/252), Wasm BBQ/OMG (21/8) and the 50/0 fast-memory signal
distinction. Its official profiler gate sampled the main VM and all four worker
VMs with **5/5 profiles, 297 traces and 296 named hot frames**. Twenty focused
repetitions passed 20/20 with every VM sampled and minima of 274 traces and 271
named hot frames. Full Linux/aarch64 CTest passed 148/148, the JSC harness tests
passed 3/3, and distribution verification passed for the cumulative 03/04/05
stages. No CRT, WebKit patch or harness change was needed. This closes
1D-B/C/D on Linux/aarch64 and therefore closes JavaScriptCore Tranche 1 on all
four acceptance hosts. W^X, SIMD, Wasm threads and Windows/arm64 JSC remain
separate non-gating follow-ups.

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
   (`docs/design/linux_pthread_lifecycle.md`).
2. *An executable linked its own `libc.a` while the shared libraries used `libc.so`*, so a
   process had two libcs with separate thread/key registries and allocators (a library's
   `pthread_getspecific` returned the main thread's value in every worker; heap panics).
   Under shared runtime linkage (`CRT_CXX_RUNTIME_LINKAGE=shared`) `crt-c++` now links
   `libc.so`/`libm.so`/`libdl.so` into the executable too.
3. *`siginfo_t` had no `si_addr`, `ucontext_t` was a private blob and handlers got a null
   context*: Bionic/kernel layouts and real forwarding on Linux (`docs/design/signal_delivery.md`).
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
   names in its tests (see `docs/porting/porting_status.md`). The libc++ recipe now searches the install prefix first on
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
   arm64 context to the Bionic/Linux aarch64 layouts with register write-back (`docs/design/signal_delivery.md`).
2. *Thread stack bounds.* A thread created by `pthread_create` with only a stack size had no recorded base on
   macOS, so `pthread_getattr_np()` reported a null stack; JavaScriptCore's soft limit then put the LLInt frame
   zero-fill at address 0x800000 and every worker-thread context cycle died with `EXC_BAD_ACCESS`. The new thread
   now records the bounds Apple reports (`docs/design/pthread_policy.md`).
3. *Bionic surface.* `getauxval()` (a macOS stub, `AT_HWCAP` etc. report no features), `<asm/hwcap.h>`,
   `gettid()`/`SYS_gettid` mapping (1B), real `getrusage()` (1B).
4. *Tests.* `jit_memory_test` (RWX and reserve-then-commit on Apple Silicon), `signal_threads_test` (the real macOS
   signal cases), `pthread_native_tls_test`, `bionic_surface_test`, and `jsc_watchdog_test`/`jsc_context_cycle`
   run on macOS.

*Open on macOS at the time 1C closed.* x86_64 macOS has neither the signal conversion nor JIT
permissions. The arm64 DFG/concurrent-JIT replay closed in 1D-A and its FTL/WebAssembly/profiler
replays closed on 2026-10-09 above. The isolated stage-chain replay remains Tranche 10.

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
VM traps are real signals on the CRT's per-thread signal implementation (`docs/design/signal_delivery.md`, "Windows:
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
`docs/design/linux_pthread_lifecycle.md`): header and dtv at the thread pointer, static blocks at the offsets read from the
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

*Not covered in that 1A/1B/1C replay.* DFG/FTL/WebAssembly and W^X, the wrapper
byte-for-byte comparison (it was an x86_64 check), and a shared-library TLS
case in `pthread_native_tls_test` itself (JSC is the multi-module evidence).
DFG/FTL/WebAssembly and the sampling profiler are now closed by the 1D replays
above; W^X remains separate hardening.

### 2. Linux WPE reference baseline

**Closed 2026-10-10 on Linux/x86_64.** The repository-owned entry point is:

```sh
python3 tools/build_webkit_wpe_reference.py \
  --native-sysroot out/web-reference/sysroot
```

`--native-sysroot` is an optional lookup root for unpacked *native Linux*
development packages; it is not a CRT sysroot. The tool verifies the pinned
2.54.0 archive (SHA-256
`efa9bcc3cb891c2d88f50eec710d9ccee71cbdf1040420361eb98c17355eb452`),
extracts it without changes, and rejects any source file named by the current
CRT patch manifest unless it still has its upstream `sha256_before`. It builds
WPE WebKit, MiniBrowser, the Web/Network processes, inspector resources and the
injected bundle with the host compiler, then installs them under the work tree.
The private install is significant: an upstream Release build intentionally
ignores `WEBKIT_EXEC_PATH` and uses its compiled-in process locations.

The reference enables WPEPlatform and only its built-in headless display;
legacy WPE, DRM-display and Wayland backends are off. `USE_LIBDRM` remains on
without enabling the DRM backend because the supported WPE snapshot/buffer path
uses DRM pixel-format definitions. Video and WebCodecs remain in their upstream
supported relationship. No CRT header, library, SDK or carried WebKit patch is
used.

The fixture is `libcrtweb/tests/wpe-reference/reference.html`. The native C11
harness explicitly creates and connects `wpe_display_headless_new()`, constructs
the web view for that display, resizes its toplevel to 640x480, loads the local
file, and requires both:

- DOM/canvas proof: `CRT WPE reference|local-html-ok|42|192x96`.
- Visible `WebKitImage`: 640x480, valid BGRA8888 storage, and at least four
  distinct rendered colors before conversion to PPM.

Accepted output on Ubuntu 26.04.1/x86_64 (GCC 15.2.0, CMake 4.2.3, Ninja
1.13.2): `WPE_REFERENCE_SNAPSHOT width=640 height=480 stride=2560
fnv1a64=da2864e59e05c36f`. The PPM SHA-256 is
`efef2b53b70d6882cda4ad00d4bccfa12c2c11d1cc65bfd98ebb250d980a51c3`.
`out/web-reference/configure-fingerprint.json`, `result/result.json` and
`result/wpe-reference.ppm` retain the local evidence. The exact pixel hash is
observational (fonts and native graphics stacks can vary); the portable pass
gate is the proof string, dimensions, storage validity and multi-color frame.
This is the known-good native baseline to diff when `PlatformCRT` misbehaves.

### 3. `PlatformCRT` graphics and input prototype

**3A -- Linux prototype.** A WPEPlatform platform implementation (the reference
boundary, see `crtweb_porting.md`) that reaches shared-memory output: WebKit
rendering -> adapter -> `crtgfx` -> `crtui` external surface. It exists to prove
the surface and input contracts below, not to be the cross-platform
architecture. **3B+ -- the product:** `PlatformCRT` as a new WebKit port
(`OptionsCRT.cmake`, `PlatformCRT.cmake`, `platform/crt`, `UIProcess/crt`, ...),
then GPU-buffer output through the same external-surface contract.

**Source gate before 3B -- closed 2026-10-10.** The pinned WPE tarball is the *reference*
source: the WPE baseline, the JSCOnly bring-up and the Linux 3A prototype. `PlatformCRT` is a new
cross-platform port, and the tarball lacks the Windows port entirely (no `PlatformWin.cmake`, no
Windows IPC backend; it also lacks `OptionsWin.cmake`, `PlatformMac.cmake` and 35,810 other `Source/`
files). The full WebKit tree at the commit the signed tag points at (`73f39d84...`) is therefore
pinned as a second source, role *PlatformCRT product source*, in `recipe.json`
(`product_source`) with its own fetch-and-verify path, `tools/fetch_webkit_commit.py`:

- *What is pinned.* The commit id, its **tree id** (`6d856c31...`) and the SHA-256 of the complete
  recursive listing (461,979 entries). A git commit/tree id is a hash over every directory and file,
  so the three together pin every byte of the tree and also detect an added or dropped file.
- *Fetch.* One depth-1, trees-only (`--filter=blob:none`) fetch of that commit, about 14 MB and a
  couple of seconds; `--checkout` materialises only `product_source.checkout_paths` (`Source/`:
  69,574 files, 1.2 GB, 30 s) and fetches their blobs in one batch. The tool verifies the commit and
  tree ids and the listing, and (online) that the tag still peels to the pinned commit. Nothing is
  patched in place.
- *The tarball against the commit.* `--correspondence` hashes every file of the release tarball as a
  git blob and compares it with the tree. Result: of 38,842 files, **34,417 are identical, 130 are
  identical after CRLF->LF (ANGLE Windows shader headers and one `.bat`, converted by `.gitattributes`
  when the tarball was made), 0 differ in content**, and 4,295 exist only in the tarball (generated
  `Documentation/` and `NEWS`). So the tarball is a strict subset of the tagged tree; this replaces
  the six-file sampling recorded in Tranche 0. The 35,810 `Source/` files only the commit has are the
  Windows, Mac, iOS, PlayStation... ports.

**Licensing gate -- closed 2026-10-10.** Per-file headers are now scanned
(`tools/scan_webkit_headers.py`, result committed as `libcrtweb/third_party/webkit/license-scan.json`):
69,573 files under `Source/` of the pinned commit, classified from SPDX tags and standard license
texts; anything else is reported as unclassified, never guessed.

| Class | Files | Where it matters |
|---|---:|---|
| BSD-2-Clause | 25,041 | Apple's and most WebKit code (WTF 704, JSC 3,292, WebCore 12,108, WebKit 5,951) |
| BSD-3-Clause | 19,534 | Skia, ANGLE, Chromium-derived code, parts of WebCore |
| LGPL-2+ / 2.1+ / 2 / 2.1 | 3,138 | WebKit's own LGPL code (WebCore 2,045, WebKit 672+42, JSC 197) |
| Apache-2.0, MIT, MPL, Unicode/ICU, ISC, ... | ~4,200 | bundled components (pdf.js, mimalloc, ICU headers, ...) |
| GPL / GPL-with-exception | 23 (+1 `GPL-2.0` in a `.clang-format`) / 14 | **none are WebKit code**: ANGLE's flex/bison skeletons (build-time tools) and bison-generated parsers carrying the Bison exception, yasm in libwebrtc, `WebCore/xml/XPathGrammar.*` (bison output) |
| no header | 13,036 | build files, data, tests; inherits the directory license file (e.g. JSC `COPYING.LIB`) |
| unclassified with a copyright line | 1,693 (85 in the components a port is built from, listed) | Info.plist/ICU/mimalloc/WebCore Cocoa and XR headers: to be classified by hand before they ship |

What the gate decides for PlatformCRT: WebKit's own code is BSD-2 or LGPL-2(.1)+, both of which CRT
can modify and redistribute with notices and source correspondence (already `notices_required_in_sdk`
and `source_correspondence_required` in the recipe); no GPL file belongs to a port's runtime, and the
build must keep the bundled GPL tools (ANGLE flex/bison, yasm) out of the product. The 85 unclassified
core files and the bundled components' notices go into the Tranche 10 notice/SBOM closure.

**Patch manifest.** `libcrtweb/patches/manifest.json` is schema 2: every modified WebKit file
records its license class (all 17 current files are BSD-2-Clause; `JavaScriptCore/CMakeLists.txt` has
no header and records `COPYING.LIB` as its directory-level license basis), the policy is stated in
the file, and a `new_files` list is where CRT-owned platform files are declared (origin `crt`, the
project default license from `LICENSE.md`, and the header must be present). A patch is a derivative
under the modified file's license; CRT code is never mixed into an LGPL file beyond the patch.
`tools/check_webkit_patch_manifest.py [--tree DIR]` enforces it (re-measuring classes and
`sha256_before` against a tree) and is covered by `tools/test_webkit_licensing.py`.

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

**Contracts frozen for 3A (version 1, 2026-10-10).** The field-level definition is the
header `libcrtweb/platform/crtweb_surface_wire.h`: plain C, fixed-width little-endian structs, no
WebKit, GLib or CRT type, so the native WPE side and a CRT application include it unchanged. Both
contracts are carried by one `AF_UNIX`/`SOCK_SEQPACKET` connection (one datagram per message,
`SCM_RIGHTS` for buffer descriptors); the consumer listens, the producer connects.

*Frame producer.*

- **Roles.** The consumer (a `crtui` application, through `crtweb::SurfaceClient`) sends HELLO with
  the view's logical size and scale; the producer answers CONFIG (version, pool size 3, pixel
  format). `crtui` keeps only the SurfaceView's bounds, clip, opacity, z-order, damage and
  hit-testing; the application associates the view id with the client's image
  (`crtui_skia_surface_provider`).
- **Pixels.** `AR24` (DRM_FORMAT_ARGB8888): B,G,R,A bytes, alpha premultiplied, origin top-left, a
  row stride in bytes, device pixels (logical size x scale).
- **Ownership and lifetime.** The producer owns a pool of three memfd-backed buffers and announces
  each with BUFFER_ADD (descriptor, size, stride, format). A FRAME names a complete, immutable
  picture; the consumer owns that buffer until it sends BUFFER_RELEASE, and the producer never writes
  it meanwhile. The producer retires buffers (BUFFER_REMOVE) only after release, e.g. on resize.
  No `WPEBuffer` is visible to the consumer: WebKit's buffer is copied into the pool, so WPE's
  buffer lifetime rules stop at the producer.
- **Damage and serials.** `serial` (frame) and `damage_serial` strictly increase; `damage` is
  advisory and the buffer is always a complete picture (3A reports full damage).
- **Acknowledgement and pacing.** FRAME_ACK(serial) tells the producer the frame was composed;
  only then does WPE see `buffer_rendered`, which is what lets WebKit start the next frame. At most
  one frame is in flight.
- **Slow or stuck consumer (the drop policy).** The producer waits at most 250 ms for an
  acknowledgement and then carries on, so page script and timers never freeze. Commits that arrive
  while a frame is in flight, or while the consumer holds all three buffers, coalesce into one
  pending commit, *latest wins*: no unbounded queue, no stale frame shown late. When the consumer
  releases a buffer the pending picture is presented.
- **Resize.** RESIZE(width, height, scale) from the consumer; the producer answers with buffers of
  the new size (the old ones retire after release).
- **Shutdown.** BYE or end of stream closes the view on the producer; the consumer treats the same
  as the page going away.
- **Not in v1.** GPU buffers (descriptor planes, fence/semaphore lifetime) are Tranche 9 and will
  extend BUFFER_ADD/FRAME; cursor, clipboard and drag are Tranche 4.

*Input.* A separate path (the Tranche 0 decision stands): `crtgfx` native event -> the client's
`forward()` -> wire -> WPE event; `crtui` supplies only focus, bounds and hit-testing, and its frozen
v1 key enum is not extended.

- **Pointer.** Action (move/down/up/enter/leave), pointer type and id (mouse, pen, touch), button
  (1 primary, 2 middle, 3 secondary), press count (1/2/3), modifiers, position in *view-local logical
  pixels*; `forward()` converts window coordinates and drops positions outside the view.
- **Wheel.** Position and pixel deltas.
- **Keys.** X11/XKB *keysym* plus the *XKB keycode* (Linux evdev code + 8: `KEY_A` 30 -> 38), modifiers
  Ctrl/Shift/Alt/Meta/CapsLock, repeat flag. WebKit derives `key` from the keysym and `code` from the
  keycode, so both are carried; a keycode of 0 makes WebKit drop the event. A non-Linux consumer maps
  its native scan code to the evdev numbering before sending.
- **Text.** Committed Latin text (one grapheme per TEXT message) becomes a press/release of its
  keysym on the key a US layout types it with. **IME/composition, selection and touch gestures are
  deferred past Web v1**, as decided before 3A.
- **Focus.** FOCUS(focused) becomes `wpe_view_focus_in/out`.

**3A -- result (Linux/x86_64, 2026-10-10).** Three pieces in two worlds, and no WebKit patch:

- `libcrtweb/platform/wpe-crt/wpe_crt_platform.c`: a GIO module that registers a `WPEDisplay`
  named `crt` on WPE's `wpe-platform-display` extension point. A platform can therefore live
  outside the WebKit tree (the 3A question in `crtweb_porting.md` is answered): it is selected by
  `WPE_DISPLAY=crt` and `WPE_PLATFORMS_PATH`. It provides display, toplevel and view classes,
  copies each committed WPE shared-memory buffer into the pool, forwards input as WPE events, and
  exposes no EGL/DRM device, so WebKit renders into shared memory.
- `libcrtweb/platform/crtweb_surface_client.h`: the consumer-side adapter (C++17, header-only). The
  newest frame is a Skia raster image over the mapped buffer; its release proc returns the buffer;
  `frame_presented()` acknowledges; `forward()` maps `crtui` pointer, wheel, key and text input.
- Tests. `tools/build_webkit_wpe_crt.py` builds the module and a native host
  (`libcrtweb/tests/platform-crt/wpe_crt_host.c`, a `WebKitWebView` on the `crt` display) against
  the Tranche 2 reference build, builds `surface_probe.c` *with the CRT toolchain* from the installed
  SDK, and runs them: the probe listens, the host connects. Checks: handshake and CONFIG; the first
  640x480 frame's four fixture quadrants; serials increasing; ack/release; pointer down/up ->
  page `click` report *and* the pixel effect in a later frame; key `a`, Ctrl+Shift+`b` (code and
  modifiers), wheel, committed text into a focused field; resize to 320x240 (new buffers, proportional
  quadrants); and a **stalled consumer** (no acknowledgement, no release, six repaints): the page keeps
  reporting presses, the consumer ends up holding all three buffers, and after recovery a new frame
  arrives. Passed 4 of 4 consecutive runs (about 4 s each). The page's `document.title` is the report
  channel (the embedder, here the host program, forwards title changes to
  `wpe_toplevel_set_title`; WebKit does not).
  `crtweb_web_surface_test` (CTest, hermetic, CRT-built with Skia) drives `SurfaceClient` against a
  fake producer without a browser: a SurfaceView composed between UI below and above, acknowledgement,
  release when an image is dropped, latest-frame-wins, damage serial, title, input translation,
  resize; it fails when a pixel expectation is mutated.

*What the CRT runtime lacked, found by building the consumer with it:* `SOCK_SEQPACKET`,
`MSG_TRUNC`, `MSG_CTRUNC`, `SOCK_RDM` (`<sys/socket.h>`) and `socketpair()` (Linux and macOS
syscalls; ENOSYS on Windows). `socketpair_test` covers stream and seqpacket pairs, `SOCK_CLOEXEC`
and record truncation.

*Honest limits.* The producer copies each frame (one memcpy of the buffer per commit; fine for
software output, replaced by buffer import in the GPU tranche); the prototype transport is
Linux-only (`SOCK_SEQPACKET`; Windows and macOS need their own transport decision, e.g. a
named-pipe/stream framing, before Tranches 6 and 7; both were then made in CRT libc, below); keys other than US-layout Latin text have no
IME path; the consumer test runs without a window or GPU (raster target), so live crtgfx
presentation of a web SurfaceView is not shown yet. The two host-side gates below (full-commit pin,
per-file license scan and patch manifest) are closed (see above). The product
port subsequently completed B1-B4 below; B5 first render is the active step.

#### 3B plan -- the `PlatformCRT` port (decided 2026-10-10)

*What the port is.* A new WebKit port, `-DPORT=CRT`, built from the pinned full commit
(`product_source`) plus CRT-owned files, with as few carried patches as possible. WebKit already
selects everything per port by *file name* (`Source/cmake/Options${PORT}.cmake`, and
`Platform${PORT}.cmake` in each module through `WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS`) and by
`-DWTF_PLATFORM_<PORT>=1`, which is how the PlayStation port declares itself. So a port is mostly
*new files*; the only change to an existing file so far is registering the name (patch 0005, one
line). New files live in `libcrtweb/port/` (mirroring the WebKit tree), are declared under
`new_files` in the patch manifest, and are copied over a hard-linked copy of the pinned checkout by
`tools/build_webkit_jsc.py --port CRT`; the checkout itself is never modified.

*Which port it follows.* Measured on the pinned tree, not assumed:

| Port | Shape | Port-specific files | Verdict |
|---|---|---:|---|
| WPE/GTK | GLib main loop, GIO, libsoup, GStreamer, WPEPlatform | 272 (WPE) / 528 (GTK) + 499 under `glib` | the Tranche 2 *reference*, not the model: GLib would have to be ported to every host |
| Win | Win32, curl, Skia, Windows IPC backend | 182 | the model for Windows-specific parts (IPC `win`, process launch) at Tranche 6 |
| **PlayStation** | POSIX, **generic run loop**, curl, Skia, C embedding API (`WKView`), no GLib | **59** (WTF 4, WebCore 16, WebKit 36, JSC shell 2, WebDriver 1) | **the structural model**: the same shape as CRT's three-host runtime |

*Decisions.*

- D1 **Structure after PlayStation**: generic `RunLoop`/`WorkQueue`, POSIX threading and file system
  from WTF's `posix/`, `unix/`, `generic/`; curl network; Skia painting; no GLib, GIO or GObject in the
  port. The 59 PlayStation files give the surface to replace (system fonts, theme/scrollbar, screen,
  MIME types, user agent, accessibility stubs, resource-usage stubs, process launcher, page client,
  web view, process entry points).
- D2 **Identity**: `-DBUILDING_CRT__=1 -DWTF_PLATFORM_CRT=1`; `PLATFORM(CRT)` then works with no WTF
  patch. `OS(LINUX)` persona on all hosts, as in the JSC bring-up (patches 0001-0004 carry over).
- D3 **Process model**: UI, Web and Network processes (GPU process off until Tranche 9); upstream
  `Platform/IPC/unix` on Linux and macOS (the 3A transport already uses SOCK_SEQPACKET with
  `SCM_RIGHTS` on CRT libc); upstream `Platform/IPC/win` is available for Windows (Tranche 6).
- D4 **Graphics**: WebKit's bundled Skia (m154, its own copy) painting on the CPU in the Web process
  into shared memory first, handed to the UI process and out through the frozen 3A frame contract;
  GPU buffers are Tranche 9. WebGL, WebGPU, ANGLE, media and WebRTC are off.
- D5 **Network**: WebKit's libcurl backend. It needs OpenSSL (`OpenSSLHelper`, `CurlSSLVerifier`,
  `crypto/openssl`), and libcurl must be OpenSSL-backed (`CURLOPT_SSL_CTX_FUNCTION`), which CRT's curl
  port (mbedTLS) is not. OpenSSL and an OpenSSL-backed libcurl are new CRT ports for this stage; the
  `crtmedia` networking stack is untouched. Replacing the backend with CRT networking is Tranche 8.
- D6 **Fonts**: ~~no fontconfig~~ *revised at B3*: the bundled Skia's CMake and WebKit's Skia font
  backend (`FontCacheSkia.cpp`, `SkiaSystemFallbackFontCache.cpp`) use fontconfig on every platform
  that is not Android or Windows, so keeping it out needs a WebKit patch. fontconfig is a CRT port
  (`porting/recipes/fontconfig.json`); the font set is whatever the embedder's `FcConfig` scans
  (`FONTCONFIG_FILE`, later `crtweb`'s own), which for the tests is `libcrtgfx/assets/fonts`. The
  CRT `SystemFontDatabase` stays the standard-family stub of the PlayStation file until B5.
- D7 **Embedding**: following PlayStation, a C view API (`WKView`-style) inside WebKit, wrapped by
  `libcrtweb`'s own C API in Tranche 4; no WebKit, WPE or GLib type is public.
- D8 **Dependencies are CRT ports** (AGENTS.md: no host-library shortcut). Needed beyond what
  exists (ICU, zlib, libpng, SQLite, FreeType, curl): HarfBuzz (+ICU), libjpeg(-turbo), libwebp
  (+demux), libxml2, libpsl, OpenSSL; libxslt (`ENABLE_XSLT`) and woff2/brotli (`USE_WOFF2`) start
  disabled.

*Milestones.* Each has a gate that is a command and a measurement; Linux/x86_64 first, and a milestone
is not closed on another host until it passes there (Tranches 6/7 own those replays).

| | Milestone | Gate |
|---|---|---|
| B0 | Source and licensing gates | closed above |
| **B1** | Port skeleton: `PORT=CRT` configures and builds JavaScriptCore from the pinned commit plus overlay | **done 2026-10-10; replayed 2026-10-11 on macOS/arm64 and 2026-10-12 on Windows/x64 and Linux/aarch64**: the 1B/1C JSC acceptance passes through `--port CRT` |
| **B2** | Dependency ports (D8): HarfBuzz, libjpeg-turbo, libwebp, libxml2, libpsl, OpenSSL, OpenSSL-backed libcurl | **done 2026-10-10 (Linux/x86_64), replayed 2026-10-11 (macOS/arm64) and 2026-10-12 (Windows/x64, OpenSSL and libcurl static; Linux/aarch64)**: each recipe configures, builds, installs and passes its run test, and WebKit's own `find_package()` calls resolve all of them (`tools/check_web_dependencies.py`) |
| **B3** | WebCore and PAL build under `PORT=CRT` with the bundled Skia: CRT replacements for the WTF/WebCore platform files | **done 2026-10-11 (Linux/x86_64), replayed on macOS/arm64, Windows/x64 and Linux/aarch64 (2026-10-12)**: `WebCore` and `PAL` link into a test program that starts WTF, creates a `Page`-less WebCore object graph and paints a Skia surface; no GLib symbol in the link (`tools/build_webkit_crt_port.py --run-tests`) |
| **B4** | WebKit multi-process: UIProcess/WebProcess/NetworkProcess for CRT, process launcher, IPC over Unix sockets | **done 2026-10-12 (Linux/x86_64)**: the UI process launches the Web and Network processes and a message round-trips; killing a child is detected (`crtweb_process_test`) |
| **B5** | First render | **done 2026-10-12 (Linux/x86_64), replayed on Windows/x64 and macOS/arm64 (2026-10-13)**: `reference.html` renders, the proof string is the WPE reference's, the frame reaches `surface_probe` through the 3A contract (`crtweb_render_test`, `--webkit --run-tests`) |
| **B6** | Input | **done 2026-10-12 (Linux/x86_64), replayed on Windows/x64 and macOS/arm64 (2026-10-13)**: `surface_probe`'s full scripted run passes against `crtweb_surface_host` (the CRT port's frame producer) instead of the WPE module |
| **B7** | Close | **done 2026-10-13 (Linux/x86_64), replayed on Windows/x64 and macOS/arm64 (2026-10-13)**: 100x create/destroy without growth, host-ABI audit clean (static and runtime), patch manifest and license scan re-run, docs and `HISTORY.md` updated |

*B2 result (Linux/x86_64, 2026-10-10).* Seven recipes with `"group": "web"` (`libjpeg-turbo` 3.2.0,
`libwebp` 1.6.0, `libxml2` 2.15.4, `libpsl` 0.23.3, `harfbuzz` 14.6.0, `openssl` 3.5.9,
`curl-openssl` 8.21.0), each with a run test (details in `docs/porting/porting_status.md`). The
`openssl` test completes a TLS 1.3 handshake in process and the `curl-openssl` test fetches HTTPS from
an in-process TLS server and checks the `CURLOPT_SSL_CTX_FUNCTION` hook WebKit's curl backend relies
on. `tools/check_web_dependencies.py` runs WebKit's own Find modules (from the pinned product source)
for all eleven requirements of the Win/PlayStation ports against the prefix with a cross toolchain
file and pkg-config limited to the prefix: all resolve, none to a host path. What the loop found and
fixed in CRT: `getpeername`, `SO_TYPE`, `struct linger`, `getnameinfo`/`NI_*`,
`getservbyname`/`getservbyport`, `inet_ntoa` (`docs/porting/bionic_libc_gaps.md`); in the port tooling:
a `cmake` build system, `configure_script`, `install_subdir`, `make_targets`/`install_targets`,
`@CRT_ARCH@`, per-test `library_dirs`. Decisions worth keeping: libjpeg-turbo, not IJG libjpeg
(Skia's JPEG codec uses the turbo API); OpenSSL 3.5 LTS, not 3.6 or 4.0; the OpenSSL-backed libcurl is
a second install (`<prefix>/curl-openssl`) because both curls share the soname `libcurl.so.4`, so a
consumer must put its directory first (tests do) and the web stage should not put `<prefix>/lib` on
`LD_LIBRARY_PATH` ahead of it. *Open:* macOS, Linux/aarch64 and Windows for every row (the `cmake`
system is Linux/macOS only until replayed), SIMD (`WITH_SIMD=0`, `no-asm`), the CA bundle.

*B1 and B2 on macOS/arm64 (2026-10-11).* Replayed on the in-tree SDK after a rebuild of `crt-ui-dist`
(verified; full `ctest` 179/179). **B1:** `tools/build_webkit_jsc.py --port CRT` passes in `baseline-jit` mode from
an empty work root (compose 29 s, configure 11 s, build 127 s; 43 acceptance steps including the FTL and
WebAssembly tiers, clean Mach-O host-ABI audit) and in `interpreter` mode. It needed one more carried patch,
`0006-crt-port-no-apple-sdk`: on a Darwin build host the pinned commit's top-level `CMakeLists.txt` includes
`WebKitXcodeSDK.cmake`, which selects an Apple SDK and stops for any port but Mac, IOS and JSCOnly (the release
tarball's file did not do this, so JSCOnly never met it); the patch skips the include for `PORT=CRT`
(target group `crt-port-macos`, hash-checked against the pinned checkout by `check_webkit_patch_manifest.py --tree`).
**B2:** all seven recipes configure, build, install and pass their run tests from a clean state, static and shared
(OpenSSL: TLS 1.3 handshake in process; curl-openssl: HTTPS against an in-process TLS server and the
`CURLOPT_SSL_CTX_FUNCTION` hook), libpng and SQLite3 included for the gate, and `tools/check_web_dependencies.py`
resolves all eleven `find_package()` calls inside the prefix. Every image of the prefix and of the test binaries
loads only CRT libraries and `libSystem`. What the replay found, all fixed in CRT or the tooling:

1. *`-lpthread` bound libc to the host.* On macOS `-lpthread` is Apple's `libpthread.tbd`, a re-export of
   `libSystem`; listed before the CRT libc it satisfied every libc symbol of a static OpenSSL test from the host
   (100 imports, Apple's `pthread_once` with Bionic's `int` control, SIGBUS). `tools/crt-cc` now drops
   `-lpthread`/`-lrt` on macOS, as `tools/crt-c++` already did on every host (Bionic merged them into libc).
2. *Two libcs in one process.* A C test executable always linked the static `libc.a` while the port's dylibs load
   `libc.dylib`; with a TLS server thread in the process, `curl-openssl`'s shared test crashed in about 10% of runs
   with different corruption each time. `tools/crt-cc` honours `CRT_CXX_RUNTIME_LINKAGE=shared` for macOS
   executables (libc.dylib plus the sysroot rpath, like `crt-c++`), tests may carry a per-test `env`, and the shared
   tests of the six C recipes select it; 0/100 failures afterwards. (Older shared tests of existing recipes still link
   `libc.a`; they pass because they do not cross threads and heap, and should move to the same switch.)
3. *Recipe portability.* ELF-only flags moved into per-host overrides (`-Wl,--allow-shlib-undefined` in libpsl and
   HarfBuzz), test link lines name `.dylib` on macOS, OpenSSL's `Configure` target is `darwin64-arm64-cc`
   (`@CRT_ARCH_DARWIN@`), HarfBuzz uses the shared C++ runtime like ICU, and `check_web_dependencies.py` presents
   `Darwin` (it hard-coded Linux).
4. *Apple frameworks in libcurl, and IPv6.* With IPv6 enabled curl's configure links CoreFoundation, CoreServices
   and SystemConfiguration on a Darwin host triple. The first answer, `--disable-ipv6` as in the `curl` recipe, left
   IPv6 off; it is replaced (2026-10-11, below) by a CRT libc IPv6 surface on macOS plus a fail-closed configure probe.

*IPv6 on macOS without Apple frameworks (2026-10-11).* Goal: curl HTTPS over IPv6 using CRT libc's IPv6, with no
Apple framework in libcurl. Both parts were needed, because CRT's macOS socket layer had no IPv6 at all
(`socket(AF_INET6)` passed Bionic's 10, which Darwin does not number AF_INET6; no `sockaddr_in6` translation;
`inet_pton`/`inet_ntop` and `getaddrinfo` were IPv4-only):

- *libc, all hosts:* `inet_pton`/`inet_ntop` for `AF_INET6` (RFC 4291 parse including `::` and trailing dotted quad,
  RFC 5952 output including `::ffff:a.b.c.d`), `in6addr_any`/`in6addr_loopback`, `IN6ADDR_*_INIT` and the
  `IPV6_V6ONLY`/`IPV6_UNICAST_HOPS`/`IPV6_MULTICAST_*` numbers (Bionic's), `getaddrinfo()` returning IPv6 literals
  (with a numeric `%scope`) and `AF_INET6` results instead of `EAI_FAMILY`, as a list and not one entry. Windows
  keeps `EAI_FAMILY` for IPv6 until its Winsock translation exists.
- *libc, macOS translation:* `socket(AF_INET6)` to Darwin's 30; `sockaddr_in6` both ways (Darwin's length-prefixed
  layout) for bind/connect/sendto/recvfrom/accept/getsockname/getpeername/recvmsg; `IPPROTO_IPV6` options to Darwin's
  numbers, with an honest `ENOPROTOOPT` for any option without a translation; the `SOL_SOCKET` table now also covers
  `SO_KEEPALIVE`, `SO_BROADCAST`, `SO_LINGER`, `SO_SNDBUF`, `SO_RCVBUF` (they were passed with Linux numbers);
  `getaddrinfo()` takes only the *answers* for names from the host resolver and converts every A/AAAA entry to
  Bionic structs and the caller's port. No Apple type reaches a consumer.
- *curl:* `porting/shims/macos/curl-no-apple-frameworks/TargetConditionals.h` is placed first on the include path of
  the two curl recipes on macOS (`target_overrides.macos.include_dirs`). Its content is an `#error`: curl's one
  framework probe (a program that includes `<TargetConditionals.h>` and needs `TARGET_OS_MAC`; the CRT SDK header
  answers yes for other consumers) fails closed and nothing else in libcurl includes that header. Configure now
  reports `IPv6: enabled`, `otool -L` on both libcurls lists only CRT libraries and `libSystem`.
- *Evidence:* `socket_ipv6_test` (text conversion, literals, passive/loopback, `localhost` through the host resolver
  returning both families, TCP and UDP over `::1`, `IPV6_V6ONLY`; on macOS it cannot skip, and two mutations of the
  translation make it fail) and `curl_openssl_https_ipv6_test` (shared and static: TLS server on `::1`,
  `CURLOPT_IPRESOLVE_V6`, `https://[::1]:port/` and `https://localhost:port/`, required `CURL_VERSION_IPV6`,
  `CURLINFO_PRIMARY_IP == "::1"`, the SSL_CTX hook); 40/40 repeated runs each; full `ctest` 180/180; the mbedTLS
  `curl` roundtrip test passes with IPv6 on.
- *Not done:* DNS AAAA in the built-in resolver used on Linux/Windows (macOS uses the system resolver; Linux still
  answers only literals for IPv6), Windows AF_INET6, interface-name zones (`%en0`), `IPV6_JOIN_GROUP` and the packet-info
  options, and the SDK-level `04-gfx-media` stage rebuild of curl with IPv6 (the recipe change reaches it on its next
  rebuild).

*IPv6 and the Tranche 3A replay on Windows/x64 (2026-10-12).* The same finding held on Windows (the PAL passed Bionic
numbers to Winsock) and is fixed there (`HISTORY.md`, `docs/porting/bionic_libc_gaps.md`): `AF_INET6`, `sockaddr_in6`,
the `SOL_SOCKET`/`IPPROTO_IPV6` option tables, `getaddrinfo()` IPv6 results and RFC 6761 `localhost`. Evidence:
`socket_ipv6_test` in full on Windows (literals, passive/loopback, `localhost` both families, TCP and UDP over `::1`,
`IPV6_V6ONLY`, a dual-stack listener taking an IPv4 client as `::ffff:127.0.0.1`, `SO_KEEPALIVE`/`SO_LINGER`/buffers,
`IPV6_UNICAST_HOPS`, `sendmsg`/`recvmsg` with a `sockaddr_in6` name; three mutations fail it), full CTest 167/167.
The 3A wire contract passes `crtweb_wire_layout_test` unchanged on Windows. Not replayable on this host yet: the
`SurfaceClient` consumer test and the WPE `crt` module, because `socketpair()`, `SOCK_SEQPACKET` and `SCM_RIGHTS` are
Linux/macOS-only and this tree has no Skia; the transport for Windows (an AF_UNIX stream with length framing and a
named shared mapping in place of descriptor passing is the obvious shape) is still the open decision.

*Windows AF_UNIX for the 3A transport (2026-10-12).* The transport decision for Windows is made in CRT libc, not in
`libcrtweb`: Winsock's AF_UNIX stream sockets plus a CRT-owned SOCK_SEQPACKET (framing) and SCM_RIGHTS (handle and
socket duplication into the peer process), see `HISTORY.md` and `docs/porting/bionic_libc_gaps.md`. The frozen wire v1
is unchanged: `crtweb_wire_transport_test` runs `crtweb_wire_send/recv` over it between two Windows processes
(CONFIG, a BUFFER_ADD carrying a memfd that the consumer maps and reads, FRAME, TITLE, FRAME_ACK, BUFFER_RELEASE, and a
size-mismatch EPROTO). Gate C, the `SurfaceClient` consumer test (`crtweb_web_surface_test`), now runs on Windows/x64 too
(2026-10-12): the Windows tree was built with `CRTGFX_ENABLE_SKIA=ON` (pinned Skia m148 through
`crtgfx-skia-build` against the CRT libc++, GPU backend on) and the test is registered for `linux` and `windows` in
`libcrtweb/CMakeLists.txt`. It prints `ok` (not `skipped`) and passed 8 of 8 consecutive runs: handshake, memfd buffers
imported as a Skia image behind a `SurfaceView`, scene order, ack/release, damage and serials, resize and input
translation over the CRT AF_UNIX/SEQPACKET layer. The WPE `crt` module stays Linux-only (WPE does not build on Windows).

*Tranche 3 on macOS/arm64 (3A contracts and B3, 2026-10-12).* Replayed on the in-tree SDK. **3A:** the frozen wire
layout (`crtweb_wire_layout_test`), the wire contract over its real transport (`crtweb_wire_transport_test`: CONFIG,
a BUFFER_ADD carrying a memfd that the consumer maps, FRAME, TITLE, ack, release, a lying header as `EPROTO`, between
two processes) and the `SurfaceClient` consumer (`crtweb_web_surface_test`, hermetic, CRT-built with Skia: composition
between UI layers, acknowledgement, release on drop, latest-frame-wins, damage serial, title, input translation,
resize) pass. The WPE `crt` display module and the native end-to-end run (`tools/build_webkit_wpe_crt.py`) are **not
replayable**: WPE needs GLib/GIO and does not build for the CRT target on macOS; they remain the Linux reference, and
the product port's own process model (B4) replaces them. What the replay found, fixed in CRT libc and the tests:

1. *No `SOCK_SEQPACKET` on Darwin.* `socketpair(AF_UNIX, SOCK_SEQPACKET)` and the listener model both failed with
   `EPROTONOSUPPORT`; this was the open "macOS transport decision". It is made in CRT libc, as for Windows
   (`HISTORY.md`, `docs/porting/bionic_libc_gaps.md`): a `SOCK_STREAM` socket carrying a 4-byte length per record,
   with `SCM_RIGHTS` sent in the same `sendmsg()` as the length so Darwin delivers the descriptors with the first byte
   of the record. `socket()`, `socketpair()` and `accept()` mark a descriptor framed (an in-process table, copied by
   `dup`, dropped by `close`, inherited by `fork`, and handed across `exec` in `CRT_SEQPACKET_FDS`, see the review
   follow-up below), `SO_TYPE` reports `SOCK_SEQPACKET`,
   `read`/`write`/`send`/`recv`/`sendmsg`/`recvmsg` keep record semantics, over-long records are truncated with
   `MSG_TRUNC`, zero-length records are zero-length reads, `MSG_PEEK` is `EOPNOTSUPP`. Both ends must be CRT. The gate
   is `unix_socket_test` Gate B, which now runs on macOS instead of skipping (file, memfd mapping, TCP socket, three
   descriptors, `MSG_CMSG_CLOEXEC`, a small control buffer with `MSG_CTRUNC`, and a second process) and
   `socketpair_test`.
2. *Message flags were Linux numbers.* `MSG_DONTWAIT` (0x40) reached Darwin as `MSG_WAITALL`; `MSG_EOR`, `MSG_TRUNC`,
   `MSG_CTRUNC` and `MSG_NOSIGNAL` differ too. They are translated in both directions for every socket call;
   `MSG_CMSG_CLOEXEC` (no Darwin equivalent) is applied to the received descriptors, and Darwin's untruncated
   `cmsg_len` after `MSG_CTRUNC` is clamped to what was delivered, as Linux reports it.
3. *`crtweb_web_surface_test` read `std::string` size 0.* In-tree C++ compiles on macOS use the host SDK's libc++
   headers while the program links the imported libc++ archive: `title_.assign()` ran in the archive and `size()` was
   inlined from the SDK's differently laid out `std::string`. The target now takes the imported libc++ headers (the
   same decision Windows already has for Skia code); `crtweb_surface_wire.h` is unchanged. Other in-tree macOS C++
   tests that pass `std::string` across that boundary are at the same risk and should move to the same headers.

*Review follow-up on the macOS `SOCK_SEQPACKET` layer (2026-10-13).* A code review of the first version
(`6152300`) found real gaps, all confirmed against the source and fixed before B4 builds on it:
(1) a non-blocking or `MSG_DONTWAIT` receive read the payload with blocking reads and kept no progress, so a half
record could hang or be lost; the receiver now keeps the partly received record per socket and returns `EAGAIN`;
(2) descriptors received with a header leaked when the record then failed (oversize length, peer closed mid-record);
they are owned by the state until delivery and closed on failure, and extra ones are closed with `MSG_CTRUNC`;
(3) the framing flag was lost across `exec`: the spawn/exec paths now carry it in `CRT_SEQPACKET_FDS` and the new
image restores it, which is also the B4 answer for a launcher that passes a `socketpair()` end to a child (a child
that connects its own socket works as before); (4) `readv`/`writev` made one record per vector; they are one record
now; (5) senders were not serialized, and descriptors made with `dup` did not share state; a send lock and a shared
state object fix both; (6) a framed `dup` at or above the 4096 table limit became a plain stream; it is `EMFILE`.
Not changed, by decision: a sender that already wrote part of a record completes it by waiting for `POLLOUT` (a half
record would corrupt the stream; a sender that wrote nothing gets `EAGAIN`), and the record cap, `MSG_PEEK`
and the `fontconfig` `-D__linux__=1` (below, with its removal condition).

**B3:** `tools/build_webkit_crt_port.py --run-tests` was Linux-only; it now takes the macOS persona of
`build_webkit_jsc.py` (named CRT compilers, `CMAKE_PROJECT_INCLUDE`, `__linux__`/`__unix__`, Apple Silicon W^X
header, a Mach-O audit). From an empty work root it composes the tree, applies patches 0001-0009 and builds WTF,
bmalloc, JavaScriptCore (all tiers), the bundled Skia m154, PAL and WebCore into `crtweb_webcore_graph_test`, which
reports the same numbers as Linux: element `<div/>`, text width 86.70 px, 216 dark pixels. The audit: no GLib/GIO/
GObject symbol, and every load command is a CRT SDK library, a port library or `libSystem` (26 images). The macOS
replay also needed: three web ports made portable (`fontconfig`: the CRT gperf ahead of Xcode's 3.0.3 on `PATH`,
because configure's length-type probe runs bare `gperf`, and `__linux__` for its `statfs` branch; `libwpe`: `__unix__`
for `eglplatform.h`; `crt-egl`: `libEGL.dylib`/`libGLESv2.dylib` with install names, via `crt_shared_libraries`) and
one more carried patch, `0009-wtf-persistence-darwin-size-types`: WTF's persistence coders give only the fixed-width
integers their own overloads, so `size_t` (unsigned long, distinct from Darwin's `uint64_t`) in WebCore's generic
`KeyedEncoder`/`KeyedDecoder` hit an undefined `Coder<unsigned long>` (3 translation units); the patch adds
`unsigned long`/`long` forwarding overloads under `WTF_CRT_INT64_IS_LONG_LONG`. One more finding: FindCURL found the
mbedTLS curl's `libcurl.pc` through `PKG_CONFIG_PATH` and linked it next to the OpenSSL one (Linux merges them by
soname; Mach-O loaded two `libcurl.4.dylib`), so the driver now puts `curl-openssl/lib/pkgconfig` first.

*Not covered by the macOS replay:* the CA bundle, SIMD/asm paths (still off), Linux/aarch64 and Windows for B2, and
promoting the web group into the automated all-ports set (it waits for Windows).

*B1, B2 and B3 on Windows/x64 (2026-10-12).* Replayed on the in-tree SDK (`dist/05-ui`, rebuilt after the pull):
**B1** `build_webkit_jsc.py --port CRT --mode baseline-jit` passes every step (a ~1% JSC `Invalid value for lock: 0`
abort in one 4-thread context-cycle process, the failure already recorded for JSCOnly, passed on the repeat).
**B2** the seven web ports configure, build, install and pass their run tests through `crt-port-build.py
--use-crt-shell --test`; OpenSSL 3.5.9 and the OpenSSL-backed libcurl are **static only** on Windows (a target that
keeps the LLP64 `BN_ULONG` as `unsigned long long`: `porting/shims/win32/openssl-crt-windows.conf`; the Unix target's
ELF version script and soname have no PE counterpart), and the libcurl test fetches HTTPS from an in-process TLS
server over IPv4 and over `::1`. `tools/check_web_dependencies.py` resolves all eleven `find_package()` calls inside
the prefix (SQLite3 is the `sqlite-amalgamation` port). **B3** `build_webkit_crt_port.py --run-tests` builds WTF,
bmalloc, JavaScriptCore, PAL, WebCore and the bundled Skia (about 7,900 steps) and the graph test prints text width
86.70 and 216 dark text pixels, the Linux and macOS numbers; the import audit lists only CRT DLLs, port DLLs and
KERNEL32, and no GLib symbol. The findings are in `HISTORY.md` (2026-10-12): COFF reports undefined symbols that an ELF
`--gc-sections` link drops (`__dso_handle`, `__cxa_thread_atexit`, `CurlSSLHandle::platformInitialize()`,
`SkStrikeRef`), the 8,191-character `.cmd` limit and the 32,767-character spawn limit, native-program redirection, and
the OpenSSL LLP64 target. *Open on Windows:* the multi-process milestones B4-B7, shared OpenSSL/libcurl, the CA bundle,
SIMD/asm paths, and a clean-prerequisite story for Perl and pkgconf (`docs/guides` does not list them yet).

*Tranche 3 on Linux/aarch64 (B1, B2, B3 and the 3A consumer contracts, 2026-10-12).* Ubuntu 26.04 aarch64 (QEMU, 4 CPUs,
7.4 GB RAM), Clang 21.1.8, in-tree SDK rebuilt with `cmake --fresh` (0 warnings, `crt-ui-dist` verified, `ctest`
157/157), with no change to CRT, the patches or the harnesses. **B1** `--port CRT` passes in `baseline-jit` (44 steps,
clean host-ABI audit; compose 42 s, build 348 s, acceptance 63 s) and `interpreter` modes. **B2** the web ports'
16 run tests pass, static and shared (TLS 1.3 handshake; HTTPS through the OpenSSL-backed libcurl on IPv4 and `::1`),
and `check_web_dependencies.py` resolves all eleven calls once the libpng and SQLite ports are in the prefix.
**B3** `build_webkit_crt_port.py --run-tests -j3` builds 7,857 steps in 26 min 30 s (Release, 3.2 GB) and the graph
test prints text width 86.70 and 216 dark pixels, as on the other three hosts; no GLib symbol, 26 `DT_NEEDED`
entries, none from the host. **3A** the wire layout and transport tests pass, and with Skia on `crtweb_web_surface_test`
passes 20/20 (`ctest` 164/164). *Not replayed:* the WPE `crt` display module and the native end-to-end run, which need
the Tranche 2 native WPE reference, which is the Linux/amd64 comparison baseline by decision and is not replayed
on aarch64.
*Findings:* `CurlSSLHandleCRT.cpp` was missing from the patch manifest (declared; every host's licensing unit test
failed on it); the dependency check needs libpng and SQLite3 beyond `port-build-web`; a work root on a small tmpfs
`/tmp` fails, so use a real disk.

*B3 result (Linux/x86_64, 2026-10-11).* `tools/build_webkit_crt_port.py --run-tests` composes the
tree (pinned commit + `libcrtweb/port` + patches 0001-0008), configures `-DPORT=CRT
-DCRT_ENABLE_WEBCORE=ON` against the B2 prefix and builds WTF, bmalloc, JavaScriptCore (all JIT
tiers, as B1), the bundled Skia m154 (782 sources, 2.6 min at 12 jobs: risk 1 below is retired),
PAL and WebCore (about 6,600 translation units; a full build took roughly half an hour to an hour
at 12 jobs, not timed separately) into
`crtweb_webcore_graph_test`, which runs: WTF and JSC initialisation, `ProcessWarming::
initializeNames()`, a `Document` (no Page) with an element created in it, a Skia-backed
`ImageBuffer` painted with two rectangles and read back (exact pixels), and text drawn through
fontconfig -> FreeType -> HarfBuzz -> Skia (width 86.70 px for nine DejaVu Sans Mono glyphs at
16 px, 216 dark pixels). The program has no GLib/GIO/GObject symbol and its `DT_NEEDED` set is the
SDK, the port prefix and the loader (`acceptance-b3.json`). Dependencies added by this milestone,
all CRT ports: `fontconfig` 2.17.1, `crt-egl` (Khronos headers + the CRT "no GPU" EGL/GLES
libraries) and static `libwpe` 1.16.3. *What the plan did not foresee, found by building:* fontconfig
is mandatory (D6); WebCore links EGL/GLES unconditionally; WebKit's OpenSSL crypto sources are
written for OpenSSL 1.1 and need a force-included shim for `EVP_PKEY_get0_RSA` (one file, no source
change); `PLATFORM(PLAYSTATION)` guards select types in nine WebCore files (patch 0006) and
`Page.cpp` calls a setting that exists only for GTK/WPE (patch 0008), because upstream's PlayStation
port is not Skia-clean at this commit; the CRT toolchain needed `__dso_handle` for shared-runtime
executables and the loader as a needed object for `__tls_get_addr`
(`docs/porting/bionic_libc_gaps.md`). *Not exercised:* inserting nodes into a Page-less
`Document` crashes in `Element::insertionSteps` (WebCore expects a window; the test creates but does
not insert), networking and TLS, any process model, the CA bundle. *Open:* macOS, Linux/aarch64
and Windows for B3; `UserAgent`, curl TLS handle (CA store) and display files are still the
PlayStation SDK ones or absent and are CRT-owned work in B4/B5; first-run font scan of the bundled
fonts takes about two minutes (fontconfig cache).

*B4 result (Linux/x86_64, 2026-10-12).* `tools/build_webkit_crt_port.py --webkit --run-tests` also builds
`libWebKit.so` and the `WebProcess` and `NetworkProcess` executables and runs
`libcrtweb/tests/webkit/crtweb_process_test`, a UI process written against the C API: creating a view
launches both child processes (separate pids, children of the test, running the CRT executables);
`WKPageLoadHTMLString` goes UI -> Web process and `didFinishNavigation` comes back over the
SOCK_SEQPACKET/`SCM_RIGHTS` IPC connection; `kill -9` of the Web process is seen as
`webProcessDidCrash` and the pid is gone. 20/20 consecutive runs pass (0.7 s each) and no child outlives
the test. The IPC layer is upstream's Unix backend (`Platform/IPC/unix`, with PlayStation's
select()-based socket monitor: the generic RunLoop has no file-descriptor sources), and the launcher is
CRT's (`UIProcess/Launcher/crt/ProcessLauncherCRT.cpp`: `posix_spawn`, a reaper thread per child).
*Decisions this forced (all CRT-owned files or carried patches, `docs/porting/crtweb_porting.md`):*
upstream's PlayStation WebKit list is stale at this commit (a mis-cased file name, a compositor
that no longer builds, an SDK-only bundle loader), so PlatformCRT takes the PlayStation files that
still build, supplies its own injected-bundle loader, user agent and libwpe loader interface, and
replaces the compositing stack: the maintained one (LayerTreeHost, ThreadedCompositor,
AcceleratedSurface) requires GLib, EGL and, for software frames, the WPEPlatform buffer protocol, so the
Web process draws through a CRT `DrawingAreaCRT` that **accepts calls and paints nothing in B4**;
B5 makes it paint on the CPU and adds the UI-side counterpart. Patches 0010-0013 widen
`PLATFORM(PLAYSTATION)` sites in WebKit, select the PlayStation-shaped C API headers, make the framework
header copies symlinks (clang's `#pragma once` does not deduplicate copies: 26 files redefined the same
enumerators) and hook the drawing area. *Not exercised:* painting, input, any network request (the
Network process starts and idles), the injected bundle (the CRT Linux libdl cannot load ELF), shutdown
under load, Windows and macOS (the launcher needs a `/proc/self/exe` replacement on macOS; Windows uses
upstream's `win` launcher at Tranche 6).

*B4 replayed on macOS/arm64 (2026-10-12).* The same gate passes (`--webkit --run-tests`, incremental on the B3 work root;
`crtweb_process_test` 10/10, no leftover child, no IPC error). Five things differed from Linux and are fixed in CRT:
patch 0014 (a `uint64_t`/`size_t` definition that is one type only on LP64 Linux), `-fno-blocks` in the wrappers outside
real-SDK compiles, `readlink("/proc/self/exe")` (the launcher's sibling lookup), `posix_spawn` reporting a failed exec
(the launcher otherwise had a silent child), and, the only one that needed design, **framed SOCK_SEQPACKET descriptors
passed with `SCM_RIGHTS`**: the Network process makes the pair that connects it to a Web process and hands one end to the
UI process to forward, and the framing flag is per process, so the receiving end read raw. The record header now carries
a mask of the framed descriptors of the record (`docs/porting/bionic_libc_gaps.md`). The test finds its children with
`ps(1)` (no `/proc`). *Not exercised on macOS:* the same set as Linux (painting, input, any network request, the
injected bundle).

*B4 replayed on Windows/x64 (2026-10-12).* The same gate passes (`--webkit --run-tests`, `crtweb_process_test` 10/10,
no leftover child). Upstream's `win` launcher is not used: the `posix_spawn` launcher and the Unix IPC backend run
over the PAL, as on the other hosts. What differed is PAL and build, not WebKit: a static `libWebKit` (the DLL would
exceed the PE export limit), `-D_LIBCPP_WIN32API` plus patch 0015 for `std::filesystem`, `readlink("/proc/self/exe")`,
cross-process `kill(pid, SIGKILL)`, sockets passed to another process with `DuplicateHandle` (a `WSADuplicateSocket`
copy dies with the sender), a graceful `shutdown` when the last handle of an AF_UNIX socket closes, and non-inheritable
sockets (details in `docs/porting/bionic_libc_gaps.md`). The test lists children with Toolhelp32 and allows the reaper
a moment to retire the killed process. *Not exercised on Windows:* the same set as Linux and macOS.

*B5 result (Linux/x86_64, 2026-10-12).* `DrawingAreaCRT` now paints: WebCore draws the dirty
rectangles into a `ShareableBitmap` and sends `DrawingAreaProxy::Update`; the UI process (upstream's
`DrawingAreaProxyCoordinatedGraphics` and its Skia `BackingStore`) composes them and answers
`DisplayDidRefresh`. `WKViewCRTCopyPixels` (a CRT extension of the C API) copies the backing store out.
`crtweb_render_test` loads `libcrtweb/tests/wpe-reference/reference.html` into a 640x480 view and finds
the Tranche 2 proof string, evaluated in the page, equal to the native WPE reference's
(`CRT WPE reference|local-html-ok|42|192x96`: DOM text, a script-set dataset value, a canvas size), a
complete frame with 678 distinct colours that includes all five of the fixture's CSS/canvas colours, and
then delivers it through `crtweb_surface_producer.h` (the frozen 3A producer contract, header-only C)
to `surface_probe --reference`, built with the CRT toolchain, which checks size, colours and the
document title and acknowledges the frame. 15/15 runs pass (2.3 s each). *Diff against the WPE
reference (`out/web-reference/result/wpe-reference.ppm`):* same size, same layout (the aside starts at
the same column), the four solid probe points are identical to the pixel; 8.3 % of the pixels differ by
more than 24 channel units, all of it text, because the CRT port's font set is the bundled
`DejaVuSansMono` and Pretendard (CSS `sans-serif` resolves to DejaVu Sans Mono) where the WPE host used
its own sans-serif; that is risk 4, now measured. *Not exercised:* animation, scrolling, input (B6),
resize, device scale factors other than 1, damage-limited frames (every frame is sent with full
damage), more than one frame in the consumer.

*B6 result (Linux/x86_64, 2026-10-12).* `libcrtweb/tests/webkit/crtweb_surface_host` is the CRT counterpart
of the Tranche 3A WPEPlatform module: it loads `crt_surface.html`, sends frames and titles to the
consumer (`crtweb_surface_producer.h`) and turns the consumer's pointer, wheel, key, text, resize and
focus messages into WebKit events through new C API entry points (`WKViewCRTHandlePointer/Wheel/Key/
Text`, `WKViewCRTSetSize`). The 3A acceptance consumer, `surface_probe`, run unchanged and complete
(built with the CRT toolchain), passes: first frame at 640x480 with the four quadrant colours; pointer
down/up/click reported by the page and quadrant A turning white in pixels; `a` and Ctrl+Shift+`b` with
the right `key`/`code`/modifier report; wheel; committed text `q` as a keydown and `Z` typed into a
focused field (the `input` event); resize to 320x240 with proportional quadrants; and a consumer that
stops acknowledging and releasing (the pool of three runs dry, the page keeps running, the newest
picture arrives after recovery). Stable over repeated runs (4.4-5.5 s each). *Found by the run and fixed:*
the fontconfig port installs no configuration, so a generic or WebKit-default family name matched
nothing, the primary font of an empty `<input>` had no metrics and the control was 6 px high (the
driver's configuration now aliases the generic and default names to the bundled font; a real embedder
needs the same); PlayStation's `handleEditingKeyboardEvent` is a stub, so typed characters never reached
a text field (patch 0017, the Windows port's key-command logic); `WKViewSetSize` does not reach the
drawing area (`WKViewCRTSetSize`); the UI process keeps painting the old backing store after a shrink
(patch 0016 exposes its size so a stale picture is not sent); a first frame sent before the page's
first paint was white. *Not exercised:* IME and non-US-layout text (the committed-text path types ASCII
through a US layout table), touch and pen pointers, drag, device scale factors other than 1, pointer
leave.

*B7 result (Linux/x86_64, 2026-10-13).* `libcrtweb/tests/webkit/crtweb_lifecycle_test` (driver gate
`lifecycle_and_host_abi`) runs 100 create/load/paint/release cycles of a context, page configuration
and 320x240 view; each checks the canvas and background pixels, exactly one Web and one Network process
while alive, and the Web process gone after release. After a 10-iteration warm-up the run grew by +0
descriptors, +0 threads and +264..332 KiB RSS (flat at 65.9 MiB), bounds being 2, 2 and 24 MiB; no child
process outlives the run. The single Network process is shared by design (default website data store),
so the test requires at most one, not none. *Host ABI audit:* every produced binary and `libWebKit.so`
resolves only to the CRT SDK, the port prefix and the build tree (plus the host dynamic loader), and the
runtime `/proc/<pid>/maps` of the UI, Web and Network processes (123 distinct files) contain nothing
else (the test program itself is also checked for GLib symbols and unresolved DT_NEEDED, as in B3). *Rechecks:* the
per-file license scan of the pinned tree is byte-identical to the recorded `license-scan.json`, the
patch manifest (18 patches) verifies against the checkout, and the licensing tests pass. *Found and
fixed:* patch 0018, an upstream `RedBlackTree::remove` that leaves the removed node's `CheckedPtr`
links in place, crashed `WebProcessProxy` teardown through the generic RunLoop's timer tasks. *Link
set, by upstream's published licenses and not machine-verified by this repository:* ICU (Unicode),
OpenSSL 3 (Apache-2.0), curl (curl license), HarfBuzz (MIT), FreeType (FTL, used under FTL),
fontconfig, libxml2, libpsl, expat (MIT-style), libwebp (BSD-3), libjpeg-turbo (IJG/BSD-3), libpng,
zlib, SQLite (public domain), libwpe (BSD-2), CRT's EGL/GLES stub libraries. *Not covered:* forced Web
process termination and recovery, navigation while resizing (Tranche 5), non-Linux hosts.

*B5-B7 replayed on Windows/x64 (2026-10-13).* The three driver gates run unchanged in structure
(`tools/build_webkit_crt_port.py`; programs found as `.exe`, DLLs through `PATH`, the surface socket named by its
drive-letter path) and pass 3/3 consecutive runs: `crtweb_render_test` (proof identical, 678 colours,
`surface_probe` accepts the frame; no native WPE reference exists on this host, so the pixel diff is not computed),
the scripted 3A input run, and `crtweb_lifecycle_test` (100 cycles, +0 handles, +0 threads, RSS flat at 13.6 MiB, at most
one Network process, no orphan). The host ABI audit has a Windows form: the static import closure of every produced
binary, and the Toolhelp module list of the UI, Web and Network processes (111 files: CRT SDK, port prefix, build
tree, System32 DLLs). The replay found, in CRT, two handle leaks that Linux does not have (per-thread signal-state
handles, and a descriptor forwarded by a process that made both ends of a socketpair; `docs/porting/bionic_libc_gaps.md`)
and no new WebKit patch. *Not exercised on Windows:* the same set as Linux, plus a sender that exits before its
descriptor is received.

*B5-B7 replayed on macOS/arm64 (2026-10-13).* The same three gates pass through `tools/build_webkit_crt_port.py`
(loader path `DYLD_LIBRARY_PATH`): `crtweb_render_test` (proof identical, 678 colours, `surface_probe` accepts the
frame; the native WPE reference is not on this host, so the pixel diff is not computed), the scripted 3A input run, and
`crtweb_lifecycle_test` (100 cycles: threads +0, descriptors +0, RSS +96 KiB from 20.6 MiB, at most two children at
once, none left). The Host ABI audit has a Mach-O form: the static recursive audit of every produced binary (direct
dependencies are CRT SDK, port prefix, build tree or libSystem; no GLib symbol) and the runtime one, in which the driver
samples `vmmap(1)` of the UI, Web and Network processes while the test runs (`/proc` does not exist; an observer started
inside the process under test would be a child that disturbs what it measures): everything mapped outside the OS's dyld
shared cache is the SDK, the prefix or the build tree (365 files, no violation). The replay found **one** defect, in
PlatformCRT: the PlayStation socket-monitor thread blocks in `select()` holding its `Connection`, and a `close()` in
another thread does not wake a blocked `select()` on macOS, so every lifecycle left a monitor thread and a work queue
(two threads and about 200 KiB per cycle). Patch 0019 gives that `select()` a 100 ms bound; the loop already re-reads the
descriptor. The test observes children and resources through libproc on macOS (`proc_listchildpids` returns a pid count,
not bytes). *Not re-run on Linux and Windows:* patch 0019 applies there too (target group `crt-port`); it changes a
timeout, not a contract. *Not exercised on macOS:* the same set as Linux.

*Risks, named so they are watched rather than discovered.* (1) ~~The bundled Skia m154 must build with
CRT's clang and libc++~~ *retired by B3 (it does)* (CRT's own Skia is m148 and is built by other tooling); (2) WebCore is about
16,000 files, so build time and the unified-source layout dominate iteration; (3) OpenSSL and an
OpenSSL-backed curl add a TLS stack beside mbedTLS in one SDK, which needs a clear
boundary (web stage only); (4) fonts without fontconfig change text metrics against the WPE reference,
so B5 compares structure and colours, not text pixels; (5) thread stack and TLS limits the JSC work
left open (1 MiB default stack, `__tls_get_addr`) will be hit harder by the Web process.

### 4. `libcrtweb` and the WebView

`crtweb_runtime_create`, `crtweb_view_create/load_url/load_html/go_back/
go_forward/reload/set_size/set_focus`, and title/load/navigation callbacks,
plus `crtui`'s web view on top. Application code sees no WebKit type.

Policy and rules: [`libcrtweb_api_policy.md`](../design/libcrtweb_api_policy.md); the header is
`libcrtweb/include/crtweb/crtweb.h`. Work is split at the ownership boundary:

| Milestone | Scope | Gate |
|---|---|---|
| **C0** (done) | Freeze the plain-C runtime/view API and its rules; the page-control extension of the web surface protocol | policy document; header; `wire_layout_test` covers the extension's layout |
| **C1** (done) | `libcrtweb.so` over `PlatformCRT`: runtime, views, navigation, history, script, frame copy, input, callbacks | `crtweb_api_test` includes only `<crtweb/crtweb.h>`; exports are `crtweb_*` only |
| **C2** (done) | The `crtui` WebView: `crtweb::WebView` over the out-of-process `crtweb_host` | `crtweb_webview_test` composes the page in a crtui window and drives it through crtui input |
| **C4** (done) | Several runtimes per process, each with its own website data store | `crtweb_api_test`: two runtimes, isolated cookies and storage, either one destroyed alone |
| **C5** (done) | Cookies and website data: data directory (persistent or ephemeral), cookie accept policy, clearing | a cookie set by a page is visible to the next load, survives a runtime restart in a persistent directory, and is gone after clear or in an ephemeral runtime |
| **C6** (done) | Downloads: request callback with the destination decision, progress, result | a `Content-Disposition: attachment` response is saved byte-for-byte; cancel and failure are reported |
| **C7** (done) | Popups and dialogs: `window.open`/`window.close`, alert/confirm/prompt answered by the application | a popup is vetoed or adopted as a new view; dialog results reach the page |
| **C8** (done) | IME: composition, commit of UTF-8 text, cancel | `compositionstart/update/end` events and a committed Hangul string in an input; through `crtweb::WebView` too |
| C3 (done) | Host replays (C0-C8): macOS/arm64, Windows/x64 | the same tests: **macOS/arm64 and Windows/x64 done 2026-10-13** |

*Why the WebView is out of process.* An application that uses `crtui` links its own static C runtime,
Skia and the host window/GPU libraries (the `-Wl,--exclude-libs,ALL` static link of the SDK examples,
chosen because a shared CRT `libc.so` interposes the host's glibc inside `libvulkan.so`), while
`libWebKit.so` needs the shared CRT libc, libc++ and its own Skia. The two cannot share an address
space. The 3A protocol already put the page in its own process, so the WebView is that protocol plus a
control extension (`crtweb_surface_control.h`: navigation commands in, load/URL/history/script events
out, as chunked 128-byte messages), and `crtweb` itself is the in-process API of the host and of any
application that links the shared runtime.

*C0-C2 result (Linux/x86_64, 2026-10-13).* `libcrtweb/tests/webkit/crtweb_api_test` includes only
`<crtweb/crtweb.h>` (no WebKit, WPE, GLib or JavaScriptCore header is on its include path) and passes
in about 2.3 s, 4/4 runs: argument and struct-size errors, one runtime per process, owner-thread
enforcement (another thread gets `CRTWEB_ERROR_STATE`), `file:` loads with started/committed/finished
events, title and URL callbacks, `get_title` truncation and needed-size, canvas and background pixels
through `copy_frame`, resize (a stale size is not served), script results (number, string, error),
the application name in `navigator.userAgent`, a navigation veto (the vetoed load never starts), back
/forward/reload and a failed load event, pointer click and typed text reaching the page, two views in
one runtime with independent state, no callback after `view_destroy`, and a second runtime after
`runtime_destroy`. `libcrtweb.so` exports only `crtweb_*`. `crtweb_webview_test` (in-tree, static
CRT/Skia/crtui link, run by the driver's webview gate against `crtweb_host`) composes the page in a
crtui window under and over crtui containers, forwards crtui pointer input (the click changes the
title), evaluates script, navigates and walks history across two `file:` pages, reports a failed
load, refuses a 2 MiB argument without losing the connection, resizes, and shuts the host down;
1.5 s, 3/3 runs. The same driver run, from a clean `--recompose` build with patches 0018-0020 applied,
passes every B3-B7 gate again. *Found and fixed by the run:* (1) WebKit's curl backend reported
"Unsupported protocol" for `file:` because the web-stage curl was built `--disable-file`; the
recipe now enables FILE (a local-content scheme; the media `curl` port is unchanged). (2) A stale SDK
`libc.so` returned -1 from `inet_pton(AF_INET6)`, which WebCore treats as success, so the Web
process aborted non-deterministically on a `file:` URL; the fix was in the source and the SDK was
rebuilt (`bionic_libc_gaps.md`). (3) Patch 0020: PlayStation's `WebPage::platformUserAgent` ignored
the application name. (4) The patch-manifest checker could not express stacked patches on one file
(0019 on 0010, 0020 on 0017); it now accepts a patch that starts from the upstream bytes or from the
previous patch's result. *Not done / open:* macOS/Windows replay of Tranche 4 (the driver gates it
Linux-only), `on_navigation_request` and script callbacks across the WebView's process boundary
beyond what the control extension carries, GPU frames, and a packaged `06-web` SDK (Tranche 10).
IME, downloads, popups and dialogs, cookies and website data and several runtimes per process were
open at C2 and are closed by C4-C8 below.

*C4-C8 result (Linux/x86_64, 2026-10-13).* `crtweb_api_test` now also runs a loopback HTTP server
(`libcrtweb/tests/common/crtweb_test_http_server.h`: cookies, a 70,000-byte attachment, a popup opener, an
input page) so the engine's real network path is exercised; 6.4 s, 3/3 runs, and
`crtweb_webview_test` repeats every item through `crtweb::WebView` and a real `crtweb_host`, 6.1 s.
*C4, several runtimes:* two runtimes in one process, each with its own data store; a cookie set through
one is invisible to the other, destroying one leaves the other working, and a second `crtweb_host`
(its own runtime) runs beside the first. *C5, website data:* a cookie set by a page is read back by the next
load, `crtweb_runtime_clear_data` (cookies, cache) removes it and calls back from the pump, the
`NEVER` policy stores nothing, and a runtime with a `data_directory` keeps the cookie across a
runtime restart (and across a host restart through the WebView). Local-storage and IndexedDB
directories are configured but not exposed to clearing in v1. *C6, downloads:* an attachment is saved
byte-for-byte to the destination the application chose, with url, suggested filename, type, progress
and the final status; a refused download is cancelled and the view keeps working. Over the protocol the
host asks the application and waits (cancelling after 60 s). *C7, popups and dialogs:* `window.open` from
a click creates a new `crtweb_view` (or is vetoed); `window.close()` is reported; alert, confirm and
prompt are answered by the application and reach the page (`true`, `false`, the typed text), with
the documented defaults when no callback is set, and a destroyed view releases its pending dialogs. Over
the protocol the host never opens a second window: it reports the popup URL and the application starts a
second `WebView` (second host and runtime), which the test does. *C8, IME:* `compositionstart`,
`compositionupdate` and `compositionend` reach the page, a marked Hangul string becomes the committed
text, text outside a US layout is inserted, and a cancelled composition removes the marked text; through
`crtweb::WebView` as commands. *Found and fixed by the run:* (1) the non-Cocoa Web process has no
composition entry point (GTK/WPE drive an input-method context through key events), so patch 0021
adds three WebPage messages (`SetCompositionCRT`, `ConfirmCompositionCRT`, `InsertTextCRT`) over
`Editor::setComposition/confirmComposition/insertText`; (2) `Editor::cancelComposition` left the marked
text in the field, so cancel replaces it with nothing and confirms; (3) the shared
`WKContextSetDownloadClient` path (`WKContextDownloadClient`) and `WKPageUIClientV9` work on `PlatformCRT`
as is, no patch. *Not accepted by Tranche 4 (decisions in `libcrtweb_api_policy.md`):* authentication challenges,
permission prompts (camera is now planned in Tranche 8C), file pickers and context menus, a JavaScript message channel, device scale,
local-storage clearing, and IME candidate-window geometry (the page's caret rectangle is not
reported back to the application).

*Tranche 4 replayed on macOS/arm64 (2026-10-13).* `tools/build_webkit_crt_port.py --webkit --run-tests` builds
`libcrtweb.dylib`, `crtweb_host` and `crtweb_api_test` and runs the whole gate: B3-B7 unchanged, `crtweb_api_test`
(C0-C8, with the loopback HTTP server; 5/5 runs) and the in-tree `crtweb_webview_test` (a crtui window composing the
page and driving it through `crtweb_host`; 8/8 runs), the public-header and export firewall (`libcrtweb.dylib` exports
`crtweb_*` only, no WebKit/WPE/GLib name in the headers), and the host ABI audit of the host and the library. Four CRT
defects and two harness defects stood between the Linux result and that, none in WebKit and no new carried patch:
(1) `realpath()` lost the tail of a path after a symlink (`/tmp/x`, `/var/...`, `/etc/hosts` on macOS: the
remaining components were copied from the buffer the resolution had just overwritten), which also broke the launcher's
`readlink("/proc/self/exe")` whenever the executable was started through `/tmp`; (2) `remove()` of a symlink to a
directory called `rmdir` (it used `stat`); (3) the macOS curl-openssl in the web prefix had been built with
`--disable-file` (the recipe changed for `file:` URLs on Linux; the prefix must be rebuilt: `port-rebuild-curl-openssl`);
(4) harness: the test links AppKit/Metal, whose `/usr/lib/libc++.1.dylib` dependency is replaced by the SDK's when
`DYLD_LIBRARY_PATH` names the SDK (matching is by leaf name), putting two C++ runtimes and heaps into one process
and crashing at exit; the gate runs this test without a loader path (`crtweb_host` has its own rpaths); (5) the driver only enabled Tranche 4 on Linux. *Not exercised on
macOS:* the same set as Linux, and the `ctest` entry of `crtweb_webview_test` only reports "skipped" without
`CRTWEB_HOST` (the driver's gate is the evidence).

*Tranche 4 replayed on Windows/x64 (2026-10-13), which closes it.* The driver builds `libcrtweb.dll`,
`crtweb_host.exe` and `crtweb_api_test.exe` and runs the gate (B3-B7 unchanged, `crtweb_api_test` C0-C8 against the
loopback HTTP server, the in-tree `crtweb_webview_test` against the host, the public-header and PE export firewall
(`libcrtweb.dll` exports `crtweb_*` only), and the host ABI audit of the host and the library): 3/3 consecutive
runs, 15/15 `crtweb_api_test` alone, no leftover process, `ctest` 186/186. Found and fixed, none in the public API:
(1) a DLL's static constructors were never run, so `libcrtweb.dll`'s global `std::map` was unconstructed (toolchain
wrappers and `dllcrt.c`); (2) SQLite's POSIX record-lock protocol failed on `LockFileEx` (a per-process lock table
in `fcntl`); (3) `file:` URLs and the data directory spelled as `C:\dir` are rewritten to `/c/dir` by
`libcrtweb` (libcurl and SQLite's unix VFS are POSIX-persona code); (4) patch 0022, a teardown race in
`Connection::readyReadHandler()`; (5) the tests' `rm -rf` through `system()` and the stale `--disable-file` curl.
*Not exercised on Windows:* the same set as Linux/macOS; local-storage clearing and IME candidate geometry remain
outside v1. The replay deliberately did not change the Web process model (GPU, recovery: Tranche 5).

### 5. Multi-process lifecycle

**Closed on Linux/x86_64, 2026-10-13, and replayed on macOS/arm64,
2026-10-13, and Windows/x64, 2026-10-14.** The earlier B4-B7/Tranche 4 evidence
on both replay hosts is retained.

UIProcess with WebProcess and NetworkProcess: launch through WebKit's
`ProcessLauncher`/`AuxiliaryProcess` on CRT PAL primitives, IPC, clean shutdown,
forced WebProcess termination and recovery/reload, 100x view create/destroy,
navigation while resizing and resource-leak audit. GPUProcess joins this contract
when Tranche 9 enables GPU rendering; it is disabled in the accepted CPU port.

The public-API gate is `libcrtweb/tests/webkit/crtweb_api_lifecycle_test.cpp`,
built and run by the existing `tools/build_webkit_crt_port.py --webkit --run-tests`
entry point alongside B3-C8. It includes only the public `crtweb` header and the
test-owned process/resource observer, and requires:

- Three forced WebProcess deaths, one callback per death, a replacement PID,
  reload on the same view and exact canvas/background pixels at the final extent.
  After death, frame copy must return `CRTWEB_WOULD_BLOCK` without modifying the
  destination. Recovery policy belongs to the application.
- Cooperative shutdown and shutdown of an infinite JavaScript loop, both below
  five seconds, with all owned children reaped when destruction returns.
  Destroying one busy runtime leaves a sibling runtime usable.
- Twenty launch-in-flight shutdowns before the first pump.
- One hundred runtime/view create/load/resize/paint/destroy cycles, alternating
  explicit view destruction with runtime-owned destruction, with script work
  pending at teardown. No callback reaches the retired view. Every cycle leaves
  no owned child; on Linux the observer counts zombies as remaining children.
- After ten warm-up cycles, RSS growth <=24 MiB, thread growth <=2, and open
  descriptor/handle growth <=2. Missing resource observations fail acceptance.

`crtweb_webview_test` also observes the WebProcess children of its owned host
after cross-site navigation. Because the public consumer cannot distinguish the
active page from cached/prewarmed implementation details, it probes candidates
one at a time and stops when the active page's termination callback arrives. It
then reloads/composes a fresh frame three times and runs twenty host
launch/load/resize/shutdown cycles, with child and consumer resource bounds.
Runtime/host shutdown still owns and retires the whole cached/prewarmed process
pool. The existing B7 static and runtime Host ABI audits remain part of the
combined gate. The policy is in [`libcrtweb_api_policy.md`](../design/libcrtweb_api_policy.md).

Reproduce on the Linux acceptance tree:

```sh
python3 tools/build_webkit_crt_port.py \
  --sdk-root out/linux-host-ninja-debug/dist/05-ui \
  --deps-prefix out/linux-host-ninja-debug/port-tests/install \
  --work-root out/web-crt-webcore --reuse-tree --webkit --run-tests -j 8
```

The combined result is written to `out/web-crt-webcore/acceptance-b3.json`;
`crtweb_api_lifecycle_test` defaults to the required 100 cycles (an optional
numeric argument >=12 is only a diagnostic shortened run). The accepted public
run grew RSS by 28 KiB after warm-up, with +0 threads and +0 descriptors.
Internal B7's 100 cycles grew RSS by 228 KiB, with +0 threads and +0 descriptors;
WebView's twenty host cycles grew +0 KiB RSS, +0 threads and +0 descriptors.
All eight entries of the combined B3-C8/Tranche 5 result passed, including the
static/runtime Host ABI audits and public header/export boundary.
WebView recovery also covers the previously accepted cookie/download/popup/IME
path before identifying and terminating the host's active WebProcess.

### 6. Windows/x64 replay

**Closed 2026-10-14.** The same driver command, using
`out/windows-host-ninja-debug/dist/05-ui`, the corresponding port prefix and
`C:\crtw\b3w` as the reusable work root, passes all eight combined entries.
The observer counts native handles instead of Linux fds.

- Public API: three forced deaths recover into replacement PIDs; launch-before-
  first-pump, busy and sibling-runtime teardown pass; 100 cycles finish at
  **+4 KiB RSS, -1 thread, +0 handles** after warm-up.
- `crtui` WebView: three active-process deaths each produce exactly one callback,
  a finished reload, a new PID, a fresh frame and the expected pixels. Twenty
  host cycles finish at **+4 KiB RSS, -2 threads, +0 handles**, with the bounded
  settle path unused (`0 ms`).
- Internal B7: 100 cycles finish at **-32 KiB RSS, -3 threads, +0 handles**,
  with at most two children. Runtime PE audit covers 108 mapped files in all
  three roles (UI/Web/Network), and static PE audit covers 14 binaries; both
  report no violations. The WebView host/library Host ABI audit is also clean.

The replay fixed three ownership boundaries: duplicated socket entries in an fd
snapshot must be released with `CloseHandle`, not `closesocket`; WebView shutdown
must drain queued surface messages whose handles Windows has already duplicated;
and the termination latch remains set until replacement navigation commits.
The test separately avoids treating simultaneous destruction of the active and
cached process pool as application recovery: it terminates observed candidates
sequentially until the active callback identifies the relevant death. The strict
new-PID/load/frame/pixel conditions remain unchanged, while whole-pool ownership
is exercised by runtime and host shutdown. The driver always rebuilds the
in-tree WebView test before running it, so a stale executable cannot satisfy the
gate. Exact results are in `C:\crtw\b3w\acceptance-b3.json`; detailed defects and
the Windows AF_UNIX spawn-snapshot regression are recorded in `HISTORY.md`.

The same WebKit and `PlatformCRT` on the CRT PAL (not a WPE-for-Windows
effort). Focus on process spawning, IPC handles, shared memory, executable
pages, fonts, basic key/pointer input, TLS and DLL
boundaries. D3D12 surface integration belongs to Tranche 9, after the
shared-memory render/input path is accepted.

### 7. macOS/arm64 replay

**Closed 2026-10-13.** B4-B7, Tranche 4 and the new Tranche 5 lifecycle gates,
including late session-cleanup launches, sibling-runtime isolation, descriptor
counts and the static/runtime Host ABI audits, are replayed.

Same WebKit, same `PlatformCRT`, same `libcrtweb` API. Apple's native WebKit
framework is deliberately not used as a shortcut -- that would not be a
cross-platform CRT port.

*Replayed on macOS/arm64 (2026-10-13), nothing in the engine, the port or the PAL changed.* The combined
`tools/build_webkit_crt_port.py --webkit --run-tests` gate, from a clean recompose of all 22 carried patches, passes all
eight entries: `crtweb_webcore_graph_test`, `crtweb_process_test`, `crtweb_api_test`, `crtweb_api_lifecycle_test`,
B5 render, B6 input, internal B7 with its Host ABI audit, and the crtui WebView gate. The Tranche 5 evidence on
macOS: three forced WebProcess deaths of the public API recover into a replacement PID with the exact pixels; 100
create/load/resize/paint/destroy cycles of the public API grow **+16 KiB RSS, +0 threads, +0 descriptors** (the
numbers vary by one thread between runs of the test: 4/4 standalone runs pass); the WebView terminates the active and
cached/prewarmed children of its owned host three times, receives exactly one termination event each time and
recomposes (`killed=3 replacement=1`), and its 20 host launch/shutdown cycles grow +0/+0/+0; internal B7 100 cycles
**+64 KiB RSS, +0 threads, +0 descriptors**, 364 mapped files in the three live processes (sampled by `vmmap` from
outside) all SDK, prefix or build tree, 15 binaries pass the static recursive Mach-O audit, the header/export
firewall of `libcrtweb.dylib` is clean, and no `Error receiving IPC message` line appears in the gate or in
repeated standalone runs (patch 0022 makes a closed descriptor a close). Process and resource observation
uses libproc (`proc_listchildpids`, `proc_name`, `proc_pidinfo`) declared inside the tests, as for B7, and is not part
of the target ABI. *Method note:* a gate run without `--recompose` after a pull that adds a patch uses a stale tree and
looks like a pass or a failure of the wrong thing; the driver's SHA gate catches the mismatch only when a
patch is re-applied. *Not exercised on macOS:* GPUProcess (Tranche 9), and a run of the whole gate three times in a row
(one clean gate plus the standalone repeats above).

### 8. CRT subsystem substitution

Replace subsystems one at a time, after the working browser/lifecycle gates. The **WPE reference**
uses GLib/libsoup/GStreamer, but the **PlatformCRT product** is PlayStation-shaped and already uses
WebKit Curl over CRT-built OpenSSL/libcurl and CRT PAL sockets. It does not link GLib/libsoup or
GStreamer. At tranche entry, HTML video was compiled (`ENABLE_VIDEO=ON`) without a registered
playback engine, and capture/MediaStream were disabled. The 8B/8C implementations and accepted
replays follow below. Removing nonexistent product dependencies is not an acceptance item.
Keep the reference unchanged for comparisons.

#### 8A. Network ownership and explicit trust

Adopt the CRT network trust policy at the product's actual boundary, retaining WebKit's browser
request scheduling, cookies, cache, same-origin/CORS, redirects and downloads. The private media
HTTP range/GET transport cannot replace a browser's arbitrary methods, headers and cookie policy.
WebKit's OpenSSL certificate verifier and WebCrypto also prevent simply substituting the media
mbedTLS build of Curl. Reuse the existing CRT-owned Curl/OpenSSL ports, not host libcurl/SSL.
The public contract and process-wide trust decision are owned by
[libcrtweb API policy](../design/libcrtweb_api_policy.md#network-trust-tranche-8a).

Acceptance requires actual **browser** loads (not just Curl port smoke), both accepted and rejected:

- Default HTTP succeeds; default/explicit-empty HTTPS fails even with ambient CA/trust variables.
- A copied explicit PEM CA accepts its server; unrelated CA, invalid PEM, expired certificate and
  wrong IP SAN each fail through `CRTWEB_LOAD_FAILED`.
- The loudly named development-only opt-out permits the wrong-SAN server, only when explicit.
- An accepted load follows a main-frame 302, loads an external script, makes a POST with an
  application header through a 307 redirect, and round-trips the HTTP-only cookie/body/method.
- Bad config size/boolean and oversized bundle are rejected; caller memory may be overwritten
  after configure; NULL resets defaults before creation; mutation after runtime creation/destruction
  is rejected.
- The separate-process WebView proves default denial, explicit CA acceptance, wrong SAN rejection,
  explicit insecure acceptance and startup failure for a missing CA file; every host is reaped.
- The existing API, lifecycle, rendering/input and static/runtime Host ABI gates remain green.

Implementation: `crtweb_network_configure`, `CurlSSLHandleCRT`, and separate-process WebView/host
launch options. `crtweb_network_test` includes only the public header, and the combined driver
records `crt_network_trust` alongside the existing eight entries in `acceptance-b3.json`.
`tools/test_crtweb_network.py` is a **build-host-only** fixture: Python HTTP/TLS server, OpenSSL
CLI supporting `req -addext`/`ca` for fresh per-run PKI, no Internet or committed private
keys. Run fixture unit tests with `python3 tools/test_crtweb_network_fixture.py`; they are not the
browser acceptance result. The Windows target path conversion is in the driver; new policy code
is shared across hosts; its required macOS/Windows replays are recorded below.

**Linux/x86_64 evidence (2026-10-14).** The final combined run passes all nine entries, including
ten public-API network cases and five separate-host WebView cases. The public API's 100 lifecycle
cycles finish at +128 KiB RSS, +0 threads, +0 descriptors; WebView's twenty cycles finish at
0/0/0. Static Host ABI checks cover fifteen binaries, runtime mapping covers 123 files across
UI/Web/Network processes, both with no violation; the header/export firewall remains clean.
Fixture unit tests (3), licensing tests (15), driver tests (3) and the 22-patch/18-overlay-file
license manifest gate pass. This is **8A Linux acceptance**, not closure of all Tranche 8.

Reproduce from the installed `05-ui` SDK and web dependency prefix:

```sh
python3 tools/build_webkit_crt_port.py \
  --sdk-root out/linux-host-ninja-debug/dist/05-ui \
  --deps-prefix out/linux-host-ninja-debug/port-tests/install \
  --work-root out/web-crt-webcore --reuse-tree --webkit --run-tests -j 8
```

Machine-readable result: `out/web-crt-webcore/acceptance-b3.json` (`crt_network_trust.cases`,
alongside the existing regression/ABI entries). The latest verification log is
`out/web-crt-webcore/tranche8a-gate-final.log`; these are generated, not committed artifacts.

#### 8B. Playback

Register a CRT-owned WebCore `MediaPlayerPrivate` engine consuming only installed `crtmedia`
public extractor/codec/frame/audio-sink C APIs. FFmpeg and host audio types stay private to
`libcrtmedia`. First acceptance is CPU H.264/AAC MP4, MP3 and PCM WAV from local files and the
browser's own HTTP loader: `canPlayType`, metadata, play/pause/seek/ended, volume/mute, frame pixels
and clock progression. One worker thread per player owns every crtmedia handle (blocking demux, decode,
audio write); the WebCore main thread owns notifications and painting. Unsupported, missing and garbage
sources are reported as `MEDIA_ERR_SRC_NOT_SUPPORTED`; repeated lifetimes are bounded.

The installed predecessor SDK must contain FFmpeg-enabled `libcrtmedia`
(`CRTMEDIA_ENABLE_FFMPEG=ON`) and export its extractor/codec APIs. Headers and a media DLL alone
are insufficient: the options-OFF Windows SDK omits those symbols. Windows links the SDK's
`libcrtmedia_dll.dll.a` import library; Linux VA-API stubs are not part of that host path.

Design decisions, kept visible:

- **Browser loader, not a second HTTP stack.** The private `crtmedia` HTTP transport (Curl/mbedTLS)
  is not linked into the Web process's OpenSSL/Curl closure. For `http(s):` the engine asks WebKit's own
  resource loader (so TLS policy from 8A, cookies, cache and redirects are the browser's) and spools the
  body to a temporary file that `crtmedia` plays. A consequence: playback starts when the transfer has
  finished; seeking inside an unfinished transfer and progressive start are **not** in this gate.
- **Engine identity.** WebCore's engine and player enumerations have no neutral CRT value; the engine
  uses the `MediaFoundation` slot (unused on every CRT host) rather than carrying a patch through the
  enumeration, the GPU-process serializers and `MediaPlayerPrivateRemote`.
- **CPU only.** Software decode (`prefer-hardware-decode` is off) and RGBA frames copied into WebKit's own
  Skia; no GPU decode or presentation (Tranche 9). MSE, WebCodecs, WebAudio, DRM and text/audio/video
  track objects are not enabled by this engine; the rate is 1x.
- **Host libraries stay out of the Web process.** `libcrtmedia.so` is linked against VA-API for its
  hardware decoder. The web stage builds `libva.so.2`/`libva-drm.so.2` *stubs* (every call fails with
  `VA_STATUS_ERROR_UNIMPLEMENTED`, `libcrtweb/platform/va-stub`) so no host driver library is mapped. The
  SDK's `libcrtmedia.so`/`libcrtui.so` also record the C++ runtime as an unversioned `libc++.so` that is
  a *separate file* from `libc++.so.1`; left alone, the loader would map two runtimes. The build tree
  therefore holds a `libc++.so` symlink to the SDK's `libc++.so.1`, and the gate asserts exactly one
  C++ runtime is mapped. Both are packaging defects of the SDK to fix at the source (Tranche 10).

**Linux/x86_64 evidence (2026-10-14).** `crtweb_media_test` (public header only; 11 s, 3/3 runs, plus
the combined gate) passes: `canPlayType` (supported, unknown type, unsupported codec), metadata and
`loadeddata`, duration ~1 s and 64x64, paused at zero, the first frame in the view, `play` moves the clock and
`pause` stops it, a later frame and a seeked frame differ from the first (view pixels), `seeked` and
`currentTime`, `ended`, volume/mute reflected, MP3 and WAV metadata/duration/ended, a garbage MP4
and a missing file both end in `MEDIA_ERR_SRC_NOT_SUPPORTED`, HTTP playback through the browser
loader, twelve load/play/reset lifetimes with the Web process at 17 -> 14 threads, and every web process
gone after the runtime. The combined gate (now eleven entries with `media_playback`) is green; the
Web process maps 36 files with no Host ABI violation, `libcrtmedia.so` and the two stubs mapped, one
C++ runtime. **Not verified at that checkpoint:** audio *output*. Every run used the wall
clock fallback (`crtmedia_audio_sink_open` failed); that alone did not establish whether hardware was
absent or inaccessible. The 2026-10-15 follow-up below distinguishes endpoint delivery from hardware.
Also unverified: other codecs than the test assets'
(H.264 Constrained Baseline, AAC, MP3, PCM) and A/V sync over long clips.

This was **8B Linux acceptance**, not closure of Tranche 8 at that checkpoint:
macOS/Windows replay of 8A and 8B and the 8C capture gate still remained.

#### 8C. Capture (separate from playback)

**Scope approved 2026-10-14:** add video camera capture and explicit application-owned permissions
to the browser target. Microphone, display capture and WebRTC remain deferred; `crtmedia/capture.h`
provides video capture only. This approval is not implementation/acceptance evidence and does not
enable devices or `MEDIA_STREAM` by itself. The permission/security design is owned by
[libcrtweb API policy](../design/libcrtweb_api_policy.md#camera-permissions-tranche-8c).

Gate sequence (after the CPU playback seam of 8B):

1. **Permission lifecycle.** Public C callback/completion and separate WebView messages, default
   denial, requesting/top-level origins, one-shot tokens, asynchronous pump delivery. Test no
   callback, denial, explicit grant, duplicate/late decisions, application revocation, frame/document
   replacement, view destruction and process/host death. A grant must not leak between views or
   runtime data stores. Host/platform denial is not overridden by the application.
2. **CRT source/device adapter.** Register a CRT video-capture factory behind WebCore's
   `RealtimeMediaSourceCenter`; consume only public CRT capture/frame types. Preserve secure
   contexts, frame policy and device-ID/label privacy. Device discovery must not start streaming;
   capture open/start follows explicit permission. Test unsupported audio/display requests,
   constraints, no device, busy/denied devices, negotiated format/rate, finite dequeue/cancellation
   and failure propagation; do not pretend that a grant guarantees available hardware.
3. **Browser frames and release.** `{video: true, audio: false}` produces a real video track and
   actual camera pixels through a `<video>` element and the CPU frame/WebView compositor contract.
   Test `track.stop()`, clones/last-owner release, revoke, navigation, unplug and process loss: end
   tracks, stop/release the CRT source, join workers, reject stale generation frames and avoid leaks.
4. **Physical-host replay.** Linux/x86_64 first, then macOS/arm64 and Windows/x64. Default automated
   gates never activate a camera; synthetic sources are test-only and labelled as such. Physical
   runs require explicit opt-in and record host, opaque device selection, negotiated format/rate,
   frame/pixel evidence and repeated-lifetime resource deltas. Synthetic/no-device passes do not
   close the physical gate. No device is opened by the current 8A implementation.


**Linux/x86_64 evidence, steps 1-3 (2026-10-14; synthetic source).** MediaStream is enabled in the CRT
build (`ENABLE_MEDIA_STREAM`) with no WebRTC, microphone or display capture. `crtweb_capture_test`
(public header only, 1.3 s, 3/3 runs, plus the combined gate entry `camera_capture`) uses a
*labelled synthetic camera* ("CRT test camera (synthetic)", selected by `CRTWEB_TEST_SYNTHETIC_CAMERA`,
lifecycle recorded in `CRTWEB_TEST_CAMERA_STATE`) so no physical device is opened. It proves: a secure
context from loopback HTTP; **no callback denies** and the device is never opened; an explicit denial;
the request carries requesting and top-level origins, the camera kind and device count; audio-only and
camera+microphone requests fail honestly (`OverconstrainedError` here, no grantable request reaches the
application); a grant yields one video track and no audio track whose frames reach a `<video>` and the view
(four known quadrant colours, a live sweeping bar); the source is opened and started only after the grant;
`track.stop()` stops and closes it; a completed request cannot be completed again; application
revocation ends the track and releases the source, and the next request asks the application again;
navigating replaces the document, releases the source and asks again; a second view asks on its own and
is not granted by the first view's grant; a request pending on a destroyed view is already denied. The
same pipeline over the separate-process WebView (host without a callback denies; grant; camera pixels
composed under crtui; revoke releases the device) is in `crtweb_webview_test`. The whole gate (eleven
entries) passes.

*Design and findings.* The capture source is `CRTVideoCaptureSource` over `crtmedia/capture.h`
(`libcrtweb/port/Source/WebCore/platform/mediastream/crt`): a `RealtimeVideoCaptureSource`, a device
manager that only enumerates metadata, a worker owning the capture handle with generation-checked stop, and
`VideoFrameCRT` CPU frames; `MediaPlayerPrivateCRT::load(MediaStreamPrivate&)` shows the track. Device
lists and constraint validation run in the UI process (WebKit's non-GLib path), so no capture IPC
supplement exists. Enabling MediaStream on a port with neither Cocoa nor GStreamer needed: patch 0023
(`RealtimeMediaSource.cpp` never includes `VideoFrame.h` outside those ports), exclusion of the stale
generic mock capture sources (explicit failure stubs), and a `UserMediaCaptureManager.h` for
`WebProcess.cpp`. WebKit scopes a grant to the frame: after `track.stop()` the same document is not asked
again, a revocation resets that.

**Not closed (honest status).**

- **Physical camera, Linux: run 2026-10-14 (opt-in, `crtweb_camera_probe --allow-physical-camera 3`).**
  Host Linux/x86_64, one UVC webcam ("USB Camera (046d:08d8)", V4L2 through `crtmedia` capture). The
  browser path ran end to end: one permission request from `http://127.0.0.1:<port>`, a grant for this run,
  `getSettings()` = 640x480 at 30 fps with hashed device/group ids, 20 samples of the view over two seconds
  all distinct pictures with every pixel lit, and `readyState` = `ended` after `track.stop()`. The probe
  exited 0. *Limits:* one run on one device; the negotiated format was the default request (no constraint
  matrix); source release was observed through the track state and a clean process exit, not by an
  independent device-busy check; repeated-lifetime resource deltas for the physical device were not
  measured (the synthetic device's lifecycle is gated, the physical one is a single run).
- **macOS/arm64 and Windows/x64 replay plan at the Linux acceptance point.** The macOS replay is now
  recorded below, followed by the Windows automatic replay and its remaining device limits.
  The original replay work included lifting host guards for the engine, capture
  gates and `crtweb_media_test`/`crtweb_capture_test`; the VA-API stubs and the `libc++.so` alias are
  Linux packaging only; the capture backends already exist in `libcrtmedia` (AVFoundation, Media
  Foundation); replace `/proc` and POSIX-socket helpers in the tests (`crtweb_media_test`'s process
  scan, the HTTP server header) with the replay hosts' equivalents, and run 8A, 8B and 8C gates plus a
  physical-camera probe on each host. The same applies to the 8A network gates and 8B playback gate.
- Microphone, display capture and WebRTC remain out of scope; the synthetic device is a test facility.

**macOS/arm64 replay of 8A, 8B and 8C (2026-10-14).** The combined gate (now eleven entries) passes from a clean
recompose of the 23 carried patches: `crtweb_network_test` with its fifteen browser/WebView trust cases, `crtweb_media_test`
(3/3 runs, Web process 28 -> 23..25 threads over twelve lifetimes, no process left), `crtweb_capture_test` with the
labelled synthetic camera (3/3), and every earlier B3-C8/Tranche 5 entry unchanged (public API lifecycle 100 cycles
+0 KiB/+0/+0, WebView 20 cycles +0/+0/+0, internal B7 +64 KiB/+0/+0, header/export firewall, static and runtime audits).
That initial combined replay used only the synthetic camera and did not record real audio output.
The two device-evidence gaps were subsequently closed by the opt-in follow-up below.
What differed from Linux, all fixed at the source and none in WebKit:

1. **`libcrtmedia.dylib` exported FFmpeg, libcurl and mbedTLS** (3,485 symbols besides its 103 `crtmedia_*`). WebKit's
   `curl_*` calls were linked to the embedded mbedTLS copy because libcrtmedia comes first on the link line (two-level
   namespace binds to the first dylib that defines the name): every `file:` load stalled after the response, and TLS would
   not have worked either. The dylib now exports `crtmedia_*` only (`libcrtmedia/cmake/crtmedia_exports_macos.txt`).
2. **libcrtmedia bound libc to the host.** CMake places `libc.dylib` after the frameworks, which re-export libSystem, so
   FFmpeg's `pthread_once`, `pthread_mutex_*` and friends were libsystem_pthread's and wrote into Bionic-layout objects
   (bus error in `avcodec_find_decoder_by_name` in the Web process). The CRT libc is first on its link line now;
   146 libc and 43 libm bindings are the CRT's, five libSystem ones remain.
3. **Two `libc++` images from the SDK.** `libc++.dylib` was a regular copy of `libc++.1.dylib` (the dist copier
   dereferenced the symlinks) with the same install name, so dyld mapped a second runtime for the consumers that recorded
   `@rpath/libc++.dylib` (libcrtmedia, libcrtui). `tools/create_dist.py` keeps the symlinks on macOS.
4. **Audit policy.** The OS media frameworks are libcrtmedia's PAL: accepted as dependencies of `libcrtmedia.dylib` and of
   nothing else, listed under `known_host_libraries`; the Web process also maps Apple's own `/usr/lib/libc++.1.dylib` and
   `libc++abi` through them, which the gate records (`system_cxx_runtime_files`) and does not count as a CRT runtime; the
   dyld shared cache (including its iOSSupport volume that `vmmap` lists), the frameworks' ICU data and log caches are
   OS-owned paths.
5. Overlay defects that only a macOS-shaped configuration exposed: `DrawingAreaCRT.cpp` included the player header
   unconditionally, `MediaPlayerPrivateCRT.h` declared the MediaStream bridge member without `ENABLE(MEDIA_STREAM)`;
   `CRT_DISABLE_CRTMEDIA_PLAYER` is a new configure knob (player off, stream off passes the Tranche 4/5 gates, which is how
   the cause of item 1 was isolated). A reused CMake cache keeps `ENABLE_MEDIA_STREAM=OFF` from an earlier configure:
   pass `--define ENABLE_MEDIA_STREAM=ON` once when switching a build directory.
6. The test observers: `crtweb_media_test` uses libproc (`proc_listchildpids`, `proc_pidinfo`) and `vmmap(1)`.

**macOS/arm64 device follow-up (2026-10-14, explicit user opt-in).** The same public browser API now
has physical-camera and real audio-output evidence on this machine:

| Evidence | Observed result |
| --- | --- |
| Physical camera | FaceTime HD camera, negotiated 640x480 at 30 fps, through `getUserMedia`, application grant, CRT capture and the view framebuffer |
| Consecutive camera executions | 3/3 pass; 20 sampled frames each, 19/18/19 distinct picture hashes; one application permission request each |
| Stop and reopen | `track.readyState=ended` after stop in every run; runtime destroyed and device reopened by the next probe |
| Audio output device | Default built-in MacBook Pro speakers (`system_profiler -json SPAudioDataType`, build-host observation) |
| HTML MP3 -> CoreAudio | 8000 Hz mono; 5184 accepted frames and nonzero samples; position 4693 frames; nine clock queries applied, seven with advancing position |
| Output errors/fallback | Zero write failures and zero sink-open/wall-clock fallbacks in the media run |
| Runtime mapping | `libcrtmedia` mapped, one CRT `libc++`/`libc++abi` image pair, Apple's framework runtime separately recorded; no mapping violation |
| Final combined gate | 12/12 pass, including the optional physical-camera entry; network trust, synthetic capture, header/export firewall and static/runtime audits pass |
| Lifecycle regression | Public API 100 cycles +16 KiB/+1 thread/+0 fd (within bounds), internal B7 +80 KiB/+0/+0, WebView 20 cycles +0/+0/+0; media threads 28 -> 24; no leftover work-tree processes |

The player emits `CRTWEB_AUDIO sink_closed` summaries under `CRTWEB_MEDIA_AUDIO_TRACE=1`. Counts use
the actual sink return value, retry short writes and bounded zero writes, and count nonzero PCM after
volume/mute processing. Clock counters increment only where a successful sink-position query actually
re-anchors the player clock. `media_playback.audio_output` records these fields in `acceptance-b3.json`.
Silence alone, decode-only output, a wall-clock fallback or failed writes cannot satisfy the real-output
requirement. This proves PCM delivery and a progressing device clock, not acoustic loopback, speaker
volume or a human listening result. The player still interpolates wall time between sink-clock anchors.

Reproduce after building the `05-ui` SDK and the web dependencies, using an existing composed work tree:

```sh
python3 tools/build_webkit_crt_port.py \
  --sdk-root out/macos-host-ninja-debug/dist/05-ui \
  --deps-prefix out/macos-host-ninja-debug/port-tests/install \
  --work-root /private/tmp/webB3f --webkit --run-tests --reuse-tree \
  --require-audio-output --allow-physical-camera -j 8
```

The explicit camera flag builds/runs `crtweb_camera_probe --allow-physical-camera 3` three times,
removes the synthetic-camera setting and records `physical-camera-run/run-{1,2,3}.log`; no images are
written. Without this flag the automated capture gate remains synthetic. The audio log is
`media-run/audio-output.log`. On macOS these device runs clear `DYLD_LIBRARY_PATH` and use the linked
SDK/port RPATHs: forcing a global loader override reproduced a typed-operator-new initialization
abort at CoreAudio entry, whereas the RPATH execution passes and retains both CRT and framework runtime
boundaries in its mapping audit. The media test reads its trace environment in `main()`, after CRT startup,
rather than in a Mach-O static initializer. The initial empty environment had previously hidden diagnostics.

Camera access was already available to the ordinary process on this host; no new TCC prompt appeared.
This does not remove the OS permission requirement on a fresh machine (the signed LaunchServices bundle
procedure in `crtmedia_encode_capture_acceptance.md` may still be required there). The three executions
prove stop/teardown/reopen but do not measure same-process physical-camera RSS/thread/fd growth, device
busy state independently, microphone capture or permission revocation at the OS level. Windows automatic
and subsequent explicit device replay are recorded below; Linux real-audio evidence,
common permission-fix replay and that camera resource stress remain open.

**Windows/x64 automatic Tranche 8 replay (2026-10-15).** The FFmpeg-enabled cumulative
`05-ui` SDK and the same `PlatformCRT` port pass the combined automatic gate. FFmpeg
8.1.2 was configured, built and installed using CRT mksh and CRT-built GNU Make;
Git Bash/MSYS tools are not the self-host evidence. The SDK was freshly configured
and `verify_dist` passed for `01-c` through `05-ui`; core CTests pass 155/155 and
twelve CPU/base-media CTests pass. These are prerequisite checks, not browser
hardware acceptance.

| Evidence | Final automatic result |
| --- | --- |
| Combined gate | 11/11: WebCore graph, process/API/lifecycle, B5/B6/B7, WebView, playback, synthetic capture and network |
| Network trust | 15/15 browser/WebView HTTP/TLS policy cases; PKI fixture uses its own subprocess-local OpenSSL config |
| Public API lifecycle | 100 cycles after warm-up: +4 KiB RSS, -3 threads, +0 handles |
| Internal B7 lifecycle | 100 iterations: -36 KiB RSS, -3 threads, +0 handles; at most two children |
| WebView lifecycle | 20 cycles: +16 KiB RSS, -2 threads, +0 handles; header/export firewall empty |
| CPU playback | Video/MP3/WAV, error cases, HTTP loader, playback/seek/pause/end; 24 measured lifetimes after 12 representative warm-ups |
| Media process | Same WebProcess PID before/after measurement; 23 -> 15 threads under the unchanged +3 bound |
| Media mapping | 54 files, `libcrtmedia.dll` present, one CRT `libc++.dll`/`libc++abi.dll` pair; no mapping violations |
| Static/runtime Host ABI | 17 binaries and 140 mapped files across UI/Web/Network roles; no violations |
| Synthetic capture | Permission grant/deny/cancellation plus token teardown, allocation churn, stale-token identity and unknown-handle rejection pass; no physical browser camera opened |
| Real audio output | Not verified: zero output sinks, 39 sink-open fallbacks; default render endpoint unavailable (`0x80070490`) |
| Cleanup | No remaining browser test processes from the work tree |

Toolhelp observations must be positive within a bounded interval, and the PID must
remain the same across measured playback lifetimes. Each playback cycle waits for
numeric `readyState >= 3` and requires clock progress. Three additional sequential
media/synthetic-capture runs pass with 23 -> 22, 23 -> 15 and 22 -> 15 threads.
Neither the representative warm-up nor media-readiness wait raises the resource
bound; this is not exhaustive early-play/loading-race coverage. Earlier transient
snapshot, cold-baseline and stalled-playback failures remain documented in
[`Windows investigation`](../bringup/crtweb_windows_tranche8.md).

The capture repetitions found a common permission-token use-after-free after
view destruction. Resolved tokens now retain inert storage in their runtime,
without WebKit request/device references, until runtime destruction. Completion
checks ownership before dereferencing; token addresses are not reused within
that runtime. Applications never free tokens and discard them at runtime teardown
(`libcrtweb_api_policy.md`). Five strengthened synthetic runs pass on Windows;
Linux/macOS must replay this changed regression before accepting the common fix.

Declared OLE32/Media Foundation imports are allowed only at `libcrtmedia.dll`'s
media PAL boundary. A direct host-CRT import in a CRT artifact still fails the
static audit. Native Media Foundation components transitively map SystemRoot
`ucrtbase`/`msvcrt`/`msvcp_win`; those OS runtime mappings are separately recorded,
not described as CRT-owned C++ images or as an absence of all host runtimes.
The clean import audit does not establish export privacy: Windows media DLL
export narrowing remains Tranche 10 work.

Reproduce after refreshing the FFmpeg-enabled `05-ui` SDK and web dependencies:

```powershell
python tools/build_webkit_crt_port.py `
  --sdk-root out/windows-host-ninja-debug/dist/05-ui `
  --deps-prefix out/windows-host-ninja-debug/port-tests/install `
  --work-root C:/crtw/b3w --reuse-tree --webkit --run-tests -j 8
```

Generated evidence: `C:/crtw/b3w/acceptance-b3.json`,
`out/web-windows-tranche8-ready-final.log`, `media-run/audio-output.log` and
`capture-run/capture-output.log` under that work root. Real output requires an
available default render endpoint and `--require-audio-output`; no sink was
started in the separate three-role endpoint probe. The current machine cannot
close that gate at the automatic-replay checkpoint. Browser physical-camera access requires explicit authorization
and `--allow-physical-camera`; it was not run in that checkpoint. The inadvertently selected
earlier native media device test is preserved in the investigation, not counted
as browser permission/camera acceptance. At that checkpoint Tranche 8 remained
open for these device gates; subsequent Windows device evidence follows below.

**Windows/x64 explicit device follow-up (2026-10-15).** The user subsequently
requested real output and physical-camera verification. The default render
endpoint now exists as **Remote Audio** (Windows endpoint observation); all
three default roles return success. This is an OS render endpoint, not proof
of the local physical speakers or a human listening result.

The first strict-output run still failed: WASAPI `Initialize` returned
`0x88890008` for mono float 44100/8000 Hz and mono S16 8000 Hz, while stereo S16
44100 Hz opened. The media PAL now enables documented shared-engine PCM
channel/rate conversion and its default-quality resampler, without exposing
native formats in the public API. The existing audio-sink CTest includes
write/drain regressions for those three decoded mono formats when Windows has
a working endpoint; the no-device path remains explicit. All four format
probes and that CTest pass, and `01-c` through `05-ui` SDK verification passes.

| Device evidence (focused and final combined replay) | Observed result |
| --- | --- |
| HTML MP3 -> WASAPI Remote Audio | 8000 Hz mono; 5184 accepted frames/nonzero samples; position 5184 frames; nine clock updates, five advancing |
| Output errors/fallback | Zero write failures and zero sink-open/wall-clock fallbacks; strict `require_audio_output=True` passes |
| Media lifetime | Same WebProcess PID across 24 measured lifetimes; 25 -> 27 threads, within unchanged +3 bound |
| Physical camera | QuickCam for Notebooks Deluxe; browser permission -> CRT capture -> video/view pixels; 640x480 at 30 fps |
| Three camera executions | 3/3 pass; 20 samples and 20 distinct pictures each; one application permission request per run |
| Stop/reopen | `readyState=ended` after every stop, successful reopen in the next process; no images saved |
| Final combined gate | 12/12 with output required and physical camera explicitly enabled; network trust 15/15 |
| Lifecycle regression | Public API 100 cycles -28 KiB/-3 threads/+0 handles; internal B7 -64 KiB/-3/+0; WebView 20 cycles -16 KiB/-2/+0 |
| ABI mapping | 18 static binaries, 140 runtime mapped files across UI/Web/Network roles; media maps 67 files with one CRT C++ pair, no violations |
| Cleanup | No remaining work-tree browser test processes |

The initial failed audio run is preserved in `out/web-windows-tranche8-devices.json`
and `.log`, not relabelled as a pass. Corrected focused output evidence is
`out/web-windows-audio-required.json` and `.log`. The final combined device replay
passes with both `--require-audio-output --allow-physical-camera` appended to
the Windows reproduction command above. Its evidence is
`C:/crtw/b3w/acceptance-b3.json`, `out/web-windows-tranche8-hardware-final.log`,
`media-run/audio-output.log` and `physical-camera-run/run-{1,2,3}.log` under the work root.
Camera runs do not yet establish same-process RSS/thread/handle bounds or an
independent device-busy check. Microphone, OS-level permission revocation and
acoustic loopback remain outside this evidence.
At that Windows checkpoint, Linux real-audio evidence, Linux/macOS replay of
the common permission-token fix and same-process physical-camera resource
stress remained open. The Linux follow-up below supersedes its Linux gaps.

**Linux/x86_64 physical-device follow-up (2026-10-15).** The user explicitly
authorized short audio output and physical video-camera runs. Initial endpoint
delivery was not hardware output: the account lacked the `audio` group in its
effective process credentials, playback nodes were not writable, and PipeWire
exposed only `auto_null` (`Dummy Output`, `support.null-audio-sink`, virtual).
The user added the group and logged in again; device access is now effective.
The agent did not change group membership, ACLs or session routing. PipeWire
still exposes Dummy Output, so Pulse PCM/position alone is not hardware proof.
Read-only session inspection after login finds the built-in audio card's active
PipeWire profile is `off` (`tranche8-linux-pipewire-profile.log` under the work
root); its profile/routing was not changed for this validation.

The new opt-in PAL diagnostic (`CRTMEDIA_AUDIO_SINK_TRACE`) identifies the
backend/device and a sink identity. The browser's close diagnostic joins that
identity to accepted nonzero PCM, device position and clock re-anchors; allocator
address reuse is handled chronologically. Strict Linux output requires an exact
`/dev/snd/pcmC<number>D<number>p` playback node and the complete native-format
fixture, not merely an open sink, decoded data or a wall/software clock.

The first post-login replay exposed a pre-existing PAL bug: `sscanf` reported
two numeric assignments even when its trailing `p` literal did not match a
capture node ending in `c`. It opened `/dev/snd/pcmC0D0c`; this result is invalid
as playback evidence, and no captured PCM was saved to a file. Complete-name
validation now rejects capture/suffix nodes before open. `WRITEI_FRAMES` also
uses the actual `snd_xferi.result`, not its input `frames` field. A deterministic
CRT-linked PAL test substitutes the OS boundary and verifies direction filtering,
lowest playback selection and partial/zero/full/invalid transfer counts without
opening any hardware. Bionic's cleaned `sound/asound.h` and the Linux kernel's
transfer implementation were checked; no host audio client library or public
native type was introduced.

The browser test generates a quiet 300-ms, 400-Hz, stereo S16/48kHz WAV fixture.
Early native output delivered only its last 2,112 frames: paused preroll discarded
decoded PCM. Retaining the first buffer exposed another loss path (8,192 of
14,400 frames): decoder backpressure released compressed samples instead of
retrying the same input. Both are corrected. Audio-only readiness requires real
decoded PCM, which stays owned until playback; video seek preroll retains
interleaved PCM in a queue capped at 4 MiB/256 buffers (overflow fails loading),
preventing a full audio decoder from blocking the target video frame. Input and
PCM ownership are released on seek/teardown. The fixture waits paused after
readiness, and strict output now requires all 14,400 frames with nonzero samples;
a tail fragment does not pass. The initial seek-event regression is retained
in `tranche8-linux-seek-checkpoint.json` and `.log` under the work root, not
relabeled as a combined pass.

| Focused strict replay | Run 1 | Run 2 |
| --- | --- | --- |
| Native sink | `/dev/snd/pcmC0D0p`, 48000 Hz, stereo | Same |
| Accepted fixture PCM | 14,400 frames; 28,320 nonzero samples | Same |
| Device position / advancing clock updates | 28,144 frames / 4 | 28,776 frames / 4 |
| Write errors / sink-open fallbacks | 0 / 0 | 0 / 0 |
| Browser seek/playback/lifecycle | Pass; 12 measured lifetimes | Pass; 12 measured lifetimes |
| WebProcess mappings | 36 files; one CRT C++ pair; no violations | Same |

These are hardware PCM/device-clock delivery results, not a listening or
acoustic-loopback test. Hardware position can include underrun silence; it is
not a count of delivered nonzero samples. Only the native stereo WAV fixture
is physical-output proof here: decoded mono/float sources (including MP3)
still use Pulse on this machine, whose default is virtual. No general hardware
format conversion, long-clip A/V-sync or all-codec output claim is inferred.
Trace evidence for both focused runs opens only `/dev/snd/pcmC0D0p`, with no
capture PCM node. Generated artifacts:
`out/web-crt-webcore/tranche8-linux-audio-complete-{1,2}.json`,
`tranche8-linux-audio-complete.log` and `tranche8-linux-browser-audio-syscalls.log`.
Earlier endpoint-only, wrong-direction and incomplete-fixture checkpoints remain
diagnostic artifacts, not substitutes for these complete-output results.

Linux also replays the strengthened permission-token regression three times:
stale completion after view destruction, allocation churn, replacement-request
identity and unknown opaque handles are rejected safely. Live-camera stress
exposed `currentTime` remaining zero because streams have no file decode worker
to own their clock. Stream play/pause now maintains monotonic anchors, and last
track end freezes the clock. Synthetic regressions cover advance, pause/resume
and a clone keeping the source live after its original track stops.

The opt-in physical gate now adds twenty measured lifetimes after three warm-ups
in one UI/WebProcess pair. Four modes rotate: stop the original track, explicitly
revoke its grant, navigate while capture is active, and stop the original while
a clone remains live before releasing the last clone. Every cycle requires at
least three fresh frame serials/two distinct hashes and unchanged +24 MiB/+2
thread/+2 descriptor bounds in both processes. Linux independently observes
zero `/dev/video*` descriptors after each release; a new process per capture
cannot satisfy this gate. The new macOS/Windows child-resource observers and
shared playback/capture fixes still require replay on those hosts; Tranche 8
is not closed by the Linux evidence alone. No camera images are saved.

Reproduce after refreshing and verifying the FFmpeg-enabled cumulative `05-ui` SDK:

```sh
cmake --build out/linux-host-ninja-debug --target crt-ui-dist -j 8
python3 tools/build_webkit_crt_port.py \
  --sdk-root out/linux-host-ninja-debug/dist/05-ui \
  --deps-prefix out/linux-host-ninja-debug/port-tests/install \
  --work-root out/web-crt-webcore --reuse-tree --webkit --run-tests \
  --require-audio-output --allow-physical-camera -j 8
```

Both device options are explicit opt-ins; default automatic gates do not open
a physical camera or require an audio endpoint. Basic media CTests pass 6/6,
device-evidence checks 12/12, licensing 15/15, CMake path checks 2/2, JSC tool
checks 6/6 and TLS-fixture checks 4/4. The 23-patch/63-modified-file/24-CRT-overlay
manifest passes. SDK refresh/verification evidence is
`out/web-crt-webcore/tranche8-linux-playback-fix-build.log`.

The final combined Linux gate passes **12/12**, including all fifteen browser/
WebView network-trust cases, strict complete-fixture output, synthetic capture
and physical camera. Final audio delivers the same 14,400 frames/28,320 nonzero
samples, position 29,688 frames, four advancing clock updates, zero write errors
and zero sink-open fallbacks. Media lifetimes keep one WebProcess (16 -> 13
threads); mapping is 36 files with one CRT C++ pair and no violations.

| Final physical-camera/lifecycle evidence | Result |
| --- | --- |
| Three ordinary camera runs | USB Camera (046d:08d8), 640x480/nominal 30 fps; 20 samples each, 19/20/19 distinct hashes; one permission request each, ended after stop |
| Same-process camera stress | 3 warm-ups + 20 measured cycles, WebProcess PID 72359 unchanged, all four release modes, 24 total permission requests |
| Stress final UI delta | +44 KiB RSS, +0 threads, +0 descriptors |
| Stress final WebProcess delta | -76 KiB RSS, -1 thread, +0 descriptors |
| Stress largest measured RSS delta | UI +44 KiB; WebProcess +1,512 KiB; thread/descriptor increases never exceed zero |
| Native camera release | Zero camera descriptors in both processes after every measured cycle |
| Public API / internal B7 / WebView lifetimes | 100 / 100 / 20 cycles; final RSS +52 / +240 / +0 KiB, final thread/descriptor deltas +0 |
| Host ABI | 18 static binaries, 132 runtime files across UI/Web/Network roles, no violations |
| Final cleanup | No remaining work-tree browser test process |

Generated final evidence: `out/web-crt-webcore/acceptance-b3.json`,
`tranche8-linux-hardware-final.log`, `media-run/audio-output.log`,
`physical-camera-run/run-{1,2,3}.log` and `physical-camera-run/stress.log`.
This closes the Linux device follow-up, not the current shared-code replay
obligations on macOS/arm64 and Windows/x64.

**macOS/arm64 initial shared-code replay (2026-10-15): 11/12; superseded by the closure below.**
The `05-ui` SDK was rebuilt and verified before replaying `ae1dac2`; the pristine pinned tree was
recomposed with 22 applicable patches from the 23-entry manifest. The earlier 2026-10-14 camera
result covered three ordinary processes. The Linux follow-up added twenty measured lifetimes in one
UI/WebProcess pair, and this new gate does not yet pass on this Mac.

| Combined replay evidence | Result |
| --- | --- |
| Browser PCM fixture | Complete 14,400 frames / 28,320 nonzero samples, 48kHz stereo, CoreAudio; default output is built-in MacBook Pro speakers |
| Sink clock | Position 8,849 frames before drain; four applied queries, two advancing; zero write failures or sink-open fallbacks |
| Playback regressions | Paused PCM retention, decoder backpressure, seek, media errors and twelve measured lifetimes pass; Web process threads 28 -> 24 |
| Network and synthetic capture | All fifteen browser/WebView trust cases and strengthened permission-token/stream-clock regressions pass |
| Public API / B7 / WebView lifetimes | 100 / 100 / 20 cycles; respective RSS -16 / +80 / +0 KiB, threads -1 / +0 / +0, descriptors +0 throughout |
| Host ABI | Static/runtime audits and header/export firewall pass; media mapping has one CRT C++ pair and no violations |
| Three ordinary camera processes | 3/3 pass, FaceTime HD camera, negotiated 640x480/30 fps, 20 samples each with 18/19/18 distinct pictures; track ended after stop |
| Same-process camera stress | **Failed**, not twenty completed lifetimes; initial three-warm-up run exceeds the Web thread bound (+4) at the first measured clone cycle |
| Cleanup | No remaining work-tree test/Web/Network processes after the combined and focused runs |

PCM delivery and sink-clock progress are device/API evidence, without human listening or acoustic
loopback. Device position is not a count of nonzero samples played. The existing B5 reference-image
comparison is unavailable (`reference or consumer frame missing`); this replay adds no WPE-reference
pixel-equivalence claim.

The failed checkpoint is preserved under
`out/macos-host-ninja-debug/tranche8-replay-20261015/`: `acceptance-initial.json`, `driver.log`,
`camera-stress-initial.log`, `audio-output-initial.log` and `audio-devices.json`. The SDK verification,
six basic media CTests, capture-conversion CTest, fourteen evidence-parser tests, fifteen licensing
tests and the manifest check pass. This is not a combined hardware acceptance.

**Camera investigation and test refinement.** Three warm-ups did not exercise clone mode before its
first measured invocation. Four warm-ups cover all four release modes, but that alone still fails
on native resource growth. Adding autorelease pools to PAL start/stop/release did not resolve the
growth; those experimental runtime changes were reverted. Sampling the CRT WebProcess and inspecting its fds identify
`com.apple.cmio.portraitblur.pteffect_init` / Espresso model initialization and AppleDepth model files.
The diagnostic idle run falls back to 20 threads and 18 descriptors (baseline 27/18), showing that
at least part of the immediate increase is asynchronous OS framework work. This observation does not
prove bounded twenty-cycle behavior or exclude a separate capture/restart defect.

The probe now keeps the +24 MiB RSS / +2 thread / +2 descriptor limits, initializes every release path,
and allows at most 2,000 ms for post-stop observations to meet them, recording `settled_ms` and
`settle_timeout_ms`. The evidence parser rejects observations beyond that deadline; historical
pre-settling traces remain readable. A timeout also records video DOM/track state for the next
reproduction. No OS camera effects, permissions, routing or system settings were changed, and no
camera images were written.

Focused results remain red:

- `camera-four-mode-warmup.log`: three measured cycles, then Web threads +3.
- `camera-native-observe.log` and `camera-native.sample.txt`: retained failure and subsequent idle
  observations of the native model work.
- `camera-bounded-settle.log` / `.json`: six measured cycles, then `video-clock` timeout at total
  iteration 11; one earlier cycle settled in 1,423 ms.
- `camera-restart-state.log` / `.json`: resource failure at the fourth measured clone cycle;
  `settled_ms=2011`, Web threads +4, descriptors +2. The deadline failure is retained as a failure.

The next camera work is to distinguish native first-frame startup from DOM playback/clock state on
the failing restart, and establish a bounded AVFoundation start/stop lifecycle while continuing to
report total process resources. macOS 8B is accepted with the complete fixture; macOS 8C repeated
physical lifetimes and the current Windows shared-code replay remain open. No libc/PAL/public ABI
change was retained from this replay's investigation.

**Native AVFoundation lifecycle comparison (same device, 2026-10-15).** To test whether the repeated
open/stop/release pattern itself contradicts macOS behavior, a separate build-host diagnostic uses
Apple SDK headers and libSystem, with no CRT runtime or WebKit. Its source and reproducible build/run
procedure are in [`libcrtmedia/tests/native`](../../libcrtmedia/tests/native/README.md); it is not a
CMake/CTest target, SDK dependency or substitute for the physical-camera gate. `otool -L` confirms
Apple frameworks, libSystem and libobjc rather than CRT libraries. Each new session requests the
default camera at 640x480/NV12, receives at least three actual frame callbacks, calls `stopRunning`,
removes the delegate, drains the serial callback queue, releases session/output/delegate ownership
and drains the lifetime's autorelease pool. No images are saved. Four warmups precede twenty
measured sessions in a single process, followed by thirty idle seconds; the two runs are sequential.

| Native host control | Post-release cadence | Completed measured sessions | Peak measured RSS / threads / fds delta | Final idle threads / fds |
| --- | --- | --- | --- | --- |
| `native-camera-control.log` | Observe at 300ms and 2,000ms; restart after 2,000ms | 20/20 | +2,080 KiB / +1 / +0 | 4 / 8 (baseline 17 / 8) |
| `native-camera-control-rapid.log` | Observe/restart after 300ms | 20/20 | +2,528 KiB / +0 / +0 | 3 / 8 (baseline 17 / 8) |

Both logs live beside the earlier failed replay artifacts. All 24 lifetimes including warmups receive
frames in each run; the rapid run also explicitly checks/logs `running_after_stop=0`. These observations
fit the existing resource bounds without a threshold increase. Before warmup, however, native
initialization grows RSS by tens of MiB and creates temporary framework threads/descriptors; even after
successful session teardown, the idle worker count takes seconds to drop. The earlier sampled CRT
WebProcess and this **separate native executable** are different experiments, not the same evidence.

The local Apple SDK's `AVCaptureSession.h` (`startRunning`/`stopRunning` method documentation) specifies
blocking session start/stop. It does **not** specify a process-wide RSS/thread/fd return-to-baseline
deadline or evicting AppleDepth/Espresso caches. Therefore:

- Repeated session start/stop/release is a valid native lifecycle on this machine, not a Linux-only
  assumption. The CRT PAL's stop/delegate-detach/queue-drain ownership sequence is consistent with
  that lifecycle; its integration still needs the failing browser test resolved.
- The four-mode warmup and bounded settling observation avoid treating the first clone path or an
  immediate post-stop snapshot as a leak. The +24 MiB/+2 thread/+2 fd and 2,000ms bounds remain CRT
  acceptance policy, **not an AVFoundation guarantee**. A single breach does not establish a leak;
  neither does a later decline establish twenty-cycle browser acceptance.
- The control deliberately omits CRT's allocator, the PAL's NV12-to-YUV420P conversion/frame queue,
  RGBA presentation, WebKit/DOM clocks, permission revocation, navigation and track cloning. It cannot
  certify those paths or identify the remaining failure. The earlier `video-clock` timeout is not
  evidence that native capture stopped supplying frames; the new DOM diagnostic must distinguish
  source delivery from autoplay/playback/clock state on its next reproduction.

At this checkpoint, keep the failed **11/12** result and gate unchanged. The next investigation compares direct CRT/PAL
capture lifetimes with this host-native control, then isolate DOM play/load/track replacement if the
direct path succeeds. Record total resources and ownership/first-frame/stop completion separately;
do not change global OS effects, relax thresholds or substitute a new process per cycle to obtain green.

**macOS/arm64 8C closure (2026-10-15, follow-up to `1085835`): final combined gate 12/12.**
The missing piece was not an unsupported native session lifecycle. A CRT/PAL-only probe reproduced
the progressively slower restarts without WebKit. Sampling finds Portrait/Espresso model work
calling CRT new/delete and spending most of its time waiting for the allocator lock or scanning the
heap: malloc searched live blocks and free coalesced by sweeping the whole heap. The original probe
grew from 1,640ms to 21,728ms per lifetime before its diagnostic run was terminated. A forward-only
free-coalescing experiment still failed at cycle 16 (only two frames within its first-frame budget).
Neither failed run is resource acceptance; the first also lacked the macOS resource-observer define
and reported RSS/threads `-1`.

The allocator now indexes released blocks by size class, keeps physical-neighbour links for local
coalescing and appends via a tail. It stays CRT-owned and globally locked, not host malloc or Scudo.
Only private heap bookkeeping changes; public Bionic-facing types, symbols and error semantics do
not. The minimum alignment is 16 bytes even on Windows frontends with 8-byte long double. Allocator
regressions and the cross-host replay boundary are owned by
[allocator acceptance](allocator_baseline.md#web-camera-follow-up-2026-10-15).

Allocator-only browser repeats passed twice but the combined run still caught thread +3; preserve
`acceptance-binned-first.json` and `camera-binned-combined-failed.log`. Native-stack observation now
shows CoreMedia/ANE/framework work rather than the former allocator bottleneck. PAL start, stop and
release now also bound their Cocoa autoreleases, just as open already did; callers need not supply a
Cocoa run loop or pool. This is correct native-call ownership, not proof that every extra native
cached thread was an autorelease leak. No WebKit stream-clock workaround, upstream patch, OS effect
setting change, threshold increase or separate-process substitution was used.

| Accepted camera evidence | Measured lifetimes | Peak UI RSS / threads / fds delta | Peak Web RSS / threads / fds delta | Max settling |
| --- | --- | --- | --- | --- |
| Direct CRT/PAL, allocator isolation (`pal-camera-binned.log`) | 20 after 4 warmups | Single process: +2,448 KiB / +0 / +0 | Not WebKit; lifetime 360-1,491ms | Snapshot at 300ms |
| Browser, allocator + native pools (`camera-binned-pools.log`) | 20 after 4 warmups | +16 KiB / +0 / +0 | +8,896 KiB / +2 / +0 | 0ms |
| Longer same-process browser run (`camera-pools-40.log`) | 40 after 4 warmups | +32 KiB / +0 / +0 | +9,888 KiB / +2 / +0 | 1,962ms |
| Final combined gate (`camera-combined-final.log`) | 20 after 4 warmups | +32 KiB / +0 / +0 | +4,944 KiB / +1 / +0 | 0ms |

The final Web baseline is 163,280 KiB / 48 threads / 14 fds; UI baseline is 35,776 KiB / 13 / 8.
Apple's model initialization now progresses during warmup, so the absolute baseline is higher than
the earlier slow/incomplete initialization checkpoint; this is not a claim of lower absolute RSS.
Each measured browser cycle keeps the same PID, receives three fresh frame serials and at least two
distinct pictures, rotates stop/revoke/active navigation/last-clone release, and ends the released
track. Permission counts are 25 for twenty cycles and 45 for forty. The +24 MiB/+2 thread/+2 fd and
2,000ms limits are unchanged. macOS still reports native camera-fd identity as unavailable (`-1`),
not a fabricated zero; the gate uses total process fd counts and the explicit worker/track lifecycle.
No camera image data is saved.

The faster runtime exposed a separate **observer race** in B7, not a failing lifetime test. An
external `vmmap` could reach an already-exited WebProcess while the test itself passed 100 cycles.
The macOS test now atomically publishes the first painted UI/Web/Network PID checkpoint and holds
it alive until the host observer releases it, before measured warmup. The driver rejects incomplete,
duplicate, malformed or wrong-parent records. This restores all three observed roles without
weakening the static/runtime mapping rules or adding an observer child to the measured UI process.

Final validation: **12/12 combined gates**; three ordinary physical probes pass with 20/20/20 distinct
pictures; complete 14,400-frame/28,320-nonzero-sample CoreAudio fixture and progressing sink clock;
twelve playback lifetimes (Web threads 29 -> 25); network trust 15 cases; public API/B7 100 cycles
each (+32 KiB/+0 threads/+0 fds each); WebView and Host ABI/export/header gates pass. CTest passes
190/190, deliberately excluding the two camera-to-file tests; allocator debug/canary/fault and
contention regressions pass, as do the 1M-operation and eight contention sweep cases. Device/map
evidence unit tests pass 17/17; licensing 15/15; patch-manifest and `05-ui` verification pass.
One ordinary probe retains a shutdown `Error sending IPC message: Bad file descriptor` stderr line;
it exits zero with the track ended and no surviving child. This gate does not certify stderr-free
shutdown. The existing B5 reference-pixel comparison remains unavailable, not newly proven here.

Final evidence is retained under `out/macos-host-ninja-debug/tranche8-replay-20261015/`:
`acceptance-camera-final.json`, `driver-camera-final.log`, `camera-combined-final.log`, the focused
logs above, `pal-camera-before.sample.txt`, `camera-binned-threads.sample.txt`, `b7-checkpoint.json`,
`ctest-binned.log` and allocator reports. The earlier failed JSON/logs remain distinct. This closes
**macOS 8C**, not Windows's current shared-code replay or native Linux/Windows allocator/fork replay.

Reproduce the combined gate (the physical-camera flag remains explicit):

```sh
python3 tools/build_webkit_crt_port.py \
  --sdk-root out/macos-host-ninja-debug/dist/05-ui \
  --deps-prefix out/macos-host-ninja-debug/port-tests/install \
  --work-root /private/tmp/webB3f --reuse-tree --webkit --run-tests \
  --require-audio-output --allow-physical-camera -j 8
```

**Windows/x64 current shared-code checkpoint (`d3687a9`, 2026-10-15): initial 10/12.**
The runtime and `05-ui` SDK were rebuilt after a fresh Windows preset configuration;
`01-c` through `05-ui` verification, core CTest 155/155, base-media CTest 5/5,
seed-42 allocator baseline through 1M operations and all eight contention cases
pass. Native allocator/fork evidence and limits belong to
[allocator acceptance](allocator_baseline.md#web-camera-follow-up-2026-10-15).

The initial combined replay is **not** a Windows closure. All earlier API,
network, mapping and synthetic-capture gates pass, but strict complete-fixture
output and physical-camera stress fail. The 48kHz stereo fixture delivers all
14,400 frames/28,320 nonzero samples with zero write errors/fallbacks, yet its
four sink-clock queries contain zero advances: decoder EOF was posted while
the entire short clip still fit in the device queue. Three ordinary physical
camera runs pass, but the first same-process stress fails at warm-up iteration
four (`clone-after-stop`), before any measured resource gate can be accepted.
The failed JSON and logs are retained as `out/web-windows-current-initial-failed.json`,
`out/web-windows-current-replay.log`, `out/web-windows-current-audio-initial-failed.log`
and `out/web-windows-current-camera-initial-failed.log`.

The CRT player now continues applying the sink clock until accepted PCM has
drained instead of equating decoder EOF with output EOF. Focused strict output
passes with all 14,400 frames/28,320 nonzero samples, position 14,400, 28 clock
updates/20 advances and zero errors/fallbacks; same-PID media threads 17 -> 19
remain within the existing +3 bound. No fixture duration, output counter or
acceptance threshold was changed. This shared player change requires focused
Linux/macOS replay; earlier results do not validate this new revision.

Camera investigation finds a Windows MF ring ownership defect: dequeue left a
storage pointer in its old slot, and teardown freed every slot rather than only
the still-queued range. A consumer could already have freed or reused that
storage. The private ring now clears transferred slots and discards only active
entries. A device-free regression in the existing frame test covers wrapped
indices, caller-held frames, inactive stale entries and repeated drain; it passes.
The corrected SDK focused physical gate passes three ordinary runs and twenty
measured lifetimes after four warmups in the same WebProcess (PID 6668). All
four release modes pass. Peak UI RSS/thread/handle deltas are +32 KiB/+0/+0;
peak Web deltas +1,212 KiB/+1/+2, final Web +1,196 KiB/+0/+0; settling is 0ms
throughout. Windows native camera-handle identity is unavailable (`-1`), not
reported as zero; total process handle counts remain bounded. Source include
closure passes 7/7 after bundling the portable private helper for every OS.
Evidence: `out/web-windows-current-focused-camera.json` and
`out/web-windows-current-camera-after-queue.log`. The final result follows below.
Early LLDB attach runs and focused
failures are diagnostic artifacts, not resource/camera acceptance; the original
limits and twenty measured cycles remain unchanged. No camera images are saved.

**Windows/x64 current replay closure (2026-10-15): final combined gate 12/12.**
The corrected queue backend, uninstrumented capture source and output-drain
player pass the complete driver with `--require-audio-output --allow-physical-camera`.
The initial 10/12 failure remains separate; it is not relabelled as acceptance.

| Final Windows evidence | Measured result |
| --- | --- |
| Complete PCM output | 48kHz stereo, 14,400 accepted frames/28,320 nonzero samples; position 14,400; 28 applied clock updates/18 advancing |
| Output errors/fallback | 0/0; Remote Audio endpoint, not local-speaker listening or acoustic loopback |
| Playback lifecycle | 24 measured lifetimes after twelve warmups, same PID; Web threads 25 -> 19, unchanged +3 bound |
| Ordinary physical camera | QuickCam for Notebooks Deluxe, 640x480@30; 3/3, twenty samples/twenty distinct pictures per run, ended after stop |
| Same-process camera stress | 4 warmups + 20 measured lifetimes, WebProcess PID 17388 unchanged; stop/revoke/navigation/last-clone release; 25 permission requests |
| Camera UI baseline / final delta | 18,240 KiB/16 threads/245 handles; -24 KiB/-3 threads/+0 handles |
| Camera Web baseline / final delta | 42,192 KiB/24 threads/291 handles; +876 KiB/-2 threads/-4 handles |
| Peak measured camera deltas | UI +12 KiB RSS, no thread/handle growth; Web +876 KiB RSS, no thread/handle growth; maximum settling 0ms |
| Freshness and ownership | At least three fresh frame serials/two distinct hashes per measured cycle; native camera handle identity unavailable (`-1`), total handles bounded |
| API/B7/WebView lifecycle | 100/100/20 cycles; RSS -4/-36/+4 KiB, threads -3/-3/-2, handles +0 throughout |
| Host ABI | 18 static binaries, 140 mapped files across UI/Web/Network; media maps 67 files, one CRT C++ image pair; no violation; header/export firewall clean |
| Network and cleanup | All fifteen browser/WebView trust cases pass; no remaining work-tree browser test process |

The +24 MiB/+2 thread/+2 handle camera limits and 2,000ms settling budget are
unchanged. Separate-process probes cannot substitute for the twenty same-PID
lifetimes. No camera images are saved and no microphone capture is requested.
The existing Windows B5 pixel-diff reference remains unavailable; successful
surface transport does not establish a new native-WPE pixel comparison.

Evidence: `C:/crtw/b3w/acceptance-b3.json`, `out/web-windows-current-final.log`,
`media-run/audio-output.log`, `physical-camera-run/run-{1,2,3}.log` and
`physical-camera-run/stress.log` under that work root. Reproduce with the
Windows driver command above, adding both explicit device options after an
FFmpeg-enabled `05-ui` refresh. Core/allocator, source-closure and software
regressions are recorded in `HISTORY.md` and allocator acceptance.
The final artifact refresh verifies `01-c` through `05-ui`, includes the new
private header in the 04 source archive and passes the five base-media tests
(`out/web-windows-current-artifacts-final.log`, `out/web-windows-current-media-final.log`).

Windows Tranche 8 replay is complete. The focused macOS playback replay below
also passes. At this checkpoint cross-host closure still required focused Linux playback of the
common device-drain correction (including the complete PCM fixture and
device-free frame regression), and native Linux
allocator/fork execution of the indexed allocator; the final Linux section below closes both. The earlier Linux/macOS
captures remain accepted; the MF ownership fix is Windows-specific. Packaging,
Windows media export narrowing and GPU integration remain their existing
Tranche 10/9 work, not implicitly completed by this replay.

#### macOS focused 8B replay of the Windows output-drain fix (2026-10-15)

Replayed `8c989ac` on macOS/arm64 without another runtime patch. Refreshed and
verified cumulative SDKs `01-c` through `05-ui`, then used the port driver with
`--reuse-tree --webkit`, targets `crtweb_media_test`, `WebProcess` and
`NetworkProcess`, and `-j 8`. The work tree is `/private/tmp/webB3f`; its build log
confirms recompilation of `MediaPlayerPrivateCRT.cpp` and the media test, not
execution of pre-change binaries. The SDK/dependency roots are
`out/macos-host-ninja-debug/dist/05-ui` and
`out/macos-host-ninja-debug/port-tests/install`.

Ran `build_webkit_crt_port.run_media_gate(..., "macos", True)` three times in
separate work directories, with `CRTWEB_MEDIA_TRACE=1`. This is the existing
strict audio-output gate plus a focused check that the complete generated
fixture's sink-close position is at least its accepted 14,400 frames. All
three runs pass both checks; no test deadline or resource bound was relaxed.

| Run | Accepted PCM frames / nonzero samples | Position at sink close | Applied clock updates / advances | Web threads over 12 measured lifetimes |
| --- | --- | --- | --- | --- |
| 1 | 14,400 / 28,320 | 18,479 | 48 / 44 | 28 -> 25 |
| 2 | 14,400 / 28,320 | 18,629 | 48 / 44 | 28 -> 25 |
| 3 | 14,400 / 28,320 | 18,048 | 47 / 43 | 27 -> 25 |

The fixture is 300ms, S16/stereo/48kHz. CoreAudio position is sampled at sink
close after playback, so it may exceed the accepted frame count; it is not a
second PCM-delivery count or a sample-exact observation of the `ended` event.
Current default output was the built-in MacBook Pro speakers, independently
observed with `system_profiler SPAudioDataType`. Each run has zero sink-open
fallbacks and zero write failures across its sinks. This proves API/device
delivery and clock progress, not human listening or acoustic loopback.

The existing media assertions also pass: metadata/pixels, play/pause/seek/EOF,
paused-preroll retention of the complete PCM clip, volume/mute, MP3/WAV,
local/HTTP sources, unsupported/missing/garbage sources and repeated teardown.
The original Web thread bound (+3) is unchanged. Each runtime map audit has
1,665 mapped-file observations, no violations, `libcrtmedia` mapped, no libva
mapping, and one CRT runtime pair (`libc++.1.0.dylib`, `libc++abi.1.0.dylib`).
Apple's `/usr/lib` libc++/libc++abi pair is recorded separately as an OS media
framework dependency. Device runs remove `DYLD_LIBRARY_PATH` and retain the
linked RPATHs. The tests' child teardown assertions pass and a final process
inventory finds no remaining WebProcess/NetworkProcess from this build.

Base-media CTest passes **7/7** (frame, GPU frame, transport queue, format,
player, audio sink and AVFoundation conversion). The frame test includes the
new portable MF queue ownership regression; this is not execution of the
Windows MF device backend on macOS. Stage-source include closure passes 7/7
and the generated macOS 04 source archive contains `capture_mf_queue.h`.
Device-evidence/parser tests pass 17/17, licensing 15/15, and the manifest gate
accepts 23 patches, 63 modified files and 24 CRT-owned files.

Local evidence root: `out/macos-host-ninja-debug/tranche8b-drain-replay-20261015/`.
It contains `sdk-build.log`, `webkit-build.log`, `base-media.log`,
`audio-devices.txt`, `replay.log`, `summary.json` and, under `run-{1,2,3}/`,
`result.json`, `gate.log` and `media-run/audio-output.log`. The local
`replay.py` harness calls the checked-in gate and records the additional drain
check; it does not replace the gate implementation. Earlier failed/final
Tranche 8 checkpoints are preserved in their original directories.

This completes the macOS focused common-output-drain replay. It is **not** a
new combined 12-gate or camera run: earlier macOS 8A/8C acceptance remains
unchanged. Linux focused playback of this correction and native Linux
allocator/fork regressions were still open at this checkpoint; both close below.

#### Linux focused 8B replay and Tranche 8 closure (2026-10-15)

Replayed the `ef081d2` runtime plus the Linux-private correction below on
Linux/x86_64. A fresh preset configuration and `crt-c-build` rebuild the current
runtime; cumulative SDKs `01-c` through `05-ui` all verify. The WebKit driver
rebuilds the composed tree with `--reuse-tree --webkit`, targets
`crtweb_media_test`, `WebProcess` and `NetworkProcess`, and `-j 8`. Its build log
records recompilation of `MediaPlayerPrivateCRT.cpp`; this is not execution of
the pre-drain-fix common player. After correcting the Linux media backend, a
final `crt-ui-dist` refresh regenerates and verifies all five SDK manifests.

The first focused run failed video/MP3/HTTP `ended` assertions even though its
ALSA fixture delivered complete PCM. The common player correctly waited for
output, but the Pulse MP3 stream's native read cursor stopped at 8,902 of 9,216
accepted frames. A one-frame-prebuffer experiment did not fix the failure and
was reverted. Both checkpoints remain in their original evidence directories.
PipeWire 1.6.2's
[`stream_process` and native drain handling](https://github.com/PipeWire/pipewire/blob/1.6.2/src/modules/module-protocol-pulse/pulse-server.c)
explain the remaining sub-period tail: underrun may generate silence instead
of consuming a buffer smaller than `minreq`, whereas native DRAIN flushes it.

`audio_sink_linux.c` now detects a stopped Pulse stream with accepted data
beyond the native read cursor, sends DRAIN, waits for its acknowledgement and
queries the real cursor again. Position is never synthesized from elapsed time
or accepted writes. Drain/re-query errors remain errors with the caller's
position unchanged. Seven socketpair/fork fake-peer cases exercise the actual
packet/ACK path: playing, stopped-and-consumed, stopped-with-tail, drain error,
re-query error, malformed playing flag and cursor beyond accepted data. They
contact no sound server or device. No public ABI, shared WebKit player,
macOS/Windows backend or OS audio configuration changes in this follow-up.

The checked-in evidence parser also now requires the complete PCM fixture's
position to reach at least its accepted 14,400 frames, not merely advance once.
The new regression rejects positions 1 and 14,399, accepts 14,400 and 14,401,
and distinguishes generic Pulse endpoint evidence from native ALSA delivery.
Its negative control fails before this predicate is added; the final parser
suite passes **18/18**. The already recorded macOS/Windows fixture positions
satisfy the same floor; this turn does not claim new execution on either host.

With the final verified SDK, three separate calls to the existing strict
`run_media_gate(..., "linux", True)` all pass, with `CRTWEB_MEDIA_TRACE=1`:

| Run | Accepted PCM frames / nonzero samples | Position at sink close | Applied clock updates / advances | Web threads over 12 measured lifetimes |
| --- | --- | --- | --- | --- |
| 1 | 14,400 / 28,320 | 25,360 | 13 / 10 | 17 -> 14 |
| 2 | 14,400 / 28,320 | 22,656 | 13 / 10 | 16 -> 14 |
| 3 | 14,400 / 28,320 | 23,752 | 13 / 10 | 17 -> 14 |

The 300ms S16/stereo/48kHz fixture uses native ALSA
`/dev/snd/pcmC0D0p`, with zero fallback or write failures across sinks. Position
is sampled at close and may exceed accepted PCM; it is neither another delivery
count nor a sample-exact observation of `ended`. PipeWire's separately observed
default remains virtual `auto_null` / Dummy Output. Pulse-only streams establish
endpoint/clock behavior, not physical output; the complete fixture's direct
ALSA route establishes native device delivery. Neither constitutes human
listening or an acoustic loopback test.

All existing media assertions pass: metadata/pixels, play/pause/seek/EOF,
complete paused-preroll PCM retention, volume/mute, MP3/WAV, local/HTTP sources,
unsupported/missing/garbage sources and repeated teardown. The unchanged Web
thread bound is +3. Each runtime audit records 35 mapped files, no violation,
`libcrtmedia` and exactly one CRT libc++/libc++abi pair. Linux's private libva
stubs remain mapped as documented, not hardware-decoding evidence. Child
teardown assertions pass; final inventory finds no surviving WebProcess,
NetworkProcess or media-test process from this build.

Base-media CTest passes **7/7** (frame, GPU frame, transport queue, format,
player, audio sink and Linux audio sink). This includes the portable MF queue
ownership regression, not execution of the Windows MF device backend. Source
include closure passes 7/7 and the final 04 source archive contains
`capture_mf_queue.h`. Licensing passes 15/15; the patch gate accepts 23 patches,
63 modified files and 24 CRT-owned files. Driver/JSC helper tests pass 2/2 and
6/6, and TLS fixture tests 4/4; the latter are not another full browser network
replay. Native core passes 136/136, allocator environment sweep 130/130,
baseline correctness tiers 4/4 and contention cases 8/8; measurements and
performance caveats belong to
[allocator acceptance](allocator_baseline.md#linux-native-indexed-allocator-replay-2026-10-15).

Reproduction from the repository root:

```sh
cmake --fresh --preset linux-host-ninja-debug \
  -DCRTMEDIA_ENABLE_FFMPEG=ON -DCRTGFX_ENABLE_SKIA=ON \
  -DCRTGFX_ENABLE_GPU_BACKEND=ON -DCRTUI_ENABLE_LVGL=ON \
  -DCRT_USE_IMPORTED_LIBCXX=ON
cmake --build out/linux-host-ninja-debug --target crt-c-build crt-ui-dist -j 8
python3 tools/build_webkit_crt_port.py \
  --sdk-root out/linux-host-ninja-debug/dist/05-ui \
  --deps-prefix out/linux-host-ninja-debug/port-tests/install \
  --work-root out/web-crt-webcore --reuse-tree --webkit \
  --target crtweb_media_test --target WebProcess --target NetworkProcess -j 8
```

Then call the checked-in `run_media_gate` three times in separate directories
with the same build/SDK/dependency roots, strict Linux output enabled and
`CRTWEB_MEDIA_TRACE=1`. The generated local helper
`out/web-crt-webcore/tranche8_close_replay.py` performs those calls with
run-local font caches; it does not replace or weaken the gate. Final evidence
under `out/web-crt-webcore/` is `tranche8-close-playback/{summary.json,run-{1,2,3}/}`,
each run containing `result.json`, `gate.log` and `media-run/audio-output.log`.
Build logs are `tranche8-close-{configure,sdk-build,webkit-build,sdk-final}.log`;
the final media log is `tranche8-close-base-media-final.log`. Earlier failures
are preserved as `tranche8-close-playback-initial-failed/` and
`tranche8-close-playback-prebuf1-failed/`; the corrected component-install
checkpoint before final manifest regeneration is
`tranche8-close-playback-pulse-fixed/`. Parser negative-control and protocol
test logs are `tranche8-close-drain-negative-control.log` and
`tranche8-close-pulse-rpc-test.log`.

**Closure:** 8A network, 8B CPU playback and 8C video capture/permission/lifetime
gates are accepted on Linux/x86_64, macOS/arm64 and Windows/x64. The common
output-drain correction and shared indexed allocator now have their required
native replays. This focused Linux follow-up does not claim a new combined
12-gate run or reopen cameras: prior three-host 8A/8C evidence, including the
four-mode twenty-same-PID physical lifetime gates, remains accepted. Earlier
failed checkpoints are preserved, not relabeled as passes. Tranche 8 is closed;
GPU integration (9), isolated packaging/security (10) and the stated media
exclusions remain separate work.

### 9. GPU integration

Start with WebKit's Skia output to an external surface consumed by CRT's Skia.
Later optimize to a shared/zero-copy GPU buffer import. Sharing one Skia
context between WebKit and CRT is explicitly not an initial acceptance item.

Keep three boundaries separate:

1. **9A, GPU final composition.** Consume the current CPU BGRA shared-memory
   WebView image through `crtui` External Surface into a real application-owned
   Ganesh GPU target and native presentation surface. Prove pixels, UI ordering,
   ancestor clip/opacity, input, viewport resize, fresh updates, producer death/
   replacement and teardown. Classify this as `cpu-upload`, not zero-copy.
2. **9B, GPU resource production/import and synchronization.** Define a separate
   negotiated resource contract without changing the frozen v1 fields. Prove
   backend/device/format compatibility, native GPU-ready and release completion,
   bounded in-flight buffers and explicit CPU fallback. Decide producer/
   GPUProcess ownership without passing private Skia objects between different
   revisions or loading native GPU clients into the incompatible shared-runtime
   browser address space. The CPU `crtgfx_gpu_fence` is not a native GPU fence.
3. **9C, lifecycle and host closure.** Add GPU device/process loss and recovery,
   resize while resources are outstanding, delayed/closed consumers and bounded
   repeated resource lifetimes. Replay the accepted path and relevant existing
   lifecycle/Host ABI gates on macOS/arm64 and Windows/x64 after Linux. The
   deferred GPU-process lifetime gate is the CRT-owned worker selected in 9B,
   explicitly confirmed on 2026-10-17. An application-side GPU target alone
   does not fulfill it. Upstream GPUProcess activation/accelerated WebCore is
   not claimed or required by this owned-worker boundary.

Public ownership/completion policy belongs to
[GPU final composition](../design/libcrtweb_api_policy.md#gpu-final-composition-tranche-9a).
9A does not turn on accelerated compositing, enable GPUProcess, replace the
WebCore EGL/GLES stubs or change the CPU `crtweb_view_copy_frame()` API.

#### 9A Linux implementation and replay (2026-10-15)

`crtweb/skia.h` adds an optional header-only C++ companion,
`crtweb::compose_gpu`, over the existing WebView/External Surface provider. It
requires an application-owned Ganesh context and a GPU surface belonging to
that exact context, rejects raster/null targets or an abandoned context, and
reports no-frame as `CRTUI_WOULD_BLOCK`. After synchronous scene composition it
flushes and checks submission with a CPU completion wait, even after partial
composition. Successful submission completes before source retirement; an IO
failure requires recovery/teardown of the GPU path before retiring sources. Presentation
and `frame_presented()` acknowledgement remain the caller's responsibility.
The helper never acknowledges a failed composition. No new public C ABI or
WebKit patch; the browser and application retain independent Skia revisions.

The new `crtweb_webview_gpu_test` launches the real `crtweb_host`, not a synthetic
producer, and uses an actual GPU offscreen target. Exact acceptance-only GPU
readback proves red UI below, clipped-out web pixels, 50% web opacity, and blue
UI above. A forwarded pointer changes the page from green to magenta; retained
old source pixels remain green across subsequent update/resize. A fresh GPU
target and browser viewport change from 160x100/90x70 to 200x120/130x90. Twenty
alternating updates use the same browser/GPU context and retain the +24 MiB RSS,
+2 thread/+2 descriptor bound. Killing the WebProcess leaves the last picture
composable, then a new process produces a fresh green picture. Shutdown reaps
the owned browser host/children. Live mode additionally wraps a real acquired
Wayland/Vulkan swapchain image and presents the requested bounded frame count.

The source image is explicitly checked **not** to be texture-backed; the target
must belong to the real GPU context. Production performs upload and GPU
composition, not GPU readback. The test cannot establish GPU-buffer IPC,
zero-copy or GPU WebCore rendering merely by observing a GPU-backed target.

**Linux 9A result:** the final checked-submit implementation passes offscreen
**3/3** and live Wayland/Vulkan **3/3**, each live run presenting **5/5** frames.
Every run passes the pixel/input/resize/twenty-update/recovery/cleanup assertions;
twenty-update deltas are **+40 KiB RSS, +0 threads, +0 descriptors**. These are
application host counters after the initial warm draw, not device-memory or
native-fence leak measurements. The GPUProcess/device resource-lifetime gate
remains separate. Browser static audits pass; each of the six repeated Linux
runs records 104 mapped-file observations across Host/Web/Network (the final
public CLI run records 105), zero violations, and the CRT-owned
`libEGL.so`/`libGLESv2.so` link stubs. Native GPU clients remain
in the application, not those browser processes.

The existing CPU WebView test also passes: three process recoveries and twenty
launch/load/resize/shutdown cycles with +0 KiB RSS/+0 threads/+0 descriptors.
Wire layout/transport and CPU surface CTest passes 3/3. GPU evidence/map helper
tests pass 8/8; existing device evidence 18/18, driver path helpers 2/2, JSC
helpers 6/6, source include closure 7/7 and licensing 15/15 pass. The unchanged
patch manifest remains 23 patches/63 modified/24 CRT-owned files; no new upstream
patch was introduced. Public header leak scan is clean.

Reproduction (after the existing FFmpeg/Skia/GPU/LVGL/imported-libc++ Linux
predecessor configuration; fresh configuration log below):

```sh
python3 tools/build_webkit_crt_port.py \
  --sdk-root out/linux-host-ninja-debug/dist/05-ui \
  --deps-prefix out/linux-host-ninja-debug/port-tests/install \
  --work-root out/web-crt-webcore --reuse-tree --webkit --run-gpu-tests -j 8
# Add --gpu-window-frames 5 to exercise a real presentation window.
```

`--run-gpu-tests` can run alone (no audio/camera tests) or alongside `--run-tests`.
It refreshes the in-tree consumer, builds the real host and writes a separate
`acceptance-gpu.json`. A missing GPU/host returns 77 in ordinary CTest (an actual
skip); the driver passes `--require-gpu` and fails instead. The evidence parser
requires the expected native backend, all checks, exact live frame count and
the `source=cpu-shm interop=cpu-upload` classification. This first implementation
observed native browser mappings on Linux only; the Windows extension and replay
are recorded below. macOS was still pending at this checkpoint, not a clean
empty runtime audit; its native observer and accepted replay are recorded below.

Strict negative controls fail for a missing host and a child-scoped unusable
Vulkan ICD selection (both exit 1); missing host without strict mode exits 77,
and an invalid live frame count exits 2. The parent environment is unchanged.
A separate fresh GPU-OFF configuration, reusing the pinned local Skia/LVGL
sources, keeps CPU WebView/surface targets and omits the GPU target. Ordinary
GPU CTest without `CRTWEB_HOST` is explicitly skipped, not GPU acceptance.

Evidence under `out/web-crt-webcore/`: `tranche9-configure.log`,
`tranche9-consumer-build.log`, `tranche9-host-build-driver.log`,
`tranche9-replay-final.log` and `tranche9-replay/summary.json`; each
`tranche9-replay/{offscreen,window}-{1,2,3}/` has `result.json`, `gate.log` and
`gpu-webview-run/{stdout,stderr}.log`. The generated local `tranche9_replay.py`
calls the checked-in strict gate without replacing it. The earlier successful
pre-submit-check repetition remains in `tranche9-replay-before-submit-check/`.
The final public CLI invocation is recorded in `tranche9-gpu-driver-final.log`
and `acceptance-gpu.json`. CPU regression, wire, negative controls and GPU-OFF
configuration logs are `tranche9-cpu-webview.log`, `tranche9-wire-ctest.log`,
`tranche9-negative-controls.log`, `tranche9-ctest-skip.log` and
`tranche9-gpu-off-configure-final.log`. An initial GPU-OFF configure lacked its
local pinned source paths and failed; it is preserved separately rather than
claimed as a build failure or a successful CPU-only build.

The first pixel checkpoint raced the initial blank frame: a title notification
is not a paint-completion event. The test now waits for expected source pixels
as well as fresh serial/geometry before evaluating GPU output. The retained
failure is `out/web-crt-webcore/tranche9-initial-gpu.log`. The first map-audit
checkpoint correctly exposed missing observer classifications, not a GPU
render failure: CRT-owned no-context EGL/GLES stubs are recorded explicitly,
and the CRT's own immediately unlinked, non-executable memfd data mappings are
reported separately as `ANON_SHM`, never allowed as arbitrary `/tmp` libraries.
That checkpoint is retained in `tranche9-map-checkpoint-failed.json` and
`tranche9-gpu-final-offscreen-1.log`.

GPU-buffer IPC, native producer/consumer GPU fences, accelerated WebCore and
GPUProcess/device-loss acceptance remain open. Host replays below extend this
9A slice without replacing the existing CPU surface contract and fallback.

#### 9A Windows/x64 replay (2026-10-15..16)

Replayed `332e904` plus the Windows observer/build-order changes.
This is the same CPU-shared-memory upload path, now targeting D3D12/Ganesh;
it does not request audio or physical camera capture. At that checkpoint macOS
replay was still pending (recorded below); 9B and 9C remain independent open gates.

The initial fresh combined `-j 8` SDK/consumer build failed: the release-grade
`02-cxx` publisher replaced its directory while the GPU/CPU-surface consumers
read libc++ headers, producing `cstddef` not found. The three Web consumers now
wait for `crt-libcxx-dist`, not just its earlier lightweight sysroot writer.
A second fresh configuration and the same combined `-j 8` command succeed;
all cumulative SDKs `01-c` through `05-ui` verify. The failed checkpoint remains
`out/web-windows-tranche9a-sdk-consumer-build.log`; corrected configuration/build
logs have `-final` suffixes. This is target ordering, not a `-j 1` workaround.

The acceptance-only Windows Toolhelp observer lists modules separately for the
browser Host/Web/Network processes. Missing roles fail rather than returning
an unobserved audit. Runtime roots/OS DLL closure and CRT EGL/GLES link stubs
are classified separately; direct host-runtime imports remain subject to the
existing static PE audit. Final-compositor D3D12/shader compiler, Vulkan and
native GL clients are rejected in the CPU browser path. The existing desktop/
media D3D11/DXGI contract is not evidence of accelerated WebCore rendering.
GPU evidence/map tests pass 11/11. Wire/CPU-surface CTest passes 3/3; a separate
no-host GPU CTest is explicitly skipped, strict no-host returns 1 and invalid
live frame count returns 2, not GPU acceptance.

**Windows GPU result:** offscreen **3/3**, live D3D12/Ganesh **3/3**, each live
execution presenting **5/5** frames. All six pass pixel readback, UI ordering,
ancestor clipping/opacity, input-driven pixel change, fresh viewport/target
resize, retained old-image immutability, twenty alternating updates, producer
termination/replacement and child cleanup. Twenty-update RSS deltas are
**+128/+144/+140 KiB** offscreen and **+136/+112/+128 KiB** live, each **+0 threads,
+0 handles**. The unchanged bounds remain +24 MiB/+2/+2. These are application
working-set observations, not device-memory or native-fence lifetime evidence.
Every run observes 141 module records across Host/Web/Network, both CRT-owned
GL stubs and zero runtime/static browser Host ABI violations. OS-transitive
`msvcrt.dll`, `ucrtbase.dll` and `msvcp_win.dll` mappings are recorded separately;
they do not waive the static prohibition on direct host-runtime imports.

The existing CPU WebView replay also passes: three producer recoveries and
twenty host/load/resize/shutdown lifetimes, final **-4 KiB RSS/-2 threads/+0
handles**, settling 0ms. A separate fresh GPU-OFF configuration registers CPU
WebView/surface targets but no GPU consumer. No browser/media implementation,
public API, wire field or WebKit patch was changed for this replay.

The refreshed B7 gate passes all **100** view lifetimes: after ten warmups,
**-52 KiB RSS/-3 threads/+0 handles**, at most one Web and one Network process
alive together. Its static PE audit covers **18** binaries, and its UI/Web/
Network runtime audit observes **140** module records, both with zero violations.
Evidence: `out/web-windows-tranche9a-lifecycle.{json,log}`.

The first local repetition used a generated launcher with `CREATE_NO_WINDOW`.
Its CRT relaunch created a `conhost` child, so the unchanged all-children cleanup
assertion failed although rendering and the map audit passed. A diagnostic
repeat identified that exact child; neither failed run is accepted evidence.
The corrected launcher matches the checked-in driver's inherited-console
subprocess invocation. No child-name exclusion, teardown sleep or resource-bound
relaxation was added; failure-only PID/name diagnostics remain in the test.

Public strict entry point after the predecessor refresh:

```powershell
python tools/build_webkit_crt_port.py --sdk-root out/windows-host-ninja-debug/dist/05-ui --deps-prefix out/windows-host-ninja-debug/port-tests/install --work-root C:/crtw/b3w --reuse-tree --webkit --run-gpu-tests -j 8
# Add --gpu-window-frames 5 for the bounded native presentation replay.
```

The generated `out/web_windows_tranche9a_replay.py` runs the already-built binary
sequentially, using the checked-in strict evidence/map predicates and a static
audit of the same browser artifacts. Its six measurements have no concurrent
build. Evidence: `out/web-windows-tranche9a-replay-final/summary.json`, each
`{offscreen,window}-{1,2,3}/{result.json,stdout.log,stderr.log}` and
`cpu/{stdout,stderr}.log`; aggregate output is
`out/web-windows-tranche9a-replay-final.log`. The failed generated-launcher
checkpoints remain in `out/web-windows-tranche9a-replay/` and
`out/web-windows-tranche9a-replay-diagnostic/`. The initial public strict
offscreen run passes independently (`out/web-windows-tranche9a-driver-offscreen-initial.log`).
The final public strict CLI with `--gpu-window-frames 5` also passes, including
5/5 presentations and the native browser map audit:
`out/web-windows-tranche9a-driver-live-final.log` and
`C:/crtw/b3w/acceptance-gpu.json`. This closes Windows 9A replay; macOS was still
pending at that checkpoint (its replay is below), as were GPU-resource/GPUProcess gates.
Wire/negative-control logs are `out/web-windows-tranche9a-wire-negative.log`;
GPU-OFF configuration/target logs have the `web-windows-tranche9a-gpu-off-` prefix.

#### 9A macOS/arm64 replay (2026-10-16)

Replayed `3b09d45` plus acceptance-only native observer and runtime-build
environment fixes. A fresh `macos-host-ninja-debug` configuration retains the
predecessor's imported-libc++/Skia/Metal/LVGL/FFmpeg/Curl options and local pinned
source paths. The combined `-j 8` SDK and GPU/CPU-consumer build succeeds with
the Windows publisher-ordering correction. The composed browser tree is
`/private/tmp/webB3f`; host/Web/Network and B7 are rebuilt against `05-ui`.
No audio-output or physical-camera test is requested by this GPU replay.

**Metal/Ganesh result:** offscreen **3/3**, native window **3/3**, every live
run presenting **5/5** frames. Each passes the unchanged GPU pixel/readback,
UI ordering/ancestor clipping/opacity, pointer-driven pixel change, fresh
viewport/target resize, retained-source immutability, twenty alternating
updates, producer termination/replacement and browser cleanup assertions.

| Mode | Run | Twenty-update RSS delta | Thread delta | Descriptor delta | Live presentations |
| --- | --- | --- | --- | --- | --- |
| Offscreen | 1 | +208 KiB | +0 | +0 | Not requested |
| Offscreen | 2 | +160 KiB | +0 | +0 | Not requested |
| Offscreen | 3 | +160 KiB | +0 | +0 | Not requested |
| Metal window | 1 | +240 KiB | +0 | +0 | 5/5 |
| Metal window | 2 | +144 KiB | +0 | +0 | 5/5 |
| Metal window | 3 | +144 KiB | +0 | +0 | 5/5 |

The unchanged bound is +24 MiB/+2 threads/+2 descriptors after warm draw.
These are application-process observations, not Metal device-memory, native
GPU-fence or GPUProcess lifetime measurements. The source remains explicitly
non-texture-backed CPU BGRA shared memory (`source=cpu-shm interop=cpu-upload`);
only the application-owned Ganesh target and presentation are GPU-backed.

The acceptance checkpoint atomically publishes every browser PID while the
view is alive. An external observer validates Host/Web/Network identities and
parentage with native `ps`, saves their raw `vmmap` reports and releases the
checkpoint before producer-loss testing. Empty/missing roles or a failed map
observation cannot pass. vmmap's masked `Path:` and `Analysis Tool:` headers
are not mapped images: only real address/protection region rows are parsed,
including images under `/opt`, `/Library` and `/Volumes`. Current non-executable,
immediately unlinked `crt_memfd_<own-pid>_<six-digits>.tmp` files under the actual
temporary directory are reported as `ANON_SHM`. Named/executable/foreign-PID
files are not exempt, nor are arbitrary temporary libraries.

All six repetitions observe **4,634** browser file/role records with zero
violations and both CRT EGL/GLES link stubs. Each also records three anonymous
shared-data files separately. Apple's shared-cache/system region inventory
is recorded separately: it can list Metal and system C++ images without proving
the browser uses those APIs. The unchanged static Mach-O audit still rejects
direct Metal or host-libc++ imports by browser artifacts; declared media
framework imports belong to `libcrtmedia` alone. Negative unit cases cover
these distinctions. Application-side native Metal clients are deliberately
outside the CPU browser firewall.

The final public strict CLI independently passes **5/5** presentations, +208
KiB/+0/+0 over twenty updates, 4,635 browser map observations, and clean static
and runtime audits. Device runs clear `DYLD_LIBRARY_PATH` and use their linked
RPATHs. The header leak scan and exported-symbol check pass (37 `crtweb_*`
exports, no foreign names). A final native process-name inventory finds no
remaining test/browser process.

CPU WebView replay passes three process recoveries and twenty
launch/load/resize/shutdown lifetimes with **+0 KiB/+0 threads/+0 descriptors**,
settling 0ms. B7 passes **100** lifetimes, after ten warmups **+32 KiB/+0/+0**,
with at most one Web and one Network child alive together. Its static audit
lists eighteen browser artifacts; runtime UI/Web/Network observation has
1,668 file/role records and zero violations. Wire layout/transport and CPU
surface CTest passes **3/3** after final SDK publication.

Two failed checkpoints are preserved, not counted as acceptance:

- The first public entry fails SDK verification **before GPU execution**.
  The consumer driver's `CRT_CXX_RUNTIME_LINKAGE=shared` reaches a nested
  libc++ build and injects an absolute `dist/01-c/lib` RPATH. The builder now
  clears consumer linkage/hosted/port-RPATH hints and owns its compiler/link
  policy. Its fingerprint invalidates old build trees once, since Ninja does
  not track environment-only link changes. Fetched sources/pins and runtime
  ABI are unchanged; no packaging check was bypassed or weakened. The final
  cumulative `01-c` through `05-ui` SDK refresh and verification pass.
- After that fix, GPU pixels/recovery/cleanup and static Host ABI pass, but
  the initial observer misclassifies report metadata and CRT memfd data.
  Region/protection parsing and the narrow anonymous-data classification
  above resolve it; foreign libraries still fail the audit.

GPU evidence/observer unit tests pass **17/17**, backend boundary 7/7, existing
device evidence 18/18, libc++ predecessor/environment isolation 6/6 and local
fetch recovery 1/1, driver/JSC helpers 2/2 and 9/9, source closure 7/7 and
licensing 15/15. The manifest remains 23 patches/63 modified/24 CRT-owned files.
A separate fresh GPU-OFF configure registers CPU surface/WebView targets but
no GPU consumer. Ordinary no-host GPU CTest is explicitly skipped; direct
no-host execution returns 77, strict no-host returns 1, invalid frame counts
0/301 and unknown arguments return 2. These negative controls are not GPU
acceptance. No deadline, child-name exclusion or resource bound was relaxed.

Public reproduction after configuring the same predecessor features:

```sh
python3.14 tools/build_webkit_crt_port.py \
  --sdk-root out/macos-host-ninja-debug/dist/05-ui \
  --deps-prefix out/macos-host-ninja-debug/port-tests/install \
  --work-root /private/tmp/webB3f --reuse-tree --webkit \
  --run-gpu-tests --gpu-window-frames 5 -j 8
# Omit --gpu-window-frames for strict offscreen-only replay.
```

Evidence root: `out/macos-host-ninja-debug/tranche9a-replay-20261016/`.
Configuration/SDK/browser logs are `configure.log`,
`sdk-consumer-build-initial.log`, `browser-build.log`, `sdk-build-final.log`
and `sdk-verify-final.log`. `replay.py` repeats the already-built consumer
sequentially with the checked-in strict evidence/map predicates and static
audit, without a concurrent build. `summary.json`, `replay.log` and each
`{offscreen,window}-{1,2,3}/` retain result JSON, stdout/stderr, PID checkpoint
and native vmmap logs. Final public CLI evidence is `driver-live-final.log`
and `acceptance-gpu-final.json` (also `/private/tmp/webB3f/acceptance-gpu.json`).
The retained failures are `driver-offscreen-initial.log` /
`acceptance-gpu-initial-build-failed.json` and
`driver-offscreen-environment-fixed.log` / `acceptance-gpu-map-failed.json` /
`map-checkpoint-failed/`. CPU/B7 logs and JSON are in `regressions/`; boundary,
wire, skip, negative-control and GPU-OFF evidence have the matching names
under the evidence root.

This closes macOS and the three-host **9A** baseline, not 9B/9C or the packaged
`06-web` stage. Linux/Windows native execution of the host-neutral runtime-build
environment fix was not performed on this Mac; their existing 9A baselines
remain the host evidence above, not new results claimed by this replay.

#### 9B Linux shared-resource implementation and replay (2026-10-16)

Linux/x86_64 now has a real native-resource boundary, separate from 9A's CPU
upload in the consumer. The [resource policy](../design/libcrtweb_api_policy.md#gpu-buffers-and-synchronization-tranche-9b)
owns the v2 protocol, five-handle ownership, native fence/semaphore epochs and
explicit fallback. `crtweb_gpu_worker` is a CRT-owned GPU producer with its own
static CRT/SDK Skia, not a WebKit GPUProcess or a library loaded into the
shared-runtime browser. Its ordinary WebView owns separate Host/Web/Network
processes. **Browser CPU -> worker upload -> shared GPU memory -> application**
is the measured path. Only worker -> app is zero-copy/shared GPU memory; no
claim of GPU WebCore, end-to-end zero-copy, or a shared private Skia ABI.
The resource companion currently exposes evaluation/resize, not the full
WebView control/event API; the existing v1 CPU WebView remains available.

Hardware: Intel UHD Graphics 630 (CFL GT2), Mesa 26.0.8, Vulkan 1.4.335.
External-memory, semaphore and fence OPAQUE_FD import/export capabilities are
queried independently of the VA-API dma-buf/modifier extension set. Both device
and driver UUID must match. Optimal tiling remains opaque; allocation extent,
size and memory type are checked, never guessed from a CPU stride.

**Accepted Linux results:** offscreen **3/3**, native Wayland/Vulkan window
**3/3**, each window **5/5** presentations. Every run uses actual browser
frames, checks GPU-backed source/target, exact green/magenta composition,
ancestor clipping, half opacity, UI below/above, stalled-consumer immutability,
viewport/resource generation resize and twenty fresh alternating updates.
Each run imports six resources over two generations, releases twenty-three
epochs, retires all three old-generation images, and settles at three live
images (peak four, hard protocol limit six). Worker counters are twenty-four
uploads, twenty-three completed releases/reuses, **zero GPU readbacks**;
consumer CPU uploads are zero. Pixel readback is acceptance-only.

Across twenty post-resize updates, UI RSS delta is **+0 KiB**, worker **+20 KiB**;
both thread and descriptor deltas are **0** in all six runs. Limits remain
+24 MiB / +2 threads / +2 descriptors per process. Final shutdown checks the
owned worker and every observed Host/Web/Network descendant are stopped and
reaped. Static Host ABI audits and all three browser-role mapping audits pass
with no violations; only the existing CRT-owned no-context EGL/GLES stubs are
mapped, not native GPU clients in the browser.

The independently built PAL test passes twenty epochs between two logical
Vulkan devices, exact GPU pixel identity, unsignaled ready/release timeouts,
no overwrite before release, no reuse of an already consumed ready signal,
bad device/driver UUID and allocation-shape rejection, and borrowed-FD
ownership. The plain-C wire test checks 72/256-byte layouts, five-FD/CLOEXEC
transfer and complete FD cleanup on malformed version/backend/format/extent,
surplus/missing/truncated descriptors, short and zero-length packets. Native
timeouts grant no ownership and do not enqueue an unsignaled GPU wait.

Negative controls: missing prerequisites exit **77**, strict missing
prerequisites **1**, invalid live count **2**. A child-scoped llvmpipe ICD
selection genuinely lacks the native fence/semaphore sharing capability:
ordinary mode exits **77**, strict mode **1**, and explicit fallback exits
**0** with CPU mode and **zero imported resources**. Fallback never satisfies
the strict resource parser. A missing browser executable fails strictly and
is cleaned up. No parent GPU environment was changed.

The rebuilt CPU WebView passes its full control/input/recovery and twenty-cycle
lifecycle test. The rebuilt 9A consumer passes strict GPU composition and a
5/5 native window run; v1 wire layout/transport and the hermetic SurfaceView
also pass. GPU-OFF fresh configure and Skia-object build succeed: the three
shared-image factories are unsupported stubs, the native adapter has no GPU
imports, the CPU WebView target remains and GPU worker/resource targets are
omitted. This is not a full GPU-OFF product build/run claim. Fresh GPU-ON build
regenerates and verifies all five cumulative SDKs (`01-c`..`05-ui`), with the
shared-image sources included in the isolated `04-gfx-media` source asset.

The strict resource driver is `run_gpu_resource_gate()` in
`tools/build_webkit_crt_port.py`; its independent entry options are
`--webkit --run-gpu-resource-tests [--gpu-resource-window-frames 5]`, usable
without media/camera gates. It rejects missing native evidence, CPU fallback,
incorrect frame counts and browser Host ABI/mapping violations. The checked-in
resource-evidence tests (4), existing 9A evidence tests (17), driver tests (2),
GPU boundary tests (7), stage closure tests (8), licensing tests (15) and patch
manifest (23 patches / 63 modified / 24 CRT-owned files) pass. No WebKit pin,
patch or public C API changed.

Evidence under `out/web-crt-webcore/`: `tranche9b-replay/summary.json` and each
`{offscreen,window}-{1,2,3}/{stdout,stderr}.log` / `result.json`;
`tranche9b-replay/negative/`; `acceptance-gpu-resources.json` and
`tranche9b-resource-gate-final.log` (the checked-in strict function invoked
directly); `tranche9b-native-resource-final.log`, `tranche9b-wire-final.log`,
`tranche9b-focused-ctest.log`, `tranche9b-vulkan-capabilities.log`,
`tranche9b-regression-crtweb_webview{,_gpu}_test/`, `tranche9b-gpu-off.log`,
`tranche9b-configure-final.log`, `tranche9b-build-final.log` and
`tranche9b-verify-sdk-final.log`. The generated `tranche9b_replay.py` calls the
checked-in strict evidence/map/Host ABI checks; it does not replace them.

The initial extensionless local HTML fixture returned `Frame load interrupted`
and never produced the expected green pixels. The final test creates an owned
private `.html` fixture, then still waits for fresh GPU pixels rather than a
title/load notification. Failures are preserved in
`tranche9b-resource-{initial,diagnostic}.log`; the first fixed checkpoint is
`tranche9b-resource-fixture-fixed.log`. Browser binaries are the unchanged
accepted 9A implementation, exercised against the freshly published SDK.
Optional broad WebKit recompilation triggered by refreshed SDK timestamps was
interrupted (`tranche9b-browser-rebuild{,-final}.log`); it is **not** counted as
a fresh WebKit build or a completed public-CLI run. All new worker, adapter and
consumer code is rebuilt in the final in-tree build.

This accepts the **Linux 9B resource slice**. Native D3D12/Metal shared resources
and replay remain open. Device/process loss, consumer disconnect/recovery,
repeated full resource lifetimes, and the deferred upstream GPUProcess
lifecycle gate remain 9C work; normal queue draining is not a bounded
hung-driver recovery claim. Tranche 9 and `06-web` packaging are not closed.

#### 9B Windows/x64 D3D12 shared-resource implementation and replay (2026-10-16)

Windows had no native resource backend when the Linux slice landed (`skia_shared.cc` returned unsupported and
the strict gate refused the host), so this is an implementation plus replay, not a replay of Linux code. The
[resource policy](../design/libcrtweb_api_policy.md#gpu-buffers-and-synchronization-tranche-9b) owns the
protocol; the D3D12 form is `src/arch/windows/skia_shared_d3d12.cc` behind the unchanged
`crtgfx_skia_shared_image` interface and the same v2 wire. The browser is the unchanged accepted CPU
implementation; **browser CPU -> worker upload -> shared GPU texture -> application** is the measured path and only
worker -> application is shared GPU memory. No claim of GPU WebCore, end-to-end zero-copy, a WebKit GPUProcess or a
shared private Skia ABI.

Hardware: Intel UHD Graphics 630 (vendor 0x8086, device 0x3e92), WDDM user-mode driver 31.0.101.2140. The
shared pair is one adapter: identity is the LUID + vendor + device id and the user-mode driver version + subsystem +
revision (`ca9e00000000000086800000923e0000` / `5c08650000001f008680122200000000`); a second logical device on the same adapter is the
consumer in the PAL test, and a different adapter negotiates CPU mode.

**Accepted Windows results:** offscreen **3/3**, native D3D12 window **3/3**, each window **5/5** presentations,
every run through the checked-in strict driver (`--webkit --run-gpu-resource-tests
[--gpu-resource-window-frames 5]`). Each run uses actual browser frames, checks GPU-backed source and target,
exact green/magenta composition, ancestor clipping, half opacity, UI below/above, stalled-consumer immutability,
generation/extent resize and twenty fresh alternating updates. Every run imports six resources over two
generations (18 shared objects: texture + two fences each), releases twenty-three epochs, retires all three
old-generation images and settles at three live images (peak four, hard protocol limit six). Worker counters are
24 uploads, 23 completed releases/reuses, **zero GPU readbacks**; consumer CPU uploads are zero. Across the twenty
post-resize updates the UI process changes by **+4 KiB RSS, +0 threads, -1 handle** in all six runs, the worker by
**+116..+204 KiB RSS, +0 threads, +0 handles**; the limits remain +24 MiB / +2 threads / +2 descriptors per
process. Final shutdown checks that the worker and every observed Host/Web/Network descendant are stopped and
reaped (none left behind). The static PE Host ABI audit of `crtweb_host.exe` and `libcrtweb.dll` and the
Toolhelp module audit of all three browser roles pass with no violations; the browser processes map no D3D12,
Vulkan, OpenGL or d3dcompiler module, only the CRT-owned no-context EGL/GLES stubs.

The independently built PAL test (`crtgfx_skia_shared_image_test`) passes twenty epochs between two logical D3D12
devices in one process: exact GPU pixel identity, unsignaled ready/release fences that time out, no overwrite
before release, a consumed ready epoch that cannot authorize a second read, bad device/driver identity and
allocation-shape rejection, and borrowed-descriptor ownership (the import duplicates; the caller still closes).
`windows_fd_adopt_handle_test` covers the PAL hook that turns an NT handle into a descriptor (in-process duplicate,
a spawned child signalling through its copy, `close()` closing the kernel handle). The plain-C wire test and its
5-descriptor layout checks are unchanged and pass; BUFFER_ADD now requires exactly
`crtgfx_shared_image_handle_count(descriptor)` descriptors (five for Vulkan, three for D3D12).

Negative controls: missing prerequisites exit **77**, strict missing prerequisites **1**, an invalid frame count
**2**. A genuinely different adapter (the application on device index 1, the software Microsoft Basic Render
Driver, vendor 0x1414) makes the worker answer `mode=cpu-fallback resources=0`; ordinary fallback mode exits **0**
with CPU mode and **zero imported resources**, and the strict run on the same pair exits **1** with "negotiate
shared GPU channel" failed: fallback never satisfies the strict resource parser. A missing browser host fails
strictly and everything is cleaned up. No parent GPU environment was changed.

Regressions on the same build: the 9A gate (`--run-gpu-tests --gpu-window-frames 5`) passes with CPU-upload
D3D12 composition, `backend=1`, 5/5 presentations and 20 updates at +120 KiB / +0 / +0; the B3-C8 and Tranche 5
gate passes (public API lifecycle, WebView lifecycle, B7 100 view lifetimes at +0 handles); `ctest` passes 208/208
(3 skipped by design: they need `CRTWEB_HOST`/worker or a capture device); resource-evidence unit tests (4),
GPU boundary tests (7), stage-source closure tests (8) and the patch manifest pass. A fresh GPU-OFF configuration
(`out/web-windows-tranche9b-gpu-off`, `CRTGFX_ENABLE_GPU_BACKEND=OFF`) registers no worker/resource/PAL-test targets,
compiles `skia_shared.cc` to the unsupported stubs and `skia_shared_d3d12.cc` to an empty object; this is not a
GPU-OFF product build/run claim.

**Found and fixed by the replay.**
(1) D3D12 shares textures and fences as NT handles, which the CRT could not send: `__crt_windows_fd_adopt_handle()`
(a PAL hook, see `bionic_libc_gaps.md`). (2) The descriptor and wire were five-descriptor Vulkan only; they now carry
a backend-specific count and a D3D12/NT-handle shape, and the worker and PAL test follow it. (3) The first adapter
identity was all zeros: the Skia-bridge translation unit's 4-byte `wchar_t` gives the real `DXGI_ADAPTER_DESC1` a
512-byte `Description`, so `GetDesc1()` filled other offsets; a zero identity matched every adapter and the mismatch
tests passed vacuously. The identity is read in `gpu_win32.c` (`crtgfx_gpu_win32_adapter_identity()`), and the
driver version comes from `CheckInterfaceSupport` on `IDXGIDevice`, `ID3D11Device` or `ID3D10Device`, whichever the
driver answers. (4) `GrD3DTextureResourceInfo`'s pointer constructor adopts without `AddRef`, so a texture wrapper
released the shared resource when it went out of scope. (5) `crtweb::WebView::launch()` fell back to `/tmp` (absent
on Windows) when `TMPDIR` was unset; it now tries `TEMP` and `TMP` first. (6) The Skia object libraries compile
against `dist/02-cxx` while the dist publisher can rewrite it ("cstddef/cctype not found" when a consumer and its
Skia objects were built together); they now wait for `crt-libcxx-dist`.

*Not done / open at the Windows checkpoint:* Linux and macOS had not re-run the shared edits (descriptor handle-count helper, wire
validation, worker/test changes, `WebView::launch()` temp-directory fallback, CMake ordering, evidence parser); macOS Metal shared resources, device/process loss, consumer disconnect and recovery, repeated
full resource lifetimes and the deferred upstream GPUProcess lifecycle gate (9C); `D3D12_FENCE` timeouts are a
two-second bound, not a hung-driver recovery claim. A handle is duplicated into the peer when it is sent, so a
worker that exits after BUFFER_ADD does not invalidate resources the application already imported. Tranche 9 and
`06-web` packaging are not closed.

Evidence: `C:/crtw/b3w/acceptance-gpu-resources.json` per run (kept as `rep9b-{offscreen,window}-{1,2,3}.json`),
the driver logs `rep9b-*.log`, `reg9b-9a.log` and `reg9b-full.log`, and the GPU-OFF configure log
`gpuoff-configure.log`, all under `C:/crtw/`.

#### 9B macOS/arm64 availability replay (2026-10-16)

The initial replay at `436e7d9` confirmed that **macOS 9B was not implemented or accepted**.
This was not a Metal device/permission failure or a regression in the accepted
9A renderer. Only the Linux/Vulkan and Windows/D3D12 native resource adapters
existed. On this host `src/skia_shared.cc` compiled the unsupported factories,
even with `CRTGFX_HAVE_METAL=1`; enabling the ordinary GPU backend did not
enable native resource sharing at that checkpoint.

Four missing pieces preceded a successful Metal replay:

1. Native backing export/import and independent producer/consumer ownership,
   with real GPU ready/release completion epochs and bounded waits; neither
   CPU ACKs nor upload/readback substitutes meet 9B.
2. A Metal-specific handle kind/count and allocation/device validation in the
   fixed-width resource descriptor and v2 transport. Only Vulkan
   opaque-FD and D3D12 NT-handle shapes were valid. Native references must have
   a real cross-process ownership/cleanup path, not raw process-local names.
3. A native browser-role checkpoint/observer in `gpu_resource_test.cc`. Its
   macOS branch returned false; the existing 9A observer was not
   automatically attached to the separate worker/consumer test.
4. A macOS strict resource driver/evidence path. `run_gpu_resource_gate()`
   explicitly refused this host and `gpu_resource_evidence()` had no Metal
   backend/synchronization label. These safeguards were not relaxed.

**Actual native results:** the newly built PAL probe returns **77** ordinarily
and **1** with `--require-gpu`. With real host/worker executable paths supplied,
the resource consumer also returns **77** ordinarily and **1** in strict
offscreen and strict `--window-frames 5` modes, before creating shared images.
The strict driver returns `passed: false` with reason
`9B native resource backend/replay pending on this host`. The opt-in
`--probe-fallback` path runs the worker and exits **0**, reporting CPU fallback
and **zero resources**; the resource-evidence parser rejects this as GPU
acceptance. An invalid window count returns **2**. The ordinary PAL/resource
CTest entries are **two skips**, not two accepted resource tests.

**Shared-change and 9A regressions:** freshly built v1 layout, v1 transport,
v2 resource wire and hermetic CPU SurfaceView CTests pass **4/4** on the real
Darwin socket/descriptor adapter. The 9A Metal consumer passes offscreen
**3/3** and native-window **3/3**, with **5/5** presentations per window run.
All six runs pass GPU composition, clip/opacity/order, input, resize, twenty
updates and producer replacement/cleanup; each records **+176 KiB RSS,
+0 threads, +0 descriptors**. Every run's strict native browser mapping
observer verifies all three roles, **4,634 file/role observations**, both
CRT-owned GL stubs and no Host ABI violations. This is the existing CPU-shm
upload final compositor, **not** a shared GPU resource pass.

The rebuilt CPU WebView passes three producer recoveries and twenty complete
lifetimes at **+0 KiB / +0 threads / +0 descriptors**. B7 is **not an entirely
green regression batch**: its first run exits **1** after 82 completed
lifetimes (`FAILED the page loaded`, `FAILED every iteration completed`).
The matching macOS crash report identifies a pre-exec spawn child (PID 75675,
not the B7 UI process PID 75102) with `SIGSEGV` in
`unindex_free_block -> malloc_unlocked -> malloc ->
__crt_seqpacket_export_env -> __crt_sys_posix_spawn`. This is actionable
runtime evidence, not merely a slow page or an acceptance-observer timeout.
The same unmodified binaries/SDK and native observer complete a second B7 run
**100/100**, **+32 KiB RSS / +1 thread / +0 descriptors**, at most two children.
Both runs pass static Host ABI (18 listed artifacts) and native UI/Web/Network
mapping audits (1,668 and 1,667 file/role observations, no violations).
The first result remains failed in `regressions.json`; the successful repeat
is separate, not substituted for it. No root cause or fix is claimed. The
spawn child's inherited allocator state and its post-fork allocations need a
focused follow-up; these paths predate the 9B commits, and this replay does
not establish that 9B introduced the failure.

The requested in-tree worker/consumer and Skia objects were rebuilt after the
pull; cumulative SDKs `01-c` through `05-ui` were republished and each passes
`verify_dist.py`. The browser executables under `/private/tmp/webB3f/build`
reuse the previously accepted 9A product build; this pass is not a fresh
WebKit rebuild or a completed public resource-CLI run. Resource evidence (4),
9A evidence (17), GPU backend boundary (7), stage-source closure (8), driver
(2) and licensing (15) unit suites pass **53 tests**; the unchanged patch
manifest remeasures **23 patches / 63 modified / 24 CRT-owned files** against
the pristine pinned product source. No public ABI, WebKit patch/pin or
acceptance limit was changed. No physical camera or audio-output test ran.

Evidence: `out/macos-host-ninja-debug/tranche9b-replay-20261016/` contains
`configure.log`, `build.log`, `probes.py` / `probes.json` / `probes.log`, the
individual `pal-*.log` / `resource-*.log` outputs, `resource-ctest.log`,
`wire-ctest.log`, `sdk-verify.log`, `*-unit.log` and `patch-manifest.log`.
`regressions.py` / `regressions.log` and each
`9a-{offscreen,window}-{1,2,3}/result.json`, raw `vmmap-*` and stdout/stderr
retain the native 9A evidence. `regressions.json` / `cpu-b7/` retain the initial
CPU/B7 results; `b7-spawn-child-crash.ips` preserves the OS crash report;
`b7-repeat.log` / `b7-repeat/result.json` retain the independent repeat.
These generated files are local ignored
artifacts, not committed fixtures. The strict native reproduction after
building `crtgfx_skia_shared_image_test` is:

```sh
out/macos-host-ninja-debug/libcrtgfx/crtgfx_skia_shared_image_test --require-gpu
# Expected at that checkpoint: exit 1, shared memory/semaphore/fence unavailable.
```

At that checkpoint, the three-host 9A baseline remained accepted, while the
macOS Metal leaf of 9B and 9C remained open. The subsequent implementation and
native resource evidence follow below. Linux was not re-executed on this Mac;
no global Tranche 9 or `06-web` packaging completion was claimed.

#### 9B macOS/arm64 Metal shared-resource implementation (2026-10-16)

Implemented behind the existing `crtgfx_skia_shared_image` interface, without
changing WebKit source, upstream pins, the 72-byte image descriptor, the
256-byte v2 packet, the frozen v1 CPU transport or public `crtweb.h`.
The adapter owns IOSurface BGRA8 textures and independent monotonic ready /
release MTLSharedEvents. Ganesh submits and native signal commands use the
same queue. Native CPU event waits are bounded before GPU waits are enqueued;
no CPU ACK, callback or socket message grants GPU ownership. Device identity
uses the Metal registry ID; driver identity hashes the OS driver generation.

Two CRT descriptors (repeatable capability metadata plus a lease) redeem three
Mach send rights through a random ephemeral bootstrap name, without installing
a launchd service. Native event handles use Metal's public coding hook and
`NSXPCCoder`, not guessed private fields. Export leases keep native references
alive after producer-wrapper destruction; imported IOSurface/event references
survive closure of all transfer descriptors. **Exporter process liveness is
required until import completes**; pre-import death and device-loss recovery
remain 9C. Bootstrap access or native-event absence returns unsupported.
The precise ownership contract is in
[Web API policy](../design/libcrtweb_api_policy.md#gpu-buffers-and-synchronization-tranche-9b).

The private native Objective-C++ TU uses the Apple SDK. A separate C99 TU
uses CRT headers for descriptor/pthread ABI and Bionic constants. First shared
PAL execution exposed `socketpair` / `pthread_create` binding to libSystem
because frameworks came before CRT libc; the shared target now puts CRT libc
first, including isolated SDK-stage links. IOSurface is an explicit graphics
prerequisite, not an expanded browser Host ABI allowance. The first SDK
publication's undeclared-IOSurface failure is retained.

The SDK loop also exposed two build-policy problems: Apple's install-time
archive index timestamp changed full predecessor hashes, and libc++ configure
could read `01-c` while its packaged publisher replaced it. Both lightweight
and packaged installs now normalize Darwin indexes with selected `ranlib -D`;
configure waits for the packaged C publisher where that target exists. Full
predecessor byte hashing remains intact. Reinstalling all six C runtime
static/shared inputs twice preserves their full hashes; the Darwin unit
fixture forces an index rewrite and separately compares object payload bytes.

Evidence is retained under
`out/macos-host-ninja-debug/tranche9b-metal-20261016/`: native probes, failed
build/shared-link checkpoints, PAL/wire runs, `replay.py` invoking the checked-in
strict gate, per-run `result.json`, raw stdout/stderr and external native
`vmmap-*`/PID checkpoints. This is a working-tree implementation on `436e7d9`,
using the unchanged accepted browser product in `/private/tmp/webB3f/build`;
it is not a fresh full WebKit rebuild. `DYLD_LIBRARY_PATH` is removed for
native GPU runs. The observer validates application -> GPU worker -> host ->
Web/Network parentage before sampling each browser's real maps; missing or
foreign executable mappings fail closed.

**Accepted native replay:** Apple M1 Pro, macOS/arm64, offscreen **3/3** and
real Metal window **3/3**, every window **5/5** frames. Evidence was captured
on October 16; completion notes were reconciled on October 17. Each strict
run verifies twenty alternating browser updates, GPU-backed source/target,
pixel identity, clipping/opacity/ordering, a stalled consumer, resize retirement
and descendant teardown. Each imports six resources over two generations,
releases twenty-three epochs and ends at three live images (peak four, limit
six). Worker counters are **24 uploads / 23 releases/reuses / 0 GPU readbacks**;
consumer CPU uploads are **0**. Pixel readback is acceptance-only.

| Mode | Run | UI RSS | UI threads / FDs | Worker RSS | Worker threads / FDs | Frames |
| --- | --- | --- | --- | --- | --- | --- |
| Offscreen | 1 | +208 KiB | +0 / +0 | +256 KiB | -3 / -3 | n/a |
| Offscreen | 2 | +160 KiB | +0 / +0 | +240 KiB | -3 / -3 | n/a |
| Offscreen | 3 | +144 KiB | +0 / +0 | +96 KiB | -3 / -3 | n/a |
| Metal window | 1 | +160 KiB | +0 / +0 | +32 KiB | -3 / -3 | 5/5 |
| Metal window | 2 | +160 KiB | +0 / +0 | +96 KiB | -3 / -3 | 5/5 |
| Metal window | 3 | +80 KiB | +0 / +0 | +64 KiB | -3 / -3 | 5/5 |

These are process deltas across the twenty post-resize updates, not GPU-memory
measurements. Bounds remain +24 MiB / +2 threads / +2 descriptors per process;
the worker's negative deltas include completed capability-lease teardown.
Every run passes static Host ABI and external native browser-map observation
with zero violations: Host/Web/Network have 1545/1544/1544 mapped-file paths
(4633 total), including OS shared-cache mappings, not that many direct dylib
imports. Native parentage is checked before observation; the owned worker and
observed browser descendants are stopped and reaped afterward.

The independently linked PAL passes in **both static and shared CRT/graphics
forms**: twenty native epochs, exact pixels, unsignaled and consumed-event
timeouts, no overwrite before release, bad device/driver/allocation rejection,
borrowed descriptor ownership, import after producer-wrapper destruction,
continued use after transfer-FD closure, and all capability offers settling
to zero. The shared consumer links the CRT-built pinned Skia/FreeType archives
for normal Skia public API use alongside the graphics DSOs; this is not a
standalone upstream Skia shared-build claim. `dyld_info -fixups` confirms the
bridge's `fcntl/free/fstat/malloc/memfd_create/poll/pthread_create/socketpair/write`
bind to **CRT libc**, not libSystem. Generated in-tree and isolated-stage
link-command tests guard the ordering.

Negative controls pass: missing prerequisite ordinary/strict exit **77/1**,
invalid frame count **2**, missing browser **1**, and a deliberately mismatched
declared device identity negotiates **CPU / zero resources** (ordinary **0**,
strict **1**). The strict evidence parser rejects every case. The identity
control uses the real native worker but does not claim a second physical GPU
or an actual unsupported Metal device on this single-GPU machine.

Wire/CPU CTests pass **4/4**, including exact Metal-two / D3D12-three / Vulkan-five
descriptor shapes, CLOEXEC and malformed-transfer cleanup. An interim sandboxed
listen test failed with `EPERM`; the native unrestricted repeat passes, and
both logs are retained. All **99** focused unit tests, the unchanged WebKit
patch manifest (**23 / 63 / 24**), final `01-c`..`05-ui` SDK regeneration and
verification pass. Fresh GPU-ON/GPU-OFF configurations and native/stub object
compilation pass; this is not a full fresh GPU-OFF or isolated product build.
The earlier availability replay's CPU WebView/B7/9A results remain separately
dated evidence, not additional full regressions silently counted here.

`summary.json` and `offscreen-{1,2,3}/` / `window-{1,2,3}/` retain the six strict
runs. `final-checks.log`, `final-pal-{static,shared}.log`,
`shared-{backend,consumer}-fixups.log`, `final-wire-cpu-ctest.log`,
`negative/result.json`, `final-verify-*.log`, `final-unit.log`,
`reinstall-stability.log` and fresh-configuration/object logs retain the final
checks. On October 17 the static/shared native PAL and 99 unit tests pass
again; the native process scan finds no remaining test/browser processes.
`closeout-pal-{static,shared}-20261017.log`, `closeout-unit-20261017.log` and
`closeout-processes-20261017.log` retain that confirmation.
The checked-in strict entry remains `tools/build_webkit_crt_port.py`
with `--webkit --run-gpu-resource-tests [--gpu-resource-window-frames 5]` and
the existing SDK/dependency/work arguments. The independent native PAL can
be repeated with:

```sh
cmake --build --preset macos-host-ninja-debug --target crtgfx_skia_shared_image_test
out/macos-host-ninja-debug/libcrtgfx/crtgfx_skia_shared_image_test --require-gpu
```

This closes the macOS **9B resource slice**, not accelerated WebCore, 9C,
`06-web` packaging or a fresh Linux/Windows replay of this working tree.
The earlier intermittent B7 spawn/allocator failure was a separate
follow-up, not fixed by this adapter; its native runtime fix/replay follows
below. Exporter death before import, consumer
disconnect recovery and hung/lost-driver handling remain open 9C contracts.

#### B7 native fork/spawn allocator follow-up (2026-10-17)

The October 16 failure above remains failed, including the original
`b7-spawn-child-crash.ips` and the independent unmodified successful repeat.
The crash is in a spawn child's `unindex_free_block()` while allocating the
framed-socket exec environment, not in the Metal resource adapter. Review
found native Linux/macOS fork copying allocator state without taking its
lock; native spawn also skipped the CRT runtime reset before child-side
PAL operations. A copied-lock reset alone cannot repair partially updated
free-list metadata.

The fix stabilizes the allocator with its own lock across the native kernel
snapshot. The parent unlocks on success/error, the child resets the inherited
lock, and application atfork callbacks remain outside that guard so they may
allocate. Spawn uses the runtime-only child reset and still does **not** call
application atfork handlers. macOS framed descriptors are still exported
after file actions, preserving renumbered fd inheritance. Bionic/Scudo
references, `_Fork()`'s unchanged raw contract and post-fork safety limits
are in [Process Fork Model](../design/process_fork.md). Windows's memory-copy
fork is unchanged; it allocates internally and cannot take this outer guard.

`fork_heap_snapshot_test` includes the real allocator in a private test
fixture, holds its heap lock in a worker and asks another initialized thread
to fork/spawn. Its pre-fix binary exits **1** with
`fork copied a heap critical section`. After the fix, both snapshots wait
until the holder releases; child allocation, allocating prepare/parent/child
callbacks, spawn callback suppression, and framed socket data inherited after
`dup2` actions all pass. The clean CMake build repeats **10/10** successfully;
CTest has a 30-second outer timeout and marks the separate Windows mechanism
as **skipped**, not a native-COW pass.

macOS/arm64 validation with the regenerated cumulative SDK:

- C-stage CTests **138/138** pass, including allocator diagnostics, fork,
  pthread, spawn error reporting and socket inheritance.
- SDK `01-c` through `05-ui` publication and verification pass.
- B7 **three independent 100/100** lifetime runs pass with the same native
  observer and unchanged limits. Post-warm-up RSS is **+32/+48/+32 KiB**;
  threads and descriptors are **+0** in every run, at most two children.
- Each run passes static Host ABI over **18 artifacts** and native
  UI/Web/Network mapping audits (**1,667** file/role observations per run,
  no violations). Native Mach-O fixups confirm WebKit's
  `malloc/free/realloc/calloc/posix_spawn` bind to **CRT libc**.
- The published shared libc and reused WebKit/lifecycle binaries are SHA-256
  recorded and remain unchanged during all three runs. This is not a fresh
  WebKit build, GPU/9C replay, physical-camera or audio-output validation.
- Linux and Windows x86_64/aarch64 branches pass Clang syntax checks using
  project headers only. No Linux/Windows native execution is claimed here;
  Linux replay of the new regression remains queued.

The deterministic negative control demonstrates the missing allocator
snapshot exclusion, not a deterministic recreation of the exact historical
invalid-pointer instruction. The clean B7 repeats support closing this
focused macOS follow-up, not a claim that all allocator or multithreaded
post-fork safety issues are impossible. No public ABI, WebKit patch/pin,
acceptance limit or fallback policy changed.

Local ignored evidence is under
`out/macos-host-ninja-debug/b7-spawn-allocator-20261017/`:
`before-fix` / `before-fix.log`, `fresh-{configure,build,repeat}.log`,
`core-ctest.log` / `final-core-ctest.log`, `sdk-build.log`, `syntax-check.log`,
`webkit-allocator-fixups.log`,
`replay.py` / `replay.log` / `replay.json` and `b7-{1,2,3}/result.json` /
`lifecycle.log`. The original October 16 evidence is not overwritten.
For the checked-in regression on each native Linux/macOS host:

```sh
cmake --build --preset macos-host-ninja-debug --target fork_heap_snapshot_test
ctest --preset macos-host-ninja-debug -R '^fork_heap_snapshot_test_runs$' --repeat until-fail:10
# On Linux, replace the preset with linux-host-ninja-debug.
```

For the real B7 observer, reuse the documented WebKit driver with
`--webkit --run-tests` after publishing the SDK; the local `replay.py`
records three focused `run_lifecycle_gate(..., "macos")` runs without
republishing dependencies between them. Tranche **9C** and `06-web` packaging
remain open; `STATUS.md` is intentionally unchanged.

#### Linux native fork/spawn replay (2026-10-17)

Replayed the `71f79c9` native-COW guard on Linux/x86_64 with a fresh
`linux-host-ninja-debug` configure and rebuilt C runtime/tests. The deterministic
real-allocator `fork_heap_snapshot_test` passes **10/10** (`2.01 s` per run),
including exclusion of the held heap critical section, child allocation,
allocating atfork callbacks, spawn callback suppression and socket inheritance
after `dup2` actions. The full C-stage passes **138/138**. No fix, public ABI or
Windows memory-copy fork change was needed on this host.

Evidence: `out/web-crt-webcore/tranche9c-20261017/{configure,fork-core-build,fork-repeat,core-ctest}.log`.
The subsequent fresh graphics build publishes/verifies the cumulative SDKs,
so real GPU-worker/browser spawn replays use the updated CRT runtime as well.
This closes the Linux native-fork replay item; it is not evidence of Windows
native-COW behavior or a replacement for native 9C host replay.

#### 9C owned-worker lifecycle: Linux implementation/replay (2026-10-17)

The GPU-process boundary is explicitly the separate CRT-owned worker, confirmed
2026-10-17. This resolves the earlier deferred GPUProcess terminology: it does
not enable upstream WebKit GPUProcess, accelerated WebCore, WebGL or WebGPU.
The earlier 9A/9B checkpoints retain their then-open wording; the current
[lifecycle policy](../design/libcrtweb_api_policy.md#owned-gpu-process-lifecycle-tranche-9c)
owns shutdown, recovery and context/device ordering. Browser -> worker is still
CPU upload, and only worker -> app is shared GPU memory.

`crtweb_gpu_resource_test --lifecycle --require-gpu` adds an independent strict
9C program mode, not a rename of 9B success. It warms seven policies, measures
twenty complete worker/resource lifetimes in one application process, and
proves one more fresh session after the final controlled loss (**28 sessions**
per run). Coverage includes normal exit, abrupt consumer disconnect, exact
owned-worker SIGKILL, owned Host SIGKILL, Web-process SIGKILL/reload, controlled
application context loss and native owner teardown/recreation, and resize while
the consumer retains an old image. A deterministic native-transfer fixture
prequeues three real resource announcements (**15 received Vulkan FDs**), then
proves BYE, no imports, all transfer references closed and bounded native
handle/lease-thread cleanup. It does not race a timed browser resize to guess
whether handles were queued.

Peer/worker/Host loss preserves independently imported, readable last GPU
pixels on a healthy application context. Web-process loss changes a magenta
page back to fresh green pixels through an actual replacement process. A lost
context exposes no GPU image or ACK; all targets/imports and the context are
dropped before the existing backend-owned destructive device hook is invoked.
The next device/context/worker then produces real pixels again. This is
**controlled context abandonment plus native owner teardown/recreation**, not
a physical GPU reset or proof that arbitrary hung-driver calls are interruptible.
The old allocations/epochs are never adopted by a replacement session.

Linux/x86_64 strict replay is **3/3**, each **20/20** measured lifetimes plus
warm-up/final recovery (84 worker sessions in the three runs). RSS delta is
**+20/+20/+20 KiB**, thread and fd deltas **0** throughout; bounds remain
+24 MiB / +2 threads / +2 descriptors after warm-up. Complete per-case elapsed
times are **200..951 ms** across these runs. Every observed Host/Web/Network
descendant is gone/reaped, no worker remains between sessions, static Host ABI
audits pass and each runtime-map audit records **103** file/role mappings with
no violations. Browser roles still load only CRT GL stubs, not native GPU
clients. SHA-256 records confirm the tested worker/consumer/browser/runtime
artifacts do not change between replays.

Fresh GPU-ON configure/build publishes and verifies SDKs `01-c`..`05-ui` with
the synchronized native-fork runtime. The unchanged previously accepted browser
binaries run against that SDK; no fresh full WebKit rebuild is claimed. CPU
WebView's twenty-cycle lifecycle, strict 9A/9B native-window regressions (5/5
frames each), native shared-image/wire/SurfaceView CTests **5/5**, licensing and
patch-manifest checks also pass. Missing prerequisites remain a CTest **skip**,
not 9C acceptance. The new strict parser requires every case/coverage/resource
record and rejects 9B-only output, skipped/fallback evidence, incomplete fault
coverage, excess resources/time and a falsely labelled physical reset.

The initial runtime cases passed but the browser-map audit correctly rejected
a shutdown log interleaved with an unfinished stdout map line. The final test
flushes complete map records before teardown. That failed audit remains in
`initial/combined.log`; `driver/` is the successful pre-queued-transfer-check
checkpoint, not the final strengthened fixture. Final evidence under
`out/web-crt-webcore/tranche9c-20261017/`: `build.log` / `build-final.log`,
`replay.py` / `replay.log` / `replay.json`, `replay-{1,2,3}/{stdout,stderr}.log`
and `result.json`, `driver-final.log` /
`driver-final/acceptance-gpu-lifecycle.json`, `regression-{cpu,9a,9b}/`,
`regression-audits.json`, `focused-ctest.log`, `skip-ctest.log`, `verify-sdk.log`.
The generated replay harness uses the checked-in strict parser, browser-map
and Host ABI audits rather than replacing them.

**Remaining closure at the Linux checkpoint:** Windows/x64 and macOS/arm64
must run this new 9C mode
and their relevant CPU/9A/9B/native Host ABI regressions. Their accepted 9B
resource backends and earlier 9A device/process tests do not substitute for the
new complete-lifetime/fault replay. Linux 9C is accepted; global 9C and Tranche 9
remain open until both native replays are recorded. No packaged `06-web` or
security closure is implied.

Native replay entry after publishing the host's fresh `05-ui` SDK:

```sh
python3 tools/build_webkit_crt_port.py \
  --sdk-root <host-build>/dist/05-ui --deps-prefix <web-deps-prefix> \
  --work-root <existing-webkit-work> --webkit --reuse-tree \
  --run-gpu-lifecycle-tests
```

This independent gate does not open camera/audio devices. It rebuilds the
current worker/consumer and writes `acceptance-gpu-lifecycle.json`. macOS uses
the native parent/name-checked vmmap checkpoint, with time for the fault warm-up;
Windows uses the existing native process/module observations. Repeat the native
gate three times without changing artifacts and retain all results. The CTest
entry is `crtweb_gpu_lifecycle_test_runs` (300-second outer timeout), with the
driver supplying `CRTWEB_HOST`, `CRTWEB_GPU_WORKER`, fonts and native observers.

#### 9C macOS/arm64 Metal native replay (2026-10-17)

Replayed Linux's `8154c8c` on an **Apple M1 Pro / macOS 27.0.1 (26A434)**
through the real Metal IOSurface/shared-event backend. No runtime code,
upstream source/patch/pin, public C/v1 wire ABI, fallback policy or acceptance
limit needed changing. The GPU process is the **CRT-owned worker**, not upstream
WebKit GPUProcess or accelerated WebCore. Browser -> worker remains CPU upload;
worker -> application uses native shared GPU resources.

**Build/checkpoint separation:** the in-tree worker/consumers and cumulative
SDK `01-c`..`05-ui` were rebuilt and verified first. An initial immutable
reused-browser checkpoint passes 9C 3/3, 9A/9B six runs each, CPU WebView and
B7; its evidence stays at the replay root, not relabelled as rebuilt-browser
acceptance. The official CLI then detects the refreshed SDK, reconfigures
and rebuilds **2,158 steps** of JSC, bundled Skia, WebCore/WebKit and the
Web/Network/Host executables. Its strict 9C mode passes independently
(**+752 KiB/+0 threads/+0 descriptors**). B7's executable is also refreshed
against that browser. Final acceptance below uses those rebuilt artifacts,
without republishing SDKs or rebuilding between repetitions. This is an
incremental product rebuild, not a clean full WebKit or isolated `06-web` build.

**Final strict 9C: 3/3**, each with **20/20 measured complete lifetimes**, seven
policy warm-ups and one final fresh-pixel session (**84 worker sessions** in
these three runs). Normal exit, consumer disconnect, exact owned worker/Host
SIGKILL, Web-process replacement/reload, controlled context loss/native owner
recreation, and retained-old-image resize all pass. All 28 per-run case
records pass; elapsed times are **195..3,536 ms**, including native observation
pauses, below the unchanged 30-second per-case limit. Three prequeued real
Metal resource announcements are drained without imports; BYE and native
capability/lease cleanup pass. This is not a timing guess about whether a
browser resize happened to queue transfers.

RSS after warm-up is **+816/+752/+656 KiB**, threads **+0** and descriptors
**+0**, within the unchanged +24 MiB/+2/+2 bounds. These are bounded increases,
not a zero-RSS-growth claim. Each session checks that its observed browser
descendants are gone and its worker is reaped; no old resource/epoch is adopted
into a replacement session. A healthy application retains drawable imported
pixels after producer/connection death; an abandoned context exposes neither
image nor ACK. Borrowed images/targets and context are destroyed before the
native Metal owner teardown and a fresh device/session paints real pixels.
This is **controlled context abandonment plus owner teardown/recreation**, not
physical GPU reset or interruptibility of arbitrary hung-driver calls.

Every final 9C run passes native parent/name observation and browser
Host/Web/Network mapping audit (**4,633 file/role observations**, no violations),
plus static Host ABI. The browser still uses CRT GL stubs; Apple's transitive
OS framework mappings are recorded separately, not evidence of accelerated
WebCore. Selected worker/consumer/browser/runtime artifact hashes remain
unchanged throughout the final matrix.

**Regressions on the rebuilt browser:**

- 9A **offscreen 3/3 + native Metal window 3/3**, each window **5/5** frames;
  exact pixels, composition/input/resize/updates/recovery/cleanup and native
  browser audits pass (**4,634** file/role observations per run).
- 9B **offscreen 3/3 + native Metal window 3/3**, each window **5/5** frames;
  native shared events, resource reuse/retirement/ownership, twenty updates
  and browser audits pass (**4,633** file/role observations per run).
- CPU WebView recovery and twenty complete lifetimes pass at
  **+0 KiB/+0 threads/+0 descriptors**. Refreshed B7 passes **100/100** at
  **+32 KiB/+0 threads/+0 descriptors**, at most two children; static Host ABI
  and native UI/Web/Network mapping pass (**1,668** file/role observations).
- C-stage CTests **138/138** and native shared-image PAL/wire/SurfaceView
  **5/5** pass. The existing shared PAL consumer is rerun against the freshly
  published DSOs and passes twenty native Metal epochs, exact pixels and
  independent transfer-FD/resource ownership. This is not an upstream Skia
  shared-build claim.
- All **59** focused evidence/backend/stage-closure/licensing/driver unit
  tests pass; the unchanged pristine-source manifest remeasures **23 patches /
  63 modified / 24 CRT-owned files**. A fresh GPU-enabled CMake configuration
  registers the lifecycle test; that check is configuration-only, not a fresh
  product build. Default resource/lifecycle CTests without a browser report
  **two skips**, not native acceptance. SDK `01-c`..`05-ui` verification passes
  again after the official driver finishes publication.

Local ignored evidence:
`out/macos-host-ninja-debug/tranche9c-macos-20261017/` contains `build.log`,
initial `replay.json` / `regressions.json`, `driver.log` /
`driver-acceptance-gpu-lifecycle.json`, `b7-refresh.log`, unit/manifest/fresh
configuration/CTest logs and OS/model metadata. **`final/replay.json` and
`final/regressions.json` are the accepted rebuilt-browser checkpoint**, with
`final/9{a,b,c}-*/result.json`, stdout/stderr and raw `vmmap-*` logs. The
`replay.py` / `regressions.py` harnesses use checked-in strict parsers,
native observers and Host ABI audits, not replacement pass criteria.
`final-artifacts-before.json` and `closeout.json` record fourteen artifact
hashes and final native process cleanup; all hashes stay unchanged and no
test/browser/GPU-worker process remains. No physical camera/audio test ran.

The official entry exercised here (an existing configured product tree is
required) is:

```sh
python3 tools/build_webkit_crt_port.py \
  --sdk-root out/macos-host-ninja-debug/dist/05-ui \
  --deps-prefix out/macos-host-ninja-debug/port-tests/install \
  --work-root /private/tmp/webB3f --webkit --reuse-tree \
  --target crtweb_host --run-gpu-lifecycle-tests -j 4
```

This closes the **macOS 9C slice** and replays its 9A/9B baselines.
At that checkpoint Windows/x64 and global 9C/Tranche 9 remained open; the
subsequent Windows replay below closes them. Linux/Windows were not executed on this Mac. `06-web` packaging,
security closure and `STATUS.md` are unchanged.

#### 9C Windows/x64 native replay (2026-10-18)

Replay from `9b76335` closes the remaining native 9C host; this is not
a repetition of the earlier Windows 9A/9B acceptance. The fresh predecessor
configuration preserves the imported-libc++/Skia/GPU/FFmpeg/LVGL settings.
Current worker/consumers build and SDKs `01-c`..`05-ui` verify. The official
entry rebuilt 2,152 browser/JSC/bundled-Skia steps against that SDK and linked
WebKit, Web/Network/Host and B7 executables. This is an incremental product
rebuild, not a clean full build or isolated `06-web` build.

The initial rebuilt-browser 9C run failed before the lifecycle cases: the
unread-transfer fixture sampled 219 native handles before its first socket
and 231 after cleanup. A separate GPU-free CRT `socketpair`/close probe starts
at 53 handles, reaches 68 once and stays at 68 over eight repetitions. Windows
CRT lazily initializes process-wide Winsock services; that cold-start cost
must not be counted as leaked GPU transfers. The Windows-only fixture now
opens and closes a transport pair before sampling its baseline, retaining
the original +2 handle/thread bounds and printing before/after resources.
No runtime code, GPU fallback or loss policy changes. The repaired checkpoint
passes three strict 9C repetitions with zero handle growth. The official
driver then passes independently (+248 KiB/+2 threads/+0 handles, native
mapping and Host ABI clean). Failed logs
remain in `out/web-windows-tranche9c-initial/` and
`out/web-windows-tranche9c-diagnostic/`; the GPU-free probe log is
`out/web-windows-tranche9c-socket-baseline.log` and repaired checkpoint is
`out/web-windows-tranche9c-transport-fixed/`.

```powershell
python tools/build_webkit_crt_port.py --sdk-root out/windows-host-ninja-debug/dist/05-ui --deps-prefix out/windows-host-ninja-debug/port-tests/install --work-root C:/crtw/b3w --reuse-tree --webkit --target crtweb_host --target WebProcess --target NetworkProcess --target crtweb_lifecycle_test --run-gpu-lifecycle-tests -j 8
```

Build logs: `out/web-windows-tranche9c-configure.log`,
`out/web-windows-tranche9c-sdk-build.log` and
`out/web-windows-tranche9c-driver.log`; the successful official repeat is
`out/web-windows-tranche9c-driver-final.log` with its JSON/stdout/stderr preserved
in `out/web-windows-tranche9c-official-final/`. Focused evidence/backend/closure/license/
driver helpers pass **68/68**; their log is `out/web-windows-tranche9c-helpers.log`.
The unchanged manifest remains 23 patches / 63 modified / 24 CRT-owned files.
Native D3D12 PAL passes twenty shared-fence epochs and exact pixels with three
transfer handles. PAL/wire/SurfaceView CTests pass **5/5**; no-browser resource/
lifecycle CTests report **two skips**, not native acceptance. Strict missing
prerequisites return 1 and invalid lifecycle/window mode returns 2. Logs have
the `web-windows-tranche9c-{pal-strict,focused-ctest,skip-ctest,negative-controls}`
prefix. The Windows native-COW allocator fixture is inapplicable to memory-copy
fork; its skip must not be described as a Windows native-COW replay.
The refreshed C-stage has **156 passes / 1 expected native-COW skip / 0 failures**
(`out/web-windows-tranche9c-core-ctest.log`, 157 entries). This correctness run
overlapped browser compilation; its elapsed time is not a performance baseline.

**Final strict 9C: 3/3**, each with seven policy warm-ups, twenty measured
complete lifetimes and final fresh pixels: **84 worker sessions** across three
runs. All normal, consumer-disconnect, exact owned worker/Host termination,
Web-process replacement/reload, context-loss/recreation and held-image resize
cases pass. Case elapsed times are **734..3,038 ms**, below the unchanged
30-second limit. RSS is **-8/+120/+196 KiB**, threads **+2** and handles **+0**
per run, within +24 MiB/+2/+2 bounds. These are bounded resources, not a
zero-thread-growth claim. The deterministic three-announcement fixture drains
nine transferred native D3D12 handles without imports, receives BYE and returns
to its baseline **231 handles / 8 threads** on all three runs. Imported pixels
survive producer/connection death in the healthy app; an abandoned context
exposes neither image nor ACK. All observed browser descendants and workers
are gone between lifetimes.

Native execution reports backend **1** with device identity
`ca9e00000000000086800000923e0000` and driver identity
`5c08650000001f008680122200000000` on Windows 11 Pro build 26100. Native
Host/Web/Network mapping passes **144 file/role observations per GPU run**
without violations; static Host ABI passes. CRT browser GL stubs remain in use;
transitive System32 DLL mappings are classified separately from direct imports,
not claimed as accelerated WebCore.

**Regressions on the same rebuilt browser:**

- 9A **offscreen 3/3 + native D3D12 window 3/3**, each window **5/5** frames;
  pixels/composition/input/updates/resize/recovery/cleanup and browser audits pass.
- 9B **offscreen 3/3 + native D3D12 window 3/3**, each window **5/5** frames;
  shared textures/fences, twenty updates, resource reuse/retirement/ownership and
  audits pass. App RSS is +4 KiB, worker RSS +108..204 KiB, threads +0 and
  handles -1..0 during the update measurements.
- CPU WebView passes three forced-process recoveries and twenty complete
  lifetimes at **-28 KiB/-2 threads/+0 handles**. Refreshed B7 passes
  **100/100**, **-52 KiB/-3 threads/+0 handles**, at most two children;
  runtime mapping passes **143 observations**, and all **18** produced PE
  executables/DLLs pass the static Host ABI gate.

Final local evidence is `out/web-windows-tranche9c-final/summary.json`,
`9{a,b,c}-*/{result.json,stdout.log,stderr.log,launch.json}`, `cpu/`, `b7.json`,
`artifacts-{before,after}.json` and `closeout.json`. All **15** strict GPU
invocations plus CPU and B7 pass; **14** selected artifact SHA-256 hashes stay
unchanged before/after the matrix and at closeout. The read-only native process
inventory finds no remaining test/browser/GPU-worker process in the replay
build trees. The generated `out/web_windows_tranche9c_replay.py` uses checked-in
strict evidence/map/Host ABI predicates, with no builds between measurements.
Its socket scratch under `out/web9c-run/` respects the fixed 108-byte AF_UNIX
address limit; launch records retain exact working directories. SDK and browser
locations are unchanged, and this is not a path-with-spaces acceptance run.
Native children inherit the console as in the official driver; no child names,
cleanup requirements, bounds or deadlines are excluded to obtain a pass.

This closes **Windows 9C and, with the prior Linux/macOS evidence, global 9C /
Tranche 9**. Only the test's Windows cold-transport baseline changes; Linux/
macOS fixture control flow, runtime code, wire/public ABI and upstream patches
are unchanged. Physical GPU reset, arbitrary hung-driver recovery, audio and
camera are outside this replay. The D3D12 hook removes a logical device only
after borrowed objects/context destruction and then recreates the native owner;
it is controlled loss, not a physical reset guarantee. The owned GPU-process
boundary is `crtweb_gpu_worker`, not upstream WebKit GPUProcess or accelerated
WebCore. `06-web` packaging/security (Tranche 10) and `STATUS.md` remain unchanged.

### 10. Distribution and security closure

Isolated `06-web` build from the installed `05-ui` SDK: JSC/WebCore/WebKit ->
`libcrtweb` -> a browser sample, `verify_dist.py`, dependency/RPATH audit, and
publication only after all three hosts pass. Include the provenance/SBOM/CVE
policy artifacts named in Tranche 0. This tranche also owns the complete
build-host/target split: gperf, Perl, Python, Ruby and ICU data generators execute
for the build host and never enter the SDK or target ABI audit; ICU, libc++, JSC,
WebCore and WebKit are built for the target. Acceptance includes an
x86_64-host -> Linux/aarch64-target isolated build.

#### Linux-first development packaging contract (2026-10-18)

The first implementation is deliberately a **development candidate**, not a
three-host or public-release closure. `tools/build_stage_06_web.py` consumes an
installed, option-ON `05-ui` SDK and an extracted/pinned `06-web` source asset.
It has no dependency on the repository's runtime objects or an existing WebKit
build. A source-only Git checkout cache is permitted, but its pinned commit,
tree/listing and carried-patch hashes are rechecked. CMake/Ninja, Python, Perl
and Ruby run on the build host. Host gperf and ICU tools are built in a separate
host directory with the host compiler/runtime; target ICU explicitly uses that
ICU cross-build root for data generation. Target ports and JSC/WebCore/WebKit
use the predecessor's CRT toolchain. The initial Linux-only entrypoint is now
extended to native macOS/arm64 as recorded below; other host/target pairs are
rejected rather than treating a native build as cross-target proof.
In particular, inherited native port recipes can still execute CRT-built make
or gperf on the same architecture; this is not the complete cross-build split.
The explicit host gperf passed to WebKit and ICU host data tools do not establish
the remaining aarch64 proof by themselves.

The predecessor must install the current `crtgfx/shared_image.h` and
`crtgfx/skia_shared.h`, the SDK Skia archive/headers, media archives and notices,
and LVGL provenance. Old option-OFF or pre-9B SDKs are not silently combined
with headers/libraries from a repository build. The in-tree option-ON publisher
was extended to provide this installed development interface, including the
media libcurl shared-SONAME closure. This is not a new clean replay of every
preceding isolated source stage.

The application owns SDK Skia (milestone 148); WebKit privately builds its own
Skia (milestone 154). `libcrtweb`/WebKit and the Web/Network processes remain
replaceable together behind the C/process boundary. The sample is a separate-
process CPU-shared-memory WebView; `--smoke` checks a local page load, script-
changed title and exact background pixels without a window. It does not claim
GPU rendering, physical GPU reset or new 9C evidence. The owned GPU worker is
packaged separately. The application/GPU worker keeps the accepted static CRT
runtime; the shared-runtime WebKit build environment is not propagated into
these native window/GPU PAL consumers. User-provided PEM trust is mandatory for
HTTPS; the sample has no sandbox and is for local/trusted content, not general
untrusted browsing.

Packaging must not overwrite the media stack's mbedTLS `libcurl` with WebKit's
OpenSSL `libcurl` under the same SONAME. The Linux stage selects the latter's
static archive privately in WebKit and excludes archive globals from its dynamic
exports. The media SONAME/manifest remains intact. No host generator executable
is copied into the target SDK. A relocatable fontconfig file names only the
bundled fonts, not build-host font paths.
FindCURL can append pkg-config's shared library even after
`CURL_LIBRARY_RELEASE` selects the static archive. The stage binds
`pkgcfg_lib_PC_CURL_curl` to that same archive too; the installed WebKit must
have neither a direct `libcurl.so` dependency nor exported private curl symbols.

WebKit, VA stubs and a browser-private copy of the media library are installed under
`libexec/crtweb/lib/`; only `libcrtweb.so` is added to the SDK's common `lib/`.
The normal media library and GPU worker retain their real hardware backend.
Linux C++ runtime copies made by older SDK publishers are checked byte-for-byte
and normalized to one image with relative aliases. The producers/copy paths in
stages 02 through 05 now preserve those aliases instead of dereferencing them.
Neither repair changes the installed input SDK. The packaged-browser observer
checks the three CPU browser roles, one mapped image per LLVM runtime family,
SDK-only library mappings and owned descendant cleanup. Its font cache is
run-owned, non-executable data, not a host-library exception.
Deleted, non-executable CRT IPC mappings (`memfd`, `crt_memfd_*` and the
`WK2SharedMemory` shm fallback) are classified separately; executable, live or
unrelated temporary files do not receive that exception. Output-stage metadata
tools are updated from the asset without replacing the predecessor's compiler
wrappers. The prerequisite manifest declares only the target architecture's
existing ELF interpreter SONAME for the shared-runtime TLS import.

The Windows media export fix is shared by in-tree and isolated `04-gfx-media`
builds. `crtmedia_public_symbols.txt` enumerates the 75 public header functions;
the generated `.def` intersects it with the configuration's defined symbols so
FFmpeg-OFF builds do not require optional codec symbols. Internal `crtmedia_*`
helpers, FFmpeg/curl globals and SDK GUIDs are not a public API. A cross-generated
PE fixture is a useful unit check, **not Windows native DLL/export acceptance**.

The first fresh Linux dependency build exposed a real shared-runtime wrapper
defect before WebKit: GNU ld rejected `libc++abi.so`'s `__tls_get_addr` when
linking an ICU tool. The existing empty loader stub named the interpreter but
advertised no import. The wrapper now creates an explicit-target, link-only
import stub for that one TLS symbol in private per-link storage; it does not
link host libc or relax unrelated unresolved-symbol checks. The actual shared
CRT TLS/thread-destructor test and missing-import negative control pass, with
stub cleanup on both paths (`out/tranche10/loader-probe-v2/result.json`). This is
a toolchain-boundary fix, not a new public libc/PAL symbol or loader implementation;
policy lives in [C++ runtime](../design/cxx_runtime.md#linux-shared-runtime-loader-imports).

Each candidate carries the 64 inventoried WebKit notices checked against their
hashes, target-port notices/recipes, the product pin and patch/overlay records,
source-correspondence instructions, a package-level **SPDX 2.3 JSON** composition
and a dated WebKit advisory snapshot. Mixed/unreviewed license expressions stay
`NOASSERTION`; that is not a legal-compliance certification. The full per-file
scan and bundled-component inventory remain authoritative provenance inputs.
`verify_dist.py` validates the manifest/layout, dependencies, portable runtime
paths, no host generators, security evidence and SBOM presence. A security
snapshot must have been current at packaging time; immutable old packages do
not become structurally invalid merely because time passes.
The SPDX composition covers WebKit, its target ports and the predecessor's
declared redistributed dependencies; it is not a completed inventory/license
review of the inherited C/C++ runtime and shell. That inherited notice closure
also remains a publication gate. Source correspondence records the full
predecessor-content SHA-256 as well as the source asset's identity.

On 2026-10-18 the official [WPE advisory index](https://wpewebkit.org/security/)
listed WSA-2026-0006 as newest. The version-bound check of all 50 linked
advisories did not mark the existing **2.54.0** pin affected. However, the
[release index](https://wpewebkit.org/releases/) already lists **2.54.1**
(2026-10-06): advisory success is not stable-series freshness. The snapshot
records both facts and cannot grant public-release approval. Before publication,
update/reverify the pin and source/patch correspondence, replay the product and
package gates, and confirm upstream's still-undocumented support window.
Windows/x64 native packaging and x86_64-host -> Linux/aarch64-target isolated
proof remain required; macOS/arm64 development acceptance is recorded below.
**Tranche 10 is not closed.**

#### Linux/x86_64 isolated build and relocation replay (2026-10-18)

The official source-stage entrypoint produced `out/tranche10/sdk06-v10` from
the installed option-ON `out/tranche10/predecessor05-v2`, not an in-tree runtime
build. The predecessor itself was generated by the in-tree publisher; this is
proof of the isolated **05 -> 06** transition, not a new clean 01 -> 05 chain.
The first isolated WebKit build completed 8,305 actions. The accepted final
asset reused verified dependency inputs and completed 1,369 incremental WebKit
actions, then freshly configured/clean-built the two SDK-only consumers.
It is not claimed as another clean full WebKit build.

The local, dirty-development source asset has SHA-256
`7ab65fb0330d35e7c641f5d1c8bb856128efd2ada15615837ad1d775dfa9d67b`.
Its full predecessor-content identity is
`f0ba95615f086c3f9428351427450729e3f49b0802b88b97232c0e9c61857347`.
The input SDK and all four compiler wrappers remain unchanged. Source recipe,
archive and final log are under `out/tranche10/assets-v10/` and
`out/tranche10/native-v10.log`; reproduce the accepted transition with:

```sh
python3 tools/crt-stage-build.py \
  --sdk-root out/tranche10/predecessor05-v2 \
  --recipe out/tranche10/assets-v10/06-web.json \
  --asset out/tranche10/assets-v10/crt-tranche10-v10-linux-06-web-source.tar.xz \
  --previous-source-asset out/tranche10/assets-v9/crt-tranche10-v9-linux-06-web-source.tar.xz \
  --source-cache out/web-crt-webcore/product \
  --work-root out/tranche10/native-v2 --output /absolute/path/to/new-sdk06 \
  --reuse-work-root --dependency-jobs 12
```

On an empty work-root, omit the reuse/previous-asset options. The optional
source cache contains only verified upstream sources. A local asset is not a
published release: regenerate a clean committed source asset and complete the
remaining gates before publication. Reusing this work-root after the accepted
run can omit `--previous-source-asset` for the unchanged v10 asset. A changed
follow-up asset must name v10, rather than v9, as its previous input.

The accepted SDK passes the 21 selected native dependency smokes, C API test,
installed browser load/script/exact-pixel smoke, three-role runtime observer,
HTTP/TLS **10/10**, static Host ABI and `verify_dist.py`. The C library exports
exactly **37** public functions, with no private header/export leakage; WebKit
exports no `curl_*` and has no direct shared-curl dependency. No host generator
is bundled, no VA stub is installed in common `lib/`, and each CPU browser role
maps one image per LLVM runtime family. All observed descendants retire.

Copied the SDK with symlinks preserved to `out/tranche10/relocated SDK` and
freshly built its `examples/web-browser` outside the SDK with only its installed
toolchain/headers/libraries. With the entire original `native-v2` directory
temporarily renamed out of reach, both the installed example and external
consumer pass the observer and load/script/pixels, and the separately rebuilt
C API probe passes HTTP/TLS **10/10** again. Relocated `verify_dist.py` passes;
the original build directory was restored. No loader-path override is used.
The 3,583-entry SDK inventory remains byte/alias-identical across the copy;
its SHA-256 is
`f4b636ae42cde5222a8739e2401581c76a652c98a6598f3f892ebaaceea8a575`.
Selected binary hashes and input/copy invariance are in
`out/tranche10/final-artifacts.json`. Relocation commands/results and native
observations are in `relocation-final.log`, `relocated-{installed,external}-observation/`
and `relocated-network.json` under `out/tranche10/`.

Fresh CMake/C-stage **138/138**, the registered distribution CTest, **75**
focused distribution tests (**74 pass, one expected macOS-only skip**), 15
licensing tests and six Web driver/lifecycle-evidence tests pass. The unchanged
patch gate measures **23 patches / 63 modified / 24 CRT-owned files** against
the pristine pinned product checkout. Logs remain under `out/tranche10/`.

Failed investigation checkpoints are not acceptance: missing PNG/SQLite roots
and temporary font fixtures were corrected in owned stage glue; the first
consumer link leaked the browser's shared-runtime setting into a static app;
the initial RPATH pass touched an inherited static example; the observer
initially rejected deleted CRT IPC data; and FindCURL initially retained a
shared curl alongside its static archive. The v9 run was deliberately
interrupted to align packaged metadata tools with the final manifest. Final
v10 uses the official entrypoint without those diagnostic repairs. No upstream
patch, owned public ABI or acceptance limit was changed. No new GPU/physical-device,
Windows/macOS or aarch64-cross acceptance is claimed.

#### macOS/arm64 isolated build and relocation replay (2026-10-18)

Native replay on macOS 27.0.1/Apple M1 Pro produced
`out/tranche10-macos-20261018/sdk06` through the official source-stage entrypoint.
The input `predecessor05` is an immutable copy of the refreshed, option-ON
in-tree `05-ui` publisher output. This proves the isolated **05 -> 06**
transition, not a new clean isolated 01 -> 05 chain. Host gperf/ICU tools are
separate host builds; target libraries use the installed CRT wrappers. No
existing WebKit object or binary is imported. This is native arm64 proof, not
the remaining x86_64-host -> Linux/aarch64 cross-target gate.

The final local dirty-development source asset is
`assets-v4/crt-tranche10-macos-v4-macos-06-web-source.tar.xz`, SHA-256
`bcc2932cf3312e1ae495777272fcefc3ebbd1a378722f826ff80077d76cd223a`.
The full predecessor-content identity is
`d8d12a481f4bb76c2f53d7cb431b240220c25f9ec5c20aaf2fb49c6a189a793b`.
Input inventory is identical before/after the pass; compiler wrappers remain
unchanged. A fresh 8,301-action WebKit build completed in the v2 checkpoint;
the accepted v4 asset completed 1,369 incremental actions after verified
packaging-only source correspondence and freshly clean-built both small
consumers. It is not claimed as another clean full WebKit build. Reproduce
with the retained input tree (choose a nonexistent output):

```sh
python3 tools/crt-stage-build.py \
  --sdk-root out/tranche10-macos-20261018/predecessor05 \
  --recipe out/tranche10-macos-20261018/assets-v4/06-web.json \
  --asset out/tranche10-macos-20261018/assets-v4/crt-tranche10-macos-v4-macos-06-web-source.tar.xz \
  --work-root /private/tmp/crtw10m --output /absolute/path/to/new-sdk06 \
  --reuse-work-root --dependency-jobs 6
```

For a fresh empty work root, omit `--reuse-work-root`. A changed follow-up
asset must name v4 with `--previous-source-asset`; the accepted run upgraded
from the retained v3 asset that way. The optional old `/private/tmp/webB3f`
source cache lacked complete Git checkout metadata and was rejected, not
trusted or patched into a build. The product checkout was fetched afresh at
the pinned commit/tree/listing. The new early cache validator also rejects
modified, untracked or ignored generated content. Source cache metadata is not
a reason to import old objects or weaken provenance.

The accepted SDK passes 21 selected native dependency smokes, C API execution,
the installed CPU browser, HTTP/TLS **10/10**, export/header checks, strict
static Host ABI and `verify_dist.py`. Public exports are exactly **37**;
private WebKit exports no `curl_*` and has no direct shared-curl dependency.
The existing mbedTLS media curl stays in common `lib/`; WebKit owns a private
static OpenSSL curl. Versioned Mach-O images, including
`libWebKit.dylib.SOVERSION`, and their relative aliases are preserved together.
New Web images use ordered package-relative install names/RPATHs and are
ad-hoc signed after editing. Inherited runtime images are not rewritten.

The native ps/vmmap observer holds the painted sample at an explicit
checkpoint and validates Host/Web/Network ancestry, mappings and cleanup.
Each role maps exactly one SDK libc++ and libc++abi image. Darwin's existing
libSystem unwinder contract is unchanged; no separate CRT libunwind image is
claimed. Apple framework/shared-cache mappings, including the OS's transitive
C++ runtime, are recorded separately, not counted as CRT images. Direct Mach-O
dependencies still undergo the strict Host ABI firewall; media frameworks are
allowed only at the existing `libcrtmedia` PAL boundary. Fontconfig uses
bundled fonts and an owned non-executable cache. PID-scoped, immediately
unlinked, non-executable CRT `memfd` data is recorded separately; live,
executable, unrelated or foreign-PID temporary files do not get that exception.

Copied the output with symlinks preserved to
`out/tranche10-macos-20261018/relocated SDK/06-web`, and built its installed
`examples/web-browser` in a fresh external directory using only that SDK's
toolchain/headers/libraries. With the **entire** `/private/tmp/crtw10m` tree
temporarily renamed out of reach, the original installed browser, relocated
browser and external consumer each pass load/script/exact background pixels,
all three roles, one CRT C++ image per family and complete owned descendant
cleanup. No loader-path override is used. Both SDKs pass Host ABI and
distribution checks, and the rebuilt relocated C API probe passes HTTP/TLS
**10/10** again. The original work tree is restored.

The **3,532-entry** output inventory is byte/alias-identical across copying
and runtime checks; its canonical inventory SHA-256 is
`21258622d84ed0e9e3de596004d094d65527e136394b3e097cce2e0a2de570d0`.
Evidence lives under `out/tranche10-macos-20261018/`: `replay-result.json`,
`relocation-replay.log`, `{installed,relocated,external}-observation/`,
`{installed,relocated}-host-abi.json`, `relocated-network.json`,
`public-exports.json`, input/output inventories and the retained source assets.
Official build logs and pre-publication reports are retained alongside them.
Native C-stage **138/138** and **113** focused distribution/source/security/
licensing/Web-helper cases (**112 pass, one expected Darwin-nm/Windows-export
fixture skip**) pass. The patch gate remains **23 patches / 63 modified /
24 CRT-owned files**.

Failed checkpoints are not acceptance: v1 exposed incomplete source-cache Git
metadata; v2 omitted the real `.dylib.SOVERSION` image; v3 exposed Apple's
prohibition on deleting and adding the same RPATH in one invocation. Separate
diagnostic copies confirmed the RPATH repair and the existing narrow IPC
classification before v4's official run. Those logs/copies remain separate.
No new upstream patch, public ABI, runtime/PAL policy or acceptance limit was
changed. This is CPU package/consumer acceptance, not new GPU-loss, physical
camera/audio, Windows or cross-target evidence.

The candidate includes the 64 hash-checked WebKit notices, target-port recipes/
notices, source correspondence and SPDX 2.3 package composition. Its fresh
2026-10-18 advisory snapshot checks all 50 linked advisories, passes for the
2.54.0 pin, and still reports 2.54.1 as the newest micro. `NOASSERTION` remains
honest unreviewed license metadata, and `public_release_allowed` is **false**.
Windows native packaging, cross-target proof, inherited runtime/shell notice
closure, current stable-pin revalidation and upstream support confirmation
remain required. **Tranche 10/public release is not closed.** `STATUS.md` is
unchanged.

#### Windows/x64 isolated build and relocation replay (in progress, 2026-10-18)

Replay starts from `65cd4c6`. The existing Windows predecessor has Skia/FFmpeg/
LVGL enabled but CURL disabled, so its publisher does not install the complete
media dependency/provenance set. The new stage correctly rejects it before
building WebKit; it is not accepted as an option-ON predecessor. Building the
missing curl/mbedTLS layer with the original CRT recipes and explicit Windows
SDK import-library path precedes a fresh full predecessor publication.

Native Windows packaging implementation/replay is ongoing. Host gperf/ICU
generators are configured separately with the native MinGW compiler/runtime;
Git Bash is a build-host configure tool only. Target ports retain the installed
CRT mksh/make and CRT wrappers. The existing pinned WebKit cache passes the
source-only/pristine verification; no previous browser objects are inputs.
The Windows layout uses a private package DLL directory (PE has no ELF RPATH),
with exact module/process identity observations and the same public API/export
boundaries. No installed/relocated SDK or public release success is claimed yet.

The rebuilt full option-ON predecessor verifies and its immutable input copy
is byte/alias-identical. Native `libcrtmedia.dll` exports exactly the 75 reviewed
public functions, with no FFmpeg/curl/GUID/private helper exports. A separate
CRT-linked consumer imports the actual DLL through `libcrtmedia_dll.dll.a`,
resolves all 75 names with `dlsym`, and passes 100 frame geometry/release cycles,
including callbacks into caller-owned code and zeroed released wrappers.
`out/tranche10-windows/media-dll-smoke.json` records its real PE imports and
successful native output. This closes the Windows media export sub-gate, not
the isolated Web SDK gate. CRT curl/mbedTLS static/shared smokes and HTTP/HTTPS
round trips pass; the latest closeout runs 81 focused helper cases (including
36 stage cases/native Windows observer) plus 2 WebKit driver cases, all passing.

The incremental host-generator probe built native MinGW/UCRT gperf and ICU;
that is not fresh acceptance. The first official fresh build exposed its stale
archive-name assumption at `gentest` before target compilation. ICU's upstream
MinGW fragment needs both supported make variables `STATIC_PREFIX=` and
`STATIC_PREFIX_WHEN_USED=` in static-only mode; its unquoted `$(SHELL)` requires the installed Git shell's
space-free Windows alias. Git's launcher also prepends its own incompatible
MinGW C++ DLLs: the same host `icupkg` exits 127 under that search order and 0
with the selected compiler's runtime directory first. The owned host launcher
sets that precedence inside the shell. Neither GNU runtime nor host tools are
copied into the target SDK; no source patch or target ABI workaround is used.
These are host-tool constraints, not a successful spaced host-tool build.

The official local source asset v1 has SHA-256
`fcd19e92d5d994ff06b939326f21b12189c2cfe419ab69242d9e663b20148806`.
It attempted a build from `out/tranche10-windows/predecessor05` in `C:/crtw/t10w`,
using explicitly supplied `CRT_CC`, `CRT_CXX`, `CRT_AR`, `CRT_RANLIB` and
`CRT_WINDOWS_SDK_LIBPATH` as required by the predecessor manifest. The first
invocation rejected the omitted LLVM environment before compilation; the
identical-input retry retains that SDK checkpoint but fails in the host phase;
no target compilation or isolated SDK acceptance is claimed for v1. A new v2
asset/work-root will verify the corrected host naming without stale archives.
Logs: `out/web-windows-tranche10-native-v1-tools.log` and
`out/web-windows-tranche10-{preflight-final,media-dll-smoke-final,host-probe-v6}.log`.

The v2 fresh host build passes, but target ICU stops with Windows error 53
when CRT make invokes native `icupkg`: the host compiler's runtime DLLs are
not in that isolated shell's search path. The v3 host phase binds those DLLs
by adjacency under `host/` only and records their hashes; they must never
enter target dependencies or the SDK. Native MinGW `pkgdata` in turn executes
compiler commands through Git Bash. The owned `crt_icu_host_pkgdata.py` derives
an options file/CRT-shell relays with physical SDK launcher paths and preserved
compiler flags, rather than changing upstream source/generated options or using
host gcc for target data. It normalizes native path separators, selects the
intended Git shell and restores the CRT namespace inside the target wrappers.

The minimum real static/DLL data-generation probe passes; its DLL imports
only `KERNEL32.dll`, not host UCRT/GNU C++ libraries, and the original options
remain byte-identical. A complete data make in the retained v2 tree also passes
as a **diagnostic**, not source-stage acceptance. Logs are
`out/web-windows-tranche10-icu-native-probe-v4.log` and
`out/web-windows-tranche10-icu-data-make-diagnostic.log`.
The new official v3 run uses fresh `C:/crtw/t10w3` and asset SHA-256
`d8d8d2292bf1a47f2eccc2b50360a5647d617300a179aa62f4cf2cf8770537c3`;
its log is `out/web-windows-tranche10-native-v3.log`. Fresh target ICU builds,
and its static Unicode smoke passes, but the shared smoke reports
`U_FILE_ACCESS_ERROR`: MinGW host `pkgdata` links the real data DLL but omits
the shared `-I` install step, leaving the 20,480-byte stub in the prefix instead
of the 33,127,936-byte data DLL. The owned host bridge now completes only that
explicitly named DLL/import-archive installation and verifies copied hashes.
Installing the real data DLL makes the **same already-built** static/shared
Unicode consumers pass; this retained-tree diagnostic is not stage acceptance
(`out/tranche10-windows/icu-install-diagnostic.json`). No upstream option file,
source or target runtime is patched.

The official v4 source asset has SHA-256
`2f38e604ab0a078b59ac90fad4c4f93fda26e42ef95bd52e8782c8aba6dc8125`
and starts from fresh `C:/crtw/t10w4`; its log is
`out/web-windows-tranche10-native-v4.log`. The input predecessor remains
unchanged. Output publication and installed/relocated acceptance are still
pending. A later packaging-only retry will also carry the CRT drive-path
conversion for fontconfig data and loopback CA fixtures; its heavy compile-input
identity must match v4, and the small consumers must still be freshly rebuilt.
The running extracted v4 asset is not edited in place. On 2026-10-19 its fresh
target ICU static/shared Unicode consumers both pass, reproducing the install
fix through the official source-stage path. Windows Core replay passes
156 tests with one expected native-COW skip; the existing memory-copy fork
mechanism is unchanged (`out/web-windows-tranche10-core-ctest.log`).

The independent fresh SDK-only consumer preflight builds both browser and GPU
worker from the installed predecessor, without any WebKit build-tree library.
It exposed native backslashes in CMake `-D` source paths and missing
`SK_BUILD_FOR_UNIX` in these two Skia consumers. POSIX CMake path spelling and
the same header configuration as the installed CRT Skia archive fix both;
no MSVC header/host CRT is added. The successful four-action compile/link log
is `out/web-windows-tranche10-consumer-preflight-final.log`. This is not browser
execution or GPU replay. These small consumers will be clean-built again by
the packaging-only retry; earlier setup/configure/compile failures remain
diagnostics, not acceptance.

Prepared retry asset v6 has SHA-256
`4b3bfa04b14754016febbdd83f69071e174d9e56fa3d5d49b6fd98f0893d16e9`.
Comparison of the two archives confirms all 188 heavy compile inputs and the
driver's compile-phase identity are identical to v4
(`out/web-windows-tranche10-retry-identity.log`). V5 is an intermediate source
archive only, not an accepted or executed build. This comparison was valid for
v6, but the later native-Perl/port-builder changes below alter heavy compile
inputs: **they require a fresh source asset/work-root**, not that packaging retry.

V4 stops during OpenSSL configure: native host Perl cannot open the packaged
SDK's CRT-virtual `/porting/...` config path. The port builder now substitutes
a physical host root for `configure_interpreter` while retaining CRT compiler
commands. Strawberry Perl's Win32 File::Spec is incompatible with OpenSSL's
Unix target; Git's POSIX Perl needs `IPC::Cmd`/`ExtUtils::MakeMaker` modules.
The existing build-host-only `C:/crtw/perl-overlay` supplies them through the
explicit `CRT_WEB_HOST_PERL_LIB` input; file hashes and the Perl binary/version
are recorded, not shipped. No global package installation or upstream patch
is used. WebKit's generators still use their accepted native Windows Perl.

OpenSSL configure passes with that separation. Calling host Perl directly
from CRT make loses generated-file redirections; invoking it through the
existing `crt-native-tool` boundary preserves them (nonempty `builddata.pm`
and `installdata.pm`). A fresh OpenSSL **diagnostic** build at
`ports-openssl-native-probe` configures/builds/installs and passes the native
TLS 1.3 static smoke (`TLS_AES_256_GCM_SHA384`), logged in
`out/web-windows-tranche10-openssl-diagnostic-v5.log`. The documented Windows
shared-test exclusion remains explicit; it is not a new shared-build pass.
This result is not an accepted source-stage SDK. Earlier Perl/module/direct-launch failures
are preserved in `out/web-windows-tranche10-openssl-diagnostic*.log`.

The new official fresh run uses `C:/crtw/t10w8` and source asset v8 SHA-256
`7478be3cfbee3cd64de63dcc654bc96288e03b6f6e5fae71313d006608a65666`.
It sets `CRT_WEB_HOST_PERL_LIB=C:/crtw/perl-overlay` in addition to the five
explicit external LLVM/Windows SDK inputs listed above. Log:
`out/web-windows-tranche10-native-v8.log`; intended output:
`out/tranche10-windows/sdk06-v8`. V7 was prepared but never executed. V4 and
its subsequently modified diagnostic dependencies are not reused as build
inputs; only the verified source-only WebKit cache and immutable predecessor
are reused. Final publication/installed/relocated acceptance remains pending.

V8 again passes both ICU Unicode smokes, then stops in libpng's man5 install
at `mkdir -p` with `Error 5`, without a useful child diagnostic. The directory
exists and the exact single CRT mkdir command passes on replay. One owned
orphan mksh (PID 16620, parent 8000 already absent, creation 08:11 KST) remains
with its only thread `Wait/Suspended`; after path/PID/parent checks only that
process is retired. This is a failed checkpoint, not a successful cleanup gate
or a claimed PAL fix. The root cause is unconfirmed. An identical-asset/input
retry uses `--reuse-work-root` and
`out/web-windows-tranche10-native-v8-retry1.log`; it must pass without such
intervention. The initial log and staged SDK checkpoint are retained.
Retry 1 passes libpng install and both round-trip smokes without intervention,
then configures/builds/installs OpenSSL through the source-stage's host boundary.
It is currently building curl/OpenSSL; this is not yet a complete Web dependency
or SDK acceptance. V8's host-tools record hashes 258 external Perl module files
and three host compiler runtime DLLs, separately from target provenance. The
additional WebKit driver unit suite passes 2/2 alongside the 77 focused cases.
The retry's curl/OpenSSL static IPv4 and IPv6 HTTPS smokes pass; both shared
variants retain their documented Windows exclusion. The ordinary fontconfig
dependency recipe still builds/runs a CRT-linked gperf on this native host,
separately from WebKit's host gperf. That auxiliary executable is not copied
into the SDK. It is **not** proof of the complete host/target generator split:
the open cross-target gate must also rebind fontconfig's generator, rather
than execute an aarch64 target tool on an x86_64 build host.

Retry 1 builds/installs gperf, expat and FreeType, then fails immediately in
fontconfig configure: recipe `path_prepend` uses native Windows `;`/`C:/...`
before a CRT mksh POSIX PATH, hiding `printf`, `sed` and `expr`. The port builder
now uses `/c/...` plus `:` for Windows CRT-shell recipe PATH values, retaining
native host PATH behavior otherwise. Its new regression passes (33 stage
helper cases; 78 focused cases plus 2 WebKit driver cases). No applet/API or
upstream patch is added. These changed tool inputs invalidate source-stage
reuse; v8 is now a retained diagnostic tree, not an accepted build. The
remaining-port diagnostic uses the current builder and pinned v8 inputs,
logging `out/web-windows-tranche10-remaining-ports-diagnostic.log`. A new
official asset/work-root remains necessary before final SDK acceptance.
The original stage identity files for the out-of-band diagnostic roots
`t10w2`, `t10w3`, `t10w4` and `t10w8` are preserved as
`inputs-before-diagnostic.json`; their active identity is explicitly revoked
(`diagnostic_only`, null source identity). Even an old driver must refuse to
treat those modified outputs as an identical-input source-stage retry. The
classification log is `out/web-windows-tranche10-diagnostic-identities.log`.

The corrected PATH exposes a second fontconfig boundary: its CRT-linked gperf
exits `53` with the CRT shell PATH, whereas the same binary/input succeeds
with a native SDK DLL PATH (`STATUS_DLL_NOT_FOUND` for direct native launch).
The prepared native host gperf succeeds through a CRT-shell/native-tool relay
on that same input. The Windows stage now derives fontconfig's recipe to drop
its target-gperf dependency and binds both bare configure probes and Make's
`GPERF` to the owned host relay. Linux/macOS recipe behavior is unchanged;
the cross-target gate must still cover those hosts' generator bindings.
Logs: `out/web-windows-tranche10-fontconfig-gperf-diagnostic{,-v2}.log` and
`out/web-windows-tranche10-remaining-ports-diagnostic-v2.log`.

ICU's owned options/relay writers also pin LF output so their recorded UTF-8
hashes describe actual Windows file bytes, not a CRLF-translated variant.
The Windows byte-level regression and host-gperf recipe regression both pass
(35 stage helper cases). This metadata repair changes the helper input and
must be present in the new fresh official build, not retroactively attributed
to v8. No target runtime or upstream source is modified.

With the host relay, fontconfig configures/builds/installs and passes its real
font-match shared smoke. The next HarfBuzz CMake configure exposes an SDK-mode
archiver assumption: repository `tools/crt-ar.cmd` is not installed in the SDK.
The CMake port path now selects the declared external LLVM `CRT_AR` (with LLVM
discovery fallback), matching the packaged `crt-toolchain.cmake`, while keeping
the repository wrapper in repository mode. A space-path regression covers
that selection; 36 stage cases now pass. Remaining-port diagnostic v3 uses
this correction (`out/web-windows-tranche10-remaining-ports-diagnostic-v3.log`).
The final official source-stage build is still pending, not accepted from
these diagnostics.

#### Current Windows blocker (2026-10-19)

Remaining-port diagnostic v3 passes HarfBuzz shared shaping, libjpeg-turbo
static/shared round trips and libpsl shared suffix tests, then repeats the
unexplained `Error 5` in libwebp's header installation. Its libraries compiled;
`install-sh` copying `types.h` fails. This is the second affected package
after libpng, not evidence of a codec API failure or missing source/header.

The task-specific Windows Application Error records confirm **mksh access
violations (`0xc0000005`)**, rather than a plain permission-error diagnosis.
The 277 matching event records include two in `ntdll.dll` and 275 with an
unknown module/zero offset; event count is not a count of separately diagnosed
defects. Exported evidence is
`out/web-windows-tranche10-native-crash-events.json` (record IDs, timestamps,
fault PID/path/code/offset). At that checkpoint no dump/stack-based root cause
had been established. Fork/exec state restoration and the shell/PAL startup boundary were candidates,
not a confirmed explanation and not an excuse to patch upstream configure.

No retries or host-shell substitution are used to call this a successful SDK.
The failed diagnostic command is interrupted after child exit; closeout finds
no remaining SDK mksh/make/toybox process
(`out/web-windows-tranche10-failed-process-closeout.json`). The earlier explicitly
retired libpng orphan remains a failed-checkpoint detail above. No active
source-stage build or published Windows `06-web` SDK remains. A bounded native
runtime reproduction/stack investigation is needed before a final fresh
source-stage run; if the runtime changes, rebuild and freeze a new predecessor
rather than mutate the accepted input. Windows/public Tranche 10 remains open.
Final closeout reconfirms the immutable predecessor's complete 3,353-entry
byte/alias inventory, unchanged v8 source archive hash, absent accepted Windows
output and revoked diagnostic identities
(`out/tranche10-windows/closeout.json`, `out/web-windows-tranche10-closeout.log`).

The subsequent [bounded native investigation](../bringup/windows_mksh_tranche10_crash.md)
fixes a confirmed copied wait-HANDLE ownership defect, with before/after
regression evidence, C-stage pass and diagnostic libwebp installation replay.
It additionally reproduces a parent-only RNG DLL address fault in a minimal
fork consumer and fixes native-cache rebinding for inherited descriptors
and getrandom, including raw _Fork (100 regression repetitions pass).
It separately fixes copied DIR native-search state: child read/rewind/close
and EOF pass 100 fork-runtime repetitions, while the same test with the old
dirent implementation faults in the native search path. Full C stage remains
156 passed / one expected skip; Windows/arm64 compilation is not native replay.
The next fresh libwebp configure still faults at an actual PC=0. Bounded
stack-copy observations isolate a fourth defect: a nested memory-copy fork
trusts stale TEB.StackLimit and omits live frames. Copying the actual committed
extent fixes its regression (100 repeats and C stage pass). The failed
`predecessor05-pal-fixed`/`t10-pal-fixed` trial remains diagnostic-only;
an orphan empty `data/local/shpicurb.tmp` also makes its inventory check fail.
Neither that trial nor its runtime-only corrections are Windows SDK acceptance.

The final fresh port replay uses a newly frozen option-ON predecessor
`out/tranche10-windows/predecessor05-pal-replay-final`, a separate execution
SDK at `C:/crtw/t10-pal-replay-final/sdk`, and a fresh port work/install root.
Its libc archive/DLL, mksh and make hashes match the final runtime build.
Libwebp configure (87.9 s), make -j4 (92.3 s), install (14.7 s), and static
and shared round trips all pass. The corrected native debugger records
26,782 created processes, root exit 0, no access violation/invalid-handle
events; the input's full 3,353-entry inventory is unchanged. Report:
`out/tranche10-windows/pal-replay-final-fresh-port.json`; log:
`out/mksh-still-fresh-libwebp-final.log`. This closes the bounded fresh-port
runtime replay, **not** the full Windows `05-ui -> 06-web` stage or relocated
consumer acceptance. The earlier stack-fixed trial stopped at the debugger's
cumulative-process capacity and is not counted as a successful build.
It also captures the native heap fault at `ntdll + 0x31577`, but does not
establish that all historical heap/unknown-module crashes share these causes. No fresh Windows
SDK is accepted from that partial runtime correction; the new predecessor
and full fresh/relocated replay remain required.

##### Fresh Windows full-stage replay after PAL fixes

Windows/x64 development-package acceptance closed 2026-10-20. The following
dated checkpoints retain failed attempts separately from the accepted v12 SDK;
public-release and cross-target gates remain under Tranche 10.

The new official run starts from the verified immutable
`out/tranche10-windows/predecessor05-pal-replay-final` (3,353 entries,
runtime hashes matching the successful fresh port checkpoint). A clean
`f03626c` source asset is pinned at SHA-256
`6ae757b69492e8e01c663971917f896f7314b06fd60e21f86e60348a97f5c85e`
under `out/tranche10-windows/assets-pal-v9`. The installed predecessor's
`tools/crt-stage-build.py` is the bootstrap, with bytecode writes disabled
to preserve the input. The build starts from new `C:/crtw/t10w9`; intended
output is `out/tranche10-windows/sdk06-pal-v9`. No old objects/installed
dependency prefix or revoked source-stage identity is reused. Only the
verified pristine WebKit source-only cache is copied.

External LLVM/Windows SDK inputs and the host-only Perl overlay remain
explicit. Jobs are 4. Owned native debugging retains full exception dumps
on unhandled failures without global WER/registry changes; successful
per-DLL trace noise is suppressed. The runtime/relocation acceptance runs
after packaging are ordinary non-debugger executions. State/command:
`out/tranche10-windows/run-pal-v9.json`; log:
`out/web-windows-tranche10-pal-v9.log`. Output publication, installed and
space-path relocation/consumer checks remain pending; no success or public
release authorization is claimed by starting this run.

The fresh host phase has completed: native gperf and ICU generators live only
under `C:/crtw/t10w9/build/06-web/host`. `host-tools.json` records their paths,
tool versions, the selected POSIX Perl/module inputs and native runtime DLL
hashes. Target ICU builds with CRT wrappers and the separately supplied
host ICU generators: configure (69.6 s), make -j4 (304.6 s), install (77.9 s),
and both static/shared Unicode build-and-run smokes pass. The remaining
target dependency ports are still in progress.

The first owned-debugger attempt stops after 2,008.5 s in fontconfig's
`xargs -n` configure probe, not in FreeType or an observed heap access violation.
The dump identifies `RoOriginateErrorW` (`0x40080201`, HRESULT `0x80070002`)
inside native CreateProcess while trying a nonexistent PATH candidate. This
is a Windows debugger error notification, not an established PAL crash
([Microsoft contract](https://learn.microsoft.com/en-us/windows/win32/api/roerrorapi/nf-roerrorapi-rooriginateerrorw)).
A bounded identical CRT xargs probe passes normally, fails under the old
debugger, and passes with the notification correctly consumed; the corrected
debugger logs the notification explicitly. Report:
`out/tranche10-windows/winrt-debugger-probe.json`; full stopped-process dump
and LLDB stack are retained locally. No target runtime/recipe/source asset
changes. The input inventory is unchanged, no output is published, and no
owned build process remains. The identical source/predecessor identity is
resumed through the official `--reuse-work-root` contract with the corrected
external diagnostic tool; this is continuation of the same clean work-root
build, not reuse of an earlier accepted or revoked product tree. Original
failed evidence remains separate from `run-pal-v9-resume1.json` and
`out/web-windows-tranche10-pal-v9-resume1.log`.

All selected target ports have now completed their build/install and
recipe-declared execution tests in that same new work tree. ICU static/shared
Unicode, libpng static/shared round trips, CRT EGL, curl HTTPS/IPv6 static,
fontconfig shared matching, and the remaining declared image/text/TLS probes
pass. OpenSSL and its curl variant retain their explicit Windows static-only
contract; shared TLS probes are recorded skips, not shared acceptance.
The verified source-only WebKit tree is now copied for the fresh product
build. No WebKit objects or final Windows SDK are accepted at this checkpoint.

The first resume then reaches composition and stops in the build-host
Strawberry GNU patch 2.5.9 (`patch.c:354`, assertion `hunk`) on the first
manifest-pinned COFF patch. The same patch passes GNU patch 2.7.6 dry-run
against the pristine source. A second identical-input continuation explicitly
selects Git's hosted patch 2.7.6 while retaining native Strawberry Perl;
the host patch path/version/hash is recorded in `run-pal-v9-resume2.json`.
This changes external build-host selection only, not target recipes or the
pinned source asset. Recomposition still verifies every patch's before/after
SHA-256. Neither stopped attempt published an SDK; the predecessor remains
unchanged. The active log is `out/web-windows-tranche10-pal-v9-resume2.log`.

That continuation composes/configures successfully and reaches 7,625/8,300
Ninja steps, then stops in the native-host serializer generator, not a PAL
fault. CMake's batch line is 35,176 characters (35,142 excluding the batch
error suffix), beyond cmd.exe and CreateProcess limits. The CRT-owned
Windows CMake hook now passes only this generator's argv through UTF-8 JSON
to a Python in-process adapter; pinned upstream sources remain unchanged.
It preserves original dependencies, outputs, cwd, following commands and
unchanged-file timestamps. The adapter is a build-host input, not target ABI.

`python -B tools/test_webkit_host_generators.py -v` passes oversized argv,
spaces/Unicode, quotes/backslashes/newlines/literal @, adjacent imports,
no-op rebuild and dependency-triggered rebuild checks. Existing configure
tests (2) and stage tests (36) also pass; the new regression joins the existing
CTest workflow without adding a second CI run. The actual 309-argument
WebKit serializer generates all 20 expected outputs in a separate spaced
directory (`out/tranche10-windows/real serializer probe/result.json`).
Since this is a build-input change, the v9 work tree is retained as failed
evidence and must not supply objects to the next full-stage attempt. A new
SHA-pinned development-only asset/work root is required; final publication
and relocation gates remain open.

The new full attempt uses empty `C:/crtw/t10w10`, output
`out/tranche10-windows/sdk06-pal-v10`, and source SHA-256
`2ba21d13087c1e1c25ea04a8adc7ec82bc265aa4558950d7d9a14fd750c619f1`
(`assets-pal-v10`). This is an explicitly `--allow-dirty` **local development
snapshot**, based on f03626c plus the uncommitted host-generator fix, not a
claim that f03626c itself contains the fix or a public release asset. Its
archive hash and individual adapter file hashes are frozen in
`out/tranche10-windows/run-pal-v10.json`. No previous objects, dependency
install prefix or host-tool build is reused; only the reverified pristine
source-only WebKit cache is supplied. The immutable 3,353-entry predecessor
is unchanged. Host patch 2.7.6/native Perl and the corrected external
debugger are explicit inputs. Log: `out/web-windows-tranche10-pal-v10.log`.
Full build, publication and ordinary product-hidden space-path consumer
acceptance are still pending.

The v10 native host generators have completed afresh;
`C:/crtw/t10w10/build/06-web/host-tools.json` records their separate host-only
inputs. The new regression also passes through an actual fresh CMake/CTest
directory (`out/web10-host-generator-cmake-check`, 1/1). Target ports are now
rebuilt and all selected recipe execution probes pass in v10 itself; successes
from v9 are not substituted. New target ICU configure/make/install take
59.6/293.6/75.8 s, with static/shared Unicode smokes passing. OpenSSL TLS,
curl HTTPS/IPv6 static, fontconfig shared matching and the declared text/image
probes also pass; the documented Windows OpenSSL/curl shared skips remain.
Fresh WebKit composition/compilation is next; SDK publication and relocation
are not accepted from this dependency checkpoint.

The v10 full WebKit configure completes (165.7 s), producing a 36,006-byte
serializer argv JSON in the new build tree. The actual fresh Ninja build
passes the formerly failing serializer step and continues into WebKit shared
objects (7,686/8,300 observed). This confirms the host-command fix in the
real product build, not only the synthetic/standalone generator probes;
final linking, SDK validation and relocation remain pending.

The v10 product subsequently completes all 8,300 compile/link steps, including
libcrtweb, Host/Web/Network processes and the C API test. The separate API
diagnostic passes (`out/web10-v10-api-diagnostic.log`, 12 processes, exit 0).
The official stage nevertheless stops after 4,764.5 s in target-discovery
audit: `/c/.../dependencies/lib` was incorrectly treated as a native Windows
root path. No SDK is published; the predecessor's complete inventory is
unchanged. The owned debugger records 244,314 processes and no access
violation/unhandled fault; this is not proof of a single cause for every
historical crash.

The audit now normalizes CRT drive spelling before canonical containment.
All 175 actual cache records pass, while other drives, sibling prefixes and
parent-directory escapes remain rejected (37 stage tests pass). This is a
post-build audit repair only. A new local development asset, SHA-256
`e753684674b9f624b9457959b354601a670ea056adfaca0666fa345b12179e8a`
(`assets-pal-v11`), passes the existing packaging-retry verifier against
the v10 archive: all 190 compile inputs and the compile-driver AST identity
are unchanged. Proof: `out/tranche10-windows/packaging-retry-pal-v11.json`.
The official bootstrap resumes with `--reuse-work-root` and that exact
`--previous-source-asset`; mandatory recomposition/reconfiguration and clean
consumer rebuilds are retained. The new output is
`out/tranche10-windows/sdk06-pal-v11`, with active command/state
`run-pal-v11.json` and log `out/web-windows-tranche10-pal-v11.log`.
Final packaged and product-hidden relocated acceptance remain pending.

The v11 continuation completes the mandatory 1,368-step recompilation,
target audit, C API test and clean browser/GPU-worker build/install. It then
stops before publication at the first WebKit notice hash mismatch. This is
Windows checkout CRLF conversion: all 64 notices verify against their pinned
SHA-256, with 22 requiring CRLF-to-LF conversion. Packaging now writes the
exact hash-matching bytes without modifying the checkout; any content change,
bare-CR substitution or added whitespace remains rejected. Stage tests pass
38 cases. The v11 input inventory remains unchanged and no final SDK exists.

On 2026-10-20, the completed work SDK is used for an explicitly unpublished
packaging diagnostic with the repaired notice writer. Provenance/security,
browser smoke/role observation, real HTTP/TLS, Host ABI and distribution
verification all pass (`out/tranche10-windows/candidate-v12-diagnostic.json`,
`out/web10-candidate-diagnostic.log`). This diagnostic never calls
`publish_tree` and is not accepted as the official source-stage result.
The notice-only source-asset retry must still pass input-identity verification
and the official bootstrap before installed/relocated consumer acceptance.

The v12 development asset is pinned at SHA-256
`ba5c9c2fdf2cf9047b44f007d3abe9f45a2bbcb0739430bbb1df556a68cb5586`.
The v11-to-v12 proof again verifies all 190 compile inputs and compile-driver
identity (`packaging-retry-pal-v12.json`), with no compiler/dependency/target
source change. The official installed-SDK bootstrap uses the same t10w10
work root and exact v11 previous archive; command/state is
`out/tranche10-windows/run-pal-v12.json`, log is
`out/web-windows-tranche10-pal-v12.log`, intended output is
`out/tranche10-windows/sdk06-pal-v12`. This remains an uncommitted local
development snapshot, not a clean-f03626c/public release claim. The temporary
diagnostic candidate is retained as a checkpoint and rebuilt/revalidated by
the official path before any output is accepted.

The official v12 continuation finishes successfully on 2026-10-20 (exit 0,
1,799.4 s). Mandatory recomposition/reconfiguration and all 1,368 recompilation
steps complete, followed by C API replay, clean browser/GPU-worker consumer
build/install, provenance/security, installed browser observation, real
HTTP/TLS, Host ABI and `verify_dist.py`. The bootstrap publishes
`out/tranche10-windows/sdk06-pal-v12` only after those checks. Its manifest pins
the v12 source SHA above and predecessor content SHA-256
`4b72f594e4c046eac09543a17922dda3110835298b155f4563b1cc2bee4eb5ab`;
the complete 3,353-entry predecessor inventory remains unchanged. The owned
debugger observes 13,555 processes in this packaging continuation and exits
normally; this does not establish a common cause for every historical fault.
The separate non-debugger installed/space-path replay also passes, with log
`out/web-windows-tranche10-pal-v12-relocation.log` and evidence root
`out/tranche10-windows/replay-pal-v12`. Its external consumers were built before
hiding product work, so a second stricter replay builds them while work is
already hidden rather than inferring build isolation from runtime success.

That strict replay copies the SDK to the actual spaced path
`out/tranche10-windows/relocated SDK pal-v12-hidden-build` without a short-path
alias. It renames `C:/crtw/t10w10` to `C:/crtw/t10w10-relocation-hidden` before
fresh external CMake configure/build of `examples/web-browser` and compilation
of the public C API network consumer. Both use only the copied SDK toolchain,
headers and libraries. With product work still hidden, ordinary non-debugger
execution passes all three painted-browser checkpoints (installed, relocated,
external rebuilt), each observing HostProcess/WebProcess/NetworkProcess and no
foreign non-OS/non-SDK modules, runtime-family duplication or owned process
leftovers. The copied SDK passes all 10 HTTP/TLS cases, including redirect/POST/
cookie behavior and positive/negative certificate validation. Both SDKs pass
`verify_dist.py`, public export and native Host ABI audits.

Evidence: `out/tranche10-windows/replay-pal-v12-hidden-build/result.json`,
its per-browser observations, `network.json`, installed/relocated Host ABI
reports and `out/web-windows-tranche10-pal-v12-hidden-build.log`. The report
records `passed`, `product_work_hidden`,
`external_consumers_built_with_product_work_hidden`, `product_work_restored`,
`input_unchanged` and `inventory_unchanged` as true. All 3,603 SDK byte/alias
entries match before/after and across the relocated copy (inventory SHA-256
`15bc222fbdb9915fcf8e08b608edcb5c51ee91b0ed5dd5858e04a8c14c83777a`);
the complete 3,353-entry predecessor inventory also remains unchanged. Product
work is restored and no observed browser/debugger processes remain.

This closes native Windows development SDK acceptance, not a public release or
cross-target build proof. OpenSSL/curl remain Windows static-only as recorded
above; no new GPU/hardware-media acceptance is claimed by the CPU browser
observer. The historical heap fault did not recur in this replay, which is not
proof that all historical failures shared one cause.

Current local logs use `out/web-windows-tranche10-*`; failed predecessor,
missing import-path and host-configure diagnostics remain separate from final
acceptance. `TODO.md` retains only the remaining Tranche 10 gates; `STATUS.md`
is unchanged.

#### Linux aarch64 cross-build replay (2026-10-20)

This replay starts from `c5fea84` plus the local development changes, on a
Linux/x86_64 build host with Clang/LLD 21.1.8. It is **compile/package proof**,
not native aarch64 browser, HTTP/TLS, GPU, audio or camera execution.

**Predecessor preparation.** No aarch64 SDK was present on this host. Built
target compiler-rt from the same pinned AOSP LLVM revision used by the imported
C++ runtimes (`37f38d1f3276b62fba09462ab4807dce846c732d`), using CRT C headers
and Clang intrinsic headers only. The new Linux/aarch64 CMake toolchain builds
libc/libm/libdl/startup/shell; the imported LLVM driver builds the target
libunwind/libc++abi/libc++ static/shared pair. Then cross-build the pinned
FreeType, FFmpeg, curl/mbedTLS/zlib, xkbcommon and Skia inputs. Wayland uses the
separately built native, pinned 1.26.0 scanner, while client/protocol objects
are target code. The standalone 03/04/05 compile/install projects provide the
installed option-ON UI predecessor. They do **not** run their native tests on
the x86_64 host, and this is not a clean native 01..05 acceptance replay.

`out/tranche10-cross/sdk05` passes `verify_dist.py --stage 05-ui`. Its
**119 ELF files and all members of 28 static archives are AArch64**, with no
foreign object. The 3,271-entry predecessor content inventory SHA-256 is
`6262b4bca5ed1107c1387f08a3ac6f06278ab5aeeefdd7c5881bef73d5cc73fc`.
Preparation logs are `out/tranche10-cross/{builtins-*,bootstrap-*,libcxx-*,
skia-build,xkbcommon-build,wayland-*,stage03-v2,stage04-v2,stage05,package05-final,
verify05}.log`; the preflight inventory is independent of the later Web build.

Only explicit **target OS** graphics imports are used: Vulkan loader
1.4.341.0-1, Wayland client 1.24.0-2, VA 2.23.0-1ubuntu1 and DRM 2.4.131-1,
plus target VA development/kernel-UAPI headers (linux-libc-dev 7.0.0-14.14).
The arm64 `.deb` bytes are checked against the Ubuntu Resolute Packages
indexes and the signed InRelease (Ubuntu archive keyring `gpgv` passes).
Their identities are in `out/tranche10-cross/target-system/imports.json`.
Packages are extracted only under that work directory: none is installed on
the build host, no glibc sysroot is exposed to CRT, and these OS libraries are
not redistributed in the SDK. VA/V4L2 headers cross only their existing narrow
PAL adapter through `CRT_LINUX_SDK_ROOT`.

**Boundary fixes exercised by preparation.** Linux port builds now choose the
build host's make instead of executing the SDK's aarch64 make. Installed
wrappers persist the target identity for direct/unactivated calls, and cross
activation does not put target shell/toybox/make on the host PATH. Imported
libc++ receives the target architecture and prefers its already-built ABI
library; pthread-in-libc capability is declared rather than accepting a
static-try-compile false positive and emitting `pthread` autolink records.
The same investigation found Clang's default `--print-libgcc-file-name`
returning GCC 15's `libgcc.a` on Linux. The core query now explicitly requests
compiler-rt, including the target triple for cross queries; the fresh native
regression selects `libclang_rt.builtins-x86_64.a`, not a renamed GCC archive.

Fresh Linux/x86_64 C-stage configure/build/CTest passes **139/139**. The shared
loader positive TLS/thread-local-destructor and unrelated missing-import
negative tests also pass. Focused packaging/runtime helper regressions pass
**121 cases, with three expected skips** (118 executed passes), recorded in
`out/tranche10-cross/{native-configure,native-build,native-ctest,native-loader,
focused-regressions}.log`. `STATUS.md` is unchanged.

**Isolated `06-web` result.** The official isolated stage consumed the
`sdk05` predecessor (predecessor inventory SHA-256 prefix `204130e7…`) and the
pinned 06 source asset (`03abe775…1c46d1`) on the same Linux/x86_64 host and
completed the full 8,305-step WebKit build, then passed `verify_dist.py --stage
06-web`. The output SDK (`out/tranche10-cross/sdk06`) has **162 ELF files, all
AArch64, and none x86-64**, 28 AArch64-only static archives, and
`lib/libcrtweb.so` is `ELF 64-bit ... ARM aarch64`.

`cross-build-evidence.json` (`passed: true`) records the split: build host
Linux/x86_64, target Linux/aarch64. Host-executed generators are separate from
target artifacts: gperf, ICU `genrb`/`icupkg`/`pkgdata` (built in
`build/06-web/host/`), and the system Perl 5.40.1, Python 3.14.4 and Ruby 3.3.8,
each recorded with path, SHA-256 and `x86_64` machine type. Target ICU, libc++,
JavaScriptCore, WebCore and WebKit are recorded with SHA-256 and are AArch64.
fontconfig's gperf-generated and Python-generated headers are regenerated in a
separate host directory (`host/fontconfig-proof`) and compared with the target
build's headers (match; generated-header SHA-256 `299726ec…`). The record states
`target_execution: not attempted on the build host`.

Limits: this proves the cross compile/link/package split only. No aarch64
process was run, so browser, HTTP/TLS, Host ABI maps, GPU, audio and camera
behavior on Linux/aarch64 stay **unverified** until a native aarch64 host
replays the `06-web` gates. Public release remains open.

#### WebKit 2.54.1 re-pin and Linux/x86_64 replay (2026-10-20)

The pin moved from 2.54.0 to the newest stable micro release, **2.54.1** (2026-10-06, commit
`537776966344c4982751692dd475c94124504271`, tree `60295dee…`, 462,083 entries). Provenance:

- The archive SHA-256 `0de8bdfb…` equals the release page, and its detached `.asc` verifies with the same
  key as before (Adrian Perez de Castro, `5AA3BC33…123B`; not web-of-trust certified). **The upstream tag
  `wpewebkit-2.54.1` is lightweight, so unlike 2.54.0 there is no tag signature**; `recipe.json` records
  `tag_signed: false`, `tag_object: null` and an `archive_signature` instead, and
  `tools/fetch_webkit_commit.py` accepts a lightweight tag that resolves to the pinned commit.
- `--correspondence` measured the tarball against the commit: 38,844 files, 34,419 identical, 130 identical
  after CRLF->LF, **0 differ**; the only archive-only files are the 4,294 generated API docs and `NEWS`.
- The per-file license scan of the new tree (69,575 files) differs from 2.54.0 by two BSD-2-Clause files;
  the 64 license/notice files are byte-identical. No new license decision was needed.
- `check_webkit_security.py`: 2.54.1 is not affected by any of the linked advisories and is the latest micro.
  Upstream's support window for the 2.54 series is still not documented; that stays a release blocker.

Of the 57 files our 23 patches modify, only `WebProcessProxy.cpp`, `WebPage.cpp` and `WebPage.h` changed
upstream. All 23 patches apply unchanged to 2.54.1 on all three OS patch chains (no new hunks, no fuzz
failures); only four `sha256_before`/`sha256_after` pairs in the manifest were re-measured (0010 and 0021).
CRT-owned code needed one adaptation: upstream renamed `wtf/linux/RealTimeThreads.h` to
`HighPriorityThreads.h` (`PlatformCRT.cmake`). The new WTF thread-scheduling code calls
`setpriority()`/`sched_setscheduler(SCHED_RESET_ON_FORK)`, which the CRT libc lacked, so `getpriority`,
`setpriority`, `PRIO_*`, `id_t` and `SCHED_RESET_ON_FORK` were added with Bionic's signatures (Linux:
real syscalls; macOS/Windows: the single priority 0, nonzero fails with `ENOTSUP`, like
`sched_setscheduler`). **macOS and Windows were not built or run for 2.54.1 or for these libc additions.**

Replay (Linux/x86_64, a fresh work root composed from the new pin, `build_webkit_crt_port.py --webkit
--run-tests`): all eleven gate entries pass -- WebCore graph, process, public API, API lifecycle (100 cycles),
render/input through the surface protocol, lifecycle and Host ABI, WebView through crtui, media playback,
camera capture and network trust. Findings on the way, none caused by 2.54.1:

- The in-tree SDK `dist/05-ui` now ships a shared mbedTLS `libcurl.so.4` and `libz.so.1` (since
  `18eeb3a`). The wrapper-generated RPATH lists the SDK first and beats `LD_LIBRARY_PATH`, so the Web
  processes loaded the mbedTLS curl, which has no FILE protocol, and every `file:` load hung (the 2.54.0
  build fails identically against the same SDK). The replay therefore ran with those shared files removed
  from the in-tree SDK, the pre-`18eeb3a` state; they were restored afterwards. The in-tree driver gate is
  **not green against an unmodified current in-tree SDK** until the driver controls the curl search order
  (the isolated SDK packaging puts the private closure first and is unaffected). Linking the port with a
  later `--enable-new-dtags` was tried and made the wrapper link differently, so it was not adopted.
- `crtweb_lifecycle_test` counted six extra descriptors between iterations 10 and 20 on this host (cgroup
  `memory.*`, `/proc/meminfo`, `/proc/zoneinfo`, opened lazily by WTF's memory-pressure monitor and then
  flat through iteration 100; 2.54.0 does the same). Its warm-up is now 20 iterations; the leak bound is
  unchanged.

#### WebKit 2.54.1 replay on macOS/arm64 (2026-10-20)

Native replay on macOS 27.0.1/Apple M1 Pro of the pin above, through the official isolated entrypoint: the
in-tree `05-ui` SDK was regenerated first (all five stage publishers rebuilt with the inherited-runtime
packaging and each `verify_dist.py` pass), copied to an immutable `predecessor05`, a `06-web` source asset and
recipe were generated from the clean tree (`tools/create_stage_source.py`, asset SHA-256
`ae5b4f3b88f5f06ee624bbeabc3f06f942e863d7dcbbbf52d2724b4d57581db9`), and `tools/crt-stage-build.py` built
`out/tranche10-macos-2541/sdk06` in a fresh work root with the product checkout fetched afresh at commit
`537776966344c4982751692dd475c94124504271`:

```sh
python3 tools/crt-stage-build.py --sdk-root out/tranche10-macos-2541/predecessor05 \
  --recipe out/tranche10-macos-2541/assets/06-web.json \
  --asset out/tranche10-macos-2541/assets/crt-tranche10-macos-2541-macos-06-web-source.tar.xz \
  --work-root /private/tmp/crtw10n --output "$PWD/out/tranche10-macos-2541/sdk06" --dependency-jobs 6
```

A **fresh 8,302-action** WebKit build completed with all 23 carried patches applied unchanged (the macOS chain
included) and the CRT-owned `HighPriorityThreads.h` adaptation; no new patch, hunk or macOS code was needed. The
SDK passes the stage's own gates: installed `crtweb_browser --smoke` (load/pixels/script), HTTP/TLS **10/10**, the
export check (**37** public `crtweb_*` exports, none foreign), strict static Host ABI and `verify_dist.py`; the native
`ps`/`vmmap` observer finds Host/Web/Network, one CRT C++ image per role, no violation and no owned process left
(4,631 mapped files, Apple shared-cache images recorded separately). The SDK manifest, `web-sbom.spdx.json` (30
packages) and `web-source-correspondence.json` name WebKit **2.54.1** and the five inherited-runtime components;
`web-security.json` records 50 advisories checked, `pin_is_latest_micro: true` and `public_release_allowed: false`.

Relocation: the SDK was copied with symlinks to `relocated SDK/06-web` (a space path) and its installed
`examples/web-browser` rebuilt in a fresh directory with only that SDK's toolchain; with the **entire** work root
`/private/tmp/crtw10n` renamed out of reach, the installed browser, the relocated browser and the external rebuild
each pass the observer (same 4,631 mappings, one C++ runtime per role, nothing left), `verify_dist.py` passes on both
SDKs, and no loader-path override is used. The work root was restored afterwards. The libc additions of the re-pin
build and behave on macOS as documented: `getpriority`/`setpriority(0)` succeed, a nonzero priority and
`SCHED_RESET_ON_FORK` fail with `ENOTSUP`; the new WTF thread-priority code is Linux-only, so nothing in the
Mach-O build calls it.

Native in-tree C-stage/Web `ctest` **201/201** (the GPU cases skip without their opt-in), the stage tests (45, three
skipped), runtime-notice, create-dist, licensing, source-closure and host-generator tests, and the patch gate (23
patches / 63 modified / 24 CRT-owned files) pass. *Not done here:* the in-tree `build_webkit_crt_port.py` gate was not
re-run against 2.54.1 on macOS (the isolated stage is the replay; the `libcurl.so.4` search-order problem recorded above
is an ELF RPATH issue and does not exist on this host, where the gate removes the loader path from the framework-linked
test and the Web stage links WebKit's private OpenSSL curl); Windows is not replayed; upstream's support window for the
2.54 series is still unconfirmed.

#### Inherited runtime notices (2026-10-20)

Every SDK stage redistributes libc/libm/libdl, the shell and (from `02-cxx`) the imported LLVM C++ runtime,
but only the ports had notices and SBOM entries. `third_party/crt-runtime/components.json` now declares
`crt-libc`, `mksh`, `toybox`, `onetrue-awk` (first stage `01-c`) and `llvm-runtimes` (`02-cxx`);
`tools/crt_runtime_notices.py` installs each component's notices under `share/licenses/<name>/`, writes
`share/crt/dependencies/<name>/provenance.json` and adds a `crt-runtime` entry to the manifest, and
`create_dist.py`/`verify_dist.py` apply and require it for every stage. The `06-web` SBOM lists them as
inherited packages and carries a declared license only where the whole component has one verified text
(`Apache-2.0 WITH LLVM-exception` for the LLVM runtimes, whose license texts are vendored byte for byte from
the pinned revision; `0BSD` for toybox); `crt-libc`, `mksh` and onetrue-awk stay `NOASSERTION`.

`crt-libc/NOTICE` is generated from the in-file headers: 36 distinct license texts cover 139 files; the
other 290 files in the scanned trees have no header and are project-owned under `LICENSE.md`. Cross-checking
against `third_party/bionic/import_manifest.json` leaves five items that need a human decision and are
recorded in the component provenance: four imports that carry no in-file header (`gdtoa_fltrnds.h`,
`s_signgam.c`, `s_llrint.c`, `s_llrintf.c`) and `gdtoa_support.c`, which the manifest called project-owned but
which carries an Android Apache-2.0 header. **Resolved by engineering review on 2026-10-20** (not legal sign-off),
by comparing each file with its upstream: the four header-less files are **byte-identical** to Bionic main at the
pinned `731631f3…` and to OpenBSD `master` / FreeBSD `main` (fetched 2026-10-20), and none has a header upstream
either. `gdtoa_fltrnds.h` is an 18-line include fragment of netlib gdtoa (the same block, comments included, is inline
in the headed `dtoa.c` and `strtod.c`) and is included only by `strtof.c`, so it is attributed to the Lucent notice
(NOTICE group 2); `s_llrint.c` and `s_llrintf.c` are six-line specializations that `#include` the BSD-2-Clause
`s_lrint.c` (group 4); `s_signgam.c` is three lines (two includes and `int signgam = 0;`) attributed to FreeBSD
msun, with the question whether such a file needs a license text at all left to legal review.
`gdtoa_support.c` is a **derivative of Bionic's Apache-2.0 `gdtoa_support.cpp`** (17 lines, two global mutexes; the
header is verbatim): it is now classified `bionic-main`/`adapted` in the import manifest, and carries a
modification notice as Apache-2.0 section 4(b) requires. The decisions live in
`third_party/bionic/header_decisions.json`, each bound to the SHA-256 of the reviewed file (a change reopens it);
`crt_runtime_notices.py` renders them in the NOTICE ("Imported files without an in-file license header") and records
them in the provenance (`review_findings` is empty, `review_decisions` lists the four). The LGPL
source offer for WebKit and FFmpeg is described in `docs/guides/lgpl_source_offer.md`.

*LGPL source offer, generated (2026-10-20).* `share/licenses/SOURCE-OFFER.md` is no longer written by hand:
`tools/crt_source_offer.py` renders it from `third_party/crt-runtime/SOURCE-OFFER.template.md`, the release-time values
in `third_party/crt-runtime/release.json` (issue tracker `https://github.com/webos21/crt/issues`, offer period 3 years,
lower values rejected) and the build's own recipes (FFmpeg 8.1.2 and WPE WebKit 2.54.1: upstream address, SHA-256, size,
the 23 carried patches). `create_dist.py` installs it in every stage that redistributes FFmpeg, `build_stage_06_web.py`
renders it again for `06-web` with WebKit, `verify_dist.py` rejects an SDK whose offer is missing, unresolved,
for another version or stage, or without a component's archive SHA-256, and `prepare_release_assets.py` adds the
release asset `crt-<tag>-corresponding-source.tar.xz` (the tracked tree of the release commit plus
`CORRESPONDING-SOURCE.json`; **upstream archives are not copied in**, by decision, so the offer text promises a copy on
request through the issue tracker for the offer period). Evidence on macOS/arm64: the in-tree `04-gfx-media` and
`05-ui` SDKs verify with the offer; a **fresh** isolated `05 -> 06` build (the incremental packaging retry was rightly
refused because the predecessor changed) produced `sdk06b` whose offer names both components, whose manifest lists it as a
notice of `ffmpeg` and `webkit`, and which passes `verify_dist.py`, HTTP/TLS 10/10 and the installed and relocated
(work root hidden) process observer. A rehearsal bundle of the current tree is 25 MB (2,143 members, the 23 patches and the recipes included). New tests: `test_crt_source_offer.py` (10), the release-tool cases (25). The
Windows and Linux/aarch64 SDKs have not been regenerated with the generator, and `prepare_release_assets.py` still
publishes only `01-c`..`04-gfx-media`. The `libclang_rt.builtins` archive is the
build toolchain's; its exact version is not pinned by the SDK.

This is an engineering inventory, not a legal review, and it was first exercised on Linux/x86_64. *macOS/arm64
(2026-10-20):* all five stage publishers were regenerated; every manifest lists `crt-libc`, `mksh`, `toybox` and
`onetrue-awk` from `01-c` and `llvm-runtimes` from `02-cxx`, `verify_dist.py` requires them and passes, and the
macOS `crt-libc/NOTICE` is the same inventory (139 files with a header, 290 project-owned) because the scan is
over the sources, not the host. The isolated `06-web` SDK carries them in its SPDX document with the same
declared-license policy (`Apache-2.0 WITH LLVM-exception`, `0BSD`, `NOASSERTION` for the rest). The Windows SDKs
were not rebuilt, and the aarch64 SDK was not regenerated with it. Source-offer text for the LGPL parts of the Web stage and a legal sign-off remain the
publication gate.

## Host order

Linux first (WPE is the reference), with an early three-host JavaScriptCore
replay; then Windows/x64; then macOS/arm64. Linux/aarch64 is not a default
replay host: it is replayed only when a change has assembly or other
architecture-specific code (policy and rationale:
[`runtime_roadmap.md`](../design/runtime_roadmap.md), Host Order), so later
tranches such as B4-B7 need not include it.
