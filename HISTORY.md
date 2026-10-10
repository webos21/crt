# HISTORY: CRT Resolved Work Log

Reverse-chronological record of completed work on the CRT shell/rootfs/porting
loop and PAL, one dated entry per resolved item (grouped by the commit(s) that
landed it). Current/open work lives in `TODO.md`. This file retains October
2026; earlier months are summarized in [docs/history](docs/history/README.md),
with a pinned Git source and recovery instructions for the complete original log. Dates are the git author date of the commit that introduced or last
substantively updated each entry, so an entry whose investigation spanned
multiple days is dated by its span (`start..resolved`) or by its last
substantive update.

## 2026-10-10

### Web Tranche 3A on Linux/x86_64: frame-producer and input contracts frozen, WPEPlatform `crt` display

A WPE display can be added from outside the WebKit tree: `WPEDisplay` is a GIO extension point
(`wpe-platform-display`, modules scanned from `WPE_PLATFORMS_PATH`). `libcrtweb/platform/wpe-crt` is
such a module; with `WPE_DISPLAY=crt` the unmodified WPE 2.54.0 reference build renders into it, and
no WebKit file changed. Each committed shared-memory buffer is copied into a producer-owned pool of
three memfds and announced over an `AF_UNIX` `SOCK_SEQPACKET` connection defined by
`libcrtweb/platform/crtweb_surface_wire.h` (contract v1: pixel format, ownership, serials, damage,
acknowledgement-paced single frame in flight, latest-wins coalescing with a 250 ms bounded wait,
resize, shutdown; pointer/wheel/key/text/focus with XKB keysym + keycode). The consumer adapter
`libcrtweb/platform/crtweb_surface_client.h` exposes the newest frame as a Skia image for a `crtui`
SurfaceView and maps `crtui` input to the wire.

Verification: `tools/build_webkit_wpe_crt.py` runs the native host against a CRT-built probe (frames
and pixels, serials, ack/release, pointer -> click and its pixel effect, key code/modifiers, wheel,
committed text, resize, and a stalled consumer that holds all buffers) 4/4 passes; the hermetic
`crtweb_web_surface_test` (CRT-built, Skia raster target, fake producer) and the new
`socketpair_test` pass in the full serial `ctest` (172/172, sound-card test excluded).

Findings, all fixed in CRT rather than worked around: `wpe_buffer_import_to_pixels` is
(transfer none), so unreffing it crashed after a few frames (our bug); WebKit never pushes the
document title to the toplevel (the embedder does); WebKit takes XKB keycodes (evdev + 8) and drops
key events with keycode 0, so the wire carries the XKB keycode and committed text maps ASCII through a
US layout; and the CRT libc had no `SOCK_SEQPACKET`/`MSG_TRUNC`/`MSG_CTRUNC`/`SOCK_RDM` and no
`socketpair()` (Linux and macOS syscalls added, ENOSYS on Windows). `crtweb_` joins the test-name
prefixes the C-stage workflows exclude. Open: 3B gates, the Windows/macOS transport, a live
crtgfx presentation of a web SurfaceView, GPU buffers. Not built here: the new `socketpair` syscall stubs for
Linux/aarch64, macOS (both) and the Windows ENOSYS stub (written from the neighbouring `socket` stubs; the Linux/x86_64
build and tests are the evidence).

### Documentation consolidation: adopted policy replaces exploratory drafts

Removed 13 marketing/study/restructuring drafts after comparing them with the
current contracts. Their source-portability and embedded-development direction
is owned by `docs/design/project_meanings.md`; stage separation and SDK usage
by `runtime_roadmap.md` and `docs/guides/distribution.md`; graphics references and
native host boundaries by `libcrtgfx_wayland_plan.md`, the graphics API policy,
and the Host ABI firewall. The English/Korean marketing copies no longer form
a parallel product definition.

Superseded QuickJS/Ozone/desktop plans and unsupported claims about foreign
binary reuse, certification, exact board simulation, driver-crash isolation,
resource limits, and pixel identity were not promoted into policy. The draft
`crt-clang`/`.cfg` launcher proposal was not an implemented SDK interface and
was not retained as a promised feature. Existing SDK wrappers and package
contracts remain authoritative.

Merged the separate loader plan into `docs/design/dynamic_loading.md`, retaining
compatibility levels, module ownership, selection criteria, and the staged ELF
loader proposal. Shared-artifact policy remains separate and now acknowledges
the existing Linux `CRT_1.0` version scripts. Replaced roadmap completion diaries
with acceptance links, removed obsolete bootstrap/Android-target wording from
project scope, corrected the FAQ's widget description, and consolidated the
shell guides' installed-layout explanation. Initial hello and Windows fork
investigation records retain their distinct reproduction value.

The removed drafts and original loader plan remain recoverable from commit
`4e5eead68048723c37e46c22d80bca43915ac093` using `git show <commit>:<path>`;
their original paths are under `docs/marketing`, `docs/study`, `docs/refine`,
and `docs/linker_loader.md`. This is archival access, not active documentation.
Validation checked repository Markdown destinations and section anchors, deleted
path references, and whitespace. No runtime behavior changed.

### Web Tranche 2 closed: native Linux WPE reference baseline

Built the verified, pristine WPE WebKit 2.54.0 release on the physical Ubuntu
26.04.1/x86_64 host with GCC 15.2.0, CMake 4.2.3 and Ninja 1.13.2. The new
`tools/build_webkit_wpe_reference.py` verifies the archive and every upstream
`sha256_before` named by the CRT patch manifest, configures the WPE port with
legacy/DRM-display/Wayland backends off and the built-in headless backend on,
builds MiniBrowser plus its process/resources/bundle closure, and installs it
under a private work prefix. Native development packages were unpacked only
under `out/web-reference/sysroot`; the host installation was not changed, and
this is deliberately not a CRT SDK build.

The repository-owned C11 harness and local HTML fixture explicitly create and
connect the headless display, construct a WebKit web view for it, resize to
640x480, and exercise HTML, CSS, DOM, JavaScript and canvas. The real
multi-process run returned the exact proof
`CRT WPE reference|local-html-ok|42|192x96`, a 640x480/stride-2560 BGRA8888 image with
multiple colors, and FNV-1a `da2864e59e05c36f`. The resulting PPM SHA-256 is
`efef2b53b70d6882cda4ad00d4bccfa12c2c11d1cc65bfd98ebb250d980a51c3`.
The pixel hash is recorded as local evidence, not made a portable gate.

The bring-up also resolved three reference-configuration traps without patching
upstream. Disabling Video while leaving WebCodecs on produces an incomplete
`VideoFrame`; disabling both still leaves the WPE build's generated
`JSHTMLMediaElement` custom binding inconsistent. The accepted build therefore
keeps their supported upstream relationship. Disabling `USE_LIBDRM` also hides
`drm_fourcc.h` while the snapshot backing-store path still uses
`DRM_FORMAT_XRGB8888`, so libdrm stays enabled even though the DRM display backend
is off. Finally, Release builds ignore `WEBKIT_EXEC_PATH`; using a private CMake
install prefix supplies the compiled-in Web/Network process paths without a
system install. No WebKit source file, CRT runtime code or carried patch changed.

## 2026-10-09

### Web Tranche 1 closed with the Linux/aarch64 1D-B/C/D replay

Regenerated and verified the cumulative `05-ui` SDK from `a8a20fa`, then built
fresh Baseline-JIT and sampling-profiler JSCOnly trees from the verified WPE
WebKit 2.54.0 pin on Ubuntu 26.04/aarch64 with Clang 21.1.8. The ordinary
Baseline-JIT tree passed every 1B..1D-C gate. FTL/B3 produced 158 reports at the
reduced threshold, 108 at default thresholds and 252 with serial compilation;
the FTL-off negative control produced zero. WebAssembly passed interpreter-only,
BBQ, BBQ+OMG and software-bounds configurations with 21 BBQ and 7 OMG reports,
and its C API signal probe measured 50 fast-memory signal deliveries versus zero
with explicit bounds checks. The 0/1/4/8-thread context cycles, JS and Wasm
watchdogs, RSS bounds and ELF host-ABI audit all passed.

The separate `ENABLE_SAMPLING_PROFILER=ON` build reran the complete 1B..1D-C
sequence before its profiler gate. It again proved FTL (157/108/252 reports),
Wasm BBQ/OMG (21/8 reports) and the 50/0 fast-memory signal distinction. The
official profiler run sampled the main VM and all four worker VMs with 5/5
profiles, 297 traces and 296 named hot frames. Twenty focused repetitions passed
20/20 with every VM sampled; the minima were 274 traces and 271 named hot
frames. The final host-ABI audit was clean, full Linux/aarch64 CTest passed
148/148, the JSC harness tests passed 3/3, and distribution verification passed
for the cumulative 03/04/05 stages. No runtime, WebKit patch or harness change
was required. This was the last native 1D-B/C/D replay, so JavaScriptCore
Tranche 1 is closed on all four acceptance hosts; W^X, SIMD, Wasm threads and
Windows/arm64 JSC remain separate non-gating follow-ups.

### Windows emulated-TLS churn is bounded and Tranche 1 follow-ups are classified

Added `emutls_thread_churn_test`, a bounded Windows/x64 regression that runs a
warm-up and two sequential batches of 128 short-lived pthreads, each touching a
256 KiB compiler-lowered `__thread` object, while recording process private
bytes. The pre-fix implementation retained every terminated thread's emutls
array and objects: the second batch added 42,024,960 bytes and total post-warm-up
growth was 84,049,920 bytes. `libc.dll` now keeps its process-wide emutls key but
frees only the exiting CRT pthread's object slots and array, after pthread-key
destructors, on both normal return and explicit `pthread_exit()`. This does not
restore the unsafe process-exit teardown. After the fix, both batch and total
growth are zero; the churn regression passes 20/20 repetitions, the churn,
process-exit-race and multi-DLL emutls tests pass 3/3, and the full Windows/x64
CTest passes 165/165.

Closed the non-implementation Tranche 1 follow-ups at the same boundary. Host-ABI
audits are complete on Linux/x86_64, macOS/arm64 and Windows/x64. The tested 1 MiB
pthread default is now explicit policy: CRT keeps the default and an embedder
selects a larger stack for workloads that require it. The isolated `06-web`
stage chain and the full build-host/target split are Tranche 10 work. The fixed
roles are host gperf/Perl/Python/Ruby/ICU generators versus target ICU/libc++/JSC/
WebKit, with an x86_64-host -> Linux/aarch64-target proof deferred to that stage.

### Web Tranche 1D-B/C/D accepted on macOS/arm64

Completed the missing native macOS replay with fresh Baseline-JIT and sampling-profiler
JSCOnly builds against the regenerated, verified `05-ui` SDK. FTL/B3 passed at reduced,
default and serial thresholds (153, 107 and 252 FTL reports; zero in the negative control).
WebAssembly passed interpreter, BBQ, OMG and software-bounds modes (21 BBQ and 5 OMG reports),
and its C API probe measured 50 fast-memory signal deliveries versus zero with explicit bounds
checks. Context cycles on 0/1/4/8 threads, JS/Wasm watchdog termination and the Mach-O host-ABI
audit were green. The separate profiler build reran all earlier gates, then sampled the main VM
and four worker VMs with 5/5 profiles and 1,167 named traces/hot frames. Twenty focused profiler
repetitions passed 20/20, with a minimum of 1,156 named frames.

The FTL replay exposed a persona-versus-object-ABI mismatch: CRT intentionally presents macOS
to WebKit as Linux-shaped source, but Darwin arm64 reserves x18 while WebKit's Linux register
table allocates it. Under B3/Air pressure generated code reused x18 across a runtime call and
faulted near `operationStringFromCharCode`. Carried patch
`0004-darwin-arm64-reserved-x18`, with verified before/after hashes and a removal condition,
selects WebKit's existing reserved-x18 table only for macOS/arm64; it does not enable Apple SDK
source paths or change Linux/aarch64.

