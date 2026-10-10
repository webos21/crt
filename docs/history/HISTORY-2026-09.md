# CRT History — September 2026

September converted the August runtime/graphics groundwork into the staged CRT
SDK architecture and closed the major upper-runtime milestones through
**`04-gfx-media`**.

The month established the cumulative chain:

```text
01-c
  |
02-cxx
  |
03-gfx-simple
  |
04-gfx-media
```

and then validated GPU presentation, hardware media, decoded GPU texture
interop, capture/encode, and HTTP/HTTPS streaming across the primary hosts.

This file is a condensed archive of the original `HISTORY.md`.

---

## Month at a glance

Major outcomes:

- formalized cumulative SDK stages and isolated predecessor-only builds;
- completed native GPU presentation with Skia on Metal, D3D12, and Vulkan;
- hardened `libcrtgfx` backend boundaries;
- completed hardware H.264 decode on macOS, Windows, and Linux;
- completed decoded GPU-frame integration:
  - macOS: direct zero-copy;
  - Linux: direct zero-copy;
  - Windows: measured GPU-copy fallback, no CPU readback;
- completed encode and capture across the three hosts;
- completed bounded HTTP/HTTPS networking and streaming;
- published the first `v0.4.0-preview.1` developer preview;
- after `04-gfx-media` closure, reordered the future roadmap to
  `05-ui -> 06-web`.

---

## Stage architecture and distribution closure

### `01-c`

The C runtime became a real standalone SDK stage rather than merely part of a
monolithic repository build.

The stage packaged:

- CRT libc/libm/libdl and startup/runtime support;
- shell/rootfs tooling;
- port recipes and CRT compiler wrappers;
- the generic source-stage build runner.

A packaged `01-c` SDK could compile and run an external C consumer outside the
repository tree.

### `02-cxx`

The C++ stage became an explicit cumulative successor of `01-c`.

The isolated transition:

```text
01-c -> 02-cxx
```

validated that libc++, libc++abi, and the applicable unwinder could be built
from a pinned source-stage asset using only the predecessor SDK plus declared
build tools.

Acceptance included:

- static/shared runtime builds;
- C++ ABI/runtime smoke tests;
- package verification;
- no repository header/library leakage.

The transition was validated on the supported desktop hosts.

### `03-gfx-simple`

The simple graphics stage became the next isolated cumulative transition:

```text
02-cxx -> 03-gfx-simple
```

It contained the native window/input layer without requiring the full
Skia/media stack.

Important distribution work included:

- a standalone stage CMake project;
- reusable window-source inventory shared with the in-tree build;
- installed `gfx-simple` example rebuilt as an external consumer;
- Linux xkbcommon provenance and redistributed-dependency metadata;
- runtime prerequisite verification;
- cross-host path/toolchain fixes found only by real isolated builds.

By the end of the stage work, `03-gfx-simple` could be rebuilt and verified from
a packaged `02-cxx` SDK.

### `04-gfx-media`

The final September stage accumulated:

- Skia CPU/GPU;
- platform GPU backends;
- FFmpeg media;
- hardware decode;
- decoded GPU-frame bridging;
- capture/encode;
- HTTP/HTTPS streaming.

The isolated:

```text
03-gfx-simple -> 04-gfx-media
```

build became a major acceptance gate. It repeatedly exposed missing private
headers, stale source registries, RPATH/RUNPATH problems, omitted backend source
files, and undeclared runtime dependencies that in-tree builds could not see.

This reinforced a core release rule:

> A capability is not closed until its installed/isolated stage can reproduce
> the feature without repository build-tree leakage.

---

## Native GPU and Skia presentation

September completed live native GPU presentation across the three primary
graphics hosts:

| Host | GPU path |
| --- | --- |
| Linux | Vulkan + native Wayland |
| Windows | D3D12 |
| macOS | Metal |

The work included:

- native device/context/surface setup;
- Skia Ganesh integration;
- real window presentation;
- resize/swapchain recreation;
- pixel verification;
- backend-object and lifetime hardening.

Linux gained a dedicated native Wayland presentation backend while the older
software-oriented backend remained available during migration.

Backend-boundary work deliberately kept platform-native objects behind CRT
interfaces and prevented host SDK types from becoming public ABI.

---

## Hardware H.264 decode

The hardware-decode roadmap was closed on all three hosts.

### macOS

VideoToolbox became the first verified hardware path.

Acceptance required evidence of:

