# CRT

CRT is an experimental Bionic-compatible OS Abstraction Runtime / PAL.

The project aims to provide a low-level runtime surface, based on Android Bionic
libc, that makes Linux, BSD, and Android native source code easier to rebuild
across Linux, Windows, macOS, and Android.

The first deliverable looks like a C runtime library, but the broader goal is a
portable low-level foundation for native libraries and applications.

## Goal

CRT exposes a Bionic-compatible libc/API/ABI surface while hiding host operating
system differences behind explicit platform adaptation layers.

The central idea is:

> If Linux/BSD/Android-style low-level runtime facilities are made portable at
> the libc/PAL layer, upper layers can focus on graphics, application lifecycle,
> event integration, packaging, and host UX instead of repeatedly solving files,
> sockets, threads, TLS, memory mapping, dynamic loading, errno, signals, clocks,
> and process basics for each OS.

This project focuses on rebuild-based source portability. It is not a container,
VM, subsystem, or full foreign userspace emulator.

## Non-Goals

CRT is not trying to provide:

- Docker/LXC-style Linux execution.
- A Windows Subsystem for Linux style environment.
- Unmodified execution of arbitrary Linux binaries.
- Unmodified execution of Android APKs.
- A complete Android framework, HAL, Binder service, package manager, or ART
  environment.
- Full glibc binary ABI compatibility for existing Linux binaries.
- Direct ports of specific GUI toolkits such as Qt, GTK, or Enlightenment.

Large projects such as Qt, GTK, Enlightenment, Chromium, or Chrome are useful as
examples and long-term compatibility benchmarks, not as immediate porting
targets.

## Scope

The intended runtime scope includes:

- `libc`
- `libm`
- `libdl`
- `libstdc++` / C++ ABI support
- dynamic linker support, later
- platform adaptation layers for Linux, Windows, macOS, and Android
- architecture support for x86_64 and aarch64 only

## Stack

The current baseline decisions are:

- Base runtime: Android Bionic libc
- Language: C99 + architecture-specific assembly
- Primary compiler: LLVM Clang
- Primary linker: LLD
- Compiler runtime: compiler-rt
- C++ runtime direction: libunwind and libc++
- Build system: CMake
- Default generator: Ninja
- Test integration: CTest

The project ABI intentionally follows the Linux/Bionic-style runtime surface
rather than each host OS ABI where those differ. In particular, all CRT targets
are built with `-Xclang -fwchar-type=int` so `wchar_t` and wide string literals
are signed 32-bit on Windows as well as on Linux/macOS. External libraries built
against the CRT sysroot must use the same flag.

Rust may be used later for tooling or optional internal modules behind a stable C
ABI, but the core runtime must remain buildable without requiring Rust.

## Current Status

The project has moved beyond the initial hello-world bring-up. The current
repository builds a Bionic-compatible CRT/PAL baseline with:

- `libc`, `libm`, `libdl`, and a small C++ ABI bootstrap, each available as
  static and host-native shared artifacts.
- Bionic/Linux-style public headers and type policy through the CRT sysroot.
- Linux, macOS, and Windows startup, syscall/PAL, fd, process, socket, signal,
  pthread, TLS, mmap, stdio, locale, wchar, math, dynamic-loading, and rootfs
  support at the level needed by the current tests and ports.
- Android-like rootfs output with mksh and selected toybox applets as core
  project artifacts, not ordinary third-party ports.
- Recipe-backed porting tests under `out/<preset>/port-tests/`, with zlib,
  libpng, SQLite amalgamation, bzip2, xz, pcre2, mbedTLS, and curl verified
  through the current documented status. curl is `shared-pass` on Linux,
  macOS, and Windows, including real HTTP and HTTPS round trips against
  `example.com` for both static and shared libcurl.

For the authoritative short-form snapshot, see `STATUS.md`. For the per-port
matrix, see `docs/porting_status.md`. Historical first-bring-up notes are kept
under `docs/bringup/`.

The next product-level target is an Electron-class rebuilt application runtime,
documented in `docs/runtime_roadmap.md`: `libcrtgfx` (Skia + Wayland-style
compositor boundary + Chromium Ozone path), `libcrtmedia` (FFmpeg/codecs/audio/
video), and `libcrtjs` (QuickJS first, V8 later). Before that upper-runtime work
starts in earnest, the remaining libc/PAL planned items in `TODO.md` should be
reduced.

## Prerequisites

### Common

All platforms need:

