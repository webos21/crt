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
consumers" gap `docs/guides/release_preview.md`/`HISTORY.md` record for the
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

Done 2026-09-28. Media Foundation H.264 hardware encode
(`avcodec_find_encoder_by_name("h264_mf")`, FFmpeg's own `libavcodec/mfenc.c`,
`porting/recipes/ffmpeg.json`'s `--enable-mediafoundation
--enable-encoder=h264_mf`) and a new real Media Foundation capture backend
(`src/arch/windows/capture_mf.c`), with the software fallback and MP4
decode-back round trip run back to back on the identical synthetic source,
mirroring Tranche 3/4B's own shape.

**A real, project-wide `tools/crt-cc` linker bug, found while wiring
`--enable-mediafoundation`'s own configure probe.** FFmpeg's
`check_func_headers mfapi.h MFCreateAlignedMemoryBuffer -lmfplat
-fcrt-real-windows-sdk` probe failed to link (`lld: error: unable to find
library -lmfplat`) even with `CRT_WINDOWS_SDK_LIBPATH` set correctly.
Root-caused by manually reproducing the exact probe outside FFmpeg's
configure: this project's Windows target (`x86_64-w64-windows-gnu`) links
through LLD's *MinGW* driver flavor (`clang -v` shows `ld.lld -m i386pep`),
not `lld-link`, and that driver silently ignores `-Wl,/libpath:` (an
MSVC/`lld-link`-only flag) -- it is not an error, just a no-op. This had
never surfaced before because `kernel32.lib`/`synchronization.lib`
(`CRT_WINDOWS_SYSTEM_LIBS`) already resolve through mingw-w64's own bundled
`x86_64-w64-mingw32/lib` import stubs, so `CRT_WINDOWS_SDK_LIBPATH` was
never actually load-bearing until `mfplat.lib` (a real-SDK-only import
library with no mingw-w64 stub) needed it. Fixed in `tools/crt-cc` by
changing both Windows link sites from `-Wl,/libpath:${CRT_WINDOWS_SDK_LIBPATH}`
to plain `-L${CRT_WINDOWS_SDK_LIBPATH}` (a normal clang driver flag, correctly
recognized by the MinGW driver); confirmed with a minimal clang/lld repro
before and after the fix, then confirmed for real by rebuilding FFmpeg
end to end (`--enable-mediafoundation --enable-encoder=h264_mf` now
configures, builds, and installs cleanly).

**Two more real, `libcrtmedia`-scoped link gaps, found building the new
Windows backend/test targets for the first time.** (1) Any final link that
actually pulls in `capture_mf.c.obj` (`crtmedia_shared`'s DLL, and any
executable whose own code calls a `crtmedia_capture_*` function -- not
`crtmedia_encode_mf_test`, which never does) failed with `ld.lld: error:
could not open 'libuuid.a'`: LLD's MinGW driver adds `-luuid` to its own
internal default-library set whenever such an object is present, independent
of clang's `-nodefaultlibs` (which only suppresses *clang's* defaults, not
LLD's own) -- fixed by linking the real SDK's `uuid.lib` explicitly
(`CRTMEDIA_WINDOWS_UUID_LIB`). (2) `MFEnumDeviceSources` (device
enumeration) is declared in `mfidl.h` alongside everything else
`mfplat.lib`/`mfreadwrite.lib` cover, but is actually exported from a third,
separate import library, `Mf.lib` -- found by grepping every `um/x64` `.lib`
in the real SDK for the symbol after `ld.lld: undefined symbol:
MFEnumDeviceSources` with `mfplat.lib`/`mfreadwrite.lib` already linked.

**Real finding in `capture_mf.c` itself, found running the new capture test
against this host's own real webcam (a Logitech UVC camera).** Requesting
`IMFSourceReader::SetCurrentMediaType()` with subtype NV12 directly --
even with `MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING` set, which is supposed
to let Media Foundation's built-in video processor MFT convert from
whatever the device natively streams -- failed with `MF_E_INVALIDMEDIATYPE`
(`0xC00D5212`). This camera's own native types are RGB24 and I420 only (no
NV12); enumerating every native type by index showed I420 already present
verbatim. Fixed by having `crtmedia_capture_backend_open()` prefer the
camera's own native NV12 or I420 type directly (no conversion requested at
all) over asking the video processor to convert some other native type,
falling back to the original NV12-via-video-processor request only if
neither is natively offered. I420 is byte-for-byte this project's own
packed YUV420P plane layout, so the reader thread copies it out directly
(`crtmedia_mf_copy_i420_to_yuv420p()`) instead of running the NV12
de-interleave path (`crtmedia_mf_convert_nv12_to_yuv420p()`, kept for the
video-processor fallback and for any camera that does stream NV12 natively).

Real results, three consecutive runs each:
`crtmedia_capture_mf_test: ok frames=30
device=\\?\usb#vid_046d&pid_08d8&mi_00#... size=320x240 fps=30` (real
capture -> software encode -> MP4 -> decode-back) and
`crtmedia_encode_mf_test: ok path=mf frames=100 encode_ms=~18
decode_ms=~28 fallback_frames=100 fallback_encode_ms=~24
fallback_decode_ms=~11` (synthetic frames -> real `h264_mf` hardware encode
-> MP4 -> decode-back, compared against the Tranche 1 software mp4v-es
fallback on the identical source). Full `ctest` suite: 153/153.

