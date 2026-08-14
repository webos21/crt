# Project Status

A single, lightweight snapshot of "does it currently pass, and what's known
to still be broken." This is deliberately not a release process (no tags,
versions, or CHANGELOG yet -- `LICENSE.md` itself only landed recently, and
there are no external consumers) -- just a page that stays honest about
current state so the next session doesn't have to re-derive it from
`HISTORY.md`. Update this alongside any change that shifts what "passing"
means; if this page and `HISTORY.md`/`TODO.md` disagree, the dated entries
in those two win.

## What "passing" currently means

- **CI**: `.github/workflows/ci.yml`, a 5-leg GitHub Actions matrix (macOS
  aarch64, Linux arm64/amd64, Windows arm64/x64), each running this
  project's own `cmake --workflow <os>-host-ninja-debug` preset (configure +
  build + `ctest`) on every push. All 5 legs green as of
  [run 31759586497](https://github.com/webos21/crt/actions/runs/31759586497)
  (2026-08-14, the pcre2 macOS-confirmed commit). Before the matrix
  existed, Linux validation had been almost entirely
  manual, on real aarch64 hardware -- x86_64 Linux had never actually been
  built until this matrix existed, and immediately surfaced two real,
  previously-invisible bugs (see `HISTORY.md`'s 2026-08-11 entries). CI's
  own `cmake --workflow` step does not run `port-test-recipes` (a
  separate, heavier target that fetches and builds third-party sources)
  -- that's verified locally/per-host instead, see below.
- **`ctest`**: 83 registered tests on Windows and 77 on macOS in the
  latest local run (count is slightly
  OS-dependent -- a few targets, like `windows_export_hygiene_test`, only
  exist on their own OS), all passing locally on Windows as of this
  session's `.init_array`/pseudo-relocation work and locally on macOS as
  of the curl/host-libc audit pass; CI is the source of truth for Linux
  counts. Run locally via
  `cmake --workflow --preset <os>-host-ninja-debug` or
  `ctest --test-dir out/<preset>`.
- **Ports**: see `docs/porting_status.md` for the full per-library,
  per-host table. `zlib`/`libpng`/`sqlite-amalgamation`/`bzip2` are at
  `shared-pass` on all three OSes. `xz` (liblzma) is now
  `shared-pass` on Linux, macOS, and Windows: a real, full
  compress/decompress round trip at preset 9|EXTREME with CRC64 passes
  against both static and shared builds on every verified OS. `libffi`'s
  own `port-test-libffi` now passes both its static and shared variants
  on Windows too (`ffi_call()` round trip, `result=42`) -- the shared
  variant's `_pei386_runtime_relocator` gap (Windows/PE "runtime pseudo
  relocation" support, a new PAL feature) is fixed; see `HISTORY.md`'s
  2026-08-12 entry for the full writeup and `tests/windows_pseudo_reloc_
  dll.c`/`consumer.c` for its own permanent `ctest` regression coverage.
  `pcre2` is now `shared-pass` on
  all three OSes: a real `pcre2_compile()`/`pcre2_match()` round trip
  with three named capture groups passes on every host for both static
  and shared builds, macOS confirmed by the user. All six of those ports
  now have an official,
  recipe-declared `port-test-<name>` CMake target (aggregated as
  `port-test-recipes`); re-run directly on Windows this session and
  confirmed green across the board. `libffi` overall stays `partial` only
  because of its unrelated, pre-existing `-O1`/`-O2`
  `ffi_call()`-repeat-call bug. `mbedtls` (the next port in the queue
  after `pcre2`, crypto library only) is now `shared-pass` on all three
  OSes: a real SHA-256 known-answer check plus an AES-128-CBC
  encrypt/decrypt round trip passes on every host against both the
  static and shared build, macOS confirmed by the user. See `HISTORY.md`'s
  2026-08-14 entry and `porting/recipes/mbedtls.json`'s own notes for
  the full trail (three new, generalizable `tools/crt-port-build.py`
  extensions -- `build.skip_configure`, a base `build.install_args`
  field, and a per-OS `build_make_args` field for a `make` variable
  that must reach the build step only, never `make install` -- plus
  several recipe patches, including disabling `MBEDTLS_NET_C` since
  this PAL's sockets surface doesn't yet cover everything mbedtls's own
  networking helper needs; deferred to `curl`, the next and last port
  in this queue). `curl` (8.21.0) is now `shared-pass` on Linux and
  macOS (Windows `configure-blocked`): a real HTTP GET
  and HTTPS GET (real TLS handshake via the mbedTLS backend) round trip
  against `example.com` both pass on Linux and macOS, for both static and
  shared libcurl. Linux uses curl's own real default configuration; macOS
  declares `--disable-ipv6` and `--disable-threaded-resolver` for this
  tranche, keeping the build on this CRT's currently-verified IPv4
  synchronous resolver path rather than leaking Darwin SystemConfiguration
  headers or depending on curl's async resolver worker-pool behavior.
  curl was the
  first port in this queue to reach a real internet hostname over the
  network, and it surfaced two real, general, previously-invisible libc
  bugs, not curl-specific ones: `getaddrinfo()` had no real DNS
  resolution at all (fixed with a real, minimal synchronous DNS client
  added to `libc/src/socket.c`), and `fcntl(fd, F_SETFL, O_NONBLOCK)`
  was a pure no-op on Linux/macOS (the actual cause of an indefinite
  `curl_easy_perform()` hang, root-caused by direct process inspection
  and temporary instrumentation in curl's own source -- fixed by
  forwarding `F_GETFL`/`F_SETFL` to the real `fcntl(2)` syscall). A real
  Windows build attempt found two more distinct bugs: curl's own
  `AC_EGREP_CPP`-based socket probe only reads `CPPFLAGS`, not `CFLAGS`
  (fixed); and, once configure passed, mbedtls's own Windows `.dll`
  build turned out to statically embed this project's libc with no
  symbol-visibility control and re-export its internal symbols, colliding
  with this project's own `c.lib` once curl links against both (not
  fixed this session -- not a curl bug, see `TODO.md`). A real macOS
  build attempt found one more, in `tools/crt-port-build.py` itself:
  curl's own configure-time "runtime libs availability" probe (compiles
  and *runs* a test program against mbedtls's shared libs) failed
  because mbedtls's `.dylib` files have no `-install_name` set, so dyld
  can't resolve them via `LC_RPATH`. First attempt (`make_env()` also
  setting `DYLD_LIBRARY_PATH`/`LD_LIBRARY_PATH` as a runtime-loader
  fallback for every subprocess it spawns, matching what
  `run_port_tests()` already did for test binaries) turned out
  insufficient on real macOS hardware: the same error recurred, because
  configure execs through `/bin/sh`, and macOS strips `DYLD_`-prefixed
  environment variables across an exec of any SIP-protected system
  binary -- so the env var never survives into configure's own child
  probe. The real, durable fix landed in `porting/recipes/mbedtls.json`
  instead: three new `library/Makefile` patches add
  `-install_name @rpath/$@` to the APPLE_BUILD `-dynamiclib` recipes for
  all three `.dylib`s, baking the correct load-command path into the
  library itself (immune to any environment-stripping), resolved at
  runtime by the consumer's own `-Wl,-rpath` LDFLAGS `make_env()`
  already sets unconditionally. Verified on macOS by rebuilding curl and
  running both static and shared HTTP/HTTPS round trips against
  `example.com`. A follow-up macOS audit rebuilt the installed port
  dylibs and confirmed they depend on this project's `@rpath/libc.dylib`;
  suspicious libc/POSIX references no longer bind directly from
  `libSystem`, aside from libSystem itself remaining the intended Darwin
  PAL/backend boundary. See `HISTORY.md`'s 2026-08-14 entry and both
  `porting/recipes/curl.json`'s and `porting/recipes/mbedtls.json`'s own
  notes for the full trail. **Windows**: a real rebuild attempt got
  past the previously-documented mbedtls-DLL duplicate-symbol blocker
  (`libcurl.la`/`libcurlu.la` now link cleanly; not deliberately fixed,
  so still tracked as open/unexplained in `TODO.md` rather than closed)
  and surfaced a different, real, now-fixed bug instead: curl's own CLI
  tool failed to link with `setmode`/`_spawnv`/`_P_WAIT` undeclared in
  GNU Libtool's own generated wrapper source -- the same bug class
  already fixed for libpng via
  `porting/shims/win32/libtool_wrapper_compat.h`, but curl.json was
  never wired up to use that shim. Fixed by wiring in the missing
  `force_include` plus a new shim alias (`#define setmode _setmode`)
  for a variant libpng never hit (curl also undefines `__MINGW32__`,
  so the wrapper's own rename block never fires and calls the bare,
  un-prefixed name directly). **Update, verified on real Windows
  hardware**: that fix, plus four more real bugs found chasing it
  through to an actual network round trip -- a second, worse shim
  header-footprint collision that silently mis-detected
  `pipe()`/`realpath()`/`sched_yield()` as absent via curl's own
  generic autoconf function probes (the `pipe()` one is what actually
  caused an `Out of memory` failure, curl's wakeup-pipe mechanism
  believing it had no `pipe()`); missing `-U_WIN32` cflags on the test
  programs themselves (`fatal error: 'winsock2.h' file not found`);
  Windows's `fcntl(F_SETFL, O_NONBLOCK)` finally implemented for real
  (`SetNamedPipeHandleState`/`ioctlsocket(FIONBIO)`, fixing the exact
  same indefinite-hang bug already fixed for Linux/macOS earlier in
  this pass, just not reachable on Windows until `pipe()` detection was
  fixed); and a real Winsock `WSAENOTCONN`-right-after-a-successful-
  non-blocking-`connect()` race (reinterpreted as `EAGAIN` for
  non-blocking sockets, confirmed transient with a standalone probe --
  a bare retry after ~200ms succeeds with no further connect()/select()
  calls). Result: **curl's HTTP round trip now passes end to end on
  Windows for the first time ever** (`curl_easy_perform()` against
  `http://example.com/` returns a real `200 OK`). **HTTPS does not
  work yet**: a new, distinct, NOT YET ROOT-CAUSED crash
  (`STATUS_ACCESS_VIOLATION`, reproduced twice) right after mbedTLS
  begins its handshake. Windows status: `partial`. See
  `porting/recipes/curl.json`'s own notes for the full trail.

