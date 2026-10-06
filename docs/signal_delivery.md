# Signal Delivery

This document records how CRT signals reach the real host OS, why that was
missing, and what it took to fix it.

## Problem

`libc/src/signal.c`'s `sigaction()`/`raise()` were pure software bookkeeping:
`signal_actions[]` tracked what handler should run, and `raise()` invoked it
directly and synchronously. Nothing ever told the real kernel to route an
actual signal -- a child exiting and generating `SIGCHLD`, a real `kill()`
from another process, Ctrl-C -- through that bookkeeping. `sigprocmask()` had
the same gap: it only updated a process-local mask, never the host's real
signal mask.

This is invisible until something relies on a *real* OS signal interrupting a
blocking host call. GNU make's `jobserver_acquire()` is exactly that: it
calls `pselect()` on the jobserver token pipe with `SIGCHLD` blocked
everywhere except atomically during the `pselect()` call itself, relying on
the kernel to deliver an already-pending or in-flight `SIGCHLD` as an
`EINTR`. With no real delivery path at all, `pselect()` blocked forever even
after every child had already exited -- the concrete symptom that started
this work: `port-rebuild-zlib`'s `make -j 10` hanging indefinitely.

## Architecture

A new private interface, `libc/include/private/crt_signal_backend.h`, sits
between the existing bookkeeping in `signal.c` and one backend per host under
`libc/src/arch/{linux,macos,windows}/common/signal_backend.c`:

```c
enum crt_signal_backend_action { CRT_SIGNAL_BACKEND_DEFAULT, CRT_SIGNAL_BACKEND_IGNORE, CRT_SIGNAL_BACKEND_DISPATCH };
int  __crt_signal_backend_set_action(int bionic_sig, enum crt_signal_backend_action action);
int  __crt_signal_backend_set_mask(int how, const sigset_t* set);
void __crt_signal_dispatch(int bionic_sig);
```

`sigaction()` calls `__crt_signal_backend_set_action()` after updating
`signal_actions[]`, so every disposition change also reaches the real host.
`sigprocmask()` calls `__crt_signal_backend_set_mask()` the same way, so
blocking host calls (`pselect()`, `poll()`, ...) actually observe the
block/unblock state. `__crt_signal_set_mask()` (used by
`POSIX_SPAWN_SETSIGMASK`) and `__crt_signal_reset_defaults()` (used by
`POSIX_SPAWN_SETSIGDEF`) also push to the backend, because a real POSIX mask
and `SIG_IGN` disposition both survive `exec()` at the host level, not just in
this process's own bookkeeping.

`__crt_signal_dispatch()` is the other direction: each backend's own
OS-level signal entry point calls it after translating the host's native
signal number back to Bionic/Linux numbering. It looks up `signal_actions[]`
and invokes the registered handler exactly like a self-directed `raise()`
would (both now share one `deliver_signal()` helper in `signal.c`).

### macOS

Real delivery needs the *real* `sigaction()`/`sigprocmask()` from
`libSystem.B.dylib`, despite this libc defining public symbols with those
exact same names. Resolving them via `dlopen()`/`dlsym()` was the obvious
approach, but `libdl` depends on `libc` (`libdl/CMakeLists.txt` links `dl`
against `c`), so `libc` calling into `libdl` here would be a circular target
dependency.

The fix: the Mach-O export-trie parsing engine that used to live entirely
inside `libdl/src/arch/macos/dl_macos.c` was extracted into a shared,
libc-private helper --
`libc/include/private/crt_macho_symbol.h` /
`libc/src/arch/macos/common/macho_symbol.c`. Both
`libc/src/arch/macos/common/signal_backend.c` (to resolve the real
`sigaction`/`sigprocmask`) and `libdl/src/arch/macos/dl_macos.c` (for general
`dlsym()`) call into it; `libdl/CMakeLists.txt` adds `libc/include` to its
own targets' include path so `dl_macos.c` can see the private header --
`libdl` already fully depends on `libc`, so this widens an existing
dependency rather than adding a new one. See `docs/dynamic_loading.md` for
the trie-parsing details themselves; nothing about the algorithm changed,
only where it lives.

