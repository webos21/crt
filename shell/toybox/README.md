# toybox

This directory contains the Android `external/toybox` import used for the first
CRT command applet set.

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

The initial CRT overlay is intentionally smaller than Android's generated
toybox configuration. It disables applets such as `gzip`, `mount`, `ps`,
`pgrep`, `pkill`, `nproc`, `flock`, and `unshare` until the matching CRT/PAL
surface is implemented.

Current macOS smoke status:

- `crt_toybox echo toybox-ok` passes.
- `/system/bin/sh` is an mksh symlink in the generated rootfs.
- Single external toybox applet execution through mksh works.
- Compound external-command sequencing and pipeline teardown still need
  waitpid/SIGCHLD/job-loop hardening before porting recipes can switch to this
  shell by default.

Follow the shared import and patch policy in `docs/shell_import.md`.
