# toybox

This directory is reserved for the Android `external/toybox` import.

Toybox is the preferred first source for Android-like command applets under
`/system/bin` and `/bin`.

## First Applet Set

- `cat`
- `echo`
- `pwd`
- `true`
- `false`
- `test`
- `[`
- `expr`
- `basename`
- `dirname`
- `mkdir`
- `rm`
- `cp`
- `mv`
- `ln`
- `chmod`
- `uname`
- `sed`
- `grep`

Applets requiring deeper terminal, procfs, device, login, networking, or mount
integration should be deferred until the shell and rootfs model is stronger.

Follow the shared import and patch policy in `docs/shell_import.md`.
