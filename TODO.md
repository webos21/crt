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
   - **Fetch pinned + sparse-checked-out, and `crtgfx-skia-fetch`/
     `-configure`/`-build` verified real end-to-end on Windows (2026-08-21).**
     `CRTGFX_SKIA_REF`/`CRTGFX_SKIA_EXPECTED_COMMIT` now default to a real
     commit SHA (`13ffba253fc7854fd3b34f67c82dfb2418dc2944`, captured via
     `git ls-remote` the same day) instead of the previous empty/floating
     `refs/heads/chrome/m148` -- the same reproducibility fix already applied
     to `libstdc++/third_party/*/recipe.json`'s own `source.ref`, see those
     recipes' notes and `HISTORY.md`'s dated entry for the fuller
     reproducibility-gap reasoning. `CRTGFX_SKIA_SPARSE_PATHS` (new) trims the
     fetch the same way (cone-mode sparse-checkout, derived empirically via
     `ninja -t inputs skia` against a real build, not guessed) from 260MB+ to
     ~100MB. `CRTGFX_SKIA_SYNC_DEPS` now defaults OFF: confirmed for real
     that `git-sync-deps` unconditionally downloads Skia's entire third-party
     dependency set regardless of GN feature flags (8.6GB, including a full
     Emscripten/WASM toolchain, before being killed), and separately
     confirmed unnecessary for this project's own CPU-raster-only GN
     config once `skia_use_wuffs` (GIF decode, the one codec flag left at
     Skia's own default `true` while every sibling codec was already off) is
     also disabled -- `ninja -t inputs skia` then shows zero
     `third_party/externals/` references at all. `tools/build_skia.py` also
     gained two real Windows fixes, both confirmed necessary: auto-
     bootstrapping a pinned `gn` binary via Skia's own `bin/fetch-gn` when
     `bin/gn.exe` is not already present (the previous bare-`gn`-no-`.exe`
     check never matched on Windows), and a throwaway `python3.bat` PATH
     shim (GN's own `.gn` dotfile hardcodes `script_executable = "python3"`,
     which a stock Windows Python install does not provide by that name).
     Verified via a genuinely fresh fetch + `gn gen` + full `ninja` build
     through the real `crtgfx-skia-fetch`/`crtgfx-skia-build` CMake targets
     (not a scratch script): a real `libskia.a` (21MB) was produced. A real
     mistake was made and caught mid-implementation while doing this: the
     fetch's own `--depth 1` was dropped from the initial partial clone by
     mistake (an interactive test that validated the command sequence had
     actually kept it), producing a 189MB `.git` (464,512 packed objects)
     for one real `crtgfx-skia-fetch` run before being caught and fixed back
     to ~22MB. See `HISTORY.md`'s dated entry for the full trail.
   - **New, separate, pre-existing gap found while verifying the full link**
     (2026-08-21, NOT caused by the pinning/sparse-checkout work above --
     confirmed by inspection that nothing touched here changed
     `crt_cxx_build_flags`, `detect_cxx_standard_include_dirs()`, or either
     target's own `target_link_libraries()`): with `CRTGFX_ENABLE_SKIA=ON`,
     `crtgfx_skia_raster_smoke.exe` fails to *link* on Windows with a long
     list of `lld-link: error: duplicate symbol` (`printf`, `fprintf`,
     `snprintf`, `fabsf`, `fabsl`, `frexpl`, `wmemcpy`, `wmemset`,
     `wmemcmp`, ...) between this project's own `c.lib`/`m.lib` and objects
     that carry their own copies of the same symbols (`skia_raster_smoke.
     cc.obj` itself, several `libskia.a` members, and MSVC's own
     `libcpmt.lib`). Root cause, diagnosed but not yet fixed: the top-level
     `CMakeLists.txt`'s own `crt_cxx_build_flags` deliberately omits
     `-nostdinc++` on Windows only (`$<NOT:$<PLATFORM_ID:Windows>>>` guard,
     by design -- this project's own Windows C++ bootstrap library, `cxx`/
     `cxx_shared`, relies on real MSVC STL headers, unlike Linux/macOS)
     -- so any Windows CMake-native C++ translation unit that reaches a
     real C stdio/math header (directly or, as here, transitively through
     Skia's own headers) gets MSVC UCRT's own inline-materialized
     `printf`/`fprintf`/`snprintf`/`fabsf`/... as real, externally-visible
     symbols in that same object file, which then collides with this
     project's own freestanding `c.lib`/`m.lib` definitions of the exact
     same names once both get linked into one final executable. This
     appears to be the first target that ever links this project's own
     `c`/`cxx` bootstrap libraries together with something (Skia's own
     headers) that also pulls in real MSVC UCRT stdio/math headers on
     Windows -- a genuinely separate, deeper Windows-C++-runtime-
     architecture question from "did the Skia fetch/build itself work"
     (which is now fully verified, see above), deliberately left open
     rather than rushed in the same pass. `CRTGFX_ENABLE_SKIA` stays OFF by
     default (matching its pre-existing default; the default `ctest` suite
     is unaffected either way since this target is only built when that
     option is explicitly turned on).
   - **Skia's source pin/sparse-checkout now lives in a real `recipe.json`**
     **(2026-08-22)**: `libcrtgfx/third_party/skia/recipe.json`, matching
     `libstdc++/third_party/{libcxx,libcxxabi,libunwind}/recipe.json`'s
     shape (`source.type`/`repository`/`ref`/`expected_commit`/
     `sparse_paths`/`sync_deps`, plus a `patches` array for the `.gn`
     `script_executable` build-config edit and a `notes` array for the
     pinning history), adapted for Skia's GN/Ninja build (no
     `cmake.options` section -- that lives in `tools/build_skia.py`'s
     own `default_gn_args()`, unchanged). `libcrtgfx/CMakeLists.txt`
     reads it via `string(JSON ...)` at configure time instead of
     hardcoding the same six values inline; verified via a genuinely
     fresh configure producing matching cache values, plus full default
     `ctest` still 120/120. A matching placeholder `recipe.json` was
     also added for Wayland (`libcrtgfx/third_party/wayland/
     recipe.json`, `source.ref` left `null` -- there is still nothing to
     fetch, per the existing "intentionally not a checkout" status) so
     the same recording location exists the moment that changes. See
     `HISTORY.md`'s dated entry for the fuller writeup.
   - **Follow-up investigation (2026-08-22): routed Skia's own GN build**
     **through the project-owned imported libc++ instead of real MSVC STL**
     (the architecturally-correct fix for the gap just above), and found +
     fixed eight distinct real bugs getting there, in order: (1) sparse-
     checkout's `--sync-deps` needing `tools/` too (already fixed, see
     above); (2) GN's Windows `msvc_toolchain` template hardcodes
     `cl.exe`/`clang-cl.exe` and ignores the top-level `cc`/`cxx` GN args
     entirely -- fixed the same way this project's own macOS build already
     does (`tools/build_skia.py`'s `default_gn_args()`), by setting GN's
     `target_os = "linux"` even on a real Windows build, which selects the
     generic `gcc_like_toolchain` (which does respect `cc`/`cxx`) and
     Skia's own generic POSIX source set; (3) `crt-cc.cmd`/`crt-c++.cmd`
     (needed because GN's `gcc_like_toolchain` invokes `cc`/`cxx` via
     `subprocess.check_output(shell=True)` and a bare `{{cc}}` ninja
     substitution, neither of which can launch `mksh.exe` as a separate
     leading argument) plus `CRT_HOST_CC`/`CRT_HOST_CXX` resolved via
     `shutil.which()`; (4) **a genuinely new PAL bug**: `libc/src/env.c`'s
     `__crt_rootfs_bootstrap()` runs at every CRT-libc process's own
     startup and, whenever `CRT_ROOTFS` is not already set, auto-detects
     it from `argv[0]` (`mksh.exe` lives under `.../rootfs/system/bin/`)
     and then unconditionally `chdir("/")`s -- discarding whatever real
     cwd ninja launched it with, so every compile failed with "no such
     file or directory" on its own GN-relative source path (e.g.
     `../../modules/skcms/src/skcms_TransformHsw.cc`). Fixed by having
     `tools/build_skia.py` pre-seed `CRT_ROOTFS` itself before `mksh.exe`
     ever starts (its own `__crt_rootfs_bootstrap()` returns immediately
     once `CRT_ROOTFS` is already set), matching what
     `tools/crt-libcxx-build.py` already did for the same reason; (5)
     mksh's own `exec()`/command-lookup cannot run a program whose path
     contains a space, forward slashes or not (a stock Windows LLVM
     install always lands under `"C:\Program Files\LLVM\..."`) -- fixed
     by converting `CRT_HOST_CC`/`CRT_HOST_CXX` to their 8.3 short-path
     form (`windows_short_path()`, duplicated from
     `tools/crt-port-build.py`'s own helper of the same name); (6) a real
     PATH-format conflict between `gn.exe` (a genuinely native tool that
     needs a real, semicolon-delimited Windows `PATH` to find `python3`,
     per Skia's own `.gn` dotfile) and `mksh.exe` (whose `MKSH_PATHSEPC`
     stays `:` on this project's Windows build by deliberate design, see
     `shell/toybox/PATCHES.md`'s own `MKSH_CRT_WINPATH` writeup -- a real,
     backslash-form Windows directory cannot appear in mksh's PATH at all,
     since mksh reads the drive letter's own `:` as a second separator)
     -- both tools are invoked from the *same* `gn gen` subprocess tree
     (GN's own `is_clang.py` compiler probe shells out through
     `crt-cc.cmd` mid-`gn gen`), so no single PATH value could satisfy
     both. Fixed by patching the fetched `.gn` dotfile's
     `script_executable = "python3"` line to an absolute path
     (`pin_gn_script_executable()`, replacing the previous PATH-shim
     approach) so `gn.exe` no longer needs PATH for this at all, freeing
     `PATH` to stay pure POSIX (`/system/bin:/bin:/usr/bin`) for `mksh.exe`.
     Also fixed two more, unrelated bugs found along the way: (7)
     `--target-arch` arrives from CMake in `CMAKE_SYSTEM_PROCESSOR`
     spelling (`AMD64`/`ARM64`), which `tools/crt-cc`'s own
     `--target=${target_arch}-w64-mingw32` construction needs in GNU-
     triple spelling (`x86_64`/`aarch64`) -- fixed via a new
     `normalize_target_arch()` (mirrors `tools/crt-libcxx-build.py`'s own
     `detect_target_arch()`); (8) clang's own `--target=x86_64-w64-
     mingw32` predefines `_WIN32`/`_WIN64`, which Skia's `SkFeatures.h`
     reads as `SK_BUILD_FOR_WIN` regardless of what GN's `target_os` GN
     arg says -- and `SK_ALWAYS_INLINE` (`SkAttributes.h`) expands to the
     bare MSVC keyword `__forceinline` under `SK_BUILD_FOR_WIN`, which
     this project's mingw-target (non-`clang-cl`) invocation does not
     recognize at all (`error: unknown type name '__forceinline'`).
     Fixed by explicitly defining `-DSK_BUILD_FOR_UNIX` for the Windows
     branch too (`SkFeatures.h`'s own auto-detection is a single `#if
     !defined(SK_BUILD_FOR_*)` guard around the whole block, so defining
     any one of them up front skips the rest) -- internally consistent
     either way, since GN's `target_os = "linux"` already selects Skia's
     generic POSIX *source files* (e.g. `SkOSFile_posix.cpp`, not the
     real Win32 `SkOSFile_win.cpp`); this exact override already existed
     for macOS (which hits the identical problem via `__APPLE__`), just
     hadn't been extended to the Windows branch yet. With all eight
     fixed, Skia's own GN build got real compilation underway (34/544
     ninja steps reached, real `.o` files produced) before hitting a
     *different* class of problem,
     genuine library-completeness gaps rather than toolchain wiring:
     Skia's `SkMathPriv.h` needs C++20 `<bit>` (`std::countl_zero`/
     `countr_zero`/`popcount`), but the pinned libc++ commit predates
     that header's C++20 support entirely (its own `<bit>` has only the
     pre-existing internal `__popcount` helpers, no `_LIBCPP_STD_VER`
     gating or public entry points at all); separately, this project's
     own libc `<inttypes.h>` did not declare `imaxdiv_t`/`imaxabs`/
     `imaxdiv`/`wcstoimax`/`wcstoumax`, needed by the imported libc++'s
     own `<cinttypes>` wrapper. The eight toolchain-wiring fixes above
     are real and independently useful regardless of either gap
     (verified: full default `ctest` suite still 120/120 with
     `CRTGFX_ENABLE_SKIA` left OFF), so they were kept and committed
     even though the link itself was still blocked at the time.
   - **`<inttypes.h>` gap fixed for real (2026-08-22)**: added
     `imaxdiv_t`/`imaxabs()`/`imaxdiv()` (implemented directly in terms
     of `intmax_t`'s own truncating division/modulo, not delegated to
     `ldiv()`/`lldiv()`, since `intmax_t` is `long` on Linux/macOS but
     `long long` on this project's Windows target -- confirmed via a
     real compiler probe on both) and `wcstoimax()`/`wcstoumax()`
     (thin wrappers over the already-existing `wcstoll()`/`wcstoull()`,
     matching how `strtoimax()`/`strtoumax()` already wrapped
     `strtoll()`/`strtoull()`) to `include/inttypes.h`/
     `libc/src/inttypes.c`. This was a real, deliberate choice between
     three options the user asked to have spelled out first: bump the
     libc++ pin, patch a header in, or fix libc directly -- the
     `<cinttypes>` gap turned out to be entirely independent of the
     libc++ pin question (it is this project's *own* libc, not
     anything libcxx ships), so it was fixed directly rather than
     patched around, matching this project's own porting-loop
     discipline (`AGENTS.md`). Verified via a second, real Linux build
     attempt (WSL/Ubuntu 20.04, see the entry right below) that this
     was the *only* other blocker Windows and Linux both hit: after
     this fix, the Skia GN build's failure surface shrank to exactly
     one object file (`SkMathPriv.o`), and every remaining error traces
     to the still-open `<bit>` gap alone -- confirmed via direct log
     inspection, not assumed. Zero regression: full default `ctest`
     120/120 on both Windows and Linux/WSL.
   - **The `<bit>` gap itself fixed for real on Linux, via a source
     migration (2026-08-22)**: `libstdc++/third_party/{libcxx,libcxxabi}/
     recipe.json`'s own `source.repository` (the dead Android fork
     `platform/external/libcxx{,abi}`, frozen since a 2024-12-20 placeholder
     "Empty merge" commit -- confirmed via that repo's own `refs/heads/main`
     tip) were migrated to `toolchain/llvm-project`'s own `libcxx`/`libcxxabi`
     subtrees at the same pinned commit `../libunwind/recipe.json` already
     used (`37f38d1f3276b62fba09462ab4807dce846c732d`) -- confirmed that
     commit's own `libcxx/include/bit` has real `_LIBCPP_STD_VER >= 20`
     gating and a real `countl_zero`/`countr_zero`/`popcount`, unlike the
     dead fork's 158-line version. This discarded the old fork-specific
     patches (written against a file layout the new source no longer has)
     and required rediscovering their equivalents one build error at a
     time: CMake driver/target-ordering fixes (`cxx-headers` needing a
     combined `add_subdirectory()`, `LIBCXX_CXX_ABI=system-libcxxabi`,
     `cmake --install --component` scoping), five bounded `libc/` subtree
     fetches (`shared`, `src/__support`, `hdr`, `include`, `src/errno`) for
     llvm-libc utility headers libcxx's own `charconv.cpp` now depends on,
     two real general libc completeness gaps fixed in this project's own
     code (`include/linux/futex.h` + `SYS_futex` for `atomic.cpp`'s futex
     wait/wake, `O_NOFOLLOW` in `include/fcntl.h` for `filesystem/
     operations.cpp`), and the `__dso_handle`/`generate-cxx-headers`/
     `find_package(Python3)` wiring both recipes' own drivers needed.
     **Linux/WSL: fully verified** -- full default `cmake --build` +
     `ctest` with `CRT_USE_IMPORTED_LIBCXX=ON`, **100% tests passed, 0
     failed, out of 104**, on a fully wiped build tree (WSL/Ubuntu-26.04).
     **macOS: this migration left it broken (never verified here at the
     time), fixed same-day once reported.** `crt-libcxx-build` failed
     with `fatal error: 'mach-o/dyld.h' file not found` (libcxx's own
     `src/include/refstring.h`, a real-host-libstdc++-interop feature
     gated by a macro pair Clang's driver predefines from the real
     target triple, unaffected by this recipe's `-U__APPLE__` flag --
     disabled the whole feature via a new `libcxx/recipe.json` patch,
     this project never coexists with a host libstdc++ in the same
     process anyway); separately, `crt-libcxx-smoke` failed with
     `unrecognized arguments: --host-cc ... --host-cxx ...`
     (`tools/test_libcxx_runtime.py`'s own argparse was never updated
     for the same-day `--host-cc`/`--host-cxx` addition to the shared
     `CRT_LIBCXX_PLATFORM_ARGUMENTS` list `crt-libcxx-smoke`'s custom
     target also consumes). Both fixed and re-verified genuinely fresh:
     `crt-libcxx-smoke` now passes both linkage legs on macOS, matching
     Linux. See `HISTORY.md`'s later same-day entry.
     **Windows: real progress, intentionally left incomplete.** Six more
     distinct, real bugs were found and fixed the same way (evidence-based,
     one build error at a time, each with its own recipe.json/`win32_shim`
     note): a `_tls_index` link failure (libc++abi's `cxa_exception_storage.
     cpp` unconditionally takes Clang's native-`thread_local` branch, which
     needs a real CRT's TLS directory this project's own `crt1.o` never
     provides -- patched to fall through to the already-working
     pthread-key-based branch on this target); `_LIBCPP_MSVCRT_LIKE` being
     defined for any `_WIN32` target regardless of which CRT actually backs
     it (wrongly routing locale support at `_locale_t`/`_create_locale`
     instead of this project's own already-correct POSIX-shaped `newlocale`/
     `freelocale`); and four rounds of missing declarations resolved by
     extending `libstdc++/third_party/win32_shim/` (already-established
     precedent from libunwind's own Windows needs) with real, verified
     Win32 surface -- `AreFileApisANSI`/`WideCharToMultiByte`/
     `MultiByteToWideChar`/`CP_ACP`/`CP_OEMCP` for `filesystem/path.cpp`,
     `LARGE_INTEGER`'s `LowPart`/`HighPart` for `filesystem/time_utils.h`,
     and a new `win32_shim/winerror.h` with the 49 real `ERROR_*` codes
     `system_error.cpp` needs (values read directly from this machine's own
     real SDK `winerror.h`, not assumed), plus redirecting `<print>`'s own
     terminal-detection to the portable `isatty()` path (this project has
     no MSVC-CRT `_get_osfhandle()`/fd model to feed `GetConsoleMode`).
     The build then reached a **new, deeper blocker**: `libcxx/src/
     CMakeLists.txt` unconditionally compiles `support/win32/{locale_win32,
     support}.cpp` (and `fstream.cpp`/`random.cpp` reach similar territory)
     for any Windows target, and these call real MSVC-CRT-only entry points
     (`_create_locale`/`_free_locale`, `wcrtomb_s`, `errno_t`, `rand_s`,
     `_wfopen`) this project's own libc has no equivalent of at all --
     fixing this properly needs a CMake-level patch dropping/replacing
     those files, not another header shim, a distinctly bigger scope than
     the fixes above. **Deliberately stopped here** (explicit user decision
     when asked, given Linux's already-complete verification covers the
     actual motivating `<bit>` gap): the six Windows fixes above are real,
     permanent, and kept (each is dormant/inert under the default
     `CRT_USE_IMPORTED_LIBCXX=OFF`, confirmed via a full default-config
     Windows regression run, 120/120 `ctest` passing, zero impact), but
     `CRT_USE_IMPORTED_LIBCXX=ON` does not yet build clean on Windows.
     Remaining Windows work, next time this is picked up: patch
     `libcxx/src/CMakeLists.txt` (via `libstdc++/third_party/libcxx/
     recipe.json`'s own `patches`) to drop `support/win32/locale_win32.cpp`/
     `support/win32/support.cpp` from `LIBCXX_SOURCES` when targeting this
     project's own libc, then work through whatever `fstream.cpp`/
     `random.cpp` still need (`_wfopen`, `rand_s`) the same evidence-based
     way. **Resolved 2026-08-23:** the recipe now omits those UCRT-only
     sources, routes random-device through the CRT `/dev/urandom` path, and
     supplies the bounded filesystem/kernel32 shim and fd adapters required
     by the remaining libc++ sources. `crt-libcxx-build`, staging, and both
     smoke linkage modes now pass on Windows; see `HISTORY.md`'s 2026-08-23
     entry. A dedicated Windows `<filesystem>` behavior test (especially
     UTF-32 `wchar_t` to native UTF-16 path conversion) remains worthwhile
     before claiming that API family's runtime semantics are fully covered.
   - **A real WSL/Ubuntu-20.04 attempt confirmed the "Linux would reach**
     **the same wall faster" projection above, and surfaced one genuinely**
     **new, environment-specific finding along the way (2026-08-22).**
     Skia's GN build reached real compilation with *zero* toolchain-
     wiring fixes needed on Linux (26-31/544 steps, vs. needing all
     eight Windows-specific fixes above just to get compilation
     started) and landed on exactly the same two gaps predicted --
     confirming the projection was correct, not just plausible. Along
     the way: the WSL distro's own default git (2.25.1, Ubuntu 20.04's
     stock `apt` version) made `tools/crt-libcxx-build.py`'s partial-
     clone fetch of `libunwind` (sparse-checked-out from the full
     `toolchain/llvm-project` monorepo) balloon to 3.6GB+ and eventually
     fail with `HTTP 502`, instead of the "tens of MB" that same fetch
     already reliably produces on Windows (`git 2.55.0`) -- an old
     git's partial-clone (`--filter=blob:none`) negotiation against a
     JGit/Gerrit backend is real evidence of being far less efficient
     than a modern one for this exact kind of fetch, not a bug in this
     project's own fetch logic. Fixed by upgrading WSL's git via the
     official `ppa:git-core/ppa` to 2.50.1, after which the identical
     fetch completed in seconds at the expected size. Worth checking
     for on any *other* older-Linux-distro host this project's own
     recipes get run against for the first time -- an unexpectedly slow
     or oversized partial-clone fetch is a git-version question first,
     before assuming a recipe-level bug.
   - The first exposed C++ gap, CRT-owned `operator new/delete`, is complete
     and covered by `cxx_allocation_test`. The remaining gate is a real
     project-owned libc++ standard-library import: the default Skia archive
     uses `std::string`, shared ownership, streams, and locale even with GPU
     disabled. `CRTGFX_ENABLE_SKIA` therefore remains OFF by default; do not
     link host libc++ as a substitute. After libc++ is imported, verify the
     deterministic `crtgfx_skia_raster_smoke` on static and shared paths on all
     three hosts (Windows fetch/build itself is now verified -- see above --
     but the final link needs the new gap just above resolved first).
   - Cross-host build-driver status: the Linux route selects the POSIX
     `tools/crt-ar` wrapper; the Windows route selects `tools/crt-ar.cmd`,
     passes the invoking Python through `CRT_HOST_PYTHON`, and recognizes the
     MSVC STL include root. Those routes were statically checked on macOS, but
     their real host GN/Ninja workflows still require execution before Skia is
     reported as cross-host verified.
   - The Windows-vs-Linux-toolchain-wiring projection that used to be
     recorded in this bullet was confirmed for real via an actual WSL
     attempt -- see the dated bullet above ("A real WSL/Ubuntu-20.04
     attempt confirmed...") for the verified result, and `HISTORY.md`'s
     matching entry for the full writeup. No longer a projection.
   - **C++ runtime prerequisite (runtime smoke complete on all three hosts, both linkage modes):** each of
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
   - Remaining C++ runtime gate, real and open, updated 2026-08-21 (steps
     1-6 are now done -- libunwind, libcxxabi, and libcxx all build clean
     on Windows, redirected to their own Android/Bionic locale/random
     backends instead of MSVC's, and a real client program compiles+
     links+runs end to end on both linkage modes: `imported_libcxx_test:
     ok` for both `imported_libcxx_test_static.exe` and `imported_libcxx_
     test.exe`, matching macOS and Linux. Step 7 remains -- see
     `HISTORY.md`'s dated entries for the full session writeup, including
     the many toolchain bugs found and fixed along the way).
     Track B (debug backtraces via frame-pointer walking /
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
     3. **DONE: libunwind and libcxxabi both build clean on Windows**,
        static and shared. libunwind is now built as a real
        recipe.json-driven target (`libstdc++/third_party/libunwind/`,
        `target_os: ["linux", "windows"]`) rather than an external CMake
        project fought at arm's length -- see steps below for the fixes
        this took. libcxxabi now formally depends on it (`"dependencies":
        ["libunwind"]`) and links its shared import library directly into
        `libc++abi.dll`'s own `CMAKE_SHARED_LINKER_FLAGS` (a real,
        empirically-required fix, not the "link-time-only in
        tools/crt-c++" design originally assumed -- see
        `libstdc++/third_party/libcxxabi/recipe.json`'s own notes for why:
        unlike an ELF `.so`, a Windows DLL must resolve every referenced
        symbol at its own link time, confirmed via `_Unwind_*`/vtable-for-
        `std::logic_error` undefined-symbol errors the first time
        `LIBCXXABI_ENABLE_SHARED=ON` actually got exercised). Closing the
        one remaining libunwind gap (`findUnwindSections()`'s real,
        non-`__SEH__`-related need for `<windows.h>`/`<psapi.h>` PE/COFF
        module enumeration) took a genuinely new project-owned header shim
        directory, `libstdc++/third_party/win32_shim/` (added to the
        include path for windows recipe builds in
        `tools/crt-libcxx-build.py`'s `common_cmake_args()`), plus two
        more real gaps found in turn while getting it fully building and
        linking:
        - `libunwind.cpp`'s `AddressSpace.hpp::findUnwindSections()` and
          `RWMutex.hpp`'s internal locking needed `EnumProcessModules`
          (redirected to the real kernel32 export `K32EnumProcessModules`,
          verified via `llvm-objdump -p` against this machine's real
          kernel32.dll/psapi.dll -- matches how real Windows SDK `psapi.h`
          itself does this on Vista+, avoiding a new psapi.lib
          dependency), `GetCurrentProcess`/`GetLastError`, real PE/COFF
          `IMAGE_DOS_HEADER`/`IMAGE_NT_HEADERS`/`IMAGE_FILE_HEADER`/
          `IMAGE_SECTION_HEADER`/`IMAGE_FIRST_SECTION` (Microsoft's own
          published PE/COFF spec, not an internal format), and `SRWLOCK`
          plus its four Acquire/Release functions (also real kernel32
          exports, verified the same way). `UnwindCursor.hpp` also
          unconditionally `#include`s `<ntverp.h>` under `#ifdef _WIN32`
          even though the one macro it defines is dead code under this
          project's DWARF-exceptions build (only read inside an
          `_LIBUNWIND_SUPPORT_SEH_UNWIND` block) -- shimmed with the real
          value (10011) verified against this machine's actual installed
          Windows 10 SDK, for accuracy in case a future SEH-enabled path
          ever reads it.
        - The first shim draft caused a *different* failure once headers
          resolved: `ld.lld: error: undefined symbol: __declspec(dllimport)
          K32EnumProcessModules(void*, void**, unsigned long, unsigned
          long*)` -- the parenthesized signature was the tell: these
          declarations lacked `extern "C"`, so when included from
          `libunwind.cpp` (a C++ TU), they got C++-mangled linkage that
          could never match kernel32.lib's real plain-C export names
          (confirmed present and correctly named via `llvm-nm`). Fixed by
          wrapping both shim headers' declarations in
          `#ifdef __cplusplus extern "C" { ... } #endif`.
        - `cmake/config-ix.cmake`'s `check_library_exists(pthread
          pthread_once ...)` is a false positive on Windows, the same
          `CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY`-means-check_library
          _exists-never-actually-links root cause already diagnosed for
          `LIBCXXABI_HAS_CXA_THREAD_ATEXIT_IMPL` on Linux (see below) --
          confirmed for real via `lld: error: unable to find library
          -lpthread` (this project deliberately has no separate pthread
          library at all, matching modern Bionic). Fixed with
          `LIBUNWIND_HAS_PTHREAD_LIB=OFF` via `target_overrides.windows`.
        - libcxxabi's own `stdlib_stdexcept.cpp`/`stdlib_typeinfo.cpp`
          define `std::runtime_error`/`std::bad_cast`/`std::bad_typeid`'s
          vtables, but never `#define _LIBCPP_BUILDING_LIBRARY` the way
          three sibling files in the same directory already do (only
          libcxxabi's own, *different* `_LIBCXXABI_BUILDING_LIBRARY` macro
          gets set project-wide, via `add_definitions` in libcxxabi's top-
          level `CMakeLists.txt`) -- so libcxx's own `<stdexcept>`/
          `<typeinfo>` headers see these vtables declared `dllimport`
          instead of `dllexport`, and `cxxabi_shared/libc++abi.dll`'s link
          fails with `undefined symbol: vtable for std::runtime_error`
          and two siblings. Real upstream LLVM never hits this because its
          blessed build configures libcxx and libcxxabi together in one
          CMake project graph sharing this define; this project's three-
          separate-recipe build does not. Fixed with a one-line
          `add_definitions(-D_LIBCPP_BUILDING_LIBRARY)` patch right after
          libcxxabi's own existing `add_definitions` call.
     4. **DONE: libcxx redirected to its own Android/Bionic locale/random
        backends instead of the MSVC one; static libc++ now works
        end-to-end on Windows, real exceptions included** (`imported_
        libcxx_test: ok`, matching macOS's and Linux's own passing
        marker). Getting here took two real batches of fixes, in order:
        first getting `crt-libcxx-build` itself fully green (closing the
        gap the previous entry above left open), then getting a real
        client program to actually compile+link+run against the result
        (`crt-libcxx-smoke`, never reached before on Windows).
        Batch 1 -- `crt-libcxx-build` itself:
        - `cxx_filesystem` (libc++'s `<filesystem>` static archive)
          doesn't exist as a CMake target on Windows at all -- libcxx's
          own top-level `CMakeLists.txt` sets `ENABLE_FILESYSTEM_DEFAULT
          OFF` specifically `if (WIN32)` (ON everywhere else). Fixed by
          adding proper `target_overrides.<os>.build_targets` support to
          `tools/crt-libcxx-build.py` (mirrors the existing `options`
          override) and dropping `cxx_filesystem` from the Windows list
          only; deliberately not overriding `LIBCXX_ENABLE_FILESYSTEM`
          back to `ON` (nothing in this project needs `<filesystem>` yet,
          no found reason to second-guess upstream's own default).
        - The MSVC-locale/random gap this section previously called a
          "materially bigger, deeper change than the shims above" turned
          out smaller than expected once actually attempted: `locale_
          bionic.h`'s own guard is `__BIONIC__` (which this project's
          recipe build already defines unconditionally), not
          `__ANDROID__` -- its `__ANDROID__`-specific NDK-fallback code
          never fires for us, leaving only `<stdlib.h>`/`<xlocale.h>`,
          both already provided by this project's own sysroot
          (`libc/src/locale.c`, `include/xlocale.h` -- confirmed to
          already cover nearly every `_l`-suffixed function libcxx calls;
          `islower_l`/`isupper_l`/`iswalnum_l`/`iswgraph_l` were already
          *implemented* in `libc/src/locale_l.c` but missing from the
          public header, a pre-existing oversight fixed along the way).
          Six independent `_LIBCPP_MSVCRT_LIKE`-family branch points
          needed a matching `__BIONIC__`-scoped patch (all in `libstdc++/
          third_party/libcxx/recipe.json`'s `patches`, all deliberately
          scoped to keep macOS/Linux's own already-verified behavior
          untouched -- `__BIONIC__` is defined project-wide, not just on
          Windows): `include/__locale`'s top-level backend `#include`
          selection, its separate `__libcpp_locale_guard` struct
          selection (keyed off a *different* macro, `_LIBCPP_LOCALE__
          L_EXTENSIONS`, deliberately left alone rather than flipped --
          see the recipe's own notes for why), its separate `ctype_base::
          mask` bitmask selection; `include/__config`'s `_LIBCPP_USING_
          WIN32_RANDOM` selection (`random_device`'s `rand_s()` path --
          this project's own `/dev/urandom` emulation, already
          implemented for Windows, needs nothing new); `src/locale.cpp`'s
          own *separate* backend `#include` (keyed off `_LIBCPP_MSVCRT ||
          __MINGW32__`, not `_LIBCPP_MSVCRT_LIKE` -- missed by grepping
          for that macro alone, since `__MINGW32__` is unconditionally
          defined by Clang for this target regardless); `src/system_
          error.cpp`'s `strerror_s()` vs. portable `strerror_r()`
          selection. One more, unrelated CMake-level fix: `lib/
          CMakeLists.txt`'s own `if(WIN32) file(GLOB
          LIBCXX_WIN32_SOURCES ../src/support/win32/*.cpp) ... endif()`
          unconditionally compiles the now-entirely-dead MSVC support
          files (`support.cpp`/`thread_win32.cpp`/`locale_win32.cpp` --
          real MSVC UCRT symbols like `errno_t`/`_create_locale`/
          `wcrtomb_s`/`process.h` none of the source patches above touch
          at all) regardless of what any header selects; no-op'd the same
          way this recipe already no-ops upstream's `if(MINGW)` blocks.
        - `libc++.dll`'s own shared build needed the exact same libunwind-
          import-library fix already applied to `libc++abi.dll`
          (`CMAKE_SHARED_LINKER_FLAGS=@INSTALL_PREFIX@/lib/libunwind.
          dll.a` via `target_overrides.windows`, plus an explicit
          `"dependencies": ["libcxxabi", "libunwind"]` -- previously
          transitive only) -- confirmed via `_Unwind_Resume` undefined
          and 376 more references the first time `cxx_shared`'s own link
          was actually reached.
        Batch 2 -- getting a real client program (`tests/imported_libcxx_
        test.cc`, via `crt-libcxx-smoke`) to actually compile+link+run,
        never attempted on Windows before now:
        - `tools/test_libcxx_runtime.py` needed the same `rootfs`
          (toybox/uname resolution)/restricted-PATH/`CRT_HOST_CXX`/
          `--windows-sdk-libpath` treatment `tools/crt-libcxx-build.py`'s
          own `common_cmake_args()` already established, none of which
          this separate script had ever picked up (it had never actually
          reached a real compile on Windows before). Silent failures all
          the way down: mksh's own script-loading needs forward-slash
          paths the same as its exec() path lookup does; `CRT_HOST_CXX`
          unset meant a bare `clang++` mksh could never resolve; PATH
          pointed at plain `sysroot/bin` (DLLs only, no toybox), so
          `uname -m` inside `tools/crt-c++` failed with exit 127 and
          `set -eu` propagated that silently (no error text at all) to
          the whole script.
        - `tools/crt-c++` itself had never linked a real Windows C++
          *executable* before (only `CRT_CXX_BUILDING_RUNTIME=1`/
          shared-DLL paths were ever exercised): its `windows)` case's
          non-shared branch never had a `prelibs` variable at all (unlike
          `tools/crt-cc`'s matching branch), so `crt1_ctors_begin.o`/
          `crt1_ctors_walker.o`/`crt1_ctors_end.o` (global constructor
          walking) and `crt1_pseudo_reloc.o` (`_pei386_runtime_
          relocator()`) were silently missing from every C++ executable
          link -- confirmed via `undefined symbol: _pei386_runtime_
          relocator, referenced by crt1.c:117:(mainCRTStartup)`. Fixed by
          adding the identical `prelibs` machinery crt-cc already has.
        - A *client* compiling against the pre-built static `libc++.a`
          still saw every class member `__declspec(dllimport)`-decorated
          (`include/__config`'s `_LIBCPP_DLL_VIS` only has two states:
          `dllexport` when building the library, `dllimport` for
          "everyone else," no third "consuming a plain static archive"
          state) -- confirmed via lld naming the exact archive and
          mangled symbol it could see but not use ("is available... but
          cannot be used because it is not an import library"). Fixed
          with `-D_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS`, libcxx's own
          documented escape hatch, added to `tools/crt-c++`'s Windows
          compile flags whenever `CRT_CXX_RUNTIME_LINKAGE` is static and
          `CRT_CXX_BUILDING_RUNTIME` is unset (shared consumers correctly
          keep real dllimport decoration).
        - Client code compiled through `tools/crt-c++` was never getting
          `-fdwarf-exceptions` at all (only the internal recipe build
          had it) -- confirmed via `undefined symbol: __gxx_personality_
          seh0`, Clang's default native-SEH personality for `*-w64-
          mingw32`, which this project's own libc++abi (built DWARF-only)
          never exports. This is the project-wide application of the
          same decision `docs/cxx_runtime.md`'s "Windows exception-table
          format" section already documents -- fixed by adding the same
          flag to `tools/crt-c++`'s own Windows `common_flags`, not just
          the recipe build's.
        - `libc/src/arch/windows/common/dllcrt.c`'s `crtDllMainCRTStartup()`
          now calls `_pei386_runtime_relocator()` on `DLL_PROCESS_ATTACH`
          (the DLL analogue of `crt1.c`'s own executable-startup call),
          needed the moment libc++.dll's own auto-imported data reference
          to libunwind.dll appeared -- confirmed via lld-link's own
          refusal to produce the image at all ("output image has runtime
          pseudo relocations, but the function _pei386_runtime_relocator
          is missing"). `crt1_pseudo_reloc.o` had to be wired into every
          shared-DLL link path that could hit this: `tools/crt-cc` and
          `tools/crt-c++`'s own `shared_mode` branches, and the main
          project's own CMake-built DLL targets
          (`crt_configure_shared_runtime()` in the top-level
          `CMakeLists.txt`, via `$<TARGET_OBJECTS:crt1_pseudo_reloc>`).
        - Adding `crt1_pseudo_reloc.o` to every shared DLL link surfaced a
          real, unrelated regression: `pseudo_reloc.c`'s own diagnostic
          `fprintf(stderr, ...)` calls (only ever exercised from
          executables before) failed linking into `m.dll`/`dl.dll`/
          `c++.dll` at all (`undefined symbol: stderr`, a cross-DLL DATA
          reference these small DLLs never provide themselves). Switching
          to this project's own `write()` fixed that but broke something
          more subtle: `libc/src/fd.c` bundles `read()`/`write()`/every
          other fd primitive into one translation unit, so any reference
          to `write()` anywhere in a link pulls in that whole object,
          including its own real `read()` -- and `tests/windows_dll_
          symbol_priority_dll.c` deliberately defines its *own*
          conflicting `read()` (a regression fixture testing DLL symbol-
          priority resolution), so the two collided: `duplicate symbol:
          read`, a real failure this change caused. Root-caused all the
          way to zero dependency on this project's own libc instead of
          chasing the exact symbol each time: both `write()` and
          `abort()` (which itself needed enough of `signal.c`/`exit.c`'s
          own archive-member resolution to reach the same collision) were
          replaced with raw kernel32 calls (`WriteFile`/`GetStdHandle`/
          `ExitProcess`) -- genuinely nothing beyond a function-call IAT
          thunk to kernel32, matching this file's own "must run before
          absolutely anything else" design intent even more literally
          than before. Full `ctest` (119/119) reconfirmed clean after.
        **DONE: the *shared* leg of `crt-libcxx-smoke`
        (`CRT_CXX_RUNTIME_LINKAGE=shared`) now also passes** (`imported_
        libcxx_test: ok`, matching the static leg and matching macOS/
        Linux). Four genuinely separate gaps, found and fixed in turn:
        - `libc++.dll` was not exporting enough of `basic_string<char>`'s
          (and similar containers') *inline* member functions
          (constructors, `size()`, `data()`, `append()`, the destructor,
          ...) to satisfy a client that sees the whole class
          `__declspec(dllimport)`-decorated -- confirmed via lld naming
          dozens of specific missing `basic_string`/`__vector_base_
          common`/`length_error` members even though `src/string.cpp`'s
          own explicit template instantiation genuinely compiles every
          one of those members as a real, external (`T`) symbol (`llvm-
          nm` confirmed). Two independent, layered causes, both in
          `libstdc++/third_party/libcxx/recipe.json`'s `patches`:
          `-fvisibility-inlines-hidden` (libcxx's own top-level
          `CMakeLists.txt`, unconditional upstream) makes Clang emit real
          `.drectve` `-exclude-symbols:` directives for `_LIBCPP_INLINE_
          VISIBILITY`-marked members on Windows/PE -- safe on ELF/Mach-O
          (visibility there only controls cross-DSO export, never
          whether a client TU may define the symbol itself locally), but
          fatal once a client sees the whole class `dllimport`-decorated
          and is therefore forbidden from defining *any* of its members
          locally, hidden-visibility ones included; disabled for
          `if (NOT WIN32)` only. Necessary but not sufficient by itself
          -- undefined-symbol errors persisted identically even with a
          freshly rebuilt, exclude-symbols-free object, which led to the
          real, deeper cause: `include/__config`'s `_LIBCPP_EXTERN_
          TEMPLATE_TYPE_VIS` (the `extern template class` *declaration*-
          site macro every TU sees via `<string>`) was left completely
          empty by upstream when `_LIBCPP_BUILDING_LIBRARY` is defined,
          while its sibling `_LIBCPP_CLASS_TEMPLATE_INSTANTIATION_VIS`
          (the explicit-instantiation *definition*-site macro) correctly
          became `dllexport` -- a genuine declaration/definition
          mismatch, and Clang enforces a real C++/MSVC-ABI rule that a
          later definition cannot add a DLL attribute a preceding
          declaration of the same entity lacked. Confirmed via the exact
          compiler warning once the `.drectve` fix stopped masking it:
          `"'dllexport' attribute ignored on explicit instantiation
          definition ... 'dllexport' attribute is missing on previous
          declaration"`, naming `basic_string<char>`/`basic_iostream<char>`
          by name. Fixed by matching the declaration-site macro to the
          same `_LIBCPP_DLL_VIS`; `llvm-nm` confirmed real `__imp_`
          exports for `basic_string<char>`'s members afterward.
        - `tools/crt-c++`'s own Windows *shared* branch never linked
          `libc++abi.dll.a` at all -- only `libc++.dll.a` and (optionally)
          `libunwind.dll.a`, unlike the *static* branch's already-correct
          `libc++.a` + `libc++abi.a` + `libunwind.a`. Confirmed as a
          genuinely separate, previously-latent gap once the export-table
          fix above resolved every `basic_string` error and left a
          completely different set behind: `__cxa_allocate_exception`/
          `__cxa_throw`/`__cxa_begin_catch`/`__cxa_end_catch`/
          `std::terminate`/`__gxx_personality_v0`/vtable-for-
          `std::length_error`/vtable-for-`__cxxabiv1::__*_type_info`, all
          genuinely, correctly exported from `libc++abi.dll.a` itself
          (`llvm-nm`-verified) but never linked into the client at all.
          Fixed by adding the same three-candidate-name lookup loop
          (`libc++abi.dll.a`/`libc++abi_dll.lib`/`c++abi_dll.lib`) already
          used for `libc++.dll.a`/`libunwind.dll.a` in the same branch.
        - Once linking succeeded outright, the resulting `.exe` still
          failed to *run*, exiting `3221225781` (`0xC0000135` /
          `STATUS_DLL_NOT_FOUND`): `tools/test_libcxx_runtime.py` reused
          the same `env` for both the *compiler* invocation (needs
          `PATH="/system/bin:/bin:/usr/bin"`, the mksh/toybox-only POSIX
          string `tools/crt-libcxx-build.py`'s own `common_cmake_args()`
          already establishes) and for directly running the resulting
          *native* Windows executable -- but the real Windows DLL loader
          parses `PATH` as semicolon-separated backslash directories and
          found none in that POSIX string, so it could never find
          `libc++.dll`/`libc++abi.dll`/`libunwind.dll` (staged in
          `sysroot/bin`, not copied next to the smoke-test binary).
          Fixed by building a separate real-Windows-`PATH` environment
          (from the original inherited environment, with `sysroot/bin`
          prepended) just for the run step.
        - Even after that, `STATUS_DLL_NOT_FOUND` persisted -- traced all
          the way to `install/bin` never receiving `libc++.dll`/
          `libc++abi.dll` at all (only `libunwind.dll` made it, and only
          the two `.dll.a` *import libraries* + `.a` static archives made
          it to `install/lib`), even though the real `.dll` runtime
          binaries genuinely existed in the raw, un-installed build tree
          (`build/libcxx/lib/libc++.dll`, `build/libcxxabi/lib/
          libc++abi.dll`). Root-caused to both `libcxx/lib/CMakeLists.txt`
          and `libcxxabi/src/CMakeLists.txt`'s own upstream
          `install(TARGETS ...)` calls never specifying a `RUNTIME
          DESTINATION` -- CMake silently skips installing an artifact
          kind with no destination given rather than defaulting it, and a
          Windows/PE shared-library target's actual `.dll` is a `RUNTIME`
          artifact, distinct from its `ARCHIVE` (`.dll.a`) import library
          (already correctly installed via the existing `ARCHIVE
          DESTINATION`). Confirmed via direct comparison against
          libunwind's own sibling `install(TARGETS ...)` rule, which
          already has a working `RUNTIME DESTINATION` clause and does
          stage `libunwind.dll` correctly every time. Fixed with one new
          patch per recipe (`libstdc++/third_party/{libcxx,libcxxabi}/
          recipe.json`), each adding `RUNTIME DESTINATION
          ${..._INSTALL_PREFIX}bin COMPONENT ...` alongside the existing
          `LIBRARY`/`ARCHIVE` clauses.
        See HISTORY.md's dated entry for the full writeup of everything
        in this item, including the static-leg work from the same day.
     5. Then repeat the same recipe.json-driven build on Linux
        (host-provided libunwind
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
     6. **Add a real cross-OS regression test**: `_Unwind_RaiseException`
        actually reaching a C++ `catch` block, verified identically on all
        three hosts once the build passes -- `crt-libcxx-smoke` already has
        the right shape (exception throw/catch is already one of its
        checks, now confirmed working on macOS, on Linux for both linkage
        modes, and on Windows for both linkage modes too -- `imported_
        libcxx_test: ok`, see step 4 above) to extend rather than needing
        a new test from scratch. All three hosts, both linkage modes,
        genuinely green as of this item's completion.
     7. **DONE: native-callback/boundary safety net for Windows.** Checked
        directly (2026-08-21, see `docs/cxx_runtime.md`'s "Known cost:
        DWARF-compiled code has zero Windows-native unwind info"):
        `-fdwarf-exceptions` builds emit no `.pdata`/`.xdata` at all for
        *any* non-leaf function, throwing or not -- confirmed by diffing
        `-fseh-exceptions` vs `-fdwarf-exceptions` object output for
        identical source. This is a real Windows x64 ABI-conformance gap
        (the OS assumes a function with no unwind-table entry is a leaf),
        not just a C++ `catch`-interop limitation, so it affects hardware
        exceptions propagating through a CRT/libc++ frame, WER/debugger/ETW
        stack walks that cross one, and CRT/libc++ code registered directly
        as a raw native OS callback (window proc, `CreateThread` entry
        point, vectored exception handler, COM vtable). Plain `LoadLibrary`
        plus calling a non-throwing export is unaffected.

        This item's own original design ("a boundary shim compiled with
        real SEH at every point CRT/libc++ code is entered from or exits
        into native OS-driven control flow") was **built as a standalone
        repro first, and empirically disproved**: a `-fseh-exceptions`
        `__try`/`__except` wrapped directly around a call into a chain of
        `-fdwarf-exceptions`-compiled functions does NOT catch a hardware
        fault raised several frames deeper -- the OS's own frame-based
        search still has to walk the untabled frames beneath the boundary
        to reach it, and fails at the first one, so the process crashes
        uncontrolled regardless of the boundary. `SetUnhandledException
        Filter()` was tried next (its contract -- "only called once
        nothing else handled it" -- can never preempt a legitimate
        `__except`) and also disproved: reaching "nothing else handled it"
        itself needs the same broken walk to complete, so it never fires
        for a deep-DWARF-chain fault either.

        **What actually works, and shipped**: `libc/src/arch/windows/
        common/dwarf_unwind_safety_net.c` installs a process-wide
        `AddVectoredExceptionHandler()` (VEH) callback at CRT startup
        (`crt1.c` for executables, `dllcrt.c` for DLLs) -- confirmed
        empirically that VEH, unlike frame-based SEH, does not depend on
        walking the stack at all and reliably fires for the same
        deep-DWARF-chain fault with no boundary shim anywhere in the link.
        The one real risk (VEH fires unconditionally, *before* any
        frame-based `__except`, confirmed empirically to preempt and break
        even a fully legitimate one) is closed by gating the handler on
        `RtlLookupFunctionEntry()` -- the same table lookup the OS's own
        dispatch relies on -- called on the faulting address: non-NULL
        (real `.pdata` present, regardless of relation to CRT/libc++) means
        defer completely (confirmed to let a real `__except` elsewhere
        catch it exactly as if this file did not exist); NULL (the actual
        gap) means take over, log a brief diagnostic, and `ExitProcess()`
        with this project's own existing `128 + <POSIX signal>` convention
        (see `libc/src/signal.c`'s `abort()`) instead of undefined
        behavior. Wired into every executable and shared DLL this project
        builds (`libc/CMakeLists.txt`'s `CRT_STARTUP_OBJECTS`, the
        top-level `CMakeLists.txt`'s `crt_configure_shared_runtime()`,
        `tools/crt-cc`/`tools/crt-c++`'s own Windows branches) -- needs no
        per-callsite boundary shim at all, which turned out to be strictly
        simpler than (and a real fix, unlike) the item's own original
        per-callback-wrapper design. A permanent regression test
        (`tests/windows_dwarf_unwind_safety_net_test.c` +
        `_victim.c`) exercises this through the real production toolchain
        (`tools/crt-cc` with an explicit `-fdwarf-exceptions` flag, no
        libcxx dependency needed), asserting the victim's fault produces
        the controlled `128 + SIGSEGV` exit rather than any other outcome.

        **Scope, stated honestly**: this fixes exactly the third bullet
        above (a raw OS callback is now safe) and turns the first two
        (hardware fault mid-DWARF-frame, OS-driven stack walk crossing
        one) from "undefined behavior" into "a controlled, deterministic
        process exit" -- but it does NOT let a C++ `catch` recover from a
        hardware fault (never promised, matches existing non-`/EHa`
        semantics), does NOT fix third-party tools that do their own
        separate OS-native frame-based walk (a debugger's live call-stack
        view, WER's own minidump writer, an ETW profiler still misbehave
        crossing a DWARF frame exactly as before), and does NOT attempt a
        real backtrace (only the single faulting address is reported --
        a DWARF-CFI-based backtrace via this project's own libunwind would
        be a real, valuable follow-up, deliberately not attempted here
        since libunwind is only an optional `CRT_USE_IMPORTED_LIBCXX`
        component today, not a base libc dependency). Sharpens, does not
        replace, the existing "exceptions may cross the bridge... default
        answer being no" caveat in the Windows MSVC ABI Bridge Lane section
        of `docs/cxx_runtime.md`. See HISTORY.md's dated entry for the full
        three-stage empirical trail (the standalone repro, the VEH-vs-SEH-
        vs-SetUnhandledExceptionFilter comparison, and the final
        RtlLookupFunctionEntry-gated design) and the exact repro
        commands used.
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
