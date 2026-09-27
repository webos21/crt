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

### 4. macOS/arm64

Add AVFoundation capture and VideoToolbox encode behind the same contracts,
including a software fallback and MP4 decode-back acceptance on real hardware.

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