- Git
- CMake 3.25 or newer
- LLVM Clang
- LLVM Clang++
- compiler-rt from the active Clang installation

Ninja is the default generator for all host presets.

### Linux

Install:

- CMake
- Ninja
- Clang/LLVM
- LLD
- compiler-rt

Example package names vary by distribution, but the required commands should be
available on `PATH`:

```sh
git --version
cmake --version
ninja --version
clang --version
clang++ --version
ld.lld --version
```

### Windows 11

Install:

- Git for Windows
- CMake
- Ninja
- LLVM/Clang
- Visual Studio Build Tools 2022 with the Windows SDK
- Windows Developer Mode enabled

The Windows SDK provides import libraries such as `kernel32.lib` and
`synchronization.lib`, which are needed by the current Windows backend. Use a
Developer PowerShell or Developer Command Prompt so the Windows SDK library
paths are visible to the linker.

Windows Developer Mode is required for non-elevated symlink creation. Several
porting recipes install shared-library aliases with `ln -s`/`symlink()` (for
example SONAME-style `libfoo.so` links), and the Windows PAL maps those calls to
Windows reparse-point symlinks. Without Developer Mode, those install steps can
fail with `EPERM` unless the build runs elevated.

The configure preset sets `CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY` because
CRT controls its own startup and C runtime boundary. This avoids CMake's default
compiler check trying to link a hosted MSVC runtime executable before CRT has
configured its own targets.

The build also disables CMake's default Windows C standard libraries for CRT
targets, and does the same for C++ after the C++ runtime bootstrap is enabled.
This prevents CMake's hosted MSVC defaults such as `oldnames.lib` from being
added to freestanding `clang++` links. CMake tries to locate the required
Windows SDK import libraries from the installed SDK. If that fails, either run
from a Visual Studio Developer shell or pass them explicitly:

```powershell
cmake --preset windows-host-ninja-debug `
  -DCRT_WINDOWS_KERNEL32_LIB="C:\Path\To\kernel32.lib" `
  -DCRT_WINDOWS_SYNCHRONIZATION_LIB="C:\Path\To\synchronization.lib"
```

Useful checks:

```powershell
git --version
cmake --version
ninja --version
clang --version
lld-link --version
where kernel32.lib
where synchronization.lib
```

Windows Defender's real-time scanning inspects every file the build writes,
which is significant here given how many small object files, port-build
artifacts, and rootfs entries a full build and porting-loop run produce.
Adding process and folder exclusions noticeably speeds up local builds. From
an elevated PowerShell session:

```powershell
# Add Process Exclusions
Add-MpPreference -ExclusionProcess "cmake.exe", "ninja.exe", "clang.exe", "clang++.exe", "clang-cl.exe", "lld.exe", "llvm-nm.exe"

# Add Folder Exclusions (Update paths based on your actual machine setup)
Add-MpPreference -ExclusionPath "C:\Program Files\LLVM"
Add-MpPreference -ExclusionPath "C:\path\to\your\projects\build"
```

### macOS

Install:

- Xcode or Xcode Command Line Tools
- CMake
- Ninja

Useful checks:

```sh
xcode-select -p
clang --version
cmake --version
ninja --version
```

## Provenance Checks

Imported Bionic and FreeBSD/msun source provenance is tracked in
`third_party/bionic/import_manifest.json`. After changing imported files or
their policy, run:

```sh
python3 tools/check_import_manifest.py
```

When CMake finds Python, the same check is available as:

```sh
cmake --build --preset macos-host-ninja-debug --target check-import-manifest
```

## Build

CTest reads generated metadata from each `out/<preset>/` directory. After adding
or removing tests, run the matching `cmake --preset ...` step on every host
before comparing test counts. The configure log prints `CRT registered tests`
for the selected OS preset; the count should match across macOS, Linux, and
Windows unless a test is intentionally gated by platform.

CMake caches the selected compiler and target OS in each `out/<preset>/`
directory. If a build command shows the wrong compiler, such as `/usr/bin/cc` or
`/usr/bin/c++`, or the wrong target define, such as `CRT_TARGET_OS_MACOS=1` in a
Linux build, clear or refresh that preset's build directory before rebuilding:

```sh
cmake --fresh --preset <os-host-ninja-debug>
```

If your CMake does not support `--fresh`, remove the matching `out/<preset>/`
directory and configure again.

CTest does not build missing test executables. If CTest reports `Unable to find
executable`, rerun the matching build preset or use the workflow preset:

```sh
cmake --workflow --preset <os-host-ninja-debug>
```

### Sysroot

