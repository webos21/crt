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

### Public Preview / Promotion Preparation

Goal: turn the completed `04-gfx-media` milestone into the first externally consumable CRT developer preview, while keeping hardware decode, zero-copy, and QuickJS as follow-up publicity milestones rather than blockers for the initial launch.

#### In Progress — First Public Developer Preview

* [x] **Synchronize public-facing project status**
  Done 2026-09-22 (`HISTORY.md`): re-checked `README.md`, `STATUS.md`,
  `TODO.md`, `docs/runtime_roadmap.md`, `docs/distribution.md`, `docs/
  crtmedia_hardware_decode_acceptance.md`, `docs/release_preview.md`, and
  `docs/release_notes_v0.4.0-preview.1.md` (the live GitHub release body)
  against the actual repo/release state rather than assuming any of them
  were current. Found and fixed real staleness in `README.md` (one line:
  the Portability Proof FFmpeg row still said "hardware H.264 decode on
  macOS and Windows"), `STATUS.md` (said the hardware-decode tranche and
  Windows/macOS package audit were still open, and only Windows release
  assets were published), `TODO.md` (this whole section, the "Hardware
  video decode" tranche closure, and the release-artifacts writeup), and
  `docs/release_preview.md`/`docs/release_notes_v0.4.0-preview.1.md` (both
  still said Windows-only assets and an unresolved upload decision, even
  though the release API already showed all three hosts live). `docs/
  runtime_roadmap.md`, `docs/distribution.md`, and `docs/
  crtmedia_hardware_decode_acceptance.md` were already accurate -- confirmed
  by reading them, not assumed from a stale prior check.

  * [x] Reconcile `STATUS.md`, `TODO.md`, `HISTORY.md`, and `docs/runtime_roadmap.md`.
  * [x] Remove or clearly mark stale blockers that have already been resolved.
  * [x] Confirm that the documented `03-gfx-simple -> 04-gfx-media` predecessor-only acceptance status matches the latest verified Linux, Windows, and macOS results.
  * [x] Make the distinction between completed work, current work, and planned work unambiguous.
  * [x] Keep Linux hardware decode limitations documented separately from the already accepted software graphics/media runtime.

* [x] **Prepare the first public CRT release**
  Published 2026-09-21 as a GitHub pre-release
  (`https://github.com/webos21/crt/releases/tag/v0.4.0-preview.1`); confirmed
  2026-09-22 via the public API that Windows/x64, macOS/arm64, and
  Linux/x86_64 asset sets are all attached (27 files total: 3 hosts x
  [4 SDK archives + 3 stage-source assets + `SHA256SUMS` + manifest],
  re-counted directly from the release API, not assumed), matching the
  table below. Full per-host build
  detail is in `HISTORY.md`, not restated here.

  * [x] Create the first GitHub release, tentatively:

    * `v0.4.0-preview.1`
  * [x] Position it as:

    * `First public developer preview of the CRT Graphics/Media SDK stage`
  * [x] Treat `04-gfx-media` as the current public SDK milestone.
  * [x] Do not block the release on Linux VA-API hardware decode.
  * [x] Clearly mark `05-js` as roadmap/skeleton work rather than part of the current supported preview.
  * [x] Include a concise verified-platform summary:

    | Platform | Native Window | GPU    | Skia | FFmpeg | HW H.264 Decode        |
    | -------- | ------------- | ------ | ---- | ------ | ---------------------- |
    | Linux    | Wayland       | Vulkan | Yes  | Yes    | VA-API verified        |
    | Windows  | Win32         | D3D12  | Yes  | Yes    | D3D11VA verified       |
    | macOS    | Cocoa         | Metal  | Yes  | Yes    | VideoToolbox verified  |

* [ ] **Prepare downloadable / directly testable release artifacts**
  Contract and runbook in `docs/release_preview.md`; `CRT_RELEASE_TAG` and
  `tools/prepare_release_assets.py` done. All three asset sets (Windows/x64
  commit `fd01d7c`, macOS/arm64 commit `972d913`, Linux/x86_64 commit
  `a90bf10` -- each a real, documented reason the tag itself does not build
  that host's stage, not an oversight; see `docs/release_preview.md`) are
  built, checksummed, uploaded, and confirmed live on the GitHub release
  (2026-09-22, re-checked via the public API: 27 assets, 3 hosts). The
  release body is confirmed byte-identical to the repo's own
  `docs/release_notes_v0.4.0-preview.1.md` right now -- re-verify this after
  any future edit to that file, since nothing keeps them in sync
  automatically. Full per-host build detail is in `HISTORY.md`.

  * [x] Provide release artifacts that allow an external developer to try CRT without reconstructing the entire development environment from repository history.
  * [x] Include or document the staged SDK layout:

    * `01-c`
    * `02-cxx`
    * `03-gfx-simple`
    * `04-gfx-media`
  * [x] Provide at least one minimal build-and-run example.
  * [ ] Verify the release artifact from a clean environment where practical.
  * [x] Document the supported host/compiler requirements for each platform.
  * [ ] A fresh-clone rebuild of the Windows set specifically (the one host
    whose published asset set was built by reconfiguring an existing build
    directory rather than a fresh clone, unlike macOS and Linux) -- optional
    hardening, not required to keep this release as it is.

* [x] **Turn the README into a public landing page**
  Done 2026-09-21 (`README.md`): headline/supporting message, the unchanged
  Bionic/PAL definition directly below, an architecture overview and a
  "Where It Stands" section; toolchain policy and release-engineering detail
  moved down. The demo, "What already works" matrix, portability proof and FAQ
  are separate items below.

  * [x] Replace the current engineering-first opening with a concise project message.
  * [x] Candidate headline:

    * `Linux-style C/C++ software. Native on Linux, Windows and macOS.`
  * [x] Candidate supporting message:

    * `Same source model. Native executables. Native GPUs. No VM or container.`
  * [x] Keep the precise Bionic/PAL definition immediately below the high-level message.
  * [x] Move detailed toolchain and implementation policy farther down the README or link to dedicated documentation.
  * [x] Add a short architecture overview showing:

    ```text
    Application
        |
        v
    Skia / FFmpeg
        |
        v
    CRT C++ Runtime
        |
        v
    Bionic-compatible CRT / PAL
        |
        +-----------+-----------+
        |           |           |
      Linux       Windows      macOS
     Wayland       Win32       Cocoa
     Vulkan        D3D12       Metal
    ```

* [x] **Add a visible "What already works" matrix**
  Done 2026-09-21 (`README.md`, "What Already Works"): 16 capabilities x 3
  hosts with Verified / Partial / In progress / Planned, evidence dates, and
  per-host limits (Linux libdl, Linux Skia GPU on a VM, Linux audio, Linux
  hardware decode). Fresh Linux/x86_64 (WSL2) `ctest` 121/121 run for it.

  * [x] Show verified functionality instead of relying only on descriptive claims.
  * [x] Cover at least:

    * libc / libm / libdl
    * libc++ / libc++abi / libunwind
    * pthread
    * sockets
    * mmap
    * TLS
    * native window/input
    * Skia CPU
    * Skia GPU
    * FFmpeg
    * native audio
    * hardware decode status
  * [x] Clearly distinguish:

    * verified
    * partially verified
    * in progress
    * planned

* [x] **Add a "Portability Proof" section**
  Done 2026-09-21 (`README.md`, "Portability Proof"): 11 upstream projects with
  versions, recorded per-host results and what each test runs, curl/Skia/FFmpeg
  runtime evidence, the exact patch exceptions, and the `port-test-*` commands.

  * [x] Show real upstream software that has been built and exercised through the CRT sysroot/runtime.
  * [x] Include representative ports such as:

    * zlib
    * libpng
    * SQLite
    * bzip2
    * xz
    * PCRE2
    * mbedTLS
    * curl
    * FreeType
    * Skia
    * FFmpeg
  * [x] Highlight real runtime evidence where available, especially:

    * curl HTTP/HTTPS round trips
    * Skia live native presentation
    * FFmpeg decode/playback
  * [x] Prefer evidence-based wording over broad compatibility claims.

* [ ] **Create a three-platform visual demo**
  2026-09-21: only a first Windows clip exists -- the `media-player` example
  (native window plus FFmpeg decode/playback, CPU framebuffer), uploaded to
  GitHub and embedded in the README "Demo" section; the file is not stored in the
  repository. It covers none of the Skia, GPU, text, input, or resize items
  below and is not the same application on three platforms. Nothing has been
  recorded for Linux or macOS.

  * [ ] Record the same CRT application running on:

    * Linux / Wayland / Vulkan
    * Windows / Win32 / D3D12
    * macOS / Cocoa / Metal
  * [ ] Include visible evidence of:

    * native window creation
    * Skia rendering
    * text rendering
    * keyboard/mouse input
    * resize behavior
    * GPU presentation
    * FFmpeg video decode/playback where practical
  * [ ] Produce a short 20–30 second version suitable for the README and social posts.
  * [ ] Keep the source application and runtime path as equivalent across the three platforms as practical.
  * [ ] Add the demo near the top of the README.

* [x] **Document current hardware decode status transparently**
  Done 2026-09-21: `README.md` "Hardware Decode Status" and the per-host
  section of `docs/crtmedia_hardware_decode_acceptance.md`. Also captured the
  first real Linux fallback evidence (`fallback=yes`, 14/14 `crtmedia_*` tests,
  Linux x86_64/WSL2, FFmpeg without VA-API).

  * [x] macOS:

    * VideoToolbox hardware-backed H.264 decode verified.
    * CPU transfer and clean EOS verified.
  * [x] Windows:

    * D3D11VA hardware-backed H.264 decode verified.
    * Repeated lifecycle testing verified.
  * [x] Linux:

    * VA-API hardware-backed H.264 decode verified 2026-09-22 (physical Intel UHD 630, native Ubuntu desktop).
    * WSL / virtualized-driver limitations are kept distinct from CRT runtime defects (see `docs/crtmedia_hardware_decode_acceptance.md`'s own historical record).
  * [x] Hardware decode is now verified on all three hosts (2026-09-22); no longer withheld from cross-platform claims.

* [x] **Prepare public messaging / FAQ**
  Done 2026-09-21: `docs/faq.md` (Why CRT, why Bionic-shaped, rebuild-based
  model, seven comparisons, non-goals, readiness) plus a short "Why CRT?" in
  `README.md`. Comparison wording follows `docs/project_stacks.md` and
  `docs/project_meanings.md`, not the aspirational `docs/marketing/` notes.

  * [x] Add a concise `Why CRT?` section.
  * [x] Explain why CRT uses a Bionic-shaped interface.
  * [x] Explain rebuild-based source portability:

    * not binary compatibility
    * not a VM
    * not a container
  * [x] Prepare concise comparisons for common questions:

    * musl
    * SDL
    * Qt
    * WSL
    * Wine
    * Cosmopolitan
    * Android/Bionic
  * [x] Clearly state non-goals:

    * not Android APK compatibility
    * not an Electron clone
    * not full POSIX compatibility
    * not production-ready `1.0`

* [ ] **Soft-launch before the main announcement**

  * [ ] Share the preview with a small number of relevant technical communities first.
  * [ ] Target:

    * C / C++ systems developers
    * embedded Linux developers
    * HMI / IVI developers
    * graphics/runtime developers
  * [ ] Collect recurring questions and misunderstandings.
  * [ ] Update README/FAQ based on that feedback before a broader launch.

* [ ] **Prepare the main public launch**

  * [ ] Prepare a Show HN submission after:

    * first GitHub release exists
    * README landing page is updated
    * 3-platform demo is available
    * quick-start path is verified
  * [ ] Candidate title:

    * `Show HN: CRT – Linux-oriented C/C++ software, rebuilt natively for Linux, Windows and macOS`
  * [ ] Structure the announcement around:

    1. the portability problem
    2. CRT's architectural approach
    3. concrete proof
    4. current limitations
    5. roadmap
  * [ ] Avoid feature-list-only promotion.

#### Follow-up Promotion Milestones

* [ ] **Milestone 2 — Three-platform hardware video decode**
  Engineering evidence for all three hosts is complete (`HISTORY.md`'s
  "Hardware video decode" tranche closure); what remains here is the
  separate public-announcement decision below, not further engineering.

  * [x] Complete native Linux VA-API H.264 hardware decode evidence.
  * [x] Publish the completed platform mapping:

    * Linux -> VA-API
    * Windows -> D3D11VA
    * macOS -> VideoToolbox

    Live in `README.md`'s "Hardware Decode Status", the GitHub release's own
    capability table, and `docs/crtmedia_hardware_decode_acceptance.md`.
  * [ ] Use this as a separate technical/public announcement.

* [ ] **Milestone 3 — Hardware decode to native GPU zero-copy**

  * [ ] Connect decoded hardware surfaces to the native graphics path without mandatory CPU readback where supported.
  * [ ] Target:

    * VA-API -> Vulkan
    * D3D11VA -> D3D12
    * VideoToolbox -> Metal
  * [ ] Demonstrate decode -> GPU texture -> Skia presentation.
  * [ ] Treat this as a major graphics/media runtime milestone and separate publicity event.

* [ ] **Milestone 4 — QuickJS application runtime**

  * [ ] Promote QuickJS only after the runtime includes meaningful execution, event-loop, module, and native graphics/media bindings.
  * [ ] At that point, reposition CRT from a native runtime/SDK toward a broader embedded application runtime where justified.

#### Promotion Principles

* [ ] Lead with **working evidence**, not architectural ambition.
* [ ] Prefer `this runs today` over `we plan to support`.
* [ ] Keep unsupported or environment-limited paths explicit.
* [ ] Treat each major engineering milestone as a separate communication opportunity.
* [ ] Avoid waiting for every roadmap item before making the first public release.
* [ ] Keep the first public message centered on the already completed `04-gfx-media` runtime.

### Zero-copy decoded textures

Promoted 2026-09-22 from `Planned` (`HISTORY.md`'s own "Hardware video
decode" closure entry, which itself replaces the old section that used to be
here -- full per-step evidence for macOS/Windows/Linux hardware decode lives
there now, not in this file). This is a stub, not a phased plan: connect
decoded hardware surfaces (D3D11 texture, `CVPixelBuffer`/Metal texture,
VA-API surface) to the native graphics path without a mandatory CPU
readback, where the host supports it, with a measured copy fallback where it
does not.

* [ ] Design explicit ownership and synchronization for each hardware
  surface type (D3D11VA -> D3D12, VideoToolbox -> Metal, VA-API -> Vulkan).
* [ ] Decide the acceptance host order and per-host acceptance contract
  (mirroring `docs/crtmedia_hardware_decode_acceptance.md`'s own shape for
  the decode tranche this follows).
* [ ] Demonstrate decode -> GPU texture -> Skia presentation on at least one
  host before claiming general zero-copy support.
* [ ] Keep the existing CPU-resident `crtmedia_frame` contract and
  `CRTMEDIA_FORMAT_KEY_PREFER_HARDWARE_DECODE` opt-in unchanged; this is
  additive, not a replacement.

### Next release hardening

Real follow-up from closing "Hardware video decode" and fixing the packaged
demo's own opt-in (`HISTORY.md`, 2026-09-22): none of this blocks
`v0.4.0-preview.1`, which is already published and stays as it is (its
`crtmedia_player_demo` decodes in software on every host, documented in its
own release notes) -- this is scoped to whatever CRT publishes next.

* [x] Make the packaged `crtmedia_player_demo`/`examples/media-player`
  request hardware decode by default, with a documented software-only
  escape hatch (`CRTMEDIA_PLAYER_DEMO_SOFTWARE_ONLY=1`) and the isolated
  stage's own packaged-example acceptance step requiring
  `hardware_decode=yes`. Verified on Windows/D3D11VA (`HISTORY.md`,
  2026-09-22).
* [ ] Re-run that same packaged-example acceptance step on macOS and Linux
  to confirm `hardware_decode=yes` there too (this session was Windows-only;
  no such host was available).
* [ ] Produce the next release (whatever version) from one clean, frozen
  commit/tag on every host, so its recipes' own embedded source commit
  matches the tag exactly -- `v0.4.0-preview.1`'s three asset sets each
  record a different commit than the tag for real, documented reasons
  (`docs/release_preview.md`); avoiding a repeat is a process fix, not new
  engineering. (The current `v0.4.0-preview.1`'s own remaining hardening --
  a fresh-clone Windows rebuild and a clean-machine test -- is tracked under
  "Prepare downloadable / directly testable release artifacts" above, not
  duplicated here.)



## Planned

### Upper runtime roadmap

The completed cross-host baseline and its exact validation evidence stay in
[`STATUS.md`](STATUS.md) and [`HISTORY.md`](HISTORY.md); the product boundary
and dependency order stay in [`docs/runtime_roadmap.md`](docs/runtime_roadmap.md).
The allocator baseline decision gate is closed: keep the current allocator and
leave Scudo conditional. Hardware video decode (the roadmap's first tranche)
is closed (`HISTORY.md`, 2026-09-22); **Zero-copy decoded textures**, the
next tranche, is promoted into `In Progress` above. Promote the remaining
tranches below one at a time when their own prerequisite evidence and
acceptance host are available.

1. **Encode and capture.** Build capture, conversion, hardware/software encode,
   timestamp, and muxing paths on top of the accepted media frame contract.
2. **Networking and streaming.** Add transport, buffering, back-pressure,
   reconnect, and protocol integration only after local media timing is stable.
3. **WebRTC, then JavaScript.** Treat WebRTC as a consumer-driven integration
   milestone. Build the real QuickJS core and CRT bindings before extending
   isolated distribution acceptance from `04-gfx-media` to `05-js`; a stage
   skeleton alone is not completion.

The intended execution order is encode/capture, networking/streaming, WebRTC,
and finally the complete JavaScript application-runtime layer, continuing on
from zero-copy interop above.


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
