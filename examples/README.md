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