- hardware device creation;
- hardware pixel-format negotiation;
- real hardware frame observation;
- successful CPU transfer fallback path;
- clean EOS/lifecycle.

The bring-up also exposed real pthread/Apple ABI assumptions that were fixed in
CRT rather than hidden in media code.

### Windows

D3D11VA hardware H.264 decode was verified on physical GPU hardware.

The existing public CPU-frame API remained valid: decoded hardware frames could
still be transferred into the normal `crtmedia_frame` contract.

### Linux

VA-API hardware H.264 decode was verified on physical Linux hardware and later
included in release-stage acceptance.

### Reporting semantics

`crtmedia_codec_is_hardware_accelerated()` was tightened so it reflects
observed hardware-frame use rather than merely successful creation of a
hardware device.

Package acceptance was then replayed so hardware-decode support existed in the
actual `04-gfx-media` SDK, not only in the repository build.

---

## Decoded GPU textures and zero-copy work

After hardware decode, CRT added an additive GPU-frame dequeue path without
breaking the existing CPU-frame API.

A key architecture decision was preserved:

> `crtmedia` does not depend on `crtgfx`; the graphics layer imports a
> `crtmedia_gpu_frame` through an optional bridge.

### macOS — direct zero-copy

VideoToolbox/FFmpeg hardware frames retain their `CVPixelBuffer` backing and
are imported through Metal into Skia without CPU readback.

Final acceptance included real presentation, resize, pixel verification, and
lifecycle tests.

### Linux — direct zero-copy

VA-API frames are exported as DRM PRIME/dma-buf resources and imported into
Vulkan/Skia.

The final normalized result reported real zero-copy, GPU-backed textures, and
no CPU readback.

### Windows — honest GPU-copy classification

D3D11VA frames cross into the D3D12/Skia path through one GPU-side copy.

The result was deliberately classified as:

```text
interop=gpu-copy
```

rather than falsely calling it zero-copy.

The acceptance schema separated:

- zero-copy;
- GPU-copy;
- CPU-copy;
- GPU-frame delivery;
- texture backing;
- CPU readback.

Lifecycle testing also found and fixed device-affinity/resource-cache defects
that single-decoder tests had missed.

---

## Encode and capture

The Encode & Capture roadmap was completed through Tranche 6 on Linux,
macOS, and Windows.

### Portable baseline

CRT first established a deterministic software path:

```text
synthetic frames
    ->
software encoder
    ->
MP4 muxer
    ->
reopen
    ->
decode-back verification
```

This provided an environment-independent reference before native capture and
hardware encoders were introduced.

### Linux

- V4L2 capture backend;
- native-format conversion to CRT-owned frames;
- real USB/UVC camera acceptance;
- VA-API H.264 hardware encode.

### macOS

- AVFoundation camera capture;
- VideoToolbox H.264 hardware encode.

### Windows

- Media Foundation capture;
- Media Foundation hardware H.264 encode.

Cross-host closure added timing/discontinuity and lifecycle tests plus
isolated-stage package verification.

---

## Networking and streaming

The final `04-gfx-media` roadmap item was bounded networking and HTTP/HTTPS
streaming.

The work intentionally started from transport semantics rather than exposing
curl directly as public API.

### Tranche 0–1 — contract and bounded transport

`crtmedia_result` gained distinct timeout, cancellation, and protocol errors.

A host-neutral bounded byte queue provided:

- high/low watermark back-pressure;
- partial reads/writes;
- timeout vs `WOULD_BLOCK`;
- EOF semantics;
- cancellation;
- safe release of blocked readers/writers.

### Progressive HTTP input

The FFmpeg extractor gained HTTP input through CRT's curl stack and custom AVIO
behavior.

Acceptance covered:

- progressive non-seekable input;
- HTTP Range-backed seeking/reconnect where supported;
- real cross-host replay.

### HTTP output

CRT added bounded fragmented-MP4 HTTP output with slow-receiver back-pressure
and decode-back verification.

### Reconnect policy

Input reconnect became conservative and explicit:

- `Range`;
- `If-Range`;
- response validation;
- lost connection is not silently treated as clean EOF.

Output does not automatically resume in a way that could splice an invalid
byte stream.

### HTTPS

TLS was closed with verification enabled by default and explicit CA/SAN
acceptance rather than the earlier permissive test behavior.

### Lifecycle and packaging

Repeated open/close/reconnect/upload/TLS tests checked fd/thread/handle growth.

