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

### 2. Linux V4L2 capture — implementation complete, physical gate pending

`crtmedia/capture.h` adds host-neutral enumerate/open/start/dequeue/stop/
release calls. The private Linux backend scans V4L2 streaming capture nodes,
negotiates YUV420, NV12, or YUYV plus frame rate, requests a four-buffer mmap
queue, converts/copies each dequeue into a tightly packed owned YUV420P frame,
then requeues the native buffer before returning. PTS is a strictly monotonic
microsecond timeline relative to the first captured frame. No V4L2 type or
fourcc appears in an installed header.

`crtmedia_capture_conversion_test` is resource-free and verifies byte-exact
YUV420/NV12/YUYV conversion and owned-release behavior. `crtmedia_capture_
v4l2_test`, when a supported device exists, captures 30 frames and runs them
through software MPEG-4 encode, MP4 mux, reopen, and decode-back with exact
frame counts and monotonic PTS. The current Linux/x86_64 host has no
`/dev/video*`; the conversion test passes and the live test is correctly
classified `Skipped` with `reason=no-v4l2-device`. Tranche 2 remains open until
that same live gate passes on physical camera hardware.

### 3. Linux VA-API H.264 encode

Feed V4L2 or existing GPU-resident frames to VA-API H.264 where device
affinity permits it, mux MP4, decode back, and compare timing/ownership with
the software fallback. Report the actually-used path rather than capability
inference.

Deferred, not blocked: this host's own physical VA-API acceptance is out of
order relative to macOS below for a real resource reason (no available camera
on the Linux/x86_64 acceptance host at the time macOS's own real camera made
it the more productive next tranche to run), not a design or implementation
gap. No VA-API H.264 encode closure is claimed until this tranche itself runs.

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

**4B. VideoToolbox H.264 hardware encode — not started.** Add a second,
real hardware encoder behind `crtmedia_codec_create_encoder()`
(`mime=video/avc`) on top of the exact same, now-proven AVFoundation capture
path above, reporting the actually-used path rather than capability inference
(mirroring Tranche 3's own requirement), before this tranche is considered
closed.

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
