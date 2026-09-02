# libcrtmedia test assets

`test_tone.wav` -- a tiny (~4 KB), mono, 8 kHz, 16-bit PCM WAV file: a
0.25-second 440 Hz sine tone. Generated with Python's standard-library
`wave` module (no external tool, no third-party binary), not recorded or
copied from anywhere -- entirely project-authored, public domain. Used by
`tests/demux_decode_test.c` to exercise the real FFmpeg-backed WAV demux +
`pcm_s16le` decode path (`crtmedia/demux.h`) deterministically, with no
real audio device or file larger than needed for that.

`test_video.mp4` -- a tiny (~19 KB), 1-second, 64x64, 25 fps synthetic test
pattern (`lavfi`'s own `testsrc`), H.264 (Constrained Baseline profile,
`yuv420p`, no B-frames, so decode order == PTS order) muxed with a
1-second 440 Hz sine tone (AAC, 44.1 kHz mono) in an MP4 container. Used by
`tests/demux_decode_video_test.c` to exercise the H.264 video decode +
AAC audio decode + MOV/MP4 demux path together, including EOF drain and
(via `src/demux.c`'s own explicit `thread_count`) real multi-threaded
decode.

`test_tone.mp3` -- a tiny (~10 KB), mono, 8 kHz, 1-second 440 Hz sine tone
encoded as MP3 (`libmp3lame`). Used by `tests/demux_decode_mp3_test.c` to
exercise the MP3 demux + `mp3`/`mp3float` decode path specifically (a
different decoder than the WAV fixture's `pcm_s16le`).

Both new files are synthetic (`lavfi` test-pattern/sine-wave sources, no
recorded or copied real-world content, entirely project-generated) --
encoded with a real `ffmpeg` binary (not part of this project's own
toolchain or build; a local, dev-time-only generation tool, the same
relationship a font/image test asset generated with any external editor
would have to the project that later ships it) rather than Python's stdlib
`wave` module, since neither H.264 nor MP3 encoding has a pure-stdlib
equivalent the way raw PCM WAV does. No archive/URL/hash provenance to
record, matching `test_tone.wav`'s own precedent -- these files are not
downloaded third-party content.

See `libcrtgfx/assets/fonts/README.md` for the matching pattern this
follows.
