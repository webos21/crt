# Toybox Patch Notes

This file records CRT-local changes made on top of the Android
`external/toybox` import. Keep this list updated whenever imported toybox
source is edited.

## Policy

- Prefer filling CRT/PAL/sysroot gaps over patching toybox.
- When toybox source must be patched, keep the change small and record:
  - the touched files;
  - why the change belongs in toybox source or CRT glue;
  - whether it affects public CRT/Bionic ABI;
  - what would let us remove or upstream the patch later.
- Do not expose host SDK or Windows CRT behavior as public ABI to make a toybox
  applet compile.

## Windows LLP64 Pointer-Width Fixes

Status: active CRT-local patch.

Touched files:

- `src/lib/args.c`
- `src/lib/lib.h`
- `src/main.c`
- `src/toys/posix/du.c`
- `src/toys/posix/find.c`
- `src/toys/posix/ls.c`
- `src/toys/posix/sed.c`
- `src/toys/posix/tsort.c`
- `src/toys/posix/xargs.c`

Reason:

Android/Linux 64-bit toybox can rely on LP64, where `long` and pointers are
both 64-bit. Windows x86_64 is LLP64, where `long` remains 32-bit while
pointers are 64-bit. Several toybox internals used `long` as a generic
pointer-sized storage slot. On Windows x86_64 this can truncate pointers and
break applets such as `ls -l` or `ls -s`.

Change summary:

- `struct dirtree.extra` now uses `intptr_t` so callers may store either small
  integer state or pointer-width values without truncation.
- `ls` no longer routes string pointers through `unsigned long` varargs slots.
- The common option parser no longer treats the generated `GLOBALS` area as a
  plain `long[]`. It now advances by the actual option storage type:
  `char *`, `struct arg_list *`, `long`, or `FLOAT`.
- `main.c`, `sed`, and `xargs` use `intptr_t`/`uintptr_t` for pointer tagging
  and stack-depth checks.
- `find -printf` separates integer and string printf arguments instead of
  relying on LP64 vararg equivalence.
- `du` accumulates `dirtree.extra` through pointer-width integer casts.
- `tsort` (2026-08-16, found while auditing the next disabled-applet batch,
  see `HISTORY.md`): its own `bsearch()`-argument-adjustment trick round-tripped
  a `char **` through `unsigned long`, truncating it on Windows. Changed to
  `uintptr_t`.

ABI impact:

None on the CRT/Bionic public ABI. These are imported toybox source
portability fixes. They must not be used as a reason to change public CRT
`long`, pointer, or syscall ABI shape.

Removal/upstream condition:

This patch can be reduced if upstream toybox gains an LLP64-safe internal
storage model, or if this project replaces the local import with an upstream
revision that no longer assumes `sizeof(long) == sizeof(void *)` for these
paths. Disabled applets must still be audited before enabling them because
more LP64 assumptions may exist outside the current applet tranche.

Verification:

- `cmake --build --preset windows-host-ninja-debug --target rootfs`
- `ctest --preset windows-host-ninja-debug --output-on-failure`
- Windows direct applet smoke with `CRT_ROOTFS` set:
  - `system/bin/ls.exe -l /`
  - `system/bin/toybox.exe ls -s /`
  - `system/bin/sed.exe 'N;s/\n/-/'`
  - `system/bin/toybox.exe getopt -o a:b -- -a hello -b world rest`

Known remaining shell gap:

Windows mksh external-command lists, pipelines, and redirections should move to
the CRT-owned shell child-spec path. Real public Windows `fork()` remains a
long-term research item; do not change CRT/Bionic public ABI or expose host SDK
process semantics to hide that gap.

## Windows Applet Name Lookup

Status: active CRT-local patch.

Touched file:

- `src/main.c`

Reason:

