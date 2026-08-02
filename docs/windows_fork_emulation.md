# Windows Fork Emulation Plan

## Goal

Windows public `fork()` is a long-term research item, not the short-term shell
architecture. The immediate goal is to make mksh and toybox usable on Windows
through an explicit CRT-owned shell child-spec path that models the common
fork-then-exec shell case without pretending that Windows can cheaply provide a
full POSIX `fork()`.

This document defines the current Windows process bootstrap contract and keeps
the eventual project-owned `fork()` emulation tranche separate from the
mksh/toybox enablement path. The project does not import the MSYS2/Cygwin
runtime model.

The first shared primitive is fd table serialization. The same mechanism should
serve:

- shell child-spec execution on Windows;
- future Windows `fork()` emulation research;
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

`private/crt_fd_table.h` defines the private in-memory snapshot ABI:

- magic/version/capacity header;
- fixed maximum entry count matching the current fd table size;
- per-entry fd number, descriptor kind, flags, and host handle value;
- `CRT_FD_SNAPSHOT_FLAG_INHERITABLE` marks handles that are directly
  inheritable by a child.
- `CRT_FD_SNAPSHOT_FLAG_REMOTE_PROCESS_HANDLE` marks file-like handles that
  have already been duplicated into the concrete child process. These values
  are meaningful in the child, not closeable parent handles.

The current Windows implementation exports file-like handles as parent-owned
duplicates, then duplicates them into the concrete child process after
`CreateProcessA` succeeds. Socket fd entries are converted to
`WSADuplicateSocketA()` protocol records once the child process id is known.

The current Windows test tranche validates one loopback TCP connection across
the real child process boundary: the parent creates an accepted socket pair,
passes the client socket fd through `__crt_shell_fork_exec()`, the child sends
and receives through the duplicated fd, and the parent verifies the peer I/O and
exit status. This exercises the `WSADuplicateSocketA()` snapshot-pipe path.

Linux and macOS expose the same private fd snapshot API as an explicit
`ENOTSUP` stub. They already have native `fork()` fd inheritance and do not need
this bootstrap format.

## Export/Import Semantics

`__crt_fd_snapshot_export()` duplicates each eligible Windows fd handle and
records it in the snapshot. Descriptors marked `FD_CLOEXEC` are filtered out.
For the snapshot-pipe path, the parent duplicates file-like handles into the
real child process and marks those entries as remote process handles. The
snapshot owns only the parent-side duplicates that are not remote child handle
values.

`__crt_fd_snapshot_import()` duplicates each snapshot handle into the current
process fd table. Import does not consume the snapshot, so callers can dispose
the snapshot afterwards.

`__crt_fd_snapshot_encode()` and `__crt_fd_snapshot_decode()` provide the fd
text transport format. Windows `posix_spawn()` normally injects this encoded
snapshot as `CRT_FD_SNAPSHOT` in the child environment block, under the broader
`CRT_CHILD_BOOTSTRAP=1` contract.

When the snapshot contains socket fds, Windows uses a stronger bootstrap path:
the child is created suspended with an inherited snapshot pipe handle, the
parent calls `WSADuplicateSocketA()` for the real child process id, encodes the
updated snapshot, writes it to the bootstrap pipe, and resumes the child. The
child calls `WSASocketA(FROM_PROTOCOL_INFO, ...)` while importing the snapshot.

The Windows CRT startup calls `__crt_child_bootstrap()` before `main()`, imports
the fd table from either `CRT_FD_SNAPSHOT` or the snapshot pipe, restores the
bootstrap cwd/rootfs/signal-mask state, and disposes the inherited snapshot
handles.

The first unit test performs an in-process round trip:

1. create a pipe;
2. export the fd table;
3. close the original pipe descriptors;
4. import the snapshot;
5. verify the same fd numbers work for read/write again.

This intentionally validates the fd table mechanics independently from the
larger Windows `fork()` resume problem.

The second unit test crosses a real process boundary:

