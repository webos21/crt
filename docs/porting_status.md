# Library Porting Status

This document records third-party source portability against the CRT sysroot.
Recipes live under `porting/recipes/` and include source URLs, archive names,
hashes, dependencies, build options, tests, and per-host status.

This file is intentionally a current-status index. Long investigation trails
belong in `HISTORY.md` and in each recipe's own `notes` field.

## Porting Policy

Porting failures should normally become CRT work items. The preferred direction
is to keep upstream source unchanged and fill missing Bionic-compatible
headers, libc/libm/libdl/linker/C++ runtime, startup/sysroot, or PAL behavior in
this repository. Host SDK leakage or ad hoc upstream patches should be treated
as policy exceptions and documented before use.

Each status update should follow the porting loop:

1. Expose missing requirements with upstream `configure` or direct `crt-cc`
   compile/link/run tests.
2. Check Bionic's public header, source, ABI shape, and errno policy.
3. Implement the CRT/PAL/sysroot extension.
4. Re-run the same porting test and repeat until it passes or a documented
   policy decision blocks it.

Status notes should call out deliberate deviations from Bionic. Temporary
host-specific compatibility shims are acceptable only when they are documented
as such and do not silently replace the Bionic-compatible public surface.

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

## Status Values

- `configure-pass`: upstream configure/make/install flow passed with CRT
  wrappers.
- `shared-pass`: `configure-pass`, and the shared library
  (`.so`/`.dll`/`.dylib`) built, installed, loaded, and ran correctly at
  runtime. When a recipe declares tests, the preferred verification is
  `cmake --build --preset <preset> --target port-test-<name>` or
  `port-test-recipes`.
- `static-pass`: `configure-pass`, with shared-library building attempted and
  root-caused but not achieved.
- `manual-pass`: basic manual source build or direct runtime test passed, but
  the current recipe flow should be rerun and recorded.
- `amalgamation-pass`: upstream amalgamation source built and installed through
  the recipe flow without modifying upstream source.
- `smoke-pass`: earlier curated integration smoke passed; native upstream build
  flow is still pending.
- `partial`: useful subset validated; important upstream features remain open.
- `configure-blocked`: upstream configure is available, but the full recipe
  build is blocked by a missing CRT/sysroot policy or API surface.
- `pending`: not yet verified for that host.

## Current Summary

The current completed queue is:

```text
zlib -> libpng -> SQLite amalgamation -> bzip2 -> xz -> pcre2 -> mbedTLS -> curl
```

`curl` closes the current networking/TLS porting queue: Linux, macOS, and
Windows all passed real HTTP and HTTPS round trips against `example.com` for
both static and shared libcurl. **Windows has since regressed** (found
2026-09-01, unrelated `ffmpeg` work) -- see the `curl` section below.

`ffmpeg` (added 2026-09-01) is `libcrtmedia`'s first demux/software-decode
port, outside the networking/TLS queue -- `shared-pass` on all three hosts.
See the `ffmpeg` section below.

mbedTLS's Windows DLL symbol-export hygiene and Windows `make install`
symlink/delete timing (both once open cross-cutting follow-ups from this
queue) are fixed -- see the mbedTLS/curl sections below and `HISTORY.md`'s
2026-08-15 entries.

`expat` and `freetype` (both added 2026-08-24) are graphics-stack dependencies
outside the networking/TLS queue. Expat supports the Wayland scanner build;
FreeType now feeds Skia's custom-directory font manager. The resulting
Wayland/Skia/FreeType input-and-text milestone is complete on all three hosts;
their package-specific status remains in the sections below.

## make

- Version: `android-toolchain-44fc4fe66a484b91844c302f03eaa8438e065d17`
- Recipe: `porting/recipes/make.json`
- Build system: `android_host_tool`
- Dependencies: none
- Status:
  - Linux: `manual-pass`
  - macOS: `manual-pass`
  - Windows: `manual-pass`
- Automated recipe tests: none

