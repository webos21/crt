# libcrtgfx Live GPU Presentation Acceptance Contract

This document freezes the backend-neutral test shape TODO.md's "Finish live
GPU presentation evidence before hardware decode" tranche uses on every host
(Linux/Vulkan, macOS/Metal, Windows/D3D12). Step 1 of that tranche exists so
the remaining per-platform steps (macOS/arm64, macOS/x86_64, Windows/x64) add
evidence against one already-agreed shape instead of each inventing its own
frame count, resize target, or pass/fail definition. It refines
`docs/libcrtgfx_gpu_backend_boundary.md`'s own object-boundary contract:
nothing here may reopen backend-field access from a generic test.

## Why one shared shape

Before this tranche, "live presentation works" evidence was a visual
observation ("close the window to exit") plus a bare exit code. That evidence
class has a real, confirmed failure mode: 2026-09-17's Ganesh/Vulkan
single-frame-in-flight fence bug (`gpu_vulkan.c`'s
`crtgfx_gpu_vulkan_end_ganesh()`) deadlocked every *second* frame, yet a
human watching the first frame render correctly, or an acceptance script that
only ever asked for one frame, could not detect it. A frame count of exactly
one is not a live-presentation acceptance case; it is a construction smoke
test wearing the same clothes.

The generic test shape is intentionally identical across backends:

```text
draw_reference_scene()
request_resize(900, 520)      -- only when a resize case is exercised
verify_frame(...)             -- pixel-exact/tolerance readback check
continue_present(...)
```

Backend differences live entirely below this line, inside each owner's own
`gpu_vulkan.c`/`gpu_metal.c`/`gpu_win32.c` -- exactly the boundary Tranches
1-6 already established. The generic test never sees a native handle.

## Frozen decisions

- **Reference scene.** Reuse `tests/skia_reference_scene.h`'s existing
  `draw_reference_scene()`/`check_reference_scene()` pair unchanged. No new
  rendering workload. Its shapes are drawn at fixed small (0-60px) canvas
  coordinates regardless of the surface's own extent, so the same checks
  apply whether the live surface is 64x64 or 900x520 -- only the coordinates
  `check_reference_scene()` itself samples matter, not the surface size.
- **Frame counts.** A run takes an explicit, finite `frame-count`; `0` (run
  until window close) is a manual/interactive mode only, never an acceptance
  case. `5` is the standard acceptance frame count, matching the evidence
  already accepted for Linux/aarch64.
- **Resize case.** When exercised, the scripted resize target is fixed at
  `900x520` (logical/point units) and is requested immediately after the
  first presented frame -- so the second frame presented is always the first
  frame reflecting the new extent (`resize_frame=2`).
- **Canonical checked frame.** Exactly one frame per run is pixel-checked:
  the first post-resize frame when a resize is exercised, otherwise the
  first frame at the initial extent. This keeps the check deterministic and
  cheap (one CPU readback per run) without needing to verify every frame.
- **Pixel-check method.** A plain `SkSurface::readPixels()` against the
  live, Ganesh-wrapped swapchain/layer surface -- the same real Skia API
  `tests/skia_gpu_offscreen_smoke.cc` already uses offscreen. No new
  backend-specific readback hook: the existing swapchain image usage flags
  (`VK_IMAGE_USAGE_TRANSFER_SRC_BIT` etc., already required for Ganesh to
  wrap the image at all) already make this a real, supported operation.
- **Pass/fail rule.** Reuse `check_reference_scene()`'s own existing
  per-channel policy unchanged: exact match on binary alpha (`255`/`0`) and
  channel-dominance checks, a tolerance band only where the scene's own
  alpha-blended overlay is not bit-exact across backends. No new tolerance
  framework; a genuinely new check added by a later step should extend that
  shared function, not invent a parallel one.
- **Logical vs. backing size.** `requested_size` is the logical/point size a
  caller asked for (the scripted resize argument, or the initial window
  size when no resize is exercised). `backing_size` is the value
  `crtgfx_gpu_surface_get_size()` reports afterward -- the real backing
  pixel extent. These are expected to differ on a Retina macOS host
  (`requested_size=900x520`, `backing_size=1800x1040`); the contract treats
  the backing size as authoritative for anything pixel-coordinate-related,
  never assuming a 1:1 point-to-pixel ratio.