## Known gaps

- **Windows static-archive constructor limitation**: executable
  `.init_array`/`.fini_array` and PE/Mach-O equivalents are fixed and
  covered by `tests/init_array_test.c`, but `lld-link` still does not
  reliably bracket constructor records contributed by a third-party
  static archive the way GNU ld's default ELF script does. xz routes
  around this with a documented recipe patch; a future Windows port that
  relies on archive-contained constructors may need a similar policy.
- **libffi**: `ffi_call()` alone and closures alone each work correctly in
  isolation, but calling `ffi_call()` and then any further libffi call in
  the same process reliably segfaults when the caller is compiled at
  `-O1`/`-O2` (never `-O0`). Root-caused to a callee-saved GPR getting
  corrupted somewhere in the `ffi_call()`/`ffi_call_SYSV` chain on aarch64;
  not yet isolated to an exact instruction, and not re-tested for an
  x86_64 analogue. See `porting/recipes/libffi.json`'s notes.
- **Windows `make install` symlink races**: an intermittent
  `ln: ... File exists` on libtool-generated header/lib alias symlinks when
  rebuilding a port whose install directory already has a valid symlink
  from a prior run. Looks like same-session Windows delete-pending/handle-
  timing noise (an isolated minimal repro never reproduced it), not a real
  toybox/CRT `rm`-on-symlink bug, but not confirmed from a genuinely cold
  `out/` directory yet. See `TODO.md`.