1. parent creates a pipe;
2. parent uses `posix_spawn()` to run the same executable;
3. `posix_spawn()` transports the fd snapshot through `CRT_FD_SNAPSHOT`;
4. child CRT startup imports the fd table before `main()`;
5. child writes to the inherited pipe fd passed through `argv`;
6. parent reads the byte and verifies the child exit status.

## Shell Child-Spec Direction

The short-term Windows shell path should treat fork-then-exec as a first-class
internal operation:

1. shell or libc builds a child spec containing executable path, argv/envp,
   cwd/rootfs, signal policy, fd file actions, close-on-exec policy, and stdio
   flush policy;
2. parent starts the child suspended when fd/socket snapshot data must be
   patched after the child pid is known;
3. parent transports the fd snapshot through the bootstrap pipe;
4. child CRT startup imports the fd table, cwd/rootfs, and signal state before
   entering `main()`;
5. parent registers the child for `waitpid()` and process-group approximation;
6. pipelines and command substitutions tear down unused pipe ends in both
   parent and child according to the child spec.

This path is allowed to power mksh/toybox Windows execution even while public
`fork()` remains incomplete. It is still a libc/PAL boundary, not host SDK
leakage and not a broad change to Bionic public ABI.

## Fork Bootstrap Research

Windows `fork()` should use the same child bootstrap data rather than adding a
separate `posix_spawn()`-only path:

1. parent builds an fd snapshot;
2. parent encodes ordinary file-handle snapshots into `CRT_FD_SNAPSHOT`, or
   creates a snapshot pipe for socket-bearing snapshots;
3. parent starts the child with `CreateProcessA(..., bInheritHandles=TRUE, ...)`;
4. child CRT startup detects `CRT_CHILD_BOOTSTRAP` before `main()`;
5. for socket-bearing snapshots, parent uses `WSADuplicateSocketA()` with the
   child pid, writes the updated snapshot to the pipe, and resumes the child;
6. child imports the fd table snapshot;
7. child imports cwd/rootfs/environment/signal policy;
8. child enters fork-resume mode and returns from `fork()` with value `0`.

`posix_spawn()` still fills `STARTUPINFOA` std handles for host compatibility,
but those handles now come from the same fd snapshot that is transported to the
child. `posix_spawn_file_actions_addopen()`, `addclose()`, and `adddup2()` are
applied to the snapshot before it is encoded, so non-stdio descriptors can be
created or remapped for the child without mutating the parent fd table.
`fork()` emulation can reuse the same descriptor import path, but it still must
solve the harder POSIX contract: the child resumes at the original `fork()` call
site with copied enough stack/register/runtime state for mksh's normal child
branch to run.

## Shell-Oriented Contract

The existing shell-facing `__crt_shell_spawn()` and `__crt_shell_fork_exec()`
helpers are the intended Windows short-term shell contract. They should be
grown into a precise child-spec API rather than treated as temporary smoke-test
helpers. mksh and toybox integration work may adapt their Windows build/glue to
route fork-then-exec patterns through this helper, while leaving arbitrary
post-fork child execution unsupported on Windows.

Windows `execve()` is implemented only for this shell child contract: it spawns
the target with the same bootstrap machinery, waits, and exits the current
process with the child's exit status. It does not claim Bionic/Linux in-place
image replacement semantics.

## Open Items

- socket duplication torture tests beyond the current single TCP loopback smoke;
- broader child registry stress tests for process-group waits and shell
  pipeline teardown. The current Windows fd snapshot test already covers a
  small multiple-child `waitpid(-1)` drain;
- full signal disposition propagation beyond mask/default reset;
- mksh/toybox child-spec adapter for external commands, pipelines, redirection,
  command substitution, and exec-builtin-like replacement;
- memory/state policy for real `fork()` emulation;
- stack/register resume proof-of-concept for static CRT executables;
- ASLR/base-address constraints and failure diagnostics;
- direct Windows `fork()` policy tests that clearly distinguish unsupported
  arbitrary fork from supported shell child-spec execution.
