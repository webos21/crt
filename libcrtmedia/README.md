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
