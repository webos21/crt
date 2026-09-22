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

* [ ] **Synchronize public-facing project status**

  * [ ] Reconcile `STATUS.md`, `TODO.md`, `HISTORY.md`, and `docs/runtime_roadmap.md`.
  * [ ] Remove or clearly mark stale blockers that have already been resolved.
  * [ ] Confirm that the documented `03-gfx-simple -> 04-gfx-media` predecessor-only acceptance status matches the latest verified Linux, Windows, and macOS results.
  * [ ] Make the distinction between completed work, current work, and planned work unambiguous.
  * [ ] Keep Linux hardware decode limitations documented separately from the already accepted software graphics/media runtime.

* [ ] **Prepare the first public CRT release**
  2026-09-21: `v0.4.0-preview.1` is published as a GitHub pre-release
  (`https://github.com/webos21/crt/releases/tag/v0.4.0-preview.1`) with the
  Windows/x64 assets only; Linux and macOS assets are still to come, which is why
  this item stays open.

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
  2026-09-21: contract and runbook in `docs/release_preview.md`;
  `CRT_RELEASE_TAG` and `tools/prepare_release_assets.py` (collect, verify,
  checksum; blocks the default-OFF `04-gfx-media`) done and exercised on
  Windows. A complete Windows/x64 asset set with the release tag
  `v0.4.0-preview.1` (commit `fd01d7c`, option-ON `04-gfx-media` with the
  `media-player` example, checksums, manifest) is built and accepted by
  `prepare_release_assets.py` in about 50 minutes and published with the tag
  `v0.4.0-preview.1` (on `fd01d7c`); the notes are in
  `docs/release_notes_v0.4.0-preview.1.md`. A complete macOS/arm64 set is now
  built from a fresh clone and accepted by `prepare_release_assets.py` (7 files
  plus checksums and manifest, in 3 + 7.5 minutes), in
  `out/release/v0.4.0-preview.1-macos-aarch64/` (not committed, not uploaded). It
  records commit `972d913`, **not** the tag commit `fd01d7c`: the tag could not
  build a macOS `04-gfx-media` (two bugs the in-tree build hides, fixed in
  `32ab384` and `972d913`; see `docs/release_preview.md`). The repo copy of the
  release notes is updated for it (tag unchanged, commit difference explained,
  macOS quick start).

  2026-09-22: a complete Linux/x86_64 set is now also built from a **fresh
  clone** and accepted by `prepare_release_assets.py` (7 files plus checksums
  and manifest, ~11 min for FreeType/FFmpeg, ~3 min for Skia), in
  `out/release/v0.4.0-preview.1-linux-x86_64/` (not committed, not uploaded).
  It records commit `a90bf10`, **not** the tag commit `fd01d7c`, the same
  "the tag itself does not build this stage" situation macOS hit: the tag
  predates this same day's Linux VA-API tranche (`5c64ee5`, `e52eb1b`) and a
  real, previously-latent bug this exact release run found and fixed
  (`a90bf10`) -- `examples/gfx-simple/CMakeLists.txt` never linked a real
  host `libwayland-client`, which its own stale comment claimed only started
  mattering at `04-gfx-media`; rebuilding the packaged `gfx-simple` example
  from a genuinely extracted `03-gfx-simple` archive (this exact quick start
  had never actually been run from a downloaded Linux archive before, per
  `docs/release_preview.md`'s own note) surfaced it immediately. Every
  extracted binary was run for real: `03-gfx-simple`'s ready-made and
  freshly-rebuilt `crtgfx_window_demo`/`crtgfx_window_example` both present
  60 frames; `04-gfx-media`'s `crtmedia_player_demo` presents 25 frames
  (real VA-API hardware decode active) and `crtgfx_skia_gpu_window_demo`
  presents 5 real-Vulkan frames with `pixel_check=pass`. The repo copy of the
  release notes is updated for it (tag unchanged, both commit differences
  explained, a Linux quick start, and the real `llvm-ar`/`llvm-ranlib`
  IFUNC-segfault workaround). **Still open:** the
  maintainer uploading the macOS and Linux files and pasting updated notes
  into the GitHub release body (which still says Windows only), a
  fresh-clone rebuild of the Windows set, a test on a machine that never had
  CRT's build environment, and the upload decision.

  * [ ] Provide release artifacts that allow an external developer to try CRT without reconstructing the entire development environment from repository history.
  * [x] Include or document the staged SDK layout:

    * `01-c`
    * `02-cxx`
    * `03-gfx-simple`
    * `04-gfx-media`
  * [x] Provide at least one minimal build-and-run example.
  * [ ] Verify the release artifact from a clean environment where practical.
  * [x] Document the supported host/compiler requirements for each platform.

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
  Engineering evidence for all three hosts is complete (see "Hardware video
  decode" above); what remains here is the separate public-announcement
  decision below, not further engineering.

  * [x] Complete native Linux VA-API H.264 hardware decode evidence.
  * [ ] Publish the completed platform mapping:

    * Linux -> VA-API
    * Windows -> D3D11VA
    * macOS -> VideoToolbox
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

### Hardware video decode

Add real hardware-accelerated H.264 decode while preserving the existing `crtmedia` software-decode contract and Host ABI ownership rules. This tranche ends at a validated CPU-resident decoded frame obtained from a hardware decoder. Native decoded-surface sharing with Vulkan, Metal, or D3D remains explicitly deferred to **Zero-copy decoded textures**.

Recommended host order:

`macOS/arm64 → Windows/x64 → Linux`

Use macOS/arm64 as the first-green reference host, then apply the same Phase-A contract to Windows/D3D11VA and Linux/VA-API.

---

* [x] **0. Freeze the Phase-A hardware-decode acceptance contract.**
  Completed 2026-09-18, recorded in `HISTORY.md`; contract in
  `docs/crtmedia_hardware_decode_acceptance.md`.
* [x] **1. First real hardware-decode green on macOS/arm64 (VideoToolbox).**
  Completed 2026-09-18, recorded in `HISTORY.md`: `RESULT backend=videotoolbox
  ... frame_count=25 fallback=no eos=pass clean_exit=pass`.
* [x] **2. Harden common Phase-A reporting and lifetime behavior.**
  Completed 2026-09-18, recorded in `HISTORY.md`:
  `crtmedia_codec_is_hardware_accelerated()` is true only after a real
  hardware frame is downloaded; flush/reuse and 15-cycle lifecycle tests added.

---

* [x] **3. Enable and validate Windows/x64 D3D11VA hardware decode.**
  Completed 2026-09-19, recorded in `HISTORY.md`: `RESULT backend=d3d11va
  ... hw_frame_observed=yes cpu_transfer=pass frame_count=25 fallback=no
  eos=pass clean_exit=pass` on a physical Intel UHD 630; flush/reuse,
  15/15 hardware lifecycle, full Windows CTest 149/149. Narrow fix: the
  `-fcrt-real-windows-sdk` sentinel scopes real Windows headers to three
  configure probes and four FFmpeg objects. The isolated `04-gfx-media`
  stage (adapted here: mingw headers fetched before the FFmpeg build) has
  since been re-run end to end on Windows multiple times (adding the
  `media-player` example, a release-tagged build, and item 7's own package
  audit below); see that item for the package-level result.

---

* [x] **4. Enable and validate Linux VA-API hardware decode.**
  Completed 2026-09-22, recorded in `HISTORY.md` and
  `docs/crtmedia_hardware_decode_acceptance.md`: `RESULT backend=vaapi
  hw_requested=yes hw_device_created=yes hw_pixfmt_offered=yes
  hw_frame_observed=yes cpu_transfer=pass frame_count=25 fallback=no
  eos=pass clean_exit=pass` on a real, native (non-VM, non-WSL) Ubuntu
  desktop with a physical Intel UHD 630 and Mesa's `iHD` driver; flush/reuse
  25+25 hardware frames, 15/15 hardware lifecycle, full `ctest` 132/132 (no
  regressions, including the plain software-decode path). Gate order followed
  exactly as planned: W0 `vainfo` (real `VAProfileH264High`/`Main`
  `VAEntrypointVLD`, the entrypoint both prior hosts lacked) -> W1 a
  CRT-free host FFmpeg VAAPI decode of the project fixture (pass, no hang --
  the WSL2 self-deadlock did not reproduce) -> W2 the plain CRT FFmpeg
  baseline (pass, matches WSL2's own recorded fallback `RESULT` line exactly)
  -> W3+ below. Four independent, real build-environment/toolchain gaps
  fixed getting there, none a VA-API-specific defect: (1) GNU Binutils
  2.46's `ar`/`ranlib`/`nm` all segfault with a real IFUNC-relink crash
  against this project's own `libm.so` on this host -- fixed via
  `--ar=@AR@ --ranlib=@RANLIB@ --nm=@NM@` (a new `@RANLIB@` token,
  `tools/crt-port-build.py`), mirroring the Windows override's existing
  `llvm-ar`/`llvm-nm` use. (2) `PKG_CONFIG_LIBDIR`'s deliberate isolation
  (vendored dependencies only) hid the real, host-provided `libva.pc` --
  fixed via a new `@HOST_PKG_CONFIG_PATH@` token (queried from the host's
  own `pkg-config --variable pc_path pkg-config`, never a hardcoded
  multiarch triplet), scoped to this recipe's own Linux `env`. (3)
  `<va/va.h>`/`<va/va_drm.h>` are real host headers `tools/crt-cc`'s own
  `-nostdinc` sysroot does not expose -- closed with a new
  `-fcrt-real-linux-sdk` sentinel (mirroring `-fcrt-real-apple-sdk`/
  `-fcrt-real-windows-sdk`), scoped to the three real objects that need it.
  (4) The final link needs the real host `libva.so`/`libva-drm.so` -- fixed
  with a new `CRTMEDIA_LINUX_VAAPI_LIBS` (`libcrtmedia/CMakeLists.txt`,
  `find_library()`, mirroring `libcrtgfx`'s own `CRTGFX_LINUX_VULKAN_LIB`
  direct-link precedent). The isolated `04-gfx-media` stage (packaged
  distribution build) was then also re-run end to end on this same host,
  closing Step 7 for Linux too (see that step's own note): it kept two more
  independent copies of the same real-host-library link logic
  (`distribution/stages/04-gfx-media/CMakeLists.txt`,
  `examples/media-player/CMakeLists.txt`), neither of which had ever linked
  a real Linux `libwayland-client`/`libva`/`libva-drm` before, plus a new
  `linux-vaapi-runtime`/`linux-vaapi-driver` `external_prerequisites`
  declaration (`tools/crt_dist_prerequisites.py`) once `verify_dist.py`'s own
  binary-dependency audit flagged both as undeclared. A new regression test
  (`tools/test_stage_source_closure.py`) now fails if the isolated stage's
  own VA-API libs ever drift from libcrtmedia's, mirroring the existing
  macOS-framework test for the identical class of drift. All 8 stage tests,
  the rebuilt `gfx-gpu`/`gfx-skia`/`media-player` examples, `verify_dist.py`,
  and atomic publication passed; the published archive's own
  `crtmedia_player_demo` presents all 25 frames running directly. VA surface
  -> Vulkan zero-copy stays out of this tranche's scope, deferred to
  **Zero-copy decoded textures**.

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

* [x] **7. Close distribution and package acceptance.**
  Linux done 2026-09-22 (see item 4's own writeup for the two extra
  parallel-copy link fixes and the new `linux-vaapi-runtime`/
  `linux-vaapi-driver` prerequisite declaration this needed): the isolated
  `04-gfx-media` stage rebuilt from a fresh FreeType/FFmpeg/Skia state on
  this same native Linux host, its 8 stage tests, `gfx-gpu`/`gfx-skia`/
  `media-player` example rebuilds, `verify_dist.py`'s binary-dependency
  audit, and atomic publication all passed, and the published archive's own
  `crtmedia_player_demo` presents all 25 frames running directly.

  macOS done 2026-09-22, recorded in `HISTORY.md`: a fresh clone re-ran both
  halves this bullet lists. In-tree (FFmpeg port rebuilt from a fresh state,
  `CRTMEDIA_ENABLE_FFMPEG=ON`): full `ctest` 132/132, `crtmedia_hw_decode_test`/
  `_flush_test`/`_lifecycle_test` byte-identical to Tranche 2's `RESULT` lines.
  Isolated `04-gfx-media` stage (fresh FreeType/FFmpeg/Skia, no reused
  install): 8 stage tests, the rebuilt `gfx-gpu`/`gfx-skia`/`media-player`
  examples (`gfx-skia`'s scripted resize passed in 3.2 s -- the exact case
  that hung before the two release-hardening fixes now on `main`), and
  `verify_dist.py`'s binary-dependency audit all passed, plus a manual
  `otool -L` cross-check of every packaged binary and `lib/*.dylib` against
  `tools/crt_dist_prerequisites.py`'s macOS declarations (no undeclared
  framework).

  Windows done 2026-09-22: the isolated `04-gfx-media` stage rebuilt end to
  end from the current commit (`tools/crt-cc` had changed since the last
  Windows run -- for Linux's `-fcrt-real-linux-sdk` sentinel, a no-op on
  Windows -- which invalidated the FreeType/FFmpeg build cache and forced a
  genuine full rebuild, 2907.4 s total). All 8 stage tests, the rebuilt
  `gfx-gpu`/`gfx-skia`/`media-player` examples, `verify_dist.py`, and atomic
  publication passed, on the same physical Intel UHD 630 (driver
  31.0.101.2140) item 3 used. Import audit (`llvm-objdump -p`): the
  packaged `crtmedia_player_demo.exe` imports `KERNEL32`, `ole32`, `USER32`,
  `d3d11`, `dxgi`, and the synch API set thunk; `libcrtmedia.dll`'s own
  import table is unchanged by hardware decode (`ole32`/`KERNEL32`/synch
  only, no `d3d11`/`dxgi`) -- the D3D11/DXGI imports come entirely from
  `crtgfx_window`'s pre-existing Win32 swap-chain presenter, confirmed by
  diffing against `crtgfx_window_demo.exe` (no `crtmedia` linked at all),
  which carries the identical `d3d11`/`dxgi` import set. FFmpeg's D3D11VA
  hwaccel resolves `d3d11.dll`/`dxgi.dll` at runtime (`LoadLibrary`), not by
  static import, matching item 3's own in-tree finding. No unexpected host
  ABI or allocator-domain dependency was introduced into `crtmedia` by
  hardware decode. Separate, non-blocking observation (not a hardware-decode
  regression): `libcrtmedia.dll`'s export table has 3134 entries, only 52 of
  them `crtmedia_*` -- the mingw-target linker exports every global symbol
  by default absent an explicit export list, and FFmpeg's D3D11VA objects
  newly contribute roughly 1300 DXVA-mode/D3D11-IID constant-table symbols
  to that; `libc.dll` (1039 exports) and `libcrtgfx_gpu.dll` (756) already
  show the identical pre-existing pattern, so this is not new. Full result
  in `HISTORY.md`. Item 7 is now closed on all three hosts.

  * Rebuild FFmpeg and `libcrtmedia` from a fresh state on each acceptance host. (done: macOS, Linux, Windows)
  * Rebuild the normal `04-gfx-media` cumulative stage. (done: macOS, Linux, Windows)
  * Run packaged media consumers, not only build-tree tests. (done: macOS, Linux, Windows)
  * Audit binary/runtime dependencies:

    * macOS: VideoToolbox/CoreVideo/CoreMedia-related frameworks (done -- confirmed via `otool -L` against `tools/crt_dist_prerequisites.py`)
    * Windows: D3D11/DXGI-related imports (done -- clean; see item 7's own note)
    * Linux: expected VA-API/runtime library dependencies (done -- `libva.so.2`/`libva-drm.so.2`, declared in `tools/crt_dist_prerequisites.py`)
  * Confirm no unexpected host ABI or allocator-domain dependency is introduced. (done: macOS, Linux, Windows)
  * Confirm existing public `crtmedia` ABI and software-only callers remain compatible. (done: macOS, Linux, Windows -- software-only path stays `hardware_accelerated=false`/decodes in software on all three)
  * Record exact host/architecture, GPU, decoder backend, FFmpeg configuration, test command, and result in `HISTORY.md`.
  * Keep raw logs/results outside git unless they are small, stable project fixtures.

---

### Acceptance gate

This tranche is complete when all of the following are true:

* [x] macOS/arm64 decodes the project H.264 fixture through real VideoToolbox hardware frames. (Tranche 1)
* [x] Windows/x64 decodes the same fixture through real D3D11VA hardware frames. (Tranche 3)
* [x] Linux decodes the same fixture through real VA-API hardware frames on a capable native host. (Tranche 4)
* [x] Every hardware path successfully transfers at least one decoded hardware frame into the existing CPU-resident `crtmedia` frame contract.
* [x] The expected decoded frame count, timestamp behavior, image-content validation, EOS, and cleanup checks pass on every host.
* [x] `hardware_active` or its equivalent means “a real hardware frame was actually observed,” not merely “a hardware device was created.”
* [x] Software-only decode remains green on every host.
* [ ] Hardware-unavailable or unsupported configurations fall back cleanly to software without breaking the decode contract.
* [ ] No public API exposes platform-native decoded textures or surfaces yet.
* [ ] Host/platform resources remain owned and released by FFmpeg/platform APIs, not by unrelated CRT allocator domains.
* [ ] Fresh packaged `04-gfx-media` builds and binary/import audits remain green on the supported matrix.

**Decision:** when this gate is green, move **Hardware video decode** to `HISTORY.md` and promote **Zero-copy decoded textures** into `In Progress`.

### Recommended execution order

* [x] macOS/arm64 — establish the first real VideoToolbox green.
* [x] Harden common reporting/lifetime semantics using the macOS evidence.
* [x] Windows/x64 — solve D3D11VA FFmpeg enablement and obtain real hardware evidence.
* [x] Linux — first remove any toolchain/build-environment blocker, then obtain real VA-API evidence.
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
