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
`libc/src/arch/windows/common/syscall.c`:

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

## Known Bug: Subshells Silently Corrupt The Interpreter's Own fds

Found while investigating a real `port-rebuild-zlib` `./configure` failure on
Windows aarch64 that produced *zero* output and a bare exit status 1 --
diagnosed with `mksh -x` tracing (`CRT_PORT_SHELL_XTRACE=1` in
`tools/crt-port-build.py`, temporary), which pinpointed the exact failing
construct: `mname=\`(uname -a || echo unknown) 2>/dev/null\`` in zlib's
`configure`, a parenthesized subshell carrying its own `2>/dev/null`
redirection, used inside a command substitution.

`MKSH_CRT_SHELL_CHILD_SPEC`'s `exchild()` fast path
(`shell/mksh/src/jobs.c`) used to skip the raw-fork/spawn machinery for both
`TCOM` (a single simple command) and `TPAREN` (a `(...)` subshell), running
either directly in-process via `execute()`. That is safe for `TCOM`, but not
for `TPAREN`: a subshell's entire point is process-level isolation --
redirections, `cd`, variable changes, and `exit` must not leak back to the
parent shell. In the *real*, non-CRT mksh design this isolation comes for
free because `exchild()` forks one real child to run the whole subshell body;
running it in-process instead means any redirection the subshell sets up
(`iosetup()`, before the `TPAREN` switch case even runs) mutates the
*interpreter's own* fd table with nothing to restore it afterward. For
`(...) 2>/dev/null` specifically, that permanently clobbers the shell's real
stderr -- every later `errorf()`/warning in the same script, no matter how
unrelated, silently vanishes into `/dev/null` instead of being reported,
which is exactly the "zero output, just exit 1" symptom this presented as.

Fixed by removing `TPAREN` from the fast-path condition, leaving only `TCOM`
eligible (`shell/mksh/src/jobs.c`). A subshell now always goes through the
normal job-bookkeeping path; since its node type is not `TEXEC`,
`crt_mksh_spawn_exec_child()` declines it and it falls through to the raw
`fork()` call. This does not make subshells work on Windows aarch64 (raw
`fork()` there still unconditionally returns `ENOTSUP`, since aarch64 has no
`RtlCloneUserProcess`-based fork path at all), but it turns the silent
corruption into an honest, immediately visible `mksh: can't fork - try
again` failure -- matching the actual, documented limitation instead of
masking it. On Windows x86_64 (where raw `fork()` via `RtlCloneUserProcess`
does work), this fix means subshells with their own redirections now get
real process isolation for the first time, instead of a latent, unnoticed
version of the same fd-corruption bug.

There is a second, independent copy of the same guard: `execute()` itself
(`shell/mksh/src/exec.c`) has its own `MKSH_CRT_SHELL_CHILD_SPEC` check --
`if ((flags&XFORK) && !(flags&XEXEC) && t->type != TPIPE && t->type != TCOM
&& t->type != TPAREN) return exchild(...)` -- deciding whether to call
`exchild()` at all. `comsub()` (backtick/`$(...)` command substitution)
calls `execute(t, XXCOM|XPIPEO|XFORK, NULL)` *directly*, bypassing
`exchild()`'s own entry-point guard entirely; for a `TPAREN` node this
second check also used to exclude it, so a subshell reached through command
substitution specifically -- exactly the `` `(uname -a || echo unknown)
2>/dev/null` `` construct that exposed this whole bug -- kept corrupting the
interpreter's fd table even after the `jobs.c` fix, because it never went
through `jobs.c`'s `exchild()` guard in the first place. Fixed the same way:
removed `TPAREN` from this second check too, leaving only `TCOM` excluded.
Confirmed via the same real `port-rebuild-zlib` reproduction that a rebuilt
`mksh.exe` still hit the identical silent failure after only the `jobs.c`
fix, which is what surfaced this second guard.

This is a genuine, narrow correctness fix, not a resolution of the underlying
"no real fork() on Windows aarch64" gap -- any real-world script whose
`configure`/build process needs a subshell to actually complete (not just
fail loudly) still requires the real Windows `fork()` emulation described
above.

## Windows aarch64: `RtlCloneUserProcess` Works, But `CreateProcessA` Crashes In The Clone

The "no `RtlCloneUserProcess`-based fork path at all" statement above was an
untested assumption, not a verified platform limitation. On real Windows
aarch64 hardware, `ntdll.dll` exports `RtlCloneUserProcess` exactly as it does
on x86_64, and the `struct crt_rtl_user_process_information` shape (`HANDLE`,
`ULONG`, LLP64 sizes) is identical across both architectures. Removing the
`#if defined(__x86_64__) || defined(_M_X64)` guards around
`__crt_sys_fork()`, `init_ntdll()`, and `fd_set_inherit_for_fork()` in
`libc/src/arch/windows/common/syscall.c` makes `fork()` succeed on aarch64:
parent and child get genuinely distinct PIDs, and the full `ctest` suite
(`fork_test`, `fork_signal_test`, `fork_runtime_reset_test`,
`job_control_test`, `rootfs_process_test`, `shell_smoke_test`, ...) passes
while now exercising real fork instead of the `ENOTSUP` fallback path.