Install the project sysroot into the matching build directory:

```sh
cmake --build --preset <os-host-ninja-debug> --target sysroot
```

The generated sysroot is installed under:

```text
out/<preset>/sysroot/
  include/
    public CRT headers
  lib/
    crt1.o
    libc.a
    libdl.a
    libm.a
    libc++.a
    libc.dylib / libc.so / c.dll
    libdl.dylib / libdl.so / dl.dll
    libm.dylib / libm.so / m.dll
    libc++.dylib / libc++.so / c++.dll
    libclang_rt.builtins.a
```

### macOS

The currently verified preset is macOS host debug:

```sh
cmake --preset macos-host-ninja-debug
cmake --build --preset macos-host-ninja-debug
ctest --preset macos-host-ninja-debug
```

Or run configure, build, and test in one step:

```sh
cmake --workflow --preset macos-host-ninja-debug
```

Install the project sysroot into the build directory:

```sh
cmake --build --preset macos-host-ninja-debug --target sysroot
```

### Linux

On a Linux host with Clang and Ninja installed:

```sh
cmake --preset linux-host-ninja-debug
cmake --build --preset linux-host-ninja-debug
ctest --preset linux-host-ninja-debug
cmake --build --preset linux-host-ninja-debug --target sysroot
```

Or run configure, build, and test in one step:

```sh
cmake --workflow --preset linux-host-ninja-debug
```

The test executable is expected at:

```text
out/linux-host-ninja-debug/tests/hello_c
```

### Windows 11

Open a Developer PowerShell or Developer Command Prompt with the Windows SDK
environment configured, then run:

```powershell
cmake --preset windows-host-ninja-debug
cmake --build --preset windows-host-ninja-debug
ctest --preset windows-host-ninja-debug
cmake --build --preset windows-host-ninja-debug --target sysroot
```

Or run configure, build, and test in one step:

```powershell
cmake --workflow --preset windows-host-ninja-debug
```

The test executable is expected at:

```text
out/windows-host-ninja-debug/tests/hello_c.exe
```

The Windows bring-up intentionally links `kernel32` for the OS boundary while
still avoiding the hosted C runtime with `-nostdlib`, `-nostartfiles`, and
`-nodefaultlibs`.

## Porting Tests

Porting tests validate whether unmodified upstream library source can be rebuilt
against the CRT sysroot. This is separate from the normal CRT unit test flow in
`## Build`.

The porting loop is also how the CRT grows. When an upstream package fails, the
preferred fix is to fill the missing Bionic-compatible CRT/sysroot/PAL surface,
not to patch the upstream package or expose host SDK headers by accident.

The implementation loop for each port is:

1. Run upstream `configure` or a direct `crt-cc` compile/link/run test to expose
   missing headers, types, symbols, or behavior.
2. Check how Android Bionic defines or implements the required surface.
3. Decide the CRT/PAL/sysroot extension policy and implement it in this
   repository.
4. Re-run the same porting test. If it fails again, return to step 1 and repeat
   with the next missing surface.

Bionic is the compatibility reference for this loop. New public headers, types,
macros, symbols, errno values, return-value behavior, and ABI shapes should be
checked against Android Bionic before they are added to the CRT sysroot. Host
OS-specific APIs may still be used behind PAL adapters, but they should not
define the public CRT surface unless a documented compatibility shim is being
added deliberately.

The intended user workflow is:

1. Build and install the CRT sysroot for the current host preset.
2. Download and extract upstream source archives under `out/<preset>/port-tests/src`.
3. Load the CRT porting environment.
4. Run the upstream project's native `configure`, `make`, and `make install`.
5. Record the result in `docs/porting_status.md` and the matching recipe.

The porting work area is always under the build tree:

```text
out/<preset>/port-tests/
  downloads/
  src/
  build/
  install/
  logs/
```

### Recipes

Porting recipes live under `porting/recipes/`. A recipe is the source of truth
for one upstream library's porting metadata:

- `source.url`: upstream archive URL.
- `source.archive`: expected archive filename.
- `source.source_dir`: extracted source directory name.
- `source.sha256`: authoritative archive integrity hash.
- `source.sha1` and `source.md5`: compatibility metadata only.
- `dependencies`: other recipes that must be installed into `PORT_PREFIX` first.
- `build.system`: initial build driver, such as `configure`, `amalgamation`, or
  `manual`.
- `build.automated`: optional boolean; `false` records an upstream build flow
  that exists but is not yet included in aggregate CMake port builds.
