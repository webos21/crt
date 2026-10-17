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

Native Linux/macOS `fork()` stabilizes the project-owned heap before taking
the kernel snapshot. User prepare handlers run first; then
`__crt_malloc_before_fork()` acquires the allocator lock. The parent releases
it with `__crt_malloc_after_fork_parent()` on both success and error, before
user parent handlers. The child resets its inherited lock before user child
handlers. Thus all three kinds of application callback can allocate without
recursively taking this internal lock. Resetting a copied lock alone cannot
make an in-progress free-list mutation coherent.

The native `posix_spawn()` path uses the same allocator snapshot boundary and
runtime-only child reset, **without invoking application `pthread_atfork()`
handlers**. This matters on macOS: file actions renumber CRT's framed
`SOCK_SEQPACKET` descriptors, so the child exports their final numbers into
the exec environment, and that private adapter allocates. Moving the export
before file actions would lose the descriptor mapping rather than fix it.
`_Fork()` remains the raw primitive; it does not acquire allocator locks or
promise that application malloc is async-signal-safe afterward. This change
also does not make every CRT/host library operation after a multithreaded fork
async-signal-safe. Windows's memory-copy backend is unchanged: it allocates
while copying and cannot use this outer native-COW heap guard.

References: Bionic's [`fork.cpp`](https://android.googlesource.com/platform/bionic/+/refs/heads/main/libc/bionic/fork.cpp)
separates the raw primitive and application atfork boundary. The allocator
side follows the lock-before-snapshot principle documented in Android
Scudo commit [`e274f502dac9`](https://android.googlesource.com/platform/external/scudo/+/e274f502dac9e28654778c589646acedee4fbc87),
not a claim that CRT imports Scudo or Bionic's complete spawn implementation.

After a successful `fork()`, the child process runs project-owned runtime reset
hooks before user atfork child handlers:

- TLS/current-thread state is rebound to the surviving calling thread.
- Linux thread registry and detached-thread reaper bootstrap state are cleared.
- pthread key/reaper locks, malloc heap lock, and stdio `FILE` locks are reset
  so the child is not left with locks owned by vanished threads.
- fd tracking has an explicit `__crt_fd_after_fork_child()` reset hook. Linux
  needs no fd-table reconstruction; macOS resets the private framed-socket
  locks while preserving inherited descriptor state. On Windows it is an
  integration point used by the memory-copy
  `fork()` child bootstrap described below.

The internal hook boundary is:

- `__crt_atfork_prepare()` runs user prepare handlers in reverse registration
  order.
- `__crt_atfork_parent()` runs user parent handlers in registration order.
- `__crt_atfork_child()` resets CRT runtime state first, then runs user child
  handlers in registration order.

**Update: Windows `fork()` is no longer experimental or `ENOTSUP` -- it is
implemented and verified on both Windows architectures**, as a real,
general-purpose `_Fork()`/`fork()`, not just a shell-child-spec workaround.
Everything below this point in the file (the "Windows Direction"/"Research
References" sections that used to live here, plus the `ENOTSUP`-era test
tranches) described the *investigation and design work that led to* that
implementation, written while it was still an open research question --
kept only as historical background. For the actual, current design (a
Cygwin/MSYS-style memory-copy `fork()`, selected in
`libc/src/arch/windows/common/syscall.c`'s `__crt_sys_fork()`, with the
per-architecture register/stack/CONTEXT-restoring implementations in
`libc/src/arch/windows/{aarch64,x86_64}/fork_memcopy.c`) see
[`docs/design/windows_fork_emulation.md`](windows_fork_emulation.md); for the full
chronological investigation (the `RtlCloneUserProcess` research, the spawn
broker that was built first and then retired, the exact bugs found building
each piece, and every reverted attempt) see
[`docs/bringup/windows_fork_emulation_history.md`](../bringup/windows_fork_emulation_history.md).
The private `__crt_shell_fork_exec()` helper mentioned in this file's older
text below is still real (mksh's own child-spec path still uses it for the
patterns it was built for), but it is no longer *the* Windows process
story -- real `fork()` now backs it and everything else that needs process
duplication on Windows.

## Test Policy (current)

Windows `fork()` now has the same real test coverage Linux/macOS always
had, not an `ENOTSUP` placeholder: `fork_test`, `fork_signal_test`, and
`fork_runtime_reset_test` all run for real on Windows (aarch64 and x86_64),
covering child exit status, pipe fd inheritance, `pthread_atfork()` handler
ordering, signal mask/disposition inheritance across `fork()`, and stdio
lock reset in the child. `windows_fd_snapshot_test` covers the fd-snapshot
transport `posix_spawn()`/the memory-copy `fork()`'s own child bootstrap
both build on. Real signal delivery (not just process-local bookkeeping)
and `SIGCHLD` reaching a blocked `pselect()`/`poll()` are also implemented
on all three OSes; see `docs/design/signal_delivery.md` for the per-OS backend
architecture and the `pselect()` atomicity fix that made it actually
usable by GNU make's jobserver -- also now exercised for real by parallel
`make -jN` port builds on Windows, see [`archived HISTORY.md`](https://github.com/webos21/crt/blob/4e5eead68048723c37e46c22d80bca43915ac093/HISTORY.md)'s 2026-08-11 entries.

Coverage added after the original fork tranche now includes fd 3+ redirection,
subshell/command-substitution patterns, concurrent-child stress, and GNU make
jobserver-style pipe contention (`mksh_shell_smoke_test` and
`process_stress_test`). The remaining Windows mechanism limits are documented
in `windows_fork_emulation.md`, not kept as stale generic TODO entries here.

`fork_heap_snapshot_test` includes the real allocator as a private fixture,
holds its heap lock in another thread and verifies that native fork/spawn
wait for release. It also exercises allocating atfork callbacks, spawn's
callback suppression and framed-socket inheritance after `dup2` file actions.
CTest bounds it to 30 seconds. Windows reports a real CTest skip for this
native-COW-specific fixture; its memory-copy tests remain separate. The
macOS failure/fix evidence belongs to
[the B7 follow-up](../acceptance/crtweb_acceptance.md#b7-native-forkspawn-allocator-follow-up-2026-10-17);
the native [Linux replay](../acceptance/crtweb_acceptance.md#linux-native-forkspawn-replay-2026-10-17)
records the same regression and C-stage results on Linux.

---

**Historical text below this line** describes the Windows fork design
question while it was still open (written before the memory-copy `fork()`
above existed) -- kept for context on how the current design was arrived
at, not as current status. See the "Update" note above for what's actually
true today.

## Windows Direction (historical)

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

(This is exactly what the memory-copy `fork()` implementation that shipped
later actually does -- see `docs/design/windows_fork_emulation.md`.)

## Research References (historical)

The Windows fork research that led to the current design drew on:

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

## Test Policy (historical, `ENOTSUP`-era)

The first test tranche validated:

- `_Fork()`/`fork()` child exit status;
- pipe fd inheritance;
- `pthread_atfork()` handler ordering;
- Windows `ENOTSUP` policy until fork emulation landed.

The second test tranche added:

- Bionic/POSIX-shaped `sigset_t` manipulation APIs;
- `sigaction()` and `SA_SIGINFO` bootstrap behavior;
- `sigprocmask()`/`pthread_sigmask()` process-local mask storage;
- inherited signal action behavior across `fork()` on Linux/macOS;
- Windows fork/signal inheritance kept behind the explicit `ENOTSUP` policy.

The third test tranche added:

- fork while another thread owns a `FILE` lock;
- child-side stdio lock reset before returning from `fork()`;
- Windows `ENOTSUP` policy for the same runtime-reset test surface.

All three tranches' Windows `ENOTSUP` placeholders have since been replaced
by the real tests listed under "Test Policy (current)" above.