Android `toolchain/make` is built as a host tool through the CRT/rootfs
environment and then used by configure-style porting recipes. Windows has been
verified directly on aarch64 and x86_64 (`make.exe --version` and a wildcard
directory-reading check). Linux/macOS have been repeatedly exercised indirectly
because this `make` drives the real `configure && make && make install` flows
for the current port queue, but they stay `manual-pass` until standalone checks
are recorded.

Key issues already resolved include MinGW version macro assumptions,
`___chkstk_ms`/`__main` compiler ABI helpers, and target-arch propagation in
`tools/crt-port-build.py`.

## zlib

- Version: `1.3.1`
- Recipe: `porting/recipes/zlib.json`
- Build system: `configure`
- Dependencies: `make`
- Status:
  - Linux: `shared-pass`
  - macOS: `shared-pass`
  - Windows: `shared-pass`
- Automated recipe tests:
  - `roundtrip-static`
  - `roundtrip-shared`

zlib builds and installs both static and shared artifacts on all three hosts.
The recipe tests run real compress/decompress round trips against both build
shapes.

Important CRT/PAL work exposed by zlib included shared-library wrapper support,
Windows symlink support for SONAME-style aliases, Linux/macOS shared rpath
handling, and correct resolution of this project's own CRT shared libraries
rather than host libraries.

## libpng

- Version: `1.6.57`
- Recipe: `porting/recipes/libpng.json`
- Build system: `configure`
- Dependencies: `zlib`
- Status:
  - Linux: `shared-pass`
  - macOS: `shared-pass`
  - Windows: `shared-pass`
- Automated recipe tests:
  - `roundtrip-static`
  - `roundtrip-shared`

libpng builds and runs against zlib in `PORT_PREFIX` on all three hosts. The
recipe tests exercise real libpng create/write/destroy paths against both
static and shared builds.

The Windows port drove a large amount of general CRT/tooling work: GNU
Libtool/MinGW detection fixes, `malloc.h`, force-included libtool wrapper
compatibility, rootfs mksh/toybox execution, regex, `lseek()` on pipes,
`install-sh` script execution, `which`/`readlink`/`stat` applets, and Windows
symlink/readlink/lstat behavior. See `HISTORY.md` and the recipe notes for the
full trail.

## SQLite amalgamation

- Version: `3.53.4`
- Recipe: `porting/recipes/sqlite-amalgamation.json`
- Build system: `amalgamation`
- Dependencies: none
- Status:
  - Linux: `amalgamation-pass`
  - macOS: `amalgamation-pass`
  - Windows: `amalgamation-pass`
- Automated recipe tests: recipe-built smoke/link checks are handled by the
  amalgamation flow.

SQLite's amalgamated `sqlite3.c` builds without upstream source patching. The
recipe steers Windows away from native Windows headers and keeps macOS on the
portable Unix path rather than Darwin-specific VFS paths. Shared-library output
has also been exercised as part of the broader amalgamation build support, but
the conservative recipe status remains `amalgamation-pass`.

## bzip2

- Version: `1.0.8`
- Recipe: `porting/recipes/bzip2.json`
- Build system: `amalgamation`
- Dependencies: none
- Status:
  - Linux: `shared-pass`
  - macOS: `shared-pass`
  - Windows: `shared-pass`
- Automated recipe tests:
  - `roundtrip-static`
  - `roundtrip-shared`

bzip2 builds static and shared library artifacts on all three hosts. The tests
run real compression/decompression round trips against both variants.

This port is part of the completed static/shared verification queue and should
continue to be checked through `port-test-bzip2` or the aggregate
`port-test-recipes`.

## xz

- Version: `5.8.3`
- Recipe: `porting/recipes/xz.json`
- Build system: `configure`
- Dependencies: `make`
- Status:
  - Linux: `shared-pass`
  - macOS: `shared-pass`
  - Windows: `shared-pass`
- Automated recipe tests:
  - `roundtrip-static`
  - `roundtrip-shared`

