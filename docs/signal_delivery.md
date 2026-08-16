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

### Windows

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
- Went further than the synthetic test, matching the Linux/macOS
  real-world verification style: temporarily lifted `tools/crt-port-
  build.py`'s hardcoded `jobs = 1 if target_os == "windows" ...`
  restriction (a local, reverted-immediately test patch) and reran a real
  port build (`zlib`, `./configure && make -j 8 && make install`) with
  genuine parallel jobs on Windows -- something no Windows port build had
  ever actually done before. **This found a second, separate, still-open
  bug**: `make.exe: /system/bin/mksh: Bad file descriptor` followed by
  `make.exe: INTERNAL: Exiting with 1 jobserver tokens available; should
  be 8!` -- GNU Make's own process-spawn failure when creating a
  *concurrent* recipe shell, not a `pselect()`/`SIGCHLD` symptom (that
  mechanism is confirmed correct by the regression test above; this is a
  distinct failure mode, not a hang). Points at a race or gap in this
  Windows PAL's own concurrent process-spawn/fd-inheritance path, not at
  anything in this document's own signal-delivery design. Not root-caused
  this session -- reverted the test patch, rebuilt `zlib` normally (`-j 1`)
  to restore a known-good state, reran full `ctest` (80/80, clean, no
  residual corruption). `tools/crt-port-build.py`'s `jobs = 1 if
  target_os == "windows"` restriction stays in place until this separate
  bug is fixed; see `TODO.md`, "in progress", for the tracking entry.

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
