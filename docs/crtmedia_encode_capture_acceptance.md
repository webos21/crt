# Encode & Capture Acceptance

## Scope and host order

Encode & Capture extends the accepted `crtmedia_frame`/`crtmedia_gpu_frame`
contracts into this pipeline:

```text
capture or synthetic producer
  -> pixel conversion
  -> software/hardware encoder
  -> timestamped encoded samples
  -> container muxer
  -> reopen and decode-back verification
```

The implementation order is physical Linux/x86_64, macOS/arm64, then
Windows/x64. Linux is the first host because V4L2, VA-API, DRM/dma-buf, and
the existing Vulkan zero-copy path are all inspectable in one environment.
Host-specific capture and hardware encode do not change the public ownership
or timestamp contracts below.

## Frozen common contract (Tranche 0)

- Public timestamps are signed microseconds. Capture frames use
  `crtmedia_frame::timestamp_us`; encoded output uses `pts_us`, `dts_us`, and
  `duration_us`. A backend must convert its native clock at its boundary.
- Encoder input follows the existing frame ownership contract. `queue_frame()`
  copies CPU planes before returning; a later GPU-input path must explicitly
  retain/release the submitted `crtmedia_gpu_frame` instead of borrowing it.
- `crtmedia_encoded_sample` owns its compressed bytes until
  `crtmedia_encoded_sample_release()` and carries key-frame state separately
  from EOS. EOS is queue state, never a fake zero-byte sample.
- `crtmedia_muxer` owns the output stream and copies track configuration.
  Call order is create -> add tracks -> start -> write samples -> finish ->
  release. The first container is MP4, but track indexing is already
  multi-track.
- FFmpeg and host SDK types remain private implementation details. Public
  formats use `crtmedia_format` and codec-specific `csd-0` bytes.
- Capture output uses the existing CPU/GPU frame contracts. The first public
  capture ABI is now backed by the Linux V4L2 producer: native mmap buffers
  are converted/copied into owned YUV420P frames and immediately requeued, so
  caller/encoder back-pressure cannot retain or exhaust the driver queue.
- Software encode remains the mandatory fallback. Hardware paths report what
  actually ran and must be compared against the same mux/decode-back gate.

The first portable video format is `video/mp4v-es` with YUV420P input, backed
by FFmpeg's built-in MPEG-4 Part 2 encoder. This is intentionally independent
of external x264/OpenH264 packages. H.264 hardware encoding begins with
VA-API in Tranche 3.

## Tranches and gates

### 0. Common encode/mux/capture contract — complete on 2026-09-27

Public additions:

- `crtmedia_codec_create_encoder()` / `queue_frame()` /
  `dequeue_encoded_output()` / `get_output_format()`
- owned `crtmedia_encoded_sample`
- `crtmedia_muxer` MP4 track/start/write/finish lifecycle
- format keys for pixel format, frame rate, and bitrate

### 1. Linux synthetic software encode and MP4 — complete on 2026-09-27

`crtmedia_encode_mux_test` generates 100 deterministic 64x48 YUV420P frames
at PTS `0, 33333, 66666, ...`, encodes them with the software MPEG-4 encoder,
writes MP4, reopens it through `crtmedia_extractor`, and decodes it through
`crtmedia_codec`. Acceptance requires:

- exactly 100 encoded packets and 100 decoded frames;
- exact, monotonic round-trip PTS for every frame;
- exact track duration of 3,333,300 microseconds;
- expected dimensions/layout and bounded first/last luma values;
- no checked-in generated media artifact.

Verified on physical Linux/x86_64 with the project's CRT-linked FFmpeg 8.1.2
port. The recipe now enables only the additional MPEG-4 encoder/decoder/parser
and MP4 muxer needed by this gate. Its build also uses `-iquote.` so an
in-place rebuild cannot select stale installed FFmpeg headers ahead of the
fresh source tree.

### 2. Linux V4L2 capture — complete on 2026-09-28

`crtmedia/capture.h` adds host-neutral enumerate/open/start/dequeue/stop/
release calls. The private Linux backend scans V4L2 streaming capture nodes,
negotiates YUV420, NV12, YUYV, or MJPEG/JPEG plus frame rate, requests a
four-buffer mmap queue, converts/copies each dequeue into a tightly packed
owned YUV420P frame, then requeues the native buffer before returning. PTS is
a strictly monotonic microsecond timeline relative to the first captured
frame. No V4L2 type or fourcc appears in an installed header.