xz/liblzma builds and runs on all three hosts. The tests perform a real
compress/decompress round trip at preset `9|EXTREME` with CRC64 against both
static and shared builds.

The Windows route includes a documented constructor/archive policy workaround.
Executable `.init_array`/`.fini_array` equivalents are covered by CRT tests, but
archive-contained constructor bracketing remains a known Windows limitation
outside this specific recipe.

## pcre2

- Version: `10.47`
- Recipe: `porting/recipes/pcre2.json`
- Build system: `configure`
- Dependencies: `make`
- Status:
  - Linux: `shared-pass`
  - macOS: `shared-pass`
  - Windows: `shared-pass`
- Automated recipe tests:
  - `match-static`
  - `match-shared`

pcre2 is scoped to the 8-bit code unit width for this tranche. The tests run a
real `pcre2_compile()`/`pcre2_match()` round trip with named capture groups
against both static and shared builds on all hosts.

Windows uses the usual CRT policy of steering upstream away from native
`windows.h` paths where the Bionic/POSIX path is the intended compatibility
surface.

## mbedTLS

- Version: `3.6.7`
- Recipe: `porting/recipes/mbedtls.json`
- Build system: `configure` recipe with `skip_configure`
- Dependencies: `make`
- Status:
  - Linux: `shared-pass`
  - macOS: `shared-pass`
  - Windows: `shared-pass`
- Automated recipe tests:
  - `crypto-static`
  - `crypto-shared`

mbedTLS builds static and shared crypto libraries on all three hosts. The tests
run a SHA-256 known-answer check plus an AES-128-CBC encrypt/decrypt round trip
against both static and shared builds.

Important follow-ups:

- Windows shared DLL export hygiene is fixed. The hand-written mbedTLS DLL
  build embeds this project's libc and, with no symbol-visibility control,
  used to re-export virtually all of it alongside its own real API -- this
  originally caused a real curl runtime bug when a stale embedded `read()`
  shadowed the current CRT implementation. Fixed with a
  `-Wl,--exclude-symbols` entry per real libc symbol (via a checked-in
  linker response file, `porting/recipes/mbedtls-windows-exclude-symbols.rsp`)
  applied to all three of mbedTLS's Windows DLL link recipes. Verified via
  `llvm-readobj --coff-exports`: zero libc symbols remain in any of the
  three DLLs' export tables. See `porting/recipes/mbedtls.json`'s own
  notes for the full trail, including two general `tools/crt-port-build.py`
  fixes found along the way.
  - **This `.rsp` file is a hand-generated snapshot, not something
    regenerated automatically at build time -- it drifts stale every time
    a new public libc symbol is added, and nothing catches that until some
    port's DLL link happens to pull in the newly-added symbol's
    translation unit.** Confirmed for real (2026-08-17): building mbedtls
    after this session added `sendmsg`/`recvmsg`/`link` (and several other
    batches) failed with `ld.lld: error: duplicate symbol: __crt_sys_sendmsg`
    (also `__crt_sys_recvmsg`/`__crt_sys_link`) -- `libc/src/arch/windows/
    common/syscall.c` compiles as a single translation unit, so pulling in
    any one of its symbols (mbedtls needs basic file I/O, unrelated to
    sockets) pulls in the whole `.obj`, including newer `__crt_sys_*`
    symbols this `.rsp` file's snapshot predated. Regenerated by diffing
    the checked-in list against a fresh `llvm-nm --defined-only -g` dump
    of `lib/c.lib` (filtered to valid plain-C identifiers, dropping
    compiler-generated string-literal/`.weak`/`.refptr` symbols): 917 ->
    947 entries, a strict superset (nothing present in the old list was
    missing from the new one, so no `--exclude-symbols` target was ever
    silently dropped). Re-verified the same way as the original fix
    (`llvm-readobj --coff-exports` shows zero libc symbols; both
    `crypto-static`/`crypto-shared` recipe tests pass). This maintenance
    obligation remains standing -- see `TODO.md`'s Notice and
    `porting/recipes/mbedtls.json`.
