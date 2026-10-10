# CRT History — August 2026

August established the portability/runtime foundation that became **`01-c`**
and brought the imported C++ runtime and early graphics/media work far enough
to form **`02-cxx`** and the beginnings of the later graphics stages.

This is a condensed archive of the original `HISTORY.md`. It preserves
completed engineering outcomes and acceptance evidence while omitting most
step-by-step debugging detail.

---

## Month at a glance

The project moved from a libc/PAL experiment into a reproducible source-porting
environment:

```text
Bionic-shaped libc/PAL
        |
        +-- shell / rootfs / process model
        +-- configure + make portability
        +-- upstream port recipes
        |
        +-- imported libc++ / libc++abi / libunwind
        |
        +-- window/input + Skia/FFmpeg groundwork
```

Major outcomes:

- `01-c` acquired a real shell/rootfs and configure-driven build environment.
- Windows gained practical `fork()`/`posix_spawn()` support sufficient for real
  upstream builds.
- The PAL surface expanded in response to actual ports rather than speculative
  POSIX completeness.
- A pinned imported LLVM C++ runtime became viable across the supported hosts,
  forming the basis of `02-cxx`.
- Skia, FreeType, FFmpeg, graphics-window, and media-frame integration moved
  from planning into real cross-host bring-up.
- CI and clean-tree testing became an increasingly important acceptance rule:
  reused build trees were repeatedly shown to hide dependency and staging bugs.

---

## `01-c` — libc, PAL, shell, rootfs, and source portability

### Shell and rootfs became core CRT artifacts

On 2026-08-02, `shell/` was established as part of the CRT runtime rather than
as an unrelated third-party port.

The stage included:

- `crt_tiny_sh`;
- Android `mksh`;
- Android `toybox`;
- an Android-like rootfs layout with `/system/bin`, `/bin`, `/usr/bin`, `/tmp`,
  `/dev`, and `/proc/self`;
- CRT-built GNU make as a bootstrap build tool;
- rootfs-aware configure recipe execution.

Windows shell execution was extended to support:

- cwd/environment propagation;
- fd snapshot import/export;
- close-on-exec and file actions;
- child tracking and `waitpid()`;
- socket descriptor transport;
- pipelines and basic redirection.

The goal was not shell feature parity for its own sake. The shell existed to
run real upstream `configure && make && make install` flows using CRT APIs.

### Windows process model became usable for real ports

A major August theme was making Unix-shaped build systems work on Windows
without abandoning CRT's Bionic/POSIX-facing model.

Important results included:

- initial `RtlCloneUserProcess` and spawn-broker experiments, superseded on
  August 6–7 by `CreateProcess` + memory-copy fork on arm64 and x86_64;
- startup address-layout control, register restoration, and runtime reset for
  the new fork model; the broker was retired, not the month-end architecture;
- correct fd inheritance and redirection across fork/spawn;
- shebang execution and executable-file recognition for scripts;
- fixes for shell subshell isolation and redirection;
- GNU make jobserver/parallel-build fixes;
- improved signal delivery on Linux/macOS and honest Windows fallback behavior
  at this stage.

These changes were validated through real upstream builds, especially zlib and
libpng, rather than only unit tests.

### The Bionic/POSIX surface grew from consumer pressure

Ports and shell work added or completed many missing APIs and headers,
including examples such as:

- `alloca.h`, `ar.h`;
- `memrchr`;
- `confstr`;
- `ttyname`, `getlogin`, `eaccess`;
- `putenv`;
- `bsd_signal`;
- `pselect`;
- process/signal helpers;
- additional stat/fd/spawn behavior.

The project increasingly adopted the rule:

> If an upstream Linux/Bionic-oriented project expects a reasonable runtime
> behavior, fix CRT/PAL first rather than patching the consumer.

### Configure-driven upstream ports became a real acceptance suite

By mid-August, CRT was rebuilding a growing collection of real upstream
software through its own sysroot, wrappers, shell, and make environment.

Work covered or advanced:

- zlib;
- libpng;
- bzip2;
- xz/liblzma;
- PCRE2;
- mbedTLS;
- curl;
- libffi;
- expat;
- FreeType and later graphics dependencies.

