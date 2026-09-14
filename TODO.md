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

Active threads, not a flat list of one-off items. The cumulative binary-package
chain through the current `05-js` skeleton and the physical `libcrtgfx`
window/GPU/Skia split are complete. Predecessor-only isolated-stage
build/package/verify acceptance through the option-ON
`03-gfx-simple -> 04-gfx-media` transition is complete on Windows and macOS;
the dated evidence belongs in [`HISTORY.md`](HISTORY.md). **On Linux it is
not complete**: its prerequisite/ELF inventory, low-level Vulkan presentation,
and independent path-with-spaces evidence pass, but the isolated stage-build
tool runs both packaged examples *before* `verify_dist.py`/atomic publish and
`crtgfx_skia_example` crashes there every time. The tool therefore has never
reached `verify_dist.py` or produced a published `dist/04-gfx-media` on Linux
at all. An earlier revision of this paragraph and commit `15fcb1e` overstated
that result; see `HISTORY.md`'s 2026-09-14 corrections and the dedicated item
below.

- [ ] **Root-cause and fix the `SkSL::stod`/`strtof_l` crash blocking live
  Linux `examples/gfx-skia` presentation.** 100% reproducible as of
  2026-09-14 (re-confirmed across three independent isolated-stage rebuilds
  this session; `crtgfx_gpu_example` is unaffected). Root cause is now fully
  understood -- see `HISTORY.md`'s 2026-09-14 correction entry for the full
  trace: real glibc's own `strtof_l` (not this project's own, correctly
  implemented one in `libc/src/locale_l.c`) gets called with uninitialized
  glibc-internal locale/TLS state and crashes, because
  `crtgfx_skia_example`'s `DT_NEEDED` order lets real glibc's `libc.so.6`
  (pulled in transitively by `libvulkan.so.1`/`libwayland-client.so.0`) win
  the ELF dynamic linker's breadth-first global-scope search for the
  unversioned `strtof_l` symbol, ahead of this project's own `libc.so`. A
  naive fix (reordering `target_link_libraries()` so `libc++.so.1`/
  `libunwind.so` link first) does fix this but was found to regress a
  separate, already-fixed collision -- `readdir`/`opendir` then
  incorrectly bind to this project's own `libc.so` instead of real glibc,
  breaking Vulkan device enumeration (`device_count=0`) -- so it was
  reverted. This is a genuine architectural tension: this project's own
  `libc.so` unversioned-exports the entire POSIX symbol surface as
  global-default, which cannot simultaneously satisfy both symbol families'
  correct resolution via link order alone.
  **Chosen direction: give `libc.so` its own private
  ELF symbol-version namespace** (e.g. a single `CRT_1.0` node covering the
  whole export surface via `-Wl,--version-script=`, reusing the same
  mechanism the `libunwind.so` `__register_frame` fix already proved out).
  Versioned symbol lookup filters breadth-first global-scope candidates by
  version-string match rather than "first found wins", so once every
  consumer that calls into `libc.so` (`libc++.so`/`libc++abi.so`/
  `libunwind.so`/`libcrtgfx.so`/every executable) is rebuilt against the
  newly-versioned `libc.so`, `strtof_l@CRT_1.0` and `readdir@CRT_1.0`
  correctly skip real glibc's `@GLIBC_2.17` definitions (and vice versa) --
  independent of link order, resolving the tension above without picking a
  side. A single whole-surface version node is preferred over scoping only
  the `_l`-suffixed locale family: it needs no per-symbol-family safety
  judgment and mirrors how real glibc versions its own entire export
  surface; fall back to narrow scoping only if whole-surface versioning is
  found to break something real glibc-linked-library interop depends on.
  Before touching the real `libc.so` build (which forces a full
  `libc++`/`libc++abi`/`libunwind`/`libcrtgfx` rebuild plus the ~13-minute
  isolated Skia stage rebuild per iteration), validate the version-filtered
  resolution mechanism itself with a small, disposable two-`.so` repro
  (`readelf --version-info`/`LD_DEBUG=bindings`) outside the real build.
  **Done, 2026-09-14, confirmed:** a throwaway four-`.so` repro (two "base"
  libs each exporting a same-named symbol returning a different value --
  one under a `GLIBC_2.17`-style node standing in for real glibc, one under
  a private `CRT_1.0` node standing in for a re-versioned `libc.so`; two
  "consumer" libs, each linked against one base lib so its own `DT_VERNEED`
  records that specific version) reproduced the real bug exactly when built
  *without* versioning -- with the "glibc" base lib listed first in
  `DT_NEEDED` order (mirroring `crtgfx_skia_example`'s real order), the
  "CRT" consumer incorrectly bound to the "glibc" base lib's definition
  (`LD_DEBUG=bindings` showed both consumers binding to the same, first-found
  library). Rebuilding the identical layout *with* the version scripts fixed
  it: each consumer correctly bound to its own base lib's definition
  regardless of `DT_NEEDED` order (`LD_DEBUG=bindings` showed
  `` `foo' [GLIBC_2.17]`` and `` `foo' [CRT_1.0]`` resolving to their
  respective libraries). This confirmed the chosen direction mechanically
  before paying the real rebuild cost. The whole-surface `CRT_1.0` map is now
  implemented in the real Linux `libc.so` build and validated through a clean
  libc++ rebuild on WSL2 Ubuntu 26.04/x86_64; `libc++.so.1` records
  `strtof_l@CRT_1.0`. That host also carries two minimal,
  real-provider reproductions (2026-09-14; full evidence in `HISTORY.md`): a
  plain CRT C executable linked to host `libvulkan.so.1` fails physical-device
  enumeration with `VK_ERROR_INCOMPATIBLE_DRIVER` when its CRT `opendir`/
  `readdir` exports remain visible, while a shared-libc++ locale-stream probe
  using `--exclude-libs,ALL` keeps Vulkan's directory calls on glibc but binds
  libc++'s `strtof_l` to glibc and SIGSEGVs. With the new map, both probes
  pass simultaneously and `LD_DEBUG=bindings` shows Vulkan directory calls
  reaching `GLIBC_2.2.5` while libc++ locale parsing reaches `CRT_1.0`.
  Proceed by merging the independently tested native-Linux arm64 work,
  preserving these two cases as a permanent Linux-only regression, and then
  completing the full isolated Linux 04 transition on native hardware. Do
  not accept link-order changes as a fix.

### Distribution hardening

Harden the completed cross-host distribution baseline before adding another
large upper-runtime dependency. Work in independently verifiable tranches and
move each completed tranche into [`HISTORY.md`](HISTORY.md):

1. **Binary dependency and absolute-path policy.** Scan installed binaries for
   undeclared non-system `.so`, `.dylib`, or `.dll` dependencies. Decide path
   remapping or release stripping before rejecting absolute source/build/debug
   paths, then enforce the selected policy in `verify_dist.py`. The same
   macOS binary-import audit above found a concrete instance of the absolute-
   path question this item still has to decide: FreeType's own installed
   `lib/libfreetype.6.dylib` (declared a `runtime_artifact`, but currently
   unused -- Skia links `libfreetype.a` statically, and nothing else loads
   it) carries an absolute build-time temp path as its own `LC_ID_DYLIB`
   (GNU Libtool's Darwin install-name computation baking in `$(prefix)`
   as given at build time), not a portable `@rpath/...` spelling. Harmless
   today only because nothing dynamically links it; fix by giving
   FreeType's own macOS dylib link recipe an explicit `-install_name
   @rpath/$@` patch (same shape as `porting/recipes/mbedtls.json`'s
   existing one) as part of deciding this item's policy.
2. **External consumers and path regression.** Add explicit external
   CMake/configure-make consumer checks where the stage contract requires them.
   Preserve the now-complete three-host space-containing-prefix run as a
   regression acceptance requirement.
3. **Deferred `05-js` isolated stage.** Extend
   `04-gfx-media -> 05-js` only after the real QuickJS core and bindings exist.
   Apply the same pinned asset, predecessor-only build, test, external-consumer,
   verification, and atomic-publish contract as the earlier transitions.
## Planned

### Upper runtime roadmap

The completed cross-host baseline and its exact validation evidence stay in
[`STATUS.md`](STATUS.md) and [`HISTORY.md`](HISTORY.md); the product boundary
and dependency order stay in [`docs/runtime_roadmap.md`](docs/runtime_roadmap.md).
Promote one tranche at a time into In Progress when its prerequisite evidence
and acceptance host are available.

1. **Finish live GPU presentation evidence.** Close the remaining macOS/x86_64
   live path, pixel-exact macOS checks, resize-plus-Ganesh coverage on macOS,
   and pixel-exact resize coverage on Windows. Keep Graphite as a later backend
   rather than part of the current Ganesh acceptance bar.
2. **Hardware video decode.** Complete Phase A backend bring-up and tests.
   The current blockers are the Windows D3D11VA configure path, a WSL-hosted
   binutils 2.46 `ar`/`nm` crash, and an unattempted macOS pass. Preserve a
   software fallback and report hardware use separately from decode success.
3. **Zero-copy decoded textures.** Add explicit ownership and synchronization
   for D3D surfaces, `CVPixelBuffer`/Metal textures, and VAAPI/Vulkan or native
   Linux surfaces; retain a measured copy fallback where interop is absent.
4. **Encode and capture.** Build capture, conversion, hardware/software encode,
   timestamp, and muxing paths on top of the accepted media frame contract.
5. **Networking and streaming.** Add transport, buffering, back-pressure,
   reconnect, and protocol integration only after local media timing is stable.
6. **WebRTC, then JavaScript.** Treat WebRTC as a consumer-driven integration
   milestone. Build the real QuickJS core and CRT bindings before extending
   isolated distribution acceptance from `04-gfx-media` to `05-js`; a stage
   skeleton alone is not completion.

The intended execution order is live GPU evidence, hardware decode, zero-copy
interop, encode/capture, networking/streaming, WebRTC, and finally the complete
JavaScript application-runtime layer.

### Windows process signals and Toybox `timeout`

Close the two confirmed Windows PAL gaps that keep Toybox `timeout` disabled:
`__crt_sys_kill()` currently supports only the calling process's
`kill(pid, 0)` probe and returns `ENOSYS` for a real child or process group,
while `deliver_signal()` synthesizes `SA_SIGINFO` with the caller's pid and
zero `si_code`/`si_status` even when the Windows child registry already knows
which child exited and its exit code. Keep this tranche limited to
non-interactive child lifecycle and timeout enforcement; Ctrl-C/Ctrl-Break,
foreground-terminal arbitration, stopped-child reporting, and re-enabling
mksh job control remain in the separate deferred section below.

1. **Freeze the Bionic-facing contract and reproduce each failure.** Check
   Bionic's `kill()` pid-domain, errno, default-action, and `SIGCHLD`
   `siginfo_t` rules before changing the PAL. Add bounded Windows regressions
   for a live/dead foreign-pid `kill(pid, 0)` probe, positive child signaling,
   negative process-group signaling, and a child exiting with status 7 whose
   `SA_SIGINFO` handler must observe the real pid, `CLD_EXITED`, and status 7.
   Retain direct consumer evidence that `timeout 2 sleep 10` currently runs
   for roughly 10 seconds instead of enforcing its deadline.
2. **Thread real child-exit information through signal dispatch.** Extend the
   private signal backend/dispatch interface so Windows can pass a populated
   `siginfo_t` from `child_process_table`/`child_pid_table` and
   `GetExitCodeProcess()` without changing `raise()`'s self-delivery contract.
   Natural exits must report `CLD_EXITED` and the actual status; a termination
   mechanism introduced below must distinguish `CLD_KILLED` and its signal.
   Preserve the existing once-per-state-change notification and blocked-
   `SIGCHLD`/`pselect()` behavior.
3. **Implement positive-pid `kill()` with explicit, honest semantics.** Use
   documented Windows APIs for existence/access probing and `SIGKILL`; design
   a CRT-owned cooperative delivery channel for catchable signals so
   `SIGTERM`/user handlers are not silently reduced to unconditional
   `TerminateProcess()`. Define PID-reuse protection, permissions, pending-
   signal coalescing, and delivery checkpoints up front. Unsupported signals
   must fail explicitly rather than report success without delivery.
4. **Implement the non-interactive process-group subset needed by
   `timeout`.** Connect the CRT-managed pgid to real spawned-child membership
   and support Toybox's default `kill(-pgid, SIGTERM)`/`SIGKILL` path, including
   descendants. Evaluate documented Windows process groups and Job Objects
   against the post-`fork()` `setpgid(0, 0)` lifecycle before choosing the
   mechanism; do not pull in undocumented suspend/resume APIs or claim full
   POSIX job control as part of this tranche.
5. **Accept through the real packaged consumer.** Require positive-pid and
   process-group signal tests, correct `SIGCHLD` pid/code/status for natural
   and killed exits, `timeout 2 sleep 10` completing near two seconds with
   status 124, `timeout --preserve-status 10 sh -c 'exit 7'` returning 7, and
   the TERM-to-KILL `-k` path. Re-run the existing `pselect_sigchld`, process-
   stress, fd-snapshot, fork, waitpid, shell, and full Windows CTest coverage;
   rebuild the packaged shell/dist and repeat the timeout cases there. Only
   then enable the applet, update `docs/toybox_applet_status.md`, move the
   completed record to `HISTORY.md`, and remove this Planned section.

### Focused CRT/PAL follow-ups

These are real remaining limitations, but none blocks the completed
`libcrtgfx` CPU-raster milestone. Promote one into active work when a consumer
or host investigation supplies the required evidence.

- Extend the resolver from its current synchronous UDP IPv4/A-record baseline
  when IPv6, TCP fallback, search domains, or caching becomes a consumer
  requirement.
- Revisit a CRT-owned ELF loader/Android-linker boundary only after a real
  upper-runtime consumer requires behavior the host loader adapter cannot
  provide.
- Harden FreeType's fetch beyond the single SourceForge URL fix (`5b87197`)
  -- add retry-on-transient-failure, a documented fallback mirror, and
  SHA-256 verification of the cached archive before reuse, matching the
  reliability bar other `porting/recipes/*.json` ports already meet.
- Root-cause the packaged Windows `02-cxx` `<iostream>` static-initialization
  crash. Reproduce it against packaged and in-tree builds, compare `.ctors`
  and `std::ios_base::Init` startup behavior, and keep the stage runner free of
  retries that could hide the fault. `<cstdio>` working is a diagnostic fact,
  not evidence that C++ startup is complete.

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
`flags.h` check); `timeout` (tracked by the concrete Windows process-signal
plan above); and a confirmed-not-guessed deferred list
(`ps`/`top`/`iotop`/`pgrep`/`pkill`, `mount`/
`umount`, `ifconfig`, `login`, procfs-heavy commands).