- macOS `.dylib` install-name handling was fixed by recipe patches adding
  `-install_name @rpath/$@`.
- Linux `getauxval()`/`<sys/auxv.h>` support was added generally after mbedTLS
  exposed the missing surface.

## curl

- Version: `8.21.0`
- Recipe: `porting/recipes/curl.json`
- Build system: `configure`
- Dependencies: `zlib`, `mbedtls`
- Status:
  - Linux: `shared-pass`
  - macOS: `shared-pass`
  - Windows: `shared-pass`
- Automated recipe tests:
  - `http-roundtrip-static`
  - `http-roundtrip-shared`

curl is the final port in the current queue and is `shared-pass` on all
three hosts. The tests perform real HTTP and HTTPS requests against
`example.com` through libcurl, zlib, mbedTLS, DNS, sockets, non-blocking fd
behavior, and the CRT sysroot. This is intentionally not a local
loopback-only test.

**Windows regression, found 2026-09-01 (unrelated work, verifying `ffmpeg`
on Windows), fixed the same day:** `http-roundtrip-static` started failing
at runtime with `curl_http_roundtrip_test: http://example.com/ failed: Out
of memory` on the very first (plain HTTP) request, even though basic TCP
connectivity to the test target worked -- confirmed NOT caused by
`porting/recipes/make.json`'s `HAVE_DOS_PATHS` cflag (real A/B test) and
NOT the previously-documented `pipe()`-misdetection "Out of memory" bug
this file's own notes below already describe (`config.log` confirmed
`pipe()` still detects as `yes`). Root-caused via real `lldb` debugging of
the live binary plus a standalone probe built against this project's own
CRT sysroot: four distinct, real, previously-invisible Windows PAL bugs in
`libc/src/arch/windows/common/syscall.c`, none curl-specific, each one
surfacing only once the prior fix let curl's logic run further than ever
before -- `eventfd()`/`sys/eventfd.h` (added after this port's last
Windows verification) falsely detected as usable by curl's link-only
configure probes, sending curl down a permanently-`ENOSYS`-stubbed wakeup
path; `SOCK_CLOEXEC` and (later) `MSG_NOSIGNAL` -- both declared
unconditionally on every host by this project's own portable
`<sys/socket.h>`, matching `eventfd()`'s own policy -- passed straight
through to Winsock's `socket()`/`send()`, which do not understand either
bit and fail the call outright; and `poll()`/`select()` unconditionally
reporting a socket "writable" with no real check at all, so curl's
non-blocking-connect completion check always lied and a subsequent
`send()` genuinely, persistently returned `WSAEWOULDBLOCK`. Fixed
generally (autoconf cache-variable priming for the first, `type`/`flags`
bit-stripping for the second and fourth, and a real
`winsock.select()`-backed writability check for the third) -- see
`porting/recipes/curl.json`'s own notes for the full per-bug trail.
Windows `ctest` stayed clean at 123/123 throughout.

The first curl tranche is scoped to HTTP/HTTPS and uses a project-owned CA
policy: no default CA bundle is baked in, so deployment consumers must provide
trust material explicitly.

General CRT/PAL work exposed by curl included:

- real `getaddrinfo()` DNS lookup for A records;
- `fcntl(F_SETFL, O_NONBLOCK)` behavior for Linux/macOS and then Windows
  pipes/sockets;
- socket macro/type/header gaps such as `AF_UNIX`, `IN6_IS_ADDR_*`, and
  `getsockopt()`;