`crtmedia_capture_conversion_test` is resource-free and verifies byte-exact
YUV420/NV12/YUYV conversion and owned-release behavior. `crtmedia_capture_
v4l2_test`, when a supported device exists, captures 30 frames and runs them
through software MPEG-4 encode, MP4 mux, reopen, and decode-back with exact
frame counts and monotonic PTS.

Real, honest hardware finding, not a code defect: this project's own physical
Linux/x86_64 acceptance host's first real USB UVC webcam (`gspca_zc3xx`
driver) offers **no raw YUV420/NV12/YUYV streaming format at all** --
confirmed via `v4l2-ctl --list-formats-ext`, which lists only `'JPEG'`
(`V4L2_PIX_FMT_JPEG`, not even `V4L2_PIX_FMT_MJPEG`'s own fourcc, a real,
driver-level substitution `VIDIOC_S_FMT` performs silently) at 320x240/
640x480 -- matching the common reality that most UVC webcams only stream
MJPEG at useful resolutions/frame rates. The V4L2 backend's negotiation list
now falls back to MJPEG/JPEG last (after every raw format, since it costs a
real per-frame decode) and decodes each captured JPEG image through FFmpeg's
own built-in `AV_CODEC_ID_MJPEG` decoder (`porting/recipes/ffmpeg.json` gained
`--enable-decoder=mjpeg`, a zero-new-host-dependency addition -- FFmpeg's
MJPEG decoder is self-contained, no external libjpeg) before handing off the
same owned, tightly packed YUV420P frame contract every other source format
already produces. A second real finding on the same camera: its JPEG frames
decode as `AV_PIX_FMT_YUVJ422P` (4:2:2 chroma), not 4:2:0 -- handled by
vertically downsampling the decoded chroma rows (averaging adjacent row
pairs), the identical technique the raw-YUYV branch already uses. This MJPEG
path is compiled in only when `CRTMEDIA_ENABLE_FFMPEG` is on (a plain
FFmpeg-less configure gains no new dependency); `crtmedia_v4l2_convert_to_
yuv420p()` itself is untouched and stays resource-free-testable for the three
raw formats.

