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