The profiler replay exposed a macOS `pthread_create()` publication race analogous to the
Windows/x64 finding. A target wrapper learns its kernel-allocated stack from Apple's pthread API,
but the creator could return a Bionic-shaped `pthread_t` before those bounds were stored. An
immediate WebKit `pthread_getattr_np()` then derived stack origin `0x100000` and crashed in
`sanitizeStackForVM`. The target now release-publishes the bounds and wakes the creator before
the thread id is returned; the detached lifetime handshake is shared by macOS and Windows.
The three focused pthread regressions each pass 100/100, the JSC harness unit tests pass 3/3,
and full macOS CTest passes 125/125. Linux/aarch64 is now the only remaining 1D-B/C/D native
replay.

## 2026-10-08

### Web Tranche 1D-D: JavaScriptCore sampling profiler accepted on Windows/x64

Rebuilt the pinned JSCOnly source against the installed Windows/x64 `05-ui` SDK with
`ENABLE_SAMPLING_PROFILER=ON`. The separate profiler build passed every 1B..1D-C gate
before its profiler-specific gate, including DFG/FTL, Wasm fast-memory faults and
watchdog VM traps. Its official profiler step sampled the main VM and all four
`$262.agent` worker VMs: **5/5 profiles, 153 traces and 153 named hot-function frames**.
The aggregate acceptance and PE host-ABI audit were clean. Twenty further isolated
profiler runs passed 20/20, always sampled all five VMs, and produced at least 152
traces and 152 named hot frames.

The first worker-profiler run exposed a real libc/PAL prerequisite rather than a JSC
profiler defect. On Windows, `pthread_create()` could return before its target wrapper
had called `GetCurrentThreadStackLimits()` and recorded the stack in the pthread control
block. WebKit immediately calls `pthread_getattr_np()` from the creator; it could
therefore observe `{ base = NULL, size = 1 MiB }`, manufacture `0x100000` as the stack
origin, and crash the worker in `sanitizeStackForVMImpl`. The Windows start wrapper now
release-publishes the real bounds and wakes the creator, while `pthread_create()` waits
for that publication before returning the `pthread_t`. A second creator-publication
handshake keeps a detached thread's control block alive until the creator has finished
publishing its id. `pthread_stack_bounds_test`
locks in the contract by querying a live worker immediately after creation and requiring
a non-null, plausible stack range. The final source passes the full Windows CTest 164/164,
the focused pthread/signal/TLS subset 7/7, the stack-bound regression 100/100 and the JSC
harness unit tests 2/2; the detached-thread regression also passes 100/100. The cumulative
`05-ui` distribution was regenerated from that source and every stage passed `verify_dist`;
its direct profiler replay passed again with 5/5 VMs, 138 traces and 138 named hot frames.

One redundant post-package full-suite replay was stopped after its second Wasm acceptance
process exceeded the normal duration before reaching the profiler. The official complete
1B..1D-D run, the 20/20 profiler repetitions and the final packaged-SDK direct profiler run
remain green; treat this as a 1D-C observation and reopen that gate only if it reproduces.
This closes 1D-D on Windows/x64; Linux/aarch64 and macOS/arm64 native replays remain.

### Web Tranche 1D-D: JavaScriptCore sampling profiler accepted on Linux/x86_64

Added a distinct `sampling-profiler` mode to `tools/build_webkit_jsc.py`. It builds
the pinned JSCOnly source from the installed and verified `05-ui` SDK with
`ENABLE_SAMPLING_PROFILER=ON`, while retaining Baseline JIT, DFG, FTL and WebAssembly,
and reruns the complete 1B..1D-C acceptance before its profiler gate. Interpreter and
ordinary Baseline-JIT modes continue to compile the profiler out, so their existing
configuration and evidence remain isolated.

The new `jsc_sampling_profiler_acceptance.js` validates JSC's platform-support result,
profile interval, non-empty traces and named hot-function frames. It exercises the main
VM plus four concurrent `$262.agent` worker VMs, each with an independently started
profiler. The official run produced 5/5 valid profiles, 1,320 traces and 1,320 named hot
frames; its host-ABI audit was clean. Twenty further runs passed 20/20 with all five VMs
sampled every time and minima of 1,288 traces and 1,288 hot frames. The relevant rebuilt
signal/TLS CTest subset passes 9/9 and the tool suite passes 104/104. Tranche 1D-D is
closed on Linux/x86_64. Full in-tree CTest is 169/170; the sole failure is the known,
pre-existing no-sound-card `crtmedia_playback_pipeline_test_runs` pacing check, unrelated
to Web/JSC. This run also closes the installed-`05-ui` SDK half of the packaging follow-up.
ICU/libc++abi's loader-provided global-dynamic TLS is recorded as the accepted current loader
boundary rather than an active Tranche 1 gap; it becomes work when CRT owns the loader. Native
Linux/aarch64, macOS/arm64 and Windows/x64 replays remain.

### Windows CI: architecture-correct VM-trap coverage and scheduler-safe sleep assertion

Repaired the two Windows failures in GitHub Actions run 37707937441. The
`signal_vmtrap_test` source included the x86_64-only `hlt` scenario in its
Windows/arm64 build: its handler advanced `uc_mcontext.gregs[REG_RIP]` and its
inline assembly emitted the one-byte x86 instruction. The complete `hlt`
scenario and its default test-list entry are now x86_64-only. Windows/arm64
continues to exercise its actual JSC trap shape through the existing AArch64
`brk #0xf000` test, including Linux-layout `pc` rewriting, as well as the common
worker, initial-thread, concurrent and blocked-wait signal tests. The source
compiles cleanly with the CI target and flags (`aarch64-w64-mingw32`,
`-femulated-tls`, `-ffixed-x18`, `-Werror`).

The Windows/x64 job's lone `bionic_surface_test` failure was not reproducible
in 200 isolated runs. Its sleep check nevertheless imposed a false two-second
upper bound on `usleep(20000)`: POSIX guarantees that the call does not return
before the requested interval, but an oversubscribed CI runner may resume it
arbitrarily later. The regression now checks the real contract (at least 19
ms, retaining the existing one-millisecond timer tolerance) instead of treating
scheduler delay as a libc failure. The rebuilt focused Windows/x64 signal and
Bionic-surface tests pass, the latter passes 200/200 isolated repetitions, and
a fresh configure/build/test workflow passes all 143 Windows/x64 CI tests.

### Windows emulated TLS is owned once by libc across DLL boundaries

Closed the follow-up exposed by the Windows JSC 1D-C exit-race fix. The first
project-owned `__emutls_get_address` stopped compiler-rt's unsafe process-exit
teardown, but JSC, graphics and tests still compiled that source into each
consumer. PE/COFF does not coalesce the runtime's counter, Win32 TLS key or
per-thread slot array, so a control object shared by two DLLs could have one
index but two different values.

`libc/src/arch/windows/common/emutls.c` is now a normal source of both `libc.a`
and `libc.dll`; it retains the no-process-exit-teardown rule and advances the
global high-water mark when it observes an already-numbered control. Windows
shared-library links through `crt-cc` now import libc/libm/libdl instead of
embedding their static archives, and both wrappers plus the common CMake DLL
policy prevent a consumer from auto-exporting libc's emutls import thunk. The
Windows compiler-rt archive staged by the project is a copied archive with only
`emutls.c.obj` removed, so archive ordering cannot silently restore a private
runtime while all other builtins remain available. JSC, in-tree/isolated Skia,
the 05-ui predecessor contract and the standalone gfx-skia example no longer
carry an emutls source/object/archive or the old duplicate-definition bypass.

The new `emutls_multi_module_test` builds DLL A and DLL B against `libc.dll`.
A exports a control object; writes through either DLL are visible through the
other on the same thread, a new pthread starts from the template and cannot
overwrite the main thread, two compiler-lowered `__thread` values are isolated,
and the shared plus four private controls receive five distinct nonzero indexes.
PE inspection confirms `libc.dll` exports `__emutls_get_address`, both fixture
DLLs import it from `libc.dll` without re-exporting it, and the staged filtered
builtins archive no longer defines it. `pthread_native_tls_test`, the 100-child
`emutls_exit_race_test` and the new test pass; the stage closure/dependency unit
tests pass 14/14; the full Windows build and CTest pass 143/143. Per-thread
allocation reclamation under high thread churn remains a separately measured
follow-up; process-exit teardown must not return.

## 2026-10-07

### Windows/x64: silent exit 134 at process exit was compiler-rt's emulated-TLS teardown (2026-10-08)