Real result, three consecutive runs, this physical USB UVC camera: `crtmedia_
capture_v4l2_test: ok frames=30 device=/dev/video0 size=640x480 fps=30` -- 30
real captured (and MJPEG-decoded) frames, software MPEG-4 encoded, MP4 muxed,
reopened, and decoded back with exact frame count and monotonic PTS, using
the unmodified existing encoder/muxer/decoder pipeline. Full `ctest` green
except one pre-existing, environment-caused failure unrelated to this
tranche (`crtmedia_playback_pipeline_test_runs`'s own real-wall-time-pacing
check, which needs a real audio device -- this host reports `aplay -l: no
soundcards found`, so the player's audio-sink-paced sleep path is never
exercised; not something this tranche's own V4L2/MJPEG change touches).

### 3. Linux VA-API H.264 encode — complete on 2026-09-28

`crtmedia_codec_create_encoder()` gained a `mime="video/avc"` branch
(`libcrtmedia/src/codec.c`): creates a real `AV_HWDEVICE_TYPE_VAAPI` device
and an `AV_PIX_FMT_VAAPI`/`sw_format=AV_PIX_FMT_NV12` hardware frame pool,
then feeds FFmpeg's own `h264_vaapi` encoder (`porting/recipes/ffmpeg.json`
gained `--enable-encoder=h264_vaapi`, reusing the same libva/libva-drm this
recipe's existing `--enable-vaapi` decode support already requires -- no new
host dependency). `crtmedia_codec_queue_frame()` converts the caller's
YUV420P planes into an NV12 scratch frame (Y direct copy, U/V interleaved),
uploads it into a real hardware surface via `av_hwframe_get_buffer()`/
`av_hwframe_transfer_data()`, and sends that hardware frame to the encoder --
there is no software H.264 fallback inside this codec instance (FFmpeg ships
no built-in H.264 encoder; only external x264/OpenH264, which this project's
own porting policy declines per this document's own Tranche 0 note), so
creation itself fails honestly (`CRTMEDIA_ERROR_UNSUPPORTED`) on a host with
no usable VA-API encode device, rather than silently downgrading to a
different codec. Reaching `CRTMEDIA_OK` from `crtmedia_codec_create_encoder(
"video/avc")` is therefore itself the "actually used VA-API, not inferred"
evidence this tranche's own acceptance gate requires -- no separate
diagnostic API was needed.

`crtmedia_muxer_add_track()` gained the matching `mime="video/avc"` ->
`AV_CODEC_ID_H264` branch. No bitstream-format conversion code was needed on
this project's own side: FFmpeg's `h264_vaapi` encoder emits Annex-B
(start-code-prefixed) NAL units for both extradata and every packet (its own
`vaapi_encode_h264.c`/`cbs_h2645.c`, confirmed by reading `ff_cbs_h2645_
assemble_fragment()` directly, not assumed), and libavformat's own `movenc.c`
(`mov_write_single_packet()`) already detects Annex-B H.264 extradata (its
own first byte `!= 1`, the avcC `configurationVersion`) and reformats both
the extradata and every packet into the MP4-required avcC/length-prefixed
form automatically (`ff_isom_write_avcc()`/`ff_nal_parse_units()`) -- a real,
already-existing FFmpeg feature this tranche only needed to discover and
rely on, not reimplement.

New `crtmedia_encode_vaapi_test`: 100 deterministic synthetic YUV420P frames
(the identical generator Tranche 1's own `crtmedia_encode_mux_test` uses) ->
VA-API H.264 encode -> MP4 mux -> reopen -> plain software H.264 decode-back,
run back to back against the same 100 frames through the existing mp4v-es
software encoder (the "compare timing/ownership with the software fallback"
requirement) on the identical source. Real result, three consecutive runs,
this physical host's Intel UHD 630 (confirmed `VAEntrypointEncSlice` for
`VAProfileH264*` via `vainfo`): `crtmedia_encode_vaapi_test: ok path=vaapi
frames=100 encode_ms=~630 decode_ms=~120 fallback_frames=100
fallback_encode_ms=~50 fallback_decode_ms=~60` -- both round trips decode
back exactly 100 frames with monotonic PTS; the VA-API path's own wall time
here reflects real per-frame VA-API submit/sync overhead at this tiny
64x48 synthetic resolution, not a throughput regression at realistic capture
resolutions (not separately measured in this tranche). A standalone system-
`ffmpeg` sanity encode (`h264_vaapi`, unrelated to this project's own build)
confirms the driver/MP4 combination independently produces a standard,
`ffprobe`-verified `h264`/`yuv420p` MP4 on this same hardware. Deferred to
Tranche 4/5 evidence: measuring the VA-API path against a real V4L2-captured
source end to end (Tranche 2 above and this tranche are each independently
proven; a combined capture-to-VAAPI-encode pipeline test is not required by
either tranche's own acceptance gate and was not additionally built here).

### 4. macOS/arm64 — advanced ahead of Tranche 3

Advanced out of the plan's own numeric execution order (2026-09-27) because
the macOS/arm64 acceptance host has real capture hardware (a built-in FaceTime
HD camera) immediately available, unlike the Linux host at Tranche 3 above --
the tranche *number* is not reassigned; only the order it actually ran in
changed, exactly mirroring Tranche 2's own "implementation complete, physical
gate pending" split. Split internally into 4A and 4B specifically so a real
capture-contract problem and a real hardware-encoder problem can never be
confused with each other: 4A proves the existing, unmodified `crtmedia_
capture` ABI and the existing software MPEG-4 encoder both work against a
second, completely independent OS backend; only once that is green does 4B
add a second, real encode backend on top of the same already-proven capture
path.

**4A. AVFoundation capture -> existing software MPEG-4 encoder — complete on
2026-09-27.** `src/arch/macos/capture_avfoundation.c` implements the same six
`capture_internal.h` backend calls the Linux V4L2 backend does, driven
entirely through `objc_msgSend` against real `AVCaptureSession`/
`AVCaptureDeviceInput`/`AVCaptureVideoDataOutput` (no host SDK header, no
Objective-C compilation mode -- matching `window_cocoa.c`/`gpu_metal.c`'s own
established convention). The one genuinely new technique this backend needed,
confirmed for real via three standalone probes on this exact Mac before any
of it was written: a delegate object conforming to
`AVCaptureVideoDataOutputSampleBufferDelegate` can be built entirely at
runtime through the plain Objective-C runtime C API
(`objc_allocateClassPair`/`class_addMethod`/`class_addIvar`/
`objc_registerClassPair`), including a per-instance ivar carrying this
backend's own C state pointer into the delegate callback -- no
`@interface`/`@implementation` needed anywhere in this project's own source.
Requests `kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange` (NV12) explicitly
and converts directly from the locked `CVPixelBuffer`'s own Y/UV plane
pointers into an owned, tightly packed YUV420P `crtmedia_frame` --
`crtmedia_avfoundation_convert_nv12_to_yuv420p()`
(`capture_avfoundation_test_control.h`) is the private, resource-free-
testable seam, mirroring `crtmedia_v4l2_convert_to_yuv420p()`'s own role.

Real, honest environment finding, not a code defect: `authorizationStatus
ForMediaType:` and `requestAccessForMediaType:` both proved unreliable when
probing from a bare, un-launched binary on this exact host (macOS 26) -- real
camera frames only ever arrived when the same binary was invoked through
LaunchServices (`open`) from a signed `.app` bundle declaring
`NSCameraUsageDescription`. A plain CTest-invoked executable is exactly the
case that does not get it, so `crtmedia_capture_avfoundation_test` reports a
precise `skip reason=camera-authorization-unavailable` there rather than a
false pass or a hard failure -- the real, camera-hardware-backed run below
was done by wrapping the exact same, unmodified CTest binary in a throwaway
signed bundle and launching it with `open -W`, a verification technique, not
a change to how the test itself is built or invoked by CTest.

Real result, three consecutive runs, the built-in FaceTime HD camera:
`crtmedia_capture_avfoundation_test: ok frames=30
device=47B4B64B-7067-4B9C-AD2B-AE273A71F4B5 size=640x480 fps=30` -- 30 real
captured frames, software MPEG-4 encoded, MP4 muxed, reopened, and decoded
back with exact frame count and monotonic PTS, using the unmodified existing
encoder/muxer/decoder pipeline. `crtmedia_capture_avfoundation_conversion_
test` (resource-free: tightly packed and padded-stride synthetic NV12,
plus three rejected-misuse cases) and the full `ctest` suite (142/142 with
`CRTGFX_ENABLE_SKIA=ON`/`CRT_USE_IMPORTED_LIBCXX=ON`/
`CRTMEDIA_ENABLE_FFMPEG=ON`) both pass. A real, unrelated stale-build
regression surfaced and was fixed along the way: this Mac's already-installed
FFmpeg predated the "Encode and capture" Tranche 0/1 recipe additions
(`--enable-encoder=mpeg4`/`--enable-muxer=mp4`), so `crtmedia_codec_create_
encoder()` failed even for the pre-existing, previously-accepted
`crtmedia_encode_mux_test` until the stale `port-tests/install` FFmpeg
artifacts were removed and `port-rebuild-ffmpeg` re-run -- the same known,
already-documented "rebuilding a port does not by itself relink its
consumers" gap `docs/release_preview.md`/`HISTORY.md` record for the
hardware-decode tranche; not specific to capture.

**4B. VideoToolbox H.264 hardware encode — complete on 2026-09-28.**
`crtmedia_codec_create_encoder()` (`mime=video/avc`) now selects
`avcodec_find_encoder_by_name("h264_videotoolbox")` on macOS, the direct
counterpart of Tranche 3's own `h264_vaapi` selection on Linux. Unlike the
VA-API path, VideoToolbox's own encoder lists `AV_PIX_FMT_YUV420P` directly
in its own `avc_pix_fmts[]` (confirmed by reading `libavcodec/
videotoolboxenc.c` directly), so this backend needs no `hw_frames_ctx`/
upload step at all -- the existing plain software-frame `codec->decode_frame`
path (already shared with the mp4v-es encoder) is reused unmodified;
`is_videotoolbox_encoder` exists purely so `crtmedia_codec_get_output_
format()` reports `mime=video/avc` for this instance. `porting/recipes/
ffmpeg.json`'s macOS `target_overrides` gained `--enable-encoder=
h264_videotoolbox` and a new `libavcodec/Makefile` patch giving
`videotoolboxenc.o` the same per-object `-fcrt-real-apple-sdk` CoreMedia/
CoreVideo/VideoToolbox framework access `videotoolbox.c`/`hwcontext_
videotoolbox.c` already had.

A second, genuinely new problem surfaced building this, distinct from (and
more subtle than) the framework-header need above: `videotoolboxenc.c` is
the first real-Apple-SDK file in this project that *also* directly uses this
project's own pthread/dl surface for real (a file-scope `pthread_once`
guarding lazy compat-symbol loading via `dlsym(RTLD_DEFAULT, ...)`, plus a
real per-instance `pthread_mutex_t`/`pthread_cond_t` pair). Confirmed for
real: letting this file see the real Apple SDK's own `<pthread.h>`
(exactly like its two siblings, which never touch pthread at all) produced
a real, reproduced infinite spin -- this project's own linked `pthread_
once()` (the only implementation ever linked into this project's own
binaries; real Apple libSystem's own is never linked) spinning forever on
memory laid out per Apple's real, much larger opaque `pthread_once_t`
struct. Three narrower fixes were tried and rejected in turn (pre-defining
just the `<pthread.h>` include guard; declaring a competing `pthread_once_t`
typedef; swapping this whole compile's header-search order to prefer this
project's own sysroot) -- each confirmed broken for a different reason, all
recorded in full in `porting/shims/macos/ffmpeg_videotoolboxenc_pthread_
dlfcn_compat.h`'s own top comment. The fix that actually works leaves
`<pthread.h>` completely untouched (Apple's real types are unavoidably what
`once_ctrl`/`lock`/`cv_sample_sent` really are in this translation unit,
since CoreFoundation's own header chain pulls them in independently of
anything done to the `<pthread.h>` umbrella itself) and instead uses
`#pragma redefine_extname` to redirect just the 9 specific pthread calls
this file makes to wrapper functions that resolve and call through the
*real* Apple implementation in `/usr/lib/system/libsystem_pthread.dylib` via
this project's own `dlopen()`/`dlsym()` -- correct precisely because that
memory really is Apple-shaped, so only Apple's own real implementation can
safely interpret it. A separate, simpler half of the same shim blocks just
`<dlfcn.h>` (via `-D_DLFCN_H_`, a plain macro guard with no opaque-struct
fallout) so this file's own `dlsym(RTLD_DEFAULT, ...)` compat-symbol lookups
use this project's own `RTLD_DEFAULT=(void*)0` convention instead of Apple's
real `(void*)-2` (which this project's own `dlsym()` would otherwise
misinterpret as a real image-handle pointer and crash on, confirmed for
real with a standalone probe). Both halves were verified independently with
standalone probes against this exact host's real SDK before landing in the
recipe, matching this project's own established discipline.

Real result: `tests/encode_videotoolbox_test.c` (a structural mirror of
Tranche 3's own `encode_vaapi_test.c` -- 100 deterministic synthetic frames,
VideoToolbox H.264 encode, MP4 mux, reopen, decode-back, compared against
the same round trip through the Tranche 1 software mp4v-es encoder run back
to back on the identical source) reports `crtmedia_encode_videotoolbox_test:
ok path=videotoolbox frames=100 encode_ms=102.8 decode_ms=15.1
fallback_frames=100 fallback_encode_ms=4.2 fallback_decode_ms=6.7` on this
host's real Apple Silicon VideoToolbox hardware encoder. Full `ctest` suite:
143/143 (142 pass, plus `crtmedia_capture_avfoundation_test`'s own already-
documented, expected `skip reason=camera-authorization-unavailable` when run
outside a LaunchServices-launched bundle -- see 4A above).

### 5. Windows/x64

Add Media Foundation capture and hardware encode behind the same contracts,
including a software fallback and MP4 decode-back acceptance on real hardware.

### 6. Cross-host closure

Close discontinuity and dropped-frame timing, stop/drain/flush behavior,
capture/encoder ownership stress, installed-header/link smoke, isolated
`04-gfx-media` stage coverage, packaged-SDK acceptance, and documentation
consistency on all three hosts.

## Non-goals for the current tranche

- network transport, adaptive streaming, or WebRTC;
- audio capture/encode before the video timing path is stable;
- claiming zero-copy merely because capture and encode are both hardware
  accelerated;
- importing host SDK types into installed CRT headers;
- patching upstream media source to bypass a missing CRT/PAL surface.
