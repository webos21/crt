# TODO: CRT Shell, Rootfs, And Porting Loop

This file tracks the shell/rootfs/porting work queue. The list is ordered by
state: completed work first, current work second, and planned follow-up last.
Detailed policy and provenance stay in `docs/` and import manifests.

## Notice

- Keep recipe statuses current in
  [`porting/recipes/*.json`](porting/recipes/) and
  [`docs/porting_status.md`](docs/porting_status.md) whenever a host is
  rerun. Porting policy and the normal configure/make loop live in
  [`docs/sysroot_ports.md`](docs/sysroot_ports.md); completed porting
  investigations belong in [`HISTORY.md`](HISTORY.md).
- A port is not done until both static and shared builds are attempted
  in the same pass on each host, with any host-specific deferral recorded
  in the recipe notes and status matrix. See
  [`docs/porting_status.md`](docs/porting_status.md) for status meanings.
- For CMake wiring changes, do not trust a long-lived local `out/`
  directory. Verify with a fresh clone or at least
  `cmake --fresh --preset <preset>` before calling the change done; stale
  `CMakeCache.txt`/rootfs artifacts have hidden real CI-only ordering
  bugs before. The resolved cases are recorded in [`HISTORY.md`](HISTORY.md).
- Keep toybox applet enablement tied to audited CRT/PAL support, especially
  LLP64 assumptions on Windows. The live applet list and deferrals are in
  [`docs/toybox_applet_status.md`](docs/toybox_applet_status.md).
- Keep terminal/tty behavior coherent for shell and configure use. Current
  syscall/ioctl coverage is tracked in
  [`docs/sysroot_ports.md`](docs/sysroot_ports.md), with interactive job
  control policy deferred in [`docs/job_control.md`](docs/job_control.md).
- Treat `CRT_SPAWN_NATIVE_WINDOWS=1` as a narrow launcher hint for native
  host tools only. The wrapper details live in [`tools/crt-cc`](tools/crt-cc),
  [`tools/crt-c++`](tools/crt-c++), [`tools/crt-native-tool`](tools/crt-native-tool),
  and [`docs/sysroot_ports.md`](docs/sysroot_ports.md).
- If a new public libc or `__crt_sys_*` symbol is added, regenerate or replace
  [`porting/recipes/mbedtls-windows-exclude-symbols.rsp`](porting/recipes/mbedtls-windows-exclude-symbols.rsp)
  in the same pass. The reason is documented in
  [`porting/recipes/mbedtls.json`](porting/recipes/mbedtls.json) and
  [`docs/porting_status.md`](docs/porting_status.md).


## Done

See [`HISTORY.md`](HISTORY.md) for the full, dated, reverse-chronological
record of completed work. This section stays empty in `TODO.md` itself --
when an item below is finished, move its writeup into `HISTORY.md` (dated,
newest entry first) rather than leaving it here.

## In Progress

Active threads, not a flat list of one-off items.

### Upper runtime roadmap

The upper-runtime work is now starting. The long-term target remains an
Electron-class rebuilt native application runtime, but the first practical
goal is narrower: a lightweight, Bionic-compatible UI/runtime stack that can
prove JavaScript, graphics, media, event-loop, filesystem, dynamic-loading,
threading, and host-window boundaries without trying to clone Electron's full
desktop API ecosystem. See [`docs/runtime_roadmap.md`](docs/runtime_roadmap.md)
and the 2026-08-17 study notes under [`docs/study/`](docs/study/).

Initial source tree shape:

```text
libcrtjs/
  include/
  src/
    *.c                        # common runtime code, directly under src/
    arch/{linux,macos,windows}/
  third_party/quickjs/

libcrtgfx/
  include/
  src/
    *.c                        # common runtime code, directly under src/
    arch/{linux,macos,windows}/
  third_party/
    skia/
    wayland/

libcrtmedia/
  include/
  src/
    *.c                        # common runtime code, directly under src/
    arch/{linux,macos,windows}/
  third_party/ffmpeg/
```

