# Job Control Minimal Surface

## Goal

The CRT shell needs a small job-control surface before full interactive shell
support exists. The first tranche is intentionally narrow:

- `setpgid()`;
- `getpgrp()`;
- `setsid()`;
- `tcgetpgrp()`;
- `tcsetpgrp()`.

This is enough for shell configure probes and early mksh/toybox inventory work
to see a coherent POSIX/Bionic-shaped process-group API without claiming full
terminal job control.

## Linux And macOS

Linux and macOS route `setpgid()`, `getpgrp()`, and `setsid()` to native kernel
syscalls through the PAL syscall layer.

`tcgetpgrp()` and `tcsetpgrp()` use the public Bionic/Linux ioctl request
numbers `TIOCGPGRP` and `TIOCSPGRP`. macOS maps those request numbers to the
Darwin ioctl values internally, matching the existing `TIOCGWINSZ` mapping
policy.

## Windows

Windows does not have POSIX sessions, foreground process groups, or terminal
job control. The first policy is a console process-group approximation:

- `getpgrp()` returns a CRT-managed process-group id, initialized to `getpid()`;
- `setpgid(0, 0)` and same-process `setpgid()` update that CRT-managed id;
- `setsid()` sets the CRT-managed session and process group to `getpid()`;
- `tcgetpgrp()`/`tcsetpgrp()` succeed only for CRT tty/console fds and fail with
  `ENOTTY` for non-tty fds;
- unsupported cross-process `setpgid()` returns `ENOTSUP`.

This is not full interactive job control. Later work still needs console
Ctrl-C/Ctrl-Break delivery policy, process-group waits, stopped-child status,
and terminal foreground arbitration.
