# CRT Distribution Examples

These examples are installed into the cumulative CRT SDK distributions. Each
example directory contains source that can be rebuilt with the packaged
`crt-toolchain.cmake`; `bin/` contains the matching ready-to-run host binary.

Activate the distribution before running a binary so its `bin/` directory and
external compiler contract are available:

```sh
. ./activate.sh
./examples/bin/crtgfx_window_demo 120
```

On Windows, use:

```bat
call activate.cmd
examples\bin\crtgfx_window_demo.exe 120
```

On Windows, `activate.cmd` also puts the SDK's `bin\` directory on `PATH`. A
program you rebuild imports the runtime DLLs there (for example
`libcrtgfx.dll`), and Windows has no RPATH, so without activation it fails to
start with `STATUS_DLL_NOT_FOUND` (exit code `0xC0000135`). The ready-made
programs in `examples\bin` link the CRT statically and run either way. On Linux
and macOS a rebuilt program finds the SDK's shared libraries through the RPATH
that `crt-toolchain.cmake` sets.

To rebuild an example outside the SDK directory:

```sh
cmake -S examples/gfx-simple -B <work>/gfx-simple-build \
  -DCMAKE_TOOLCHAIN_FILE=<absolute-sdk-path>/crt-toolchain.cmake
cmake --build <work>/gfx-simple-build
```

`gfx-simple` first appears in `03-gfx-simple`. `gfx-gpu` first appears in
`04-gfx-media`. `gfx-skia` is present only when that stage was configured with
Skia enabled. The finite frame-count argument is intended for bounded smoke
runs; omit it (or pass zero) to keep a demo open until its window is closed.

`media-player` also first appears in `04-gfx-media`, and is present only when
that stage was configured with `CRTMEDIA_ENABLE_FFMPEG` enabled -- like
`gfx-skia`, `crtmedia_extractor`/`crtmedia_codec` are compiled out of
`libcrtmedia` entirely otherwise. It takes an optional path to a media file
to play (video + audio, looping until the window is closed); with no
argument it plays the bundled `test_video.mp4` fixture installed alongside
`main.c`:

```sh
./examples/bin/crtmedia_player_demo examples/media-player/test_video.mp4
```

