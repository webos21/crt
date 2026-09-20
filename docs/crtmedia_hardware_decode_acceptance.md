# crtmedia Hardware Decode (Phase A) Acceptance Contract

This document freezes TODO.md's "Hardware video decode" tranche's scope,
state model, and result-reporting shape before any implementation work
(Tranche 1 onward) begins, matching the precedent set by
`docs/libcrtgfx_gpu_backend_boundary.md` and
`docs/libcrtgfx_live_presentation_acceptance.md` for the two prior
tranches. Nothing in this document changes the public `crtmedia` ABI or
any existing behavior; it records what already exists, two real gaps
found while auditing it, and the exact contract later tranches close
against.

## Scope

In scope for this tranche:

- Compressed H.264 input, decoded through a real platform hardware
  decoder (VideoToolbox on macOS, D3D11VA on Windows, VA-API on Linux).
- A hardware-backed decoded frame, explicitly transferred/downloaded to a
  CPU-resident `crtmedia_frame` through the existing frame contract.
- Automatic, first-class software fallback whenever hardware decode is
  requested but unavailable, unsupported, or fails.

Explicitly out of scope (deferred to **Zero-copy decoded textures**):

- `CVPixelBuffer` → Metal texture sharing.
- D3D11/D3D12 decoded-texture sharing.
- VA-API surface → Vulkan image sharing.
- Any zero-copy decode-to-render path.
- Hardware encode.

## What already exists (audited, not assumed)

Real Phase-A groundwork already landed 2026-09-08, before this tranche's
own planning pass, and needs no rebuilding:

- **Format opt-in.** `CRTMEDIA_FORMAT_KEY_PREFER_HARDWARE_DECODE`
  (`crtmedia/format.h`) is an `int32`, video-only, opt-in key. Unset (the
  default) is byte-identical to today's software-only behavior for every
  existing caller.
- **Reporting API.** `crtmedia_codec_is_hardware_accelerated()`
  (`crtmedia/codec.h`) already exists as the public query for whether a
  given decoder instance actually used a real hardware decoder.
