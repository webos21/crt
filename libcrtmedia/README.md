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