The final isolated `04-gfx-media` builds on all three hosts included the
curl/mbedTLS/zlib dependency chain and installed streaming consumers.

With this, Networking & Streaming — and the planned September
`04-gfx-media` roadmap — was closed on all three hosts.

---

## Public developer preview

On 2026-09-21 the project published its first developer preview:

```text
v0.4.0-preview.1
```

The release was positioned around the Graphics/Media SDK.

Release preparation added:

- per-host SDK archives;
- pinned source-stage assets;
- checksums;
- release manifests;
- package verification;
- rebuilt installed examples;
- a public-facing README.

The release process itself found packaging issues that only appear after
extracting an archive and rebuilding examples as external consumers,
strengthening the predecessor-only and installed-SDK acceptance model.

---

## Roadmap transition after `04-gfx-media`

Once Networking & Streaming closed, the upper-runtime roadmap was reordered.

The earlier QuickJS/WebRTC sequence was replaced with:

```text
04-gfx-media
      |
05-ui
  crtui + LVGL
      |
06-web
  JavaScriptCore / WebCore / WebKit / PlatformCRT
```

Reasons:

- WebKit already brings JavaScriptCore, so a separate mandatory QuickJS stage
  would duplicate investment;
- WebRTC is a consumer-driven browser/media integration, not a prerequisite
  for browser bring-up;
- an application UI layer and producer-agnostic external surface should exist
  before WebView integration.

September 30 already implemented and replayed `05-ui` Tranches 0–3 on
Windows/x64, macOS/arm64, and Linux/x86_64. October continues with the
remaining UI tranches, full stage closure, and `06-web` bring-up.

---

## End-of-month stage state

| Stage | September 2026 state |
| --- | --- |
| `01-c` | cumulative C runtime/PAL SDK established and packaged |
| `02-cxx` | imported C++ runtime stage established and isolated |
| `03-gfx-simple` | native window/input stage established and isolated |
| `04-gfx-media` | GPU/Skia/media/network stage closed across primary hosts |
| `05-ui` | Tranches 0–3 implemented and replayed on three hosts; full closure still pending |
| `06-web` | planned after `05-ui` |

---

## Historical note

The original September `HISTORY.md` contained detailed per-host diagnostics for
GPU drivers, ABI mismatches, stage-source omissions, RPATH fixes, camera/device
availability, and individual acceptance runs. Those details remain available
from Git history and the subsystem acceptance documents:

- `docs/acceptance/crtmedia_hardware_decode_acceptance.md`
- `docs/acceptance/crtmedia_zero_copy_decode_acceptance.md`
- `docs/acceptance/crtmedia_encode_capture_acceptance.md`
- `docs/acceptance/crtmedia_networking_acceptance.md`
- `docs/design/runtime_roadmap.md`

This monthly archive intentionally keeps the stable outcomes rather than the
full investigation transcript.


## September 30: `05-ui` Tranches 0–3 already delivered

The original September log closes the public headless model, private LVGL
renderer, crtgfx input adapter, and SDK/privacy sample on Windows/x64,
macOS/arm64, and native Linux/x86_64. LVGL v9.6.0 was pinned and fetched with
size, SHA-256, and commit checks; its types and headers stayed private.

The model uses generation-checked handles, owner-thread validation, queued
reentrant input, and target-to-window bubbling. The renderer produces caller-
stride BGRA8888 pixels; the adapter adds keyboard focus, pointer/slider/wheel,
focus-loss cancellation, and resize delivery. Automatic layout and later surface
integration were not yet closed. The scripted window run is not proof of real
hardware input: macOS wheel sign, Wayland seat delivery/scroll, interactive
resize, and touch were explicitly unverified.

Privacy required more than hiding headers: the first DLL exported 2,286 `lv_*`
symbols. Explicit public visibility plus hidden LVGL symbols reduced the public
exports to exactly 35 `crtui_*` functions on all three formats. A sample calling
`lv_obj_create` fails the source/object privacy checks. The installed `ui-basic`
example reports `presented=30 pixel_check=pass input_check=pass`.

Two dependency-order findings matter for reproducing that result:

- On macOS, linking libSystem before CRT libc bound Apple's 64-byte mutex init
  to CRT's 40-byte storage. CRT libc first fixed the shared consumer crash;
  `crtui_contract_shared_test_runs` guards it. Other dylibs with the same order
  remained a follow-up at this date.
