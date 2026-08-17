# Toybox Applet Status

## Goal

Track, applet by applet, which toybox commands are wired into
`shell/CMakeLists.txt`'s `crt_toybox` target and why any applet that isn't is
still deferred. `TODO.md`'s own "planned" section just points here now --
this is the detail. See `HISTORY.md`'s dated entries for the full writeup of
each batch actually landed; this file only tracks what's still open.

## Registration mechanism, and its two traps

Applets are registered across four generated files: `shell/toybox/crt/
generated/newtoys.h` (`USE_X(NEWTOY(...))`, must stay in strict `LC_ALL=C`
sorted order -- `toy_find()` binary-searches it), `shell/toybox/src/android/
linux/generated/config.h` (`CFG_X`/`USE_X` gate, with a small CRT-local
override layer at `shell/toybox/crt/generated/config.h` that forces some
applets off regardless of upstream Android's own default), `flags.h`
(`FLAG_x` bit-position macros), and `globals.h` (`GLOBALS()` backing
storage). These are normally produced by toybox's own `mkflags`/`genconfig`
C-preprocessor pipeline (`scripts/make.sh`/`scripts/genconfig.sh`), which
this project does not run at build time -- `flags.h`/`globals.h` are
committed snapshots, hand-edited when enabling an applet the snapshot
predates.

Found the hard way while enabling `df`/`stty` (2026-08-16, see
`HISTORY.md`):

1. **`globals.h`, not `flags.h`, is the real source of truth for
   `GLOBALS()` union storage.** `flags.h`'s `#define TT this.X` pattern is
   generated unconditionally for every applet in `newtoys.h` regardless of
   whether real union backing exists -- checking `flags.h` alone looks like
   "this applet is ready" but isn't. The actual storage is a separate
   `struct X_data { ... }; ... struct X_data X;` pair inside `extern union
   global_union` in `globals.h`. Missing it fails the build with "no member
   named 'X' in union global_union". Fix: hand-add the struct, copied
   verbatim (field-for-field, same order) from the applet's own `GLOBALS()`
   macro content -- low-risk since it's a direct mirror of already-known
   fields, unlike `flags.h`'s bit-position math below.
2. **`flags.h`'s checked-in snapshot leaves any applet that was disabled
   when it was generated on the dead `FORCED_FLAG` multiplier instead of
   the real `1LL` one, even after `config.h` re-enables it.** `mkflags`
   emits `#define FLAG_x (FORCED_FLAG<<N)` for a disabled applet's flags,
   and `FORCED_FLAG` is `0LL` unless the specific `.c` file `#define
   FORCE_FLAGS` before including `toys.h` (only a handful of files do,
   e.g. `cat.c`/`cp.c`/`id.c`, for unrelated multiplexed-applet reasons).
   This compiles fine and silently no-ops every flag with no warning --
   `df -h` behaved exactly like plain `df`, no error, just wrong output.
   The bit *positions* mkflags assigns don't depend on enabled state
   (derived from the applet's `allflags` superset, not just what's
   currently compiled in) and are already correct even while disabled --
   only the multiplier is stale. Fix: hand-flip `FORCED_FLAG` to `1LL` for
   just that applet's `FOR_X` block -- a narrow, mechanical substitution,
   not a bit-position change, so far lower-risk than hand-editing new flag
   positions (which this project has otherwise judged too risky to do
   without a real `mkflags` run).

`flags.h` additionally contains raw high-byte sentinel bytes in some
optstring comments (from `mkflags`' own `mark_gaps()`, e.g. the `ls`
applet's) that are not valid UTF-8 by themselves -- a plain text-editing
round trip can silently mangle them into replacement characters elsewhere in
the file, nowhere near the intended edit (caught once via full-file diff
review before committing, see `HISTORY.md`'s 2026-08-16 entry). Always diff
the *whole* file after editing `flags.h`, not just the intended hunk; if
corrupted, restore via `git checkout` and reapply the change with a
byte-preserving method (e.g. reading/writing via an ISO-8859-1/Latin-1
codec, which round-trips every byte value 1:1) instead of a normal
UTF-8-assuming text edit.

## Still open

- **`expand`, `logger`, `fold`, `uudecode`, `cal`, `split`, `strings`**:
  audited and LLP64-safe, but need `globals.h` extended first (their
  `GLOBALS()` struct is missing from the committed `union global_union`
  entirely -- see trap 1 above). `flags.h` needs the same per-applet
  `FORCED_FLAG` check as trap 2 above before being considered done, not
  just `globals.h` -- check each of these seven individually rather than
  assuming they all need only the `globals.h` fix.
- **`timeout`**: its original hang is fixed (a real, general Windows
  `poll()` bug -- `poll_handle()` called `PeekNamedPipe()` unconditionally
  on any pipe handle to answer `POLLIN`, but that call does not reliably
  report "no data" on a pipe's *write* end the way it does for a real read
  end; confirmed with a minimal standalone repro. Fixed by tracking pipe
  write ends (`fd_pipe_write_only[]`) and never calling `PeekNamedPipe()`
  on one; new regression `tests/poll_pipe_write_end_test.c`. This is a
  real, general `poll()` fix independent of `timeout`'s own status), but
  the applet itself still needs two more, separate PAL features before
  it's actually correct:
  - `deliver_signal()`'s `SA_SIGINFO` path (`libc/src/signal.c`) always
    hands the handler a zeroed `siginfo_t` (`si_code = 0`, `si_status =
    0`) regardless of which signal or why -- for `SIGCHLD` specifically
    this means a handler can never learn which child exited or how.
    `timeout.c`'s own handler reads exactly those fields, so it always
    computes a wrong exit status (always `128`, since `si_code` can never
    equal the real `CLD_EXITED`), even for a child that exited
    successfully. This project's own child-tracking tables
    (`child_process_table`/`child_pid_table`, already used by
    `waitpid()`) have the real data; `SIGCHLD` dispatch just doesn't
    thread it through to `siginfo_t` yet.
  - `kill()` still only supports signaling the calling process itself (a
    pre-existing, previously-documented gap) -- sending a signal to a
    genuinely different process is a no-op, so `timeout`'s own deadline
    enforcement (`kill(pid, SIGTERM)` on the child once the clock runs
    out) silently does nothing. Confirmed directly: `timeout 2 sleep 10`
    ran the full ~10 seconds instead of being cut off at ~2.

  `timeout` stays disabled until at least the `kill()` gap closes --
  re-registering it now would ship a command that reports wrong exit codes
  and, worse, silently fails to enforce the one thing it exists to do.
- Beyond those, the Android/Bionic-parity diff is exhausted for
  non-`/proc`-dependent applets. `shell/toybox/crt/generated/config.h`
  already forces `flock`/`gzip`/`zcat`/`mount`/`nproc`/`pgrep`/`pkill`/
  `ps`/`umount`/`unshare` to `0` regardless of upstream, matching the
  deferred-applet list below. `install`/`realpath`/`whoami` (alias
  `logname`) have no source file in this tree at all and would need a real
  upstream import first.

## Deferred applets, and why

Investigated concretely (2026-08-16, upstream source read for each, not
guessed). `df` and `stty` were in this same list and turned out to be
tractable -- see `HISTORY.md`'s 2026-08-16 entry. Everything below stays
deferred for a specific, confirmed reason, not just "not done yet":

- **`ps`/`top`/`iotop`/`pgrep`/`pkill`**: all five are registered from one
  shared file (`shell/toybox/src/toys/posix/ps.c`, ~2000 lines) whose
  `get_ps()`/`get_threads()` does a real recursive `/proc` walk over
  *every process on the system* (`/proc/$PID/stat`, `/status`, `/io`,
  `/statm`, `/exe` readlink, `/cmdline`, `/fd/*`, `/proc/$PID/task/*`,
  `/proc/tty/drivers`, cgroup) -- architecturally a much larger surface
  than the `/proc/self/*` virtual files this project already has. Add
  through toybox only once the rootfs/PAL provides real multi-process
  `/proc/$PID` data, not mksh builtins. Already forced off via
  `shell/toybox/crt/generated/config.h` regardless of upstream Android's
  own config.
- **`mount`/`umount`**: call the real Linux `mount(2)`/`umount(2)` kernel
  syscalls directly -- genuinely inapplicable outside real Linux with root
  and a real VFS/block-device concept, which this PAL's architecture
  doesn't have at all.
- **`ifconfig`**: needs deep Linux-specific socket ioctls (`SIOCGIFCONF`,
  `SIOCGIFFLAGS`, `SIOCGIFADDR`, `SIOCGIFHWADDR`, `SIOCSIFADDR`, ...) with
  no Windows equivalent -- Windows needs the entirely different IP Helper
  API (`GetAdaptersAddresses` etc.), a separate networking-PAL feature
  beyond the existing curl-oriented socket layer.
- **`login`**: needs `crypt()` (unimplemented), `getspnam()`/a shadow
  password DB (unimplemented), and real multi-user `setuid` session
  switching -- architecturally mismatched with this project's
  single-host-process-per-invocation model.
- **device-manager or other procfs-heavy commands**: same `/proc`-breadth
  gap as the `ps` family above.