The last Windows 1D-C symptom: 4-8% of runs of even a 0.3 s Wasm script (the first group of the acceptance script, dump options on or
off, only with concurrent JIT) ended with exit status 134, output cut in mid-line, or the `Invalid value for lock: 0` message. Found by
bisecting options (concurrent JIT off: 0/80; Wasm off: still failing; Baseline-only: 0/80; DFG on: 9/80), a 0.3 s reproducer, and
instrumenting `abort()`/`_exit()` (their callers): 14 of 15 failures happened after the script had printed its last line, and
`abort()` was called from `__emutls_get_address` -- compiler-rt's win_abort() after `TlsGetValue` failed with
ERROR_INVALID_PARAMETER ("The parameter is incorrect"; the CRT's link stubs for `__acrt_iob_func`/`__stdio_common_vfprintf` abort silently
before emutls can print that). compiler-rt's emutls registers an `atexit()` handler that frees its TLS index and mutex. `exit()` runs
atexit handlers while other threads still run (only the final `ExitProcess` stops them), so a compiler thread touching a thread_local in
that window used a freed index. The project now owns `__emutls_get_address` (`libc/src/arch/windows/common/emutls_link_stubs.c`, compiled into
JavaScriptCore and the graphics consumers): same control-object layout and per-thread array, state initialised on first use whatever
control object comes first (compiler-rt's fast path skipped that for an already numbered one) and no teardown at exit. New test
`emutls_exit_race_test` (spawns itself 100 times; threads spin on a thread_local while the main thread calls `exit(0)`; plus per-thread
semantics and a pre-numbered control object). After the change: 300/300 and 100/100 runs of the reproducer (about 7% before), the
60-run BBQ acceptance script with the dump options, 40 BBQ+OMG, 40 more BBQ, the official `baseline-jit` acceptance (131 s,
passed) and full CTest 163/163. Not changed: libc/libc++ and other modules that link compiler-rt's own emutls still have its atexit
teardown, and a control object shared between two modules is numbered by each module's own counter (per-module arrays); moving
`__emutls_get_address` into libc so there is one state per process is the follow-up in `TODO.md`.

### Windows/x64: signal-state leaks behind the `instance-lifecycle` hang (2026-10-07)

Two more defects of the redirect protocol, found by chasing the test-order dependence and the 120 s hang under the BBQ
dump options (about 7% of runs).
1. **Signal mask leak.** The sender wrote the handler's mask into the stopped target before redirecting it. A target that
   was taking a fault at that moment ran the fault handler with the changed mask, saved it as its own and restored it, so
   the mask (SIGUSR1) stayed blocked for good. Showed up as `fault,hlt`/`fault,worker` failing 40-60% when the fault-storm test ran
   first (and as the "leaked main-thread mask 512" seen earlier). The target now sets and restores its own mask after it has
   claimed the interruption; the sender no longer touches it.
2. **Lost acknowledgement.** The claim word lived in the frame on the target's stack. A thread whose redirect did not take
   effect carried on using that stack and overwrote the frame (the pump's captured claim was `746351308`), so the sender's
   cancel CAS failed and it waited for an acknowledgement for ever while holding the registry lock: exiting threads piled up
   in `registry_remove` and `WTF::Thread::suspend` waited for its answer (all-thread stack captured with lldb). The handshake is
   now one word in `crt_signal_state` (`inject_word` = serial * 4 + phase: waiting, claimed, cancelled, running) that
   survives the stack being reused; a stale stub finds a different serial and only resumes the interrupted context.
`signal_vmtrap_test` can now run tests in a chosen order (`SIGNAL_VMTRAP_ONLY=fault,worker`) and re-run them; the fault storm
runs first by default, with a mask postcondition. After the fix: all order pairs pass 12/12, 0 hangs in 150 runs
(BBQ-only 110, BBQ+OMG 40; 3-4 of 40 before), full CTest 158/158. Still open: a JSC `Invalid value for lock: 0` abort
(`WTF::LockAlgorithm::unlockSlow` on `JITThunks::m_lock`, in a Baseline-JIT compiler thread, first group of the script) in
about 4% of runs with the dump options and none without them. `signal_sync_stress_test` (mutex/cond under signals) and a flags-survival
check (`lock cmpxchg` + `jne` interrupted) both pass, so the CRT's own synchronisation is not the evident cause.

### Windows/x64: BBQ-only SIGSEGV at a JIT address root-caused and fixed

Reproduced with `jsc_wasm_acceptance.js` under the BBQ-only options (about 10% of runs, always in `memory-and-traps`, 0.5 s in).
A temporary log in JSC's Wasm `trapHandler` showed it returning `NotHandled` because the ucontext it received had its pc at
`__crt_windows_signal_entry`, not at the faulting load. A thread suspend request had redirected the thread (SetThreadContext)
after the kernel had built the exception record (pc = JIT address) but before it captured the context, so the VEH saw a
record with the JIT pc and a context already pointing at the stub. `exception_handler` only recognised the record carrying
the stub address; it now also checks the context pc, runs the injected handler, restores the saved context and lets the
faulting instruction fault again (`libc/src/arch/windows/common/signal_backend.c`). A new regression test (`test_fault_while_signalled` in `signal_vmtrap_test`: 200000
`hlt` faults on one thread while another floods it with SIGUSR1) showed a second defect of the same class: the injected frame sat
128 bytes below the target's stack pointer, where the kernel builds the exception CONTEXT/record of a fault in flight, so the frame
(its claim word) could be overwritten and the sender waited for ever or the handler saw a wrong pc. The frame now sits 16 KiB
below the stack pointer (falling back to the old margin on a shallow stack). Before: the test hung or crashed every run; after: it
passes. After both fixes: 0 SIGSEGV in 70 BBQ-only runs (about 10% before), the signal tests pass 20 consecutive runs, full CTest
157/157, `jsc_watchdog_test` 5/5. The test runs last in `main` because, run first, a later worker-interruption round of
the same test intermittently never gets its handler to start with the 16 KiB placement; that order dependence is not understood
yet and is tracked in `TODO.md`. The temporary JSC logging was removed.
Not fixed here: JSC `Invalid value for lock: 0` aborts (about 5% of runs) and a rare `instance-lifecycle` hang under the dump options.


- **Web Tranche 1D-B (FTL) accepted on Windows/x64 and the signal/runtime part of the still-open 1D-C (WebAssembly)
  replay hardened.** The installed/verified `05-ui` SDK's JSCOnly binary passes FTL at
  reduced/default thresholds and with serial compilation plus the FTL-off negative control. A near-final full run also
  passed the Wasm interpreter/BBQ/OMG and software-bounds matrix, watchdog termination of JS and Wasm loops, context cycles
  and the host-ABI audit, but the final stress below prevents accepting 1D-C. Because Windows
  has no `strace`, new `jsc_wasm_signal_probe.cpp` runs the OOB probe through JavaScriptCore's C API and brackets it with
  CRT's signal-delivery generation: fast memory handles 50 hardware faults, bounds checks handle zero.
  The work mapped JSC's `hlt` VM trap (`STATUS_PRIVILEGED_INSTRUCTION`) to Linux-compatible `SIGSEGV`/`SI_KERNEL` with a
  null address, delivers signals held by a fault handler after its mask is restored, adds a pump for pending delivery when
  the target is repeatedly inside Windows code, and serializes injection/delivery per target. The final duplicate-delivery
  race was subtler: the sender polled a claim stored below the target's original stack pointer; after context restoration
  the target reused that memory, so the sender could see zero again, restore the pending bit and run one signal twice.
  A stable acknowledgement now lives on the sender's stack, while the target-frame claim is only cancellation arbitration.
  Restored strict upper bounds in `signal_vmtrap_test` caught the old bug in 19-20/20 runs; after the fix
  `signal_vmtrap_test`, `signal_threads_test` and `memcpy_atomicity_test` each pass 20 consecutive runs, full Windows CTest
  passes 157/157, and `crt-ui-dist`/`verify_dist` complete. One near-final aggregate JSC run had all FTL/Wasm steps green but a
  single earlier DFG 4-thread context-cycle process hit JSC's `Invalid value for lock: 0`; the same focused configuration
  then passed 10/10 and a second full official harness passed every step plus the aggregate/host-ABI gates (131 s).
  Final review additionally kept the sender stack alive until a just-claimed handler's acknowledgement store. The SDK
  rebuilt from that exact source exposed the remaining non-signal-closure result: its dump-based BBQ run hung, while
  bounded no-dump repetitions were BBQ-only 9/10 (one controlled JIT-address SIGSEGV, exit 139) and BBQ+OMG 10/10.
  Therefore Windows 1D-C remains in `TODO.md`; 1D-B and the resolved PAL work belong here. Temporary mask/ring tracing was
  removed. Open: isolate the Windows BBQ failure; replay 1D-B/C on macOS/arm64 and Linux/aarch64; then profiler 1D-D.

## 2026-10-06

- **Web Tranche 1D-B (FTL) and 1D-C (WebAssembly) on Linux/x86_64; two CRT bugs fixed that only concurrent Wasm tier-up
  exposed.** `tools/build_webkit_jsc.py` gained step 4 (`run_ftl_acceptance`) and step 5 (`run_wasm_acceptance`) on the same
  `baseline-jit` binary, each requiring evidence of its tier: FTL compile reports (108-252 per script run; zero with FTL off),
  Wasm interpreter-only / BBQ / BBQ+OMG separated by `useBBQJIT`/`useOMGJIT` and proven by the tier's disassembly dump, fast
  memory proven by `strace` counting the SIGSEGVs JSC's fault handler handled (50 vs 0 with software bounds checks), watchdog
  termination of a Wasm loop on main and worker threads. New `jsc_ftl_acceptance.js`, `jsc_wasm_acceptance.js` (raw-byte
  modules, nine trap kinds, 400 instances, async), `jsc_wasm_oob_probe.js`; `jsc_watchdog_test wasm`.
  The Wasm script first hung (~1 in 10 runs under 4-way CPU contention) and then crashed (~1 in 100). A hang's backtrace
  (gdb as the parent; ptrace attach is blocked) showed every thread in `crt_spin_lock(thread_lock)` of `libc/src/tls.c`: the
  registry lookup behind errno/`pthread_self` held a global lock, WTF parked the holder in its SIGUSR1 suspend handler and
  the resumer needed `pthread_self` for `pthread_kill`. The registry is now a lock-free-reader table keyed by tid (writers
  only take the lock). Crashes in JIT code (SIGSEGV/SIGILL) remained until the second bug: CRT's OpenBSD `memcpy` copied 4
  bytes as four byte stores, and WTF patches call displacements with `memcpy` while another thread executes them (BBQ->OMG
  call-site patching); glibc builds compile the same copy to one `mov`, the CRT wrapper's `-fno-builtin` does not. Copies up to
  16 bytes are now single overlapping 2/4/8-byte accesses, as in Bionic. A native glibc build of the same source ran 1,200
  runs without failure and the CRT runtime 0 in 800 after both fixes (before: ~1% crashes plus the hangs). New tests:
  `signal_threads_test` park/resume in a thread hot in errno (fails every run on the old registry) and
  `memcpy_atomicity_test` (over a million torn 4-byte values before, none after). Verified: full `ctest` 163/163 serial
  (the sound-card test excluded), tooling 102/102, `crt-ui-dist`/`verify_dist`, interpreter and baseline-jit harness passes
  from the installed `05-ui` SDK, stress FTL/Wasm/watchdog 100/100 each. Details and limits: `docs/acceptance/crtweb_acceptance.md`
  ("1D-B and 1D-C result"). Open: replays of 1D-B/C on the other hosts (needs the libc fixes), the sampling profiler (1D-D),
  SIMD and Wasm threads, and a Wasm loop with no call is not interruptible by the watchdog (upstream).

- **Windows/arm64: thread-directed signals, per-thread masks and fault-to-signal mapping (same gate as Windows/x64).**
  180e43c had written `libc/src/arch/windows/common/signal_backend.c` against the x64 `CONTEXT` and the Linux x86_64
  `ucontext_t`, which broke the windows-arm64 CI build (`greg_t`/`REG_R8` undeclared); first guarded to keep masks, waits and
  `sigsuspend` only, then completed. The file now carries both layouts: the ARM64 `CONTEXT` (912 bytes, x0-x30/sp/pc/v0-v31),
  an arm64 entry stub (`bl __crt_windows_signal_run`), and a Linux-aarch64 `ucontext_t` frame for the handler
  (`private/crt_linux_ucontext_aarch64.h`, with the FP/SIMD record), as the macOS backend does; the header's own
  `ucontext_t` stays private on Windows/arm64. `brk` -> SIGTRAP leaves the pc on the instruction (Linux behaviour, the handler
  steps over it); a thread parked in a CRT wait gets x19-x30/sp in its context. `signal_vmtrap_test` gained an arm64 path
  (`b .` loop, `brk #0xf000`) and `signal_threads_test` now runs on all Windows. Evidence: windows-arm64 CI at `41326fd`,
  135/135 tests, `signal_vmtrap_test` 4.88 s and `signal_threads_test` 1.80 s (really executed, not skipped). The JavaScriptCore
  replay on Windows/arm64 is not done.
- **Web Tranche 1D-A replayed on Windows/x64: DFG with concurrent compiler threads is green on the first run.** Same
  Windows Baseline-JIT binary as 1C with `JSC_useDFGJIT`/`JSC_useConcurrentJIT` and four compiler threads, polling traps
  off, so the Windows signal VM-trap gate now has its DFG consumer. The harness's step 3 passes in full (DFG script at
  jitPolicyScale 0.01, at default thresholds and with serial compilation, the 1B and JIT scripts under DFG, thread
  cycles, watchdog on a DFG-compiled loop, host-ABI audit, no FTL code); the DFG script and the watchdog ran 100/100 and the
  4/8-thread cycles 40/40 without a failure; the compiler threads are visible (15 threads against 12 with concurrent
  JIT off). Nothing in CRT, the patches or the harness had to change. Evidence: `docs/acceptance/crtweb_acceptance.md`.
- **Web Tranche 1D-A replayed on Linux/aarch64: DFG with concurrent compiler threads is green on the first run.**
  Ubuntu 26.04 aarch64 QEMU guest at `8e30763`: `cmake --fresh` of the old tree (24 commits had changed shared libc,
  `crt-c++` and the dist prerequisites), clean build with 0 warnings, `crt-ui-dist`/`verify_dist` for 03/04/05, tooling
  102/102, `ctest` 140/140 (the new `signal_vmtrap_test` skips on aarch64 and is not counted). `--mode baseline-jit` from an
  empty work root passes every 1B/1C step and all of step 3 (DFG at 0.01/default thresholds/serial with 123/108/137
  `using DFG` reports, 1B and JIT scripts under DFG, cycles on 0/1/4/8 threads, watchdog on a DFG-compiled `spin`, no FTL
  report, host-ABI audit clean); stress: DFG script 100/100, watchdog 100/100, 4/8-thread cycles 40/40; `useDFGJIT=false`
  gives zero reports; `JITWorker` threads exist in `/proc/<pid>/task` only with concurrent JIT on; interpreter mode
  unchanged and passing. No CRT or patch change was needed. Windows/x64 was the remaining 1D-A replay (done, see above).
- **Web Tranche 1D-A replayed on macOS/arm64: DFG with concurrent compiler threads is green on the first run.**
  Fresh `crt-ui-dist` and an empty-tree `--mode baseline-jit` build; all 1B/1C steps and step 3 (DFG at 0.01/default
  thresholds/serial, 1B and JIT scripts under DFG, context cycles on 0/1/4/8 threads, watchdog on DFG code) pass,
  host-ABI audit clean; stress DFG 100/100, watchdog 100/100, cycles 40/40; DFG-off gives zero reports and FTL none;
  compiler threads confirmed by `sample` (no `/proc` on macOS). No CRT or patch change was needed.

## 2026-10-05

- **Web Tranche 1D-A on Linux/x86_64: JavaScriptCore's DFG tier with concurrent compiler threads runs on the CRT runtime;
  the `libatomic.so.1` prerequisite is removed.** The 1C binary already contains DFG, so 1D-A is a run-time step in
  `tools/build_webkit_jsc.py` (`--mode baseline-jit`, "step 3"): `DFG_ON_OPTIONS` (DFG and concurrent JIT on, four compiler
  threads, FTL/WebAssembly off, polling traps off), the same at default thresholds and with serial compilation, the 1B/1C
  scripts under DFG, context cycles on 0/1/4/8 threads and the watchdog with a DFG-compiled `spin`; each step requires DFG
  compile reports and no FTL report. New `libcrtweb/tests/jsc/jsc_dfg_acceptance.js` (7 groups). Everything passed on the
  first run with no CRT change (DFG script 100/100, watchdog 100/100, 4/8-thread cycles 40/40; 108-137 DFG reports per
  script; the report count is zero with DFG off; `JITWorker` threads present with concurrent JIT on and absent without it).
  Evidence and limits: `docs/acceptance/crtweb_acceptance.md` ("1D-A result"). Replays on aarch64/macOS/Windows are open.
  Also: `libatomic.so.1` dropped from `tools/crt_dist_prerequisites.py`, `docs/guides/distribution.md` and `docs/guides/release_preview.md`
  (a fresh `02-cxx`..`05-ui` SDK has no libatomic `DT_NEEDED` on Linux/x86_64 and the aarch64 replay's host-ABI audit
  was clean; `crt-ui-dist`, `verify_dist` and the 102 tooling tests pass). `TODO.md` lost its stale Windows
  software-signal-mask text and now points 1D-A's replays at Linux/aarch64, macOS/arm64, then Windows/x64.


- **Windows/x64: thread-directed signals, per-thread masks and a real `sigsuspend`; JavaScriptCore's Baseline JIT now runs
  with `JSC_usePollingTraps=false` (Web Tranche 1, signal VM-trap gate).** Windows has no kernel signal delivery, so the CRT
  builds it in the PAL (`libc/src/arch/windows/common/signal_backend.c`), not in WebKit. Each thread owns a mask/pending
  set/wake event (`crt_signal_state` inside `crt_thread_context`); `pthread_kill` to a CRT thread (worker or initial)
  suspends the target, captures its CONTEXT, builds a signal frame (saved CONTEXT, Linux-x86_64-layout `ucontext_t`,
  `siginfo_t`) on its stack, puts the handler's mask in place while it cannot run, redirects it to a stub that runs the
  handler and then restores the (possibly rewritten) context with `RtlRestoreContext`. Hardware faults become signals through a
  vectored handler (`int3`->SIGTRAP with RIP after the instruction, AV->SIGSEGV, illegal instruction->SIGILL, divide->SIGFPE).
  `sigaction` honours `sa_mask`, `SA_NODEFER`, `SA_RESETHAND`; `pthread_sigmask`/`sigprocmask` are per-thread and a new
  thread inherits its creator's; `sigsuspend` blocks atomically and returns EINTR after the handler. `ucontext_t` is the
  Linux x86_64 layout on Windows too (JSC reads and rewrites `REG_RIP`); `getcontext`/`makecontext` keep the Windows-only
  state (xmm6-15, TEB stack bounds) in unused slots. A thread parked in operating-system code is never interrupted (loader/heap
  locks, mid-syscall contexts), so the CRT's own blocking waits (`__crt_wait32` behind mutex/cond/sem/once, `pthread_join`,
  `nanosleep`) are interruptible instead: the sender wakes them (`WakeByAddressAll`/the thread's event) and the handler runs
  on the waiting thread with a `getcontext` snapshot (what WTF's thread suspension and JSC's conservative scan need). Bugs found
  by running it: a signal that coalesced while a handler ran was later delivered with a null `ucontext_t`; the new-thread mask
  was wiped by the context init; `sigsuspend` did not deliver what its temporary mask had held back; a hang that turned out
  to be a test teardown race; and, only in JSC, a VMTraps sender blocked forever in `sem_wait` because the target sat in
  `pthread_cond_wait` (8 hangs in 60 runs before the interruptible waits). Verification: `signal_vmtrap_test` (int3, loop
  interruption of a worker and of the initial thread, 8 threads x 300 rounds, blocked waits) 100/100 and `signal_threads_test`
  green, full Windows ctest 135/135, JSC Baseline-JIT acceptance with polling traps off (compile proof 2,340 reports,
  `jsc_watchdog_test` main x3 + worker x3) and the 1B interpreter acceptance green; `jsc_watchdog_test` 100/100 runs, no hang
  or crash. Not covered (by design): `kill(pid)`/process groups/SIGCHLD from other processes, `poll`/`select`/file I/O waits
  (a signal there stays pending until the call returns), and threads not created by `pthread_create`.

- **Web Tranche 1 replayed on Linux/aarch64: native thread TLS implemented for aarch64, and the JavaScriptCore
  interpreter and Baseline JIT run on the CRT runtime.** First verification of the shared libc/headers/wrappers/
  recipes/harness work on an aarch64 Linux host (Ubuntu 26.04 aarch64 QEMU guest, Clang 21.1.8, from a deleted
  `out/`): clean build exit 0 with no warning from CRT sources; C-stage ctest 113/114. The one failure,
  `pthread_native_tls_test`, was the documented gap (every thread reported one TLS address), and the same gap crashed
  JSC's 4- and 8-thread context cycles with `SIGTRAP`, while the script and the 0/1-thread cycles passed.

  `libc/src/arch/linux/common/thread_tls.c` now supports aarch64 (TLS variant I, `tpidr_el0`): the 16-byte header
  and a fresh dtv at the new thread pointer, static blocks above it at the offsets read from the initial thread's dtv
  (never recomputed), the thread pointer aligned to the strictest `p_align`, and the loader's thread descriptor below it
  copied at the same thread-pointer-relative offsets. Its size (glibc's `struct pthread`, 0x720 here) is not exported by
  `ld.so` (no `_thread_db_*`; the TLS functions are `GLIBC_PRIVATE`), so the copy window is a fixed upper bound
  (0x1000) clamped to pages that `mincore()` proves are mapped, and setup declines below 0x100 bytes. Found by
  disassembling `ld.so` that it reads fixed offsets below `tp` (`tp-40`, `tp-0x720`, `tp-0x71c`, `tp-0x600`).
  `__crt_sys_clone_thread` already passed the TLS argument in the aarch64 `clone` order, so no assembly changed;
  x86_64 behaviour is unchanged (both targets syntax-check clean).

  Verified: `pthread_native_tls_test` passes and fails when the setup is made to decline (mutation, restored); full
  `ctest` 136/136 (was 135/136); tooling unittests 102/102; `crt-ui-dist` and `verify_dist` pass. JSC interpreter
  acceptance passes (cycles on 0/1/4/8 threads, 150 each, 0 failures; host-ABI audit clean) and so does the
  Baseline JIT (2,340 compile reports, the same count as Linux/x86_64; compiled loop terminated by the watchdog); 150/150
  watchdog runs, and 20/20 four- and eight-thread cycles in each mode. Three shared-library TLS modules
  (`libJavaScriptCore`, `libc++abi`, `libicuuc`) are in the process, so the multi-module path is exercised.

  A fresh host needed more than the checklist said, now recorded in `docs/acceptance/crtweb_acceptance.md`: `libc++-21-dev`,
  `libc++abi-21-dev`, `libunwind-21-dev` (the build forces `-stdlib=libc++`) and `ruby`; the ICU port built into the
  harness's `--deps-prefix` (`port-build-icu`); and `-DCRTUI_ENABLE_LVGL=ON` for `crt-ui-dist`, whose
  `crtui-lvgl-fetch` target cannot run while configure fails (an existing "Small UI follow-up"). Open on aarch64:
  DFG/FTL/WebAssembly and W^X (1D), and a shared-library TLS case inside `pthread_native_tls_test` itself.

### Windows: thread_local in the in-tree build (`pthread_native_tls_test`)

`pthread_native_tls_test` did not link on Windows (`undefined symbol: _tls_index`): the freestanding
startup has no PE TLS directory, and `tools/crt-cc` already lowers `thread_local`/`__thread` to
`__emutls_get_address()` (`-femulated-tls`, compiler-rt on Win32 TLS slots) for every port, but the
in-tree CMake build did not. `crt_build_flags` now adds `-femulated-tls` on Windows, and the test
links `emutls_link_stubs.c`, `uuid.lib` and `--allow-multiple-definition` (the accommodations
`libcrtgfx` documents for Skia's `thread_local`; a duplicate-`fprintf` warning in the archive member
is expected). The test now passes (6 threads: distinct addresses, `.tdata`/`.tbss` initial values,
isolation, creator untouched); the 28 pthread/tls/fork/thread/signal tests and 151 of 152 in the
full Windows CTest pass. No native PE TLS was implemented: the emulated scheme is the existing
decision (`docs/guides/import_bionic.md`, Errno TLS Tranche). `bionic_surface_test` also failed on
Windows (`gettid()` on the initial thread != `getpid()`): `GetCurrentThreadId()` is not the pid, so
the macOS mapping (initial thread's id recorded at startup returns the pid, other threads keep
their own) in `gettid()` (`libc/src/process.c`) now applies to Windows too. Full Windows CTest 152/152
(two timeouts in the first parallel run right after a libc relink passed on rerun). A forked child's
only thread is a new native thread, so `__crt_atfork_child()` re-records it as the initial thread
(Windows only); `bionic_surface_test` checks `gettid() == getpid()` in a child (mutation: removing
the re-record fails exactly that check). The sweep test timed out in parallel runs right after a
libc relink and passes alone; the cause was not investigated.

### Windows/x64: JavaScriptCore 1C (Baseline JIT) green with polling traps (Web Tranche 1 Windows replay)

The Baseline JIT passes its acceptance on Windows/x64: the assembly interpreter alone compiles nothing, the JIT script produces
2,340 compile reports (the Linux/x86_64 number), 1B passes again with the JIT, 150-cycle contexts on 0/1/4/8 threads, the
watchdog terminates a compiled loop on the main thread and on workers, and the PE audit is clean. It needed one carried patch
(`libcrtweb/patches/0003-coff-assembler-flavor`: COFF assembler directives, and the System V argument-register/`sysv_abi`
conventions JavaScriptCore selects with `OS(WINDOWS)`, plus offlineasm's `--platform=Windows`), `-mno-ms-bitfields`, and a CRT
fix: `getrlimit(RLIMIT_STACK)` reported 8 MiB for a 1 MiB main stack, so the assembly interpreter overflowed the real stack
instead of throwing a RangeError. A libc++ recipe patch gives Windows consumers that hide `_WIN32` the library's own iostream
configuration (so JavaScriptCore.dll links the shared libc++.dll). Signal-based VM traps, the sampling profiler, DFG/FTL and
ARM64 are not covered; the first run uses `JSC_usePollingTraps=true` by decision.

### Windows/x64: JavaScriptCore 1B (interpreter) green, and eight CRT gaps it exposed (Web Tranche 1 Windows replay)

With Ruby installed on the build host, the pinned WPE WebKit 2.54.0 builds `jsc` on Windows/x64 from the installed `05-ui` SDK
and passes the 1B acceptance: `jsc_acceptance.js` (8 groups, peak 74 MB) and `jsc_context_cycle` (150 cycles on 0/1/4/8 threads,
0 failures, about 60 KiB growth), with a clean PE import audit. The persona is Linux-shaped, as on macOS, because the pinned
tarball has no WIN32 sources. Getting there fixed eight CRT gaps, each with a test where the host allows it: libm `atanf`;
`libc.dll` exporting `environ`; a weak `_fltused` for DLLs; one libc per process (executables link `libc.dll` under shared
linkage, `CRT_CXX_STL_LINKAGE` for libc++); `syscall(SYS_gettid/SYS_getpid)`; partial `munmap`, reserve-only `PROT_NONE` and
commit-on-`mprotect`; `size_t` instead of 32-bit `unsigned long` lengths in the memory-map entry points (a 16 GiB mapping was
truncated to 0); and the real stack in `pthread_getattr_np`. New tests: `long_argv_test` (earlier), `mmap_partial_test`,
`pthread_stack_bounds_test`, and the extended `bionic_surface_test`; the new ones fail on the old behaviour (mutation-checked).
libc++.dll's missing `<sstream>` exports turned out to be a configuration mismatch: upstream disables those explicit
instantiations on Windows (LLVM PR41018) and a consumer that hides `_WIN32` took the other branch; a libc++ recipe patch fixes
it and the lane now links the shared libc++.dll (one libc and one libc++ per process). Not done: 1C, the watchdog, the
isolated stage chain, ARM64.

### Windows/x64: getrusage, the JSC harness's Windows lane (Web Tranche 1 Windows replay)

`getrusage(RUSAGE_SELF)` on Windows returned zeros; it now reports the user/kernel CPU time (`GetProcessTimes`) and the
peak working set as `ru_maxrss` in KiB (`K32GetProcessMemoryInfo`), with `RUSAGE_CHILDREN` and `RUSAGE_THREAD` still zero
(Windows keeps no such accounting). `bionic_surface_test` runs its `getrusage` case on Windows, and
`jsc_context_cycle.cpp` reads the peak through it instead of `/proc/self/statm`. `tools/build_webkit_jsc.py` gained a
Windows lane: PATH-based DLL lookup, `JavaScriptCore.dll`, no rpath, a polled peak-working-set measurement (Python has no
`resource` module on Windows), and a PE import audit via `llvm-objdump -p` with the distribution's own Windows contract
(`crt_dist_prerequisites.py`) as the OS-DLL allowlist. Checked on real data only: the audit passes the CRT-built ICU tools
and rejects `python.exe`; the measurement reports 214 MB for a 200 MB allocation. JavaScriptCore itself has not been
configured on Windows: the host has no Ruby. `jit_memory_test` already passes in full on Windows/x64.

### Windows/x64: gperf port (Web Tranche 1 Windows replay)

`gperf` (needed for WebCore's build, not JavaScriptCore's) builds on Windows and its `generate-hash` recipe test
passes. It needed only a recipe override: `--build=x86_64-unknown-linux-android` instead of the usual MinGW
triple. gperf bundles gnulib, which chooses its replacement layer from the configured host OS; for a mingw
host it substitutes Windows headers (`io.h`), `struct stat` -> `_stati64` and `rpl_fstat`, none of which this
POSIX/Bionic-shaped sysroot has, and undefining `_WIN32`/`__MINGW32__` afterwards does not undo them (tried first:
the build failed in `stat-time.h` and `fstat.c`). The Bionic host is the CRT's own identity and also makes
configure's runtime probes run. No CRT defect was found by this port.

### Windows/x64: ICU port and three PAL defects it exposed (Web Tranche 1 Windows replay)

The first step of the JavaScriptCore replay on Windows is the ICU port. It builds and both recipe tests pass
(`unicode-static`, `unicode-shared`; ICU 78.3 with its embedded data). Getting there found three real CRT
defects, fixed in the PAL rather than worked around in the recipe:

- **Silent argument truncation.** The Windows startup (`libc/src/arch/windows/common/crt1.c`) kept at most 255
  arguments and 8191 command-line characters (and the fork relaunch and spawn paths had the same 8192 limit).
  ICU's `libicuin` link has about 280 arguments, so the tail -- the libraries -- vanished and the link failed with
  undefined symbols far from the cause. Limits are now the OS's (32767 characters, at most 16385 arguments); spawn
  already failed explicitly with `E2BIG`. New `long_argv_test` spawns itself with 700 arguments (mutation: the old
  limits give `argc=241`).
- **Programs without `.exe`.** ICU's data Makefile runs `../bin/icupkg`, a tool its own build produced as
  `icupkg.exe`. `stat()`, `access(X_OK)` and spawn now find `name.exe` when `name` does not exist and has no
  extension (MSYS/Cygwin behaviour; mksh checks with `stat`/`access` before it runs a command). Covered by
  `long_argv_test` (mutation fails it). The widened `stat` is a semantic change on Windows: only ICU and zlib/libpng
  port tests were re-run, not every Windows port.
- **Port test `PATH`.** `tools/crt-port-build.py` did not put the SDK `bin` directory (libc.dll) on the `PATH` of a
  test that loads a port DLL.

Recipe-only accommodations (none patch ICU): see `porting/recipes/icu.json` notes -- the POSIX platform path, the
64-bit MinGW configure fragment (`icu_cv_host_frag=mh-mingw64`; the `-U__MINGW64__` the POSIX path needs made
configure pick the 32-bit one, giving an underscore-prefixed data symbol and a mis-named data DLL), and `;`
`PATH` in ICU's `INVOKE`. A first diagnosis blamed literal quotes in `ICULIBS_*`; that was wrong, the arguments
were being truncated. Full Windows CTest 153/153; zlib and libpng port tests pass. Not done: expat/pcre2 and the
other ports were not re-run, gperf on Windows, Windows/ARM64.

## 2026-10-04

- **macOS CI: `bionic_surface_test` failed (`gettid()` in a forked child != `getpid()`).** The macOS
  `gettid()` maps the initial thread to the pid by comparing with the thread id recorded at startup, and a forked
  child's thread has an Apple thread id of its own, so the child fell through to the raw id. The Windows fix
  (child re-records its thread as initial in `__crt_atfork_child`) now applies to macOS too. The failure was not
  seen locally because the test binary was stale; with the fix removed the rebuilt test fails here as in CI.

- **Web Tranche 1C replayed on macOS/arm64: the JavaScriptCore Baseline JIT runs on the CRT runtime.** Clean
  `--mode baseline-jit` build 153 s; assembly interpreter (no code compiled), JIT script (2,340 compile reports),
  1B script under the JIT, VM cycles on 0/1/4/8 threads, a watchdog that kills compiled code on main and worker
  threads, and the Mach-O host-ABI audit all pass, repeatedly. No WebKit patch for W^X: `mmap()` promotes anonymous
  RWX to `MAP_JIT`, `__crt_jit_write_protect` wraps `pthread_jit_write_protect_np`, and JSC's
  `OS_THREAD_SELF_RESTRICT` extension point is bound to it by a force-included header. Second carried patch
  (`0002-macho-assembler-flavor`): Mach-O spellings of the inline/offlineasm assembler keyed on
  `WTF_CRT_MACHO_ASM`; its last hunk drops the bare GDB debug labels the Linux persona emitted, which split the
  `.alt_entry` atoms and left zero-filled gaps in the linked LLInt (found by comparing the `.o` with the dylib at
  the symbol; it looked like a `-dead_strip` problem at first and the `CRT_MACOS_NO_DEAD_STRIP` workaround was
  removed). CRT changes: real macOS signals (libSystem forwarding, Darwin/arm64 to Bionic/Linux aarch64
  `siginfo`/context conversion with register write-back), real stack bounds for threads created with only a
  size, `getauxval`/`<asm/hwcap.h>`, `jit_memory_test` and `signal_threads_test` extended for Apple Silicon.
  Open: macOS/x86_64 (no signal conversion, no JIT permissions), 1D.

- **Web Tranche 1A/1B replayed on macOS/arm64: the JavaScriptCore interpreter runs on the CRT runtime.**
  Decision (A): WebKit sees macOS as a Linux-shaped POSIX platform (`libcrtweb/cmake/crt_webkit_platform.cmake`,
  `-U__APPLE__ -D__linux__=1`, explicit CRT compilers) with one hash-checked carried patch (`WTF::RawHex`,
  `libcrtweb/patches/manifest.json`). `jsc_acceptance.js` (8 groups, 69 MB peak) and 150 context cycles on 0/1/4/8
  threads pass and the new Mach-O host-ABI audit is clean. It found and led to: libc++.dylib linking the host
  libc++abi, ICU's bare install names, `gettid()`/`syscall(SYS_gettid)` on macOS, initial-thread stack bounds,
  a real `getrusage()` (it was a stub on every host), `memset_explicit()`, UTC `tzname`/`timezone`, `-lm`/`-ldl`
  handling in the wrappers, ELF-only option filtering, and fixes to two flaky tests. `jit_memory_test` can only
  check RW to RX on Apple Silicon (no `MAP_JIT`). The Baseline JIT (1C) needs a Mach-O assembler-flavor patch, W^X
  through `OS_THREAD_SELF_RESTRICT` and a real macOS signal backend (`docs/acceptance/crtweb_acceptance.md`).

- **Web Tranche 1C closed on Linux/x86_64: the JavaScriptCore Baseline JIT runs on CRT, and the work
  found five more signal/thread defects.** The pre-JIT gate (harness `--mode`, the CRT
  `jit_memory_test`, 1B rerun from the installed `05-ui` SDK, `libatomic` and gperf cleanups) is closed;
  the JIT build compiles every tier in (a Baseline-only build does not compile: upstream header-order and
  B3 dependencies) and isolates the Baseline JIT with JSC options. Results: the 1B script with the JIT
  off compiles nothing; the JIT script, 1B script, VM cycles on 1/4/8 threads and a watchdog that kills a
  compiled infinite loop on main and worker threads all pass, with 2,340 `using Baseline ... into N bytes`
  compile lines as proof; 10/10 harness runs and 150/150 watchdog runs. Details and the W^X/DFG/FTL
  boundaries are in `docs/acceptance/crtweb_acceptance.md`.

  An intermittent watchdog crash (8 in 100) led to the fixes: `pthread_kill` dereferenced the initial
  thread's tid-valued `pthread_t` (my own code; now validated against live CRT threads); `sigsuspend` was a
  stub, so a thread WTF believed suspended kept running; `sigaction` discarded `sa_mask` and the flags;
  the signal mask was process-wide instead of per thread (all four now real on Linux); and `pause()` was
  missing. New tests `signal_threads_test`, an initial-thread case in `bionic_surface_test`, and
  `jit_memory_test`; mutations of `sigsuspend` and `pthread_kill` fail them. ctest 158/159 serially (the
  sound-card test; two capture tests contend for the now-attached webcam only under `-j4`), tooling 102/102.

- **Web Tranche 1C plan refined (documentation).** Checked against the pinned WebKit: the JIT conflicts
  with `C_LOOP`, DFG defaults on with the JIT and FTL depends on DFG, `ENABLE_MPROTECT_RX_TO_RWX` is 0
  and Linux executable memory is created RWX at once without `MAP_JIT`, and the JIT enables
  signal-based VM traps. 1C is now the Baseline JIT only (DFG/FTL/WebAssembly/profiler are 1D and
  later), W^X is a hardening follow-up rather than part of first green, proof of compiled code is
  required, and a pre-JIT gate (harness build modes, a CRT `jit_memory_test`, rerunning 1B from the
  installed `05-ui` SDK) precedes it. Two review items were already done (gperf dropped from the JSCOnly
  checks; the host `libatomic` dependency removed at its root in the libc++ build).

- **Second review of the `06-web` plan folded in, and its host-ABI firewall found a real
  leak.** Checked against the pinned `WebKitCommon.cmake`: gperf is required only when WebCore
  is enabled (so a WebCore prerequisite, not a JSCOnly one), while Perl (with `English`,
  `FindBin`, `JSON::PP`), Python and Ruby are required for every port. `docs/acceptance/crtweb_acceptance.md`
  now separates build-host tools from target dependencies (and notes that a cross build will
  need that split for gperf/ICU's generators), adds the configure fingerprint
  (`webkit-jsc-config.json`), the Host ABI firewall for every JSC acceptance, a mandatory gate
  before Tranche 3B (pin the full WebKit commit behind the signed tag as the `PlatformCRT`
  product source; the WPE tarball stays the reference, and lacks the Windows port), and a
  per-file license scan plus patch manifest before the first carried WebKit patch. The harness
  gained the fingerprint, the host-tool/Perl-module check, and `host_abi_audit()`; it no longer
  demands gperf. `libcrtweb/README.md` and several stale README lines (`06-web` "no
  implementation", "Tranche 1 next") were brought up to date.

  The new audit failed at once: the SDK's `libc++.so.1` recorded `NEEDED libatomic.so.1` (the
  libc++ build linked the host libatomic), which pulled glibc's `libc.so.6` into every
  JavaScriptCore process beside CRT's `libc.so`. `LIBCXX_HAS_ATOMIC_LIB=OFF` is now set on every
  target (it was Windows-only); `libc++.so.1` no longer needs libatomic and the audit passes
  with no exceptions. The harness result (script, VM cycles on 1/4/8 threads) is unchanged.
- **Web Tranche 1 on Linux/x86_64: JavaScriptCore (JSCOnly) builds with the CRT toolchain
  and passes the interpreter acceptance (1A and 1B); the exercise found and fixed several
  fundamental CRT/PAL defects.** `tools/build_webkit_jsc.py` builds the pinned WPE WebKit
  2.54.0 against an installed SDK (C_LOOP interpreter, JIT/FTL/WebAssembly off, Generic
  event loop, CRT ICU/gperf ports, host Ruby) and runs `libcrtweb/tests/jsc/`: an eight-group
  script (arithmetic, objects, JSON, RegExp, ICU-backed Unicode/Intl, exceptions, GC stress,
  microtasks; peak RSS 198 MB) and a C API program that creates/uses/releases a whole VM 150
  times and 20 times on each of 1/4/8 concurrent threads with flat memory. A clean run takes
  about six minutes. Details in `docs/acceptance/crtweb_acceptance.md` (Tranche 1).

  The defects, each found by a failure in this work and fixed in CRT, not in WebKit:
  `thread_local`/`__thread` was shared by all CRT threads on Linux (clone without
  `CLONE_SETTLS`) -- native per-thread TLS blocks built from `PT_TLS` images and the
  initial thread's dtv (x86_64; aarch64 not yet); an executable linked `libc.a` while the
  shared libraries used `libc.so`, giving the process two libcs with separate thread
  registries and allocators (`CRT_CXX_RUNTIME_LINKAGE=shared` now links libc.so into the
  executable); `siginfo_t` lacked `si_addr`, `ucontext_t` was private and `SA_SIGINFO`
  handlers got a synthesized record and a null context (Bionic/kernel layouts, real
  forwarding on Linux); `open(O_CLOEXEC)` silently ignored the flag; GNU ld.bfd produced a
  PIE with a stray `.rela.plt` entry when `PT_TLS` was present (CTest executables now use
  LLD); and a long list of missing Bionic surface (`pthread_kill`, `pthread_getattr_np` for the
  initial thread, `sched_*`, `memmem`, `usleep`, `mkostemp`, `fallocate`, `sysinfo`,
  `sendfile`, `getprogname`, the extra clock ids, `sys/ucontext.h`, `SYS_gettid`/`__NR_*`,
  `<cxxabi.h>`, ELF32/Nhdr in `<elf.h>`). New tests: `pthread_native_tls_test` (mutation:
  disabling the TLS setup fails it) and `bionic_surface_test`. Verified: full build, ctest
  156/157 (sound-card test only), tooling 102/102, `crt-c-dist`; the changed libc sources
  also syntax-check for aarch64 Linux, macOS and Windows (not run there).

- **Review of the `06-web` plan folded into the docs.** Checked against the pinned 2.54.0
  source: `PlatformCRT` is a new WebKit port (WPEPlatform only the Linux prototype
  boundary); `WPEProcessManager` is built only for Android; the tarball has no Windows IPC
  backend; Tranche 1 split 1A/1B/1C; the frame-producer and input contracts are to be frozen
  before Tranche 3A, with IME deferred past v1.

## 2026-10-03

- **Web Tranche 1A (in progress): `gperf` and `icu` ported as CRT recipes on
  Linux -- the first C++ ports -- and the review of the `06-web` plan folded into
  the docs.** JSCOnly needs ICU >= 70.1 (`data`, `uc`, `i18n`; verified in
  `OptionsJSCOnly.cmake`, default `Generic` event loop so no GLib) and WebKit's
  CMake needs Ruby, Perl and Python as host tools, and gperf once WebCore is on; gperf 3.3 (GPG-verified) and ICU 78.3 (SHA-256 and
  SHA-512 checked against the release) are CRT ports, Ruby is a host tool the
  maintainer installs. Both pass recipe tests (`gperf` runs and emits a hash;
  `icu` static and shared exercise case mapping, NFD and Swedish collation).
  Building them exposed CRT gaps, all fixed rather than worked around:
  - The port builder always used the C-only `01-c` sysroot; a recipe can now say
    `build.sysroot_stage: 02-cxx` (and `make_subdir`, and C++ recipe tests with
    `"language": "c++"`).
  - `tools/crt-c++` compiles with `-ffreestanding`, so an unmodified `int main`
    was mangled (`_Z4mainv`) and crt1 could not link it; `CRT_CXX_HOSTED=1` adds
    `-fhosted` (opt-in; the default is unchanged).
  - `-lm`/`-ldl` from an upstream makefile linked the shared libm into a
    static-runtime executable (`NEEDED libm.so`, no rpath, so the host loader hit
    glibc's `/lib/.../libm.so` linker script: "invalid ELF header", which also
    broke ICU's configure `sizeof(wchar_t)` probe); both wrappers drop them in
    that configuration.
  - Shared libraries linked through the wrapper had no `__dso_handle`
    (`crtbegin_so.o` added to libc and the Linux `-shared` link).
  - CRT headers formed an include ring (`<sys/types.h>` -> `<stdint.h>` ->
    `<wchar.h>` -> `<locale.h>` -> `<xlocale.h>` -> `<stdio.h>`) that gnulib's
    wrapper headers broke at any entry point: `bits/crt_types.h` and the new
    `bits/crt_wtypes.h` are leaf headers, and `<xlocale.h>` no longer includes
    the heavy headers.
  - libc gained Bionic's `getprogname()`/`setprogname()`/`__progname` (gnulib
    requires it), and `<elf.h>` the ELF32 types ICU's `pkg_genc` writes.
  Verified: full build, ctest 154/155 (only the sound-card test; the camera test
  skips without a webcam), tooling 102/102, `crt-c-dist` verifies with the new
  object. Windows and macOS are not attempted for either port. No WebKit code is
  built yet.
  Review changes to `docs/acceptance/crtweb_acceptance.md` / `crtweb_porting.md` (checked
  against the pinned 2.54.0 source): `PlatformCRT` is a new WebKit port with
  WPEPlatform only as the Linux prototype boundary; `WPEProcessManager` is
  compiled only for Android and is not a generic hook; the tarball has no Windows
  IPC backend or `PlatformWin.cmake`, so the Windows transport must be written;
  Tranche 1 split into 1A/1B/1C with a build harness against the installed SDK;
  the frame-producer and input contracts are frozen before 3A (separate `crtweb`
  input path, IME deferred past v1); stale candidate wording fixed.

- **Windows replay of the isolated-SDK text-file relocation found and fixed a
  Windows-only defect.** The macOS/Linux fix for staging paths in `.pc`, `.la` and
  `*-config` files had never run on Windows, and its first Windows run failed in the
  final `verify_dist.py` of the isolated 04 stage (after a 4575 s build): the CRT
  shell passes the install prefix as `/c/crtw/...`, a spelling `tools/crt_text_
  relocate.py` did not know. `root_spellings()` now also yields `/c/...` and `/C/...`;
  two new tests in `tools/test_text_relocate.py` fail without it. The chain was rebuilt
  clean from a regenerated 03 SDK: isolated 04 with the 03 SDK's own tool (4560 s,
  17/17, verified, published; `--reuse-work-root` now leaves its dependency layers
  cached), isolated 05-ui with the 04 SDK's own tool and embedded recipe (165.7 s,
  8/8, sample and packaged demo `presented=30 pixel_check=pass input_check=pass`,
  verified, published). No `.la` remains, the `.pc` files use `${pcfiledir}`, and a
  `curl-config` moved to a path containing a space reports the new location. The
  relocation fix is now verified on Windows, macOS and Linux; evidence in
  `docs/acceptance/crtui_acceptance.md`.

- **Application UI (`05-ui`) closed; documents reconciled; Web Runtime (`06-web`)
  Tranche 0 closed.** With the macOS/arm64 and Linux/x86_64 replays of Tranche 7
  accepted, `crtui` Tranches 0-7 are closed on all three hosts (per-tranche evidence
  in the entries below and `docs/acceptance/crtui_acceptance.md`). `README.md`, `STATUS.md`
  (updated at the owner's request, synchronized to 2026-10-03), `TODO.md`,
  `docs/design/runtime_roadmap.md`, `docs/guides/distribution.md`, `docs/guides/faq.md`,
  `docs/design/project_meanings.md`, `libcrtui/README.md` and the acceptance documents now
  describe `05-ui` as accepted and `06-web` as the current stage; the completed
  Application UI section left `TODO.md`, and the stale "not verified"/"replay
  remains" statements in `docs/acceptance/crtui_acceptance.md` were replaced by the replay
  records. `STATUS.md` gained a `libcrtui` baseline, its known limitations (no IME,
  selection, touch or pointer-drag scrolling; scripted input only; `05-ui` not a
  release asset; `crt-ui-dist` needs `CRTUI_ENABLE_LVGL=ON`) and a UI check table.

  `AGENTS.md` was brought up to date as well: its project structure now lists every
  top-level folder (the upper-runtime libraries, `distribution/`, `examples/`,
  `cmake/`, `benchmark/`, `libcrtweb/`) and the real state of `libm`, `libstdc++`,
  `shell/awk`, `porting`, `tools` and the reserved, unbuilt `linker/`; Android is
  stated to be the source of Bionic libc, not a target OS; WebKit is named as the
  `06-web` goal; the stack, layer 4/5 and reference-document sections were
  corrected, and the document-management rules were pointed to. Stale statements
  in `libcrtmedia/README.md` (encode/capture "active", "no network yet") and
  `libcrtgfx/README.md` (no libwayland-client, no GPU backend) were corrected, and
  the root `README.md` Build section now says `crt-ui-*` needs `CRTUI_ENABLE_LVGL=ON`.

  **Web Tranche 0 closed.** WPE WebKit **2.54.0** (2026-09-16, the current stable
  series) is pinned in `libcrtweb/third_party/webkit/recipe.json` and verified. The signed tag
  `wpewebkit-2.54.0`, its commit `73f39d84ea9d...` and the archive size (46,202,080
  bytes) were confirmed from GitHub and HTTP; the archive was then downloaded and its
  SHA-256 recomputed (`efa9bcc3cb89...eb452`, equal to the release page); the tag's
  signature verifies with `gpg` against the signer's key (Adrian Perez de Castro,
  `5AA3BC33...123B`, DSA-1024 expiring 2027-03-16, not web-of-trust certified); and
  six sampled source files are byte-identical to the tagged commit (the top-level
  `NEWS` exists only in the tarball). The release notes (WPEPlatform stable and
  default, libwpe API deprecated, Skia-only 2D with the compositor in the web process,
  Ninja required, `WPEProcessManager`) were checked against the raw pages, not only a
  summary. The archive holds 38,843 files; `tools/scan_webkit_licenses.py` inventoried
  its 64 license/notice files (LGPL-2/2.1, an Apple BSD-style notice, and bundled
  Skia, ANGLE, pdf.js, gtest and others) in `libcrtweb/third_party/webkit/license-inventory.json`.
  One design-relevant finding: WebKit bundles its **own Skia, milestone 154**, while
  CRT uses m148, so the two stay separate copies. `tools/fetch_webkit.py` and
  `tools/scan_webkit_licenses.py` (with `tools/test_fetch_webkit.py`, 6 tests) make
  this reproducible; `docs/porting/crtweb_porting.md` holds the upstream mapping and
  `docs/acceptance/crtweb_acceptance.md` the frozen scope and the security/SBOM policy, which the
  owner confirmed. Left explicit in the recipe: per-file license headers were not
  scanned, the tarball is not proven equal to the commit tree beyond the sampled
  files, and upstream's security support window is undocumented on the pages read.

- **`crtui` Tranche 7 replayed on Linux/x86_64: the isolated `05-ui` stage passes
  -- Tranche 7 closed.** One fix: `tools/build_stage_05_ui.py` could not find
  Debian's versioned `llvm-nm-21`; it now tries `/usr/bin/llvm-nm-N` and
  `/usr/lib/llvm-*/bin`. Chain from empty work/cache dirs: 03 SDK -> isolated 04
  (1102 s, verified) -> isolated 05-ui (23.5 s, also from space-containing paths):
  ctest 8/8 including the real-clip media view test, installed `ui-basic` rebuilt
  externally and the prebuilt demo `presented=30 pixel_check=pass input_check=pass`,
  verify and atomic publish pass; package has the Skia/media companions, the LVGL
  notice/recipe and no LVGL header; `libcrtui.so` exports only `crtui_*`. The
  text-file relocation fix also holds on Linux (no `.la`, no staging paths,
  relocated SDK's `pkg-config`/`curl-config` follow the move). Shortcut: the 05-ui
  run used the freshly regenerated recipe/asset rather than the one embedded in the
  04 SDK, which predated the fix. ctest 154/155 (sound-card test only), tooling
  94/94.

- **Isolated SDKs no longer carry the staging path in text files.** The 04
  stage installs ports into a temporary prefix that autotools/pkg-config baked
  into `lib/pkgconfig/*.pc`, `*.la` and `bin/curl-config`; Mach-O/ELF had been
  relocated but text had not, and `verify_dist.py` never looked
  (`docs/guides/distribution.md` criterion 8). New `tools/crt_text_relocate.py` (drops
  `.la`, `prefix=${pcfiledir}/../..`, location-relative `*-config`) runs in
  `build_stage_04_gfx_media.py`; `verify_dist.validate_text_paths()` rejects
  leftovers; `tools/test_text_relocate.py` adds 5 tests (tooling 94/94). Rebuilt
  the clean macOS chain (isolated 04 607 s, 05-ui 47 s, 8/8): no stale path in the
  SDK, `pkg-config`/`curl-config` follow a moved copy. Windows/Linux unverified.

- **Isolated `04-gfx-media -> 05-ui` stage replayed on macOS/arm64 (`crtui`
  Tranche 7), after one fix.** The stage builder could not find `llvm-nm`
  behind Apple's clang shim; `tools/build_stage_05_ui.py` now uses `xcrun`.
  Rebuilt the whole chain clean afterwards: 03 SDK -> isolated 04 with the 03
  SDK's own tool (615 s, 17/17) -> isolated 05-ui with the 04 SDK's own tool
  (48.8 s, ctest 8/8, installed sample and packaged demo `presented=30
  pixel_check=pass input_check=pass`, verify and publish pass), and again from a
  path containing spaces. The isolated `libcrtui.dylib` binds libc to `libc.dylib`
  and the dylib-linked sample runs cleanly. Full ctest 162/162, tooling 89/89,
  `crt-ui-dist` verified. Inherited absolute paths in 04-layer text files
  (`.la`, `curl-config`, `.pc`) were recorded as a follow-up and fixed the same day
  (next entry). Linux replay is next.

- **Isolated `04-gfx-media -> 05-ui` stage accepted on Windows/x64
  (`crtui` Tranche 7).** `distribution/stages/05-ui/CMakeLists.txt` and
  `tools/build_stage_05_ui.py` build crtui (private LVGL v9.6.0, fetched live and
  re-verified against its pinned SHA-256), the Skia/MediaView companions, eight
  tests and the installed `ui-basic` sample from a ~118 KB source asset against the
  isolated 04 SDK alone. The entrypoint refuses the ordinary default-OFF 04
  package, declares LVGL as a private static dependency with its MIT notice and
  pinned recipe (the in-tree package never shipped the notice), rebuilds the sample
  externally from the SDK, runs the packaged demo, verifies and publishes
  atomically. `libcrtui/cmake/crtui_sources.cmake` is now the single source list
  for the in-tree and stage builds. `STAGE_SUCCESSORS` gained `04 -> 05`;
  `crt-stage-05-source` exists, `crt-gfx-media-dist` and the 04 source asset embed
  `05-ui.json`, and the 04 entrypoint copies it into its SDK. `tools/crt_dist_
  prerequisites.py` needed no entry (05-ui equals 04 on every OS).

  From the real isolated 04 SDK (4594 s, built from the packaged 03 SDK), the 05
  stage passes in 123-158 s: ctest 8/8 including the compositor test and the
  media-view test (`interop=gpu-copy gpu_frames=5 cpu_frames=0 releases=5`), the
  rebuilt sample and the packaged `crtui_window_demo` both report `presented=30
  pixel_check=pass input_check=pass`, and `verify_dist.py` passes; a work/output
  path containing a space passes too. In-tree `crt-ui-dist` still verifies the whole
  01..05 chain, full CTest is 177/177 (one no-camera skip), and the stage-source
  closure test (7 tests, now covering 05-ui) passes. Removing the LVGL notice makes
  `verify_dist.py` fail.

  A first draft of the `verify_dist.py` check required the companion libraries of
  every in-tree package, which would have rejected the legitimate Skia-OFF package
  (headers install unconditionally); it now applies only to the isolated stage.

  **Clean-build verification.** With `out/` and all scratch directories deleted,
  the whole chain was rebuilt and passed: `cmake --fresh`, default CTest 127/127,
  `crt-gfx-simple-dist` (01-c..03), an isolated 04 built from the 03 SDK with that
  SDK's own `crt-stage-build.py` and no cache (4556 s, 17/17), then the isolated
  05-ui built with the 04 SDK's own tool and embedded `05-ui.json` (8/8, sample and
  packaged demo `presented=30 pixel_check=pass input_check=pass`, verified, 132 s,
  also from space-containing paths), and in-tree `crt-ui-dist` (LVGL on, Skia off)
  verifies 01..05. The 05-ui `verify_dist` logic moved into `validate_ui_stage()`
  with fake-tree tests in `tools/test_verify_dist.py` (15 tests; the first draft's
  check makes one fail). The earlier "old 04 SDK rejects the 05 recipe" finding is
  resolved: a 04 SDK built from a 03 SDK packaged from this tree carries the
  current `crt_stage_recipe.py`.

  The clean build exposed two defects the previous `out/` had hidden: libc++'s
  sparse checkout failed on Windows with "Filename too long" (neither long paths
  nor git `core.longpaths` enabled), fixed by passing `-c core.longpaths=true` to
  `tools/crt-libcxx-build.py`'s own git commands; and `capture_mf.c` could compile
  before the mingw-w64 headers were fetched (`'mfapi.h' file not found`) because
  `crtmedia_backend_objects` did not depend on `crtgfx-mingw-w64-headers-fetch`,
  now ordered in `libcrtmedia/CMakeLists.txt`. macOS/arm64 and Linux/x86_64
  branches of the stage project are written but unverified.

## 2026-10-02

- **`crtui` Tranche 6C replayed on Linux/x86_64 with no change -- Tranche 6
  closed.** `crtui_media_view_test` decodes the real H.264 fixture through VA-API
  and composes it in the final Vulkan present: `backend=vulkan interop=zero-copy
  gpu_frames=5 cpu_frames=0 releases=5`, identical over three runs, and 5/5
  frames in a real Wayland/Vulkan window. (The `interop=` label is derived from
  the backend by the test; the real evidence is `cpu_frames=0` plus the Linux
  import accepting only VA-API dma-buf frames.) Mutation `(void)previous;` in
  `crtui_skia_media_provider_submit()` fails ownership (`releases=1`), reverted.
  ctest 154/155 (sound-card test only), tooling 80/80, `crt-ui-dist` verifies and
  installs `libcrtui_skia_media.a`/`crtui/skia_media.h`; `libcrtui.so` still
  exports only `crtui_*`. Needed only `CRTMEDIA_ENABLE_FFMPEG=ON` in the dev tree.

- **`crtui` Tranche 6C replayed on macOS/arm64 with no change.**
  `crtui_media_view_test` decodes the real H.264 fixture with VideoToolbox and
  composes it in the final Metal present: `backend=metal interop=zero-copy
  gpu_frames=5 cpu_frames=0 releases=5`, byte-identical over five runs, and 5/5
  frames in a real Cocoa window. The replaced-image-release mutation fails the
  ownership check. Full ctest 162/162 (expected camera skip), tooling 80/80,
  `crt-ui-dist` verified with `libcrtui_skia_media.a` installed. Linux replay is
  next.

- **`crtui` MediaView Tranches 6A/6B accepted on Windows/x64.** Core `libcrtui`
  gained `crtui_media_view_create()` / `CRTUI_WIDGET_MEDIA_VIEW`, a distinct
  widget that follows the SurfaceView scene rules (clip, ancestor opacity,
  z-order, damage, hit-testing) and appears in the external-surface snapshot;
  core still holds no decoder, frame, GPU or Skia object. The optional static
  C++ companion `crtui_skia_media` (`crtui/skia_media.h`) owns one imported image
  per MediaView through `crtgfx_skia_import_media_frame()`: success moves frame
  ownership, failure leaves the frame untouched, CPU frames are rejected rather
  than labelled zero-copy, and replace/clear/destroy release after any
  in-flight compositor reference. The Skia compile settings shared by the two
  companions were folded into one CMake function.

  `crtui_media_view_test` decodes the real H.264 fixture through the
  hardware-preferring decoder: `backend=d3d12 interop=gpu-copy gpu_frames=5
  cpu_frames=0 releases=5` with UI composed below and above the video, a resize,
  replacement and clear, and the same path presents 5/5 frames in a real D3D12
  window. The first run failed on the test's own cleared-view check, which built
  its empty target at the pre-resize size and was correctly rejected by
  `crtui_skia_compose()`; the test now uses the current window size. The Skia-
  enabled `crt-ui-test` gate is 8/8, privacy is exact at `declared=68
  exports=68 lvgl_exports=0 objects=1 headers=5`, and full Windows CTest is
  177/177 (one expected no-camera skip). This machine had no Skia build, so the
  pinned Skia was fetched and built (834 steps) with `crtgfx-skia-build` and
  `CRT_USE_IMPORTED_LIBCXX=ON`. Tranche 6C (macOS, Linux) remains.

- **Interrupted libc++ runtime sparse fetches now recover instead of poisoning
  later builds.** A pulled Windows/x64 working copy failed `crt-c-dist` with
  `libunwind: checkout_subdir 'libunwind' not found`: the
  `.clone-libunwind` cache still contained valid Git metadata, sparse rules and
  the pinned commit, but every sparse worktree file was missing. The fetch
  driver had treated `clone_dir` existence as proof that clone/fetch/checkout
  had completed, so every retry reused the incomplete cache and failed before
  it could repair it.

  `tools/crt-libcxx-build.py` now validates an existing clone cache,
  re-establishes the requested cone-mode sparse paths, force-checks out an
  already-cached 40-character pinned commit when available, fetches only when
  that commit is absent, and removes non-Git/incomplete cache paths before
  recloning. The forced checkout is intentional: `git sparse-checkout
  reapply` restores skip-worktree policy but does not recreate tracked files
  that have been deleted from the worktree. A local-repository regression test
  now deletes all three sparse trees while preserving clone metadata and
  proves that the next fetch reconstructs `libunwind`, `cmake` and
  `runtimes/cmake`; it is registered in CTest. Verified on Windows/x64:
  `crt-libcxx-fetch` repaired the real damaged cache, the libc++ fetch and
  predecessor tests pass 2/2, and the originally failing `crt-c-dist` command
  completes through `CRT distribution verified`.

- **`crtui` External Surface Tranche 5 closed on all three hosts after the
  unchanged Linux/x86_64 replay and a clean-distribution dependency fix.** The
  Skia-enabled official UI gate passes 7/7 and stayed green for three
  consecutive runs; the fresh LVGL-OFF contract/layout/surface/input gate is
  4/4. A real Wayland/Vulkan window presented 5/5 frames three times with
  `gpu/ordering/clip/opacity/damage/resize/ownership=pass` and balanced
  producer ownership. Core `libcrtui.so` retains no Skia, Vulkan or `crtgfx`
  dynamic dependency, and package privacy is exact at
  `declared=67 exports=67 lvgl_exports=0 objects=1 headers=4`.

  The clean cumulative replay found that `crt-gfx` installed
  `libcrtgfx_skia.so` without any aggregate target building it: the existing
  Skia window demo only links the static bridge. `crt-gfx-media-build` now
  explicitly builds `crtgfx_skia_shared` after `crt-gfx-simple-dist`. A new
  build directory then generated and verified every cumulative distribution
  stage through `05-ui` without a manual target invocation, including the
  optional `libcrtui_skia.a` and `crtui/skia.h`. Tooling is 79/79. The full
  feature-enabled CTest result is 158/159; every `crtui` test passes, and the
  sole failure is the already-recorded independent playback wall-time residual
  on this no-sound-device host. With Windows/x64, macOS/arm64 and Linux/x86_64
  accepted, the External Surface contract now unblocks `06-web`; media binding
  remains Tranche 6.

## 2026-10-01

- **The macOS AVFoundation capture lifecycle residual is fixed and the full
  suite is green.** Two independent defects had been hidden by the earlier
  no-camera authorization path. First, dequeue transferred a queued frame's
  storage to the caller without clearing the slot, so backend teardown could
  free caller-released storage again. Queue take/drop/clear now make ownership
  transfer explicit; release detaches the sample-buffer delegate, drains its
  dispatch queue and only then destroys capture state. A resource-free queue
  ownership regression runs with the existing AVFoundation conversion test.

  Second, the backend synthesized AVFoundation preset and pixel-buffer keys as
  strings. It consequently reported 640x480 while this camera delivered
  1920x1080, and the lifecycle encoder rejected its first frame. The PAL now
  uses the framework's real `AVCaptureSessionPreset*` and CoreVideo
  `kCVPixelBuffer*Key` ABI symbols and explicitly requests the negotiated
  dimensions. The real lifecycle gate passes all 15 capture and 15
  VideoToolbox cycles: 450 captured/released/decoded frames and 1,500 hardware
  encoded/released/decoded frames. Full macOS CTest is 160/160.

- **`crtui` External Surface Tranche 5 replayed and accepted unchanged on
  macOS/arm64.** The official Skia-enabled UI gate passes 8/8, and a fresh
  `CRTUI_ENABLE_LVGL=OFF` tree passes contract/layout/surface/input 4/4. The
  real Metal-window compositor presented 5/5 frames with
  `gpu/ordering/clip/opacity/damage/resize/ownership=pass` and balanced
  producer ownership. Core `libcrtui.dylib` has no Skia, Metal or `crtgfx`
  dynamic dependency; privacy remains exact at
  `declared=67 exports=67 lvgl_exports=0 objects=1 headers=4`.

  `crt-ui-dist` rebuilt and verified cumulative `dist/05-ui`, with the
  optional `libcrtui_skia.a` and `crtui/skia.h` installed separately from core
  `libcrtui`. Tooling is 79/79. The initial UI replay exposed only the
  independent macOS camera lifecycle residual; the AVFoundation ownership and
  negotiation fix recorded above then made full CTest 160/160. No functional
  UI source change was needed. Linux replay remains before global Tranche 5
  closure.

- **`crtui` External Surface Tranche 5B accepted on Windows/x64: a real
  texture-backed producer is now composed with CRT UI in a real GPU window.**
  The optional static `crtui_skia` companion keeps Skia/GPU dependencies out of
  core `libcrtui`. It consumes the producer-neutral 5A layer snapshot, renders
  LVGL UI preorder ranges as opaque/transparent planes, draws each
  producer-owned `SkImage` directly between those planes, and calls the
  provider's matching release exactly once. Producer pixels are neither copied
  through LVGL nor read back to the CPU. Geometry-only LVGL placeholders retain
  the complete scene's coordinate and ancestor-clip behavior while
  SurfaceViews remain holes.

  The new `crtui_surface_compositor_test` uses a texture-backed synthetic GPU
  producer. Exact pixel readback proves UI-below/surface/UI-above order,
  ancestor clipping and 50% opacity; it also proves damage propagation, resize,
  stable-id mapping, one acquire/release pair per frame, and the no-frame
  `WOULD_BLOCK` path without a spurious release. Its real-window mode presented
  5/5 frames through a D3D12 swapchain with balanced ownership.
  The Skia-enabled official UI gate passes 7/7, and DLL privacy remains exact:
  `declared=67 exports=67 lvgl_exports=0 objects=1 headers=4`. Tranche 5 remains
  open only for unchanged macOS/arm64 and Linux replay (5C); MediaView remains
  Tranche 6.

- **`crtui` External Surface Tranche 5A accepted on Windows/x64: the
  producer-neutral scene boundary is now real, without mislabelling metadata as
  GPU display.** `crtui_surface_view_create()` now creates a normal opaque-id
  widget that participates in CRT layout, clipping and hit-testing. The new
  allocation-free `crtui_window_get_surface_layers()` snapshot exposes each
  visible SurfaceView's stable id, scene preorder/z, absolute bounds,
  ancestor-intersected clip and multiplicative effective opacity. Damage is
  accumulated per view, clipped at snapshot time, carries a monotonic serial,
  and remains explicit until the compositor clears it. `crtui` stores no
  producer pointer and never acquires, retains, releases or dereferences a
  producer frame; the final compositor maps the stable view id to its own
  surface.

  The new LVGL-free `crtui_surface_test` passes with
  `create/geometry/clip/opacity/z/damage/hit_test/visibility/thread=pass`, including
  capacity-query/no-partial-write and invalid-operation coverage. The complete
  UI gate is 6/6 with LVGL ON and 4/4 in a fresh LVGL-OFF tree. DLL privacy
  remains exact after the additive API: `declared=66 exports=66 lvgl_exports=0`.
  Full Windows CTest is 168/168 (one expected no-camera skip); the cumulative
  `crt-ui-dist` rebuilt and verified through packaged `05-ui`, whose installed
  headers/DLL pass the same 66/66 privacy check. This deliberately does **not**
  close Tranche 5: `crtgfx` still needs the real
  final compositor that interleaves producer-owned GPU content with UI above
  and below it without routing producer pixels through LVGL (TODO 5B), followed
  by macOS/Linux replay (5C).

- **`crtui` Tranche 4 closed on Linux/x86_64 and therefore on all three
  hosts.** The official LVGL-enabled UI gate passed 5/5 three consecutive
  times; a fresh `CRTUI_ENABLE_LVGL=OFF` tree passed layout/input 2/2. The real
  Wayland demo and an externally rebuilt sample using only packaged `05-ui`
  each ran 3/3 with `presented=30 pixel_check=pass input_check=pass`. The
  cumulative distribution verified through `05-ui`, including its generated
  `libxdg-shell-protocol.a`; packaged privacy is `declared=63 exports=63
  lvgl_exports=0 objects=1 headers=3`, and the sample's only dynamic host
  dependency is `libwayland-client.so.0`.

  Tooling is 79/79. Full CTest is 150/151: every `crtui` test and all other
  tests pass except the previously recorded
  `crtmedia_playback_pipeline_test_runs` wall-time check on this host, where
  `aplay -l` still reports no sound cards. No functional source change was
  needed for the Linux replay; the required clean-tree Wayland dependency
  build followed the repository's documented build-then-reconfigure sequence.
  Full evidence is in `docs/acceptance/crtui_acceptance.md`; External Surface is next.

- **`crtui` Tranche 4 replayed and accepted on macOS/arm64; a cumulative-stage
  build race was found and fixed.** The LVGL-enabled UI gate is 6/6; a fresh
  `CRTUI_ENABLE_LVGL=OFF` tree passes layout/input 2/2; direct layout, input,
  render and 40-cycle lifecycle coverage all pass. The in-tree demo and an
  externally rebuilt packaged `05-ui` sample both ran in real Cocoa windows
  with `presented=30 pixel_check=pass input_check=pass`; packaged privacy is
  `declared=63 exports=63 lvgl_exports=0 objects=1 headers=3`.

  The first `crt-ui-dist` run caught a real parallel race: stage-03 dist and
  stage-04 build were siblings, allowing two libc++ staging targets to refresh
  `dist/02-cxx` while a Skia demo linked. The two C++ SDK publishers are now
  serialized, and the stage-04 aggregate/direct Skia consumer waits for the
  complete verified `crt-gfx-simple-dist` predecessor; repeated full runs then
  built and verified every cumulative stage through `05-ui`. Tooling is 79/79.
  Full CTest is 157/158: only the known AVFoundation capture/encode lifecycle
  test fails repeatedly on this camera environment; all six crtui tests pass.
  Linux replay remains before global Tranche 4 closure; full evidence is in
  `docs/acceptance/crtui_acceptance.md`.

- **`crtui` Tranche 4 implemented and accepted on Windows/x64: CRT-owned
  layout, styling and the v1 widget set.** Added free/Row/Column/Stack layout
  with fill/grow/margin/padding/gap/alignment, ScrollView/List, the narrow style
  mask, Image, Switch, Checkbox and single-line TextInput, including committed
  text and editing keys through the crtgfx adapter. The private LVGL renderer
  maps these states to real pixels while the shared DLL still exports only the
  63 declared `crtui_*` functions (`lvgl_exports=0`). The layout engine was
  tightened before acceptance to use allocation-free multi-pass flex traversal,
  so OOM cannot silently leave geometry stale.

  New `crtui_layout_test` passes with LVGL both ON and OFF; input/render gates
  cover the new behavior and real pixels, including 40 lifecycle cycles. Fixed
  `crt-ui-test` to build every executable it invokes (layout/input, conditional
  render, and the macOS shared-contract test) before CTest. Windows results:
  crtui 5/5, full CTest 167/167 with one expected no-camera skip, real Win32
  `crtui_window_demo 30` green, and `crt-ui-dist` verified. The installed
  `examples/ui-basic` was rebuilt in a fresh directory from packaged `05-ui`
  only and ran against its packaged DLLs:
  `presented=30 pixel_check=pass input_check=pass`. macOS/arm64 and Linux replay
  remain open before global Tranche 4 closure; full evidence is in
  `docs/acceptance/crtui_acceptance.md`.