- macOS mbedTLS install-name/rpath issues;
- Windows `/dev/urandom` backed by `RtlGenRandom()`;
- Windows non-blocking connect/send transient error mapping;
- Windows libtool wrapper shims for curl's generated helper executable;
- Windows `__crt_sys_open()`'s `O_CREAT` path gained the same
  delete-pending/handle-timing retry `__crt_sys_unlink()`/
  `__crt_sys_symlink()` already had (see the "Windows symlink/delete
  timing" note elsewhere in this file), found via a real `Error 5` on
  `install-pkgconfigDATA` during a from-scratch `port-rebuild-curl`.

The risk once inherited from mbedTLS is now fixed (see the mbedTLS section
above); a from-scratch `port-rebuild-curl`/`port-test-curl` against the
fixed mbedTLS confirms no regression, both statically and shared.

## libffi

- Version: `3.4.5`
- Recipe: `porting/recipes/libffi.json`
- Build system: `configure`
- Dependencies: `make`
- Status:
  - Linux: `shared-pass`
  - macOS: `shared-pass`
  - Windows: `shared-pass`
- Automated recipe tests:
  - `call-static`
  - `call-shared`
  - `repeat-call-static`
  - `repeat-call-shared`

libffi configures, builds, and installs static and shared artifacts on Linux,
macOS, and Windows. Its basic `ffi_call()` path works against both build shapes,
and Wayland's closure-based argument marshaling has run successfully in the
three-host `crtgfx-wayland-smoke` path. Windows shared-library output and
import-library use are also operational; the CRT implements the PE runtime
pseudo-relocation support needed by ordinary consumers of `libffi.dll.a`.

The optimized repeat-call failure was a consumer ABI mismatch, not a proven
libffi assembly defect. libffi requires `ffi_call()` return storage to be at
least register-sized, so both tests now use `ffi_arg` rather than `int`. On
macOS, the library was also built with `__APPLE__` while normal CRT consumers
hide that host macro; this changed the public `ffi_cif` size by omitting
Darwin's `aarch64_nfixedargs` field from the consumer. `ffi_prep_cif()` then
overwrote the next stack object. The recipe now adapts the generated installed
`ffitarget.h` to select that ABI field through `CRT_TARGET_OS_MACOS`, leaving
upstream source unchanged and preserving the normal Bionic-shaped compiler
surface.

Fresh real macOS arm64, Linux arm64, and Windows runs now pass all four
official tests, including `repeat-call-static` and `repeat-call-shared` at
`-O1`, so all three hosts are `shared-pass`. Linux keeps `__linux__`
consistently and Windows keeps `_WIN32` consistently, so neither needed the
Darwin generated-header adaptation. The former callee-saved-register diagnosis
and all related partial statuses are closed.

## expat

- Version: `2.8.3`
- Recipe: `porting/recipes/expat.json`
- Build system: `configure`
- Dependencies: `make`
- Status:
  - Linux: `shared-pass`
  - macOS: `shared-pass`
  - Windows: `shared-pass`
- Automated recipe tests:
  - `roundtrip-static`
  - `roundtrip-shared`

Added outside the networking/TLS queue above as a build dependency for the
current core Wayland external build: upstream `wayland-scanner` parses
protocol XML via expat. The tests run a real `XML_Parse()` round trip
(nested elements, an attribute, character data) against both static and
shared builds. In addition to its earlier indirect use by the completed
Wayland build, a fresh macOS arm64 `port-rebuild-expat` and
`port-test-expat` run on 2026-08-25 passed both official round trips. Expat is
therefore `shared-pass` on all three hosts.

Windows needed two real, expat-specific fixes plus one general Windows
infrastructure fix, all found and root-caused this session (2026-08-24):

- expat's own `configure.ac` classifies this recipe's `--build=@CRT_MINGW_
  TRIPLE@` host as `mingw*`, which unconditionally adds `lib/random_rand_s.c`
  to the build (an Automake `MINGW` conditional, not a `#ifdef _WIN32` branch
  this recipe's usual `-U_WIN32` CFLAGS trick can reach) -- that file calls a
  bare `rand_s()`, a Microsoft CRT extension this project's libc does not
  implement. Fixed with a small, documented compatibility shim
  (`porting/shims/win32/expat_rand_s_compat.h`) providing a real `rand_s()`
  in terms of this project's own already-working `getrandom()`.
- The same `mingw*` classification also excludes `lib/random_dev_urandom.c`
  from the build (upstream's own "MinGW has no /dev/urandom" assumption),
  so this recipe's base `--with-dev-urandom` (used on Linux/macOS) left an
  undefined-symbol link error on Windows; reversed to `--without-dev-urandom`
  there since `getrandom()` already wins expat's own runtime entropy
  priority chain ahead of dev-urandom regardless.
- A general Windows infrastructure bug in `tools/fetch_ports.py`: relative
  symlink targets from a POSIX tar archive (e.g. `porting/recipes/make.json`'s
  own `build-aux/config.guess -> ../gnulib/build-aux/config.guess`) resolve
  through `os.readlink()` but fail every real Win32 file API (`WinError 123`)
  when the target contains forward slashes -- fixed generally (not
  expat-specific) by normalizing tar-extracted symlink targets to backslashes
  on Windows right after extraction.

See `porting/recipes/expat.json`'s own notes for the full trail and
`HISTORY.md`'s 2026-08-24 entry.

## freetype

- Version: `2.14.3`
- Recipe: `porting/recipes/freetype.json`
- Build system: `configure`
- Dependencies: `make`
- Status:
  - Linux: `shared-pass`
  - macOS: `shared-pass`
  - Windows: `shared-pass`
- Automated recipe tests:
  - `glyph-rasterize-static`
  - `glyph-rasterize-shared`

Added as Phase 1 of the now-completed three-host CPU-raster/input/text
milestone: real text rendering needs a real font rasterizer, and Skia has
first-class, non-host support for FreeType
(`skia_use_freetype`/`SkFontMgr_custom_*` GN targets). The tests run a real `FT_New_Face()`/
`FT_Set_Pixel_Sizes()`/`FT_Load_Char(..., FT_LOAD_RENDER)` round trip
against a bundled real font (`libcrtgfx/assets/fonts/DejaVuSansMono.ttf`)
and check the rasterized `'A'` glyph bitmap has plausible non-zero
dimensions AND at least one genuinely inked pixel, against both static and
shared `libfreetype`.

Windows needed three real, general fixes, all found and root-caused this
session (2026-08-24):

- A genuine Windows-only GNU Make syntax collision: FreeType's own
  generated `builds/unix/unix-def.mk` unconditionally resolves `TOP_DIR`
  (and everything derived from it) to a real, drive-letter-bearing absolute
  path (`C:/Users/...`) via `$(shell cd $(TOP_DIR); pwd)` -- harmless on a
  real POSIX host, but at least 11 separate rule-declaration lines across
  FreeType's own hand-rolled `builds/*.mk` tree combine two such absolute
  expansions on one line, which GNU Make misparses as static-pattern-rule
  syntax and aborts on (`target pattern contains no '%'`). Fixed with three
  new, generic `tools/crt-port-build.py` recipe fields read from
  `target_overrides.<os>` (`configure_cwd`, `pre_configure_copy`,
  `post_configure_patch`) that bypass FreeType's own crash-prone recursive
  `$MAKE setup unix` wrapper dispatch and patch the one root-cause line to
  a relative no-op self-assignment -- Linux/macOS keep using upstream's own
  wrapper unmodified.
- A real, general stack-overflow bug in this project's own ported
  `make.exe`: it silently segfaults (zero output) parsing FreeType's own
  unusually large Makefile tree under `ld.lld`'s plain 1 MiB default PE
  stack reserve. Fixed generally in `porting/recipes/make.json`'s own
  Windows `LDFLAGS` (`-Wl,--stack,16777216`, a 16 MiB reserve), not in this
  recipe.
- libtool mis-wrapping the Windows resource compiler (`llvm-rc.exe`) for
  FreeType's own hand-rolled, non-Automake `ftver.rc` VERSIONINFO-resource
  rule (`Exactly one input file should be provided`). Worked around
  narrowly, in this recipe only (`target_overrides.windows.env.RC=""`),
  since `ftver.rc` is purely cosmetic Windows DLL/EXE metadata with no
  effect on font rasterization -- an empty `$RC` is a real, upstream-
  supported "no resource compiler" code path FreeType's own
  `ifneq ($(RC),)` guard already handles.

Also fixed a general, Windows-only transient file-lock bug in
`tools/fetch_ports.py`'s own archive-extraction rename (`WinError 5`,
consistent with a real-time antivirus/indexer scan racing the rename of a
freshly-extracted directory), with a 5-attempt retry-with-backoff wrapper
matching `tools/crt-port-build.py`'s own established `remove_tree()`
precedent for the same class of problem.

`--with-pic` added later the same day, found wiring Skia's real
`SkFontMgr_custom_directory` in (see `HISTORY.md`'s matching entry):
linking `libcrtgfx.so` against the static `libfreetype.a` failed
(`recompile with -fPIC`) since libtool's own default only builds the
*shared* object with PIC code. Applied unconditionally, not Linux-only,
since this project intends to link FreeType into shared `libcrtgfx`
builds on every host eventually.

See `porting/recipes/freetype.json`'s own notes for the full trail and
`HISTORY.md`'s 2026-08-24 entry.

## ffmpeg

- Version: `8.1.2`
- Recipe: `porting/recipes/ffmpeg.json`
- Build system: `configure`
- Dependencies: `make`
- Status:
  - Linux: `shared-pass`
  - macOS: `shared-pass`
  - Windows: `shared-pass`
- Automated recipe test: `libcrtmedia`'s own `crtmedia_demux_test` (not a
  `porting/recipes/*.json` `tests[]` entry -- gated behind the
  `CRTMEDIA_ENABLE_FFMPEG` CMake option instead, see `libcrtmedia/
  CMakeLists.txt`)

First FFmpeg port for `libcrtmedia`'s demux/software-decode bridge
(`crtmedia/demux.h`/`audio.h`, `HISTORY.md`'s 2026-09-01 entries).
Deliberately narrow first pass: local `file`-protocol demux only, one
container (MOV/MP4/M4A), one video codec (H.264), three audio codecs
(AAC/MP3/PCM), LGPL-only, no assembly-optimized codec paths. Verified end
to end on all three hosts: `crtmedia_demux_test` demuxes+decodes a real,
tiny, project-authored WAV fixture and checks the exact known sample
count, not just that the library links.

Windows needed five real, Windows-specific fixes beyond the ones common to
every host (`--arch=`/`--target-os=`/`--host-cc=`/the `-U_WIN32` family/
`HAVE_ALIGNED_MALLOC`): `porting/recipes/make.json`'s own Windows
`make.exe` needed `-DHAVE_DOS_PATHS=1` (a real GNU Make self-re-exec bug,
see that recipe's own notes), two more real `@ROOT@`/`@AR@` token-
substitution gaps in `tools/crt-port-build.py` (the same backslash-eaten-
by-mksh class of bug as `TMPDIR`'s own fix), a new `porting/shims/win32/
install-sh` (this project's Windows rootfs has no real `install(1)`
binary), and a `post_configure_patch` neutralizing FFmpeg's own
unconditional `doc/examples` install (out of scope, would need `install(1)`
support for zero benefit). See `porting/recipes/ffmpeg.json`'s own notes
for the full per-fix trail and `HISTORY.md`'s 2026-09-01 entries for the
dated narrative.

`--disable-zlib` is set explicitly (base `configure_args`, all hosts):
FFmpeg's own `./configure` auto-detects zlib and silently links
`libavformat`'s optional compressed-metadata support against it whenever a
`zlib` port build happens to already exist in the shared install prefix --
undefined, build-order-dependent behavior for a feature never in this
pass's scope. Made explicit and deterministic instead.
