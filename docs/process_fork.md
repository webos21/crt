# Process Fork Model

## Goal

`fork()` is a core PAL feature for the CRT shell. It is required for Unix-like
shell behavior, child process management, pipes, redirection, signal delivery,
and configure-script execution.

The project follows a Bionic-shaped public contract:

- expose `_Fork()`, `fork()`, `vfork()`, and `pthread_atfork()`;
- keep `_Fork()` as the low-level process creation primitive;
- make `fork()` run `pthread_atfork()` handlers around `_Fork()`;
- make child processes preserve normal POSIX fd inheritance;
- make unsupported host behavior fail explicitly instead of silently exposing a
  host-native non-Bionic process model.

## Bionic Reference Shape

Bionic separates `_Fork()` from `fork()`. `fork()` runs registered atfork
prepare handlers, calls `_Fork()`, and then runs parent or child handlers.

Important behavior to preserve:

- prepare handlers run in reverse registration order;
- parent and child handlers run in registration order;
- after fork, only the calling thread exists in the child;
- child runtime state must be made coherent before returning to user code;
- fd inheritance, signal disposition, wait status, pid/tid state, and errno/TLS
  state must remain Bionic-compatible.

## Current Implementation

Linux and macOS currently provide native `_Fork()`/`fork()` behavior through the
host kernel syscall backend:

- Linux uses the existing raw syscall path.
- macOS uses the Darwin syscall backend.
- `pthread_atfork()` handler ordering is implemented in project-owned libc code.
- `vfork()` currently aliases `fork()` until a separate Bionic-compatible policy
  is needed.

Windows currently returns `ENOTSUP` for `_Fork()`/`fork()`. This is an explicit
bootstrap policy, not the long-term target.

## Windows Direction

Git Bash, MSYS2, and Cygwin show that fork-like behavior can be approximated on
Windows, but their implementation is a full POSIX runtime strategy and their
source/license/runtime shape is not adopted here.

The CRT should instead build a project-owned Windows fork tranche around:

- `CreateProcess` child bootstrap mode;
- serialized CRT fd table import;
- inheritable file, pipe, socket, console, and null handles;
- cwd, rootfs, and environment propagation;
- signal disposition/mask propagation where the Windows console/process model
  allows it;
- child process registry integration for `waitpid()`;
- tight tests for the exact shell patterns required by mksh/toybox.

Before a full Windows `fork()` emulation is attempted, `posix_spawn()` and the
child fd table import path should share as much infrastructure as possible.

## Test Policy

The first test tranche validates:

- `_Fork()`/`fork()` child exit status;
- pipe fd inheritance;
- `pthread_atfork()` handler ordering;
- Windows `ENOTSUP` policy until fork emulation lands.

Future tests should add:

- signal inheritance and `SIGCHLD`;
- close-on-exec behavior;
- multi-fd redirection;
- fork after stdio/malloc/pthread lock activity;
- pipeline and command substitution shell smoke tests.