The backend registers a plain C function
(`crt_macos_signal_entry`) as the real handler via the real `sigaction()`,
translating between Bionic/Linux signal numbers and Darwin's (they differ
for several signals -- `SIGBUS`, `SIGUSR1`/`2`, `SIGCHLD`, `SIGSTOP`/`TSTP`/
`CONT`, `SIGURG`, `SIGIO`, `SIGSYS`) via a static lookup table in both
directions, and does the same translation for `sigprocmask()`'s 32-bit
Darwin mask and different `SIG_BLOCK`/`UNBLOCK`/`SETMASK` values (`1`/`2`/`3`
on Darwin vs. `0`/`1`/`2` on Bionic/Linux).

One implementation pitfall: the local Darwin-shaped `struct sigaction`
originally used field names `sa_handler`/`sa_sigaction`. This libc's own
`<signal.h>` `#define`s those names (`sa_handler` -> `__sigaction_handler.
sa_handler`) for its own public `struct sigaction`, and the macro rewrote the
unrelated Darwin struct's fields too, producing a `expected ')'` compile
error. Fixed by renaming the Darwin struct's union members to
`handler_plain`/`handler_siginfo`.

### Linux

CRT Linux executables are linked `-nostdlib -nostartfiles -nodefaultlibs` and
own their entire syscall surface, so there is no libSystem-equivalent to
dlsym from. The backend calls the raw `rt_sigaction(2)`/`rt_sigprocmask(2)`
syscalls directly (new stubs in `libc/src/arch/linux/{x86_64,aarch64}/
syscall.S`), matching Android Bionic's own `libc/bionic/sigaction.cpp`.

This project's own public signal numbering, `SA_SIGINFO`, and
`SIG_BLOCK`/`UNBLOCK`/`SETMASK` values already match the real Linux kernel
ABI exactly (Linux is Bionic's native platform), so -- unlike macOS -- no
translation table is needed. `sigset_t` is a plain 64-bit `unsigned long`,
the same size the kernel expects for the `sigsetsize` argument the raw
syscalls require, so masks pass straight through too.


**Real `siginfo_t` and `ucontext_t` on Linux (2026-10-04).** `siginfo_t` has the
Bionic/Linux layout (a 128-byte record with the `si_pid`/`si_uid`/`si_status`/
`si_addr`/`si_value`/... accessors over its union) on every host, and on Linux the
signal backend now forwards the kernel's own `siginfo_t` and `ucontext_t` to an
`SA_SIGINFO` handler (`__crt_signal_dispatch_info`) instead of a synthesized record
and a null context. `ucontext_t`/`mcontext_t` are the Bionic/kernel layouts on Linux
x86_64 (`uc_mcontext.gregs[REG_RIP]`, ...) and aarch64 (the sigcontext), because
JavaScriptCore reads them in its thread-suspend handler; `getcontext`/`swapcontext`
save into the same slots. Windows keeps the private, opaque `mcontext_t` and
the synthesized record (a self-directed `raise()` also synthesizes it everywhere); macOS/arm64 does
the same conversion as Linux, see below.
`stack_t` follows the kernel order (`ss_sp`, `ss_flags`, `ss_size`).

**Per-thread masks, `sa_mask`/flags and a real `sigsuspend` on Linux (2026-10-04).** The signal mask is
the calling thread's real kernel mask (`sigprocmask`/`pthread_sigmask` use `rt_sigprocmask`; a new thread
inherits its creator's); `sigaction` passes `sa_mask` and `SA_RESTART`/`SA_NODEFER`/`SA_ONSTACK`/
`SA_NOCLDSTOP`/`SA_NOCLDWAIT` to the kernel (`SA_RESETHAND` is emulated by resetting CRT's table before the
handler runs); `sigsuspend` is `rt_sigsuspend`, so it blocks until a handler has run. WTF suspends a thread
by signalling it and parking it in `sigsuspend`; the old stub returned at once and let a "suspended"
thread run on. `pause()` polls the delivery generation (10 ms) on every host. Windows has its own
implementation, see "Windows: thread-directed signals" below. `signal_threads_test` covers the per-thread
behaviour on Linux and Windows.

**Real signals on macOS/arm64 (2026-10-04, Web Tranche 1C replay).** CRT threads on macOS are real Apple
pthreads (`control->native_thread`), so the backend (`libc/src/arch/macos/common/signal_backend.c`) forwards
to libSystem's own `sigaction`, `pthread_sigmask`, `sigsuspend` and `pthread_kill` (found through the Mach-O
export-trie lookup above, not `dlsym`). The kernel's Darwin `siginfo_t` and arm64 `mcontext64` are converted to
the Bionic `siginfo_t` and the Linux aarch64 `ucontext_t`/sigcontext layout before the handler runs
(`libc/include/private/crt_linux_ucontext_aarch64.h`), and the registers a handler wrote are written back before
the kernel context is restored, so JavaScriptCore's suspend/resume and VM-trap handlers see the same shapes as
on Linux. Signal numbers are mapped Bionic to Darwin and back. Only aarch64 is converted: on macOS/x86_64 the
backend still reports "not provided" (open). `signal_threads_test` runs the Linux cases on macOS, and mutation
of the register write-back makes it fail.

