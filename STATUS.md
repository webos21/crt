# Project Status

A single, lightweight snapshot of "does it currently pass, and what's known
to still be broken." This is deliberately not a release process (no tags,
versions, or CHANGELOG yet -- `LICENSE.md` itself only landed recently, and
there are no external consumers) -- just a page that stays honest about
current state so the next session doesn't have to re-derive it from
`HISTORY.md`. Update this alongside any change that shifts what "passing"
means; if this page and `HISTORY.md`/`TODO.md` disagree, the dated entries
in those two win.

- **Real keyboard/mouse input wired through `crtgfx/window.h`'s public
  `crtgfx_window_poll_event()` API on all three targets, all three now**
  **verified live with real physical input (2026-08-25).** Linux: real
  `wl_seat`/`wl_keyboard`/`wl_pointer` + a new `libcrtgfx/third_party/
  xkbcommon` port. Windows: real `WM_KEYDOWN`/`WM_CHAR`/mouse-message
  handling. macOS: real `NSEventTypeKeyDown`/`FlagsChanged`/mouse-event
  handling -- implemented as reasoned-but-unverified (no macOS host
  access this session), then confirmed working on real macOS hardware by
  the user the same day. See `TODO.md`'s "Current baseline" bullets and
  `HISTORY.md`'s matching dated entry for the full trail.
- **Skia's real FreeType-backed font manager: end-to-end text rendering**
  **now verified on all three targets (Windows/macOS 2026-08-24,**
  **Linux 2026-08-25).** `crtgfx` -> Skia -> `SkFontMgr_New_Custom_
  Directory()` -> this project's own FreeType port, a real
  `canvas->drawString()` producing real ink pixels, not a stub font
  manager. Linux was the last one, unblocked by fixing this project's
  own imported static `libc++.a` (next bullet) -- `crtgfx_keyboard_
  interactive` (a manual demo binary) now also draws real typed text on
  screen this same way. See `HISTORY.md`'s matching dated entries for
  the full trail.
- **This project's own imported static `libc++.a` fixed on Linux
  (2026-08-25, resolving the gap this page previously listed as open).**
  The originally-reported "only 3 archive members, no locale/iostream at
  all" was a stale build artifact, not a structural bug -- a genuinely
  fresh rebuild produces a full, correct archive with no source changes.
  Fixing that exposed a real, second bug (a `__dso_handle` multiple-
  definition conflict between `libc.a`'s own copy and a shim `libcxx`/
  `libcxxabi` inject for macOS), fixed at the CMake level. Both `CRT_USE_
  IMPORTED_LIBCXX=ON` and the default `OFF` config pass the full `ctest`
  suite with no regressions. See `HISTORY.md`'s matching dated entry.
- **New `freetype` port (2.14.3, 2026-08-24): `shared-pass` on Linux x64**
  **and Windows x64, macOS not attempted yet.** Phase 1 of the "notepad-
  capability" plan (real font rasterization -- see `TODO.md` for phases
  2/3, keyboard input and Wayland `wl_seat`). A real `FT_New_Face()`/
  `FT_Set_Pixel_Sizes()`/`FT_Load_Char(..., FT_LOAD_RENDER)` round trip
  against a bundled real font (`libcrtgfx/assets/fonts/DejaVuSansMono.ttf`)
  confirms an actual rasterized, non-empty glyph bitmap on both hosts, not
  just a successful compile. Windows needed three real, general bugs found
  and fixed: a GNU Make static-pattern-rule syntax collision from
  Windows drive-letter colons in FreeType's own generated Makefile (fixed
  via three new, generic `tools/crt-port-build.py` recipe fields --
  `configure_cwd`/`pre_configure_copy`/`post_configure_patch`); a real
  stack-overflow in this project's own ported `make.exe` parsing
  FreeType's unusually large Makefile tree (fixed generally in
  `porting/recipes/make.json`'s own Windows `LDFLAGS`, a 16 MiB stack
  reserve); and libtool mis-wrapping the Windows resource compiler for a
  purely cosmetic `ftver.rc` rule (worked around narrowly, this recipe
  only). Also fixed a general, Windows-only transient file-lock bug in
  `tools/fetch_ports.py`'s own archive-extraction rename. See
  `HISTORY.md`'s matching entry and `docs/porting_status.md`'s new
  `freetype` section for the full trail. Full `ctest` stayed at 121/121 on
  Windows throughout.