- **Windows `make -jN` regression coverage**: the fd_snapshot/`fstat()`
  pipe-content bug that broke parallel `make` on Windows is fixed and
  stress-tested (libpng scale, `-j 12`), but has no permanent regression
  test yet. See `TODO.md`.
- **Windows `fcntl(fd, F_SETFL, O_NONBLOCK)` is still a no-op**: fixed
  for real on Linux/macOS (forwards to the real `fcntl(2)` syscall) as
  part of curl's own port, but Windows's `fcntl()` backend has no
  unified syscall to forward to and keeps its prior (also broken,
  non-regressing) no-op behavior for now -- a real fix needs per-fd-type
  handling (winsock's `ioctlsocket(FIONBIO)` for sockets, overlapped I/O
  for anonymous pipes). Documented as a TODO comment directly in
  `libc/src/fd.c`; not yet hit by a real Windows port build.
- **DNS resolver is deliberately minimal**: `getaddrinfo()` now does a
  real DNS lookup (added for curl, see `HISTORY.md`'s 2026-08-14
  entry), but only a single synchronous UDP query for an A (IPv4)
  record -- no AAAA/IPv6, no TCP fallback for truncated responses, no
  search-domain suffixes, no caching. Sufficient for curl's own basic
  HTTP/HTTPS needs; would need to grow if a future port needs more.

## Next

- Porting matrix expansion: `bzip2`, `xz`, `pcre2`, `mbedtls`, and
  `curl` are `shared-pass` on Linux/macOS where verified; Windows is
  still blocked only on curl's shared link against mbedtls's current DLL
  export surface. Fixing the mbedtls DLL export/static-dependency policy
  closes out this queue (`openssl` stays held back until needed).
- Broader POSIX/rootfs surface hardening beyond what each port's own build
  happens to exercise.
- C++ runtime phase 2 and an ELF loader/dynamic-linker prototype: not
  started, no committed design yet.