- **Per-host hwaccel mapping.** `hw_type_for_platform()`
  (`libcrtmedia/src/codec.c`) already maps
  `CRT_TARGET_OS_MACOS → AV_HWDEVICE_TYPE_VIDEOTOOLBOX`,
  `CRT_TARGET_OS_WINDOWS → AV_HWDEVICE_TYPE_D3D11VA`,
  `CRT_TARGET_OS_LINUX → AV_HWDEVICE_TYPE_VAAPI` -- exactly Phase A's own
  recommended per-host backend choice, already generic across all three
  hosts in one code path (no host-specific C files needed, unlike
  `libcrtgfx`'s own Vulkan/D3D12/Metal backend split).
- **Device creation, decode, and CPU download.**
  `crtmedia_codec_create_decoder()` already attempts
  `av_hwdevice_ctx_create()` when the opt-in key is set, wires
  `AVCodecContext::get_format` to offer the hw pixel format, and falls
  back to a fresh software `avcodec_open2()` if hardware device
  creation or codec open fails. `crtmedia_codec_dequeue_output()` already
  downloads a still-hardware-resident frame via
  `av_hwframe_transfer_data()` before handing it back as a CPU-resident
  `crtmedia_frame` (possibly `CRTMEDIA_PIXEL_FORMAT_NV12`).
- **Test and fixture.** `libcrtmedia/tests/hw_decode_test.c` already
  exercises the opt-in path against `libcrtmedia/assets/test_video.mp4`
  (a real, project-owned 64x64/25-frame H.264 MP4 fixture, already used
  by `tests/extractor_codec_test.c` for the software path). It already
  asserts frame count (25), dimensions (64x64), monotonic timestamps,
  owned frame storage, and -- when hardware decode is actually active --
  runs the resulting NV12 frame through `crtmedia_frame_convert_to_rgba()`
  and checks for real, non-degenerate output. Registered unconditionally
  as `crtmedia_hw_decode_test_runs` on every host once
  `CRTMEDIA_ENABLE_FFMPEG=ON`.

This tranche reuses every one of these unchanged. No new public API, no
new fixture, no new generic test file is needed to reach a first green.

## Two real gaps found while auditing the above

Freezing this contract meant reading the actual current implementation,
not assuming Phase A's 2026-09-08 landing already worked end to end on
real hardware. It has not: two concrete, confirmed gaps stand between
today's code and any host actually reporting `hardware_active=true`
honestly.

1. **The FFmpeg recipe never enables any hwaccel, on any host.**
   `porting/recipes/ffmpeg.json`'s `configure_args` uses
   `--disable-everything` plus explicit `--enable-decoder=`/
   `--enable-demuxer=`/`--enable-parser=`/`--enable-protocol=` allowlists
   -- there is no `--enable-hwaccel=h264_videotoolbox` /
   `h264_d3d11va` / `h264_vaapi` anywhere in the base args or any
   `target_overrides` block, on any host. This is not an oversight this
   pass discovered by inference: the recipe's own first verification note
   (2026-08-31) records `./configure`'s own summary literally reading "no
   encoders/**hwaccels**/muxers/filters/bsfs/indevs/outdevs" -- a
   self-documented confirmation, unchanged through every later dated note
   in that file (2026-09-01 through 2026-09-13). Consequence: even though
   `codec.c`'s hardware-decode logic is fully wired, `av_hwdevice_ctx_
   create()` may still succeed (creating a real device context is a
   `libavutil`/`hwcontext_*` capability independent of whether any
   decoder's hwaccel wrapper was compiled in), but the linked H.264
   decoder itself has no registered hwaccel to actually decode into that
   device's pixel format -- so `crtmedia_hw_decode_test` has, in all
   likelihood, silently taken the software-fallback path on every host
   it has ever run on, "passing" without ever having exercised real
   hardware. Fixing this (per-host `--enable-hwaccel=...`, verified from
   `./configure`'s own summary output) is explicitly Tranche 1/3/4's own
   job, not this freeze step's. **Closed for macOS (Tranche 1,
   `--enable-hwaccel=h264_videotoolbox`) and Windows (Tranche 3,
   2026-09-19: `--enable-d3d11va --enable-hwaccel=h264_d3d11va,
   h264_d3d11va2`, `h264_d3d11va2` being the variant that offers
   `AV_PIX_FMT_D3D11` through a device context); Linux/VA-API remains
   open (Tranche 4).**

2. **`crtmedia_codec_is_hardware_accelerated()` did not keep its own
   documented promise. Closed 2026-09-18 (Tranche 2).** Its header
   comment (`crtmedia/codec.h`) reads: "never a caller-side guess, this
   reflects what really happened." The implementation did not:
   `libcrtmedia/src/codec.c` set `codec->hardware_accelerated = 1`
   immediately once `avcodec_open2()` succeeded with a non-null
   `hw_device_ctx` -- *before* a single frame had been decoded, and the
   value was never revisited afterward. A real decoder can legitimately
   open successfully with a hardware device attached and still never
   actually decode a single frame into that hardware format (the same
   file's own comment on `crtmedia_codec_get_format()` already
   acknowledges this exact possibility: "a real decoder can legitimately
   decide not to offer the requested hw format at all"). In that case
   this API would have reported `hardware_active=1` even though every
   real decoded frame took the software path -- precisely the failure
   mode this tranche's own Step 0 scope explicitly ruled out ("Do not
   report hardware decode as active merely because `AVHWDeviceContext`
   creation succeeded"). Fixed by moving the flag's own true/false
   decision into `crtmedia_codec_dequeue_output()`, set only once a
   frame's format is confirmed equal to `hw_pix_fmt` *and* its
   `av_hwframe_transfer_data()` CPU download has actually succeeded.
   `crtmedia_codec_flush()` deliberately does not reset it -- the API
   answers whether this instance has ever used hardware, not whether the
   immediately previous frame did. Verified for real on macOS/arm64: see
   `HISTORY.md`'s 2026-09-18 Tranche 2 entry for the exact `RESULT` line
   and state-transition evidence.

## State model

Six states, matching TODO.md's own Step 0 list, mapped onto the real code
paths above:

| State | Where it lives today |
| --- | --- |
| hardware decode requested | `CRTMEDIA_FORMAT_KEY_PREFER_HARDWARE_DECODE` set on the input `crtmedia_format` |
| hardware device created | `av_hwdevice_ctx_create()`'s return value inside `crtmedia_codec_create_decoder()` |
| hardware pixel format selected | `codec->hw_pix_fmt` assigned from `hw_type_for_platform()`'s mapping, offered via `crtmedia_codec_get_format()` |
| real hardware frame observed | `codec->decode_frame->format == codec->hw_pix_fmt` inside `crtmedia_codec_dequeue_output()`, latched in the private `hw_frame_observed` diagnostic (`codec_test_control.h`) |
| hardware frame downloaded to CPU | `av_hwframe_transfer_data()` succeeding in that same function -- this is what now actually sets the public `hardware_accelerated` flag |
| software fallback used | any of the above failing, or the opt-in key unset -- `codec->hw_pix_fmt` stays `AV_PIX_FMT_NONE` |

`hardware_active` (the public `crtmedia_codec_is_hardware_accelerated()`
value) means "a real hardware-backed frame was actually observed and
downloaded" as of Tranche 2 (2026-09-18) -- state four and five above,
not state two. States one through four are also independently observable
through the private `crtmedia_codec_hw_diagnostics` struct
(`libcrtmedia/src/codec_test_control.h`, non-installed, test-only) for a
caller that needs the full breakdown rather than the one collapsed public
boolean.

## ABI decision

The public `crtmedia` ABI stayed unchanged through Tranche 2's own
reporting fix, exactly as planned: `crtmedia_codec_is_hardware_
accelerated()` kept its existing signature, and fixing *when* its
backing field is set was a correctness fix, not an API addition. The
finer-grained per-run diagnostic breakdown Tranche 2 also wanted
(requested backend, device created, pixel format offered, frame
observed, CPU transfer performed) landed as `crtmedia_codec_test_get_hw_
diagnostics()` in a new, private, non-installed header
(`libcrtmedia/src/codec_test_control.h`) -- test/diagnostic reporting,
never a new public `crtmedia` header, mirroring `libcrtgfx/src/
gpu_test_control.h`'s own established role for GPU backend-boundary
tests. Formalizing this as a real (if private) query function rather
than deriving every field ad hoc inside the test itself turned out to
matter concretely: once the public boolean's own meaning changed to
"frame transferred", `hw_device_created` and `hw_pixfmt_offered` needed
their own independently-tracked source of truth to keep reporting
correctly instead of collapsing to "no" alongside it.

## Result record

Since Tranche 1, `crtmedia_hw_decode_test` prints one additional final
machine-readable line to stdout, alongside its existing
`crtmedia_hw_decode_test: ok`/`FAIL ...` output:

```text
crtmedia_hw_decode_test: RESULT backend=<videotoolbox|d3d11va|vaapi|none> hw_requested=<yes|no> hw_device_created=<yes|no|n/a> hw_pixfmt_offered=<yes|no|n/a> hw_frame_observed=<yes|no> cpu_transfer=<pass|fail|n/a> frame_count=<N> fallback=<yes|no> eos=<pass|fail> clean_exit=<pass|fail>
```

Field meanings:

- `backend`: the per-host `hw_type_for_platform()` mapping's own name, or
  `none` when hardware decode was not requested for this run.
- `hw_requested`: whether `CRTMEDIA_FORMAT_KEY_PREFER_HARDWARE_DECODE`
  was set to a nonzero value for this run.
- `hw_device_created`: whether `av_hwdevice_ctx_create()` succeeded;
  `n/a` when `hw_requested=no`. Since Tranche 2, tracked independently in
  `codec->hw_device_created` -- no longer read from the public
  `hardware_accelerated` value.
- `hw_pixfmt_offered`: whether the decoder's own format negotiation ever
  offered `hw_pix_fmt` back through `crtmedia_codec_get_format()`; `n/a`
  when `hw_requested=no`. Tracked independently in `codec->hw_pixfmt_
  offered`, set inside that callback itself.
- `hw_frame_observed`: whether at least one real decoded frame's format
  equaled `hw_pix_fmt` before download -- the one field distinguishing a
  genuine hardware decode from a device that opened but never decoded
  through hardware. Tracked independently in `codec->hw_frame_observed`;
  distinguishing this from `hw_device_created`/`hw_pixfmt_offered` is
  exactly what let Tranche 0's gap 2 be observed directly in real
  acceptance evidence before it was fixed.
- `cpu_transfer`: `pass`/`fail` from `av_hwframe_transfer_data()` on the
  canonical observed hardware frame; `n/a` when no hardware frame was
  ever observed.
- `frame_count`: total decoded video frames (must equal the fixture's own
  25 regardless of which path decoded them).
- `fallback`: `yes` when `hw_requested=yes` but decode ultimately ran in
  software (device creation failed, codec open failed, or no hardware
  frame was ever observed).
- `eos`/`clean_exit`: same meaning as the live-presentation tranche's own
  `RESULT` line -- `pass` iff the run reached clean teardown with no
  assertion failure.

This shape distinguishes the three real outcomes TODO.md's Step 5 already
names: `hw_frame_observed=yes` (hardware-decode PASS), `hw_requested=yes,
hw_frame_observed=no, fallback=yes` (fallback PASS, not hardware-decode
PASS), and any `CHECK()` failure (FAIL).

## What Tranche 0 deliberately does not do

No source file changes in this pass: `porting/recipes/ffmpeg.json`'s
missing hwaccel flags (Tranche 1/3/4's job), `codec.c`'s premature
`hardware_accelerated` assignment (Tranche 2's job), and
`hw_decode_test.c`'s new `RESULT` line (Tranche 1's job, alongside the
recipe fix, so the first real hardware-decode run already reports through
this frozen shape) are all deferred to their own tranches. This mirrors
`docs/libcrtgfx_gpu_backend_boundary.md`'s and
`docs/libcrtgfx_live_presentation_acceptance.md`'s own Step/Tranche 0:
freeze the contract and the real findings first, implement against it
next.
