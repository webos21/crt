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

## Direction

Porting tests are expected to drive CRT implementation work. When an original
upstream source tree fails to configure, build, link, or run, the first response
is to identify the missing Bionic-compatible CRT/sysroot/PAL surface and add it
to this project.

Use this loop for every porting failure:

1. Run upstream `configure` or a direct `crt-cc` compile/link/run test and record
   the missing header, type, macro, symbol, linker behavior, or runtime behavior.
2. Check the corresponding Android Bionic public header, source implementation,
   ABI shape, errno behavior, and documented compatibility policy.
3. Decide whether this project should import, adapt, stub with a clear policy, or
   implement project-owned CRT/PAL behavior, then make the change in the CRT
   sysroot/runtime.
4. Re-run the same porting test. If the port still fails, go back to step 1 and
   continue with the next missing surface.

Android Bionic is the reference implementation for public CRT compatibility.
When a port exposes a missing requirement, resolve it by checking Bionic first:
header location, feature-test visibility, type width/layout, macro value,
function prototype, symbol aliasing, errno mapping, and runtime semantics. The
CRT/PAL may use Linux, macOS, or Windows host facilities internally, but those
host APIs must be hidden behind adapters and must not silently redefine the
public sysroot surface.

If an upstream package selects a host-specific path because of compiler
predefines such as `__APPLE__`, `_WIN32`, or `__linux__`, decide explicitly
whether to steer it toward the Bionic/POSIX path with recipe flags, provide a
documented compatibility shim, or postpone the port until the needed Bionic-like
surface exists. Do not add Darwin/glibc/MSVC-specific ABI shapes to public
headers just to make one port compile.

The default policy is:

- keep upstream source unmodified;
- do not include host libc/SDK headers through the CRT wrapper;
- do not make a port pass by linking against host libc facilities accidentally;
- use recipe `build.env` only to describe CRT toolchain capability or stable
  configure cache results;
- record any unavoidable port-specific workaround with the reason and the
  expected long-term replacement;
- promote recurring port failures into focused CRT tests once the missing
  surface is implemented.

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

The env files reset `CPPFLAGS` and `LDFLAGS` instead of appending previous shell
values, so repeated sourcing does not accumulate stale `out/` paths. Use
`CRT_EXTRA_CPPFLAGS`, `CRT_EXTRA_LDFLAGS`, `CRT_EXTRA_CFLAGS`,
`CRT_EXTRA_CXXFLAGS`, or `CRT_EXTRA_LIBS` for package-specific additions.

Porting recipes live under `porting/recipes/`. They record source URLs,
archives, SHA256 hashes, dependencies, build-system type, build arguments, and
host status. `configure` recipes run upstream configure/make/install.
`amalgamation` recipes build an upstream single-source distribution directly
without modifying the upstream source tree. `android_host_tool` recipes build
Android-owned host tools from their Android.bp source lists so the porting
environment can bootstrap project-owned build tools before running third-party
configure recipes. The current human-readable matrix is
`docs/porting_status.md`.

Build and install zlib from the extracted upstream source directory:

Android exposes zlib as a separate `external/zlib` / `libz` public library, not
as part of Bionic libc. Keep this project aligned with that model: zlib belongs
in the CRT sysroot/runtime library set and may be linked privately by components
that need it, but it should not be folded into `libc`.

**Update**: neither example below passes `--static`/`--disable-shared`
anymore -- `porting/recipes/zlib.json` and `porting/recipes/libpng.json`
both use `configure_args: []` (upstream's own default, which builds both
static and shared) and both reach `shared-pass` on all three OSes; see
`docs/porting_status.md` for the current per-host status matrix and the
real bugs that had to be fixed to get there.

```sh
cd /path/to/zlib-1.3.1
./configure --prefix="$PORT_PREFIX"
make -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 2)"
make install
```

Build libpng from the extracted upstream source directory after zlib has been
installed into `PORT_PREFIX`:

