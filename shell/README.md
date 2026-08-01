# CRT Shell

`shell/` is a first-class CRT component, at the same architectural level as
`libc/`, `libm/`, `libdl/`, `libstdc++/`, and `linker/`.

The shell is not treated as an ordinary porting-test library. Its job is to
provide the Android-like command environment that later porting tests run on
top of:

```text
rootfs/system/bin/sh      -> crt_tiny_sh first, then mksh
rootfs/system/bin/toybox  -> toybox
rootfs/bin/<applets>      -> toybox applet links or launchers
```

## Policy

- Use Android `external/mksh` as the first `/system/bin/sh` reference.
- Use Android `external/toybox` as the first command applet reference.
- Keep upstream source unmodified where possible.
- Fill CRT/PAL/sysroot gaps before patching mksh or toybox.
- Build the first shell tranche as static CRT executables.
- Keep `crt_tiny_sh` project-owned. It is a bootstrap smoke runner for the CRT
  shell/process contract, not the final Android shell.
- Defer interactive job control, pty support, and full terminal behavior until
  non-interactive configure-script execution works.

## Milestones

1. Project-owned tiny shell smoke runner.
2. Import/provenance policy and source inventory.
3. mksh compile inventory against `tools/crt-cc`.
4. Non-interactive `/system/bin/sh -c 'echo ok'`.
5. Redirection, pipeline, command substitution, and here-doc behavior.
6. Minimal toybox applet set for configure scripts.
7. Porting tests using the CRT shell as their default shell.

## Bootstrap Tiny Shell

`crt_tiny_sh` is built from `shell/src/tiny_sh.c` and installed as `sh` in the
runtime rootfs. It intentionally implements only a small non-interactive subset:

- `sh -c "..."`;
- simple whitespace and quote tokenization;
- one or more pipeline stages with `|`;
- `<`, `>`, and single-digit `n>` redirection;
- builtins: `echo`, `cat`, `upper`, `pwd`, `cd`, `true`, `false`, `exit`;
- external command spawning through the CRT process path.

This runner exists so shell/PAL smoke tests and early porting probes can execute
inside the CRT process model before Android `external/mksh` is imported.
