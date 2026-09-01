# libcrtmedia test assets

`test_tone.wav` -- a tiny (~4 KB), mono, 8 kHz, 16-bit PCM WAV file: a
0.25-second 440 Hz sine tone. Generated with Python's standard-library
`wave` module (no external tool, no third-party binary), not recorded or
copied from anywhere -- entirely project-authored, public domain. Used by
`tests/demux_decode_test.c` to exercise the real FFmpeg-backed WAV demux +
`pcm_s16le` decode path (`crtmedia/demux.h`) deterministically, with no
real audio device or file larger than needed for that.

See `libcrtgfx/assets/fonts/README.md` for the matching pattern this
follows.