### 6. Cross-host closure

Close discontinuity and dropped-frame timing, stop/drain/flush behavior,
capture/encoder ownership stress, installed-header/link smoke, isolated
`04-gfx-media` stage coverage, packaged-SDK acceptance, and documentation
consistency on all three hosts.

**Windows/x64 done 2026-09-28.** macOS/arm64's and Linux/x86_64's own
replays of the same two tests, and their own isolated-stage closures, are
all done too (below) -- this tranche is now closed on all three hosts.

Two new tests, both host-generic (unmodified across all three hosts --
`crtmedia_capture_enumerate()`/`crtmedia_codec_create_encoder("video/avc")`
already dispatch to whichever real backend each host's own recipe enables),
so this Windows pass is the *first* real-hardware run, not the only one:

- `tests/timing_discontinuity_test.c`: 20 deterministic synthetic frames
  with two real timeline gaps (a dropped-frame-sized gap and a ~10x stall)
  through software mp4v-es encode -> MP4 mux -> reopen -> decode-back,
  asserting every timestamp survives *exactly*, at both the encoded-sample
  and the muxed-container level -- no frame synthesized to fill a gap, no
  timestamp rounded away. This is possible to assert with exact equality
  (not just "close enough") because of two real properties confirmed by
  reading the source directly: `libcrtmedia/src/codec.c`'s encoder keeps an
  explicit PTS FIFO of the caller's own submitted timestamps rather than
  deriving output timestamps from frame-rate cadence, and `libcrtmedia/
  src/muxer.c` sets the MP4 stream's own time_base to `AV_TIME_BASE_Q`
  (microseconds), making every `av_rescale_q()` at mux/demux time an exact
  identity conversion. Real result: `crtmedia_timing_discontinuity_test: ok
  frames=20 gap1_us=66666 gap2_us=500000`.
- `tests/capture_encode_lifecycle_test.c`: 15 iterations of real Media
  Foundation capture (enumerate/open/start/dequeue 30 frames/stop/release,
  each frame fed through a software mp4v-es encoder, muxed, and decoded
  back) and, independently, 15 iterations of real `h264_mf` hardware encode
  (100 synthetic frames, muxed, decoded back) -- modeled directly on the
  Windows zero-copy Tranche 5/6 lifecycle test precedent (a real
  `DXGI_ERROR_DEVICE_REMOVED` found only on the *second* of 15 decoder
  cycles, HISTORY.md 2026-09-28), since a single pass through a device/
  encoder API rarely exercises real create/destroy resource-lifetime bugs.
  Real result: both halves completed all 15 iterations cleanly, every
  captured/submitted frame released exactly once -- `crtmedia_capture_
  encode_lifecycle_test: ok capture_iterations=15 capture_frames=450
  capture_releases=450 hw_iterations=15 hw_samples=1500 hw_releases=1500`.
  Unlike the GPU-texture-cache bug this test's own design mirrors, no
  device-lifetime bug was found here -- a real, positive closure result,
  not just an absence of testing.

**Packaged/isolated-stage closure.** Re-verifying the *ordinary* cumulative
`crt-gfx-media-dist` after Tranche 5 surfaced two more real packaging gaps,
both now fixed and re-verified green:

- `tools/verify_dist.py` failed the packaged `libcrtmedia.dll` outright
  ("unresolved packaged binary dependencies: ... undeclared MFPlat.DLL/
  MFReadWrite.dll/MF.dll") -- `tools/crt_dist_prerequisites.py` had no entry
  declaring these as expected external Windows runtime dependencies (unlike
  `ole32.dll`'s own pre-existing `windows-com-runtime` entry). Fixed by
  adding a `windows-media-foundation-runtime` entry alongside it.
  `distribution/stages/04-gfx-media/CMakeLists.txt`'s Windows branch also
  needed the same `capture_mf.c` source and MF/uuid import-library wiring
  `libcrtmedia/CMakeLists.txt` already carries -- its per-file compile-
  options wiring was moved into the shared `crt_add_crtmedia_targets()`
  (`libcrtmedia/cmake/crtmedia_targets.cmake`) rather than duplicated per
  caller, mirroring the existing Linux `capture_v4l2.c` precedent in that
  same function.
- `tools/create_stage_source.py`'s file registry turned out to be stale
  since *before the whole "Encode and capture" roadmap began*: every
  Tranche 1/3/4A/4B/5/6 test file, `capture_mf.c` itself, macOS's
  `capture_avfoundation.c` itself, and its own private `capture_
  avfoundation_test_control.h` header were all silently absent from every
  isolated stage source asset on all three hosts, not just Windows.
  `tools/test_stage_source_closure.py` (which checks exactly this) now
  passes again after adding all of them.

Real, full, from-scratch isolated `04-gfx-media` stage rebuild
(`tools/crt-stage-build.py`, real FreeType/FFmpeg/Skia -- `--enable-
mediafoundation --enable-encoder=h264_mf` configured and built cleanly
through the packaged `tools/crt-cc` too, confirming the Tranche 5 linker
fix is portable, not an in-tree-only artifact): all phases passed, 3165.5s
total (`build and install FreeType/FFmpeg` alone: 2817.6s). The isolated
stage's own smaller ctest subset (10 tests, no real-hardware capture/encode
coverage by design -- that bar is carried by the two host-generic tests
above, run in-tree) passed 10/10, all four packaged examples (`gfx-gpu`,
`gfx-skia`, `media-player`, and the zero-copy `crtgfx_skia_media_window_
demo` bridge -- the one that actually links `capture_mf.c.obj` in) rebuilt
and ran successfully against the freshly packaged SDK, `verify_dist.py`
passed, and the SDK published atomically. Full in-tree `ctest`: 155/155.

**macOS/arm64 replay, done 2026-09-28.** `crtmedia_timing_discontinuity_
test` needed no changes at all and reproduced the identical result:
`crtmedia_timing_discontinuity_test: ok frames=20 gap1_us=66666
gap2_us=500000`.

`crtmedia_capture_encode_lifecycle_test` surfaced one real gap in the
shared test itself, not in this host's own capture/encode backends: its
`run_capture_cycle()` classified `device_count==0`/a failed `open()` as an
honest skip, but had no equivalent classification for a device that opens
successfully and then never yields even its first real frame -- exactly
what happens on this host when the compiled test binary is invoked as a
plain, un-launched process, per Tranche 4A's own already-documented finding
(`authorizationStatus`/`requestAccessForMediaType:` are unreliable outside a
LaunchServices-launched, signed bundle). Reproduced directly: a fresh,
ad-hoc-signed throwaway `.app` bundle around the same, unmodified test
binary, launched with `open -W`, still failed the same way -- a brand-new
bundle identifier gets a real, interactive TCC camera-permission prompt the
first time, and nothing in this environment can click "Allow" -- so this is
a genuine environment property, not a stale-permission artifact. Fixed in
`tests/capture_encode_lifecycle_test.c` itself (host-generic, so Linux's own
future replay benefits too): a failure on the *first* dequeue of a cycle is
now classified the same honest way as a missing device; a failure *after*
at least one real frame was already captured is unchanged, still a genuine
regression. Real result after the fix: `crtmedia_capture_encode_lifecycle_
test: ok capture_iterations=0 capture_frames=0 capture_releases=0
capture_decoded=0 hw_iterations=15 hw_samples=1500 hw_releases=1500
hw_decoded=1500` -- all 15 VideoToolbox hardware-encode create/queue/drain/
EOS/destroy cycles completed cleanly with no resource-lifetime bug found
(a real, positive closure result for that half, matching the Windows
lifecycle test's own outcome), while the capture half reports an honest,
precise skip rather than a false failure in this non-interactive
environment. A real capture-half pass on this host is possible in principle
(the exact LaunchServices-launch technique Tranche 4A itself used, once a
human has interactively approved that one specific bundle's camera access
at least once) but is out of scope for an automated verification pass.

One more real `create_stage_source.py` registry gap surfaced while checking
whether this replay's own new code would carry over to an isolated stage
build: `porting/shims/macos/ffmpeg_videotoolboxenc_pthread_dlfcn_compat.h`
(Tranche 4B's own compatibility shim, referenced only from a string inside
`porting/recipes/ffmpeg.json`'s own Makefile patch, never from a bundled
C source's `#include`) was missing from the macOS `project_paths_by_os`
list -- the same class of gap `tools/test_stage_source_closure.py` cannot
catch (it only scans bundled sources' own `#include` directives) that the
Windows Tranche 6 pass already found for `capture_mf.c`. Fixed by adding it.

**macOS/arm64 isolated-stage closure, done 2026-09-28.** Running the actual
isolated `04-gfx-media` stage build on macOS -- the first time since Tranche
4A landed -- immediately surfaced two more real gaps, both predating this
whole tranche and both invisible to any in-tree build or to `tools/
test_stage_source_closure.py`:

- `distribution/stages/04-gfx-media/CMakeLists.txt`'s own macOS branch set
  `CRTMEDIA_BACKEND_SOURCES` to only `audio_sink_coreaudio.c` --
  `capture_avfoundation.c` itself (Tranche 4A) was never added, so the
  isolated stage's own `libcrtmedia.dylib` failed to link with real
  undefined `_crtmedia_capture_backend_*` symbols the moment its own
  `capture.c` was compiled in. The exact same class of gap `capture_mf.c`
  had on Windows (this section's own Windows paragraph above), just
  undiscovered until now since this was the first real isolated macOS
  stage build attempted since Tranche 4A.
- `capture_avfoundation.c`'s own `-Wno-cast-function-type-mismatch`
  compile-flag override (needed for its `objc_msgSend` casts) lived only
  in `libcrtmedia/CMakeLists.txt`'s own `set_source_files_properties()`
  call, never reached by the isolated stage's own separate CMakeLists.txt.
  Fixed by moving it into the shared `crt_add_crtmedia_targets()`
  (`libcrtmedia/cmake/crtmedia_targets.cmake`), mirroring how `capture_
  mf.c`'s/`capture_v4l2.c`'s own per-file compile-flag overrides already
  live there instead of being duplicated per caller -- both callers now
  get it automatically, and the duplicate in `libcrtmedia/CMakeLists.txt`
  was removed.
- Once the library actually linked, `verify_dist.py` immediately reported
  a third real gap: `tools/crt_dist_prerequisites.py`'s own
  `macos-media-runtime` entry listed AudioToolbox/VideoToolbox/CoreVideo/
  CoreMedia but not `AVFoundation.framework` -- a real, confirmed
  `LC_LOAD_DYLIB` dependency of `libcrtmedia.dylib` since Tranche 4A that
  had simply never been exercised by `verify_dist.py` before, since no
  isolated stage had ever actually linked `capture_avfoundation.c.o` until
  the two fixes above. Fixed by adding it (`Foundation.framework`/
  `CoreFoundation.framework`/`libobjc.A.dylib` were already covered by the
  existing `macos-cocoa-window-runtime` entry, inherited cumulatively).

With all three fixed, a real, full, from-scratch isolated `04-gfx-media`
stage rebuild on macOS (`tools/crt-stage-build.py`, real FreeType/FFmpeg/
Skia) passed end to end: 497.7s total (FreeType/FFmpeg 374.0s, Skia 60.9s,
everything else under 10s each). The isolated stage's own smaller ctest
subset passed 10/10, all four packaged examples (`gfx-gpu`, `gfx-skia`,
`media-player`, and the zero-copy `crtgfx_skia_media_window_demo` bridge --
the one that actually links `capture_avfoundation.c.o` in, real result:
`RESULT backend=metal interop=zero-copy gpu_frame=yes texture_backed=yes
... frames_presented=20 ... pixel_check=pass`) rebuilt and ran successfully
against the freshly packaged SDK, `verify_dist.py` passed, and the SDK
published atomically. Full in-tree `ctest`, re-confirmed after these
CMake/prerequisite changes: 145/145.

**macOS/arm64 lifecycle follow-up, resolved 2026-10-01.** A later run with
camera access available exposed two defects that the earlier honest
first-frame skip could not exercise. The AVFoundation queue handed a frame's
storage to the caller but left the same pointer in its ring slot; backend
teardown then attempted to free all slots, including storage already released
by the caller. Queue take/drop/clear now encode the ownership transfer by
clearing moved slots, and teardown detaches the sample-buffer delegate, drains
the dispatch queue, clears only still-owned frames, then releases the native
objects. The resource-free AVFoundation conversion test now also exercises
this queue ownership rule.

The second defect was negotiation: the backend built session-preset and
CoreVideo dictionary keys from ad-hoc strings, reported 640x480 through
`out_actual`, but received 1920x1080 buffers from the real camera. The software
encoder correctly rejected that mismatch. The PAL now references the real
`AVCaptureSessionPreset*`, `kCVPixelBufferPixelFormatTypeKey`,
`kCVPixelBufferWidthKey` and `kCVPixelBufferHeightKey` framework symbols and
requests the selected dimensions explicitly. This keeps the public
`out_actual` contract honest without exposing host headers or changing the
cross-host API.

The unchanged lifecycle workload now reports
`capture_iterations=15 capture_frames=450 capture_releases=450
capture_decoded=450 hw_iterations=15 hw_samples=1500 hw_releases=1500
hw_decoded=1500`. Full macOS CTest is 160/160.

**Linux/x86_64 replay, done 2026-09-28.** `crtmedia_timing_discontinuity_
test` needed no changes and reproduced the identical result: `crtmedia_
timing_discontinuity_test: ok frames=20 gap1_us=66666 gap2_us=500000`.

`crtmedia_capture_encode_lifecycle_test` needed no code changes on this
host either. Its first run happened while the USB UVC webcam this session
had been using (Tranche 2's own acceptance camera) was not physically
connected -- a real, honest `capture_iterations=0` result via the exact
same first-dequeue skip classification the macOS replay's own fix already
added, with the VA-API hardware-encode half still completing all 15
iterations cleanly (`hw_iterations=15 hw_samples=1500 hw_releases=1500
hw_decoded=1500`). Reconnecting the camera and rerunning the identical,
unmodified binary produced a full real-hardware pass on both halves: `ok
capture_iterations=15 capture_frames=450 capture_releases=450
capture_decoded=450 hw_iterations=15 hw_samples=1500 hw_releases=1500
hw_decoded=1500` -- no resource-lifetime bug found in either the V4L2
capture path or the VA-API encode path across 15 create/destroy cycles
each. Full in-tree `ctest`: 138/139 (the one failure is the same pre-
existing, environment-caused `crtmedia_playback_pipeline_test_runs` gap
this host has had since before this tranche began -- no sound card at all,
confirmed via `aplay -l`, unrelated to capture/encode).

**Linux/x86_64 isolated-stage closure, done 2026-09-28.** Running the
actual isolated `04-gfx-media` stage build on Linux -- the first time
since `capture_v4l2.c` gained its MJPEG branch and VA-API H.264 encode
landed (this tranche's own Tranche 2/3 sections above) -- immediately
surfaced one real, previously-undiscovered gap, invisible to any in-tree
build:

- `tools/crt-cc`'s own `-fcrt-real-linux-sdk` sentinel (added for VA-API's
  `<va/va.h>` in the "Hardware video decode"/"Zero-copy decoded textures"
  tranches) exposes only plain `/usr/include` as a lowest-priority
  `-isystem` fallback -- correct for `va/va.h` itself (confirmed installed
  directly there), but `capture_v4l2.c`'s own `<linux/videodev2.h>` pulls
  in `<linux/ioctl.h>`, which itself does `#include <asm/ioctl.h>`. This
  exact real Debian/Ubuntu host has no plain `/usr/include/asm` symlink at
  all -- only the real multiarch location, `/usr/include/x86_64-linux-gnu/
  asm/ioctl.h` (confirmed directly) -- so the isolated stage's own
  `crt-cc`-driven compile failed outright: `fatal error: 'asm/ioctl.h'
  file not found`. This is the identical class of gap `distribution/
  stages/04-gfx-media/CMakeLists.txt`'s own `CRTGFX_LINUX_VULKAN_LIB`/
  `CRTMEDIA_LINUX_VAAPI_LIB` `find_library()` calls already document and
  fall back from (this isolated stage's own `crt-toolchain.cmake` skips
  the host ABI probe that would otherwise populate this automatically) --
  just never hit before now because no isolated stage had ever actually
  compiled a kernel-UAPI-including source (as opposed to a userspace
  library header like `va/va.h`) through this sentinel until `capture_
  v4l2.c` did. Fixed in `tools/crt-cc` itself: when `-fcrt-real-linux-sdk`
  is requested, it now also probes `dpkg-architecture -qDEB_HOST_MULTIARCH`
  (the identical fallback mechanism, reused rather than duplicated) and
  adds `/usr/include/<triplet>` as a second, still-lower-priority `-isystem`
  fallback when it exists -- best-effort, so a non-Debian host or one with
  a plain `/usr/include/asm` symlink already (common on Fedora/RHEL) is
  unaffected.

With that one fix, a real, full, from-scratch isolated `04-gfx-media` stage
rebuild on Linux (`tools/crt-stage-build.py`, real FreeType/FFmpeg/Skia --
FFmpeg configured and built with both the MJPEG decoder and the `h264_vaapi`
encoder through the packaged `tools/crt-cc` too, confirming both Tranche 2's
and Tranche 3's own fixes are portable, not in-tree-only artifacts) passed
end to end: 876.5s total (FreeType/FFmpeg 699.0s, Skia 154.7s, everything
else under 10s each). The isolated stage's own smaller ctest subset passed
12/12 (including `crtmedia_capture_v4l2_test`, real hardware: `ok frames=30
device=/dev/video0 size=640x480 fps=30`, and `crtmedia_zero_copy_test`),
all packaged examples (`gfx-gpu`, `gfx-skia`, `media-player`, and the
zero-copy `crtgfx_skia_media_window_demo` bridge -- real result: `RESULT
backend=vulkan interop=zero-copy gpu_frame=yes texture_backed=yes
cpu_readback=no frames_presented=20 ... pixel_check=pass
post_resize_present=pass clean_exit=pass`) rebuilt and ran successfully
against the freshly packaged SDK, `verify_dist.py` passed, and the SDK
published atomically. Full in-tree `ctest`, re-confirmed after the
`tools/crt-cc` fix: 138/139 (the same pre-existing no-sound-card gap noted
above).

## Non-goals for the current tranche

- network transport, adaptive streaming, or WebRTC;
- audio capture/encode before the video timing path is stable;
- claiming zero-copy merely because capture and encode are both hardware
  accelerated;
- importing host SDK types into installed CRT headers;
- patching upstream media source to bypass a missing CRT/PAL surface.
