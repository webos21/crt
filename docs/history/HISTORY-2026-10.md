# CRT history — October 2026

This is a retrospective of October 1–31, not a current support matrix. It
summarizes the [complete original record, lines 1262–5966](https://github.com/webos21/crt/blob/88b172cf153a8b786fc98a8e17872ebce8b0efa2/HISTORY.md#L1262-L5966).
The [archive index](README.md#exact-source-and-recovery) explains recovery.

## UI and native media (October 1–4)

- **UI Tranche 4:** CRT-owned layout, styling, scrolling and text input shipped
  over private LVGL. Windows, macOS and Linux replayed the renderer and
  rebuilt the UI example against packaged `05-ui`. The macOS replay caught
  concurrent publication of `02-cxx` while a Skia demo linked; publication was
  serialized and the Skia build ordered behind the verified predecessor.
- **External surfaces and MediaView:** Tranche 5 separated scene/layer metadata
  from frame ownership. The Skia companion composes the producer image between
  UI planes, with clipping, opacity, damage and matched release; core UI
  exports no renderer ABI. Real D3D12, Metal and Wayland/Vulkan runs passed
  the five-frame surface check. A clean Linux package exposed a missing build
  edge for the installed shared Skia bridge. Tranche 6 added MediaView's GPU
  frame handoff and retained Windows GPU-copy as distinct from zero-copy.
- **Isolated UI delivery:** Tranche 7 verified `04-gfx-media -> 05-ui` on
  Windows/x64, macOS/arm64 and Linux/x86_64 and closed the UI tranche. It
  rebuilt installed examples, retained private LVGL notices without exposing
  its headers, and caught staging prefixes in text metadata that binary-only
  relocation checks missed. The text relocation pass made config tools and
  `.pc` paths package-relative, removed `.la` files and covered CRT shell
  drive spellings on Windows. The UI closure made Web Runtime the next stage.
- **Capture and build recovery:** AVFoundation teardown had released storage
  already transferred to the caller, and synthesized preset/key strings
  misreported actual camera dimensions. Queue ownership, delegate teardown
  and use of real framework constants fixed the authorized-device replay
  (15 capture and 15 VideoToolbox cycles; macOS CTest 160/160).
  The libc++ fetch driver also learned to reconstruct missing sparse
  worktree files from verified cached pins rather than trusting clone presence.

Evidence: [UI acceptance](../acceptance/crtui_acceptance.md),
[capture acceptance](../acceptance/crtmedia_encode_capture_acceptance.md),
and the [early-month entries](https://github.com/webos21/crt/blob/88b172cf153a8b786fc98a8e17872ebce8b0efa2/HISTORY.md#L5190-L5966).

## WebKit: JavaScriptCore and the CRT platform (October 3–13)

- **Source and port setup:** first pinned and inventoried WPE WebKit 2.54.0
  as a reference. WebKit's private Skia stayed separate from CRT's Skia.
  The gperf and ICU ports exercised a C++ recipe sysroot, hosted `main`
  opt-in, static library selection, shared CRT startup and header layering.
  Declared build-host generators remained separate from target libraries.
- **Tranches 1–2:** the installed SDK drove the interpreter and Baseline,
  DFG/FTL, Wasm and profiler gates in their recorded native scopes. Linux
  contention exposed a deadlock when suspend/resume needed the TLS registry
  lock held by a parked thread, and torn short copies during concurrent JIT
  call-site updates. The fixes were a lock-free read path and Bionic-shaped
  small-copy behavior, with signal-thread and memcpy regressions. Windows
  required thread-directed trap delivery, signal-state cleanup and a single
  CRT owner for emulated TLS across DLLs; macOS and Windows published thread
  stack bounds before returning the thread ID. Distinguish polling-trap and
  signal-trap results, actual tier evidence and Windows/arm64 core coverage
  from JavaScriptCore acceptance. The Linux WPE run remained a reference, not
  the product backend.
- **Tranche 3:** froze frame/input contracts and validated a Linux
  WPEPlatform CRT prototype before building the real `PlatformCRT` overlay.
  A pinned upstream commit, source-archive comparison, per-file licensing
  and patch manifest preceded JavaScriptCore, WebCore/Skia and dependency
  builds. The multi-process path added software-frame transport and input;
  B7 exercised repeated create/destroy and both static and runtime host-ABI
  audits. Linux ARM64 evidence here covers the recorded early slices, not
  native acceptance of a packaged `06-web`.
- **IPC and platform corrections:** IPv6 ABI/option translation, AF_UNIX
  streams and descriptor transfer, Windows handle ownership, and macOS
  sequenced-packet behavior were verified separately. The public ABI kept
  CRT shapes rather than exposing host socket layouts. The stream-backed
  adapters preserved record boundaries on Windows and macOS without claiming
  full host support for every AF_UNIX feature.
- **Public browser boundary:** `libcrtweb`, its host process, and the
  `crtui` WebView kept WebKit headers private. The lifecycle gates covered
  load/close, recovery, teardown, multiple runtimes, website data, downloads,
  dialogs/popups and the bounded IME contract. Windows constructors and record
  locks, and macOS path tails, symbolic-directory removal and loader paths
  were replay findings, not WebKit API changes.

Reproduce using the [Web acceptance gates](../acceptance/crtweb_acceptance.md)
and `tools/build_webkit_jsc.py`; keep the native host and per-mode limitations
with each result.

## Web media and GPU integration (October 13–18)

- **Tranche 8:** bound Web network trust to CRT, connected `crtmedia` playback
  and tested camera permission tokens and capture teardown. macOS initially
  hung or faulted because `libcrtmedia` exported its private curl symbols,
  FFmpeg bound pthread calls to libSystem, and SDK copying could duplicate a
  libc++ image. Export scoping, CRT-first linkage and preserved symlinks fixed
  those regressions. OS media frameworks remained declared PAL dependencies.
  Output-drain fixes made pacing evidence depend on accepted PCM output;
  camera evidence distinguishes synthetic sources from opt-in physical
  devices. Capture and audio behavior remain scoped to the actual hardware
  and sink used in each run.
- **Tranche 9:** separately accepted GPU final composition, shared GPU
  resources and owned-worker recovery. Native backends were Vulkan on Linux,
  D3D12 on Windows and Metal on macOS. A macOS availability report was not
  resource acceptance; a later Metal replay verified real resource import.
  The owned-worker shutdown and recovery checks did not turn the browser's
  CPU-painted frame path into zero-copy WebCore rendering.
- **Process lifecycle:** the B7 pre-exec allocator fault led to a native
  fork/spawn heap-snapshot regression and fixes on macOS, replayed on Linux.
  Windows follow-ups used committed stack extent rather than stale TEB
  limits, preserved child-owned descriptor handles and rebound native RNG
  and directory search state.

Evidence: [Tranche 8 closure](../acceptance/crtweb_acceptance.md#linux-focused-8b-replay-and-tranche-8-closure-2026-10-15),
[Metal shared-resource replay](../acceptance/crtweb_acceptance.md#9b-macosarm64-metal-shared-resource-implementation-2026-10-16),
[B7 heap snapshot](../acceptance/crtweb_acceptance.md#b7-native-forkspawn-allocator-follow-up-2026-10-17),
and [Windows worker lifecycle](../acceptance/crtweb_acceptance.md#9c-windowsx64-native-replay-2026-10-18).

## Web distribution and release qualification (October 18–28)

- **Tranche 10:** built the predecessor-only `05-ui -> 06-web` package and
  ran relocated browser/network and binding audits. Windows exposed
  host-generator/target ICU separation, media DLL exports, and fork/port-tool
  defects. Linux isolated stages exposed missing curl and a VA-API lifetime
  issue; macOS native replay exposed isolated-stage and binding differences.
  The [October 22 closure](../acceptance/crtweb_acceptance.md#tranche-10-closure-and-owner-release-approval-2026-10-22)
  accepted only the recorded scope. Windows/arm64 exercised `01-c..05-ui`,
  not `06-web`; the Linux/aarch64 Full package was built cross-target, not
  executed as native product acceptance. A labelled synthetic camera and
  null audio sink in the installed Linux replay were not hardware evidence.
- **Redistribution:** the 2.54.1 WebKit re-pin, inherited notices, generated
  LGPL source offer and FFmpeg relink kit became release checks. The owner
  approved the recorded scope under the WebKit support policy, not an
  unrestricted publication. The old binary Preview was withdrawn while
  its source tag remained. The later local release rehearsal used a
  verified WebKit 2.54.2 source pin.
- **Local assembly:** at clean `3741ea5`, the Dev/Full collectors were
  exercised with native Linux/x64, macOS/arm64 and Windows/x64 evidence,
  a native Linux/aarch64 Development build and a separate Linux/aarch64
  cross Full build. Rehearsals checked extraction, inherited-runtime hashes,
  source bundles, provenance and where applicable browser/transport
  behavior. These were not tagged uploads, promoted SDK pins or native
  ARM64 Full execution. Conflicting Linux source-asset variants remained
  an identity question at month end; the November record owns their later
  common-source selection.

Reproduction entry points and exact receipts: [Web acceptance](../acceptance/crtweb_acceptance.md),
[final assembly review](../acceptance/release_qualification.md#final-combined-assembly-review-2026-10-28),
and [November source selection](../acceptance/release_qualification.md#common-source-candidate-selection-2026-11-02).

## Build-system consolidation and SDK split (October 22–27)

### Complete build-system consolidation (B5)

The B0–B5 sequence unified the public CMake stage targets and established
configuration preflights, validated predecessor/source identity and correct
cache reuse. Cache hits depended on actual output inventory and tool/config
identity, rather than directory existence or the spelling of `PATH`.
Interrupted preparation and changing inputs invalidated reuse. Windows
replay exposed deep source paths, missing DLL byproducts, carried patch
execution needing declared host tools and publication contention. Fixes kept
execution and recovery scoped to the affected artifacts. Ordinary media SDKs
were brought into the isolated Web relink contract rather than exempted.
Native B5 build-system closure is separate from release qualification.
[B5 evidence and commands](../acceptance/build_system_acceptance.md#b5-closure-2026-10-26).

### Development and Full SDK split

Development is the reduced `02-cxx` profile with the required static/shared
ports and a portable contract for external consumers; Full retains its
cumulative graphics/media/UI/Web payload. The split checked notices,
dependencies, relocation, static/shared provider behavior and source-hidden
consumption. Separate S5 runs exercised extracted archives, hidden producer
trees and negative/handoff cases on the recorded native hosts; on Windows
the restricted-token denial was part of the evidence. Do not infer CLI
adoption or a public release from an SDK-only pass. Windows/arm64 kept its
lower-stage Full reference. [SDK split acceptance](../acceptance/sdk_split_acceptance.md).

## First native CLI, PAL and provenance (October 28–31)

- **Ownership:** `crt-cli` is a separate native executable. It owns project
  and SDK selection and invokes LLVM directly from `crt.json`; CRT owns
  startup/link policy, SDK identity and producer tooling. Temporary exported
  policy and immutable SDK handoffs enabled native replay without assuming
  promotion. CRT added platform/home and loaded-image queries and packaged
  JSON/SHA-2 ports instead of duplicating OS code in the CLI.
- **Producer defects:** Linux `readdir` was reading the wrong `getdents64`
  field for directory type; the corrected byte offset and file-path
  regression landed at `64b447b`. Replay also exposed missing process-group
  spawn behavior and a Linux shared-executable search-path gap. ARM64 Linux
  required its kernel-specific `O_NOFOLLOW`/`O_DIRECTORY` values. Native
  Windows evidence kept process/module inventory distinct from ELF and
  Mach-O inspection; October findings on Windows provider options and
  stdio are not statements about the later November fixes.
- **Windows strings and trust:** the adopted application manifest requests
  the UTF-8 active code page (Windows 10 1903+), and CRT reports the active
  encoding. Initial ARM64 validation ran 167/167 C cases; its public/Unicode
  replay limits remained explicit. The controlled HTTPS harness tested
  transport and explicit-key verification with disposable credentials,
  not an official publisher identity. D1 separately exposed direct make
  `SHELL` paths with spaces; the explicit symlink bridge passed.
- **Licensing:** Apache-2.0 replaced the former project default for original
  CRT work. Imported/adapted code kept its terms. Overlay manifests and
  per-file checks guard the split; the licensing-only pass did not relabel
  old artifacts or move consumer locks.

### Revalidate the latest CLI on Linux/x86_64 with a fresh clean Dev SDK

The clean `f826397` Dev SDK (`v0.4.0-cli.20261031`, local) retained exact
producer identities and passed 147/147 core tests plus imported C++ and
static/shared consumer checks. CLI `36c6f65` passed 684 native checks, cold
and warm builds, SDK-hidden execution and an ELF import audit. A separate
self-build fixed-point replay passed and the controlled HTTPS run covered
17 cases. No archive promotion, production pin or public release followed
from this test. [CLI-owned replay and limits](https://github.com/webos21/crt-cli/blob/af849da/docs/acceptance/bootstrap_acceptance.md#linux-latest-source-revalidation-2026-10-31).
