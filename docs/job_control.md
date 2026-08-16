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

## Interactive Job Control: Decided Policy, Not Yet Implemented

**Scope note first**: this project's own mksh build defines
`MKSH_NOPROSPECTOFWORK` unconditionally, for every host (`shell/CMakeLists.txt`
-- not Windows-specific), which auto-defines mksh's own `MKSH_UNEMPLOYED` and
compiles out mksh's entire internal job-control implementation (`fg`/`bg`,
Ctrl-Z suspend, tty-pgrp save/restore, `FMONITOR`) everywhere, not just
Windows. mksh has no interactive job control on any host this project builds
for today -- the non-interactive configure/make-driving use case never needed
it. The policy below is a **forward-looking design decision**, recorded so
whoever eventually re-enables `MKSH_UNEMPLOYED` (or builds interactive shell
support some other way) has a concrete starting point, not a description of
current runtime behavior. No code changes accompany this section.

### Ctrl-C / Ctrl-Break delivery

Real Win32 mechanics that shape this: `GenerateConsoleCtrlEvent(CTRL_C_EVENT,
0)` always broadcasts to every process attached to the current console and
cannot be targeted at one process group; `CTRL_BREAK_EVENT` *can* be targeted
at a specific process group via that same call's group-id argument. A process
created with `CREATE_NEW_PROCESS_GROUP` (already used by this PAL's
`posix_spawn()` for `POSIX_SPAWN_SETPGROUP`/`POSIX_SPAWN_SETSID`, see
`libc/src/arch/windows/common/syscall.c`) is automatically exempted from
`CTRL_C_EVENT` by Windows itself, unless it explicitly re-enables handling via
`SetConsoleCtrlHandler(NULL, FALSE)`. A registered `HandlerRoutine` runs on a
**new thread** inside the process, asynchronously, with none of a real POSIX
signal handler's restrictions -- and none of its safety either.

Decided policy: bridge console control events into the *same*
`signal_actions[]`/`raise()` dispatch `libc/src/arch/windows/common/
signal_backend.c` already uses for `SIGCHLD`, following that file's own
established pattern rather than inventing a new one -- but, deliberately,
**not** synchronously from the handler thread itself. The handler thread's
only job is to set an atomic "SIGINT is pending" flag (the same shape as
`__crt_windows_check_sigchld_pending()`'s own child-registry scan); actual
dispatch (`__crt_signal_dispatch(SIGINT)`) happens on the main thread, at the
same checkpoints `SIGCHLD` already uses (`pselect()`/`select()`/`poll()`, plus
any future checkpoints added the same way). This keeps the existing,
already-verified single-threaded assumption in `signal.c`'s dispatch path
intact instead of introducing real concurrent-access hazards for a first
pass, at the same accepted cost `SIGCHLD` already has: a plain blocking
`read()`/`write()` with no polling checkpoint in between still cannot be
interrupted (see `docs/signal_delivery.md`'s own scope note). Both
`CTRL_C_EVENT` and `CTRL_BREAK_EVENT` map to `SIGINT` -- Bionic's own
`signal.h` (see `include/signal.h`) has no `SIGBREAK`, and upstream mksh/ksh
convention treats a Ctrl-Break interrupt the same as Ctrl-C for this purpose.

### Foreground process-group approximation

The existing `CREATE_NEW_PROCESS_GROUP` mechanism turns out to already
provide most of real POSIX foreground/background Ctrl-C semantics for free,
without needing per-job `GenerateConsoleCtrlEvent` targeting for the common
case: a job launched in a *new* Windows process group (background jobs,
`POSIX_SPAWN_SETPGROUP`/`SETSID`) is automatically exempt from `CTRL_C_EVENT`
by Windows itself, exactly matching "only the foreground job's process group
receives `SIGINT`" -- while a foreground job, left in the shell's own
console process group (no `CREATE_NEW_PROCESS_GROUP`), naturally receives the
same broadcast Ctrl-C the shell itself does, alongside it. Decided policy:
keep foreground jobs out of a new process group entirely (matching the
current default); only background (`&`) jobs and explicit job-control
candidates get `CREATE_NEW_PROCESS_GROUP`. The one piece this project's
CRT-managed `pgid` (a plain integer today, per "Windows" above, tied to
nothing real) still needs once job control is real: record the mapping from
that integer to the real Windows process-group id (the group leader's own
PID, per `GenerateConsoleCtrlEvent`'s own semantics) at the point a job is
actually spawned into a new process group, so `tcsetpgrp()`-driven foreground
arbitration and an explicit `CTRL_BREAK_EVENT` targeted at one specific
background job (rarer, but occasionally needed -- e.g. a job-control `kill
-INT %1`) both have a real id to act on instead of an opaque local integer.

### Stopped-child status

No real Windows equivalent exists for POSIX's `SIGTSTP`/`SIGSTOP`-driven
"stopped" process state (`WIFSTOPPED`) -- no OS-level suspend-a-process
signal comparable to `SIGCHLD`'s real signaled-process-handle mechanism this
PAL already bridges. Low-level alternatives exist (per-thread `SuspendThread`,
the undocumented `NtSuspendProcess`) but this project has consistently
avoided reaching for undocumented NT internals elsewhere, and a
signal-shaped POSIX suspend built out of them would need a fair amount of new
machinery for a feature mksh itself has compiled out anyway (`MKSH_UNEMPLOYED`
above). Decided policy: stay honest rather than fake it -- `SIGTSTP`/`SIGSTOP`
remain pure software bookkeeping (self-delivery only via `raise()`, matching
`signal_backend.c`'s already-documented policy for every signal besides
`SIGCHLD`/the `SIGINT` bridge above), and `waitpid()`/`wait()` never report
`WIFSTOPPED` for a real Windows child. Stopped-child support stays explicitly
out of scope until real interactive job control (`MKSH_UNEMPLOYED`
re-enabled) is an actual priority, not a "later" this document leaves
ambiguous.

**Investigated (2026-08-16), still deferred -- design for whoever picks this
up.** Confirmed there is no *documented* way to suspend an entire other
Windows process. Considered and rejected: `SuspendThread()` per-thread alone
(real race -- a thread created between enumeration and suspension escapes
it); `DebugActiveProcess()` + `SuspendThread()` (attaches a real debugger,
broader side effects on the target's own exception handling); Job Object
`Freeze` (`JobObjectFreezeInformation`) -- checked directly against this
project's own Windows SDK headers (`.../Windows Kits/10/Include/
10.0.28000.0/um/winnt.h`): Microsoft's own public header leaves this value
nameless (`JobObjectReserved1Information = 18`), with no accompanying struct
in `jobapi2.h` either -- **not** more official than the option below, and
arguably less battle-tested. If this is ever built: Job Objects for
grouping/descendant tracking (`CreateJobObject`/`AssignProcessToJobObject`/
`QueryInformationJobObject` -- fully documented, and solves "which processes
belong to this job" including grandchildren for free) + `NtSuspendProcess`/
`NtResumeProcess` (undocumented, but stable since Windows XP and what
Process Explorer/Process Hacker/PowerToys/PowerShell's own
`Suspend-Process`/`Resume-Process` all actually use) applied to each tracked
pid for the actual freeze/thaw action. Using the latter would still be an
explicit, narrow reversal of this project's "avoid undocumented NT
internals" pattern, not a general policy change -- would need its own
documented justification/risk/mitigation at the time, not silently. Also
confirmed via `docs/runtime_roadmap.md`: none of the planned upper-runtime
components need this (see `TODO.md`'s "Interactive job control" section) --
there is no roadmap pressure to build it, only a possible future interactive
UX want.