x86_64's `rt_sigaction` requires `SA_RESTORER` plus a real, executable
restorer address (a tiny trampoline the kernel jumps to after running the
handler, whose only job is `rt_sigreturn(2)`); aarch64 needs neither --
the kernel supplies its own default restorer from the vDSO. The x86_64
restorer (`__crt_signal_restore_rt`, in `syscall.S`) is
`movq $15, %rax; syscall`, stripped of Bionic's CFI/unwind decorations
(debugger-quality-only, not functionally required), with no trailing `ret`
since `rt_sigreturn` never returns normally.

This project's CMake presets refuse to cross-compile
(`cmake --preset linux-host-ninja-debug` fails immediately unless run on an
actual Linux host), so this backend was originally code-review-verified
against Android Bionic and the Linux kernel UAPI headers only, without being
built or executed. See "Linux Verification" below for the real-host
follow-up that closed that gap. Same caveat still applies to the
Linux/Windows portions of `docs/dynamic_loading.md`.

### Windows: thread-directed signals (2026-10-05)

Windows/arm64 (2026-10-06) uses the same machinery with the ARM64 `CONTEXT` and a Linux-aarch64 `ucontext_t` handler frame
(`private/crt_linux_ucontext_aarch64.h`) instead of the x86_64 layout; `brk` raises SIGTRAP with the pc on the instruction.

The Windows signal VM-trap gate for JavaScriptCore (`JSC_usePollingTraps=false`) needs what Linux's kernel gives for free,
so `libc/src/arch/windows/common/signal_backend.c` builds it, same-process and thread-directed only:

- **State.** `crt_signal_state` in each thread's `crt_thread_context`: mask, pending set, wake event, a duplicated thread
  handle, `alive`/`waiting`/`in_wait` flags. The initial thread is attached in `__crt_env_set_initial`, others in
  `pthread_start`; a new thread starts with its creator's mask; after `fork()` the child re-attaches.
- **Asynchronous delivery** (`pthread_kill` to another CRT thread): `SuspendThread` -> `GetThreadContext` -> frame
  `{CONTEXT, ucontext_t (Linux x86_64 layout), siginfo_t}` on the target's stack below `rsp - 128` -> the handler's mask
  (old mask | `sa_mask` | the signal unless `SA_NODEFER`) is stored while the target cannot run -> `SetThreadContext` to
  `__crt_windows_signal_entry` -> `ResumeThread`. After the handler the stub copies the (possibly modified) `ucontext_t`
  back and restores it with `RtlRestoreContext` (resolved from ntdll at attach time). Signals that arrived while the mask
  held them are delivered with the same interrupted context before the restore.