- `build.configure_args`: arguments appended before `--prefix=$PORT_PREFIX`.
- `build.sources`, `build.archive`, and `build.install_headers`: direct
  single-source/amalgamation build inputs when the upstream archive has no
  configure script.
- `build.cflags`: recipe-owned feature switches for the upstream package, kept
  separate from `CRT_EXTRA_CFLAGS`.
- `build.env`: recipe-specific configure/cache variables for CRT toolchain
  capability declarations.
- `status`: per-host result for Linux, macOS, and Windows.
- `notes`: known gaps, required CRT surface, or follow-up policy.

The human-readable success matrix is maintained in
`docs/porting_status.md`.

The long-term Windows porting environment is an Android-like shell and command
rootfs running on this CRT/PAL rather than MSYS/Git Bash as a runtime
compatibility layer. See `docs/android_shell_environment.md`. The initial
rootfs scaffold can be created with:

```sh
cmake --build --preset windows-host-ninja-debug --target rootfs
```

### Environment

First install the CRT sysroot:

```sh
cmake --build --preset macos-host-ninja-debug --target sysroot
```

Load the CRT porting environment from the repository root. On Linux/macOS, or
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

The environment helpers intentionally reset `CPPFLAGS` and `LDFLAGS` to the CRT
port prefix instead of appending the previous shell values. Use
`CRT_EXTRA_CPPFLAGS`, `CRT_EXTRA_LDFLAGS`, `CRT_EXTRA_CFLAGS`,
`CRT_EXTRA_CXXFLAGS`, or `CRT_EXTRA_LIBS` when an upstream package needs extra
flags.

### Manual Examples

Example zlib build from an extracted upstream source directory:

```sh
cd /path/to/zlib-1.3.1
./configure --static --prefix="$PORT_PREFIX"
make -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 2)"
make install
```

Example libpng build after zlib has been installed into `PORT_PREFIX`:

```sh
cd /path/to/libpng-1.6.57
./configure --disable-shared --enable-static --prefix="$PORT_PREFIX"
make -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 2)"
make install
```

The wrappers use only CRT sysroot libc headers plus Clang resource headers, and
link configure test executables with CRT startup/static runtime archives. See
`docs/sysroot_ports.md`.

### Automation

CMake provides recipe-backed porting targets. They still run the upstream
`configure && make && make install` flow under `out/<preset>/port-tests`, but
they make the common test path easier to repeat.

On Linux and macOS, the recipe targets normally let the host POSIX shell and
host userland drive upstream `configure`/`make`. That does not mean the produced
libraries are host-libc builds: `CC`/`CXX` still point at `tools/crt-cc` and
`tools/crt-c++`, which use the CRT sysroot headers, startup objects, and
libraries. On macOS, `otool -L` will still show `/usr/lib/libSystem.B.dylib` on
the final Mach-O files because libSystem is the PAL/backend boundary for Darwin
syscalls, dyld, pthreads, and process services. The porting audit checks for
the more important distinction: port dylibs should record this project's CRT
dylibs (`@rpath/libc.dylib`, plus libm/libdl/libc++ where used), and ordinary
libc/POSIX symbols such as malloc, stdio, sockets, pthreads, time, and string
functions should resolve through the CRT layer rather than directly from
libSystem. The current macOS port install tree has been rebuilt and audited
with `otool -L`/`nm -m -u` on that basis.

On native Windows, Autoconf `configure` scripts are POSIX shell scripts. The
CMake port targets may be launched from PowerShell or `cmd.exe`, but configure
recipes still require Git Bash or MSYS2 build tools (`bash` or `sh`, plus
`make`) to be installed and visible in `PATH`. If the shell is not discoverable,
set `CRT_PORT_SHELL` to the full path of `bash.exe` or `sh.exe` before running
the CMake port target:

```bat
set CRT_PORT_SHELL=C:\msys64\usr\bin\bash.exe
cmake --build --preset windows-host-ninja-debug --target port-rebuild-zlib
```

This shell is only used to run upstream configure/make scripts. The compiled
objects and libraries still use the CRT sysroot wrappers and the Windows native
Clang/LLD toolchain selected by the preset.

List the available recipes:

```sh
cmake --build --preset macos-host-ninja-debug --target port-list
```

Fetch and extract all recipe sources:

```sh
cmake --build --preset macos-host-ninja-debug --target port-fetch
```

Fetch and extract one recipe source:

```sh
cmake --build --preset macos-host-ninja-debug --target port-fetch-zlib
```

