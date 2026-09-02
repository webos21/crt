# libcrtmedia

Media upper runtime layer.

FFmpeg is the first planned reference stack. The initial scope is software
decode into explicit frame/audio-buffer objects; GPU texture and host audio
device handoff should wait until `libcrtgfx` has a stable surface/frame
abstraction.

The CPU video frame handoff contract is defined:
`include/crtmedia/frame.h`'s `crtmedia_frame` (packed RGBA8888/BGRA8888
and planar YUV420P, per-plane stride/dimensions, color range/space,
timestamp, release-callback ownership), `crtmedia_frame_describe_planes()`,
and `crtmedia_frame_convert_to_rgba()`. `tests/frame_test.c` covers it
deterministically; `tests/frame_skia_smoke.cc` (built and registered from
`libcrtgfx/CMakeLists.txt`, not this directory's own -- see that file's
own comment) hands a synthetic frame in each format to a real Skia
`SkImage`/`SkSurface`. See `HISTORY.md`'s 2026-08-31 entry for the full
design trail.

A real FFmpeg-backed demux/software-decode bridge exists behind the
`CRTMEDIA_ENABLE_FFMPEG` CMake option (default OFF, matching
`CRTGFX_ENABLE_SKIA`'s own opt-in shape): `include/crtmedia/demux.h` +
`src/demux.c` open a local file (`file` protocol only, no network yet),
demux one container (MOV/MP4/M4A), and decode H.264 video into
`crtmedia_frame` / AAC+MP3+PCM audio into the new
`include/crtmedia/audio.h`'s `crtmedia_audio_buffer` -- no FFmpeg type is
ever named in either public header. Built against `porting/recipes/
ffmpeg.json` (LGPL-only, `--disable-x86asm`/`--disable-inline-asm`, see
that recipe's own notes). `tests/demux_decode_test.c` verifies it
end-to-end against a real, tiny, project-authored WAV fixture
(`assets/test_tone.wav`). Verified end-to-end on all three hosts: Linux
(WSL), macOS, and native Windows -- see `porting/recipes/ffmpeg.json`'s
own `status`/`notes` for the full per-host fix trail and `HISTORY.md`'s
2026-09-01 entries for the dated narrative.

Real H.264+AAC (MP4) and MP3 fixture coverage exists too:
`tests/demux_decode_video_test.c` (threaded H.264 decode, PTS ordering,
EOF drain/flush) and `tests/demux_decode_mp3_test.c` (the `mp3`/`mp3float`
decode path specifically), plus `tests/demux_decode_malformed_test.c`
(null args, a nonexistent path, non-media bytes, a truncated real
fixture -- no crash, a real defined error/EOF outcome every time). See
`assets/README.md` for the new fixtures' own provenance and `HISTORY.md`'s
2026-09-02 entry for the full trail. Verified on all three hosts, Linux,
Windows, and macOS (macOS re-verified the same day, `crtmedia_demux_video_
test` doubling as the first real confirmation that FFmpeg's own threaded
H.264 decode works through this project's pthread PAL on macOS too).

The public media API policy -- an `AMediaFormat`/`AMediaExtractor`/
`AMediaCodec`-shaped core layered under the existing, retained
`crtmedia_demuxer_*` convenience API, FFmpeg never in a public header --
is decided in `docs/libcrtmedia_api_policy.md`. The core itself is now
implemented and verified on all three hosts: `include/crtmedia/format.h`
(`crtmedia_format`, a real key-value store including `csd-0` codec-config
buffers for H.264/AAC), `include/crtmedia/extractor.h` (`crtmedia_
extractor`, demux-only, no decode), and `include/crtmedia/codec.h`
(`crtmedia_codec`, the real async buffer-queue decoder --
`queue_input`/`dequeue_output`/`flush`, `CRTMEDIA_WOULD_BLOCK`
backpressure). `tests/format_test.c` covers the key-value store
deterministically; `tests/extractor_codec_test.c` decodes the same real
MP4 fixture `demux_decode_video_test.c` covers, end to end through the
new core, and gets the identical real result.

`crtmedia_demuxer_*` (`demux.h`) is now rebuilt over this new core too --
`src/demux.c` composes one `crtmedia_extractor` plus one `crtmedia_codec`
per decodable track instead of its own independent FFmpeg integration
(no `AVFormatContext`/`AVCodecContext` in the file at all anymore), with
a real `CRTMEDIA_WOULD_BLOCK` backpressure path that never silently drops
an unqueued sample. Every existing `crtmedia_demux_*_test` passes
completely unchanged after the rebuild -- verified on Linux, Windows,
and macOS (macOS re-verified the same day). See `HISTORY.md`'s
2026-09-02 entry for the full trail.

The software player (`TODO.md`'s upper-runtime roadmap) is under way:
`include/crtmedia/player.h`/`src/player.c` is a host-independent master
clock/play-pause-seek-stop state machine/A/V-sync core (real
`CLOCK_MONOTONIC`-anchored clock, audio as the reference clock once a
real audio track exists, `WAIT`/`PRESENT_NOW`/`DROP` per-frame timing
decisions) -- `tests/player_test.c` covers it against the real host clock.
`include/crtmedia/audio_sink.h` is the host-independent audio-output
contract (`_open`/`_close`/`_write`/`_get_position_frames`, no host audio
API type ever named); its Windows backend,
`src/arch/windows/audio_sink_wasapi.c`, drives real WASAPI (`IMMDevice
Enumerator`/`IAudioClient`/`IAudioRenderClient`) directly, without
`#include`-ing any Windows SDK header -- matching `libcrtgfx/src/arch/
windows/window_win32.c`'s own hand-rolled-COM-vtable convention exactly,
built as its own `crtmedia_backend_objects` OBJECT library mirroring
`libcrtgfx`'s own `crtgfx_backend_objects`. Its Linux backend,
`src/arch/linux/audio_sink_linux.c`, tries a raw ALSA kernel PCM ioctl
first (`/dev/snd/pcmC*D*p`, the same real UAPI Android's own NDK audio
stack ultimately rests on via `tinyalsa` at its lowest HAL layer), then
falls back to a hand-rolled real PulseAudio native-protocol client spoken
directly over the real `$PULSE_SERVER` Unix socket (its own wire format
has no public header at all -- confirmed byte-for-byte via a real
`strace` capture against WSLg's own bundled `libpulse.so`, not guessed).
Neither backend ever links a host client library (no `libasound`/
`libpulse`), matching this project's own established no-host-client-
library policy. `tests/audio_sink_test.c` verified for real: against the
real default Windows audio device (opens it, writes real silence,
confirms playback position never moves backwards, drains on close), and
on real WSL against WSLg's own live PulseAudio bridge (the full real
AUTH/stream-creation/data-write/position-query/drain exchange). A real
`getegid()` libc gap (hardcoded to 0 on every platform) was found and
fixed on Linux along the way -- the Pulse backend's own required
`SCM_CREDENTIALS` handshake needs a real gid. The macOS (CoreAudio)
backend, buffering/frame-drop policy wiring, and the full demux+decode+
sink+clock render-loop pipeline are not yet built. See `HISTORY.md`'s
2026-09-02 entries for the full trail.