This surfaced one real, independent bug: `fd_set_inherit_for_fork()` only
looped over `fd >= 3`, so descriptors 0/1/2 were never marked
`HANDLE_FLAG_INHERIT` before the clone. A single-level `fork()` with the
child only touching its own inherited stdio still worked (Win32 standard
handles are often already inheritable at process start), but a *nested*
scenario -- a subshell that itself does `` `( cmd ) 2>&1` `` -- reliably hit
`mksh: 2>&1 : bad file descriptor`, because the forked child's fd 1/2 Win32
handles were not actually valid in the child's own handle table. Looping
`fd_set_inherit_for_fork()` from `fd = 0` (matching
`fd_restore_inherit_after_fork()`, which already covered the full range)
fixed this.

With both of those fixed, `port-rebuild-zlib` on Windows aarch64 gets
significantly further -- past the `./configure[40]: can't fork - try again`
failure entirely -- but still fails, now at zlib's own
"Checking for obsessive-compulsive compiler options" compiler sanity check,
which itself runs as `` `( $CC -c ... ) 2>&1` `` (a subshell inside a command
substitution). Bisecting with file-based debug logging (writing to a
per-PID log file via `CreateFileA`, since inherited-handle-based logging like
`WriteFile(GetStdHandle(STD_ERROR_HANDLE), ...)` is exactly the kind of thing
under test and cannot be trusted here) narrowed the failure to one exact
spot: inside `__crt_sys_posix_spawn()`, when a process that *is itself* a
`RtlCloneUserProcess` clone calls `CreateProcessA()` to spawn a further
child, `CreateProcessA()` crashes (Windows reports the process's exit code as
`STATUS_ACCESS_VIOLATION`, whose low byte -- what `waitpid()` surfaces via
`WEXITSTATUS()` -- is `5`). Everything logged right up to "about to call
`CreateProcessA`" (fd snapshot export, `prepare_spawn_startup()`,
`CreatePipe()` for the bootstrap pipe); nothing after `CreateProcessA()` ever
logs, including the immediate post-call line. Plain syscalls in the cloned
child -- `CreateFileA`, `WriteFile`, `DuplicateHandle`, `GetCurrentProcessId`
-- all work fine; only `CreateProcessA` (or something it depends on) does not.

The likely explanation: `RtlCloneUserProcess` clones a process at the raw NT
level (address space, handle table) without repeating the CSRSS (Win32
subsystem) registration handshake that a normal `CreateProcess`-spawned
process goes through. Lower-level `ntdll`/`kernel32` calls that do not need
CSRSS keep working in the clone; `CreateProcessA` is a more involved Win32
API that does, and the clone is not a CSRSS-registered client. This matches
the historical reputation of `RtlCloneUserProcess`-based "fork on Windows"
research (Interix/Cygwin-adjacent): the clone is real at the NT level but not
fully alive as a Win32 process.