Per-recipe fetch targets also fetch recipe dependencies. For example,
`port-fetch-libpng` fetches both libpng and zlib because libpng declares zlib in
`dependencies`.

Build an automated recipe against the CRT sysroot:

```sh
cmake --build --preset macos-host-ninja-debug --target port-build-zlib
cmake --build --preset macos-host-ninja-debug --target port-build-libpng
cmake --build --preset macos-host-ninja-debug --target port-build-sqlite-amalgamation
```

Force an automated recipe to rebuild:

```sh
cmake --build --preset macos-host-ninja-debug --target port-rebuild-zlib
```

Build aggregate recipe groups:

```sh
cmake --build --preset macos-host-ninja-debug --target port-build-configure
cmake --build --preset macos-host-ninja-debug --target port-rebuild-configure
cmake --build --preset macos-host-ninja-debug --target port-build-recipes
cmake --build --preset macos-host-ninja-debug --target port-rebuild-recipes
```

Run recipe-declared runtime checks:

```sh
cmake --build --preset macos-host-ninja-debug --target port-test-xz
cmake --build --preset macos-host-ninja-debug --target port-test-recipes
```

`port-test-<name>` first ensures the corresponding `port-build-<name>` target is
installed, then compiles and runs the checks declared in that recipe's `tests`
array. `port-test-recipes` runs every recipe that currently declares automated
tests. These tests now cover real static/shared round trips for zlib, bzip2,
xz, libpng, pcre2, mbedTLS, and curl where the recipe declares the matching
test entries; curl intentionally uses real `example.com` HTTP/HTTPS checks
rather than a local loopback-only substitute.

`tools/fetch_ports.py` and `tools/crt-port-build.py` are the lower-level helpers
used by those CMake targets. They read recipes from `porting/recipes/` and are
kept for project automation and agent-side regression checks. Automated port
builds also ignore inherited host `CPPFLAGS`, `CFLAGS`, `CXXFLAGS`, `LDFLAGS`,
and `LIBS`; pass project-specific additions through the matching `CRT_EXTRA_*`
variables.

CMake target names are generated from the recipes at configure time. If a new
recipe file is added, rerun the matching `cmake --preset ...` command before
using its `port-fetch-<name>`, `port-build-<name>`, or `port-test-<name>` target.

### Skia For libcrtgfx

Skia is treated as a core `libcrtgfx` dependency, not as an ordinary host
library and not as an upstream source patch. The source/build automation lives
behind dedicated CMake targets so it can use the same CRT sysroot discipline as
the porting recipes:

```sh
cmake --build --preset macos-host-ninja-debug --target crtgfx-skia-fetch
cmake --build --preset macos-host-ninja-debug --target crtgfx-skia-configure
cmake --build --preset macos-host-ninja-debug --target crtgfx-skia-build
```

The default Skia source track is the Chrome/Skia milestone `m148`. Override it
at configure time when needed:

```sh
cmake --preset macos-host-ninja-debug -DCRTGFX_SKIA_VERSION=m149
cmake --preset macos-host-ninja-debug \
  -DCRTGFX_SKIA_REF=refs/heads/chrome/m148 \
  -DCRTGFX_SKIA_EXPECTED_COMMIT=<full-commit-hash>
```

`crtgfx-skia-build` installs Skia under
`out/<preset>/external/skia/install`. Re-run configure after that install exists
so `CRTGFX_ENABLE_SKIA` can see the real headers and library, then rebuild and
run the normal test workflow. The Skia bridge deliberately does not provide
fake Skia headers; applications should include normal Skia headers through the
CRT sysroot.

## Repository Layout

```text
docs/
include/
porting/
platform/
arch/
cmake/
libc/
libm/
libdl/
libstdc++/
linker/
tests/
tools/
```

Only some of these directories exist today. The layout reflects the intended
long-term structure.

## Design Documents

See:

- `docs/project_meanings.md`
- `docs/project_stacks.md`
- `docs/runtime_roadmap.md`
- `docs/bringup/hello_bringup.md`
- `docs/header_abi.md`
- `docs/dynamic_loading.md`
- `docs/cxx_runtime.md`
- `docs/linker_loader.md`
- `docs/shared_libraries.md`
- `docs/sysroot_ports.md`
- `docs/porting_status.md`

## License

See [`LICENSE.md`](LICENSE.md). CRT follows the same per-file license policy
as Android Bionic: project-owned code uses a default BSD-style license, and
code imported or adapted from Bionic and other upstream projects (mksh,
toybox, awk, ...) keeps its own original license.