- **New `expat` port (2.8.3, 2026-08-24): `shared-pass` on Linux x64 and**
  **Windows x64, macOS not attempted yet.** Added as a build dependency for
  the upcoming core Wayland external build (`wayland-scanner` needs it to
  parse protocol XML), not part of the existing networking/TLS port queue.
  Windows needed a `rand_s()` compatibility shim, a `--without-dev-urandom`
  override, and a general `tools/fetch_ports.py` fix for a real Windows
  symlink-resolution bug (`WinError 123` on forward-slash relative symlink
  targets from a POSIX tar archive) -- see `HISTORY.md`'s matching entry and
  `docs/porting_status.md`'s new `expat` section. Full `ctest` stayed at
  121/121 on Windows throughout.
- **New Wayland core external build (2026-08-24): verified end to end on**
  **both Linux x64/WSL (real live-compositor round trip) and Windows x64**
  **(build+link verified, designed headless fallback).**
  `crtgfx-wayland-configure`/`-build`/`-smoke` CMake targets, a pinned
  `libcrtgfx/third_party/wayland/recipe.json` (Wayland 1.26.0, wayland-
  scanner + wayland-client only this pass). No Meson/Ninja/pkg-config
  host-tool dependency at all: `tools/build_wayland.py` drives every
  compile/link step directly via `tools/crt-cc` (rewritten mid-session
  from an initial Meson-based version, at the user's own explicit
  direction, specifically to drop that host-tool dependency). On Linux,
  `crtgfx-wayland-smoke` ran a real `wl_registry` round trip against a
  live Wayland compositor this session's own WSL environment happened to
  have reachable (WSLg's own Weston), enumerating 20 real compositor
  globals. On Windows (no real compositor available), it correctly hits
  the designed graceful fallback and still reports `ok`. Full `ctest`:
  **104/104 on Linux x64, 121/121 on Windows x64.**
  Nine real bugs found and fixed getting both platforms working, none
  routed around: three cross-platform CRT/PAL gaps (`SO_PEERCRED`/`struct
  ucred`/`MSG_CMSG_CLOEXEC`/`MSG_DONTWAIT`/`MSG_NOSIGNAL`/`SOCK_CLOEXEC`
  in `include/sys/socket.h`, `ppoll()` in `include/poll.h`/`libc/src/
  poll.c` -- rewritten portable, not Linux-only, after Windows compile
  surfaced Wayland's own unconditional call site -- and `epoll_create()`
  in `include/sys/epoll.h`/`libc/src/epoll.c`); three Windows-specific
  build-orchestration gaps in the new scripts themselves (missing POSIX-
  form `PATH` for `crt-cc`'s own internal `printf`/`sed` calls, a missing
  `CRT_WINDOWS_SDK_LIBPATH` for linking a real Windows executable directly
  via `crt-cc` for the first time in this project, and a `CRT_HOST_CC`
  omission in the smoke-test script specifically); and two more general,
  unrelated bugs: `tools/crt-port-build.py`'s AR/RANLIB/STRIP defaults now
  also search the real `clang` binary's own directory (fixes a genuine
  GNU-ranlib segfault on a real Ubuntu/WSL host with no `llvm-ranlib` on
  PATH), and `CRT_BUILD_PRESET_NAME` (top-level `CMakeLists.txt`) now
  computes a real relative path instead of just a basename, fixing
  `port-build-*`/`port-test-*` targets inside any nested shadow build
  directory. A known, standing "transient bootstrap-libc++-overwrites-
  the-real-imported-one" ordering artifact (this file's own 2026-08-18
  macOS entry's class of bug) recurred twice on Windows during this same
  investigation, unrelated to Wayland itself -- re-fixed both times by
  restaging `crt-libcxx-sysroot`; a real, permanent fix to the underlying
  ordering hazard is still an open follow-up, not solved here. See
  `HISTORY.md`'s matching entry for the full trail.
- **New opt-in `crtgfx-skia-smoke` CMake target (2026-08-23): passing.**
  `cmake --build <dir> --target crtgfx-skia-smoke` now builds and runs
  `crtgfx_skia_raster_smoke` end to end via a dedicated shadow build
  directory, regardless of the calling directory's own current
  `CRTGFX_ENABLE_SKIA`/`CRT_USE_IMPORTED_LIBCXX` cache state -- mirrors
  `crt-libcxx-smoke`'s existing self-sufficient role. `cmake --workflow`
  itself is unchanged; both flags stay `OFF` by default there. See
  `HISTORY.md`'s matching entry for the two real bugs found getting a
  from-scratch run working.
- **Windows CI (`windows-x64`/`windows-arm64`) fixed after going red from
  the mingw32-unification push (2026-08-23): two real bugs, diagnosed from
  the user's own pasted CI logs (a real host MinGW-w64 install on GitHub's
  runner poisoning `crt_cxx_build_flags`'s implicit-include detection, and
  `CMAKE_HOST_SYSTEM_PROCESSOR` misreporting under x64 emulation on the
  real ARM64 runner). See `HISTORY.md`'s matching, topmost entry. Verified
  regression-free locally (121/121); the fixes themselves can only be
  confirmed by CI on the real runner images -- watch the next push.
