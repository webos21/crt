# TODO: CRT Shell, Rootfs, And Porting Loop

This file tracks the shell/rootfs/porting work queue. Only current work and
planned follow-up stay here; completed work moves to `HISTORY.md` in reverse
chronological order. Detailed policy and provenance stay in `docs/` and import
manifests.

## Notice

- Keep recipe statuses current in
  [`porting/recipes/*.json`](porting/recipes/) and
  [`docs/porting_status.md`](docs/porting_status.md) whenever a host is
  rerun. Porting policy and the normal configure/make loop live in
  [`docs/sysroot_ports.md`](docs/sysroot_ports.md); completed porting
  investigations belong in [`HISTORY.md`](HISTORY.md).
- A port is not done until both static and shared builds are attempted
  in the same pass on each host, with any host-specific deferral recorded
  in the recipe notes and status matrix. See
  [`docs/porting_status.md`](docs/porting_status.md) for status meanings.
- For CMake wiring changes, do not trust a long-lived local `out/`
  directory. Verify with a fresh clone or at least
  `cmake --fresh --preset <preset>` before calling the change done; stale
  `CMakeCache.txt`/rootfs artifacts have hidden real CI-only ordering
  bugs before. The resolved cases are recorded in [`HISTORY.md`](HISTORY.md).
- Keep toybox applet enablement tied to audited CRT/PAL support, especially
  LLP64 assumptions on Windows. The live applet list and deferrals are in
  [`docs/toybox_applet_status.md`](docs/toybox_applet_status.md).
- Keep terminal/tty behavior coherent for shell and configure use. Current
  syscall/ioctl coverage is tracked in
  [`docs/sysroot_ports.md`](docs/sysroot_ports.md), with interactive job
  control policy deferred in [`docs/job_control.md`](docs/job_control.md).
- Treat `CRT_SPAWN_NATIVE_WINDOWS=1` as a narrow launcher hint for native
  host tools only. The wrapper details live in [`tools/crt-cc`](tools/crt-cc),
  [`tools/crt-c++`](tools/crt-c++), [`tools/crt-native-tool`](tools/crt-native-tool),
  and [`docs/sysroot_ports.md`](docs/sysroot_ports.md).
- If a new public libc or `__crt_sys_*` symbol is added, regenerate or replace
  [`porting/recipes/mbedtls-windows-exclude-symbols.rsp`](porting/recipes/mbedtls-windows-exclude-symbols.rsp)
  in the same pass. The reason is documented in
  [`porting/recipes/mbedtls.json`](porting/recipes/mbedtls.json) and
  [`docs/porting_status.md`](docs/porting_status.md).
- Keep work status in exactly one place per purpose, not restated across all
  three: [`HISTORY.md`](HISTORY.md) holds the detailed, dated record of what
  was actually done and why; an item here in `TODO.md` should track live
  progress in a line or two, not re-narrate what a finished sub-part already
  accomplished (once something is done, move the detail to `HISTORY.md` and
  cut it here rather than leaving both). [`STATUS.md`](STATUS.md) is updated
  only when explicitly asked for, not as part of routine documentation
  passes -- do not touch it on a normal work/doc-cleanup turn.


## Done

See [`HISTORY.md`](HISTORY.md) for the full, dated, reverse-chronological
record of completed work. This section stays empty in `TODO.md` itself --
when an item below is finished, move its writeup into `HISTORY.md` (dated,
newest entry first) rather than leaving it here.

## In Progress

Active threads, not a flat list of one-off items.

### Upper runtime roadmap