```sh
cd /path/to/libpng-1.6.57
./configure --prefix="$PORT_PREFIX"
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

CMake exposes the same recipe-backed flow as build targets:

```sh
cmake --build --preset macos-host-ninja-debug --target port-list
cmake --build --preset macos-host-ninja-debug --target port-fetch
cmake --build --preset macos-host-ninja-debug --target port-build-make
cmake --build --preset macos-host-ninja-debug --target port-fetch-libpng
cmake --build --preset macos-host-ninja-debug --target port-build-zlib
cmake --build --preset macos-host-ninja-debug --target port-build-libpng
cmake --build --preset macos-host-ninja-debug --target port-build-configure
cmake --build --preset macos-host-ninja-debug --target port-rebuild-zlib
cmake --build --preset macos-host-ninja-debug --target port-build-sqlite-amalgamation
cmake --build --preset macos-host-ninja-debug --target port-build-recipes
cmake --build --preset macos-host-ninja-debug --target port-rebuild-configure
```

The CMake recipe targets always compile and link through the CRT wrapper
toolchain (`tools/crt-cc` / `tools/crt-c++`) and the CRT sysroot. The shell that
drives upstream `configure` and `make` is host-dependent:

- Linux/macOS use the host POSIX shell and host coreutils. Those hosts already
  provide the complete shell/userland environment upstream configure scripts are
  normally tested against, and the CRT boundary is still enforced by the wrapper
  compiler's `-nostdinc`, `CRT_SYSROOT`, startup object, and library flags.
  On macOS, final Mach-O executables and dylibs may still list
  `/usr/lib/libSystem.B.dylib` in `otool -L`; that is the intended Darwin
  PAL/backend dependency, not evidence that the upstream library was built
  against the host C library headers or startup files. The useful audit is
  stricter: rebuilt port dylibs should record this project's CRT dylibs such as
  `@rpath/libc.dylib`, and ordinary libc/POSIX undefined symbols should resolve
  through those CRT dylibs rather than directly from libSystem. The macOS
  port-install tree has been checked with `otool -L` and `nm -m -u` using that
  distinction.
- Native Windows uses the project-owned rootfs mksh and toybox applets. Windows
  has no native shebang handling for `tools/crt-cc` / `tools/crt-c++`, so CMake
  adds `--use-crt-shell` there and runs the recipe commands through
  `out/<preset>/rootfs/system/bin/mksh`.

On Windows, the CMake `port-build-*` and `port-rebuild-*` targets for configure
recipes run:

1. upstream `./configure` under `out/<preset>/rootfs/system/bin/mksh`;
2. `make -jN` under that same mksh;
3. `make install` under that same mksh.

`make` is now the first project-built bootstrap tool. AOSP does not carry GNU
make under `platform/external`; Android keeps the source under `toolchain/make`
and prebuilts under `platform/prebuilts/build-tools`. The CRT recipe follows
the Android.bp `cc_binary_host` source list and installs `make` into
`PORT_PREFIX/bin`. Configure recipes prefer that installed make before falling
back to any host make. On Windows the recipe intentionally uses the POSIX-like
Android config path instead of the upstream Win32 make path, so failures expose
missing Bionic/POSIX CRT/PAL behavior.

On Windows CRT-shell builds, configure recipes append `SHELL=/system/bin/mksh`
to both the build and install invocations, keeping recipe command execution on
the project-owned shell/process path. **Update: `make` now runs with real
parallelism (`os.cpu_count()` jobs) by default on Windows too, matching
macOS/Linux** -- a real jobserver crash and a related token-accounting bug
were both root-caused and fixed, then verified against a real libpng build
(`-j 12`, zero errors/warnings/jobserver messages) before removing
`tools/crt-port-build.py`'s old Windows-only serial-default special case; see
`HISTORY.md`'s 2026-08-11 entries. `--jobs N` overrides the default for any
single invocation.

The older Windows bootstrap path can still be used manually by invoking
`tools/crt-port-build.py` without `--use-crt-shell` and setting
`CRT_PORT_SHELL`, but it is no longer the default CMake target path:

```bat
set CRT_PORT_SHELL=C:\msys64\usr\bin\bash.exe
cmake --build --preset windows-host-ninja-debug --target port-rebuild-zlib
```

The lower-level configure-only smoke path is still available:

```powershell
& 'C:/Users/appos/AppData/Local/Programs/Python/Python314/python.exe' `
  tools/crt-port-build.py `
  --preset windows-host-ninja-debug `
  --target-os windows `
  --port zlib `
  --use-crt-shell `
  --configure-only `
  --rebuild
```

In CRT-shell mode `crt-port-build.py` discovers the host LLVM tools and, on
Windows, passes the Windows SDK library directory to the compiler wrappers as an
internal backend detail. This is the preferred way to expose missing
CRT/PAL/rootfs behavior from configure scripts without patching upstream source.

The project now also builds bootstrap CRT shell and make artifacts:

```text
out/<preset>/rootfs/system/bin/sh
out/<preset>/rootfs/bin/sh
out/<preset>/rootfs/usr/bin/sh
out/<preset>/port-tests/install/bin/make
```

The rootfs shell is now the standard configure recipe driver, and make is built
against the CRT sysroot before normal configure recipes run their build and
install steps.

