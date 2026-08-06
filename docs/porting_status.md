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
| libpng | 1.6.57 | `porting/recipes/libpng.json` | pending | configure-pass | configure-blocked | configure | Depends on zlib in `PORT_PREFIX`. Windows aarch64 `configure` (autoconf-generated, unlike zlib's hand-written one) has driven a long chain of real CRT/mksh/regex bugs found and fixed along the way -- most recently: the Windows `fork()` self-relaunch's fd-inheritance gap, a builtin-to-external pipe deadlock (`CreatePipe()` buffer size), missing `egrep`/`fgrep` toybox aliases, no ERE alternation support (fixed by replacing the hand-rolled regex matcher with a real ported Bionic/NetBSD regex engine, see `third_party/bionic/README.md`'s "Regex Tranche"), and `lseek()` not failing on pipes (`ESPIPE`) causing toybox `grep`/`egrep`/`fgrep` to silently drop all piped input. `configure` now reaches `configure: error: no acceptable ld found in $PATH` (this project's rootfs has no standalone `ld` binary) -- see `TODO.md`, "in progressing", for the full trail and where to resume. |
| SQLite amalgamation | 3.53.4 | `porting/recipes/sqlite-amalgamation.json` | smoke-pass | amalgamation-pass | smoke-pass | amalgamation | macOS recipe builds upstream sqlite3.c into libsqlite3.a with `-U__APPLE__`, so SQLite uses the generic Unix path against Bionic-shaped CRT headers instead of Darwin-only statfs/VFS extensions. |
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