- **Not interrupted:** a thread whose RIP is in ntdll/kernelbase/kernel32/win32u, whose mask blocks the signal, or whose
  stack cannot be written; the signal stays pending. The CRT's own blocking waits are therefore interruptible instead:
  `__crt_wait32` (mutex/cond/sem/once), `pthread_join` and `nanosleep` bracket the blocking call with
  `__crt_windows_signal_wait_begin/end` (`private/crt_signal_wait.h`); the sender wakes the thread
  (`WakeByAddressAll`/its event) and the handler runs on the waiting thread with a `getcontext` snapshot. `sigsuspend`
  waits on the wake event, atomically with its temporary mask, and returns `EINTR` after the handler. `poll`/`select`/file
  I/O waits are not interruptible yet (a signal sent during them is delivered when they return).
- **Hardware exceptions** reach handlers through a vectored exception handler installed when a handler is set for
  SIGTRAP/SIGSEGV/SIGILL/SIGFPE: `int3` (RIP + 1, as Linux reports it), access violation, illegal instruction, divide by
  zero. A handler may rewrite `REG_RIP`; unhandled exceptions keep the existing controlled-exit path.
- **Windows-only context state** lives in slots a signal frame does not use: rdi/rsi in `gregs`, xmm6-xmm15 in
  `__fpregs_mem._xmm[6..15]`, the TEB stack bounds in `__reserved1[0..2]` (offsets in `private/crt_ucontext_offsets.h`,
  checked against `offsetof()` in `libc/src/ucontext.c`).
- **Out of scope:** `kill(pid)`, process groups, SIGCHLD from other processes (unchanged), threads not made by
  `pthread_create`, async-signal-safety beyond what the handler itself guarantees.

Evidence: `signal_vmtrap_test` (100/100 runs), `signal_threads_test`, and JavaScriptCore with polling traps off
(`jsc_watchdog_test` 100/100); see `HISTORY.md` and `docs/crtweb_acceptance.md`.

### Windows (SIGCHLD)

No longer a no-op stub for `SIGCHLD`: Windows has no kernel mechanism that
generates a `SIGCHLD`-equivalent *async* signal the way Linux/macOS do, but
it does have everything needed to build a real, synchronously-polled
equivalent, reusing state that already exists for `waitpid()` -- the child
registry (`child_process_table` in `libc/src/arch/windows/common/
syscall.c`, see `docs/windows_fork_emulation.md`) already holds a live
process `HANDLE` per not-yet-reaped child, and a process `HANDLE` becomes
kernel-signaled the moment that process exits (that is what `WaitForSingle
Object()` already polls for `waitpid()` itself).

`__crt_windows_check_sigchld_pending()` (`syscall.c`) is the core of it: a
cheap, non-blocking scan of that registry (`WaitForSingleObject(handle, 0)`
per live child) that returns 1 the first time it finds a live child whose
handle has become signaled *and* `SIGCHLD` is currently unblocked (checked
via the existing `__crt_signal_get_mask()`), marking that child's slot in a
new parallel `child_notified_table` so the same exit is not reported again --
matching real `SIGCHLD`'s edge-triggered semantics (delivered once per state
transition, not repeatedly while a zombie sits unreaped). If `SIGCHLD` is
currently blocked, the function deliberately does *not* mark anything, so a
later call -- once unblocked -- still finds the exit: this is what gives
Windows the same "pending while blocked, delivered on unblock" behavior the
real kernel provides for free on Linux/macOS.

Two call sites use it, matching the two points a real kernel would actually
need to deliver:

