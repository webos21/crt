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

After a successful `fork()`, the child process runs project-owned runtime reset
hooks before user atfork child handlers:

- TLS/current-thread state is rebound to the surviving calling thread.
- Linux thread registry and detached-thread reaper bootstrap state are cleared.
- pthread key/reaper locks, malloc heap lock, and stdio `FILE` locks are reset
  so the child is not left with locks owned by vanished threads.
- fd tracking has an explicit `__crt_fd_after_fork_child()` reset hook. It is a
  no-op for current Linux/macOS native fork inheritance and for Windows while
  `fork()` remains `ENOTSUP`, but it is the integration point for future Windows
  child bootstrap.

The internal hook boundary is:

- `__crt_atfork_prepare()` runs user prepare handlers in reverse registration
  order.
- `__crt_atfork_parent()` runs user parent handlers in registration order.
- `__crt_atfork_child()` resets CRT runtime state first, then runs user child
  handlers in registration order.

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
CRT shell use the same child bootstrap record that a constrained fork would use.
The current implementation transports fd snapshots through `CRT_FD_SNAPSHOT`,
wraps them in `CRT_CHILD_BOOTSTRAP`, applies `posix_spawn_file_actions_*` to
that snapshot, filters `FD_CLOEXEC` descriptors, and imports fd/cwd/rootfs/signal
mask state in CRT startup before `main()`.

The private `__crt_shell_fork_exec()` helper is the phase-1 shell contract. It
wraps the clearer `__crt_shell_spawn()` child-spec API, which carries path,
argv/envp, file actions, cwd, rootfs, signal mask/default reset, and stdio flush
policy as one shell child contract. This supports shell-style "prepare child fd
state, create child, exec target" flow, but it does not copy the parent's stack,
heap, or program counter. Public `fork()` remains `ENOTSUP` on Windows until a
stricter compatibility tranche is implemented and tested.

## Test Policy

The first test tranche validates:

- `_Fork()`/`fork()` child exit status;
- pipe fd inheritance;
- `pthread_atfork()` handler ordering;
- Windows `ENOTSUP` policy until fork emulation lands.

The second test tranche adds:

- Bionic/POSIX-shaped `sigset_t` manipulation APIs;
- `sigaction()` and `SA_SIGINFO` bootstrap behavior;
- `sigprocmask()`/`pthread_sigmask()` process-local mask storage;
- inherited signal action behavior across `fork()` on Linux/macOS;
- Windows fork/signal inheritance kept behind the explicit `ENOTSUP` policy.

The third test tranche adds:

- fork while another thread owns a `FILE` lock;
- child-side stdio lock reset before returning from `fork()`;
- Windows `ENOTSUP` policy for the same runtime-reset test surface.

Future tests should add:

- signal inheritance and `SIGCHLD`;
- close-on-exec behavior;
- broader multi-fd redirection beyond the initial shell smoke coverage;
- fork after malloc/pthread lock activity;
- command substitution shell smoke tests.
