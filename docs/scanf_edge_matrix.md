# scanf Edge-Case Matrix

This document defines the comparison loop for reducing differences between this
CRT's scanner and Android Bionic's `vfscanf`/`vfwscanf` behavior.

## Goal

`tests/scanf_edge_matrix_test.c` is intentionally written as a standalone C
program. It is built by CTest against this CRT, and the same file can also be
built on an Android/Bionic environment to verify the expected behavior against
the real Bionic runtime.

The matrix is not a replacement for importing Bionic's scanner source directly.
It is the guardrail used while this project still adapts Bionic/BSD scanner
logic to the local C99 `scan_source` backend.

## Current Coverage

The active matrix covers:

- input failure versus matching failure;
- assignment count after partial conversions;
- `%c` width with early EOF after at least one byte;
- `%i` with `0x` and `0b` prefixes that have no following digits;
- pushback behavior after incomplete binary prefixes;
- Bionic/BSD scanset edge cases, including leading `]`, leading `-`, negation,
  ranges, and chained ranges such as `[a-c-e]`;
- Bionic `%b`;
- Bionic `%w` integer width modifiers;
- BSD compatibility conversions `%D`, `%O`, `%U`;
- deprecated `%q` length modifier;
- `%p` pointer destination assignment;
- gdtoa-backed C99 hexadecimal floating input;
- NaN payload consumption.

## Local CRT Run

```sh
cmake --build --preset macos-host-ninja-debug --target scanf_edge_matrix_test
ctest --preset macos-host-ninja-debug -R scanf_edge_matrix_test --output-on-failure
```

Use the matching preset on Linux or Windows.

## Android/Bionic Reference Run

Build the same source with an Android NDK compiler or inside an Android build
environment:

```sh
$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/<host-tag>/bin/aarch64-linux-android35-clang \
  tests/scanf_edge_matrix_test.c \
  -o /tmp/scanf_edge_matrix_test
```

Then run it on an Android device or emulator:

```sh
adb push /tmp/scanf_edge_matrix_test /data/local/tmp/
adb shell /data/local/tmp/scanf_edge_matrix_test
```

The expected success line is:

```text
scanf_edge_matrix_test: ok
```

When a case differs, the local CRT should be changed only after checking the
corresponding Bionic source snapshot under `third_party/bionic/stdio/`.

## Deferred Cases

Still to add:

- native wide scanner edge cases from Bionic `vfwscanf.cpp`;
- stream error flag behavior after illegal multibyte sequences;
- exact `errno` behavior for integer overflow;
- locale/xlocale decimal point behavior;
- allocation modifier failure paths;
- positional and malformed format behavior if Bionic exposes stable semantics
  worth matching.