- `__crt_signal_backend_set_mask()` (`signal_backend.c`): called every time
  `sigprocmask()` changes the software mask. If a call unblocks `SIGCHLD`
  and a child had already exited while it was blocked, this calls
  `__crt_signal_dispatch(SIGCHLD)` synchronously, right there -- mirroring
  real kernel signal delivery happening on the way back to userspace from
  the *same* `sigprocmask()` syscall that does the unblocking on Linux/
  macOS. This is what `pselect()`'s existing atomicity check (see above)
  actually observes on Windows: `pselect()` itself needed no Windows-
  specific change at all.
- `__crt_sys_poll()`'s own blocking loop (`syscall.c`): checked once per
  iteration (already looping on a 1ms `Sleep()`, since this Windows `poll()`
  is a hand-rolled busy-wait, not a single blocking syscall), covering a
  child that exits *while* a `pselect()`/`select()`/`poll()` call is
  genuinely blocked rather than having already exited beforehand.

Because `__crt_signal_dispatch()` is called synchronously from whichever
thread is already running `sigprocmask()` or `__crt_sys_poll()` -- never
from a separate background thread -- the handler still runs on the same
thread that would have been "interrupted", matching real single-threaded
POSIX signal delivery; no new locking or threading was introduced anywhere
in this design.

**Scope.** This covers `pselect()`/`select()`/`poll()` interruption by a
real `SIGCHLD` -- the concrete case that motivated this whole backend
interface (GNU make's `jobserver_acquire()`). It deliberately does *not*
cover interrupting a plain blocking `read()`/`write()`/etc with `EINTR`:
those are single Win32 syscalls with no polling loop to hook a check into
here, and making them interruptible would need a much larger overlapped-I/O
rework -- out of scope for this fix.

Every signal other than `SIGCHLD` stays exactly as before: pure software
`signal_actions[]` bookkeeping, self-delivery only via `raise()`/`abort()`.
Console control events (Ctrl-C/Ctrl-Break) and structured exception
handling are a distinct, real Win32 mechanism that could eventually be
bridged into the same `signal_actions[]`/`raise()` dispatch the same
general way, but that is separate future work, not part of this fix.

**Verification status:** confirmed on a real Windows host -- see "Windows
Verification" below. (Originally landed code-review-verified only, via
`-fsyntax-only -Wall -Wextra` against the real `x86_64-w64-mingw32`/
`aarch64-w64-mingw32` target triples and macro set `tools/crt-cc` actually
uses, since this project's CMake presets refuse to cross-compile
`CRT_TARGET_OS=windows` from any other host.)

## `pselect()` Atomicity

Fixing real delivery was not sufficient by itself. `pselect()`
(`libc/src/poll.c`) implemented the `sigmask` argument as three separate
steps: `sigprocmask(SIG_SETMASK, sigmask, &oldmask)`, then a plain
`select()`, then `sigprocmask(SIG_SETMASK, &oldmask, 0)` to restore. A signal
that was already pending -- exactly the case when a child has already exited
before the caller gets around to calling `pselect()` -- gets delivered
synchronously as part of the *first* `sigprocmask()` call (real kernel signal
delivery happens on the way back to userspace from any syscall), before
`select()` is ever entered. The non-atomic sequence has no way to notice
this: it silently swallows the interruption and blocks forever on an event
that already happened. This is the textbook `pselect()` lost-wakeup problem,
and it is precisely what GNU make's `jobserver_acquire()` comment documents
relying on `pselect()` never doing.

The fix adds `unsigned long __crt_signal_delivery_generation(void)`
(`libc/include/private/crt_signal.h`), a monotonically increasing counter
bumped in `signal.c`'s `deliver_signal()` every time a real handler actually
runs (not for `SIG_IGN` or default disposition, matching real `EINTR`
semantics: only a *caught* signal counts). `pselect()` reads it immediately
before and after its internal unblocking `sigprocmask()` call; if it changed,
a signal was delivered as part of that exact call, and `pselect()` restores
the mask and returns `-1`/`EINTR` immediately, exactly as a real atomic
`pselect()` would.