This work exposed general-purpose bugs in argv handling, tool invocation,
archive/linker selection, file metadata, pipe handling, and shell behavior.

The important milestone was not the number of libraries alone: CRT's port
recipes had become a regression system for the runtime and PAL.

---

## `02-cxx` — imported libc++ / libc++abi / libunwind

### Imported LLVM runtimes replaced the bootstrap-only C++ path

August brought the pinned LLVM runtime components into CRT as real build
artifacts:

- libc++;
- libc++abi;
- libunwind where applicable.

The work established both static and shared C++ runtime paths and began
treating them as an independent cumulative SDK layer above `01-c`.

Key issues solved included:

- runtime/source provenance and pinning;
- sysroot/rootfs installation;
- C++ ABI boundary smoke tests;
- exception/unwind support;
- `__dso_handle` handling for separate DSOs;
- static/shared linkage differences across Linux, Windows, and macOS;
- avoidance of accidental host C++ runtime leakage.

Windows static and shared libc++ execution, including real throw/catch, was
verified on August 21; the Windows imported-runtime migration was completed on
August 23. Linux and macOS also had runnable imported-runtime evidence. These
are dated runtime checks, not a claim that every later consumer was already green.

### Fresh-build discipline became part of acceptance

Several apparent libc++/graphics defects turned out to be stale build or
staging artifacts. Other failures only appeared after a genuinely clean
rebuild.

This led to an important project-wide rule:

> A reused `out/` directory is not evidence that new CMake, sysroot, or
> distribution wiring is correct.

Fresh configure/build/install runs increasingly became mandatory evidence.

---

## Early graphics and media groundwork

### Window/input and keyboard support matured

The graphics layer continued to evolve beyond a simple framebuffer/window
abstraction.

Linux work included a real libxkbcommon source port and keyboard integration.
Cross-host window/input tests were strengthened, while clean CI builds exposed
missing dependency edges that incremental builds had hidden.

### Skia bring-up moved from toolchain work to real rendering

Skia was pinned and integrated using the imported libc++ runtime. A substantial
amount of work was needed to keep Skia on CRT's POSIX/Bionic-shaped path even
on Windows and macOS.

By late August:

- Skia CPU/raster builds were real rather than placeholder configuration;
- FreeType integration was working;
- Skia smoke/coverage tests existed;
- Windows, Linux/WSL, and macOS had begun receiving real cross-host validation.

The work also exposed missing C++20/libc surface needed by Skia and clarified
where CRT should fix its own runtime versus patch upstream.

### Media-frame contracts connected FFmpeg-style data to Skia

By 2026-08-31, a host-neutral media-frame contract and Skia bridge were being
validated across the three primary hosts.

Tests covered:

- packed RGBA/BGRA ownership and wrapping;
- YUV420P conversion;
- release-callback lifetime;
- deterministic pixel checks;
- real Skia raster round trips.

This established an important design rule later used throughout `04-gfx-media`:

> `crtmedia` owns media contracts; `crtgfx` may bridge them into rendering,
> without making the media layer depend on graphics.

---

## Cross-host and CI hardening

August repeatedly found problems that were invisible on the original
development host or in reused build trees.

Important themes:

- Linux and Windows CI were repaired after macOS-focused libc++/Skia work
  introduced cross-host regressions.
- Windows x86_64 and arm64 were both exercised, exposing ABI-specific issues.
- Linux/WSL clean clones were used to verify assumptions that had previously
  only been reasoned about.
- macOS real hardware runs caught Apple-specific loader, SDK, and ABI problems.

This established the later host policy used by the upper-runtime roadmap:
implement on one host, but replay early enough on the other hosts that platform
assumptions cannot accumulate.

---

## End-of-month state

By the end of August:

### Effectively established

- Bionic-shaped C runtime/PAL;
- CRT shell/rootfs and upstream build environment;
- configure/make based source-port workflow;
- large and growing upstream recipe suite;
- imported libc++/libc++abi runtime;
- native window/input foundation;
- Skia CPU/raster path;
- FFmpeg/media-frame integration groundwork.

### Still to be completed in September

