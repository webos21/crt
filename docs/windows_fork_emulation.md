# Windows Fork Emulation Plan

## Goal

Windows `fork()` remains unsupported for now and must keep returning `ENOTSUP`.
This document defines the feasibility path for a project-owned emulation that
can support the CRT shell without importing the MSYS2/Cygwin runtime model.

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
- descriptors 0, 1, and 2 are initialized from `GetStdHandle()`.
- `open()`, `pipe()`, and `dup()` allocate project-owned fd slots.
- `close()` closes the host handle and clears the fd slot.
- `posix_spawn()` currently duplicates only stdin/stdout/stderr into
  `STARTUPINFOA`, so non-standard descriptors are not yet transferred.

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

The current Windows implementation exports only file-like handles: regular
files, pipes, consoles, and null handles. Socket serialization is deliberately
deferred because a robust Windows implementation should use a Winsock-specific
policy such as `WSADuplicateSocket`, not plain file handle inheritance.

Linux and macOS expose the same private API as an explicit `ENOTSUP` stub. They
already have native `fork()` fd inheritance and do not need this bootstrap
format.

## Export/Import Semantics

`__crt_fd_snapshot_export()` duplicates each eligible Windows fd handle as an
inheritable handle and records it in the snapshot. The snapshot owns those
duplicated handles until `__crt_fd_snapshot_dispose()`.

`__crt_fd_snapshot_import()` duplicates each snapshot handle into the current
process fd table. Import does not consume the snapshot, so callers can dispose
the snapshot afterwards.

`__crt_fd_snapshot_encode()` and `__crt_fd_snapshot_decode()` provide the first
text transport format. Windows `posix_spawn()` currently injects the encoded
snapshot as `CRT_FD_SNAPSHOT` in the child environment block. The Windows CRT
startup calls `__crt_child_bootstrap()` before `main()`, detects that variable,
imports the fd table, and disposes the inherited snapshot handles.

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
4. child CRT startup detects bootstrap mode before `main()`;
5. child imports the fd table snapshot;
6. child imports cwd/rootfs/environment/signal policy;
7. child enters either fork-resume mode or exec/spawn mode.

`posix_spawn()` still prepares `STARTUPINFOA` std handles for host compatibility,
but it now also transports the full file-like CRT fd table through the snapshot
bootstrap. `fork()` emulation can reuse the same descriptor import path and
focus on memory/runtime-state policy.

## Open Items

- fd close-on-exec and inheritable-handle filtering;
- sockets through Winsock duplication;
- cwd/rootfs/environment import record;
- child process registry integration with `waitpid()`;
- signal disposition/mask propagation;
- memory/state policy for real `fork()` emulation.
