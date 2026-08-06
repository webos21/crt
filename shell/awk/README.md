# awk

This directory contains an import of [onetrueawk/awk](https://github.com/onetrueawk/awk)
(Brian Kernighan's reference AWK -- the same lineage NetBSD and many BSDs ship
as their actual system `awk`), used as this project's own `/system/bin/awk`.

```sh
/system/bin/awk 'BEGIN { print "ok" }'
```

## Why This Exists

Bare Windows has no `awk` at all, and most autoconf-generated `configure`
scripts require one -- the exact same gap that motivated building `mksh` and
`toybox` from source against this project's own CRT in the first place,
rather than depending on whatever (if anything) happens to already be
installed on a given Windows machine. Linux and macOS need no such workaround
(the OS itself always provides a real `awk`); this import specifically closes
the Windows gap, and gets built identically on all three hosts for
consistency.

Two alternatives were considered and rejected:

- **toybox's own `toys/pending/awk.c`.** Already vendored under
  `shell/toybox/src/toys/pending/`, but disabled by default even in Bionic's
  own upstream toybox config (`CFG_AWK 0`) -- a real signal of incompleteness,
  not just an oversight in this project's own curated config.
- **A host-installed `awk`** (e.g. Git for Windows' bundled `gawk.exe`).
  Rejected: an uncontrolled, per-developer-machine dependency linked against a
  different runtime (MSYS), which breaks the same reproducibility/self-
  containment principle that justified building `mksh`/`toybox` against this
  project's own CRT rather than borrowing whatever the host happens to have.

## Layout

```text
shell/awk/                imported repo metadata (LICENSE, import_manifest.json, ...)
shell/awk/src/             imported onetrueawk C source, plus two vendored
                            generated files (see below)
```

## Generated Files

Upstream's own build (`makefile`) generates two files that this project
vendors as pristine, checked-in output rather than regenerating at build
time -- matching how `shell/toybox/crt/generated/*.h` are already vendored
rather than regenerated via toybox's Kconfig at every build:

- `src/awkgram.tab.c` / `src/awkgram.tab.h` -- from `awkgram.y` via
  `bison -d -b awkgram` (this project's CMake build otherwise has no
  bison/yacc dependency at all; only regenerate if `awkgram.y` itself
  changes).
- `src/proctab.c` -- from `src/maketab.c`, compiled and run as a native host
  tool (this project's own `crt-cc`) against `awkgram.tab.h`:
  `./maketab awkgram.tab.h > proctab.c`. `maketab.c` itself stays in `src/`
  for reference/reproducibility but is not part of the `crt_awk` build --
  only its one-time output is compiled.

See `import_manifest.json` for the exact upstream commit and generation tool
versions.

## Project-Owned Source Adjustment

`src/parse.c`'s `ptoi()`/`itonp()` helpers smuggle a small `int` through a
bison-stack `Node*`-typed slot (never a real pointer dereferenced as one --
see upstream's own "swearing that p fits" comment at the site) by casting
through `long`. On Windows LLP64, `long` is 32 bits while pointers are 64,
which truncates real round trips through these functions. Changed to
`intptr_t`, which preserves the exact same behavior on every platform
(including the LP64 systems this trick was originally written for) while
also being correct on LLP64. See the inline comment at the change site.

## CRT/libm Gaps Found By First Compile

None of these are awk-specific; all were filled in CRT/PAL/libm rather than
by patching awk source:

- `atan2()` / `atan()` -- `libm/src/freebsd/e_atan2.c`, `s_atan.c` (FreeBSD
  msun, a separate upstream from `libc/src/regex/`'s NetBSD regex import).
- `system()` -- `libc/src/process.c`.
- `rand()` / `srand()` / `random()` / `srandom()` -- `libc/src/random.c`.
- `SIGFPE` `si_code` `FPE_*` constants -- `include/signal.h`.
- `<stdnoreturn.h>` -- missing entirely before this.
- `popen()` / `pclose()` -- `libc/src/stdio.c` (needed for awk's `"cmd" |
  getline` / `print | "cmd"`).

## Bug Found By First Real Use

Running actual awk programs (not just compiling awk itself) surfaced a real,
general `printf`/`snprintf` bug, unrelated to awk specifically: `%g` with an
**explicit** precision (`%.6g`, `%.30g`, etc. -- as opposed to the implicit
default of 6) printed the right digits with the wrong leading zero padding,
e.g. `%.6g` of `4.0` printed `"000004"` instead of `"4"`, and `%.30g`
printed a 30-character run of zeros ahead of the `4`. `format_double_general()`
/ `format_long_double_general()` (`libc/src/printf.c`) were passing the
caller's original, still-`precision_set` spec straight through to
`write_formatted()`, which -- correctly, for `%d`/`%x`/etc. -- treats a set
precision as "zero-pad the digit string to at least this many characters";
applied a second time to a digit string %g had already fully rendered, it
produced this. The sibling fixed-point/exponential formatters already
cleared `precision_set` before their own final `write_formatted()` call;
this one code path had just been missed. No prior test exercised `%g` with
any precision other than the implicit default, so this had never been
caught. Fixed, with regression coverage added to `tests/printf_test.c`.
This is exactly why `onetrueawk`'s own `get_str_val()` (`tran.c`) hit it
immediately: converting any integral number to a string always calls
`snprintf(s, sizeof(s), "%.30g", val)`.