- **Test-only API boundary.** The generic test/demo source calls only the
  public `crtgfx/gpu.h`, `crtgfx/window.h`, and `crtgfx/skia.h` entry points
  plus the shared `tests/skia_reference_scene.h` helpers. It never includes
  `gpu_internal.h`, never sees a native Vulkan/Metal/D3D12 handle, and never
  gains a new private hook as part of pixel verification -- readback rides
  the same public `SkSurface` the draw call already used.

## Result record

Every run -- pass or fail -- prints exactly one final line to stdout:

```text
crtgfx_skia_gpu_window_demo: RESULT backend=<name> requested_size=<W>x<H> backing_size=<W>x<H> frames_requested=<N> frames_presented=<N> resize_frame=<n|n/a> pixel_check=<pass|fail|skip> post_resize_present=<pass|fail|n/a> clean_exit=<pass|fail>
```

Field meanings:

- `backend`: `vulkan` | `d3d12` | `metal` | `none`.
- `requested_size` / `backing_size`: logical and real backing extent, see
  above. `backing_size` reflects whatever the surface reports at process
  exit (post-resize when a resize was exercised).
- `frames_requested`: the `frame-count` argument (`0` prints as `0`, meaning
  unbounded/manual -- such a run is not acceptance evidence regardless of
  this record's other fields).
- `frames_presented`: the real count of successful presents.
- `resize_frame`: the 1-based index of the canonical post-resize frame, or
  `n/a` when no resize was exercised.
- `pixel_check`: `pass`/`fail` from the canonical frame's readback check, or
  `skip` when the run ended (by failure) before reaching that frame.
- `post_resize_present`: whether the canonical frame's own present call
  succeeded, or `n/a` when no resize was exercised.
- `clean_exit`: `pass` iff the process reached its normal teardown path with
  no failure and (for a bounded run) `frames_presented == frames_requested`.

This shape is deliberately the same on every backend; only values differ
(Retina `backing_size`, `backend=d3d12`/`metal`, ...). An acceptance script
should parse this one line rather than the surrounding diagnostic output on
stderr, which is free to vary.

## Reference implementation and evidence

`libcrtgfx/tools/skia_gpu_window_demo.cc` implements this contract in the one
C++ source shared by every backend (see its own top comment). Linux/aarch64
is this contract's reference host -- it already had the strongest live
presentation baseline (Tranche 2's fence-bug fix, verified 5-frame-plus-
resize and 30-frame-no-resize runs) -- and validated the shape end to end
2026-09-18:

```text
crtgfx_skia_gpu_window_demo: RESULT backend=vulkan requested_size=900x520 backing_size=900x520 frames_requested=5 frames_presented=5 resize_frame=2 pixel_check=pass post_resize_present=pass clean_exit=pass
crtgfx_skia_gpu_window_demo: RESULT backend=vulkan requested_size=800x480 backing_size=800x480 frames_requested=5 frames_presented=5 resize_frame=n/a pixel_check=pass post_resize_present=n/a clean_exit=pass
```

Linux/aarch64 is not a new acceptance gap this tranche needs to close (see
`TODO.md`'s own Step 5) -- its role here is a regression control confirming
the shared contract is sound, not a platform this tranche is hardening
further.

macOS/arm64 closed this contract's gap 2026-09-18 on real Metal hardware
with no changes to this file's own implementation:

```
crtgfx_skia_gpu_window_demo: RESULT backend=metal requested_size=900x520 backing_size=1800x1040 frames_requested=5 frames_presented=5 resize_frame=2 pixel_check=pass post_resize_present=pass clean_exit=pass
crtgfx_skia_gpu_window_demo: RESULT backend=metal requested_size=800x480 backing_size=1600x960 frames_requested=5 frames_presented=5 resize_frame=n/a pixel_check=pass post_resize_present=n/a clean_exit=pass
```

Confirms the contract's `backing_size`/`requested_size` split works exactly
as designed against a real Retina display: a `900x520`-point resize request
reports as a real `1800x1040`-pixel backing extent, and the pixel check
still passes because it is written against the reported backing size, never
an assumed 1:1 ratio. The originally planned macOS/x86_64 native-execution
gap is retired rather than closed (see `HISTORY.md`'s 2026-09-18 entry) --
this project's macOS acceptance target is now Apple Silicon/macOS 27+. The
one remaining real gap this contract's own values must still close is
Windows/x64 pixel-exact resize (`TODO.md` Step 4).