The zlib recipe also undefines Windows compiler predefines such as `_WIN32` and
`_MSC_VER` so upstream zlib stays on its generic POSIX path instead of selecting
the Win32 `<io.h>` branch. This is a recipe-level declaration of the CRT target
surface, not an upstream source patch. It sets `RANLIB=true` because zlib treats
ranlib as an optional archive-index refresh and the LLVM `ar` path already
produces the static archive needed by the porting test. The Windows mksh
subshell status quirk zlib's `ranlib || true` line exposed is fixed -- see
`HISTORY.md`'s 2026-08-16 entry and `tests/mksh_subshell_status_test.c`.

Per-recipe fetch targets resolve recipe dependencies. For example,
`port-fetch-libpng` also fetches zlib, and zlib fetches the Android make source
needed to bootstrap the recipe build tool.

`tools/fetch_ports.py` and `tools/crt-port-build.py` read `porting/recipes/` and
are the lower-level helpers used by those CMake targets. `crt-port-build.py`
uses the same clean-flag policy for automated builds and ignores inherited host
`CPPFLAGS`, `CFLAGS`, `CXXFLAGS`, `LDFLAGS`, and `LIBS` unless the matching
`CRT_EXTRA_*` variable is set.

## Documented PAL Behavior Differences

The public sysroot should stay Bionic/POSIX-shaped, but some host backends cannot
faithfully represent every low-level Unix concept. In those cases, keep the API
usable for source ports, return success for safe no-op compatibility behavior
when possible, and document the difference explicitly.

| API | Linux | macOS | Windows |
| --- | --- | --- | --- |
| `fchown(fd, owner, group)` | Native syscall behavior. | Native syscall behavior. | Validates the CRT fd and returns success as a no-op ownership adapter. `stat()`/`fstat()` report synthetic uid/gid values. Future PAL work may map selected cases to Windows security descriptors/SIDs, but POSIX uid/gid ownership is not currently enforced. |

## SQLite Follow-up Gaps

SQLite amalgamation now builds on macOS through the recipe flow, but that result
only proves that the library compiles and links against the CRT sysroot. The
following items were exposed by SQLite and still need deeper implementation
before file-backed SQLite databases should be treated as robust runtime
validation.