No separate `src/common/` layer in any of the three upper-runtime
libraries (2026-08-18, `libcrtgfx` first, then `libcrtjs`/`libcrtmedia`
matched the same way) -- common/host-independent runtime code lives
directly under `src/`, and only genuinely per-host code lives under
`src/arch/{linux,macos,windows}/`. See `HISTORY.md`.

Boundary decisions from `docs/study`:

- Keep `libc`/PAL focused on Bionic-compatible low-level runtime behavior.
  Graphics, JavaScript, media, host windows, GPU APIs, and application-level
  event loops stay in sibling upper-runtime libraries, not in libc.
- Start with **QuickJS** before V8. QuickJS is the smallest useful pressure
  test for event-loop, timers, module loading, native bindings, filesystem,
  dynamic loading, and process behavior. V8 waits until the C++ runtime,
  JIT/code-memory policy, atomics, threading, dynamic loading, and signal/
  exception story are stronger.
- Start `libcrtgfx` with **Skia as renderer** and a **Wayland-compatible
  boundary** as protocol/compositor vocabulary, but do not build a full
  desktop environment first. The public 2D drawing surface should expose
  normal Skia headers rather than a broad project-owned wrapper API; `crtgfx`
  owns runtime/window/surface/event/backend integration around Skia. Keep Skia
  independent from Wayland protocol parsing; host backends should map top-level
  surfaces to native windows (`HWND`, `NSWindow`, Linux
  Wayland/DRM/EGL/Vulkan/OpenGL as needed) and let Skia render directly to the
  host-appropriate GPU/software target. See
  [`docs/libcrtgfx_api_policy.md`](docs/libcrtgfx_api_policy.md).
- Treat Wayland as a compatibility boundary and future Chromium/Ozone leverage
  point, not as a reason to force Linux display-server internals into Windows
  or macOS. SDL2/GLFW/WebGPU/Vulkan-style alternatives remain fallback
  references if a host-native prototype proves the Wayland path too heavy.
  Use WSLg, Wawona/Wayoa/Cocoa-Way-style projects, Weston, wlroots, and
  Wayland protocol libraries as architecture references first; do not vendor a
  full compositor until the `crtgfx` surface/frame boundary has tests. From
  WSLg specifically, take the top-level-surface-to-native-window, explicit
  buffer handoff, and host-compositor presentation shape; exclude WSL/Linux
  binary execution, distro/VM packaging, RDP rail integration, and vGPU/VA-API
  dependency. See [`docs/libcrtgfx_wayland_plan.md`](docs/libcrtgfx_wayland_plan.md).
- Start `libcrtmedia` after the JS/gfx skeleton exists. FFmpeg is the first
  reference stack, with software decode first; GPU texture/audio-device handoff
  comes only after `libcrtgfx` has a real surface/frame abstraction.

Current baseline, completed on Windows first and recorded in
[`HISTORY.md`](HISTORY.md):

- `libcrtgfx`, `libcrtjs`, and `libcrtmedia` exist as default workflow,
  sysroot, and rootfs runtime artifacts. They build static/shared libraries
  and the shared runtime files are copied into `/system/lib` and `/usr/lib`.
- Common upper-runtime code links through this project's CRT libraries
  (`libc`, `libm`, `libdl`, `libc++`). Only narrow host backend objects are
  allowed to speak native OS window/GPU APIs directly.
- `libcrtgfx` now has real host adapters on all three targets:
  `include/crtgfx/window.h` flows through
  `src/wayland_weston.c`'s Weston-style toplevel/surface state, with
  per-host code underneath -- Win32 (`src/arch/windows/window_win32.c`),
  a hand-rolled core-protocol-Wayland+`xdg-shell` client
  (`src/arch/linux/window_wayland.c`), and real Cocoa driven from C via
  the Objective-C runtime, no `.m` file
  (`src/arch/macos/window_cocoa.c`, 2026-08-18) -- see
  `docs/libcrtgfx_wayland_plan.md`'s "Linux Host Adapter"/"macOS Host
  Adapter" sections for what each covers, its documented scope cuts, and
  how it was verified on real hardware/a real compositor session.
