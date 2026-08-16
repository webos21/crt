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
- **`ctest`**: 88 registered tests on Windows and 77 on macOS in the
  latest local run (count is slightly
  OS-dependent -- a few targets, like `windows_export_hygiene_test`, only
  exist on their own OS), all passing locally on Windows (88/88, most
  recently confirmed after the mbedtls Windows DLL symbol-hygiene fix and
  the two new Windows process/symlink regression tests) and
  locally on macOS as
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
  in this queue). `curl` (8.21.0) is now **`shared-pass` on all three
  OSes**, closing out this whole porting queue (`bzip2` -> `xz` ->
  `pcre2` -> `mbedtls` -> `curl`; `openssl` stays deliberately held
  back). A real HTTP GET and HTTPS GET (real TLS handshake via the
  mbedTLS backend) round trip against `example.com` passes on Linux,
  macOS, and Windows, for both static and shared libcurl, verified
  directly on real hardware for all three hosts. Getting there
  surfaced a long chain of real, general, previously-invisible PAL
  bugs across the whole session -- among the most notable: `getaddrinfo()`
  had no real DNS resolution at all (a minimal synchronous DNS client
  added to `libc/src/socket.c`); `fcntl(fd, F_SETFL, O_NONBLOCK)` was a
  pure no-op on every OS (curl's own internal wakeup-pipe mechanism
  needs it for real -- now forwards to the real syscall on Linux/macOS
  and implemented for real on Windows via `SetNamedPipeHandleState`/
  `ioctlsocket(FIONBIO)`); mbedtls's own macOS `.dylib` files had no
  `-install_name` set, breaking dyld resolution; and, on Windows,
  mbedTLS's portable entropy source had no working `/dev/urandom` to
  read from at all, crashing the TLS handshake with a null
  function-pointer call inside its RNG -- root-caused with a real
  `lldb` backtrace and fixed by implementing a real `/dev/urandom`
  device backed by `RtlGenRandom()`. One real, general risk was found
  and, at the time, left open, not curl-specific: mbedtls's own Windows
  `.dll` build re-exported this project's entire libc with no
  symbol-visibility control, which could silently shadow real libc
  fixes for any consumer that also links mbedtls's DLL until mbedtls
  itself is rebuilt too. **This is now fixed** (see "Known gaps" below
  and `porting/recipes/mbedtls.json`'s own notes) -- confirmed with a
  from-scratch `port-rebuild-curl`/`port-test-curl` against the fixed
  mbedtls, which also surfaced and fixed one more independent Windows
  delete-pending-race bug in `__crt_sys_open()`. See `HISTORY.md`'s
  dated entries and `porting/recipes/curl.json`'s own notes (a long,
  blow-by-blow trail) for the full writeup.

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
  `-O1`/`-O2` (never `-O0`) -- **on aarch64 Windows only**, root-caused to
  a callee-saved GPR getting corrupted somewhere in the
  `ffi_call()`/`ffi_call_SYSV` chain, not yet isolated to an exact
  instruction. x86_64 Windows is now confirmed clean (tested for the
  first time, see `HISTORY.md`'s 2026-08-15 entry), narrowing this to an
  aarch64-specific issue. A permanent regression now exists
  (`porting/tests/libffi_repeat_call_test.c`); next step is a real `lldb`
  session on aarch64 Windows hardware. See `porting/recipes/libffi.json`'s
  notes.
- **DNS resolver is deliberately minimal**: `getaddrinfo()` now does a
  real DNS lookup (added for curl, see `HISTORY.md`'s 2026-08-14
  entry), but only a single synchronous UDP query for an A (IPv4)
  record -- no AAAA/IPv6, no TCP fallback for truncated responses, no
  search-domain suffixes, no caching. Sufficient for curl's own basic
  HTTP/HTTPS needs; would need to grow if a future port needs more.

## Next

- Porting matrix expansion through curl is **done**: `bzip2`, `xz`, `pcre2`,
  `mbedtls`, and `curl` are all `shared-pass` on Linux, macOS, and Windows
  (`openssl` stays deliberately held back until something needs it). The
  real, general risk once open from this queue -- mbedtls's Windows DLL
  symbol-export hygiene -- is fixed; see `HISTORY.md`'s 2026-08-15 entry.
- Before starting the next upper-runtime phase, reduce the remaining
  libc/PAL planned work in `TODO.md`: libffi correctness, rootfs virtual
  files/devices, DNS resolver growth, console/job-control policy, and
  toybox applet expansion only where the Bionic-compatible backing
  surface exists. The mksh subshell status quirk is fixed -- see
  `HISTORY.md`'s 2026-08-16 entry.
- The next product-level target is documented in `docs/runtime_roadmap.md`:
  an Electron-class rebuilt runtime made of `libcrtgfx` (Skia + Wayland-style
  compositor boundary + Chromium Ozone path), `libcrtmedia` (FFmpeg/codecs/
  audio/video), and `libcrtjs` (QuickJS first, V8 later).
- C++ runtime phase 2 and an ELF loader/dynamic-linker prototype remain
  separate lower-layer tracks needed before the browser-class target can become
  realistic.