| Area | Current state | Bionic-compatible direction |
| --- | --- | --- |
| `fcntl()` record locks | Basic `F_GETLK/F_SETLK/F_SETLKW` support is implemented. Linux uses native `fcntl`; macOS adapts Bionic `struct flock` and command values to Darwin; Windows maps range locks to `LockFileEx`/`UnlockFileEx`. | Add multi-process contention tests and refine Windows conflict reporting where POSIX owner pid information cannot be represented directly. |
| `statfs()` / `fstatfs()` data | Public ABI shape follows Bionic `sys/vfs.h`. Linux uses native `statfs`; macOS converts Darwin `statfs64` to the Bionic-shaped struct; Windows fills generic block size/name length and uses `GetDiskFreeSpaceExA` for path-based space data. | Improve Windows `fstatfs()` volume discovery and fill more filesystem fields where host APIs provide stable equivalents. Continue documenting unsupported host-specific fields instead of adding Darwin-only members. |
| `utimes()` / `futimes()` | Real timestamp updates are implemented. Linux uses `utimensat`; macOS uses native `utimes`/`futimes`; Windows uses `SetFileTime`. | Add broader tests for `NULL` times, invalid microseconds, directory paths, permission failures, and sub-second rounding behavior. Preserve Bionic errno behavior. |
| `ioctl()` | Public constants now follow the Linux/Bionic ioctl surface for common termios/socket requests, including `FIONREAD`, `TIOCGPGRP`, `TIOCSPGRP`, `TIOCGWINSZ`, and `TIOCSWINSZ`. Linux calls native `ioctl`; macOS maps Bionic request numbers to Darwin request numbers for the implemented common requests; Windows supports `FIONREAD` for pipes/sockets and `TIOCGWINSZ` for console handles. Unsupported requests fail through host-compatible errno paths. | Expand request coverage only when ports need it, preferably by importing the relevant Bionic/Linux UAPI header tranche. Add focused tests for terminal attributes, socket nonblocking ioctls, and any device-specific request before exposing more constants. |
| Job control | `setpgid()`, `getpgrp()`, and `setsid()` use native syscalls on Linux and macOS. `tcgetpgrp()`/`tcsetpgrp()` use the Bionic ioctl constants, with macOS request-number translation. Windows exposes a documented console process-group approximation: current-process `setpgid()`, `getpgrp()`, and `setsid()` are CRT-managed, `tc*pgrp()` works only on tty/console fds, and non-tty fds return `ENOTTY`. | Keep this as the non-interactive shell baseline. Full interactive job control still needs Ctrl-C/Ctrl-Break delivery policy, process-group waits, stopped-child status, and terminal foreground arbitration. See `docs/job_control.md`. |
| `fchown()` | Linux/macOS call native syscalls. Windows validates the CRT fd and returns success as a documented no-op ownership adapter; `stat()`/`fstat()` expose a synthetic uid/gid because Windows has no direct POSIX uid/gid ownership model. | Keep the public Bionic/POSIX call surface usable for ports. Later, optionally map ownership to Windows security descriptors/SIDs behind the PAL while preserving the documented fallback behavior for unsupported uid/gid changes. |
| `sysconf()` | Bionic `_SC_*` numbers are exposed for configure-heavy probes. The common switch follows Bionic-style return policy for constants such as `_SC_OPEN_MAX`, `_SC_CLK_TCK`, `_SC_MAPPED_FILES`, `_SC_MONOTONIC_CLOCK`, thread limits, and POSIX feature probes. Runtime values are routed through narrow PAL hooks: Linux follows Bionic's sysfs/procfs style for CPU and memory queries, macOS uses Darwin `sysctl`, and Windows uses `GetSystemInfo`/`GlobalMemoryStatusEx`. Unknown names return `-1` with `ENOSYS`; known unsupported names return `-1` without setting errno. | Expand the table from Bionic `bits/sysconf.h` as more ports need it. Revisit `_SC_MAPPED_FILES` when file-backed `mmap` support is completed, and add tests for every newly claimed positive capability. |
| `sys/mman.h` | Public constants and function surface follow Bionic/Linux UAPI shape: `MAP_*`, `MS_*`, `MADV_*`, `MCL_*`, `mmap64()`, `msync()`, `madvise()`, `posix_madvise()`, `mlock*()`, `mincore()`, and `mremap()`. Linux uses native syscalls. macOS maps the common calls to Darwin syscalls and returns `ENOSYS` for Linux-only calls such as `mremap()`. Windows supports anonymous mapping with `VirtualAlloc`, file-backed mapping with `CreateFileMappingA`/`MapViewOfFileEx`, `mprotect()` with `VirtualProtect`, and lock/sync adapters where Win32 has a direct equivalent. | Continue importing Bionic cleaned UAPI mmap headers as ports need more constants. Windows `MAP_FIXED` currently attempts fixed-address mapping but does not forcibly replace existing mappings like Linux; document any further divergence and add file-backed mmap tests before using it as a SQLite runtime validation signal. |
| SQLite recipe flags | The recipe uses `-U__APPLE__`, `SQLITE_WITHOUT_ZONEMALLOC`, and `SQLITE_ENABLE_LOCKING_STYLE=0` to stay on SQLite's generic Unix path. | Keep this policy unless the CRT intentionally adds a documented Darwin compatibility shim. The public sysroot should remain Bionic-shaped, not Darwin-shaped. |
| SQLite shell | `shell.c` is not part of the current recipe and currently exposes more surface such as `pwd.h`. | Treat the shell as a separate porting step. First add Bionic-compatible `pwd.h`/user database stubs or PAL-backed functions, then build/link the shell without modifying upstream source. |
| Runtime smoke depth | Current smoke uses an in-memory database. | Add file-backed SQLite tests after record locking and timestamp updates are implemented: create/open/write/query, journal/WAL basics if enabled, close/reopen, and concurrent lock behavior. |

## Current Result

**This section is a point-in-time snapshot from early in the porting effort
and duplicates (with stale data) what `docs/porting_status.md` now tracks
properly per-host, per-port, and kept current -- see that file for the real
status matrix (all of make/zlib/libpng/sqlite-amalgamation now reach
`shared-pass`/`amalgamation-pass` on Linux, macOS, and Windows; bzip2 and xz
also reach `shared-pass` on all three OSes; libffi is `partial`).** Kept below
only as a historical record of what the *first* successful pass looked like and
what it required.

On macOS host, the following original configure/make flows first passed with
the strict CRT sysroot wrapper (all three now build `shared`, not just
`static` -- see `docs/porting_status.md`):

| Port | Source flow | Result |
| --- | --- | --- |
| Android toolchain make | Android.bp host-tool source list | Windows x86_64 pass |
| zlib 1.3.1 | `./configure --static && make && make install` | macOS pass; Windows x86_64 pass through rootfs mksh and CRT-built make |
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
