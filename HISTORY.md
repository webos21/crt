# HISTORY: CRT Resolved Work Log

Reverse-chronological record of completed work on the CRT shell/rootfs/porting
loop and PAL, one dated entry per resolved item (grouped by the commit(s) that
landed it). Current/open work lives in `TODO.md`; this file is append-only
history. Dates are the git author date of the commit that introduced or last
substantively updated each entry, so an entry whose investigation spanned
multiple days is dated by its span (`start..resolved`) or by its last
substantive update.

## 2026-08-23

- **`crtgfx_skia_raster_smoke`/`libcrtgfx.dylib` fixed and verified for real
  on macOS (arm64 host), same underlying class of bug as the Linux and
  Windows entries below but a genuinely different fix -- user reported the
  exact `cmake --build --preset macos-host-ninja-debug ... --target
  crtgfx_skia_raster_smoke` command failing with dozens of undefined
  `std::__1::...` symbols (basic_string, iostream/locale/stringstream,
  `__shared_ptr` internals, `__libcpp_verbose_abort`, ...) linking both the
  smoke executable and `libcrtgfx.dylib` against `libskia.a`. Two real,
  distinct root causes in `libcrtgfx/CMakeLists.txt`, plus one stale-build-
  artifact issue (not a code bug):
  1. **The Linux-only `CRTGFX_CRT_STATIC_LIBS`/`CRTGFX_CRT_SHARED_LIBS`
     bootstrap-`cxx`-to-real-imported-libc++ swap (see the Linux entry
     below) had never been extended to macOS.** Added a matching `elseif`
     branch; deliberately no `libunwind` substitution here, unlike Linux --
     macOS never builds one (`libstdc++/third_party/libunwind/recipe.json`
     excludes macOS from its own `target_os` list: Darwin's libSystem
     already provides a real native unwinder).
  2. **The real fix, and the one that's actually macOS-specific**: Apple
     ld (`ld64`/`ld-prime`) scans each static archive exactly once,
     left-to-right, only pulling members needed by symbols already
     outstanding at that point -- unlike GNU ld/lld on Linux, it has no
     `--start-group`/`--end-group` equivalent. `crtgfx`'s own build was
     linking `${CRTGFX_CRT_STATIC_LIBS}` (now including the real
     `libc++.a`) *before* `${CRTGFX_SKIA_LIBRARIES}` in its own
     `target_link_libraries()` calls, so `libc++.a` always landed *before*
     `libskia.a` on any consumer's flattened link line -- by the time ld
     reached `libskia.a`'s own extensive std::string/iostream/locale use
     (SkSL's compiler/parser/debug-trace code), those symbols were no
     longer "outstanding" and got silently skipped. Fixed by simply
     reversing the call order in `crtgfx`'s own plain `else()` branch
     (Skia's libraries first, `CRTGFX_CRT_STATIC_LIBS` second) -- the
     reverse of the intuitive "dependencies after the thing that needs
     them" ordering, but correct for a single-pass linker. Two more
     elaborate approaches were tried and rejected before landing on this:
     a second explicit `target_link_libraries()` mention of `libc++.a` on
     the smoke executable's own target did nothing (CMake deduplicates a
     library by resolved path, silently keeping only `crtgfx`'s own
     earlier-propagated mention); `-Wl,-force_load,libc++.a` did fix the
     *link*, but unconditionally pulling in every object in `libc++.a`
     dragged in `iostream.cpp.o`'s always-run global constructor
     (`std::ios_base::Init`) even though nothing in the program uses
     `std::cin`/`cout` -- confirmed for real that this crashed at
     dyld-init time with a null-vtable `EXC_BAD_ACCESS` in
     `codecvt<char,char,mbstate_t>::encoding()`, before `main()` even
     runs, and that the identical `<iostream>` code links and runs fine
     through `tools/crt-c++`'s own normal (non-force_load) static link
     recipe -- i.e. real imported libc++ global constructors work fine on
     this project's freestanding macOS runtime; only unconditionally
     force-loading the whole archive was the problem. Also removed a
     stray, pre-existing hardcoded `target_link_libraries(
     crtgfx_skia_raster_smoke PRIVATE cxx c)` specific to that one
     executable target, which re-added the small bootstrap ABI shim on
     top of the real one `crtgfx` now correctly propagates.
  3. **Not a code bug**: `sysroot/lib/libc++.a` had reverted to the small
     bootstrap stub (18KB, `cxxabi.c.o`/`msvcabi.c.o`/`new_delete.cc.o`)
     instead of the real, already-built 9.1MB archive at
     `external/llvm-runtimes/install/lib/libc++.a` -- the top-level
     `sysroot` custom target's own `cmake --install` step re-installs the
     bootstrap `cxx`/`cxx_shared` targets' own `libc++.a`/`.dylib` output
     names on every run, overwriting whatever `crt-libcxx-sysroot` had
     staged there earlier. Re-ran `tools/install_libcxx_runtimes.py`
     directly to restage the real archive; a subsequent full default
     `cmake --build` (which runs the whole `crt-libcxx-build` ->
     `crt-libcxx-sysroot` chain after `sysroot` itself, in the right
     order) restaged it correctly on its own, confirming this was a
     transient ordering artifact from this session's targeted, partial
     rebuilds rather than a standing bug.
  Verified for real: `crtgfx_skia_raster_smoke` links, runs, and prints
  `crtgfx_skia_raster_smoke: ok`; `libcrtgfx.dylib` (`crtgfx_shared`)
  links; a full default `cmake --build` and the full `ctest` suite
  (105/105) both pass with no regressions.

- **`crtgfx_skia_raster_smoke` also verified for real on Linux (WSL/Ubuntu
  26.04, amd64), direct follow-up to the Windows mingw32-unification entry
  just below -- user asked "did the Skia smoke test actually pass on WSL
  too?", which surfaced that it never had (only "Skia's own build" -- the
  `libskia.a` archive -- had ever been verified there, per the entry
  below's own record of prior claims).** Building and running it for real
  (fresh clone at this commit, `crt-libcxx-sysroot` + `crtgfx-skia-build`
  both already independently verified working on this platform) found and
  fixed three more real, Linux-specific bugs, then one real environment
  gap, none overlapping with the Windows fixes below:
  1. **A genuine GNU-ld (`ld.bfd`) circular-archive-resolution gap**,
     same class as the one `tools/crt-cc`'s own Linux branch already
     documents, but never hit by a *regular CMake* target before:
     `crtgfx`(STATIC)'s own PRIVATE `c`/`m`/`dl`/`cxx` deps and PUBLIC
     `libskia.a` dep get flattened into any consumer's link line with
     `libskia.a` always last (confirmed via the generated link command),
     so `ld.bfd`'s single left-to-right archive scan finishes scanning
     `libm.a`/`libc++.a`(=`cxx`)/`libc.a` before it ever discovers
     `libskia.a`'s own needs (`atan2f`, `operator new`/`delete`, even
     `__dso_handle`) -- `undefined reference to 'atan2f'` etc, then
     `hidden symbol '__dso_handle' isn't defined`. Fixed with `-Wl,
     --start-group`/`--end-group`, wrapped around `crtgfx`'s (and
     `crtgfx_shared`'s) own `CRTGFX_CRT_STATIC_LIBS`/`CRTGFX_SKIA_
     LIBRARIES` linking directly in `libcrtgfx/CMakeLists.txt`, not on
     each consumer -- confirmed for real that a consumer-side attempt
     (wrapping the markers around `crtgfx_skia_raster_smoke`'s own
     `target_link_libraries()`/`target_link_options()` calls instead)
     does not reliably bracket `crtgfx`'s own separately-flattened
     transitive dependencies, splitting the group with `crtgfx`'s own
     `libm.a`/`libskia.a` landing outside it.
  2. **`crtgfx_skia_raster_smoke`'s own redundant, explicit `cxx c` link**
     (pre-existing, for the non-Windows-imported-libcxx case) landed
     *outside* the new group (this call happens before `crtgfx` itself
     gets flattened in), and CMake's own link-line de-duplication kept
     only that first, ungrouped occurrence -- silently dropping the
     grouped one from `crtgfx`'s own propagated deps. Since `cxx`'s
     `new_delete.cc` is exactly what provides `operator new`/`delete`,
     this left them unresolved even with fix 1 in place. Fixed by not
     re-linking `cxx c` explicitly for this Linux+Skia case at all --
     `crtgfx`'s own grouped propagation already provides it.
  3. **Linux needed the same bootstrap-`cxx`-to-real-imported-libc++**
     **swap already done for Windows** (see the entry below), just never
     applied here: once fixes 1-2 resolved the `libm`/`operator new`
     gaps, real `std::__1::basic_string`/`std::__1::locale`/iostream
     symbols Skia's own SkSL debug-trace code needs (`std::__1::to_
     string`, `std::__1::locale::classic()`, iostream vtables, ...)
     stayed undefined -- the bootstrap ABI shim never provided a real
     STL. Fixed the same way as Windows: swap `cxx`/`cxx_shared` for the
     real imported `libc++.a`+`libc++abi.a`+`libunwind.a` (`.so` variants
     for the shared case) chain, gated on `CRT_TARGET_OS STREQUAL
     "linux" AND CRTGFX_ENABLE_SKIA AND CRT_USE_IMPORTED_LIBCXX`, applied
     once at `CRTGFX_CRT_STATIC_LIBS`/`CRTGFX_CRT_SHARED_LIBS`'s own
     definition so every existing consumer inherits it unchanged (no
     Windows-style DLL-vs-static naming split needed on Linux). Headers
     needed no equivalent Linux-side fix: `crt_cxx_build_flags`'s own
     existing Linux fallback (real host `libc++-dev` headers, reached via
     the project's own `-stdlib=libc++`-during-detection trick) already
     supplied Itanium-ABI-compatible headers -- confirmed for real, the
     *compile* step for `skia_bridge.cc`/`skia_raster_smoke.cc` never
     failed on this platform, only the link.
  4. **A genuine `ld.bfd`-specific runtime-loader bug**, found only after
     all three link-time fixes above: the fully-linked, fully-resolved
     binary failed to even *load*: `error while loading shared libraries:
     unexpected PLT reloc type 0x00`. `readelf -r` showed `.rela.plt`
     holding exactly one bogus entry (`R_X86_64_NONE`, sym 0, offset 0)
     glibc's own `ld.so` refuses to process. Bisected by hand (manually
     replaying the exact real link command with pieces removed/varied,
     confirmed each result with a fresh `readelf -r`): ruled out `--gc-
     sections` and the `--start-group`/`--end-group` markers alone (a
     trivial `c`/`m`/`dl`-only group, or one also carrying `libc++`/
     `libc++abi`/`libunwind`, both stayed clean) -- only the full,
     real combination (through `ld.bfd` specifically) produced it. User
     suggested switching the linker to `lld` outright rather than chasing
     `--gc-sections` further; installed `lld` (user's own `apt` install)
     and relinking the identical inputs/flags with `-fuse-ld=lld` instead
     produced a clean `.rela.plt` immediately -- a real, if obscure,
     `ld.bfd` fragility around `--start-group`-wrapped PIE archive
     linking that `lld` does not share. Added `-fuse-ld=lld` to `crtgfx_
     skia_raster_smoke`/`crtgfx_shared`'s own Linux+`CRTGFX_ENABLE_SKIA`
     link options only (regular Linux executables keep the system
     default `ld.bfd` unchanged, which has always worked fine for them).
  With all four fixed, the *real* `ctest`-driven run genuinely passes:
  `crtgfx_skia_raster_smoke: ok`, confirmed twice for consistency. An
  earlier direct/manual invocation (outside `ctest`, from a fresh nested
  WSL shell) printed a *different*, legitimate failure first
  (`end_frame (-3)`, Wayland `present_software` -- unrelated to any of
  the above, a real Wayland/WSLg environment-connectivity difference
  between that shell and the one `ctest` itself ran in, not a code bug)
  -- recorded here since it cost real diagnosis time before `ctest`'s own
  run showed the true, passing result. Full regression via the real
  `ctest`-driven suite, run twice: **100% tests passed, 105/105**, with
  `CRTGFX_ENABLE_SKIA=ON` and `CRT_USE_IMPORTED_LIBCXX=ON` both on
  (`CRTGFX_ENABLE_SKIA` stays `OFF` by default; unaffected either way).
  Verified in a disposable clone (`/tmp` turned out to be tmpfs on this
  WSL distro and was wiped mid-session by a VM idle-restart, losing a
  full build in progress -- redone under `$HOME`, which persists).

- **Windows regular-CMake C++ target unified to `--target=*-w64-mingw32`
  (matching every `tools/crt-cc`/`tools/crt-c++`-driven "port" build), and
  `crtgfx_skia_raster_smoke` finally links and runs
  (`crtgfx_skia_raster_smoke: ok`) -- resolving the ABI-mismatch finding
  documented in the entry directly below, by the direction the user chose
  after an explicit, read-only review ("mingw32 방식으로 진행하자").** Full
  Windows regression after every fix below: **100% tests passed, 121/121**
  (120 pre-existing + the newly-registered `crtgfx_skia_raster_smoke_runs`),
  both in an isolated scratch build dir and on the real
  `windows-host-ninja-debug` preset. This was a long chain of real,
  independently-discovered bugs, each confirmed against actual build/link
  output before being fixed (never guessed):
  1. **`CMakeLists.txt`'s top-level Windows target-selection block** now
     always sets `CMAKE_C_COMPILER_TARGET`/`CMAKE_CXX_COMPILER_TARGET`/
     `CMAKE_ASM_COMPILER_TARGET` to `x86_64-w64-mingw32`/
     `aarch64-w64-mingw32` (both the `CRT_TARGET_ARCH=host` and explicit
     cross-arch cases), instead of leaving "host" on Clang's own bare
     `-pc-windows-msvc` default. Aligns with `docs/cxx_runtime.md`'s own
     already-documented "ABI Policy" (Itanium/GNU C++ ABI lane for
     CRT-targeted code, a separate future MSVC bridge lane for real DLL
     interop) and its DWARF-CFI exception-table design, neither of which
     the pre-existing MSVC-simulate default for regular CMake C++ code
     actually matched.
  2. **CMake's own library-naming convention shifted** with the target
     (`c.lib`/`c_dll.lib` MSVC-style -> `libc.a`/`libc_dll.dll.a` GNU/
     MinGW-style for every regular `add_library()` target: `c`/`m`/`dl`/
     `cxx`/their `_shared` variants), breaking every hardcoded
     `sysroot/lib/<name>.lib`-style path -- `tools/crt-cc`/`tools/crt-c++`
     themselves (used project-wide, not just by tests), which hardcoded
     the old MSVC-style names for the regular-build-produced `c.lib`/
     `m.lib`/`dl.lib`/`c++.lib`/`c_dll.lib`/etc. Fixed by updating those
     literals to the new GNU-style names (`tools/crt-c++`'s own already-
     existing `libc++.a`-first / `c++.lib`-fallback `elif` chain for the
     C++ runtime specifically was left alone -- it already preferred the
     new name; only the unconditionally-hardcoded `c`/`m`/`dl` paths in
     both wrappers needed updating).
  3. **A real, previously-latent CMake/Ninja generator gap**: `windows_
     dwarf_unwind_safety_net_test`/`init_array_test`'s own
     `__attribute__((constructor))`/`((destructor))` now lower to plain,
     link-order-sensitive `.ctors`/`.dtors` sections (GNU convention)
     instead of the order-insensitive `.CRT$XCU`/`.CRT$XTX` (MSVC
     convention) -- and CMake's Ninja generator does **not** actually
     preserve `${CRT_STARTUP_OBJECTS}`/`${CRT_STARTUP_END_OBJECTS}`'s
     intended interleave position relative to a target's own directly-
     compiled source files: confirmed for real by inspecting the
     generated `build.ninja`, every `$<TARGET_OBJECTS:...>`-sourced object
     (including `crt1_ctors_end.o`, meant to be strictly last) gets
     grouped together and placed *ahead of* the target's own source
     objects regardless of where each was listed in `add_executable()`'s
     SOURCES. `init_array_test`'s own constructor silently never ran as a
     result (clean exit, no crash -- only caught because the test
     deliberately checks for its own required output line). Fixed with a
     new top-level `crt_link_startup_end_objects(target)` helper
     (`CMakeLists.txt`) that links `CRT_STARTUP_END_OBJECTS` via
     `target_link_libraries()` instead (which *does* append in real call
     order, after a target's own primary objects -- already relied on
     elsewhere in this file for `crt1_pseudo_reloc`/`crt1_dwarf_safety_
     net`), called as the last statement touching each affected
     executable target across `tests/CMakeLists.txt`, `shell/
     CMakeLists.txt`, and `libcrtgfx/CMakeLists.txt` (10 call sites total).
  4. **`data_model_test.c`'s own Windows `long double` ABI assertion was
     wrong for the new target**: real, verified via `clang -target
     x86_64-w64-mingw32/aarch64-w64-mingw32 -dM -E`, x86_64 Windows now
     gets genuine 80-bit extended-precision `long double`
     (`sizeof==16`/`LDBL_MANT_DIG==64`, matching generic x86_64 Linux) --
     only `aarch64-w64-mingw32` keeps `long double == double`
     (`sizeof==8`/`53`, Microsoft's own ARM64 ABI choice, unrelated to
     GNU-vs-MSVC). Split the test's Windows branch by architecture to
     match.
  5. **No wiring existed anywhere for regular CMake C++ code to reach this
     project's own imported libc++** (`${CRT_SYSROOT}/include/c++/v1`,
     `libc++.a`/`libc++abi.a`/`libunwind.a`) on Windows -- the pre-existing
     `crt_cxx_build_flags` fallback (`CRT_CXX_STANDARD_INCLUDE_DIRS`) had
     always, on Windows, resolved to the *real host MSVC STL* instead
     (found via a direct `clang++ -v` compiler probe, since regular CMake
     C++ code previously compiled `-pc-windows-msvc`) -- exactly the ABI
     mismatch documented in the entry below. Under the new mingw32 target
     that probe finds nothing at all (no real mingw-w64 GCC/libstdc++
     install on this machine to fall back to either), so the very first
     real Skia header reaching `<cstddef>` failed outright. Fixed by
     adding an explicit `-isystem${CRT_SYSROOT}/include/c++/v1` (via
     `target_compile_options()`, landing in `<FLAGS>` ahead of the
     inherited chain -- the same ordering fix already established below)
     and swapping the link from the small bootstrap `cxx`/`cxx_shared` ABI
     shim to the real imported `libc++.a`+`libc++abi.a`+`libunwind.a`
     static chain (`libc++.dll.a`+`libc++abi.dll.a`+`libunwind.dll.a` for
     the `crtgfx_shared` DLL case), scoped to `crtgfx_skia_objects`/
     `crtgfx_skia_raster_smoke`/`crtgfx_shared` only (every other Windows
     C++ consumer of `crt_cxx_build_flags` only exercises the bootstrap
     shim and has never needed a real STL) -- gated on a new `CRTGFX_
     ENABLE_SKIA=ON` + Windows guard requiring `CRT_USE_IMPORTED_LIBCXX=ON`
     and the sysroot headers already staged (`libcrtgfx/CMakeLists.txt`,
     mirroring the file's own existing `SkSurface.h`/`CRTGFX_SKIA_
     LIBRARIES` FATAL_ERROR pattern -- deliberately a configure-time file-
     existence check, not a `add_dependencies()` ninja edge, since a
     direct edge onto `crt-libcxx-sysroot` hits a real dependency cycle:
     `crtgfx_skia_objects`'s output folds into the `crtgfx` STATIC_LIBRARY
     that the `sysroot` target's own DEPENDS list already needs, and `crt-
     libcxx-configure` already depends on `sysroot` -- confirmed for real,
     "CMake Generate step failed" naming exactly this strongly-connected
     component).
  6. **Skia's own `SK_BUILD_FOR_WIN`/`__forceinline`/`<crtdbg.h>` trap**
     (already fixed for Skia's own GN build via `-DSK_BUILD_FOR_UNIX`, see
     `tools/build_skia.py`'s own comment) hit again here, since `crtgfx_
     skia_objects`/`crtgfx_skia_raster_smoke` `#include` the same Skia
     headers directly through a completely separate (regular CMake, not
     GN) compile path: fixed with the identical `-DSK_BUILD_FOR_UNIX`
     `target_compile_definitions()` on both targets.
  7. **`libm` was genuinely missing `atan2f`** (only `atan2`/`atan2l`
     existed -- confirmed via `llvm-nm`/source search, a real, pre-
     existing gap unrelated to this migration, just never exercised until
     `SkPathBuilder::arcTo()`/`SkComputeRadialSteps()` called it). Added as
     a cast-wrapper around the existing `atan2()` in `libm/src/basic.c`,
     matching that same file's own established `asinf`/`acosf`/`coshf`
     precedent (this project's real single-precision FreeBSD source,
     `e_atan2f.c`, was never imported -- a wrapper was chosen over porting
     an unverifiable new algorithm).
  8. **This project's Windows PAL has never implemented native COFF
     Thread-Local Storage** (`_tls_index`/`.tls` section/
     `IMAGE_TLS_DIRECTORY`, normally supplied by real mingw-w64's `crt/
     tlssup.c` or real MSVC's own `tlssup.obj`) -- `libc/src/tls.c`'s own
     TLS mechanism is a separate, explicit `TlsAlloc`/`TlsGetValue`-based
     scheme, unrelated to the compiler-emitted native lowering a plain
     C++11 `thread_local` (`SkStrikeCache::GlobalStrikeCache()`) uses by
     default (`ld.lld: error: undefined symbol: _tls_index`). Asked the
     user whether to implement real native TLS support (a genuine, all-new
     PAL feature) or find another way; re-investigation found `clang
     -femulated-tls` -- confirmed for real via a standalone compile +
     `llvm-nm` -- lowers `thread_local` to `__emutls_get_address()` calls
     instead, backed by compiler-rt's own `emutls.c` (itself just
     `TlsAlloc`/`TlsGetValue`/`TlsSetValue` under the hood, already present
     in the vendor `clang_rt.builtins-x86_64.lib` this project always
     links) -- the same emulated-TLS strategy Bionic/Android itself
     historically used for the identical reason, matching this whole
     project's own "Bionic-compatible" identity far better than a brand
     new native-TLS implementation would have. Added `-femulated-tls` to
     both `tools/crt-cc` and `tools/crt-c++`'s own Windows `common_flags`
     (project-wide, so every port this wrapper ever compiles gets
     consistent `thread_local` lowering), then force-rebuilt Skia from
     scratch (the flag lives inside the wrapper script, invisible to
     Skia's own GN/ninja command-line hashing, so a stale `libskia.a`
     would otherwise never pick it up) -- confirmed via `llvm-nm` that
     `SkStrikeCache`'s own `thread_local` now references `__emutls_get_
     address`, not `_tls_index`.
  9. **`crt_compiler_rt_builtins` had silently resolved to an empty
     INTERFACE library on every regular Windows CMake build all along**
     (top-level `CMakeLists.txt`, pre-existing, unrelated to this
     migration): its `--print-libgcc-file-name` probe calls the raw
     compiler directly, with no knowledge of `CMAKE_C_COMPILER_TARGET`,
     so on Windows it always queried Clang's own bare default and got back
     the unresolved literal `"libgcc.a"`. Invisible until `__emutls_get_
     address` became the first symbol any regular CMake Windows build
     actually needed from that archive. Fixed with a Windows-specific
     probe (`-print-resource-dir` + the same real-archive candidate search
     already proven in `tools/crt-c++`'s own `CRT_CXX_BUILDING_RUNTIME`
     comment), normalized through `file(TO_CMAKE_PATH ...)` (a second,
     separate real bug this surfaced: the raw backslash-separated Windows
     path broke `cmake_install.cmake`'s own string parsing at *install*
     time -- "Syntax error in cmake code" -- confirmed for real, since
     this was the first time that `install(FILES "${CRT_COMPILER_RT_
     BUILTINS}" ...)` line ever actually received a non-empty, real path
     to install).
  10. **Two more real, narrow link-time gaps**, both scoped to `crtgfx_
      skia_raster_smoke`/`crtgfx_shared` once `crt_compiler_rt_builtins`
      started actually linking real content: (a) something reachable only
      once both `crtgfx.a` and `libskia.a` are present together makes
      lld-link go looking for `libuuid.a` (bisected for real; no `strings`-
      visible COFF `.drectve` directive found in any linked archive to
      explain the exact trigger) -- fixed by supplying the real Windows
      SDK `Uuid.Lib` directly, mirroring this file's own existing `user32.
      lib`/`gdi32.lib` `find_file()` pattern; (b) `clang_rt.builtins-
      x86_64.lib` -- confirmed via `llvm-nm` to be a real, vendor-shipped
      archive bundling more than pure arithmetic builtins -- has at least
      two object members (`emutls.c.obj`, `enable_execute_stack.c.obj`)
      that each carry their own internal fallback `fprintf` (itself
      calling the real UCRT's `__stdio_common_vfprintf`, clearly meant for
      a real-CRT host, not this freestanding one), which COFF/lld-link's
      archive semantics (pulling in a member makes *every* symbol it
      exports part of the final image, unlike ELF's narrower per-symbol
      resolution) turns into a hard `duplicate symbol: fprintf` against
      this project's own real one in `libc.a`. Fixed with `-Wl,--allow-
      multiple-definition` (lld keeps the first-encountered definition,
      matching link order -- this project's own `libc.a` links well before
      the vendor archive) plus two tiny, deliberately-non-functional link
      stubs (new `libc/src/arch/windows/common/emutls_link_stubs.c`,
      compiled only into the affected targets) for `__acrt_iob_func`/
      `__stdio_common_vfprintf` themselves, which `emutls.c.obj`'s own
      `win_error()` diagnostic path (reached only if a genuine Win32
      `TlsAlloc`/`TlsSetValue` call fails -- an already-fatal, essentially
      unreachable condition) also references and this freestanding CRT
      has no real UCRT to provide; the stubs just `abort()` immediately,
      never attempting a real UCRT-compatible implementation for a path
      that in practice never runs.
  - `crtgfx_shared` (the DLL variant, part of the default `ALL` target
    whenever `CRTGFX_ENABLE_SKIA=ON`, unlike the test-only `crtgfx_skia_
    raster_smoke`) needed the identical set of fixes (5, 6, 9, 10) applied
    a second time for its own, separately-linked image, once its own
    build was reached in a full default-target rebuild.

- **`-DCRTGFX_ENABLE_SKIA=ON` reconfigure + `crtgfx_skia_raster_smoke` build
  attempted on Windows; fixed one more real bug (a project-wide UCRT
  `printf` inline-definition clash), then hit a genuine, deeper
  architectural gap and deliberately stopped, by explicit user choice, once
  a real fix meant either reworking this project's own Windows C++ target
  triple or Skia's.** Direct follow-up to the CPU-raster archive build
  fix just below. Reconfiguring with `CRTGFX_ENABLE_SKIA=ON` picked up the
  just-built `libskia.a` automatically (`CRTGFX_SKIA_LIBRARIES` auto-
  detects it if present) and registered `crtgfx_skia_raster_smoke_runs`
  (120 -> 121 tests). Building it surfaced two real problems:
  1. **`lld-link: error: duplicate symbol: printf`** (one copy from
     `.../ucrt/stdio.h:956`, one from this project's own real
     `c.lib(printf.c.obj)`). `-E` preprocessing the actual test file
     traced this to a real, if indirect, chain: some Skia header reaches
     `<limits>` -> the real MSVC `<cwchar>` -> the real MSVC `<cstdio>` ->
     `#include_next`'s own way into ucrt's real `stdio.h` -- which, by
     default, *defines* (not just declares) `printf` as an inline
     wrapper (confirmed by reading `ucrt/stdio.h` directly, its own
     `#if defined _NO_CRT_STDIO_INLINE ; #else { ... } #endif` pattern
     around every such wrapper). `_NO_CRT_STDIO_INLINE` is ucrt's own
     real, documented escape hatch for exactly this -- added to
     `crt_cxx_build_flags` project-wide (top-level `CMakeLists.txt`,
     Windows-only) rather than scoped to the one test that surfaced it,
     since any future C++ code reaching the same real-MSVC-STL chain
     (itself reachable on purpose, for libc++'s own internal
     `include_next` needs) would hit the identical clash. A separate,
     complementary fix landed alongside it (though it turned out not to
     be the actual root cause of the printf clash): `libcrtgfx/
     CMakeLists.txt`'s own attempt to put this project's `libc/include`
     ahead of the real SDK dirs via `target_include_directories(...
     SYSTEM BEFORE ...)` never actually worked, because this project's
     own custom `CMAKE_CXX_COMPILE_OBJECT` rule emits `<FLAGS>` (where
     `crt_cxx_build_flags`'s own inherited real-SDK `-isystem` chain
     lives) before `<INCLUDES>` (where `target_include_directories`
     lands) on the real command line -- confirmed via the regenerated
     `build.ninja`. Fixed by adding the override via
     `target_compile_options()` instead, which does land in `<FLAGS>`,
     ahead of the inherited chain.
  2. **A genuine, deeper architectural ABI mismatch**, once compilation
     succeeded: `lld-link: error: undefined symbol` for real Skia C++
     methods (`SkPaint::SkPaint()`, `SkCanvas::drawRect()`, ...) that
     demonstrably do exist in `libskia.a` (confirmed via `llvm-nm`,
     real "T"-defined symbols, e.g. `_ZN7SkPaintC1Ev`). Root-caused to
     this project's own regular CMake C++ code (`crtgfx.lib`, `c++.lib`,
     and now `crtgfx_skia_raster_smoke.cc` itself) compiling with
     clang's *default* Windows target (`x86_64-pc-windows-msvc`, MSVC-
     style C++ name mangling -- confirmed by the linker's own error text
     using MSVC `__cdecl`-decorated demangled names, and by
     `CMakeLists.txt`'s own comment: "'-pc-windows-msvc' ... matches
     what CMAKE_C_COMPILER=clang already defaults to with no explicit
     --target at all"), while Skia itself is built via `tools/
     crt-cc.cmd`/`crt-c++.cmd`, which explicitly force
     `--target=x86_64-w64-mingw32` (Itanium/GNU-style mangling --
     `_ZN7SkPaintC1Ev` is exactly that form) for GNU-macro-compatible
     autoconf-style third-party code. These two mangling schemes are not
     link-compatible. This is the first target in the whole project that
     ever tried to link the two together directly (every earlier Skia-
     touching CMake code either only used Skia's C API surface via
     `crtgfx/skia.h` at a boundary that didn't require this, or never
     actually linked against `libskia.a`'s own C++ symbols). Given the
     user's own explicit choice (asked directly, given the real scope
     jump): **stopped here rather than picking a fix** -- either
     retargeting this one test (and transitively `crtgfx.lib`/`c++.lib`/
     `c.lib`, real rework, not scoped) to `-w64-mingw32` to match Skia,
     or retargeting Skia's own build to `-pc-windows-msvc` to match this
     project's regular code (risking the GNU-macro compatibility Skia's
     own GN/autoconf-shaped build genuinely needs), are both real,
     substantial architectural decisions, not narrow shims. `-D
     CRTGFX_ENABLE_SKIA=ON` was reverted back to the documented default
     `OFF` afterward (`crtgfx_skia_raster_smoke` is a plain, non-EXCLUDE_
     FROM_ALL executable target -- leaving it ON would have broken the
     default `cmake --build`/`ctest` workflow for anyone touching this
     tree next). Both real fixes above (`_NO_CRT_STDIO_INLINE`, the
     `target_compile_options` isystem-ordering fix) are kept regardless
     (real, permanent, harmless under the default `CRTGFX_ENABLE_SKIA=
     OFF`) -- confirmed via a full default-config Windows regression run
     after reverting: **100% tests passed, 120/120, zero regression.**

- **Skia's CPU-raster archive (`libskcms.a`/`libskia.a`) now builds clean on
  Windows via `cmake --build --target crtgfx-skia-build`, matching what
  already worked on macOS/Linux arm64.** Direct follow-up to the Windows
  imported-libc++ completion just below: with a real Windows `libc++.dll`
  finally in place, retrying the Skia build for the first time since
  surfaced five more real, distinct bugs, fixed the same evidence-based way
  as everything else this session:
  1. **`python3` unresolvable inside Skia's own generated ninja rules**
     (`gn/toolchain/BUILD.gn`'s `gcc_like_toolchain` template hardcodes the
     bare token `python3` into `tool("alink")`'s pre-archive `rm.py`
     cleanup and `tool("copy")`/`tool("copy_bundle_data")`'s `cp.py`, none
     of it routed through GN's own `script_executable` the way `gn gen`'s
     `exec_script()` calls are). `tools/build_skia.py`'s own
     `env["PATH"]` is deliberately kept pure POSIX/rootfs-relative for
     `crt-cc.cmd`/`crt-c++.cmd`'s own internal mksh needs, which a native
     `cmd.exe` command-name lookup cannot parse as a real search path at
     all -- confirmed for real: `'python3'은(는) 내부 또는 외부 명령...
     아닙니다`. New `pin_gcc_toolchain_python()` (mirroring the already-
     established `pin_gn_script_executable()`) patches the fetched
     toolchain file's own bare `python3` tokens to an absolute path.
     First attempt wrapped the substituted path in GN-string-escaped
     quotes -- syntactically valid GN, but it tripped a real, documented
     `cmd.exe /c` quirk: with more than one quoted segment on the command
     line, `cmd.exe` strips only the very first and very last quote
     character of the *whole* line rather than leaving each quoted
     segment intact, corrupting the command ("지정된 이름, 디렉터리 이름
     또는 볼륨 레이블 구문이 틀렸습니다"). Fixed by reusing this file's
     own already-established `windows_short_path()` (an 8.3 short path
     never contains spaces, so it needs no quoting at all -- inserted
     bare, exactly matching the original unquoted `python3` token's own
     shape) instead of quoting a long-form path.
  2. **`<direct.h>` file-not-found** compiling Skia's own
     `src/ports/SkOSFile_stdio.cpp` (`_mkdir`, its only real use from that
     header -- confirmed by grepping the whole file for every other real
     `<direct.h>`-family function first). New
     `libstdc++/third_party/win32_shim/direct.h`, a thin inline wrapper
     around this project's own real `mkdir(path, 0777)`.
  3. **win32_shim itself was never on Skia's own include path at all** --
     confirmed via the `<direct.h>`/`<io.h>` failures happening in the
     first place, even though both already existed for the imported
     libc++ recipe's own separate build. `SkOSFile_stdio.cpp`'s own
     `#ifdef _WIN32` branch is a raw compiler-native check (not routed
     through Skia's own `SK_BUILD_FOR_*` abstraction the `target_os =
     "linux"` trick already redirects), so it correctly reflects this
     project's real `x86_64-w64-mingw32` target regardless of GN's own
     `target_os` string, and genuinely needs real Windows-shaped headers.
     `default_gn_args()`'s own `extra_cflags` now adds
     `-I<repo>/libstdc++/third_party/win32_shim` for `target_os ==
     "windows"`, reusing the existing shim directory rather than
     duplicating it.
  4. **`_wfopen` undeclared** in the same file (`SkOSFile_stdio.cpp`'s own
     wide-path `fopen`, a real MSVC-CRT function distinct from `_wopen`
     already in `win32_shim/io.h` -- takes a wide *mode string*, not an
     `int` flags bitmask, and returns `FILE*`, not an fd). Confirmed for
     real that `wchar_t` is genuinely 2 bytes (UTF-16) for this project's
     own real target (`__SIZEOF_WCHAR_T__ == 2`), matching what Skia's own
     caller already assumes (it builds a UTF-16 code-unit buffer and casts
     it straight to `wchar_t*`). Implemented as a real `_wfopen()` in
     `win32_shim/io.h`, converting both the wide path and wide mode string
     to narrow via `wcstombs()` (the same conversion `_wopen()` already
     used) before calling this project's own real `fopen()`.
  5. **`crt-ar: neither CRT_HOST_AR, llvm-ar, nor ar was found`**, then,
     once that was fixed, a real parsing bug in `tools/crt-ar` itself.
     `tools/build_skia.py` already pre-resolves `CRT_HOST_CC`/
     `CRT_HOST_CXX` via `shutil.which()` (using this *script's* own
     unrestricted PATH) before overwriting `env["PATH"]` to its POSIX-only
     form, for exactly this class of problem -- `CRT_HOST_AR` needed the
     same treatment (added, no `windows_short_path()` needed this time:
     confirmed by reading `crt-ar.cmd`, it never routes through mksh.exe
     at all, unlike `crt-cc.cmd`/`crt-c++.cmd`). With that fixed, `crt-ar`
     found `llvm-ar` but then failed archiving with a real, separate bug:
     `tools/crt-ar`'s own Windows-specific `@response-file` expansion
     assumed one path per line (`line.strip().strip('"')` per line), but
     confirmed for real by reading the actual generated `.rsp` file that
     Skia's own `gcc_like_toolchain` `tool("alink")` rule
     (`rspfile_content = "{{inputs}}"`) instead emits every object path
     space-separated on a single line -- the old code treated that whole
     line as one argument, so `llvm-ar` received one nonexistent path with
     an embedded space ("no such file or directory") instead of two real
     ones. Fixed with `shlex.split(contents, posix=False)` (tokenizes on
     whitespace across both the single-line and one-per-line shapes alike,
     while leaving backslashes alone -- `posix=True` would otherwise treat
     `\` as an escape character and corrupt real Windows-style paths).
  All five fixes are scoped entirely to files/paths only ever reached
  while actually building Skia (`tools/build_skia.py`, `tools/crt-ar`,
  `libstdc++/third_party/win32_shim/`) -- none of them touch the default
  CMake build graph at all (confirmed: `tools/crt-ar` has exactly one
  consumer project-wide, `tools/build_skia.py`), so no separate regression
  run was needed. Verified end to end: `cmake --build --preset
  windows-host-ninja-debug --target crtgfx-skia-build` exits 0, both
  `libskcms.a` and `libskia.a` link, and Skia installs into
  `external/skia/install`.

- **Completed the current Windows imported-libc++ build migration.** A fresh
  sparse checkout of the pinned LLVM source now builds `libunwind`,
  `libc++abi`, and `libc++` as both static and shared Windows runtime
  libraries, stages all eight libraries into the CRT sysroot, and passes
  both static and shared `crt-libcxx-smoke` executions (`imported_libcxx_test:
  ok`).  The final porting loop closed several real CRT-boundary gaps without
  importing UCRT: sparse checkout is reapplied after SHA checkout; the
  project-owned Win32 shim supplies the narrow Kernel32/filesystem surface
  libc++ actually uses; `_wopen`/`_close` adapt to the CRT fd/path boundary;
  the accidental POSIX `sendfile` fast path is disabled on Windows; and the
  runtime link uses the separately-built libc++abi import library plus the
  compiler-owned compiler-rt builtins archive.  `__security_cookie` and
  `__security_check_cookie` now live in the CRT Windows compiler-ABI object,
  rather than leaking a MSVC startup runtime dependency.  Final verification:
  `crt-libcxx-build`, `crt-libcxx-sysroot`, and `crt-libcxx-smoke` succeeded,
  followed by the default Windows `ctest` suite, **120/120 passed**.

## 2026-08-22

- **Fixed Linux aarch64 `crt-libcxx-smoke`'s shared leg: `tools/crt-cc`
  never gated its own C++ runtime link the way `tools/crt-c++` already
  did, leaking a spurious `libc++.so` dependency into `libunwind.so`.**
  Reported directly ("linux amd64에서는 libcxx 빌드가 성공했는데, 여기
  (linux arm64)에서는 libcxx smoke test에서 다음과 같은 에러가 난다"),
  with the run failing at `imported_libcxx_test`'s shared leg:
  `ld: warning: libc++.so, needed by .../libunwind.so, not found` at
  link time, then `error while loading shared libraries: .../libc++.so:
  file too short` at run time.

  Root cause, confirmed via `readelf -d libunwind.so` and by capturing
  libunwind's own real link command (`ninja -t commands`): `libunwind.so`
  carried a genuine `DT_NEEDED libc++.so` entry it has no real reason to
  need (libunwind implements the Itanium `_Unwind_*` C ABI, no C++
  standard library dependency). LLVM libunwind's own `CMakeLists.txt`
  drives its final link through `CMAKE_C_COMPILER` even though it has
  `.cpp` sources, so `libunwind.so` itself links via `tools/crt-cc` (the
  plain-C wrapper), not `tools/crt-c++`. `tools/crt-c++` already gates
  its own C++ runtime library selection behind
  `CRT_CXX_BUILDING_RUNTIME` (set globally by `crt-libcxx-build.py` for
  every recipe, so libcxx/libcxxabi/libunwind never link back against
  themselves mid-build) -- but `tools/crt-cc` never had the equivalent
  gate at all: its Linux/macOS/Windows branches unconditionally appended
  `libc++.a`/`libc++.so`/`libc++.dylib`/`c++.lib` to every link,
  regardless of `CRT_CXX_BUILDING_RUNTIME`. Harmless as long as
  `libc++.so` itself was a real ELF file or a plain symlink (the spurious
  dependency still resolved at both link and load time), which is
  presumably why this had gone unnoticed through the 2026-08-21 session's
  own static+shared verification -- but genuinely broken once real
  upstream LLVM libcxx's own CMake started emitting `libc++.so` as a
  GNU-ld `INPUT(libc++.so.1 -lc++abi -lunwind)` linker script instead
  (real, current LLVM behavior -- Debian/Ubuntu's own `libc++-dev`
  packages ship the identical construct, meant only for the *static
  linker's* own link-time convenience). A linker script has no ELF
  `SONAME` for `ld` to read, so `ld` fell back to recording the literal
  argument text `libc++.so` as `libunwind.so`'s own `DT_NEEDED` -- and
  the *dynamic loader*, unlike the static linker, has no concept of
  `INPUT()` scripts at all, so it tried to `mmap()` the 37-byte text file
  as an ELF image and failed with "file too short".

  Fixed by adding the same `CRT_CXX_BUILDING_RUNTIME` gate `tools/crt-c++`
  already had to `tools/crt-cc`, factored into one shared `cxx_runtime_lib`
  variable computed once up front (empty while building the runtime
  itself; the correct static/shared archive or import library otherwise)
  and referenced from all three OS branches, matching `tools/crt-c++`'s
  own existing scope -- not just the Linux branch this specific bug
  surfaced on: the identical unconditional-libc++-link shape existed on
  macOS and Windows too, just never triggered there (macOS's own
  `libunwind` recipe is deliberately excluded via `target_os`, and
  Windows' `crt-libcxx-build` has its own separate, still-open gate
  blocking it from reaching this link at all -- see TODO.md).

  Verified for real on a genuinely fresh build (wiped `external/
  llvm-runtimes/{build,install}` entirely, not an incremental one):
  `readelf -d libunwind.so` now shows only `libdl.so`/`libc.so`/
  `libm.so`, no `libc++.so`; both `crt-libcxx-smoke` legs build, link,
  and run to completion again -- `imported_libcxx_test: ok` on static
  and shared. Full `cmake --build --preset linux-host-ninja-debug` +
  `ctest` (104/104) confirm no regression.

- **Fixed macOS `crt-libcxx-build`/`crt-libcxx-smoke` after the libcxx/
  libcxxabi migration off the dead Android fork (two real, separate
  bugs)**, closing the gap the migration's own commit left open (that
  work verified Linux fully and Windows partially, but never macOS).
  Reported directly ("linux에서 libcxx 빌드까지 성공했는데, 이 기기
  (macos)에서 에러가 난다"). Reproduced on a genuinely fresh build
  (wiped `external/llvm-runtimes` entirely -- the old fork's checkout
  and every build/install artifact under it were completely stale
  against the new `toolchain/llvm-project`-sourced recipe).

  **Bug 1**: `libcxxabi`'s build failed compiling `stdlib_stdexcept.cpp`
  with `fatal error: 'mach-o/dyld.h' file not found`, via `libcxx`'s own
  private `src/include/refstring.h` (reached through libcxxabi's
  `../libcxx/` relative include). Root cause: that header's
  `_LIBCPP_CHECK_FOR_GCC_EMPTY_STRING_STORAGE` block (real-host-
  libstdc++-interop code, checking whether an exception's stored string
  is libstdc++'s own empty-string singleton) is guarded by `#if
  defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) ||
  defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__)` in this new,
  actively-maintained source -- unlike the dead fork's own pre-migration
  copy, which used a plain `#ifdef __APPLE__` this recipe's `-U__APPLE__`
  flag already neutralized. These two environment macros are predefined
  by Clang's own driver directly from the real `arm64-apple-macosx`
  target triple (this recipe passes no explicit `--target=`, so Clang
  defaults to the real host triple) -- completely unaffected by
  `-U__APPLE__`, which only undefines that one literal macro. Once that
  branch is taken, `tools/crt-c++`'s own unconditional `-nostdinc` (even
  for `CRT_CXX_BUILDING_RUNTIME=1`, building this runtime itself, by
  design -- correct and necessary for every other translation unit in
  this build) makes the real macOS SDK's own `<mach-o/dyld.h>`
  unreachable. Fixed by disabling the whole feature in
  `libstdc++/third_party/libcxx/recipe.json` (a `patches` entry turning
  the `#if` into `#if 0`), not just the one `#include` -- this project
  never links a host libstdc++ into the same process as its own
  independent libc++abi in the first place, so the ABI-interop this code
  exists for cannot occur here regardless of headers.

  **Bug 2**, found immediately after Bug 1 while re-verifying
  `crt-libcxx-smoke`: `tools/test_libcxx_runtime.py` failed every
  invocation with `error: unrecognized arguments: --host-cc ... --host-cxx
  ...`. Root cause: the same day's earlier `--host-cc`/`--host-cxx`
  addition (passing the top-level CMake configure's own
  `CMAKE_C_COMPILER`/`CMAKE_CXX_COMPILER` through to the recipe bootstrap,
  fixing a real "silently built with the wrong/too-old host compiler"
  class of bug) went into a *shared* `CRT_LIBCXX_PLATFORM_ARGUMENTS` CMake
  list that `crt-libcxx-smoke`'s own custom target also consumes --
  correct for `crt-libcxx-configure`/`crt-libcxx-build` (which invoke
  `tools/crt-libcxx-build.py`, whose argparse accepts both flags), but
  `crt-libcxx-smoke` invokes a *different* script
  (`tools/test_libcxx_runtime.py`) that was never updated to accept them.
  Fixed by adding matching `--host-cc`/`--host-cxx` arguments to
  `test_libcxx_runtime.py`'s own argparse (`--host-cc` accepted but
  unused -- this script only ever compiles a C++ client through
  `tools/crt-c++`, never a plain C translation unit; `--host-cxx` now
  takes precedence over this script's own `CRT_HOST_CXX` env-var/
  `shutil.which()` fallback chain for its `-fno-typed-cxx-new-delete`
  feature probe and, on Windows, its `CRT_HOST_CXX` export for
  `tools/crt-c++`), matching `tools/crt-libcxx-build.py`'s own existing
  precedence order for the identical flags.

  Verified on this real macOS aarch64 host, genuinely fresh at every
  step (no stale `external/llvm-runtimes` artifacts from the pre-
  migration fork involved): `crt-libcxx-build` exits 0, `crt-libcxx-
  sysroot` stages cleanly, and `crt-libcxx-smoke` now passes both linkage
  legs -- `imported_libcxx_test: ok` for both the static and shared
  builds, matching Linux's own already-complete verification. Default
  `cmake --build`/`ctest` (`CRT_USE_IMPORTED_LIBCXX` stays OFF by default,
  so this whole area is normally dormant) stays green at 104/104
  throughout, no regressions.

- **Migrated `libcxx`/`libcxxabi`'s recipe source off the dead Android forks
  onto the live `toolchain/llvm-project` monorepo, fixing the C++20 `<bit>`
  gap that blocked Skia's GN build -- fully verified on Linux/WSL (100%
  tests passed, 104/104), substantial real progress on Windows with the
  remaining gap clearly scoped and deferred by explicit user decision.**
  Direct continuation of this same day's earlier `<inttypes.h>` entry below:
  confirmed `platform/external/libcxx`/`platform/external/libcxxabi` (the
  two recipes' own `source.repository` at the time) are dead -- their
  `refs/heads/main` tips are the exact commits already pinned, and the tip
  commit's own message is a 2024-12-20 placeholder ("Empty merge ... into
  aosp-main-future") with no real content change, confirmed by direct
  inspection. Root cause of the `<bit>` gap: that frozen fork's own
  `include/bit` is only 158 lines, with the pre-existing internal
  `__popcount()` helpers but no `_LIBCPP_STD_VER` gating and no public
  `std::countl_zero`/`countr_zero`/`popcount` at all. Moved both recipes to
  `toolchain/llvm-project`'s own `libcxx`/`libcxxabi` subtrees at the exact
  same pinned commit `libunwind/recipe.json` already used
  (`37f38d1f3276b62fba09462ab4807dce846c732d`) -- confirmed that commit's
  real `libcxx/include/bit` has `_LIBCPP_STD_VER >= 20`/`>= 23` gating and a
  real `countl_zero` declaration, by fetching and reading it directly before
  migrating, not assuming.

  **The migration meant discarding the old recipes' own patches** (written
  against the dead fork's exact file layout, which the new source does not
  share at all) and rediscovering their equivalents one real build error at
  a time, exactly the same evidence-based discipline as every other fix
  this session -- fetch the real pinned-commit source via a scratch probe
  clone (`git fetch --filter=blob:none --depth 1 <ref>` + `git ls-tree`/
  `cat-file -p` for individual blobs) before writing any patch, never guess
  from memory. In order encountered:
  - **CMake structural fixes**: `libcxx/CMakeLists.txt` and
    `libcxxabi/CMakeLists.txt` at this commit no longer call `project()`
    themselves at all (confirmed via `grep -c '^project('` -> 0 on both),
    needing the same driver-`CMakeLists.txt` treatment `libunwind`'s own
    recipe already established (`libstdc++/third_party/{libcxx,libcxxabi}/
    standalone/CMakeLists.txt`, new files). `libcxxabi`'s own driver must
    `add_subdirectory()` libcxx too (for its `cxx-headers` interface
    target, needed by `cxxabi_shared_objects`'s own
    `target_link_libraries()`) -- and in a specific order (`libcxxabi`
    first, `libcxx` second): libcxx's own `HandleLibCXXABI.cmake` does an
    immediately-evaluated `if (TARGET cxxabi_shared)` check (not a deferred
    generator expression), so add_subdirectory()-ing libcxx first left that
    check false, surfacing much later as `CMake Error ... Target
    "libcxx-abi-shared" not found`.
  - **`LIBCXX_CXX_ABI=system-libcxxabi`** (was `libcxxabi`, the in-tree
    default): the in-tree value assumes an in-tree `cxxabi_shared`/
    `cxxabi_static` CMake target exists in the *same* configure, which this
    project's two independently-configured recipes never have; the
    path-based `system-libcxxabi` value (pointed at
    `LIBCXX_CXX_ABI_INCLUDE_PATHS`/`LIBCXX_CXX_ABI_LIBRARY_PATH`) matches
    how the two recipes actually relate.
  - **Five bounded `libc/` subtree fetches**, each added only after a real
    `fatal error: '<path>' file not found` traced to it, and each checked
    for size/scope before fetching (never the 2814-file `libc/src/` proper,
    the real OS-specific llvm-libc implementation this project has no use
    for, having its own complete libc): `libc/shared` (`from_chars_floating_
    point.h`'s own `#include "shared/fp_bits.h"`, for `charconv.cpp`),
    `libc/src/__support` (`fp_bits.h`'s own further `#include "src/__support/
    FPUtil/FPBits.h"` -- llvm-libc's deliberately OS-independent low-level
    utility layer, 319 files), `libc/hdr` (`FPBits.h`'s own `#include "hdr/
    limits_macros.h"` -- llvm-libc's own "overlay mode" escape hatch,
    `#ifdef LIBC_FULL_BUILD ... #else #include <limits.h> #endif`, which
    resolves to this project's own real header since `LIBC_FULL_BUILD` is
    never defined here; confirmed the same shape on `errno_macros.h` and
    one `libc/hdr/func/*.h` proxy before generalizing), `libc/include`
    (`types.h`'s own unconditional `#include "include/llvm-libc-macros/
    float16-macros.h"`, 304 files, fetched whole to front-load the likely
    next miss), and `libc/src/errno` (`str_to_integer.h`'s own `#include
    "src/errno/libc_errno.h"`, just 3 files).
  - **`__dso_handle`/`generate-cxx-headers`/Python3 wiring**: both recipes'
    own `extra_files`-written `src/__crt_dso_handle.cpp` shim needed
    re-wiring into `set(LIBCXX_SOURCES ...)`/`set(LIBCXXABI_SOURCES ...)`
    (the new source's own file lists never reference it, unlike the old
    fork's) -- confirmed for real via `undefined reference to
    '__dso_handle'` linking both shared libraries. Separately,
    `libcxx/include/CMakeLists.txt`'s own `generate-cxx-headers` custom
    command (produces the installed `libcxx.imp` IWYU-mapping file) reads
    `${Python3_EXECUTABLE}` without either recipe's own standalone driver
    ever calling `find_package(Python3)` -- silently no-ops on Linux (a
    bare `cd <dir>` with nothing after it, confirmed via `ninja -v`; POSIX
    happily "succeeds" running just `cd`) but fails hard on Windows
    (`CreateProcess failed: The system cannot find the file specified`).
    Fixed by adding `find_package(Python3 REQUIRED COMPONENTS Interpreter)`
    to both standalone drivers, and adding the `generate-cxx-headers`
    target explicitly to `libcxx/recipe.json`'s own `build_targets` (a
    plain `--target cxx --target cxx_experimental` never pulled it in on
    its own, confirmed via `cmake --install` failing with `file INSTALL
    cannot find .../libcxx.imp`).
  - **Two real, general libc completeness gaps**, fixed as real additions
    to this project's own code (not workarounds): `include/linux/futex.h`
    (new file, matching the existing `include/linux/{auxvec,limits,random,
    types,version}.h` siblings) + `SYS_futex` in `include/sys/syscall.h`,
    for `atomic.cpp`'s own `_LIBCPP_FUTEX(...)` wait/wake macro under
    `__linux__` (values confirmed against the WSL host's own real
    `/usr/include/linux/futex.h` and `/usr/include/asm-generic/unistd.h`/
    `unistd_64.h`, not assumed); and `O_NOFOLLOW` added to `include/
    fcntl.h` (already had every other `O_*` flag but this one), for
    `filesystem/operations.cpp`.

  **Linux/WSL: fully verified.** Full default `cmake --build --preset
  linux-host-ninja-debug` + `ctest` with `CRT_USE_IMPORTED_LIBCXX=ON`, from
  a fully wiped `external/llvm-runtimes/build/{libcxx,libcxxabi}` tree (not
  an incrementally-patched one) -- **100% tests passed, 0 failed, out of
  104**. This is the same environment/distro-migration session described in
  the entry below (`Ubuntu-20.04` -> `Ubuntu-26.04` mid-session, WSL clone
  re-established fresh).

  **Windows: six more real, distinct bugs found and fixed the same
  evidence-based way, then a clear, deliberate stop.** Using a separate,
  isolated build directory (`out/windows-imported-libcxx`, `-
  DCRT_USE_IMPORTED_LIBCXX=ON`) to avoid disturbing the existing default
  (`OFF`) Windows build during iteration:
  1. A Windows-only ordering constraint (not a migration bug):
     `tools/crt-libcxx-build.py`'s own configure phase needs `mksh.exe`
     already staged in the rootfs to run `crt-cc.cmd`/`crt-c++.cmd`, but
     `CRT_USE_IMPORTED_LIBCXX=ON` makes `rootfs` itself depend on
     `crt-libcxx-sysroot` (for the final `c++.dll` copy) -- a real,
     pre-existing chicken-and-egg cycle the project's own `CMakeLists.txt`
     already documents and avoids reversing. Worked around for this
     verification pass only by manually bootstrapping a minimal rootfs
     (`tools/create_rootfs.py` run by hand with just `mksh`/`toybox`/the
     three non-libcxx runtime libraries) before letting CMake take over.
  2. `_tls_index` undefined at link time building `libc++abi.dll`:
     `cxa_exception_storage.cpp`'s own `__has_feature(cxx_thread_local)`
     branch (always true for Clang, a language feature not a platform
     capability) declares a real `static thread_local __cxa_eh_globals`,
     which on Windows PE lowers to code referencing the native
     `_tls_index`/`_tls_used` TLS-directory symbols a real CRT's
     `tlssup.obj` normally provides -- this project's own `crt1.o` has none
     (confirmed by reading `libc/src/tls.c`/`libc/include/private/
     crt_tls.h`: this project implements its own `TlsAlloc`-based dynamic
     TLS instead of relying on compiler-native PE TLS anywhere). Scope-
     checked (grepped all of `libcxx/src` and `libcxxabi/src` for
     `thread_local` -- only this one real, unconditional declaration; the
     other four hits already route through this project's own
     already-working `__libcpp_tls_key`/`__thread_specific_ptr`
     abstractions) before patching this file's own branch selector to
     exclude `_WIN32`, falling through to its own already-correct
     pthread-key-based fallback branch.
  3. `_LIBCPP_MSVCRT_LIKE` defined unconditionally for any `_WIN32` target
     ("Both MinGW and native MSVC provide a MSVC-like environment" --
     false for this project's own independent libc): wrongly selected
     `__locale_dir/support/windows.h`'s `_locale_t`/`_create_locale`/
     `_free_locale` locale backend instead of the POSIX-shaped `locale_t`/
     `newlocale`/`freelocale`/`uselocale` this project's own `include/
     locale.h` already provides (the same shape Android/Bionic's own real
     `locale.h` uses, which is also why `locale_base_api.h` already has its
     own separate, correct `__ANDROID__` branch for this exact situation).
     Patched `include/__config`'s (and its `__cxx03/` twin's) own
     `_LIBCPP_MSVCRT_LIKE` `#define` to exclude `__BIONIC__`, leaving the
     block's other genuinely-still-correct Windows facts
     (`_LIBCPP_WIN32API`/`_LIBCPP_SHORT_WCHAR`/`_LIBCPP_HAS_OPEN_WITH_WCHAR`/
     `_LIBCPP_HAS_BITSCAN64`) untouched.
  4. Four rounds of missing Win32 declarations, each resolved by extending
     `libstdc++/third_party/win32_shim/` (the small, project-owned,
     "real declarations for exactly what's used, never a full SDK header"
     precedent `libunwind`'s own Windows needs already established) rather
     than reaching for the real SDK: `UINT`/`AreFileApisANSI`/
     `WideCharToMultiByte`/`MultiByteToWideChar`/`CP_ACP`/`CP_OEMCP`/
     `MB_ERR_INVALID_CHARS` for `filesystem/path.cpp`'s narrow<->wide path
     conversions (confirmed real kernel32.dll exports via `llvm-objdump -p`
     on this machine's own `kernel32.dll`); `LARGE_INTEGER`'s own
     `LowPart`/`HighPart` fields (the existing shim only had `QuadPart`,
     enough for `chrono.cpp` but not `filesystem/time_utils.h`'s own
     FILETIME<->LARGE_INTEGER split/merge); a new `win32_shim/winerror.h`
     (49 real `ERROR_*` System Error Code constants `system_error.cpp`
     references, values read directly from this machine's own real SDK
     `Windows Kits\10\Include\...\shared\winerror.h` rather than recalled
     from memory, even though that real header is not itself reachable
     from this recipe's own isolated include path); and redirecting
     `<print>`'s own terminal-detection dispatch (`include/print` +
     `src/print.cpp`, patched together since the header declares and the
     `.cpp` defines in lockstep) to its already-portable, already-working
     `isatty()`-based path, since `_get_osfhandle()` needs this project's
     own internal fd->HANDLE table (a private `static get_fd_handle()` in
     `libc/src/arch/windows/common/syscall.c`, not worth exposing as new
     public API just to feed an MSVC-CRT-shaped shim) -- accepted losing
     `std::print`'s native `WriteConsoleW` fast path as an honest,
     documented degradation to plain byte-oriented `fwrite()`.
  5. Hit a **new, deeper class of blocker** after all six fixes:
     `libcxx/src/CMakeLists.txt` unconditionally compiles `support/win32/
     locale_win32.cpp` and `support/win32/support.cpp` into
     `LIBCXX_SOURCES` for any Windows target (a CMake-level file-list
     selection, not gated by any of the preprocessor macros patched
     above), and both call real MSVC-CRT-only functions this project's
     own libc has no equivalent of at all: `_create_locale`/`_free_locale`,
     `wcrtomb_s`, `errno_t`, `rand_s`. `fstream.cpp` (`_wfopen`) reaches
     similar territory. Fixing this properly needs a CMake-level patch
     dropping/replacing those specific files, not another header shim --
     recognized as a distinctly bigger, open-ended scope than the six
     fixes above.
  6. **Asked the user how to proceed, given the honest scope jump** (a
     concrete `AskUserQuestion`, not a unilateral judgment call) --
     explicit answer: stop the Windows push here, since Linux's already-
     complete verification (100%, 104/104) already covers the actual
     motivating `<bit>` gap. All six real Windows fixes above are kept
     (each is dormant/inert under the default `CRT_USE_IMPORTED_LIBCXX=OFF`
     -- confirmed via a full default-config Windows regression run,
     `cmake --build --preset windows-host-ninja-debug` + `ctest`, **100%
     tests passed, 120/120, zero regression**), `CRT_USE_IMPORTED_LIBCXX=ON`
     on Windows tracked as open follow-up work in `TODO.md`, with the exact
     next step (CMake-level source-list patch for `support/win32/
     {locale_win32,support}.cpp`, then `fstream.cpp`/`random.cpp`) already
     scoped so a future session does not have to re-derive it.

- **Verified via a real WSL/Ubuntu-20.04 attempt that a Linux build of the
  Skia GN item reaches the same two library-completeness gaps Windows hit,
  with zero toolchain-wiring fixes needed to get there -- then fixed one
  of the two gaps for real (`<inttypes.h>`'s missing `imaxdiv_t`/
  `imaxabs`/`imaxdiv`/`wcstoimax`/`wcstoumax`), verified clean on both
  hosts.** Direct follow-up to the user's own question ("WSL로 리눅스
  빌드 한번 시도해보자") after the Windows-only Skia toolchain-wiring
  pass earlier this same day.

  **WSL setup, and one real, environment-specific finding along the way.**
  Cloned this repo fresh into WSL's own native filesystem (`~/crt`, not
  the `/mnt/c/...` Windows mount) -- confirmed necessary for real: the
  Windows-mounted copy has CRLF line endings (this machine's
  `core.autocrlf=true`; the repository's own stored blobs are LF-only,
  confirmed via `git show HEAD:tools/crt-cc`), which breaks every
  `#!/bin/sh` shebang script's own interpreter line on Linux. Installed
  `clang-18`/`libc++-18-dev`/`libc++abi-18-dev` (the user's own doing,
  after `sudo` needing a password blocked doing it directly) to match
  this project's own `CMAKE_CXX_FLAGS="-stdlib=libc++"` requirement on
  Linux (`CMakeLists.txt`'s own comment: a stock Linux Clang defaults to
  GNU libstdc++, not libc++, unlike Apple Clang). The base project (no
  Skia) built and passed 104/104 `ctest` on the first real attempt, no
  fixes needed at all.

  Fetching `libstdc++/third_party/libunwind/recipe.json` (sparse-checked-
  out from the full `toolchain/llvm-project` monorepo, `--filter=
  blob:none --depth 1` before the sparse-checkout that's supposed to
  narrow it) then ballooned to 3.6GB+ over many minutes before failing
  outright with `error: RPC failed; HTTP 502`, instead of the "tens of
  MB" that exact same fetch already reliably produces on Windows.
  Root-caused to WSL/Ubuntu 20.04's own default `apt` git (`2.25.1`, a
  2020-era release, versus this machine's own `git 2.55.0` on Windows) --
  confirmed by isolating the variable: after the user upgraded WSL's git
  to `2.50.1` via the official `ppa:git-core/ppa`, the identical fetch
  (same recipe, same commit, same sparse paths) completed in seconds at
  5.7MB. An old git's partial-clone (`--filter=blob:none`) negotiation
  against a JGit/Gerrit backend (both `android.googlesource.com` and
  `skia.googlesource.com` run this) is real, verified evidence of being
  far less efficient than a modern git's for this exact kind of fetch --
  not a bug in `tools/crt-libcxx-build.py`'s or `tools/fetch_skia.py`'s
  own fetch logic, which already worked correctly once the git version
  was current. Worth checking for on any other older-Linux-distro host
  this project's recipes get run against for the first time.

  **Skia GN build on Linux: zero toolchain-wiring fixes needed, landed on
  the same two gaps Windows did.** With a working modern git, the full
  imported-libc++ (`CRT_USE_IMPORTED_LIBCXX=ON`) build succeeded end to
  end (104/104 `ctest`), and `crtgfx-skia-fetch`/`-build` ran cleanly
  through `gn gen` and into real compilation at 26/544 ninja steps --
  none of the eight Windows-specific fixes from earlier the same day
  (GN toolchain hardcoding, `mksh.exe` launcher/PATH/cwd-bootstrap
  issues, `--target-arch` spelling, the `SK_BUILD_FOR_WIN`/`__forceinline`
  macro fix) were needed at all, confirming each one really was
  Windows-specific or a consequence of Windows being unable to exec a
  `#!/bin/sh` script directly. The build then hit exactly the two gaps
  TODO.md's own dated entry already predicted from earlier that day:
  `SkMathPriv.h`'s `std::countl_zero`/`countr_zero`/`popcount` (C++20
  `<bit>`) and `<cinttypes>`'s `imaxdiv_t`/`imaxabs`/`imaxdiv`/
  `wcstoimax`/`wcstoumax` -- verifying the earlier same-day projection
  ("a Linux attempt would reach the same wall, just faster") for real
  rather than leaving it as a reasoned guess.

  **The `<inttypes.h>` gap, fixed for real.** Before fixing anything, the
  user asked which of three possible approaches this actually was:
  bumping the libc++ pin, patching a header in around the gap, or fixing
  libc directly. Answered by investigating both gaps concretely first
  (see the *separate* `<bit>` investigation, still open, in this same
  day's other dated entry/TODO.md bullet) -- the `<cinttypes>` gap turned
  out to be entirely independent of the libc++ pin question: it is this
  project's *own* `include/inttypes.h`/`libc/src/inttypes.c` that were
  missing real declarations, not anything libcxx itself ships, so it was
  fixed directly rather than patched around, matching this project's own
  porting-loop discipline (`AGENTS.md`). Added `imaxdiv_t`/`imaxabs()`/
  `imaxdiv()` (implemented directly in terms of `intmax_t`'s own
  truncating division/modulo -- C99-defined semantics, exactly matching
  `div_t`/`ldiv_t`/`lldiv_t` -- rather than delegated to `ldiv()`/
  `lldiv()`, since a real compiler probe confirmed `intmax_t` is `long`
  on Linux/macOS but `long long` on this project's own
  `--target=x86_64-w64-mingw32` Windows target, so guessing which one to
  delegate to would have been wrong on one platform or the other) and
  `wcstoimax()`/`wcstoumax()` (thin wrappers over the already-existing
  `wcstoll()`/`wcstoull()`, matching how `strtoimax()`/`strtoumax()`
  already wrapped `strtoll()`/`strtoull()` in the same file).
  `include/inttypes.h` also gained an `#include <stddef.h>` (for
  `wchar_t`, needed by the two new wide-character declarations --
  confirmed real POSIX/glibc headers declare these in `<inttypes.h>`,
  not `<wchar.h>`, despite the wide parameter type, and `<stddef.h>`
  alone is enough rather than the full `<wchar.h>`, avoiding a heavier/
  circular include).

  Verified on both hosts, not just one: full default `ctest` still
  120/120 on Windows and 104/104 on Linux/WSL after the fix (zero
  regression), and a fresh Skia GN rebuild on WSL confirmed the fix
  worked as intended -- the failure surface shrank to exactly one
  object file (`SkMathPriv.o`), and every remaining compile error traces
  to the still-open `<bit>` gap alone, confirmed via direct log
  inspection rather than assumed. The `<bit>` gap itself (migrating
  libcxx/libcxxabi's own recipe source from the stale, frozen `platform/
  external/libcxx`/`libcxxabi` Android mirrors -- confirmed dead: its
  `refs/heads/main` tip is the exact commit already pinned, and that
  commit's own message is `"Empty merge ab/12770256 into aosp-main-
  future"` -- to `toolchain/llvm-project`'s own actively-maintained
  `libcxx`/`libcxxabi` subtrees, matching what `libunwind`'s recipe
  already correctly does, and re-verifying/porting the existing
  `_LIBCPP_MSVCRT_LIKE`-family patches against the different source
  tree) remains open, deliberately deferred as a separate, larger
  follow-up.

- **Skia's source pin/sparse-checkout/local-patch record moved into a real
  `recipe.json` (`libcrtgfx/third_party/skia/recipe.json`), matching
  `libstdc++/third_party/{libcxx,libcxxabi,libunwind}/recipe.json`'s own
  shape; a placeholder `recipe.json` was also added for Wayland
  (`libcrtgfx/third_party/wayland/recipe.json`), which has nothing pinned
  or fetched yet.** Prompted by a direct request to give Skia the same
  "recorded SHA-pin + patches" treatment the libcxx recipes already have.
  Skia's build is GN/Ninja-based, not CMake, so the new recipe reuses
  only the *shape* (`source.type`/`repository`/`ref`/`expected_commit`/
  `sparse_paths`/`sync_deps`, plus a `patches` array and a `notes`
  array) -- there is no `cmake.options`/`target_overrides`/
  `build_targets` section, and no attempt to route Skia through
  `tools/crt-libcxx-build.py`'s CMake-specific engine (this project
  already explicitly rejected that once, in
  `libcrtgfx/third_party/skia/README.md`'s original text, for the same
  reason `tools/crt-libcxx-build.py`'s own module docstring rejects
  reusing `tools/crt-port-build.py`'s generic porting-recipe engine).
  `libcrtgfx/CMakeLists.txt` now reads `recipe.json`'s own `source.*`
  fields at configure time via CMake's `string(JSON ...)` (available
  since this project's own `cmake_minimum_required(VERSION 3.25)`,
  comfortably past the 3.19 floor that command needs) to populate the
  `CRTGFX_SKIA_VERSION`/`REF`/`REPOSITORY`/`EXPECTED_COMMIT`/
  `SPARSE_PATHS`/`SYNC_DEPS` CACHE variable *defaults*, replacing the
  previous hardcoded literals -- those six values now live in exactly
  one place instead of two that could silently drift apart. Verified
  for real, not just written and assumed correct: a genuinely fresh
  configure (wiped `CMakeCache.txt`/`CMakeFiles`) produced cache values
  that exactly match `recipe.json`'s own content, and a full default
  `cmake --build` + `ctest` (`CRTGFX_ENABLE_SKIA` left OFF) still passes
  120/120.

  The one real build-configuration edit this project makes to the
  fetched Skia checkout -- `tools/build_skia.py`'s
  `pin_gn_script_executable()`, rewriting the `.gn` dotfile's
  `script_executable = "python3"` line to an absolute path (see this
  same file's earlier entry today for why) -- is now cross-referenced
  from `recipe.json`'s own `patches` array, marked `"dynamic": true`
  since the replacement value (the host's own `sys.executable`) is
  discovered at build time and cannot be recorded as a fixed find/
  replace string the way libcxx's own recipe patches are. The existing
  "do not patch Skia's own upstream *source* to make it build" policy
  (already stated in the README before this change, restated in
  `recipe.json`'s own notes now) is unchanged and was not reconsidered
  -- this recipe's `patches` array is scoped to build-configuration
  files only, a deliberately narrower thing than a real source-code
  patch.

- **Routed Skia's own GN build through the project-owned imported libc++
  instead of real MSVC STL (`tools/build_skia.py`), fixing eight distinct
  real bugs along the way; the final link is still blocked on two
  separate, genuine library-completeness gaps, deliberately left open.**
  Follow-up to 2026-08-21's `crtgfx_skia_raster_smoke.exe` duplicate-
  symbol link-failure diagnosis (see that entry and `TODO.md`'s own
  dated sub-bullet for the fuller root-cause chain): the real fix is to
  stop Skia's own C++ code (and, eventually, `libcrtgfx`'s CMake-native
  Skia targets) from ever reaching real MSVC UCRT/STL headers at all.

  **Bugs found and fixed, in order, each confirmed with direct evidence
  before being fixed (manual `mksh.exe`/PowerShell repros, ninja build
  logs, `llvm-nm`-style inspection) -- never guessed:**
  1. GN's Windows `msvc_toolchain` template hardcodes `cl.exe`/
     `clang-cl.exe` and ignores the top-level `cc`/`cxx` GN args
     entirely (confirmed via the literal `cl : warning D9002: unknown
     option '-isystem...' ignored` in a build log -- real `cl.exe` was
     compiling Skia the whole time). Fixed by setting GN's
     `target_os = "linux"` even on a real Windows build, selecting the
     generic `gcc_like_toolchain` (which does respect `cc`/`cxx`) and
     Skia's own generic POSIX source set -- the exact same trick this
     project's macOS build already used for an analogous reason.
  2. GN's `gcc_like_toolchain` invokes `cc`/`cxx` via
     `subprocess.check_output(shell=True)` and a bare `{{cc}}` ninja
     substitution, neither of which can launch `mksh.exe` as a separate
     leading argument. Fixed by pointing GN's `cc`/`cxx` at the
     pre-existing `tools/crt-cc.cmd`/`tools/crt-c++.cmd` native-Windows
     launchers (built for `tools/crt-libcxx-build.py`'s own CMake
     integration) plus resolving `CRT_HOST_CC`/`CRT_HOST_CXX` via
     `shutil.which()` (mksh's own `$PATH` is POSIX-rooted and has no
     real host compiler on it).
  3. **A genuinely new PAL bug, not previously documented anywhere in
     this project**: `libc/src/env.c`'s `__crt_rootfs_bootstrap()` runs
     at every CRT-libc process's own startup and, whenever `CRT_ROOTFS`
     is not already set in the environment, auto-detects it from
     `argv[0]` (any binary living under `.../rootfs/system/bin/`,
     `bin/`, or `usr/bin/` qualifies -- `mksh.exe` always does) and then
     unconditionally `chdir("/")`s, discarding whatever real cwd the
     parent process (ninja) launched it with. Confirmed for real by
     isolating to a two-line repro run directly via `mksh.exe`: `pwd`
     reported `/` even though `mksh.exe` was launched with cwd already
     set to the Skia GN output directory, and every ninja-driven compile
     failed with "no such file or directory" on its own GN-relative
     source path (e.g. `../../modules/skcms/src/skcms_TransformHsw.cc`)
     as a direct result. Fixed by having `tools/build_skia.py` pre-seed
     `CRT_ROOTFS` itself before `mksh.exe` ever starts (its own first
     check is `if (getenv("CRT_ROOTFS") != 0) return;`), matching what
     `tools/crt-libcxx-build.py` already did, for a different reason,
     for the same variable.
  4. mksh's own `exec()`/command-lookup cannot run a program whose path
     contains a space, forward slashes or not -- a stock Windows LLVM
     install always lands under `"C:\Program Files\LLVM\..."`. Confirmed
     directly: `mksh.exe -c '"/c/Program Files/LLVM/bin/clang++" --version'`
     failed "inaccessible or not found" even with forward slashes (so
     this is not the separate, already-documented "mksh needs forward
     slashes" gotcha), while the identical command using the real 8.3
     short-path form succeeded. Fixed by converting `CRT_HOST_CC`/
     `CRT_HOST_CXX` to their 8.3 short-path form before exporting them
     (`windows_short_path()`, duplicated from `tools/crt-port-build.py`'s
     own helper of the same name, matching this project's existing
     "small helpers duplicated per script" convention rather than adding
     a shared module for one function).
  5. A real PATH-format conflict between two tools invoked from the
     *same* `gn gen` subprocess tree: `gn.exe` itself is a genuinely
     native tool that needs a real, semicolon-delimited Windows `PATH`
     to find `python3` (Skia's own `.gn` dotfile hardcodes
     `script_executable = "python3"`), while `mksh.exe` (invoked
     mid-`gn gen`, since GN's own `is_clang.py` compiler probe shells
     out through `crt-cc.cmd`) needs `PATH` to stay this project's own
     deliberate `:`-separated POSIX form -- `shell/toybox/PATCHES.md`'s
     own `MKSH_CRT_WINPATH` writeup documents that `MKSH_PATHSEPC` stays
     `:` on this project's Windows mksh build by deliberate design, so a
     real Windows directory (itself containing a `:` after the drive
     letter) breaks mksh's own PATH parsing outright if it appears in
     PATH at all -- confirmed for real: even prepending a correctly-
     formatted real-Windows entry pointing directly at the directory
     containing `printf.exe` still failed `printf: inaccessible or not
     found` from inside mksh. No PATH value satisfies both consumers at
     once. Fixed by patching the fetched `.gn` dotfile's own
     `script_executable = "python3"` line to an absolute path
     (`pin_gn_script_executable()`, verified idempotent, replacing the
     previous `ensure_python3_shim()` PATH-prepending approach) so
     `gn.exe` no longer depends on `PATH` for this at all, freeing
     `PATH` to stay pure POSIX (`/system/bin:/bin:/usr/bin`, matching
     `tools/crt-libcxx-build.py`'s own equivalent handling exactly) for
     `mksh.exe`.
  6. `--target-arch` arrives from CMake in `CMAKE_SYSTEM_PROCESSOR`
     spelling (`AMD64`/`ARM64`), which `tools/crt-cc`'s own
     `--target=${target_arch}-w64-mingw32` construction needs in GNU-
     triple spelling (`x86_64`/`aarch64`) -- left unnormalized,
     `CRT_TARGET_ARCH=AMD64` produced `--target=AMD64-w64-mingw32`, not
     a real clang triple, surfacing as `clang++: error: unsupported
     option '-mavx2' for target 'AMD64-w64-mingw32'` (and, for TUs with
     no `-m` flags, the blunter `error: unknown target triple
     'AMD64-w64-windows-gnu'`). Fixed via a new `normalize_target_arch()`
     (mirrors `tools/crt-libcxx-build.py`'s own `detect_target_arch()`),
     applied once right after argument parsing.
  7. clang's own `--target=x86_64-w64-mingw32` predefines `_WIN32`/
     `_WIN64` regardless of GN's own `target_os` GN arg, which Skia's
     `SkFeatures.h` reads as `SK_BUILD_FOR_WIN` -- and `SK_ALWAYS_INLINE`
     (`SkAttributes.h`) expands to the bare MSVC keyword `__forceinline`
     under `SK_BUILD_FOR_WIN`, which this project's mingw-target (never
     `clang-cl`) invocation does not recognize (`error: unknown type
     name '__forceinline'`). Fixed by explicitly defining
     `-DSK_BUILD_FOR_UNIX` for the Windows branch too -- `SkFeatures.h`'s
     own auto-detection is a single `#if !defined(SK_BUILD_FOR_*)` guard
     around the whole block, so defining any one of them up front skips
     the rest entirely. Internally consistent either way, since GN's
     `target_os = "linux"` already selects Skia's generic POSIX *source
     files* (e.g. `SkOSFile_posix.cpp`, not the real Win32
     `SkOSFile_win.cpp`) -- this exact override already existed for
     macOS (which hits the identical problem via `__APPLE__`), it just
     hadn't been extended to the Windows branch yet.

  **Where it stands.** With all eight fixed, Skia's own GN build got
  real compilation underway for the first time (34 of 544 ninja steps
  reached, real `.o` files produced) before hitting a different class of
  problem: genuine library-completeness gaps rather than toolchain
  wiring. Skia's `SkMathPriv.h` needs C++20 `<bit>`
  (`std::countl_zero`/`countr_zero`/`popcount`), but the pinned libc++
  commit predates that header's C++20 support entirely (its own `<bit>`
  has only the pre-existing internal `__popcount` helpers, no
  `_LIBCPP_STD_VER` gating or public C++20 entry points at all);
  separately, this project's own libc `<cinttypes>`/`<inttypes.h>`/
  `<wchar.h>` do not declare `imaxdiv_t`/`imaxabs`/`imaxdiv`/
  `wcstoimax`/`wcstoumax`, needed by the imported libc++'s own
  `<cinttypes>` wrapper. Both are deliberately left open (see `TODO.md`)
  rather than patching Skia's own upstream source or reopening the
  already-verified libc++ commit pin in the same pass -- a real decision
  point (touching the libc++ pin risks the already-verified static/
  shared libc++ end-to-end work from 2026-08-19/21) that the user chose
  to defer rather than push through immediately. The eight toolchain-
  wiring fixes above are real and independently useful regardless of
  that open question: full default `ctest` still passes 120/120 with
  `CRTGFX_ENABLE_SKIA` left OFF (its pre-existing default), confirming
  zero regression to the rest of the project.

## 2026-08-21

- **sysroot/rootfs staging verified end to end for the imported-libc++
  configuration Skia actually needs, and Skia's own fetch pinned +
  sparse-checked-out + verified building real end to end on Windows for
  the first time.** Prompted by a request to (1) confirm libc/libdl/
  libm/libstdc++ genuinely stage correctly into both `sysroot` and
  `rootfs` in the shape `libcrtgfx`'s Skia bridge needs before relying on
  it further, and (2) bring Skia's own fetch up to the same pinned/
  sparse-checked-out/recipe-described discipline `libstdc++/third_party/
  {libcxx,libcxxabi,libunwind}/recipe.json` already has (see this same
  file's own entry earlier this same day).

  **Part 1 -- sysroot/rootfs, confirmed working.** Reconfigured with
  `CRT_USE_IMPORTED_LIBCXX=ON` (the toggle `libcrtgfx/CMakeLists.txt`'s
  own comment already says Skia genuinely needs -- "[l]inking it into
  libcrtgfx requires the full libc++ standard library"), built `sysroot`
  and `rootfs` from scratch, and inspected the real result directly:
  `sysroot/lib` carries both static and shared import libraries for
  `c`/`m`/`dl`/libc++/libc++abi, `sysroot/bin` carries the real runtime
  DLLs, `sysroot/include/crtgfx/skia.h` is present, and `rootfs/system/
  lib`+`rootfs/usr/lib` receive all 8 expected runtime libraries
  (`c.dll`/`m.dll`/`dl.dll`/`c++.dll`/`c++abi.dll`/`crtgfx.dll`/`crtjs.
  dll`/`crtmedia.dll`). Full `cmake --build` + `ctest` (120/120)
  confirmed no regression from leaving this toggle on.

  **Part 2 -- Wayland has nothing to pin.** Checked directly:
  `libcrtgfx/third_party/wayland/README.md` states outright "It is
  intentionally not a checkout" -- the current Linux adapter (`src/
  wayland_weston.c`, `src/arch/linux/window_wayland.c`) is a hand-
  written client of the documented core Wayland/xdg-shell wire
  protocols, not built against any fetched upstream source, matching
  `TODO.md`'s own already-decided policy ("do not vendor a full
  compositor until the crtgfx surface/frame boundary has tests"). Left
  as-is per the user's own explicit choice when asked.

  **Part 3 -- Skia's fetch, pinned and trimmed, verified via a real
  build.** `CRTGFX_SKIA_REF`/`CRTGFX_SKIA_EXPECTED_COMMIT` now default to
  a real commit (`13ffba253fc7854fd3b34f67c82dfb2418dc2944`, captured via
  `git ls-remote https://skia.googlesource.com/skia.git refs/heads/
  chrome/m148` that day) instead of an empty/floating value -- that
  specific commit's own message ("Remove CQ for unsupported branch
  refs/heads/chrome/m148") confirms the branch was already frozen by
  Skia's own infra at pin time. `tools/fetch_skia.py` gained cone-mode
  sparse-checkout support (a new `--sparse-path`, repeatable), mirroring
  `tools/crt-libcxx-build.py`'s own mechanism. The actual sparse set
  (`bin`, `build_overrides`, `client_utils`, `gn`, `include`, `modules`,
  `specs`, `src`, `third_party`, `toolchain`) was derived empirically,
  the same discipline already used for libcxx: `gn gen` first against
  the full checkout with a real `gn` binary (bootstrapped via Skia's own
  `bin/fetch-gn`, itself pinned to a `git_revision` hardcoded in that
  script), then `ninja -t inputs skia` against a real build to see
  exactly what the `skia` target's own transitive inputs are, not
  guessed. `modules/` had to stay whole rather than trim to the one
  module actually linked (`modules/skcms`, 252K of the dir's 19M): `gn
  gen` needs to at least *load* every module's own `BUILD.gn` to
  evaluate its own `enabled = skia_enable_<x>` condition even when
  disabled, confirmed for real via a failed `gn gen` ("Unable to load
  ... modules/skottie/BUILD.gn") once trimmed too far.

  `CRTGFX_SKIA_SYNC_DEPS` now defaults OFF, a real and important safety
  fix independent of the pin itself: running `git-sync-deps` for real
  against this project's own minimal CPU-raster-only GN config (every
  optional codec/GPU backend already off) downloaded 8.6GB before being
  killed -- including a complete Emscripten/WASM toolchain (node.js, a
  Python distribution, WASM binaries), wholly unrelated to a Windows
  static-library CPU-raster build, and Skia's own `git-sync-deps`
  unconditionally fetches its entire `DEPS`-declared third-party set
  regardless of which GN features are actually enabled. Separately
  confirmed via `ninja -t inputs skia` that this project's own minimal
  config needs zero `third_party/externals/` content at all -- once one
  more flag was also disabled: `skia_use_wuffs` (GIF decode), the one
  codec flag Skia's own `gn/skia.gni` defaults to `true` that this
  project's `tools/build_skia.py` had not already turned off alongside
  every sibling codec (`skia_use_libpng_decode`, `_libjpeg_turbo_decode`,
  etc.) -- fixed alongside the pin.

  `tools/build_skia.py` also gained two real Windows fixes, both found
  and confirmed necessary while actually trying to drive this through
  the real `crtgfx-skia-build` CMake target (which, unlike this
  session's own manual scratch testing, never passes an explicit `--gn`
  path): (1) `gn = args.gn or str(source / "bin" / "gn")` never resolved
  on Windows even when `bin/gn.exe` genuinely existed -- `Path("bin/gn")
  .exists()` is always false without the `.exe` suffix -- fixed to check
  the right suffix and, if still missing, auto-bootstrap via Skia's own
  `bin/fetch-gn`, the same way a real Skia developer would; (2) `gn gen`
  failed outright with `ERROR Could not find "python3" from dotfile in
  PATH` -- Skia's own `.gn` dotfile hardcodes `script_executable =
  "python3"`, a real, unconditional upstream requirement a stock Windows
  Python install does not satisfy by that name (unlike most Linux/macOS
  distro Python packages) -- fixed with a throwaway, project-owned
  `python3.bat` PATH shim, prepended only to this one subprocess's own
  environment, never touching any real system PATH (matching the "wrap
  what's needed, don't require host changes" discipline `tools/crt-cc`
  and friends already use).

  A real mistake was made and caught mid-implementation, the *same*
  mistake as `libstdc++/third_party/*/recipe.json`'s own earlier fix
  this same day: `tools/fetch_skia.py`'s first version of the sparse
  clone also dropped `--depth 1` from the initial partial clone (an
  interactive scratch test that validated the exact command sequence had
  actually kept `--depth 1`; the flag was lost transcribing that test
  into the real file). Caught for real, not just in theory: a genuine
  `crtgfx-skia-fetch` run without it produced a 189MB `.git` (464,512
  packed objects) before being fixed back down to ~22MB.

  Verified via the real, actual project machinery end to end, not a
  scratch script: `crtgfx-skia-fetch` (fresh fetch, ~98MB total, ~22MB
  `.git`) then `crtgfx-skia-build` (real `gn gen` + full `ninja` build
  through the CMake target, auto-bootstrapping both `gn.exe` and the
  `python3` shim along the way) produced a genuine `libskia.a` (21MB).

  **A separate, pre-existing, previously-undiscovered gap was found
  trying to go one step further** (reconfiguring with
  `CRTGFX_ENABLE_SKIA=ON` and building `crtgfx_skia_raster_smoke`):
  the final link fails on Windows with a long list of `lld-link: error:
  duplicate symbol` (`printf`, `fprintf`, `snprintf`, `fabsf`, `fabsl`,
  `frexpl`, `wmemcpy`, `wmemset`, `wmemcmp`, ...) between this project's
  own `c.lib`/`m.lib` and objects (the smoke test's own compiled object,
  several `libskia.a` members, MSVC's own `libcpmt.lib`) that carry
  their own copies of the same symbol names. Root-caused, not merely
  observed: the top-level `CMakeLists.txt`'s own `crt_cxx_build_flags`
  deliberately omits `-nostdinc++` on Windows only (by design -- this
  project's own Windows C++ bootstrap library, `cxx`/`cxx_shared`, is
  built against real MSVC STL headers, unlike Linux/macOS), so any
  Windows CMake-native C++ translation unit that reaches a real C stdio/
  math header -- directly, or transitively through Skia's own headers,
  as here -- gets MSVC UCRT's own inline-materialized copies of these
  functions compiled in as real, externally-visible symbols, colliding
  with this project's own freestanding libc/libm once both get linked
  into the same final executable. This appears to be the first target
  that ever links this project's own `c`/`cxx` bootstrap libraries
  together with something that also pulls in real MSVC UCRT headers on
  Windows -- confirmed to be unrelated to anything changed in this same
  pass (`crt_cxx_build_flags`, `detect_cxx_standard_include_dirs()`, and
  both targets' own `target_link_libraries()` calls were all untouched).
  Deliberately left open as a new, separate, tracked item (`TODO.md`'s
  Skia section) rather than rushed into the same pass -- a genuinely
  different, deeper Windows-C++-runtime-architecture question from "does
  the Skia fetch/build itself work," which is now fully verified.
  `CRTGFX_ENABLE_SKIA` was turned back off (its original, pre-existing
  default) before finishing this pass, and a stale `exports.def` left
  over from briefly toggling it on (a real but purely local incremental-
  build artifact, confirmed by deleting it and rebuilding clean --
  `bin/crtgfx.dll`'s own `.def`-export-generation step had not correctly
  regenerated after the objects list it scans changed) was cleared before
  the final regression check. Full `cmake --build` + `ctest` (120/120)
  with `CRTGFX_ENABLE_SKIA=OFF` confirms the default workflow is
  unaffected by any of this pass's changes.

- **libcxx/libcxxabi/libunwind source pinned to exact commit SHAs, and
  sparse-checkout extended to trim libcxx/libcxxabi's own unused `test/`
  suites.** Followed a request to evaluate vendoring the source into this
  project's own tree versus keeping the existing build-time `git clone`
  -- the evaluation's own two concrete, low-cost recommendations (pin
  exact SHAs regardless of the vendoring decision; skip full vendoring
  for now, since a floating `refs/heads/main` was the real risk, not the
  network dependency) were then implemented directly.

  All three `libstdc++/third_party/{libcxx,libcxxabi,libunwind}/
  recipe.json` had `"ref": "refs/heads/main"` -- a floating branch
  reference, not a pinned commit, with no lockfile recording which
  commit a given build actually fetched. `apply_patches()`'s own
  fail-fast behavior (a patch's `find` text not matching raises
  `SystemExit` immediately, confirmed already in place) only catches
  drift in the *specific* text each of this project's 18 patches
  touches -- everything else in three multi-megabyte C++ runtime
  components could silently change between builds with zero signal at
  all. Pinned via `git ls-remote <repo> refs/heads/main` on each of the
  three repos the same day: libcxx `4f4a65c06cecf421b56b9fea867d3aa7200f7f1a`,
  libcxxabi `65715172d940193fee91631e19adb138bce340c6`, libunwind (from
  `toolchain/llvm-project`) `37f38d1f3276b62fba09462ab4807dce846c732d`.

  Pinning to a raw SHA needed a real `tools/crt-libcxx-build.py` fix, not
  just a JSON edit: `fetch_recipe()`'s existing `git clone --branch <ref>`
  (used for the initial shallow clone in both the sparse and non-sparse
  paths) does not accept an arbitrary commit SHA -- confirmed for real
  against this project's actual git host, `android.googlesource.com`'s
  Gerrit/JGit backend rejects it outright ("Remote branch <sha> not
  found in upstream origin"). Confirmed the fix empirically before
  writing it: `git fetch --depth 1 origin <sha>` (as opposed to `clone
  --branch`) DOES work against the same host for an arbitrary reachable
  commit, for both the small standalone repos and the giant
  `toolchain/llvm-project` monorepo alike -- so `fetch_recipe()` no
  longer passes `--branch` on the initial clone at all, relying entirely
  on a separate `git fetch origin <ref>` + `git checkout --detach
  FETCH_HEAD` step that works identically whether `ref` is a branch name
  or a raw SHA (the non-sparse branch, unused by any of today's three
  recipes but fixed the same way regardless rather than left as a latent
  trap for a future recipe).

  A real mistake was made and caught while implementing this: the first
  version of the fix also dropped `--depth 1` from the initial clone
  step, reasoning from an interactive test that had actually still kept
  `--depth 1` (a misreading of the interactive test's own command,
  caught only once the real background build was observed genuinely
  stuck -- `ps`/`Get-Process` showed a `git` process's CPU time climbing
  continuously, over ten real CPU-minutes, cloning the *entire* commit
  history of the giant `toolchain/llvm-project` monorepo before being
  killed. `--filter=blob:none` alone is not a substitute for `--depth
  1`: it only defers file *content*, never trims the *commit graph*
  itself, which for a repo with llvm-project's history is the actually
  expensive part even with zero blobs downloaded. Fixed by restoring
  `--depth 1` (clean now: ~6s for the same monorepo clone step, matching
  the original interactive test once it was actually reproduced
  faithfully).

  `sparse_paths`/`checkout_subdir: "."` was also added to libcxx and
  libcxxabi (previously used only by libunwind's monorepo-subpath
  extraction) -- a second, distinct use of the same mechanism: trimming
  a same-repo component's own unused directories, not extracting a
  subdirectory from a larger monorepo. Both repos already ARE their own
  component; both also carry their own `test/` suite this project's
  build never runs at all (`LIBCXX_INCLUDE_TESTS=OFF`/
  `LIBCXXABI_INCLUDE_TESTS=OFF` already set) -- measured directly at
  37MB of libcxx's ~52MB full fetch and 5.7MB of libcxxabi's ~7.3MB.
  The kept directory sets (`CMakeLists.txt`/`include`/`lib`/`src`/`cmake`
  for both, plus `utils` for libcxx only) were derived by actually
  reading each repo's own top-level `CMakeLists.txt` for every
  `add_subdirectory()`/`include()` it reaches unconditionally (both
  self-contained: neither is extracted from a larger monorepo the way
  libunwind is, so `CMAKE_MODULE_PATH` resolves every shared CMake
  module to each repo's own `cmake/Modules/`, no `extra_checkout_dirs`
  needed) and grepping for direct-by-path script references
  (`utils/cat_files.py`/`utils/merge_archives.py`/`utils/gen_link_
  script.py`, kept for libcxx since two of the three are invoked from
  `lib/CMakeLists.txt` directly, not worth the risk of chasing exactly
  which `LIBCXX_ENABLE_*` option would prove the third is never
  reached). Confirmed via a genuinely fresh fetch (existing checkouts
  wiped first) through the real project script: libcxx dropped from
  ~52MB to 9.0MB, libcxxabi from ~7.3MB to 612KB, `test/` gone from
  both, and a full `crt-libcxx-build` + `crt-libcxx-smoke` + `ctest`
  cycle against the freshly pinned-and-trimmed source still reports
  `imported_libcxx_test: ok` for both linkage modes and 120/120 tests
  passing -- no regression from either the pin or the trim.

- **TODO.md item 7 (native-callback/boundary safety net for Windows)
  done -- and its own original design disproved along the way, by an
  empirical repro built specifically to test it.** Full technical detail
  lives in `TODO.md`'s C++ runtime prerequisite section, step 7, and
  `docs/cxx_runtime.md`'s "Known cost: DWARF-compiled code has zero
  Windows-native unwind info" section; this entry records the
  investigation's own narrative.

  The item's own original text proposed "a boundary shim compiled with
  real SEH (`-fseh-exceptions`, so it has genuine `.pdata`) at every point
  CRT/libc++ code is entered from or exits into native OS-driven control
  flow." Before implementing that, it was built as a minimal standalone
  repro (raw `clang --target=x86_64-w64-mingw32`, no CRT sysroot needed --
  a chain of four `__attribute__((noinline))` functions with
  volatile-touched stack buffers compiled `-fdwarf-exceptions`, called
  from an outer function compiled WITHOUT that flag whose own real
  `.pdata`-backed frame wraps the call in a genuine `__try`/`__except`)
  and it failed: the `__except` never caught the fault. A control run of
  the identical repro with the callee chain ALSO compiled without
  `-fdwarf-exceptions` (real `.pdata` throughout) confirmed the harness
  itself was sound -- the same `__except` caught the same fault cleanly
  there. Root cause: the OS's frame-based unwind search has to walk every
  intervening frame's own table entry to compute the next frame up; it
  fails at the FIRST untabled frame it meets, long before ever reaching
  the boundary's own handler further out -- a single SEH frame at the
  call-in point cannot make the frames beneath it walkable.

  `AddVectoredExceptionHandler()` (VEH) was tried next and does NOT have
  this problem: confirmed empirically that a VEH callback (registered
  with either `First=1` or `First=0` -- both behaved identically) reliably
  fires for the exact same deep-DWARF-chain fault, with no SEH boundary
  anywhere in the link at all -- VEH dispatch is driven directly from the
  fault's own context, not a stack walk. But a second repro (the same
  fault, this time in a fully `.pdata`-backed callee chain with a real,
  working `__except` around it, plus a VEH handler registered alongside)
  showed VEH always fires FIRST, unconditionally, regardless of `First=1`
  vs `First=0` -- preempting and completely breaking the legitimate
  `__except`'s own chance to run. `SetUnhandledExceptionFilter()` was
  considered as the non-preempting alternative (its whole contract is
  "only called once nothing else handled it," so it structurally cannot
  preempt a working `__except`) but a third repro disproved it too: it
  never fired at all for the deep-DWARF-chain fault, for the identical
  reason the plain SEH boundary failed -- reaching "nothing else handled
  it" itself requires completing the same broken walk.

  The fix that actually works, verified by a fourth and fifth repro:
  gate the VEH handler on `RtlLookupFunctionEntry()` -- the exact same
  table lookup the OS's own frame-based dispatch already relies on --
  called on the faulting address itself. When it returns non-NULL (real
  `.pdata` present at the fault site, regardless of whether that code has
  anything to do with this project's own CRT/libc++), the handler defers
  completely (`EXCEPTION_CONTINUE_SEARCH`) -- confirmed via the fifth
  repro that this lets a real `__except` elsewhere in the chain catch the
  exception exactly as if the handler were never installed. When it
  returns NULL (a genuine DWARF-only frame -- the fourth repro's own
  scenario), the handler takes over.

  Shipped as `libc/src/arch/windows/common/dwarf_unwind_safety_net.c`: a
  process-wide `AddVectoredExceptionHandler()` registration installed at
  CRT startup (`crt1.c`'s `mainCRTStartup()` right after
  `_pei386_runtime_relocator()`, and `dllcrt.c`'s
  `crtDllMainCRTStartup()` on `DLL_PROCESS_ATTACH`, matching that same
  file's own established wiring pattern), gated by the `RtlLookupFunction
  Entry()` check above; on a genuine gap it writes a brief raw-`WriteFile`
  diagnostic (exception code, faulting address -- no backtrace attempted,
  see the file's own top comment for why) and calls `ExitProcess()` with
  this project's own existing `128 + <POSIX signal number>` convention
  (`libc/src/signal.c`'s `abort()` already uses `128 + SIGABRT`).
  Diagnostic output and process termination both use raw kernel32 calls
  only, matching `pseudo_reloc.c`'s own zero-libc-dependency discipline
  (a hardware-fault handler cannot assume the rest of this project's own
  libc state is intact, and must not risk the same fd.c/`read()`-bundling
  collision that file's own history already found once).

  Wiring this into every shared DLL surfaced one more real, genuine
  regression along the way: `_crt_install_dwarf_unwind_safety_net` (a
  plain global, non-`static`, function -- required, since `crt1.c`/
  `dllcrt.c` call it across translation units) got auto-exported by
  GNU-ABI `-shared` links' own default "export every global symbol unless
  something is explicitly `dllexport`'d" behavior, and `tests/
  windows_dll_symbol_priority_test`'s own chained-DLL fixture (one DLL
  importing another, both independently linking a fresh copy of the same
  startup object) hit a real `ld.lld: error: duplicate symbol` the moment
  that chain's own middle DLL saw the symbol both defined locally and
  imported from the DLL underneath it. Fixed with `-Wl,--exclude-
  symbols=_crt_install_dwarf_unwind_safety_net` added to `tools/crt-cc`/
  `tools/crt-c++`'s own Windows `-shared` branches, alongside the same
  fix applied preemptively to `_pei386_runtime_relocator` (the identical
  latent risk by the identical mechanism, confirmed structurally sound
  but never actually exercised by any existing chained-DLL test) rather
  than left as a known dormant trap beside the one that did fail. The
  project's own CMake-native shared targets (`c_shared`/`m_shared`/
  `dl_shared`/`cxx_shared`) were checked and confirmed NOT affected: they
  use the MSVC-ABI toolchain path, where nothing auto-exports without an
  explicit `dllexport`, and none of their own object files ever reference
  the startup-hook symbol by name from another DLL's import library (the
  duplicate only manifests when a linker actually resolves a live
  reference to the colliding name, not merely from linking against an
  archive that happens to contain it).

  A permanent regression test (`tests/windows_dwarf_unwind_safety_net_
  test.c` + `_victim.c`) exercises the real, shipped mechanism end to end
  through the production toolchain -- the victim is a plain C program
  (no libcxx dependency, so this stays in the default ctest suite rather
  than gated behind the separate, opt-in `CRT_USE_IMPORTED_LIBCXX`
  pipeline) compiled via `tools/crt-cc` with an explicit
  `-fdwarf-exceptions` flag, reproducing the same call-chain shape the
  standalone repros above used. Writing this test surfaced one more real,
  useful finding: this project's own Windows `waitpid()`
  (`libc/src/arch/windows/common/syscall.c`) always encodes a child's
  `GetExitCodeProcess()` value as a plain `WIFEXITED` status
  (`(exit_code & 0xff) << 8`) with no `STATUS_*`-pattern detection or
  signal synthesis at all -- so the test asserts `WIFEXITED(status) &&
  WEXITSTATUS(status) == 128 + SIGSEGV`, not `WIFSIGNALED()`/`WTERMSIG()`
  the way an equivalent POSIX-host assertion would read. Mapping a real
  hardware fault back into `WIFSIGNALED()` on Windows is exactly the
  separate, larger, still-not-yet-decided "bridge `SIGSEGV`/`SIGFPE`/
  `SIGILL` through `signal()`/`raise()`" question `docs/signal_
  delivery.md`'s own "Next Steps" already tracks -- this test deliberately
  checks what the PAL genuinely produces today, not that separate,
  undecided feature. Full default `cmake --build` + `ctest` (120/120,
  the new test included) confirms no regression.

- **Shared libc++ now also works end to end on Windows: `imported_libcxx_
  test: ok` for both the static and shared legs of `crt-libcxx-smoke`,
  matching macOS and Linux for both linkage modes.** Direct continuation
  of the static-leg entry immediately below, same day. Full technical
  detail lives in `TODO.md`'s C++ runtime prerequisite section, step 4 --
  this entry records the narrative. Four genuinely separate root causes,
  found and fixed one at a time, each hiding the next behind it:
  1. **The export-table gap.** The shared client linked with dozens of
     `undefined symbol: __declspec(dllimport) std::__1::basic_string<...>`
     errors (and siblings for other containers), even though `src/
     string.cpp`'s own explicit template instantiation genuinely compiles
     every one of those members as a real, external (`T`) symbol
     (confirmed via `llvm-nm` on the compiled object). Two independent,
     layered causes: `-fvisibility-inlines-hidden` (libcxx's own
     top-level `CMakeLists.txt`, unconditional upstream) is harmless on
     ELF/Mach-O (visibility there only controls cross-DSO export, never
     whether a client TU may define the symbol locally itself) but fatal
     on Windows/PE once a client sees the whole class `dllimport`-
     decorated (via `extern template class ... basic_string<char>`) and
     is therefore forbidden from locally defining *any* of its members,
     hidden-visibility inline ones included -- confirmed by dumping
     `string.cpp.obj`'s own `.drectve` COFF section and finding real
     `-exclude-symbols:` directives for exactly the mangled names lld
     later reported undefined. Disabled for `if (NOT WIN32)` only.
     Necessary but not sufficient by itself: the same undefined-symbol
     errors persisted identically even after a fresh rebuild confirmed
     the `.drectve` exclude-symbols list was empty, which is what led to
     the real, deeper cause -- `include/__config`'s `_LIBCPP_EXTERN_
     TEMPLATE_TYPE_VIS` (the `extern template class` *declaration*-site
     macro every TU sees via `<string>`) was left completely empty by
     upstream whenever `_LIBCPP_BUILDING_LIBRARY` is defined, while its
     sibling `_LIBCPP_CLASS_TEMPLATE_INSTANTIATION_VIS` (the explicit-
     instantiation *definition*-site macro, used in `src/string.cpp`
     itself) correctly became `dllexport` -- a genuine declaration/
     definition mismatch, and Clang enforces a real C++/MSVC-ABI rule
     that a later definition cannot add a DLL attribute a preceding
     declaration of the same entity lacked. Confirmed via the exact
     compiler warning once the `.drectve` fix stopped masking it:
     `"'dllexport' attribute ignored on explicit instantiation
     definition ... 'dllexport' attribute is missing on previous
     declaration"`, naming `basic_string<char>` (`string.cpp`) and
     `basic_iostream<char>` (`ios.cpp`) by name. Fixed by matching the
     declaration-site macro to the same `_LIBCPP_DLL_VIS`; `llvm-nm`
     confirmed real `__imp_`-prefixed exports for `basic_string<char>`'s
     members in the freshly rebuilt `libc++.dll.a` afterward, and every
     `basic_string`-related undefined-symbol error was gone on the next
     `crt-libcxx-smoke` run.
  2. **The missing `libc++abi.dll.a` link.** Fixing (1) traded one error
     set for a completely different one: `__cxa_allocate_exception`/
     `__cxa_throw`/`__cxa_begin_catch`/`__cxa_end_catch`/`std::terminate`/
     `__gxx_personality_v0`/vtable-for-`std::length_error`/vtable-for-
     `__cxxabiv1::__si_class_type_info`/`__cxxabiv1::__class_type_info`,
     all from libcxxabi rather than libcxx. Checked `include/
     __cxxabi_config.h`'s own `_LIBCXXABI_FUNC_VIS` family first (same
     class of bug as (1)) and found it structurally sound -- same macro
     consistently used for both declaration and definition sites, no
     split-macro mismatch -- and confirmed via `llvm-nm` on the freshly
     built `libc++abi.dll.a` that every one of those symbols genuinely,
     correctly IS exported already. That proved the DLL's own export
     table was fine and the bug had to be at the client's link step
     instead: `tools/crt-c++`'s own Windows *shared* branch (`elif
     [ "$runtime_linkage" = shared ]`) only ever searched for and added
     `libc++.dll.a` and (optionally) `libunwind.dll.a` to
     `cxx_runtime_libs` -- `libc++abi.dll.a` was never in the list at
     all, unlike the *static* branch immediately below it, which already
     correctly links `libc++.a` + `libc++abi.a` + `libunwind.a`. A
     genuine, previously-latent asymmetry (the shared leg had simply
     never worked before this pass, so it had never been exercised).
     Fixed by adding the same three-candidate-name lookup loop
     (`libc++abi.dll.a`/`libc++abi_dll.lib`/`c++abi_dll.lib`) already used
     for the other two libraries in the same branch.
  3. **The DLL-search-`PATH` bug.** Fixing (2) made the shared client
     link completely clean for the first time -- and then fail to *run*,
     exiting `3221225781` (`0xC0000135` / `STATUS_DLL_NOT_FOUND`).
     `tools/test_libcxx_runtime.py` reused the exact same `env` for both
     the *compiler* invocation (needs `PATH="/system/bin:/bin:/usr/bin"`,
     the mksh/toybox-only POSIX string `tools/crt-libcxx-build.py`'s own
     `common_cmake_args()` already establishes, since the compile step
     runs through `mksh.exe`) and for directly running the resulting
     *native* Windows `.exe` afterward -- but the real Windows DLL loader
     parses `PATH` as semicolon-separated backslash directories and found
     none at all in that POSIX string, so it could never locate
     `libc++.dll`/`libc++abi.dll`/`libunwind.dll` (staged into
     `sysroot/bin`, not copied next to the smoke-test binary itself).
     Never mattered for the static leg (no runtime DLL dependency beyond
     kernel32, always found via the system directories regardless of
     `PATH`). Fixed by building a separate, real Windows `PATH`
     environment (derived from the original inherited environment, with
     `sysroot/bin` prepended) used only for the run step, leaving the
     mksh-only `PATH` untouched for the compile step.
  4. **The missing `RUNTIME DESTINATION`.** `STATUS_DLL_NOT_FOUND`
     persisted even after (3) -- traced this time all the way to
     `install/bin` never receiving `libc++.dll`/`libc++abi.dll` at all
     (only `libunwind.dll` made it there; `install/lib` only ever got the
     two `.dll.a` import libraries and `.a` static archives), even though
     the real `.dll` runtime binaries genuinely existed in the raw,
     un-installed build tree the whole time (`build/libcxx/lib/
     libc++.dll`, `build/libcxxabi/lib/libc++abi.dll`). Root-caused to
     both `libcxx/lib/CMakeLists.txt`'s and `libcxxabi/src/CMakeLists.txt`'s
     own upstream `install(TARGETS ...)` calls never specifying a
     `RUNTIME DESTINATION` clause, only `LIBRARY DESTINATION` and
     `ARCHIVE DESTINATION` -- CMake silently skips installing an artifact
     kind with no destination given at all rather than defaulting it
     somewhere sensible, and a Windows/PE shared-library target's actual
     `.dll` is a `RUNTIME` artifact, entirely distinct from its `ARCHIVE`
     (`.dll.a`) import library (which the existing `ARCHIVE DESTINATION`
     clause already installed correctly). Confirmed by direct comparison
     against libunwind's own sibling `install(TARGETS ...)` rule, which
     already carries a working `RUNTIME DESTINATION` clause and has
     staged `libunwind.dll` correctly on every single build this whole
     session. Fixed with one new recipe.json patch apiece (`libstdc++/
     third_party/{libcxx,libcxxabi}/recipe.json`), each adding `RUNTIME
     DESTINATION ${..._INSTALL_PREFIX}bin COMPONENT ...` alongside the
     existing clauses -- confirmed via the next build's own progress
     output actually printing `-- Installing: .../install/bin/libc++.dll`
     for the first time, and via the sysroot-staging step's own summary
     line finally listing `bin/libc++.dll, bin/libc++abi.dll,
     bin/libunwind.dll` together.
  After all four fixes, `crt-libcxx-smoke` reports `imported_libcxx_test:
  ok` for both `imported_libcxx_test_static.exe` and `imported_libcxx_
  test.exe` (shared), and a full default `cmake --build` + `ctest` run
  reconfirmed 119/119 with no regression.

- **Static libc++ now works end to end on Windows, real exceptions
  included: `imported_libcxx_test: ok`, matching macOS and Linux's own
  passing marker for the first time.** Full technical detail (every
  patch, every gap, every fix, in order) lives in `TODO.md`'s C++ runtime
  prerequisite section, step 4 -- this entry records the narrative and
  the reasoning behind the two-track investigation that got here.
  The immediate trigger was deciding, after investigating and discussing
  the tradeoff, to redirect libcxx's own locale/`random_device` backend
  selection from the Windows/MSVC one (needing real Universal CRT
  `ucrtbase.dll` functions this project has consistently avoided linking
  everywhere else) to the Android/Bionic one already present in the same
  fork of libcxx this project already fetches. That redirect turned out
  smaller than the previous entry's own "materially bigger, deeper
  change" assessment had estimated: `support/android/locale_bionic.h`'s
  real guard is `__BIONIC__` (already defined project-wide), not
  `__ANDROID__` as assumed, and this project's own `xlocale.h`/`/dev/
  urandom` emulation already covered nearly everything the backend
  needs. Six independent `_LIBCPP_MSVCRT_LIKE`-family branch points
  across `include/__locale`, `include/__config`, `src/locale.cpp`, and
  `src/system_error.cpp` needed matching, carefully `__BIONIC__`-scoped
  patches (deliberately not a blanket "flip the macro off" -- macOS and
  Linux define `__BIONIC__` too and had to stay on their own,
  already-verified paths) -- one of the six (`src/locale.cpp`'s own
  separate backend `#include`, gated on `_LIBCPP_MSVCRT ||
  __MINGW32__`) was missed by grepping for `_LIBCPP_MSVCRT_LIKE`/
  `_LIBCPP_MSVCRT` alone, since `__MINGW32__` is unconditionally defined
  by Clang for this target regardless of either macro. `islower_l`/
  `isupper_l`/`iswalnum_l`/`iswgraph_l` turned out to already be
  *implemented* in `libc/src/locale_l.c` but missing from the public
  `include/xlocale.h` -- a small, pre-existing header/implementation
  drift, fixed alongside.
  Getting `crt-libcxx-build` itself green this way (plus a small,
  unrelated `cxx_filesystem`/`target_overrides.<os>.build_targets`
  CMake-target-selection fix, and the same libunwind-import-library
  linker-flag fix already applied to `libc++abi.dll` now applied to
  `libc++.dll` too) was only half the work. The other half -- actually
  compiling, linking, and running a real client C++ program against the
  result via `crt-libcxx-smoke` -- had never been reached on Windows
  before at all, and surfaced a long chain of further real, previously
  latent gaps in the general-purpose Windows C++ toolchain itself, not
  specific to this recipe:
  - `tools/test_libcxx_runtime.py` (a separate script from `tools/crt-
    libcxx-build.py`, never exercised on Windows before) needed the same
    rootfs-for-toybox/restricted-PATH/`CRT_HOST_CXX`-propagation/
    `--windows-sdk-libpath` treatment the other script's own
    `common_cmake_args()` had already worked out -- confirmed via a
    chain of silent failures (mksh's own forward-slash-only script-path
    recognition; a bare `clang++` mksh could never resolve without
    `CRT_HOST_CXX` set; `uname -m` returning exit 127 under `set -eu`
    with the wrong PATH, propagating silently with no error text at all
    since the failing command was inside a `$(...)` substitution).
  - `tools/crt-c++` itself had never actually linked a real Windows C++
    *executable* before (every prior Windows C++ link through it was
    either the recipe's own `CRT_CXX_BUILDING_RUNTIME=1` build or a
    shared-DLL path) -- its own `windows)` case's non-shared branch never
    had a `prelibs` variable at all, unlike `tools/crt-cc`'s otherwise
    near-identical branch, silently dropping `crt1_pseudo_reloc.o` and
    the `crt1_ctors_*` global-constructor-walking objects from every C++
    executable link. Confirmed via `undefined symbol: _pei386_runtime_
    relocator, referenced by crt1.c:117:(mainCRTStartup)`.
  - A client linking the pre-built *static* `libc++.a` still saw every
    class member `__declspec(dllimport)`-decorated -- `include/__config`'s
    `_LIBCPP_DLL_VIS` only has two states (`dllexport` while building the
    library, `dllimport` for "everyone else"), no third "plain static
    archive" state. Confirmed via lld's own diagnostic literally naming
    the real, non-decorated symbol sitting right there in `libc++.a`
    that it could see but not use. Fixed with `-D_LIBCPP_DISABLE_
    VISIBILITY_ANNOTATIONS`, libcxx's own documented escape hatch for
    exactly this, added to `tools/crt-c++` only for static, non-runtime-
    build consumers.
  - Client code through `tools/crt-c++` was never getting `-fdwarf-
    exceptions` at all (only the recipe build itself had it) --
    confirmed via `undefined symbol: __gxx_personality_seh0`, Clang's
    default native-SEH personality for this target, which this project's
    own DWARF-only libc++abi never exports. This is the project-wide
    application of the exact decision `docs/cxx_runtime.md`'s "Windows
    exception-table format" section already documents in the abstract;
    it had only ever actually been applied inside the recipe build
    itself until now.
  - `libc/src/arch/windows/common/dllcrt.c`'s DLL entry point now calls
    `_pei386_runtime_relocator()` on `DLL_PROCESS_ATTACH` (the DLL
    analogue of `crt1.c`'s own executable-startup call), needed the
    moment `libc++.dll`'s own auto-imported reference into `libunwind.
    dll` appeared -- confirmed via lld-link's own refusal to produce the
    image at all. Wiring `crt1_pseudo_reloc.o` into every place that
    could need it (`tools/crt-cc`, `tools/crt-c++`, and the main
    project's own CMake-built DLL targets via `crt_configure_shared_
    runtime()`) surfaced one more, genuinely unrelated regression:
    `pseudo_reloc.c`'s own diagnostic prints, only ever linked into
    executables before, first failed linking into `m.dll`/`dl.dll`/
    `c++.dll` at all (`undefined symbol: stderr`, a cross-DLL DATA
    reference those small DLLs never provide), and after switching to
    this project's own `write()`, broke a completely different existing
    test instead: `libc/src/fd.c` bundles `read()` and `write()` into one
    translation unit, so any reference to `write()` anywhere in a link
    pulls in that whole object including its real `read()` -- and
    `tests/windows_dll_symbol_priority_dll.c` deliberately defines its
    *own* conflicting `read()` (a regression fixture testing DLL symbol-
    priority resolution), so the two collided: `duplicate symbol: read`,
    confirmed as a real regression this change caused (the test was
    passing in the same session's own earlier 119/119 `ctest` run).
    Root-caused properly rather than chasing one symbol at a time:
    `abort()` needed enough of `signal.c`/`exit.c`'s own archive-member
    resolution to reach the identical collision, so both `write()` and
    `abort()` were replaced with direct kernel32 calls (`WriteFile`/
    `GetStdHandle`/`ExitProcess`) -- zero dependency on this project's
    own libc at all, matching this file's own "must run before
    absolutely anything else" design intent even more literally than the
    first attempt did. Full `ctest` reconfirmed 119/119 clean afterward.
  With all of that in place, `imported_libcxx_test_static.exe` compiles,
  links, and actually runs to completion -- real `vector`/`string`/
  exception-throw-catch coverage, genuinely confirmed on Windows for the
  first time. The **shared** leg (`CRT_CXX_RUNTIME_LINKAGE=shared`)
  remains open, and is a materially different, deeper problem from
  everything above: `libc++.dll` does not export enough of `basic_
  string<char>`'s (and similar containers') *inline* member functions to
  satisfy a client that sees the whole class `dllimport`-decorated --
  real libc++'s own Windows-shared-library story only reliably works for
  types it explicitly `extern template`-instantiates and exports, not
  arbitrary inline STL usage in client code. Confirmed via lld naming
  dozens of specific missing members across `basic_string`,
  `__vector_base_common`, and `length_error`. Deliberately left open
  rather than attempted in the same pass -- see `TODO.md`'s own step 4
  for the decision to stop here, and step 6 for how the cross-OS
  regression test this unblocks (Windows static leg) still needs the
  shared leg fixed before it can cover Windows fully the way it already
  does macOS and Linux.

- **Closed the `findUnwindSections()` gap: libunwind and libcxxabi now
  build clean on Windows, static and shared** (`crt-libcxx-build` reaches
  libcxx itself for the first time). Three real gaps found and fixed in
  turn getting libunwind's shared `unwind.dll` to actually link, after the
  new `libstdc++/third_party/win32_shim/{windows,psapi,ntverp}.h` header
  shim (added to the include path for windows recipe builds in
  `tools/crt-libcxx-build.py`'s `common_cmake_args()`) closed the compile-
  time gap from the prior entry:
  1. The shim's first draft declared `GetCurrentProcess`/`GetLastError`/
     `K32EnumProcessModules`/the four `SRWLOCK` Acquire/Release functions
     without `extern "C"`. Since `libunwind.cpp` is a C++ translation
     unit, these got C++-mangled linkage -- confirmed via the exact error
     text, `ld.lld: error: undefined symbol: __declspec(dllimport)
     K32EnumProcessModules(void*, void**, unsigned long, unsigned
     long*)`, a demangled C++ signature, not a plain C symbol name (which
     `llvm-nm` confirmed kernel32.lib genuinely exports as, correctly).
     Fixed by wrapping both shim headers' declarations in
     `#ifdef __cplusplus extern "C" { ... } #endif`.
  2. `UnwindCursor.hpp` also unconditionally `#include`s `<ntverp.h>`
     under plain `#ifdef _WIN32` (not `__SEH__`-gated), even though the
     one macro it defines (`VER_PRODUCTBUILD`) is only read inside an
     `_LIBUNWIND_SUPPORT_SEH_UNWIND` block, dead code under this
     project's DWARF-exceptions build. Shimmed with the real value
     (10011), verified against this machine's actual installed Windows 10
     SDK (`...\Windows Kits\10\Include\10.0.28000.0\shared\ntverp.h`), for
     accuracy in case a future SEH-enabled path ever reads it.
  3. `cmake/config-ix.cmake`'s `check_library_exists(pthread pthread_once
     ...)` reported a false positive on Windows -- confirmed for real via
     `lld: error: unable to find library -lpthread` once the object files
     actually linked (this project deliberately has no separate pthread
     library at all, matching modern Bionic; see AGENTS.md's build/
     runtime boundary principles). Root cause: the exact same
     `CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY`-means-
     `check_library_exists`-never-actually-links issue already diagnosed
     for `LIBCXXABI_HAS_CXA_THREAD_ATEXIT_IMPL` on Linux. Fixed with
     `-DLIBUNWIND_HAS_PTHREAD_LIB=OFF` via a new
     `target_overrides.windows` block in `libunwind/recipe.json` (its
     first). `LIBUNWIND_HAS_DL_LIB` was left alone: `-ldl` in that same
     link command did not error, since this project genuinely does ship a
     separate libdl the linker successfully resolves.
  Manually reproducing single failing link/compile steps to iterate
  quickly (rather than re-running the whole ~5-9 minute pipeline for every
  attempt) needed real environment replication -- `CRT_HOST_CC`/
  `CRT_MKSH_EXE`/`CRT_WINDOWS_SDK_LIBPATH`/etc. set by hand, matching
  exactly what `tools/crt-libcxx-build.py`'s subprocess env sets -- and
  had to be run from PowerShell rather than the Bash tool: MSYS/Git-Bash's
  own environment/exec layer interfered with mksh.exe's builtin `printf`
  resolution in a way that never happens through the real pipeline's own
  `cmd.exe`-launched process lineage (`printf: inaccessible or not
  found`, reproducible even with every other env var correctly matched,
  and gone the moment the same command ran from a plain PowerShell
  session instead).
  With libunwind now clean, libcxxabi needed one more real fix:
  `stdlib_stdexcept.cpp`/`stdlib_typeinfo.cpp` (defining `std::
  runtime_error`/`bad_cast`/`bad_typeid`'s vtables) never `#define
  _LIBCPP_BUILDING_LIBRARY` the way three sibling files in the same
  directory already do -- only libcxxabi's own, textually similar but
  functionally distinct `_LIBCXXABI_BUILDING_LIBRARY` macro gets set
  project-wide (`add_definitions` in libcxxabi's own top-level
  `CMakeLists.txt`). Since `_LIBCPP_BUILDING_LIBRARY` is what actually
  controls whether libcxx's own `<stdexcept>`/`<typeinfo>` headers expand
  `_LIBCPP_EXCEPTION_ABI` to `dllexport` or `dllimport`, these two files
  built their vtables under a `dllimport` declaration, and
  `cxxabi_shared/libc++abi.dll`'s link failed with `undefined symbol:
  vtable for std::runtime_error` and two siblings. Real upstream LLVM
  never hits this because its blessed build configures libcxx and
  libcxxabi together in one CMake project graph sharing this define; this
  project's three-separate-recipe build does not. Fixed with a one-line
  `add_definitions(-D_LIBCPP_BUILDING_LIBRARY)` patch right after
  libcxxabi's own existing, adjacent `add_definitions` call (a harmless
  "macro redefined" warning fires for the three files that also
  self-`#define` it, not fatal).
  With libcxxabi clean too, `crt-libcxx-build` finally reached libcxx
  itself and hit one more small, real gap plus a genuinely bigger,
  different one:
  - `cxx_filesystem` (libc++'s `<filesystem>` static archive) is not a
    CMake target on Windows at all: libcxx's own top-level
    `CMakeLists.txt` sets `ENABLE_FILESYSTEM_DEFAULT OFF` specifically
    `if (WIN32)` (ON everywhere else), with no stated rationale in the
    fetched source -- `ninja: error: unknown target 'cxx_filesystem'`, a
    hard failure. Fixed by adding proper
    `target_overrides.<os>.build_targets` support to
    `tools/crt-libcxx-build.py` (mirrors the existing `options` override
    exactly -- `build_recipe()` now checks
    `cmake.target_overrides.<target_os>.build_targets` before falling
    back to the common `cmake.build_targets`) and using it to drop
    `cxx_filesystem` from the Windows list only, leaving macOS/Linux's
    own already-verified list untouched. Deliberately not overriding
    `LIBCXX_ENABLE_FILESYSTEM` back to `ON` for Windows instead: nothing
    in this project currently needs `<filesystem>`, and there is no found
    reason in the source to second-guess upstream's own considered
    Windows-specific default.
  - With that fixed, `cxx`/`cxx_experimental`/`cxx-generated-config`
    compile almost entirely clean -- except 6 of libcxx's own `.cpp`
    files (`algorithm.cpp`, `chrono.cpp`, `iostream.cpp`, `ios.cpp`,
    `locale.cpp`, `random.cpp`) hit two real, unconditional missing
    headers: `<xlocinfo.h>` (via `include/support/win32/locale_win32.h`,
    libcxx's own Windows locale backend, selected because
    `include/__config` sets `_LIBCPP_MSVCRT_LIKE` unconditionally for any
    `_WIN32` target) and `<winapifamily.h>` (`src/chrono.cpp`, gating
    UWP-vs-desktop API availability). Both are real Universal CRT/Windows
    SDK headers, not something this project can freely stub the way the
    libunwind PE/COFF shim above did: `locale_win32.h` alone references
    at least 8 distinct MSVC UCRT `_locale_t`/`_X_l`-suffixed functions,
    and giving them real (not just link-satisfying) behavior would mean
    linking `ucrtbase.dll` -- a real, hosted MSVC C runtime this project
    has consistently avoided everywhere else (the same "own the
    toolchain" principle behind building libunwind from source and
    rejecting host `libunwind-dev`). Android's own libcxx fork (the
    actual source this project fetches) already carries an alternate,
    real `support/android/locale_bionic.h` backend selected via
    `__ANDROID__` in the same `include/__locale` `#if` chain --
    architecturally the better fit for a project whose entire premise is
    a Bionic-compatible libc (which already provides `xlocale.h`), but
    switching to it is materially bigger than the shims above: the same
    `_WIN32` block in `include/__config` that selects the MSVC locale
    backend also sets other Windows-wide assumptions in the same breath
    (e.g. `_LIBCPP_SHORT_WCHAR`, assuming 16-bit `wchar_t` -- this
    project's own Windows `wchar_t` is deliberately 32-bit via
    `-Xclang -fwchar-type=int`, so that assumption may already not hold
    regardless of the locale question), so this needs a real audit before
    being attempted, not a quick flip.
  Decision: stop here and record the finding rather than picking one of
  the two real directions (a real MSVC UCRT-locale-extension shim vs.
  switching libcxx's own locale/wchar backend selection to the
  Android/Bionic one already present in its own source) without
  discussing the tradeoff first -- see `TODO.md`'s C++ runtime
  prerequisite section, step 4, for the full writeup and the open
  decision. Full local `ctest` (119/119 on Windows) confirms no
  regression to the default build/test workflow, which never touches the
  `crt-libcxx-*` targets.

- **Investigated whether `-fdwarf-exceptions` on Windows breaks native DLL
  loading or exception interop, and found a real, previously-unscoped cost.**
  Checked directly by compiling identical throw/catch (and separately, a
  plain non-throwing) C/C++ source for `x86_64-w64-mingw32` with
  `-fseh-exceptions` versus `-fdwarf-exceptions` and diffing the resulting
  object sections: `-fdwarf-exceptions` emits **no `.pdata`/`.xdata` at
  all** for any non-leaf function -- not limited to functions that actually
  throw -- only a non-standard `.eh_frame` section only CRT's own
  from-source libunwind understands, with the personality symbol also
  differing (`__gxx_personality_seh0`, real/OS-visible, under SEH vs.
  `__gxx_personality_v0`, opaque to the OS, under DWARF). Since the Windows
  x64 ABI requires unwind-table entries for every non-leaf function and
  treats a missing entry as "this is a leaf, skip register restoration,"
  this is a genuine ABI-conformance gap, not just a C++ `catch`-interop
  limitation: it can affect a hardware exception propagating through a
  CRT/libc++ frame, any Windows-native stack walk (debugger, WER minidump,
  ETW) that crosses one, and CRT/libc++ code registered directly as a raw
  OS callback (window proc, thread entry point, vectored exception
  handler, COM vtable) -- all independent of whether any C++ exception is
  even involved. Confirmed plain `LoadLibrary` plus calling a non-throwing
  export is unaffected; the risk is specifically about an OS-driven unwind
  having to traverse a DWARF-only frame. Documented in `docs/cxx_runtime.md`
  ("Known cost: DWARF-compiled code has zero Windows-native unwind info",
  sharpening rather than replacing the existing MSVC ABI Bridge Lane
  caveat) and recorded as a not-yet-started design task (a boundary shim
  compiled with real SEH at every CRT/native OS control-flow crossing) in
  `TODO.md`'s C++ runtime prerequisite section, item 7 (renumbered from 6
  by the later same-day entry below, which added a new item 5). Decision:
  document now, defer the actual shim design -- it is not yet a real
  requirement
  since the Windows `crt-libcxx-build` itself isn't fully green yet (the
  `psapi.h`/PE-module-enumeration gap from the prior entry below is still
  open).

- **Fixed the first-ever Linux `crt-libcxx-build` run: two real gaps in
  this project's own Linux headers, exactly the "will likely hit related,
  if not identical, gaps once reached" TODO.md called out for the
  Windows-then-Linux libunwind/libcxxabi/libcxx rollout.** Reported
  directly ("`cmake --build --preset linux-host-ninja-debug --target
  crt-libcxx-build`가 실패한다"). Reproduced on the real Linux aarch64
  host; libunwind's `AddressSpace.hpp`/`UnwindCursor.hpp` failed to
  compile with two independent errors:
  1. `error: unknown type name 'Elf_Half'` (and `Elf_Phdr`/`Elf_Addr`).
     Root cause: `AddressSpace.hpp` only self-defines its `ElfW(type)`
     macro and the `Elf_Half`/`Elf_Phdr`/`Elf_Addr` typedefs as a
     `#if !defined(ElfW)` fallback for hosts (its own comment says
     FreeBSD) whose `<link.h>` doesn't already provide `ElfW()`. This
     project's `include/link.h` never defined `ElfW()` at all, so the
     fallback triggered -- and the fallback is circular by construction
     (`typedef ElfW(Half) Elf_Half;` expands to `Elf_Half Elf_Half;`
     when `ElfW(type)` is defined as `Elf_##type`, since `Elf_Half` isn't
     defined yet). Fixed by adding a real `ElfW(type)` macro to
     `include/link.h`, right after its `#include <elf.h>`: real glibc/
     Bionic define it as `Elf32_##type` or `Elf64_##type` depending on
     `__LP64__`/`__ELF_NATIVE_CLASS`, but this project only ever targets
     ELF64 (`include/elf.h`'s own comment: "this project has no 32-bit
     target"), so the branch collapses to an unconditional
     `#define ElfW(type) Elf64_##type`.
  2. `error: use of undeclared identifier 'SYS_rt_sigprocmask'`, from
     `UnwindCursor.hpp`'s `isReadableAddr()` (gated on the real, not
     `__SEH__`-related, `_LIBUNWIND_CHECK_LINUX_SIGRETURN` feature --
     inspired by Abseil's `AddressIsReadable`, it makes a raw
     `syscall(SYS_rt_sigprocmask, /*how=*/~0, ...)` to probe whether an
     address is readable without risking a real signal-mask change,
     since an invalid `how` is guaranteed to fail after the kernel's
     `copy_from_user` check but before validating `how` itself).
     `include/sys/syscall.h` only ever carried two hand-curated syscall
     numbers (`SYS_getpid`, `SYS_renameat2`); `rt_sigprocmask` was never
     added. Verified the real kernel syscall numbers before adding them
     (not guessed): `arch/x86/entry/syscalls/syscall_64.tbl` gives `14`
     for x86_64; `include/uapi/asm-generic/unistd.h` (aarch64's generic
     syscall ABI) gives `135`. Added `SYS_rt_sigprocmask` to
     `include/sys/syscall.h` for both architectures.

  Verified for real: `crt-libcxx-build` exits 0 from a clean state and
  is a no-op ("Up-to-date") on immediate rerun; a full `cmake --build
  --preset linux-host-ninja-debug` (whole project) also passes clean.
  Standalone `crt-cc` probes of `<wchar.h>`, `<stdio.h>`, and
  `<wctype.h>` each compile clean in isolation, confirming no header-
  ordering regression from unrelated earlier work.

  **Follow-up the same day, chasing `crt-libcxx-smoke` (the actual
  link+run exception-throw/catch check) to green -- real gaps, each
  root-caused and fixed in turn:**
  1. **Static link, first attempt:** the *first* Linux static link of
     `libc.a`/`libclang_rt.builtins.a`/`crt1.o` together failed with
     `undefined reference to '__crt_run_init_array'` (from `crt1.S`).
     Root cause: `tools/crt-cc` already carried the fix for this (a
     `${CRT_SYSROOT}/lib/crt1_init_array.o` link line, added when
     `__crt_run_init_array()`/`__crt_run_fini_array()` were split into
     their own object -- see `libc/CMakeLists.txt`'s own comment) but
     `tools/crt-c++` never got the equivalent line -- a plain omission,
     not a design gap. Fixed by adding the identical line to
     `tools/crt-c++`'s Linux static branch.
  2. **Static link, second attempt:** `undefined reference to
     '__getauxval'` (from `libclang_rt.builtins.a`'s aarch64
     outline-atomics `lse-init.o`) and `undefined reference to
     'getauxval'` (from `libdl`'s `dl_linux.c.o`). Both symbols are
     genuinely implemented (`libc/src/arch/linux/common/auxv.c`) and
     already used successfully by this project's normal *dynamic* Linux
     builds; the bug is link *order*: both `tools/crt-cc` and
     `tools/crt-c++` list `libdl.a`/`libclang_rt.builtins.a` *after*
     `libc.a`, so by the time either is scanned, `libc.a` has already
     had its one-and-only left-to-right archive scan and won't be
     revisited for a symbol only discovered as needed later -- ld/lld's
     default single-pass archive resolution, not a missing
     implementation. Fixed by wrapping the whole static archive set
     (`libc.a`/`libm.a`/`libdl.a`/the C++ runtime archives/
     `libclang_rt.builtins.a`) in `-Wl,--start-group ... -Wl,--end-group`
     in both `tools/crt-cc` and `tools/crt-c++`, so ld/lld iterates the
     set until nothing new resolves, independent of listed order.
  3. **Static link, third attempt: passed.** `crt-libcxx-smoke`'s static
     leg now builds, links, and runs to completion --
     `imported_libcxx_test: ok`, real vector/string/exception-throw-
     catch coverage, genuinely confirmed on Linux for the first time.
  4. **Shared link:** `libc++.so: undefined reference to '__cxa_atexit'`
     and `libc++abi.so: undefined reference to
     '__cxa_thread_atexit_impl'`. Root cause, confirmed by reading real
     LLVM libcxxabi's own fetched source: libcxxabi never implements
     `__cxa_atexit`/`__cxa_finalize` itself (Itanium C++ ABI convention:
     the platform *libc* provides these, not libc++abi -- real glibc and
     real Bionic both do). This project's only prior implementation of
     either symbol was `libstdc++/src/cxxabi.c`, part of the *bootstrap*
     `cxx`/`cxx_shared` targets predating the imported-libc++ work --
     never linked at all on the imported-libcxx path. Added a real
     `__cxa_atexit()`/`__cxa_finalize()`/`__dso_handle` (`__dso_handle`
     marked hidden-visibility, matching the "one private copy per DSO"
     reasoning already established in the macOS libcxx `__dso_handle`
     entry above) to libc itself, Linux-only, matching
     `CRT_LINUX_AUXV_FILE`'s own precedent: `libc/src/arch/linux/common/
     cxa_atexit.c`. Guarded the same three symbols back out of
     `libstdc++/src/cxxabi.c` on Linux only (`#if
     !defined(CRT_TARGET_OS_LINUX)`) to avoid a genuine
     multiple-definition conflict on the still-supported bootstrap
     Linux C++ path (any program using function-local statics needs
     that file's own `__cxa_guard_acquire()`, which would otherwise drag
     its now-duplicate `__cxa_atexit` copy in alongside).

     `__cxa_thread_atexit_impl` (the Bionic API 23+ thread_local-
     destructor extension) was a separate, subtler false positive:
     libcxxabi's own `config-ix.cmake` probes for it via
     `check_library_exists(c __cxa_thread_atexit_impl "" ...)`, which
     reported "found" even though this project's libc genuinely never
     implements it (confirmed by hand: reproducing the identical probe
     through `tools/crt-cc` fails to link). Root cause: `crt-libcxx-
     build.py` configures every out-of-tree LLVM runtime with
     `CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY`, so
     `check_library_exists`/`check_function_exists`-style probes only
     ever compile-and-archive a `.o`, never actually link an executable
     -- an undefined symbol can never surface, so every such probe in
     `config-ix.cmake` across all three imported components silently
     reports a false "found". Confirmed via
     `build/libcxxabi/CMakeFiles/CMakeConfigureLog.yaml`, which showed
     the probe's own `ninja`/`crt-ar` command line never invokes the
     linker at all. Fixed by adding a `target_overrides.linux` entry to
     `libstdc++/third_party/libcxxabi/recipe.json` forcing
     `-DLIBCXXABI_HAS_CXA_THREAD_ATEXIT_IMPL=OFF` -- supplying the
     correct, hand-verified answer the probe cannot reach itself, and
     letting libcxxabi fall back to its own portable, already-
     implemented `__cxa_atexit()`+TLS-key-based `cxa_thread_atexit.cpp`
     path instead (documented, accepted limitations only -- e.g.
     `dlclose()`'d DSOs -- not something this project introduced).
  5. **Shared link, next layer: passed the link, crashed at runtime.**
     `abort()` inside `libunwind::UnwindCursor<...>::isReadableAddr()`'s
     own `assert(errno == EFAULT || errno == EINVAL)` -- the exact
     function `SYS_rt_sigprocmask` (fixed earlier this same entry) backs
     -- firing on *every* real run, not just a rare edge case. Root
     cause, found via `gdb -batch -ex "break abort" -ex run -ex bt`:
     this project's public, glibc/Bionic-compatible variadic `syscall(2)`
     (`libc/src/syscall_public.c`) had never been implemented at all --
     `long syscall(long number, ...)` was a pure stub, unconditionally
     `return __set_errno(ENOSYS)`, `number` never even inspected.
     Harmless for this project's own code (every internal caller already
     has its own fixed-syscall-number trampoline, e.g. `__crt_sys_read`
     in `libc/src/arch/linux/{aarch64,x86_64}/syscall.S`) but a real gap
     for any external caller making a raw numbered `syscall()` directly,
     which is exactly what libunwind's `isReadableAddr()` does. Fixed
     with a real implementation: `__crt_generic_syscall(number, a1..a6)`,
     a new per-architecture raw-syscall assembly trampoline appended to
     each existing `libc/src/arch/linux/{aarch64,x86_64}/syscall.S`
     (shifting the 7 incoming C-ABI argument registers into the raw
     SVC/`syscall`-instruction argument registers -- aarch64: AAPCS64's
     x0..x6 down to SVC's x8+x0..x5; x86_64: SysV's
     rdi/rsi/rdx/rcx/r8/r9+stack down to `syscall`'s
     rax/rdi/rsi/rdx/r10/r8/r9, r10 replacing rcx because the `syscall`
     instruction itself clobbers rcx/r11, matching the existing
     `__crt_sys_prctl`/`__crt_sys_epoll_pwait` trampolines' own
     established 4th-argument workaround), with `libc/src/
     syscall_public.c` rewritten to unpack the C variadic argument list
     into that primitive's fixed 6-`long` signature and translate a
     negative-in-[-4095,-1] raw kernel return into the normal
     errno-plus-(-1) libc convention (matching `libc/src/fd.c`'s own
     `normalize_syscall_result()` precedent). Scoped to Linux only
     (`#if defined(CRT_TARGET_OS_LINUX)`); macOS/Windows keep the
     original unconditional ENOSYS stub, matching how a raw numbered
     `syscall(2)` is a Linux-specific POSIX extension to begin with.
     Verified for real by hand first (a standalone probe reproducing
     `UnwindCursor.hpp`'s exact call now returns `errno=EINVAL` as
     expected, not `ENOSYS`), then via the actual crash: gone.
  6. **Shared link, static leg regression check:** re-ran the full
     project build (`cmake --build --preset linux-host-ninja-debug`) and
     `ctest` after each of the above -- 104/104 passing throughout, no
     regressions from the `__cxa_atexit`/`syscall()` changes touching
     every Linux link.

  7. **Found immediately after, root-caused, and fixed the same day, at
     the user's explicit request to keep going:** with everything above
     fixed, the shared-linkage binary ran to `main()` and executed the
     `throw std::runtime_error(...)`, but `_Unwind_RaiseException`'s
     phase-1 search never found the `catch` clause -- `std::terminate()`
     fired (`terminating with uncaught exception of type
     std::runtime_error: caught`) even though the throw and catch are in
     the same function. Root-caused via `gdb -batch -ex "break
     crt_dl_backend_iterate_phdr" -ex run -ex finish`: the
     `_Unwind_RaiseException`/personality-routine call chain itself lives
     in `libcxxabi.so`/`libunwind.so` (separate ELF images from the main
     executable in the shared-linkage build), so libunwind's own
     `findUnwindSectionsByPhdr()` needs `dl_iterate_phdr()` to report
     *every* loaded shared object, not just the main executable, to
     locate `.eh_frame`/`.gcc_except_table` for a PC inside `__cxa_throw`
     itself (the very first frame phase-1 unwinding examines).
     `libdl/src/arch/linux/dl_linux.c`'s `crt_dl_backend_iterate_phdr()`
     -- by original, documented design (see `include/link.h`'s own
     former comment) -- reported exactly one entry, the main executable,
     "because this project has no real ELF dynamic linker yet". That was
     a correct, deliberate simplification when nothing else needed more,
     but the imported-libc++ shared-linkage path is the first real
     consumer that does.

     Fixed for real, not with a new ELF loader: this project already
     delegates actual `.so` loading to the *real* system dynamic linker
     (`/lib/ld-linux-aarch64.so.1`, confirmed via `tools/crt-cc`/
     `crt-c++`'s own `-dynamic-linker` link flag), which already
     maintains the standard, public SVR4 "rendezvous" `struct r_debug`/
     `link_map` linked list documented in real glibc's own `<link.h>`
     (confirmed against this host's `/usr/include/link.h`, Ubuntu
     24.04) -- real glibc's own `dl_iterate_phdr()` walks exactly this
     same structure internally, and gdb/lldb rely on the identical
     protocol. `crt_dl_backend_iterate_phdr()` now: (a) reports the main
     executable exactly as before, via AT_PHDR/AT_PHNUM (kept, not
     replaced -- more reliable than trusting an ELF header read through
     a link_map entry, since the kernel hands it directly to every
     process at exec()); (b) finds `struct r_debug` via the documented
     `_DYNAMIC`/`DT_DEBUG` technique (walking this executable's own
     linker-synthesized `_DYNAMIC` array for the `DT_DEBUG` entry the
     real dynamic linker fills in at startup -- the same technique gdb
     itself uses, deliberately not the simpler-looking `extern struct
     r_debug _r_debug` glibc's own header also documents, since that
     symbol lives inside the separate ld.so image, not something this
     project's own link resolves against); (c) walks `r_debug->r_map`,
     skipping the main executable's own entry (matched by `l_ld ==
     _DYNAMIC`, not list position) since it was already reported
     precisely in step (a), and for every other entry reads the ELF
     header at its `l_addr` load bias to locate that object's own
     `e_phoff`/`e_phnum` (link_map itself carries a load bias and a
     `.dynamic` pointer, not phdr/phnum directly). `struct link_map`/
     `struct r_debug` are declared locally in `dl_linux.c` as this
     project's own minimal copy of the stable SVR4 fields only (not
     glibc's full `<link.h>`, which mixes this protocol with a large
     surface of unrelated dlopen()/audit-interface/ld.so.cache plumbing
     this project has no use for).

     Verified for real: both `crt-libcxx-smoke` legs now build, link,
     *and run to completion* -- `imported_libcxx_test: ok` on **both**
     the static leg (already passing since item 3 above) **and now the
     shared leg** (previously crashing/terminating), real vector/string/
     exception-throw-catch coverage confirmed for both linkage modes on
     Linux for the first time. Re-ran the existing `dl_iterate_phdr_
     dladdr_test` and the full `ctest` suite (104/104) to confirm the
     `dl_iterate_phdr()` rewrite has no regression for this project's
     own prior single-entry callers. `include/link.h`'s own comment
     updated to match the new, real multi-image behavior.

  **`crt-libcxx-build`/`crt-libcxx-smoke` are now fully green on Linux,
  both static and shared linkage, matching the already-verified macOS
  state and Windows' own separate, still-open work (TODO.md).**

- **Fixed a real macOS `crt-libcxx-build` regression: `libcxx` needed its
  own `__dso_handle` shim, not just `libcxxabi`'s.** Reported directly
  ("`cmake --build ... --target crt-libcxx-build` 명령에 의해 빌드가 잘
  되었었는데, 뭔가 compile options이 바뀌어서 잘 안 되는 것 같다"), with
  the user pointing at commit `a4c48b6` ("libcxx is working on macos") as
  the last-known-good point to diff against. Reproduced directly: linking
  `libc++.1.0.dylib` failed with `ld: fixup error (kind=arm64_adrp_lo12)
  at '__ZNSt3__18__get_dbEv'+0x54 from debug.cpp.o, target ___dso_handle
  does not have address`, on a genuinely fresh build (wiped `external/
  llvm-runtimes/{build,install,libcxx}` and rebuilt from nothing --
  ruling out a stale-`out/` artifact before looking for a source-level
  cause, per this file's own repeated lesson about exactly that trap).

  Root cause, confirmed via `nm` rather than guessed: `debug.cpp.o`
  (part of `libcxx` itself -- `src/debug.cpp`'s `__get_db()`) carried an
  undefined `___dso_handle` with no definition anywhere in `libcxx`'s own
  build. This project's own `crt1.o` never defines `__dso_handle` at all
  (Bionic's real `crtbegin`/`crtend` split was never imported), so
  nothing else was going to provide it either. The 2026-08-21 libcxx/
  libcxxabi/libunwind restructuring (`6b57e48`) had already found and
  fixed this exact gap for `libcxxabi` (a `src/__crt_dso_handle.cpp`
  shim, `extern "C" void* __dso_handle = &__dso_handle;`, wired into
  `LIBCXXABI_SOURCES` via a `libcxxabi/recipe.json` patch) -- but never
  gave `libcxx` its own copy. That's not a redundant near-miss: per the
  Itanium C++ ABI, `__dso_handle` identifies one specific DSO/shared-
  object image, so `libcxxabi`'s copy was never going to satisfy a
  reference from a *different* shared library (`libc++.dylib`) at
  `arm64_adrp_lo12` (a direct, same-image relocation, not an indirect
  cross-image one) -- `libcxx` genuinely needed its own.

  This also means the "`CRT_USE_IMPORTED_LIBCXX=ON` is verified on
  macOS" claim TODO.md recorded the same day (`6b57e48`'s own entry) was
  wrong, or at best verified against a stale `install/lib/libc++.dylib`
  left over from before the libcxxabi/libcxx recipe.json split -- a
  third instance of this project's own documented "a local dev tree
  with an existing `out/` directory is not a reliable test of new
  CMake-level wiring" trap (`TODO.md`'s own note), this time hiding a
  build failure behind stale *install* artifacts rather than stale
  *configure* state. TODO.md's claim corrected in the same pass.

  Fixed by giving `libcxx/recipe.json` the identical shim
  (`extra_files`: `src/__crt_dso_handle.cpp`, same content as
  `libcxxabi`'s). No `patches` entry was needed the way `libcxxabi`
  required one: `libcxx`'s own `lib/CMakeLists.txt` already does
  `file(GLOB LIBCXX_SOURCES ../src/*.cpp)`, which picks up the new file
  automatically. Each shared library ends up with its own, image-local
  `___dso_handle` definition -- exactly the Itanium ABI's intent, and
  confirmed via `nm` on both `libc++abi.dylib` and (now) `libc++.dylib`.
  For static linking, no duplicate-symbol risk exists either: whichever
  archive's own copy is needed first to satisfy an outstanding
  `__dso_handle` reference is the only one ever pulled in (`libc++.a`
  appears before `libc++abi.a` on `tools/crt-c++`'s own macOS static
  link line, so `libcxx`'s copy resolves `debug.cpp.o`'s own reference
  before `libcxxabi`'s copy is ever needed for anything).

  Verified on this real macOS aarch64 host, genuinely fresh (wiped
  `build`/`install`/staged `libcxx` source, no incremental artifacts
  involved): `crt-libcxx-build` now exits 0, `nm` confirms
  `libc++.dylib` carries its own defined `___dso_handle`, and the
  default `cmake --build`/`ctest` workflow (which doesn't touch
  `crt-libcxx-*` targets at all) stays green at 104/104 throughout, no
  regressions. Also re-ran `crt-libcxx-sysroot` (fresh stage of the
  fixed `libc++.{dylib,a}`/`libc++abi.{dylib,a}` into the CRT sysroot)
  and `crt-libcxx-smoke` for real this time, not trusting the earlier
  claim: both the static and shared `imported_libcxx_test` builds link
  and run to completion, printing `imported_libcxx_test: ok` -- the real
  vector/string/RTTI/exception-throw-catch coverage TODO.md's checklist
  originally claimed is now genuinely confirmed, not just documented.

- **Fixed a live, 7-commit-long GitHub Actions CI regression (`linux-amd64`,
  `linux-arm64`, `windows-x64` all red since `940af4c`) plus the
  `math_test` `SegFault` the user hit locally on Windows, prompted by
  "macOS에서 skia와 libcxx 빌드를 구성했는데, 타 OS에 대한 고려가 좀
  부족했던 것 같다" -- verified for real on a second OS via this session's
  newly available WSL Ubuntu access, not just reasoned about. Two
  unrelated root causes, both from the 2026-08-18 Skia/libc++ bring-up
  push sequence:
  1. **`fma()`/`fmaf()` self-recursion on Windows x86_64**
     (`libm/src/basic.c`), `STATUS_STACK_OVERFLOW`. A *different* function
     pair from the already-fixed 2026-08-18 `fmal()`/`lrint()`-family
     self-recursion bugs (those were real Linux aarch64 findings; `fma()`/
     `fmaf()` there use genuine hardware FMADD, so they never hit this),
     but the same root-cause *class*: `__builtin_fma`/`__builtin_fmaf`
     only lower to a native FMADD/VFMADD instruction when the compile
     target defines `__FMA__` (`-mfma`, or an `-march=` implying it) --
     unconditional on aarch64 (ARMv8 baseline), but x86_64 FMA3 is an
     *optional* CPU feature this project's build opts into on no host.
     Without it, the builtins fall back to libcalls literally named
     `fma`/`fmaf` -- exactly the functions being defined here, so real,
     stack-growing infinite self-recursion, not a hang (`fmal()`'s bug
     was a tail-branch to itself instead of a real call, since long
     double has no matching hardware op on any architecture -- a related
     but distinct failure signature). Found by bisecting `math_test.c`
     with `fprintf` checkpoints (no interactive debugger available),
     isolating to a minimal `mini_fmin_test.c` repro, and confirming via
     `llvm-objdump -d --x86-asm-syntax=att` that the compiled `.obj`'s
     `callq` instruction resolves to the function's own symbol. Fixed by
     gating the `__builtin_fma`/`__builtin_fmaf` path behind
     `#if defined(__FMA__) || defined(__aarch64__) || defined(_M_ARM64)`,
     falling back to `x*y+z`/`(float)((double)x*(double)y+(double)z)`
     otherwise -- matching this file's own already-accepted precision bar
     for `fmal()` right below it. Verified: isolated repro passes both
     broken and fixed states; full `math_test` clean; Windows
     `ctest --preset windows-host-ninja-debug` 119/119 after a genuine
     `cmake --fresh` reconfigure; reproduced identically on Linux amd64
     via a clean WSL Ubuntu clone (104/104), confirming the reasoning that
     this is a compiler/architecture fact, not an OS-specific one.
  2. **Linux CI has never installed `libc++-dev`/`libc++abi-dev`**
     (`.github/workflows/ci.yml`), for the entire history of that file --
     confirmed via `git log -p` on the file. `CMakeLists.txt` has forced
     `-stdlib=libc++` for `CRT_TARGET_OS=linux` since `597280c`, whose own
     2026-08-18 HISTORY.md entry shows `libc++-18-dev` was installed *by
     hand* on the real Linux aarch64 host that validated that fix -- but
     that manual step was never carried into the CI workflow file itself.
     Without the dev package, CMake's own one-time CXX-compiler-ABI-
     detection step (which runs before this project's own freestanding
     flags ever apply) fails outright linking a trivial program
     (`ld: cannot find -lc++`), not the graceful "falls back to GNU
     libstdc++" behavior an earlier `CMakeLists.txt` comment assumed for
     this exact scenario -- that assumption does not hold on real modern
     Clang (reproduced directly on both Clang 10 and Clang 18 via WSL
     Ubuntu). No `gh` CLI or repo admin rights were available in this
     session, so root-caused entirely from the public GitHub Actions REST
     API (`/actions/runs`, `/actions/runs/{id}/jobs`) rather than raw
     logs: `linux-amd64`/`linux-arm64` fail at the "Configure, build, and
     test" step in well under a build's worth of time (consistent with an
     early configure failure), while `windows-arm64` staying green the
     whole time was the clue that separated this from the `fma` bug
     above (aarch64 never hits the `__FMA__` gap). Verified for real
     without root: `apt-get download` (no sudo needed) fetched
     `libc++-18-dev`/`libc++abi-18-dev` plus their actual runtime
     dependents (`libc++1-18`, `libc++abi1-18`, `libunwind-18`/-dev,
     pulled by hand since `apt-get download` does not resolve
     dependencies the way a real `apt-get install` would) into a local
     prefix; the same `-stdlib=libc++` link that failed before now
     succeeds and the resulting binary runs. A full from-clean
     `cmake --preset linux-host-ninja-debug` configure/build/`ctest`
     against this local prefix (`CPATH`/`LIBRARY_PATH` env vars, since
     system-wide install needs root this session doesn't have) passed
     104/104, including `math_test_runs`. Fixed by adding
     `libc++-dev libc++abi-dev` (unversioned package names, which track
     whatever version the unversioned `clang` package defaults to on a
     given Ubuntu release, so no explicit version pin is needed) to
     `ci.yml`'s Linux install step; `--no-install-recommends` stays safe
     since it only skips Recommends, not the hard Depends that pull in
     the actual runtime `.so` files. Not yet confirmed green on a real
     GitHub Actions run (requires an actual push).
  3. **Separately, fixed a real (if non-fatal) Windows CMake configure
     warning**: `CRT could not locate the Clang C++ standard-library
     include directory on Windows; continuing with limited C++ support`
     (`CMakeLists.txt:207`). Root cause: `CMAKE_CXX_IMPLICIT_INCLUDE_
     DIRECTORIES` is genuinely empty for a Windows Clang install
     targeting the default `*-pc-windows-msvc` triple -- confirmed
     directly against this machine's own generated
     `CMakeCXXCompiler.cmake` -- not a regex-matching miss like the
     already-fixed Linux multiarch gap right above this code (that
     variable really is `""`; CMake's own implicit-include-directory
     detection does not run the same way for a Clang that simulates
     MSVC, which CMake expects to rely on the `INCLUDE` environment
     variable instead, something this freestanding build intentionally
     never sets). The existing `VC/Tools/MSVC/.*/include$` regex was
     already written to handle this exact path shape but never got real
     data to test against. Fixed by probing the compiler directly when
     the CMake-detected list comes up empty on Windows (`clang++ -E -x
     c++ -v` against an empty translation unit, parsing its own
     `#include <...> search starts here` listing, then filtering the
     result through `EXISTS()` since the captured block also contains
     one harmless non-path header line). Verified: the warning is gone
     on a genuine `cmake --fresh` reconfigure, and the probe correctly
     lists all 7 real search paths (Clang resource dir, MSVC STL,
     5 Windows Kits UCRT/shared/um/winrt/cppwinrt dirs); the subsequent
     full `cmake --build` still succeeds, including `c++.dll`/
     `crtgfx.dll`/`crtjs.dll`/`crtmedia.dll`.
  A genuine second-OS Linux verification for this pass ran on a clean WSL
  Ubuntu clone rather than the shared `/mnt/c` Windows working tree: this
  machine's Git for Windows has `core.autocrlf=true`, so files checked out
  there (including `tools/crt-c++`'s `#!/bin/sh` shebang) carry CRLF line
  endings that break a direct Linux exec of that script -- a real,
  environment-local interop trap, not a repo bug (there is no
  `.gitattributes` forcing LF, and a genuine fresh CI clone on a native
  Linux filesystem is unaffected either way).

- **Restructured the libcxx/libcxxabi/libunwind build from three hardcoded
  Python scripts into a declarative, per-component recipe.json** --
  `libstdc++/third_party/{libunwind,libcxxabi,libcxx}/recipe.json`, each
  next to the component it describes, following the same schema/spirit as
  `porting/recipes/*.json` but driven by a dedicated new
  `tools/crt-libcxx-build.py` engine rather than reusing the generic
  porting engine (a deliberate choice: the two build shapes are genuinely
  different -- git source with a sparse subpath vs. tarball+sha256, three
  CMake projects with a real cross-referencing build order vs. one
  independent `./configure`+`make` each). Replaces (deleted)
  `tools/fetch_libcxx_runtimes.py` and `tools/build_libcxx_runtimes.py`,
  whose every CMake flag/URL/branch was previously buried in Python with
  no single place to see "what does building libcxx actually take."
  `tools/install_libcxx_runtimes.py` and `tools/test_libcxx_runtime.py`
  needed no changes (the former already matched libunwind-named files
  generically; the latter always drives the final link through
  `tools/crt-c++`, which now handles libunwind's link dependency itself).

  Real libunwind is now part of this build (previously never fetched or
  built at all, only documented as a future gap): AOSP
  `toolchain/llvm-project/libunwind`, fetched via a partial clone + cone
  sparse-checkout scoped to just the `libunwind` subpath (confirmed ~25MB,
  not the full LLVM monorepo). Enabled on Linux and Windows; deliberately
  *not* built on macOS (Darwin's own `libSystem` unwinder already works,
  confirmed by the existing macOS `crt-libcxx-smoke` pass with no
  libunwind at all) and deliberately *not* satisfied via a host-installed
  Linux package either, even though one is easy to reach for (e.g.
  Ubuntu's `libunwind-dev`) -- that would put a `DT_NEEDED` on a library
  this project does not own the build of into every C++ binary it ships,
  the same class of thing already avoided elsewhere (`-lpthread`/`-lrt`
  absorbed into libc; `CRTGFX_ENABLE_SKIA`'s own "never link host libc++
  as a substitute" policy) and a genuinely different case from CI's
  libc++-dev/libc++abi-dev install just above, which is build-time-only
  and never ends up linked into anything this project ships. `libcxxabi`'s
  own recipe.json deliberately does *not* wire `LIBCXXABI_USE_LLVM_UNWINDER`/
  `LIBCXXABI_LIBUNWIND_INCLUDES` at all: that CMake option only changes
  libcxxabi's own compile-time API selection, and the automatic CMake
  `TARGET unwind_shared` link-wiring it half-depends on only ever fires
  inside a combined single-configure LLVM "runtimes" build (impractical
  here, since AOSP's libcxx/libcxxabi/libunwind are three separate git
  repos) -- so the real link dependency is added at the final consumer
  instead, in `tools/crt-c++`'s own per-OS `cxx_runtime_libs`.

  Windows verification surfaced three real, previously-hidden toolchain
  bugs, each root-caused from first principles (isolated PowerShell/mksh
  repros, not guessed), all now fixed and documented in the code:
  1. **mksh's own exec resolution requires a forward slash to recognize a
     literal path.** A bare Windows backslash path (`C:\Program
     Files\LLVM\bin\clang++.exe`, exactly what `shutil.which()`/`Path()`
     produce on Windows) has none, so mksh instead treats the *entire*
     string as a single command name to search `$PATH` for and reports it
     "inaccessible or not found" -- even though the file genuinely exists.
     An 8.3 short-path workaround was tried first and looked plausible
     (matches an existing `tools/crt-port-build.py` precedent for a
     different problem) but was empirically wrong: the short path failed
     identically. Isolating this down to a two-line repro script run
     directly via `mksh.exe` (bypassing both this project's own scripts
     and PowerShell's own quoting, which independently mangled an earlier
     attempt at this same repro) pinned the real cause. Fixed by returning
     forward-slash paths from every host-tool lookup.
  2. **`CMAKE_CXX_COMPILER_ARG1` is not reliably honored by every
     CMake-driven TryCompile**, even though it works for CMake's initial
     compiler *identification* probe. Confirmed directly: the actual
     generated build command for "Detecting CXX compiler ABI info" ran
     bare `mksh.exe -D__BIONIC__ ...` with no `crt-c++` argument at all,
     while the equivalent C compiler probe in the very same configure run
     correctly included `crt-cc`. Fixed by abandoning the ARG1 mechanism
     entirely in favor of two small native launcher wrappers,
     `tools/crt-cc.cmd`/`tools/crt-c++.cmd` (real, directly-executable
     `.cmd` files CMake can point `CMAKE_C_COMPILER`/`CMAKE_CXX_COMPILER`
     straight at, reading which `mksh.exe` to invoke from a `CRT_MKSH_EXE`
     environment variable instead of a CMake cache entry -- so there is
     nothing left for CMake's own ARG1 handling to silently drop).
  3. **libunwind's own `CMakeLists.txt` is not self-contained.** Unlike
     the sibling libcxx/libcxxabi checkouts (which still carry their own
     standalone-build `project()` bootstrap), it unconditionally assumes
     it is being `add_subdirectory()`'d from the upstream LLVM monorepo's
     `runtimes/CMakeLists.txt` driver and `include()`s several shared LLVM
     CMake utility modules (`HandleCompilerRT.cmake`,
     `LLVMCheckCompilerLinkerFlag.cmake` from a sibling `cmake/`;
     `HandleFlags.cmake` from `runtimes/cmake/`) that a sparse checkout of
     just `libunwind/` never carries, surfacing as three sequential
     "include could not find requested file" errors (one per missing
     module, only discovered one at a time as each earlier `include()`
     itself got past its own error). Fixed two ways together: a small
     project-owned driver (`libstdc++/third_party/libunwind/standalone/
     CMakeLists.txt`) that calls `project()` for real and
     `add_subdirectory()`s the actual fetched source, referenced via
     `recipe.json`'s new `cmake.driver` field; and a new
     `extra_checkout_dirs` recipe field that copies the two missing shared
     directories as siblings of `libunwind/` under the shared source root,
     matching the relative path shape libunwind's own `CMakeLists.txt`
     expects.

  With these three fixed, `crt-libcxx-configure` succeeds for all three
  recipes on Windows, and `crt-libcxx-build` gets well into compiling
  libcxxabi before hitting a *different*, genuine class of problem: real
  C++ ABI source-portability gaps in AOSP's libcxxabi/libcxx for a
  mingw-style Windows target (never built for one before). Deliberately
  **not** patched in this same pass -- scoped, reasoned about, and left as
  an explicit, documented follow-up rather than rushed; see `TODO.md`'s
  own C++ runtime prerequisite section for the specifics
  (`_LIBCPP_WIN32API` conflating still-correct Windows facts with
  subsystems that need redirecting to this project's own PAL) and why
  Linux was not attempted either once this was found on Windows first.
  Full local `ctest` (119/119 on Windows) confirms this restructuring
  itself introduced no regression to the default build/test workflow,
  which never touches the `crt-libcxx-*` targets.

- **Resumed the libunwind/libc++/libc++abi Windows build the same day,
  following a user-provided review of the plan** (correctly separating the
  "C++ exceptions actually working" track from a different "debug
  backtrace capture" track the review's own suggestions were really aimed
  at -- kept out of this work entirely per explicit instruction, recorded
  in `TODO.md`). Worked through the revised, ordered plan's first three
  steps for real, executing and fixing forward rather than reasoning in
  the abstract -- six more genuine bugs found and fixed along the way, on
  top of the three from the recipe.json restructuring earlier the same
  day:
  1. **`-fdwarf-exceptions` confirmed and applied for real** (previously
     just a hypothesis): empirically verified with a two-line `-dM -E`
     probe that this flag stops clang from predefining `__SEH__` for the
     `*-w64-mingw32` target at all, then applied to the actual build. Had
     to go on *both* `CMAKE_CXX_FLAGS` and `CMAKE_C_FLAGS` -- libunwind,
     unlike libcxx/libcxxabi, has several plain C source files
     (`UnwindLevel1.c`, `UnwindLevel1-gcc-ext.c`, `Unwind-sjlj.c`) that
     reach the same `__SEH__`-gated `<windows.h>` include via `unwind.h`
     transitively, confirmed still failing on the CXX-only flag alone
     after `cxa_personality.cpp`/`Unwind-seh.cpp` were already fixed by
     it. With both flags set, `cxa_personality.cpp`, `Unwind-seh.cpp`,
     `UnwindLevel1.c`, `UnwindLevel1-gcc-ext.c`, `Unwind-sjlj.c`, and
     `Unwind-EHABI.cpp` all compile clean with zero `<windows.h>` errors.
  2. **Real `_aligned_malloc`/`_aligned_free`** added to this project's
     own libc (`libc/src/malloc.c`), thin wrappers over the existing
     `posix_memalign()` (not `aligned_alloc()` -- its stricter C11 "size
     must be a multiple of alignment" contract does not match the looser
     real `_aligned_malloc()` one, and libc++/libc++abi's own
     `operator new(size, align_val_t)` callers do not guarantee that
     relationship). First placed in `include/malloc.h`, which turned out
     wrong and had to move to `include/stdlib.h`: libc++/libc++abi's own
     source (`stdlib_new_delete.cpp`, `fallback_malloc.cpp`, libcxx's own
     `src/new.cpp`) never `#include <malloc.h>` at all, only
     `<cstdlib>`/`<new>` -- matching how a real MSVC `<stdlib.h>` declares
     these symbols directly, confirmed by the build still failing with
     "undeclared identifier" from `<malloc.h>` alone until moved.
  3. **libunwind wired as a real dependency of libcxxabi, correcting an
     earlier (wrong) assumption from the same day's recipe.json
     restructuring.** That earlier work assumed a link-time-only reference
     in `tools/crt-c++` would be enough, based on how the plain default
     (non-LLVM-unwinder) Itanium ABI surface libcxxabi compiles against
     needs no libcxxabi-side CMake change. True for the *static*
     `cxxabi_static`/`libc++abi.a` build (confirmed: it links completely
     clean with zero libunwind anywhere in scope -- a static archive step
     never checks for undefined references) but wrong for the *shared*
     `cxxabi_shared`/`libc++abi.dll` build, which -- unlike an ELF `.so`
     -- must resolve every referenced symbol at its own link time on
     Windows, confirmed directly via `ld.lld: error: undefined symbol:
     _Unwind_Resume` and nine siblings (plus vtable-for-`std::logic_error`/
     `std::bad_cast`/etc.) the first time `LIBCXXABI_ENABLE_SHARED=ON`
     actually got exercised for real. Fixed by adding `"dependencies":
     ["libunwind"]` to libcxxabi's own recipe.json (build order:
     libunwind, then libcxxabi, then libcxx) and a windows-only
     `CMAKE_SHARED_LINKER_FLAGS` override linking libunwind's own shared
     import library directly into `libc++abi.dll` -- deliberately the
     *shared* import library, not the static archive, so the actual
     unwinder code lives in exactly one place (`unwind.dll`) with
     `libc++abi.dll` importing it, avoiding a duplicate-symbol risk
     against the separate `libunwind.a` `tools/crt-c++`'s own
     *static*-linkage path still adds directly at the final consumer
     (which remains genuinely link-time-only as originally designed).
  Along the way, three more real, previously-hidden toolchain bugs
  surfaced and got fixed, all in `tools/crt-cc`/`tools/crt-c++`
  themselves (so they benefit every future recipe/port, not just this
  one):
  - **`-Wl,/libpath:...` was folded unquoted into `$libs`**, which is
    expanded unquoted later (relying on word splitting, like every other
    flag accumulator in these scripts) -- silently tearing a
    space-containing Windows SDK path (`C:\Program Files (x86)\...`) into
    multiple broken argv entries the moment a real link actually needed
    it (`clang++: error: no such file or directory: 'Files'`). The
    existing `resource_dir` special-casing already showed the right
    pattern (pass it as its own quoted argument directly on the exec
    line, not through an unquoted-expansion variable) -- applied the same
    treatment here, in both scripts.
  - **CMake's own Windows-Clang toolchain module silently injects the
    standard MSVC library set** (`kernel32.lib user32.lib gdi32.lib
    winspool.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comdlg32.lib
    advapi32.lib`) via `CMAKE_CXX_STANDARD_LIBRARIES`/`CMAKE_C_STANDARD_
    LIBRARIES` unless explicitly cleared -- exactly what the top-level
    `CMakeLists.txt` already does for this project's own targets, but
    `tools/crt-libcxx-build.py`'s own `common_cmake_args()` never
    replicated. Confirmed directly: `ld.lld: error: unable to find
    library -lkernel32` and nine siblings, none of which exist as bare
    `-l`-searchable names in this sysroot (`kernel32.lib` is linked by
    its own full path elsewhere; this project never needs GDI/OLE/
    shell32 at all). Fixed by clearing both, matching the existing
    top-level precedent exactly.
  - **libcxxabi/libcxx/libunwind's own upstream CMake unconditionally
    links a real mingw-w64 distribution's CRT stub import libraries**
    (`mingw32`/`moldname`/`mingwex`/`msvcrt`/`advapi32`/...) whenever
    CMake's built-in `MINGW` variable is true -- which it is for any
    `*-w64-mingw32` target regardless of whether a real mingw-w64
    distribution actually backs it. This project's own target is ABI-
    compatibility-only, never a real mingw-w64 install, so none of these
    exist in the sysroot and never will (`lld: error: unable to find
    library -lmingw32` and 15 siblings). Fixed with a small, documented
    `patches` entry (using the mechanism already proven for the
    `__crt_dso_handle.cpp` shim) in all three recipe.json files, no-op'ing
    the exact upstream `if (MINGW) ... endif()`/`add_library_flags_if(
    MINGW ...)` blocks -- `tools/crt-c++` already supplies the complete
    real Windows link line.
  With all nine bugs from the day's two passes fixed, every libunwind
  source file now compiles clean **except one**: `libunwind.cpp`'s
  `findUnwindSections()`, via `AddressSpace.hpp`'s own genuine (not
  `__SEH__`-related) `#elif defined(_LIBUNWIND_SUPPORT_DWARF_UNWIND) &&
  defined(_WIN32)` branch, which needs real PE/COFF module-enumeration
  APIs this project has never had a reason to declare before
  (`EnumProcessModules` from `psapi.h`, plus the real `IMAGE_DOS_HEADER`/
  `IMAGE_NT_HEADERS`/`IMAGE_FILE_HEADER`/`IMAGE_SECTION_HEADER`/
  `IMAGE_FIRST_SECTION` PE/COFF header layout from `winnt.h`). Deliberately
  left for a follow-up rather than rushed -- see `TODO.md`'s own C++
  runtime prerequisite section, step 3, for the scoped next step (a small
  project-owned header shim declaring the minimal subset needed, matching
  how `libc/src/arch/windows/` already declares raw Win32 function
  prototypes without `<windows.h>`, not a patch to libunwind itself).
  Full local `ctest` (119/119 on Windows, fresh `cmake --fresh`
  reconfigure) confirms none of this pass's changes introduced a
  regression to the default build/test workflow either.

## 2026-08-18

- **Completed the first runnable Android libc++/libc++abi environment on
  macOS.** `crt-libcxx-configure`/`build` now produce both static and shared
  libraries through the CRT wrappers, `crt-libcxx-sysroot` stages the full
  header/runtime set, and `CRT_USE_IMPORTED_LIBCXX=ON` installs the imported
  dylibs into the Android-like rootfs. The new `crt-libcxx-smoke` target runs
  static and shared `std::vector`, `std::string`, RTTI, and a real
  `std::runtime_error` throw/catch. The work exposed and fixed Bionic
  personality, API-level, aligned-allocation, C99 math, and Darwin symbol
  interposition gaps. Static Darwin links also stage libc++'s upstream
  not-weak policy so libSystem cannot interpose its C++ allocator symbols;
  notably, legacy `-lpthread`/`-lrt` are now absorbed into
  libc so host libSystem pthread symbols cannot consume Bionic-shaped objects.
  The complete 104-test macOS workflow passes with imported rootfs mode on.
  Linux/Windows remain open pending a CRT-built current AOSP LLVM libunwind;
  `platform/external/libunwind` was confirmed retired, so no host unwinder
  fallback was introduced.

- **Fixed `cmake --workflow --preset linux-host-ninja-debug` failing
  outright on a real Linux aarch64 host, then two more real bugs the fix
  exposed underneath -- a genuine porting-loop chain, not one bug.**
  Reported: `libstdc++/src/new_delete.cc` failed with `'bits/c++config.h'
  file not found`, then (after that) `'features.h' file not found`, then
  (after installing `libc++-18-dev` per this session's own suggestion,
  matching README.md's stated C++ runtime direction) `math_test` hung at
  100% CPU forever, then (after fixing that) crashed with `SIGSEGV`.

  1. **Root cause of the original error**: `CMakeLists.txt`'s
     `CRT_CXX_STANDARD_INCLUDE_DIRS` detection loop filtered
     `CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES` with a regex anchored to the
     literal substring `include/c++` -- which matches a plain
     `/usr/include/c++/13`, but not real Debian/Ubuntu GCC's *second*,
     target-triple-specific directory that actually carries the
     machine-generated `bits/c++config.h`
     (`/usr/include/aarch64-linux-gnu/c++/13` on aarch64,
     `x86_64-linux-gnu` on x86_64) -- confirmed directly via `clang++ -E
     -x c++ -v /dev/null` and `CMakeCXXCompiler.cmake`'s own
     `CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES`, both of which do list it.
     Broadened the regex to match a `c++` path *component* anywhere
     (`/c[+][+](/|$)`), not just after a literal `include/` prefix.
  2. **That alone wasn't enough**: with the include path fixed, GNU
     libstdc++'s own header wrappers (`<cstdlib>` et al) still failed --
     they need real, private glibc-internal headers (`<features.h>` and,
     transitively, `bits/os_defines.h`'s own further requirements) this
     project deliberately never exposes, since it owns its own
     Bionic-compatible libc, not glibc. This project's actual stated C++
     runtime direction is libc++, not GNU libstdc++ (README.md's "Stack"
     section; the `CMAKE_CXX_COMPILE_OBJECT` rule override already
     assumed libc++'s own `include_next` relationship with this
     project's C headers) -- Apple Clang defaults to libc++
     unconditionally on macOS, but a stock Ubuntu/Debian Clang install
     defaults to GNU libstdc++ instead, `-stdlib=libc++` has to be
     requested explicitly (confirmed directly: even after installing
     `libc++-18-dev`, `clang++ -E -x c++ -v` still resolved to GNU
     libstdc++'s paths without that flag). Fixed by setting
     `CMAKE_CXX_FLAGS=-stdlib=libc++` on Linux only, but *only* right
     before `project()`/`enable_language(CXX)` (the one point it's
     actually consulted, for the one-time
     `CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES` detection) -- reset back to
     empty immediately after, since every real per-target C++ compile
     already gets its own explicit `-isystem`/`-nostdinc++` flags with no
     further use for `-stdlib=` itself, and leaving it in `CMAKE_CXX_FLAGS`
     made Clang's own `-Werror,-Wunused-command-line-argument` reject it
     on every compile-only (`-c`) translation unit.
  3. **Real libc++ has the same `<features.h>` need, for a different,
     legitimate reason**: LLVM libc++'s own `<__config>` unconditionally
     does `#if defined(__linux__) #include <features.h> ... #endif`
     (checked directly, no glibc-vs-not guard of its own) to detect
     `__GLIBC_PREREQ` for its own internal ABI-compat branching. Checked
     Android Bionic's own real `include/features.h` for the correct
     answer rather than improvising one: it is a one-line
     glibc-source-compatibility shim, `#include <sys/cdefs.h>` and
     nothing else -- no `__GLIBC_PREREQ`, so libc++ correctly falls back
     to its conservative "not glibc" branch. Added the exact same thing
     as `include/features.h` (this project already had `<sys/cdefs.h>`).
     With that, the *original* C++ build error was fully resolved --
     `cxx`/`cxx_shared` (`libstdc++/src/new_delete.cc`) and every C++ test
     (`cxx_allocation_test`, `cxx_frontend_test`, `cxx_runtime_test`)
     built and linked clean.
  4. **`math_test` then hung at 100% CPU, forever** -- a *different*,
     unrelated, genuinely pre-existing `libm` bug the fixed C++ build
     simply let the workflow reach for the first time (macOS's own ARM64
     `long double` is exactly the same 64 bits as `double`, so it never
     exercises this path at all). Root-caused with `clang -S` on the
     function in isolation, not guessed: `fmal()`'s entire body compiled
     down to a single instruction, `b fmal` -- an unconditional branch to
     itself. Real Linux aarch64/x86_64 `long double` (128-bit quad /
     80-bit extended) has no native hardware FMA instruction the way
     `double`/`float` do (confirmed `fma()`/`fmaf()` *do* compile to a
     single real FMADD/VFMADD instruction each), so Clang lowers
     `__builtin_fmal()` into a call to the runtime support symbol
     literally named `fmal` -- which this project's own `fmal()` *is*,
     making its `return __builtin_fmal(x, y, z);` body genuine, confirmed
     infinite self-recursion. Fixed with a plain `x * y + z` (not a true
     single-rounding-step FMA -- a known, documented precision
     simplification, not a claim of bit-exact FMA semantics, matching
     this file's existing bar for long double elsewhere).
  5. **Then `SIGSEGV`, immediately after "fmal" fix, in the very next
     check** -- root-caused the same way: `clang -S` on `lrint()` (plain
     `double`, not even the `long double` variant) showed the exact same
     self-referencing lowering, `bl lrint` this time (a real call with a
     saved return address, not a tail-call branch, so this class crashes
     from stack overflow instead of spinning forever -- otherwise
     identical bug). Confirmed this is *every* `__builtin_lrint`/
     `__builtin_lround`/`__builtin_llround` (and their `f`/`l` suffixed
     variants, 9 functions total, double included, not just long double)
     the same way, regardless of precision -- plausibly because `lrint()`
     specifically must honor the *current dynamic* `fesetround()` mode,
     which has no single fixed instruction to lower to at compile time.
     Reimplemented all 9 in terms of this project's own already-real
     `floor()`/`ceil()`/`trunc()`/`round()` (confirmed working --
     `round()` and `fegetround()` are each checked earlier in
     `math_test.c`, both already passed by the time this bug was
     reached) plus `fegetround()`: `lround()`/`llround()` always round
     half away from zero regardless of the current mode (matching
     `round()`'s own real C99 semantics exactly, so a direct call
     suffices); `lrint()`/`lrintl()` switch on `fegetround()`'s four
     modes for real. One documented simplification: `lrint()`'s
     `FE_TONEAREST` case uses `round()`'s away-from-zero tie-breaking
     rather than IEEE round-to-nearest-ties-to-even -- only observable on
     an exact `.5` input, matching this file's existing precision bar.

  **Verified end-to-end on the real Linux aarch64 host that reported the
  original failure**: the user's own exact command,
  `cmake --workflow --preset linux-host-ninja-debug`, now completes
  clean start to finish -- configure, full build (all 104 registered
  tests link, including every C++ test and `math_test`), and
  `ctest --preset linux-host-ninja-debug` 104/104. Isolated standalone
  repros (`clang -S` on each function; direct calls to `atanl`/`atan2l`/
  `fmal`/`lrint`/`lround`/`llround` and their `f`/`l` variants through the
  real `crt-cc` toolchain) independently confirmed each fix before
  re-running the full workflow. `termios_line_control_test`/
  `crtgfx_window_smoke` re-confirmed passing for real (via `script -qc
  <binary> /dev/null`, this environment's own way of getting a real pty)
  after the full rebuild, since both depend on the same `libm`/C++
  toolchain path. Requires `libc++-18-dev`/`libc++abi-18-dev` (or
  whichever matches the active Clang major version) installed on Linux --
  a real, one-time system package install, not something this session's
  code changes can substitute for; the user installed it directly via
  `sudo apt-get install`.

- **Flattened `src/common/` out of `libcrtgfx`, `libcrtjs`, and
  `libcrtmedia`** -- common/host-independent runtime code now lives
  directly under each library's `src/` (`libcrtgfx/src/window.c`,
  `wayland_weston.c`, `wayland_weston_internal.h`;
  `libcrtjs/src/runtime.c`; `libcrtmedia/src/runtime.c`), with only
  genuinely per-host code staying under `src/arch/{linux,macos,
  windows}/`. Requested directly ("libcrtgfx/src/common 폴더는 굳이
  필요없다", then "libcrtmedia, libcrtjs도 동일한 폴더 구조로 바꾸고") --
  the extra `common/` layer added no real separation once each library
  had exactly one non-arch-specific translation unit (or two, for
  `libcrtgfx`) sitting directly inside it. `git mv` for every moved file
  (history preserved), `CMakeLists.txt` source lists and
  `target_include_directories` updated in all three libraries, and every
  current-state doc/comment reference to the old `src/common/...` paths
  fixed (`libcrtgfx/README.md`, `docs/runtime_roadmap.md`, `TODO.md`'s
  own initial-source-tree diagram and current-baseline bullet,
  `window_cocoa.c`'s own top comment) -- `HISTORY.md`'s own older,
  already-committed entries that mention the old `src/common/...` paths
  are left as-is, since they describe what was true at the time, not
  current-state documentation.

  Verified after a genuine `cmake --fresh` reconfigure (not an existing
  `out/` tree): builds clean on this real macOS host, `crtgfx_window_smoke`
  still passes, full `ctest` suite 103/103, no regressions. `libcrtjs`/
  `libcrtmedia` only had placeholder `runtime.c` stubs at this point (no
  real per-host code yet), so their moves were mechanical and lower-risk
  than `libcrtgfx`'s.

- **Implemented a real macOS `libcrtgfx` host window backend
  (`libcrtgfx/src/arch/macos/window_cocoa.c`), replacing the
  `CRTGFX_ERROR_UNSUPPORTED` stub** -- the last of the three hosts to
  get one, matching the same-day Linux entry below. Per
  `docs/libcrtgfx_wayland_plan.md`'s named architecture reference class
  (Wawona/Wayoa/Cocoa-Way-style projects), this drives real Cocoa
  (`NSWindow`/`NSView`/`CALayer`) from plain C via the Objective-C
  runtime's own C ABI (`objc_msgSend`/`objc_getClass`/
  `sel_registerName`/`objc_allocateClassPair`) rather than the
  Objective-C language -- no `.m` translation unit, no ARC, matching
  `window_win32.c`'s own "no host SDK headers" style (that file never
  includes `<windows.h>`; this one never `#import`s `<Cocoa/Cocoa.h>`).

  Before writing the real backend, validated the technique itself with
  three standalone probes on this real macOS aarch64 build host (not
  reasoned from memory): (1) confirmed every AppKit/Foundation/
  QuartzCore extern symbol used (`NSDefaultRunLoopMode`,
  `kCAGravityResize`, the ObjC runtime functions) actually links and
  resolves; (2) built a full window-creation-through-presentation path
  (`NSApplication` bootstrap, `NSWindow` init with an `NSRect` argument,
  a layer-backed `NSView`, `CGImageCreate` from a raw pixel buffer,
  `layer.setContents:`) and confirmed it runs without an Objective-C
  exception, which also settled the one real architecture-dependent
  question -- AAPCS64 (arm64) resolves an `NSRect`-returning message
  send (`-frame`/`-bounds`) through the ordinary `objc_msgSend` entry
  point, unlike x86_64's SysV ABI, which needs the dedicated
  `objc_msgSend_stret` entry point for a struct this size; (3) confirmed
  `objc_allocateClassPair()`-based runtime class creation correctly
  dispatches method calls and round-trips an ivar, needed for the
  `NSWindowDelegate` class this backend defines to receive
  `windowShouldClose:`/`windowWillClose:`/`windowDidResize:`.

  Performance direction (the concrete lesson taken from the Wawona/
  Cocoa-Way reference class, and the "성능을 끌어올릴 수 있는 구현
  방향" the request asked for): presents each frame by building a
  `CGImage` from the caller's BGRA8888 buffer and setting it as the
  content view's `CALayer.contents`, which the WindowServer composites
  in hardware the same way it composites every other app's layers --
  not the naive `-drawRect:`+`CGContextDrawImage` invalidation round
  trip. Also a genuine improvement over this project's own Win32
  adapter, not just a port of the same idea:
  `-nextEventMatchingMask:untilDate:inMode:dequeue:` accepts a real
  deadline, so `crtgfx_host_window_dispatch()` blocks the thread
  efficiently in the OS's own run-loop wait instead of Win32's
  `PeekMessage`+`Sleep(1)` busy-poll loop (Win32 has no real
  wait-with-timeout primitive to poll with, so it never had this option).

  `crtgfx_host_window_present_software()` copies the caller's pixel
  buffer into its own allocation each frame (freed via a real
  `CGDataProviderCreateWithData` release callback) rather than wrapping
  the `crtgfx_weston_toplevel` software buffer in place -- that buffer
  can be mutated or reallocated by the very next
  `crtgfx_window_begin_frame()` call before `CALayer`/WindowServer has
  actually consumed the previous frame, a real tear/use-after-free
  hazard a zero-copy wrap would carry. This is more conservative than
  this project's own Linux Wayland backend, whose "buffer torn down on
  the next present rather than gated on `wl_buffer::release`" scope cut
  is already documented there as a real, currently-theoretical tear
  risk -- the macOS backend simply doesn't take on that same risk, in
  exchange for one `memcpy` per frame (still far cheaper than the
  `-drawRect:` path it replaces).

  `libcrtgfx/CMakeLists.txt`: `window_cocoa.c` replaces
  `window_stub.c` as the macOS backend source; `crtgfx`/`crtgfx_shared`
  now link `Foundation`/`AppKit`/`QuartzCore`/`CoreGraphics`/`libobjc`
  on macOS (`PUBLIC`, mirroring how Windows already publicly links
  `user32.lib`/`gdi32.lib` there), since a static archive member's
  undefined AppKit/libobjc symbols only resolve once something actually
  links those frameworks in, and the shared `.dylib` needs them resolved
  at its own link time regardless.

  Initial automated verification directly on this real macOS aarch64
  host, after a genuine `cmake --fresh` reconfigure (not an existing
  `out/` tree), looked complete but wasn't: `crtgfx_window_smoke` passed
  with no `CRTGFX_ERROR_UNSUPPORTED` fallback, `crtgfx_window_demo` ran
  continuously for 8+ seconds with stable CPU/memory and a stable
  open-fd count (no leak) and no crash, and the full `ctest` suite was
  103/103 -- but this session's `screencapture`/`System Events` calls
  both failed with a TCC permission denial in this sandboxed context, so
  none of that evidence actually confirmed the window was rendering
  correctly. It wasn't: the user watched the real screen and reported
  the window showed one frame and then never visibly updated again,
  exactly unlike the same-day Windows/Linux backends ("계속 화면이
  바뀌는데 여기는 멈춰 있다"), even though the process stayed healthy
  and kept calling `crtgfx_host_window_present_software()` every frame
  without any error return -- process-level health metrics alone cannot
  distinguish "presenting new frames correctly" from "silently frozen".

  First hypothesis (implicit `CATransaction` never flushing without
  `-[NSApplication run]`'s own run-loop-idle observer) was reasoned
  carefully but turned out to be **wrong** -- fixing it (explicit
  `CATransaction begin`/`commit`) did not change the symptom, caught by
  re-testing rather than trusting the reasoning. The user then granted
  Screen Recording permission to this session directly
  ("TCC 권한 주었으니 직접 모니터링 하면 된다"), which let this session
  finally see the actual bug for itself: `screencapture` a second apart
  produced byte-identical screenshots, confirming the freeze directly,
  and a live `lldb attach`+`bt` on the running (0% CPU, healthy-looking)
  process showed it blocked inside `-nextEventMatchingMask:untilDate:...`
  with `until_date` printed as a real date roughly **two and a half
  weeks in the future** -- the actual, real root cause. Temporary
  `dprintf()` instrumentation of `crtgfx_now_ms()`'s inputs confirmed
  `clock_gettime()` was returning wildly inconsistent, non-advancing
  timestamps instead of real elapsed time, which fed a bogus multi-day
  interval into `-[NSDate dateWithTimeIntervalSinceNow:]` and made the
  event wait block far past the caller's real `timeout_ms` -- silently
  hanging `crtgfx_window_demo`'s entire frame loop the first time a real
  event backlog needed draining, since the broken deadline check never
  correctly told it to give up and return.

  Real root cause: a **symbol collision**, not an ABI or logic bug.
  `crtgfx_window_demo`/`crtgfx_window_smoke` link this project's own
  libc (`c`) as their actual C runtime, and that static archive's own
  `clock_gettime()` (`libc/src/time.c`) wins symbol resolution over real
  Darwin libSystem's `clock_gettime` at link time. `window_cocoa.c` had
  declared and called `clock_gettime()` with Darwin's *real* raw
  `CLOCK_MONOTONIC` value (`6`, confirmed earlier from this session's own
  standalone probes against the real system headers) -- correct for
  libSystem's implementation, but this project's own macOS
  `__crt_sys_clock_gettime()` only recognizes its *own*, differently-
  numbered clock ids (`include/time.h`: `CLOCK_REALTIME=0`/
  `CLOCK_MONOTONIC=1`) and returns `-EINVAL` for anything else --
  **leaving the output `struct timespec` completely unwritten, i.e.
  stack garbage**, for every single call, which is exactly the
  "wildly inconsistent, non-advancing" pattern the debug prints showed.
  Fixed by using this project's own `CLOCK_MONOTONIC` value (`1`) instead
  of Darwin's raw one, since the symbol that actually ends up linked is
  confirmed (not assumed) to be this project's own implementation, not
  the real Darwin one the original code was reasoning about.

  Re-verified after the real fix with the newly-granted Screen Recording
  permission, closing the loop this session couldn't close on its own
  before: `crtgfx_window_smoke` still passes; three `screencapture`
  screenshots taken one second apart while `crtgfx_window_demo` ran are
  all pixel-different from each other (`md5` confirms three distinct
  hashes), showing the same shifting gradient/diagonal-band pattern
  Windows/Linux produce -- the window genuinely animates now, confirmed
  by this session directly, not only by the user. Full `ctest` suite:
  103/103, no regressions. x86_64 macOS gets the identical fix (the
  symbol-collision mechanism is link-time, not architecture-specific)
  but has not been independently run on real Intel/Rosetta hardware this
  session. See `docs/libcrtgfx_wayland_plan.md`'s "macOS Host Adapter"
  section for the full writeup, and this project's own
  `libc/src/time.c`/`include/time.h` for the clock-id numbering this bug
  fell into.

- **Implemented a real Linux `libcrtgfx` host window backend, replacing the
  `CRTGFX_ERROR_UNSUPPORTED` stub.** Reported: `crtgfx_window_smoke_test`
  failed to even compile on Linux (`-Werror -Wunused-variable` on locals
  the test's Windows-only real-path branch used but the Linux/macOS
  `#else` stub branch never touched) -- fixed as part of the same change
  by giving Linux a real backend instead of just papering over the
  compile error, matching the ask directly ("linux에도 gfx 구현을 하도록
  하자").

  `libcrtgfx/src/arch/linux/window_wayland.c` (new, replacing
  `window_stub.c`) is a hand-rolled Wayland client -- no
  `libwayland-client` dependency, matching `window_win32.c`'s own
  no-host-SDK-headers style -- speaking the real core `wl_display`/
  `wl_registry`/`wl_compositor`/`wl_shm`/`wl_surface` protocol plus the
  stable `xdg_wm_base`/`xdg_surface`/`xdg_toplevel` shell extension
  directly over the `$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY` Unix socket.
  Every opcode and argument layout was taken directly from the real
  upstream `wayland.xml`/`xdg-shell.xml` protocol definitions (fetched
  and cross-checked, not guessed) -- a wrong one is a hard compositor-side
  protocol error, not a soft failure. Software presentation only
  (`wl_shm`), matching the Windows GDI `StretchDIBits` path: a shared
  memory buffer via `memfd_create()` (this project's own, already built
  with exactly this consumer in mind -- see its own doc comment) +
  `mmap()`, handed to the compositor as a `wl_shm_pool`/`wl_buffer` via
  `sendmsg()`/`SCM_RIGHTS` fd-passing (also this project's own, already
  implemented). `CRTGFX_PIXEL_FORMAT_BGRA8888_PREMULTIPLIED` maps exactly
  onto `WL_SHM_FORMAT_ARGB8888`'s real in-memory byte order, no
  conversion needed.

  Needed one build-system fix along the way: `crtgfx_backend_objects` is
  deliberately compiled *outside* `crt_build_flags` (`-nostdinc`/this
  project's own `-isystem` sysroot include), by design, so `window_win32.c`
  can freely declare raw Win32 API surface without this project's own
  headers getting in the way -- but the new Linux file needs the *opposite*:
  it speaks this project's own POSIX/Bionic-shaped surface (`memfd_create()`,
  `sendmsg()`/`SCM_RIGHTS`, `<sys/socket.h>`/`<sys/un.h>`, `mmap()`,
  `poll()`), the same as every other CRT/PAL source file, not a foreign
  host SDK. Without `crt_build_flags`, `#include <sys/socket.h>` silently
  fell through to the host's own real glibc header instead of this
  project's -- caught immediately as `memfd_create`/`ftruncate` "implicit
  function declaration" errors once the host header path was resolved (a
  real ABI-mismatch risk if it had silently "worked": the final
  `-nostdlib`/`-nodefaultlibs` executable only ever links this project's
  own `libc`, not host glibc, so even successfully-compiled host-header
  declarations would have failed to link, or worse, linked against
  structurally-incompatible ABI if the symbol happened to exist under the
  same name). Fixed by attaching `crt_build_flags` to
  `crtgfx_backend_objects`, scoped to Linux only so Windows/macOS keep
  their original host-SDK freedom unchanged.

  Also unified `tests/window_smoke_test.c`'s two separate code paths (a
  Windows-only "exercise everything" branch and an everyone-else
  "just check for CRTGFX_ERROR_UNSUPPORTED" branch, which is what left
  the unused Linux/macOS locals from the original report) into one: any
  OS may now return `CRTGFX_ERROR_UNSUPPORTED` as a legitimate "no usable
  backend in this environment" skip (matching how this project's other
  environment-dependent tests, e.g. `termios_echo_roundtrip_test.c`,
  already skip rather than fail), but once `crtgfx_window_create()`
  reports success on *any* host, the full real path (get real size, draw,
  present, pump events, destroy) is required to work, not just tolerated.

  **Verified on a real GNOME/Mutter Wayland session, not just compiled**:
  `crtgfx_window_smoke` passes the complete real path end to end,
  including a real compositor-negotiated `xdg_surface::configure`/
  `ack_configure` handshake and a real `wl_shm` buffer commit;
  `crtgfx_window_demo` ran continuously for several seconds (hundreds of
  present cycles) with a stable open-fd count (`/proc/<pid>/fd` checked
  directly -- no leak from the per-frame `memfd_create()`/`wl_shm_pool`/
  `wl_buffer` churn) and no crash or compositor-initiated connection
  kill (which is exactly what a real protocol error would have caused,
  immediately). Graceful `CRTGFX_ERROR_UNSUPPORTED` fallback confirmed
  directly both with `$WAYLAND_DISPLAY`/`$XDG_RUNTIME_DIR` unset
  (the headless-CI shape) and pointed at a nonexistent socket path, so
  CI keeps passing exactly as before. Full `ctest` green throughout
  (grew to 103/103 with `crtgfx_window_smoke_runs` now exercising the
  real path instead of the stub). Not yet exercised on a non-GNOME/Mutter
  compositor (a wlroots-based one like Sway, or KDE's KWin) -- expected
  to behave the same (universal core-protocol + stable-xdg-shell only,
  nothing GNOME-specific used), but not independently confirmed. See
  `docs/libcrtgfx_wayland_plan.md`'s new "Linux Host Adapter" section for
  the full design writeup and documented scope cuts (single window/
  connection, no input, buffer teardown not `wl_buffer::release`-gated,
  object ids never recycled).

## 2026-08-17

- **Started the upper-runtime stack as first-class CRT artifacts and brought
  up the initial Windows `libcrtgfx` Wayland/Weston-style software-present
  path.** `libcrtgfx`, `libcrtjs`, and `libcrtmedia` are no longer optional
  placeholders gated behind `CRT_ENABLE_UPPER_RUNTIME`: the top-level
  `CMakeLists.txt` always adds them, `sysroot` depends on their static/shared
  targets, and `tools/create_rootfs.py` now copies shared runtime libraries
  into both `/system/lib` and `/usr/lib` alongside `libc`, `libm`, `libdl`,
  and `libc++`. The upper-runtime libraries build as normal CRT runtime
  artifacts: common code links through this project's own `c`/`m`/`dl`/`cxx`
  or `c_shared`/`m_shared`/`dl_shared`/`cxx_shared` libraries, while only the
  narrow host backend object layer is allowed to call native OS window/GPU APIs
  directly.

  `libcrtgfx` now has the first real source tree and public API:
  `include/crtgfx/window.h` exposes a small host-independent C surface for
  window creation, event pumping, size queries, and software frame submission.
  The implementation deliberately stopped being a direct Win32-window wrapper:
  public `crtgfx_window_*` calls flow into `src/common/wayland_weston.c`, which
  owns the first Weston-style toplevel/surface state. Host-specific code now
  implements `crtgfx_host_window_*` below that boundary. On Windows,
  `src/arch/windows/window_win32.c` maps the toplevel to a native host window
  and reports resize/close events back into the common Weston-style state.

  The first frame path is CPU software present, not Skia yet:
  `crtgfx_window_begin_frame()` returns a BGRA8888-premultiplied framebuffer
  owned by the common toplevel state, and `crtgfx_window_end_frame()` commits
  it to the host backend. The Windows backend presents that committed buffer
  with a `StretchDIBits()` blit. `crtgfx_window_smoke` creates a hidden window,
  fills and commits a software frame, and verifies the path under CTest;
  `crtgfx_window_demo` opens a visible window and animates the same software
  frame path for manual bring-up. The Win32 adapter uses internal ABI
  declarations rather than including `<windows.h>`, avoiding the earlier
  collision between Windows SDK headers and this project's Bionic-style public
  headers.

  Also recorded the API/policy direction in
  `docs/libcrtgfx_api_policy.md` and `docs/libcrtgfx_wayland_plan.md`: expose
  normal Skia headers for 2D drawing later, keep project-owned `crtgfx` headers
  focused on runtime/surface/presentation/event/backend contracts, use WSLg/
  Weston/wlroots/Wawona-style projects as architecture references, and
  explicitly exclude WSL/Linux-binary execution, VM/distro packaging, RDP rail,
  and vGPU/VA-API dependency from the Windows plan.

  Verified on Windows x86_64:
  `cmake --preset windows-host-ninja-debug`,
  `cmake --build --preset windows-host-ninja-debug --target
  crtgfx_window_smoke crtgfx_window_demo`,
  `ctest --preset windows-host-ninja-debug -R crtgfx_window_smoke_runs
  --output-on-failure`, and a default
  `cmake --build --preset windows-host-ninja-debug` all pass. The build log
  confirms `crtgfx`, `crtjs`, and `crtmedia` are installed into the sysroot and
  their shared runtime files are copied into the Android-like rootfs. A full
  `ctest --preset windows-host-ninja-debug` was started and confirmed the new
  `crtgfx_window_smoke_runs` entry in the default list, but was manually
  interrupted after an unrelated DNS test produced no output for over a minute.
  macOS and Linux backends are intentionally left as the next follow-up: they
  should reuse the common Weston-style toplevel/software-buffer contract and
  add only host adapter code under their respective `src/arch/` directories.

- **Fixed a real infinite-loop bug in every aarch64 `ucontext.S`
  (`swapcontext()`'s resume point), found live on real macOS aarch64
  hardware.** `tests/ucontext_test.c` hung indefinitely -- `ctest`
  printed `Start 73: ucontext_test_runs` and never returned, and the
  live process sat at 100% CPU with no crash and no output. Confirmed by
  attaching `lldb` to the still-running process: three separate samples,
  each taken after a fresh `attach`/`detach` cycle, landed at the exact
  same instruction (`ucontext.S`'s `mov w0, #0` / `ret` pair immediately
  after `swapcontext()`'s resume label) -- landing on the identical byte
  address every time, rather than scattered across a loop body, is what
  pointed at a true self-referencing infinite loop rather than an
  ordinary tight loop elsewhere in the test.

  Root cause: `swapcontext()` patched the saved link-register slot
  (mcontext offset 88) with the address of a local `1:` resume stub
  (`adr x2, 1f; str x2, [x0, #88]`) instead of leaving the real caller's
  return address there, mirroring the x86_64 sibling files' own `1:`
  stub trick (`libc/src/arch/{linux,macos,windows}/x86_64/ucontext.S`).
  That trick is correct on x86_64 because `retq` pops its target address
  off the *stack*, so as long as the saved stack pointer points at the
  right slot, `retq` at `1:` naturally continues on to the real original
  caller. AAPCS64 has no such mechanism: `ret` just branches to whatever
  is in the x30 *register*, and branching does not clear or advance that
  register. So by the time execution reached the resume stub, x30 still
  equaled the stub's own address (nothing between the jump into it and
  its own `ret` ever changed x30) -- the stub's `ret` branched straight
  back into itself, forever. This was flagged as a real risk in the
  implementing commit's own message ("Linux/macOS and aarch64 reasoned
  carefully from the same proven register set but not yet run on real
  hardware from this session") and confirmed the first time it actually
  ran on real aarch64 hardware.

  Fixed in all three aarch64 variants (`libc/src/arch/{linux,macos,
  windows}/aarch64/ucontext.S`) by removing the resume-stub patch
  entirely: `stp x29, x30, [x0, #80]` a few lines earlier in
  `swapcontext()` already saves the real caller's return address at that
  same offset, exactly like `getcontext()` does, which is all a plain
  `ret` needs to resume at the right place. The "a resumed context
  appears to return 0" contract (POSIX: resuming a saved context behaves
  as if the call that saved it had just returned) moved to an
  unconditional `mov w0, #0` immediately before every point this file
  jumps *into* a restored context instead -- `setcontext()`'s own `ret`,
  and `swapcontext()`'s second `ret` after loading the new context --
  rather than depending on a separate stub to set it while also handling
  the return-address plumbing.

  Verified directly on this real macOS aarch64 host: `ucontext_test`
  (a real coroutine round trip -- `makecontext()`/`swapcontext()` yield-
  and-resume, plus a `getcontext()`/`setcontext()` "returns twice"
  round trip) now prints `ok` instead of hanging. Full suite:
  **102/102** on `macos-host-ninja-debug`, no regressions. Linux aarch64
  and Windows aarch64 get the identical fix, reasoned to apply the same
  way since the underlying AAPCS64 `ret`-via-link-register mechanism is
  architecture-defined, not OS-specific -- not independently re-verified
  on real hardware for those two hosts this session.

- **Fixed the real macOS `ifaddrs`/`/dev/zero` regressions found by
  `cmake --workflow --preset macos-host-ninja-debug` on this host.** The
  build first failed in `libc/src/ifaddrs.c` because this project's public
  `include/ifaddrs.h` exposes the Bionic/POSIX `ifa_dstaddr` macro
  (`ifa_ifu.ifu_dstaddr`), and the macOS adapter's private Darwin mirror
  struct reused `ifa_dstaddr` as an internal field name. That macro expanded
  inside the private struct declaration and at the call site, breaking the C
  syntax. Fixed by renaming the private Darwin field to `ifa_ifu` and keeping
  the public ABI/macro shape unchanged. Also made the Darwin IPv4 sockaddr
  translator return an explicit `struct sockaddr*` cast so the `-Werror`
  build stays clean.

  After that build fix, the full macOS CTest run exposed one runtime mismatch:
  `dev_zero_test` failed at `open("/dev/zero", O_WRONLY)`. Linux/Bionic
  `/dev/zero` accepts writes and discards them; this macOS host's real
  `/dev/zero` is readable but rejects write-only opens. Fixed the macOS PAL
  path by mapping write-only `/dev/zero` opens to the real `/dev/null` (an
  exact discard sink for that access mode) and by making `access("/dev/zero",
  F_OK|R_OK|W_OK)` report the Bionic/Linux surface while still rejecting
  `X_OK`. Read opens continue to use the real macOS `/dev/zero`.

  Verified: `cmake --workflow --preset macos-host-ninja-debug` completes
  configure, full rebuild, and **101/101** tests on this machine.

- **Verified `PTHREAD_PROCESS_SHARED` on real macOS hardware and fixed two
  stale pre-existing test expectations it broke**, closing the "reasoned
  but not yet verified on real hardware" caveat the same-day
  `PTHREAD_PROCESS_SHARED` entry below carried (written from a
  Windows-only dev session). Running the full suite on this real macOS
  host surfaced two real `ctest` failures:
  ```
  83/95 pthread_barrier_test_runs ... Failed
  pthread_barrier_test: barrierattr pshared
  92/95 pthread_attr_test_runs ... Failed
  pthread_attr_test: mutex pshared attr
  ```
  Root cause: `tests/pthread_barrier_test.c` and `tests/
  pthread_attr_test.c` both predate the `PTHREAD_PROCESS_SHARED` work and
  still hardcoded the *pre-change* contract --
  `pthread_barrierattr_setpshared()`/`pthread_mutexattr_setpshared()`/
  `pthread_rwlockattr_setpshared()` called with `PTHREAD_PROCESS_SHARED`
  must return `ENOTSUP`. That was true everywhere before this feature
  landed; on Linux/macOS now (`CRT_PSHARED_SUPPORTED` in
  `libc/src/pthread.c`) it correctly returns `0` instead, which the old
  hardcoded `!= ENOTSUP` checks read as a failure. Not an implementation
  bug -- confirmed by reading `libc/src/pthread.c`'s own
  `pthread_mutexattr_setpshared()`, which does exactly what Bionic
  parity requires (`0` when `CRT_PSHARED_SUPPORTED`, `ENOTSUP` otherwise).
  The commit that introduced the feature added a new, correctly-gated
  `tests/pthread_process_shared_test.c` and updated `tests/
  pthread_spin_test.c`, but missed that these two older, unrelated-looking
  attr tests also asserted the old contract. Fixed by giving both files
  the same `CRT_PSHARED_SUPPORTED` gate (`defined(CRT_TARGET_OS_LINUX) ||
  defined(CRT_TARGET_OS_MACOS)`) the new test and the implementation
  itself already use, and branching the expected `setpshared()` result on
  it instead of hardcoding `ENOTSUP`.

  This also incidentally provided the real-hardware verification the
  macOS `os_sync_wait_on_address`/`os_sync_wake_by_address_*` `SHARED`-flag
  work itself was still waiting on:
  `tests/pthread_process_shared_test.c`'s real cross-thread mutex/rwlock/
  cond/barrier contention over `PTHREAD_PROCESS_SHARED` objects passed
  cleanly on this real macOS host, exercising
  `__crt_wait32_shared`/`__crt_wake32_*_shared` (`libc/src/wait.c`) for
  real -- updated that file's own comment from "UNVERIFIED" to record the
  confirmation, matching `HISTORY.md`'s established `linkat()` precedent
  for the same discipline.

  Verified: full suite **95/95** on `macos-host-ninja-debug` (89 termios/
  sendmsg baseline + `termios_line_control_test` + the newer
  `dl_iterate_phdr`/epoll/`PTHREAD_PROCESS_SHARED`-era tests), no other
  regressions.

- **Implemented `ucontext.h`** (`getcontext`/`setcontext`/`makecontext`/
  `swapcontext`), the last of the six "lower priority, no identified
  near-term consumer" Bionic libc gaps from the 2026-08-16 audit --
  closing out every item that audit found, across all three priority
  tiers. Unlike the other five items in that tier, there is no honest
  host-level reason to hold any host back here: the underlying mechanism
  (save/restore the callee-saved register set + stack pointer + resume
  address) is pure userspace state manipulation, no syscall or OS
  primitive involved, so this project's own already-verified per-host/
  per-arch `setjmp`/`longjmp` assembly (`libc/src/arch/*/*/setjmp.S`) was
  the direct model for new `ucontext.S` files covering Linux/macOS/
  Windows x86_64/aarch64 (six files). `mcontext_t`/`ucontext_t` are a
  private, self-consistent layout (not bit-compatible with any host's
  native ucontext_t) -- a deliberate, safe choice since this project's
  own signal delivery doesn't hand a real `ucontext_t` to `SA_SIGINFO`
  handlers today (the third handler parameter is opaque `void*`).
  `makecontext()` supports up to 4 pointer-width arguments uniformly
  across every host/arch, matching Windows x64's real 4-register limit
  (the tightest of the ABIs covered) rather than the 6 SysV/AAPCS64
  themselves allow, for one simple, uniform bootstrap-frame layout.

  A real coroutine round-trip test (`tests/ucontext_test.c`: getcontext/
  setcontext setjmp-style resume, then a full makecontext/swapcontext
  coroutine with argument passing and a `uc_link`-driven return) caught
  and fixed two genuine bugs during development, both on Windows x86_64
  (the only host verifiable from this Windows-only dev session):
  - A struct-layout bug: the makecontext bootstrap frame used `long`/
    `unsigned long` fields, which are only 4 bytes on Windows' LLP64 data
    model (unlike Linux/macOS's LP64, where they're already 8) -- silently
    breaking the fixed-byte-offset indexing the trampoline assembly
    depends on. Fixed with explicit `int64_t`/`uint64_t` on every host.
  - A genuine `swapcontext()` resume-point bug: its saved stack-pointer
    slot used the same `%rsp+8`-adjusted convention as `getcontext()`/
    `setcontext()` (which resume via a raw `jmp` to a separately-saved
    target address), but `swapcontext()`'s own resume point completes via
    a real `retq` instead -- which needs the *unadjusted* stack pointer
    to correctly pop the real return address. Saving the adjusted value
    made `retq` pop an unrelated stack slot and jump to garbage,
    reproducing as a reliable `STATUS_ACCESS_VIOLATION` jumping to
    address 0 inside a real coroutine yield. Fixed on all three x86_64
    variants (Linux/macOS/Windows); aarch64 was never affected, since
    AAPCS64's `ret` resolves via the X30 link register rather than
    popping the stack, so it has no analogous adjusted-vs-unadjusted
    distinction to get wrong.

  Windows also turned out to need one more real, host-specific piece
  neither Linux nor macOS do at all: keeping the thread's TEB
  (`NT_TIB.StackBase`/`StackLimit`, plus `DeallocationStack` -- stable,
  documented offsets `gs:0x08`/`gs:0x10`/`gs:0x1478`) in sync with
  whichever stack is currently running. Left stale after switching to a
  `makecontext()`-created stack, various things that consult it can
  behave incorrectly; found for real when the coroutine test's first
  `swapcontext()`-into-a-new-stack reliably crashed until this was added.
  `getcontext()`/`setcontext()`/`swapcontext()` now all save/restore
  these three fields alongside the register set; `makecontext()` seeds
  them from `uc_stack` for a context's first activation.

  Verified directly on Windows x86_64 (real coroutine round trip, real
  bugs found and fixed via a vectored-exception-handler diagnostic
  session). Linux/macOS and aarch64 (all three hosts) reasoned carefully
  from the same proven register set but not yet run on real hardware
  from this Windows-only dev session -- matching this session's
  established discipline for other Linux/macOS raw-ABI work.

- **Implemented `ifaddrs.h`** (`getifaddrs`/`freeifaddrs`), the fifth of
  the six "lower priority" Bionic libc gaps. `struct ifaddrs` matches
  real Bionic's minimal shape; scoped to AF_INET (IPv4) only on every
  host, consistent with this project's existing AF_INET/AF_UNIX-only
  sockaddr translation scope on macOS (`socket.c`).
  - **Linux**: real, via `/sys/class/net` (real, and deliberately
    independent of the exact kernel `struct ifreq`/`ifconf` byte layout,
    unlike `SIOCGIFCONF`'s packed-array-of-`ifreq` return) for interface
    enumeration, then `SIOCGIFADDR`/`SIOCGIFNETMASK`/`SIOCGIFBRDADDR`/
    `SIOCGIFFLAGS` ioctls per interface through a deliberately over-sized
    private `ifreq` shape (safe regardless of the exact real kernel
    `ifr_ifru` union size). Reasoned from well-known stable UAPI ioctl
    numbers, flagged unverified pending real Linux hardware/CI.
  - **macOS**: real, resolving the actual Darwin `getifaddrs()`/
    `freeifaddrs()` at runtime (the same `__crt_macho_find_symbol_in_
    loaded_image` + `dlsym(RTLD_NEXT)` technique this project's
    `getaddrinfo()` macOS backend already uses, avoiding a name collision
    with this project's own public `getifaddrs()` symbol), then
    translating each Darwin-native (`sa_len`-prefixed) sockaddr into this
    project's own Bionic/Linux-shaped sockaddr.
  - **Windows**: real, via the IP Helper API's `GetAdaptersInfo()`
    (`iphlpapi.dll`, loaded dynamically at runtime like this project's
    other optional Windows APIs), chosen over the newer, larger,
    versioned `IP_ADAPTER_ADDRESSES`/`GetAdaptersAddresses()` specifically
    because `GetAdaptersInfo()`'s small, non-versioned struct is much
    lower risk to transcribe correctly from documentation. Verified
    directly: `tests/ifaddrs_test.c` confirmed real adapter names/flags/
    addresses are returned on this dev machine.

  Real macOS testing the same day (see the entry above, dated after this
  one) found and fixed a real build break this work introduced: the
  private Darwin mirror struct's `ifa_dstaddr` field name collided with
  the public `ifa_dstaddr` macro (`ifa_ifu.ifu_dstaddr`) this same header
  defines, breaking C syntax the moment the macOS-only code path was
  actually compiled (invisible from this Windows-only session, since
  that `#elif defined(CRT_TARGET_OS_MACOS)` branch never compiles here).

- **Implemented `uchar.h`/`threads.h`/`sys/prctl.h`/`glob.h`**, four of
  the six "lower priority, no identified near-term consumer" Bionic libc
  gaps, and fixed two real, previously-undetected bugs surfaced while
  implementing `glob.h`.
  - **`uchar.h`**: `mbrtoc16`/`c16rtomb`/`mbrtoc32`/`c32rtomb`, layered on
    the existing `mbrtowc()`/`wcrtomb()` UTF-8<->UTF-32 codepoint
    conversion (this project's `wchar_t` is a 32-bit codepoint on every
    host via `-fwchar-type=int`). `mbrtoc16`/`c16rtomb` add real UTF-16
    surrogate-pair handling for codepoints outside the BMP, using two
    sentinel values in `mbstate_t`'s spare byte that real UTF-8 decoding
    never produces.
  - **`threads.h`**: C11 thin wrapper over pthreads, matching real
    Bionic's own approach. `thrd_create()` adapts `thrd_start_t`'s
    `int(*)(void*)` signature to `pthread_create()`'s `void*(*)(void*)`
    via a small heap-allocated shim. Added `pthread_mutex_timedlock()` to
    `pthread.c`/`pthread.h` as a real new Bionic-parity primitive
    `mtx_timedlock()` needed, reusing `realtime_until()`/
    `__crt_wait32_timed{,_shared}()`.
  - **`sys/prctl.h`**: real on Linux (raw `prctl` syscall trampoline,
    x86_64=157, aarch64=167 -- reasoned carefully, flagged unverified
    pending real Linux hardware/CI), `ENOSYS` on macOS/Windows since
    `prctl` is a Linux-only kernel concept (real Bionic's own header is
    Linux-specific too).
  - **`glob.h`**: real `glob()`/`globfree()` built on `opendir`/
    `readdir`/`fnmatch`/`stat`, not a stub. Supports `GLOB_APPEND`/
    `GLOB_DOOFFS`/`GLOB_ERR`/`GLOB_MARK`/`GLOB_NOCHECK`/`GLOB_NOSORT`/
    `GLOB_NOESCAPE`, multi-component wildcard patterns, the standard
    hidden-dotfile convention, and `errfunc`/`GLOB_ERR` reporting for
    both failed `opendir()` (wildcard segments) and failed `stat()`
    (literal segments).

  Implementing `glob()` surfaced two real, previously-undetected libc
  bugs -- neither had a dedicated regression test before this:
  - **`fnmatch()`**: `match_here()`'s end-of-pattern base case returned
    the *inverted* value -- `return *s == 0 || ...` is 1 (`FNM_NOMATCH`)
    exactly when there IS a match (both pattern and string exhausted),
    and 0 (match) exactly when there ISN'T. This broke every `fnmatch()`
    call whose pattern's trailing wildcard needed that base case to
    report success, e.g. `fnmatch("*.txt", "alpha.txt", 0)` always
    failed. Affects every real `fnmatch()` consumer in the tree (toybox
    `find`/`grep`/`tar`/`ip`/`modprobe` via `portability.h`) that hit
    this path -- there was no `fnmatch_test.c` to have caught it. Fixed;
    added one.
  - **`remove()`**: only ever called `unlink()`, so `remove()` on a
    directory always failed -- the C standard requires `remove()` to
    work for files and (empty) directories alike, same as every real
    libc. Fixed to `stat()` the path first and dispatch to `rmdir()` for
    directories. Found because `glob_test.c`'s own cleanup (removing a
    test directory between runs) kept silently failing. Added a
    regression case to `stdio_file_test.c`.

  New tests: `uchar_test.c`, `threads_test.c`, `prctl_test.c`,
  `fnmatch_test.c`, `glob_test.c` (real directory-tree wildcard matching,
  `GLOB_MARK`/`APPEND`/`DOOFFS`/`NOCHECK`/`ERR`, `errfunc` reporting for
  both failure paths). Full ctest suite passing on Windows; `cmake
  --fresh` reconfigure verified clean.

- **Implemented `PTHREAD_PROCESS_SHARED`**, closing out every item in
  TODO.md's "Bionic libc completeness before `libcrtgfx`" section. Real,
  per-primitive, per-host support rather than a single blanket flag:

  - **`libc/src/wait.c`**: added process-shared variants of the private
    wait/futex primitive -- `__crt_wait32_shared`, `__crt_wait32_timed_shared`,
    `__crt_wake32_one_shared`, `__crt_wake32_all_shared` -- alongside the
    existing four private ones.
    - **Linux**: real and genuinely cross-process. Uses the plain
      `FUTEX_WAIT`(0)/`FUTEX_WAKE`(1) operations instead of the existing
      `CRT_FUTEX_WAIT_PRIVATE`(128)/`CRT_FUTEX_WAKE_PRIVATE`(129) ones. This
      matters because the private operations key off `(mm_struct, virtual
      address)` -- an optimization that does *not* correctly coordinate
      waiters across independent processes even over genuinely
      `MAP_SHARED` memory, since two processes' mappings of the same shared
      region generally have different virtual addresses. The non-private
      operations key off the futex's physical backing (the mapped page +
      offset) instead, which is what makes cross-process rendezvous work.
    - **macOS**: real, using `os_sync_wait_on_address`/
      `os_sync_wake_by_address_*`'s documented `OS_SYNC_WAIT_ON_ADDRESS_SHARED`
      / `OS_SYNC_WAKE_BY_ADDRESS_SHARED` flag bit (`0x1`) instead of the
      existing `0` (private) flag -- mirrors the Linux private/shared
      split. Reasoned carefully from the documented header shape (libSystem
      `<os/os_sync_wait_on_address.h>`, macOS 14.4+/iOS 17.4+) but, like
      every other Linux/macOS raw-ABI addition this Windows-only dev
      session (`linkat()`, `sendmsg`/`recvmsg`, `eventfd`/`timerfd`/`epoll`,
      `dl_iterate_phdr`/`dladdr`), **not yet verified against real Apple
      hardware** -- flagged unverified in the source comment until real
      macOS CI/hardware confirms it.
    - **Windows**: `ENOTSUP` from all four. `WaitOnAddress`/
      `WakeByAddressSingle`/`WakeByAddressAll` are documented by Microsoft
      as operating on the calling process's own virtual address space
      only, with no flag or variant that extends them across process
      boundaries -- an honest architectural limitation, not a missing
      feature to implement. A real fix would need an entirely different
      mechanism (a named kernel object such as `CreateMutexA`/
      `CreateEventA`, or handle duplication/inheritance) -- out of scope
      for this primitive.

  - **`libc/src/pthread.c`**: threaded a per-object "shared" flag through
    mutex/rwlock/cond/barrier storage (a spare `__private[]` word each --
    all four types had room) and their `*attr_setpshared` functions, gated
    by a `CRT_PSHARED_SUPPORTED` macro (`1` on Linux/macOS, `0` on
    Windows). `pthread_mutex_lock`/`trylock`/`unlock`,
    `pthread_rwlock_rdlock`/`wrlock`/`unlock`, `pthread_cond_signal`/
    `broadcast`/`wait`/`timedwait`, and `pthread_barrier_wait` all now
    dispatch to the shared or private wait/wake primitive based on that
    flag. `pthread_mutexattr_setpshared`/`pthread_rwlockattr_setpshared`/
    `pthread_barrierattr_setpshared` now accept `PTHREAD_PROCESS_SHARED`
    for real on Linux/macOS (previously unconditional `ENOTSUP`
    everywhere, confirmed dead-code-checked in `pthread_mutex_init`/etc.
    since `setpshared` itself already rejected the value before the bit
    could ever reach `_init`); Windows keeps `ENOTSUP`.
    `pthread_condattr_getpshared`/`setpshared` were added (Bionic has
    these; this project didn't before), bit-packed into the existing
    `pthread_condattr_t` alongside the clock-id bits (`0xff` clock mask,
    `0x100` pshared bit -- same pattern as `pthread_mutexattr_t`'s
    existing type-mask/pshared-bit/robust-bit layout).
  - **`pthread_spin_init`**: fixed to accept `PTHREAD_PROCESS_SHARED`
    unconditionally, on **every host including Windows**. Unlike the other
    four primitives, this project's spinlock never calls into an OS
    wait/wake primitive at all -- `pthread_spin_lock`/`trylock`/`unlock`
    are pure `__atomic_*` compiler builtins on a plain `int`, and atomic
    CPU instructions on genuinely shared memory behave correctly across
    process boundaries on every host this project targets. The previous
    unconditional `ENOTSUP` here was overcautious, not a real limitation.
  - New regression: `tests/pthread_process_shared_test.c` -- functional
    round-trip coverage for mutex/rwlock/barrier/cond including real
    cross-thread contention (forcing the actual wait/wake path, not just
    uncontended lock/unlock), gated behind the same `CRT_PSHARED_SUPPORTED`
    macro so it exercises the real functional path on Linux/macOS and the
    `ENOTSUP` contract on Windows. `tests/pthread_spin_test.c` updated to
    exercise a real pshared spinlock round trip on every host. Full ctest
    suite (110 tests) passes on Windows; `cmake --fresh` reconfigure
    verified clean.
  - Full detail and current status: `docs/bionic_libc_gaps.md`'s
    `PTHREAD_PROCESS_SHARED` entry. `docs/import_bionic.md`'s per-tranche
    notes (mutex/rwlock/spinlock/cond/private-wait-futex tranches) updated
    with pointers to the new status rather than rewritten wholesale.

- **Implemented `dl_iterate_phdr`/`link.h`/`elf.h`/`dladdr`**, the last
  "medium priority" item from the Bionic libc gap audit
  (`docs/bionic_libc_gaps.md`) that had a concrete design path.
  `include/elf.h` (ELF64 types/constants) carries none of the reasoned-
  but-unverified caveat other raw-syscall work this session needed: the
  System V ABI's ELF64 object format is a fixed, documented binary spec,
  not something that varies by host kernel/architecture the way syscall
  numbers do. `include/link.h` adds `struct dl_phdr_info` matching real
  Bionic's own minimal 4-field shape (not glibc's larger extension with
  `dlpi_adds`/`dlpi_subs`/etc.). `dladdr`/`Dl_info` were added to
  `include/dlfcn.h`.

  Real per-host implementations wherever each host actually has something
  real to report, not stubs:
  - **Linux** (`libdl/src/arch/linux/dl_linux.c`): `dl_iterate_phdr()`
    reports exactly one entry -- the main executable -- built from the
    real `AT_PHDR`/`AT_PHNUM` values the kernel handed this process at
    `exec()` (the existing `getauxval()`). Only one entry, because this
    project has no real ELF dynamic linker yet
    (`docs/dynamic_loading.md`'s own "Linux" section: `dlopen()` doesn't
    actually load shared objects today, so there is nothing else to
    report) -- a real, narrow answer, not a fabricated one. The load bias
    is computed from the `PT_PHDR` segment's own link-time `p_vaddr` when
    present (`bias = AT_PHDR - PT_PHDR.p_vaddr`, a standard technique;
    falls back to `0`, correct for a non-PIE executable, when `PT_PHDR` is
    absent). `dladdr()` checks whether the target address falls inside
    one of that same executable's own `PT_LOAD` segments and, if so,
    reports its real path via `/proc/self/exe`.
  - **macOS** (`libdl/src/arch/macos/dl_macos.c`): `dl_iterate_phdr()`
    calls the callback zero times and returns `0`. `dlpi_phdr`/
    `dlpi_phnum` are fundamentally `Elf64_Phdr`-shaped; Mach-O's real load
    commands/segment commands are a genuinely different format, and
    fabricating ELF-shaped data from them would be actively wrong for any
    caller walking the array expecting real ELF semantics, not merely
    imprecise -- so this is an honest "no ELF images to report" (a
    legitimate result `dl_iterate_phdr()`'s own contract already allows,
    not an error return, since it has none), matching link.h's own
    documented reasoning. `dladdr()` is real: a new shared helper,
    `__crt_macho_find_image_for_address()`, was added to `libc/src/arch/
    macos/common/macho_symbol.c` (and exposed via the existing
    `crt_macho_symbol.h` private header, since it needed the same dyld
    loaded-image-list infrastructure `dlopen()`/`dlsym()` already use) --
    it walks every loaded image's `LC_SEGMENT_64` load commands for the
    one whose real, slide-adjusted address range contains the target
    address, matching real Darwin `dladdr()`'s own `dli_fbase` convention
    (the image's mach_header address).
  - **Windows** (`libdl/src/arch/windows/dl_windows.c`): `dl_iterate_phdr()`
    is the same honest zero-entries result as macOS, for the same reason
    (PE has no ELF program headers either). `dladdr()` is real:
    `GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS)` finds
    which loaded module contains the address directly, no manual PE
    header parsing needed -- a documented Win32 fact makes the resulting
    `HMODULE`'s own value equal to the module's real load base address,
    so it doubles directly as `dli_fbase`; `GetModuleFileNameA` supplies
    `dli_fname`.

  On every host, `dli_sname`/`dli_saddr` (the nearest-symbol part of the
  real `dladdr()` contract) are always left `NULL`/`0` -- POSIX/Bionic
  both document that as a legitimate result when no matching symbol is
  found, not a failure, and this project does not parse any host's symbol
  table for reverse address-to-name lookup yet.

  New permanent regression: `tests/dl_iterate_phdr_dladdr_test.c` -- real
  `elf.h` struct-size checks (`sizeof(Elf64_Ehdr) == 64`, etc.) that run
  on every host, not just Linux/ELF ones, since they're purely about this
  header matching the real ABI spec; `dl_iterate_phdr()` internal-
  consistency checks tolerant of either the zero-entries (macOS/Windows)
  or one-entry (Linux) shape, since both are legitimate depending on
  host; a real `dladdr()` lookup against the test binary's own `main()`,
  which found the running executable correctly on Windows (verified
  directly) -- the Linux/macOS backends are reasoned carefully but not
  yet run on real hardware from this session, matching the same open
  caveat `sendmsg`/`recvmsg`'s trampolines and `eventfd`/`timerfd`/
  `epoll`'s trampolines carried until real testing closed theirs. All 109
  tests pass on Windows via a genuine `cmake --fresh` reconfigure.

- **Fixed a real `termios_line_control_test` false failure on a real Linux
  terminal, root-caused to intentional Linux kernel pty behavior, not a
  CRT/PAL bug.** Reported: the test passed on GitHub CI but failed for
  real on this host (`c_cflag round trip mismatch (CS7/PARENB/CSTOPB/
  PARODD)`) -- CI never actually exercises this path (no `/dev/tty`
  there at all, so the test always `skip`s), while a real interactive
  session does have one. Root-caused with a standalone repro issuing the
  raw `TCGETS`/`TCSETS` ioctls directly, entirely bypassing this
  project's own `libc/src/arch/linux/common/termios.c` (confirmed: the
  raw kernel round trip already loses `CS7`/`PARENB` before this
  project's translation layer is even involved), then confirmed against
  a further isolated per-bit repro and the real Linux kernel source
  (`drivers/tty/pty.c`'s `pty_set_termios()`, found via search):
  `c_cflag &= ~(CSIZE | PARENB); c_cflag |= (CS8 | CREAD);` -- the kernel
  deliberately forces every pseudo-terminal to `CS8`/no-parity
  unconditionally, since there is no real UART behind a pty for a
  character-size/parity setting to mean anything. `CSTOPB` (also
  asserted by this test) is *not* subject to this and round-trips
  correctly, as does everything else this same test checks (`VMIN`/
  `VTIME`, `B9600` speed via `cfsetspeed()`/`cfgetispeed()`/
  `cfgetospeed()`) -- confirmed independently, all pass. Fixed by
  narrowing `tests/termios_line_control_test.c`'s assertion to what a
  pty actually promises (`CSTOPB` only), with the finding recorded
  in-line: this is not an environment limitation to skip past (unlike
  the existing "no `/dev/tty` at all" skip case both termios tests
  already have) but expected, real, well-documented kernel behavior that
  would reject the same request against a real glibc/Bionic libc too,
  since essentially every interactive Linux session (SSH, tmux, a
  terminal emulator, `script`) runs on a pty. Verified: reproduced the
  exact failure via `script -qc <binary> /dev/null` (this host's own
  session has no directly-attached console either), confirmed the fix
  resolves it (`termios_line_control_test: ok`), full `ctest` 93/93.

- **Fixed a real mbedtls Windows build break the user hit directly**:
  `ld.lld: error: duplicate symbol: __crt_sys_sendmsg` (also
  `__crt_sys_recvmsg`/`__crt_sys_link`) during `port-rebuild-mbedtls`.
  Root cause: `porting/recipes/mbedtls-windows-exclude-symbols.rsp` is a
  hand-generated snapshot (per `docs/porting_status.md`'s existing entry,
  originally produced via `llvm-nm` against `lib/c.lib` to give mbedtls's
  Windows DLL build recipe one `-Wl,--exclude-symbols=NAME` flag per real
  libc symbol, preventing this project's entire libc from leaking into
  mbedtls's own DLL export tables the way it originally did) -- not
  something regenerated automatically at build time. `libc/src/arch/
  windows/common/syscall.c` compiles as a single translation unit, so
  when mbedtls's DLL link needs even one of its symbols (ordinary file
  I/O, unrelated to sockets or `link()`), the whole `.obj` -- including
  every `__crt_sys_*` symbol in it -- becomes a candidate DLL export
  again; anything not already in that snapshot list leaks straight
  through, then collides with the same symbol already present directly
  in `c.lib` when a *later* DLL in the same link chain (here,
  `libmbedx509.dll`, which depends on `libmbedcrypto.dll.a`) needs it
  too. This session's earlier `sendmsg`/`recvmsg` work (and `link()`,
  from an earlier point in the same overall session) added exactly the
  symbols the snapshot predated.

  Fixed by regenerating the full list rather than hand-patching just the
  three reported names: dumped `lib/c.lib`'s current symbol table with
  `llvm-nm --defined-only -g`, filtered to valid plain-C identifiers
  (dropping compiler-generated string-literal/`.weak`/`.refptr` noise),
  diffed against the checked-in list to confirm it was a strict superset
  (917 -> 947 entries, nothing in the old list had gone missing from
  current `c.lib`, so no existing `--exclude-symbols` target was silently
  dropped), and wrote the union back out sorted. The 30 newly-added names
  covered more than just the three from the error report: also
  `__crt_sys_tcdrain`/`tcflow`/`tcflush`/`tcsendbreak`,
  `__crt_windows_set_args`, and every public symbol from this session's
  `semaphore.h`/`eventfd`/`timerfd`/`epoll`/`memfd_create` work
  (`sem_*`, `eventfd*`, `timerfd_*`, `epoll_*`, `memfd_create`,
  `sendmsg`/`recvmsg` themselves) -- all of which would have hit this
  exact same failure mode on whatever future port happened to pull them
  in first, not just mbedtls.

  Verified for real, not just reasoned: a full `port-rebuild-mbedtls`
  now builds `libmbedcrypto.dll`/`libmbedx509.dll`/`libmbedtls.dll`
  cleanly; `llvm-readobj --coff-exports` cross-referenced against the
  full current libc symbol list shows zero libc symbols in
  `libmbedcrypto.dll`'s export table (matching the original fix's own
  verification method); both `crypto-static` and `crypto-shared` recipe
  tests pass. The underlying maintenance gap -- this `.rsp` file has no
  mechanism to catch future drift automatically -- is recorded as a
  standing caution in `TODO.md`'s note section, not just fixed this once.

- **Implemented `sys/epoll.h`/`sys/eventfd.h`/`sys/timerfd.h`**, the first
  "medium priority" batch from the Bionic libc gap audit
  (`docs/bionic_libc_gaps.md`). All three are Linux-only in real Bionic
  too (Android only ever runs on the Linux kernel) -- declared on every
  host so portable code that merely includes and compiles against the
  surface keeps working everywhere (matching this project's existing
  `libc/src/inotify.c` precedent for a Linux-only kernel feature), but
  every function returns `ENOSYS` on macOS/Windows.

  Linux gets real raw syscall trampolines (`libc/src/arch/linux/
  {x86_64,aarch64}/syscall.S`): `eventfd2` for `eventfd()`;
  `epoll_create1`/`epoll_ctl`/`epoll_pwait` for `epoll_create1()`/
  `epoll_ctl()`/`epoll_wait()` (`epoll_wait()` is implemented over
  `epoll_pwait` with a `NULL` sigmask, matching how glibc itself
  implements it -- aarch64 has no separate `epoll_wait` syscall number at
  all, only `epoll_pwait`, so this keeps one codepath for both
  architectures); `timerfd_create`/`timerfd_settime`/`timerfd_gettime`
  (reusing this project's existing `struct itimerspec`/`CLOCK_REALTIME`/
  `CLOCK_MONOTONIC` from `<time.h>` rather than redeclaring them). Every
  syscall number was reasoned carefully from the same well-established,
  stable Linux syscall tables `sendmsg`/`recvmsg`'s own trampolines used
  (`arch/x86/entry/syscalls/syscall_64.tbl` for x86_64,
  `include/uapi/asm-generic/unistd.h` for aarch64) -- **not independently
  verified on real Linux hardware from this Windows-only session**,
  matching that exact same open caveat.

  `struct epoll_event` needed particular care beyond just the syscall
  numbers: the real Linux kernel ABI (`include/uapi/linux/eventpoll.h`)
  packs it to 12 bytes on x86_64 (`__attribute__((packed))`, a historical
  ABI-compat quirk carried from the original i386 design) but expects the
  naturally-aligned 16-byte layout on aarch64 -- an architecture-
  conditional version of the exact same class of bug `struct cmsghdr`'s
  Linux-vs-macOS `cmsg_len` width mismatch was (see this file's
  2026-08-16 entry on that fix). Handled with an `#if defined(__x86_64__)
  || defined(_M_X64)` conditional on the struct's own `__attribute__
  ((packed))`, plus a compile-time `sizeof()` check
  (`__crt_epoll_event_size_check`) in `include/sys/epoll.h` itself so a
  mistake here fails the build immediately rather than silently
  corrupting every `epoll_ctl()`/`epoll_wait()` call's event data at
  runtime -- this check runs on every host/architecture this project
  builds for, not just Linux, since it's purely about this project's own
  header matching the real kernel ABI.

  New permanent regressions: `tests/eventfd_test.c` (a real write-
  accumulates/read-drains-and-resets round trip on Linux, an `ENOSYS`
  check elsewhere), `tests/timerfd_test.c` (a real one-shot 50ms timer
  observed firing via `poll()` on Linux, `ENOSYS` checks elsewhere),
  `tests/epoll_test.c` (the architecture-conditional `struct epoll_event`
  size check on every host, plus a real add-a-pipe-read-fd/observe-it-
  become-readable-after-a-write/remove-it round trip on Linux). All 108
  tests pass on Windows via a genuine `cmake --fresh` reconfigure.

- **Gave Windows real `tcdrain`/`tcflow`/`tcflush`/`tcsendbreak` backing**,
  prompted directly by reviewing the real Linux/macOS termios ports below
  and asking the same question of Windows: `libc/src/termios.c`'s
  dispatcher only ever had a real Windows branch for `tcgetattr`/
  `tcsetattr` (fixed 2026-08-16, the `fd_termios_shadow` round-trip work) --
  the other four fell straight through to the generic `isatty()`-check-
  then-no-op stub, exactly the same "no real backing at all" gap class
  Linux's and macOS's own versions had before their same-day real ioctl
  ports, just never previously singled out for Windows specifically.
  Unlike Linux/macOS, a Windows console isn't a real BSD/Linux tty line
  discipline, so this isn't a 1:1 ioctl port: `__crt_sys_tcdrain()` calls
  the real `FlushFileBuffers()` (the correct Win32 "block until written
  data reaches the device" call, even though a console has no internal
  buffering layer to meaningfully drain); `__crt_sys_tcflush()` calls the
  real `FlushConsoleInputBuffer()` for `TCIFLUSH`/`TCIOFLUSH` (discards
  unread input, matching POSIX semantics exactly) and is an honest no-op
  for `TCOFLUSH` (a console has no output queue to discard from);
  `__crt_sys_tcflow()`/`__crt_sys_tcsendbreak()` stay honest no-ops
  entirely -- a console has no software/hardware flow-control or break-
  condition concept to back them with, matching this project's existing
  `TIOCSWINSZ` precedent (declare the real POSIX surface, document what a
  console genuinely can't do, don't fake it) rather than inventing
  behavior. All four still validate the fd is a real console handle first
  (`GetFileType`/`GetConsoleMode`, same as `tcgetattr`/`tcsetattr`) and
  return `EBADF`/`ENOTTY` correctly. `tests/termios_line_control_test.c`
  (already landed for the macOS port, host-generic, no `#ifdef` gating)
  covers this without needing a new test file. All 105 tests pass on
  Windows via a genuine `cmake --fresh` reconfigure.

- **Ported real macOS termios (`tcgetattr`/`tcsetattr`/`tcdrain`/`tcflow`/
  `tcflush`/`tcsendbreak`)**, closing the gap the same-day Linux termios
  entry below explicitly deferred ("macOS keeps its pre-existing
  hardcoded-stub fallback for now"). Confirmed the stub was still broken
  on this real macOS host first, the same way the Linux bug was found:
  `tests/termios_echo_roundtrip_test.c` run under a real pty (`script -q
  ... termios_echo_roundtrip_test`, with `CRT_USE_HOST_TTY=1` to get past
  the `macos_use_host_dev_tty()` gate in `libc/src/fd.c`) reproduced the
  exact same class of failure -- `tcgetattr()` always returned one fixed,
  hardcoded `struct termios`, `tcsetattr()` silently discarded everything
  it was asked to set.

  Implemented via `libc/src/arch/macos/common/termios.c` (new), using the
  real `TIOCGETA`/`TIOCSETA{,W,F}`/`TIOCDRAIN`/`TIOCFLUSH`/`TIOCSTART`/
  `TIOCSTOP`/`TIOCIXON`/`TIOCIXOFF`/`TIOCSBRK`/`TIOCCBRK` ioctls (the same
  family `fd.c`'s `isatty()` already used `TIOCGETA` from, as a pure
  probe). Real Darwin's own `struct termios` differs from this project's
  public, Bionic-shaped one in every way that matters -- not just field
  widths like Linux's kernel struct needed (`tcflag_t`/`speed_t` are
  8-byte `unsigned long` here, `NCCS` is 20 not 32, no `c_line` field),
  but also completely different `c_cc[]` control-character indices (e.g.
  Bionic's `VINTR`=0 vs. Darwin's `VINTR`=8), completely different
  `c_iflag`/`c_oflag`/`c_cflag`/`c_lflag` bit positions for every
  same-named flag, and literal baud numbers in `c_ispeed`/`c_ospeed`
  instead of Bionic/Linux's small-integer B-codes (this project's own
  `B9600` is the literal integer `13`, not `9600`). Every ioctl request
  number, struct size, and flag-bit value was cross-checked against this
  build host's real Xcode SDK headers (`<sys/termios.h>`/`<sys/ttycom.h>`)
  and a host-native (real system `clang`, real libSystem) reference
  program compiled and run directly on this machine printing each
  constant -- not copied from memory, matching this session's own
  sendmsg/recvmsg SCM_RIGHTS investigation methodology (see that entry
  above). `tcflow()`'s BSD-style dispatch to four separate ioctls
  (`TIOCSTOP`/`TIOCSTART`/`TIOCIXOFF`/`TIOCIXON` rather than one generic
  ioctl taking the action code) and `tcsendbreak()`'s set-break/sleep-
  400ms/clear-break sequence were carefully reasoned from this SDK's own
  `<sys/ttycom.h>` request-name comments and this host's `man 3 tcflow`
  page (matching the well-known, essentially unchanged-for-decades BSD
  `lib/libc` implementation lineage), not independently confirmed against
  Apple's closed-source `Libc` -- flagged the same honest way any
  raw ioctl mapping this project couldn't cross-check against real source
  gets flagged elsewhere.

  A handful of Bionic-only bits (`IUCLC`, `OLCUC`, `XCASE` -- SysV-only
  legacy concepts) and Darwin-only bits (`CIGNORE`, `ALTWERASE`,
  `NOKERNINFO`, the hardware flow-control `*_OFLOW`/`*_IFLOW` bits,
  `VDSUSP`/`VSTATUS`) have no counterpart on the other side and are
  dropped in translation, each documented at its own drop site in the new
  file rather than guessed at; the `NLDLY`/`CRDLY`/`TABDLY`/`BSDLY`/
  `VTDLY`/`FFDLY` output-delay bit groups are dropped too, since Darwin's
  own header marks them "unimplemented ... will currently result in
  unexpected behaviour" and translating them bit-for-bit would add
  meaningful complexity for zero real behavior.

  Verified on this real macOS host: `tests/termios_echo_roundtrip_test`
  now passes (`ok`, not `skip`) under `CRT_USE_HOST_TTY=1` with a real
  pty. Added a new regression, `tests/termios_line_control_test.c`
  (registered as `termios_line_control_test_runs`), covering what the
  echo-roundtrip test doesn't: `tcdrain`/`tcflow` (all 4 actions)/
  `tcflush` (all 3 selectors)/`tcsendbreak`, a `c_cflag` round trip
  through `CS7`/`PARENB`/`CSTOPB`/`PARODD` (exercises the 2-bit `CSIZE`
  index translation, not just single-bit flags), a `VMIN`/`VTIME` `c_cc`
  round trip (exercises the index-translation table at indices other than
  the echo test's own `VINTR`/`VEOF`), and a `B9600` speed round trip
  (exercises the B-code<->literal-baud-number translation) -- all pass
  under the same real-pty harness. Full suite: 90/90 (89 plus the new
  test), both static and shared `libc` builds, no regressions.

- **Implemented real Linux termios (`tcgetattr`/`tcsetattr`/`tcdrain`/
  `tcflow`/`tcflush`/`tcsendbreak`), fixing a real
  `termios_echo_roundtrip_test` failure.** Found while auditing CRT/PAL
  behavior ahead of the libcrtgfx tranche, on a real Linux aarch64 host
  with an actual terminal attached (`ctest` itself has no controlling
  tty, so this had never failed there): `tcgetattr()`/`tcsetattr()` on
  every non-Windows host were pure software stubs -- `tcsetattr()`
  silently discarded everything it was asked to set, and `tcgetattr()`
  always returned one fixed, hardcoded `struct termios` regardless of any
  prior `tcsetattr()` call. This is the exact same class of bug already
  found and fixed on Windows (see `tests/termios_echo_roundtrip_test.c`'s
  own comment, written as that regression), just never caught on Linux
  until a real terminal was available to run the test against.
  Implemented for real via the same `TCGETS`/`TCSETS{,W,F}` ioctls
  `isatty()` already used as a pure success/failure probe
  (`libc/src/arch/linux/common/termios.c`, new): converts between the
  real Linux kernel `struct termios` (`asm-generic/termbits.h`, cross-
  checked against the real kernel UAPI header -- no separate speed
  fields at all, `NCCS=19` not this project's public `NCCS(32)`, baud
  rate packed into `c_cflag`'s `CBAUD`/`CBAUDEX` bits) and this
  project's own public `struct termios`; extracting/injecting speed
  needed no lookup table since this project's own `B0`/`B9600`/
  `B38400`/`B115200` constants already are the literal raw CBAUD-encoded
  values. Also wired up the previously-always-no-op `tcdrain()`/
  `tcflow()`/`tcflush()`/`tcsendbreak()` to the matching `TCSBRK`/
  `TCXONC`/`TCFLSH` ioctls (already-defined constants in
  `include/sys/ioctl.h`, unused until now) while in there -- same class
  of latent bug, cheap to fix alongside the main one. macOS keeps its
  pre-existing hardcoded-stub fallback for now (different BSD ioctl
  numbers/struct layout, not ported this session); Windows already had
  its own real fix from the earlier stty-driven investigation this same
  test regresses.
  - **Verified the bug and the fix on this real Linux aarch64 host**,
    without a genuine attached console available in this sandbox: used
    `script -qc <binary> /dev/null` to allocate a real pty and run the
    test attached to it as `/dev/tty`. Confirmed the exact failure first
    (reverting to the pre-fix stub reproduced the user's own report
    byte-for-byte: `termios_echo_roundtrip_test: tcgetattr() did not
    return exactly what tcsetattr() was asked to set (round-trip
    mismatch)`), then confirmed the fix resolves it
    (`termios_echo_roundtrip_test: ok`). A standalone follow-up check
    exercised `tcdrain()`/`tcflush()`/`tcflow()`/`tcsendbreak()` against
    the same real pty, all succeeding. Full `ctest` 89/89 throughout
    (the termios test itself reports `skip` under plain `ctest`, which
    has no controlling tty -- expected, matches the test's own designed
    behavior for a console-less environment). Not yet independently
    re-confirmed by the user on the machine that originally reported
    this, or on macOS/Windows.

- **Fixed the real macOS `sendmsg`/`recvmsg`+`SCM_RIGHTS` end-to-end failure
  the fixes below were still waiting on**, found by actually running
  `tests/sendmsg_scm_rights_test` on real macOS hardware for the first time
  (`sendmsg_scm_rights_test_runs` failing in `ctest`: `AF_UNIX bind
  errno=97`). Four distinct real ABI bugs stacked on top of each other,
  found and fixed one layer at a time -- each fix exposed the next failure
  underneath it rather than resolving the test outright, so all four had to
  be found before it passed:

  1. **AF_UNIX `bind()`/`connect()` rejected outright.** `to_darwin_sockaddr()`
     (`libc/src/socket.c`) only ever handled `AF_INET`, hard-rejecting every
     other family with `errno = EAFNOSUPPORT` (Bionic's own value, `97` --
     coincidentally the same number as Darwin's real `ENOLINK`, which made
     the first-glance symptom look like an errno-translation bug in
     `__crt_macos_to_bionic_errno()`; that function was checked and is
     correct, this was a red herring). Real Darwin/XNU's own
     `struct sockaddr_un` -- like `struct sockaddr_in` before it -- carries a
     1-byte `sun_len` + 1-byte `sun_family` prefix instead of Linux's plain
     2-byte family field. Fixed by adding a `struct crt_darwin_sockaddr_un`
     alongside the existing `struct crt_darwin_sockaddr_in`, unioned into a
     new `union crt_darwin_sockaddr_storage`, and generalizing
     `to_darwin_sockaddr()`/`from_darwin_sockaddr()` to dispatch on
     `sa_family` between the two instead of assuming `AF_INET`. All 8 call
     sites (`bind`/`accept`/`connect`/`sendto`/`recvfrom`/`sendmsg`/
     `recvmsg`/`getsockname`) updated to the union type.

  2. **`sendmsg` failing `EINVAL` even after AF_UNIX worked.** Real
     Darwin/XNU's own `struct msghdr` uses a 4-byte `int msg_iovlen` and
     4-byte `socklen_t msg_controllen`, not 8-byte `size_t` like Linux's
     real ABI -- the same class of divergence `struct cmsghdr.cmsg_len`
     already had to handle (previous entry below). Since `sendmsg()`/
     `recvmsg()` pass this struct's bytes straight through to the raw
     kernel syscall, the wrong width shifts every field after `msg_iovlen`
     (`msg_control`/`msg_controllen`/`msg_flags`) to the wrong byte offset.
     Fixed by narrowing both fields under `CRT_TARGET_OS_MACOS` in
     `include/sys/socket.h`, matching the `cmsg_len` precedent.

  3. **Still `EINVAL` after the field-width fix.** Real Darwin aligns
     ancillary-data (`cmsghdr`) records to 4 bytes
     (`__DARWIN_ALIGNBYTES32`), not to `sizeof(size_t)` (8 bytes) like this
     project's `CMSG_ALIGN` assumed unconditionally. Since `CMSG_DATA`/
     `CMSG_SPACE`/`CMSG_NXTHDR` are all defined in terms of `CMSG_ALIGN`,
     the wrong alignment put the payload at a byte offset the real kernel
     didn't expect. Fixed by adding `__CRT_CMSG_ALIGN_UNIT` (4 bytes on
     macOS, `sizeof(size_t)` elsewhere) and rewriting `CMSG_ALIGN` in terms
     of it.

  4. **Still `EINVAL` after the layout was byte-for-byte identical to a
     real host-native reference program** (root-caused by writing a tiny
     C program that builds the exact same `SCM_RIGHTS` control message,
     compiling it once with the real host `clang`/libSystem and once
     through this project's own `crt-cc`/sysroot, and diffing the raw
     bytes -- the technique that also found bugs 2 and 3 above). The one
     remaining byte difference was `cmsg_level`: real Darwin's `SOL_SOCKET`
     is `0xffff`, but this project's own Bionic-shaped `SOL_SOCKET` (`1`)
     was going out untranslated. `setsockopt()`/`getsockopt()` already
     translate this via `CRT_DARWIN_SOL_SOCKET` for their own `level`
     parameter, but that translation never extended to a cmsghdr's own
     `cmsg_level` field inside `msg_control`. Fixed by adding
     `translate_cmsg_levels()` (`libc/src/socket.c`), which walks a
     `msg_control` buffer's `cmsghdr` chain via the existing
     `CMSG_FIRSTHDR`/`CMSG_NXTHDR` macros, translating `cmsg_level`
     Bionic->Darwin (for `sendmsg`, on a local translated copy so the
     caller's own buffer is never mutated) or Darwin->Bionic (for
     `recvmsg`, in place after the syscall returns, since `recvmsg`
     already fills the caller's buffer directly). Both `sendmsg()` and
     `recvmsg()` were also restructured so this control-buffer translation
     runs independently of whether `msg_name` is set -- the prior code
     only entered its Darwin-translation branch when `msg_name != 0`, but
     the actual `SCM_RIGHTS` test case (`tests/sendmsg_scm_rights_test.c`)
     uses an already-connected socket with `msg_name == 0` and only
     `msg_control` set, so it was skipping ancillary-data translation
     entirely.

  Verified: `tests/sendmsg_scm_rights_test` now prints
  `sendmsg_scm_rights_test: ok` when run directly, and the full
  `ctest --preset macos-host-ninja-debug` suite passes 89/89 with no
  regressions from any of the four `socket.c`/`socket.h` changes above.

- **Fixed a real macOS-only `struct cmsghdr` ABI bug, caught by CI on the
  very first push of the `sendmsg`/`recvmsg` work below.** Real Darwin/XNU's
  own `struct cmsghdr` uses a 4-byte `socklen_t cmsg_len` (X/Open XSI
  compliance); this project's `include/sys/socket.h` used an 8-byte
  `size_t cmsg_len` unconditionally, which is correct for Linux's real ABI
  (explaining why both `linux-amd64` and `linux-arm64` CI legs -- which
  exercise the exact same `sendmsg`/`recvmsg`/`SCM_RIGHTS` code path via
  `tests/sendmsg_scm_rights_test.c` -- passed cleanly) but shifts every
  field after `cmsg_len` by 4 bytes when the real macOS kernel parses a
  message built with this layout: `cmsg_level` gets read from bytes that
  were actually the zero-valued upper half of the 8-byte `cmsg_len`,
  reading as `0` instead of `SOL_SOCKET`. `macos-aarch64` CI failed with
  "Process completed with exit code 8" (`cmake --workflow`'s propagated
  exit status) at the "Configure, build, and test" step -- GitHub's own
  Actions log viewer required sign-in to show the underlying compiler/test
  output directly, so this was root-caused from the job/step-level
  annotation plus a from-first-principles ABI review (real Bionic vs. real
  Darwin `<sys/socket.h>`), not from reading the raw log. Fixed by making
  `cmsg_len`'s type conditional on `CRT_TARGET_OS_MACOS`
  (`socklen_t` there, `size_t` everywhere else) -- every `CMSG_*` macro is
  already defined in terms of `sizeof(struct cmsghdr)`, so this is the
  only line that needed to change; no other code (not `libc/src/socket.c`,
  not the Windows `__crt_sys_sendmsg()`/`__crt_sys_recvmsg()` SCM_RIGHTS
  detection, not the test files) had to know about it. All 104 tests still
  pass on Windows via a genuine `cmake --fresh` reconfigure; the actual
  fix still needs the same real-macOS-CI confirmation the syscall-number
  reasoning below was already waiting on.

- **Implemented `sendmsg`/`recvmsg` + `SCM_RIGHTS` fd passing and
  `memfd_create`**, the last two "high priority" findings from the Bionic
  libc gap audit below -- both concretely block the Wayland-compositor-
  boundary goal, unlike `semaphore.h`/`<stdatomic.h>` (the previous entry)
  which were general-purpose.

  `sendmsg`/`recvmsg` (`include/sys/socket.h`: `struct msghdr`/`struct
  cmsghdr`/`SCM_RIGHTS`/`CMSG_FIRSTHDR`/`CMSG_NXTHDR`/`CMSG_DATA`/
  `CMSG_SPACE`/`CMSG_LEN`; `libc/src/socket.c`: the public `sendmsg()`/
  `recvmsg()` dispatching to new `__crt_sys_sendmsg()`/
  `__crt_sys_recvmsg()`): Linux and macOS get real raw syscall trampolines
  (`libc/src/arch/{linux,macos}/{x86_64,aarch64}/syscall.S`) with full
  native `SCM_RIGHTS` support, no PAL invention needed -- both kernels have
  supported this natively for decades. **The syscall numbers were
  carefully reasoned, not copy-pasted from a reference, and were NOT
  independently verified on real hardware from the Windows-only session
  that wrote them**: Linux x86_64 `sendmsg`=46/`recvmsg`=47 sit immediately
  after this project's own already-tested `sendto`=44/`recvfrom`=45 in
  `arch/x86/entry/syscalls/syscall_64.tbl`; Linux aarch64 (generic table)
  `sendmsg`=211/`recvmsg`=212 sit immediately after `sendto`=206/
  `recvfrom`=207; Darwin/XNU `sendmsg`=28 (0x1c)/`recvmsg`=27 (0x1b) sit
  immediately *before* this project's own already-tested, directly-
  confirmed `recvfrom`=29 (0x1d)/`accept`=30 in the classic BSD socket
  syscall block. This matches the exact same gap `linkat()`'s own Linux/
  macOS trampolines had earlier this session until the user's real
  hardware testing closed it (see that entry below) -- `tests/
  sendmsg_scm_rights_test.c`'s real AF_UNIX `SCM_RIGHTS` fd-passing round
  trip (create a `memfd_create()`-backed fd with known content, pass it
  over a connected `AF_UNIX SOCK_STREAM` pair, read the content back
  through the *received* fd on the other end) is what verifies these
  numbers for real the next time it runs on real Linux/macOS CI or
  hardware, not this session.

  Windows has no `SCM_RIGHTS`-equivalent mechanism for `AF_UNIX` sockets
  at all -- cross-process handle sharing there is `DuplicateHandle()`-
  based, a completely different, PID-targeted model, not a socket-
  ancillary-data one. `__crt_sys_sendmsg()`/`__crt_sys_recvmsg()` on
  Windows detect an `SCM_RIGHTS` control message up front (walking
  `msg_control` via the same `CMSG_FIRSTHDR`/`CMSG_NXTHDR` macros real
  callers use) and fail immediately with `-ENOTSUP`, rather than silently
  sending/receiving only the data half of the message and dropping the
  fds the caller actually needed transferred. Plain multi-`iovec` data
  still works on Windows: Winsock has no native `sendmsg()`/`recvmsg()`,
  so every iovec segment is gathered into one contiguous buffer (a single
  `send()`/`sendto()` call, preserving one-call-one-datagram semantics
  correctly for datagram sockets, not split into multiple separate
  sends) and scattered back out symmetrically on the receive side.

  `memfd_create` (`include/sys/mman.h`, `libc/src/mman.c`): deliberately
  **not** implemented as a Linux raw syscall, specifically to avoid a
  *third* unverified syscall number stacked on top of the `sendmsg`/
  `recvmsg` ones above. Instead implemented as a fully portable function --
  create a uniquely-named file, then unlink it immediately -- the exact
  same proven technique this project's own `tmpfile()` already uses (see
  `libc/src/stdio.c`), so it needed zero new PAL work and is provably
  correct on every host right now rather than pending real-hardware
  verification. This gives the real thing the near-term consumer actually
  needs (an anonymous fd, nameless on the filesystem, safe to
  `mmap(MAP_SHARED)` and hand to another process via the `SCM_RIGHTS`
  mechanism above) without Linux memfd's `F_ADD_SEALS`/`F_GET_SEALS`
  sealing support, which isn't implemented (`MFD_ALLOW_SEALING` is
  accepted but has no effect).

  New permanent regressions: `tests/memfd_create_test.c` (flag validation,
  two independent memfds proven not to collide, a real `write`/`lseek`/
  `read` round trip, and a real `mmap(MAP_SHARED)` round trip proving a
  write through the mapping reaches the underlying fd), `tests/
  sendmsg_scm_rights_test.c` (a real multi-iovec gather/scatter round trip
  over AF_INET loopback on every host, plus the platform-specific
  behavior above -- the real `SCM_RIGHTS` round trip on Linux/macOS, the
  documented `ENOTSUP` on Windows). All 104 tests pass via a genuine
  `cmake --fresh` reconfigure plus full rebuild.

- **Implemented `semaphore.h` and public `<stdatomic.h>`**, the two
  cheapest "high priority" findings from the Bionic libc gap audit below
  (both had every needed primitive already sitting in the tree, unlike the
  `sendmsg`/`recvmsg`+`SCM_RIGHTS`/`memfd_create` pair, which stays
  deliberately deferred to when the compositor-boundary work actually
  begins).

  `semaphore.h` (`libc/src/semaphore.c`): `sem_t`/`sem_init`/`sem_destroy`/
  `sem_wait`/`sem_trywait`/`sem_timedwait`/`sem_post`/`sem_getvalue`, built
  over the same private futex/wait-address primitive
  (`__crt_wait32`/`__crt_wake32_*`) that already backs `pthread_mutex`/
  `pthread_cond`/`pthread_rwlock` -- a plain non-negative count word, CAS
  fast path for `sem_wait`/`sem_trywait`/`sem_post`, blocking via
  `__crt_wait32`/`__crt_wait32_timed` when the count is `0`. `pshared`
  cross-process semaphores return `ENOTSUP`, matching `PTHREAD_PROCESS_
  SHARED`'s existing status on `pthread_mutex`/`rwlock`/`spinlock`. Named
  semaphores (`sem_open`/`sem_close`/`sem_unlink`) are declared but always
  fail with `ENOSYS`, matching real Bionic's own policy -- Android has
  never supported them either. New regression: `tests/semaphore_test.c`,
  covering argument validation, a same-thread post/wait/trywait/getvalue
  round trip, a real `sem_timedwait` timeout, and a real cross-thread
  `pthread_create` + blocking `sem_wait()` + `sem_post()` wakeup (not just
  the CAS fast path).

  Public `<stdatomic.h>` (`include/stdatomic.h`): a real C11 atomics
  header, not just the existing private `libc/include/private/
  crt_atomic.h` int-only internal layer. Implemented over Clang's
  `__c11_atomic_*` builtins acting on real `_Atomic(T)`-qualified types --
  verified directly against this project's exact `-std=gnu99
  -ffreestanding` build flags with a throwaway probe before writing the
  real header, not assumed from the C11 spec text: `_Generic` (a real C11-
  only feature the standard's own reference `<stdatomic.h>` text leans on
  for type-generic macros) isn't available under `-std=gnu99`, but that
  turned out not to matter, since `__c11_atomic_*` builtins are themselves
  already type-generic compiler magic -- they infer the pointee type
  straight from the `_Atomic`-qualified pointer argument, so plain macros
  are enough. `_Atomic` itself is a Clang language extension available
  regardless of `-std=gnu99` vs. `-std=c11`, also confirmed by direct
  probe rather than assumed. Covers `memory_order`, the core scalar
  `atomic_*` typedefs plus the `stdint.h`-backed ones (`atomic_size_t`,
  `atomic_intptr_t`, etc. -- `atomic_char16_t`/`atomic_char32_t`
  intentionally deferred alongside the still-missing `uchar.h`),
  `atomic_flag`, every `atomic_*`/`atomic_*_explicit` operation, fences,
  and the `ATOMIC_*_LOCK_FREE` macros (mapped straight to Clang's own
  `__CLANG_ATOMIC_*_LOCK_FREE` predefined macros). New regression:
  `tests/stdatomic_test.c`.

  All 102 tests pass via a genuine `cmake --fresh` reconfigure plus full
  rebuild.

- **Audited this project's libc surface against real Android Bionic before
  starting `libcrtgfx`**, per `docs/runtime_roadmap.md`'s own "reduce the
  remaining planned libc/PAL items... before starting the upper runtime in
  earnest" order-of-work note. Evidence-based (grepped `include/`/
  `libc/src/` directly, not guessed from memory of what Bionic "probably"
  has): confirmed real, concrete gaps most relevant to the Wayland-
  compatible compositor boundary goal -- `sendmsg`/`recvmsg` +
  `SCM_RIGHTS`/`CMSG_*` ancillary-data fd passing (entirely absent; this is
  Wayland's core wire-protocol mechanism for handing shared-memory/DMA-BUF
  fds between client and compositor, so no compositor boundary is possible
  without it), `memfd_create` (absent; the modern anonymous shared-memory-
  fd mechanism that pairs with it, matching `docs/project_meanings.md`'s
  own "ashmem/memfd-style shared memory" architecture layer), `semaphore.h`
  (entirely absent despite the rest of pthreads being complete enough to
  implement it cheaply over the existing private futex/wait-address layer),
  and public `<stdatomic.h>` (still only a private internal layer, already
  known-deferred per `docs/import_bionic.md` but now with a concrete near-
  term consumer since QuickJS bring-up is the very next roadmap step).
  Also found, lower priority: `sys/epoll.h`/`sys/eventfd.h`/
  `sys/timerfd.h`, `dl_iterate_phdr`/`link.h`/`elf.h`/`dladdr`,
  `PTHREAD_PROCESS_SHARED` support, and general completeness items
  (`glob.h`, `sys/prctl.h`, `ucontext.h`, `ifaddrs.h`, `threads.h`,
  `uchar.h`) with no identified near-term consumer. Full findings and
  priority tiers recorded in `docs/bionic_libc_gaps.md`, pointed to from
  `TODO.md`'s new "Bionic libc completeness before `libcrtgfx`" section.
  Investigation only -- no implementation yet, pending a decision on what
  to build now versus defer to when the compositor-boundary work actually
  starts.

- **Moved the toybox applet status detail out of `TODO.md` into
  `docs/toybox_applet_status.md`**, mirroring `docs/job_control.md`'s
  existing pattern (a short pointer in `TODO.md`, full detail in `docs/`).
  `TODO.md`'s "in progress" section had grown a long, applet-by-applet
  writeup (the `globals.h`/`flags.h` registration mechanism and its two
  traps, the still-open `expand`/`logger`/`fold`/`uudecode`/`cal`/`split`/
  `strings` and `timeout` items, and the deferred-applet list with each
  one's concrete reason) that belongs in `docs/` per this project's own
  stated policy ("Detailed policy and provenance stay in `docs/`"), not
  repeated inline in the work-queue file every time it's touched. No
  content was dropped, only relocated -- see `docs/toybox_applet_status.md`
  for the current detail and this entry as the historical record that the
  investigation behind it happened.

- **Enabled `df`/`stty`, fixing two real, general PAL bugs the enablement
  uncovered along the way.** Both were investigated concretely (upstream
  source read directly, not guessed) as part of a review of the remaining
  "Keep deeper Linux-like applets deferred" list and found unexpectedly
  tractable given infrastructure this session already built: `df.c` uses
  `xgetmountlist()` (`/proc/mounts` + `getmntent()`, both already
  implemented) and `statvfs()` (already backed by real
  `GetDiskFreeSpaceExA()`); `stty.c` uses `tcgetattr`/`tcsetattr`/
  `ioctl(TIOCGWINSZ/TIOCSWINSZ)`, all already implemented. Both were
  LLP64-audited clean. Getting them actually working end to end (not just
  compiling) surfaced three separate, real issues:
  - **`globals.h`, not `flags.h`, is the real source of truth for
    `GLOBALS()` union-member storage** -- a correction of this session's
    own earlier assumption (used for the `expand`/`logger`/`fold`/
    `uudecode`/`cal`/`split`/`strings` batch, see `TODO.md`). `flags.h`'s
    `#define TT this.X` pattern is generated unconditionally for every
    applet in `newtoys.h` regardless of whether real union backing exists;
    the actual storage is a separate `struct X_data { ... }; ... struct
    X_data X;` pair inside `extern union global_union` in `globals.h`,
    which was missing for both `df`/`stty`. Confirmed by contrasting a
    working applet (`cut`, has a real `struct cut_data cut;` member)
    against `df`/`stty` (genuinely absent). Fixed by hand-adding
    `struct df_data`/`struct stty_data` (copied verbatim, field-for-field,
    from each applet's own `GLOBALS()` macro) and their union members --
    judged low-risk unlike `flags.h`'s bit-position `FLAG_x` machinery,
    since this is a direct, mechanical mirror of already-known fields.
  - **`flags.h`'s checked-in snapshot silently disables any applet that
    was off when it was generated, even after `config.h` re-enables it.**
    Both `df`/`stty` compiled fine after the `globals.h` fix, but every
    flag silently did nothing at runtime (`df -h` behaved exactly like
    plain `df` -- no error, just wrong output; `stty -a`/`stty -g` fell
    through to the bare no-flags path). Root-caused with a debug print of
    `toys.optflags`/`FLAG_x`: mkflags emits `#define FLAG_x
    (FORCED_FLAG<<N)` instead of `#define FLAG_x (1LL<<N)` for any flag
    that was disabled in the config snapshot flags.h was generated
    against, and `FORCED_FLAG` itself is `0LL` unless the specific
    `.c` file defines `FORCE_FLAGS` before including `toys.h` (a small
    number of files do, e.g. `cat.c`/`cp.c`/`id.c`, for unrelated
    multiplexed-applet reasons) -- so every one of `df`/`stty`'s flags
    were dead code, always evaluating to 0, without any compiler warning.
    The bit *positions* mkflags assigns are independent of enabled state
    (derived from the applet's full `allflags` superset, not just what's
    currently compiled in) and were already correct. Fixed by hand-editing
    just the `FOR_df`/`FOR_stty` blocks in `flags.h`, changing
    `FORCED_FLAG` to `1LL` for each of their (already correctly
    positioned) flags -- a narrow, mechanical substitution, not a
    bit-position change, so it doesn't carry the same risk this session
    already judged too high for hand-editing *new* flag positions.
  - **`tcgetattr()`/`tcsetattr()` had no real round-trip fidelity beyond
    three bits, a general PAL gap `stty` was simply the first thing in
    this tree to actually exercise.** `__crt_sys_tcgetattr()` re-derived
    `c_iflag`/`c_oflag`/`c_cflag`/`c_cc[]`/speeds from hardcoded constants
    on every call and only reflected `ISIG`/`IEXTEN`/`ICANON`/`ECHO`+
    `ECHOE`+`ECHOK` from the real Windows console mode -- and Windows only
    exposes one bit (`ENABLE_ECHO_INPUT`) for all three of `ECHO`/`ECHOE`/
    `ECHOK` combined, no separate control for "erase visually on
    backspace" vs. "erase whole line on kill char". `stty -echo` (clearing
    only `ECHO`, leaving `ECHOE`/`ECHOK` set -- exactly what real
    stty does and what its own set-then-verify check requires) came back
    from a follow-up `tcgetattr()` with all three cleared, a real
    mismatch that made even the single most common `stty` invocation
    pattern fail with "unable to perform all requested operations".
    Root-caused with a debug byte-diff of the mismatching `struct
    termios`, not guessed. Fixed with a per-fd shadow
    (`fd_termios_shadow[]`/`fd_termios_shadow_valid[]` in
    `libc/src/arch/windows/common/syscall.c`, reset on fd-slot reuse
    alongside the existing `fd_nonblock[]`/`fd_pipe_write_only[]`
    pattern): once `tcsetattr()` has been called on an fd at least once,
    `tcgetattr()` returns that exact struct back verbatim (full fidelity
    for every field, including the ones -- `c_cc[]`, speeds, most of
    `c_iflag`/`c_oflag`/`c_cflag` -- Windows' console has no real backing
    for at all) instead of re-deriving a fresh default; a never-set fd
    keeps deriving its initial reading from the real console mode bits,
    unchanged from before. This is a real, general fix (any termios
    consumer doing a set-then-verify pattern, not stty-specific). New
    permanent regression: `tests/termios_echo_roundtrip_test.c` (skips
    gracefully if no real console is attached, confirmed necessary: this
    project's own dev environment for this session has no real console at
    all in some contexts and only a partial one -- input works, output
    screen-buffer queries don't -- in others).

  `TIOCGWINSZ` genuinely returns `ENOTTY` in that same partial-console
  environment (`GetConsoleScreenBufferInfo` fails with no real output
  screen buffer, confirmed with a throwaway repro against `CON`/`CONOUT$`/
  `CONIN$` directly) -- correct behavior for that real condition per both
  POSIX and upstream toybox's own `perror_exit`-on-failure semantics, not
  a bug, so `stty -a`/`stty size` can't be exercised end-to-end in every
  environment even though the applet itself is correct. `df -h`/`df -k`/
  `df`, and `stty -g`/`stty -echo`/`stty echo`/`stty -icanon`/`stty
  icanon`/`stty sane`/bare `stty` were all verified directly against the
  real rootfs binaries. New permanent ctests:
  `crt_mksh_rootfs_df_runs`/`crt_mksh_rootfs_stty_runs` (mksh-driven,
  matching the `dos2unix` pattern) plus the standalone
  `termios_echo_roundtrip_test` above. All 100 tests pass via a genuine
  `cmake --fresh` reconfigure plus full rebuild.

- **Root-caused and fixed `timeout`'s hang -- a real, general Windows
  `poll()` bug, not a signal issue as first suspected.** Investigated with
  a minimal standalone repro (`fork()` a child that exits almost
  immediately; `pipe()`; `poll()` the pipe's *write* end for `POLLIN` with
  a 3000ms timeout, nothing ever written to it) rather than guessing from
  `timeout.c`'s own source: `poll()` returned "ready" in about 7ms, not
  after the real 3000ms timeout. `__crt_sys_poll()`'s `poll_handle()`
  (`libc/src/arch/windows/common/syscall.c`) called `PeekNamedPipe()`
  unconditionally on any `FILE_TYPE_PIPE` handle to answer `POLLIN` --
  that call's documented behavior only covers a pipe's read end; called on
  the write end it does not reliably report "no data available" the way
  `POLLIN` semantics require. `timeout.c` upstream deliberately polls an
  otherwise-unused pipe's write end as a pure sleep-until-timeout
  mechanism for its non-`-i` (non-inactivity) mode -- exactly the shape
  that exposed this. Fixed by tracking which fd is a pipe write end
  (`fd_pipe_write_only[]`, set in `__crt_sys_pipe()`, checked in
  `poll_handle()` before ever calling `PeekNamedPipe()`) so a write end
  now correctly reports "not ready" until the real timeout elapses. New
  permanent regression: `tests/poll_pipe_write_end_test.c` -- a real,
  general fix (any code polling a pipe write end for readability, not
  just `timeout`), independent of `timeout`'s own applet status below.

  Verifying the real `timeout` applet end to end after this fix (not just
  the standalone repro) surfaced two more, separate, deeper gaps rather
  than closing the item outright -- the hang is gone, but the applet still
  isn't fully correct:
  - `deliver_signal()`'s `SA_SIGINFO` path (`libc/src/signal.c`) always
    hands the handler a zeroed `siginfo_t` (`si_code = 0`, `si_status =
    0`) regardless of which signal or why. `timeout.c`'s own `SIGCHLD`
    handler reads exactly those fields to learn the exited child's real
    status, so it always computed a wrong exit code (`128`, since
    `si_code` can never equal the real `CLD_EXITED`) even for a child that
    exited successfully. This project's own child-tracking tables
    (`child_process_table`/`child_pid_table`, already used by
    `waitpid()`) hold the real data; `SIGCHLD` dispatch just doesn't
    thread it through to `siginfo_t` yet.
  - `kill()` still only supports signaling the calling process itself (a
    gap already known from earlier in this session, not new) -- sending a
    signal to a genuinely different process is a no-op. `timeout`'s own
    deadline enforcement is exactly `kill(pid, SIGTERM)` on the child once
    the clock runs out, so it silently does nothing: confirmed directly
    with `timeout 2 sleep 10`, which ran the full ~10 seconds instead of
    being cut off at ~2. The child was never actually terminated; the
    command just returned once it finished on its own.

  `timeout` stays disabled -- re-registering it now would ship a command
  that reports wrong exit codes and, worse, silently fails to enforce the
  one thing it exists to do. Both gaps are real, separate, and scoped for
  whoever picks this up next; the original infinite-hang symptom that
  prompted this investigation is genuinely fixed and covered by a
  permanent regression regardless. All 97 tests pass via a genuine
  `cmake --fresh` reconfigure plus full rebuild.

- **Implemented real Windows POSIX-semantics rename, re-enabled `dos2unix`/
  `unix2dos`.** The `rename()`-over-a-file-with-an-open-handle limitation
  flagged the same day (the previous entry below) is fixed for real:
  `windows_rename_posix_semantics()` in `libc/src/arch/windows/common/
  syscall.c` now tries `SetFileInformationByHandle(FileRenameInfoEx,
  FILE_RENAME_FLAG_POSIX_SEMANTICS | FILE_RENAME_FLAG_REPLACE_IF_EXISTS)`
  first -- the one Win32 mechanism that actually replicates POSIX
  rename()'s "replace a file even if something else still has it open"
  behavior (Windows 10 1607+, NTFS) -- before falling through to the
  pre-existing `MoveFileExA()`-based retry loop unchanged, so hosts/
  filesystems where the new call isn't available keep the old (partial)
  behavior rather than losing rename() entirely. Verified directly with
  the same minimal standalone repro that found the original bug: opening
  a file read-only without closing it, then renaming a different file
  onto that same path from the same process, which used to fail with
  `ERROR_ACCESS_DENIED` regardless of how long a retry loop waited, now
  succeeds and the target's content is correctly replaced.

  Building this surfaced a second real bug before it ever reached
  `ctest`: the new code path's `CreateFileA()` call omitted
  `FILE_FLAG_OPEN_REPARSE_POINT`, so opening a *symlink* as the rename
  source silently followed the reparse point and renamed the symlink's
  *target* instead of the link itself -- real POSIX `rename()` never
  follows a symlink this way. Caught by the existing
  `crt_mksh_rootfs_which_stat_readlink_runs` regression (its `ln -sf`
  step creates a temp symlink and renames it over the final destination,
  toybox's own `ln.c` force-overwrite pattern, exactly the shape that
  exposed it) failing after this change, not by anything written
  specifically for `dos2unix`/`unix2dos` -- fixed by adding the flag, all
  96 tests (including that one) pass again.

  `dos2unix`/`unix2dos` re-enabled (`newtoys.h`, rootfs aliases -- both
  already had `CFG_x=1` and their `flags.h` union members from the base
  Android config, same as the rest of the same-day applet batch) and
  functionally verified for real: `printf 'a\r\nb\r\n' | dos2unix` then
  `unix2dos` round-trips through 6 -> 4 -> 6 bytes correctly. New
  permanent regression: `crt_mksh_rootfs_dos2unix_runs` in
  `shell/CMakeLists.txt`. All 96 tests pass via a genuine
  `cmake --fresh` reconfigure plus full rebuild, and the actual `zlib`
  port build (`port-rebuild-zlib`/`port-test-zlib`, both static and
  shared) was re-run end to end given how foundational `rename()` is.

- **`linkat()`'s Linux/macOS raw syscall trampolines are confirmed
  working**: the user built and ran this project on real Linux and macOS
  hardware, through the full `curl` port test (the last, heaviest port in
  this project's own queue). Closes the "unverified" gap the same-day
  `linkat()`/`link()` PAL implementation entry left open -- the Windows
  `CreateHardLinkA` path was already verified in-session; the Linux
  x86_64/aarch64 and macOS x86_64/aarch64 `__crt_sys_link` trampolines
  (added by hand-mirroring the existing `__crt_sys_symlink`/
  `__crt_sys_unlink` pattern, using well-established syscall numbers, with
  no cross-toolchain available to test them in-session) could not be.
  `TODO.md`'s own dated verification item for this is now resolved.

- **Diffed this project's own toybox applet set against the real
  Android/Bionic reference config and closed almost the entire gap,
  after the user asked specifically for a Bionic/Android-parity check
  (not just further ad hoc LLP64 auditing).** `shell/toybox/src/android/
  linux/generated/config.h` turns out to already be a full, real Android
  AOSP-derived defconfig snapshot (`CFG_x=1` for 99 applets, most of
  which this project's own `shell/toybox/crt/generated/newtoys.h` had
  never registered) -- comparing the two directly, rather than continuing
  to hand-pick "next candidates," surfaced the true remaining gap in one
  pass. First, `cut` (the user's own trigger for this investigation, hit
  while testing a real `configure` script): already compiled
  (`CRT_TOYBOX_SOURCES`), already `CFG_CUT=1`, already had its
  `GLOBALS()` union member in `flags.h` -- needed only a `newtoys.h`
  entry and a rootfs alias, LLP64-audited clean. Extending that same
  check to the *whole* config-enabled set found 23 more names in exactly
  the same state (already compiled, already `CFG_x=1`, already in
  `flags.h`) -- LLP64-audited clean and, this time, also functionally
  smoke-tested for real (not just pointer-width-safe) before enabling:
  `cmp`, `comm`, `cpio`, `dd`, `diff`, `du`, `env`, `file`, `find`,
  `getconf`, `hostname`, `md5sum` (+ its `sha1sum`/`sha256sum`/
  `sha512sum` `OLDTOY` aliases, matching Android's own choice to leave
  `sha384sum` disabled), `microcom`, `nl`, `od`, `paste`, `patch`, `seq`,
  `setsid`, `tar`, `truncate`, `xxd`. Both `newtoys.h` insertions were
  verified against `LC_ALL=C sort -c` (matching `toy_find()`'s own
  `strcmp()` byte-order requirement) before and after, given the same
  day's earlier `crc32`/`cp` sort-order regression.

  Functional testing (not just LLP64 auditing) caught two real gaps pure
  pointer-width review would have missed entirely:
  - **`timeout` hangs instead of enforcing its deadline** -- confirmed by
    directly running `timeout 3 true` and having it block well past 3
    seconds. Its `SIGCHLD`-plus-`siginfo_t` async handler
    (`sigsetjmp`/`siglongjmp` out of the handler) combined with a
    `poll()` loop is a real, different shape from anything this PAL's
    Windows signal backend has been exercised against before (see
    `docs/signal_delivery.md`'s own scope notes on blocking syscalls with
    no polling checkpoint) -- left disabled, not investigated further
    this pass.
  - **`dos2unix`/`unix2dos` hit a genuine, non-transient Windows
    limitation**, not a bug that retrying fixes: their shared
    `copy_tempfile()`/`replace_tempfile()` implementation (write a
    converted copy to a tempfile, then rename it over the original) keeps
    the *original* file's own read handle open for the whole conversion,
    and `rename()`-over-a-file-with-another-open-handle reliably fails on
    Windows with `ERROR_ACCESS_DENIED` -- reproduced directly with a
    minimal standalone repro (open a file read-only without closing it,
    then `rename()` a different file onto that same path from the same
    process). Root-caused precisely enough to also find and fix a real,
    general, adjacent bug along the way: `__crt_sys_rename()`
    (`libc/src/arch/windows/common/syscall.c`) was the one
    `MoveFileExA()`/`DeleteFileA()`-family call site in that file that
    never got the delete-pending/handle-timing retry loop
    `__crt_sys_unlink()`/`__crt_sys_symlink()`/`__crt_sys_link()` already
    have -- fixed, a real and generally useful hardening, but confirmed
    (via the same standalone repro) *not* sufficient to fix `dos2unix`
    itself, since that failure isn't transient at all. Fixing `dos2unix`
    for real would need `FILE_RENAME_POSIX_SEMANTICS`
    (`SetFileInformationByHandle`/`FileRenameInfoEx`, Windows 10 1607+,
    the one Win32 mechanism that actually replicates POSIX
    rename-over-open-handle semantics) or restructuring the temp-file
    pattern to close the original before renaming -- left disabled, not
    attempted this pass.
  - `dnsdomainname` was drafted into the batch by mistake (its `NEWTOY`
    lives in the same file as `hostname`'s) and reverted before landing --
    Android's own config actually leaves `CFG_DNSDOMAINNAME=0`.

  Also confirmed via `shell/toybox/crt/generated/config.h` (a small,
  already-existing `#undef`/`#define` override layer this project applies
  on top of the base Android config, `#include`d first in the actual
  compiled `config.h`) that ten more Android-enabled names are *already*
  deliberately forced off here regardless of upstream's own choice --
  `flock`, `gzip`, `zcat`, `mount`, `nproc`, `pgrep`, `pkill`, `ps`,
  `umount`, `unshare` -- matching (and for `ps`/`mount`/`umount`/`pgrep`/
  `pkill`, reinforcing) `TODO.md`'s own existing "Keep deeper Linux-like
  applets deferred" list. Three more Android-enabled names have no source
  file at all in this project's own toybox import and would need a real
  upstream pull first: `install`, `realpath`, `whoami` (an `OLDTOY` alias
  of `logname`, which is absent).

  New rootfs applet-alias entries added to `tools/create_rootfs.py` for
  every name actually enabled. All existing regression tests
  (`crt_mksh_rootfs_toybox_applet_sweep_runs` in particular, which
  dispatches every currently-registered applet name) plus fresh manual
  functional smoke tests for each newly-enabled name pass. All 95 tests
  pass via a genuine `cmake --fresh` reconfigure plus full rebuild.

- **Fixed a real `cp` regression from the same day's earlier toybox
  applet batch, caught by the user asking for a real zlib port
  rebuild.** Inserting `CRC32` between `CKSUM` and `CP` in
  `shell/toybox/crt/generated/newtoys.h` left `crc32` sorted *before*
  `cp` (`'r' > 'p'`), breaking `toy_find()`'s required strict-ascending
  binary-search invariant over `toy_list[]` -- `cp` alone became silently
  unreachable (`toybox: Unknown command cp`). None of that batch's own
  regression tests happened to dispatch `cp`, so `ctest` stayed green;
  this only surfaced when the user asked for an actual `zlib` port
  rebuild, whose `make install` step calls `cp` directly. Root-caused by
  extracting every registered applet name from `newtoys.h` in file order
  and checking it against `LC_ALL=C sort -c` (matching `strcmp()`'s own
  byte-order comparison, which `toy_find()` actually uses) -- confirmed
  `crc32`/`cp` was the only inversion. Fixed by swapping the two lines
  (correct order: `cksum`, `cp`, `crc32`). New permanent regression:
  `crt_mksh_rootfs_toybox_applet_sweep_runs` in `shell/CMakeLists.txt`
  asks toybox itself for its own real, current applet list (plain
  `toybox` with no arguments -- not a hardcoded copy that could drift out
  of sync) and dispatches every single one, catching any future entry
  that becomes unreachable this same way regardless of the underlying
  cause. Verified the test actually catches this exact bug class by
  temporarily reintroducing the swapped order, confirming the new test
  fails with `broken:cp`, then reverting and confirming it passes. Also
  re-ran the actual `zlib` port build end to end (`port-rebuild-zlib`,
  `port-test-zlib`) after the fix -- both static and shared round-trip
  tests pass, `make install`'s own `cp`/`chmod` calls succeed. All 95
  tests pass via a genuine `cmake --fresh` reconfigure plus full rebuild.

- **Implemented real `linkat()`/`link()` PAL backing and enabled the
  `link` toybox applet, closing the one real gap the LLP64 audit batch
  (below) had left open.** `libc/src/fd.c`'s `linkat()` was an
  unconditional `ENOTSUP` stub on *every* host, not just Windows -- a real
  PAL gap, not an LLP64 issue, confirmed by grepping every `libc/src/arch/
  */*/syscall.S` for a `link`/`linkat` trampoline and finding none at all.
  - **Windows**: new `__crt_sys_link()` in `libc/src/arch/windows/common/
    syscall.c`, backed by the real `CreateHardLinkA` (an NTFS feature, not
    a reparse-point emulation the way `symlink()` needs) -- same
    delete-pending/handle-timing retry policy `CreateSymbolicLinkA`/
    `CreateFileA(O_CREAT)` already use. **Directly verified on this
    session's own Windows hardware**: a standalone smoke test, the
    `crt_mksh_rootfs_link_runs` ctest regression, and `toybox link`/`
    system/bin/link.exe` all round-trip a real hardlink correctly (content
    visible through the new name, survives unlinking the original).
  - **Linux x86_64/aarch64, macOS x86_64/aarch64**: new `__crt_sys_link`
    raw syscall trampolines added to each arch's own hand-written
    `syscall.S`, mirroring the exact register-shuffle style the existing
    `__crt_sys_symlink`/`__crt_sys_unlink` trampolines in the same files
    already use. x86_64 (both Linux and Darwin) keeps a legacy 2-arg
    `link` syscall available (Linux `__NR_link`=86, right next to
    `__NR_unlink`=87 already used; Darwin `SYS_link`=9, next to
    `SYS_unlink`=10 already used) -- no register shuffling needed, the
    existing `__crt_sys_link(oldpath, newpath)` C signature already
    matches. Linux aarch64 dropped the legacy syscall entirely (same
    reason `__crt_sys_symlink`/`__crt_sys_unlink` already route through
    `symlinkat`/`unlinkat` there) -- routes through
    `linkat(AT_FDCWD, oldpath, AT_FDCWD, newpath, 0)` (`__NR_linkat`=37)
    instead, with the same kind of register-shuffle `__crt_sys_symlink`
    already does on that arch. **Not verified by an actual build or run
    this session** -- this dev environment is Windows-only with no
    cross-toolchain available to even compile these three files, let alone
    execute them. The syscall numbers and calling convention are
    well-established, stable ABI facts (unchanged across kernel/Darwin
    versions for decades), and the code mechanically mirrors an existing,
    already-working pattern in the exact same files rather than
    inventing a new one, but this is still a real, open gap in this
    project's own "always verify with a real build" discipline until
    confirmed on actual Linux/macOS hardware -- flagged explicitly in
    `TODO.md` rather than silently assumed correct.
  `libc/src/fd.c`'s `linkat()` itself now resolves both paths via
  `make_at_path()` (already existed) and, on non-Windows, additionally
  routes them through `rootfs_path_for_host()` before calling
  `__crt_sys_link()` -- matching `unlink()`/`symlink()`'s own existing
  pattern exactly.

- **Enabled six more toybox applets after auditing the remaining disabled
  ones for LLP64 pointer-width safety: `cksum`/`crc32`, `tsort`, `tty`,
  `unlink`, `uuencode`.** Continues the `which`/`readlink`/`stat`/`touch`/
  `id`/`xargs` batch's own discipline. Read every plausible next candidate
  in `shell/toybox/src/toys/posix/` (`link`, `unlink`, `cksum`, `tty`,
  `logger`, `expand`, `fold`, `tsort`, `uudecode`, `uuencode`, `cal`,
  `split`, `strings`) for two separate hazards, both real:
  - **A genuine LLP64 pointer-truncation bug, found in `tsort.c`**: its own
    `bsearch()`-argument-adjustment trick round-tripped a `char **` through
    `unsigned long` ("do the usual LP64 trick to MAKE IT SHUT UP", the
    comment's own words) -- silently truncates the pointer on Windows
    x86_64/aarch64 (LLP64: `long` stays 32-bit, pointers are 64-bit).
    Changed to `uintptr_t`. Recorded in `shell/toybox/PATCHES.md`'s
    existing "Windows LLP64 Pointer-Width Fixes" section (`tsort.c` added
    to its touched-files list) rather than a new section, since it's the
    same patch category as the `find`/`du`/`ls`/`sed`/`xargs` fixes already
    there.
  - **A previously-undocumented *second* registration gap**, found by
    actually trying to run the newly-compiled applets, not just compiling
    them: being listed in `shell/CMakeLists.txt`'s `CRT_TOYBOX_SOURCES` and
    having a `shell/toybox/crt/generated/newtoys.h` entry is *still* not
    enough -- `shell/toybox/src/android/linux/generated/config.h`'s
    `USE_x(...)`/`CFG_x` macro pair (checked-in, would normally come from
    toybox's own `genconfig.sh`/Kconfig-style `.config`) gates whether a
    `newtoys.h` entry actually compiles into `toy_list[]` at all --
    `USE_x(...)` literally expands to nothing when `CFG_x` is `0`. All
    seven candidates above already had a `CFG_x 0`/empty `USE_x` pair
    sitting in this file (apparently included in whatever broader
    Android-defconfig-driven pass originally generated it), so flipping
    both lines to `1`/`__VA_ARGS__` was enough -- no need to run the real,
    much heavier `genconfig.sh`+`mkflags` pipeline. Confirmed via a direct
    `toybox: Unknown command` failure before this fix and working `toybox
    cksum`/`tsort`/`tty`/`unlink`/`uuencode`/`crc32` (both standalone and
    through the `toybox` multiplexer) after it.
  - **Seven of the thirteen candidates need more than this** and were left
    disabled: `expand`, `logger`, `fold`, `uudecode`, `cal`, `split`,
    `strings` all use toybox's `GLOBALS()` macro, and their per-applet
    state struct is missing from `shell/toybox/src/android/linux/generated/
    flags.h`'s `union global_union` entirely (confirmed: `readlink`/`stat`/
    `touch`/`id`/`xargs`'s own union members are already present in this
    same file, so no one has needed a *real* `flags.h` regeneration for
    this project's own batches so far -- this would be the first). Adding
    them for real needs toybox's own `mkflags` C-preprocessor-based
    generation pipeline (`scripts/make.sh`/`scripts/genconfig.sh`), not a
    two-line hand-edit like `config.h` above -- kept out of this batch as a
    separate, real prerequisite rather than hand-editing a 7600-line
    generated union by guesswork.
  - **`link` was audited and dropped, not an LLP64 issue**: `linkat()`
    (and therefore `link()`, which calls it) is an unconditional `ENOTSUP`
    stub in `libc/src/fd.c` on every host today, not just Windows -- a
    real, separate PAL gap.
  New permanent regression coverage:
  `crt_mksh_rootfs_llp64_batch_runs` in `shell/CMakeLists.txt`, matching
  the existing `crt_mksh_rootfs_which_stat_readlink_runs` pattern (spawns
  `crt_mksh` with `CRT_ROOTFS`/`PATH` set, drives all six new applets
  through a real mksh pipeline). All 93 tests pass, verified via a genuine
  `cmake --fresh` reconfigure plus full rebuild.

- **Added the six virtual rootfs files TODO.md had queued: `/proc/mounts`,
  `/proc/self/status`, `/proc/self/cmdline`, `/proc/self/environ`,
  `/proc/stat`, and `/dev/zero`.** Split cleanly by what each host already
  provides for real: Linux already has a genuine kernel procfs and a real
  `/dev/zero` device, so nothing changed there at all (confirmed via
  `rootfs_path_for_host()`'s existing `host_path_exists()` check, which
  already passes real host paths straight through). macOS has a real
  `/dev/zero` device too (also free), but no `/proc` whatsoever -- a real
  host fact, same as `/proc/self/exe`'s own precedent. Windows has neither.
  Two different mechanisms, matching what each gap actually needed:
  - **`/dev/zero`** (Windows only): a new `CRT_FD_KIND_ZERO` synthetic fd in
    `libc/src/arch/windows/common/syscall.c`, the same no-real-HANDLE shape
    `CRT_FD_KIND_URANDOM` already established -- `read()` zero-fills the
    caller's buffer, `write()` discards (matching real `/dev/zero`
    semantics), wired into `open()`/`close()`/`fstat()`/`stat()`/`access()`.
    Bonus fix picked up along the way: `fstat()` on a `CRT_FD_KIND_URANDOM`
    fd was calling `GetFileType()` on a placeholder value that was never a
    real Windows handle (undefined behavior, just never previously
    exercised) -- now short-circuited the same way `CRT_FD_KIND_ZERO` is.
  - **`/proc/mounts`, `/proc/stat`, `/proc/self/status`,
    `/proc/self/cmdline`, `/proc/self/environ`** (Windows and macOS): a new
    shared, portable virtual-file layer in `libc/src/fd.c`, compiled only
    for `!CRT_TARGET_OS_LINUX`. Content is generated fresh into a small
    stack buffer on every `open()` and handed to the caller through a real
    anonymous pipe (write the whole thing in, close the write end, return
    the read end) -- ordinary `open()`+`read()`+`close()` works with no
    synthetic-fd bookkeeping needed on either host, unlike `/dev/zero`
    above. Cross-platform argv access needed its own per-host answer:
    macOS already has a real, documented API for this
    (`_NSGetArgc()`/`_NSGetArgv()`, matching `/proc/self/exe`'s own
    `_NSGetExecutablePath()` precedent), but Windows had nothing --
    `libc/src/arch/windows/common/crt1.c`'s `mainCRTStartup()` parses argv
    into a file-local static array that nothing outside that file could
    reach. Storage for the Windows copy had to live inside `fd.c` itself
    (part of libc), not in `crt1.c`: `crt1.c`'s own object is only ever
    linked into the final executable (`CRT_STARTUP_OBJECTS`), never into
    `c_shared.dll`, so a raw global defined in `crt1.c` left `fd.c`'s
    reference to it permanently unresolved when linking the DLL variant
    (caught by actually building `c_shared`, not just `c` -- the failure
    only showed up there). Fixed by following `environ`'s own existing
    pattern exactly: `libc/src/env.c` owns `environ`'s real storage and
    `crt1.c` just calls `__crt_env_set_initial()` into it; likewise `fd.c`
    now owns `windows_argc`/`windows_argv`'s storage and exposes a new
    `__crt_windows_set_args()` setter that `crt1.c` calls right before
    `main()` runs. Content choices were kept deliberately honest rather
    than fabricated: `/proc/stat`'s per-core counters are real zeros with a
    comment explaining no host-portable jiffies source exists (only the
    `cpu`/`cpuN` line *count* is real, from the same
    `sysconf(_SC_NPROCESSORS_ONLN)` hook `docs/sysroot_ports.md` already
    documents); `/proc/mounts` uses the literal fstype `crtfs` rather than
    guessing NTFS/APFS/etc, the same spirit as Linux's own kernel using the
    literal fstype `rootfs` for its initial ramfs; `/proc/self/status`'s
    `State:` is always `R (running)`, which is simply true by construction
    (whatever reads its own status is, by definition, currently running).
    `stat()`/`lstat()`/`access()` also recognize all five paths (reporting
    `S_IFREG`, size 0 -- matching real kernel procfs's own "generated on
    read, not stored" stat shape) so a `test -f`/`test -r` guard before
    reading one of these files behaves the same on every host.
  New permanent regression coverage: `tests/dev_zero_test.c` (read/re-read
  all-zero, write-discards, `fstat`/`stat`/`access` report a char device)
  and `tests/virtual_proc_test.c` (structural checks for `/proc/mounts` and
  `/proc/stat` that pass against both the real Linux kernel content and
  this PAL's own synthetic content; `/proc/self/status`'s `Pid:` line
  checked against a real `getpid()`; `/proc/self/cmdline`'s first
  NUL-terminated token checked against the test's own real `argv[0]`;
  `/proc/self/environ` checked for a ctest-injected `ENVIRONMENT` var,
  deliberately using a var present at exec time rather than a live
  `setenv()` call afterward, since real Linux's own `/proc/self/environ` is
  frozen at exec time and does not reflect later `setenv()`/`putenv()`
  calls -- matching that exactly rather than only working by accident on
  the synthetic Windows/macOS path). All 92 tests pass, verified via a
  genuine `cmake --fresh` reconfigure plus full rebuild, per this same
  date's own standing discipline note in `TODO.md`.

- **Seriously evaluated actually implementing Windows stop/resume + `fg`/
  `bg` (scope A: command-driven, e.g. `kill -STOP`/`fg`/`bg` -- not live
  Ctrl-Z keypress detection, which stays permanently out of scope per CRT's
  own PAL philosophy of using each platform's real native primitives rather
  than recreating a full POSIX environment the way Cygwin does), and decided
  to keep it deferred rather than build it now.** Confirmed there is no
  *documented* Win32 API to suspend an entire other process:
  `SuspendThread()` per-thread has a real race (a thread created between
  enumeration and suspension escapes it); `DebugActiveProcess()` +
  `SuspendThread()` attaches a real debugger with broader side effects; Job
  Object `Freeze` (`JobObjectFreezeInformation`) was checked directly
  against this project's own installed Windows SDK headers (`.../Windows
  Kits/10/Include/10.0.28000.0/um/winnt.h`) and turned out to be **no more
  official than the alternative** -- Microsoft's own public header leaves
  the value nameless (`JobObjectReserved1Information = 18`), with no
  accompanying struct in `jobapi2.h` at all. The least-bad design, if this
  is ever picked up: Job Objects for grouping/descendant tracking (fully
  documented `CreateJobObject`/`AssignProcessToJobObject`/
  `QueryInformationJobObject`) + `NtSuspendProcess`/`NtResumeProcess`
  (undocumented, but stable since Windows XP and what Process Explorer/
  Process Hacker/PowerToys/PowerShell's own `Suspend-Process` all actually
  use) for the actual freeze/thaw action -- recorded in `docs/
  job_control.md`'s "Stopped-child status" section. Using it would still be
  an explicit, narrow reversal of this project's consistently-followed
  "avoid undocumented NT internals" pattern (fork() emulation, `/dev/
  urandom` via `RtlGenRandom`, etc. always stick to documented Win32 APIs
  even in unusual combinations), not a general policy change. **Decisive
  factor**: reviewed `docs/runtime_roadmap.md`'s actual planned components
  (`libcrtjs`/QuickJS+V8, `libcrtgfx`/Skia+Wayland-compositor+Ozone,
  `libcrtmedia`/FFmpeg) and found none of them depend on POSIX job-control
  signals in their own real-world implementations -- a Wayland compositor
  manages client visibility via protocol messages, not process signals;
  Android's own background-app-freeze uses the Linux cgroup freezer, not
  signals (and is Linux-kernel-specific regardless). The roadmap's one
  signal-related mention (V8's "signal/process behavior" prerequisite) is a
  separate matter entirely -- `SIGSEGV`-trap-based WASM bounds checking and
  `SIGPROF`-style profiling, the "vectored exception handling" question
  `docs/signal_delivery.md`'s own "Next Steps" already tracks independently,
  answerable with fully documented Windows APIs
  (`SetUnhandledExceptionFilter`/`AddVectoredExceptionHandler`), no
  undocumented-internal question at all. With no real roadmap dependency,
  stays deferred -- `TODO.md`'s "Interactive job control" section updated
  with this finding so a future session doesn't have to re-derive it.

- **Decided and documented the Windows interactive-job-control policy**
  (TODO.md's "Decide and document the minimal Windows console
  process-group policy" item) -- design and documentation only, no code,
  by explicit user choice: this project's own mksh build defines
  `MKSH_NOPROSPECTOFWORK` unconditionally on every host (not just
  Windows, `shell/CMakeLists.txt`), which compiles out mksh's entire
  internal job-control implementation everywhere, so there is no current
  interactive-job-control gap actively being worked -- this is
  forward-looking policy for whoever eventually re-enables it. Three
  pieces, all recorded in `docs/job_control.md`'s new "Interactive Job
  Control" section: (1) Ctrl-C/Ctrl-Break delivery -- bridge
  `SetConsoleCtrlHandler` into `signal_actions[]`/`raise()`, both
  `CTRL_C_EVENT`/`CTRL_BREAK_EVENT` mapped to `SIGINT` (Bionic's own
  `signal.h` has no `SIGBREAK`), following `docs/signal_delivery.md`'s
  existing `SIGCHLD` pattern exactly -- an atomic pending-flag set from
  the handler thread (Win32 runs it on a new thread per event), actual
  dispatch on the main thread at the same `pselect()`/`select()`/
  `poll()` checkpoints, not synchronous dispatch from the handler thread
  itself, to avoid introducing real concurrency hazards into the
  existing single-threaded dispatch assumption. (2) Foreground
  process-group approximation -- realized that `CREATE_NEW_PROCESS_GROUP`
  (already used by this PAL's `posix_spawn()` for
  `POSIX_SPAWN_SETPGROUP`/`SETSID`) gives most of real POSIX
  foreground/background Ctrl-C semantics for free: Windows automatically
  exempts a `CREATE_NEW_PROCESS_GROUP` child from `CTRL_C_EVENT`, so
  keeping only background jobs in a new process group (foreground jobs
  stay in the shell's own console group) reproduces "only the foreground
  job's group gets `SIGINT`" without needing per-job
  `GenerateConsoleCtrlEvent` targeting for the common case; the one real
  gap is recording the mapping from this project's own CRT-managed
  `pgid` integer (currently opaque, tied to nothing real) to the actual
  Windows process-group id once a job is spawned into one. (3)
  Stopped-child status -- decided to stay honest rather than fake it:
  no real Windows equivalent to `SIGTSTP`/`SIGSTOP`-driven process
  suspension exists without reaching for undocumented NT internals
  (`NtSuspendProcess`) this project has consistently avoided elsewhere,
  so `WIFSTOPPED` support for a real Windows child stays explicitly out
  of scope until real job control is an actual priority, not left
  ambiguously "later". `docs/signal_delivery.md`'s own "Next Steps" was
  updated to point at this decision instead of asking the same open
  question again. Moved out of TODO.md's "in progress" into a new
  "Interactive job control" section under "planned", scoped to the three
  concrete follow-up pieces above plus re-enabling `MKSH_UNEMPLOYED`
  itself, once this is ever prioritized.

- **Fixed a real CI-only failure (`mksh_subshell_status_test_runs`/
  `mksh_shell_smoke_test_runs`) in the two commits just above, caused by
  `add_subdirectory(tests)` running before `CRT_ROOTFS` was ever set.**
  Both new mksh-interpreter tests reference `${CRT_ROOTFS}` directly in
  their own `set_tests_properties(... ENVIRONMENT ...)` calls, to resolve
  `/bin/sh` through the real rootfs the same way a real shell script run
  would. `CRT_ROOTFS` is a `CACHE PATH` variable, normally set inside a
  big `if(Python3_Interpreter_FOUND)` block -- but that block, and the
  `add_subdirectory(tests)` call that needs the variable, were both in
  the top-level `CMakeLists.txt`, in the wrong order: `tests` first, the
  `CRT_ROOTFS` `set()` about 30 lines later. On a tree with any prior
  configure (this dev machine, all session), `CRT_ROOTFS` was already
  sitting in `CMakeCache.txt` from way back, so the wrong ordering never
  showed -- but on a genuinely fresh configure (a truly clean clone, or
  CI, which always starts from one), `${CRT_ROOTFS}` is empty the first
  time `tests/CMakeLists.txt` reads it, baking a literal
  `ENVIRONMENT "CRT_ROOTFS="` into the generated `CTestTestfile.cmake` --
  every case in both tests then failed outright with `posix_spawn()`
  `ENOENT`, unable to resolve `/bin/sh` with no rootfs to resolve it
  against. **Reproduced directly**, not just inferred from CI's
  generically unhelpful "Process completed with exit code 1": cloned the
  repo fresh into a scratch directory, ran the exact
  `cmake --workflow --preset windows-host-ninja-debug` CI uses, and hit
  the identical failure locally (LLVM version, `clang 22.1.8`, confirmed
  identical to what CI's own "Install LLVM (Windows)" step fetches as
  latest -- ruled out before finding the real cause). Fixed by hoisting
  just the `set(CRT_ROOTFS ... CACHE PATH ...)` line (not the whole
  block -- the real `rootfs` custom *target* further down genuinely does
  need `add_subdirectory(shell)`'s targets, which already run first) to
  right after `add_subdirectory(shell)`, before `add_subdirectory(tests)`.
  Verified against a second genuinely fresh clone with the fix applied,
  `cmake --fresh` in this same tree (discards the cache that was masking
  the bug locally), and a full rebuild: `ctest` 90/90 in every case.
  **This is the second real occurrence of the exact same bug class**:
  2026-08-12's `windows_pseudo_reloc_test` `DEPENDS` gap (see that date's
  entry further down) was root-caused the identical way -- a reference
  to `CRT_ROOTFS` before it was set, invisible on this same dev tree,
  real on every fresh CI checkout -- and left behind an explicit
  "Methodological note" saying so. That note didn't stop the same class
  from recurring. Promoted to a standing, checked-every-time discipline
  item in `TODO.md`'s `## note` section instead of a one-off HISTORY.md
  paragraph, since a note only read once clearly isn't enough on its
  own: verify any new `CMakeLists.txt`-level wiring against a genuinely
  fresh clone or `cmake --fresh`, not just an incremental rebuild of
  whatever's already configured, before considering it done.

- **Fixed a real `make -jN` hang: `windows_handle_looks_executable()`
  blocking `ReadFile()` on a pipe, reached via a path the 2026-08-11
  jobserver-pipe fix didn't cover.** Found while attempting to resume the
  libffi `lldb` investigation below: `libffi`'s own parallel build
  (`make -j4`, this project's default since 2026-08-11) hung
  indefinitely on this aarch64 Windows machine -- never observed during
  the original zlib/libpng `-jN` stress testing. A live `lldb` attach to
  the stuck `make.exe` (installing Python 3.11 first, needed to unblock
  `lldb.exe` itself -- see below) showed the main thread blocked in
  `ntdll!NtReadFile`, reached via `main -> jobserver_parse_auth -> fcntl
  -> fstat -> __crt_sys_fstat -> stat_from_handle ->
  windows_handle_looks_executable -> ReadFile`. Same class of bug as
  2026-08-11 (that fix is still present and correct -- confirmed via
  `git log`, not reverted): `windows_handle_looks_executable()`
  unconditionally peeks a handle's first 2 bytes via `ReadFile` looking
  for an MZ/shebang signature, which blocks forever on a pipe with no
  data queued -- but reached this time via a path where
  `GetFileType(handle)` did not report `FILE_TYPE_PIPE` for this
  particular handle, so `__crt_sys_fstat()`'s existing `FILE_TYPE_PIPE`
  special-case (which routes to `stat_virtual_pipe()` instead) never
  triggered, and execution fell through to `stat_from_handle()`'s
  general path anyway. Fixed at the point of actual risk rather than
  only at the one call site the 2026-08-11 fix covered:
  `windows_handle_looks_executable()` itself now returns 0 immediately
  unless `GetFileType(handle) == FILE_TYPE_DISK` -- only a real regular
  file can meaningfully carry an MZ/shebang signature in the first
  place, so this is correct regardless of which caller or code path
  reaches it. **Verified**: rebuilt the CRT, then the `make` port itself
  (statically linked against `libc.a`; needed its own explicit
  `--rebuild` to pick up the fix, since a sysroot rebuild alone doesn't
  relink already-installed ports -- the first re-attempt still hung
  because only the CRT, not `make.exe` itself, had been rebuilt), then
  libffi's real `configure`/`make -j4`/`make install`/test suite
  completed cleanly with no hang. Full `ctest` 88/88.
  - **Also unblocked `lldb` itself for future use on this machine**:
    `lldb.exe` (and `lldb-dap.exe`/`lldb-mcp.exe`, which share the same
    `liblldb.dll`) failed to start at all with `error: unable to find
    'python311.dll'` -- this LLVM build's `lldb` needs a real Python 3.11
    install on `PATH` (checked: no bundled copy anywhere under the LLVM
    install tree). Installing Python 3.11 and adding its directory to
    `PATH` (e.g. `export PATH="<Python311 install dir>:$PATH"` before
    invoking `lldb.exe`) fixes it. `WinDbg`/`cdb` are not installed on
    this machine as an alternative.
- **Re-ran the libffi `repeat-call-static`/`repeat-call-shared` regression
  after the fix above (unrelated code, but the rebuild needed to test it
  happened to also re-run this) -- it did not reproduce.** 20/20 clean
  passes at `-O1` (the committed test's own flag), 10/10 at `-O2` (tried
  separately), full recipe test suite green, full `ctest` 88/88. This
  contradicts the 2026-08-15 entry below, which found this reproduces
  reliably on this same machine. No change was made to libffi's own
  source, build flags, `FFI_DEFAULT_ABI` override, or anything in
  `sysv.S`/`ffi.c` this session -- the only change landed in between is
  the unrelated Windows PAL fix above, which shares no code path with
  libffi's own aarch64 calling-convention assembly. **Not declaring this
  fixed**: the cause of the non-reproduction is unknown (possibly a host
  LLVM/toolchain update between 2026-08-15 and now, possibly the bug was
  always narrower/more timing-sensitive than the earlier
  100%-reproducing characterization suggested -- neither confirmed).
  `porting/recipes/libffi.json`'s `windows` status stays `partial`
  pending either a repeated failure to restore confidence this is still
  live, or enough clean reruns across enough rebuilds to reconsider it.
  See that recipe's own notes for the full writeup.

- **Root-caused and fixed the "Windows mksh subshell status quirk"**
  TODO.md and `docs/sysroot_ports.md` had tracked, unexplained, since
  zlib's own `-@ ($(RANLIB) $@ || true) >/dev/null 2>&1` line first
  exposed it (worked around at the recipe level with `RANLIB=true`,
  never root-caused). Isolated with a series of direct `mksh -c`
  probes: `(false); echo $?` printed `0` instead of `1`, but the exact
  same command without the subshell (`false; echo $?`) printed the
  correct `1` -- and, critically, `(false)` **on its own line**
  (`printf '(false)\necho "status=$?"\n' | mksh`) printed the correct
  `1` too. Only a `;`-joined `(subshell); next_command` on one line
  failed. Traced with temporary `fprintf` instrumentation through
  `exec.c`/`jobs.c`'s `execute()`/`exchild()`/`j_waitj()`/`j_sigchld()`
  chain (removed before the real fix landed): `exchild()` always
  computed and returned the *correct* status (confirmed directly in the
  trace output), so the job-control machinery was never the problem.
  The actual bug: `shell/mksh/src/exec.c`'s `execute()` has an
  early-return path -- `if ((flags&XFORK || t->type == TPAREN) && ...)
  return (exchild(...));` -- that only a TPAREN (subshell) node ever
  takes without `XFORK` already set, because `|| t->type == TPAREN` is
  gated behind `MKSH_CRT_SHELL_CHILD_SPEC`, a **Windows-only** macro
  (`shell/CMakeLists.txt`; added earlier to guarantee a subshell always
  gets real process isolation). That path returns `exchild()`'s result
  directly without ever setting the shared `exstat` global the way
  every other path through `execute()` does at its own `"Break:"` tail
  further down in the same function. Invisible whenever the immediate
  caller uses `execute()`'s return value directly (a standalone
  `(cmd)` on its own line, where `main.c`'s `shell()` loop assigns
  `exstat = execute(...)` itself) -- but `case TLIST:`'s own loop
  (`;`-separated commands) discards the return value of every list item
  except the last one, relying entirely on each item's own dispatch
  having already updated `exstat` as a side effect. `TCOM` (a plain
  command) already does this via the shared `"Break:"` tail; `TPAREN`,
  via this early-return path, never did. Fixed by making that
  early-return path set `exstat` too, mirroring the `"Break:"` tail
  exactly (`shell/mksh/src/exec.c`). On real upstream/non-Windows mksh
  this exact code path is compiled out entirely (a TPAREN without
  `XFORK` falls through to the normal switch-statement dispatch and
  already reaches `"Break:"` correctly) -- genuinely Windows-only, not
  just Windows-first-observed. New permanent regression,
  `tests/mksh_subshell_status_test.c` (spawns the real `/bin/sh` via
  `posix_spawn()`, not a C-level unit test -- the bug is entirely about
  mksh's own interpreter behavior): 8 cases covering the original repro,
  a specific non-0/1 exit code, a subshell with its own redirection
  (matching zlib's real shape), the exact TODO.md-documented
  `(cmd || true) >/dev/null 2>&1` pattern, a successful (`0`-exit)
  subshell, two regression guards for paths that were never broken
  (plain command, command substitution), and a subshell as the *final*
  list item (also never broken). Needs `CRT_ROOTFS` set to resolve
  `/bin/sh` through the rootfs -- set via a new `ENVIRONMENT` test
  property rather than a compile-time path, reusing this project's
  existing `getenv("CRT_ROOTFS")`-based resolution the same way running
  a real shell script from a host shell already would. Full `ctest`
  (89/89, up from 88) and the full `port-test-recipes` aggregate (every
  recipe with automated tests, run after the fix, before the new test
  was even added) both stay clean -- confirming no regression across
  every configure-driven port build this interpreter change touches.

- **Expanded Windows shell smoke test coverage for real mksh interpreter
  behavior** (TODO.md's "Expand Windows shell smoke tests" item): the
  existing `tests/shell_smoke_test.c` only ever exercised this project's
  own `__crt_shell_spawn()`/`posix_spawn()` PAL primitives directly from
  C, never actually routing anything through mksh's own script parser.
  New `tests/mksh_shell_smoke_test.c` (same `/bin/sh -c script` +
  `posix_spawn()` + captured-stdout technique as
  `tests/mksh_subshell_status_test.c`) covers all four requested areas
  with 15 cases: fd 3+ redirection (`exec 3>file`/`exec 3<file`, the
  classic three-step `3>&1 1>&2 2>&3 3>&-` stdout/stderr swap idiom, and
  fd 3 through a subshell's own redirection); `{ }` grouped commands
  (shares the current shell's variables/`cd` state, unlike a `(...)`
  subshell -- checked with a direct contrasting pair -- and its own
  exit status still propagates correctly through a `;`-list, the same
  class of check the subshell-status fix above needed for `TPAREN`);
  `&`/`wait` backgrounding (a background job doesn't block the next
  statement, `wait` with no args blocks until it finishes, `$!` + `wait
  $!` retrieves its real exit status rather than just unblocking, and
  multiple concurrent background jobs can each be waited on
  individually by their own captured pid); and autoconf-shaped
  subshell/redirection idioms (`(exit $ac_status)` as a brace group's
  tail statement -- the exact scenario `shell/mksh/src/exec.c`'s own
  comment documents as historically broken and the reason `TPAREN` gets
  real process isolation on Windows at all; a subshell directly as an
  `if` condition, both taken and not-taken; and a redirected-subshell-
  probe-then-`test $?`-check pattern generalizing the real zlib
  `ranlib`/`RANLIB=true` shape beyond that one specific case). Needed
  `PATH=/system/bin:/bin:/usr/bin` added to the test's own `ENVIRONMENT`
  property alongside `CRT_ROOTFS` (discovered directly: without it,
  `rm`/`cat` inside the test scripts failed with "inaccessible or not
  found", since resolving external commands by bare name needs `PATH`
  set the same way a real port build's own shell environment already
  has it, per `tools/crt-port-build.py`'s own `make_env()`). Full
  `ctest` (90/90) stays clean.

## 2026-08-15

- **libffi's aarch64-Windows `ffi_call()` repeat-call register-corruption
  bug: recreated the lost repro as a permanent test, and confirmed x86_64
  Windows is clean -- narrowing the bug's scope for the first time since
  it was found.** The documented repro (`ffi_prep_cif` -> `ffi_call
  (add_ints)` -> `ffi_prep_cif` again, no closures, `-O1`) had only ever
  lived in an uncommitted scratch file since its 2026-08-07 discovery;
  recreated it as `porting/tests/libffi_repeat_call_test.c`, wired into
  `porting/recipes/libffi.json`'s own `tests` array
  (`repeat-call-static`/`repeat-call-shared`, cflags forcing `-O1`
  explicitly). Run for the first time ever on x86_64 Windows (this
  machine): passes cleanly, both static and shared, tried at both `-O1`
  and `-O2` -- the bug does not reproduce here at all, confirming it is
  aarch64-Windows-specific, not a general Windows libffi issue (every
  prior session only ever exercised a single, non-repeated `ffi_call()`
  on x86_64, so this question had genuinely never been answered before).
  Also ruled out one plausible-looking hypothesis before it could waste
  debugging time: read `ffi.c`/`Makefile.am` and confirmed
  `win64_armasm.S` (MSVC ARMASM syntax) and `sysv.S` (GNU/LLVM syntax)
  implement the identical symbol set for two different assemblers, not
  two different ABI code paths -- this project's clang+LLVM-assembler
  toolchain always uses `sysv.S` regardless of `FFI_SYSV`/`FFI_WIN64`
  selection, and `ffi.c` calls `ffi_call_SYSV` unconditionally either
  way (`cif->abi` only changes behavior for variadics/HFA floats, neither
  exercised by `add_ints(int,int)`), so the recipe's existing
  `FFI_DEFAULT_ABI` override is very unlikely to be the actual cause.
  Full `port-test-recipes` aggregate and Windows `ctest` (88/88) both
  stay clean. **Not yet resolved**: the aarch64 bug itself is still open
  and still needs a real `lldb` single-step session on aarch64 Windows
  hardware to isolate the exact corrupted instruction -- next step,
  pending the user's own aarch64 machine. See `porting/recipes/
  libffi.json`'s own notes for the full trail.

- **Fixed `/proc/self/exe` self-relaunch on macOS**, found via a genuinely
  new, portable regression, `tests/process_stress_test.c` (many
  concurrent `posix_spawn()`-based workers self-relaunching via
  `/proc/self/exe`, deliberately run on every host, not just Windows):
  every worker exited `127` on macOS. Root cause: macOS has no `/proc`
  filesystem at all -- confirmed directly (`ls /proc` itself fails with
  `ENOENT` on real macOS) -- so a literal `"/proc/self/exe"` handed to a
  raw `execve(2)`/`posix_spawn(2)`/`access(2)`/`stat(2)` syscall there
  always fails with `ENOENT`; this project's `posix_spawn()` is built on
  `fork()` + `execve()` (`libc/src/process.c`'s
  `__crt_sys_posix_spawn()`), and a failed `execve()` there falls through
  to `_exit(127)`, the conventional "exec failed" code. Windows already
  had the equivalent fix (`GetModuleFileNameA()` in
  `libc/src/arch/windows/common/syscall.c`); macOS simply never got its
  own version. This went unnoticed for a while because the self-relaunch
  code in `tests/windows_fd_snapshot_test.c`/`tests/rootfs_process_test.c`
  turned out not to actually exercise this path on macOS the way it
  looked at a glance (an easy-to-miss `#if` guard several lines above the
  call site). Fixed by resolving `/proc/self/exe` through
  `_NSGetExecutablePath()` (the real Mach-O "what is my own running
  executable's path" API, declared locally rather than including the real
  `<mach-o/dyld.h>` SDK header, matching this project's usual convention
  for macOS host APIs) in both places this project resolves paths before
  syscalls: `libc/src/process.c`'s `translate_exec_path_for_rootfs()`
  (`execve()`/`posix_spawn()`) and `libc/src/fd.c`'s
  `rootfs_path_for_host()` (`open()`/`stat()`/`access()`/etc., so a
  `readlink("/proc/self/exe", ...)`-style "find my own path" idiom --
  which `shell/toybox/src/lib/portability.c` uses -- resolves correctly
  too, not just exec targets). Verified with a minimal, isolated
  `crt-cc`-built reproduction (confirmed failing before the fix, passing
  after) in addition to the full `ctest` suite (79/79 passing). See
  `docs/android_shell_environment.md`'s new "`/proc/self/exe` On macOS"
  section for the full writeup.

- **Closed out curl's Windows port for good: HTTPS now works, and curl
  8.21.0 is `shared-pass` on all three OSes, finishing the whole
  `bzip2` -> `xz` -> `pcre2` -> `mbedtls` -> `curl` porting queue.**
  Continuing directly from the prior day's entry (HTTP working end to
  end on Windows, HTTPS crashing with `STATUS_ACCESS_VIOLATION`), root-
  caused with a real `lldb` backtrace on the user's own Windows
  machine (`lldb.exe -b -s cmds.txt -- the.exe`, `run` then `bt`):
  `frame #0: 0x0` (a literal NULL function-pointer call) inside
  `mbedtls_ctr_drbg_reseed_internal`, called from
  `mbedtls_ctr_drbg_random` <- `mbedtls_ssl_write_client_hello`.
  Reading mbedTLS's own `library/entropy_poll.c` explained exactly why:
  its portable entropy source only defines a real `getrandom()`
  wrapper for actual `__linux__`/`__FreeBSD__`/`__NetBSD__`/
  `__DragonFly__` -- the generic `__unix__` macro this whole port's
  Windows recipes define (to route mbedTLS/curl onto their portable
  Unix code path instead of native `_WIN32`) matches none of those, so
  it falls straight through to `fopen("/dev/urandom", "rb")`, which
  this project's Windows PAL never implemented at all (no real device,
  and no native Windows path maps onto it the way `/dev/null` already
  does via the real `NUL` device). That entropy-source failure
  silently short-circuited curl's own `vtls/mbedtls.c` `mbedtls_init()`
  before its `mbedtls_ctr_drbg_seed()` call ever ran, leaving the
  module-global CTR_DRBG context's entropy callback null -- invisible
  until now because `mbedtls_crypto_test.c` (this port's own
  regression test) only exercises SHA-256/AES-128-CBC with fixed test
  vectors, never anything needing real random bytes. Fixed for real,
  not routed around: `libc/src/arch/windows/common/syscall.c` gained a
  real `/dev/urandom` (and `/dev/random`, treated identically) virtual
  device -- no real Windows HANDLE backs it at all; a new
  `CRT_FD_KIND_URANDOM` fd kind is serviced directly in
  `__crt_sys_read()` by calling `RtlGenRandom()` (advapi32.dll's
  exported `SystemFunction036`, loaded via `GetProcAddress` the same
  way winsock is, not a static import-library dependency). Verified:
  full local libc rebuild + `ctest` 85/85, and the HTTPS round trip
  then passed -- `curl_http_roundtrip_test: ok http=200 https=200`.
  - Getting there cleanly took two more real fixes, both found chasing
    an intermittent hang specifically in the `http-roundtrip-shared`
    (not static) test after the entropy fix landed. The shared test
    hung indefinitely only when run through the official `cmake
    --build --target port-test-curl` harness, never when run directly
    by hand -- a real `lldb -p <pid>` *attach* to the live hung process
    (not `run`, since this is a genuine block, not a crash) showed the
    main thread stuck in `ntdll!NtReadFile`, reached from
    `KernelBase!ReadFile` <- **`libmbedcrypto.dll!__crt_sys_read`** <-
    `libmbedcrypto.dll!read` <- `libcurl-4.dll!Curl_wakeup_consume` <-
    `multi_runsingle`/`multi_perform`/`curl_easy_perform`. The critical
    detail: `__crt_sys_read`/`read` resolved from **`libmbedcrypto.dll`
    itself**, not the real `c.dll` -- confirming the Windows
    mbedtls-DLL symbol-export-hygiene issue documented earlier (a
    link-time "duplicate symbol" error that had stopped reproducing) is
    very much still real, just no longer a hard link error: mbedtls's
    own hand-rolled Windows `.dll` build statically embeds this
    project's entire libc with no symbol-visibility control and
    re-exports it, and since mbedtls had never actually been rebuilt
    during this whole Windows debugging pass (its own "installed
    stamp" stayed valid the whole time -- curl's own dependency chain
    only requires it be *installed*, not freshly built),
    `libmbedcrypto.dll` was still shipping a *pre-fix* embedded copy of
    `__crt_sys_read()`, silently shadowing the real, already-fixed
    `c.dll` symbol that `libcurl-4.dll` should have resolved to
    instead. Confirmed directly: a plain `cmake --build --target
    port-rebuild-mbedtls` (forcing a fresh mbedtls rebuild, picking up
    every libc fix from this whole session into its own embedded copy)
    followed by a curl rebuild made both `http-roundtrip-static` and
    `http-roundtrip-shared` pass cleanly and repeatably, both in well
    under a second. **Not a curl bug, and the underlying mbedtls
    Windows DLL export-hygiene issue is still NOT fixed** -- this is a
    workaround (rebuild mbedtls alongside any future libc change), not
    a resolution; a real fix still needs either symbol-visibility
    control added to mbedtls's own Windows `.dll` build, or curl
    linking against mbedtls's static libraries specifically for its
    own shared build. Recorded in both `curl.json`'s and
    `mbedtls.json`'s own notes, and as a standing item in `TODO.md`.
  - Separately, `tools/crt-port-build.py`'s own `run_checked_output()`
    (used to execute every port's test binary) never redirected the
    child's `stdin`, silently inheriting whatever `stdin` the harness's
    own deep `cmake -E env -> cmd.exe /C -> python.exe` invocation
    chain happened to have -- a real, if secondary, contributing factor
    to the confusion while diagnosing the shared-build hang above,
    though not its primary cause. Fixed generally, independent of any
    specific bug: `run_checked_output()` now passes
    `stdin=subprocess.DEVNULL` explicitly, since a port's own test
    binary is never given any input on purpose and should never be
    able to block on it regardless of which library code ends up
    touching fd 0.
  - **Final result: curl 8.21.0 is `shared-pass` on all three OSes**,
    closing out this whole porting queue (`bzip2` -> `xz` -> `pcre2` ->
    `mbedtls` -> `curl`; `openssl` stays held back). Both
    `http-roundtrip-static` and `http-roundtrip-shared` pass a real
    HTTP GET and HTTPS GET (real TLS handshake via mbedTLS) against
    `http(s)://example.com/` on Linux, macOS, and Windows, verified
    directly on real hardware for all three, not just Linux/CI. Full
    `port-test-recipes` aggregate (bzip2/libffi/libpng/mbedtls/pcre2/
    xz/zlib, plus curl itself) and Windows `ctest` (85/85) both stay
    clean. See `porting/recipes/curl.json`'s and `porting/recipes/
    mbedtls.json`'s own notes for the full trail.

- **Fixed mbedtls's Windows DLL symbol-export-hygiene gap for real --
  the last open general risk from the whole curl porting queue -- and
  confirmed it end to end with a from-scratch curl rebuild.** The
  problem was worse than the original `read()`/`__crt_sys_read()`
  finding suggested: `llvm-nm --defined-only -g` against this project's
  own `c.lib` showed 917 real libc symbol names, and `llvm-readobj
  --coff-exports` against the installed `libmbedcrypto.dll` confirmed
  virtually all of them were being re-exported alongside mbedtls's real
  API (`comm -12` between the two lists came back almost entirely
  populated). Fixed at the recipe level, not in `tools/crt-cc` itself: a
  `-Wl,--exclude-symbols=NAME` flag per real libc symbol, applied to all
  three of mbedtls's own Windows DLL link recipes
  (`libmbedtls.dll`/`libmbedx509.dll`/`libmbedcrypto.dll`) via a new
  Windows `LDFLAGS` env override in `porting/recipes/mbedtls.json`.
  `--exclude-symbols` only ever controls what a DLL exports, never which
  definition wins during linking, so unlike a link-order-based fix it
  cannot introduce a new `duplicate symbol` build failure -- confirmed
  by trying exactly that first (reordering `c.lib`/`m.lib`/`dl.lib`/
  `c++.lib` ahead of user args in `tools/crt-cc`'s Windows `shared_mode`
  branch) and reverting it after a purpose-built three-binary fixture
  (`tests/windows_dll_symbol_priority_dll.c`/`_middle.c`/`_consumer.c`,
  a DLL linking a "mbedtls" stand-in DLL's import library alongside real
  `c.lib` -- an EXE-to-DLL version was tried first and did NOT reproduce
  the bug, since lld-link resolves that shape correctly regardless of
  link order; only a DLL's own link, importing from another DLL that
  also happens to export a same-named symbol, is affected) showed it
  introduces a hard `duplicate symbol` error whenever a DLL genuinely
  needs a symbol from both `c.lib` and a linked import library at once
  -- curl's own real shape. Two real, general bugs in
  `tools/crt-port-build.py` surfaced getting the recipe-level fix
  working, both fixed generally: (1) `CRT_EXTRA_LDFLAGS`-style variables
  (documented as a manual-shell-only convention via `tools/crt-env.*`)
  silently did nothing when set from a recipe's own `env` block --
  `apply_recipe_env()` runs *after* `make_env()` already computed the
  real `LDFLAGS` from those variables, so a recipe-set one was always
  one step too late; used the real `LDFLAGS` key directly instead
  (a flag accumulator var `apply_recipe_env()` correctly appends onto).
  (2) The full, spelled-out 917-symbol flag list (~32KB of literal
  command-line text) blew straight past this project's own rootfs
  mksh's argv length limit (`Argument list too long`) -- moved into a
  checked-in linker response file
  (`porting/recipes/mbedtls-windows-exclude-symbols.rsp`) referenced via
  a single short `-Wl,@<path>` argument instead, which needed
  `apply_recipe_env()` to gain the same `@ROOT@`/`@PORT_PREFIX@`/etc.
  path substitution `configure_args`/`make_args`/`install_args` already
  had. Verified: `llvm-readobj --coff-exports` against all three rebuilt
  DLLs shows zero libc symbol names left in any export table (1159/287/
  136 real mbedtls exports remain); mbedtls's own `crypto-static`/
  `crypto-shared` tests still pass. **A genuinely fresh, from-scratch
  `port-rebuild-curl` against the fixed mbedtls then surfaced one more
  real, independent bug**: `make install` failed with `Error 5`
  (`ERROR_ACCESS_DENIED`) on `install-pkgconfigDATA` -- the same
  delete-pending/handle-timing race just fixed for `unlink()`/
  `symlink()` (see the entry below), but through `__crt_sys_open()`'s
  own single-shot, no-retry `CreateFileA(..., CREATE_ALWAYS, ...)` this
  time (`install-sh` copying `libcurl.pc` over an existing file).
  Extended the same shared retry loop to `__crt_sys_open()` for
  `O_CREAT` opens (moved the shared `windows_is_delete_race_error()`
  helper earlier in `syscall.c`, right after `fail_last_error()`, so
  `__crt_sys_open()` can reach it too, alongside `__crt_sys_unlink()`/
  `__crt_sys_symlink()` further down). With both fixes in place: a
  clean `port-rebuild-curl` installs without error, and `port-test-curl`
  passes `http-roundtrip-static`/`-shared` with a real HTTP 200 and
  HTTPS 200 against `example.com`, exactly as before the whole
  investigation started -- confirming no regression. Full
  `port-test-recipes` aggregate (all recipes with automated tests) and
  the full Windows `ctest` suite (88/88, up from 85 -- the three new
  regressions below) both stay clean. See `porting/recipes/
  mbedtls.json`'s and `curl.json`'s own notes for the full trail.

- **Windows shell/process stress hardening: added a permanent, real
  concurrent-load regression covering the fd_snapshot/`fstat()` bug
  class that once broke parallel `make -jN`.** `tests/process_stress_test.c`
  (new, runs on every host via the plain `add_crt_test` wiring, not just
  Windows -- the bug class is Windows-PAL-specific, but the jobserver
  *protocol* under test is worth covering everywhere for free) spawns 40
  worker children before reaping any of them (genuinely concurrently
  live, not spawn-one-wait-one), all racing to read a shared, inherited
  6-token pipe the same way GNU make's own jobserver protocol works:
  each worker blocks for a token, verifies a separate `FD_CLOEXEC`-marked
  "secret" pipe pair was correctly NOT inherited (`EBADF` on both ends,
  checked by every worker, not just one), then hands its token back. The
  parent then does a full `waitpid(-1)` drain (all 40 pids accounted for,
  no duplicates, a final call confirming `ECHILD`) and verifies all 6
  tokens came back through the shared pipe with none lost or duplicated
  -- proving 40 concurrently-live processes reading/writing one inherited
  fd under real contention never corrupts the pipe or leaks a
  close-on-exec fd. Passes cleanly (`ctest -R process_stress_test`).

- **Windows symlink/delete timing verification: root-caused and fixed
  the intermittent `ln: ... File exists`/`ERROR_ACCESS_DENIED` failures
  TODO.md had open, plus added a real, deterministic regression (not a
  flaky timing test).** `DeleteFileA()` only *marks* a file for deletion
  while another handle is still open on it -- this project's own
  `open()` already passes `FILE_SHARE_DELETE`, so this isn't even
  Defender-specific, any second handle on the same file reproduces it --
  the directory entry isn't actually removed until every handle closes.
  A `rm -f old && ln -s new old` pair issued back-to-back (libtool's own
  SONAME-symlink install pattern, re-run on every port rebuild) can
  observe the old entry as still present for that short window:
  `unlink()`'s own `DeleteFileA` reports success (accepted, not yet
  completed), and the immediately-following `symlink()`'s
  `CreateSymbolicLinkA` either still sees the old file "there"
  (`ERROR_ALREADY_EXISTS`, surfacing as `ln`'s "File exists") or gets
  refused a new create at that exact path (`ERROR_ACCESS_DENIED`) --
  both exactly the symptoms TODO.md had recorded from real port-install
  runs. Fixed in `libc/src/arch/windows/common/syscall.c`:
  `__crt_sys_unlink()`'s `DeleteFileA` call and `__crt_sys_symlink()`'s
  `CreateSymbolicLinkA` call now share a bounded retry loop (40 attempts,
  10ms apart, so up to ~400ms) that retries specifically on
  `ERROR_ACCESS_DENIED`/`ERROR_SHARING_VIOLATION`/`ERROR_ALREADY_EXISTS`
  -- the same practical idiom Git for Windows/Node.js use for the
  identical Windows quirk, not a fix for a real, persistent sharing
  violation, which still correctly fails once the budget runs out. New
  regression, `tests/windows_symlink_delete_race_test.c` (runs on every
  host; the Linux/macOS build is a plain functional smoke check since
  POSIX `unlink()` has no equivalent delete-pending window to race):
  reproduces the exact race **deterministically**, with no dependency on
  a real antivirus or external timing -- opens a file, starts a second
  thread that sleeps 30ms then closes that handle, and on the main
  thread (no sleep) immediately runs the same `unlink()`+`symlink()`
  sequence libtool does, racing the closer thread with no ordering
  guaranteed. Before the fix this would fail outright whenever the
  closer thread hadn't won yet; with the fix it retries through the
  window and succeeds every time, verified via a full `readlink()`
  round trip after the race, repeated 8 times, plus 8 more iterations
  isolating `unlink()`'s own retry loop the same way. Passes cleanly
  (`ctest -R windows_symlink_delete_race_test`). **Independently
  confirmed for real, not just by the synthetic regression above**: a
  genuinely fresh `port-rebuild-curl` (rebuilding curl from scratch
  against the newly-fixed mbedtls, see this same date's mbedtls/curl
  entry above) hit `Error 5` on `install-pkgconfigDATA` -- the exact
  same failure class TODO.md's own "New data point" note had already
  suspected but never confirmed, this time through `__crt_sys_open()`'s
  own single-shot `CreateFileA(..., CREATE_ALWAYS, ...)` rather than
  `unlink()`/`symlink()`. Extended the same shared retry loop there too
  (see the mbedtls/curl entry above for the full root-cause and
  verification -- a clean `port-rebuild-curl` and passing
  `port-test-curl` with the fix in place).

## 2026-08-14

- **Windows curl: chased the `setmode`/`_spawnv` shim-wiring fix all the
  way through to a real network round trip on the user's own Windows
  hardware, finding and fixing five more distinct, real bugs -- getting
  curl's HTTP round trip working end to end on Windows for the first
  time ever, with one new, distinct, unresolved HTTPS crash left as the
  final remaining gap.**
  - (1) Wiring `porting/shims/win32/libtool_wrapper_compat.h`'s
    `force_include` into `curl.json` (see the entry below) surfaced a
    second, worse shim-header-footprint problem: the shim's own
    `#include <string.h>` (needed for its own `strlen`/`memcpy`/
    `strncmp` calls) leaked a real `strchr()` declaration into curl's
    configure-time `AC_C_UNDECLARED_BUILTIN_OPTIONS` probe (which
    deliberately compiles `(void) strchr;` with no headers of its own,
    expecting it to fail, to detect whether the compiler treats
    undeclared identifiers as real errors). Since `force_include`
    applies the shim to every compile in the whole build via CFLAGS,
    not just the wrapper's, this made that probe wrongly succeed and
    configure hard-errored ("cannot make ... report undeclared
    builtins"). Fixed by forward-declaring exactly the three functions
    needed instead of including the whole header.
  - (2) The same blast radius silently mis-detected several real
    functions as absent, with no hard error at all: GNU Autoconf's
    classic K&R-style `char FUNCNAME ();` link-only probe
    (`ac_fn_c_check_func`, backing dozens of curl's own `AC_CHECK_FUNC`
    calls) hits a hard type-conflict compile error whenever the shim
    had already declared that same symbol with its real prototype --
    silently read as "no, doesn't exist" rather than a real failure.
    `<unistd.h>` (`pipe()`), `<stdlib.h>` (`realpath()`), and
    `<spawn.h>`'s own transitive `<sched.h>` (`sched_yield()`) all did
    this, flipping `checking for pipe/realpath/sched_yield... yes` to
    `no`. `checking for pipe... no` specifically turned out to be why
    `curl_easy_perform()` failed outright with "Out of memory" on its
    very first call: curl's own internal wakeup-pipe mechanism
    (`lib/socketpair.c`) genuinely needs `pipe()`, believed it didn't
    have it, and picked a different, broken path instead. Fixed by
    minimizing the shim's own header footprint to exactly what its own
    code directly calls -- `environ`/`malloc`/`free` forward-declared by
    hand instead of `<unistd.h>`/`<stdlib.h>`; `<spawn.h>`/`<sys/wait.h>`
    kept (unavoidable for `posix_spawn()`'s own opaque types), accepting
    `sched_yield`'s own residual, confirmed-non-fatal misdetection as a
    documented, bounded cost.
  - (3) Once `pipe()` was correctly detected, the recipe's own test
    *programs'* compile (a separate step from curl's library build)
    failed with `curl/curl.h:82:10: fatal error: 'winsock2.h' file not
    found` -- the library build's own `-U_WIN32` family CFLAGS/CPPFLAGS
    override never reached the `tests` entries' own `cflags`. Fixed by
    adding the identical undefines to both `http-roundtrip-static`'s
    and `http-roundtrip-shared`'s own `target_overrides.windows.cflags`.
  - (4) With the test finally compiling and running, `curl_easy_perform()`
    hung indefinitely -- confirmed via direct process inspection
    (0% CPU minutes past the test's own 20-second `CURLOPT_TIMEOUT`,
    which never fired) to be the *exact same* `fcntl(F_SETFL,
    O_NONBLOCK)`-is-a-no-op bug already fixed for Linux/macOS earlier
    this session, just never reachable on Windows until `pipe()` itself
    was correctly detected (bug 2 above). Fixed for real this time:
    `libc/src/arch/windows/common/syscall.c` gained real per-fd
    `O_NONBLOCK` tracking (`fd_nonblock[]`) and
    `__crt_fd_get_status_flags()`/`__crt_fd_set_status_flags()` -- SOCKET
    fds via winsock's own `ioctlsocket(FIONBIO)`, pipe fds (this
    project's fd table classifies pipes as plain `CRT_FD_KIND_FILE`, so
    `GetFileType()==FILE_TYPE_PIPE` is the only way to tell them apart)
    via `SetNamedPipeHandleState(PIPE_NOWAIT)` -- a real Win32 mechanism
    that works on `CreatePipe()`'s own handles despite never going
    through `CreateNamedPipe()` directly, since they're secretly backed
    by named-pipe kernel objects under the hood. `__crt_sys_read()`/
    `__crt_sys_write()` translate the resulting Win32-specific signals
    (`ERROR_NO_DATA` on an empty non-blocking pipe read; a documented
    Win32 quirk where a full non-blocking pipe write "succeeds" with 0
    bytes written instead of erroring) into `EAGAIN`, gated on
    `fd_nonblock[]`. `libc/src/fd.c`'s F_GETFL/F_SETFL cases now call
    these uniformly on every OS, same as F_GETFD/F_SETFD already did.
    Verified: full local Windows libc rebuild + `ctest` 85/85, no
    regressions.
  - (5) With the hang gone, `curl_easy_perform()` returned a clean
    `CURLE_SEND_ERROR` ("Failed sending data to the peer") instead --
    real progress (DNS resolved, TCP connected) but still failing.
    `CURLOPT_VERBOSE` tracing showed a real TCP connection established
    immediately followed by "Send failure: Transport endpoint is not
    connected" (`ENOTCONN`) on the very first `send()`. Root-caused with
    a minimal standalone probe (raw `socket()`/`fcntl(O_NONBLOCK)`/
    `connect()`/`select()`/`getsockopt(SO_ERROR)`/`send()`, no curl
    involved): `connect()` returned `EINPROGRESS` (a related bug fixed
    alongside this one -- `__crt_sys_connect()` was mapping non-blocking
    connect's real Winsock `WSAEWOULDBLOCK` through the same generic
    error mapper every other socket call uses, landing on `EAGAIN`, the
    wrong POSIX errno for `connect()` specifically; fixed by
    special-casing `WSAEWOULDBLOCK` to `EINPROGRESS` only in
    `__crt_sys_connect()`), `select()` correctly reported the socket
    writable, and `getsockopt(SO_ERROR)` correctly reported 0 -- yet
    `send()` still failed `WSAENOTCONN` on the first attempt, and a bare
    retry after a ~200ms delay (no further connect()/select() calls in
    between) succeeded outright. A real, reproducible Winsock race, not
    a caller bug: Winsock's own AFD (Ancillary Function Driver) socket
    layer appears to update its internal "connected" bookkeeping on a
    very slightly different schedule than the TCP/IP driver posts the
    completion `select()`/`getsockopt()` already both correctly observed
    as done. Fixed generally: a new `map_wsa_send_recv_error()`
    reinterprets `WSAENOTCONN` as `EAGAIN`, but only for a socket
    already known to be in non-blocking mode (`fd_nonblock[]`) -- exactly
    the scenario where a correct non-blocking I/O caller already has its
    own ordinary EAGAIN-retry loop, which then naturally retries and
    succeeds once the race resolves. Applied to `__crt_sys_read()`/
    `__crt_sys_write()`'s socket branches and to `__crt_sys_sendto()`/
    `__crt_sys_recvfrom()` (the latter two also switched to calling the
    real, canonical `winsock.send()`/`winsock.recv()` directly instead
    of `sendto()`/`recvfrom()` with a NULL target -- a smaller,
    independently-real hygiene fix that was NOT by itself sufficient to
    resolve this bug, confirmed directly, but kept as the more correct
    call regardless).
  - **Result: curl's HTTP round trip now passes completely on Windows
    for the first time ever** -- a real `curl_easy_perform()` against
    `http://example.com/` returns a genuine `HTTP/1.1 200 OK` with real
    Cloudflare response headers, verified directly on the user's own
    Windows machine. **HTTPS does not work yet**: a new, distinct,
    NOT YET ROOT-CAUSED crash (`STATUS_ACCESS_VIOLATION`, reproduced
    deterministically twice in a row) occurs right after `mbedTLS:
    Connecting to example.com:443` is printed -- somewhere in curl's own
    mbedTLS vtls backend or mbedTLS itself, not yet isolated further.
    Windows status: `partial`. All temporary diagnostic artifacts
    (a standalone non-blocking-connect probe, `CURLOPT_VERBOSE`) were
    removed before finalizing. See `porting/recipes/curl.json`'s own
    notes for the full, detailed trail.

- **Implemented `getauxval()`/`<sys/auxv.h>` for Linux, fixing a real
  `port-rebuild-mbedtls` build failure.** Reported: a fresh
  `port-rebuild-mbedtls` on a real Linux aarch64 host failed with
  `library/aesce.c:109:10: fatal error: 'sys/auxv.h' file not found` --
  upstream mbedtls's ARMv8 crypto-extension runtime detection
  (`#if defined(__linux__)`) calls `getauxval(AT_HWCAP)`/`getauxval(AT_HWCAP2)`,
  and this sysroot had never implemented the header or the function at all.
  Fixed generally, not with an mbedtls-specific patch, per the standing
  porting-loop discipline (checked Android Bionic's own `sys/auxv.h`/
  `getauxval.cpp` and the real Linux kernel UAPI `auxvec.h` for the exact
  semantics and `AT_*` values, extended the CRT/PAL sysroot): `libc/src/
  env.c`'s `__crt_env_set_initial()` already captures the untouched,
  kernel-provided initial `envp` pointer (never a copy) at process
  startup, and the standard Linux/System V process startup stack layout
  (`argc, argv[], NULL, envp[], NULL, auxv[], AT_NULL`) puts the ELF
  auxiliary vector immediately after `envp`'s own `NULL` terminator --
  reachable by walking that same pointer, with no `crt1.S` changes needed
  on either architecture. Added `getauxval()` (new `libc/src/arch/linux/
  common/auxv.c`, Linux-only: macOS/Windows have no equivalent kernel
  mechanism, matching real upstream, which doesn't ship this header on
  either host), plus `include/sys/auxv.h` and `include/linux/auxvec.h`
  (`AT_*` values cross-checked against the real kernel header, not from
  memory). `env.c`'s previously-`static` `initial_envp` was renamed
  `__crt_initial_envp` and exposed via a new private header
  (`libc/include/private/crt_auxv.h`) so the new Linux-only file can read
  it without otherwise touching the portable, all-OS `env.c`. Verified on
  a real Linux aarch64 host: a standalone test confirmed real, correct
  values (`AT_PAGESZ=4096`, a nonzero `AT_HWCAP`/`AT_HWCAP2` bitmask, a
  nonzero `AT_RANDOM` pointer, and `0`/`ENOENT` for an unknown type),
  `aesce.o` now compiles, and the full `port-rebuild-mbedtls`
  `(skip_configure) && make -j4 lib SHARED=1 && make install` completes
  cleanly end to end, with `libmbedcrypto.so.16` and siblings correctly
  `ldd`-resolving to this project's own sysroot. Full `ctest` 77/77
  throughout. Not yet re-verified on macOS/Windows this session (should
  be a no-op there in principle -- `aesce.c`'s `getauxval()` path is
  Linux-only upstream -- but not confirmed by an actual rebuild on
  either host). See `docs/porting_status.md`'s mbedtls row for the same
  writeup in context.
  - **Follow-up in the same session: found and fixed a real, separate
    `<inttypes.h>` bug while investigating why only aarch64 hit the
    `aesce.c` failure.** `aesce.c` is ARMv8-crypto-extension-specific
    code (`#if defined(__ARM_ARCH) && __ARM_ARCH >= 8`, upstream);
    x86_64's equivalent AES acceleration lives in a separate file
    (`aesni.c`) that detects AES-NI via compiler intrinsics/`CPUID`,
    never calls `getauxval()`, and never includes `<sys/auxv.h>` at all
    -- so x86_64 was never exposed to the gap, not because anything
    there worked around it. While confirming the mbedtls rebuild was
    otherwise clean, 4 `-Wformat` warnings turned up in
    `ssl_tls13_server.c` (`MBEDTLS_PRINTF_MS_TIME` mismatched against
    `mbedtls_ms_time_t`, an `int64_t`). Root cause was general, not
    mbedtls's: clang's own `__INT64_TYPE__`/`__INTMAX_TYPE__` (confirmed
    via `-dM -E` against the real target triples, not assumed) is plain
    `long` on Linux/macOS (LP64) but `long long` on this project's
    Windows target (`*-w64-mingw32`, LLP64) -- yet `include/inttypes.h`'s
    `PRId64`/`PRIi64`/`PRIu64`/`PRIx64`/`PRIX64`/`*MAX` macros were
    hardcoded to the `ll`-modifier forms (correct for Windows only),
    while the `*PTR`-width macros were hardcoded the other way, to the
    single-`l` forms -- also only correct on Linux/macOS: `intptr_t` is
    `long long` on this project's Windows target too, so `PRIdPTR` etc
    were *also* wrong there, just not yet caught by any real Windows
    build exercising them. Fixed all of them (`PRI{d,i,u,x,X}{64,MAX,
    PTR}` and the `SCN{d,u,x}{64,MAX,PTR}` scanf equivalents that already
    existed) with a single `CRT_TARGET_OS_WINDOWS`-conditioned
    length-modifier prefix (`CRT_PRI64_PREFIX`/`CRT_PRIPTR_PREFIX`,
    `"l"` on Linux/macOS, `"ll"` on Windows) rather than patching each
    macro ad hoc. Verified: a standalone `int64_t`/`uint64_t`/
    `intmax_t`/`uintptr_t` round trip through every fixed macro compiles
    clean under `-Wall -Wextra -Werror` and prints correct values on
    this Linux aarch64 host, the 4 mbedtls warnings are gone from a
    clean `port-rebuild-mbedtls` rerun, and full `ctest` stays 77/77 (no
    existing code in this project's own tree used any of these macros,
    so zero regression risk there). Not yet re-verified on macOS
    (expected to behave like Linux, same LP64 ABI) or Windows (the
    `*PTR` direction of this fix is Windows-only in effect and has no
    real Windows build exercising it yet).
  - **Second follow-up, same session: `port-rebuild-curl` failed with
    `undefined reference to '__getauxval'`.** Not a curl, mbedtls, or
    zlib symbol -- traced with `nm` straight to `lse-init.o` inside this
    project's own `libclang_rt.builtins.a`, LLVM compiler-rt's AArch64
    outline-atomics support (`__aarch64_have_lse_atomics`'s own
    constructor), pulled in automatically once curl's `<stdatomic.h>`
    usage made clang emit an outlined atomic op. Root cause: real glibc
    implements the public `getauxval()` as a `weak_alias` to a reserved-
    namespace `__getauxval()`, and compiler-rt's outline-atomics helper
    -- written against that real glibc ABI, not Android Bionic's (which
    only ever exports plain `getauxval()`) -- calls `__getauxval()`
    directly by that exact name. This project's new `getauxval()`
    (above) had no such alias. Fixed by adding `unsigned long
    __getauxval(unsigned long)` to `libc/src/arch/linux/common/auxv.c`
    (delegates to `getauxval()`), deliberately *not* declared in
    `include/sys/auxv.h` -- matching real glibc, this is a linkable ABI
    symbol third-party runtime-support code may assume exists, not part
    of the public API surface. Verified: `nm` on the rebuilt `libc.a`
    shows both `getauxval`/`__getauxval` as defined; a full, clean
    `port-rebuild-curl` (`configure && make -j4 && make install`)
    completes with `curl`/`libcurl.so` linking successfully this time;
    `port-test-curl` passes a REAL network round trip for both build
    shapes (`curl_http_roundtrip_test: ok http=200 https=200`, static
    and shared, against real `http://example.com/`/`https://
    example.com/`) -- exercising mbedTLS's real TLS handshake (and, in
    turn, `aesce.c`'s AES hardware-acceleration path) for real, not just
    a compile check. Full `ctest` 77/77 throughout.

- **Closed the macOS curl/shared-port audit loop and cleaned up the
  CMake install/RPATH noise.** The first curl tranche is now verified as
  `shared-pass` on macOS as well as Linux: `port-test-curl` passes both
  `http-roundtrip-static` and `http-roundtrip-shared` against real
  `http://example.com/` and `https://example.com/` via the mbedTLS
  backend (`curl_http_roundtrip_test: ok http=200 https=200`). macOS
  keeps `--disable-ipv6` and `--disable-threaded-resolver` for this
  tranche, deliberately staying on this CRT's currently-verified IPv4
  synchronous resolver path rather than leaking Darwin
  SystemConfiguration headers or relying on curl's async resolver
  worker-pool behavior.
  - **Audited the installed macOS port dylibs for accidental host-libc
    binding.** Rebuilt the stale shared outputs for bzip2, libffi, xz,
    pcre2, libpng, sqlite-amalgamation, zlib, mbedTLS, and curl, then
    checked the install tree with `otool -L` and `nm -m -u`. The rebuilt
    dylibs now record this project's CRT dylibs (`@rpath/libc.dylib`
    and siblings where used), and the audit found no suspicious
    libc/POSIX symbols (`malloc`/`free`, stdio, string/memory, sockets,
    pthreads, time, mmap, errno, etc.) binding directly from
    `/usr/lib/libSystem.B.dylib`. `libSystem` remains visible as the
    intended Darwin PAL/backend boundary for system calls, dyld,
    pthread/process services, not as the upstream port's C library.
  - **Added CRT-owned stack protector symbols.** The audit exposed
    `___stack_chk_fail`/`___stack_chk_guard` as the last host-resolved
    compiler runtime style symbols in the macOS port dylibs. `libc` now
    provides `__stack_chk_guard` and `__stack_chk_fail()` directly
    (`libc/src/stack_protector.c`), so stack-protector checks no longer
    need to bind those symbols from libSystem.
  - **Cleaned the non-fatal CMake install log noise.** CMake-generated
    install scripts for `libdl.dylib` and `libc++.dylib` were invoking
    `install_name_tool -delete_rpath <build-lib-dir>` even though the
    rpath was not present, producing noisy but harmless diagnostics
    during `sysroot` installation. `crt_configure_shared_runtime()` now
    uses the install rpath at build time on macOS with an explicit empty
    `INSTALL_RPATH`, so CMake no longer generates the unnecessary
    delete-rpath step. Verified by regenerating the macOS preset,
    confirming no generated `delete_rpath` calls remain, and running
    `cmake --build --preset macos-host-ninja-debug --target sysroot`
    cleanly.
  - **Verification in the same pass**: macOS `ctest` passed
    (`77/77`), `port-test-curl` passed with network access, and the
    non-network recipe tests for zlib, libpng, bzip2, xz, pcre2, and
    mbedTLS passed static/shared checks. `TODO.md`, `STATUS.md`,
    `README.md`, `docs/sysroot_ports.md`, `docs/porting_status.md`, and
    `porting/recipes/curl.json` were synchronized so completed material
    moved out of TODO and the current status reflects curl's Linux/macOS
    `shared-pass` plus the remaining Windows mbedTLS DLL export blocker.

- **Ported pcre2 10.47 to Linux, macOS, and Windows -- all three
  `shared-pass`**, the next entry in the porting matrix expansion queue
  after xz (`bzip2` -> `xz` -> `pcre2` -> `mbedtls` -> `curl`, see
  `TODO.md`). No dependencies, no new CRT/PAL gap -- every build-system
  quirk hit was an already-established pattern from earlier ports in
  this queue, not a new problem: (1) `--build=@CRT_MINGW_TRIPLE@` for the
  same Windows `config.guess` issue libpng/xz/libffi already hit
  (`config.guess` doesn't recognize plain Windows `uname` output); (2)
  `target_overrides.windows.env.CFLAGS` undefines `_WIN32`/`_WIN32_WCE`/
  `__WIN32__`/`WIN32` (same technique as zlib/xz/bzip2), since
  `pcre2grep.c` has a real `#include <windows.h>` + `FindFirstFile()`-
  based directory-walking implementation this sysroot has no equivalent
  for -- pcre2's own `Makefile.am` has no configure-time toggle to skip
  building `pcre2grep`/`pcre2test`, so the recipe still builds them, kept
  on their portable `opendir()`/`readdir()` path instead.
  - **`porting/tests/pcre2_match_test.c`**: a real `pcre2_compile()`/
    `pcre2_match()` round trip with three named capture groups
    (user/host/tld), each individually verified against the matched
    substrings, not just a version-string smoke check or a nonzero match
    count.
  - **A real bug, found and fixed**: the test initially failed to link
    on Windows with `undefined symbol: __declspec(dllimport)
    pcre2_compile_8 ... cannot be used because it is not an import
    library`. The library itself was built with `_WIN32` undefined
    (correct -- no `dllimport`/`dllexport` decoration, appropriate for
    static linking), but the test file's own compile step is a *separate*
    `crt-cc` invocation that does not inherit the library's own
    `target_overrides.windows.env.CFLAGS`, so it saw clang's
    default-predefined `_WIN32` and expected DLL-import-style symbols
    against a plain static archive. Fixed with the standard,
    upstream-documented convention for exactly this: `-DPCRE2_STATIC` in
    the `match-static` test's own `cflags` (deliberately *not* applied to
    `match-shared`, added afterward, which needs the default
    `dllimport`-decorated declarations to match the DLL import library it
    links against).
  - **Started static-only, then added shared, verified on all three
    hosts for real**: matching bzip2/xz's own cautious start
    (`--disable-shared --enable-static`), pcre2 first landed
    `static-pass` on Linux and Windows. Once that round-tripped for real,
    `--disable-shared`/`--enable-static` were dropped (`configure_args:
    []`, matching zlib.json's own pattern of just using the library's
    real default) and a `match-shared` test entry added. libtool
    produces `libpcre2-8-0.dll` + `libpcre2-8.dll.a` (import library) on
    Windows, `libpcre2-8.so` on Linux, `libpcre2-8.dylib` on macOS. Both
    `match-static` and `match-shared` print `pcre2_match_test: ok
    matches=4 version=10.47 2025-10-21` on Windows
    (`cmake --build --preset windows-host-ninja-debug --target
    port-test-pcre2`), Linux (WSL Ubuntu 20.04 + clang-18,
    `tools/crt-port-build.py --port pcre2 --test`), and macOS (real
    aarch64 hardware, user-run, including a clean CI pass on all 5
    GitHub Actions legs before the macOS shared verification).
  - **One macOS-specific pitfall, not a code bug**: the user's first
    `match-shared` attempt on macOS failed with `clang: error: no such
    file or directory: '.../lib/libpcre2-8.dylib'`. Root cause:
    `port-test-pcre2` reused an already-`installed`-stamped port-tests
    directory that still held the *earlier* static-only build (from
    before this session dropped `--disable-shared`), so
    `libpcre2-8.dylib` genuinely didn't exist there yet.
    `cmake --build --preset <preset> --target port-rebuild-pcre2`
    (clears the install stamp, reruns `configure`/`make`/`make install`)
    before re-running `port-test-pcre2` picked up the shared build
    correctly. Worth remembering generally: any recipe whose
    `build.configure_args`/`env` changes after it was already built once
    needs an explicit `port-rebuild-<name>` for the change to actually
    take effect against an already-installed port -- the stamp-based
    skip is deliberate (avoids re-running a slow `./configure && make` on
    every invocation), not a bug.

- **CI now skips the full 5-leg matrix for doc/porting-notes-only
  pushes** (`.github/workflows/ci.yml`'s `push`/`pull_request` triggers
  gained `paths-ignore: ["**/*.md", "docs/**", "porting/**"]`), so a
  commit that only touches `HISTORY.md`/`TODO.md`/`docs/`/
  `porting/recipes/*.json` no longer burns a full CI run. Landed
  together with the first real mbedtls code/tooling change below,
  matching the project's own batching policy (real code changes and any
  accompanying doc updates travel in the same push, rather than a
  standalone doc-only push triggering CI for nothing).

- **Ported mbedtls 3.6.7 (crypto library only) to Linux and Windows --
  both `shared-pass`**, the next entry in the porting matrix expansion
  queue after pcre2 (`bzip2` -> `xz` -> `pcre2` -> `mbedtls` -> `curl`,
  see `TODO.md`). Picked the 3.6.7 LTS release over the newer 4.2.0,
  which dropped mbedtls's plain top-level Makefile for a CMake-only
  build this project's tooling doesn't support. Needed small,
  generalizable `tools/crt-port-build.py` extensions, not one-off
  recipe hacks: a new `build.skip_configure` flag (mbedtls has no
  `./configure` step at all; skips just that one step, reusing every
  other part of the existing configure-recipe machinery -- patches,
  env/CFLAGS overrides, `make_args`/`install_args`, Windows shell
  wrapping, parallel `-jN` jobs), a new base `build.install_args`
  field (extra arguments specific to `make install` only, distinct from
  the existing `target_overrides.<os>.make_args`) for mbedtls's
  `DESTDIR=`-based install convention, unlike every other recipe here's
  autotools `--prefix=`, and (added in a follow-up pass, once static
  landed and the user asked why shared verification kept being
  deferred rather than done alongside it -- see TODO.md's new standing
  porting-loop discipline item) a per-OS `target_overrides.<os>.
  build_make_args` field for a `make` variable that must reach the
  build step only, never install.
  - **Avoiding the `programs`/`mbedtls_test` dependency chain**:
    mbedtls's own `install: no_test` -> `no_test: programs` ->
    `programs: lib mbedtls_test` chain means a plain `make install`
    would also build every example program under `programs/` and the
    whole `mbedtls_test` framework (needing the `framework/`
    git-submodule content for real, well beyond what a library-only
    port needs). `make_args: ["lib"]` targets the top-level Makefile's
    `lib` goal directly (`$(MAKE) -C library`, which itself defaults to
    a static-only build unless the `SHARED` make variable is set), and
    `install_args`' `-o no_test -o programs -o mbedtls_test` (GNU
    Make's `--assume-old` option, one per phony prerequisite) tells
    `make install` to treat those three targets as already up to date,
    so it runs straight to the `install:` recipe's own handful of
    `mkdir -p`/`cp -rp`/`cp -RP` commands without touching their
    prerequisites at all. Verified directly with native GNU Make 4.2.1
    (WSL Ubuntu 20.04) before wiring into the recipe: `make -n install
    -o no_test -o programs -o mbedtls_test DESTDIR=...` showed only the
    install recipe's own copy commands, and a real run produced exactly
    `include/{mbedtls,psa}` + `lib/lib{mbedtls,mbedx509,mbedcrypto}.a`
    plus a handful of harmless `bin/*.sh` helper scripts upstream's own
    tarball ships with the executable bit already set.
  - **Two library sources have a hard `#error`, not just an empty
    translation unit, without a Unix-like macro**: `entropy_poll.c` and
    `timing.c` both `#error` out unless either `_WIN32` or a recognized
    Unix-like macro (`unix`/`__unix`/`__unix__`/`__APPLE__`+`__MACH__`/
    `__HAIKU__`/`__midipix__`) is defined. Since this PAL has no real
    `<windows.h>`/`<bcrypt.h>`/`QueryPerformanceCounter` to satisfy the
    `_WIN32` branch (same reasoning as every prior Windows recipe's
    `_WIN32` undef), `target_overrides.windows.env.CFLAGS` adds
    `-D__unix__` alongside the usual `_WIN32`-family undefines, routing
    both files down their portable `getrandom()`/
    `fopen("/dev/urandom")`/`gettimeofday()` path instead.
  - **A real Windows build attempt surfaced a genuine PAL gap, not
    assumed in advance**: with `MBEDTLS_NET_C` (networking, on by
    default) left enabled, expecting `library/net_sockets.c`'s portable
    POSIX-sockets path to work the same way this project's own
    `libc/src/socket.c`/`tests/socket_network_test.c` already do, the
    real build instead failed with `select()`/`fd_set`/`FD_ZERO`/
    `FD_SET`/`FD_ISSET`/`suseconds_t`/`SO_TYPE` all undeclared --
    this PAL's `<sys/socket.h>` doesn't yet expose the fuller
    BSD-sockets surface mbedtls's own networking helper needs. Rather
    than extend that surface now (squarely `curl`'s territory, the next
    port in this queue, which genuinely needs sockets), added a
    `build.patches` entry disabling `MBEDTLS_NET_C` in
    `mbedtls_config.h` -- mbedtls's SSL/TLS layer only touches
    `net_sockets.c` through the swappable `mbedtls_net_context`
    callback shape, so this doesn't block AES/SHA/RSA/etc. use at all.
  - **`porting/tests/mbedtls_crypto_test.c`**: a real cryptographic
    round trip, not a version-string/link-only smoke check --
    SHA-256("abc") compared against the actual NIST/FIPS 180-4
    known-answer digest, plus a full AES-128-CBC encrypt/decrypt round
    trip verifying the decrypted output exactly reproduces the original
    plaintext (and that the ciphertext isn't simply equal to the
    plaintext, so a no-op "encrypt" couldn't accidentally pass).
    Verified natively first (WSL Ubuntu 20.04, plain gcc, against a
    native `make lib` + `make install DESTDIR=...` build) to separate
    "does the API usage make sense" from "does the CRT toolchain
    integration work," then for real through this project's own
    toolchain end to end on both Linux (WSL Ubuntu 20.04 + clang-18, a
    clean clone) and Windows (real x86_64 host): `cmake --build --preset
    <preset> --target port-test-mbedtls` prints `mbedtls_crypto_test: ok
    sha256=ba7816bf8f01cfea... aes128cbc=roundtrip-ok` on both. Full
    Windows `ctest` reran afterward (83/83) to confirm the
    `tools/crt-port-build.py` changes didn't regress anything else.
    Landed `static-pass` first on this initial pass.
  - **Shared-build expansion, same day**: `SHARED=1` added to
    `make_args` makes library/Makefile's own `all: shared static`
    build both artifacts in one invocation (library/Makefile defaults
    to `static` only otherwise). Getting a real, correctly-named
    Windows shared build (`.dll` + `.dll.a` import library) took a
    chain of further real, individually-confirmed fixes, each found by
    an actual build attempt, not guessed in advance: (1)
    `WINDOWS_BUILD`-gated `-lbcrypt` in `LOCAL_LDFLAGS` (mbedtls's
    Windows entropy source calls `BCryptGenRandom`; unneeded since
    `-D__unix__` already routes `entropy_poll.c` off that path) --
    patched out. (2) The three `.dll` link recipes' own
    `-lws2_32`/`-lwinmm`/`-lgdi32` (unneeded once `MBEDTLS_NET_C` is
    disabled) and `-Wl,-soname` (an ELF-only GNU ld concept `lld-link`'s
    PE frontend doesn't accept) and `-static-libgcc` -- patched out,
    keeping `--out-implib` (the GNU-ld-compatible import-library flag
    this project's own `crt-cc`/`lld-link` Windows shared-build path
    already understands). (3) `bignum.c`'s `mbedtls_mpi_div_mpi()`
    referencing undefined symbol `__udivti3` (a compiler-rt/libgcc
    128-bit-division intrinsic): this PAL's Windows sysroot has no
    compiler-rt/builtins archive at all -- unlike Linux/macOS, where
    `tools/crt-cc` always links `libclang_rt.builtins.a` into every
    link, shared or not, Windows' own `crt-cc` libs list has no
    equivalent. A real, general gap worth a future dedicated fix
    (noted in the recipe, not yet in `TODO.md` since routing around it
    for mbedtls specifically was straightforward) -- routed around via
    `-DMBEDTLS_HAVE_INT32` (forces bignum's portable 32-bit-limb path,
    no `__int128` division), which conflicts with `MBEDTLS_HAVE_ASM`
    per `bignum.h`'s own `check_config.h`, so `MBEDTLS_HAVE_ASM` also
    needed disabling (Windows-only, `CRT_TARGET_OS_WINDOWS`-guarded
    patch -- a plain `-U` on CFLAGS was tried first and confirmed NOT
    to work, since `mbedtls_config.h`'s own unguarded `#define`
    redefines it regardless of the command line). (4) Disabling
    `HAVE_ASM` then left `MBEDTLS_AESNI_C` unsatisfiable (`aesni.h`
    needs either `HAVE_ASM` or `-maes`/`-mpclmul` compiler intrinsics,
    neither available) -- disabled too (Windows-only patch); no
    correctness loss, `aes.c`'s portable table-based C implementation
    is used instead, confirmed correct by this port's own AES
    round-trip test. (5) A deeper, genuinely separate bug: mbedtls's
    top-level Makefile wraps its *entire* `install:`/`uninstall:`
    block in `ifndef WINDOWS` -- passing `WINDOWS=1` (needed during the
    build step to select the correctly-named `.dll` link recipes) to
    `make install` too doesn't just change what `install:` does, it
    makes `install:` **not exist at all**. Confirmed directly, not
    guessed: GNU Make's own `-p` database dump showed `install:`
    surviving only as an empty `.PHONY` entry once `WINDOWS=1` reached
    it, so `make install WINDOWS=1` silently no-ops -- exit 0, nothing
    copied, no error -- which is exactly why this went unnoticed for a
    full rebuild+test cycle (the "static" test kept passing against
    stale, pre-shared-work install artifacts) before being caught by
    checking install output file timestamps. Fixed generally, not with
    an mbedtls-specific hack: `tools/crt-port-build.py` gained the new
    `build_make_args` field described above, letting `WINDOWS=1` move
    out of the shared build+install override and reach the build step
    only -- `xz`'s own existing Windows recipe, which genuinely needs
    its `target_overrides.windows.make_args` override to reach both
    steps, is untouched. Verified for real end to end, both static and
    shared, on Linux (WSL Ubuntu 20.04 + clang-18, clean clone) and
    Windows (real x86_64 host): `crypto-static` and `crypto-shared`
    both print `mbedtls_crypto_test: ok` on both hosts; Windows's
    shared test binary confirmed via `llvm-objdump -p` to genuinely
    dynamically depend on `libmbedcrypto.dll`. Reran the full
    `port-test-recipes` aggregate (all recipes with automated tests)
    and the full Windows `ctest` suite (83/83) afterward -- no
    regressions from the `build_make_args` tooling change. `APPLE_BUILD=1`
    wired into `target_overrides.macos.make_args` (safe there, since
    `APPLE_BUILD` isn't gated by any `ifndef` around `install:`)
    correctly selects the `.dylib` link recipes -- confirmed by the
    user on real macOS hardware later the same day. All three hosts:
    `shared-pass`.
  - **New standing porting-loop discipline item added to `TODO.md`**:
    verify both the static AND shared build during the same porting
    pass, on every host, before calling a port done -- not
    static-first-then-shared-as-a-follow-up. Several ports in this
    queue (bzip2, xz, pcre2, mbedtls) landed `static-pass` first and
    only got `shared-pass` later or after being asked why shared
    hadn't been checked; this mbedtls pass is the first one done
    under the new discipline (shared verified in the same continuous
    session as static, once the gap was pointed out).
  - **`docs/status.md` moved to `STATUS.md` at the repository root**
    (same request that prompted the standing-discipline item above),
    matching `TODO.md`/`HISTORY.md`'s own top-level placement. No other
    file referenced the old path (checked directly), so this was a
    plain `git mv` with no link fixups needed.

- **Ported curl 8.21.0 to Linux -- `shared-pass`**, the last entry in
  the porting matrix expansion queue (`bzip2` -> `xz` -> `pcre2` ->
  `mbedtls` -> `curl`, see `TODO.md`; `openssl` stays held back until
  something actually needs it). Depends on zlib and mbedtls, both
  already `shared-pass` everywhere. Scoped to HTTP/HTTPS for this
  first pass, matching this queue's own established caution.
  - **First port in this queue to reach a real internet hostname over
    the network**, not a self-contained local round trip -- and it
    surfaced two real, general, previously-invisible libc bugs, not
    curl-specific ones, both root-caused by direct process/source
    inspection (a real, indefinite hang, not a clean failure), not
    guessed.
  - **`getaddrinfo()` had no real DNS resolution at all.** It only
    ever handled literal numeric IP addresses via `inet_pton()`,
    returning `EAI_NONAME` immediately for any real hostname -- a gap
    invisible until now because no prior port in this queue ever
    needed to resolve a real hostname. Implemented a real, deliberately
    minimal synchronous DNS client directly in `libc/src/socket.c`:
    parses `/etc/resolv.conf` for a nameserver (falling back to
    `8.8.8.8` if missing/unreadable -- a documented simplification on
    Windows specifically, which has no `/etc/resolv.conf` at all; a
    fuller fix would query the OS's own configured DNS servers via
    IPHLPAPI's `GetNetworkParams()`), builds and sends a single UDP
    query for an A record, and parses the response, with a couple of
    retries and a short timeout so an unresponsive nameserver can't
    hang forever. Deliberately scoped: no AAAA/IPv6, no TCP fallback
    for truncated responses, no search-domain suffixes, no caching --
    sufficient for curl's own basic HTTP/HTTPS needs, growable later if
    a future port needs more. Verified directly and in isolation before
    ever touching curl: a standalone `getaddrinfo("example.com", ...)`
    call returned the real, correct IP addresses.
    Also found and fixed along the way, the same class of gap: real-world
    POSIX systems (and Android Bionic itself) commonly expose `size_t`/
    `time_t` from `<sys/types.h>` too, not just `<stddef.h>`/`<time.h>`
    -- curl's own `CURL_SIZEOF` autoconf macro (and its own public
    `curl/multi.h` header, for `fd_set`) assumes exactly this, and this
    project's headers didn't. Fixed `<sys/types.h>` (now includes
    `<stddef.h>` for `size_t`, and defines `time_t`/`clock_t` behind a
    shared guard macro with `<time.h>` to avoid duplicate-typedef
    errors when both headers are included together) and `<sys/socket.h>`
    (now includes `<sys/select.h>` transitively for `fd_set`/`FD_SET`/
    `select()`). Also added `AF_UNIX`/`AF_LOCAL` (curl auto-enables Unix
    domain socket support once it detects `<sys/un.h>`, which this
    project already had -- only the `AF_UNIX` constant itself was
    missing), the `IN6_IS_ADDR_*` classification macros (pure bit tests
    on `struct in6_addr`, no syscall involved -- curl's own
    `lib/cf-socket.c` needs `IN6_IS_ADDR_LINKLOCAL` unconditionally, not
    behind any feature gate), and a real `getsockopt()` (curl's own
    `lib/cf-socket.c` needs `SO_ERROR` to check a non-blocking connect's
    completion status -- mirrored the existing `setsockopt()` syscall
    plumbing across all three OSes: new raw syscall stubs for Linux
    x86_64/aarch64 (`#55`/`#209`) and macOS x86_64/aarch64 (BSD syscall
    `#118`, including the same `SOL_SOCKET`/`SO_*` Darwin-numbering
    translation table `setsockopt()` already needed), and a new
    `winsock.getsockopt` entry in the dynamically-loaded `ws2_32.dll`
    function table on Windows).
  - **`fcntl(fd, F_SETFL, O_NONBLOCK)` was a pure software no-op on
    Linux/macOS -- and this, not the DNS gap above, is what actually
    hung `curl_easy_perform()` indefinitely.** `fcntl()`'s own
    `F_SETFL` case read the requested flags argument, discarded it,
    and always reported success, never marking any fd non-blocking at
    all. curl's own internal wakeup pipe (`lib/socketpair.c`'s portable
    `pipe()`-based fallback, used by *every* `curl_multi_perform()`
    call, not just DNS-related ones) sets `O_NONBLOCK` via `fcntl()`
    expecting a real non-blocking fd back, then does a best-effort
    "drain if pending, don't block otherwise" read on it
    (`Curl_wakeup_consume()`) on every call -- with `O_NONBLOCK`
    silently never taking effect, that read blocked forever on the
    very first call, before curl ever reached DNS resolution or opened
    a real network socket. Root-caused with real process inspection,
    not guessed: `/proc/PID/task/` showed only ever one thread (ruling
    out a hang inside a spawned resolver thread), `/proc/PID/fd/`
    showed only the wakeup pipe's own two fds the entire time (no
    socket ever opened), and `ps` showed 0% CPU (a real blocked
    `read()`, not a busy loop) -- then pinned to the exact call site by
    adding temporary `fprintf` instrumentation directly into curl's own
    `lib/easy.c` (bisecting `easy_perform()`'s three main steps) and
    `lib/multi.c` (bisecting `multi_perform()`'s per-iteration calls),
    reverted once the real cause (`Curl_wakeup_consume()`, called from
    `multi_runsingle()`'s admin-handle branch) was confirmed by reading
    its own source. Fixed generally in `libc/src/fd.c`, not with a
    curl-specific workaround: `F_GETFL`/`F_SETFL` now forward to the
    real `fcntl(2)` syscall on Linux/macOS via two new functions,
    `__crt_fd_get_status_flags()`/`__crt_fd_set_status_flags()`,
    mirroring exactly how the pre-existing `__crt_fd_get_cloexec()`/
    `__crt_fd_set_cloexec()` already forward `F_GETFD`/`F_SETFD` -- a
    real `fcntl(2)` syscall already implements `O_NONBLOCK` correctly
    at the kernel level for every fd type (files, pipes, sockets), so
    this needed no changes to `read()`/`write()` themselves, which
    already go straight to the raw syscall. Windows's own `fcntl()`
    backend has no unified syscall to forward to (Windows needs
    per-fd-type handling: winsock's `ioctlsocket(FIONBIO)` for sockets,
    overlapped I/O for anonymous pipes, neither attempted this pass) and
    keeps its prior no-op behavior for `F_GETFL`/`F_SETFL` specifically,
    explicitly to avoid regressing existing behavior -- documented as a
    `TODO(windows)` comment directly in the code, not yet hit by a real
    Windows curl build this session.
  - **A real `tools/crt-port-build.py` bug found and fixed along the
    way**: `@PORT_PREFIX@` substitution (needed for curl's own
    `--with-mbedtls=@PORT_PREFIX@`/`--with-zlib=@PORT_PREFIX@`, so
    configure can find its two already-installed sibling ports) was
    previously only ever applied to `make_args`/`install_args`, never
    to `configure_args` -- no prior "configure"-system recipe in this
    queue had needed to reference another port's install path from a
    configure flag before. Confirmed by a real build attempt, not
    guessed: the literal, unsubstituted string `"@PORT_PREFIX@"`
    appeared in configure's own `--with-mbedtls` argument and resulting
    `CPPFLAGS` (the build didn't immediately fail because `crt-cc`'s own
    default include/library search path happened to already cover the
    shared port-install prefix, masking the bug until a later,
    unrelated `size_t` compile probe exposed it). Fixed generally:
    `port_prefix_text` is now computed once at the top of
    `build_configure_port()` and applied to `configure_args` the same
    way it already was for `make_args`/`install_args`.
  - **`porting/tests/curl_http_roundtrip.c`**: a real round trip
    against real servers, not a version-string/link-only smoke check --
    a plain HTTP GET (proving the portable POSIX socket path actually
    drives a real TCP connection and curl correctly parses a real HTTP
    response) and an HTTPS GET (proving the mbedTLS backend performs a
    real TLS handshake and decrypts the response correctly -- the body
    could not come back as readable HTML otherwise) against
    `http(s)://example.com`, an IANA-reserved domain kept stable and
    minimal specifically for documentation/testing use (RFC 2606),
    checking both the HTTP status code and a known-stable string in the
    response body. `CURLOPT_SSL_VERIFYPEER`/`VERIFYHOST` are disabled
    for the HTTPS request since this project doesn't vendor or maintain
    a CA trust bundle (curl's own `--without-ca-bundle`/`--without-ca-path`)
    -- this doesn't weaken what the test actually proves, since the TLS
    handshake and record encryption/decryption still have to succeed
    for the response to decode into readable HTML at all.
  - **Verified for real on Linux** (WSL Ubuntu 20.04 + clang-18):
    `curl_http_roundtrip_test: ok http=200 https=200` for both the
    static and shared test, using curl's own real default configuration
    (`POSIX threaded` resolver, `--enable-shared`/`--enable-static` both
    on) -- confirmed the `fcntl()` fix alone was sufficient, no
    resolver-specific workaround needed (an earlier, more conservative
    `--disable-threaded-resolver` was tried first while still chasing
    the real root cause, then removed once the real fix was found and
    confirmed to work with curl's actual default). Also reran the full
    `port-test-recipes` aggregate (all other ports with automated
    tests) and the full Windows `ctest` suite (83/83) afterward to
    confirm the `libc`/`tools` changes didn't regress anything else.
    Linux: `shared-pass`.
  - **A real Windows build attempt found two more, distinct bugs**,
    neither one a repeat of the Linux/macOS DNS/`fcntl()` gaps above
    (those are general and already fixed everywhere).
  - **curl's own `configure` uses `AC_EGREP_CPP` (a pure
    preprocessor-only check via `$CPP $CPPFLAGS`, never `$CFLAGS`) for
    its "checking if socket is prototyped" probe.** With the usual
    `_WIN32`-family undefines only ever placed in `CFLAGS` (matching
    every other Windows recipe in this project), that one specific
    probe still saw `_WIN32` defined (clang's own default predefine for
    `*-w64-mingw32`), tried to `#include <winsock2.h>` (a header this
    PAL doesn't have), and curl concluded socket() wasn't usable at
    all -- surfacing later as a hard `#error "We cannot compile without
    socket() support!"` in `lib/transfer.c`, even though every actual
    compile/link-based probe (which does see `CFLAGS`) correctly found
    `socket()` working. Fixed by adding the identical undefines to
    `CPPFLAGS` too; confirmed directly, not assumed: "checking if
    socket is prototyped" now answers yes.
  - **Once that let configure complete, linking curl's own shared
    `libcurl.dll` failed with `ld.lld: error: duplicate symbol`** for
    `__crt_sys_shutdown`/`__crt_sys_poll`/`__crt_sys_unlink`/
    `__crt_sys_rename`/`setenv` -- each one defined both in this
    project's own `c.lib` (curl's normal Windows libc import library,
    expected) and in `libmbedcrypto.dll.a` (mbedtls's own import
    library, not expected at all -- these are this project's internal
    CRT/libc symbols, nothing to do with mbedtls's real crypto API).
    **Not a curl bug**: mbedtls's own `.dll` build
    (`porting/recipes/mbedtls.json`) statically embeds this project's
    libc into `libmbedcrypto.dll` and, with no symbol-visibility
    control at all (no `-fvisibility=hidden`, no `.def` file, no
    explicit `dllexport` annotations on mbedtls's own public API --
    upstream mbedtls's headers were never written with Windows DLL
    export control in mind), re-exports that embedded libc's own
    symbols right alongside mbedtls's real API. mbedtls's own
    standalone shared-library test still passes fine in isolation
    (nothing else there links against both mbedtls's DLL and this
    project's own `c.lib` at once) -- the collision only surfaces once
    a *third* library (curl) links against both mbedtls's shared import
    library and this project's own libc import library for the same
    final image. Not fixed this session: a real fix needs either
    proper symbol-visibility control added to mbedtls's own Windows
    `.dll` build, or curl linking against mbedtls's static libraries
    specifically even for curl's own shared build (a common, legitimate
    pattern -- a shared library statically embedding a dependency
    rather than depending on another shared library -- not yet wired
    into the recipe). Windows: `configure-blocked` (configure itself
    now succeeds after the `CPPFLAGS` fix above; the actual link step
    doesn't). macOS not yet verified this session.
  - **The user's own real macOS build attempt found one more distinct
    bug, in `tools/crt-port-build.py` itself.** curl's `configure` runs
    a real "checking runtime libs availability" probe as part of
    detecting the mbedTLS backend -- it compiles *and executes* a tiny
    test program linked against `-lmbedtls`/`-lmbedx509`/
    `-lmbedcrypto`/`-lz`. That probe failed outright ("one or more libs
    available at link-time are not available runtime"), because
    mbedtls's own hand-written `library/Makefile` builds its `.dylib`
    files with no explicit `-install_name` at all (unlike every other
    shared-library recipe here, which drives real GNU Libtool and gets
    a correct one automatically) -- ld64's default records a bare
    `libmbedcrypto.dylib` with no `@rpath`/absolute-path prefix, which
    dyld can never resolve via `LC_RPATH` (macOS's rpath mechanism only
    helps references already prefixed `@rpath/...`; unlike Linux's
    `DT_RPATH`/`DT_RUNPATH`, it does nothing for a bare name) regardless
    of the `-Wl,-rpath` flag `make_env()` already adds to `LDFLAGS`.
    mbedtls's own shared-library test had already passed on macOS
    despite this exact same install-name gap, because
    `run_port_tests()`/`port_test_env()` already sets
    `DYLD_LIBRARY_PATH` as a runtime-loader fallback before *running* a
    port's own test binary -- but configure's own internal runtime
    probes, run as part of the *build* step, never inherited it, since
    that env var was only ever set in the narrower test-running path.
    Fixed generally in `make_env()` itself, not with an mbedtls- or
    curl-specific workaround: `DYLD_LIBRARY_PATH` (macOS) /
    `LD_LIBRARY_PATH` (Linux, for symmetry and future-proofing, though
    Linux's own rpath mechanism didn't actually need this) now point at
    `PORT_PREFIX/lib` for every subprocess this tool spawns, build
    steps included, not just test runs. Verified this didn't regress
    anything on Linux: the full `port-test-recipes` aggregate stayed
    clean, and curl's own static+shared tests there still pass. Not yet
    re-verified on macOS itself this session (no local macOS hardware).
  - The `DYLD_LIBRARY_PATH` fix above turned out insufficient: the user
    re-ran `port-rebuild-curl` on real macOS hardware and hit the exact
    same "checking runtime libs availability... failed" error again.
    Root cause: `./configure` execs through `/bin/sh` (its own shebang
    interpreter), and macOS strips `DYLD_`-prefixed environment
    variables across an exec of any SIP-protected system binary like
    `/bin/sh` (documented dyld/SIP behavior, not specific to this
    project's own env-passing code) -- so the `DYLD_LIBRARY_PATH` set
    by the parent Python subprocess call never actually survived into
    configure's own child test-compile-and-run. Fixed for real,
    environment-independently, in `porting/recipes/mbedtls.json`
    instead: three new `library/Makefile` patches add
    `-install_name @rpath/$@` to the `APPLE_BUILD` `-dynamiclib` link
    recipes for `libmbedtls.dylib`/`libmbedx509.dylib`/
    `libmbedcrypto.dylib` (upstream never sets one at all). This bakes
    the correct load-command path directly into each `.dylib` at build
    time, which the consumer's own `-Wl,-rpath,PORT_PREFIX/lib`
    `LDFLAGS` (already set unconditionally by `make_env()`) then
    resolves correctly regardless of what environment variables survive
    any particular exec chain -- immune to the SIP-stripping issue
    above by construction, since it needs no environment variable at
    runtime at all. `make_env()`'s `DYLD_LIBRARY_PATH` addition is kept
    as defense in depth (harmless, still useful for any future
    runtime-loader case not gated behind a SIP-restricted exec).
    Verified on Linux (WSL Ubuntu 20.04 + clang-18, full rebuild from
    scratch): mbedtls's own `crypto-static`/`crypto-shared` tests and
    curl's own `http-roundtrip-static`/`http-roundtrip-shared` tests all
    still pass -- the new patches are inert on Linux (that Makefile
    target is only reached under `APPLE_BUILD=1`), so this is a pure
    regression check, not a positive macOS confirmation. Still needs a
    real macOS re-run to confirm.
  - **A real Windows rebuild attempt got past the previously-documented
    mbedtls-DLL duplicate-symbol blocker.** `libcurl.la`/`libcurlu.la`
    themselves now link cleanly -- no `ld.lld: error: duplicate symbol`
    at all this run. Not deliberately fixed (still tracked as
    open/unexplained in `TODO.md` rather than closed; needs a repeat
    rebuild or dedicated investigation before declaring it gone for
    good). That same rebuild surfaced a different, real bug instead,
    now fixed: curl's own CLI tool (`src/curl.c`/`curlinfo.c`, linked
    against the freshly built, still-uninstalled `libcurl.la`) failed
    with `call to undeclared function 'setmode'`/`'_spawnv'` and `use
    of undeclared identifier '_P_WAIT'` in GNU Libtool's own generated
    `.libs/lt-curl.c`/`lt-curlinfo.c` "uninstalled execution" wrapper
    source -- the exact same bug class already root-caused and fixed
    for libpng's own CLI/test binaries via `porting/shims/win32/
    libtool_wrapper_compat.h`, but curl.json was never wired up to use
    that existing shim at all (a real oversight, not a new bug). One
    genuinely new wrinkle beyond libpng's own case: libpng's recipe
    leaves `__MINGW32__` defined (only undefines the `_WIN32` family),
    so ltmain.sh's own `#elif defined __MINGW32__` rename block fires
    and the wrapper calls `getcwd`/`stat`/`chmod`/`putenv`/`setmode`
    through their underscore-prefixed spellings, which the shim's
    existing macro pairs already resolved. curl.json's own CFLAGS/
    CPPFLAGS additionally undefine `__MINGW32__` too (needed for curl's
    own configure/source to treat this target as portable POSIX, not
    just to steer the wrapper's `#include` choice) -- so that rename
    block never fires at all, and the wrapper's literal, unconditional
    `setmode(1,_O_BINARY);` call site (embedded directly by ltmain.sh's
    `func_emit_wrapper`, not itself behind any further `#ifdef`) stays
    spelled exactly `setmode`, undeclared either way (`_MSC_VER` is
    also undefined, so the MSVC `<io.h>`-declares-it branch never ran
    either). Fixed by (1) wiring the missing `force_include:
    ["porting/shims/win32/libtool_wrapper_compat.h"]` into curl.json's
    own Windows `target_overrides`, and (2) extending the shim itself
    with a fourth alias, `#define setmode _setmode` (the shim's
    existing real `_setmode()` implementation, just under the
    un-prefixed spelling too) -- confirmed harmless to add
    unconditionally: curl's own real source has exactly one reference
    to the bare name (`lib/curl_setup.h`'s `CURL_BINMODE` macro),
    itself inside a permanently-dead `#ifdef MSDOS` branch on every
    host this project targets. `_spawnv`/`_P_WAIT` needed no shim
    changes at all -- the shim's existing implementations were already
    unconditional, just never reached this recipe before because
    `force_include` itself was missing. See `porting/recipes/curl.json`'s
    own notes for the full trail. Found from the user's own real
    Windows build log; fixed source-level, not yet re-verified with a
    real Windows rebuild.

## 2026-08-13

- **Fixed two real, distinct CI-only failures for the Windows
  `_pei386_runtime_relocator` regression test (`tests/
  windows_pseudo_reloc_dll.c`/`consumer.c`, added in the entry below),
  both invisible on this local dev machine's warm build cache.** CI
  failed identically on both Windows legs (`windows-x64`/
  `windows-arm64`) for the commit introducing the test; the GitHub
  Actions job-logs API requires admin rights even on a public repo with
  no token (`403 Must have admin rights to Repository`), and only
  generic `check-runs` annotations ("Process completed with exit code
  1") were reachable -- so both rounds had to be root-caused via local
  reproduction, not by reading the actual CI log text.
  - **First attempt, plausible but wrong**: hypothesized GitHub-hosted
    Windows runners might have NTFS 8.3 short-name generation disabled,
    so `GetShortPathNameW()` would silently return the original,
    space-containing SDK `.lib` path unmodified, breaking
    `tools/crt-cc`'s unquoted shell-word-splitting. Fixed as a genuine
    robustness improvement regardless (`file(COPY)`-ing the two needed
    SDK `.lib` files into a space-free build-tree subdirectory instead
    of relying on short-path conversion, and dropping the unnecessary
    short-path treatment for `CRT_HOST_CC`, whose every use in
    `tools/crt-cc` is already individually double-quoted) -- but CI
    still failed identically on the next run, disproving the
    hypothesis.
  - **Second attempt, reproduced and root-caused for real**: rather
    than guessing again, wiped `out/windows-host-ninja-debug` entirely
    and ran the literal `cmake --workflow --preset
    windows-host-ninja-debug` command CI itself runs. This reproduced
    the failure locally: the failing command showed
    `/system/bin/mksh.exe` (no drive letter) instead of the expected
    full rootfs path. Root cause: `CRT_ROOTFS` is `set()` in the
    top-level `CMakeLists.txt` *after* `add_subdirectory(tests)` already
    runs, so `tests/CMakeLists.txt`'s own reference to `${CRT_ROOTFS}`
    saw it as empty on a genuinely fresh configure -- invisible on this
    machine because its build tree had `CRT_ROOTFS` cached from earlier,
    unrelated configures, masking the bug every previous local run. A
    second, related bug found in the same pass: `add_dependencies(
    aggregate_target A B)` only orders `A`/`B` relative to the aggregate
    target's own completion, not relative to sibling `add_custom_command()`s
    (independent DAG nodes) unless those commands' own `DEPENDS` lists
    name `A`/`B` directly -- a real, previously-latent race, invisible
    locally but real on every clean CI run. Fixed both: reference
    `${CMAKE_BINARY_DIR}/rootfs` directly (matching `CRT_ROOTFS`'s own
    definition) instead of the not-yet-set variable, and add `rootfs
    sysroot` directly to the `add_custom_command()`s' own `DEPENDS`
    lists. Verified via the same clean-tree `cmake --workflow` run
    (83/83 passed, including `windows_pseudo_reloc_test_runs`), then
    confirmed all 5 GitHub Actions legs green on the real CI run.
    Methodological note for future CMake-configure-time bugs: local
    warm-cache testing is not equivalent to CI's always-fresh-checkout
    testing -- a genuinely clean `out/` wipe is the only reliable local
    repro for this class of bug.

## 2026-08-12

- **Implemented Windows/PE "runtime pseudo relocation" support
  (`_pei386_runtime_relocator()`), fixing the `libffi` shared-link gap
  found in the previous entry, and added a permanent, project-owned
  ctest regression test for it.** GNU ld/lld's auto-import extension
  lets code reference a DATA symbol exported from another DLL with no
  `__declspec(dllimport)` annotation -- exactly what libffi's own
  `ffi.h` relies on ("GCC has autoimport and autoexport... always mark
  externally visible symbols as dllimport for MSVC clients"). The
  linker makes this work by baking a placeholder value into the
  referencing instruction/data at link time, then emitting a table of
  "pseudo relocations" for a startup-time fixup once the real DLL
  addresses are known -- real mingw-w64 crt0 processes this table via a
  function conventionally named `_pei386_runtime_relocator()`, and
  `lld-link` refuses to produce a final image with a non-empty
  pseudo-reloc table unless a symbol with that exact name exists
  somewhere in the link. Implemented from scratch
  (`libc/src/arch/windows/common/pseudo_reloc.c`), with the exact binary
  table format (a 12-byte v2 header, then 12-byte `sym`/`target`/`flags`
  entries) cross-checked against two independent sources: mingw-w64's
  own reference decoder (`mingw-w64-crt/crt/pseudo-reloc.c`) and this
  exact toolchain's own encoder (LLVM lld's `PseudoRelocTableChunk` in
  `lld/COFF/Chunks.cpp`) -- both agreed. `__RUNTIME_PSEUDO_RELOC_LIST__`/
  `_END__` (the list's own boundary symbols, auto-provided by the linker
  only when a non-empty table actually exists -- confirmed the hard way:
  an earlier, unguarded version broke every CMake-native Windows build,
  which never needs auto-import at all, with "undefined symbol") are
  declared weak, the same pattern already used for `__crt_run_fini_array`.
  Wired into `crt1.c`'s `mainCRTStartup()` as the very first statement,
  before anything else could touch an unfixed auto-imported reference.
  A second, independent bug surfaced immediately once the link-time gap
  was closed: the *runtime* access-violated on the very first relocation
  entry processed, since the target location a pseudo relocation patches
  is frequently inside `.text` (a compile-time-foldable `&global`
  reference gets embedded as a `movabs` instruction's own immediate
  operand), which is mapped execute+read-only by default -- fixed by
  temporarily `VirtualProtect()`-ing the containing page(s) around each
  write and restoring the original protection afterward, matching real
  mingw-w64's own approach.
  **New permanent regression test**: `tests/windows_pseudo_reloc_dll.c`/
  `windows_pseudo_reloc_consumer.c`, registered as ctest
  `windows_pseudo_reloc_test_runs`. Deliberately built through
  `tools/crt-cc`'s GNU-ABI path directly (via a `tests/CMakeLists.txt`
  `add_custom_command` invoking `mksh.exe tools/crt-cc`, reusing the
  project's already-established `GetShortPathNameW()`-based fix for
  `tools/crt-cc`'s own unquoted shell-word-splitting on real Windows SDK
  paths), not CMake's native build path -- confirmed directly that the
  native path's default MSVC ABI has no auto-import concept at all, so
  the mechanism can only ever be exercised through the same GNU-ABI path
  every third-party port build already uses. Also confirmed, the hard
  way, that the test's *first* draft (a plain `int* p = &value;` runtime
  assignment) passed vacuously: that codegen shape can legitimately load
  through the real import stub directly, producing an empty pseudo-reloc
  list regardless of whether the fix works at all. Rewriting it to match
  libffi's actual triggering shape -- storing the address in a
  static-duration aggregate initializer, which must be literal embedded
  bytes rather than a load instruction -- produces a genuine, non-empty
  relocation list; a negative-control run (temporarily reverting the
  `crt1.c` call site) confirmed the rewritten test fails correctly
  (reading back the raw placeholder value) without the fix, and passes
  with it restored. Full local Windows `ctest`: 83/83. `libffi`'s own
  official `port-test-libffi` shared variant also now passes
  (`libffi_call_test: ok result=42`), confirmed via the real
  `crt-port-build.py` pipeline, not just the isolated regression test.

- **Ran the new `port-test-recipes` aggregate on Windows to verify it
  end-to-end there too (the entry below only ran/confirmed it on
  macOS): bzip2, xz, zlib, and libpng all pass both their static and
  shared consumer tests; libffi's shared test surfaced one new, genuine
  Windows gap.** `cmake --build --preset windows-host-ninja-debug
  --target port-test-<name>` for each of the five recipes:
  `bzip2_roundtrip_test: ok`, `xz_roundtrip_test: ok`,
  `zlib_roundtrip_test: ok`, and `libpng_roundtrip_test: ok` all printed
  correctly for both the static and shared build of each library
  (matching what the entry below already confirmed on macOS).
  `libffi`'s static test (`libffi_call_test: ok result=42`) also passed,
  but its *shared* test failed to even link: `ld.lld: error: output
  image has runtime pseudo relocations, but the function
  _pei386_runtime_relocator is missing; it is needed for fixing the
  relocations at runtime`. This is a genuinely new finding, distinct
  from libffi's already-documented `-O1`/`-O2` `ffi_call()`-repeat-call
  bug and from the earlier session's own libffi-shared verification
  (`porting/recipes/libffi.json`'s notes) -- that earlier verification
  used `dlopen()` to load the built DLL at runtime, which never needs
  MinGW's auto-import/pseudo-relocation machinery; the new port test
  instead links directly against `libffi.dll.a` (a real import library)
  the way a normal C program consuming a prebuilt DLL would, and
  libffi's public headers apparently reference at least one exported
  *data* symbol in a way GNU ld's auto-import feature resolves via a
  runtime pseudo-relocation fixup table -- something real mingw-w64
  provides via `_pei386_runtime_relocator` (in `libmingwex.a`/its own
  CRT startup) that this project's PAL does not implement. Not
  investigated further this pass (a new startup-time PAL feature, not a
  quick fix) -- tracked as its own `TODO.md` item; `docs/porting_status.md`'s
  libffi row updated to record the gap without changing its existing
  `partial` status (which already covers other Windows-shared caveats).

- **xz/liblzma macOS is now verified beyond "it built": real
  compress/decompress round trips pass against both static and shared
  liblzma, and `tools/crt-cc` now handles GNU Libtool's Darwin
  relocatable-link path correctly.** `cmake --build --preset
  macos-host-ninja-debug --target port-rebuild-xz` initially showed the
  real blocker was not Mach-O constructor/destructor startup: macOS's
  `init_array_test` already passed, and dyld handles constructors for
  Mach-O. The actual failure was in `tools/crt-cc` treating libtool's
  `-r`/`-Wl,-r` partial links like final executable/shared links,
  adding startup objects, entry flags, CRT libraries, and `-dead_strip`;
  ld64 rejects `-r` with `-dead_strip`. After separating relocatable
  link mode, a second Darwin-specific issue appeared: xz builds with
  `-fvisibility=hidden`, and ld64's partial link lowered `private
  external` definitions to local symbols, leaving same-output references
  unresolved at final dylib link. `tools/crt-cc` now adds
  `-Wl,-keep_private_externs` only for macOS relocatable links. Verified
  with a standalone CRT-linked smoke program calling
  `lzma_easy_buffer_encode()` at preset `9|LZMA_PRESET_EXTREME` with
  `LZMA_CHECK_CRC64`, then `lzma_stream_buffer_decode()`, and comparing
  the decoded 256 KiB buffer byte-for-byte against the original. Both
  installed `liblzma.a` and `liblzma.5.dylib` passed; `otool -L`
  confirmed the shared smoke binary loads this port's installed dylib.
  The same round-trip is now formalized as a recipe-declared port test:
  `porting/tests/xz_roundtrip.c` is built and run by
  `cmake --build --preset <preset> --target port-test-xz`, and the
  aggregate `port-test-recipes` target runs every recipe that declares
  automated tests. That aggregate now also covers bzip2, zlib, libpng,
  and libffi with project-owned consumer tests under `porting/tests/`:
  bzip2/zlib/xz do real compress/decompress round trips, libpng writes
  and reads a tiny RGBA PNG through memory callbacks, and libffi verifies
  a single `ffi_call()` round trip. Full macOS `ctest` remained green
  (`75/75`).

## 2026-08-11

- **`.init_array`/`.fini_array` work concluded on all three OSes: macOS
  confirmed directly on real hardware (no code changes needed), and xz's
  own Windows round trip finally passes -- a second, distinct `lld-link`
  limitation had to be found and routed around first.** Continuation of
  the same-day investigation documented in the two entries below.
  - **macOS: confirmed on real hardware, zero code changes needed.**
    The analysis from the entry below (dyld runs Mach-O
    `__DATA,__mod_init_func` constructors automatically before any entry
    point runs; destructors are registered by dyld through the same
    Itanium C++ ABI atexit mechanism this project's `exit.c` already
    drains unconditionally via `__cxa_finalize(0)`) held up: running
    `tests/init_array_test.c` directly on real macOS hardware printed
    `init_array_test: ok`, and the CI `macos-aarch64` leg came back green
    on the same commit (all 5 legs). No macOS-specific source changes
    were needed at all.
  - **xz/liblzma's own Windows round trip: re-ran it after the general
    Windows `.init_array` fix landed, and it STILL crashed** -- not a
    leftover from the first bug, but a second, genuinely distinct
    `lld-link` limitation. Isolated with a minimal, xz-independent
    reproduction: a trivial `__attribute__((constructor))` function
    inside a standalone static archive (`.a`), linked the exact same way
    `tools/crt-cc` links any third-party port's own archive. Root cause:
    `lld-link` does not reliably merge multiple static-archive-derived
    plain (non-`$`-grouped) `.ctors`/`.dtors` contributions into one
    contiguous, correctly-bracketed region -- unlike GNU `ld`'s default
    linker script, which guarantees this via explicit `KEEP()` ordering
    rules (`KEEP(*crtbegin.o(.ctors)) KEEP(*(.ctors)) KEEP(*crtend.o
    (.ctors))`); COFF/PE and `lld-link` have no equivalent mechanism.
    Confirmed directly with symbol-address inspection: the begin/end
    sentinel markers landed immediately adjacent to each other (an empty
    bracketed range, zero bytes of real content between them) regardless
    of whether the real archived constructor was placed before, after,
    or even bundled into the very same archive as the sentinels --
    the archive-sourced entry consistently landed somewhere else in the
    final image entirely (located by a raw pointer-value search through
    the linked binary, landing in `.rdata` at a completely different,
    non-adjacent offset). This affects only the GNU-ABI (`.ctors`/
    `.dtors`) convention `tools/crt-cc` port builds use; the MSVC-ABI
    (`.CRT$XC*`/`.CRT$XT*`) convention this project's own CMake-native
    builds use is unaffected, since `lld-link`'s alphabetical `$`-suffix
    sorting is insertion-order-independent by design (re-confirmed with
    the same technique: three objects deliberately linked in reverse
    order still produced a correctly-ordered merged section).
    Given this is a genuine `lld-link` limitation (not something fixable
    by rearranging this project's own sentinel objects) and liblzma
    already ships a portable fallback purpose-built for exactly this
    situation -- its CRC32/CRC64 dispatch code (`crc32_fast.c`/
    `crc32_small.c`/`crc64_fast.c`/`crc64_small.c`/`lz_encoder.c`, all
    sharing the same `CRC32_SET_FUNC_ATTR`-guarded pattern) uses a "First
    Call Resolution" lazy dispatcher whenever `HAVE_FUNC_ATTRIBUTE_
    CONSTRUCTOR` is undefined, needing no constructor support at all --
    the fix routes around the `lld-link` gap instead of fighting it: a
    new `porting/recipes/xz.json` `build.patches` entry undefines
    `HAVE_FUNC_ATTRIBUTE_CONSTRUCTOR` in `src/common/sysdefs.h`, guarded
    by `#if defined(CRT_TARGET_OS_WINDOWS)` so it is a genuine no-op on
    Linux/macOS (where the constructor path already works correctly and
    is left untouched). `./configure`'s own `HAVE_FUNC_ATTRIBUTE_
    CONSTRUCTOR` probe is a bare `ac_fn_c_try_compile` compile-only test
    (never even links), so it always reports "yes" for this toolchain
    regardless of the archive-linking gap the probe has no way to see --
    hence overriding it after the fact via a header patch rather than
    trying to make `./configure` itself detect "no".
    **Verified**: rebuilt the whole port clean through the real
    `crt-port-build.py` pipeline (not just the isolated repro), then a
    full compress/decompress round trip (preset 9|EXTREME,
    `LZMA_CHECK_CRC64`, 256 KB real match-finder-exercising input,
    byte-for-byte compare after decode) passes on Windows against both
    the static (`liblzma.a`) and shared (`liblzma-5.dll` via
    `lzma.dll.lib`) build. `xz` (liblzma) is now **`shared-pass` on both
    Linux and Windows**; macOS still `pending` (no macOS hardware for the
    port itself this session, though the general mechanism it would
    depend on is now confirmed working there).

- **Ported xz/liblzma 5.8.3 to Linux (`shared-pass`); Windows build is
  clean too (`configure-pass`) but blocked at runtime by the
  `.init_array` gap the very same investigation found (see the dedicated
  entry below).** `porting/recipes/xz.json`, scoped to liblzma only
  (`--disable-xz`/`-xzdec`/`-lzmadec`/`-lzmainfo`/`-lzma-links`/
  `-scripts` skip the CLI tool family's sandboxing/gettext/console
  surface -- a library-only consumer like a future curl/pcre2 port
  doesn't need it). Configure+make+install succeeded on the first real
  attempt on Linux x86_64 (WSL Ubuntu 20.04 + clang-18); the real
  verification round trip is what surfaced and confirmed the
  `.init_array` bug documented in the next entry.
  - **Fully verified on Linux**: after the `.init_array` fix, a
    standalone `lzma_easy_buffer_encode()`/`lzma_stream_buffer_decode()`
    round trip at preset 9|EXTREME with `LZMA_CHECK_CRC64` (real
    match-finder allocations up to ~512 MB, not a toy input) produces
    byte-for-byte correct output against both the static (`liblzma.a`)
    and shared (`liblzma.so.5`, `ldd`-verified against this project's own
    sysroot/port install dirs) build.
  - **A second apparent bug, investigated and closed as "not a real
    bug"**: encoding a larger buffer hung inside this project's own
    `malloc()` lock-wait loop, with the lock's raw value read back as
    outright garbage despite the process being confirmed genuinely
    single-threaded (`/proc/<pid>/status` showed `Threads: 1`). A gdb
    hardware watchpoint on the lock (`watch *(int*)&heap_lock`) traced
    every corrupting write to `buffer_putc <- vsnprintf <- snprintf <-
    main` -- the verification test's *own* `main`, not libc or liblzma --
    and the corrupted bytes decoded to literal fragments of the test's
    own format string. Root cause: the test built its input with a
    `snprintf()`-in-a-loop pattern that accumulated the return value into
    a running length without checking it against the destination
    buffer's remaining size; once the accumulated length exceeded the
    buffer, `remaining = buffer_size - accumulated_length` (both
    `size_t`) underflowed to a huge unsigned value, handing the next
    `snprintf()` call an effectively unbounded size limit that wrote
    straight past a `static` buffer into whatever the linker placed next
    in BSS -- which happened to be malloc's own lock. Fixed the test
    (not the CRT); the "bug" never existed outside the test harness. A
    useful reminder that a watchpoint beats theorizing once "single
    malloc pattern reproduces fine standalone but not in the real
    program" stops narrowing things down on its own.
  - **Windows build fixes, all generalized (not xz-specific) since any
    future Windows configure recipe could hit the same gaps**: (1)
    `-U__MINGW32__` added to this recipe's own Windows CFLAGS -- xz's
    `sysdefs.h`/`mythread.h` both branch on `#ifdef __MINGW32__` for real
    mingw-w64-specific code this sysroot doesn't have an equivalent for
    (`#include <_mingw.h>`, a header this project doesn't ship; and
    `#define sigset_t _sigset_t`, which breaks against this project's own
    real, working `<signal.h>` `sigset_t`). (2) `tools/crt-port-build.py`
    now presets `$RC` to `llvm-rc` (this project's LLVM install ships
    one; nothing pointed libtool's resource-compile step at it before,
    the same class of gap `$LD`/`$DLLTOOL`/`$OBJDUMP`/`$NM` already had
    fixes for) and wraps it through `crt-native-tool` like the other
    native tools. (3) `llvm-rc`'s GNU-windres-compatibility `-i`/`-o`
    flag combination turned out to mis-invoke its own internal clang
    preprocessing step (confirmed directly: the identical `.rc` file
    compiles fine with MS-`rc.exe`-style positional-input + `-FO`
    arguments, but fails with `-i`/`-o`) -- rather than chase a `llvm-rc`
    bug, skipped building xz's optional `liblzma_w32res.rc` (a purely
    cosmetic DLL version-info resource) via a `target_overrides.windows.
    make_args` override of the two Automake-generated variables
    controlling it (`am__append_46`/`am__objects_41` for this exact xz
    version). (4) That override then exposed a real, separate
    `crt-port-build.py` gap: `target_overrides.<os>.make_args` only ever
    reached the build (`make`) step, not `make install` -- Automake's
    `install` target re-derives its own dependency chain and rebuilt the
    skipped resource anyway. Generalized: `install_args` now gets the
    same `VAR=value` overrides `make_args` does.
  - **Windows: confirmed at runtime (not just build-time) after all four
    fixes above**: `liblzma.a`/`liblzma-5.dll` both build clean, but a
    standalone test showed a trivial `lzma_version_string()` call working
    while the first real `lzma_easy_buffer_encode()` call segfaults, on
    both the static and shared build -- the same "first real call into
    the CRC dispatcher" shape as the Linux bug above. Checked what a fix
    would need to look like (not implemented this session): PE
    constructors compile into a bare `.ctors` section for this toolchain
    (confirmed via `llvm-objdump -h` on a real
    `__attribute__((constructor))` test), the GNU/MinGW convention, not
    MSVC's `.CRT$XCU` family -- `.ctors` has no ELF-style automatic
    linker-provided boundary symbols, so a correct fix needs the same
    `-1`-sentinel-object technique real mingw-w64 crt0 uses. Left as a
    tracked TODO.md item rather than rushed.
  - macOS: not attempted (no macOS hardware this session).

- **Fixed a real, general CRT startup gap for Linux: `crt1` never ran ELF
  `.init_array` (`__attribute__((constructor))` functions, also what runs
  C++ global object constructors) for the executable entry point.**
  Found while porting xz/liblzma (`porting/recipes/xz.json`), not while
  testing this project's own code directly: liblzma's CRC32 dispatcher
  (`src/liblzma/check/crc32_fast.c`) picks its implementation once via a
  `static void crc32_set_func(void) __attribute__((__constructor__))`,
  which the linker places into `.init_array` -- and a real
  `lzma_easy_buffer_encode()`/`lzma_stream_buffer_decode()` round-trip
  test (the same kind of "verified past 'it built'" check already
  established for zlib/libpng/bzip2) segfaulted at a null function
  pointer the very first time `lzma_crc32()` was called, since that
  constructor had never run and the dispatch pointer stayed
  zero-initialized. Confirmed via gdb: `rip=0x0`, called from
  `lzma_stream_header_encode` -> `lzma_crc32` -> `crc32_func` (NULL).
  A *shared* library's own `.init_array` is a completely different,
  unaffected mechanism -- the OS's own dynamic linker runs that
  automatically when the `.so`/`.dylib`/`.dll` loads -- which is exactly
  why this had never surfaced before: every prior port that got this far
  (zlib, libpng, sqlite-amalgamation, bzip2) linked shared, not static.
  - **Fix** (Linux only for now; see the follow-up items below):
    `libc/src/arch/linux/common/init_fini_array.c` adds
    `__crt_run_init_array()`/`__crt_run_fini_array()`, using the
    `__init_array_start`/`__init_array_end`/`__fini_array_start`/
    `__fini_array_end` boundary symbols the default GNU ld/lld linker
    script already provides automatically (`PROVIDE_HIDDEN`) whenever
    the corresponding output section exists. `__crt_run_init_array()` is
    called directly from `crt1.S` (both `libc/src/arch/linux/x86_64/
    crt1.S` and `.../aarch64/crt1.S`) right before `call main`/`bl main`.
    `__crt_run_fini_array()` is reached from `libc/src/exit.c` via a weak
    reference (`void __crt_run_fini_array(void) __attribute__((weak));`,
    matching the existing `__crt_windows_ensure_fork_capable_relaunch`
    weak-symbol pattern already used in this codebase), called right
    before `__crt_sys_exit()`.
  - **Why a new, separate startup object instead of folding into
    `crt1.o` directly**: `libc/CMakeLists.txt` already has
    `install(FILES "$<TARGET_OBJECTS:crt1>" DESTINATION lib RENAME
    crt1.o)`, which assumes the `crt1` object library has exactly one
    object file -- adding a second source directly broke that rename
    (`file INSTALL` with multiple source files and one `RENAME` target
    fails). Fixed instead by giving `init_fini_array.c` its own object
    library/install name (`crt1_init_array.o`, parallel to `crt1.o`),
    matching the established pattern for `dllcrt.o` on Windows (another
    fixed-named startup object referenced explicitly by
    `tools/crt-cc`/CMake rather than merged into one file). `tools/
    crt-cc`'s Linux non-shared-mode link line now includes
    `${CRT_SYSROOT}/lib/crt1_init_array.o` alongside `crt1.o`; the CMake
    test/shell executable-build paths (`tests/CMakeLists.txt`,
    `shell/CMakeLists.txt`) reference it the same way through a new
    `CRT_STARTUP_OBJECTS` variable, set once in `libc/CMakeLists.txt`
    with `PARENT_SCOPE` (`add_subdirectory(libc)` always runs before
    `add_subdirectory(shell)`/`add_subdirectory(tests)`) instead of
    repeating `$<TARGET_OBJECTS:crt1>` (now 3 objects, not 1) at every
    one of the 8 executable-creation call sites across those two files.
  - **Verified past "it built"**: re-ran the exact liblzma round-trip
    test that originally crashed -- the null-pointer call is gone, and a
    small (well under liblzma's real match-finder threshold) buffer
    round trip now succeeds with correct compressed/decompressed output.
    Full `ctest` stayed at 100% (74/74 Linux, 81/81 Windows -- Windows
    re-verified only for regression-safety, since this fix's actual
    Linux-specific code doesn't touch Windows at all beyond the
    now-shared `CRT_STARTUP_OBJECTS` CMake refactor).
  - **Known follow-up gaps, not fixed here**: (1) Windows needs a
    `.CRT$XCU`-section-walking equivalent (PE's own constructor-array
    convention) and macOS needs to walk Mach-O's
    `__DATA,__mod_init_func` -- neither implemented; only matters for
    statically-linked constructor-reliant code, which nothing in this
    project's own test suite or any prior port had ever exercised until
    now. (2) An executable linked against *shared* libc (`c_shared`)
    rather than static: `exit()` then lives inside `libc.so`, and its
    weak reference to `__crt_run_fini_array()` (which only ever lives in
    the executable's own statically-embedded `crt1` object, so it always
    sees the *calling executable's* own `.init_array`/`.fini_array`
    boundaries rather than some unrelated DSO's) likely doesn't resolve
    across that DSO boundary, since this toolchain never passes
    `-rdynamic`/`--export-dynamic`. Construction (`.init_array`) is
    unaffected by this second gap either way, since `crt1.o` is always
    statically embedded into the executable regardless of how libc
    itself ends up linked.

- **Built a permanent, cross-OS regression test for constructor/
  destructor support and used it to root-cause and fix the Windows half
  of the `.init_array`/`.fini_array` gap (see the entry above); analyzed
  macOS and found it likely needs no fix at all.** Prompted directly by
  the observation that if the gap hit both Linux and Windows, it
  probably hit macOS too, and that a dedicated test (rather than
  re-purposing xz/liblzma each time) would make it debuggable going
  forward. `tests/init_array_test.c`: one `__attribute__((constructor))`
  setting a flag, one `__attribute__((destructor))` printing a
  pass/fail line depending on whether the flag got set first --
  deliberately minimal, single source file, no external dependencies,
  wired into `tests/CMakeLists.txt` via the normal `add_crt_test()` path
  so it runs on every OS's `ctest` alongside everything else with zero
  special-casing.
  - **Windows: fixed and verified (local `ctest`, 82/82, all three OSes'
    Windows-specific work confirmed via CI on prior commits in this
    series).** The obvious first attempt -- reusing the exact GNU-style
    `.ctors`/`.dtors` bracketing technique already designed for this gap
    (see the entry above) -- built clean but the test still failed
    silently (no output at all, not even the destructor's "never ran"
    branch). Root cause, found by disassembling the actual compiled test
    object with `llvm-objdump`: this project's own CMake-native Windows
    builds (libc, `tests/`, `shell/`) compile with plain clang and no
    explicit `--target`, which defaults to `*-pc-windows-msvc` --
    entirely different from `tools/crt-cc`'s explicit
    `--target=*-w64-mingw32` used for third-party ports -- and under that
    default target, clang lowers `__attribute__((constructor))`/
    `((destructor))` to the fixed section names `.CRT$XCU`/`.CRT$XTX`,
    not `.ctors`/`.dtors` at all. The project has TWO real, simultaneously
    -live constructor/destructor conventions on Windows depending on
    compile path, not one. Confirmed `lld-link` honors MSVC's
    alphabetical `$`-suffix section-group sorting regardless of
    link-command-line order (a direct empirical test: three objects
    contributing `.CRT$XCA`/`.CRT$XCU`/`.CRT$XCZ`, linked in deliberately
    reversed order, still produced a correctly A-then-U-then-Z-ordered
    merged section) -- so `.CRT$XCA`/`.CRT$XCZ` (ctors) and `.CRT$XTA`/
    `.CRT$XTZ` (dtors) sentinels bracket that convention without needing
    the GNU convention's careful link-line positioning at all. Both
    conventions are now walked unconditionally by the same three shared
    objects (`libc/src/arch/windows/common/ctors_begin.c`/`ctors_end.c`/
    `init_fini_array.c`), `__crt_run_init_array()` called from
    `crt1.c`'s `mainCRTStartup()` right before `main()`, and
    `__crt_run_fini_array()` reached from `exit.c` via the same weak-
    reference pattern Linux already used (necessary here too:
    `exit.c` is compiled into both the static `c` library and the
    `c_shared` DLL, and the walker object is only ever linked into a
    final executable's own `crt1`, never into `c_shared` itself).
  - **A second, independent bug surfaced by the very act of testing this
    fix**: once real destructor calls started happening on Windows, the
    regression test failed a *different* way -- printing both the pass
    and fail lines, from two different OS processes (confirmed with a
    `getpid()` probe). Root cause: `libc/src/arch/windows/common/
    fork_capable_relaunch.c` (already linked into every `ctest` target,
    not something newly added) has its startup self-relaunch parent
    process `WaitForSingleObject()` its freshly-spawned child, then
    forward the child's exit code by calling `exit(code)` -- a call that
    was harmless before this fix landed, since Windows had no
    `__crt_run_fini_array()` for it to reach, but which now incorrectly
    re-ran the test's destructor a second time in the *parent* process,
    which never ran the matching constructor (it never reaches its own
    `main()` at all -- only the relaunched child does). Fixed: that call
    is now `_exit(code)`, a raw process-terminating syscall with no
    atexit/`__cxa_finalize`/fini_array side effects, which is what a pure
    wait-and-forward wrapper should have used from the start. A real,
    pre-existing (not newly introduced) latent bug, invisible until now
    for the same reason the original gap was: nothing with an observable
    destructor side effect had ever run through this relaunch path
    before.
  - **macOS: analysis-based, not yet empirically confirmed** (no macOS
    hardware this session). Unlike Linux/Windows, Mach-O constructors
    (`__DATA,__mod_init_func`) are run automatically by dyld itself for
    every dynamically-linked image -- including the main executable --
    before dyld ever transfers control to any entry point, regardless of
    what that entry symbol is named; macOS has no fully-static executable
    format, so dyld's own image-init sequence unconditionally runs first.
    Mach-O destructors (`__DATA,__mod_term_func`) are, in turn, believed
    to be registered by dyld through the same Itanium C++ ABI atexit
    mechanism this project's `exit.c` already drains unconditionally via
    `__cxa_finalize(0)`. If that holds, macOS needs no new walker code at
    all -- `tests/init_array_test.c` should just pass there as-is via the
    CI `macos-aarch64` leg. Documented as inference from well-established
    dyld/Mach-O behavior, not as a confirmed fix, pending that CI run.
  - Re-verifying xz/liblzma's own Windows round trip specifically (rather
    than the general-purpose regression test above) was not completed
    this session -- see `TODO.md`'s xz entry.

- **Ported bzip2 1.0.8 to Linux and Windows (`shared-pass` both), the
  first entry in the new porting-matrix-expansion queue** (`bzip2` ->
  `xz` -> `pcre2` -> `mbedtls` -> `curl`, `TODO.md`). Upstream ships no
  Autoconf `configure`, just a hand-written `Makefile` (`make CC=...`)
  plus a second, Linux/GNU-ld-hardcoded `Makefile-libbz2_so` for the
  shared build -- rather than teach `crt-port-build.py` to drive either
  directly, `porting/recipes/bzip2.json` builds bzip2's 7 library source
  files (excluding the CLI tool/DLL sample) through the existing
  `amalgamation` build system already proven for
  `sqlite-amalgamation.json`, so the established cross-platform
  static+shared naming/versioning convention applies unmodified with no
  new `crt-port-build.py` code. Windows needed one `-U_WIN32` CFLAGS
  override (same technique as zlib.json/libpng.json/sqlite-
  amalgamation.json): `bzlib.h`/`bzlib.c` both gate a `_WIN32` branch
  (a `WINAPI`-calling-convention `#include <windows.h>` in the header,
  an `<io.h>`+`setmode()` binary-mode dance in the source) that this
  sysroot doesn't need -- this PAL's I/O is already byte-transparent.
  No other CRT/PAL gap surfaced; it built clean on the first real attempt
  on both hosts.
  - Verified past "it built": a standalone
    `BZ2_bzBuffToBuffCompress`/`BZ2_bzBuffToBuffDecompress` round-trip
    test program linked and ran correctly against both the static
    (`libbz2.a`) and shared (`bz2.dll`/`libbz2.so.1.0.8`) build on both
    hosts. Windows verified on real x64 hardware via
    `crt-port-build.py --use-crt-shell`. Linux verified on real x86_64
    hardware through WSL Ubuntu 20.04 + a freshly-installed clang-18
    toolchain (Ubuntu 20.04's default clang-10 predates
    `__builtin_elementwise_sqrt`, needed by `libm/src/basic.c` --
    unrelated to this port, just a prerequisite for building this
    project at all in that environment); `ldd` on the dynamically-linked
    Linux test binary confirmed every CRT dependency (`libc.so`/
    `libm.so`/`libdl.so`/`libc++.so`) and `libbz2.so.1` itself resolve to
    this project's own sysroot/port install dirs, not any host package --
    the same rpath verification zlib/libpng/sqlite-amalgamation already
    established.
  - Found and routed around (not a CRT bug): reproducing the Linux leg
    from this Windows dev machine via WSL against a plain `/mnt/c`-mounted
    checkout hit `FileNotFoundError` executing `tools/crt-cc`, because
    Windows `git` checks shell scripts out with CRLF line endings and the
    Linux kernel's shebang parser then looks for a literal `/bin/sh\r`
    interpreter that doesn't exist. Real CI's Linux legs check out fresh
    via native Linux `git` (LF), so this never surfaces there -- a plain
    `git clone` of the working tree into WSL's native filesystem (LF on
    checkout) was enough to work around it for local verification.
  - macOS: not run (no macOS hardware available this session); expected
    to build the same generic-Unix way as Linux, since none of bzip2's
    source has an `__APPLE__`-gated branch, but left `pending` in
    `porting/recipes/bzip2.json`/`docs/porting_status.md` until actually
    verified, per this project's own conservative status-value policy.

- **Concluded the Windows `make -jN` work: stress-tested at libpng scale
  and enabled parallel builds by default, matching macOS/Linux, per
  explicit direction ("libpng로 시험을 해 보자. 그리고, 문제가 없으면
  linux/macOS와 동일하게 처리하도록 하자").** The two entries below this
  one root-caused and fixed the actual bugs against zlib (small, fast to
  iterate on); this entry is the follow-up scale test and the resulting
  policy change.
  - **Real libpng build, `-j 12`** (this machine's `os.cpu_count()`):
    zlib is a hand-written Makefile with ~15 compile jobs; libpng is real
    GNU Autoconf + Libtool, ~40 compile/link steps including multiple
    `contrib/libtests`/`contrib/tools` executables, a shared DLL with a
    real Libtool `nm`/`sed` export-symbol pipeline, and an actual
    dependency on zlib being installed first -- a substantially different
    and heavier concurrency shape, and historically "the single longest
    blocker chain of the whole Windows porting effort" (see the
    2026-08-07 libpng `shared-pass` entry). Completed in full with zero
    errors, zero warnings, and zero jobserver messages of any kind:
    `make` phase 295.3s, `make install` 24.0s. Verified past "it built"
    into "it actually works": `pngtest.exe` (libpng's own real
    functional self-test, not a synthetic smoke check) reports
    `libpng passes test` against a real PNG with eXIf metadata, all 7
    interlace passes.
  - **Flipped the default**: `tools/crt-port-build.py`'s
    `jobs = 1 if target_os == "windows" and use_crt_shell else
    (os.cpu_count() or 2)` special case is gone -- every OS, Windows
    included, now defaults to `os.cpu_count() or 2`, matching how
    macOS/Linux already worked; `--jobs N` still overrides it per
    invocation for testing. Re-verified both zlib and libpng build
    correctly with *no* `--jobs` flag at all (relying purely on the new
    default resolving to this machine's real core count) before landing
    the change -- not just with the flag explicitly passed during the
    original investigation.
  - Full `ctest` 81/81 throughout (before and after the default flip).

- **Root-caused and fixed the fatal Windows `make -jN` (N>1) crash
  (`make.exe: /system/bin/mksh: Bad file descriptor`, then `Error 127`),
  tested against zlib per explicit direction ("libpng는 좀 크니, zlib를
  기반으로 시험을 해서 해결하는 방향으로 가자").** Every Windows port
  build had always run serial `make -j 1`; this was the first real
  attempt at `-jN` concurrency.
  - **Reproduced reliably**, first against the real zlib port build
    (`tools/crt-port-build.py --port zlib --jobs 8`, a new CLI flag added
    to the script specifically to make this testable without hand-editing
    the `jobs = 1 if target_os == "windows" ...` default), then reduced to
    a 2-line, 2-target Makefile (`a.o`/`b.o`, each running
    `/system/bin/mksh <script>`) that reproduces the exact same failure
    in under a second -- the first job always succeeds, every job after
    it fails.
  - **First bug found, a real and necessary co-fix but not the whole
    story**: with `--jobs 2` specifically (not higher), the build hung
    completely instead of crashing. `lldb` attached to the stuck
    `make.exe` showed its main thread blocked in `NtReadFile`, reached via
    `jobserver_setup() -> fcntl(F_SETFL) -> fstat() ->
    windows_handle_looks_executable() -> ReadFile()`: this project's own
    Windows `fstat()` unconditionally peeks a handle's first 2 bytes
    (looking for an `MZ`/`#!` executable signature) via
    `SetFilePointerEx`+`ReadFile`, even for a pipe -- GNU Make's own
    jobserver pipe, freshly created with only 1 byte of real data in it
    for `-j 2` specifically, meaning the 2-byte peek blocked forever with
    nothing left to ever write the second byte. Fixed (`__crt_sys_fstat()`,
    `libc/src/arch/windows/common/syscall.c`, now special-cases
    `GetFileType(handle) == FILE_TYPE_PIPE` the same way it already
    special-cased `FILE_TYPE_CHAR`, reporting `S_IFIFO` via a new
    `stat_virtual_pipe()` instead of ever touching
    `windows_handle_looks_executable()`) -- this changed the `-j 2`
    symptom from a hang into the same `Bad file descriptor`/`Error 127`
    crash every other `-jN` already showed, meaning a *second*, deeper bug
    was still there underneath it (found next) -- but, importantly, this
    fix was accidentally lost partway through the session (a `git
    checkout` used to strip temporary debug instrumentation reverted this
    real fix along with it, unnoticed until the `-j 2` hang reappeared
    during the follow-up investigation below) and had to be re-applied a
    second time; re-verified identically afterward.
  - **Traced the real crash with targeted, reverted `crtdbg_log()`
    instrumentation** (temporary; a per-PID-file WinAPI-level logger,
    since a first attempt sharing one log file across many concurrent
    processes silently lost lines to an unsynchronized concurrent-append
    race -- switching to `crtdbg_<pid>.log` files fixed that) added at
    each decision point in `__crt_sys_posix_spawn()`/
    `prepare_spawn_startup()`/`fd_snapshot_dup2()`. Found the exact
    failing call: GNU Make's own `-jN` (N>1) design gives only the first
    job "real" stdin; every job after it gets `posix_spawn_file_actions_
    adddup2(bad_stdin_fd, FD_STDIN)`, where `bad_stdin_fd` is a
    deliberately `FD_CLOEXEC`'d, already-EOF pipe fd it sets up once at
    startup (see GNU Make's own `job.c`, `child_execute_job()`) --
    completely ordinary POSIX practice, since `dup2()` never copies the
    source fd's `CLOEXEC` flag to the new fd. This project's
    `__crt_fd_snapshot_export()` (the function that captures "the current
    process's live fd table" for a spawn's file-action processing)
    unconditionally *skipped exporting any `FD_CLOEXEC` fd at all* --
    not just deciding it shouldn't be visible in the child by default,
    but making it invisible as a `dup2()` *source* during the very same
    spawn call that was about to redirect it. `fd_snapshot_dup2()` then
    correctly, but consequently, failed with `EBADF` looking for a source
    entry that was never captured -- `posix_spawn()` returned `EBADF`
    directly, and GNU Make's own `posix_spawn_child()` prints exactly
    `"%s: %s", argv[0], strerror(r)` on a negative pid, producing the
    observed `/system/bin/mksh: Bad file descriptor` verbatim.
  - **Root fix**: stopped skipping `FD_CLOEXEC` fds during export (so
    they remain valid `dup2()` sources), and instead track "should this
    survive into the child by default" via each snapshot entry's existing
    `CRT_FD_SNAPSHOT_FLAG_INHERITABLE` bit (now computed from `FD_CLOEXEC`
    for both file and socket kinds, where it was previously hardcoded
    per-kind and never actually consulted anywhere), consulted by a new
    check in `fd_snapshot_prepare_child_duplicates()` that drops
    (`fd_snapshot_remove()`) any non-inheritable entry right before the
    parent-to-child handle-duplication step -- so a `CLOEXEC` fd is still
    available as a lookup/dup2 source during setup, but never actually
    gets duplicated into the child unless something explicitly retargeted
    it first (matching real `CLOEXEC` semantics exactly). Net diff: 2
    small, targeted changes in `libc/src/arch/windows/common/syscall.c`
    (`__crt_fd_snapshot_export()`, `fd_snapshot_prepare_child_duplicates()`)
    -- all `crtdbg_log()` instrumentation and the standalone hang repro
    were reverted/discarded before landing, confirmed via a clean
    `git diff`.
  - **Verified three ways**: (1) the minimal 2-target repro now runs both
    jobs successfully (`running a` / `running b`, exit 0); (2) full
    `ctest` after rebuilding: 81/81, including the existing
    `windows_fd_snapshot_test`; (3) the real zlib port build with
    `--jobs 8`: every compile job runs, `libz.a`/`libz.so.1.3.1` build
    and install correctly, and the resulting shared library's own
    `examplesh` smoke test (compress/uncompress/gzread/inflate/
    inflateSync/dictionary round trips) passes end to end.
  - **Also fully resolved, same session, by the same `fstat()`/pipe fix
    above (re-applied after being accidentally lost, see above)**: the
    real `-j 8` zlib build kept printing
    `make.exe: INTERNAL: Exiting with 1 jobserver tokens available;
    should be 8!` once at the very end, even after the `EBADF` crash fix
    above made the rest of the build succeed -- a distinct-looking,
    non-fatal GNU Make jobserver token-accounting warning
    (`clean_jobserver()`/`jobserver_acquire_all()` in `src/main.c`/
    `src/posixos.c`: at exit, GNU Make drains its own jobserver pipe and
    compares the recovered token count against how many it started with).
    Initially assumed unrelated (confirmed none of zlib's compile jobs
    are recursive `$(MAKE)` invocations, so the jobserver pipe fds never
    go through this PAL's spawn/fd-inheritance path for this build) --
    but `jobserver_acquire_all()` itself calls `set_blocking(job_fds[0],
    1)` right before draining the pipe, which routes through the exact
    same `fcntl(F_SETFL) -> fstat() -> windows_handle_looks_executable()`
    path as the `-j 2` hang above, silently, destructively consuming real
    token bytes out of the pipe as a side effect of just checking its
    blocking mode. Once the `fstat()`/pipe fix above was re-applied, this
    warning disappeared completely -- confirmed with a fresh `-j 8` build
    (clean exit, no warning) and a `-j 16` build (also clean, after an
    unrelated transient Windows delete-pending file-lock error on the
    first attempt -- see the "Windows symlink/delete timing verification"
    thread in `TODO.md` -- cleared on immediate retry).
    `tools/crt-port-build.py`'s `jobs = 1 if target_os == "windows"
    and use_crt_shell` default is left in place pending a real, much
    larger `libpng`/`libffi`-scale `-jN` stress run (not yet tried) before
    flipping it; the script's own new `--jobs N` CLI flag makes opting
    into parallel builds for testing a one-line change instead of a
    hand-edit in the meantime.

- **Root-caused and fixed the real mksh bug found chasing the
  `sed: bad pattern` errors earlier.** Three rounds of investigation this
  session (the second explicitly re-opened per user request after the
  `crt_toybox` `fork_capable_relaunch.c` change, on the hypothesis that
  it might be related -- ruled out: the `sed: bad pattern` symptom
  already existed before that change, and re-testing after it changed
  nothing; the third round used real interactive `lldb` debugging to go
  from "precise minimal repro" to "confirmed exact buggy line").
  - **Found the missing piece**: libtool's own `func_execute_cmds()`
    (`ltmain.sh`, present verbatim in every generated `libtool` script)
    evaluates each stored `*_cmds` command *twice* -- once implicitly
    when the script itself was first parsed, and again explicitly via
    `eval cmd=\"$cmd\"` inside the function -- not once, as every earlier
    repro attempt this session (including the first sed-gap investigation
    earlier) had assumed. Reproducing *that exact two-eval shape* (not a
    single eval) was the key: a minimal script mimicking it byte-for-byte
    reproduces mksh's real corruption exactly, while a single-eval
    version never did, no matter the input.
  - **Ruled out the `DODBMAGIC`/`XSUBPAT` hypothesis from the first round
    with hard evidence**, not just inspection: added a temporary
    `fprintf` at `eval.c`'s `case XSUB:` (reverted before landing,
    working tree clean afterward), rebuilt `crt_mksh` alone, and ran the
    minimal repro against it directly. `f` was `0x4b` at every relevant
    call -- `DODBMAGIC` (`BIT(15)`, `0x8000`) was never set, and
    `sh.h`'s own comment confirms it's scoped to `[[ x = $y ]]`-style
    test expressions only (`exec.c`'s `dbteste_getopnd()`, its only call
    site) -- nowhere near a plain `eval cmd=\"$cmd\"` assignment. Also
    confirmed via the same debug output that the corruption is already
    present in `x.str` *before* `XSUB`'s own output-emission logic runs
    at all -- meaning the bug is in the **lexer** (tokenizing the
    `eval`'d assignment text), not in expansion/substitution as first
    suspected.
  - **Bisected to a precise, minimal trigger** by systematically reducing
    the real failing sed script down to single characters (each step
    re-tested against the real mksh binary): a `[...]` bracket expression
    appearing *after* a `\(...\)` backslash-group anywhere later in the
    same double-quoted string -- even outside the parens entirely --
    retroactively corrupts that earlier `\(`/`\)` (inserts a stray `/`
    right before it). A bracket expression appearing *before* any
    backslash-group in the same string never triggers it. Minimal
    reproducer (2 lines, no libtool/sed/nm involved at all):
    ```sh
    raw_cmds="'s/x\\\\([^ ]*\\\\)z/\\\\1/'"
    for cmd in $raw_cmds; do eval cmd=\"$cmd\"; echo "$cmd"; done
    # real bash: 's/x\([^ ]*\)z/\1/'      (correct)
    # this mksh: 's/x/\([^ ]*\)z/\1/'     (corrupted: stray '/' before '\(')
    ```
  - **`lex.c:283`'s `CMDASN`/array-subscript theory ruled back out** on
    closer reading: `case SDQUOTE:` (`shell/mksh/src/lex.c`) only special-
    cases the closing `"`, `goto Subst` for everything else, and
    `Subst:`'s own switch has an explicit `default: store_char:` tail
    (line 581) that just emits any unhandled character -- including `[`
    -- as a plain `CHAR` token, with zero special array-subscript
    handling. That code path (`case SBASE:`, `cf & CMDASN`) is simply
    never reached from inside a double-quoted string at all.
  - **Found the real mechanism by instrumenting `debunk()` directly**
    (temporary `fprintf` dumps of its input/output, MAGIC bytes rendered
    as `<M>`; reverted before landing, working tree clean afterward) and
    running it against the real minimal repro. The root cause traces back
    to `func_execute_cmds`'s own `eval cmd=\"$cmd\"` idiom itself: the
    `\"..\"` around `$cmd` are **backslash-escaped literal quote
    characters, not real quoting**, from the *outer* (pre-`eval`) shell's
    perspective -- so `$cmd` is substituted as a genuinely **unquoted**
    reference, subject to the same glob-pattern "magic" marking and
    field handling any bare `$var` would get. Confirmed directly: `[`,
    `]`, and `*` in `$cmd`'s value each get a `MAGIC` sentinel byte
    prefixed (`shell/mksh/src/eval.c`'s `case ORD('['):`/`case
    ORD('*'):` block around line 1109, gated on `f & (DOPAT|DOGLOB)`) --
    exactly the glob-metacharacter-protection mechanism real *quoted*
    text should never go through. The debug dump showed the word being
    `debunk()`-processed in **multiple separate calls** rather than once
    for the whole string -- each individual call correctly strips its own
    `MAGIC` bytes (e.g. `(<M>[^` -> `([^`, `<M>]<M>*\)z/\1/'"` ->
    `]*\)z/\1/'"`), but something in how these separately-processed
    fragments get **reassembled** into the final value is where the
    stray `/` actually enters -- that reassembly code itself is not yet
    located.
  - **Got real interactive debugging working, third round**: `lldb.exe`
    ships with this project's own LLVM install (`C:\Program Files\LLVM\
    bin\lldb.exe`) but crashed instantly (`unable to find 'python311.dll'`)
    -- the LLVM Windows package links `lldb` against Python 3.11
    specifically, but this machine's own Python install is 3.14. Fixed by
    adding the machine's already-installed (separate, from some earlier
    unrelated setup) `...\Programs\Python\Python311\` directory to `PATH`
    before invoking `lldb.exe` -- no reinstall needed. `lldb -b -s
    <command-file>` (batch mode, sourcing a plain-text command script)
    works well for this environment's non-interactive tool-call model;
    `breakpoint command add -o "cmd1" -o "cmd2" ...` (single-line `-o`
    flags) is required over the interactive `breakpoint command add`
    .../`DONE` block form, which silently produces no visible output
    through a sourced command file here. `crt_mksh` already builds with
    full debug info (`-g -gcodeview`, part of this preset's default
    flags) -- breakpoints resolve real source file/line locations
    directly, no extra CMake changes needed.
  - **First obstacle**: `crt_mksh` links `fork_capable_relaunch.c` (see
    that file's own docs) and self-relaunches into a *child* process on
    every invocation until ASLR mitigation is confirmed applied -- so a
    breakpoint set on the parent (the process `lldb` actually launches)
    never fires; all the real work happens in an un-debugged child lldb
    never attaches to. Worked around by temporarily `#if 0`-ing out
    `__crt_windows_ensure_fork_capable_relaunch()`'s body (a same-session,
    fully reverted change -- working tree was clean before and after;
    not a real fix, just a debugging aid, and specifically safe here
    since this repro never calls `fork()`).
  - **Traced the real call chain with a live breakpoint on `debunk()`**:
    it is *not* called directly from `emit_word:` for this repro -- the
    backtrace showed `debunk` <- `globit` <- `glob_str` <- `glob` <-
    `expand` <- `eval`. `f=0x4b` (`DOPAT|DOGLOB`-shaped) confirmed real
    filesystem **pathname globbing** (`glob()`) is being attempted on
    `$cmd`'s value -- consistent with the "not really quoted" theory
    earlier, since real quoted text is never glob-eligible.
  - **Found the literal fragmentation, not just inferred it**: breakpoints
    at `glob()`'s entry (`eval.c:1711`) and its "no real file matched,
    fall back to the literal text" branch (`eval.c:1716`,
    `XPput(*wp, debunk(cp, cp, strlen(cp) + 1))`) showed `glob()` being
    invoked **twice, independently**, for two disjoint halves of what
    should be one continuous string: `cp="cmd=\"'s/x\([^"` (ending mid-
    bracket-expression, right after `[^`) and a *separate* call with
    `cp="]*\)z/\1/'\""` (starting right after, from `]`) -- confirming
    the earlier fragmentation observation was a real word-level split,
    not just an internal `debunk()` buffering detail. The literal space
    character that should be inside `[^ ]` is present in *neither* half
    -- consumed as an ordinary (unquoted, from the lexer's perspective)
    IFS field separator, exactly like any bare `$var` with embedded
    spaces would be split, because the shell has no concept of "this
    space is inside a regex bracket expression" -- that protection is a
    sed/regex-level concept the shell's own byte-level field-splitter
    cannot see.
  - **Traced how the split fragments get reassembled, and ruled out
    where they *don't* get corrupted**: `func_execute_cmds`'s
    `eval cmd=\"$cmd\"` is not really an assignment from mksh's execution
    model's point of view -- it is the `eval` *builtin* (`funcs.c`'s
    `c_eval()`) receiving `cmd=\"$cmd\"` as its own (now field-split-into-
    two) argument list. `c_eval()` feeds its multiple argv words back in
    via a dedicated `SWORDS`/`SWORDSEP` source type
    (`shell/mksh/src/lex.c:1276-1289`) that re-lexes them as one
    continuous stream, inserting `T1space` between consecutive words --
    exactly POSIX's own "join eval's arguments with a space" behavior.
    Suspected `T1space` itself next (`shell/mksh/src/sh.h` has *two*
    conflicting `#define T1space` -- `" "` at line 1140 vs. a
    string-pool-offset `(Treal_sp2 + 5)` at line 974, gated on whether
    `HAVE_STRING_POOLING` survives a `#ifdef __GNUC__`/`#if __GNUC__ < 4`
    chain this project's `-DHAVE_STRING_POOLING=2` build flag interacts
    with in a not-immediately-obvious way) -- **ruled this out too**,
    empirically: a breakpoint right after the `s->str = T1space;`
    assignment (`lex.c:1287`) showed `s->str` was a genuine, correct
    single-space C string (`" "`) at runtime. The rejoin mechanism itself
    is completely correct.
  - **Confirmed the exact buggy line with a live breakpoint at
    `globit()`'s entry** (`shell/mksh/src/eval.c:1745`, using the lldb
    setup earlier): the corruption traces to `globit()`'s recursive
    path-component walk unconditionally re-inserting a **hardcoded
    canonical `/`** as the separator between reconstructed components
    (the old `if (xp > Xstring(*xs, xp)) *xp++ = '/';` at line 1808-1809)
    while a *separate* piece of the same function copies the *original*
    separator byte verbatim just later it (the `while (mksh_cdirsep(*sp))
    *xp++ = *sp++;` loop) -- for `x\([^ ]*\)z...`, `mksh_sdirsep()`
    (`sh.h`, `MKSH_CRT_WINPATH`-gated: `strpbrk(s, "/\\")`, i.e. `\` is
    *also* a recognized path separator, for legitimate Windows-pathname
    support) finds the `\` right after `x` and treats it as a component
    boundary; `globit()` NULs it, recurses, and on the way back down
    writes a **fresh canonical `/`** for "a boundary was here" -- but the
    *original* byte at that boundary was `\`, not `/`, so the
    reconstructed text ends up with the wrong separator character
    substituted in, corrupting a completely ordinary sed backslash-escape
    into `/\(`.
  - **This is a real, portable bug in `globit()` itself, not a
    Windows-only quirk** -- `mksh_sdirsep()`'s non-`MKSH_CRT_WINPATH`
    (POSIX) definition is `strchr(s, '/')`, so vanilla upstream mksh runs
    through the *exact same* hardcoded-`/`-reinsertion code whenever a
    non-pathname string (like this sed script, which contains real `/`
    delimiters) gets routed into `glob()` -- POSIX unquoted-parameter-
    expansion rules do this legitimately for any bare `$var`, which is
    exactly what `\"$var\"` inside `eval` amounts to (the escaped quotes
    are literal data, not real quoting, so `$cmd` is genuinely unquoted
    and glob-eligible -- confirmed in the round earlier). It is invisible
    on POSIX purely because the hardcoded replacement (`/`) always
    happens to equal the original byte (`/`) there -- a silent no-op
    corruption. `MKSH_CRT_WINPATH` recognizing `\` as *also* a separator
    is what turns this from a latent, byte-identical no-op into a visible
    corruption, by making "original separator" and "hardcoded
    replacement" diverge for the first time.
  - **Root fix implemented in `shell/mksh/src/eval.c`**: threaded the
    actual separator byte through `globit()`'s recursion instead of
    hardcoding `/`. Added a `char dirsep` parameter to `globit()` (and its
    forward declaration); `glob_str()`'s top-level call passes `'/'` (a
    harmless default -- `xp` is always empty on that very first call, so
    the "insert a separator" branch can never fire yet); both of
    `globit()`'s two recursive call sites (the non-globbing debunk-and-
    recurse path, and the real `opendir()`/`readdir()` match path) now
    pass `odirsep` -- the exact separator byte that was just consumed
    from `sp` a few lines earlier in the same stack frame, previously
    computed but never threaded any further than that frame's own local
    variable. `*xp++ = '/'` became `*xp++ = dirsep`. On POSIX this is a
    provable no-op (`dirsep` is always `'/'` there, matching the old
    hardcoded value byte-for-byte); on `MKSH_CRT_WINPATH` it now
    reconstructs the *original* separator faithfully instead of silently
    canonicalizing it, so backslash-escape sequences are no longer
    corrupted.
  - **Verified three ways**: (1) the minimal repro now matches real bash
    exactly -- `'s/x\([^ ]*\)z/\1/'` in, byte-identical out, no stray `/`;
    (2) full `ctest` after rebuilding `crt_mksh`: 81/81 passed, no
    regressions; (3) a real `libpng` port build (`tools/crt-port-build.py
    libpng`) produced **zero** `sed: bad pattern` errors anywhere in its
    output, confirming the fix holds under the actual libtool
    `func_execute_cmds` workload that originally surfaced this, not just
    the isolated repro.
  - Was already confirmed harmless for libpng specifically even before
    the fix (the DLL still builds via `__declspec(dllexport)` markers
    regardless of the corrupted export-symbol-list script), but this was
    a real, general mksh correctness bug: `\"$var\"` (escaped-quotes, not
    real quoting) around any glob-metacharacter-containing variable,
    evaluated via `eval`, is exactly the shape GNU Autoconf/Libtool's own
    `func_execute_cmds`/`func_quote_for_eval`-family helpers use
    throughout every generated `configure`/`libtool` script -- so libpng
    was just the first place this session's real build activity happened
    to exercise it with the right ingredients (both a backslash-group and
    a later bracket in the same value). Fixed at the root rather than
    worked around specifically so that a future macOS/Linux mksh build
    (using the same vendored `shell/mksh/src/eval.c`, just without
    `MKSH_CRT_WINPATH` defined) inherits the fix automatically instead of
    carrying the same latent, currently-invisible bug forward.

- **Enabled toybox's `id`/`xargs` applets the same way (same `newtoys.h`/rootfs-alias gap as `which`/`readlink`/`stat` earlier), found and fixed two more real bugs, and used the second one to close out this session's `libpng` cleanliness pass end to end.** Found while reviewing a real libpng `configure` log the user reported as "very clean now": `checking xargs -n works` and `checking whether UID '...' is supported` both fell back ungracefully (`xargs`/`id: inaccessible or not found`) for the identical reason as the earlier applets -- source compiled into `CRT_TOYBOX_SOURCES` but never registered in `newtoys.h` or aliased in `tools/create_rootfs.py`'s `TOYBOX_APPLETS`. Added `USE_ID`/`USE_XARGS` (matching their real upstream `NEWTOY` signatures) and the two rootfs aliases; both `CFG_ID`/`CFG_XARGS` were already enabled in the base Android config.
  - **Found via `id`: `getpwuid(geteuid())` always failed with "bad uid 1".** `libc/src/arch/windows/common/syscall.c`'s `__crt_sys_geteuid()` hardcoded `return 1`, but `libc/src/user_group.c`'s synthetic passwd/group database (the "shell" user this whole PAL's rootfs is built around) only ever recognized uid **0** -- a genuine, long-standing mismatch between two pieces of this project's own code that nothing had exercised until `id`'s `getpwuid()` lookup actually needed them to agree. Fixed by changing `__crt_sys_geteuid()` to return 0, matching the synthetic passwd entry instead of the other way around (no code anywhere depended on the value being specifically 1 -- checked all `tests/*.c` usages, which only assert `geteuid() != (uid_t)-1` and internal self-consistency with `st_uid`, both unaffected by the actual value). This also changes every `stat()`-reported file's `st_uid` from 1 to 0, consistent with `id`'s own new `uid=0(shell)` output.
  - **Found via `xargs`: fork()-crashed every single invocation** (`fork_memcopy: stack commit failed`) -- the exact ASLR-mitigation self-relaunch requirement this session's earlier work (and the original Windows `fork()` effort, `docs/windows_fork_emulation.md`) already established: only binaries that link `fork_capable_relaunch.c` and opt in at startup can call `fork()`/`vfork()` on this PAL. `crt_toybox` (`shell/CMakeLists.txt`) never did -- only `crt_mksh` and the `ctest` suite did -- so *any* toybox applet that forks (today just `xargs`; potentially others later) was silently guaranteed to crash. Fixed by adding the identical conditional `fork_capable_relaunch.c` source to `crt_toybox` that `crt_mksh` already has (same Windows x86_64/aarch64 guard).
  - **Verified end-to-end**: rebuilt the full preset; `id` now prints `uid=0(shell) gid=0(shell) groups=0(shell)`, `xargs` runs real commands correctly (`echo "hello world" | xargs echo prefix:` -> `prefix: hello world`). Full `ctest` 81/81 after each change. Re-ran the real libpng `configure` afterward: `checking xargs -n works... yes`, `checking whether UID '0' is supported by ustar format... yes` -- both probes that used to degrade ungracefully now pass cleanly.
  - **Also surfaced, investigated, and left open**: two `sed: bad pattern` errors during libpng's DLL link step (libtool's own `nm`-output-parsing script), confirmed non-fatal (the DLL still builds via libpng's `__declspec(dllexport)` markers, and every consumer links fine regardless). Root-caused as far as time allowed: an isolated, reproducible test proved this project's own `mksh` over-collapses backslash pairs inside double-quoted strings compared to real bash (`"\\\\("` should keep 2 backslashes per POSIX, this mksh keeps only 1), a genuine mksh double-quote/`eval` bug -- but two separate attempts to intercept the *actual* failing `sed` invocation with a debug argv-logging wrapper both failed to reproduce the exact failure context (the wrapper never got invoked, once because `crt-port-build.py` always rebuilds the `rootfs` target first and silently overwrote the wrapper, once because a nested `mksh ./libtool` sub-process couldn't resolve the wrapper's own `#!/system/bin/mksh` shebang for a still-unexplained reason). Genuinely unresolved which exact code path produces the observed corruption; tracked under "in progressing" later rather than claimed as fixed.
  - **Unrelated, but confirmed while re-running libpng repeatedly this session**: a real, reproducible-once `make install` `Error 5` (Windows raw `ERROR_ACCESS_DENIED`, no error text) on `install-binSCRIPTS`, matching the same unexplained-`Error 5` pattern seen earlier this session on a different target (`install-man5`, aarch64, see the `libpng` shared-pass entry's own notes). Did **not** reproduce on an immediate retry. The user separately applied the Windows Defender process/folder exclusions this session's `README.md` update (later) documents, and the retry after that ran clean -- suggestive, not conclusive (one data point, and several other rebuilds were running concurrently in the background at the time confounding any timing comparison), but consistent with the working theory that these sporadic `Error N`-with-no-message failures are Defender real-time-scan file-handle contention, not a real toybox/CRT bug.

- **Documented Windows Defender build-performance exclusions in `README.md`.** The user ran a real build with process exclusions (`cmake.exe`/`ninja.exe`/`clang.exe`/`clang++.exe`/`clang-cl.exe`/`lld.exe`/`llvm-nm.exe`) and folder exclusions (the LLVM install dir and the whole project tree) applied via `Add-MpPreference` from an elevated PowerShell session. Added the exact commands to the `### Windows 11` prerequisites section, with a one-line rationale (real-time scanning inspects every file a build writes, which is significant given how many small object files, port-build artifacts, and rootfs entries a full build and porting-loop run produce). Separately noted for the record (not applied, the user's own security posture call): the user's machine already had Cloud-delivered protection, Automatic sample submission, and Tamper Protection all off, which combined with these exclusions means (a) process exclusions are broader than folder exclusions -- they skip scanning anything the named binaries touch, anywhere, not just inside the project tree -- and (b) with Tamper Protection off, the exclusion list itself isn't protected from being extended by anything else that gains code execution on the machine.

## 2026-08-10

- **Actually enabled toybox's `which`/`readlink`/`stat` applets (source had been compiled in for a while, but nothing had registered or exercised them as real, runnable commands) and fixed two genuine bugs found doing it.** `shell/CMakeLists.txt`'s `CRT_TOYBOX_SOURCES` already listed `which.c`/`readlink.c`/`stat.c`, but toybox's own applet-dispatch table (`shell/toybox/crt/generated/newtoys.h`, a hand-curated subset of upstream toybox's full catalog -- see that file) never had `USE_WHICH`/`USE_READLINK`/`USE_STAT` entries, and `tools/create_rootfs.py`'s `TOYBOX_APPLETS` rootfs-alias list didn't have them either, so `toybox which ...` genuinely failed with `Unknown command which` and no rootfs `/system/bin/which` existed at all -- confirmed directly before touching anything. Added the three `USE_*(NEWTOY(...))` lines (matching each applet's own upstream `NEWTOY` signature exactly) and the three rootfs aliases; the base Android config these CRT-local files layer on top of (`shell/toybox/src/android/linux/generated/config.h`) already had `CFG_WHICH`/`CFG_READLINK`/`CFG_STAT` enabled, so no config changes were needed there.
  - **Found via `which` itself: `getcwd(NULL, 0)` returned `EINVAL`.** `which.c` calls toybox's own `xgetcwd()` (`shell/toybox/src/lib/xwrap.c`), which is exactly `getcwd(NULL, 0)` -- the real POSIX.1-2008/GNU extension ("allocate a buffer as large as necessary automatically"), which Android Bionic's own `getcwd()` implements too. `libc/src/fd.c`'s `getcwd()` had never implemented this at all, rejecting *any* NULL `buf` outright regardless of `size` as `EINVAL`. Fixed per Bionic's own approach (no grow-and-retry loop like glibc; a single `malloc(size ? size : PATH_MAX)` then one real `getcwd()` call into it): split the existing body into a `getcwd_into()` helper reused by both the caller-supplied-buffer path and a new malloc'd-buffer path. `buf != 0 && size == 0` is still the genuine `EINVAL` case (a real buffer with no usable capacity). Added a regression case to `tests/file_path_test.c` (`getcwd(NULL, 0)` matches the known-good `getcwd(buf, size)` result; `getcwd(buf, 0)` still fails `EINVAL`).
  - **Found via `readlink` itself: silently failed (exit 1, no error text) on every real symlink whose target didn't fit in toybox's own 64-byte starting buffer.** `readlink`'s default mode runs quiet-on-error (upstream toybox behavior, matching real coreutils -- not a bug), which made this look like nothing happened at first. Root cause: `__crt_sys_readlink()` (`libc/src/arch/windows/common/syscall.c`) converted the reparse point's wide-char target directly into the caller's own `size`-byte buffer via `WideCharToMultiByte()` -- which *fails outright* (returns 0, `ERROR_INSUFFICIENT_BUFFER`) when the target doesn't fit, rather than truncating. Real POSIX `readlink(2)` never fails for that reason; it silently truncates to `size` bytes and returns however much it wrote (which can legitimately equal `size`, the caller's own signal to retry bigger) -- exactly the growth-loop toybox's `xreadlinkat()` (`shell/toybox/src/lib/xwrap.c`, starts at 64 bytes and doubles) depends on, and exactly what this project's own raw Linux `readlink(2)` passthrough already provides for free. Since every real absolute path in this rootfs is well over 64 bytes, the very first growth-loop iteration always hit this and gave up immediately. Fixed by converting into an unbounded temporary buffer first (`MAXIMUM_REPARSE_DATA_BUFFER_SIZE` bytes, always enough headroom), then copying/truncating into the caller's real buffer and returning the true (possibly-larger-than-`size`-clamped-to-`size`) length -- matching real `readlink(2)` truncation semantics.
  - **Verified end-to-end, not just compiled**: rebuilt the full `windows-host-ninja-debug` preset; `which ls` now prints `/system/bin/ls`, `stat README.txt` prints real file metadata, and `readlink` on a real symlink (whose target is a long, rootfs-translated absolute path -- confirmed too long for the old 64-byte first attempt) now succeeds instead of silently failing. Added a permanent regression test, `crt_mksh_rootfs_which_stat_readlink_runs` (`shell/CMakeLists.txt`, same Windows-only `crt_mksh_rootfs_*` chain as the existing external/pipeline/redirection/command-substitution/exec-builtin tests), exercising all three applets together through the real rootfs mksh. Full `ctest` 81/81 (79 pre-existing + `pselect_sigchld_test_runs` + this new test).
  - General CRT/PAL fixes, not toybox-specific: any other program calling `getcwd(NULL, 0)` or `readlink()` on a long target on Windows was equally affected.

- **Implemented real `SIGCHLD` delivery for Windows, replacing the honest
  no-op stub.** The user asked for the stub to actually be implemented
  rather than left as documented-but-unfixed. Windows has no kernel
  mechanism for an async child-exit signal the way Linux/macOS do, but does
  have everything needed for a real, synchronously-polled equivalent,
  reusing state that already exists for `waitpid()`: a live child's process
  `HANDLE` (already tracked in `syscall.c`'s child registry) becomes
  kernel-signaled the moment it exits. Added
  `__crt_windows_check_sigchld_pending()` (`libc/src/arch/windows/common/
  syscall.c`): a cheap, non-blocking scan of that registry
  (`WaitForSingleObject(handle, 0)` per live child), gated on `SIGCHLD`
  being currently unblocked, marking each observed exit in a new parallel
  `child_notified_table` so it is reported exactly once (edge-triggered,
  matching real `SIGCHLD`). Two call sites, matching the two points a real
  kernel would actually deliver: `__crt_signal_backend_set_mask()`
  (`signal_backend.c`) -- delivers an already-pending exit synchronously the
  moment `sigprocmask()` unblocks `SIGCHLD`, which is *also* exactly what
  `pselect()`'s existing atomicity check depends on, so `poll.c` itself
  needed zero Windows-specific changes -- and `__crt_sys_poll()`'s own
  1ms-`Sleep()` busy-wait loop (`syscall.c`), covering a child that exits
  while genuinely blocked in `pselect()`/`select()`/`poll()` rather than
  having already exited beforehand. Both call sites deliver synchronously
  on whichever thread is already running, never from a new background
  thread, so no new locking was needed anywhere. Scope: covers `pselect()`/
  `select()`/`poll()` interruption only (the case that actually motivated
  this backend interface, GNU make's `jobserver_acquire()`) -- does not
  cover interrupting a plain blocking `read()`/`write()`/etc, which would
  need a much larger overlapped-I/O rework; every signal other than
  `SIGCHLD` stays exactly as before (pure software bookkeeping). Updated
  `tests/pselect_sigchld_test.c` to drop its Windows special case entirely
  -- it now asserts the same fast-`EINTR` behavior on every host. See
  `docs/signal_delivery.md`'s rewritten "Windows" section for the full
  design writeup.
  - **Verification status: confirmed on a real Windows host.** Rebuilt the
    full `windows-host-ninja-debug` preset (`cmake --build --preset
    windows-host-ninja-debug`) and ran the full suite: `ctest` 80/80 (was
    79 before this test was added), with `pselect_sigchld_test_runs`
    itself completing in **0.23s** -- the same fast-`EINTR`-wakeup path
    already confirmed on Linux (~0.2s) and macOS (0.21s), not the
    5s-bounded-timeout fallback the old no-op stub would have hit. This
    independently confirms the real, polled `SIGCHLD` mechanism actually
    fires on Windows, not just "doesn't crash."
  - **Went further and tested the actual motivating real-world scenario**
    (matching the Linux/macOS verification style earlier): temporarily
    overrode `tools/crt-port-build.py`'s hardcoded `jobs = 1 if
    target_os == "windows" ...` restriction (a local, reverted-immediately
    test patch, not a real change) and reran a real port build
    (`zlib`, `./configure && make -j 8 && make install`) with genuine
    parallel jobs on Windows for the first time. **Found a second, separate
    bug this uncovers**: `make.exe: /system/bin/mksh: Bad file descriptor`
    followed by `make.exe: INTERNAL: Exiting with 1 jobserver tokens
    available; should be 8!` -- GNU Make's own process-spawn failure
    message when creating a *concurrent* recipe shell fails, not a
    `pselect()`/`SIGCHLD` symptom at all (that mechanism is confirmed
    working correctly by the regression test earlier). Points at a race or
    gap in this Windows PAL's own concurrent process-spawn/fd-inheritance
    path (plausibly `child_process_table`/`CRT_FD_TABLE_SIZE` bookkeeping,
    or jobserver-pipe fd duplication, under two-or-more near-simultaneous
    spawns) that has never been exercised before, since every Windows port
    build has always run with `-j 1`. **Not root-caused or fixed this
    session** -- reverted the test patch immediately, rebuilt `zlib`
    normally (`-j 1`) to restore a known-good state, and reran full `ctest`
    (80/80, clean) to confirm no residual corruption from the failed
    parallel attempt. The `jobs = 1 if target_os == "windows"` restriction
    in `tools/crt-port-build.py` **must stay in place** until this new bug
    is separately root-caused -- the `SIGCHLD` fix alone was necessary but
    not sufficient to make real Windows parallel builds safe. See the new
    "in progressing" entry later.

- **Verified the Linux signal backend (`docs/signal_delivery.md`) on a real
  Linux aarch64 host, and added the permanent `fork()` + blocked-`SIGCHLD` +
  `pselect()` regression test.** Both were open "in progressing" items
  blocked on actual Linux hardware being available; this session had one.
  Full `ctest` 74/74 via `cmake --build --preset linux-host-ninja-debug` +
  `ctest --preset linux-host-ninja-debug`. Also ran the real motivating
  scenario end to end: `port-rebuild-zlib`'s `./configure && make -j 4 &&
  make install` (this host has 4 cores, so `crt-port-build.py` picks `-j 4`
  rather than the macOS repro's `-j 10`, same jobserver/`pselect()` path)
  completed cleanly, `ldd` on the resulting `libz.so.1.3.1` resolved
  `libc.so`/`libm.so`/`libdl.so`/`libc++.so` to this project's own sysroot,
  and `examplesh`'s real compress/uncompress round trip passed. Added
  `tests/pselect_sigchld_test.c` (registered as `pselect_sigchld_test_runs`
  in `tests/CMakeLists.txt`, `TIMEOUT 30` outer safety net): installs a real
  `SIGCHLD` handler, blocks `SIGCHLD`, forks a child that exits immediately,
  sleeps briefly so the kernel queues the now-pending signal, then calls
  `pselect()` (unblocking `SIGCHLD`) against a pipe kept deliberately
  unreadable, with a bounded 5s timeout; asserts `-1`/`EINTR` in well under
  2s on Linux/macOS, and the honest bounded-timeout behavior on Windows
  (no-op signal backend, see doc). Verified the test actually exercises the
  fix, not just passes vacuously: temporarily disabled the `pselect()`
  atomicity check in `libc/src/poll.c` and confirmed the test then blocked
  for its full 5s timeout and failed, before reverting. See
  `docs/signal_delivery.md`'s new "Linux Verification"/"Regression Test"
  sections for the full writeup.
  - **Update: confirmed on macOS too.** The user ran the full suite on a
    real macOS machine after pulling this change: `ctest --preset
    macos-host-ninja-debug` -- 74/74 passing, `pselect_sigchld_test_runs`
    itself in 0.21s (the fast `EINTR`-wakeup path, same as Linux), so the
    new test exercises macOS's real `sigaction`/`sigprocmask` backend
    correctly too, not just Linux's.

- **Root-caused and fixed the recurring `libtool: error: Could not determine host file/path name corresponding to ... Continuing, but uninstalled executables may not work.` warnings during every libpng `make`/`make install`, and a real, silent path-corruption bug they were masking.** Traced through the generated `configure`/`libtool` scripts, not guessed: `case $host in *-*-mingw* ) case $build in *-*-mingw*|*-*-windows* ) # actually msys -> lt_cv_to_host_file_cmd=func_convert_file_msys_to_w32`. Autoconf's own authors used "`$build` *also* looks like mingw/windows" purely as a historical proxy for "`configure` is running inside a real MSYS2 shell" -- true for every toolchain they anticipated, but not for this project's own from-scratch `mksh`/toybox PAL, which is a genuinely native `$build` with no MSYS/Cygwin runtime underneath at all. `func_convert_file_msys_to_w32`'s real implementation shells out to a literal `cmd //c echo ...`, relying on real MSYS's own automatic POSIX-argv-to-Windows-path translation (a feature of `msys-2.0.dll`'s `exec()` layer, not of `cmd.exe` itself) -- confirmed directly that this project's rootfs `$PATH` (deliberately scoped to just its own sysroot bin dirs) can never reach a real `cmd.exe` (`mksh.exe: cmd: inaccessible or not found`), so the "conversion" always returns empty, and libtool falls back to its own documented "deliberately simplistic" recovery: a blind `s/:/;/g` on the *original*, already-host-native string -- which corrupts every path this project uses (`C:/Users/...`), since the drive-letter colon isn't a path-list separator the way a real POSIX build's colons would be. Confirmed directly in a real generated wrapper: `LIB_PATH_VALUE` had become `"C;/Users/..."` -- silently invalid, not merely a cosmetic warning.
  - **Fixed the standard, sanctioned way, not a script patch**: `lt_cv_to_host_file_cmd` and its sibling `lt_cv_to_tool_file_cmd` are both ordinary autoconf cache variables (the same `${VAR+y}` idiom already exploited for `$LD`/`$DLLTOOL`/`$OBJDUMP`/`$NM`/`lt_cv_deplibs_check_method` skips detection entirely when pre-set). Preset both to `func_convert_file_noop` -- libtool's own built-in "paths are already in host format, nothing to convert" case, the exact value a real non-mingw/non-cygwin host already gets in that same `case` statement's "otherwise" branch, so this isn't an invented value, just the value libtool itself uses for hosts that were never MSYS to begin with. `to_host_path_cmd` has no cache variable of its own -- it's derived at runtime from `to_host_file_cmd` by libtool's own `func_init_to_host_path_cmd`, so fixing the file variant fixes the path-list variant too. Generalized directly into `tools/crt-port-build.py`'s `make_env()` (alongside `lt_cv_deplibs_check_method`), not left recipe-local, since it's a fixed, permanent fact about this toolchain, true for any Windows configure-based recipe going through GNU Libtool.
  - **Deliberately scoped, verified not to touch DLL/EXE generation**: this `$build`-keyed sub-decision lives entirely *inside* the outer `case $host in *-*-mingw* )` branch that governs `archive_cmds`/import-lib naming/`-DDLL_EXPORT`/shared-library detection (this session's whole earlier libpng/libffi shared-build fix chain) -- the outer branch, keyed only on `$host`, is completely untouched by this fix.
  - **Verified end-to-end**: warning count dropped to 0 across a full rebuild (was ~27); re-inspected a real generated wrapper's `LIB_PATH_VALUE`, now correctly `"C:/Users/.../.libs:"` with the drive letter intact; ran the freshly-built `pnggetset.exe` directly (a wrapper whose `EXE_PATH_VALUE` has 3 colon-joined directory entries, the most exercised multi-entry case) -- all 6 of its PLTE/hIST/tRNS/tEXt/sPLT/unknown-chunks get-then-set roundtrip subtests `PASS`, `rc=0`. Full `ctest` 79/79.
  - Full writeup: `porting/recipes/libpng.json`'s own notes.

- **Fixed the remaining `:`-vs-`;` `PATH`-separator gap the `func_convert_file_noop` fix earlier left behind, and made it a general, reusable piece for any future port.** `LIB_PATH_VALUE`/`EXE_PATH_VALUE` stayed `:`-joined even after that fix (correct drive letters, wrong list separator for the real Windows `PATH` env var these strings eventually feed). None of libtool's own built-in `to_host_path_cmd` implementations do "leave paths alone, just rejoin with `;`" -- the one shape this toolchain actually needs -- and injecting a *new* one would mean hand-patching the generated `libtool` script, which is off the table by this project's own no-upstream-patching discipline. The wrapper's own `lt_update_lib_path()`/`lt_update_exe_path()` (ltmain.sh's generated code) do nothing more than blind string concatenation onto `getenv("PATH")`, so the corruption survives all the way into this project's own `environ` by the time `_spawnv()` is about to actually launch the target program.
  - **Fixed at the one point in this whole chain that's genuinely this project's own code**: `_spawnv()` itself, in `porting/shims/win32/libtool_wrapper_compat.h` (the same shared, `force_include`-driven shim already used for the `_getcwd`/`_stat`/etc. fix). Added `_crt_libtool_wrapper_fix_path_env()`/`_crt_libtool_wrapper_fix_path_seps()`: right before spawning, rewrite `PATH`'s list separators from `:` to `;`, leaving every drive-letter colon untouched. Not a heuristic: Windows filesystem rules forbid `:` from appearing anywhere in a real path except the drive-letter position, so any `:` that isn't in exactly that position is unambiguously a stray list separator.
  - **Verified directly, not inferred**: a standalone test (a fake helper process that prints back its own inherited `PATH`) confirmed a parent's `:`-joined value (`C:/Users/.../lib:C:/Users/.../bin:C:/Users/.../.libs:/system/bin:/bin:/usr/bin`) arrives at the real spawned child correctly rewritten to `C:/Users/.../lib;C:/Users/.../bin;C:/Users/.../.libs;/system/bin;/bin;/usr/bin` -- every drive letter intact, every list separator now real. Re-ran the full libpng rebuild afterward (0 errors, 0 warnings) and both `pngtest.exe` (`libpng passes test`, `rc=0`) and `pnggetset.exe` (all 6 subtests `PASS`, `rc=0`) end to end. Full `ctest` 79/79.
  - **Already generalized, not libpng-specific**: since the fix lives inside `libtool_wrapper_compat.h` itself (the shared shim, opted into per-recipe via the existing `force_include` mechanism in `tools/crt-port-build.py`'s `apply_recipe_env()`), any future Windows configure-based recipe that hits the same `.libs/lt-*.c`-wrapper class of bug gets this fix automatically the moment it adds the same one-line `force_include` entry to its own `target_overrides.windows` -- no new wiring needed, same as the `_getcwd`/`_stat`/`_chmod`/`_putenv`/`_setmode`/`_spawnv` declarations it already provides.
  - Full writeup: `porting/recipes/libpng.json`'s own notes.

- **Root-caused and fixed libpng's Windows shared-library build for real (`static-pass` -> `shared-pass`), instead of the documented-but-abandoned earlier attempt.** Per explicit direction to fix the actual root cause rather than patch around it (a generated-`libtool`-script post-processing shortcut was considered and rejected in favor of this). Four independent, genuine bugs found and fixed in order, each verified against the real `configure && make && make install` flow and the full `ctest` suite (79/79 throughout):
  1. **The real reason libtool always concluded "no shared-library support," even after every other GNU-ld/GNU-C detection probe was already passing**: `tools/crt-cc`'s actual Windows linker backend was never `lld-link.exe` in MSVC-compatible mode, as earlier session notes assumed -- verified directly via `crt-cc -shared -v`, the real invocation is `ld.lld -m i386pep`/`-m arm64pe`, a genuine, capable GNU/MinGW-compatible personality (confirmed: `ld.lld -m i386pep --help` lists real `--enable-auto-import` support). But libtool's own `cygwin*|mingw*` archive_cmds branch gates behind a literal `$LD --help 2>&1 | grep 'auto-import'` probe, and a *bare* `$LD --help` (no `-m` flag) falls back to `ld.lld`'s generic ELF frontend, whose help text lists PowerPC options, not PE ones -- `auto-import` is never there. Fixed at the root in `tools/crt-native-tool`: when the wrapped tool is `ld.lld` (matched case-insensitively, and matching both the real filename and the NTFS 8.3 short-path alias `tools/crt-port-build.py`'s `windows_short_path()` substitutes whenever the real path contains spaces, e.g. `LDLLD~1.EXE` -- an earlier version of this fix matched only the long filename and silently never took effect through the real pipeline, caught by re-testing through the exact same short-path invocation configure actually uses), always inject the right `-m` emulation up front. This made `checking whether ... supports shared libraries`/`checking if libtool supports shared libraries` resolve `yes` for the first time ever in this project.
  2. **New blocker exposed once shared compilation was reached**: `unknown type name 'PNG_DLL_EXPORT'`. libtool always compiles a second, `-DPIC -DDLL_EXPORT` pass for the shared variant; `pngpriv.h`'s own, upstream-documented mechanism (`#ifdef DLL_EXPORT -> PNG_BUILD_DLL -> PNG_IMPEXP=PNG_DLL_EXPORT`) then needed `PNG_DLL_EXPORT`/`PNG_DLL_IMPORT`, which `pngconf.h` only defines inside its own `#if defined(_WIN32) || ... || defined(__CYGWIN__)` block -- exactly what this recipe's CFLAGS deliberately undefine (to keep libpng off the `#include <windows.h>` path this sysroot doesn't have). Fixed via the command-line override `pngpriv.h` itself documents as sanctioned ("the builder of the library may set this on the command line"): added `-DPNG_DLL_EXPORT=__declspec(dllexport) -DPNG_DLL_IMPORT=__declspec(dllimport)` to the recipe's Windows CFLAGS.
  3. **Next blocker**: `fatal error: 'malloc.h' file not found`, in libtool's own auto-generated `.libs/lt-*.c` wrapper sources (used to run an uninstalled test/contrib executable against a not-yet-installed shared library) -- not part of libpng itself, and this sysroot never had a `<malloc.h>` at all. Added `include/malloc.h` following Android Bionic's own convention exactly (`bionic/libc/include/malloc.h`): a thin compatibility header that just re-exports `<stdlib.h>`'s malloc family for source compatibility with pre-POSIX code that expects it from the older header location; deliberately declares no allocator-extension symbols (`malloc_usable_size()`, `memalign()`, ...) since this libc doesn't implement them yet.
  4. **Final blocker**: even with linker-family detection fixed, libtool still refused to link `libpng16` as a DLL against zlib's already-built shared library specifically, silently falling back to static-only with the warning "you do not appear to have [a shared version], ... none of the candidates passed a file format test using a file magic." Root cause: libtool's `deplibs_check_method='file_magic file format (pei*-i386(.*architecture: i386)?|pe-arm-wince|pe-x86-64|pe-aarch64)'` with `file_magic_cmd='$OBJDUMP -f'` never matches this toolchain's real output -- `llvm-objdump -f` reports `file format coff-x86-64` for this project's own real `.so`/`.dll` files, not GNU objdump's `pe-x86-64` the hardcoded regex expects (a genuine LLVM-vs-GNU-binutils naming difference, confirmed by running `llvm-objdump -f` on the real installed `libz.so.1.3.1` directly). Since the regex lives inside the generated `configure`/`libtool` scripts (not reachable via a recipe CFLAGS override), fixed the standard, portable way real cross-toolchain builds already handle exactly this situation: `lt_cv_deplibs_check_method=pass_all` as a recipe env var -- autoconf's `${VAR+set}` cache-variable idiom (confirmed by reading the actual generated `configure`'s own `if test ${lt_cv_deplibs_check_method+y} then :; (cached) ... else ...` logic) honors a pre-set environment variable and skips the whole `case $host_os` detection block entirely, the exact same mechanism `$LD`/`$DLLTOOL`/`$OBJDUMP` already rely on elsewhere in this project.
  - **Verified end-to-end, not just "it built"**: a standalone test program (compiled via `crt-cc`, linked against the installed `libpng16.dll.a` import lib + `libz`) dynamically loaded the real `libpng16-16.dll` and successfully called `png_access_version_number()`/`png_get_header_ver()` (both correctly returned `1.6.57`/`10657`), then completed a real `png_create_write_struct()`/`png_create_info_struct()`/`png_destroy_write_struct()` round trip through the DLL.
  - **One more thing found and worked around along the way, real but deliberately not chased further this session:**
    - `make install`'s libtool-generated `install-header-links`/`install-exec-hook` steps (`rm -f X; ln -s Y X` for each top-level header/lib alias) intermittently failed with `ln: ... File exists` on a rebuild, specifically when `X` was already a valid symlink from a prior successful install in the same session. An isolated, minimal `rm -f <symlink>; ln -s` repro via the exact same rootfs mksh + toybox `rm` succeeded cleanly every time, so this looks like Windows delete-pending/handle-timing noise from repeated same-session rebuilds rather than a general toybox/CRT `rm`-on-symlink bug; worked around this session by fully wiping the port's own install-directory footprint before the final clean rebuild. Worth a closer look if it reproduces from a genuinely cold `out/` directory.
  - Full writeup, including the exact `crt-cc -shared -v` transcript that found the real linker identity and the `ld.lld -m i386pep --help` vs. bare `ld.lld --help` comparison that found the detection gap: `porting/recipes/libpng.json`'s own notes.

- **Fixed a real libpng build-time `libtool: syntax error: unexpected '|'` (an empty `$global_symbol_pipe`, i.e. `nm ... |  | sed ...` with nothing between two pipes).** Root-caused via `config.log`: `NM='nm'`, never resolved past the literal, unusable default -- `checking command to parse ... nm output` had been failing since the very first configure log read this session, just not investigated until now. Unlike `$LD`/`$DLLTOOL`/`$OBJDUMP`, `tools/crt-port-build.py`'s `make_env()` never preset `$NM` to a real tool at all. Fixed the same way as those three: preset `$NM` to `llvm-nm.exe` (via `find_windows_host_tool()`) and added `NM` to the `crt-native-tool` wrapping loop (`AR`/`RANLIB`/`STRIP`/`LD`/`DLLTOOL`/`OBJDUMP`/`NM`). Verified: `checking command to parse ... nm output... ok`; full libpng rebuild's `syntax error` count dropped to 0; `ctest` 79/79. General toolchain fix, applies to every Windows configure-based recipe, not libpng-specific.

- **Fixed the libpng `.libs/lt-*.c` wrapper `_getcwd`/`_stat`/`_chmod`/`_putenv`/`_setmode`/`_spawnv`/`_P_WAIT` undeclared-function errors** (previously listed later under "in progressing" as the `png-fix-itxt` issue -- turned out to affect every uninstalled-execution wrapper libtool generates for this port, not just that one tool, once actually chased down). Root cause: GNU Libtool's own generated wrapper template (`ltmain.sh`'s `func_emit_wrapper`, materialized fresh as `.libs/lt-*.c` per executable -- not part of libpng's own source) has two *independent*, mismatched conditionals: an `#if defined _WIN32 && !defined __GNUC__` that correctly selects `<unistd.h>` once this recipe's CFLAGS undefine `_WIN32` (steering libpng onto the generic POSIX code paths this sysroot actually provides), but a separate `#elif defined __MINGW32__` macro-rename block (`getcwd`->`_getcwd`, `stat`->`_stat`, `chmod`->`_chmod`, `putenv`->`_putenv`, `setmode`->`_setmode`) that fires regardless, since `__MINGW32__` stays defined (needed elsewhere, e.g. the `make` port's own `dir.c` fix) -- so the wrapper renames calls to underscore-prefixed MSVCRT-only spellings that nothing declared, even though the `<unistd.h>` branch was correctly taken.
  - **Fixed with a new project-owned shim**, `porting/shims/win32/libtool_wrapper_compat.h`: real `posix_spawn()`-based `_spawnv()`/`_P_WAIT` (deliberately `posix_spawn()`, not `fork()`+`execv()` -- a first attempt using `fork()` failed with "stack commit failed" in isolated testing, since `fork()` on this project's Windows PAL requires the *calling program itself* to have opted into the ASLR-mitigation self-relaunch dance at startup, which only `crt_mksh`/the `ctest` suite do; `posix_spawn()` is both the semantically correct "run a different program" primitive and needs no such opt-in) and a no-op `_setmode()` (this PAL's I/O is already byte-transparent, no CRLF mode to actually switch), plus `#define _getcwd getcwd` (and `_stat`/`_chmod`/`_putenv`) which safely cancels out against the wrapper's own later `#define getcwd _getcwd`-style macros via the C preprocessor's "blue paint" self-reference rule -- whichever spelling ends up in the wrapper body resolves back to this project's real Bionic-style POSIX name exactly once, regardless of order.
  - **Wired in via a new `force_include` recipe field**, added to `tools/crt-port-build.py`'s `apply_recipe_env()` (mirrors the existing `include_dirs` mechanism, but for `-include <file>` instead of `-I<dir>`) and set on `libpng.json`'s `target_overrides.windows`. First attempt folded the `-include` flag into `CPPFLAGS` (like `include_dirs` does) -- this compiled cleanly for ordinary library sources (`png.c`, `pngerror.c`, ...) but the real rebuild still showed the *exact same* set of undeclared-function errors, since Automake's `LINK` rule (`$(CCLD) $(AM_CFLAGS) $(CFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o $@ ...`) never references `$(CPPFLAGS)` at all -- and that `LINK` line is precisely where libtool generates *and* compiles a `.libs/lt-*.c` wrapper in one step when linking an executable against an uninstalled shared library, so the flag never reached it. Confirmed directly by diffing the real build log's `pngtest.o` *compile* line (had `-include`, via `CPPFLAGS`) against its `pngtest.exe` *link* line (missing `-include` entirely). Fixed by folding `force_include` into `CFLAGS` instead -- the one accumulator variable Automake's compile and link rules both read.
  - **Verified end-to-end, not just compiled**: the isolated shim was first sanity-checked standalone (a small `shim_check.c` mimicking the real wrapper's exact conditional structure, compiled and run directly). Then, after the `CFLAGS` fix, a full libpng rebuild dropped the undeclared-function error count from 160 to 0, and `pngtest.exe`/`pngcp.exe` (previously entirely absent from the build/install output) both now build and install. Ran the real, freshly-built `pngtest.exe` (the top-level libtool wrapper compiled from `lt-pngtest.c`, exercising the shim's `_spawnv()`/`_getcwd()`/etc. at actual runtime, not just compile time) directly: it printed libpng's full self-test output ending in `libpng passes test`, `rc=0`. Full `ctest` 79/79.

- **Fixed a real, general Windows `lstat()` bug found in libpng's own `configure` output: `rm: conf14228.dir/conf14228.file: Input/output error` (harmless-looking but a genuine PAL defect, not a real autoconf failure).** Root-caused, not just silenced: `__crt_sys_lstat_path()` (`libc/src/arch/windows/common/syscall.c`) used to unconditionally delegate to `__crt_sys_stat_path()` first, which opens via a plain `CreateFileA()` (no `FILE_FLAG_OPEN_REPARSE_POINT`) -- Windows transparently follows a symlink to its target for that call, the exact opposite of what `lstat()` means. For a *dangling* symlink (a completely normal, valid case -- e.g. autoconf's own `ln -s conf$$.file conf$$.dir` "does `ln -s` work" sanity probe, present in essentially every generated `configure` script, deliberately creates one as part of detecting the MSYS `ln -s file dir` gotcha) that follow-through open fails outright since there's nothing at the far end, so `lstat()` itself failed even though the symlink unquestionably exists and `lstat()` is specifically the call that's supposed to work on it regardless of whether the target does. Confirmed directly via an isolated repro (`ln -s conf$$.file conf$$.dir`, then `ls -la`/`rm -f` on the resulting dangling link): `ls -la` showed the entry's own metadata as all `?` (its `lstat()` call was failing too) and `rm -f` reproduced the exact same "Input/output error". Fixed by making `__crt_sys_lstat_path()` check `GetFileAttributesA()` first (which never follows a reparse point on its own) and, only for symlinks, open the link itself via `FILE_FLAG_OPEN_REPARSE_POINT` (the same flag `__crt_sys_readlink()` already uses for the identical reason) instead of the target -- non-symlink paths are unaffected, still delegating to the existing `__crt_sys_stat_path()`. Verified: the isolated repro now succeeds end to end (`ls -la` shows correct symlink metadata, `rm -f`/`rmdir` both exit 0); a real libpng `./configure` re-run no longer emits the error at all; added a regression case to `tests/file_path_test.c` (create a symlink to a nonexistent target, assert `lstat()` succeeds and reports `S_ISLNK`, assert `remove()` succeeds) since no existing test exercised a dangling symlink specifically. Full `ctest` 79/79. General CRT/PAL fix, not port-specific -- any future recipe (or any other program) that ever creates or encounters a dangling symlink on Windows was equally affected.

- **Fixed a real, severe Windows `fork()` correctness bug found chasing an apparent libffi build "hang": `libc/src/malloc.c`'s OS-region tracking table for memory-copy `fork()` silently stopped recording new heap regions once a process's total heap crossed 256MB (`CRT_MALLOC_MAX_OS_REGIONS` was 4096 * the 64KB chunk size), with no error of any kind -- `malloc()` kept succeeding normally, but any region allocated past that point was invisible to `fork_memcopy.c`'s `copy_heap_chunks()`, so it was silently never copied into the child. Root-caused while investigating an apparent 15-hour "hang" rebuilding libffi's Windows shared library (see the libffi entry later): a deeply self-recursive `mksh` interpreting a large generated `libtool` script (`mksh ./libtool --mode=link mksh crt-cc ...`, itself re-invoking `mksh`) is exactly the kind of long-lived, memory-growing process that can cross 256MB in practice, and the mandatory subshell fork right after it (`( cd ".libs" && rm -f ... && ln -s ... )`, a `TPAREN` -- always a real fork per the earlier-fixed libpng-era bug) then produces a child running on silently-corrupted memory, indistinguishable from a genuine hang or an intermittent crash depending on exactly what got dropped.
  - **First ruled out simpler explanations with real measurements**, not guesses: wrote a standalone `fork()` microbenchmark (linked with the real `fork_capable_relaunch.c` opt-in, matching how `crt_mksh` itself is built) and confirmed `fork()` cost scales roughly linearly with live heap size (~1ms/MB) but is fundamentally bounded -- ~110ms at 100MB, topping out around ~300ms at the (then-4096-region) cap -- nowhere near the observed multi-second-to-hours delays, ruling out "`fork()` itself is just slow" as the explanation. Also ruled out antivirus/file-location effects with a direct timing comparison (plain `ar` into the project's own `out/` build tree vs. the OS temp directory: both fast, no anomalous delay for a bare tool invocation outside the recursive `libtool` self-invocation chain).
  - **Then found and directly confirmed the real bug**: reading `append_chunk()`'s region-tracking code showed a silent `if (heap_os_region_count < CRT_MALLOC_MAX_OS_REGIONS) { ...track... }` with no `else` -- once full, new regions are simply never recorded, no error. Wrote a second, targeted test: allocate well past 256MB, stamp a known pattern into a chunk located past the (old) cap, `fork()`, and check in the child whether the pattern survived. It did not -- the child crashed (exit status 5, consistent with an access violation) touching memory the parent never copied because the tracking table had already silently stopped recording it.
  - **Fixed two ways, not just one**: (1) raised `CRT_MALLOC_MAX_OS_REGIONS` from 4096 to 65536 (256MB -> 4GB of trackable heap -- a still-fixed table, since growing it dynamically would need its own `mmap()`/`munmap()`-based allocation path to avoid the same malloc()-reentrancy hazard `append_chunk()`'s own existing comment already flags, and would introduce a *second* "is this array's own backing memory visible to fork()?" bookkeeping problem; 4GB is enough headroom that hitting it in practice should now be exceptionally rare). (2) far more importantly, `append_chunk()` now fails the allocation outright (`ENOMEM`) once the table is genuinely full, instead of silently succeeding untracked -- turning any future occurrence into an honest, immediately-visible allocation failure at the point of the oversized `malloc()` (a completely normal, already-handled failure mode every caller already expects), instead of a correctness time bomb that only detonates later, as memory corruption inside some unrelated later `fork()` call.
  - Verified: the same targeted repro now passes (`child exit status: 0`, pattern intact) at ~268MB, well past the old 256MB cap and safely within the new 4GB one. Full `ctest` 79/79, no regressions (this file backs every allocation in every process on Windows, so this was the highest-stakes check of this whole fix).
  - This is a general CRT/PAL correctness fix, not specific to libffi or even to `mksh` -- any sufficiently memory-hungry process using this project's own `fork()` on Windows could have hit the exact same silent corruption.

- **Achieved libffi's Windows shared-library build for real (root-caused a genuine libtool infinite loop, not a hang or a workaround), building on the libpng-era `ld.lld -m` fix and the newly-found `malloc.c` fork() bug.** After the `ld.lld -m i386pep`/`-m arm64pe` fix (already generic from the libpng work) let libtool's shared-library detection pass unmodified, and after the `malloc.c` fork()-region fix earlier, libffi's real `--mode=link` build for `libffi.la` still appeared to hang indefinitely (first observed as 15+ CPU-hours on one occasion). Root-caused for real, not worked around:
  1. **Bisected the exact trigger** by isolating the real `libtool --mode=link` command outside the full build (a standalone repro script driving `crt-port-build.py`'s own `make_env()`) and testing each of libffi's own `AM_LTLDFLAGS`/`-rpath`/`-bindir` flags individually and in combination, each with a generous, self-cleaning timeout (`taskkill /T /F` on the whole process tree on timeout, since a plain `subprocess.run(timeout=...)` only kills the immediate child, not the descendants a real fork()-heavy recursive `mksh`/`libtool` chain spawns -- learned the hard way after repeated orphaned-process pileups). `-bindir` combined with a real shared build (`-no-undefined` + `-rpath` together) was the one, and only, combination that never completed.
  2. **Found the actual infinite loop** by extracting and single-stepping libtool's own `func_normal_abspath`/`func_relative_path`/`func_dirname` functions in isolation (a minimal standalone `mksh` script sourcing just those definitions): `func_normal_abspath` classifies "is this an absolute path" by checking for a literal leading `/` (`case $path in /*) ... ;; *) prepend pwd ;; esac`) -- a genuine Windows drive-letter path (`C:/Users/...`, and `pwd` itself, also `C:/...`-shaped in this environment) never matches, so it's misclassified as relative and `pwd` gets prepended, producing a string that *still* never starts with `/`. Every subsequent step (the sed-based per-component ascent loop) *also* requires a leading `/` to make any progress at all -- so the string never changes, the loop's `while :; do if test / = "$path"; ...` termination condition can never become true, and it forks a fresh `echo`/`sed` pair every single iteration forever. A textbook infinite loop, confirmed directly (not inferred): fed the exact real path in, watched `$path` stay byte-for-byte identical across iterations.
  3. **Fixed via the standard, sanctioned mechanism, not a script patch**: libffi's own `Makefile.am` sets `AM_LTLDFLAGS = -no-undefined -bindir "$(bindir)"` unconditionally -- `AM_*FLAGS` variables are Automake's own documented end-user override point, precisely for cases like this. Overriding it via `make AM_LTLDFLAGS=-no-undefined` (dropping just the offending `-bindir`, keeping the legitimate `-no-undefined` requirement) is not a workaround or an upstream patch at all, just using the tool as designed. Required two additions, not one: (a) a new, general `target_overrides.<os>.make_args` recipe mechanism in `tools/crt-port-build.py` (merged into the base `make_args` the same way `configure_args`/`cflags` already are -- previously only the top-level `build.make_args` was read, silently ignoring any host-scoped override); (b) discovered via a debug print that a plain command-line override reaches the *first* `make` invocation correctly but libffi's own `SUBDIRS = include testsuite man doc .` triggers a *recursive* self-invocation of the same Makefile (to build `libffi.la` itself, as the trailing `.` entry) that does **not** reliably inherit it -- root cause not fully isolated (plausibly this project's own Windows process-spawn implementation not perfectly preserving GNU Make's own `MAKEFLAGS`-based recursive-variable-propagation convention) -- worked around by *also* exporting `AM_LTLDFLAGS` as a real environment variable and adding `-e` (`--environment-overrides`) to `make_args`, relying on ordinary OS-level environment inheritance across process spawns instead of GNU Make's own (apparently unreliable, in this specific recursive case) internal mechanism.
  - Verified end-to-end on Windows x86_64: `libffi-8.dll`/`libffi.dll.a` build and install cleanly (`make`: 85.5s, `make install`: 18.7s, no hangs at any step), and a standalone test program `dlopen()`ed the real DLL and ran a genuine `ffi_call()` round trip through it (`add_ints(3, 4) == 7`). Full `ctest` 79/79.
  - Explicitly **not** re-tested: the pre-existing, documented, aarch64-specific X19 double-call bug (see the "done" entry earlier and `porting/recipes/libffi.json`'s own notes) is unrelated to shared-vs-static linking and stays open; whether an analogous issue exists on x86_64 was not checked this session (only a single `ffi_call()` was exercised, not the two-calls-in-a-row repro that trips X19 on aarch64). `libffi`'s status stays `partial` for that reason -- the shared-library *build* capability is now solid and verified, but the `ffi_call()`-repeat-call correctness family is untouched.
  - Full trail, including the exact isolated `func_dirname`/`func_normal_abspath` repro and the bisection matrix across every flag combination: `porting/recipes/libffi.json`'s own notes.

## 2026-08-07

- **Synced `docs/porting_status.md` and `porting/recipes/*.json` status
  fields with facts already narrated in this file but never propagated to
  the status doc/recipes.** Found via user report ("porting status looks
  wrong"): commit `4628c6c` ("Record macOS/Linux confirmation of
  sqlite-amalgamation shared build") only ever updated this file's prose,
  never `docs/porting_status.md`'s table or the recipe JSONs' own
  `status`/`notes` fields -- same gap for the earlier zlib/libpng Linux
  `ldd` and macOS `otool` confirmations. Also resolved an open question
  about `make`: it is **not** Windows-specific -- `tools/crt-port-build.py`'s
  `build_port()` unconditionally builds and installs the `make` port before
  any `configure`-system recipe on every host, and `make_env()` prefers the
  freshly-built `PORT_PREFIX/bin/make` over host `make` via `$MAKE` on all
  three OSes (verified by reading the code, not just prose). Updated,
  matching the user's direct confirmation that zlib/libpng/libffi/
  sqlite-amalgamation all pass on macOS/Linux:
  - `make`: `linux`/`macos` `pending` -> `manual-pass` (matching Windows;
    kept at `manual-pass` rather than higher since no standalone
    `make --version`-style direct check has been separately recorded for
    these two hosts, only indirect, repeated use as the build driver for
    every other port).
  - `zlib`: `linux` `manual-pass` -> `shared-pass`, `macos` `configure-pass`
    -> `shared-pass` (the `ldd`/`otool` confirmations from the cross-port
    rpath fix work earlier were already real verification of "shared library
    ... load[ing] and run[ning] correctly at runtime", just never reflected
    in the status value).
  - `libpng`: `linux` `pending` -> `shared-pass`, `macos` `configure-pass`
    -> `shared-pass` (unlike Windows, libpng's real GNU Libtool build
    already produces a working shared library on Linux/macOS through this
    project's real system `ld`/`ld64` -- confirmed via the same `ldd`
    session that showed `libpng16.so.16.57.0` resolving its `libz.so.1`
    dependency correctly).
  - `sqlite-amalgamation`: `linux` `smoke-pass` -> `amalgamation-pass`
    (matching macOS/Windows; the `amalgamation` build system's own ceiling
    status stays `amalgamation-pass` even with a full shared-library
    round trip verified, matching the existing Windows-row convention of
    recording shared-library depth in notes rather than a separate status
    tier).
  - `libffi`: status values left unchanged (`partial`/`configure-pass`/
    `partial`) -- the well-documented X19 callee-saved-register `ffi_call()`
    runtime bug is real and independent of shared-vs-static linking, so a
    blanket "all pass" was **not** applied here; only added a clarifying
    note that libffi's own shared library does build successfully on
    Linux/macOS (already evidenced in this file's zlib-Linux-shared-bug
    writeup: "`libffi`/`libpng` shared builds succeeded [on Linux]"), which
    is a separate axis from the open runtime bug.

- **Windows fork() implementation: concluded.** Marking this closed as its
  own entry: the spawn-broker retirement -> Cygwin/MSYS-style memory-copy
  `fork()` transition, the libpng `configure`/`make`/`make install`
  blocker chain that exercised it, the sqlite-amalgamation/libffi porting
  follow-up, and the reverted process-reparenting attempt (each recorded
  as its own dated entry in this file) together brought the work to a
  genuinely done state: both Windows architectures have a working
  memory-copy `fork()` (`docs/windows_fork_emulation.md`), zlib/libpng/
  libffi/sqlite-amalgamation all build (libffi with one documented,
  unresolved X19 bug), and shared-library support is confirmed across all
  3 OSes. Any new fork-related problem found from here on gets its own
  fresh entry, not appended into this one.

- **Extended `porting/recipes`' `amalgamation` build system with real
  shared-library support, and turned it on for sqlite-amalgamation.**
  `tools/crt-port-build.py`'s `build_amalgamation_port()` gained a
  `"shared": true` recipe opt-in: when set, it also compiles a second,
  `-fPIC`-flagged pass over the recipe's own sources and links them via
  `tools/crt-cc`'s existing `-shared`/`-dynamiclib` support (which already
  handles everything OS/arch-specific about shared linking -- this new
  code only supplies the flags specific to *this* library's own name/
  version). Naming/versioning mirrors zlib's own established convention
  on macOS/Linux (a real versioned file plus SONAME-style symlink
  aliases), while Windows gets a plain, unversioned `<name>.dll` (no
  `lib` prefix, no version suffix) -- matching how upstream SQLite itself
  actually ships its own precompiled Windows binary as a bare
  `sqlite3.dll`. No `.lib` import library generated on Windows, same
  precedent as zlib's own shared build this session (`lld-link` can link
  a consumer directly against the built `.dll` by its exact filename).
  Turned on for `sqlite-amalgamation.json` via `"shared": true`. Verified
  end-to-end on Windows aarch64 *and* a real x86_64 cross-build
  (`out/windows-x64-cross-debug`): `sqlite3.dll` compiles, links, reports
  the correct architecture via `llvm-objdump -f`, and a standalone test
  program dynamically linked against it (built directly via `crt-cc`,
  mirroring how `examplesh`/`minigzipsh` link against zlib's shared
  build) ran a real `sqlite3_open`/`CREATE TABLE`/`INSERT`/`SELECT` round
  trip successfully on both architectures. Full `ctest` 79/79 after the
  `tools/crt-port-build.py` change.
  - **Update: confirmed on macOS and Linux too.** `otool -L` on macOS
    shows `libsqlite3.dylib` depending only on its own self-identity
    (`libsqlite3.3.dylib`, matching the `-install_name` set at link time)
    and `/usr/lib/libSystem.B.dylib` -- no accidental `@rpath/libc.dylib`
    pickup the way zlib's build hit (sqlite's own build has no equivalent
    of zlib's stray `LDSHAREDLIBC=-lc` flag to trigger it). `ldd` on
    Linux shows `libsqlite3.so`'s `libc.so`/`libm.so`/`libdl.so`/
    `libc++.so` dependencies all correctly resolving to this project's
    own sysroot. Shared-library support now confirmed working across all
    3 OSes (Windows aarch64+x86_64, macOS aarch64, Linux aarch64) for
    zlib, and across Windows aarch64+x86_64 plus macOS aarch64 and Linux
    aarch64 for sqlite-amalgamation.

- **Fixed cross-port shared-library resolution on macOS/Linux (one
  port's `.so`/`.dylib` finding *another port's*), and macOS's own
  `libc.dylib` rpath gap uncovered along the way.** Reported via the same
  `ldd` session earlier: `libpng16.so`'s `libz.so.1` dependency resolved to
  `/lib/aarch64-linux-gnu/libz.so.1` (Ubuntu's own system zlib package)
  instead of this project's own, freshly-built one sitting right next to
  `libpng16.so` in the same `PORT_PREFIX/lib` directory. Root cause: the
  `tools/crt-cc`/`tools/crt-c++` `-rpath` added for the earlier `libc.so`
  fix only covers this project's own sysroot (`${CRT_SYSROOT}/lib`) --
  a completely different directory from where third-party ports install
  their own shared libraries (`PORT_PREFIX/lib`, e.g. `port-tests/
  install/lib`), which was on no rpath at all. Fixed in
  `tools/crt-port-build.py`'s `make_env()`: `$LDFLAGS` now also carries
  `-Wl,-rpath,<PORT_PREFIX>/lib` on macOS/Linux (skipped on Windows,
  which has no rpath concept at the PE/COFF level and where `lld-link`
  in MSVC-compatible mode doesn't understand the flag at all).
  While investigating, the user separately shared `otool -L` output on
  macOS that turned up a second, related bug this same session's earlier
  "macOS is fine, deliberately left static, no need to touch" conclusion
  had missed: `libz.dylib` carried a real `@rpath/libc.dylib` dependency
  despite `tools/crt-cc`'s macOS branch only ever naming static `.a`
  archives in its own `libs` list. Cause: zlib's own `configure` sets
  `LDSHAREDLIBC=-lc` unconditionally except on MinGW, appended
  independently to its shared-library link line -- and since this
  project's own CMake build always produces both `libc.a` *and*
  `libc.dylib` side by side in `${CRT_SYSROOT}/lib`, `ld64`'s default
  `-l<name>` resolution prefers the dylib over the same-named static
  archive when both exist on the search path, silently reintroducing a
  dynamic dependency this script was specifically trying to avoid. Since
  nothing set an `-rpath` there either, that `@rpath`-relative dependency
  had no way to resolve at actual load time (libtool-built ports like
  libpng/libffi never pass a stray bare `-lc` the way zlib's own
  hand-written Makefile does, so they didn't hit this). Fixed the same
  way as Linux: `tools/crt-cc`/`tools/crt-c++`'s macOS `shared_mode`
  entry flags now also add `-Wl,-rpath,${CRT_SYSROOT}/lib`, so if a
  dylib reference sneaks in via some mechanism outside this script's own
  control, it still resolves to this project's real one. Windows
  regression-checked here (rebuilt zlib's shared library after each
  change, no behavior change -- the windows case block itself was never
  touched).
  - **Update: confirmed fixed on both hosts, from clean `out/` rebuilds.**
    Linux: `libpng16.so.16.57.0`'s `libz.so.1` dependency now resolves to
    this project's own `port-tests/install/lib/libz.so.1`, not the
    system's; the stray `/lib/ld-linux-aarch64.so.1`/system `libc.so.6`
    entries seen on an earlier, non-clean `libpng16.so.16.57.0` build
    were apparently a stale-incremental-build artifact -- gone on a
    clean rebuild, not a real bug in this fix.
    macOS: `otool -L` alone can't distinguish "has an unresolvable
    `@rpath` dependency" from "has one that resolves fine" (it only
    lists dependencies, not `LC_RPATH` commands), so `otool -l ... |
    grep -A2 LC_RPATH` on `libz.dylib` was checked instead and shows
    both expected `LC_RPATH` entries present: `.../sysroot/lib` (finds
    this project's own `libc.dylib`) and `.../port-tests/install/lib`
    (finds sibling ports' `.dylib`s, same as the Linux fix earlier) --
    `@rpath/libc.dylib` still appears in `otool -L`'s dependency list
    (expected and correct: the fix makes it *resolvable*, not absent)
    and should now load correctly at runtime.

- **Fixed the Linux `-shared` fix earlier finding the wrong `libc.so` at
  runtime.** The Linux non-PIC-`libc.a` fix landed the build/link step,
  but `ldd` on the resulting `libz.so.1.3.1` on a real Ubuntu/Debian
  aarch64 machine reported `error while loading shared libraries:
  /lib/aarch64-linux-gnu/libc.so: invalid ELF header`. Root cause: none
  of this project's own `libc.so`/`libm.so`/`libdl.so`/`libc++.so` CMake
  targets set an explicit `-soname`, so each defaults to its own bare
  output filename ("libc.so", ...) as its `DT_SONAME` -- and that's the
  literal string `tools/crt-cc`/`tools/crt-c++`'s Linux `shared_mode`
  linking then records as `libz.so.1.3.1`'s own `DT_NEEDED` entry. At
  runtime, the dynamic loader has no memory of the absolute sysroot path
  used at link time -- it re-searches the bare name via the standard
  system path, and on Debian/Ubuntu aarch64, `/lib/aarch64-linux-gnu/
  libc.so` genuinely exists as part of `libc6-dev`: a plain-text GNU-ld
  `INPUT()` linker script meant only for the *host* toolchain's own
  link-time use, not a real loadable ELF image -- so `ld.so` rejected it
  outright. Fixed by adding `-Wl,-rpath,${CRT_SYSROOT}/lib` to
  `shared_mode`'s Linux entry flags in both `tools/crt-cc` and
  `tools/crt-c++`: bakes this project's own sysroot lib dir into the
  resulting object's `DT_RUNPATH`, which `ld.so` consults (ahead of the
  system default path) specifically when resolving *that object's own*
  `DT_NEEDED` entries, so it finds this project's real `libc.so` there
  first regardless of what unrelated same-named file the host happens to
  have. Not yet re-verified with `ldd` on the user's real Linux machine
  (this fix was prepared, not run, per current session convention where
  the user runs build/regression verification directly) -- pending.
  - **Update: confirmed fixed.** `ldd` on the user's real Linux aarch64
    machine now shows `libz.so.1.3.1`'s `libc.so`/`libm.so`/`libdl.so`/
    `libc++.so` dependencies all correctly resolving to this project's
    own sysroot (`.../out/linux-host-ninja-debug/sysroot/lib/...`), not
    the host system's.

- **Implemented `__crt_sys_readlink()` for real (was an honest `-ENOSYS`
  stub).** Found while separately verifying the Windows build wasn't
  regressed by the Linux fix earlier: rebuilding zlib's shared library a
  *second* time (install dir already has `libz.so`/`libz.so.1` symlinks
  from the previous run) failed with `rm: .../libz.so: Function not
  implemented`. Traced to toybox's `dirtree.c` -- shared by every
  directory-walking applet, including `rm` -- calling `readlinkat()` on
  every symlink entry it visits to populate `try->symlink`; the stub's
  `-ENOSYS` propagated straight up into `rm` aborting. This was flagged
  as a known gap when `__crt_sys_symlink()` was implemented earlier this
  session ("nothing currently needs it") -- turned out something did,
  just not until a *rebuild* scenario exercised it. Implemented via
  `CreateFileA(..., FILE_FLAG_OPEN_REPARSE_POINT)` (opens the link
  itself rather than transparently following it, the opposite of a plain
  open) + `DeviceIoControl(FSCTL_GET_REPARSE_POINT)`, parsing the
  `SymbolicLinkReparseBuffer` arm of `REPARSE_DATA_BUFFER` (field-for-
  field per real winnt.h) and extracting `PrintName` (the human-facing
  target string `CreateSymbolicLinkA` was actually given, as opposed to
  `SubstituteName`, which may carry an NT-namespace `\??\` prefix for
  absolute targets) via `WideCharToMultiByte` (UTF-16 `PathBuffer` ->
  narrow `char*`, `CP_ACP`, matching every other narrow-char Win32 API
  this file already calls). `tests/file_path_test.c`'s Windows symlink
  block updated to match (was asserting `-ENOSYS` as the expected
  "policy"; now asserts a real round trip, mirroring the non-Windows
  branch). Verified: `file_path_test` passes, full `ctest` 79/79, and
  rebuilding zlib's shared library twice in a row (the exact scenario
  that surfaced this) no longer errors either time.

- **Fixed `tools/crt-cc`/`tools/crt-c++` `-shared` mode statically linking
  non-PIC archives on Linux.** Reported: `zlib`'s shared build, which
  worked on macOS, failed its own `configure`-time shared-library probe
  on a real Linux aarch64 host (`libffi`/`libpng` shared builds succeeded
  there, `zlib`'s specifically didn't). `configure.log` showed the real
  cause: `ld` refused `libc.a(stdio.c.o)`'s `R_AARCH64_ADR_PREL_PG_HI21`
  relocations against `stdin`/`stdout`/`stderr` ("dangerous relocation:
  unsupported relocation ... recompile with -fPIC") the moment the probe
  needed a `stdio.c.o` symbol (`getchar()`) -- `libffi`/`libpng`'s shared
  builds happened not to need any `stdio.c.o`/`env.c.o` symbol from
  `libc.a` directly, so they never tripped this. Root cause: `crt-cc`/
  `crt-c++`'s `-shared`/`-dynamiclib` mode statically linked
  `libc.a`/`libm.a`/`libdl.a`/`libc++.a` (never compiled with `-fPIC`,
  since nothing about a normal executable requires it) on every OS, not
  just macOS/Windows where that happens not to be a hard error. Fixed on
  Linux by linking the already-built *shared* counterparts
  (`libc.so`/`libm.so`/`libdl.so`/`libc++.so` -- already present in the
  sysroot via this project's own `c_shared`/`m_shared`/`dl_shared`/
  `cxx_shared` CMake targets) instead, for `shared_mode` only (normal
  executables still statically link the `.a` archives, unaffected).
  Deliberately did **not** make the same change on macOS: it was already
  confirmed working via static linking (Mach-O/AArch64 is unconditionally
  position-independent at the ABI level regardless of `-fPIC`, so this
  isn't a hard error there the way it is on Linux's stricter ELF `ld`),
  so switching it too would only add an unproven `.dylib`
  `install_name`/`@rpath` runtime-loadability question with no upside.
  `libclang_rt.builtins.a` is left static in both modes everywhere
  (leaf compiler-intrinsic code, no global-data relocations of this
  kind -- statically linking compiler-rt/libgcc into shared objects is
  itself completely normal). Not yet verified past the link step itself
  succeeding: whether the resulting `libz.so` can actually find
  `libc.so`/etc at runtime when dynamically loaded (`ldd`/`LD_LIBRARY_PATH`
  question, since they're linked by absolute sysroot path rather than
  via an installed system location) is a real follow-up to check.

- **Fixed the CMake `port-rebuild-sqlite-amalgamation` target failing
  outright on native Windows, and stopped forcing macOS/Linux port builds
  through this project's own rootfs mksh.** Reported: `sqlite-amalgamation`
  on x86_64 Windows failed with `FileNotFoundError: [WinError 2]` trying to
  spawn `tools/crt-cc` directly (a shebang script, no `.exe`) -- root
  caused to `CMakeLists.txt`'s `crt_add_build_port_target()` only adding
  `--use-crt-shell` for `configure`/`android_host_tool` recipe build
  systems, not `amalgamation`; without it, `crt-port-build.py`'s
  `make_env()` hands the bare script path to `CreateProcess`, which cannot
  interpret a shebang line the way a POSIX host can. Fixed by adding
  `amalgamation` to that condition. While investigating, also confirmed
  (by reading `tools/crt-cc`, `crt-port-build.py`'s `make_env()`/
  `build_configure_port()`, and every recipe's macOS/Linux
  `target_overrides`) that `--use-crt-shell`'s *other* effect -- routing
  `./configure`/`make`/every compiler invocation through this project's
  own from-scratch mksh, and putting the rootfs's toybox applets ahead of
  the host's own coreutils on `$PATH` -- was never actually needed on
  macOS/Linux: nothing in any recipe depends on it (the CRT sysroot
  integration is carried entirely by `CC`/`CXX` pointing at `tools/crt-cc`/
  `tools/crt-c++`, which works identically either way), and those hosts
  already have a real, complete, natively-shebang-capable shell +
  coreutils -- exactly what upstream `configure` scripts are actually
  tested against, unlike this project's deliberately-minimal,
  Windows-motivated toybox applet set. So `CRT_TARGET_OS STREQUAL
  "windows"` was added to the same condition, scoping `--use-crt-shell`
  (and the `rootfs` build dependency it requires) to native Windows only.
  Expected effect beyond fixing the immediate crash: real correctness risk
  removed (macOS/Linux configure probes now see the same coreutils/awk/
  grep upstream projects are tested against, not this project's own
  applets) and likely a real speedup for macOS/Linux configure runs (no
  longer routed through this project's own mksh for the thousands of tiny
  subprocess probes a typical `configure` script runs). Verified in this
  session only via a fast `cmake --preset` reconfigure + inspecting the
  generated `build.ninja` (`--use-crt-shell` still present for the Windows
  preset's port targets, unchanged); the real x86_64 Windows
  `sqlite-amalgamation` rebuild that reported the original crash is still
  pending verification by the user directly on that host.
  - **Update: macOS confirmed.** The user rebuilt zlib, libpng, libffi, and
    sqlite-amalgamation on a real macOS machine after this change and
    confirmed both predictions: the build is noticeably faster, and the
    previously-present spurious errors/warnings during `configure`/`make`
    are gone (`zlib`'s `libz.1.3.1.dylib` in particular confirmed to build
    correctly). Also confirmed, on request, exactly what still ties these
    builds to the CRT sysroot despite no longer routing through this
    project's own mksh: `tools/crt-cc` passes `-nostdinc
    -isystem${CRT_SYSROOT}/include` (host system headers excluded
    entirely, only this project's own Bionic-compatible headers visible)
    and links via `${CRT_SYSROOT}/lib/crt1.o -L${CRT_SYSROOT}/lib` --
    unrelated to, and unaffected by, which shell drives `configure`/`make`
    itself. See `docs/porting_status.md`'s zlib/libpng/libffi/
    sqlite-amalgamation rows for the per-port notes.

- **Fixed GNU make's Windows x86_64 build (`dir.c` compile error, then two
  more bugs found chasing it).** Reported from a real x86_64 Windows machine
  (this project's own dev machine is aarch64, whose `make` build had never
  hit any of these): `src/dir.c:1241: error: array type 'char[256]' is not
  assignable` at `d->d_name = xmalloc(len)`. Root-caused to
  `tools/crt-cc`/`tools/crt-c++` targeting `*-w64-mingw32` predefining
  `__MINGW32__` but not `__MINGW32_MAJOR_VERSION`/`__MINGW32_MINOR_VERSION`
  (only a real mingw-w64 install's own `_mingw.h` does), so `dir.c`'s `#if
  __MINGW32_MAJOR_VERSION < 3 ...` guard (an ISO C preprocessor-arithmetic
  undefined-macro-as-0 trap) wrongly took an ancient-mingw compat branch
  treating `d_name` as a pointer -- doesn't compile against this project's
  Bionic-style fixed-array `struct dirent`. Fixed by defining both macros
  in `tools/crt-cc`/`tools/crt-c++`'s Windows case block to match a real,
  current mingw-w64 install's actual values (arch-independent: same fix for
  both `aarch64-w64-mingw32` and `x86_64-w64-mingw32`; Windows-only, since
  `__MINGW32__` is never defined on macOS/Linux). Verifying this on an
  x86_64 cross-build (`out/windows-x64-cross-debug`, this project's own
  aarch64 dev machine's x64-emulation cross-arch setup from earlier this
  session) surfaced two more, genuinely x86_64-only problems past the fixed
  compile step:
  - Link failed on `undefined symbol: ___chkstk_ms` -- the MinGW-mangled
    name clang emits calls to (instead of the MSVC-triple `__chkstk` name
    this project already implemented in `libc/src/arch/windows/x86_64/
    chkstk.S`) when a function's stack frame is large enough to need a
    guard-page-safe stack probe. Both names are, per LLVM's own
    compiler-rt, the exact same routine under historical MSVC-vs-MinGW
    C-symbol-naming-convention names -- fixed by adding `___chkstk_ms` as a
    second label on the same code, right in `chkstk.S`. Not needed on
    aarch64: AArch64 COFF never had the leading-underscore name-mangling
    split x86/x86_64 did, so mingw-w64 uses the same `__chkstk` name there
    as MSVC (confirmed by checking `libc/src/arch/windows/aarch64/
    chkstk.S`, which needs no such alias).
  - Then `undefined symbol: __main` -- clang's `*-w64-mingw32`-only
    implicit call inserted at the top of every `main()`, a decades-old
    GCC/MinGW convention for running `.ctors`-section constructors a PE
    loader wouldn't run itself. This project's own CRT startup
    (`src/arch/windows/common/crt1.c`) already runs constructors through
    its own mechanism before `main()` is ever reached, so `__main()` itself
    has nothing left to do -- matching modern mingw-w64's own runtime,
    which keeps it only as an empty stub for the same reason. Added exactly
    that: an empty `void __main(void) {}` in `libc/src/arch/windows/
    common/compiler_abi.c` (same file/pattern as the pre-existing
    `__clear_cache()` compiler-support-symbol stub), arch-independent (pure
    C, no per-arch asm needed) since nothing rules out some other Windows
    port also needing it on aarch64 eventually.
  - Also found and fixed, while setting up the x86_64 cross-build repro: a
    latent `tools/crt-port-build.py` bug where `--target-arch`/
    `CRT_TARGET_ARCH` was used only for the `@CRT_MINGW_TRIPLE@` recipe-
    string substitution and never actually exported to the `crt-cc`/
    `crt-c++` child processes -- which independently auto-detect arch via
    `uname` when `$CRT_TARGET_ARCH` is unset, silently building the
    *host's* architecture instead of the requested one on a genuine
    cross-arch build (caught because a deliberately-requested x86_64 build
    on this aarch64 dev machine came out as an aarch64 binary with no
    error). Fixed in `make_env()`: export `$CRT_TARGET_ARCH` derived from
    the same already-resolved `mingw_triple`.
  Verified end-to-end on x86_64: `make.exe` compiles, links, reports
  `architecture: x86_64` via `llvm-objdump -f`, runs `--version`, and
  correctly evaluates `$(wildcard *.txt)` (exercising the exact `dir.c`
  code path that started this). Verified on aarch64 too (link succeeds,
  no regression). Full `ctest` and the real x86_64 build/regression run
  this depends on (`zlib` -> `make`) were confirmed passing by the user
  directly rather than by this session -- see
  `docs/porting_status.md`'s `make` row for the full writeup.

- **Windows shared-library (DLL) build support, end to end for zlib.**
  Discovered `porting/recipes/*.json` never produced `.so`/`.dll`/`.dylib`
  outputs at all (only static archives), traced to two gaps and fixed both:
  - `tools/crt-cc`/`tools/crt-c++` had no `-shared`/`-dynamiclib` support --
    always hardcoded EXE-building flags (`crt1.o` + entry point). Added
    `shared_mode` detection (alongside the existing `compile_only`) that
    swaps in the right start object/entry flags per OS: macOS/Linux drop
    `-e,_start` (a shared object has no `_start`), Windows swaps `crt1.o`
    for a new `dllcrt.o` and links with `/entry:crtDllMainCRTStartup
    /DLL /OPT:REF`. `dllcrt.o` (`crtDllMainCRTStartup`, in
    `libc/src/arch/windows/common/dllcrt.c`) already existed for this
    project's own `c.dll`/`c++.dll`/etc CMake DLL targets but was never
    installed into the sysroot as a standalone, reusable object the way
    `crt1.o` is for EXEs -- added that install rule to `libc/CMakeLists.txt`.
    Verified via a minimal hand-built test DLL (`llvm-readobj
    --file-headers` shows `IMAGE_FILE_DLL` set correctly; loads for real via
    `LoadLibraryA`/`FreeLibrary`), then via `crt-cc -shared` directly, then
    via the real zlib port build.
  - `libc/src/arch/windows/common/syscall.c`'s `__crt_sys_symlink()` was a
    pure `-ENOSYS` stub, which broke zlib's Makefile SONAME step (`ln -s
    libz.so.1.3.1 libz.so`) even after the DLL itself linked successfully.
    Implemented via real `CreateSymbolicLinkA()` (note the Win32 arg order
    is target/link *reversed* from POSIX `symlink(target, linkpath)`), with
    `SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE` and a
    `GetFileAttributesA()`-based best-effort directory-vs-file flag guess
    (the target need not exist yet, matching upstream Makefiles that
    symlink before the real file lands). `readlink()` stays `-ENOSYS`
    (reparse-point parsing via `DeviceIoControl`/`FSCTL_GET_REPARSE_POINT`
    is real extra work nothing currently needs). Also added `ERROR_PRIVILEGE_
    NOT_HELD` (1314) -> `EPERM` to `map_windows_error()` (previously fell
    through to a generic, unhelpful `EIO`) after hitting it for real:
    `CreateSymbolicLinkA()` with the unprivileged-create flag still requires
    Windows Developer Mode to be enabled on the machine for a non-elevated
    process -- confirmed by testing on this dev machine with Developer Mode
    off (real `EPERM` failure), then again after the user enabled it via
    `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\AppModelUnlock\
    AllowDevelopmentWithoutDevLicense` (real success). Updated
    `tests/file_path_test.c`'s Windows-only symlink block to match (it
    previously *asserted* `-ENOSYS`, documenting the old stub as "policy" --
    now asserts a real create succeeds and only `readlink()` stays
    `-ENOSYS`).
  - `porting/recipes/zlib.json`'s `configure_args` changed from
    `["--static"]` to `[]` (zlib's own default already builds both). Full
    `ctest` 79/79 after the `syscall.c` change. Verified past "it built"
    into "it actually works": zlib's own `examplesh`/`minigzipsh` test
    binaries, dynamically linked against the freshly built
    `libz.so.1.3.1`, ran a real compress/uncompress/gzip round trip
    successfully.
- **libpng and libffi shared-library attempts: root-caused, not achieved.**
  Both go through real GNU Autoconf + Libtool (unlike zlib's hand-written
  Makefile), and Libtool's MinGW shared-library detection doesn't recognize
  this toolchain: `checking for dlltool`/`checking for objdump` both came up
  false (this project ships LLVM's `llvm-dlltool`/`llvm-objdump`, not
  binaries under those literal GNU-binutils names) and `checking if the
  linker is GNU ld` incorrectly resolves `yes` for `lld-link.exe` running in
  MSVC-compatible mode (its `-v` banner says "compatible with GNU linkers",
  which apparently still matches whatever pattern this libtool.m4 vintage
  uses). Extended `tools/crt-port-build.py`'s `make_env()` to pre-set
  `$DLLTOOL`/`$OBJDUMP` the same way `$LD` already was, which fixed those
  two probes -- but `checking whether the ... linker ... supports shared
  libraries` still resolves `no` afterward, via a block of libtool-internal
  shell logic with no compiler/linker invocation logged in between (a
  static case-statement decision, not a failed compile probe). Not narrowed
  further; both ports still build and install cleanly as static-only
  (`configure_args` no longer force `--disable-shared`, matching zlib's
  convention of not fighting Libtool's own default, but the practical
  result on Windows is unchanged from before). Full trail in each recipe's
  own `notes`.

- Ran the two remaining Windows porting tests (sqlite-amalgamation and
  libffi), following up on the libpng work earlier.
  - **sqlite-amalgamation: full success.** sqlite3.c's own `SQLITE_OS_WIN`
    detection (`defined(_WIN32) || defined(WIN32) || defined(__CYGWIN__) ||
    defined(__MINGW32__) || defined(__BORLANDC__)`) fired because
    `tools/crt-cc` targets `*-w64-mingw32` (needed for the GNU-C/GNU-ld
    detection fix from the libpng chain earlier), which predefines those
    macros -- fixed the same way as zlib/libpng, via
    `target_overrides.windows.cflags` undefining them so SQLite takes its
    generic `SQLITE_OS_UNIX` path instead of `#include "windows.h"`.
    Verified via the full recipe flow (compile/archive/ranlib/install) plus
    a standalone program that actually opened an in-memory db, created a
    table, inserted, and selected the correct value back.
  - **libffi: builds and installs successfully; core features work in
    isolation; one real, well-characterized bug remains unresolved.**
    Blocker chain: `config.guess` doesn't recognize plain Windows `uname`
    (same `--build=aarch64-w64-mingw32` workaround as libpng/zlib) -> a
    broken top-level "multilib dispatcher" `Makefile` libffi's own build
    generates (a bundled `makefile.sed` mishandles a Windows drive-letter
    colon as a `Makefile` target separator, corrupting `MAKE=C:/...` into
    `MAKE=C:`) -> routed around via a new, general
    `target_overrides.<os>.make_subdir` mechanism in
    `tools/crt-port-build.py` that points `make`/`make install` straight at
    the real subdirectory `Makefile` -> `dlmalloc.c`/`ffi.c` both
    `#include <windows.h>`, which this sysroot doesn't have. First tried
    the usual `-U_WIN32` CFLAGS trick (same as zlib/libpng/sqlite), but
    that turned out wrong here specifically: `_WIN32` also gates libffi's
    own Windows-aware avoidance of the X18/TEB-reserved register in its Go-
    closures code and its `FFI_WIN64` default ABI, so hiding it silently
    re-broke both. Settled instead on a small, project-owned
    `porting/shims/win32/windows.h` (new `include_dirs` recipe mechanism)
    providing just the handful of Win32 APIs those two files actually
    call, keeping `_WIN32` defined normally so every one of libffi's own
    Windows-aware decisions resolves exactly as upstream intends. Along the
    way, discovered `InterlockedCompareExchange`/`InterlockedExchange`/
    `InterlockedCompareExchangePointer` (needed by `dlmalloc.c`'s spinlock)
    are real winnt.h compiler intrinsics, not kernel32 exports -- provided
    via `__sync_val_compare_and_swap`/`__sync_lock_test_and_set` instead --
    and added `__clear_cache()` (`libc/src/arch/windows/common/
    compiler_abi.c`) since this project's Windows builds have no
    compiler-rt builtins archive at all. With all of that, the full build
    succeeds, and `ffi_call()` alone and `ffi_closure_alloc()`/
    `ffi_prep_closure_loc()` alone (a real trampoline, exercising the
    VirtualAlloc/mprotect-equivalent `PROT_EXEC` path) each work correctly
    in isolation -- but calling `ffi_call()` and then making *any* further
    libffi call in the same process reliably segfaults, and only when the
    caller is compiled at `-O1`/`-O2` (never `-O0`). Root-caused (via a
    minimal ~20-line repro, disassembly, and ruling out X18 corruption,
    instruction-cache staleness, and shared-`ffi_cif`-state as causes) to a
    callee-saved GPR (observed: X19) that clang trusts AAPCS64 to preserve
    across the `ffi_call()` call getting corrupted somewhere in the
    `ffi_call()`/`ffi_call_SYSV` chain (`src/aarch64/ffi.c` +
    `src/aarch64/sysv.S`'s unusual caller-provided-stack-frame convention)
    -- not yet isolated to an exact instruction; would need single-
    stepping `ffi_call()`'s own compiled code with a real debugger. Status
    left at `partial` (matching Linux's existing status) rather than a
    false "pass". Full trail and the repro recipe: see
    `porting/recipes/libffi.json`'s own notes.

## 2026-08-06

- Ported Windows aarch64's Cygwin/MSYS-style memory-copy `fork()` (Phase C
  earlier) to x86_64. Most of the design carries over unchanged --
  `fork_capable_relaunch.c` (the startup self-relaunch under the ASLR-
  disabling mitigation policy) is pure Win32 API with zero
  architecture-specific code, so it was moved from `aarch64/` to a shared
  `libc/src/arch/windows/common/` rather than duplicated. What's genuinely
  new for x86_64 is `libc/src/arch/windows/x86_64/fork_memcopy.c`:
  - `CONTEXT_AMD64` (winnt.h), transcribed field-for-field and
    cross-checked directly against a real Windows SDK `winnt.h` rather
    than from memory (`P1Home` through `LastExceptionFromRip`,
    `XSAVE_FORMAT`/`M128A` included) -- then independently verified via a
    standalone `offsetof()` probe (every offset matched exactly, including
    `sizeof(CONTEXT) == 1232`) before ever being wired into the real
    build, given how costly a wrong offset would be here.
  - TEB access: x86_64 has no equivalent of aarch64's reserved X18
    platform register -- reads it via the GS segment directly
    (`%gs:0x30`, no register reservation needed anywhere else in the
    build, unlike aarch64's globally-applied `-ffixed-x18`).
    `NT_TIB.StackBase`/`StackLimit` sit at the identical `+0x08`/`+0x10`
    offsets on both architectures, so `copy_current_stack()`'s actual
    logic needed no changes.
  - setjmp()/CONTEXT register mapping: Windows x64's callee-saved set
    (`libc/src/arch/windows/x86_64/setjmp.S`) is `rbx`/`rbp`/`rdi`/`rsi`/
    `r12`-`r15`/`rsp`/return-address plus `xmm6`-`xmm15` -- and unlike
    aarch64 (AAPCS64 only guarantees the low 64 bits of `v8`-`v15`), the
    Windows x64 ABI preserves `xmm6`-`xmm15` in full (128 bits each), so
    both halves of each register needed copying into `CONTEXT.FltSave.
    XmmRegisters[6..15]`.
  - Found and fixed a real bug while wiring this up: `crt1.c`'s weak
    symbol reference to `__crt_windows_ensure_fork_capable_relaunch()`
    (and its call site) were still guarded by `#if defined(__aarch64__)
    ...` only -- so on x86_64 the startup self-relaunch silently never
    ran at all (the symbol didn't exist in that translation unit, so
    "call it if non-null" was compiled out, not evaluated false). Caught
    via `fork_test` failing with a stack-commit error whose parent-side
    address changed on every run -- exactly what unmitigated ASLR looks
    like, immediately after `GetProcessMitigationPolicy()` had been
    independently verified (via a standalone probe) to correctly report
    "not yet mitigated" vs. "mitigated" in this same x64-under-emulation
    environment. Fixed by extending that `#if` too.
  - Verified end-to-end: set up a same-OS cross-arch build
    (`-DCRT_TARGET_ARCH=x86_64` on this Windows aarch64 machine, requiring
    a new `CMAKE_C_COMPILER_TARGET`/`CMAKE_CXX_COMPILER_TARGET`/
    `CMAKE_ASM_COMPILER_TARGET` = `x86_64-pc-windows-msvc` cross-compile
    path added to the top-level `CMakeLists.txt` -- Clang needs no
    separate per-arch install, just a different `--target`), then ran the
    real x86_64 binaries under this machine's built-in x64 emulation
    (Prism/xtajit). Full `ctest` 79/79, including `fork_test`/
    `fork_signal_test`/`fork_runtime_reset_test`. Not yet re-verified on
    real x86_64 hardware -- see `docs/windows_fork_emulation.md`, "Current
    Open Issues".

- **Retired the spawn broker; moving to a Cygwin/MSYS-style `fork()` instead.**
  The broker (see "done" earlier) fixed zlib and got libpng most of the way,
  but kept surfacing new structural failure modes of its own this session
  (orphaned `mksh.exe` processes, named-pipe races, I/O timeouts, a
  process-tree-reparenting attempt that regressed the working state and had
  to be reverted -- see the entry later). Decided to isolate it out of the
  active build rather than keep hardening it, and pursue the alternative
  recorded in `docs/windows_fork_emulation.md`'s "Rejected alternatives"
  section instead: a real Cygwin/MSYS-style `fork()` (`CreateProcessA` +
  `WriteProcessMemory` memory copy + `setjmp`/`longjmp` resume), which
  removes the "unregistered clone" problem at its root instead of working
  around it process-by-process.
  - **Phase A (done):** moved `spawn_broker.c`/`crt_spawn_broker.h` into
    `libc/src/arch/windows/legacy_spawn_broker/` (kept, not deleted, but
    excluded from `libc/CMakeLists.txt`'s `CRT_SYSCALL_FILE`); reverted the
    three `__crt_windows_is_unregistered_clone()` branches in
    `__crt_sys_open()`/`__crt_sys_pipe()`/`__crt_sys_posix_spawn()`
    (`libc/src/arch/windows/common/syscall.c`) back to their pre-broker
    direct-`CreateFileA`/`CreatePipe`/`CreateProcessA` form; removed the
    `CRT_SPAWN_BROKER_MODE` dispatch from `crt1.c`. Full `ctest` stays green
    (78/78) -- current test coverage does not exercise fork-then-spawn from
    inside a clone directly, so this is a safe mechanical revert. **Known,
    accepted regression:** until the new `fork()` lands, any real scenario
    that needs a forked clone to spawn a further process (e.g. a subshell
    inside `configure` running the compiler) will fail again the same way it
    did before the broker existed; `libpng`'s build (see later) is blocked
    on this.
  - **Phase B (done):** `/DYNAMICBASE:NO` turned out to be rejected by the
    linker on aarch64 (`lld-link: error: /dynamicbase:no is not compatible
    with arm64` -- ARM64 PE images must always be relocatable, so there is
    no link-time way to disable image ASLR on this architecture at all).
    Verified instead with `STARTUPINFOEXA` +
    `UpdateProcThreadAttribute(..., PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY,
    ...)` setting `PROCESS_CREATION_MITIGATION_POLICY_BOTTOM_UP_ASLR_ALWAYS_OFF
    | _HIGH_ENTROPY_ASLR_ALWAYS_OFF` at `CreateProcessA` time (a per-process
    creation attribute, not a PE image characteristic): a probe executable
    spawned this way landed its first `malloc()` and a stack-local variable
    at byte-identical addresses across 2 independent top-level launches x 20
    children each (42/42 matching); the image's own code address was
    already deterministic on this system even *without* the mitigation
    policy, so only heap/stack needed it. This is architecture-independent
    in principle (a process-creation attribute, not a link flag), though
    only verified on aarch64 so far. **The Phase C address-matching
    assumption is confirmed feasible on real Windows aarch64 hardware.**
  - **Phase C (done, aarch64):** `__crt_sys_fork()` now dispatches to a new
    `__crt_windows_memcopy_fork()`
    (`libc/src/arch/windows/aarch64/fork_memcopy.c`) on aarch64; x86_64
    keeps the original `RtlCloneUserProcess` path untouched. `ctest` was at
    76/78 immediately after this landed (two known-open gaps in the
    startup self-relaunch's fd inheritance, since fixed -- see the
    "fd-inheritance gap" update later). Several things came up that the
    design write-up earlier didn't anticipate:
    - `malloc.c`'s `block_header` split chain is not the same thing as the
      underlying OS `mmap()`/`VirtualAlloc()` region boundaries -- a single
      64KB chunk gets subdivided into several non-64KB-aligned sub-blocks
      once anything allocates from it, and `VirtualAllocEx()` requires an
      explicit `lpAddress` to be allocation-granularity-aligned. Fixed by
      adding a separate, dedicated OS-region tracking table in `malloc.c`
      (`__crt_malloc_os_region_count()`/`_base()`/`_size()`, populated only
      in `append_chunk()`), independent of the block-split bookkeeping.
    - The child's `CONTEXT.Pc` was originally redirected to a small
      trampoline function that itself called `longjmp()` on a `jmp_buf`
      copied into the child's memory (matching the original design). This
      reproducibly crashed (`STATUS_ACCESS_VIOLATION`, DEP/execute
      violation, inside `longjmp()`'s own restore sequence) -- a
      `ReadProcessMemory()` readback taken immediately before
      `ResumeThread()` confirmed the copied `jmp_buf` bytes were correct
      at that point, but had become all-zero by the time the child's own
      code tried to read them (bisected with `WaitForDebugEvent()`/
      `ContinueDebugEvent()`, since in-process exception handlers weren't
      reliable here; exact mechanism not fully isolated). Fixed by
      skipping the trampoline/`longjmp()` indirection entirely: the parent
      already holds every register value the trampoline would have read
      from memory, so it writes them straight into the child's `CONTEXT`
      itself via `SetThreadContext()` and never asks the child to read
      resume state back out of memory at all -- a more robust design, not
      just a workaround.
    - Also newly required: fork()-capable processes self-relaunch once at
      startup under the same mitigation policy
      (`libc/src/arch/windows/aarch64/fork_capable_relaunch.c`,
      `__crt_windows_ensure_fork_capable_relaunch()`) -- Phase B only
      verified that *children spawned under the policy* get deterministic
      addresses, not the *original process itself* (ordinary ASLR), so a
      later `fork()` call's own addresses would never have matched a
      mitigated child's without this. This went through two more
      revisions: the "did I already relaunch" check moved from an
      inheritable env var marker (wrong -- survives `execve()` even when
      the new process image is not actually mitigated) to querying
      `GetProcessMitigationPolicy()` directly; and the relaunch itself
      moved from unconditional (every Windows aarch64 process, which broke
      external-command stdio entirely -- see docs) to opt-in per target via
      a weak symbol, linked only into `crt_mksh` and the ctest suite.
    - See `docs/windows_fork_emulation.md`, "Spawn Broker Retired", for the
      full account including known limitations (only the calling thread's
      stack survives into the child, per POSIX; no guard-page preservation
      past what was committed at fork time; x86_64 not yet ported at the
      time this was written -- see the 2026-08-06 "Ported Windows aarch64's
      Cygwin/MSYS-style memory-copy `fork()` ... to x86_64" entry for the
      x86_64 port).
    - **Update: fd-inheritance gap across the self-relaunch, fixed.** The
      startup self-relaunch's `CreateProcessA()` hop only forwarded the 3
      standard handles, so any other fd the process itself had received
      (e.g. via `posix_spawn_file_actions_adddup2()`) was silently lost --
      the 2 remaining `ctest` failures (`shell_smoke_test`,
      `windows_fd_snapshot_test`) both exercised exactly this. Fixed by
      reusing the exact mechanism `__crt_sys_posix_spawn()` itself already
      uses for every ordinary spawn (explicit `DuplicateHandle()`-into-
      child + a suspended-child-and-pipe handoff, not bare Windows handle
      inheritance) for the relaunch hop too: three new functions in
      `syscall.c` (`__crt_windows_fd_snapshot_relaunch_begin/_finish/
      _abort()`, declared in `private/crt_fd_table.h`) let
      `fork_capable_relaunch.c` export/duplicate/hand off the current fd
      table without needing its own `fd_table` access. This also required
      reordering `crt1.c` so `__crt_child_bootstrap()` (which imports an
      *incoming* fd snapshot into this process's own fd table) runs
      *before* the relaunch check, not after -- otherwise the relaunch's
      own export would only ever see the default fd 0/1/2 regardless of
      what this process itself had just received. `ctest` is back to
      78/78. As a side effect, this also fixed the previously-unexplained
      `mksh -c "exec mksh -c '...'"` recursive hang (verified stable across
      repeated runs and 3-deep nesting) -- it was the same gap: the
      exec'd-into relaunch was losing the inherited pipe/fd state the
      outer mksh needed.

- Started running libpng's real `configure && make && make install` through
  project-owned mksh/make on Windows aarch64 (`port-rebuild-libpng`, depends
  on zlib already installed to `PORT_PREFIX`). Not passing yet, but found and
  fixed three real, independent CRT/mksh bugs along the way, each verified
  against the full `ctest` suite (78/78 at the time) with no regressions:
  - **`regcomp()`/`regexec()` never implemented capture groups**
    (`libc/src/regex.c`): `\( \)` (BRE) / `( )` (ERE) were either treated as
    literal parenthesis characters to match (BRE) or silently miscounted
    into `re_nsub` (both modes), so `regmatch_t` subexpression bounds were
    never populated. This broke the extremely common autoconf idiom `expr
    "x$opt" : 'x--[^-]*-\(.*\)'` used to parse `--enable-X`/`--disable-X`/
    `--prefix=VALUE` style flags -- `./configure --disable-shared
    --enable-static --prefix=...` was silently corrupted into `--disable-0
    --enable-0 --prefix=0` before libpng's (autoconf-generated, unlike
    zlib's hand-written) `configure` ever got going. Rewrote the matcher to
    track real capture groups through backtracking (see `regex_test.c`),
    and fixed `re_nsub` counting to be BRE/ERE-mode-aware instead of always
    counting unescaped `(`.
  - **mksh never recognized Windows drive-letter paths as absolute**
    (`shell/mksh/src/sh.h`, `mksh_abspath()`): mksh already ships a full
    DOS-path-aware implementation behind `MKSH_DOSPATH`, but that flag also
    switches `PATH`/`CDPATH` to `;`-separated, which conflicts with this
    project's deliberate `:`-separated rootfs `PATH` convention
    (`/system/bin:/bin:/usr/bin`, see `tools/crt-port-build.py`). Added a
    narrower, CRT-owned `MKSH_CRT_WINPATH` define (alongside the existing
    `MKSH_CRT_ALLOW_LLP64`/`MKSH_CRT_SHELL_CHILD_SPEC`) that only patches
    `mksh_abspath`/`mksh_cdirsep`/`mksh_sdirsep` to recognize `X:\`/`X:/`
    and `\` as well as `/`, without touching `MKSH_PATHSEPC`. Without this,
    `cd "$(pwd)"` (autoconf's own `ac_pwd`/`ls -di .` working-directory
    sanity check) silently treated the Windows-native absolute path
    `getcwd()` returns as *relative*, doubling it into `<dir>/<dir>` and
    failing with `configure: error: working directory cannot be
    determined`.
  - **Generic `pipe()` has the same CreateProcessA-adjacent failure as the
    spawn broker's own bootstrap pipe, but was never routed through the
    broker** (`libc/src/arch/windows/common/syscall.c`,
    `spawn_broker.c`/`crt_spawn_broker.h`): `__crt_sys_pipe()` called
    `CreatePipe()` unconditionally, with no check for
    `__crt_windows_is_unregistered_clone()`. mksh forks a real subshell
    (`RtlCloneUserProcess` clone) for every command substitution/pipeline,
    and any further pipe needed *inside* that clone (nested command
    substitution, `cmd1 | cmd2`) hit exactly the already-diagnosed
    `CreatePipe()`-fails-in-an-unregistered-clone bug -- just reached via
    plain shell usage instead of `posix_spawn()`. Extended the broker
    protocol with a `want_plain_pipe` request (broker creates a pipe
    locally and `DuplicateHandle`s *both* ends back into the client,
    instead of attaching one end to a spawned target) and made
    `__crt_sys_pipe()` route through it when inside an unregistered clone.
  - Also found and fixed a fourth real bug along the way, confirmed
    necessary but **not sufficient on its own** -- see "still open" later:
    `ERROR_INVALID_NAME` (Windows error 123, returned for any path
    containing a character Windows never allows in a real filename, e.g.
    `*`) fell through `map_windows_error()`'s default case to `EIO`
    instead of `ENOENT`. Autoconf's own exit-trap cleanup runs `rm -f core
    *.core core.conftest.*`; when the glob doesn't match anything, mksh
    passes the literal pattern through (normal, expected shell behavior),
    and a literal `*` can never exist as a real Windows filename, so
    `ENOENT` is the semantically correct mapping -- and it is exactly what
    toybox's `rm -f` checks for to stay silent
    (`shell/toybox/src/toys/posix/rm.c:110`, `errno == ENOENT`). Fixed in
    `map_windows_error()`. Verified against the full `ctest` suite (78/78).
  - **Broker named-pipe server had two independent races**, found via
    `set -x` tracing and confirmed with per-PID debug logging (removed
    once fixed): (1) the server created the *next* waiting pipe instance
    only *after* servicing the current request, leaving a real window
    (the full service time, e.g. a `CreateProcessA` call, plus
    disconnect/close/recreate) with zero pending instances -- any client
    racing to connect during that window saw `ERROR_FILE_NOT_FOUND`,
    observed as mksh's `can't create pipe - try again` failing
    `checking whether build environment is sane`. Fixed by creating the
    next instance immediately after accepting the current connection,
    before servicing it. (2) `WriteFile()` returning success only means
    the response bytes reached the kernel's pipe buffer, not that the
    client read them; the server's `DisconnectNamedPipe()` tore the
    connection down immediately regardless, intermittently losing the
    response and failing the client's `read_exact()` with
    `ERROR_PIPE_NOT_CONNECTED` (233). Fixed by calling
    `FlushFileBuffers()` (blocks until the client has read everything)
    before disconnecting. Verified with 50+ repeated runs of the exact
    failing sanity-check idiom with no further "can't create pipe"
    failures, and the full `ctest` suite (78/78).
  - **The broker client's `read_exact()`/`write_exact()` had no
    timeout**, discovered while investigating orphaned `mksh.exe`
    processes left running (0% CPU, blocked, no ancestor process left
    alive) after a build had already finished. If the broker ever fails
    to respond for any reason, the mksh subshell that asked it for a
    pipe/spawn blocks in `ReadFile`/`WriteFile` forever -- Windows does
    not cascade-kill it, so it just sits there indefinitely, invisible
    in any log because it never crashes. Rewrote the broker's pipe
    handles (both client and server ends) to use `FILE_FLAG_OVERLAPPED`
    and added a bounded (20s) wait via `WaitForSingleObject` +
    `CancelIoEx` on timeout. This did **not** fully eliminate the
    orphaned-process symptom on its own (see the `TPAREN` bug just
    later, which was the real remaining cause in the cases actually
    investigated) but is an independently correct hardening: no broker
    I/O call can block a caller forever again, regardless of cause.
  - **The real bug behind the remaining orphaned-subshell/exit-127
    symptoms: a subshell reached without `XFORK` already set skipped
    real process isolation entirely** (`shell/mksh/src/exec.c`,
    `execute()`'s top entry check). A `TPAREN` (`(...)`) is supposed to
    always get a real fork via `exchild()`, but the entry check only
    forks when the caller already passed `XFORK` -- and `TLIST`'s
    handling of the *last* item in a sequence (`case TLIST` in the same
    file) passes `flags` straight through unchanged, with no `XFORK`.
    So `{ cmd1; cmd2; (subshell); }` -- a `TLIST` whose last item is a
    `TPAREN` -- reaches the entry check with no `XFORK`, skips it
    entirely, and falls through to the `case TPAREN:` handler, which
    recurses into the subshell's own content with `XFORK` freshly
    added. If that content is a single `TCOM`, the *same* entry check's
    existing `&& t->type != TCOM` term (added for a different, already-
    fixed bug -- see "Found and fixed a real mksh/CRT-shell-child-spec
    bug..." earlier) then blocks the fork for it too, since nothing there
    distinguishes "a TCOM already isolated by a real fork" from "a TCOM
    that IS the not-yet-forked subshell's own content." Concretely:
    `(exit $ac_status)` as the last statement of a `{ ...; }` group --
    exactly the idiom automake's generated `configure` uses to probe for
    optional tools like `tar` -- ran `exit` in the *interpreter itself*
    instead of a subshell, killing the whole `./configure` script
    instead of just that one probe attempt. This is what was actually
    behind the `checking how to create a ustar tar archive` exit-127
    failure recorded later (the pipe-broker races earlier are real,
    independently-fixed bugs, but were not sufficient to explain this
    one). Fixed by making the entry check fork on `t->type == TPAREN`
    unconditionally, regardless of whether `XFORK` was already set --
    unlike every other node type, "just run me in this interpreter" is
    never correct for a subshell. Verified with a standalone repro
    (`(exit 99)` as the last statement of a `{ }` group inside a loop,
    ran 3 iterations correctly instead of dying on the first) and the
    full `ctest` suite (78/78); `checking how to create a ustar tar
    archive` now correctly resolves to `none` and configure proceeds
    well past it (through `checking for gcc`) instead of aborting.
  - **Update (post spawn-broker retirement / memory-copy fork()):** the
    `can't create conftest.err: Bad file descriptor` failure this section
    used to describe was a symptom of the spawn broker, which no longer
    exists (see "Phase C" earlier). With the broker gone and memory-copy
    `fork()` in its place, `configure` now gets *substantially* further --
    all the way past `checking whether the C compiler works... yes` (the
    exact step that used to fail) and through `checking build system
    type`/`checking host system type` -- before hanging (not crashing:
    confirmed genuinely stuck via the CPU-delta technique, unchanged CPU
    across a 10s window) at the very next step, `checking for a sed that
    does not truncate output`. This is autoconf's own self-test that
    builds a `sed` script by repeatedly doubling a fixed pattern string,
    then pipes the whole thing through `sed` to find the length where
    truncation starts. **Root-caused and fixed.** Re-ran with
    `CRT_PORT_SHELL_XTRACE=1` and caught the exact stuck command via a
    live `Monitor` on the trace log: `echo "$ac_script" | sed 99q
    >conftest.sed`, where `$ac_script` is an ~11 KB doubled string. `echo`
    is a shell builtin; mksh's `MKSH_CRT_SHELL_CHILD_SPEC` Windows port
    (`shell/mksh/src/jobs.c`'s `exchild()`) skips a real `fork()` for a
    `TCOM` pipeline stage to avoid this platform's expensive memory-copy
    `fork()` when it turns out to be an external command -- but when the
    stage is a *builtin* instead, it runs synchronously in-process with no
    concurrent reader forked yet, and its `write()` into the pipe
    (`CreatePipe()`'s default buffer, ~4096 bytes) blocks forever once it
    exceeds the buffer. Binary-searched the exact threshold with a minimal
    `echo "$s" | wc -c` reproduction: 4051 bytes OK, 4101+ hangs
    indefinitely (confirmed via `timeout`, not just CPU-delta). Fixed by
    giving every `CreatePipe()` call in `syscall.c` (the generic `pipe()`
    syscall, the posix_spawn() fd-snapshot bootstrap pipe, and the
    fork-capable self-relaunch's fd handoff added just earlier) an explicit
    4 MiB buffer (`CRT_PIPE_BUFFER_SIZE`) instead of the system default --
    the latter two share the exact same synchronous-write-before-resume
    shape and would have hit the identical deadlock for a large enough fd
    table/snapshot, just not yet observed in practice. Verified: the
    isolated repro now succeeds well past the old threshold; `ctest` stays
    at 78/78; a full libpng `configure` re-run sails straight through the
    sed self-test and `checking for grep that handles long lines and -e`,
    reaching a *new*, much later, and non-hanging stopping point: `checking
    for egrep... configure: error: no acceptable egrep could be found` --
    `egrep`/`fgrep` are simply missing from `tools/create_rootfs.py`'s
    `TOYBOX_APPLETS` alias list (toybox's `grep.c` natively supports both
    as `OLDTOY` aliases of `grep`; just need to be added to the list and
    have their `USE_EGREP`/`USE_FGREP` config macros enabled in this
    project's hand-picked toybox build). See
    `docs/windows_fork_emulation.md`, "Windows Pipe Buffer Size", for the
    full writeup.
  - **Update: egrep/fgrep alias, ERE alternation, and pipe-lseek, all
    fixed; configure now blocked on a plain missing `ld`.** Three more
    real, independent bugs found and fixed chasing `checking for
    egrep`/`fgrep`, in order:
    1. `egrep`/`fgrep` weren't just missing from `tools/create_rootfs.py`'s
       `TOYBOX_APPLETS` alias list -- this project also doesn't use
       toybox's Kconfig `.config`; it hand-lists every enabled applet in
       `shell/toybox/crt/generated/newtoys.h`, which had no `OLDTOY(egrep,
       grep, ...)`/`OLDTOY(fgrep, grep, ...)` entries at all (toybox's own
       `grep.c` already supports both natively). Added both to
       `newtoys.h` and to `TOYBOX_APPLETS`.
    2. `grep -E 'bar|baz'` didn't match either alternative -- `|` was
       matched as a literal character. `libc/src/regex.c` was a 370-line
       hand-rolled backtracking matcher with **no alternation support at
       all** (not a regression; it was simply never implemented). Per
       explicit instruction, replaced it wholesale with the real Bionic/
       NetBSD Henry-Spencer strip-VM regex engine (`libc/src/regex/`,
       ported from `libc/upstream-netbsd/lib/libc/regex/` on Bionic
       `main`) -- full POSIX BRE/ERE, backreferences, bounded repetition,
       POSIX character classes, opt-in GNU BRE extensions. See
       `third_party/bionic/README.md`'s "Regex Tranche" for the full file
       list and adaptation notes (the interesting one: this project's real
       `wint_t` is `unsigned short`, which breaks the engine's negative
       sentinel comparisons, so the NLS/real-wide-char path is
       deliberately left off in favor of utils.h's own signed-`short`
       fallback -- see `libc/src/regex/netbsd-compat.h`'s top comment).
       Also added `reallocarray()` (`libc/src/reallocarray.c`) and
       `MB_LEN_MAX` (`include/limits.h`), both needed by the ported
       engine and missing from this libc before now.
    3. Even with alternation working, `grep`/`egrep`/`fgrep` still matched
       *nothing at all*, on *any* pattern, but only when reading from a
       **pipe** (a real file argument worked fine) -- root-caused to
       toybox `grep.c`'s "only run binary-file sniffing on lseekable fds"
       guard (`!lseek(fd, 0, SEEK_CUR)`): this project's Windows
       `__crt_sys_lseek()` called `SetFilePointerEx()` on whatever handle
       it was given with no check for whether it was actually seekable,
       so the guard's intended skip-on-pipe behavior silently didn't
       trigger -- grep's binary-sniffing peek-and-rewind ran on piped
       stdin too, consumed the pipe's data during the peek, and (since a
       pipe can't be rewound) never got it back, so the real read loop
       that followed started from an already-drained pipe. Fixed by
       checking `GetFileType(handle) == FILE_TYPE_PIPE` up front and
       returning `-ESPIPE`, matching POSIX `lseek(2)` on a pipe/FIFO.
       Regression-covered in `tests/fd_errno_test.c`.

    All three verified together: `ctest` 78/78 (plus new `regex_test.c`
    coverage for alternation/bounded-repetition/backreferences/POSIX
    classes/case-insensitivity, and the `fd_errno_test.c` lseek-on-pipe
    check); a full libpng `configure` re-run now sails through `checking
    for egrep`/`checking for fgrep` (`... /system/bin/grep -E` / `-F`) and
    reaches yet another new, later, ordinary (non-hanging) stopping point:
    `configure: error: no acceptable ld found in $PATH` -- this project's
    rootfs has no standalone `ld` binary at all (it drives `lld-link`/
    `crt-cc` directly, never a bare `ld`); next step is likely either
    aliasing one or making libpng's `configure` accept the existing
    toolchain wrapper instead. Not yet investigated.
  - **Update: `ld` not found, fixed.** `crt-cc` already links via
    `-fuse-ld=lld` (`tools/crt-cc`), so `ld.lld.exe` (shipped by the LLVM
    install) is this project's real linker backend already -- it just
    wasn't reachable under any name/location libtool's `AC_PROG_LD`
    ("checking for non-GNU ld") search would find, because native-Windows
    `--use-crt-shell` configure runs with `PATH` hard-restricted to this
    project's own rootfs (`/system/bin:/bin:/usr/bin`), unlike macOS/
    Linux, which append the *host* PATH and so already have a real system
    `ld` there (Xcode CLT / binutils) -- this was a Windows-only gap.
    Fixed in `tools/crt-port-build.py`'s `make_env()`: pre-set `$LD` to
    `ld.lld.exe`'s real path (found the same way `CRT_HOST_CC`/
    `CRT_HOST_CXX` already are, via `find_windows_host_tool()`, wrapped
    with the same `CRT_SPAWN_NATIVE_WINDOWS=1` prefix AR/RANLIB/STRIP
    already use) -- autoconf/libtool only search `PATH` for `ld` when
    `$LD` isn't already set, so this skips the broken search entirely.
    Verified: `checking for non-GNU ld... ...ld.lld.exe`,
    `checking if the linker (...) is GNU ld... no` (ld.lld's `-v` banner
    says "compatible with GNU linkers", not literally "GNU", so libtool
    correctly treats it as non-GNU-but-compatible), and configure sails
    through the entire libtool linker/shared-library-support detection
    phase (`checking whether ... linker ... supports shared libraries...
    yes`, ranlib/strip detection, PIC flags) to a new, much later, and
    completely different next blocker: `checking if awk () works...
    inaccessible or not found` / `configure: error: ... no` -- this
    project's rootfs has no `awk` at all yet (`checking for gawk/mawk/
    nawk/awk... no` earlier in the same log; toybox's own `awk.c` is a
    `pending`, not-yet-enabled applet).
  - **Update: real AWK ported and working.** Per explicit direction,
    ported Brian Kernighan's reference `onetrueawk` (NetBSD/many BSDs'
    own system awk) into `shell/awk/`, built with this project's own CRT
    like `mksh`/`toybox` -- deliberately *not* solved by pointing
    configure at a host-installed awk (e.g. Git for Windows' bundled
    `gawk.exe`), which would be an uncontrolled, per-machine, different-
    runtime dependency breaking the same self-containment principle that
    justified building `mksh`/`toybox` in the first place. See
    `shell/awk/README.md` and `import_manifest.json` for the full
    writeup; summary:
    - Installed `win_flex_bison` (via `winget`) to generate
      `awkgram.tab.c`/`.h` from upstream's `awkgram.y`, and built/ran
      upstream's own `maketab.c` (as a native host tool, via this
      project's own `crt-cc` against its own sysroot) to generate
      `proctab.c`. Both generated outputs are vendored as pristine,
      checked-in files (matching how `shell/toybox/crt/generated/*.h`
      are already vendored rather than regenerated at build time) -- this
      project's CMake build gained no new bison/yacc dependency.
    - One adaptation to upstream source: `parse.c`'s `ptoi()`/`itonp()`
      pointer-smuggling helpers cast through `long`, truncating on
      Windows LLP64; changed to `intptr_t`.
    - Filled several real, general (not awk-specific) CRT/libm gaps found
      compiling and then actually *running* awk programs: `<stdnoreturn.h>`
      (missing entirely), `atan2()`/`atan()` (FreeBSD msun, a separate
      upstream from the regex import), `system()`, `rand()`/`srand()`/
      `random()`/`srandom()` (a rand48-family LCG, not a literal port of
      BSD's own proprietary `random()` -- POSIX doesn't mandate a specific
      sequence), `SIGFPE` `FPE_*` `si_code` constants, and `popen()`/
      `pclose()`.
    - Found and fixed a real, general `printf`/`snprintf` bug this way
      too: `%g` with an *explicit* precision (`%.6g`, `%.30g`, ... --
      onetrueawk's own number-to-string conversion always uses `%.30g`)
      zero-padded the already-rendered digit string a second time (e.g.
      `%.6g` of `4.0` printed `"000004"`, not `"4"`) because
      `format_double_general()`/`format_long_double_general()`
      (`libc/src/printf.c`) forgot to clear `spec->precision_set` before
      their final `write_formatted()` call, unlike their fixed-point/
      exponential siblings which already did. No prior test had ever
      exercised `%g` with a non-default precision. Regression-covered in
      `tests/printf_test.c`.
    - Verified: `ctest` 79/79 (new `crt_awk_basic_runs` plus the printf
      regression cases earlier); manual smoke covering print/field-split/
      pattern-match/arrays/`printf`/`split`/`sqrt`/`atan2`/`rand`/`srand`/
      `getline`-from-`popen` all correct. A full libpng `configure`
      re-run now passes `checking for gawk... (cached) awk` /
      `checking if awk (awk) works... yes` and reaches yet another new,
      much later, and completely different next blocker: `checking for
      zlibVersion in -lz... no` / `configure: error: zlib not installed`
      -- despite zlib's own install stamp already being present in
      `PORT_PREFIX`; not yet investigated (a link/library-path issue,
      not a missing-tool issue like everything earlier).
  - Continued past that: `checking for zlibVersion in -lz... no` turned out
    to be a COFF-vs-Unix static-library-naming mismatch, not a missing
    build. This project's toolchain links via `clang -fuse-ld=lld`, whose
    lld-link backend resolves `-lfoo` to a file literally named `foo.lib`
    (matching this project's own CMake-built libs, e.g. `c.lib`/`m.lib`),
    but zlib's own autoconf/make install produces the Unix-conventional
    `libz.a`, which `-lz` could never find. Fixed generally (for all future
    Windows ports, not just zlib) with a new post-install step,
    `alias_unix_static_libs_for_windows_link()` in `tools/crt-port-build.py`:
    after every port build, copies each installed `libfoo.a` to `foo.lib`
    alongside it (skipped if `foo.lib` already exists). Verified: rebuilding
    zlib now also produces `z.lib`, and a full libpng `configure` re-run
    reaches `checking for zlibVersion in -lz... yes`.
  - Next blocker after that: `fatal error: 'windows.h' file not found`
    (`pngpriv.h:569`, guarded by
    `#if defined(_WIN32) || defined(__WIN32__) || defined(__NT__)`). This
    project's sysroot has no real Windows SDK `<windows.h>` (its own Win32
    API surface is declared privately inside `libc/src/arch/windows/`, not
    exposed publicly). Nothing near that include in
    `pngpriv.h`/`png.c`/`pngerror.c` actually references a real Windows API
    symbol from it, so -- matching the exact technique zlib's own recipe
    already uses (`porting/recipes/zlib.json`'s `-U_WIN32` etc. `CFLAGS`) --
    fixed by adding the same `-U_WIN32 -U_WIN32_WCE -U__WIN32__ -UWIN32
    -U__NT__ -U_MSC_VER` `CFLAGS` to `porting/recipes/libpng.json` (plus the
    extra `-U__NT__`, since libpng's own Windows guard also checks that
    macro), keeping libpng on its generic/POSIX code paths.
  - Next blocker after that: `fatal error: 'arm_neon.h' file not found`
    (`pngrtran.c:26`). Root-caused to `tools/crt-cc`/`tools/crt-c++`
    hardcoding `resource_dir=""` on Windows, entirely skipping clang's own
    `-print-resource-dir` query -- meaning compiler-provided architecture-
    intrinsic headers (`arm_neon.h`, `immintrin.h`, ...) were never on the
    Windows include path at all. Also found a compounding latent bug while
    fixing this: both scripts build `common_flags`/`user_args`/`libs` as
    plain strings and pass them via *unquoted* expansion, relying on word-
    splitting to become separate argv entries -- which silently breaks any
    single value containing a space, and a stock Windows LLVM install's
    resource-dir is almost always under `"C:\Program Files\..."`. Fixed
    both scripts by always querying `resource_dir` (all platforms), and
    passing `-isystem "$resource_dir/include"` as its own separately-quoted
    argument pair directly on each `exec` line, instead of folding it into
    the unquoted `common_flags` string. Verified: `ctest` 79/79 still
    passing (these scripts compile everything in the Windows build, so this
    was the highest-stakes check of this whole chain); a standalone
    `crt-cc` compile of a translation unit including `<arm_neon.h>` now
    succeeds.
  - Next blocker after that: `png.c` compiled, but `libtool`'s own link
    step picked `lib -OUT:...` (the MSVC-native static archiver, which
    this project has no `lib.exe` for) instead of using `$AR`. Root-caused
    to the *value* previously used for `AR`/`RANLIB`/`STRIP`/`LD`: the
    literal string `"CRT_SPAWN_NATIVE_WINDOWS=1 <tool-path>"`, relying on
    the calling shell to recognize that leading `VAR=val` as an
    environment-assignment prefix whenever the value is expanded
    unquoted. POSIX shells only ever do that for literal, parsed-at-parse-
    time source text -- never for a variable's word-split expansion at
    runtime. Makefile recipes happened to work anyway (make substitutes
    `$(AR)` textually into a *fresh* shell command line each time), but
    libtool's own "is `$LD` GNU ld" probe inside `configure` -- `` `$LD
    -v` ``, a command substitution of an already-parsed variable -- tried
    to run a program literally named `CRT_SPAWN_NATIVE_WINDOWS=1`, got
    "not found", and silently concluded `with_gnu_ld=no`, which is what
    sent libtool down the wrong archiving path. Fixed by adding
    `tools/crt-native-tool`, a real wrapper script (the same proven
    pattern as `tools/crt-cc`'s own `$CC` value: `"<mksh> <script>"`) that
    does `export CRT_SPAWN_NATIVE_WINDOWS=1; exec "$tool" "$@"` --
    `AR`/`RANLIB`/`STRIP`/`LD` now point at `"<mksh> tools/crt-native-tool
    <real-tool>"`, which only ever needs the calling shell to word-split a
    command name from its arguments (always reliable).
  - Next blocker after that: `with_gnu_ld` was *still* `no` even with `$LD`
    now correctly probed as GNU-compatible -- a second, independent cause:
    libtool's own per-tag config has `case $host_os in cygwin*|mingw*...)
    if test yes != "$GCC"; then with_gnu_ld=no; fi`, and `$GCC` (was our
    compiler detected as GNU-compatible at all?) was also `no`. Root-caused
    to a stock Windows LLVM install's `clang.exe` defaulting to its own
    host triple, `*-pc-windows-msvc`, whenever no `--target` is given --
    that predefined-macro set doesn't define `__GNUC__` at all (only
    `_MSC_VER`, mimicking real MSVC), which autoconf's near-universal
    "checking whether the compiler supports GNU C" probe (and libtool's
    check earlier) both read as "not GNU". Fixed by adding an explicit
    `--target=aarch64-w64-mingw32` (arch auto-detected, `CRT_TARGET_ARCH`
    overridable) to `tools/crt-cc`/`tools/crt-c++` on Windows, matching
    what this project's own `--build=aarch64-w64-mingw32` recipe
    workaround already expects. Verified harmless to this project's own
    build (we pass `-nostdinc`/`-nostdlib` and our own `-isystem`/`-L`
    throughout, so the triple only affects predefined macros and calling-
    convention details, not which headers/libs get used).
  - With both of those fixed, `png.c` through the last `.c` file compiled,
    archived via the correct `$AR` path, and linked into `libpng16.a` and
    every `contrib/tools`/`contrib/libtests` sample binary -- `make`
    completed in full. Then `make install` hit one more, final blocker:
    `./libtool: ./install-sh: can't execute: Permission denied`. Root-
    caused to two compounding gaps, both now fixed in
    `libc/src/arch/windows/common/syscall.c`:
    1. `__crt_sys_posix_spawn()` (which `execve()` is itself implemented
       on top of, via `posix_spawn()` + `waitpid()` + `_exit()`) could
       previously only ever launch real PE binaries -- nothing in this
       project's PAL had ever taught it to interpret a `#!` shebang line,
       since every script run so far had always been invoked with its
       interpreter spelled out explicitly. `install-sh` (execed directly
       by libtool's own `--mode=install`, no interpreter prefix) is the
       first thing this project has hit that assumes shebang execution
       works at the OS level. Added `windows_read_shebang()` and taught
       `__crt_sys_posix_spawn()` to re-resolve and re-exec the named
       interpreter (Linux kernel semantics: at most one unsplit optional
       argument) when the target file starts with `#!`.
    2. That alone wasn't enough: mksh's own command dispatch
       (`search_access()` in `shell/mksh/src/exec.c`) checks
       `access(path, X_OK)` *before* ever calling `execve()`, and this
       project's `stat()`/`access()` emulation only ever reported
       `S_IXUSR`/etc. for files with a real PE `"MZ"` signature (Windows
       has no on-disk executable-permission bit at all, and NTFS ACLs
       don't map onto `S_IXUSR` either, so file *content* has always been
       the only signal available). A `#!` script has no MZ signature, so
       mksh rejected `install-sh` as "can't execute: Permission denied"
       before fix 1's shebang logic ever ran. Renamed the helper
       (`windows_handle_has_mz_signature` ->
       `windows_handle_looks_executable`) and taught it to also recognize
       a `#!` prefix, used by both `stat()`'s executable-bit computation
       and the PATH-search "is this a candidate executable" check.
    Verified: `ctest` 79/79 still passing at every step earlier (the
    `stat()`/`access()` change in particular is broad -- every executable-
    permission check in the whole Windows build goes through it).
  - **End-to-end result: libpng 1.6.57's real `configure && make && make
    install` now completes in full on Windows aarch64** -- `libpng16.a`/
    `libpng.a` built, archived, and installed into `PORT_PREFIX`, along
    with all of libpng's own `contrib/tools` and `contrib/libtests` sample
    binaries, `install-sh`-driven header/pkgconfig/man-page installation,
    and the `libpng16`->`libpng` alias-copy `install-exec-hook`/
    `install-data-hook` steps. This was the single longest blocker chain
    of this whole session -- ld, awk, a real printf bug, static-lib
    naming, `windows.h`, `arm_neon.h`, `AR`/`LD` shell-quoting, the GNU-
    ld/GNU-C misdetection, and finally shebang-script execution -- each
    one a real, general CRT/PAL/tooling gap fixed on its own merits, not
    a libpng-specific workaround.
  - Checked whether any of the fixes earlier are Windows-specific-only or
    could affect macOS/Linux (couldn't literally build for those hosts
    from this Windows aarch64 machine -- static review only): the
    `lseek()` fix lives entirely in `libc/src/arch/windows/common/
    syscall.c` (macOS/Linux use raw `syscall.S` kernel syscalls, which
    already return `ESPIPE` for pipes correctly, unaffected); the `LD`
    env var fix is explicitly `if target_os == "windows"`-scoped; the
    regex engine port, `reallocarray()`, `MB_LEN_MAX`, and the `egrep`/
    `fgrep` alias additions all live in shared, non-OS-forked files with
    no Windows-specific API references, so they apply identically (and
    identically safely, given `NLS` is deliberately never defined
    regardless of platform) across all three targets.

## 2026-08-04

- Attempted to fix a real (if currently low-impact) gap in the spawn broker:
  every process it spawns shows up in Windows' own process tree as a child
  of the broker, not of the clone that logically requested it (flat instead
  of nested in Task Manager/Process Explorer/any future toybox `ps
  --forest`; `ps` itself is not enabled yet, `CFG_PS 0`). Tried the official
  `PROC_THREAD_ATTRIBUTE_PARENT_PROCESS` mechanism (same one `explorer.exe`
  uses for UAC-elevated children). Found and fixed one real bug along the
  way (inheritable handles are sourced from the *specified parent's* handle
  table with this attribute, not the actual `CreateProcessA` caller's --
  the fd-snapshot bootstrap pipe's read end had to move from
  "inheritable in the broker" to "duplicated into the client, inheritable
  there"), but hit a second, worse regression that was never fully
  isolated: spawned targets started failing entirely with
  `STATUS_DLL_INIT_FAILED` (a Windows loader-level failure, before the
  target's own `main()` ever runs), and -- the important part -- disabling
  just the reparenting attribute did **not** reliably fix it, meaning
  something in this line of changes broke the plain, non-reparented spawn
  path too, not just the reparented one. Since the actual acceptance test
  (`port-rebuild-zlib`'s real `configure`/`make`/`make install`) regressed
  back to failing, reverted both changed files
  (`libc/src/arch/windows/common/spawn_broker.c` and `.../syscall.c`) via
  `git checkout --` to the last known-good commit rather than ship a
  half-fixed state. Confirmed the revert restores the working state (zlib
  passes, `ctest` 77/77). Full blow-by-blow, what to try differently next
  time, and why job-object inheritance was ruled out as the cause: see
  `docs/windows_fork_emulation.md`, "Attempted And Reverted: Reparenting
  Spawned Processes To The Client".

- Made real `fork()` work on Windows aarch64 (`RtlCloneUserProcess` is
  exported there too; the previous x86_64-only guards in
  `libc/src/arch/windows/common/syscall.c` were simply untested
  assumptions) and fixed a genuine `fd_set_inherit_for_fork()` bug found
  along the way (fd 0/1/2 were never marked inheritable, breaking
  `2>&1` inside a forked subshell). This alone didn't make
  `configure`-driven builds work, though: `CreateProcessA()` (and, it
  turned out, `CreatePipe()`) both crash/fail when called from inside an
  unregistered `RtlCloneUserProcess` clone, because the clone never goes
  through CreateProcess's CSRSS registration handshake. Benchmarked the
  cost of routing spawns through `CreateProcessA` instead of raw fork
  (~1.2x, not the order-of-magnitude Cygwin reputation suggests),
  researched prior art (no documented CSRSS re-registration method
  exists; ruled out as too fragile), and built a "spawn broker": `fork()`
  stays untouched (still cheap `RtlCloneUserProcess`), but
  `__crt_sys_posix_spawn()` now detects when it is running inside an
  unregistered clone and, in that case, asks an always-running, never-
  cloned broker process (`libc/src/arch/windows/common/spawn_broker.c`,
  protocol in `libc/include/private/crt_spawn_broker.h`) to create the
  pipe and the real target process on its behalf, handing the resulting
  handles back via `DuplicateHandle`. Verified end to end on real Windows
  aarch64 hardware: `cmake --build --preset windows-host-ninja-debug
  --target port-rebuild-zlib` now completes zlib's full
  `configure && make && make install` with exit 0 (previously failed at
  `can't fork - try again`, then at a `CreateProcessA` crash, then at a
  `CreatePipe()` failure -- each fix exposing the next layer). Full
  `ctest` stays green (77/77) throughout. See
  `docs/windows_fork_emulation.md`, "Chosen Direction: Spawn Broker", for
  the full investigation, the benchmark numbers, the prior-art research,
  and the rejected alternatives (CSRSS re-registration, full Cygwin-style
  memory-copy `fork()`).

## 2026-08-03

- Fixed Windows aarch64 compile errors (`init_ntdll`/`fd_set_inherit_for_fork`
  unused-function under `-Werror`): both only backed the x86_64-only
  `RtlCloneUserProcess` fork path and were genuinely dead code on aarch64;
  guarded behind the same `#if defined(__x86_64__) || defined(_M_X64)`
  already used at their call site.
- Fixed 3 Windows aarch64 fork test failures (`fork_test`,
  `fork_signal_test`, `fork_runtime_reset_test`): only one of four
  `fork()`/`_Fork()` call sites treated Windows `ENOTSUP` as an expected,
  graceful pass; extended the same handling to the other three.
- Fixed 3 Windows aarch64 mksh rootfs ctest failures
  (`crt_mksh_rootfs_external_runs`/`_pipeline_runs`/`_command_substitution_runs`):
  root cause was a stale/missing `rootfs` build artifact, not a code bug --
  the `rootfs` CMake custom target had no `ALL` and nothing forced it to
  rebuild before ctest ran. Made `rootfs` part of `ALL` on Windows (the only
  host where any ctest entry depends on it); macOS/Linux keep it opt-in.
- Found and fixed a real mksh/CRT-shell-child-spec bug while investigating a
  separate, silent (`zero output, exit 1`) `port-rebuild-zlib` `./configure`
  failure on Windows aarch64: `MKSH_CRT_SHELL_CHILD_SPEC`'s `exchild()` fast
  path incorrectly ran `TPAREN` (subshells) in-process like `TCOM`, so a
  subshell's own redirection (e.g. `(cmd) 2>/dev/null`) permanently
  clobbered the interpreter's real stderr with nothing to restore it,
  silently swallowing every later error in the same script. Fixed by
  restricting the fast path to `TCOM` only (`shell/mksh/src/jobs.c`) --
  and found a second, independent copy of the same guard inside
  `execute()` itself (`shell/mksh/src/exec.c`), reached directly by
  `comsub()` (backtick/`$(...)` substitution) without ever going through
  `exchild()`, which is why the `jobs.c` fix alone did not change the
  observed behavior; fixed the same way. See
  `docs/windows_fork_emulation.md` for the full diagnosis. This does not make
  `zlib`'s `configure` pass on Windows aarch64 (still needs real `fork()`
  there), but turns the silent corruption into an honest `can't fork - try
  again` failure, and fixes a latent version of the same bug on Windows
  x86_64 (where real fork already exists).

- Fixed the `port-rebuild-zlib` `make -j 10` deadlock: `sigaction()`/
  `sigprocmask()` previously only updated process-local bookkeeping with no
  real OS-level signal delivery, so GNU make's jobserver `pselect()` could
  never be interrupted by a real `SIGCHLD`. Added a per-OS
  `crt_signal_backend` (macOS: real `sigaction`/`sigprocmask` via a shared
  Mach-O export-trie helper now also reused by `libdl`; Linux: raw
  `rt_sigaction`/`rt_sigprocmask` syscalls plus an x86_64 restorer
  trampoline; Windows: honest no-op stub) and fixed a separate `pselect()`
  lost-wakeup race (`libc/src/poll.c`) where an already-pending signal was
  silently swallowed by the non-atomic mask-then-select sequence. Verified
  against the real `port-rebuild-zlib` `configure && make -j 10 && make
  install` end to end on macOS. See `docs/signal_delivery.md`.

## 2026-08-02

- Established `shell/` as a core CRT artifact area, not a third-party port
  recipe.
- Built `crt_tiny_sh`, Android `external/mksh`, and Android `external/toybox`
  through CMake.
- Generated an Android-like rootfs with `/system/bin`, `/bin`, `/usr/bin`,
  `/tmp`, `/dev`, and `/proc/self`.
- Installed mksh and the minimal configure-oriented toybox applet set into the
  rootfs.
- Kept POSIX hosts on symlink aliases and Windows on copy-based `.exe` aliases.
- Added the first Windows shell child process contract:
  - cwd/rootfs/env propagation;
  - fd snapshot export/import;
  - file actions and close-on-exec filtering;
  - child registry integration;
  - `waitpid()` coverage;
  - socket fd transport through `WSADuplicateSocketA()`.
- Documented real Windows `fork()` as a long-term PAL research tranche instead
  of blocking the mksh/toybox milestone on full fork emulation.
- Made Windows rootfs mksh run single external commands, external-command
  pipelines, builtin-to-external pipelines, and basic input/output redirection
  against CRT toybox applets.
- Fixed Windows toybox `ls -al` directory entries that showed `?` metadata for
  `.` and `..`.
- Recorded toybox LP64/LLP64 patches in `shell/toybox/PATCHES.md`; active fixes
  cover `dirtree.extra`, `ls`, the common option parser, Windows applet path
  lookup, and known active pointer-tagging paths.
- Kept zlib aligned with Android's model: zlib is a separate `libz`
  sysroot/runtime library surface, not part of Bionic libc.
- Confirmed AOSP does not carry GNU make under `platform/external`; Android
  carries make source under `toolchain/make` and prebuilts under
  `platform/prebuilts/build-tools`.
- Added `porting/recipes/make.json` and built Android `toolchain/make` as the
  first CRT-owned bootstrap build tool.
- Taught configure recipes to prefer `PORT_PREFIX/bin/make` before falling back
  to host make.
- Unified configure recipe launching through rootfs mksh for Windows, macOS,
  and Linux target flows.
- Made Windows CRT-shell configure recipes run `make -j 1` and pass
  `SHELL=/system/bin/mksh` so recipe commands stay on the project shell/process
  path.
- Completed Windows x86_64 zlib `./configure --static && make && make install`
  through rootfs mksh and CRT-built make.
- Set the zlib recipe to undefine Windows compiler predefines so upstream zlib
  stays on its generic POSIX path rather than selecting the Win32 `<io.h>`
  branch.
- Set zlib `RANLIB=true` because the optional zlib ranlib step is redundant for
  the LLVM archive path and exposed a Windows mksh subshell status quirk.
- Added Bionic/POSIX CRT surface exposed by make/zlib/shell work:
  - `alloca.h`;
  - `ar.h`;
  - `memrchr`;
  - `confstr`;
  - `_CS_PATH` / `_CS_V7_ENV`;
  - `ttyname`;
  - `getlogin`;
  - `eaccess`;
  - `bsd_signal`;
  - `EXIT_SUCCESS` / `EXIT_FAILURE`;
  - `putenv`;
  - `pselect`.
- Added or expanded regression tests for:
  - string memory helpers;
  - `confstr`/sysconf behavior;
  - `pselect`;
  - process signal helpers;
  - Windows fd snapshot and spawn attribute behavior.
- Updated the active status docs:
  - `docs/sysroot_ports.md`;
  - `docs/porting_status.md`;
  - `docs/shell_import.md`;
  - `docs/windows_fork_emulation.md`;
  - `shell/toybox/PATCHES.md`.
