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

Windows currently keeps `_Fork()`/`fork()` as a limited, experimental surface.
It is not the short-term contract for shell execution. The near-term shell goal
is to make mksh and toybox work through the explicit shell child-spec/spawn path
while keeping real Windows `fork()` as a long-term research tranche.

## Windows Direction

Git Bash, MSYS2, and Cygwin show that fork-like behavior can be approximated on
Windows, but their implementation is a full POSIX runtime strategy and their
source/license/runtime shape is not adopted here.

Other public experiments using `RtlCloneUserProcess` demonstrate why this is a
poor short-term shell foundation: the cloned process may not have coherent
Win32/CSR runtime state, and inherited handles can fail in the child even when
raw process cloning succeeds. Research systems that repair this by reconnecting
to CSRSS depend on version-specific internal offsets and are not acceptable for
the production CRT/PAL path.

The CRT should instead build a project-owned Windows shell process tranche
around:

- `CreateProcess` child bootstrap mode;
- serialized CRT fd table import;
- duplicate file, pipe, console, and null handles transported into the concrete
  child process;
- `WSADuplicateSocketA()` for socket fd transport once the child pid is known;
- cwd, rootfs, and environment propagation;
- signal disposition/mask propagation where the Windows console/process model
  allows it;
- child process registry integration for `waitpid()`;
- tight tests for the exact shell patterns required by mksh/toybox.

The current `posix_spawn()` implementation transports fd snapshots through
`CRT_FD_SNAPSHOT`, wraps them in `CRT_CHILD_BOOTSTRAP`, applies
`posix_spawn_file_actions_*` to that snapshot, filters `FD_CLOEXEC`
descriptors, and imports fd/cwd/rootfs/signal mask state in CRT startup before
`main()`. Windows `fork()` should reuse those pieces, but they are not
sufficient by themselves: fork must also restore register/stack/runtime state
so the child returns from the original `fork()` call with value `0`.

The private `__crt_shell_fork_exec()` helper is now the intended Windows
shell-child contract for fork-then-exec patterns. It should be extended as a
clear internal child spec that includes cwd/rootfs/env, signal mask/default
policy, file actions, close-on-exec filtering, and stdio flush policy. Linux
and macOS may route through the same helper for shell-owned tests, but their
public `fork()` behavior remains native.

Real Windows `fork()` remains a long-term goal, not a prerequisite for the next
mksh/toybox milestone. A future implementation may reuse the child bootstrap
and fd snapshot machinery, but it must separately solve the harder POSIX
contract: returning from the original `fork()` call site with coherent
register, stack, runtime, Win32, and CSR state.

## Research References

The current Windows fork policy is informed by:

- simple `RtlCloneUserProcess` examples, including Cr4sh's native API sample
  (`https://gist.github.com/Cr4sh/126d844c28a7fbfd25c6`) and the Petr
  Smid-derived Cygwin mailing-list reproducer
  (`https://cygwin.com/pipermail/cygwin/2025-September/258811.html`);
- Hunt & Hackett/diversenok's analysis of Windows process cloning semantics
  (`https://diversenok.github.io/2023/04/20/Process-Cloning.html`);
- Pavel Galkin's fork experiment, which shows inherited-handle failure after
  native cloning (`https://pavelgalkin.com/blog/2025/06/23/`);
- Winnie's CSRSS-reconnect approach, treated as research only because it relies
  on reversed, version-specific Windows internals
  (`https://rlee063.github.io/winnie-afl.html`,
  `https://hackyboiz.github.io/2021/08/22/fabu1ous/winnie-2/`);
- WSL1's pico-process architecture, which is kernel/provider based and not
  reusable from this userland CRT
  (`https://learn.microsoft.com/it-it/previous-versions/windows/desktop/cmdline/wsl-architectural-overview`).

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

Real signal delivery (not just process-local bookkeeping) and `SIGCHLD`
reaching a blocked `pselect()`/`poll()` are now implemented; see
`docs/signal_delivery.md` for the per-OS backend architecture and the
`pselect()` atomicity fix that made it actually usable by GNU make's
jobserver.

Future tests should add:

- a permanent regression test for the `fork()` + blocked-`SIGCHLD` +
  `pselect()` pattern documented in `docs/signal_delivery.md`;
- close-on-exec behavior;
- broader multi-fd redirection beyond the initial shell smoke coverage;
- fork after malloc/pthread lock activity;
- command substitution shell smoke tests.
- Windows direct `fork()+execve()` tests should stay documented as unsupported
  or experimental until a real fork tranche exists. Shell progress should be
  validated with `posix_spawn()` and `__crt_shell_fork_exec()` child-spec tests.