The completed CPU baseline is summarized in [`STATUS.md`](STATUS.md); the full
dated implementation trail lives in [`HISTORY.md`](HISTORY.md) -- software-
decode evidence, the WSL `memfd_create()` fix behind `crtgfx_window_smoke`,
`libcrtmedia`'s format/extractor/codec/demuxer core, the player clock/state
machine and all three host audio sinks, the software player, the common GPU
resource contract, Skia GPU rendering on Linux/Vulkan + Windows/D3D12 +
macOS/Metal, and GPU presentation (resize/swapchain recreation, Ganesh-to-
surface rendering) are all done and verified on Linux, Windows, and macOS.
The product target is the staged embedded/native runtime described in
[`docs/runtime_roadmap.md`](docs/runtime_roadmap.md); Electron/Chromium
remains only a long-term portability benchmark, not the product definition.

Still open within "live GPU presentation everywhere": macOS x86_64 native
execution; pixel-exact (not just visually-by-eye) post-render framebuffer
verification on macOS; live resize composed with the Ganesh-wrap path
specifically on macOS (each verified separately, not yet together); and
pixel-exact (not just liveness) post-resize verification on Windows.
Graphite stays a later, separately-measured alternative to Ganesh.

1. **Add hardware decode, phase A.** The opt-in decode-side API and its
   fallback/recovery contract are done and verified on Windows and Linux
   (`HISTORY.md`'s 2026-09-08 entry). Actually enabling the real per-host
   hwaccel in the FFmpeg build itself is not done -- two real, separate
   blockers, neither fixed: (1) Windows `--enable-d3d11va` fails
   configure's own `ID3D11VideoDecoder`/`ID3D11VideoContext` dependency
   check, likely conflicting with this project's `-U_WIN32`-style
   portable-path convention; (2) Linux -- this WSL environment's own
   `ar`/`nm` (Binutils 2.46) segfaults (`Relink ... for IFUNC symbol
   'ceil'`) archiving `libavformat.a`/`libavcodec.a` against `libm.so`
   even on a plain baseline configure, a real external binutils
   regression blocking any fresh FFmpeg rebuild here right now, unrelated
   to VA-API itself. macOS is entirely unattempted (no hardware in these
   sessions).
2. **Add hardware decode, phase B.** Connect decoder-owned textures
   directly to Skia: D3D resources on Windows, `CVPixelBuffer`/Metal
   textures on macOS, and VA-API/DRM PRIME/dmabuf/Vulkan images on Linux.
   Tie decoder frame-pool release to real GPU/presentation completion and
   retain CPU-copy fallback.
3. **Add hardware encode and capture.** Stage camera, microphone, and
   screen sources; hardware H.264/HEVC encode; mux/record; latency,
   bitrate, and key-frame controls.
4. **Expand network and streaming.** Add custom I/O and HTTP range first,
   then buffering, reconnect/discontinuity handling, and HLS/DASH or the
   justified FFmpeg protocol subset.
5. **Add an optional WebRTC-shaped realtime layer.** Define source/track/
   sink and execution-context contracts before deciding whether to port
   full WebRTC for RTP/RTCP, jitter buffering, congestion control, and
   AEC/NS/AGC.
6. **Expose the service through `libcrtjs`.** Start the QuickJS engine,
   event loop, timers, modules, and native binding work now while steps
   1-3 continue in parallel. Bind media only after the extractor/codec/
   player contracts are stable, using WebCodecs-like chunks/frames/queue
   semantics and a higher-level asynchronous player API. V8 and a minimal
   Chromium/Ozone probe remain later consumers of the same contracts.

Execution order: Skia GPU, hardware decode, and the QuickJS core proceed in
parallel (the GPU resource contract that gated them is done); zero-copy
completion gates the GPU-aware JavaScript media binding, but not the
initial QuickJS bring-up.

### CRT distribution stages (01-c -> 05-js)

Full design in `docs/refine/interim-restructure.txt`; published contract in
[`docs/distribution.md`](docs/distribution.md). Full dated trail (the
`crt-c-dist`/`crt-libcxx-dist`/`crt-gfx-simple-dist`/`crt-gfx-media-dist`/
`crt-js-dist` chain, the top-level `tests/` split into `libc/tests` +
`libstdc++/tests`, the `libcrtgfx` physical window/GPU/Skia split, and every
bug found along the way) is in [`HISTORY.md`](HISTORY.md).

**Done and verified on real hardware on all three hosts -- Linux, Windows,
macOS (2026-09-09):** the full `01-c` through `05-js` cumulative dist chain,
and the `libcrtgfx` window/GPU/Skia physical split. Two real macOS-only
bugs were found and fixed along the way: `crtgfx_gpu_shared`'s missing
`-lobjc` (`gpu_metal.c`'s direct `objc_msgSend`/... calls, never propagated
from `crtgfx_window_shared`'s `PRIVATE`-scoped framework list --
`f1596c8`), and `crt-libcxx-dist`'s missing dependency on the bootstrap
`cxx`/`cxx_shared` targets (`446540c`).

Still open:
- The default packaging pass everywhere has `CRTGFX_ENABLE_SKIA=OFF` and
  `CRTMEDIA_ENABLE_FFMPEG=OFF`; an option-ON Skia/FFmpeg distribution pass
  has not been run.
- `verify_dist.py`'s acceptance checks are still thinner than
  `docs/distribution.md`'s own "Distribution Acceptance" list -- no path-
  with-spaces check, no absolute source/build path leakage check (debug
  artifacts currently contain those paths, so path remapping or release
  stripping must be decided first), no external CMake/configure-make
  consumer check.
- The `<iostream>` static-init crash found packaging `02-cxx` on Windows
  (a real, reproducible segfault via this project's own DWARF-unwind
  safety net; `<cstdio>` works) is not root-caused -- suspected
  `std::ios_base::Init`'s global-constructor path through the Windows
  `.ctors` walker, not confirmed, and not yet reproduced against the
  in-tree (non-packaged) build.
- The `sysroot`/`crtgfx_skia_objects` dependency-cycle comment in the
  top-level `CMakeLists.txt` needs reconfirming now that `sysroot` no
  longer `DEPENDS` on any `crtgfx*` target at all.

### Hands-on distribution SDK and isolated stage acceptance

Turn each packaged stage into the bootstrap SDK for the next one, while the
CRT GitHub repository remains the meta-toolchain that produces the binary and
source packages. Add versioned stage recipes whose immutable CRT GitHub
Release source assets are pinned by URL, source commit, size, and SHA-256:
`01-c` must fetch/build/test `02-cxx`; `02-cxx` must fetch/build/test
`03-gfx-simple` and then `04-gfx-media` (with `04` consuming the installed
`03` result). Keep the compiler/linker external, use the packaged CRT
mksh/toybox environment for configure/build commands, and never require
MSYS/Git Bash. Extend the same recipe model to `04 -> 05-js` once the first
three transitions are stable.

**Done and verified on all three hosts (2026-09-09):** the `02-cxx ->
03-gfx-simple` deterministic asset, schema-v2 OS identity, standalone build
entry point, and isolated stage-build acceptance (configure/build/ctest/
install/example-rebuild-and-run/`verify_dist.py`, from a path containing
spaces). The Linux source path also carries and builds the pinned
libxkbcommon port instead of borrowing its headers or library from the host.
Four real macOS-only bugs (`DYLD_LIBRARY_PATH` poisoning host tools, a
missing `-fcrt-real-apple-sdk` sentinel in `crt-cc`, `--sysroot=` fighting
`-syslibroot`, and `CMAKE_SYSROOT`-relative RPATH stripping) and one
Windows-only bug (`mksh` backslash-path exec) were found and fixed along the
way -- full trail in `HISTORY.md`. Windows was re-verified clean against the
final, merged fix set.

**The isolated `01-c -> 02-cxx` upgrade is also now verified on macOS
(2026-09-11).** A real, confirmed bug found running it for the first
time from a real packaged `01-c`: a packaged `02-cxx` SDK never carried
the pinned recipe to advance it one stage further
(`stages/recipes/03-gfx-simple.json`), so the isolated upgrade's own
final `verify_dist.py` step failed outright. Fixed (`d2eab26`,
`HISTORY.md`'s 2026-09-11 entry) with a new `tools/create_stage_source.py
--successor-recipe` option that embeds an already-generated successor
recipe inside a stage's own source asset; `build_stage_02_cxx.py` copies
it forward. Verified end to end: a fresh `01-c -> 02-cxx -> 03-gfx-simple`
chain, all isolated, all from real packaged SDKs.

**In progress:** `03-gfx-simple -> 04-gfx-media`, with Skia and FFmpeg
actually enabled for this transition's acceptance (the default packaging
pass everywhere still has `CRTGFX_ENABLE_SKIA=OFF`/
`CRTMEDIA_ENABLE_FFMPEG=OFF`, which this transition must not settle for).
Design is written up in full at
`C:\Users\Lee\.claude\plans\soft-orbiting-swan.md` (approved 2026-09-09) --
read that before continuing rather than re-deriving the design. Key
decision already made: Skia/FFmpeg/FreeType are fetched and built live by
the stage entrypoint script via their own already-pinned mechanisms
(`libcrtgfx/third_party/skia/recipe.json` + `fetch_skia.py`/`build_skia.py`;
`porting/recipes/{ffmpeg,freetype}.json` + the already-packaged
`crt-port-build.py`), not bundled into the stage-source tarball the way
03's small xkbcommon dependency was -- Skia alone is ~189MB even shallow.

Landed so far (Windows-verified): shared modules now own the reusable target
definitions for all three layers needed by the standalone stage project:
`libcrtgfx/cmake/crtgfx_gpu_{sources,targets}.cmake`,
`libcrtgfx/cmake/crtgfx_skia_targets.cmake`, and
`libcrtmedia/cmake/crtmedia_targets.cmake`. The Skia module retains the
existing Windows `uuid.lib`/`--allow-multiple-definition`/
`emutls_link_stubs.c`, macOS reversed archive-scan order, and Linux
`--start-group`/`--end-group` rules; the media module retains the backend
OBJECT-library boundary and FFmpeg static/shared link rules. In-tree-only
targets are guarded so the same modules can be called by the main build and
the future standalone project. Fresh Windows builds passed gfx 3/3 and media
5/5 tests; a fresh default-option `crt-gfx-media-dist` rebuilt and verified
the complete `02-cxx -> 03-gfx-simple -> 04-gfx-media` packaging chain. A
Skia-ON configure/generate pass also exercised all three extracted Skia
functions; the real Skia compile/link remains part of the isolated option-ON
acceptance below. The standalone
`distribution/stages/04-gfx-media/CMakeLists.txt` is also present and passes
a Windows configure/generate check through the installed 03 SDK wrappers. It
requires Skia, FreeType, and FFmpeg artifacts up front, consumes the installed
03 window libraries, registers GPU/Skia/media smoke tests, and installs the
GPU/Skia examples; this check used placeholders and therefore is structural
evidence only, not option-ON build acceptance.

Stage checklist (remove completed items from this in-progress list only after
their result is recorded in `HISTORY.md`):

- [ ] Run the real Windows isolated acceptance from freshly extracted inputs
  under a path containing spaces; expect a multi-hour Skia/FFmpeg build and
  record every discovered defect before removing this in-progress section.
  The Windows PAL/mksh, external-toolchain fail-fast, and libc++ header-order
  defects found by the preceding full runs are fixed and recorded in
  `HISTORY.md`. The latest run got through fresh FreeType/FFmpeg installs,
  all 834 pinned Skia/D3D12 Ninja edges, and standalone configure before the
  header-order failure; no final output was published. Focused compilation now
  passes both with the updated wrapper's automatic packaged-libc++ discovery
  and with the compatibility environment used for an older predecessor SDK.
  That tested source asset had SHA-256
  `454f812f52bbd12c65b6f0502ca9f9c32d58e009ccbec3c2a550defd8ee620cb`.
  The full rerun from that asset confirmed the header-order fix: FreeType and
  FFmpeg installed, Skia completed 834/834 edges, and standalone compilation
  passed the former `<cwchar>` failure. It then exposed the next packaging
  gap: installed `include/include/core/SkColorSpace.h` includes
  `modules/skcms/skcms.h`, but `build_skia.py` currently installs only Skia's
  top-level `include/` tree and `libskia.a`. The required two-file public
  skcms include closure is now installed and enforced by standalone configure,
  dependency provenance, and final dist verification; focused installation
  and pinned-header compilation pass. The fresh asset generated from committed
  fix `8b6bfc4` has SHA-256
  `3c20e93eb10b4502b704d4be9d63e53328b18870e558e0e001731e9ba4c48082`.
  Rerun it; no final output has yet been published.

  A deliberately space-containing install prefix then exposed another real
  acceptance defect: FFmpeg constructs `-I${prefix}/include` as an unquoted
  compiler option, so Clang receives separate `Install` and `19/include`
  operands. Keep this recorded rather than hiding it; first rerun with only
  the work/source path containing spaces and a no-space install prefix to
  finish separating the PAL fix from FFmpeg prefix quoting. The next run is
  the complete stage entrypoint from the fresh asset and one fresh root: it
  must rebuild FreeType and FFmpeg, fetch/build pinned Skia against staged
  FreeType, build/test/install the standalone 04 project, rebuild/run the GPU
  and Skia examples, inventory all three redistributed port libraries, and
  pass `verify_dist.py` before publishing the output.
- [ ] Repeat the final isolated acceptance on Linux and macOS before calling
  the transition complete. WSL `Ubuntu-26.04` already has CMake, Ninja,
  Clang, and Python, but this checkout currently has no Linux 03 predecessor
  SDK; build the Linux 01->02->03 chain there first. Treat CPU/FFmpeg/Skia
  results separately from Vulkan/native-Wayland live-presentation evidence.

Keep the separately listed Windows C++ initialization failure open; the stage
runner deliberately has no retry that could hide it.

The pre-existing `crt-media-test` dependency gap found during this work is
fixed: `crt-media-build` now builds all five executables that its CTest filter
runs, and a fresh Windows invocation passed 5/5 without a manual precursor
build (full evidence in `HISTORY.md`).

Acceptance must start from freshly extracted archives in a path containing
spaces, reject access to in-tree CRT headers/libraries/build artifacts,
rebuild and run the packaged examples, and exercise representative configure,
amalgamation, and dependency-chain ports -- apply the same bar to
`03-gfx-simple -> 04-gfx-media`.

The first concrete implementation of the redistributable-dependency rule is
in place for Linux Simple Graphics: `03-gfx-simple` carries libxkbcommon's
public headers, static library, license, and recipe provenance, records them in
the structured `redistributed_dependencies` manifest inventory, and
`verify_dist.py` rejects a missing declaration or file. Generalize that model
to later graphics/media ports and transitive dependencies; add generic
manifest-driven verification rather than the current xkbcommon-specific
check, scan binaries for undeclared non-system `.so`/`.dylib`/`.dll`
dependencies, and explicitly inventory OS/framework/device-driver
prerequisites that remain outside the archive.

Path-with-spaces coverage exposed argument flattening in the compiler wrappers.
Linux/macOS `crt-cc` now preserves the original argument vector and the staged
libxkbcommon build exercises that fix. Windows `crt-cc` and every `crt-c++`
host path still use transformed/flattened argument strings; the Windows stage
builders currently copy inputs to a short temporary path. Track and fix those
remaining wrapper cases rather than treating that relocation as final path-
with-spaces acceptance.

## Planned

### Focused CRT/PAL follow-ups

These are real remaining limitations, but none blocks the completed
`libcrtgfx` CPU-raster milestone. Promote one into active work when a consumer
or host investigation supplies the required evidence.

- Extend the resolver from its current synchronous UDP IPv4/A-record baseline
  when IPv6, TCP fallback, search domains, or caching becomes a consumer
  requirement.
- Complete cross-process signal delivery and meaningful `SIGCHLD` `siginfo_t`
  data before enabling toybox `timeout`.
- Revisit a CRT-owned ELF loader/Android-linker boundary only after a real
  upper-runtime consumer requires behavior the host loader adapter cannot
  provide.
- Harden FreeType's fetch beyond the single SourceForge URL fix (`5b87197`)
  -- add retry-on-transient-failure, a documented fallback mirror, and
  SHA-256 verification of the cached archive before reuse, matching the
  reliability bar other `porting/recipes/*.json` ports already meet.

### Interactive job control (deferred until it's an actual priority)

`docs/job_control.md`'s "Interactive Job Control" section has the decided
design for all three pieces below; nothing here is implemented yet, and this
project's own mksh build has job control compiled out entirely on every host
(`MKSH_NOPROSPECTOFWORK`), not just Windows -- see that section for why this
is forward-looking policy, not a current gap being actively worked.
Re-evaluated (2026-08-16) against `docs/runtime_roadmap.md`: none of the
planned upper-runtime components (`libcrtjs`/QuickJS+V8, `libcrtgfx`, `libcrtmedia`)
actually depend on POSIX job-control signals (`SIGSTOP`/`SIGTSTP`/`SIGCONT`)
or real fg/bg switching -- confirmed genuinely optional infrastructure, not
something blocking the roadmap. (V8's own "signal/process behavior"
prerequisite in that doc is a separate matter -- `SIGSEGV`-trap-based WASM
bounds checks and `SIGPROF`-style profiling, the "vectored exception
handling" question `docs/signal_delivery.md` already tracks independently,
answerable with fully documented Windows APIs.) A full Windows stop/resume
implementation would also need reversing this project's "avoid undocumented
NT internals" pattern (`NtSuspendProcess`/`NtResumeProcess` -- see
`docs/job_control.md`'s own "Stopped-child status" note for the design that
was investigated and the alternatives ruled out). Stays deferred.

- Bridge `SetConsoleCtrlHandler` (`CTRL_C_EVENT`/`CTRL_BREAK_EVENT`, both to
  `SIGINT`) into `signal_actions[]`/`raise()`, mirroring `SIGCHLD`'s existing
  pending-flag-plus-checkpoint pattern (`docs/signal_delivery.md`).
- Track the real Windows process-group id behind this project's own
  CRT-managed `pgid` integer once a job is actually spawned into a new
  process group, so `tcsetpgrp()` and a targeted `CTRL_BREAK_EVENT` have a
  real id to act on.
- Re-enable `MKSH_UNEMPLOYED` (mksh's own job control) once the above exists,
  and only then decide whether stopped-child (`WIFSTOPPED`) support is worth
  the low-level Windows work it would need -- `docs/job_control.md` currently
  keeps that explicitly out of scope.

### Toybox applet expansion (deferred until it's an actual priority)

Only when the backing Bionic-compatible CRT/PAL surface exists.
Full applet-by-applet status (what's enabled,
what's still open and why, the deferred-applet list with each one's
concrete reason, and the `globals.h`/`flags.h` registration traps found
while enabling `df`/`stty`) now lives in
[`docs/toybox_applet_status.md`](docs/toybox_applet_status.md) -- this
bullet stays a pointer. Still open there: `expand`/`logger`/`fold`/
`uudecode`/`cal`/`split`/`strings` (a `globals.h` fix, plus a per-applet
`flags.h` check); `timeout` (hang fixed, two deeper gaps remain: real
`SIGCHLD` `siginfo_t` data, cross-process `kill()`); and a confirmed-not-
guessed deferred list (`ps`/`top`/`iotop`/`pgrep`/`pkill`, `mount`/
`umount`, `ifconfig`, `login`, procfs-heavy commands).
