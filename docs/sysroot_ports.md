# Sysroot Configure/Make Porting

This document records the first Android-NDK-style source porting environment for
the CRT project.

## Goal

External libraries should be buildable from their original source trees with a
normal `./configure && make && make install` flow while using:

- CRT public headers from `out/<preset>/sysroot/include`;
- CRT startup object from `out/<preset>/sysroot/lib/crt1.o`;
- CRT static archives from `out/<preset>/sysroot/lib`;
- compiler runtime builtins installed into the CRT sysroot;
- Clang resource headers only for compiler intrinsic headers such as
  `arm_neon.h`, not for hosted libc headers.

This is closer to the Android NDK model than the earlier `ports/` CMake smoke
targets. The CMake smoke targets remain useful for fast, curated tests, but this
path is the realistic configure-time compatibility signal.

## Tools

The first wrapper tools are:

```text
tools/crt-cc
tools/crt-c++
```

`crt-cc` and `crt-c++` require `CRT_SYSROOT` and accept `CRT_TARGET_OS`.
They compile with:

```text
-ffreestanding
-fno-builtin
-Xclang -fwchar-type=int
-nostdinc
-isystem $CRT_SYSROOT/include
-isystem $(clang -print-resource-dir)/include
```

Final links are made with:

```text
-nostdlib
-nostartfiles
-nodefaultlibs
$CRT_SYSROOT/lib/crt1.o
$CRT_SYSROOT/lib/libc.a
$CRT_SYSROOT/lib/libm.a
$CRT_SYSROOT/lib/libdl.a
$CRT_SYSROOT/lib/libc++.a
$CRT_SYSROOT/lib/libclang_rt.builtins.a
```

On macOS, `-lSystem` remains the host OS boundary. Windows similarly uses the
required SDK import libraries through `CRT_WINDOWS_SYSTEM_LIBS`.

## Running

The user-facing workflow assumes that upstream archives are downloaded and
extracted manually. First install the CRT sysroot:

```sh
cmake --build --preset macos-host-ninja-debug --target sysroot
mkdir -p out/macos-host-ninja-debug/port-tests/src
mkdir -p out/macos-host-ninja-debug/port-tests/install
```

Then load the wrapper environment from the repository root. On Linux/macOS, or
from Git Bash/MSYS on Windows:

```sh
. tools/crt-env.sh macos-host-ninja-debug
```

On Windows, use the command-file environment helper to avoid PowerShell
execution-policy restrictions:

```bat
call tools\crt-env.cmd windows-host-ninja-debug
```

From PowerShell, open a configured `cmd.exe` session instead:

```powershell
cmd /k tools\crt-env.cmd windows-host-ninja-debug
```

`tools\crt-env.ps1` is also available if your PowerShell execution policy allows
local scripts.

The env files set:

- `CRT_SYSROOT=out/<preset>/sysroot`;
- `CRT_TARGET_OS`;
- `CC=tools/crt-cc`;
- `CXX=tools/crt-c++`;
- `PORT_PREFIX=out/<preset>/port-tests/install`;
- `CPPFLAGS`, `LDFLAGS`, and `PKG_CONFIG_*` for libraries already installed
  into `PORT_PREFIX`.

Build and install zlib from the extracted upstream source directory:

```sh
cd /path/to/zlib-1.3.1
./configure --static --prefix="$PORT_PREFIX"
make -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 2)"
make install
```

Build libpng from the extracted upstream source directory after zlib has been
installed into `PORT_PREFIX`:

```sh
cd /path/to/libpng-1.6.57
./configure --disable-shared --enable-static --prefix="$PORT_PREFIX"
make -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 2)"
make install
```

The installed port prefix is:

```text
out/macos-host-ninja-debug/port-tests/install/
  include/
  lib/
  lib/pkgconfig/
  bin/
```

`tools/crt-port-build.py` keeps the same flow automated for project regression
checks and agent-side bring-up, but it is not the primary documented user
workflow.

## Current Result

On macOS host, the following original configure/make flows pass with the strict
CRT sysroot wrapper:

| Port | Source flow | Result |
| --- | --- | --- |
| zlib 1.3.1 | `./configure --static && make && make install` | pass |
| libpng 1.6.57 | `./configure --disable-shared --enable-static && make && make install` | pass |

The libpng run required adding real CRT surface for:

- `vfprintf`;
- `sprintf`;
- `vsprintf`;
- `inttypes.h`;
- `strings.h`;
- `strtoimax`;
- `strtoumax`;
- `bzero`;
- `bcmp`.

## Notes

- This is still host-build only. Cross-build remains intentionally deferred.
- The first wrappers prefer static CRT archives even when shared CRT artifacts
  exist, because Autoconf test executables must run immediately during
  configure.
- The port prefix is separate from the CRT sysroot. Third-party dependency
  headers and archives go under `out/<preset>/port-tests/install`, while CRT
  headers and runtime objects stay under `out/<preset>/sysroot`.
- `libtool` may print harmless macOS probing warnings around command-line length
  and versioned symbols. These are recorded as configure-tool noise unless they
  block the build.
