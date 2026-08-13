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
  [run 31602222349](https://github.com/webos21/crt/actions/runs/31602222349)
  (2026-08-12, the `port-test-recipes` commit). Before the matrix
  existed, Linux validation had been almost entirely
  manual, on real aarch64 hardware -- x86_64 Linux had never actually been
  built until this matrix existed, and immediately surfaced two real,
  previously-invisible bugs (see `HISTORY.md`'s 2026-08-11 entries). CI's
  own `cmake --workflow` step does not run `port-test-recipes` (a
  separate, heavier target that fetches and builds third-party sources)
  -- that's verified locally/per-host instead, see below.
- **`ctest`**: 83 registered tests on Windows (count is slightly
  OS-dependent -- a few targets, like `windows_export_hygiene_test`, only
  exist on their own OS), all passing locally on Windows as of this
  session's `.init_array`/pseudo-relocation work; CI is the source of
  truth for Linux/macOS counts. Run locally via
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
  All five of those ports now have an official, recipe-declared
  `port-test-<name>` CMake target (aggregated as `port-test-recipes`);
  re-run directly on Windows this session and confirmed green across
  the board, both static and shared. `libffi` overall stays `partial`
  only because of its unrelated, pre-existing `-O1`/`-O2`
  `ffi_call()`-repeat-call bug.

## Known gaps

- **`.init_array`/`.fini_array` (ELF constructor/destructor) support**:
  fixed and verified on Linux and Windows (`ctest`) and macOS (confirmed
  directly on real macOS hardware: `tests/init_array_test.c` prints
  `init_array_test: ok`, user-run). See `TODO.md`'s dedicated entry and
  `HISTORY.md` for the full writeup, including three real bugs the
  Windows work surfaced along the way: (1) this project's own CMake-
  native Windows builds turned out to use a second, entirely separate
  constructor/destructor convention (`.CRT$XCU`/`.CRT$XTX`, MSVC ABI)
  alongside the GNU one (`.ctors`/`.dtors`) `tools/crt-cc`'s port builds
  use; (2) fixing that exposed a latent, pre-existing bug in the Windows
  startup self-relaunch's parent-process exit path
  (`fork_capable_relaunch.c`, now `_exit()` instead of `exit()`); (3) a
  separate, deeper limitation specific to linking third-party static
  archives (like `liblzma.a`) on Windows -- `lld-link` does not reliably
  merge multiple archive-derived plain `.ctors`/`.dtors` contributions
  into one contiguous region the way GNU ld's default script guarantees,
  with no COFF/PE equivalent mechanism. Routed around for xz via a
  recipe patch forcing liblzma's own portable non-constructor fallback
  path instead (see `porting/recipes/xz.json`); any *other* future port
  whose own static-archive code relies on `__attribute__((constructor))`
  on Windows would need the same kind of targeted fix. A permanent
  regression test, `tests/init_array_test.c`, now guards the general
  mechanism on every OS's `ctest` run going forward.
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

## Next

- Porting matrix expansion: `bzip2` and `xz` are done on
  Linux/macOS/Windows (`shared-pass`). The current next queue is
  `pcre2` -> `mbedtls` -> `curl`; see `TODO.md`'s "in progressing" section
  for the current queue and order.
- Broader POSIX/rootfs surface hardening beyond what each port's own build
  happens to exercise.
- C++ runtime phase 2 and an ELF loader/dynamic-linker prototype: not
  started, no committed design yet.