This does not add a raw atomic syscall (macOS/Linux still decompose the same
way), so a genuinely concurrent delivery landing in the few instructions
between the generation check and `select()` actually blocking remains a
theoretical residual race. It closes the case that matters in practice and
was actually observed: a signal (or several) already pending before the wait
begins.

## Verification

- Full local macOS test suite: 71/71 passing after the backend, shared
  Mach-O helper, and `pselect()` changes.
- Standalone repro compiled with the real `crt-cc`/sysroot toolchain (not the
  host compiler): a process installs a real `SIGCHLD` handler, forks a
  child, and blocks in `read()` on a pipe nothing ever writes to. Before this
  work there was no way for this to return; after, `read()` returns `-1`/
  `EINTR` and the handler fires.
- The same toolchain build of the original `pselect()`-based repro (parent
  blocks `SIGCHLD`, child becomes a zombie *before* the parent calls
  `pselect()`, parent expects the already-pending signal to wake it):
  hung indefinitely before the `pselect()` atomicity fix, returns `-1`/
  `EINTR` immediately after.
- A `make -j10` reproduction with 20 independent one-second targets: before
  the fix, execution permanently stalled the moment all 10 initial job slots
  were in use (job 11 never started, even though all 10 children had already
  exited); after, all 20 targets complete.
- The actual failure that started this investigation -- `port-rebuild-zlib`'s
  `./configure && make -j 10 && make install` through the project's own
  CRT-built `make` -- completes cleanly end to end.
- One detour worth recording: an early rerun of the `make -j10` repro still
  hung after the fix appeared complete. The cause was not a remaining bug --
  the installed `make` port binary was a stale artifact linked against the
  pre-fix libc (`nm` showed no `__crt_signal_backend_*`/`__crt_signal_
  dispatch` symbols in it at all). Rebuilding the `make` port against the
  current libc resolved it. Same class of trap as the earlier stale-`ctest`-
  binary issue: a build/install step that does not depend on the changed
  target will happily run old code.
- **Update: re-confirmed on macOS with the new permanent regression test.**
  After `tests/pselect_sigchld_test.c` (see "Regression Test" below) was
  added, the user ran the full suite on a real macOS machine directly:
  `ctest --preset macos-host-ninja-debug` -- 74/74 passing, with
  `pselect_sigchld_test_runs` itself completing in 0.21s (matching the fast
  `EINTR`-wakeup path also confirmed on Linux, not the bounded-timeout
  Windows path), confirming the `non-Windows` branch of the new test against
  macOS's real `sigaction`/`sigprocmask` backend, not just Linux's.

## Linux Verification

Verified end to end on a real Linux aarch64 host (previously code-review-only,
per the caveat above):

- Full `ctest` suite: 74/74 passing via `cmake --build --preset
  linux-host-ninja-debug` + `ctest --preset linux-host-ninja-debug`.
- The real `port-rebuild-zlib` `./configure && make -j 4 && make install`
  (this host has 4 cores, so `crt-port-build.py` picks `-j 4` rather than the
  `-j 10` used on the macOS repro; same jobserver/`pselect()` code path)
  completed cleanly, and the resulting `libz.so.1.3.1` resolves its
  `libc.so`/`libm.so`/`libdl.so`/`libc++.so` dependencies to this project's
  own sysroot via `ldd`, with `examplesh`'s real compress/uncompress round
  trip passing.
- A new permanent regression test, `tests/pselect_sigchld_test.c` (see
  "Regression Test" below), passes in ~0.2s.
- Regression sanity check on the fix itself: temporarily disabling the
  `pselect()` atomicity check (`libc/src/poll.c`) made the new test block for
  its full 5s bounded timeout and fail, confirming the test actually
  exercises the fix rather than passing vacuously; reverted before landing.

## Windows Verification

Verified end to end on a real Windows host (previously code-review-only,
per the caveat above):

- Full `ctest` suite: 80/80 passing via `cmake --build --preset
  windows-host-ninja-debug` + `ctest` (79 pre-existing tests plus
  `pselect_sigchld_test_runs`, added by the Linux verification pass above).