- `crtgfx_window_begin_frame()`/`crtgfx_window_end_frame()` provide the first
  BGRA8888 software buffer commit/present path. `crtgfx_window_smoke` covers
  automation and `crtgfx_window_demo` covers manual bring-up.

Next work order:

1. **Lock the `libcrtgfx` software frame lifecycle.**
   - Define the meaning of `begin_frame()`/`end_frame()` around buffer
     ownership, resize, repeated frame submission, and when a submitted buffer
     may be reused or released.
   - Make Linux Wayland honor real `wl_buffer::release` before freeing a
     submitted `wl_shm` buffer. Windows/macOS already copy the submitted frame
     into host-owned presentation storage, so they satisfy the same contract
     through a different backend policy.
   - Expand `crtgfx_window_smoke` from a single-frame smoke into a small
     repeated-frame lifecycle check, including rejecting nested
     `begin_frame()` calls.
   - Verification rule: run the full workflow on macOS/Linux/Windows and run
     the visible demo on each host when a real desktop/compositor is available.
   - Current status: macOS workflow passed locally on 2026-08-18 after the
     lifecycle/test changes; Linux Wayland backend passed C99/`-Werror`
     syntax checking from macOS. Real Linux and Windows workflow/demo
     verification is still required before this item moves to `HISTORY.md`.
