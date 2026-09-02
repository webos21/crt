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
2026-09-02 entry for the full trail. Verified on Linux and Windows; macOS
not yet re-verified for this specific coverage.

The public media API policy -- an `AMediaFormat`/`AMediaExtractor`/
`AMediaCodec`-shaped core layered under the existing, retained
`crtmedia_demuxer_*` convenience API, FFmpeg never in a public header --
is decided in `docs/libcrtmedia_api_policy.md`. The core itself is now
implemented and verified on Linux and Windows: `include/crtmedia/format.h`
(`crtmedia_format`, a real key-value store including `csd-0` codec-config
buffers for H.264/AAC), `include/crtmedia/extractor.h` (`crtmedia_
extractor`, demux-only, no decode), and `include/crtmedia/codec.h`
(`crtmedia_codec`, the real async buffer-queue decoder --
`queue_input`/`dequeue_output`/`flush`, `CRTMEDIA_WOULD_BLOCK`
backpressure). `tests/format_test.c` covers the key-value store
deterministically; `tests/extractor_codec_test.c` decodes the same real
MP4 fixture `demux_decode_video_test.c` covers, end to end through the
new core, and gets the identical real result. `crtmedia_demuxer_*`
(`demux.h`) is not yet rebuilt over this new core -- still its own,
separate, independent implementation for now, deliberately deferred (see
`TODO.md`'s own next step). See `HISTORY.md`'s 2026-09-02 entry for the
full trail.
