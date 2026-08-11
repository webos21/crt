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
  [run 31464737310](https://github.com/webos21/crt/actions/runs/31464737310)
  (2026-08-11). Before this, Linux validation had been almost entirely
  manual, on real aarch64 hardware -- x86_64 Linux had never actually been
  built until this matrix existed, and immediately surfaced two real,
  previously-invisible bugs (see `HISTORY.md`'s 2026-08-11 entries).
- **`ctest`**: 74 registered tests, all passing on every CI leg. Run locally
  via `cmake --workflow --preset <os>-host-ninja-debug` or
  `ctest --test-dir out/<preset>`.
- **Ports**: see `docs/porting_status.md` for the full per-library,
  per-host table. `zlib`/`libpng`/`sqlite-amalgamation`/`bzip2` are at
  `shared-pass` on Linux and Windows (macOS pending on all four, no
  macOS hardware this session). `libffi` and `xz` (liblzma) are each
  `partial`: `libffi` builds and works except for a known `-O1`/`-O2`
  `ffi_call()`-repeat-call bug (see below); `xz` builds and works for
  small inputs but hangs on realistic-sized ones (see below).

## Known gaps

- **`.init_array`/`.fini_array` (ELF constructor/destructor) support**:
  fixed for Linux, still missing on Windows/macOS. See `TODO.md`'s
  dedicated entry -- this project's `crt1` startup never ran
  `__attribute__((constructor))` functions (also what runs C++ global
  object constructors) for the executable entry point on any OS, until
  found and fixed for Linux while porting xz/liblzma.
- **xz (liblzma)**: builds and installs cleanly on Linux, and small
  buffer_encode/buffer_decode calls work correctly, but encoding a
  larger buffer (big enough to reach liblzma's real LZ match-finder
  initialization) hangs -- confirmed spinning inside this project's own
  `malloc()` lock-wait loop with the lock's raw value read back as
  garbage while hung, despite the process being confirmed genuinely
  single-threaded at that moment. A standalone malloc-only reproduction
  of the same allocation sizes does not reproduce it, so the corruption
  is specific to liblzma's real code path, not a general `malloc.c` bug
  on its own. Not yet root-caused; see `porting/recipes/xz.json`.
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

## Not yet started

- Porting matrix expansion: `bzip2` is done on Linux/Windows (macOS
  pending), `xz` -> `pcre2` -> `mbedtls` -> `curl` not started -- see
  `TODO.md`'s "in progressing" section for the current queue and order.
- Broader POSIX/rootfs surface hardening beyond what each port's own build
  happens to exercise.
- C++ runtime phase 2 and an ELF loader/dynamic-linker prototype: not
  started, no committed design yet.
