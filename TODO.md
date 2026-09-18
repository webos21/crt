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

### Hardware video decode

Add real hardware-accelerated H.264 decode while preserving the existing `crtmedia` software-decode contract and Host ABI ownership rules. This tranche ends at a validated CPU-resident decoded frame obtained from a hardware decoder. Native decoded-surface sharing with Vulkan, Metal, or D3D remains explicitly deferred to **Zero-copy decoded textures**.

Recommended host order:

`macOS/arm64 → Windows/x64 → Linux`

Use macOS/arm64 as the first-green reference host, then apply the same Phase-A contract to Windows/D3D11VA and Linux/VA-API.

---

* [x] **0. Freeze the Phase-A hardware-decode acceptance contract.**
  Completed 2026-09-18 and recorded in `HISTORY.md`; the frozen contract
  lives in `docs/crtmedia_hardware_decode_acceptance.md`. Audited the
  existing 2026-09-08 Phase-A groundwork (format opt-in key, `crtmedia_
  codec_is_hardware_accelerated()`, per-host `hw_type_for_platform()`
  mapping, `hw_decode_test.c`, the existing `test_video.mp4` fixture) --
  all reused unchanged, no new public API or fixture needed. Found two
  real, confirmed gaps while auditing rather than assuming Phase A already
  worked end to end: (1) `porting/recipes/ffmpeg.json` never enables any
  hwaccel on any host (`--disable-everything` plus explicit decoder/
  demuxer/parser/protocol allowlists only; the recipe's own 2026-08-31
  verification note already says so directly: "no encoders/hwaccels/
  muxers/..."), so `crtmedia_hw_decode_test` has likely always taken the
  software-fallback path silently on every host to date; (2) `crtmedia_
  codec_is_hardware_accelerated()`'s backing flag is set at successful
  `avcodec_open2()` time, not after an actual hardware-resident frame is
  observed, contradicting both this tranche's own scope rule and the
  function's own existing doc comment ("never a caller-side guess"). Both
  are real bugs for later tranches to fix (1 in Tranche 1/3/4, 2 in
  Tranche 2), not fixed in this freeze step. Also froze the six-state
  model and a new `RESULT ...` machine-readable line for `hw_decode_test`
  to print once Tranche 1 lands, matching the live-presentation tranche's
  own `RESULT` convention.

---

* [ ] **1. Establish the first real hardware-decode green on macOS/arm64 with VideoToolbox.**
  **IN PROGRESS, paused 2026-09-18 mid-session -- blocked on a real, reproducible
  deadlock, not yet root-caused.** Uncommitted working-tree changes so far
  (all still believed correct/needed, none reverted):
  - `porting/recipes/ffmpeg.json` (macOS `target_overrides`): added
    `--enable-videotoolbox`, `--enable-hwaccel=h264_videotoolbox`, and
    `--extra-cflags=-fcrt-real-apple-sdk` (the last one needed because
    `check_apple_framework`'s own configure-time probe for CoreFoundation/
    CoreMedia/CoreVideo/VideoToolbox needs real Apple SDK headers, which
    `tools/crt-cc`'s ordinary `-nostdinc` sysroot mode does not expose --
    confirmed via a real `ERROR: videotoolbox requested, but not all
    dependencies are satisfied` configure failure without it). Verified for
    real: a from-scratch `port-rebuild-ffmpeg` (after manually deleting the
    stale previously-installed `libav*`/`libsw*` headers and `.a` files
    under `out/macos-host-ninja-debug/port-tests/install` -- see the
    separate infra-gap note below for why that manual cleanup was needed)
    configures and builds cleanly, with `./configure`'s own summary
    confirming `Enabled hwaccels: h264_videotoolbox`.
  - `libcrtmedia/CMakeLists.txt`: added `-framework CoreFoundation` to
    `CRTMEDIA_MACOS_FRAMEWORKS` (was missing; `VideoToolbox`/`CoreVideo`/
    `CoreMedia` were already there from the original 2026-09-08 landing).
    Confirmed necessary via a real link failure (`_kCFAllocatorDefault`,
    `_kCFTypeDictionaryKeyCallBacks`, `___CFConstantStringClassReference`,
    ... undefined, referenced from `libavcodec.a[videotoolbox.o]`/
    `libavutil.a[hwcontext_videotoolbox.o]`) before adding it; link
    succeeds cleanly after.
  - `libcrtmedia/tests/hw_decode_test.c`: implemented the frozen `RESULT
    ...` line from `docs/crtmedia_hardware_decode_acceptance.md` (backend
    name, `hw_requested`/`hw_device_created`/`hw_pixfmt_offered` from
    `crtmedia_codec_is_hardware_accelerated()`, `hw_frame_observed`/
    `cpu_transfer` derived independently from whether any decoded frame's
    own format was `CRTMEDIA_PIXEL_FORMAT_NV12` -- see that doc's own
    "Result record" section for the exact field semantics and why `hw_
    frame_observed` deliberately does not depend on gap 2's still-open
    bug). Verified for real against the *original*, pre-hwaccel FFmpeg
    build (before the recipe change): printed `RESULT backend=videotoolbox
    hw_requested=yes hw_device_created=no hw_pixfmt_offered=no hw_frame_
    observed=no cpu_transfer=n/a frame_count=25 fallback=yes eos=pass
    clean_exit=pass` -- real, direct confirmation of Tranche 0's own gap 1
    finding (`av_hwdevice_ctx_create()` itself fails outright with no
    hwaccel compiled in, not just "decode silently stays software").

  **Blocking bug, found but not yet root-caused**: once FFmpeg is rebuilt
  with the hwaccel enabled and `crtmedia_hw_decode_test` is relinked against
  it, running the test hangs indefinitely (confirmed via `sample`: 889/889
  stack samples pinned in `main -> avcodec_find_decoder_by_name ->
  pthread_once -> crt_once_begin -> sched_yield/__crt_wait32`, i.e. this
  project's own spin-then-futex-wait `pthread_once` implementation,
  `libc/include/private/crt_atomic.h`'s `crt_once_begin()`/`crt_once_
  complete()`). The process is single-threaded at the time of the hang (the
  `sample` call graph shows only one live thread), which rules out an
  ordinary cross-thread wait for a slow initializer -- the only way this
  specific implementation (CAS 0->1 succeeds and runs the callback, or CAS
  fails and spins/waits for state to reach 2) can hang forever on one thread
  is if that same thread calls `pthread_once()` reentrantly on the *same*
  `once_control` from inside its own not-yet-complete callback: the
  reentrant call's CAS fails (state is already 1, set by this same thread),
  so it waits for state 2, which only the outer, still-blocked call would
  ever set. This is textbook reentrant-`pthread_once`-is-UB territory, so it
  may well be a genuine FFmpeg-internal pattern (e.g. hwaccel registration
  looking up its paired decoder by name, `h264_videotoolbox_hwaccel_
  select="h264_decoder"` from `configure`, from *within* the same lazy
  codec-registration `pthread_once` callback `avcodec_find_decoder()`/`_by_
  name()` share) that would deadlock on a real glibc/musl too, not
  necessarily a bug in this project's own libc -- **not yet confirmed
  either way**. An `lldb` session (breakpoint on `pthread_once`, batch
  script at the point of pausing) had matched two distinct locations (real
  Apple's dynamic `libsystem_pthread.dylib` one -- hit several times
  harmlessly during ordinary `dyld`/`libxpc` process-startup initializers,
  now reachable at all only because linking real `-framework
  CoreFoundation` pulls in real `libxpc` transitively -- and this project's
  own static one, not yet isolated) when the session was paused; next step
  is to isolate a breakpoint on *only* this project's own `pthread_once`
  symbol (delete/disable the dynamic-library location, or set an
  address-based breakpoint) and continue until the real hang, to get the
  exact FFmpeg call site making the reentrant call. `lldb`/debuggee
  processes from that session were killed before pausing; nothing left
  running.

  **Separate, real, confirmed infra gap found and worked around manually
  (not yet fixed generally, and not this tranche's own job to fix)**:
  `tools/crt-port-build.py --rebuild` only deletes a port's own install
  *stamp*, not its previously-installed files, while `tools/crt-cc`
  unconditionally prepends `-I${CRT_PORT_INCLUDE_DIR}` (the shared
  `port_prefix/include`) ahead of every other search path on every compile.
  For an ordinary single-library port this is harmless (nothing stale to
  find), but FFmpeg's own internal cross-library quote-includes (e.g.
  `libavformat/avformat.c` including `libavutil/frame.h`) can resolve to a
  *stale, previously-installed, public-headers-only* copy at
  `port_prefix/include/libavutil/...` instead of the fresh, complete
  source-tree copy on a **rebuild** specifically (confirmed for real: `make`
  failed with `fatal error: 'intmath.h' file not found` /
  `'aarch64/intreadwrite.h' file not found`, both genuine private/internal
  FFmpeg headers that only exist in the source tree, never installed).
  Worked around this session by manually `rm -rf`-ing the stale
  `port_prefix/include/libav*`, `include/libsw*`, `lib/libav*.a`,
  `lib/libsw*.a`, and their `lib/pkgconfig/*.pc` files before each
  `port-rebuild-ffmpeg`. A general fix (e.g. `--rebuild` also clearing a
  port's own previous install output, or excluding a port's own
  destination from its own `CRT_PORT_INCLUDE_DIR` while building itself) is
  real follow-up work but out of scope for this tranche -- flag separately
  if picking this up.

  **To resume**: `out/macos-host-ninja-debug`'s FFmpeg (`port-tests/
  install`) already has the hwaccel-enabled build installed from this
  session; `crtmedia_hw_decode_test` is already built and linked against
  it. Re-running it will reproduce the hang directly (no rebuild needed) --
  start there with the `lldb` isolation step above.

  * Update the macOS FFmpeg recipe/configuration only as much as required to enable H.264 VideoToolbox hardware acceleration.
  * Because FFmpeg is built with a narrow `--disable-everything` policy, verify from the configure result that the required VideoToolbox H.264 hwaccel is actually enabled.
  * Perform a fresh FFmpeg build rather than relying on an existing software-only artifact.
  * Build `libcrtmedia` against the resulting FFmpeg package.
  * Run the existing hardware-decode test on a real Apple Silicon Mac.
  * Require:

    * hardware decode requested
    * VideoToolbox device/context successfully initialized
    * VideoToolbox hardware pixel format actually selected
    * at least one real hardware-backed frame observed
    * hardware frame successfully transferred to CPU memory
    * expected decoded frame count reached
    * timestamps remain valid/monotonic
    * downloaded NV12/YUV data passes the existing image-content validation
    * clean EOS
    * clean destruction with no ownership/lifetime fault
  * Run the same fixture with hardware preference disabled and confirm that the existing software-decode path remains green.
  * Run the fixture with hardware preferred but unavailable/disabled and confirm clean software fallback rather than decode failure.

---

* [ ] **2. Harden common Phase-A reporting and lifetime behavior after the macOS first-green.**

  * Make hardware-status reporting reflect actual decode activity rather than device availability.
  * Emit or expose enough diagnostic state to distinguish:

    * requested backend
    * selected backend
    * hardware frame observed
    * CPU transfer performed
    * fallback occurred
  * Keep result reporting machine-readable where practical.
  * Repeat create/decode/EOS/release cycles to catch stale `AVBufferRef`, `AVFrame`, or platform-object ownership.
  * Exercise decoder flush/reuse if the existing `crtmedia` API supports it.
  * Confirm all FFmpeg/VideoToolbox/CoreVideo-owned objects are released through their owning APIs.
  * Do not free or reinterpret host-owned storage from CRT allocator domains.
  * Re-run ordinary software decode tests after any common-code change.

---

* [ ] **3. Enable and validate Windows/x64 D3D11VA hardware decode.**

  * Reproduce the current FFmpeg D3D11VA configure failure independently before changing the recipe.
  * Isolate whether the existing Windows macro policy, especially `_WIN32` undefinition, prevents the required D3D11 video declarations from being visible during FFmpeg configure checks.
  * Avoid globally restoring `_WIN32` across the FFmpeg build unless absolutely necessary.
  * Prefer the narrowest recipe/configure/header boundary fix that enables D3D11VA without weakening the existing CRT portability model.
  * Verify from FFmpeg configure output that the required H.264 D3D11VA hwaccel is enabled.
  * Perform a fresh FFmpeg build and rebuild `libcrtmedia`.
  * Run the same hardware-decode fixture on real Windows/x64 hardware.
  * Require:

    * D3D11VA requested
    * real D3D11 hardware frame observed
    * successful `av_hwframe_transfer_data()` or equivalent CPU transfer
    * expected frame count
    * valid timestamps
    * valid decoded image contents
    * clean EOS
    * clean destruction
  * Re-run the fixture with hardware preference disabled and verify software decode remains unchanged.
  * Verify unsupported/unavailable hardware falls back cleanly instead of failing decode.
  * Keep D3D11 texture sharing and D3D11↔D3D12 interop outside this tranche.

---

* [ ] **4. Enable and validate Linux VA-API hardware decode.**

  * First establish a clean Linux environment where the existing software-only FFmpeg recipe rebuilds successfully.
  * Treat any host-toolchain failure such as `ar`/`nm` crashes as a build-environment blocker, not a VA-API defect.
  * Use a native Linux machine with:

    * a real VA-API-capable GPU
    * working `/dev/dri/renderD*`
    * an appropriate VA driver
  * Enable only the FFmpeg VA-API/H.264 pieces required by the existing narrow recipe.
  * Verify from configure output that H.264 VA-API hardware acceleration is actually enabled.
  * Perform a fresh FFmpeg build and rebuild `libcrtmedia`.
  * Run the same hardware-decode fixture.
  * Require:

    * VA-API requested
    * real VA-API hardware frame observed
    * successful CPU transfer from the hardware frame
    * expected frame count
    * valid timestamps
    * valid decoded image contents
    * clean EOS
    * clean destruction
  * Run software-only mode and verify the baseline remains green.
  * Verify hardware-unavailable mode falls back cleanly.
  * Do not count software fallback on a non-VA-API machine as Linux hardware-decode acceptance.
  * Keep VA surface → Vulkan zero-copy interop outside this tranche.

---

* [ ] **5. Normalize the same acceptance matrix across all supported hosts.**

  * Use the same H.264 fixture and the same `crtmedia_hw_decode_test` semantics on:

    * macOS/arm64 — VideoToolbox
    * Windows/x64 — D3D11VA
    * Linux — VA-API
  * For each host, record:

    * requested hardware backend
    * actual hardware backend selected
    * whether a real hardware frame was observed
    * decoded frame count
    * first/last timestamp or monotonicity result
    * CPU-transfer result
    * pixel/image-content result
    * fallback status
    * EOS result
    * clean-exit result
  * Treat the following as distinct outcomes:

    * `decode=pass, hardware=active` → hardware-decode PASS
    * `decode=pass, hardware=inactive, fallback=yes` → fallback PASS, not hardware-decode PASS
    * `decode=fail` → FAIL
  * Keep host-specific implementation details behind FFmpeg/platform ownership boundaries.
  * Do not add platform-native texture handles to the public `crtmedia` API during this tranche.

---

* [ ] **6. Run ownership, regression, and repeated-lifecycle validation.**

  * Repeat hardware decode multiple times in one process where supported.
  * Exercise create → decode → EOS → destroy cycles repeatedly.
  * Verify no stale hardware context survives decoder destruction.
  * Confirm hardware-frame download does not leak or retain platform-native surfaces indefinitely.
  * Re-run allocator/Host ABI diagnostics if any new cross-domain ownership path is introduced.
  * Verify that generic media tests do not directly interpret:

    * `CVPixelBuffer`
    * `ID3D11Texture2D`
    * VA-API private surface structures
  * Ensure platform resources remain opaque and are destroyed by the APIs that own them.
  * Re-run existing software decode/media regression tests on every host after common code changes.

---

* [ ] **7. Close distribution and package acceptance.**

  * Rebuild FFmpeg and `libcrtmedia` from a fresh state on each acceptance host.
  * Rebuild the normal `04-gfx-media` cumulative stage.
  * Run packaged media consumers, not only build-tree tests.
  * Audit binary/runtime dependencies:

    * macOS: VideoToolbox/CoreVideo/CoreMedia-related frameworks
    * Windows: D3D11/DXGI-related imports
    * Linux: expected VA-API/runtime library dependencies
  * Confirm no unexpected host ABI or allocator-domain dependency is introduced.
  * Confirm existing public `crtmedia` ABI and software-only callers remain compatible.
  * Record exact host/architecture, GPU, decoder backend, FFmpeg configuration, test command, and result in `HISTORY.md`.
  * Keep raw logs/results outside git unless they are small, stable project fixtures.

---

### Acceptance gate

This tranche is complete when all of the following are true:

* [ ] macOS/arm64 decodes the project H.264 fixture through real VideoToolbox hardware frames.
* [ ] Windows/x64 decodes the same fixture through real D3D11VA hardware frames.
* [ ] Linux decodes the same fixture through real VA-API hardware frames on a capable native host.
* [ ] Every hardware path successfully transfers at least one decoded hardware frame into the existing CPU-resident `crtmedia` frame contract.
* [ ] The expected decoded frame count, timestamp behavior, image-content validation, EOS, and cleanup checks pass on every host.
* [ ] `hardware_active` or its equivalent means “a real hardware frame was actually observed,” not merely “a hardware device was created.”
* [ ] Software-only decode remains green on every host.
* [ ] Hardware-unavailable or unsupported configurations fall back cleanly to software without breaking the decode contract.
* [ ] No public API exposes platform-native decoded textures or surfaces yet.
* [ ] Host/platform resources remain owned and released by FFmpeg/platform APIs, not by unrelated CRT allocator domains.
* [ ] Fresh packaged `04-gfx-media` builds and binary/import audits remain green on the supported matrix.

**Decision:** when this gate is green, move **Hardware video decode** to `HISTORY.md` and promote **Zero-copy decoded textures** into `In Progress`.

### Recommended execution order

* [ ] macOS/arm64 — establish the first real VideoToolbox green.
* [ ] Harden common reporting/lifetime semantics using the macOS evidence.
* [ ] Windows/x64 — solve D3D11VA FFmpeg enablement and obtain real hardware evidence.
* [ ] Linux — first remove any toolchain/build-environment blocker, then obtain real VA-API evidence.
* [ ] Re-run the normalized cross-host acceptance matrix.
* [ ] Close packaged `04-gfx-media` and distribution/import acceptance.
* [ ] Promote **Zero-copy decoded textures**.


## Planned

### Upper runtime roadmap

The completed cross-host baseline and its exact validation evidence stay in
[`STATUS.md`](STATUS.md) and [`HISTORY.md`](HISTORY.md); the product boundary
and dependency order stay in [`docs/runtime_roadmap.md`](docs/runtime_roadmap.md).
The allocator baseline decision gate is closed: keep the current allocator and
leave Scudo conditional. Hardware video decode (the roadmap's first tranche)
is now active above; promote the remaining tranches one at a time into
In Progress when their own prerequisite evidence and acceptance host are
available.

1. **Zero-copy decoded textures.** Add explicit ownership and synchronization
   for D3D surfaces, `CVPixelBuffer`/Metal textures, and VAAPI/Vulkan or native
   Linux surfaces; retain a measured copy fallback where interop is absent.
2. **Encode and capture.** Build capture, conversion, hardware/software encode,
   timestamp, and muxing paths on top of the accepted media frame contract.
3. **Networking and streaming.** Add transport, buffering, back-pressure,
   reconnect, and protocol integration only after local media timing is stable.
4. **WebRTC, then JavaScript.** Treat WebRTC as a consumer-driven integration
   milestone. Build the real QuickJS core and CRT bindings before extending
   isolated distribution acceptance from `04-gfx-media` to `05-js`; a stage
   skeleton alone is not completion.

The intended execution order is zero-copy
interop, encode/capture, networking/streaming, WebRTC, and finally the complete
JavaScript application-runtime layer, continuing on from hardware decode above.


### Runtime architecture hardening backlog

These remain independent follow-ups. Promote one at a time when a concrete
consumer or failure justifies their cost. The backend object-boundary work
that had to precede further Upper Runtime expansion is complete (`HISTORY.md`,
2026-09-17..18); the items here were never part of that acceptance gate.

1. **Remove the "build once, then reconfigure" CMake pattern.** The native
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

2. **Move `tools/crt_dist_prerequisites.py`-style manifest thinking to
   `libc.so`'s own ELF export surface.** The current `CRT_1.0 { global: *;
   };` whole-surface version script (`libc/CMakeLists.txt`) is the right
   *shape* of fix for the collision recorded in the 2026-09-16 history entry,
   but long-term, generating the version-script/export list from an
   explicit Bionic-compatibility symbol manifest (public headers ->
   manifest -> generated `.map` file) would prevent accidental exports and
   give a real basis for `CRT_1.0`/`CRT_1.1`/`CRT_2.0`-style ABI evolution,
   rather than hand-maintaining `global: *;` indefinitely.
3. **Make binary/package/ABI lint an explicit acceptance gate.** Extend the
    distribution dependency and absolute-path checks with a separately
    runnable ABI audit: compare public exports and unresolved imports against
    the stage's declared symbol/dependency manifests, reject accidental host
    ABI leakage, and retain a baseline suitable for detecting incompatible
    changes between released CRT ABI versions.

### Conditional Scudo production allocator

Do not promote this tranche until the baseline-validation decision gate records
a repeatable current-allocator limitation. The completed Skia incident was an
allocator-domain linkage error, not evidence that the allocator algorithm must
be replaced.

1. **Separate the bootstrap allocator from a production allocator only when
   measurements justify it.**
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
2. **Decouple allocator regions from Windows fork emulation before any swap.**
   Replace `__crt_malloc_os_region_count()`/`_base()`/`_size()` with a small,
   allocator-agnostic heap-region snapshot interface consumed by both Windows
   fork implementations. Prove the interface first with the current allocator
   and the many-region fork workload; Scudo cannot be selected until the same
   contract can enumerate or otherwise preserve every child-visible region.
3. **Integrate Scudo behind an explicit allocator selection.** Import it with
   provenance, preserve Bionic-compatible public allocation behavior, keep the
   bootstrap allocator selectable for bring-up/diagnostics, and run the exact
   same correctness, fork, performance, RSS, distribution, and upper-runtime
   workloads against both implementations. Adoption requires measured benefit
   on the failing baseline without regressing supported hosts.

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