- **Latest Windows imported C++ runtime status (2026-08-23): passing.** The
  pinned LLVM libc++/libc++abi/libunwind recipes build static and shared
  libraries, stage them into the Windows sysroot, and pass both
  `crt-libcxx-smoke` linkage modes. The default Windows regression suite also
  passes 120/120 after this work. `<filesystem>` was compiled as part of
  libc++, but its Windows-specific behavioral coverage remains a follow-up.
- **Latest Skia CPU-raster build status (2026-08-23, same day): passing.**
  `cmake --build --preset windows-host-ninja-debug --target
  crtgfx-skia-build` now exits 0 -- `libskcms.a` and `libskia.a` both link
  and Skia installs, matching macOS/Linux arm64. Five real bugs fixed along
  the way (GN's own hardcoded `python3` token, a `cmd.exe` quoting trap,
  two `win32_shim` gaps, a `tools/crt-ar` response-file parsing bug); see
  `HISTORY.md`'s matching entry.
- **`crtgfx_skia_raster_smoke` on Windows (2026-08-23, same day, resolved):
  passing.** The ABI-mismatch gap noted below was resolved by unifying
  Windows regular-CMake C++ to `--target=*-w64-mingw32` project-wide
  (explicit user go-ahead, after a read-only review pass), matching what
  every `tools/crt-cc`/`tools/crt-c++`-driven "port" build already used.
  Ten more real, distinct bugs surfaced and were fixed getting there (CMake
  library-naming-convention shift, a genuine CMake/Ninja `.ctors`/`.dtors`
  link-order gap, per-arch `long double` ABI, wiring regular CMake C++ onto
  this project's own imported libc++, Skia's `SK_BUILD_FOR_UNIX` trap on
  this second compile path, a missing `atan2f`, `-femulated-tls` instead of
  new native COFF TLS support, a silently-empty `crt_compiler_rt_builtins`
  on every prior Windows CMake build, plus `uuid.lib`/a vendor builtins
  archive's own internal UCRT-only `fprintf` fallback). `crtgfx_skia_
  raster_smoke: ok`, full suite **100% passing, 121/121**, with
  `CRTGFX_ENABLE_SKIA=ON` and `CRT_USE_IMPORTED_LIBCXX=ON` both on, verified
  on both an isolated scratch build dir and the real `windows-host-ninja-
  debug` preset. `CRTGFX_ENABLE_SKIA` stays `OFF` by default (unchanged;
  the default regression suite below is unaffected). See `HISTORY.md`'s
  topmost 2026-08-23 entry for the full per-bug writeup and `TODO.md`'s
  matching dated sub-bullet.
  - The mingw32-unification's original trigger, first found and
    deliberately left open below, is superseded by the above: regular
    CMake C++ code compiling `-pc-windows-msvc` (MSVC-mangled) while Skia
    compiled `-w64-mingw32` (Itanium-mangled) is no longer true -- both
    now share the same target/ABI.
- **`crtgfx_skia_raster_smoke` on Linux/WSL (2026-08-23, same day, direct
  follow-up): passing.** Asked directly whether the smoke test itself
  (not just Skia's own `libskia.a` build) had ever passed on Linux amd64 --
  it hadn't. Building and running it for real found and fixed four more,
  entirely Linux-specific bugs: a GNU-ld (`ld.bfd`) circular-archive-
  resolution gap in `crtgfx`'s own linking (fixed with `-Wl,--start-group`/
  `--end-group`, declared directly in `libcrtgfx/CMakeLists.txt` where
  `crtgfx`'s own dependencies are set, not per-consumer -- a consumer-side
  attempt does not reliably bracket `crtgfx`'s own separately-flattened
  transitive deps); a redundant explicit `cxx` link on `crtgfx_skia_
  raster_smoke` itself landing outside that new group and getting de-duped
  away; the same bootstrap-`cxx`-to-real-imported-libc++ swap Windows
  already needed (Linux's own headers were already fine via `crt_cxx_
  build_flags`'s existing fallback, only the link needed it); and a real,
  if obscure, `ld.bfd`-specific runtime-loader bug (`unexpected PLT reloc
  type 0x00`, `readelf -r` showing a bogus `R_X86_64_NONE` `.rela.plt`
  entry) fixed by switching just the Skia-enabled targets to `-fuse-ld=
  lld`. `crtgfx_skia_raster_smoke: ok`, full suite **100% passing,
  105/105**, both flags on, verified via the real `ctest`-driven run (run
  twice for consistency). See `HISTORY.md`'s matching, topmost 2026-08-23
  entry and `TODO.md`'s matching dated sub-bullet. `CRTGFX_ENABLE_SKIA`
  stays `OFF` by default on Linux too.

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
- **2026-08-21 (third pass): libunwind and libcxxabi now build clean on
  Windows, static and shared.** The planned `libstdc++/third_party/
  win32_shim/{windows,psapi,ntverp}.h` header shim closed the
  `findUnwindSections()` gap from the pass above, plus two more real gaps
  found getting it to actually link (`extern "C"` missing on the shim's
  declarations, causing C++-mangled references that could never match
  kernel32.lib's plain-C export names; a false-positive
  `LIBUNWIND_HAS_PTHREAD_LIB` CMake probe pulling in a `-lpthread` this
  project deliberately never provides). libcxxabi's own shared `.dll` then
  needed one more fix: `_LIBCPP_BUILDING_LIBRARY` (a libcxx-side macro,
  distinct from libcxxabi's own `_LIBCXXABI_BUILDING_LIBRARY`) was never
  defined for two of its own source files, so `std::runtime_error`/
  `bad_cast`/`bad_typeid`'s vtables got declared `dllimport` instead of
  `dllexport` and the shared link failed on undefined vtable symbols.
  libcxx itself compiles almost entirely clean after fixing a `<filesystem>`
  build-target mismatch (Windows never enables it upstream), except 6 files
  hitting two real, unconditional missing Windows headers (`xlocinfo.h` via
  libcxx's own MSVC-UCRT locale backend, `winapifamily.h`) -- this is a
  materially bigger, architecturally different gap than the shims above
  (would mean linking the hosted `ucrtbase.dll` this project otherwise
  avoids everywhere; Android's own libcxx fork already carries an
  alternate Bionic-shaped locale backend that looks like the better fit,
  but switching to it needs a real audit, not a quick flip -- see
  `TODO.md`'s C++ runtime prerequisite section, step 4, and `HISTORY.md`'s
  dated entry). Full local `ctest` (119/119 on Windows) confirms no
  regression to the default build/test workflow.
- **2026-08-21 (fourth pass): static libc++ now works end to end on
  Windows, real exceptions included -- `imported_libcxx_test: ok`,
  matching macOS and Linux's own passing marker for the first time.**
  Redirected libcxx to its own Android/Bionic locale/random backends
  instead of MSVC's (six independent `_LIBCPP_MSVCRT_LIKE`-family branch
  points patched, all scoped to `__BIONIC__` so macOS/Linux stay
  untouched) rather than trying to shim real Universal CRT (`ucrtbase.
  dll`) locale/`rand_s()` functions this project has consistently avoided
  depending on elsewhere -- this project's own `xlocale.h`/`/dev/urandom`
  emulation already covered nearly everything needed. Getting a real
  client program (not just the runtime's own build) to actually
  compile+link+run then surfaced a long chain of further real gaps,
  never exercised on Windows before now: `tools/test_libcxx_runtime.py`
  needed the same rootfs/PATH/`CRT_HOST_CXX` treatment `tools/crt-libcxx-
  build.py` already had; `tools/crt-c++` had never linked a real Windows
  C++ executable before (missing `prelibs` for ctors-walking/pseudo-
  reloc, missing `-fdwarf-exceptions` on client code, missing
  `_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS` for static-archive consumers);
  `libc++.dll` needed the same libunwind-import-library fix already
  applied to `libc++abi.dll`; and wiring `crt1_pseudo_reloc.o` into every
  shared-DLL link surfaced a genuine regression (`pseudo_reloc.c`'s own
  diagnostic path colliding with a regression test's deliberately-
  shadowed `read()` via `libc/src/fd.c` bundling read/write together),
  fixed by moving that file's diagnostics to raw kernel32 calls with zero
  dependency on this project's own libc at all. The *shared* leg of
  `crt-libcxx-smoke` remains open: `libc++.dll` does not export enough of
  `basic_string`'s inline members for a client that sees the whole class
  `dllimport`-decorated -- a separate, deeper libcxx-extern-template-
  instantiation problem, deliberately left for its own pass. See
  `TODO.md`'s C++ runtime prerequisite section, step 4, and `HISTORY.md`'s
  dated entry for the full writeup. Full local `ctest` (119/119 on
  Windows) confirms no regression to the default build/test workflow.
- **2026-08-21 (fifth pass, same day): shared libc++ now also works end
  to end on Windows -- `imported_libcxx_test: ok` for both linkage modes,
  matching macOS and Linux.** Four separate root causes, each hiding the
  next: (1) libcxx's export-table gap for `basic_string`-family members,
  fixed with two layered `recipe.json` patches (`-fvisibility-inlines-
  hidden` disabled for Windows only; `_LIBCPP_EXTERN_TEMPLATE_TYPE_VIS`
  matched to `_LIBCPP_CLASS_TEMPLATE_INSTANTIATION_VIS`'s own
  `dllexport`, a real upstream declaration/definition macro mismatch
  confirmed via Clang's own "'dllexport' attribute ignored ... missing on
  previous declaration" warning); (2) `tools/crt-c++`'s shared branch
  never linked `libc++abi.dll.a` at all (only the static branch did),
  leaving every `__cxa_*`/vtable/`__gxx_personality_v0` symbol undefined
  even though `libc++abi.dll.a` itself genuinely, correctly exports all
  of them (`llvm-nm`-verified); (3) `tools/test_libcxx_runtime.py` reused
  the mksh-only POSIX `PATH` (needed for the *compiler* step) to also run
  the resulting *native* `.exe`, so the real Windows DLL loader could
  never find `libc++.dll`/`libc++abi.dll`/`libunwind.dll` in
  `sysroot/bin` -- `STATUS_DLL_NOT_FOUND` at run time despite a clean
  link; (4) even with a correct run-time `PATH`, those two DLLs were
  never staged into the sysroot at all, because both `libcxx/lib/
  CMakeLists.txt`'s and `libcxxabi/src/CMakeLists.txt`'s own upstream
  `install(TARGETS ...)` calls never specified a `RUNTIME DESTINATION`
  (only `LIBRARY`/`ARCHIVE`) -- CMake silently skips installing an
  artifact kind with no destination given, confirmed by contrast with
  libunwind's own working sibling rule. Fixed with one `RUNTIME
  DESTINATION` patch per recipe. See `TODO.md`'s C++ runtime prerequisite
  section, step 4, and `HISTORY.md`'s dated entry for the full four-part
  writeup. Full local `ctest` (119/119 on Windows) confirms no
  regression.
- **2026-08-21 (sixth pass, same day): TODO.md item 7 (native-callback/
  boundary safety net for Windows) done -- and its own originally-
  sketched design disproved by an empirical repro before being replaced
  with one that actually works.** The item's own text proposed wrapping
  every native-callback entry point in a real-SEH boundary frame; a
  standalone repro (raw `clang --target=x86_64-w64-mingw32`, no CRT
  needed) showed this does NOT catch a hardware fault raised several
  DWARF-compiled frames deep -- the OS's own frame-based search still has
  to walk the untabled frames beneath the boundary and fails at the
  first one, confirmed via a control run with the same repro using
  real-SEH callees throughout (that one caught cleanly). `Set
  UnhandledExceptionFilter()` was tried next (its own "only fires if
  nothing else handled it" contract can never preempt a legitimate
  `__except`) and also disproved the same way: it never fires either,
  since reaching "unhandled" needs the identical broken walk. What
  actually works: `AddVectoredExceptionHandler()` (VEH), which doesn't
  walk the stack at all -- confirmed to reliably fire for the same fault
  with no boundary shim anywhere. Its own real risk (VEH always fires
  *before* frame-based SEH, confirmed to preempt and break even a fully
  legitimate `__except` elsewhere) is closed by gating the handler on
  `RtlLookupFunctionEntry()` -- real `.pdata` present means defer
  completely (confirmed to let a real `__except` win exactly as if this
  handler didn't exist); absent means take over, log a diagnostic, and
  `ExitProcess()` with this project's own `128 + <POSIX signal>`
  convention. Shipped as `libc/src/arch/windows/common/dwarf_unwind_
  safety_net.c`, wired into every executable and shared DLL (`crt1.c`/
  `dllcrt.c`/`libc/CMakeLists.txt`/top-level `CMakeLists.txt`'s
  `crt_configure_shared_runtime()`/`tools/crt-cc`/`tools/crt-c++`).
  Surfaced one real regression along the way (the new startup-hook
  symbol auto-exported and collided across a chained-DLL test, fixed
  with `-Wl,--exclude-symbols=` alongside the same latent-but-never-
  triggered risk in `_pei386_runtime_relocator`) and one useful finding
  while writing the new permanent regression test (`tests/windows_dwarf_
  unwind_safety_net_test.c`/`_victim.c`): this project's own Windows
  `waitpid()` always reports `WIFEXITED`, never `WIFSIGNALED`, for any
  child regardless of how it actually died -- a separate, already-
  tracked, deliberately-undecided question (`docs/signal_delivery.md`'s
  own "Next Steps"), not something this pass expanded scope to fix. See
  `TODO.md`'s C++ runtime prerequisite section, step 7, and `HISTORY.md`'s
  dated entry for the full five-repro empirical trail. Full local `ctest`
  (120/120 on Windows, the new test included) confirms no regression.
- **2026-08-21 (seventh pass, same day): libcxx/libcxxabi/libunwind
  pinned to exact commit SHAs; libcxx/libcxxabi sparse-checkout trimmed
  to drop their own unused `test/` suites.** Evaluated (on request)
  whether to vendor the C++ runtime source into this repo versus keep
  the existing build-time `git clone`; the two low-cost recommendations
  from that evaluation (pin exact SHAs regardless; skip full vendoring
  for now) were implemented directly. All three recipes had `"ref":
  "refs/heads/main"` -- a floating branch with no lockfile, a real
  reproducibility gap this project's own patch fail-fast behavior only
  partially covers (it catches drift in the specific text a patch
  touches, not anything else in three multi-megabyte components).
  Pinning to a raw SHA needed a real `tools/crt-libcxx-build.py` fix,
  not just a JSON edit: `git clone --branch <sha>` does not work against
  `android.googlesource.com`'s Gerrit/JGit backend (confirmed for real:
  "Remote branch <sha> not found in upstream origin"), only a separate
  `git fetch origin <ref>` + `checkout --detach FETCH_HEAD` does, for
  either a branch name or a raw SHA. A real mistake was made and caught
  mid-implementation: the first fix attempt also dropped `--depth 1`
  from the initial clone (misreading an interactive test that had
  actually kept it), which silently turned into a 10+ CPU-minute full
  history clone of the giant `toolchain/llvm-project` monorepo before
  the stuck build was noticed (via `Get-Process` showing climbing git
  CPU time) and killed -- `--filter=blob:none` alone does not trim the
  commit graph, only blob content. Fixed by restoring `--depth 1`
  (~6s for the same step once corrected). Also added `sparse_paths`/
  `checkout_subdir: "."` to libcxx and libcxxabi (previously libunwind-
  only, for a different reason -- monorepo subpath extraction versus
  same-repo unused-directory trimming), derived by actually reading each
  repo's own CMakeLists.txt for what it unconditionally needs. Confirmed
  via a genuinely fresh fetch: libcxx 52MB -> 9.0MB, libcxxabi 7.3MB ->
  612KB, `test/` gone from both, and a full `crt-libcxx-build` +
  `crt-libcxx-smoke` + `ctest` cycle against the freshly pinned-and-
  trimmed source still reports `imported_libcxx_test: ok` (both linkage
  modes) and 120/120 tests passing. See `HISTORY.md`'s dated entry for
  the full writeup.
- **2026-08-21 (eighth pass, same day): sysroot/rootfs staging verified
  for the imported-libc++ config Skia needs, and Skia's own fetch pinned
  + sparse-checked-out + verified building for real on Windows.**
  `CRT_USE_IMPORTED_LIBCXX=ON` sysroot/rootfs builds confirmed correct
  (all 8 expected runtime libraries staged, `skia.h` present in
  `sysroot/include`, 120/120 no regression). Wayland has nothing to
  pin today (`libcrtgfx/third_party/wayland/README.md`: "intentionally
  not a checkout," matching TODO.md's own already-decided no-vendor-yet
  policy) -- left as-is per explicit choice. Skia: `CRTGFX_SKIA_REF`/
  `CRTGFX_SKIA_EXPECTED_COMMIT` now default to a real pinned commit
  (was empty/floating `refs/heads/chrome/m148`); `tools/fetch_skia.py`
  gained cone-mode sparse-checkout (new `CRTGFX_SKIA_SPARSE_PATHS`,
  derived empirically via `ninja -t inputs skia` against a real build,
  not guessed); `CRTGFX_SKIA_SYNC_DEPS` now defaults OFF (confirmed for
  real: unconditionally downloads Skia's entire third-party set
  regardless of GN flags -- 8.6GB including a full Emscripten/WASM
  toolchain before being killed -- and confirmed unnecessary for this
  project's own minimal CPU-raster config once `skia_use_wuffs` is also
  disabled, the one codec flag left at Skia's own default while every
  sibling was already off). `tools/build_skia.py` gained two real
  Windows fixes: `gn.exe` auto-bootstrap (the previous bare-`gn`
  existence check never matched on Windows) and a throwaway `python3.bat`
  PATH shim (`gn gen` otherwise fails outright -- Skia's own `.gn`
  dotfile hardcodes `script_executable = "python3"`, absent by that name
  on a stock Windows Python install). A real mistake (missing `--depth 1`
  on the sparse clone, the exact same class of bug already fixed once
  this same day for `libstdc++/third_party/*/recipe.json`) was made and
  caught for real (189MB `.git`, 464,512 packed objects, from one actual
  `crtgfx-skia-fetch` run) before being fixed to ~22MB. Verified through
  the real `crtgfx-skia-fetch`/`crtgfx-skia-build` CMake targets, not a
  scratch script: a genuine `libskia.a` (21MB) was produced. Going one
  step further (`CRTGFX_ENABLE_SKIA=ON` + `crtgfx_skia_raster_smoke`)
  surfaced a separate, pre-existing, previously-undiscovered Windows
  link gap (duplicate `printf`/`fprintf`/`snprintf`/`fabsf`/`fabsl`/
  `frexpl`/`wmemcpy`/`wmemset`/`wmemcmp` between this project's own
  `c.lib`/`m.lib` and MSVC-UCRT-inline-materialized copies, root-caused
  to `crt_cxx_build_flags` deliberately omitting `-nostdinc++` on
  Windows only) -- confirmed unrelated to this pass's own changes and
  deliberately left open as a new, separate, tracked `TODO.md` item
  rather than rushed. `CRTGFX_ENABLE_SKIA` restored to its original OFF
  default; a stale `exports.def` left from briefly toggling it on (pure
  local incremental-build staleness, confirmed by deleting and
  rebuilding clean) was cleared. Full `cmake --build` + `ctest`
  (120/120) with the default config confirms no regression. See
  `HISTORY.md`'s dated entry for the full writeup.
- **2026-08-22: a real WSL/Ubuntu-20.04 attempt confirmed the Skia GN
  build reaches the same two library-completeness gaps on Linux with
  zero toolchain-wiring fixes needed, then the `<inttypes.h>` gap was
  fixed for real.** WSL's own stock git (2.25.1) made a routine
  partial-clone fetch balloon to 3.6GB+ and fail (HTTP 502) instead of
  the expected tens of MB -- fixed by upgrading to git 2.50.1 via
  `ppa:git-core/ppa`, not a recipe bug. With that fixed, the base
  project built clean (104/104 `ctest`) and Skia's GN build reached
  real compilation with none of the eight Windows-specific fixes from
  earlier the same day needed, landing on the exact two gaps predicted:
  C++20 `<bit>` and `<inttypes.h>`'s missing `imaxdiv_t`/`imaxabs`/
  `imaxdiv`/`wcstoimax`/`wcstoumax`. The second one was fixed directly
  in this project's own `include/inttypes.h`/`libc/src/inttypes.c`
  (confirmed independent of the libc++ pin question) -- verified via
  120/120 (Windows) and 104/104 (Linux) `ctest`, plus a fresh Skia
  rebuild whose failure surface shrank to exactly one object file
  (`SkMathPriv.o`), tracing only to the still-open `<bit>` gap. See
  `HISTORY.md`'s dated entry for the full writeup.
- **2026-08-22: Skia's source pin/sparse-checkout moved into a real
  `libcrtgfx/third_party/skia/recipe.json`**, matching `libstdc++/
  third_party/*/recipe.json`'s own shape (adapted for GN/Ninja, no
  `cmake.options` section). `libcrtgfx/CMakeLists.txt` now reads it via
  `string(JSON ...)` instead of hardcoding the same six values inline;
  verified via a fresh configure producing matching cache values plus
  120/120 `ctest`. A placeholder `recipe.json` (`source.ref: null`) was
  also added for Wayland, which still has nothing pinned or fetched.
  See `HISTORY.md`'s dated entry.
- **2026-08-22: routed Skia's own GN build through the project-owned
  imported libc++ instead of real MSVC STL, fixing eight distinct real
  bugs; final link still blocked on two separate library-completeness
  gaps.** Follow-up to the link gap above. Fixed, in order: GN's Windows
  `msvc_toolchain` ignoring `cc`/`cxx` (bypassed via `target_os =
  "linux"`, matching the existing macOS trick); `gcc_like_toolchain`
  needing native-Windows compiler launchers (`crt-cc.cmd`/`crt-c++.cmd`
  + `shutil.which()`-resolved `CRT_HOST_CC`/`CRT_HOST_CXX`); a
  previously-undocumented PAL bug where `libc/src/env.c`'s
  `__crt_rootfs_bootstrap()` auto-`chdir("/")`s any CRT-libc process
  (`mksh.exe` included) whenever `CRT_ROOTFS` isn't already set,
  discarding ninja's own working directory (fixed by pre-seeding
  `CRT_ROOTFS`); mksh's `exec()` failing on any path containing a space
  regardless of slash direction (fixed via 8.3 short-path conversion);
  a real PATH-format conflict between `gn.exe` (needs real Windows
  `PATH` for `python3`) and `mksh.exe` (needs `:`-separated POSIX `PATH`
  by this project's own deliberate `MKSH_CRT_WINPATH` design, see
  `shell/toybox/PATCHES.md`) both sharing one subprocess tree (fixed by
  patching the fetched `.gn` dotfile's `script_executable` to an
  absolute path instead, replacing the old PATH-shim approach);
  `--target-arch` arriving as `AMD64`/`ARM64` (CMake spelling) instead
  of the GNU-triple spelling `tools/crt-cc` needs (new
  `normalize_target_arch()`); and clang's mingw target predefining
  `_WIN32` regardless of GN's `target_os`, which broke `SK_ALWAYS_INLINE`
  -> `__forceinline` (fixed via `-DSK_BUILD_FOR_UNIX`, extending the
  same override macOS already had). With all eight fixed, real
  compilation got underway for the first time (34/544 ninja steps, real
  `.o` files) before hitting two genuine library-completeness gaps: the
  pinned libc++ commit predates C++20 `<bit>` (`std::popcount`/
  `countl_zero`/`countr_zero`, needed by `SkMathPriv.h`) entirely, and
  this project's own libc `<cinttypes>` doesn't declare `imaxdiv_t`/
  `imaxabs`/`wcstoimax`/`wcstoumax`. Both deliberately left open (user
  chose to stop and document rather than risk the already-verified
  libc++ commit pin) -- see `TODO.md`'s dated sub-bullet for the full
  chain and `HISTORY.md`'s dated entry for the fuller per-bug writeup.
  Full default `ctest` (120/120) with `CRTGFX_ENABLE_SKIA` left OFF
  confirms zero regression from all eight fixes.
- **2026-08-22 (later same day): migrated libcxx/libcxxabi's recipe source
  off the dead Android forks onto `toolchain/llvm-project`, fixing the
  `<bit>` gap above for real. Linux/WSL fully verified (100%, 104/104
  tests); Windows: six more real bugs fixed, then a deliberate stop at a
  clearly-scoped remaining gap.** `CRT_USE_IMPORTED_LIBCXX=ON`:
  **Linux/WSL now builds and passes its full default `ctest` suite
  end-to-end from a fully wiped build tree** -- **100% tests passed, 0
  failed, out of 104**. On Windows, fixed real, distinct bugs the same
  evidence-based way used throughout this whole libcxx/Skia effort: a
  `_tls_index` link failure (libc++abi's own native-`thread_local`
  branch needing a real CRT TLS directory this project's `crt1.o` never
  provides -- redirected to its own already-working pthread-key
  fallback); `_LIBCPP_MSVCRT_LIKE` wrongly assumed for any `_WIN32`
  target (patched to exclude `__BIONIC__`, routing locale support back
  to this project's own already-correct POSIX-shaped `newlocale`/
  `freelocale`); and four rounds of missing Win32 declarations closed by
  extending `win32_shim/` (`AreFileApisANSI`/`WideCharToMultiByte`/
  `MultiByteToWideChar` for `filesystem/path.cpp`, `LARGE_INTEGER`'s
  `LowPart`/`HighPart` for `filesystem/time_utils.h`, a new
  `win32_shim/winerror.h` with 49 real `ERROR_*` codes for
  `system_error.cpp`, and redirecting `<print>`'s terminal detection to
  the portable `isatty()` path already used everywhere else in this
  project). Then hit a new, deeper blocker: `libcxx/src/CMakeLists.txt`
  unconditionally compiles `support/win32/{locale_win32,support}.cpp`
  for any Windows target, calling real MSVC-CRT-only functions
  (`_create_locale`, `wcrtomb_s`, `rand_s`, `errno_t`) this project's
  own libc has no equivalent of -- needs a CMake-level source-list patch,
  not another header shim, a distinctly bigger scope. **Asked the user
  how to proceed given the honest scope jump; explicit answer: stop
  here**, since Linux's complete verification already covers the actual
  motivating `<bit>` gap. All six Windows fixes are kept (real, correct,
  and permanent) and confirmed **dormant/inert under the default
  `CRT_USE_IMPORTED_LIBCXX=OFF`** -- a full default-config Windows
  regression run (`cmake --build --preset windows-host-ninja-debug` +
  `ctest`) shows **100% tests passed, 120/120, zero regression**.
  `CRT_USE_IMPORTED_LIBCXX=ON` on Windows remains open, tracked in
  `TODO.md` with the specific next step already scoped. See `TODO.md`'s
  dated sub-bullet and `HISTORY.md`'s dated entry for the full writeup.
