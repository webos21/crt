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

**Verification caveat:** this project's CMake presets refuse to cross-compile
(`cmake --preset linux-host-ninja-debug` fails immediately unless run on an
actual Linux host), so the Linux backend is code-review-verified against
Android Bionic and the Linux kernel UAPI headers only, not built or executed
in this session. Same caveat as the Linux/Windows portions of
`docs/dynamic_loading.md`.

### Windows

Left as an honest no-op stub. There is no real Windows kernel mechanism that
generates a `SIGCHLD`-equivalent async signal; child completion is already
observed directly through `waitpid()` and the child registry (see
`docs/windows_fork_emulation.md`), not through a signal. Console control
events (Ctrl-C/Ctrl-Break) and structured exception handling are a distinct,
real Win32 mechanism that could eventually be bridged into the same
`signal_actions[]`/`raise()` dispatch, but that is separate future work, not
part of this fix.

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

## Next Steps

- Verify the Linux backend on an actual Linux host once available (build and
  test presets currently refuse to run from macOS).
- Add a permanent regression test for the `fork()` + blocked-`SIGCHLD` +
  `pselect()` pattern (the isolated repro above), so this does not silently
  regress.
- Decide whether Windows should eventually bridge `SetConsoleCtrlHandler`/
  vectored exception handling into `signal_actions[]`/`raise()`, or whether
  that stays out of scope indefinitely.
- Consider whether other blocking CRT calls beyond `pselect()`/`select()`/
  `poll()` need the same "was it already pending" generation check, or
  whether real `EINTR` propagation from the underlying syscalls already
  covers them.