- On Linux, listing crtgfx before crtui fixed static `strndup` resolution. The
  final external example borrowed a missing `libxdg-shell-protocol.a` from the
  development tree, so this was not yet a clean predecessor-only UI closure.
  The options-ON in-tree curl distribution also retained an undeclared shared
  dependency; the isolated static-curl path was a different configuration.

Final September 30 recorded full-suite results were macOS 157/157 (expected
camera skip) and Linux 149/150 (no sound card); tooling was 79/79. Keep these
qualifications when citing the otherwise successful UI replay. Detailed gates
and mutation checks are in [crtui acceptance](../acceptance/crtui_acceptance.md).

## Root causes and reproducible evidence beyond the feature summary

| Dates | Finding / decision | Reproduction and evidence boundary |
| --- | --- | --- |
| Sep 1–2 | curl's failures exposed real PAL defects, including macOS pthread lookup through unsupported `RTLD_NEXT` and unadapted `SOCK_CLOEXEC`. | Exact-image pthread lookup plus fd-flag adaptation enabled threaded HTTP/HTTPS static/shared smoke. Recipe notes and the original dated log distinguish this from merely adding eventfd. |
| Sep 7 | Apple SDK consumers inferred a different libc++ string layout than the CRT-built runtime; a GPU-free string expression reproduced the SkSL crash. A separate host-libc++ weak-symbol collision caused `bad_cast`. | `_LIBCPP_CRT_BIONIC_ABI`, consistent installed headers, and Mach-O symbol isolation; cross-TU string/stream and `crtgfx_skia_sksl_test` checks. A contemporaneous FreeType failure was separate, not evidence to revert the ABI fix. |
| Sep 10 | Windows `waitpid(-1)` blocked on the first registered child rather than the first completed child, filling the registry during FFmpeg configure. | Whole-registry `WaitForMultipleObjects`, deferred mksh job reconciliation, slow-first/fast-second child regression, 40-child drain and 15 mksh tests; fresh FFmpeg static configure/build/install followed. |
| Sep 13–16 | Apparent Mesa/LLVM/SkSL heap corruption was ultimately a CRT cross-instance free: shared libc++ allocated through shared libc while executable static delete/free used another heap. Earlier attributions were retracted. | `CRT_ENABLE_DEBUG_MALLOC` owner evidence, GDB module/address mapping, cold-cache pinned m148/lavapipe reproduction; remove shared CRT links from static examples and enforce direct `DT_NEEDED` checks. Fixed run 3/3 and stage CTest 8/8 preceded a fresh-chain replay. Do not cite the earlier provisional Linux closure or LLVM attribution. |
| Sep 14–16 | Distribution correctness required relocated paths, declared dependency closure, runtime prerequisites, and static/shared ownership checks, not only successful linking. | `verify_dist.py`, extracted SDK examples and isolated stages; `RPATH`/`RUNPATH` and ELF/PE/Mach-O inventory checks. A stale predecessor missing `crt_macho.py` was distinguished from the fixed Skia defect. |
| Sep 16 | The allocator was retained based on bounded workload/contention evidence. An apparent timing gap came from quadratic qsort in percentile reporting, outside the measured workload. | Checked-in `benchmark/` JSONL, 1K/10K/100K (Linux also 1M), 1/8/16/32-thread sweeps, fork-region/fault/firewall tests; [baseline decision](../acceptance/allocator_baseline.md). Old wall times include reporting overhead; RSS can be null rather than zero. |
| Sep 18–22 | GPU presentation and hardware decode needed real pixels, resize/lifecycle, observed hardware frames, and packaged consumers. WSL attempts did not substitute for physical Linux acceptance. | [Live presentation](../acceptance/libcrtgfx_live_presentation_acceptance.md) and [hardware decode](../acceptance/crtmedia_hardware_decode_acceptance.md) record host/device prerequisites and commands. The Sep 21 preview predates later zero-copy/encode/network closure; clean-machine release verification was explicitly not pursued on Sep 23. |
| Sep 23–29 | GPU-frame interop, capture/encode, and networking were closed against distinct contracts. Windows interop used one GPU copy, not zero-copy; uploads did not gain unsafe automatic resume. | Per-host acceptance documents preserve normalized counters, fixtures, TLS negative cases, lifecycle bounds, and isolated package gates. Hardware-dependent success cannot be inferred from a headless unit test. |

For exact historical commands and intermediate corrections, use the immutable
source in the [archive guide](README.md). Current acceptance documents can evolve;
they do not replace the pinned September evidence.