When launched directly from PowerShell or another Windows host process,
`argv[0]` may contain a `\`-separated path and a `.exe` suffix. Toybox command
lookup historically expected POSIX-style applet names. The CRT rootfs uses
copy-based `.exe` applet aliases on Windows, so direct host invocation should
still resolve to the intended toybox applet during development smoke tests.

Change summary:

- `toy_find()` strips both `/` and `\` path prefixes.
- `toy_find()` strips a case-insensitive `.exe` suffix before command lookup.

ABI impact:

None on CRT/Bionic public ABI. This is toybox-local applet dispatch behavior.

Removal/upstream condition:

This can be revisited if Windows rootfs applets move away from copy-based
`.exe` aliases or if an upstream/more general toybox applet-dispatch path
handles host path syntax without local changes.

## Configure Smoke Applet Set

Status: active CRT rootfs selection.

Touched files:

- `crt/generated/newtoys.h`
- `tools/create_rootfs.py`

Reason:

The Windows CRT rootfs shell is now used for configure-stage porting smoke
tests. zlib's configure script needs a small POSIX utility set beyond basic
interactive shell commands, including `date`, `expr`, `printf`, and `tee`.

Change summary:

- Enabled toybox applet registrations for `date`, `expr`, `printf`, and `tee`.
- Added matching rootfs aliases/copies so mksh can find them through
  `PATH=/system/bin:/bin:/usr/bin`.

ABI impact:

None on CRT/Bionic public ABI. This only broadens the shell rootfs command set.

Removal/upstream condition:

This is expected to remain as part of the project-owned Android-like shell
environment. Future configure failures should add applets only after confirming
the missing command is normal POSIX/Android shell surface rather than a host
SDK leak.

## CRT confstr Header Compatibility

Status: active CRT-local patch.

Touched file:

- `src/lib/portability.h`

Reason:

Toybox carries a Bionic fallback for `confstr`, but this CRT now exposes
`confstr(_CS_PATH)` as part of the Bionic-compatible public header surface
needed by GNU make and other build tools. Leaving the fallback unconditional
under `__BIONIC__` conflicts with the public `unistd.h` prototype.

Change summary:

- The fallback is now enabled only when `_CS_PATH` was not defined by the
  active CRT headers.

ABI impact:

None on CRT/Bionic public ABI. The public ABI change is the CRT-owned
`confstr` addition; this patch only prevents imported toybox source from
redeclaring it with an incompatible fallback signature.

Removal/upstream condition:

This patch can be dropped if upstream toybox probes `confstr` availability
instead of assuming all Bionic-like environments lack it.

## mksh Windows Drive-Letter Absolute Path Recognition

Status: active CRT-local patch.

Touched files:

- `mksh/src/sh.h`
- `shell/CMakeLists.txt`

Reason:

mksh's own `mksh_abspath()` only recognized a leading `/` as an absolute
path. `getcwd()`/`pwd` have no POSIX root to report on Windows, so they
always return a Windows-native absolute path (`C:\Users\...`); without this,
`cd` treated that as *relative* and silently prepended the current directory
to it, doubling the path into `<dir>/<dir>`. This surfaced while running
libpng's (autoconf-generated) `configure` on Windows aarch64: its own
`ac_pwd=\`pwd\` && ... cd "$ac_pwd" && ls -di .` working-directory sanity
check failed with `configure: error: working directory cannot be
determined`. mksh already ships a full DOS-path-aware implementation behind
`MKSH_DOSPATH`, but that flag also switches `PATH`/`CDPATH` from `:` to
`;`-separated, which conflicts with this project's deliberate
`:`-separated rootfs `PATH` (`/system/bin:/bin:/usr/bin`, see
`tools/crt-port-build.py`), so enabling it wholesale was not an option.

Change summary:

- Added a new, narrower `MKSH_CRT_WINPATH` define (`sh.h`) that only patches
  `mksh_abspath`/`mksh_cdirsep`/`mksh_sdirsep` to recognize `X:\`/`X:/` and
  `\` as well as `/`, reusing mksh's own existing `MKSH_DOSPATH`-gated
  macro bodies verbatim rather than writing new path logic. `MKSH_PATHSEPC`
  is untouched (stays `:`).
- Enabled `MKSH_CRT_WINPATH` for the Windows build of `crt_mksh`, alongside
  the existing `MKSH_CRT_ALLOW_LLP64`/`MKSH_CRT_SHELL_CHILD_SPEC` defines.

ABI impact:

None on CRT/Bionic public ABI. This is mksh-internal path-string handling.

Removal/upstream condition:

This is expected to remain as long as this project keeps `:`-separated PATH
on Windows. If that convention ever changes, switching to plain
`MKSH_DOSPATH` (and updating every `PATH`/`CDPATH` construction site to use
`;`) would be the more natural fix instead of maintaining this narrower
variant.
