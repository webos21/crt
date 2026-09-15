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
`03-gfx-simple -> 04-gfx-media` transition, including path-with-spaces
acceptance, is complete on Windows and macOS; the dated evidence belongs in
[`HISTORY.md`](HISTORY.md). **On Linux it is still not complete**: the
`readdir`/`opendir`-vs-`strtof_l` ELF symbol collision that used to crash
`crtgfx_skia_example` immediately is fixed and validated on both
Linux/aarch64 and WSL2 Ubuntu 26.04/x86_64 (`libc.so`'s own `CRT_1.0` ELF
symbol-version namespace; see `HISTORY.md`'s 2026-09-14 entries) -- but that
fix uncovered a second, separate, pre-existing Skia heap-corruption bug
blocking the same example, tracked below. The isolated stage-build tool's
own pipeline still cannot reach `verify_dist.py`/atomic publish on Linux as
a result.

- [ ] **Root-cause the Linux Skia heap corruption blocking `examples/gfx-skia`
  presentation -- CRT's own allocator is cleared and the two observed Skia
  call sites are stock/spec-correct; the failure is strongly isolated to
  the Mesa lavapipe/llvmpipe 25.2.8 ICD path on this host, but not yet
  confirmed as a Mesa defect** (`VK_LAYER_KHRONOS_validation` only checks
  Vulkan-API-visible misuse -- it does not check ELF symbol interposition,
  the CRT/host ABI boundary, a wrongly-loaded runtime, or a driver's own
  internal memory safety, so it cannot by itself clear CRT/Skia or convict
  Mesa). `strtof_l` is fixed; `CRT_ENABLE_DEBUG_MALLOC` is complete and
  validated (guard-malloc work stays deferred -- the owner/double-free/
  canary trio was sufficient to clear CRT's own allocator, so there is no
  concrete need for it yet). A real isolated-04 rerun against the corrected
  allocator stays completely silent while the process still aborts with
  glibc's own `free(): invalid next size (fast)` inside Mesa's lavapipe ICD
  (`libvulkan_lvp.so`); reproducing under the validation layer printed zero
  VUID errors/warnings before the abort, yet the crash site moved to a
  different, equally stock/unmodified Skia call site. Full evidence:
  `HISTORY.md`'s 2026-09-15 entries. Next steps, in order (each is meant to
  close off one remaining alternative explanation before spending real time
  on a Mesa A/B comparison or any upstream patch):
  1. ~~Cheap host-ABI-binding sanity check~~ -- **done 2026-09-15**: a
     direct `LD_DEBUG=bindings,libs` audit of the preserved failing example
     found zero exceptions -- every host/Mesa library (including
     `libvulkan_lvp.so` and its own `libLLVM.so.20.1` JIT dependency) binds
     `malloc`/`calloc`/`realloc`/`free`, `memcpy`/`memmove`, `pthread_*`,
     and `opendir`/`readdir` to real glibc; every CRT-owned object binds
     the same symbols to CRT's own versioned `libc.so`; no binding crosses
     between them in either direction. This rules out a second ELF-
     interposition-class bug. Full detail: `HISTORY.md`'s 2026-09-15
     entries.
  2. ~~Fix the imported-libc++/libc++abi/libunwind absolute-RUNPATH gap~~
     -- **`$ORIGIN`-first self-containment fixed 2026-09-15** (item just
     below); the SDK's C++ runtime finds its own copy of `libc.so` first
     and runs correctly with the original checkout's `out/` tree renamed
     away entirely. **Precisely**: the absolute checkout path is still
     present as a *second*, fallback RUNPATH entry (intentionally, for the
     external build's own temporary configure-time probes) -- do not
     describe this as "no absolute path remains"; see that item's own
     still-open follow-ups for removing it from installed copies
     specifically. Sufficient for the A/B comparison in step 3 either way,
     since `$ORIGIN` is what actually resolves once a library is
     installed anywhere. The gap's other half (no relink on `libc.so`
     change) is also still open and does not block step 3.
  3. **Active next step: staged Mesa lavapipe A/B comparison**, building
     each candidate into its own prefix (never overwriting the system
     package) and keeping *everything else fixed* (same `crtgfx_skia_
     example` binary and SDK, same Vulkan loader, same Wayland session).
     Run in order, stopping as soon as a stage's result is conclusive
     enough to classify the defect (do not build every stage regardless):
     - **S0 (baseline, already done)**: system `mesa-vulkan-drivers`
       `25.2.8-0ubuntu0.24.04.2` -- fails with validation both OFF and ON
       (`HISTORY.md`'s 2026-09-15 entries).
     - **S1**: upstream Mesa tag `mesa-25.2.8` (the *same* version, built
       from plain upstream source/options, not the Ubuntu package) in its
       own prefix. Distinguishes an Ubuntu-specific patch/build-option/
       packaging defect (S1 passes) from a real upstream 25.2.8 defect or
       a defect common to this whole configuration (S1 also fails).
     - **S2**, only if S1 fails: the current latest stable Mesa release
       tag, same build options/toolchain, own prefix. S1 fails + S2 passes
       -> a since-fixed upstream defect (record a minimum supported Mesa
       version). S1 and S2 both fail -> either a still-open lavapipe
       defect or a narrower CRT/Skia-lavapipe interop problem.
     - Mesa `main`, only if S2 also fails -- do not build it earlier, it
       only adds an unnecessary extra axis before S1/S2 have narrowed
       things down.
     Per-stage control protocol (keep identical across every stage):
     pin the example/SDK by SHA-256; same Wayland compositor/session; pick
     the ICD explicitly via `VK_DRIVER_FILES` (and matching
     `VK_ICD_FILENAMES` for older-loader compatibility) pointing at a
     dedicated ICD JSON whose `library_path` is the new build's own
     absolute `libvulkan_lvp.so` path -- not a wide-open `LD_LIBRARY_PATH`
     over the whole prefix; confirm the actually-selected driver with
     `VK_LOADER_DEBUG=driver`; use a fresh or disabled shader-cache
     directory per stage so a stale cache from one build can't leak into
     another's result; never delete or overwrite the system Mesa package;
     run each stage 3x with validation OFF and 3x with it ON. Do not
     rebuild the full isolated-04 stage (FreeType/FFmpeg/Skia) for every
     Mesa candidate -- only the preserved `crtgfx_skia_example` binary is
     needed for A/B; re-run the full stage once, at the end, only against
     whichever ICD turns out known-good.
     Outcomes: a lavapipe-version swap passes -> Mesa lavapipe (or the
     Ubuntu package specifically, per the S1 split above) is the confirmed
     external defect -- record a minimum supported Mesa version or known
     limitation, and file upstream if S1 and S2 both failed (build Mesa as
     an ASan debug build first to localize the exact faulting write before
     filing, rather than filing on the glibc-abort signature alone);
     real GPU hardware, if any becomes available, also passes -> software-
     lavapipe-only limitation, stage itself is sound; every ICD tried
     (lavapipe versions and any hardware one) fails identically -> re-open
     the CRT/Skia Vulkan integration boundary as the suspect instead.
  4. Once root-caused, fix it in the right place (CRT, Skia, or a Mesa bug
     report -- do not start a Mesa upstream patch or a Skia source change
     before step 3's A/B result), then re-run the full real isolated Linux
     arm64 04 build from fresh libc++/FreeType/FFmpeg/Skia and complete
     `verify_dist.py`/atomic publication.
  **Separately, reconsider whether one known external software-ICD defect
  should keep blocking `verify_dist.py`/atomic publication entirely** (a
  policy question, not yet decided): a reasonable split is to keep
  option-ON build/link/packaging, CPU and offscreen-GPU automated tests,
  and `verify_dist.py`/dependency/RUNPATH checks as hard requirements,
  while tracking real on-screen presentation as a separate host-acceptance
  record and an isolated, single, already-known-defective software ICD as
  an explicit external limitation -- without ever substituting an
  option-OFF pass for it. Decide this with the project owner once step 3
  gives a real answer, not before.
- [ ] **Make imported libc++/libc++abi/libunwind self-contained in a
  packaged SDK.** Two distinct, confirmed gaps in the same three files
  (full evidence in `HISTORY.md`'s 2026-09-14/2026-09-15 entries).
  1. ~~Absolute RUNPATH, not `$ORIGIN`~~ -- **fixed 2026-09-15**
     (`tools/crt-cc`/`tools/crt-c++`). Both wrappers' Linux shared-link
     paths now emit `$ORIGIN` as the *first* `-rpath` entry, keeping the
     existing absolute `${CRT_SYSROOT}/lib` as a second, fallback entry
     purely for the external build's own ephemeral CMake try_compile/
     try_run configure-time probes (never installed, so `$ORIGIN` cannot
     help them) -- strictly additive, nothing that worked before stops
     working. Verified directly: rebuilt `libc++.so.1`/`libc++abi.so.1`/
     `libunwind.so.1` and confirmed via `readelf -d` both RUNPATH entries
     are present in that order; copied the rebuilt `03-gfx-simple` SDK to
     an unrelated path and confirmed via `/lib/ld-linux-aarch64.so.1
     --list libc++.so.1` that `libc.so`/`libm.so`/`libdl.so` resolve from
     the *copied* SDK's own `lib/`, both with the original checkout's
     `out/.../dist/01-c` present and with it renamed away entirely (the
     real test: no checkout-absolute path is reachable at all once
     renamed, and resolution still succeeds). Full in-tree `ctest` suite
     stayed 113/113 after the change -- no regression.
  2. **Still open: no relink on `libc.so` change.** The nested build
     treats the sysroot library as an untracked link input and can
     silently retain stale symbol references -- this fix's own
     verification still needed a manual `rm -rf .../build/{libcxx,
     libcxxabi,libunwind}` first, confirming this gap is real and
     independent of item 1. Add a real dependency edge or a predecessor-
     inventory fingerprint that forces the affected nested build
     directories fresh; prove an actual `libc.so` change propagates
     without manual deletion.
  3. **Still open, before final release (not before the Mesa A/B
     comparison above, which item 1's fix already unblocks):** the
     absolute checkout-path RUNPATH entry item 1 left in place as a
     fallback must not ship in an installed/packaged copy of these three
     libraries -- only the external build's own temporary `try_compile`/
     `try_run` probes need it, never an installed artifact. Strip it (e.g.
     `patchelf --remove-rpath` plus a fresh `--set-rpath '$ORIGIN'`, or an
     equivalent post-install rewrite) as an explicit step wherever
     `create_dist.py`'s `--libcxx-install` copies these files into a
     distribution stage, then extend `verify_dist.py` to reject any
     installed binary whose `RUNPATH`/`RPATH` contains this checkout's own
     absolute build path -- turning this specific class of leak into a
     hard packaging-acceptance gate, not just something caught by hand
     with `readelf` when someone happens to look.

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

### CRT allocator and runtime architecture hardening

An external architecture review (2026-09-14, prompted by the Skia heap-
corruption investigation above) looked at `libc/src/malloc.c`,
`libc/CMakeLists.txt`, `libcrtgfx/CMakeLists.txt`, the malloc test suite, and
the Linux Skia/Wayland/Vulkan boundary, and proposed a P0/P1/P2-ranked set of
structural improvements. The cheap `CRT_ENABLE_DEBUG_MALLOC` diagnostic used
by the active Skia investigation is implemented; the broader architecture
changes below remain unstarted and do not block that investigation. Promote
them into "In Progress" individually once a concrete need or consumer
justifies the cost, per this file's usual promotion discipline.

P0:

1. **Separate the bootstrap allocator from a production allocator.**
   `libc/src/malloc.c` is a clear, easy-to-audit design for CRT bring-up,
   but its `malloc()`/`free()`/new-chunk paths are all linear scans
   (`append_chunk()` walks the whole chunk list to find its tail;
   `malloc_unlocked()` first-fits linearly; `coalesce_free_blocks()`
   rescans the whole free list on every `free()`), all under one global
   `heap_lock` spinlock -- `O(N)` per operation as allocation count `N`
   grows, a real risk once Skia/FFmpeg/QuickJS/libc++ are all allocating
   heavily through it. Keep this implementation as the bootstrap/reference/
   diagnostic allocator; evaluate LLVM's Scudo Hardened Allocator (size-
   class allocators, per-thread caches, mmap-backed large allocations,
   built-in quarantine, official standalone build support) as the long-term
   production allocator -- a natural fit given this project's own Bionic-
   compatibility framing, since Scudo has been Android's own default
   allocator since Android 11.
2. **Keep `CRT_ENABLE_DEBUG_MALLOC`/`CRT_ENABLE_GUARD_MALLOC` as permanent
   diagnostic build modes**, not one-off throwaway debugging code, once the
   Skia bug above is closed with them -- see that item's own numbered plan
   for the current state of each.
3. **Draw an explicit "Host ABI firewall" boundary in the Linux graphics/
   media code.** The Vulkan GPU path already documents the specific reason
   it must use the *real host* `libwayland-client.so.0` rather than this
   project's own hand-rolled Wayland wire client: the host Vulkan WSI
   driver expects `libwayland-client`'s own private `wl_display` object
   layout, which differs from what this project's own Bionic-compatible
   `pthread_mutex_t` would produce if this project built that library
   itself. The same class of boundary will keep recurring as FFmpeg
   hardware acceleration, VA-API, PipeWire, and EGL are added. Write down
   the rule explicitly as project policy rather than re-deriving it per
   integration: a host library's own opaque objects (`wl_display*`,
   `VkInstance`, `VkDevice`, `AVHWDeviceContext`, ...) are only ever
   created and destroyed by that same host library -- never reinterpreted
   through, or allocated via, this project's own memory/object structures.
4. **Remove the "build once, then reconfigure" CMake pattern.** The native
   Wayland Vulkan WSI backend's own `CRTGFX_HAVE_NATIVE_WAYLAND` gate
   checks `EXISTS` on a generated `libxdg-shell-protocol.a` at *configure*
   time, so a genuinely clean tree needs `configure -> build
   crtgfx-wayland-build -> reconfigure -> build` -- already documented as a
   deliberate, temporary workaround for a real dependency-cycle risk, not
   a permanent design. Skia's own external/nested-build integration is
   accumulating similar shape. The durable fix is a physically separate
   superbuild stage (libc++/Wayland/Skia/FFmpeg as one dependency-prefix-
   producing phase) feeding a single, cycle-free runtime configure for
   `libcrtgfx`/`libcrtmedia` -- not another one-off `EXISTS` gate per new
   dependency. Budget real effort here given `05-js`'s QuickJS/V8 work will
   add at least one more such external dependency.

P1:

5. **Decouple `libc/src/malloc.c` from Windows fork emulation.**
   `heap_os_region_base[]`/`heap_os_region_size[]`/
   `__crt_malloc_os_region_count()`/`_base()`/`_size()` are this
   allocator's own OS-region bookkeeping, consumed directly by
   `arch/windows/*/fork_memcopy.c`. A future allocator swap (Scudo or
   otherwise) cannot honor this interface as-is. Extract a small, allocator-
   agnostic `crt_heap_region_count()`/`crt_heap_region_get()`-shaped
   interface between "whatever malloc implementation is active" and "the
   Windows fork snapshot walker" before attempting an allocator swap.
6. **Stop letting a compile macro change a shared struct's own layout.**
   `libcrtgfx/CMakeLists.txt` already carries a load-bearing comment
   explaining why `CRTGFX_HAVE_VULKAN` must be a directory-wide
   `add_compile_definitions()`, not a per-target one: `src/gpu.c` and
   `gpu_vulkan.c` both see the same `struct crtgfx_gpu_device`, and a
   mismatched definition between translation units changes that struct's
   own size, corrupting memory. The current fix (matching the definition
   everywhere) is correct, but the underlying shape -- one macro flip away
   from real memory corruption -- is itself worth removing as a class of
   bug: give `crtgfx_gpu_device` a fixed `{backend tag, ops vtable, opaque
   backend pointer}` layout and hide `CRTGFX_HAVE_VULKAN`/`_D3D12`/`_METAL`
   backend-specific fields entirely inside each backend's own translation
   unit, so the macro only ever changes which code exists, never the
   common struct's own layout.
7. **Move `tools/crt_dist_prerequisites.py`-style manifest thinking to
   `libc.so`'s own ELF export surface.** The current `CRT_1.0 { global: *;
   };` whole-surface version script (`libc/CMakeLists.txt`) is the right
   *shape* of fix for the collision it solves (see the Skia item above),
   but long-term, generating the version-script/export list from an
   explicit Bionic-compatibility symbol manifest (public headers ->
   manifest -> generated `.map` file) would prevent accidental exports and
   give a real basis for `CRT_1.0`/`CRT_1.1`/`CRT_2.0`-style ABI evolution,
   rather than hand-maintaining `global: *;` indefinitely.
8. **Add an upper-stage smoke gate to routine CI**, not just the C stage.
   Every recent Linux ELF-collision-family bug (`libc` vs host glibc,
   `libc` vs `libc++`, `libc++` vs Skia, Wayland vs Vulkan, Skia vs Vulkan)
   showed up specifically at a *stage boundary*, not within `01-c` alone.
   Full Skia/FFmpeg builds are too slow for every PR, but a per-PR
   `02-cxx` smoke plus a headless `libcrtgfx` smoke, with a nightly full-
   Skia/FFmpeg/predecessor-only-dist/binary-import-audit pass from a
   genuinely empty build directory and cache (this project's own CMake
   state has already been shown to interact with external-build artifact
   presence -- see item 4), would have caught several of the bugs in
   `HISTORY.md`'s dated entries closer to when they were introduced.
9. **Grow the allocator test suite past API-correctness coverage.** The
   existing `libc/tests/malloc_test.c`-family coverage (malloc/calloc/
   realloc/large-allocation/alignment correctness, a 4-thread/200-iteration
   contention test) is solid for what it checks, but does not yet cover
   the shapes this investigation actually needed: large-N random alloc/
   free/realloc stress, higher thread counts, size-distribution/
   fragmentation workloads, RSS recovery after large free, fork under
   fragmentation/many-region conditions, and (once built) expected-fault
   tests for `CRT_ENABLE_GUARD_MALLOC`/double-free/cross-instance-owner
   detection. Track ops/sec, p50/p99 allocation latency, peak RSS, and a
   fragmentation ratio so a future bootstrap-to-Scudo decision (item 1) has
   numbers behind it, not just impression.

P2:

10. **Split the Linux Wayland backend into two independent implementations**
    instead of one code path that changes object universes depending on
    whether Vulkan is enabled. `STATUS.md` already records a real, current
    limitation in the hand-rolled path (the software-window adapter does
    not recycle Wayland object IDs); the GPU path already uses the real
    host `libwayland-client.so.0` entirely separately. Making that an
    explicit `crtgfx` Wayland abstraction with two backends (CRT wire
    protocol vs. host-native) rather than one file family that
    conditionally reinterprets its own objects would be easier to maintain
    as both paths keep evolving.
11. **Make binary/package/ABI lint an explicit acceptance gate.** Extend the
    distribution dependency and absolute-path checks with a separately
    runnable ABI audit: compare public exports and unresolved imports against
    the stage's declared symbol/dependency manifests, reject accidental host
    ABI leakage, and retain a baseline suitable for detecting incompatible
    changes between released CRT ABI versions.

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