- `pselect_sigchld_test_runs` itself completed in **0.23s** -- the same
  fast-`EINTR`-wakeup path already confirmed on Linux (~0.2s) and macOS
  (0.21s), not the old bounded-5s-timeout behavior the honest no-op stub
  used to produce. Confirms the real, polled `SIGCHLD` mechanism described
  in "Windows" above actually fires, not merely that the build compiles.
- The first Windows parallel-port experiment exposed a separate
  `make.exe: /system/bin/mksh: Bad file descriptor` jobserver/fd-inheritance
  bug after the `pselect()`/`SIGCHLD` mechanism itself was already confirmed.
  That later investigation is now complete: the Windows process/fd path and
  GNU make token accounting were fixed, stress-tested with real parallel port
  builds, and the Windows-only serial restriction was removed. The full
  diagnosis belongs in `HISTORY.md`; it is no longer an open signal-delivery
  task.

## Regression Test

`tests/pselect_sigchld_test.c` (registered as `pselect_sigchld_test_runs` in
`tests/CMakeLists.txt`, `TIMEOUT 30` as an outer safety net) is the permanent
regression test for the `fork()` + blocked-`SIGCHLD` + `pselect()` pattern:
it installs a real `SIGCHLD` handler, blocks `SIGCHLD`, forks a child that
exits immediately, sleeps briefly so the child has actually exited at the OS
level, then calls `pselect()` (unblocking `SIGCHLD` for the duration)
against a pipe read end that is kept deliberately unreadable (the write end
stays open in the parent) with a 5s timeout. It asserts `pselect()` returns
`-1`/`EINTR` in well under 2s on every host, including Windows now that it
has the real (if polled) `SIGCHLD` mechanism described above -- the test no
longer special-cases Windows at all.

## Next Steps

- **Decided** (not yet implemented -- see `docs/job_control.md`'s
  "Interactive Job Control" section for the full design): bridge
  `SetConsoleCtrlHandler` (`CTRL_C_EVENT`/`CTRL_BREAK_EVENT`, both mapped to
  `SIGINT`) into `signal_actions[]`/`raise()`, following this file's own
  `SIGCHLD` pattern -- an atomic pending-flag set from the handler thread,
  actual dispatch on the main thread at the same `pselect()`/`select()`/
  `poll()` checkpoints `SIGCHLD` already uses, not synchronous dispatch from
  the handler thread itself. Vectored exception handling (`SIGSEGV`/
  `SIGFPE`/`SIGILL`) stays a separate, not-yet-decided question -- unlike
  console control events, it has no natural fit with this project's
  `MKSH_NOPROSPECTOFWORK`-disabled interactive-job-control motivation, so it
  wasn't decided alongside the console-event bridge above.
- Consider whether other blocking CRT calls beyond `pselect()`/`select()`/
  `poll()` need the same "was it already pending" generation check, or
  whether real `EINTR` propagation from the underlying syscalls already
  covers them.

## The thread registry and WTF's suspend/resume (Linux)

`libc/src/tls.c` finds the calling thread's `crt_thread_context` (errno, `pthread_self`, `pthread_getspecific`) and
`pthread_kill` validates a `pthread_t` against it. A signal handler can run at any instruction of any thread, and WTF
parks a thread in its SIGUSR1 handler (`sigsuspend`) until a second SIGUSR1, sent with `pthread_kill`, releases it. So
nothing on those paths may wait for a lock that an interrupted thread could hold. The registry is therefore a fixed
open-addressed table keyed by tid: readers take no lock, a slot's tid stays after its thread ends (the context pointer is
cleared, a tombstone), and the context pointer is published last. Only thread start, thread end and `fork` take the
writer lock. `signal_threads_test` reproduces the old deadlock deterministically. Known gap: `pthread_kill` to a thread
that has not started running yet (it registers itself) finds nothing; that was already true.
