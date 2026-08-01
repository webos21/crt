# Windows Fork Emulation Plan

## Goal

Windows public `fork()` remains unsupported for now and must keep returning
`ENOTSUP`. This document defines the feasibility path for a project-owned
emulation that can support the CRT shell without importing the MSYS2/Cygwin
runtime model.

The first shared primitive is fd table serialization. The same mechanism should
serve:

- future Windows `fork()` emulation;
- `posix_spawn()` child bootstrap;
- shell pipelines, redirection, and command substitution;
- configure-script execution under the CRT shell.

## Current Windows fd Table

The Windows PAL currently owns the descriptor table in
`libc/arch/windows/common/syscall.c`:

- `fd_table[64]` stores host `HANDLE` values or Winsock socket handles.
- `fd_kind[64]` distinguishes empty slots, file handles, and sockets.
- `fd_flags[64]` currently tracks `FD_CLOEXEC` for descriptor inheritance.
- descriptors 0, 1, and 2 are initialized from `GetStdHandle()`.
- `open()`, `pipe()`, and `dup()` allocate project-owned fd slots.
- `close()` closes the host handle and clears the fd slot.
- `posix_spawn()` now transports a CRT fd snapshot in the child environment, so
  non-standard file descriptors can be reconstructed before `main()`.

This is enough for basic Windows tests, but not enough for shell-style child
processes. A child must be able to reconstruct the CRT fd table, not merely the
three Win32 standard handles.

## Snapshot Format

`private/crt_fd_table.h` defines the initial in-memory snapshot ABI:

- magic/version/capacity header;
- fixed maximum entry count matching the current fd table size;
- per-entry fd number, descriptor kind, flags, and host handle value;
- `CRT_FD_SNAPSHOT_FLAG_INHERITABLE` marks handles prepared for child
  inheritance.

The current Windows implementation exports file-like handles and socket handle
slots through the same inheritable handle transport. This is enough to exercise
the CRT fd-table path uniformly, but robust cross-process socket duplication may
still need a Winsock-specific policy such as `WSADuplicateSocket` if plain
handle inheritance proves insufficient for broader socket cases.

Linux and macOS expose the same private API as an explicit `ENOTSUP` stub. They
already have native `fork()` fd inheritance and do not need this bootstrap
format.

## Export/Import Semantics

`__crt_fd_snapshot_export()` duplicates each eligible Windows fd handle as an
inheritable handle and records it in the snapshot. Descriptors marked
`FD_CLOEXEC` are filtered out. The snapshot owns those duplicated handles until
`__crt_fd_snapshot_dispose()`.

`__crt_fd_snapshot_import()` duplicates each snapshot handle into the current
process fd table. Import does not consume the snapshot, so callers can dispose
the snapshot afterwards.

`__crt_fd_snapshot_encode()` and `__crt_fd_snapshot_decode()` provide the fd
text transport format. Windows `posix_spawn()` injects this encoded snapshot as
`CRT_FD_SNAPSHOT` in the child environment block, under the broader
`CRT_CHILD_BOOTSTRAP=1` contract. The Windows CRT startup calls
`__crt_child_bootstrap()` before `main()`, imports the fd table, restores the
bootstrap cwd/rootfs/signal-mask state, and disposes the inherited snapshot
handles.

The first unit test performs an in-process round trip:

1. create a pipe;
2. export the fd table;
3. close the original pipe descriptors;
4. import the snapshot;
5. verify the same fd numbers work for read/write again.

This intentionally validates the fd table mechanics while keeping Windows
`fork()` disabled.

The second unit test crosses a real process boundary:

1. parent creates a pipe;
2. parent uses `posix_spawn()` to run the same executable;
3. `posix_spawn()` transports the fd snapshot through `CRT_FD_SNAPSHOT`;
4. child CRT startup imports the fd table before `main()`;
5. child writes to the inherited pipe fd passed through `argv`;
6. parent reads the byte and verifies the child exit status.

## Child Bootstrap Direction

The future Windows child bootstrap should use the same snapshot data rather
than adding a separate `posix_spawn()`-only path:

1. parent builds an fd snapshot;
2. parent encodes the snapshot into the current `CRT_FD_SNAPSHOT` transport;
3. parent starts the child with `CreateProcessA(..., bInheritHandles=TRUE, ...)`;
4. child CRT startup detects `CRT_CHILD_BOOTSTRAP` before `main()`;
5. child imports the fd table snapshot;
6. child imports cwd/rootfs/environment/signal policy;
7. child enters either fork-resume mode or exec/spawn mode.

`posix_spawn()` still fills `STARTUPINFOA` std handles for host compatibility,
but those handles now come from the same fd snapshot that is transported to the
child. `posix_spawn_file_actions_addopen()`, `addclose()`, and `adddup2()` are
applied to the snapshot before it is encoded, so non-stdio descriptors can be
created or remapped for the child without mutating the parent fd table.
`fork()` emulation can reuse the same descriptor import path and focus on
memory/runtime-state policy.

## Shell-Oriented Contract

The first shell-facing contract is `__crt_shell_fork_exec()`, a private helper
that deliberately means "create a child with fork-like fd state and then exec a
program". It is implemented through `posix_spawn()` today.

This is not a general C `fork()` replacement. It does not copy the caller's C
stack, heap, or program counter. It gives the CRT shell a stable primitive for
the common shell pattern:

1. build pipes and redirections in the parent;
2. describe child fd actions with `posix_spawn_file_actions_*`;
3. create the child through the shared Windows bootstrap path;
4. wait with the CRT child registry and `waitpid()`.

Linux and macOS already route `posix_spawn()` through the native fork/exec
backend in project-owned libc code. Windows uses `CreateProcessA` plus the CRT
snapshot transport. This keeps the shell layer source-portable while the Windows
PAL remains explicit about the constrained semantics.

Windows `execve()` is implemented only for this shell child contract: it spawns
the target with the same bootstrap machinery, waits, and exits the current
process with the child's exit status. It does not claim Bionic/Linux in-place
image replacement semantics.

## Open Items

- socket duplication torture tests and possible `WSADuplicateSocket` backend;
- child registry stress tests for multiple concurrent children, process-group
  waits, and shell pipeline teardown;
- full signal disposition propagation beyond mask/default reset;
- memory/state policy for real `fork()` emulation.
