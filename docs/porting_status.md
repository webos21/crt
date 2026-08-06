# Library Porting Status

This document records third-party source portability against the CRT sysroot.
Recipes live under `porting/recipes/` and include source URLs, archive names,
hashes, dependencies, build options, and per-host status.

Porting failures should normally become CRT work items. The preferred direction
is to keep upstream source unchanged and fill missing Bionic-compatible
headers, libc/libm/libdl/linker/C++ runtime, startup/sysroot, or PAL behavior in
this repository. Host SDK leakage or ad hoc upstream patches should be treated
as policy exceptions and documented before use.

Each status update should follow the porting loop: expose missing requirements
with upstream `configure` or `crt-cc`, check Bionic's header/source/ABI policy,
implement the CRT/PAL/sysroot extension, then rerun the same porting test and
repeat until it passes or a documented policy decision blocks it.

Status notes should call out any deliberate deviation from Bionic. Temporary
host-specific compatibility shims are acceptable only when they are documented
as such and do not silently replace the Bionic-compatible public surface.

The status values are intentionally conservative:

- `configure-pass`: upstream configure/make/install flow passed with CRT
  wrappers.
- `manual-pass`: basic manual source build or direct runtime test passed, but the
  current recipe flow should be rerun and recorded.
- `amalgamation-pass`: upstream amalgamation source built and installed through
  the recipe flow without modifying upstream source.
- `smoke-pass`: earlier curated integration smoke passed; native upstream build
  flow is still pending.
- `partial`: useful subset validated; important upstream features remain open.
- `configure-blocked`: upstream configure is available, but the full recipe
  build is blocked by a missing CRT/sysroot policy or API surface.
- `pending`: not yet verified for that host.

| Library | Version | Recipe | Linux | macOS | Windows | Build System | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| make | android-toolchain-44fc4fe66a484b91844c302f03eaa8438e065d17 | `porting/recipes/make.json` | pending | pending | manual-pass | android_host_tool | Android `toolchain/make` built on Windows x86_64 through rootfs mksh and CRT wrappers; `make.exe --version` runs with the CRT DLL path. |
| zlib | 1.3.1 | `porting/recipes/zlib.json` | manual-pass | configure-pass | configure-pass | configure | Static `libz.a`; Windows x86_64 and aarch64 both pass through rootfs mksh plus CRT-built make with serial make execution and `RANLIB=true`. aarch64 needed the spawn-broker fix for `CreateProcessA()`/`CreatePipe()` inside `RtlCloneUserProcess` clones -- see `docs/windows_fork_emulation.md`. |
| libpng | 1.6.57 | `porting/recipes/libpng.json` | pending | configure-pass | configure-pass | configure | Depends on zlib in `PORT_PREFIX`. Windows aarch64's real `configure && make && make install` now completes in full -- `libpng16.a`/`libpng.a` built, archived, and installed, along with all `contrib/tools`/`contrib/libtests` sample binaries and header/pkgconfig/man-page installation via `install-sh`. Getting there drove the single longest blocker chain of the whole Windows porting effort, each one a real, general CRT/mksh/regex/tooling gap fixed on its own merits rather than a libpng-specific workaround: the `fork()` self-relaunch fd-inheritance gap, a builtin-to-external pipe deadlock, missing `egrep`/`fgrep` toybox aliases, no ERE alternation (fixed by porting a real Bionic/NetBSD regex engine, see `third_party/bionic/README.md`'s "Regex Tranche"), `lseek()` not failing on pipes, missing `ld`/`awk` (the latter via a full `onetrueawk` port into `shell/awk/`, which also surfaced a real `printf` `%g`-precision bug), a COFF-vs-Unix static-lib naming mismatch, a missing `<windows.h>` (worked around the same way zlib's recipe already does, via `-U_WIN32` etc. `CFLAGS`), `arm_neon.h` not on the include path (`crt-cc`/`crt-c++` weren't querying clang's resource-dir on Windows), `AR`/`RANLIB`/`STRIP`/`LD` wrapped as a shell-parsed string that silently broke inside `configure`'s own `` `$LD -v` `` probe (replaced with a real wrapper script, `tools/crt-native-tool`), a stock Windows LLVM install defaulting to the MSVC target triple (no `__GNUC__`, misleading autoconf/libtool into picking the wrong archiver -- fixed via an explicit `--target=*-w64-mingw32`), and finally `install-sh` itself failing to run at all, because this project's Windows PAL had never needed to execute a `#!`-script directly nor report one as "executable" via `stat()`/`access()` (both now fixed in `libc/src/arch/windows/common/syscall.c`). See `TODO.md`, "done", for the full trail. |
| SQLite amalgamation | 3.53.4 | `porting/recipes/sqlite-amalgamation.json` | smoke-pass | amalgamation-pass | amalgamation-pass | amalgamation | macOS builds sqlite3.c with `-U__APPLE__` (generic Unix path, not Darwin-only statfs/VFS extensions). Windows aarch64 builds it with `-U_WIN32 -UWIN32 -U__CYGWIN__ -U__MINGW32__ -U__BORLANDC__` (same reasoning: `tools/crt-cc` targets `*-w64-mingw32`, which predefines `__MINGW32__`/`_WIN32`/`WIN32` and would otherwise trip sqlite3.c's own `SQLITE_OS_WIN` detection into `#include "windows.h"`, which this sysroot doesn't have) -- full recipe flow (compile/archive/ranlib/install/`.lib`-alias) verified via `crt-port-build.py`, then a standalone program linked against the installed `libsqlite3.a` actually ran `sqlite3_open(":memory:")`/`CREATE TABLE`/`INSERT`/`SELECT` and got the correct value back. |
| libffi | 3.4.5 | `porting/recipes/libffi.json` | partial | configure-pass | partial | configure | macOS/aarch64 configure/make/install passed; ffi_call and closure smoke passed against the CRT sysroot. |

## Policy

Third-party source archives are not committed. Download caches, extracted source
trees, build directories, logs, and install prefixes belong under:

```text
out/<preset>/port-tests/
  downloads/
  src/
  build/
  install/
  logs/
```

Recipe hashes use SHA256 as the authoritative integrity check. SHA1 and MD5 may
be recorded as compatibility metadata only.
