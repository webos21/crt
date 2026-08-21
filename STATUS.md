# Project Status

A single, lightweight snapshot of "does it currently pass, and what's known
to still be broken." This is deliberately not a release process (no tags,
versions, or CHANGELOG yet -- `LICENSE.md` itself only landed recently, and
there are no external consumers) -- just a page that stays honest about
current state so the next session doesn't have to re-derive it from
`HISTORY.md`. Update this alongside any change that shifts what "passing"
means; if this page and `HISTORY.md`/`TODO.md` disagree, the dated entries
in those two win.

## What "passing" currently means

- **CI**: `.github/workflows/ci.yml`, a 5-leg GitHub Actions matrix (macOS
  aarch64, Linux arm64/amd64, Windows arm64/x64), each running this
  project's own `cmake --workflow <os>-host-ninja-debug` preset (configure +
  build + `ctest`) on every push. **Was red on 3 of 5 legs (`linux-amd64`,
  `linux-arm64`, `windows-x64`) for the 7 commits from `940af4c` through
  `d7cb458`** (2026-08-18's Skia/libc++ bring-up push sequence) -- caught
  and fixed 2026-08-21, not by the pushing session itself but by directly
  querying the GitHub Actions REST API for real job/step conclusions (the
  `gh` CLI was not available in this session; unauthenticated log downloads
  return 403 "Must have admin rights", but job-level pass/fail is public).
  Two independent, unrelated root causes:
  1. **Linux (both arm64 and amd64), every failing run, at the
     "Configure, build, and test" step in under ~20 seconds** (too fast to
     be a real build failure) -- `CMakeLists.txt` forces
     `-stdlib=libc++` for `CRT_TARGET_OS=linux` (added in `597280c`, whose
     own HISTORY.md entry shows `libc++-18-dev` was installed by hand on
     the real Linux aarch64 host that validated it), but
     `.github/workflows/ci.yml`'s Linux install step has *never*, in the
     entire git history of that file, installed `libc++-dev`/
     `libc++abi-dev` -- only bare `clang lld ninja-build cmake`. Without
     it, CMake's own one-time CXX-compiler-ABI-detection step fails
     outright linking a trivial program (`ld: cannot find -lc++`), not
     the graceful "falls back to GNU libstdc++" behavior an earlier
     `CMakeLists.txt` comment assumed -- reproduced directly (both
     Clang 10 and Clang 18, via WSL Ubuntu) and confirmed fixed by
     extracting `libc++-dev`/`libc++abi-dev` and their real runtime
     dependents (`libc++1`/`libc++abi1`/`libunwind`, pulled via
     `apt-get download`, no root needed) into a local prefix and
     re-linking successfully. Fixed by adding `libc++-dev libc++abi-dev`
     to the Linux install step.
  2. **Windows x64 only** (`windows-arm64` stayed green throughout,
     itself a real clue): `libm/src/basic.c`'s `fma()`/`fmaf()` called
     `__builtin_fma`/`__builtin_fmaf`, which Clang can only lower to a
     native FMADD/VFMADD instruction when the target defines `__FMA__`
     (via `-mfma` or an `-march=` that implies it) -- true unconditionally
     on aarch64 (explaining why `windows-arm64` never hit this), but
     x86_64 FMA3 is an *optional* CPU feature this project's build never
     opts into on any host. Without `__FMA__`, the builtins fall back to
     libcalls literally named `fma`/`fmaf` -- exactly the functions being
     defined, so genuine, stack-growing infinite self-recursion
     (confirmed via `llvm-objdump` disassembly of the compiled `.obj`:
     `callq` targeting the function's own symbol), crashing `math_test`
     with `STATUS_STACK_OVERFLOW`. Fixed by gating the `__builtin_fma`/
     `__builtin_fmaf` path behind `__FMA__`/`__aarch64__`/`_M_ARM64`,
     falling back to `x*y+z` otherwise (matching this file's existing,
     already-accepted precision bar for `fmal()` just below it, which hit
     the same class of bug -- tail-branch self-recursion instead of a
     real call -- on real Linux aarch64 hardware back in `597280c`).
  Both fixes verified end to end: Windows x64 119/119 `ctest`
  (`cmake --fresh` reconfigure first), and a genuine second-OS check via
  WSL Ubuntu (not the shared `/mnt/c` working tree, whose Windows
  `core.autocrlf=true` checkout corrupts `tools/crt-c++`'s `#!/bin/sh`
  shebang for a Linux exec -- a real clone onto WSL's own ext4 filesystem
  instead) -- 104/104 `ctest` there too, both with the same uncommitted
  fixes applied. `windows-arm64`/`macos-aarch64` were not independently
  re-verified this pass (no arm64 Windows or macOS hardware in this
  session); reasoned to be unaffected (`windows-arm64` already proved it
  by staying green; macOS's own libc++ was separately established working
  in `HISTORY.md`'s 2026-08-18 entries and this pass touched no
  macOS-specific code path). **Confirmed on the real GitHub Actions run
  for the fix commit itself**: all 5 legs green, including
  `windows-arm64`/`macos-aarch64` (not independently re-verified locally
  this pass, per above) -- [run 32434729833](https://github.com/webos21/crt/actions/runs/32434729833)
  (2026-08-21, commit `9c0b30b`), ending the 7-commit regression.
  Before the 940af4c regression, all 5 legs had been green as of
  [run 31986752976](https://github.com/webos21/crt/actions/runs/31986752976)
  (2026-08-17, the Windows `tcdrain`/`tcflow`/`tcflush`/`tcsendbreak` push).
  Two failures happened along the way getting there, both since fixed and
  reconfirmed green: [run 31978303539](https://github.com/webos21/crt/actions/runs/31978303539)
  (`sendmsg`/`recvmsg`/`memfd_create`) failed `macos-aarch64` only -- both
  Linux legs passed, confirming those raw syscall trampolines -- root-
  caused (from the job-level annotation plus an ABI review, since GitHub's
  log viewer needs sign-in) to a real macOS-only `struct cmsghdr` layout
  bug; the immediate fix for that specific bug ([run 31980507866](https://github.com/webos21/crt/actions/runs/31980507866))
  still failed `macos-aarch64`, because real macOS hardware testing then
  found three *more* ABI-translation bugs in the same code (`struct
  msghdr` field widths, `CMSG_ALIGN`'s unit, `cmsg_level`/`SOL_SOCKET`
  translation) that a Windows-only session's ABI review alone hadn't
  caught -- all fixed together in the next push, green since. See
  `HISTORY.md`'s 2026-08-16/17 entries for the full trail. Before
  the matrix existed, Linux validation had been almost entirely
  manual, on real aarch64 hardware -- x86_64 Linux had never actually been
  built until this matrix existed, and immediately surfaced two real,
  previously-invisible bugs (see `HISTORY.md`'s 2026-08-11 entries). CI's
  own `cmake --workflow` step does not run `port-test-recipes` (a
  separate, heavier target that fetches and builds third-party sources)
  -- that's verified locally/per-host instead, see below.
- **`ctest`**: 119 registered tests on Windows and 104 on Linux amd64 (via
  WSL) in the latest local runs (count is slightly
  OS-dependent -- a few targets, like `windows_export_hygiene_test`, only
  exist on their own OS), all passing on both (119/119 on Windows, 104/104
  via a genuine second-OS WSL Ubuntu run -- separately confirmed on each
  host, not simultaneously, so the exact registered-test overlap between
  the two counts hasn't been cross-checked item-by-item). Most recently:
  the `fma`/`fmaf` self-recursion fix and the Linux CI `libc++-dev` fix
  above -- see that CI bullet for the full writeup. Before that, confirmed
  on Windows after implementing the last three items of the Bionic libc
  gap audit's
  "lower priority" tier -- `ifaddrs.h` (real per-host: Linux `/sys/class/
  net` + ioctls, macOS the real Darwin `getifaddrs()` resolved at
  runtime plus sockaddr translation, Windows `GetAdaptersInfo()`) and
  `ucontext.h` (real `getcontext`/`setcontext`/`makecontext`/
  `swapcontext` on every host/arch via new assembly mirroring this
  project's own proven `setjmp`/`longjmp` register sets; a real coroutine
  round-trip test caught and fixed two genuine bugs on Windows x86_64 --
  an LLP64 struct-layout mismatch, and a `swapcontext()` resume-point bug
  using an adjusted stack pointer with a `retq`-based resume path that
  needs the unadjusted one) -- see `HISTORY.md`'s 2026-08-17 entries for
  the full per-item writeups; verified via a genuine `cmake --fresh`
  reconfigure. Just before that: `uchar.h`/`threads.h`/`sys/prctl.h`/
  `glob.h`, which surfaced and fixed two real, previously-undetected
  bugs with no prior regression coverage (`fnmatch()`'s inverted
  end-of-pattern match logic, breaking every consumer including toybox's
  `find`/`grep`/`tar`; `remove()` never handling directories, contrary to
  the C standard). This closes out **every** item from the 2026-08-16
  Bionic libc gap audit -- high, medium, and lower priority alike. Just
  before that: `PTHREAD_PROCESS_SHARED` --
  real and cross-process on Linux (non-private futex ops) and macOS
  (`os_sync_wait_on_address`'s `SHARED` flag, verified for real on macOS
  hardware the same day), unconditional on every host including
  Windows for `pthread_spinlock` (pure atomics, no OS wait/wake primitive
  involved), and an honest `ENOTSUP` on Windows for the other four
  primitives (`WaitOnAddress`/`WakeByAddress*` have no cross-process
  capability to opt into at all) -- see `HISTORY.md`'s 2026-08-17 entry
  for the full per-host writeup. Just before
  that: `dl_iterate_phdr`/`link.h`/`elf.h`/`dladdr` (real per-host
  implementations wherever each host actually has something real to
  report -- see `HISTORY.md`'s 2026-08-17 entry for the full per-host
  writeup; verified directly on Windows, Linux/macOS reasoned carefully
  but not yet run on real hardware). Just before that: `sys/epoll.h`/
  `sys/eventfd.h`/`sys/timerfd.h` (Linux-only, matching real Bionic --
  real raw syscall trampolines on Linux, `ENOSYS` on macOS/Windows; not
  yet independently verified on real Linux hardware, and `struct
  epoll_event`'s real x86_64-vs-aarch64 kernel-ABI layout difference
  needed care). Before that: Windows real
  `tcdrain`/`tcflow`/`tcflush`/`tcsendbreak` backing (`FlushFileBuffers`/
  `FlushConsoleInputBuffer`, honest no-ops for the two a console genuinely
  can't back) -- prompted by real Linux/macOS termios ports landing the
  same day. Before that:
  real Linux/macOS `tcgetattr`/`tcsetattr`/`tcdrain`/`tcflow`/`tcflush`/
  `tcsendbreak` ports (verified on real hardware, found and fixed four
  real ABI bugs in the `sendmsg`/`recvmsg`/`SCM_RIGHTS` work below along
  the way), `sendmsg`/`recvmsg` + `SCM_RIGHTS` fd passing and
  `memfd_create` -- the last two findings from a Bionic libc gap audit
  done before starting `libcrtgfx` -- and `semaphore.h`/public
  `<stdatomic.h>`/`df`/`stty` are all also done, the last of those fixing
  two real PAL bugs that surfaced along the way (a stale `flags.h` snapshot leaving
  their flags dead code, and no `tcgetattr`/`tcsetattr` round-trip
  fidelity beyond three bits) -- see `HISTORY.md`. The user
  also confirmed a real Linux and macOS build+run this same date, through
  the full `curl` port test -- this closed out the one cross-platform
  verification gap this session's `linkat()`/`link()` PAL work had left
  open (the Windows `CreateHardLinkA` path was already verified
  in-session; the Linux/macOS raw syscall trampolines could not be, no
  cross-toolchain in that dev session). CI is the source of truth for
  Linux counts. Run locally via
  `cmake --workflow --preset <os>-host-ninja-debug` or
  `ctest --test-dir out/<preset>`.
- **Ports**: see `docs/porting_status.md` for the full per-library,
  per-host table. `zlib`/`libpng`/`sqlite-amalgamation`/`bzip2` are at
  `shared-pass` on all three OSes. `xz` (liblzma) is now
  `shared-pass` on Linux, macOS, and Windows: a real, full
  compress/decompress round trip at preset 9|EXTREME with CRC64 passes
  against both static and shared builds on every verified OS. `libffi`'s
  own `port-test-libffi` now passes both its static and shared variants
  on Windows too (`ffi_call()` round trip, `result=42`) -- the shared
  variant's `_pei386_runtime_relocator` gap (Windows/PE "runtime pseudo
  relocation" support, a new PAL feature) is fixed; see `HISTORY.md`'s
  2026-08-12 entry for the full writeup and `tests/windows_pseudo_reloc_
  dll.c`/`consumer.c` for its own permanent `ctest` regression coverage.
  `pcre2` is now `shared-pass` on
  all three OSes: a real `pcre2_compile()`/`pcre2_match()` round trip
  with three named capture groups passes on every host for both static
  and shared builds, macOS confirmed by the user. All six of those ports
  now have an official,
  recipe-declared `port-test-<name>` CMake target (aggregated as
  `port-test-recipes`); re-run directly on Windows this session and
  confirmed green across the board. `libffi` overall stays `partial` only
  because of its unrelated, pre-existing `-O1`/`-O2`
  `ffi_call()`-repeat-call bug. `mbedtls` (the next port in the queue
  after `pcre2`, crypto library only) is now `shared-pass` on all three
  OSes: a real SHA-256 known-answer check plus an AES-128-CBC
  encrypt/decrypt round trip passes on every host against both the
  static and shared build, macOS confirmed by the user. See `HISTORY.md`'s
  2026-08-14 entry and `porting/recipes/mbedtls.json`'s own notes for
  the full trail (three new, generalizable `tools/crt-port-build.py`
  extensions -- `build.skip_configure`, a base `build.install_args`
  field, and a per-OS `build_make_args` field for a `make` variable
  that must reach the build step only, never `make install` -- plus
  several recipe patches, including disabling `MBEDTLS_NET_C` since
  this PAL's sockets surface doesn't yet cover everything mbedtls's own
  networking helper needs; deferred to `curl`, the next and last port
  in this queue). `curl` (8.21.0) is now **`shared-pass` on all three
  OSes**, closing out this whole porting queue (`bzip2` -> `xz` ->
  `pcre2` -> `mbedtls` -> `curl`; `openssl` stays deliberately held
  back). A real HTTP GET and HTTPS GET (real TLS handshake via the
  mbedTLS backend) round trip against `example.com` passes on Linux,
  macOS, and Windows, for both static and shared libcurl, verified
  directly on real hardware for all three hosts. Getting there
  surfaced a long chain of real, general, previously-invisible PAL
  bugs across the whole session -- among the most notable: `getaddrinfo()`
  had no real DNS resolution at all (a minimal synchronous DNS client
  added to `libc/src/socket.c`); `fcntl(fd, F_SETFL, O_NONBLOCK)` was a
  pure no-op on every OS (curl's own internal wakeup-pipe mechanism
  needs it for real -- now forwards to the real syscall on Linux/macOS
  and implemented for real on Windows via `SetNamedPipeHandleState`/
  `ioctlsocket(FIONBIO)`); mbedtls's own macOS `.dylib` files had no
  `-install_name` set, breaking dyld resolution; and, on Windows,
  mbedTLS's portable entropy source had no working `/dev/urandom` to
  read from at all, crashing the TLS handshake with a null
  function-pointer call inside its RNG -- root-caused with a real
  `lldb` backtrace and fixed by implementing a real `/dev/urandom`
  device backed by `RtlGenRandom()`. One real, general risk was found
  and, at the time, left open, not curl-specific: mbedtls's own Windows
  `.dll` build re-exported this project's entire libc with no
  symbol-visibility control, which could silently shadow real libc
  fixes for any consumer that also links mbedtls's DLL until mbedtls
  itself is rebuilt too. **This is now fixed** (see "Known gaps" below
  and `porting/recipes/mbedtls.json`'s own notes) -- confirmed with a
  from-scratch `port-rebuild-curl`/`port-test-curl` against the fixed
  mbedtls, which also surfaced and fixed one more independent Windows
  delete-pending-race bug in `__crt_sys_open()`. See `HISTORY.md`'s
  dated entries and `porting/recipes/curl.json`'s own notes (a long,
  blow-by-blow trail) for the full writeup.

## Known gaps

- **Windows static-archive constructor limitation**: executable
  `.init_array`/`.fini_array` and PE/Mach-O equivalents are fixed and
  covered by `tests/init_array_test.c`, but `lld-link` still does not
  reliably bracket constructor records contributed by a third-party
  static archive the way GNU ld's default ELF script does. xz routes
  around this with a documented recipe patch; a future Windows port that
  relies on archive-contained constructors may need a similar policy.
- **libffi**: `ffi_call()` alone and closures alone each work correctly in
  isolation, but calling `ffi_call()` and then any further libffi call in
  the same process reliably segfaults when the caller is compiled at
  `-O1`/`-O2` (never `-O0`) -- **on aarch64 Windows only**, root-caused to
  a callee-saved GPR getting corrupted somewhere in the
  `ffi_call()`/`ffi_call_SYSV` chain, not yet isolated to an exact
  instruction. x86_64 Windows is now confirmed clean (tested for the
  first time, see `HISTORY.md`'s 2026-08-15 entry), narrowing this to an
  aarch64-specific issue. A permanent regression now exists
  (`porting/tests/libffi_repeat_call_test.c`); next step is a real `lldb`
  session on aarch64 Windows hardware. See `porting/recipes/libffi.json`'s
  notes.
- **DNS resolver is deliberately minimal**: `getaddrinfo()` now does a
  real DNS lookup (added for curl, see `HISTORY.md`'s 2026-08-14
  entry), but only a single synchronous UDP query for an A (IPv4)
  record -- no AAAA/IPv6, no TCP fallback for truncated responses, no
  search-domain suffixes, no caching. Sufficient for curl's own basic
  HTTP/HTTPS needs; would need to grow if a future port needs more.
- **`timeout` (toybox applet) stays disabled -- its hang is fixed, but two
  deeper gaps remain**: the original hang was a real, general Windows
  `poll()` bug (`PeekNamedPipe()` misreporting a pipe *write* end as
  readable), now fixed with a permanent regression
  (`tests/poll_pipe_write_end_test.c`, see `HISTORY.md`'s 2026-08-16
  entry). Verifying the real applet after that fix surfaced two more,
  separate issues: `SIGCHLD`'s `SA_SIGINFO` delivery
  (`deliver_signal()` in `libc/src/signal.c`) always hands the handler a
  zeroed `siginfo_t`, so `timeout` always reports the wrong exit code;
  and `kill()` still only supports signaling the calling process itself,
  so `timeout`'s own deadline enforcement (`kill(pid, SIGTERM)` on the
  child) is a silent no-op -- confirmed directly, `timeout 2 sleep 10`
  ran the full ~10 seconds instead of being cut off at ~2.
- **`TIOCGWINSZ` legitimately fails in a console with no real output
  screen buffer**: confirmed directly (`GetConsoleScreenBufferInfo` fails
  on `CON`/`CONOUT$` in this project's own dev environment for this
  session, even though `GetConsoleMode` on the same/an input handle
  succeeds -- a real, partial-console condition, not a code bug). Correct
  per POSIX and upstream toybox's own `stty.c` semantics (`perror_exit`
  on failure), but means `stty -a`/`stty size` can't be exercised
  end-to-end in every environment; `stty -g`/individual option toggles
  (which don't need window size) are unaffected and verified working.
- **`sendmsg`/`recvmsg` Linux/macOS raw syscall trampolines: both hosts now
  confirmed by real testing.** Linux `sendmsg`=46/`recvmsg`=47 (x86_64) and
  `sendmsg`=211/`recvmsg`=212 (aarch64) were confirmed correct by real CI
  (`linux-amd64`/`linux-arm64` both passed `tests/
  sendmsg_scm_rights_test.c`'s real `AF_UNIX` `SCM_RIGHTS` fd-passing round
  trip cleanly). macOS `sendmsg`=28/`recvmsg`=27 were also correct; real
  macOS hardware testing found and fixed four separate real ABI-
  translation bugs instead (`struct cmsghdr`'s `cmsg_len` width, `struct
  msghdr`'s field widths, `CMSG_ALIGN`'s alignment unit, `cmsg_level`/
  `SOL_SOCKET` translation -- see `HISTORY.md`'s 2026-08-16/17 entries).
  Windows's data-only path (no raw syscalls involved, just Winsock) was
  already verified directly, no CI dependency.
- **`eventfd`/`timerfd`/`epoll` Linux raw syscall trampolines are
  unverified on real hardware**: written 2026-08-17 following the same
  reasoning-from-already-tested-neighbors discipline `sendmsg`/`recvmsg`
  used, including a real x86_64-vs-aarch64 `struct epoll_event` kernel-ABI
  layout difference (packed 12 bytes on x86_64, natural 16 bytes on
  aarch64) that needed care -- see `HISTORY.md`. `tests/eventfd_test.c`/
  `tests/timerfd_test.c`/`tests/epoll_test.c`'s real behavior checks (under
  `CRT_TARGET_OS_LINUX`) are what verify these the next time they run on
  real Linux CI or hardware; the `ENOSYS` path on macOS/Windows and the
  `struct epoll_event` size check (architecture-only, not OS-only) are
  already verified directly from this session.
- **macOS `PTHREAD_PROCESS_SHARED`'s `os_sync_wait_on_address` `SHARED`
  flag is unverified on real hardware**: written 2026-08-17, reasoned from
  the documented libSystem header shape
  (`<os/os_sync_wait_on_address.h>`, macOS 14.4+/iOS 17.4+) --
  `OS_SYNC_WAIT_ON_ADDRESS_SHARED`/`OS_SYNC_WAKE_BY_ADDRESS_SHARED` = `0x1`
  -- same discipline as the other Linux/macOS raw-ABI entries above.
  `tests/pthread_process_shared_test.c`'s real cross-thread contention
  checks (under `CRT_PSHARED_SUPPORTED`, which is true on macOS) are what
  verify this the next time it runs on real macOS hardware; the Windows
  `ENOTSUP` path and the Linux non-private futex path are already verified
  from this session (Linux by the same reasoning that already-tested
  `sendmsg`/`recvmsg`/`eventfd` neighbors on the same syscall ABI rely on;
  the private-futex half of the same file was already confirmed correct
  by real Linux CI before this change).
- **`sys/prctl.h`'s Linux raw `prctl` syscall trampoline is unverified on
  real hardware**: written 2026-08-17, reasoned from well-known stable
  UAPI syscall numbers (x86_64=157, aarch64=167) -- same discipline as
  the other Linux raw-syscall entries above. `tests/prctl_test.c`'s real
  `PR_SET_NAME`/`PR_GET_NAME`/`PR_GET_DUMPABLE` checks (under
  `CRT_TARGET_OS_LINUX`) are what verify this the next time it runs on
  real Linux CI/hardware; the `ENOSYS` path on macOS/Windows and the
  `PR_*` constant values (fixed UAPI, not host-dependent) are already
  verified directly from this session.
- **`ifaddrs.h`'s Linux `/sys/class/net` + `SIOCGIFADDR`/`SIOCGIFNETMASK`/
  `SIOCGIFBRDADDR`/`SIOCGIFFLAGS` ioctl path is unverified on real
  hardware**: written 2026-08-17, reasoned from well-known stable UAPI
  ioctl numbers -- same discipline as above. `tests/ifaddrs_test.c`'s
  real interface-enumeration checks (host-agnostic, so they already ran
  successfully against the real Windows `GetAdaptersInfo()` backend this
  session) are what verify the Linux path the next time it runs on real
  Linux CI/hardware. The macOS backend (real Darwin `getifaddrs()`
  resolved at runtime) needed one real fix already found by real macOS
  testing the same day (a private-struct field name colliding with this
  project's own public `ifa_dstaddr` macro) -- see `HISTORY.md`.
- **`ucontext.h`'s Linux, macOS, and aarch64 (all three architectures'
  non-Windows-x86_64 combinations) assembly is unverified on real
  hardware**: written 2026-08-17, mirroring this project's own already-
  verified per-host/per-arch `setjmp`/`longjmp` register sets. Windows
  x86_64 was verified directly via a real coroutine round-trip test that
  caught and fixed two genuine bugs during development (see `HISTORY.md`'s
  full writeup) -- the same test (`tests/ucontext_test.c`) is what
  verifies the other five host/arch combinations the next time they run
  on real hardware/CI. Since the swapcontext() resume-point bug found on
  Windows x86_64 was pure x86_64 `call`/`ret` ABI mechanics (not
  Windows-specific), it was reasoned to apply identically to Linux/macOS
  x86_64 and fixed there too pre-emptively, but neither has been
  independently re-confirmed by actually running the test.

## Next

- Porting matrix expansion through curl is **done**: `bzip2`, `xz`, `pcre2`,
  `mbedtls`, and `curl` are all `shared-pass` on Linux, macOS, and Windows
  (`openssl` stays deliberately held back until something needs it). The
  real, general risk once open from this queue -- mbedtls's Windows DLL
  symbol-export hygiene -- is fixed; see `HISTORY.md`'s 2026-08-15 entry.
- Before starting the next upper-runtime phase, reduce the remaining
  libc/PAL planned work in `TODO.md`: libffi correctness, DNS resolver
  growth, console/job-control policy, and toybox applet expansion only
  where the Bionic-compatible backing surface exists. The mksh subshell
  status quirk, the six queued virtual rootfs files (`/proc/mounts`,
  `/proc/stat`, `/proc/self/status`, `/proc/self/cmdline`,
  `/proc/self/environ`, `/dev/zero`), a Bionic/Android-parity toybox
  applet diff (`cut` plus 24 more names), a real Windows
  POSIX-semantics `rename()` (re-enabling `dos2unix`/`unix2dos`), and
  `df`/`stty` (plus the two real PAL bugs their enablement uncovered) are
  fixed -- see `HISTORY.md`'s 2026-08-16 entries. Remaining toybox gap is
  now down to: `expand`/`logger`/`fold`/`uudecode`/`cal`/`split`/
  `strings` (need `globals.h` extended, and possibly a `flags.h`
  `FORCED_FLAG` fix per-applet -- see `TODO.md`), the two deeper gaps
  `timeout` still needs (real cross-process `kill()`, real `SIGCHLD`
  `siginfo_t` data) above, and the already-deliberately-deferred
  `/proc`-heavy applet set (`ps`/`top`/`iotop`/`pgrep`/`pkill`,
  `mount`/`umount`, `ifconfig`, `login`, each now with a concrete,
  confirmed reason recorded in `TODO.md` rather than "not done yet").
- A real, evidence-based Bionic libc gap audit was done before starting
  `libcrtgfx` (see `docs/bionic_libc_gaps.md`, `TODO.md`'s "Bionic libc
  completeness before `libcrtgfx`" section). **Every item found by that
  audit is now done** -- all four "high priority" findings (`semaphore.h`,
  public `<stdatomic.h>`, `sendmsg`/`recvmsg` + `SCM_RIGHTS`/`CMSG_*` fd
  passing, `memfd_create`), all three "medium priority" items
  (`epoll`/`eventfd`/`timerfd`, `dl_iterate_phdr`/`link.h`/`elf.h`/
  `dladdr`, `PTHREAD_PROCESS_SHARED`), and all six "lower priority, no
  identified near-term consumer" items (`uchar.h`, `threads.h`,
  `sys/prctl.h`, `glob.h`, `ifaddrs.h`, `ucontext.h`) -- see `HISTORY.md`.
  Implementing the lower-priority tier surfaced and fixed four real,
  previously-undetected/unresolved bugs along the way: `fnmatch()`'s
  inverted end-of-pattern match logic (broke every consumer with no
  regression test to have caught it, including toybox's
  `find`/`grep`/`tar`), `remove()` never handling directories (contrary
  to the C standard), and two Windows x86_64-specific `ucontext.h` bugs
  (an LLP64 struct-layout mismatch, and a `swapcontext()` resume-point
  bug needing an unadjusted stack pointer for its `retq`-based resume
  path) caught by a real coroutine round-trip test. Real follow-ups
  remain: the new Linux/macOS `sendmsg`/`recvmsg`, Linux
  `eventfd`/`timerfd`/`epoll`/`dl_iterate_phdr`/`dladdr`/`ifaddrs`/
  `prctl` raw syscall/ioctl trampolines, the macOS `PTHREAD_PROCESS_
  SHARED` `os_sync_wait_on_address` `SHARED` flag, and the Linux/macOS/
  aarch64 `ucontext.h` assembly all need real hardware verification (see
  "Known gaps" above). Per the user's own framing, this now positions the
  project to move into the `libcrtgfx` upper-runtime phase
  (`docs/runtime_roadmap.md`).
- The next product-level target is documented in `docs/runtime_roadmap.md`:
  an Electron-class rebuilt runtime made of `libcrtgfx` (Skia + Wayland-style
  compositor boundary + Chromium Ozone path), `libcrtmedia` (FFmpeg/codecs/
  audio/video), and `libcrtjs` (QuickJS first, V8 later).
- Skia `m148` now compiles into a CRT-toolchain CPU archive on macOS. The
  response-file archiver boundary is project-owned (`tools/crt-ar`), and the
  first allocation ABI slice (`operator new/delete`) is covered by CTest. A
  Android external libc++ and libc++abi now also build as static/shared CRT
  runtime libraries on macOS, install into the sysroot and rootfs, and pass
  static plus shared vector/string/RTTI/exception `crt-libcxx-smoke` runs; host libc++ is not used
  as a fallback. Linux and Windows select their own archiver/C++-include driver
  paths (`crt-ar` and `crt-ar.cmd` with MSVC STL discovery respectively),
  which were statically reviewed from macOS; their actual Skia GN/Ninja host
  runs remain required before cross-host Skia status can be claimed.
- C++ runtime phase 2 is complete on macOS for libc++/libc++abi. Linux and
  Windows still require a CRT-built libunwind and real host execution; current
  AOSP unwind source is in `toolchain/llvm-project`, not the retired
  `platform/external/libunwind` checkout. The ELF loader/dynamic-linker
  prototype remains a separate lower-layer track.
- **2026-08-21: the libcxx/libcxxabi/libunwind build was restructured from
  hardcoded Python scripts into per-component `recipe.json` files**
  (`libstdc++/third_party/{libunwind,libcxxabi,libcxx}/recipe.json`, driven by
  a new `tools/crt-libcxx-build.py`) -- see `HISTORY.md`'s dated entry for the
  full writeup, including three real Windows toolchain bugs found and fixed
  (mksh's own exec needing forward-slash paths, `CMAKE_CXX_COMPILER_ARG1` not
  reaching every TryCompile, libunwind's `CMakeLists.txt` needing sibling LLVM
  cmake directories a sparse checkout doesn't carry). Real libunwind now
  builds as part of this pipeline (previously never attempted at all).
  `crt-libcxx-configure` now succeeds for all three recipes on Windows;
  `crt-libcxx-build` still fails partway through libcxxabi on genuine C++ ABI
  source-portability gaps (`_LIBCPP_WIN32API` needing `<windows.h>`/MSVC-only
  `_aligned_malloc` this project's freestanding build doesn't provide) --
  real porting work, deliberately deferred rather than rushed; see `TODO.md`'s
  C++ runtime prerequisite section. Full local `ctest` (119/119 on Windows)
  confirms this restructuring introduced no regression to the default
  build/test workflow, which never touches the `crt-libcxx-*` targets.
- **2026-08-21 (second pass): executed items 1-3 of the revised libunwind
  adoption plan** (Track A / C++ exceptions only; Track B / debug backtraces
  deliberately excluded per the user's own review-informed direction -- see
  `TODO.md`'s C++ runtime prerequisite section for the full 5-item plan and
  `HISTORY.md`'s dated entry for the complete writeup). `-fdwarf-exceptions`
  now forces portable DWARF-CFI unwinding instead of native SEH on
  `*-w64-mingw32` (applied to both `CMAKE_CXX_FLAGS` and `CMAKE_C_FLAGS`);
  `_aligned_malloc`/`_aligned_free` are now real, implemented in
  `libc/src/malloc.c` via `posix_memalign()` and declared in
  `include/stdlib.h` (matching real MSVC header placement, not
  `<malloc.h>`); libcxxabi's shared `.dll` now correctly links against
  libunwind's import library (`CMAKE_SHARED_LINKER_FLAGS` override, since a
  Windows DLL must resolve every symbol at its own link time unlike a static
  `.a` or an ELF `.so`). Six more real toolchain bugs found and fixed along
  the way (`-Wl,/libpath:` word-splitting on space-containing paths, stray
  `CMAKE_{C,CXX}_STANDARD_LIBRARIES` MSVC defaults, upstream CMake's
  `if(MINGW)` blocks assuming a real mingw-w64 distribution this project's
  ABI-compatibility-only target doesn't have). All libunwind source files now
  compile clean except one function; the one remaining gap is genuinely
  isolated: `libunwind.cpp`'s `findUnwindSections()` needs real PE/COFF
  module enumeration (`EnumProcessModules`/`psapi.h` +
  `IMAGE_DOS_HEADER`/`IMAGE_NT_HEADERS`/etc. from `winnt.h`) this project has
  never declared -- planned as a small project-owned header shim, same
  pattern as `libc/src/arch/windows/`'s existing raw `__declspec(dllimport)`
  prototypes. Full local `ctest` (119/119 on Windows) confirms no regression.
