# crtweb Acceptance (Stage `06-web`)

**Status: in progress -- Tranche 0 (scope, version and license freeze) is closed;
Tranche 1 (JavaScriptCore bring-up) is in progress: its ports (gperf, ICU) come first. No WebKit code is built yet.** Detailed contract and tranche order for the
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

**1A -- dependency inventory and ports.** Threads -> CRT pthread. ICU (`data`,
`uc`, `i18n`) and gperf are pinned and built as CRT port recipes
(`porting/recipes/icu.json`, `gperf.json`), not taken from the host: the host-ICU
shortcut would be fast on Linux and wrong on Windows/macOS. Ruby (>= 2.5) is a
build-host tool that the maintainer installs. Each port passes its own recipe
test before JSC is attempted.

**1B -- interpreter first-green.** JSCOnly, `EVENT_LOOP_TYPE=Generic`, JIT off.
Acceptance: arithmetic, objects/arrays, JSON, RegExp, Promise/microtasks,
UTF-8/Unicode (ICU), exceptions, a GC stress run, and repeated runtime
create/destroy with a leak check. Linux, then Windows and macOS.

**1C -- JIT.** `ENABLE_JIT=ON`: executable memory and W^X transitions, GC plus
JIT stress, exception/unwind, repeated execution. Kept separate so a failure is
attributed to basic runtime/PAL behaviour (1B) or to executable memory (1C).

**Linux/x86_64 result (2026-10-04): 1A and 1B green; 1C (JIT) and the Windows and
macOS replays remain.** `tools/build_webkit_jsc.py` builds JavaScriptCore from the
verified pin against an installed SDK (`02-cxx` here) and runs the acceptance; the
whole run from an empty work root takes about six minutes (build 297 s) and passes.

- *1A.* JSCOnly needs ICU >= 70.1 (`data`, `uc`, `i18n`), gperf and Ruby. ICU 78.3 and
  gperf 3.3 are CRT ports (`porting/recipes/icu.json`, `gperf.json`, both passing their
  recipe tests); Ruby is the one host build tool. The first-green configuration is the
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
- *Not covered.* The JIT (1C), any host but Linux/x86_64, aarch64 (native thread TLS is not
  implemented there, so threaded JSC would still misbehave), and the sampling profiler and
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

*Recorded, not fixed:* `libc++.so.1` records `NEEDED libatomic.so.1` (a host library); the
ICU shared libraries and `libc++abi.so` use the global-dynamic TLS model and so reference
the loader-provided `__tls_get_addr`, which is why the harness links with
`--allow-shlib-undefined`; the default thread stack is 1 MiB (as in Bionic), far less than
JavaScriptCore wants, so an embedder must size the stacks of threads that run script.

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