2. **Connect the software frame path to Skia CPU raster drawing.**
   Keep normal Skia headers as the public 2D drawing API and keep
   project-owned headers focused on runtime/surface/present/event integration.
   Start with a deterministic CPU-raster `SkSurface`/`SkCanvas` smoke before
   any GPU backend.
   - Current status: `crtgfx/skia.h`, `src/skia_bridge.cc`, and
     `crtgfx_skia_raster_smoke` build wiring exist. Skia `m148` now builds and
     installs as a CRT-toolchain CPU archive on macOS; `tools/crt-ar` expands
     GN response files so this does not depend on Apple `ar` supporting them.
     No fake Skia headers are provided.
   - Current build automation: `crtgfx-skia-fetch`,
     `crtgfx-skia-configure`, and `crtgfx-skia-build` now exist. The default
     source track is Skia `m148` (`refs/heads/chrome/m148`), with
     `CRTGFX_SKIA_VERSION`, `CRTGFX_SKIA_REF`, and
     `CRTGFX_SKIA_EXPECTED_COMMIT` available for user pinning.
   - The first exposed C++ gap, CRT-owned `operator new/delete`, is complete
     and covered by `cxx_allocation_test`. The remaining gate is a real
     project-owned libc++ standard-library import: the default Skia archive
     uses `std::string`, shared ownership, streams, and locale even with GPU
     disabled. `CRTGFX_ENABLE_SKIA` therefore remains OFF by default; do not
     link host libc++ as a substitute. After libc++ is imported, verify the
     deterministic `crtgfx_skia_raster_smoke` on static and shared paths on all
     three hosts.
   - Cross-host build-driver status: the Linux route selects the POSIX
     `tools/crt-ar` wrapper; the Windows route selects `tools/crt-ar.cmd`,
     passes the invoking Python through `CRT_HOST_PYTHON`, and recognizes the
     MSVC STL include root. Those routes were statically checked on macOS, but
     their real host GN/Ninja workflows still require execution before Skia is
     reported as cross-host verified.
   - **C++ runtime prerequisite (macOS runtime smoke complete):** each of
     `libstdc++/third_party/{libunwind,libcxxabi,libcxx}/recipe.json` declares
     its own source (git repo/ref, plus a sparse-checkout subpath for
     libunwind) and CMake build options -- see `tools/crt-libcxx-build.py`'s
     own module docstring for the schema. `crt-libcxx-fetch`/`-configure`/
     `-build`/`-sysroot` drive all three through the CRT toolchain. Restructured
     2026-08-21 from hardcoded one-off Python scripts into this declarative,
     per-component recipe form (2026-08-21 HISTORY.md entry has the full
     writeup, including three real toolchain bugs the restructuring surfaced
     and fixed on Windows: a backslash-vs-forward-slash path bug in mksh's own
     exec resolution, `CMAKE_CXX_COMPILER_ARG1` not reliably reaching every
     CMake-driven TryCompile, and libunwind's CMakeLists.txt needing sibling
     `cmake/`/`runtimes/cmake/` directories a sparse checkout of just
     `libunwind/` does not carry by default). `CRT_USE_IMPORTED_LIBCXX=ON`
     was claimed verified on macOS here, but that claim was wrong (or at
     best checked against a stale `install/lib/libc++.dylib` left over
     from before this same restructuring) -- `crt-libcxx-build` actually
     failed to *link* `libc++.dylib` at all on a genuinely fresh build
     (`libcxx` never got the same `__dso_handle` shim `libcxxabi`'s own
     recipe already had). A third real instance of this project's own "a
     local dev tree with an existing `out/` directory is not a reliable
     test of new CMake-level wiring" trap noted just above. Fixed and
     **genuinely re-verified** the same day, from a fully wiped `build`/
     `install`/staged-source tree (not an existing one): `crt-libcxx-
     build` exits 0, `nm` confirms `libc++.dylib` now carries its own
     defined `___dso_handle`, and both the static and shared
     `crt-libcxx-smoke` (`imported_libcxx_test`) builds link and run to
     completion -- real vector/string/RTTI/exception-throw-catch
     coverage, this time actually confirmed rather than just documented.
     See `HISTORY.md`'s later same-day entry for the full writeup.
   - Remaining C++ runtime gate, real and open, updated 2026-08-21 (steps 1-3
     of the ordered plan below are now done; only step 3's own leftover gap
     and steps 4-5 remain -- see `HISTORY.md`'s dated entry for the full
     session writeup, including six real toolchain bugs found and fixed
     along the way). Track B (debug backtraces via frame-pointer walking /
     sanitizer_common-style stack capture) stays deliberately out of this
     ordering -- it is read-only stack walking, never destructor/landing-pad
     dispatch, so it does not help C++ exceptions at all and can be picked
     up independently whenever it is useful.
     1. **DONE: `-fdwarf-exceptions`**, applied to both `CMAKE_CXX_FLAGS`
        and `CMAKE_C_FLAGS` (libunwind, unlike libcxx/libcxxabi, has several
        plain C source files that also needed it -- confirmed for real:
        `CMAKE_CXX_FLAGS` alone left `UnwindLevel1.c`/`UnwindLevel1-gcc-
        ext.c`/`Unwind-sjlj.c` still failing on the same `<windows.h>` via
        `unwind.h`'s own `__SEH__` guard even after `cxa_personality.cpp`/
        `Unwind-seh.cpp` were already fixed by the CXX-only flag). Confirmed
        working: `cxa_personality.cpp`, `Unwind-seh.cpp`, `UnwindLevel1.c`,
        `UnwindLevel1-gcc-ext.c`, `Unwind-sjlj.c`, `Unwind-EHABI.cpp` all
        compile clean now with zero `<windows.h>` errors.
     2. **DONE: real `_aligned_malloc`/`_aligned_free`** in this project's
        own libc (`libc/src/malloc.c`, declared in `include/stdlib.h` --
        not `include/malloc.h`, since libc++/libc++abi's own source never
        `#include <malloc.h>`, only `<cstdlib>`/`<new>`, matching where a
        real MSVC `<stdlib.h>` declares them directly). Thin wrappers over
        `posix_memalign()`, not `aligned_alloc()` (the latter's stricter
        C11 "size must be a multiple of alignment" contract does not match
        the looser real `_aligned_malloc()` one, and libc++/libc++abi's own
        `operator new(size, align_val_t)` callers do not guarantee that
        relationship). Confirmed working: `stdlib_new_delete.cpp`,
        `fallback_malloc.cpp`, libcxx's own `src/new.cpp` all compile clean.
     3. **DONE (build-system side), one real gap remains (source side):**
        libunwind is now built as a real recipe.json-driven target
        (`libstdc++/third_party/libunwind/`, `target_os: ["linux",
        "windows"]`) rather than an external CMake project fought at arm's
        length -- see steps below for the fixes this took. libcxxabi now
        formally depends on it (`"dependencies": ["libunwind"]`) and links
        its shared import library directly into `libc++abi.dll`'s own
        `CMAKE_SHARED_LINKER_FLAGS` (a real, empirically-required fix, not
        the "link-time-only in tools/crt-c++" design originally assumed --
        see `libstdc++/third_party/libcxxabi/recipe.json`'s own notes for
        why: unlike an ELF `.so`, a Windows DLL must resolve every
        referenced symbol at its own link time, confirmed via `_Unwind_*`/
        vtable-for-`std::logic_error` undefined-symbol errors the first
        time `LIBCXXABI_ENABLE_SHARED=ON` actually got exercised). Every
        libunwind source file now compiles clean **except one**: `libunwind.
        cpp`'s `findUnwindSections()`, via `AddressSpace.hpp`'s own real
        (not `__SEH__`-related) `#elif defined(_LIBUNWIND_SUPPORT_DWARF_
        UNWIND) && defined(_WIN32)` branch, which needs genuine PE/COFF
        module-enumeration APIs this project has never had a reason to
        declare before: `EnumProcessModules` (`psapi.h`) plus
        `IMAGE_DOS_HEADER`/`IMAGE_NT_HEADERS`/`IMAGE_FILE_HEADER`/
        `IMAGE_SECTION_HEADER`/`IMAGE_FIRST_SECTION` (real PE/COFF header
        layout, from `<winnt.h>`). This is genuine, scoped PAL work (one
        function, cleanly isolated) -- declare the minimal subset of these
        as a small project-owned header shim (matching how `libc/src/arch/
        windows/` already declares raw `__declspec(dllimport)` Win32
        function prototypes without `<windows.h>`), not a source patch to
        libunwind itself. Next concrete step when this is picked back up.
     4. **Get the rest of libcxxabi/libcxx building clean on Windows**
        (the psapi.h/PE-header shim above is the one thing standing between
        here and a fully clean Windows `crt-libcxx-build`), then repeat the
        same recipe.json-driven build on Linux (host-provided libunwind
        was considered and explicitly rejected, see `libstdc++/
        third_party/libunwind/recipe.json`'s own notes for why, so Linux
        needs the same from-source path). **Linux `crt-libcxx-build`
        itself: DONE and verified** (2026-08-21 HISTORY.md entry): two
        real header/macro gaps this project's Linux `<link.h>`/
        `<sys/syscall.h>` never needed before (`ElfW()` and
        `SYS_rt_sigprocmask`). **Linux `crt-libcxx-smoke`'s *static* leg:
        DONE and verified** (same entry, a chain of five further real
        gaps found and fixed in turn -- `tools/crt-c++` missing
        `crt1_init_array.o`, static-archive link-order needing
        `--start-group`/`--end-group`, real `__cxa_atexit()`/
        `__cxa_finalize()`/`__dso_handle` added to libc (Linux-only,
        `libc/src/arch/linux/common/cxa_atexit.c`), libcxxabi's own
        `config-ix.cmake` probes always false-reporting "found" because
        `crt-libcxx-build.py` configures with
        `CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY` (fixed by forcing
        `LIBCXXABI_HAS_CXA_THREAD_ATEXIT_IMPL=OFF` via
        `target_overrides.linux`), and a real, previously-unimplemented
        generic variadic `syscall(2)` (`libc/src/syscall_public.c` was a
        pure `ENOSYS` stub; now backed by a new
        `__crt_generic_syscall()` raw-syscall assembly trampoline per
        architecture). **Linux `crt-libcxx-smoke`'s *shared* leg: DONE
        and verified too**, same day: the remaining gap was
        `dl_iterate_phdr()` (`libdl/src/arch/linux/dl_linux.c`) reporting
        only the main executable's own phdrs, by original documented
        design (see `include/link.h`'s former comment, "this project has
        no real ELF dynamic linker yet") -- correct when nothing else
        needed more, but libunwind's exception search needs it to report
        *every* loaded shared object (the personality-routine call chain
        itself lives inside `libcxxabi.so`/`libunwind.so`, separate ELF
        images from the executable in the shared-linkage build), so a
        `throw` inside `main()` was never finding its own `catch` clause
        and calling `std::terminate()` instead. Fixed without a new ELF
        loader: this project already delegates actual `.so` loading to
        the real system dynamic linker
        (`-dynamic-linker /lib/ld-linux-aarch64.so.1`), which already
        maintains the standard SVR4 `struct r_debug`/`link_map`
        rendezvous list (via the executable's own `DT_DEBUG` `.dynamic`
        entry) -- real glibc's own `dl_iterate_phdr()` walks exactly that
        same structure internally, so `crt_dl_backend_iterate_phdr()` now
        does too (main executable still via AT_PHDR/AT_PHNUM as before,
        every other loaded object via the `_DYNAMIC`/`DT_DEBUG`/
        `r_debug`/`link_map` walk). **`imported_libcxx_test: ok` on both
        the static and shared legs** -- real vector/string/exception-
        throw-catch coverage, genuinely confirmed on Linux, both linkage
        modes, for the first time. See the 2026-08-21 HISTORY.md entry
        for the full chain (seven real gaps found and fixed in turn) and
        the `gdb`-verified traces.
     5. **Add a real cross-OS regression test**: `_Unwind_RaiseException`
        actually reaching a C++ `catch` block, verified identically on all
        three hosts once the build passes -- `crt-libcxx-smoke` already has
        the right shape (exception throw/catch is already one of its
        checks, now confirmed working on macOS and on Linux for both
        linkage modes) to extend rather than needing a new test from
        scratch. Windows remains the one host where this is still blocked
        on step 4's own open Windows gaps above.
     6. **Design a native-callback/boundary shim for Windows, not yet
        started.** Checked directly (2026-08-21, see `docs/cxx_runtime.md`'s
        "Known cost: DWARF-compiled code has zero Windows-native unwind
        info"): `-fdwarf-exceptions` builds emit no `.pdata`/`.xdata` at all
        for *any* non-leaf function, throwing or not -- confirmed by diffing
        `-fseh-exceptions` vs `-fdwarf-exceptions` object output for
        identical source. This is a real Windows x64 ABI-conformance gap
        (the OS assumes a function with no unwind-table entry is a leaf),
        not just a C++ `catch`-interop limitation, so it affects hardware
        exceptions propagating through a CRT/libc++ frame, WER/debugger/ETW
        stack walks that cross one, and CRT/libc++ code registered directly
        as a raw native OS callback (window proc, `CreateThread` entry
        point, vectored exception handler, COM vtable). Plain `LoadLibrary`
        plus calling a non-throwing export is unaffected. Design task for
        whenever this becomes a real requirement: a boundary shim compiled
        with real SEH (`-fseh-exceptions`, so it has genuine `.pdata`) at
        every point CRT/libc++ code is entered from or exits into native
        OS-driven control flow, so the OS always has at least one real
        unwindable frame between its own dispatch and DWARF-only code.
        Sharpens, does not replace, the existing "exceptions may cross the
        bridge... default answer being no" caveat in the Windows MSVC ABI
        Bridge Lane section of `docs/cxx_runtime.md`.
3. **Run the Wayland/Weston protocol/library investigation as a separate
   `libcrtgfx` sub-track.**
   Decide what is protocol parsing, what is compositor policy, and what is
   host-native window/GPU adapter code. Study Weston/wlroots/Wayland protocol
   sources before importing code; import protocol XML/generated helpers only
   when a tested boundary requires them.
4. **Add input/event delivery to the `libcrtgfx` surface contract.**
   Cover close, resize, focus, pointer, and keyboard shape across Linux
   Wayland, Win32, and Cocoa. Keep OS-native event details behind
   `src/arch/{linux,macos,windows}`.
5. **Extend Skia integration beyond primitive CPU drawing.**
   Add image/font/text staging after the CPU-raster surface smoke is stable.
   Treat HarfBuzz/FreeType/ICU/platform-font discovery as explicit follow-up
   dependencies, not hidden Skia side effects.
6. **Add GPU and media handoff only after the frame/input contract is stable.**
   Windows D3D, macOS Metal, Linux EGL/Vulkan/dmabuf, and `libcrtmedia`
   decoded-frame/audio handoff are later optimization/integration tranches,
   not prerequisites for the first Skia raster milestone.

## Planned

### Interactive job control (deferred until it's an actual priority)

`docs/job_control.md`'s "Interactive Job Control" section has the decided
design for all three pieces below; nothing here is implemented yet, and this
project's own mksh build has job control compiled out entirely on every host
(`MKSH_NOPROSPECTOFWORK`), not just Windows -- see that section for why this
is forward-looking policy, not a current gap being actively worked.
Re-evaluated (2026-08-16) against `docs/runtime_roadmap.md`: none of the
planned upper-runtime components (`libcrtjs`/QuickJS+V8, `libcrtgfx`, `libcrtmedia`)
actually depend on POSIX job-control signals (`SIGSTOP`/`SIGTSTP`/`SIGCONT`)
or real fg/bg switching -- confirmed genuinely optional infrastructure, not
something blocking the roadmap. (V8's own "signal/process behavior"
prerequisite in that doc is a separate matter -- `SIGSEGV`-trap-based WASM
bounds checks and `SIGPROF`-style profiling, the "vectored exception
handling" question `docs/signal_delivery.md` already tracks independently,
answerable with fully documented Windows APIs.) A full Windows stop/resume
implementation would also need reversing this project's "avoid undocumented
NT internals" pattern (`NtSuspendProcess`/`NtResumeProcess` -- see
`docs/job_control.md`'s own "Stopped-child status" note for the design that
was investigated and the alternatives ruled out). Stays deferred.

- Bridge `SetConsoleCtrlHandler` (`CTRL_C_EVENT`/`CTRL_BREAK_EVENT`, both to
  `SIGINT`) into `signal_actions[]`/`raise()`, mirroring `SIGCHLD`'s existing
  pending-flag-plus-checkpoint pattern (`docs/signal_delivery.md`).
- Track the real Windows process-group id behind this project's own
  CRT-managed `pgid` integer once a job is actually spawned into a new
  process group, so `tcsetpgrp()` and a targeted `CTRL_BREAK_EVENT` have a
  real id to act on.
- Re-enable `MKSH_UNEMPLOYED` (mksh's own job control) once the above exists,
  and only then decide whether stopped-child (`WIFSTOPPED`) support is worth
  the low-level Windows work it would need -- `docs/job_control.md` currently
  keeps that explicitly out of scope.

### Toybox applet expansion (deferred until it's an actual priority)

Only when the backing Bionic-compatible CRT/PAL surface exists.
Full applet-by-applet status (what's enabled,
what's still open and why, the deferred-applet list with each one's
concrete reason, and the `globals.h`/`flags.h` registration traps found
while enabling `df`/`stty`) now lives in
[`docs/toybox_applet_status.md`](docs/toybox_applet_status.md) -- this
bullet stays a pointer. Still open there: `expand`/`logger`/`fold`/
`uudecode`/`cal`/`split`/`strings` (a `globals.h` fix, plus a per-applet
`flags.h` check); `timeout` (hang fixed, two deeper gaps remain: real
`SIGCHLD` `siginfo_t` data, cross-process `kill()`); and a confirmed-not-
guessed deferred list (`ps`/`top`/`iotop`/`pgrep`/`pkill`, `mount`/
`umount`, `ifconfig`, `login`, procfs-heavy commands).
