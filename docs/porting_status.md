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
Windows all pass real HTTP and HTTPS round trips against `example.com` for both
static and shared libcurl.

mbedTLS's Windows DLL symbol-export hygiene and Windows `make install`
symlink/delete timing (both once open cross-cutting follow-ups from this
queue) are fixed -- see the mbedTLS/curl sections below and `HISTORY.md`'s
2026-08-15 entries.

Still open:

- libffi still has a correctness issue around repeated `ffi_call()` usage at
  optimized levels on some paths.

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
- Dependencies: none
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
- Dependencies: none
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
- Dependencies: none
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
  used to re-export virtually all of it (917 real libc symbol names,
  confirmed via `llvm-nm`) alongside its own real API -- this originally
  caused a real curl runtime bug when a stale embedded `read()` shadowed
  the current CRT implementation. Fixed with a `-Wl,--exclude-symbols`
  entry per real libc symbol (via a checked-in linker response file,
  `porting/recipes/mbedtls-windows-exclude-symbols.rsp`) applied to all
  three of mbedTLS's Windows DLL link recipes. Verified via
  `llvm-readobj --coff-exports`: zero libc symbols remain in any of the
  three DLLs' export tables. See `porting/recipes/mbedtls.json`'s own
  notes for the full trail, including two general `tools/crt-port-build.py`
  fixes found along the way.
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

curl is the final port in the current queue and is `shared-pass` on all three
hosts. The tests perform real HTTP and HTTPS requests against `example.com`
through libcurl, zlib, mbedTLS, DNS, sockets, non-blocking fd behavior, and the
CRT sysroot. This is intentionally not a local loopback-only test.

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
  - Linux: `partial`
  - macOS: `configure-pass`
  - Windows: `partial`
- Automated recipe tests:
  - `call-static`
  - `call-shared`

libffi configures, builds, and installs useful artifacts, and its basic
`ffi_call()` and closure paths work in isolation. Windows shared-library output
and import-library use are now possible; the CRT implements the PE runtime
pseudo-relocation support needed by ordinary consumers of `libffi.dll.a`.

Status stays conservative because a correctness bug remains: calling
`ffi_call()` and then a further libffi call in the same process can corrupt a
callee-saved register at optimized levels on the affected paths. This needs a
focused debugger pass before libffi can be promoted to `shared-pass`.