Practical effect: `fork()` by itself is now genuinely usable on Windows
aarch64 for the "fork, then only do direct syscalls, then `_exit()`" shape
(exactly what `fork_test.c` and friends exercise). It is *not* usable for
"fork, then spawn another process" -- which is exactly what a shell subshell
that needs to run an external command does. This is a narrower, more precise
version of the original "no real fork on aarch64" gap: the previous
assumption was that `RtlCloneUserProcess` was unavailable there; it is
available, but combining it with `CreateProcessA` in the child is what does
not work. `MKSH_CRT_SHELL_CHILD_SPEC`'s own child-spec path
(`__crt_shell_spawn()`/`__crt_shell_fork_exec()`, see "Shell Child-Spec
Direction" above) deliberately avoids this exact combination for ordinary
external-command execution and pipelines -- it spawns directly via
`posix_spawn()` from the *original* (non-cloned) shell process rather than
forking first. The remaining gap is specifically real POSIX subshells
(`TPAREN`) that both need process isolation (hence a fork) *and* need to run
an external command inside that isolated child.

## Deciding The Next Step: Overhead Investigation And Prior-Art Research

Two candidate fixes for the `CreateProcessA`-in-clone crash were on the table
before any more code was written: (a) a Cygwin-style replacement of
`__crt_sys_fork()` that recreates the process via `CreateProcessA`, copies the
parent's writable memory (heap, calling thread's stack, `.data`/`.bss`) into
the fresh child with `WriteProcessMemory`, and resumes the child at the
`fork()` call site using the existing per-architecture `setjmp`/`longjmp`
(`libc/src/arch/windows/{x86_64,aarch64}/setjmp.S`, already implemented and
tested, callee-saved-register-only `jmp_buf`); or (b) something narrower that
leaves `fork()` alone. Before committing to (a) -- a genuinely large,
architecture-specific, empirically-risky change (matching `CONTEXT`/
`SetThreadContext` per arch from scratch, since nothing like that exists
anywhere in this codebase yet; walking the heap allocator's `heap_head` chunk
list in `libc/src/malloc.c` to know what to copy; fixing up the Windows
`TlsAlloc`/`TlsGetValue`-backed pthread TLS slot value in `libc/src/tls.c`,
which does **not** survive a raw memory copy into a freshly created process
the way it does under `RtlCloneUserProcess`'s real address-space clone; and
critically, depending on an *unverified* assumption that disabling image
ASLR (`/DYNAMICBASE:NO`, not currently used anywhere in this repo's link
flags) plus a bottom-up-ASLR mitigation policy actually gives the parent and
a freshly `CreateProcessA`'d child matching stack virtual addresses) -- two
things were checked first.

### Overhead benchmark

Using only already-implemented, already-verified primitives (no new fork
mechanism), 400 iterations each of:
- `fork()` via the existing `RtlCloneUserProcess` path, child immediately
  `_exit(0)`s;
- `posix_spawn()` via the existing `__crt_sys_posix_spawn()`/`CreateProcessA`
  path, spawning a trivial `int main(void) { return 0; }` executable;

both measured with `waitpid()` included, on the real Windows aarch64 machine
this work was verified on. Result (steady-state, after a cold-start warm-up
run was discarded):

| | per-call | ratio |
| --- | --- | --- |
| `fork()` (`RtlCloneUserProcess`) | ~13.4-14.2 ms | 1.0x |
| `posix_spawn()` (`CreateProcessA`) | ~16.4-17.0 ms | ~1.20-1.23x |

`CreateProcessA` is only about 20% more expensive than `RtlCloneUserProcess`
on this hardware -- nowhere near Cygwin's reputation for order-of-magnitude
slower forks. (This measures the `CreateProcessA` floor only, not the
additional `WriteProcessMemory` cost a full memory-copy fork would add on
top -- but it means that floor is not the blocking concern it was assumed to
be.) This took the "is a `CreateProcessA`-based mechanism even viable
performance-wise" question off the table; it de-risks approach (a) if it
were needed, but more importantly it means overhead is not a reason to avoid
routing *just the spawn step* through a real `CreateProcessA` call either.

### Prior-art research

Before writing a from-scratch `CONTEXT`/register-resume mechanism, existing
public research on Windows process cloning was checked:

- [The Definitive Guide To Process Cloning on Windows](https://github.com/huntandhackett/process-cloning)
  (also at [diversenok's blog](https://diversenok.github.io/2023/04/20/Process-Cloning.html))
  confirms clones cannot reliably load additional DLLs or call CSRSS-backed
  Win32 APIs, and states there is **no documented way to re-register a clone
  with CSRSS**; its own recommendation is to avoid Win32 APIs inside the
  clone entirely and stick to raw NT syscalls -- which is exactly what this
  project's `MKSH_CRT_SHELL_CHILD_SPEC` workaround already does for ordinary
  command execution (see "Shell Child-Spec Direction" above). This rules out
  a CSRSS-re-registration approach (the "Winnie"-style undocumented
  `CsrClientConnectToServer` route): it would mean depending on an
  unsupported, security-research-grade internal API with no stability
  guarantee across Windows updates, which is a bad fit for a PAL meant to
  keep working for years.
- The [Cygwin mailing list](https://cygwin.com/pipermail/cygwin/2018-January/235755.html)
  ("fast/native fork?", Jay K, Jan 2018) independently proposes optimizing
  specifically for the "`exec()` follows `fork()` immediately" case rather
  than always paying the full memory-copy cost -- validating the general
  direction of *not* treating every `fork()` as needing the expensive
  machinery, even though that thread frames it as deferring `fork()` itself
  (closer to `vfork()` semantics) rather than what this project ended up
  choosing.
- No prior art was found for a **broker/proxy process** that performs
  `CreateProcessA` on behalf of an unregistered clone and hands the resulting
  process handle back via `DuplicateHandle`. That specific combination
  appears to be novel, but it composes two independently well-established,
  fully documented techniques (privilege-separated broker processes, e.g.
  Chrome's process model; and cross-process handle duplication, which this
  codebase already relies on extensively for fd/socket inheritance -- see
  "Export/Import Semantics" above). No undocumented NT internals are needed.

## Chosen Direction: Spawn Broker (Implemented)

The crash is not really a `fork()` problem: `fork()` via `RtlCloneUserProcess`
already works correctly and cheaply for the "fork, do direct syscalls,
`_exit()`" shape that most of this project's test suite exercises. The crash
is specifically that **`__crt_sys_posix_spawn()`/`execve()`, when called from
inside an unregistered clone, cannot call `CreateProcessA` itself.**
`execve()` does not need to preserve or resume the calling process's state
the way `fork()` does -- POSIX `execve()` either replaces the process image or
fails and returns an error -- so unlike a full `fork()` replacement, fixing
this does not require memory copying, `setjmp`/`longjmp` resume, `CONTEXT`
manipulation, or any assumption about matching stack/heap virtual addresses
between processes.

Instead, the plan is:

1. A single always-running **broker process** is started once by the
   original (CSRSS-registered, never cloned) process at startup -- e.g. the
   top-level `mksh` re-execs itself once with a special marker so the second
   instance runs as a broker instead of an interactive shell. The broker is
   never itself the target of `RtlCloneUserProcess`, so it is always
   properly registered and can call `CreateProcessA` indefinitely.
2. `fork()` is untouched -- still the existing, fast `RtlCloneUserProcess`
   path (see benchmark above: cheaper than `CreateProcessA`, no reason to
   change it).
3. `__crt_sys_posix_spawn()` gains a check for "is the current process an
   unregistered `RtlCloneUserProcess` clone" (a simple flag set in the
   `RtlCloneUserProcess` child-branch of `__crt_sys_fork()`, analogous to how
   `libc/src/tls.c`'s `__crt_thread_after_fork_child()` already distinguishes
   fork-child state). If set, instead of calling `CreateProcessA` directly
   (which crashes), it sends the spawn request (application path, command
   line, environment block, fd-snapshot/startup info -- the same data
   `prepare_spawn_startup()` already assembles) to the broker over a pipe,
   reusing the existing bootstrap-pipe transport pattern from
   `__crt_sys_posix_spawn()`'s `fd_snapshot_pipe_mode` branch (see
   "Existing CREATE_SUSPENDED + Patch-Then-Resume Pattern" investigation
   above).
4. The broker (itself registered, so `CreateProcessA` works normally) creates
   the real target process, then `DuplicateHandle()`s the resulting
   `hProcess`/`hThread` into the *requesting* (cloned) process -- this is
   known to work inside a clone, since `DuplicateHandle` was directly
   confirmed to succeed there while bisecting the original crash.
5. The clone receives the duplicated handle and treats it exactly as if it
   had called `CreateProcessA` itself: records it via the existing
   `remember_child_process()` bookkeeping, returns from `posix_spawn()`/
   `execve()` normally, and later `waitpid()`s on it normally --
   `WaitForSingleObject` on a process handle does not require a genuine
   Win32 parent-child relationship, so no special-casing is needed there.

This keeps `fork()` exactly as-is (still cheap, still correct for the common
case), touches only the one call path that actually crashes, and needs no
new architecture-specific code at all (no per-arch `CONTEXT` handling, no
memory copying, no ASLR/ stack-address assumptions to validate). It is a
larger design than a one-line fix -- a broker process lifecycle, an IPC
protocol for the spawn request, and wiring the detection flag through
`__crt_sys_posix_spawn()` -- but it is scoped narrowly to the one place that
is actually broken.

**Status: implemented and verified end-to-end on real Windows aarch64
hardware.** `cmake --build --preset windows-host-ninja-debug --target
port-rebuild-zlib` now completes `./configure && make && make install` for
zlib 1.3.1 with exit 0 -- the exact failure this whole investigation started
from. Full `ctest` stays green (77/77) throughout.

Implementation:
- `libc/include/private/crt_spawn_broker.h`: the wire protocol (request/
  response structs), env var names, and the shared function declarations.
- `libc/src/arch/windows/common/spawn_broker.c`: new file. Client side
  (`__crt_windows_ensure_spawn_broker()`, `__crt_windows_spawn_broker_request()`)
  and broker side (`__crt_windows_spawn_broker_main()`,
  `broker_handle_request()`).
- `libc/src/arch/windows/common/syscall.c`: `windows_unregistered_clone`
  flag set in `__crt_sys_fork()`'s clone branch, read via
  `__crt_windows_is_unregistered_clone()`; `__crt_sys_posix_spawn()` branches
  on it to call the broker instead of `CreateProcessA` directly.
- `libc/src/arch/windows/common/crt1.c`: `mainCRTStartup()` checks
  `CRT_SPAWN_BROKER_MODE` before the normal fd/rootfs bootstrap and, if set,
  dispatches straight into `__crt_windows_spawn_broker_main()` (never calls
  the program's own `main()`).
- `libc/CMakeLists.txt`: `spawn_broker.c` added alongside `syscall.c` for
  both Windows architectures.

One thing the design write-up above did not anticipate, found while
verifying: **`CreatePipe()` itself -- not just `CreateProcessA` -- fails with
`ERROR_INVALID_HANDLE` when called from inside an unregistered clone.**
Confirmed empirically with file-based checkpoint logging (the same
per-PID-file technique used earlier in this investigation, since
inherited-handle-based logging is exactly the kind of thing under test and
cannot be trusted here): a prewarm attempt (calling `CreatePipe()` once in
the still-registered parent before ever cloning, on the theory that this was
a one-time DLL/API-set resolution problem, matching the process-cloning
guide's "load necessary libraries beforehand" advice) did **not** fix it --
the clone's own first `CreatePipe()` call still failed with the same error
regardless. This means the failure is not one-time lazy-resolution state
that survives the copy-on-write clone; something about `CreatePipe()` itself
does not work in a clone at all, full stop. The fix follows the same shape as
the `CreateProcessA` fix: the bootstrap fd-snapshot pipe is now created by
the broker (a normal process, so `CreatePipe()` works fine there) rather than
by the clone. The broker keeps the read end (inheritable, in its own
process, so the `CreateProcessA` call that spawns the real target hands it
over directly) and returns the write end to the client via `DuplicateHandle`,
the same handle-handback pattern already used for `hProcess`/`hThread`. See
`crt_spawn_broker_request_header.want_fd_snapshot_pipe` and
`crt_spawn_broker_response.fd_snapshot_pipe_write` in `crt_spawn_broker.h`.

Two things called out in the original design as explicitly *not* needed
remain confirmed not needed: no memory copying, and no ASLR/stack-address
assumptions -- both were specific to the full Cygwin-style `fork()`
replacement that was set aside in favor of this narrower fix.

**Generic `pipe()` has the exact same failure mode, and needed the same
fix.** Found while working through libpng's `configure` (an autoconf script,
unlike zlib's hand-written one, so it forks far more subshells): mksh forks a
real `RtlCloneUserProcess` clone for every command substitution and
pipeline, and any further `pipe()` call made *from inside* that clone (a
nested command substitution, `cmd1 | cmd2` inside a subshell) hit the same
`CreatePipe()`-fails-in-an-unregistered-clone behavior described above --
`__crt_sys_pipe()` called `CreatePipe()` unconditionally, with no
`__crt_windows_is_unregistered_clone()` check at all, because the original
fix only ever routed `posix_spawn()`'s own internal bootstrap pipe through
the broker. Extended the broker protocol with a `want_plain_pipe` request:
the broker creates a pipe locally (same reasoning -- it is a normal
`CreateProcessA`-spawned process, so `CreatePipe()` works fine there) and
`DuplicateHandle`s *both* ends back into the client, rather than keeping one
end for itself the way `want_fd_snapshot_pipe` does. `__crt_sys_pipe()` now
checks `__crt_windows_is_unregistered_clone()` and routes through
`__crt_windows_broker_create_pipe()` when true. See
`crt_spawn_broker_request_header.want_plain_pipe` and
`crt_spawn_broker_response.plain_pipe_read`/`plain_pipe_write` in
`crt_spawn_broker.h`.

Open follow-ups, none blocking:
- the broker process currently has no explicit shutdown path beyond the
  `atexit()` hook registered by whichever process first started it
  (`shutdown_spawn_broker_atexit()` in `spawn_broker.c`); it is not yet
  reaped if that process is killed rather than exiting normally;
- `broker_handle_request()` services one request at a time (no threading);
  fine for the request rates seen so far, worth revisiting if a workload
  ever needs many concurrent spawns from sibling clones;
- only the `zlib` recipe has been fully verified end-to-end so far; `libpng`
  is in progress (see `TODO.md`, "in progressing") and gets much further now
  with the `regex.c`/mksh-path/generic-`pipe()` fixes above, but is not
  passing yet; `libffi` and the SQLite follow-up build have not been
  attempted;
- **every process the broker spawns is reported by Windows as a child of the
  broker, not of the clone that logically requested it.** This is a real,
  known gap -- see the "Attempted And Reverted" section immediately below
  for what was tried and why it was backed out, rather than left half-fixed.

## Attempted And Reverted: Reparenting Spawned Processes To The Client

Raised as a real concern (not just cosmetic): since the broker is the one
that actually calls `CreateProcessA`, Windows' own `ParentProcessId`
bookkeeping -- what Task Manager, Process Explorer, and any future toybox
`ps --forest` would show -- reports every spawned process as a child of the
broker. A shell's process tree would render flat (everything hanging off one
broker process) instead of nested the way it does on Linux, where a
subshell's forked child is the real parent of whatever it execs.

Windows has an official, documented mechanism for exactly this:
`STARTUPINFOEXA` + `UpdateProcThreadAttribute(..., PROC_THREAD_ATTRIBUTE_PARENT_PROCESS, ...)`,
the same API `explorer.exe` uses so a UAC-elevated child process appears to
descend from the requesting shell rather than from `explorer.exe` itself. The
broker was changed to open the requesting client with the added
`PROCESS_CREATE_PROCESS` right and pass that handle as the specified parent
when spawning the real target.

This surfaced a real, and non-obvious, interaction: **generic inheritable
handles (anything marked via `SetHandleInformation(..., HANDLE_FLAG_INHERIT, ...)`,
as opposed to the handles explicitly passed via `STARTUPINFO.hStdInput/
Output/Error`) are sourced from the *specified parent's* handle table when
`PROC_THREAD_ATTRIBUTE_PARENT_PROCESS` is used, not from the actual process
that calls `CreateProcessA`.** Confirmed empirically: marking the
fd-snapshot bootstrap pipe's read end inheritable in the broker's own
process (as the original, working design did) and closing the broker's copy
right after `CreateProcessA` -- correct without reparenting, since the real
target inherits directly from its actual creator -- produced
`ERROR_NO_DATA` ("the pipe is being closed") when the client later wrote to
the write end, because the real target never actually received a working
copy of the read end at all. The fix for *that* symptom was to have the
broker `DuplicateHandle()` the read end into the *client's* process
(inheritable there) instead, and patch the bootstrap env var with that
client-local value -- which did resolve `ERROR_NO_DATA` and got the
fd-snapshot handshake working again with reparenting enabled.

But a second, worse problem replaced it, and this is where the change was
backed out rather than pushed further: **spawned target processes then
failed to start at all**, exiting with a status whose low byte
(`WEXITSTATUS`) was consistently `66` -- which is exactly
`STATUS_DLL_INIT_FAILED` (`0xC0000142`) truncated to its low byte
(`0x42 = 66`), i.e. a Windows *loader-level* failure, before the target's own
`mainCRTStartup` (and therefore `main()`) ever ran. Isolating this took a
while and did not fully converge:
- it was **not** specific to any one target binary's imports -- a trivial
  `Sleep()`-only executable failed exactly the same way as one using
  `CreateToolhelp32Snapshot`;
- it was **not** a `CREATE_SUSPENDED`-specific issue -- launching the same
  binary suspended-then-resumed via a direct, non-broker, non-reparented
  `CreateProcessA` (no clone involved at all) worked fine;
- it was **not** consistently correlated with the reparenting attribute
  being *set* -- disabling the attribute list construction (forcing the
  broker back to a plain, non-reparented spawn) while keeping every other
  change from this session **still** produced `STATUS_DLL_INIT_FAILED` on
  a later run, which is the most important negative result here: something
  in this session's combination of changes broke the previously-working
  plain (non-reparented) spawn path too, and the two problems were never
  fully teased apart before time was called on the investigation;
- confirmed **not** a job-object inheritance issue (a documented side
  effect of `PROC_THREAD_ATTRIBUTE_PARENT_PROCESS`, since the specified
  parent's job memberships apply to the new process too): `IsProcessInJob`
  on the relevant processes returned `FALSE`.

Given the actual acceptance test that matters -- `port-rebuild-zlib`'s real
`configure && make && make install` through real mksh subshells -- **also**
regressed back to failing while this was in progress, the entire reparenting
attempt (both files' changes) was reverted via `git checkout --` back to the
last known-good commit, restoring the confirmed-working state (zlib passes,
`ctest` 77/77). None of the reparenting code exists in the tree today; this
section exists purely as a record for whoever picks this up next.

What the next attempt should do differently:
- treat "does a plain (non-reparented) broker-mediated spawn still work" as
  a checkpoint to re-verify after *every* incremental change, not just at
  the end -- the fact that this regressed too, and was never isolated from
  the reparenting-specific `STATUS_DLL_INIT_FAILED`, is the main reason this
  investigation didn't converge;
- consider testing the handle-inheritance-source finding (duplicate into
  the client, not the broker) as its own, isolated, committed change first,
  fully re-verified against `port-rebuild-zlib`, *before* layering the
  `PROC_THREAD_ATTRIBUTE_PARENT_PROCESS` attempt on top of it;
- if `STATUS_DLL_INIT_FAILED` reappears, capture a crash dump or attach a
  debugger to the suspended target *before* resuming it, rather than relying
  on exit-code archaeology after the fact;
- it remains genuinely unclear whether `PROC_THREAD_ATTRIBUTE_PARENT_PROCESS`
  is fundamentally incompatible with a specified parent that is itself an
  unregistered `RtlCloneUserProcess` clone (plausible: the new process's own
  CSRSS registration may be derived from the specified parent's, and the
  clone has none) or whether this was a solvable bug in this session's
  specific implementation. Both remain open.
- Given `ps` is not currently enabled in this project's toybox build
  (`CFG_PS 0`) and `getppid()` already unconditionally returns `0` on
  Windows regardless of broker involvement, the impact of leaving this
  unfixed is currently limited to external tooling (Task Manager, Process
  Explorer) showing a flat tree -- annoying for debugging, not something any
  in-tree functionality depends on today.

### Rejected alternatives (recorded so they are not re-litigated later)

- **CSRSS re-registration** (the "Winnie"-style approach: intercept the
  clone's own initialization and call undocumented internals like
  `CsrClientConnectToServer` to establish a fresh, legal CSRSS connection
  matching the clone's new PID): rejected because there is no documented,
  stable way to do this (confirmed via the process-cloning research above),
  and depending on unsupported internal APIs is a poor fit for a PAL meant to
  track multiple Windows versions over a long time.
- **Full Cygwin-style memory-copy fork** (replace `__crt_sys_fork()` itself
  with `CreateProcessA` + `WriteProcessMemory` + `setjmp`/`longjmp` resume):
  not rejected outright -- it remains a valid fallback if the broker design
  turns out to be insufficient for some case the broker can't handle -- but
  set aside as the *first* thing to build, because it is strictly more work
  (new per-architecture `CONTEXT`/`SetThreadContext` code, heap/stack/TLS
  copying, an unverified stack-address-determinism assumption) to fix a
  problem that, on inspection, does not actually require touching `fork()`
  at all.

## Spawn Broker Retired: Moving To Full Cygwin/MSYS-Style `fork()`

The broker fixed zlib end to end and got libpng most of the way, but kept
generating new structural failure modes of its own rather than converging:
recurring orphaned `mksh.exe` processes, named-pipe instance-exhaustion and
lost-response races (both fixed, see "done" entries in `TODO.md`), missing
I/O timeouts (also fixed), and finally the process-tree-reparenting attempt
above, which regressed the previously-working state (`STATUS_DLL_INIT_FAILED`)
and had to be reverted rather than shipped half-fixed. Each fix bought
correctness in one dimension while the broker's fundamental shape --
a second, always-running process privileged to do the one thing a clone
can't -- kept exposing new edges.

Decision: retire the broker and build the "full Cygwin-style memory-copy
fork" alternative that was deliberately set aside above ("Rejected
alternatives", not rejected outright). Once a real `fork()` produces a
properly `CreateProcessA`-registered child, there is no "unregistered
clone" state left for anything to work around -- the problem this whole
document is about disappears at the root instead of being patched call site
by call site.

**Phase A (done):** `spawn_broker.c`/`crt_spawn_broker.h` moved to
`libc/src/arch/windows/legacy_spawn_broker/` -- kept for reference, excluded
from `libc/CMakeLists.txt`. The three call sites in `syscall.c`
(`__crt_sys_open`/`__crt_sys_pipe`/`__crt_sys_posix_spawn`) that branched on
`__crt_windows_is_unregistered_clone()` were reverted to their pre-broker
direct-Win32-call form; `crt1.c`'s `CRT_SPAWN_BROKER_MODE` dispatch was
removed. `windows_unregistered_clone`/`__crt_windows_is_unregistered_clone()`
themselves were left in place in `syscall.c` (harmless, still set by
`__crt_sys_fork()`'s clone branch) since the new `fork()` may still want
this state in some form. Full `ctest` (78/78) confirmed unaffected --
current coverage does not exercise fork-then-spawn-from-inside-a-clone
directly. This is an accepted, temporary regression for any real workload
that does need that (subshells calling external commands inside
`configure`, e.g. the in-progress libpng build) until Phase C lands.

**Phase B (done):** the overhead benchmark above already showed
`CreateProcessA` is not prohibitively slower than `RtlCloneUserProcess`
(~1.2x). The remaining open risk was entirely about correctness, not
performance: does an ASLR-disabled executable actually load its
image/heap/stack at the same virtual addresses across repeated
`CreateProcessA` launches? A standalone probe tested this directly on real
Windows aarch64 hardware and found two things:

1. `/DYNAMICBASE:NO` -- the originally planned mechanism -- is rejected by
   `lld-link` on aarch64 outright: `error: /dynamicbase:no is not
   compatible with arm64`. ARM64 PE images must always be relocatable;
   there is no link-time way to disable image ASLR on this architecture.
2. A different, per-process mechanism works instead:
   `STARTUPINFOEXA` + `UpdateProcThreadAttribute(...,
   PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY, ...)` with
   `PROCESS_CREATION_MITIGATION_POLICY_BOTTOM_UP_ASLR_ALWAYS_OFF |
   _HIGH_ENTROPY_ASLR_ALWAYS_OFF` set at `CreateProcessA` time. A probe
   executable spawned this way landed its first `malloc()` allocation and a
   stack-local variable's address at byte-identical values across 2
   independent top-level launches x 20 children each (42/42 addresses
   matching, both heap and stack). The image's own code address
   (`&some_function`) was already deterministic on this system even
   *without* the mitigation policy -- only the heap/stack needed it, since
   those are randomized by a separate "bottom-up ASLR" mechanism, not by
   the PE `/DYNAMICBASE` characteristic bit. Because this is a
   process-creation attribute rather than a link-time image flag, it should
   in principle be architecture-independent (only verified on aarch64 so
   far).

**The Phase C address-matching assumption is confirmed feasible**, using
the mitigation-policy mechanism in place of the originally planned
`/DYNAMICBASE:NO`.

**Phase C (done, aarch64): implemented and verified.** `__crt_sys_fork()`
now dispatches to `__crt_windows_memcopy_fork()`
(`libc/src/arch/windows/aarch64/fork_memcopy.c`) on aarch64, keeping the
original `RtlCloneUserProcess` path unchanged on x86_64. Full `ctest`
(78/78) passes with the new mechanism active, including
`fork_test`/`fork_signal_test`/`fork_runtime_reset_test`.

Implementation, in the order it actually happens:

1. `setjmp()` at the top of `__crt_windows_memcopy_fork()` captures the
   parent's callee-saved registers/Sp/Lr.
2. Self-relaunch via `CreateProcessA(..., CREATE_SUSPENDED, ...)` with the
   `PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY` attribute set (as verified in
   Phase B) -- the child never runs `mainCRTStartup` at all; its very
   first instruction is whatever the parent points its `CONTEXT.Pc` at
   below.
3. `WriteProcessMemory` copies three categories of memory into the child
   at identical addresses: the `malloc.c` heap (via a dedicated OS-region
   tracking table, `__crt_malloc_os_region_count()`/`_base()`/`_size()`
   in `malloc.c` -- **not** a walk of malloc's own `block_header` split
   chain, which subdivides one 64KB `mmap()` region into several
   non-64KB-aligned sub-blocks that `VirtualAllocEx()` rejects), the
   image's own writable PE sections (`.data`/`.bss`, found via manual PE
   header parsing -- these are already committed by the loader, so this
   step is a plain `WriteProcessMemory` with no `VirtualAllocEx()` at
   all), and the calling thread's stack (commit-only, since the loader
   already reserves the full stack region at thread creation -- adding
   `MEM_RESERVE` on top of an existing reservation fails).
4. The current thread's `crt_thread_context` block (tls.c) is copied
   explicitly (a standalone `VirtualAlloc()`, not covered by the above).
5. The copied `thread_tls_index` (part of the `.data` copy) is patched
   back to `CRT_TLS_OUT_OF_INDEXES` in the child, so the existing
   post-fork hook chain (`__crt_atfork_child()` -> `__crt_thread_after_
   fork_child()` -> `__crt_thread_set_current()` -> `windows_tls_index()`
   in tls.c, none of which needed to change) allocates a fresh,
   legitimate TLS index there instead of reusing the parent's stale one.
6. The child's initial `CONTEXT` is constructed **directly** from the
   values `setjmp()` captured in step 1 (`X19`-`X28`, `Fp`, `Sp`, `D8`-
   `D15`, and `Pc` set to the captured `Lr`, `X0` set to 1 to match
   `setjmp()`'s own longjmp-return convention) via `SetThreadContext()`,
   then `ResumeThread()`.

Step 6 is not what the design write-up above originally described.  The
first working version used a small **trampoline function**: redirect
`CONTEXT.Pc` to a real function that itself called `longjmp()` on a
`jmp_buf` copied into the child's memory (matching the design as
written). This reproducibly crashed with `STATUS_ACCESS_VIOLATION`
(DEP/execute violation) inside `longjmp()`'s own restore sequence.
Bisected using `WaitForDebugEvent()`/`ContinueDebugEvent()` (the child
spawned with `DEBUG_PROCESS`, letting the parent observe the exact
faulting instruction and register state -- ordinary in-process exception
handlers were not reliable here) down to: at the moment the trampoline
tried to read the copied `jmp_buf` back out of child memory, the bytes
had gone from byte-identical (confirmed via a `ReadProcessMemory()`
readback taken immediately before `ResumeThread()`) to all zero by the
time the child's own code executed. The exact mechanism was not fully
isolated (suspected interaction between the unusually large upfront
stack commit and the loader/`ntdll` initialization that still runs even
though the "user" entry point is redirected), but the fix sidesteps the
question entirely: since the parent already holds every value the
trampoline would have read from memory in its own local variables/
registers, it writes them straight into the child's `CONTEXT` itself via
`SetThreadContext()`, and the child never needs to read `jmp_buf` state
back out of copied memory at all. This is a strictly more robust design,
not just a workaround -- worth keeping even if the memory-corruption
mechanism above were fully understood.

**Every Windows aarch64 process now self-relaunches once at startup**
under the same mitigation policy (`libc/src/arch/windows/common/crt1.c`,
`windows_aarch64_ensure_mitigated_relaunch()`, gated by the
`CRT_FORK_MITIGATED` environment marker to relaunch only once). This was
not originally anticipated: Phase B verified that *children spawned under
the mitigation policy* get deterministic addresses, but the *parent
process itself* -- launched normally, with ordinary ASLR -- does not,
so its own heap/stack addresses would never match what a later `fork()`
call's mitigated child gets. Every CRT-built program on Windows aarch64
now pays one extra process launch's worth of startup latency for this,
whether or not it ever calls `fork()` -- a real, accepted cost of making
`fork()` possible at all with this mechanism.

Known limitations, not yet addressed:
- only the calling thread's stack is copied (matches POSIX `fork()`
  semantics -- other threads do not survive into the child -- but pthread-
  created OS threads' own stacks are not otherwise touched or cleaned up
  in the child beyond the existing `__crt_pthread_after_fork_child()`
  hook);
- the stack copy does not preserve a guard page beyond what was already
  committed at fork() time, so the child cannot auto-grow its stack past
  that point;
- x86_64 still uses the original `RtlCloneUserProcess` path and does not
  get this mechanism (or the startup self-relaunch cost) at all -- would
  need its own CONTEXT layout, its own Phase B address-determinism
  verification (untested whether the same mitigation-policy approach even
  applies there, since `/DYNAMICBASE:NO` links fine on x86_64, unlike
  aarch64), and its own TEB-access mechanism (X18 is aarch64-specific;
  x86_64 uses the GS segment register).