- formal cumulative stage separation and isolated predecessor-only builds;
- native GPU presentation on all hosts;
- full `03-gfx-simple` closure;
- full `04-gfx-media` closure;
- hardware video decode;
- decoded GPU texture interop;
- encode/capture;
- networking/streaming;
- public preview packaging.

Those items are summarized in `HISTORY-2026-09.md`.

---

## Historical note

The detailed original August entries contained extensive root-cause narratives
for individual toolchain, shell, PAL, ABI, and stale-build defects. Those
details remain recoverable from Git history and specialized design documents;
this archive intentionally records the resulting capabilities and design
decisions instead.


## Investigation and reproduction checkpoints

Read the dated entries in the pinned original log (see [archive guide](README.md))
for exact command lines, recipe revisions, and intermediate failures. These are
historical results; the commands and source layout must be read at that revision.

| Dates | Failure and resolved cause / decision | Evidence to replay or inspect |
| --- | --- | --- |
| Aug 2–7 | A cloned Windows process could not safely perform ordinary process creation. The broker introduced orphan/pipe/timeout failures; it was retired in favor of fresh-process memory-copy fork. arm64 cannot use `/DYNAMICBASE:NO`, so startup relaunch uses process mitigation attributes. | `fork_test`, `fork_signal_test`, `fork_runtime_reset_test`; Aug 6 x64-on-arm64 emulation passed 79/79, explicitly not native x64 proof. See [fork investigation](../bringup/windows_fork_emulation_history.md). |
| Aug 10–11 | The allocator's fixed OS-region registry silently omitted later regions from fork copies; fd snapshot filtering also dropped close-on-exec descriptors before spawn file actions could duplicate them. These were PAL defects exposed by libffi and parallel make. | Original Aug 10 allocator and Aug 11 fd-snapshot entries, `windows_fd_snapshot_test`, real zlib/libpng configure/make runs. Preserve action ordering: filter inheritance after file actions. |
| Aug 11–12 | Missing startup constructor/destructor execution and PE runtime pseudo relocations blocked real shared consumers. A static-only compile could not establish correctness. | Cross-OS constructor/destructor regression and static/shared recipe smoke results; the bzip2/xz entries record their separate host outcomes. |
| Aug 15–17 | Consumer pressure exposed libffi arm64 register corruption, Darwin ancillary-message layout differences, missing process-shared synchronization, and context-restore defects. Windows stop/resume job control remained a deliberate deferral. | `porting/recipes/libffi.json` repeat-call checks, [signal policy](../design/signal_delivery.md), [job-control policy](../design/job_control.md), and the dated PAL entries. An API/header addition is not proof of equal backing on every host. |
| Aug 21–23 | Windows imported C++ needed a consistent MinGW target and CRT-owned DWARF unwinding; static-library CMake probes could falsely report link capabilities. Mixing MSVC/UCRT headers into ordinary CMake consumers produced duplicate runtime symbols. | `crt-libcxx-build`, `crt-libcxx-smoke`, static/shared `imported_libcxx_test`, native-callback boundary tests; [C++ runtime policy](../design/cxx_runtime.md). Read the Aug 23 completion after the earlier partial results. |
| Aug 24–30 | Skia archive creation did not prove usable fonts or rendering. Empty font managers, stale staging/dependency edges, and event-completion ordering required real consumer tests. | FreeType recipe static/shared smoke, `crtgfx_skia_raster_smoke`, CPU coverage, window/event tests. Rebuild the actual tests after configuration changes. |
| Aug 31 | CRT's 4-byte `wchar_t` was passed to Win32's 2-byte UTF-16 APIs; a leaked `_LIBCPP_WIN32API` undef additionally changed client filesystem behavior. | Explicit `char16_t` conversion at OS boundaries, macro push/pop isolation, filesystem round trips including directory iteration and sizing calls. The original entry identifies the affected recipe patches. |
| Aug 31 | Windows checkout CRLF broke Unix wrapper shebangs even though Git blobs were LF. | `.gitattributes` and a fresh checkout; a missing display in WSL was a separate environment limitation, not a line-ending regression. |

The late-August media-frame result covers ownership/wrapping, conversion, pixel
checks, and the Skia bridge. Do not promote it to completed FFmpeg playback,
hardware decode, or isolated `04-gfx-media` acceptance: those follow in September.
