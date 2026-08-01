# CRT Shell

`shell/` is a first-class CRT component, at the same architectural level as
`libc/`, `libm/`, `libdl/`, `libstdc++/`, and `linker/`.

The shell is not treated as an ordinary porting-test library. Its job is to
provide the Android-like command environment that later porting tests run on
top of:

```text
rootfs/system/bin/sh      -> mksh
rootfs/system/bin/toybox  -> toybox
rootfs/bin/<applets>      -> toybox applet links or launchers
```

## Policy

- Use Android `external/mksh` as the first `/system/bin/sh` reference.
- Use Android `external/toybox` as the first command applet reference.
- Keep upstream source unmodified where possible.
- Fill CRT/PAL/sysroot gaps before patching mksh or toybox.
- Build the first shell tranche as static CRT executables.
- Defer interactive job control, pty support, and full terminal behavior until
  non-interactive configure-script execution works.

## Milestones

1. Import/provenance policy and source inventory.
2. mksh compile inventory against `tools/crt-cc`.
3. Non-interactive `/system/bin/sh -c 'echo ok'`.
4. Redirection, pipeline, command substitution, and here-doc behavior.
5. Minimal toybox applet set for configure scripts.
6. Porting tests using the CRT shell as their default shell.
