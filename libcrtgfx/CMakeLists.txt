set(CRTGFX_COMMON_SOURCES
  src/window.c
  src/wayland_weston.c
  src/gpu.c
)

# Skia's own source pin/sparse-checkout/local-patch record now lives in
# libcrtgfx/third_party/skia/recipe.json (2026-08-22, matching libstdc++/
# third_party/{libcxx,libcxxabi,libunwind}/recipe.json's own shape,
# adapted for a GN/Ninja-based, non-CMake build -- see that file's own
# "engine" key and libcrtgfx/third_party/skia/README.md for why there is
# no CMake-recipe-engine reuse here). This CMakeLists.txt is no longer
# where the pin itself is edited: every CRTGFX_SKIA_* CACHE default below
# reads from the recipe instead of hardcoding the values inline, so
# re-pinning Skia (or auditing what is pinned, or recording a local
# build-config patch) means updating recipe.json alone, not two places
# that could drift out of sync. The recipe's own notes array carries the
# full pinning/sparse-checkout/sync-deps rationale that used to live as
# CMake comments here -- read it there, not here, for the "why."
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/third_party/skia/recipe.json" CRTGFX_SKIA_RECIPE_JSON)
string(JSON CRTGFX_SKIA_RECIPE_VERSION GET "${CRTGFX_SKIA_RECIPE_JSON}" source version)
string(JSON CRTGFX_SKIA_RECIPE_REF GET "${CRTGFX_SKIA_RECIPE_JSON}" source ref)
string(JSON CRTGFX_SKIA_RECIPE_REPOSITORY GET "${CRTGFX_SKIA_RECIPE_JSON}" source repository)
string(JSON CRTGFX_SKIA_RECIPE_EXPECTED_COMMIT GET "${CRTGFX_SKIA_RECIPE_JSON}" source expected_commit)
string(JSON CRTGFX_SKIA_RECIPE_SYNC_DEPS GET "${CRTGFX_SKIA_RECIPE_JSON}" source sync_deps)
string(JSON CRTGFX_SKIA_RECIPE_SPARSE_COUNT LENGTH "${CRTGFX_SKIA_RECIPE_JSON}" source sparse_paths)
set(CRTGFX_SKIA_RECIPE_SPARSE_PATHS "")
if(CRTGFX_SKIA_RECIPE_SPARSE_COUNT GREATER 0)
  math(EXPR CRTGFX_SKIA_RECIPE_SPARSE_LAST "${CRTGFX_SKIA_RECIPE_SPARSE_COUNT} - 1")
  foreach(CRTGFX_SKIA_RECIPE_SPARSE_INDEX RANGE ${CRTGFX_SKIA_RECIPE_SPARSE_LAST})
    string(JSON CRTGFX_SKIA_RECIPE_SPARSE_ENTRY GET "${CRTGFX_SKIA_RECIPE_JSON}" source sparse_paths ${CRTGFX_SKIA_RECIPE_SPARSE_INDEX})
    list(APPEND CRTGFX_SKIA_RECIPE_SPARSE_PATHS "${CRTGFX_SKIA_RECIPE_SPARSE_ENTRY}")
  endforeach()
endif()

set(CRTGFX_SKIA_VERSION "${CRTGFX_SKIA_RECIPE_VERSION}" CACHE STRING "Skia Chrome milestone shorthand to fetch, for example m148 (default: libcrtgfx/third_party/skia/recipe.json's own source.version)")
set(CRTGFX_SKIA_REF "${CRTGFX_SKIA_RECIPE_REF}" CACHE STRING "Explicit Skia git ref/commit to fetch; overrides CRTGFX_SKIA_VERSION when set (default: libcrtgfx/third_party/skia/recipe.json's own source.ref)")
set(CRTGFX_SKIA_REPOSITORY "${CRTGFX_SKIA_RECIPE_REPOSITORY}" CACHE STRING "Skia git repository URL (default: libcrtgfx/third_party/skia/recipe.json's own source.repository)")
set(CRTGFX_SKIA_EXPECTED_COMMIT "${CRTGFX_SKIA_RECIPE_EXPECTED_COMMIT}" CACHE STRING "Optional full Skia commit hash expected after fetch (default: libcrtgfx/third_party/skia/recipe.json's own source.expected_commit)")
set(CRTGFX_SKIA_SPARSE_PATHS "${CRTGFX_SKIA_RECIPE_SPARSE_PATHS}" CACHE STRING "Semicolon-separated cone-mode sparse-checkout paths for the Skia fetch; empty means a full checkout (default: libcrtgfx/third_party/skia/recipe.json's own source.sparse_paths)")
option(CRTGFX_SKIA_SYNC_DEPS "Run Skia tools/git-sync-deps after fetching Skia" ${CRTGFX_SKIA_RECIPE_SYNC_DEPS})
set(CRTGFX_SKIA_FETCH_ROOT "${CMAKE_BINARY_DIR}/external/skia" CACHE PATH "Local Skia fetch/build root")
set(CRTGFX_SKIA_DEFAULT_ROOT "${CRTGFX_SKIA_FETCH_ROOT}/src")
if(DEFINED CACHE{CRTGFX_SKIA_ROOT})
  get_property(CRTGFX_SKIA_CACHED_ROOT CACHE CRTGFX_SKIA_ROOT PROPERTY VALUE)
  if(CRTGFX_SKIA_CACHED_ROOT MATCHES "/libcrtgfx/third_party/skia$" AND
     NOT EXISTS "${CRTGFX_SKIA_CACHED_ROOT}/include/core/SkSurface.h")
    set(CRTGFX_SKIA_ROOT "${CRTGFX_SKIA_DEFAULT_ROOT}" CACHE PATH "Skia checkout root for libcrtgfx" FORCE)
  endif()
else()
  set(CRTGFX_SKIA_ROOT "${CRTGFX_SKIA_DEFAULT_ROOT}" CACHE PATH "Skia checkout root for libcrtgfx")
endif()
set(CRTGFX_SKIA_DEFAULT_BUILD_DIR "${CRTGFX_SKIA_ROOT}/out/crt-${CRT_TARGET_OS}-${CRT_EFFECTIVE_ARCH_FOR_FLAGS}")
if(DEFINED CACHE{CRTGFX_SKIA_BUILD_DIR})
  get_property(CRTGFX_SKIA_CACHED_BUILD_DIR CACHE CRTGFX_SKIA_BUILD_DIR PROPERTY VALUE)
  if(CRTGFX_SKIA_CACHED_BUILD_DIR MATCHES "/external/skia/build/" OR
     CRTGFX_SKIA_CACHED_BUILD_DIR MATCHES "/libcrtgfx/third_party/skia/")
    set(CRTGFX_SKIA_BUILD_DIR "${CRTGFX_SKIA_DEFAULT_BUILD_DIR}" CACHE PATH "Skia GN build directory" FORCE)
  endif()
else()
  set(CRTGFX_SKIA_BUILD_DIR "${CRTGFX_SKIA_DEFAULT_BUILD_DIR}" CACHE PATH "Skia GN build directory")
endif()
set(CRTGFX_SKIA_INSTALL_PREFIX "${CRTGFX_SKIA_FETCH_ROOT}/install" CACHE PATH "Skia install prefix used by libcrtgfx")
set(CRTGFX_SKIA_GN "" CACHE FILEPATH "Optional gn executable for Skia build")
set(CRTGFX_SKIA_NINJA "" CACHE FILEPATH "Optional ninja executable for Skia build")
set(CRTGFX_SKIA_EXTRA_GN_ARGS "" CACHE STRING "Semicolon-separated extra raw GN arg lines for Skia build")
# The shared porting/recipes install prefix (freetype.json lands here once
# port-build-freetype has run) -- exactly matching crt-port-build.py's own
# default port_prefix for THIS preset build directory, same convention as
# CRTGFX_WAYLAND_PORT_PREFIX further down (out/<preset>/port-tests/install
# == CMAKE_BINARY_DIR/port-tests/install, since CMAKE_BINARY_DIR already
# *is* out/<preset>). Defined here (not only where it's consumed by
# tools/build_skia.py's own --freetype-prefix further down) because
# CRTGFX_SKIA_LIBRARIES, right below, needs it too: libskia.a's own object
# files reference real FT_* symbols (skia_use_freetype=true, see
# tools/build_skia.py's own default_gn_args()) that stay unresolved inside
# a plain static archive -- every consumer of CRTGFX_SKIA_LIBRARIES
# (crtgfx/crtgfx_shared/crtgfx_skia_raster_smoke) needs libfreetype.a on
# its own final link line too, not just libskia.a.
set(CRTGFX_SKIA_FREETYPE_PREFIX "${CMAKE_BINARY_DIR}/port-tests/install" CACHE PATH "Shared porting/recipes install prefix (freetype) for the Skia build")
if(NOT CRTGFX_SKIA_LIBRARIES AND EXISTS "${CRTGFX_SKIA_INSTALL_PREFIX}/lib/libskia.a")
  set(CRTGFX_SKIA_LIBRARIES "${CRTGFX_SKIA_INSTALL_PREFIX}/lib/libskia.a" CACHE STRING "Libraries needed to link Skia CPU raster support" FORCE)
elseif(NOT CRTGFX_SKIA_LIBRARIES)
  set(CRTGFX_SKIA_LIBRARIES "" CACHE STRING "Libraries needed to link Skia CPU raster support")
endif()
# Deliberately a *separate*, unconditionally-re-evaluated check, not folded
# into the one-shot "first time CRTGFX_SKIA_LIBRARIES gets populated" branch
# above: libskia.a and libfreetype.a don't necessarily finish building in
# the same configure pass (crtgfx-skia-build itself has no ninja dependency
# edge onto port-build-freetype's own *build*, only crtgfx-skia-configure/
# -build depending on it being requested -- see this file's own crtgfx-
# skia-configure/-build add_custom_target calls further down). Confirmed
# for real (2026-08-25): a build tree whose CRTGFX_SKIA_LIBRARIES cache
# entry was already populated from an earlier configure, back when
# port-build-freetype had not yet succeeded on macOS (see porting/
# recipes/freetype.json's own 2026-08-25 notes on the make-dependency fix
# that unblocked it there), kept silently linking crtgfx/crtgfx_shared
# without libfreetype.a forever after -- the one-shot guard above only
# ever fires once per build directory's cache lifetime, so freetype.a
# becoming available *later* than the first configure never re-triggered
# it. Symptom: dozens of undefined `_FT_*` symbols linking libcrtgfx.dylib
# (SkScalerContext_FreeType/SkFontScanner_FreeType, from libskia.a's own
# skia_use_freetype=true object files), not a "clean rebuild will fix it"
# situation -- a plain `cmake --build` reconfigure alone does not
# re-evaluate this any more than it did before, since the cache entry
# already exists either way. This block re-checks on *every* configure,
# regardless of whether CRTGFX_SKIA_LIBRARIES already had a value coming
# in (auto-populated above, previously cached, or an explicit user
# override), and appends libfreetype.a the moment it exists on disk if it
# isn't already present -- self-healing across a freetype port that
# builds successfully sometime after libskia.a already did, without
# needing a full cache wipe/reconfigure from scratch to notice.
if(CRTGFX_SKIA_LIBRARIES AND EXISTS "${CRTGFX_SKIA_FREETYPE_PREFIX}/lib/libfreetype.a")
  set(CRTGFX_SKIA_FREETYPE_LIBRARY "${CRTGFX_SKIA_FREETYPE_PREFIX}/lib/libfreetype.a")
  if(NOT CRTGFX_SKIA_FREETYPE_LIBRARY IN_LIST CRTGFX_SKIA_LIBRARIES)
    set(CRTGFX_SKIA_LIBRARIES "${CRTGFX_SKIA_LIBRARIES};${CRTGFX_SKIA_FREETYPE_LIBRARY}" CACHE STRING "Libraries needed to link Skia CPU raster support" FORCE)
  endif()
endif()
if(EXISTS "${CRTGFX_SKIA_ROOT}/include/core/SkSurface.h" AND CRTGFX_SKIA_LIBRARIES)
  set(CRTGFX_SKIA_AVAILABLE_DEFAULT ON)
else()
  set(CRTGFX_SKIA_AVAILABLE_DEFAULT OFF)
endif()

# libxkbcommon's own fetch/build root/install prefix path math, defined
# here (not only where the fetch/build custom targets themselves are,
# further down inside the Python3_Interpreter_FOUND-gated block) for the
# same reason CRTGFX_SKIA_INSTALL_PREFIX is defined this early: crtgfx_
# backend_objects's own target_include_directories/target_link_libraries
# calls need this path unconditionally, whether or not Python is available
# to actually populate it.
set(CRTGFX_XKBCOMMON_FETCH_ROOT "${CMAKE_BINARY_DIR}/external/xkbcommon" CACHE PATH "Local libxkbcommon fetch/build root")
set(CRTGFX_XKBCOMMON_ROOT "${CRTGFX_XKBCOMMON_FETCH_ROOT}/src" CACHE PATH "libxkbcommon checkout root")
set(CRTGFX_XKBCOMMON_BUILD_DIR "${CRTGFX_XKBCOMMON_FETCH_ROOT}/build" CACHE PATH "libxkbcommon build output directory")
set(CRTGFX_XKBCOMMON_INSTALL_PREFIX "${CRTGFX_XKBCOMMON_FETCH_ROOT}/install" CACHE PATH "libxkbcommon install prefix")
# Deliberately NOT gated on `EXISTS "${CRTGFX_XKBCOMMON_INSTALL_PREFIX}/lib/
# libxkbcommon.a"` the way CRTGFX_SKIA_LIBRARIES (above) is -- Skia is an
# opt-in port (CRTGFX_ENABLE_SKIA) that may genuinely never have been built,
# so an EXISTS check is the right way to detect "not built yet" and degrade
# gracefully. libxkbcommon is NOT opt-in: crtgfx/crtgfx_shared always
# add_dependencies() on crtgfx-xkbcommon-build on Linux (further down this
# file), so libxkbcommon.a always gets built before anything actually links
# it -- but that's a *ninja build-time* guarantee, not a *CMake configure-
# time* one. A real GitHub Actions CI failure (2026-08-25) on a from-scratch
# checkout showed the actual bug an EXISTS check causes here: `cmake
# --workflow` configures once, *before* any ninja target has run, so
# libxkbcommon.a genuinely does not exist on disk yet at configure time --
# an EXISTS-gated CRTGFX_XKBCOMMON_LIBRARIES permanently cached itself as
# empty right then, and (unlike CRTGFX_SKIA_LIBRARIES's own re-checked-
# every-configure freetype block above) nothing ever re-evaluates it later
# in the same single-pass workflow, even after the real build finishes --
# ninja bakes the empty value into every already-generated link command.
# Symptom: crtgfx_window_smoke/crtgfx_window_demo (real executables, which
# unlike a .so hard-fail on unresolved symbols) linked with `undefined
# reference to xkb_context_new` and friends, on a target (aarch64) that had
# never had a stale populated cache entry from an earlier local session to
# accidentally mask the bug, unlike every prior local Linux verification
# this port had actually received. Setting this unconditionally for Linux
# is correct specifically *because* the ninja dependency edges already make
# the file's existence a real invariant by build time -- there is no
# "maybe never built" state to detect here the way there is for Skia.
if(CRT_TARGET_OS STREQUAL "linux")
  set(CRTGFX_XKBCOMMON_LIBRARIES "${CRTGFX_XKBCOMMON_INSTALL_PREFIX}/lib/libxkbcommon.a" CACHE STRING "Libraries needed to link libxkbcommon into the Linux/Wayland backend" FORCE)
else()
  set(CRTGFX_XKBCOMMON_LIBRARIES "" CACHE STRING "Libraries needed to link libxkbcommon into the Linux/Wayland backend" FORCE)
endif()
# Building Skia against the CRT is already a useful porting gate. Linking it
# into libcrtgfx requires the full libc++ standard library, which is a later
# explicit tranche; never make ordinary CRT workflows depend on a host libc++
# as a hidden substitute. Enable this only once that runtime gate is met.
if(DEFINED CACHE{CRTGFX_SKIA_WAS_AVAILABLE})
  set(CRTGFX_ENABLE_SKIA OFF CACHE BOOL "Build libcrtgfx Skia CPU raster bridge when the full libc++ runtime is available" FORCE)
  unset(CRTGFX_SKIA_WAS_AVAILABLE CACHE)
endif()
option(CRTGFX_ENABLE_SKIA "Build libcrtgfx Skia CPU raster bridge when the full libc++ runtime is available" OFF)
if(CRTGFX_ENABLE_SKIA AND NOT EXISTS "${CRTGFX_SKIA_ROOT}/include/core/SkSurface.h")
  message(FATAL_ERROR "CRTGFX_ENABLE_SKIA=ON but ${CRTGFX_SKIA_ROOT}/include/core/SkSurface.h was not found")
endif()
if(CRTGFX_ENABLE_SKIA AND NOT CRTGFX_SKIA_LIBRARIES)
  message(FATAL_ERROR "CRTGFX_ENABLE_SKIA=ON requires CRTGFX_SKIA_LIBRARIES. Run crtgfx-skia-build, then reconfigure, or set the library path explicitly.")
endif()
# Windows-only: crtgfx_skia_objects/crtgfx_skia_raster_smoke's own C++ code
# (skia_bridge.cc, tests/skia_raster_smoke.cc) needs a real <vector>/<memory>/
# ... standard library to even compile, and -- since Skia itself is always
# built --target=*-w64-mingw32 (Itanium/GNU C++ ABI) via tools/crt-cc/tools/
# crt-c++, regardless of this outer CMake project's own target -- that real
# standard library must be THIS project's own imported libc++ (staged at
# ${CRT_SYSROOT}/include/c++/v1 by the crt-libcxx-sysroot target), not a host
# substitute. Before CMakeLists.txt's mingw32 unification, crt_cxx_build_
# flags's own Windows fallback (CRT_CXX_STANDARD_INCLUDE_DIRS, see that
# file's own comment) silently supplied the *real host MSVC STL* instead
# (found via a direct `clang++ -v` compiler probe, since regular CMake C++
# code compiled *-pc-windows-msvc by default) -- MSVC-mangled, and the root
# cause of the ABI-mismatch link failures this whole mingw32 migration was
# started to fix (see HISTORY.md's dated entry: real Skia methods confirmed
# present in libskia.a via llvm-nm, yet "undefined symbol" at link time).
# Confirmed for real (2026-08-23): after the unification, that same fallback
# probe finds nothing usable under --target=x86_64-w64-mingw32 (no host MSVC
# STL is reachable from that target, and this project ships no separate real
# mingw-w64 GCC/libstdc++ install to fall back to either) -- CMAKE_CXX_
# IMPLICIT_INCLUDE_DIRECTORIES came back as just Clang's own two bundled
# resource-dir paths, one of which incidentally matches CRT_CXX_STANDARD_
# INCLUDE_DIRS's generic "ends with /include" Windows catch-all pattern, so
# the probe never even ran -- and the first real Skia header reaching
# <cstddef> failed outright: `fatal error: 'cstddef' file not found`. Scoped
# here (not added to crt_cxx_build_flags globally) because every other
# Windows C++ consumer of crt_cxx_build_flags (add_crt_cxx_test's own tests)
# only exercises the small bootstrap libstdc++/src/ ABI shim (cxxabi.c/
# msvcabi.c/new_delete.cc) and has never needed a real standard library --
# widening this to all of them would be unverified, unnecessary scope.
if(CRTGFX_ENABLE_SKIA AND CRT_TARGET_OS STREQUAL "windows" AND NOT CRT_USE_IMPORTED_LIBCXX)
  message(FATAL_ERROR "CRTGFX_ENABLE_SKIA=ON on Windows requires CRT_USE_IMPORTED_LIBCXX=ON (and a built crt-libcxx-sysroot) -- Skia's own C++ code needs this project's real, project-owned libc++, never a host substitute.")
endif()
# Checked here (configure time, file-existence) rather than as a ninja
# add_dependencies() edge -- see the top-level CMakeLists.txt's own comment
# (right after its "sysroot"/crtgfx-skia-build wiring) for the real
# dependency cycle that a direct edge onto crt-libcxx-sysroot/sysroot hits,
# since crtgfx_skia_objects's own output is folded into the `crtgfx`
# STATIC_LIBRARY that `sysroot` itself depends on.
if(CRTGFX_ENABLE_SKIA AND CRT_TARGET_OS STREQUAL "windows" AND
   NOT EXISTS "${CRT_SYSROOT}/include/c++/v1")
  message(FATAL_ERROR "CRTGFX_ENABLE_SKIA=ON on Windows but ${CRT_SYSROOT}/include/c++/v1 was not found. Run the crt-libcxx-sysroot target first (with CRT_USE_IMPORTED_LIBCXX=ON), then reconfigure.")
endif()

if(CRT_TARGET_OS STREQUAL "windows")
  set(CRTGFX_BACKEND_SOURCES src/arch/windows/window_win32.c)
elseif(CRT_TARGET_OS STREQUAL "macos")
  set(CRTGFX_BACKEND_SOURCES src/arch/macos/window_cocoa.c)
  # window_cocoa.c drives the Objective-C runtime from plain C by casting
  # objc_msgSend (a real, variadic `id (*)(id, SEL, ...)` extern) to each
  # call site's own exact, non-variadic signature before invoking it --
  # the standard, unavoidable way to call objc_msgSend from C without the
  # compiler's ObjC-mode-only special-casing of that one function. Found
  # via this session's own cross-compile check (2026-08-30, no real macOS
  # host -- see this target's own crt_build_flags link below, which is
  # what actually made this reachable: -Wall/-Wextra bring in a real,
  # fairly new Clang diagnostic, -Wcast-function-type-mismatch, that
  # flags exactly this pattern under -Werror on this session's own local
  # LLVM 22.1.8) -- a real, previously-latent risk in code that has
  # actually passed on real macOS hardware before, not a bug in the cast
  # pattern itself (every one of these casts is intentional and correct;
  # C has no other supported way to call a function through a pointer
  # typed differently than how it will actually be invoked here, and
  # this is exactly what upstream Objective-C runtime headers themselves
  # document as the required approach for non-ObjC-mode callers).
  # Suppressed here, not by rewriting the cast pattern -- there is no
  # alternative pattern that avoids it, only ways to hide the warning.
  # -Wno-unknown-warning-option alongside it for the same cross-Clang-
  # version reason established for shell/CMakeLists.txt's own mksh fix
  # the same day: an older/different Clang (this exact file has been
  # verified on real macOS hardware with whatever AppleClang version was
  # current at the time, almost certainly one that predates this
  # diagnostic entirely) must not choke on an unrecognized flag name
  # either. set_source_files_properties(), not target_compile_options():
  # crtgfx_backend_objects links crt_build_flags, whose own -Wall/-Wextra/
  # -Werror land after a target's own target_compile_options() on the
  # real command line (the exact ordering bug shell/CMakeLists.txt's own
  # mksh fix found and fixed the same day) -- COMPILE_OPTIONS source-file
  # properties are appended at the very end instead, reliably winning
  # regardless of link order.
  set_source_files_properties(${CRTGFX_BACKEND_SOURCES} PROPERTIES
    COMPILE_OPTIONS "-Wno-cast-function-type-mismatch;-Wno-unknown-warning-option"
  )
elseif(CRT_TARGET_OS STREQUAL "linux")
  set(CRTGFX_BACKEND_SOURCES src/arch/linux/window_wayland.c)
  # Ganesh/Vulkan offscreen vertical slice (2026-09-03, TODO.md's "Enable
  # Skia GPU rendering" step) -- the first real crtgfx_gpu_device backend
  # anywhere in this project (see crtgfx/gpu.h's own top comment).
  # Linux-only for now; Windows (D3D12)/macOS (Metal) are separate, later
  # roadmap steps. Only wired in when a real libvulkan is actually found:
  # this project's own dlopen() has no real ELF dynamic loading yet
  # (confirmed by reading libdl/src/arch/linux/dl_linux.c directly), so
  # src/arch/linux/gpu_vulkan.c links against libvulkan directly rather
  # than dlopen()/dlsym()-ing it at runtime -- optional, not a new hard
  # dependency for every Linux consumer of crtgfx, matching this project's
  # own "backend absent -> honest degrade, not a build failure" precedent
  # (CRTGFX_ENABLE_SKIA's own optional-port shape). find_library() looks
  # for the unversioned libvulkan.so (the Vulkan Loader's own -dev
  # package, e.g. Ubuntu's libvulkan-dev) -- a bare libvulkan.so.1 runtime
  # package (libvulkan1) is not enough to link against by name.
  find_library(CRTGFX_LINUX_VULKAN_LIB vulkan)
  if(CRTGFX_LINUX_VULKAN_LIB)
    list(APPEND CRTGFX_BACKEND_SOURCES src/arch/linux/gpu_vulkan.c)
    # Directory-scoped, not per-target: src/gpu.c (compiled into
    # crtgfx_common_objects) and src/arch/linux/gpu_vulkan.c (compiled
    # into crtgfx_backend_objects) share struct crtgfx_gpu_device's own
    # real field layout via src/gpu_internal.h -- both compilation units
    # MUST see CRTGFX_HAVE_VULKAN identically, or the Vulkan-only fields
    # silently disagree on the struct's own real size between the two
    # object libraries (a real, silent memory-corruption risk, not just a
    # build inconsistency). add_compile_definitions() here applies to
    # every target this CMakeLists.txt still goes on to create in this
    # directory (both crtgfx_common_objects and crtgfx_backend_objects,
    # further down) -- two separate target_compile_definitions() calls
    # could drift out of sync in a way this single call cannot.
    add_compile_definitions(CRTGFX_HAVE_VULKAN=1)
    # See the real -Wl,--exclude-libs,ALL fix and its own full "why"
    # comment further down, right after add_library(crtgfx STATIC ...) --
    # deferred to there because target_link_options() needs the `crtgfx`
    # target to already exist, and it is not defined until much later in
    # this same file. CRTGFX_LINUX_VULKAN_LIB (this if-branch's own
    # condition) stays true for the rest of this configure run, so that
    # later block's own identical condition re-evaluates the same way.
  else()
    message(STATUS "libcrtgfx: libvulkan not found (install libvulkan-dev to enable it) -- crtgfx_gpu_device_create() stays CRTGFX_ERROR_UNSUPPORTED on Linux, same as Windows/macOS today")
  endif()
endif()

if(Python3_Interpreter_FOUND)
  set(CRTGFX_SKIA_FETCH_ARGS)
  if(CRTGFX_SKIA_REF)
    list(APPEND CRTGFX_SKIA_FETCH_ARGS --ref "${CRTGFX_SKIA_REF}")
  endif()
  if(CRTGFX_SKIA_EXPECTED_COMMIT)
    list(APPEND CRTGFX_SKIA_FETCH_ARGS --expected-commit "${CRTGFX_SKIA_EXPECTED_COMMIT}")
  endif()
  if(CRTGFX_SKIA_SYNC_DEPS)
    list(APPEND CRTGFX_SKIA_FETCH_ARGS --sync-deps)
  endif()
  foreach(CRTGFX_SKIA_SPARSE_PATH IN LISTS CRTGFX_SKIA_SPARSE_PATHS)
    list(APPEND CRTGFX_SKIA_FETCH_ARGS --sparse-path "${CRTGFX_SKIA_SPARSE_PATH}")
  endforeach()

  # CRTGFX_SKIA_FREETYPE_PREFIX itself is defined near the top of this file
  # (alongside CRTGFX_SKIA_LIBRARIES, which also needs it) -- passed to
  # tools/build_skia.py's own --freetype-prefix unconditionally here:
  # porting/recipes/freetype.json is a real, always-present (not opt-in)
  # recipe as of 2026-08-24, so `gn gen` always wires Skia's real
  # SkFontMgr_custom_directory against it rather than the empty/stub font
  # manager -- see tools/build_skia.py's own default_gn_args() for the
  # mechanics and porting/recipes/freetype.json's own notes for the port
  # itself.
  set(CRTGFX_SKIA_BUILD_ARGS)
  foreach(CRTGFX_SKIA_EXTRA_GN_ARG IN LISTS CRTGFX_SKIA_EXTRA_GN_ARGS)
    list(APPEND CRTGFX_SKIA_BUILD_ARGS --gn-arg "${CRTGFX_SKIA_EXTRA_GN_ARG}")
  endforeach()
  if(CRTGFX_SKIA_GN)
    list(APPEND CRTGFX_SKIA_BUILD_ARGS --gn "${CRTGFX_SKIA_GN}")
  endif()
  if(CRTGFX_SKIA_NINJA)
    list(APPEND CRTGFX_SKIA_BUILD_ARGS --ninja "${CRTGFX_SKIA_NINJA}")
  endif()
  list(APPEND CRTGFX_SKIA_BUILD_ARGS --freetype-prefix "${CRTGFX_SKIA_FREETYPE_PREFIX}")
  if(CRT_TARGET_OS STREQUAL "windows")
    # tools/build_skia.py needs this to set CRT_MKSH_EXE for crt-cc.cmd/
    # crt-c++.cmd -- see that file's own comment on why GN's own gcc_like
    # toolchain (selected via target_os="linux" even for a real Windows
    # build, also see that file's own comment) needs the .cmd wrappers
    # rather than the bare #!/bin/sh scripts. Matches libstdc++/
    # CMakeLists.txt's own CRT_LIBCXX_PLATFORM_ARGUMENTS --rootfs value
    # exactly (the *rootfs* mksh.exe, not the sysroot's -- only the
    # rootfs carries toybox's applet aliases these wrapper scripts call).
    list(APPEND CRTGFX_SKIA_BUILD_ARGS --rootfs "${CMAKE_BINARY_DIR}/rootfs")
  endif()

  add_custom_target(crtgfx-skia-fetch
    COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
      "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/fetch_skia.py"
      --dest "${CRTGFX_SKIA_ROOT}"
      --repo "${CRTGFX_SKIA_REPOSITORY}"
      --version "${CRTGFX_SKIA_VERSION}"
      ${CRTGFX_SKIA_FETCH_ARGS}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Fetching Skia ${CRTGFX_SKIA_VERSION} for libcrtgfx into ${CRTGFX_SKIA_ROOT}"
    USES_TERMINAL
  )

  add_custom_target(crtgfx-skia-configure
    COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
      "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/build_skia.py"
      --root "${CMAKE_SOURCE_DIR}"
      --source "${CRTGFX_SKIA_ROOT}"
      --build-dir "${CRTGFX_SKIA_BUILD_DIR}"
      --install-prefix "${CRTGFX_SKIA_INSTALL_PREFIX}"
      --sysroot "${CRT_SYSROOT}"
      --target-os "${CRT_TARGET_OS}"
      --target-arch "${CRT_EFFECTIVE_ARCH_FOR_FLAGS}"
      ${CRTGFX_SKIA_BUILD_ARGS}
      --configure-only
    DEPENDS crtgfx-skia-fetch port-build-freetype
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Configuring Skia with the CRT toolchain"
    USES_TERMINAL
  )

  add_custom_target(crtgfx-skia-build
    COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
      "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/build_skia.py"
      --root "${CMAKE_SOURCE_DIR}"
      --source "${CRTGFX_SKIA_ROOT}"
      --build-dir "${CRTGFX_SKIA_BUILD_DIR}"
      --install-prefix "${CRTGFX_SKIA_INSTALL_PREFIX}"
      --sysroot "${CRT_SYSROOT}"
      --target-os "${CRT_TARGET_OS}"
      --target-arch "${CRT_EFFECTIVE_ARCH_FOR_FLAGS}"
      ${CRTGFX_SKIA_BUILD_ARGS}
    DEPENDS crtgfx-skia-fetch port-build-freetype
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Building Skia CPU raster with the CRT toolchain"
    USES_TERMINAL
  )

  # crtgfx-skia-smoke: builds and runs crtgfx_skia_raster_smoke regardless
  # of *this* build directory's own current CRTGFX_ENABLE_SKIA/CRT_USE_
  # IMPORTED_LIBCXX cache values -- mirrors crt-libcxx-smoke's own role
  # (libstdc++/CMakeLists.txt) for the imported-libc++ smoke test:
  # unconditionally defined (not gated behind CRTGFX_ENABLE_SKIA the way
  # the real crtgfx_skia_raster_smoke target itself still is), so `cmake
  # --build <dir> --target crtgfx-skia-smoke` alone drives the whole chain
  # end to end without first hand-running the multi-step reconfigure dance
  # this file's own CRTGFX_ENABLE_SKIA guard otherwise requires. Unlike
  # crt-libcxx-smoke (which compiles its own smoke source directly through
  # tools/crt-c++, entirely outside the outer CMake project), this drives
  # a *separate, dedicated* nested build directory that reuses the real,
  # already-correct crtgfx_skia_raster_smoke CMake target rather than
  # reimplementing its own build recipe a second time by hand -- see
  # tools/test_crtgfx_skia_smoke.py's own docstring for the fuller
  # reasoning. Deliberately does not DEPENDS on crt-libcxx-sysroot/
  # crtgfx-skia-build the way the real target's own dependents do -- those
  # run *inside* the dedicated directory the script itself drives, not
  # this (the calling) one.
  # --target given three times (2026-08-30, then again 2026-08-31):
  # crtgfx_skia_cpu_coverage (tests/skia_cpu_coverage_test.cc) and
  # crtmedia_frame_skia_smoke (libcrtmedia/tests/frame_skia_smoke.cc, see
  # that target's own add_executable() comment above for why it is
  # defined in *this* file despite its source living under libcrtmedia/)
  # both need the exact same Skia-enabled build as crtgfx_skia_raster_
  # smoke, so each rides along in the same dedicated build directory/
  # single configure pass instead of needing its own separate `cmake
  # --build ... --target crtgfx-...` invocation -- see tools/test_
  # crtgfx_skia_smoke.py's own docstring for why a repeated --target is
  # supported there specifically for this. That script's own binary-path
  # assumption (`build_dir / "libcrtgfx" / target`) still holds for
  # crtmedia_frame_skia_smoke: CMake places a target's build output under
  # its *defining* CMakeLists.txt's own directory, not its source file's.
  set(CRTGFX_SKIA_SMOKE_TARGETS
    crtgfx_skia_raster_smoke
    crtgfx_skia_cpu_coverage
    crtmedia_frame_skia_smoke
  )
  if(CRT_TARGET_OS STREQUAL "linux" AND CRTGFX_LINUX_VULKAN_LIB)
    # crtgfx_skia_gpu_offscreen_smoke (2026-09-03) -- only ever defined on
    # Linux with a real libvulkan found (same condition its own add_
    # executable() above is gated on); the nested build directory this
    # custom target drives re-evaluates that same find_library() call
    # itself, on the same host, so it stays consistent.
    list(APPEND CRTGFX_SKIA_SMOKE_TARGETS crtgfx_skia_gpu_offscreen_smoke)
  endif()
  set(CRTGFX_SKIA_SMOKE_TARGET_ARGS)
  foreach(CRTGFX_SKIA_SMOKE_TARGET IN LISTS CRTGFX_SKIA_SMOKE_TARGETS)
    list(APPEND CRTGFX_SKIA_SMOKE_TARGET_ARGS --target "${CRTGFX_SKIA_SMOKE_TARGET}")
  endforeach()
  add_custom_target(crtgfx-skia-smoke
    COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
      "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/test_crtgfx_skia_smoke.py"
      --root "${CMAKE_SOURCE_DIR}"
      --build-dir "${CMAKE_BINARY_DIR}/crtgfx-skia-smoke"
      --cmake-c-compiler "${CMAKE_C_COMPILER}"
      --cmake-cxx-compiler "${CMAKE_CXX_COMPILER}"
      --target-os "${CRT_TARGET_OS}"
      --target-arch "${CRT_TARGET_ARCH}"
      ${CRTGFX_SKIA_SMOKE_TARGET_ARGS}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Building and running ${CRTGFX_SKIA_SMOKE_TARGETS} in a dedicated build directory (CRTGFX_ENABLE_SKIA/CRT_USE_IMPORTED_LIBCXX forced on there, this directory's own cache untouched)"
    USES_TERMINAL
  )

  # crtgfx-keyboard-interactive-skia: builds crtgfx_keyboard_interactive
  # with its OPTIONAL Skia+FreeType on-screen text rendering also turned
  # on. IMPORTANT, easy to misread: this target has nothing to do with
  # keyboard/mouse *input* itself -- that's real Wayland/xkbcommon wiring
  # in src/arch/linux/window_wayland.c, unconditionally compiled into
  # crtgfx/crtgfx_shared regardless of CRTGFX_ENABLE_SKIA, so the plain
  # `cmake --build <dir> --target crtgfx_keyboard_interactive` (no flags,
  # no dedicated directory, builds in seconds) already gets a fully
  # working keyboard/mouse test on its own. This target exists purely for
  # the separate, optional bonus of actually *seeing* typed characters
  # drawn on screen (crtgfx/skia.h's own CRTGFX_HAS_SKIA_HEADERS-gated
  # code path in that source file) -- hence the "-skia" suffix naming it
  # explicitly as the augmented variant, not the one to reach for by
  # default. Mirrors crtgfx-skia-smoke's own one-command convenience
  # (added 2026-08-25 after a user, walked through the five-command
  # manual CRTGFX_ENABLE_SKIA/CRT_USE_IMPORTED_LIBCXX reconfigure dance
  # for exactly this, correctly pointed out this project already has a
  # one-command answer for that shape of problem and asked why this
  # target didn't get the same treatment -- a real, valid UX complaint).
  # Reuses the *same* dedicated build directory as crtgfx-skia-smoke
  # (tools/test_crtgfx_skia_smoke.py's own --build-dir), not a separate
  # one -- Skia/libc++/FreeType only need building once regardless of
  # which real crtgfx executable target gets built against them
  # afterward, and the script itself already treats that directory as
  # shared/reused across --target invocations. Also means the resulting
  # binary lives in *that* dedicated directory's own tree, a completely
  # separate crtgfx build/configure from whatever the calling directory
  # (this one) is currently set to -- its own Wayland/xkbcommon wiring is
  # unaffected either way, since neither directory's own CRTGFX_ENABLE_
  # SKIA value changes how that part of the code behaves.
  # --no-run: crtgfx_keyboard_interactive needs a real human typing into a
  # real window for up to 60 seconds, unlike crtgfx_skia_raster_smoke's
  # own fixed ctest-shaped pass/fail contract -- this target only builds
  # the binary and reports its path; the user runs it themselves.
  add_custom_target(crtgfx-keyboard-interactive-skia
    COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
      "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/test_crtgfx_skia_smoke.py"
      --root "${CMAKE_SOURCE_DIR}"
      --build-dir "${CMAKE_BINARY_DIR}/crtgfx-skia-smoke"
      --cmake-c-compiler "${CMAKE_C_COMPILER}"
      --cmake-cxx-compiler "${CMAKE_CXX_COMPILER}"
      --target-os "${CRT_TARGET_OS}"
      --target-arch "${CRT_TARGET_ARCH}"
      --target crtgfx_keyboard_interactive
      --no-run
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Building crtgfx_keyboard_interactive with real Skia+FreeType text rendering in the same dedicated build directory crtgfx-skia-smoke uses (CRTGFX_ENABLE_SKIA/CRT_USE_IMPORTED_LIBCXX forced on there, this directory's own cache untouched) -- run the resulting binary yourself, it needs a real human typing into it"
    USES_TERMINAL
  )

  # Wayland core (wayland-scanner host tool + wayland-client), 2026-08-24:
  # same "external build, recipe.json is the single source of truth for
  # source pinning" shape as Skia just above, but with NO external build-
  # orchestration tool at all -- tools/build_wayland.py drives every
  # compile/link step directly via tools/crt-cc itself (no Meson/Ninja/
  # pkg-config), at the user's own explicit direction, after an initial
  # Meson-based version of this file was found to need host tooling this
  # project's own sandboxed tooling cannot install (see HISTORY.md's
  # 2026-08-24 entries for both the Meson attempt and the direct-compile
  # rewrite). Scope is also narrower than originally planned: wayland-
  # scanner + wayland-client only (no server/cursor/egl yet) -- see
  # libcrtgfx/third_party/wayland/recipe.json's own notes for the full
  # "why core-client-only, why not wired into libcrtgfx's own backend"
  # reasoning. Unlike Skia, there is no CRTGFX_ENABLE_WAYLAND flag and no
  # real libcrtgfx CMake target consumes this build's output at all yet --
  # crtgfx-wayland-configure/-build exist purely to drive the external
  # build, and crtgfx-wayland-smoke (below) is a wholly standalone test
  # executable, not a nested-shadow-directory reuse of some other real
  # target the way crtgfx-skia-smoke reuses crtgfx_skia_raster_smoke
  # (there is no equivalent real target here to reuse).
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/third_party/wayland/recipe.json" CRTGFX_WAYLAND_RECIPE_JSON)
  string(JSON CRTGFX_WAYLAND_RECIPE_VERSION GET "${CRTGFX_WAYLAND_RECIPE_JSON}" source version)
  string(JSON CRTGFX_WAYLAND_RECIPE_REF GET "${CRTGFX_WAYLAND_RECIPE_JSON}" source ref)
  string(JSON CRTGFX_WAYLAND_RECIPE_REPOSITORY GET "${CRTGFX_WAYLAND_RECIPE_JSON}" source repository)
  string(JSON CRTGFX_WAYLAND_RECIPE_EXPECTED_COMMIT GET "${CRTGFX_WAYLAND_RECIPE_JSON}" source expected_commit)

  set(CRTGFX_WAYLAND_VERSION "${CRTGFX_WAYLAND_RECIPE_VERSION}" CACHE STRING "Wayland version label to fetch (default: libcrtgfx/third_party/wayland/recipe.json's own source.version)")
  set(CRTGFX_WAYLAND_REF "${CRTGFX_WAYLAND_RECIPE_REF}" CACHE STRING "Explicit Wayland git ref/commit to fetch (default: libcrtgfx/third_party/wayland/recipe.json's own source.ref)")
  set(CRTGFX_WAYLAND_REPOSITORY "${CRTGFX_WAYLAND_RECIPE_REPOSITORY}" CACHE STRING "Wayland git repository URL (default: libcrtgfx/third_party/wayland/recipe.json's own source.repository)")
  set(CRTGFX_WAYLAND_EXPECTED_COMMIT "${CRTGFX_WAYLAND_RECIPE_EXPECTED_COMMIT}" CACHE STRING "Expected Wayland commit hash after fetch (default: libcrtgfx/third_party/wayland/recipe.json's own source.expected_commit)")
  set(CRTGFX_WAYLAND_FETCH_ROOT "${CMAKE_BINARY_DIR}/external/wayland" CACHE PATH "Local Wayland fetch/build root")
  set(CRTGFX_WAYLAND_ROOT "${CRTGFX_WAYLAND_FETCH_ROOT}/src" CACHE PATH "Wayland checkout root")
  set(CRTGFX_WAYLAND_BUILD_DIR "${CRTGFX_WAYLAND_FETCH_ROOT}/build" CACHE PATH "Wayland build output directory")
  set(CRTGFX_WAYLAND_INSTALL_PREFIX "${CRTGFX_WAYLAND_FETCH_ROOT}/install" CACHE PATH "Wayland install prefix")
  # The shared porting/recipes install prefix (expat.pc/libffi.pc live
  # here once port-build-expat/port-build-libffi have run) -- exactly
  # matching crt-port-build.py's own default port_prefix for THIS preset
  # build directory (out/<preset>/port-tests/install == CMAKE_BINARY_DIR/
  # port-tests/install, since CMAKE_BINARY_DIR already *is* out/<preset>).
  set(CRTGFX_WAYLAND_PORT_PREFIX "${CMAKE_BINARY_DIR}/port-tests/install" CACHE PATH "Shared porting/recipes install prefix (expat/libffi) for the Wayland build")

  set(CRTGFX_WAYLAND_FETCH_ARGS)
  if(CRTGFX_WAYLAND_EXPECTED_COMMIT)
    list(APPEND CRTGFX_WAYLAND_FETCH_ARGS --expected-commit "${CRTGFX_WAYLAND_EXPECTED_COMMIT}")
  endif()

  # tools/build_wayland.py (2026-08-24 rewrite) drives every compile/link
  # step itself via tools/crt-cc directly -- no Meson/Ninja/pkg-config
  # host-tool dependency at all, see that file's own top-of-file docstring.
  set(CRTGFX_WAYLAND_BUILD_ARGS)
  if(CRT_TARGET_OS STREQUAL "windows")
    # tools/build_wayland.py needs this to set CRT_MKSH_EXE/CRT_ROOTFS for
    # crt-cc.cmd -- exactly matching CRTGFX_SKIA_BUILD_ARGS's own --rootfs
    # wiring just above (same rootfs, same reason: only it carries
    # toybox's applet aliases).
    list(APPEND CRTGFX_WAYLAND_BUILD_ARGS --rootfs "${CMAKE_BINARY_DIR}/rootfs")
    # tools/build_wayland.py links wayland-scanner.exe directly via
    # tools/crt-cc (not through regular CMake target_link_libraries(
    # ${CRT_WINDOWS_SYSTEM_LIBS})), so it needs the real Windows SDK lib
    # directory passed in explicitly -- exactly matching libstdc++/
    # CMakeLists.txt's own CRT_LIBCXX_WINDOWS_SDK_LIBPATH derivation from
    # this same ${CRT_WINDOWS_KERNEL32_LIB} cache variable. Confirmed for
    # real (2026-08-24): without this, crt-cc's own Windows link branch
    # falls back to bare `-Wl,kernel32.lib -Wl,synchronization.lib`, which
    # ld.lld cannot resolve on its own ("could not open 'kernel32.lib'").
    if(CRT_WINDOWS_KERNEL32_LIB)
      get_filename_component(CRTGFX_WAYLAND_WINDOWS_SDK_LIBPATH "${CRT_WINDOWS_KERNEL32_LIB}" DIRECTORY)
      list(APPEND CRTGFX_WAYLAND_BUILD_ARGS --windows-sdk-libpath "${CRTGFX_WAYLAND_WINDOWS_SDK_LIBPATH}")
    endif()
  endif()

  add_custom_target(crtgfx-wayland-configure
    COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
      "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/fetch_wayland.py"
      --dest "${CRTGFX_WAYLAND_ROOT}"
      --repo "${CRTGFX_WAYLAND_REPOSITORY}"
      --version "${CRTGFX_WAYLAND_VERSION}"
      --ref "${CRTGFX_WAYLAND_REF}"
      ${CRTGFX_WAYLAND_FETCH_ARGS}
    COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
      "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/build_wayland.py"
      --root "${CMAKE_SOURCE_DIR}"
      --source "${CRTGFX_WAYLAND_ROOT}"
      --build-dir "${CRTGFX_WAYLAND_BUILD_DIR}"
      --install-prefix "${CRTGFX_WAYLAND_INSTALL_PREFIX}"
      --sysroot "${CRT_SYSROOT}"
      --port-prefix "${CRTGFX_WAYLAND_PORT_PREFIX}"
      --target-os "${CRT_TARGET_OS}"
      --target-arch "${CRT_EFFECTIVE_ARCH_FOR_FLAGS}"
      ${CRTGFX_WAYLAND_BUILD_ARGS}
      --configure-only
    DEPENDS port-build-expat port-build-libffi
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Fetching Wayland and building the wayland-scanner host tool with the CRT toolchain"
    USES_TERMINAL
  )

  add_custom_target(crtgfx-wayland-build
    COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
      "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/build_wayland.py"
      --root "${CMAKE_SOURCE_DIR}"
      --source "${CRTGFX_WAYLAND_ROOT}"
      --build-dir "${CRTGFX_WAYLAND_BUILD_DIR}"
      --install-prefix "${CRTGFX_WAYLAND_INSTALL_PREFIX}"
      --sysroot "${CRT_SYSROOT}"
      --port-prefix "${CRTGFX_WAYLAND_PORT_PREFIX}"
      --target-os "${CRT_TARGET_OS}"
      --target-arch "${CRT_EFFECTIVE_ARCH_FOR_FLAGS}"
      ${CRTGFX_WAYLAND_BUILD_ARGS}
    DEPENDS crtgfx-wayland-configure
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Building and installing Wayland core (wayland-scanner + wayland-client) with the CRT toolchain"
    USES_TERMINAL
  )

  # crtgfx-wayland-smoke: self-sufficient like crtgfx-skia-smoke/
  # crt-libcxx-smoke -- `cmake --build <dir> --target crtgfx-wayland-smoke`
  # alone drives a dedicated shadow build directory (sysroot/rootfs, then
  # port-build-expat/port-build-libffi, then crtgfx-wayland-build) and
  # finally compiles+runs libcrtgfx/tests/wayland_client_smoke.c directly
  # against the freshly installed libwayland-client, entirely independent
  # of *this* (the calling) build directory's own cache/state. See
  # tools/test_crtgfx_wayland_smoke.py's own docstring.
  set(CRTGFX_WAYLAND_SMOKE_ARGS)
  if(CRT_TARGET_OS STREQUAL "windows" AND CRTGFX_WAYLAND_WINDOWS_SDK_LIBPATH)
    # Same reason as CRTGFX_WAYLAND_BUILD_ARGS's own --windows-sdk-libpath
    # just above: tools/test_crtgfx_wayland_smoke.py also links an
    # executable (wayland_client_smoke.exe) directly via tools/crt-cc.
    list(APPEND CRTGFX_WAYLAND_SMOKE_ARGS --windows-sdk-libpath "${CRTGFX_WAYLAND_WINDOWS_SDK_LIBPATH}")
  endif()

  add_custom_target(crtgfx-wayland-smoke
    COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
      "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/test_crtgfx_wayland_smoke.py"
      --root "${CMAKE_SOURCE_DIR}"
      --build-dir "${CMAKE_BINARY_DIR}/crtgfx-wayland-smoke"
      --cmake-c-compiler "${CMAKE_C_COMPILER}"
      --cmake-cxx-compiler "${CMAKE_CXX_COMPILER}"
      --target-os "${CRT_TARGET_OS}"
      --target-arch "${CRT_TARGET_ARCH}"
      ${CRTGFX_WAYLAND_SMOKE_ARGS}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Building and running wayland_client_smoke in a dedicated build directory (this directory's own cache untouched)"
    USES_TERMINAL
  )

  # libxkbcommon core (xkb_context/xkb_keymap/xkb_state), 2026-08-25: same
  # "direct-compile, no Meson/Ninja/pkg-config" shape as Wayland core just
  # above -- see libcrtgfx/third_party/xkbcommon/recipe.json's own notes
  # for the one real difference (a pre-generated, committed bison output
  # instead of a bison host-tool port) and the scope decision (core keymap/
  # state API only, no xkbregistry, no Compose). Unlike Wayland, this DOES
  # feed a real libcrtgfx backend directly (src/arch/linux/window_wayland.c
  # -- Phase 3 of the "notepad-capability" plan, real keyboard input), so
  # there is no separate crtgfx-xkbcommon-smoke target the way crtgfx-
  # wayland-smoke exists standalone: the real wl_seat/wl_keyboard wiring
  # itself, tested via a real smoke test elsewhere in this file, is what
  # actually exercises this build end to end.
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/third_party/xkbcommon/recipe.json" CRTGFX_XKBCOMMON_RECIPE_JSON)
  string(JSON CRTGFX_XKBCOMMON_RECIPE_VERSION GET "${CRTGFX_XKBCOMMON_RECIPE_JSON}" source version)
  string(JSON CRTGFX_XKBCOMMON_RECIPE_REF GET "${CRTGFX_XKBCOMMON_RECIPE_JSON}" source ref)
  string(JSON CRTGFX_XKBCOMMON_RECIPE_REPOSITORY GET "${CRTGFX_XKBCOMMON_RECIPE_JSON}" source repository)
  string(JSON CRTGFX_XKBCOMMON_RECIPE_EXPECTED_COMMIT GET "${CRTGFX_XKBCOMMON_RECIPE_JSON}" source expected_commit)

  set(CRTGFX_XKBCOMMON_VERSION "${CRTGFX_XKBCOMMON_RECIPE_VERSION}" CACHE STRING "libxkbcommon version label to fetch (default: libcrtgfx/third_party/xkbcommon/recipe.json's own source.version)")
  set(CRTGFX_XKBCOMMON_REF "${CRTGFX_XKBCOMMON_RECIPE_REF}" CACHE STRING "Explicit libxkbcommon git ref/commit to fetch (default: libcrtgfx/third_party/xkbcommon/recipe.json's own source.ref)")
  set(CRTGFX_XKBCOMMON_REPOSITORY "${CRTGFX_XKBCOMMON_RECIPE_REPOSITORY}" CACHE STRING "libxkbcommon git repository URL (default: libcrtgfx/third_party/xkbcommon/recipe.json's own source.repository)")
  set(CRTGFX_XKBCOMMON_EXPECTED_COMMIT "${CRTGFX_XKBCOMMON_RECIPE_EXPECTED_COMMIT}" CACHE STRING "Expected libxkbcommon commit hash after fetch (default: libcrtgfx/third_party/xkbcommon/recipe.json's own source.expected_commit)")
  # CRTGFX_XKBCOMMON_FETCH_ROOT/_ROOT/_BUILD_DIR/_INSTALL_PREFIX themselves
  # are defined near the top of this file (alongside CRTGFX_SKIA_INSTALL_
  # PREFIX), matching that variable's own precedent: crtgfx_backend_
  # objects's own target_include_directories/target_link_libraries calls
  # (further down this file) need CRTGFX_XKBCOMMON_INSTALL_PREFIX
  # regardless of whether Python3_Interpreter_FOUND is true, the same
  # "path math works even without the tool that populates it" reasoning
  # CRTGFX_SKIA_INSTALL_PREFIX already established.

  set(CRTGFX_XKBCOMMON_FETCH_ARGS)
  if(CRTGFX_XKBCOMMON_EXPECTED_COMMIT)
    list(APPEND CRTGFX_XKBCOMMON_FETCH_ARGS --expected-commit "${CRTGFX_XKBCOMMON_EXPECTED_COMMIT}")
  endif()

  set(CRTGFX_XKBCOMMON_BUILD_ARGS)
  if(CRT_TARGET_OS STREQUAL "windows")
    list(APPEND CRTGFX_XKBCOMMON_BUILD_ARGS --rootfs "${CMAKE_BINARY_DIR}/rootfs")
    if(CRT_WINDOWS_KERNEL32_LIB)
      get_filename_component(CRTGFX_XKBCOMMON_WINDOWS_SDK_LIBPATH "${CRT_WINDOWS_KERNEL32_LIB}" DIRECTORY)
      list(APPEND CRTGFX_XKBCOMMON_BUILD_ARGS --windows-sdk-libpath "${CRTGFX_XKBCOMMON_WINDOWS_SDK_LIBPATH}")
    endif()
  endif()

  add_custom_target(crtgfx-xkbcommon-configure
    COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
      "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/fetch_xkbcommon.py"
      --dest "${CRTGFX_XKBCOMMON_ROOT}"
      --repo "${CRTGFX_XKBCOMMON_REPOSITORY}"
      --version "${CRTGFX_XKBCOMMON_VERSION}"
      --ref "${CRTGFX_XKBCOMMON_REF}"
      ${CRTGFX_XKBCOMMON_FETCH_ARGS}
    COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
      "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/build_xkbcommon.py"
      --root "${CMAKE_SOURCE_DIR}"
      --source "${CRTGFX_XKBCOMMON_ROOT}"
      --build-dir "${CRTGFX_XKBCOMMON_BUILD_DIR}"
      --install-prefix "${CRTGFX_XKBCOMMON_INSTALL_PREFIX}"
      --sysroot "${CRT_SYSROOT}"
      --target-os "${CRT_TARGET_OS}"
      --target-arch "${CRT_EFFECTIVE_ARCH_FOR_FLAGS}"
      ${CRTGFX_XKBCOMMON_BUILD_ARGS}
      --configure-only
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Fetching libxkbcommon and writing its config.h with the CRT toolchain"
    USES_TERMINAL
  )

  # OUTPUT-based (not a plain add_custom_target COMMAND) specifically so
  # ninja has a real rule that produces this *exact* file path -- required
  # because CRTGFX_XKBCOMMON_LIBRARIES (just above) now unconditionally
  # feeds this literal path into target_link_libraries(crtgfx/crtgfx_shared
  # PRIVATE ...). A raw file path passed to target_link_libraries becomes a
  # real ninja build-graph *input*, not just an order-only target
  # dependency -- if nothing in the graph claims to produce that path (a
  # plain add_custom_target's COMMAND has no OUTPUT, so it doesn't count),
  # ninja refuses to even start the build: real GitHub Actions CI failure
  # (2026-08-25, aarch64), from a from-scratch checkout: "ninja: error:
  # 'external/xkbcommon/install/lib/libxkbcommon.a', needed by
  # 'lib/libcrtgfx.so', missing and no known rule to make it". This is
  # exactly the ninja limitation CRTGFX_SKIA_LIBRARIES's own EXISTS-gating
  # (above) was *also* silently working around by simply never listing a
  # not-yet-existing file as a link input in the first place -- xkbcommon
  # can't use that same trick (see CRTGFX_XKBCOMMON_LIBRARIES's own
  # comment: it's a non-optional, always-built dependency, and an
  # EXISTS-gated value evaluated once at configure time can never become
  # true later within one single-pass `cmake --workflow` invocation). An
  # OUTPUT-based add_custom_command is the correct, general fix instead:
  # ninja then knows precisely how to produce this path on demand,
  # regardless of configure-vs-build-time ordering.
  add_custom_command(
    OUTPUT "${CRTGFX_XKBCOMMON_INSTALL_PREFIX}/lib/libxkbcommon.a"
    COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
      "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/build_xkbcommon.py"
      --root "${CMAKE_SOURCE_DIR}"
      --source "${CRTGFX_XKBCOMMON_ROOT}"
      --build-dir "${CRTGFX_XKBCOMMON_BUILD_DIR}"
      --install-prefix "${CRTGFX_XKBCOMMON_INSTALL_PREFIX}"
      --sysroot "${CRT_SYSROOT}"
      --target-os "${CRT_TARGET_OS}"
      --target-arch "${CRT_EFFECTIVE_ARCH_FOR_FLAGS}"
      ${CRTGFX_XKBCOMMON_BUILD_ARGS}
    DEPENDS crtgfx-xkbcommon-configure
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Building and installing libxkbcommon's core keymap/state API with the CRT toolchain"
    USES_TERMINAL
  )
  add_custom_target(crtgfx-xkbcommon-build
    DEPENDS "${CRTGFX_XKBCOMMON_INSTALL_PREFIX}/lib/libxkbcommon.a"
  )
endif()

set(CRTGFX_CRT_STATIC_LIBS c m dl cxx)
set(CRTGFX_CRT_SHARED_LIBS c_shared m_shared dl_shared cxx_shared)
if(CRT_TARGET_OS STREQUAL "linux" AND CRTGFX_ENABLE_SKIA AND CRT_USE_IMPORTED_LIBCXX)
  # Real imported libc++/libc++abi/libunwind (Itanium ABI, matching Skia
  # itself -- Skia's own GN build always uses this project's own imported
  # libc++ regardless of outer-project settings, see tools/build_skia.py),
  # not the small bootstrap libstdc++/src/ ABI shim (`cxx`/`cxx_shared`) --
  # see the matching, fuller Windows-side comments on crtgfx_skia_objects/
  # crtgfx_skia_raster_smoke for why this substitution is needed at all
  # (the bootstrap shim only provides operator new/delete/__cxa_* stubs,
  # not a real std::string/std::stringstream/std::locale/... -- confirmed
  # for real, 2026-08-23: `undefined reference to std::__1::basic_string
  # <...>::append(...)`, `to_string`, iostream vtables, etc. from Skia's
  # own SkSL debug-trace code, once the earlier libm/operator-new gaps
  # were separately fixed). Unlike Windows (where this swap happens per-
  # consumer, in crtgfx_skia_raster_smoke's/crtgfx_shared's own windows-
  # specific branches, alongside a DLL-vs-static import-library naming
  # split that doesn't apply on Linux), this is set once, project-wide for
  # Linux+Skia+imported-libc++, and flows through every existing consumer
  # of CRTGFX_CRT_STATIC_LIBS/CRTGFX_CRT_SHARED_LIBS unchanged.
  list(REMOVE_ITEM CRTGFX_CRT_STATIC_LIBS cxx)
  list(APPEND CRTGFX_CRT_STATIC_LIBS
    "${CRT_SYSROOT}/lib/libc++.a"
    "${CRT_SYSROOT}/lib/libc++abi.a"
    "${CRT_SYSROOT}/lib/libunwind.a"
  )
  list(REMOVE_ITEM CRTGFX_CRT_SHARED_LIBS cxx_shared)
  list(APPEND CRTGFX_CRT_SHARED_LIBS
    "${CRT_SYSROOT}/lib/libc++.so"
    "${CRT_SYSROOT}/lib/libc++abi.so"
    "${CRT_SYSROOT}/lib/libunwind.so"
  )
elseif(CRT_TARGET_OS STREQUAL "macos" AND CRTGFX_ENABLE_SKIA AND CRT_USE_IMPORTED_LIBCXX)
  # Same substitution, same reason, as the Linux branch just above -- this
  # gap was never extended to macOS when that fix landed (2026-08-23).
  # Confirmed for real the same way: linking crtgfx_skia_raster_smoke and
  # libcrtgfx.dylib against the small bootstrap libstdc++/src/ ABI shim
  # (`cxx`/`cxx_shared`) left dozens of std::__1::... symbols undefined
  # (basic_string::append/insert/push_back, std::to_string, iostream/
  # locale/stringstream machinery, __libcpp_verbose_abort, ...) from
  # Skia's own SkSL compiler/parser/debug-trace code -- that code genuinely
  # needs the real, project-owned imported libc++, not the tiny operator-
  # new/delete/__cxa_* stub shim the bootstrap provides. No libunwind
  # substitution here, unlike Linux: macOS never builds one at all -- see
  # libstdc++/third_party/libunwind/recipe.json's own target_os list and
  # notes (Darwin's own libSystem already provides a real native unwinder;
  # matches upstream LLVM's own LIBCXXABI_USE_LLVM_UNWINDER=OFF default for
  # Apple targets).
  list(REMOVE_ITEM CRTGFX_CRT_STATIC_LIBS cxx)
  list(APPEND CRTGFX_CRT_STATIC_LIBS
    "${CRT_SYSROOT}/lib/libc++.a"
    "${CRT_SYSROOT}/lib/libc++abi.a"
  )
  list(REMOVE_ITEM CRTGFX_CRT_SHARED_LIBS cxx_shared)
  list(APPEND CRTGFX_CRT_SHARED_LIBS
    "${CRT_SYSROOT}/lib/libc++.dylib"
    "${CRT_SYSROOT}/lib/libc++abi.dylib"
  )
endif()

add_library(crtgfx_common_objects OBJECT ${CRTGFX_COMMON_SOURCES})
target_link_libraries(crtgfx_common_objects PRIVATE crt_build_flags)
target_include_directories(crtgfx_common_objects PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_include_directories(crtgfx_common_objects PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
set_target_properties(crtgfx_common_objects PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

# Host backends are intentionally outside crt_build_flags: this is the narrow
# layer allowed to speak native window/GPU APIs while the common runtime above
# stays on this project's CRT surface. Windows' own backend takes this
# literally (raw dllimport declarations, no <windows.h>, no dependency on
# this project's own headers at all beyond compiler-builtin <stddef.h>/
# <stdint.h>). Linux's Wayland backend is different in kind, not degree: it
# speaks this project's own POSIX/Bionic-shaped surface (memfd_create(),
# sendmsg()/SCM_RIGHTS, mmap(), poll(), <sys/socket.h>/<sys/un.h>), the same
# as every other CRT/PAL source file, not a foreign host SDK -- so it needs
# crt_build_flags applied for the exact same reason libc/src/socket.c does:
# without it, plain #include <sys/socket.h> silently falls through to the
# host's own real glibc header/ABI instead of this project's, compiling
# against the wrong struct layouts and linking against symbols not actually
# present in the final -nostdlib/-nodefaultlibs executable at all. Scoped to
# Linux only so Windows/macOS keep the original no-flags freedom.
add_library(crtgfx_backend_objects OBJECT ${CRTGFX_BACKEND_SOURCES})
if(CRT_TARGET_OS STREQUAL "linux")
  target_link_libraries(crtgfx_backend_objects PRIVATE crt_build_flags)
endif()
target_include_directories(crtgfx_backend_objects PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_include_directories(crtgfx_backend_objects PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
if(CRT_TARGET_OS STREQUAL "linux")
  # src/arch/linux/window_wayland.c's own wl_seat/wl_keyboard wiring
  # (2026-08-25, Phase 3 of the "notepad-capability" plan) #includes
  # <xkbcommon/xkbcommon.h> -- see libcrtgfx/third_party/xkbcommon/
  # recipe.json for the port itself.
  target_include_directories(crtgfx_backend_objects PRIVATE
    "${CRTGFX_XKBCOMMON_INSTALL_PREFIX}/include"
  )
  add_dependencies(crtgfx_backend_objects crtgfx-xkbcommon-build)
endif()
set_target_properties(crtgfx_backend_objects PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

set(CRTGFX_SKIA_OBJECTS)
if(CRTGFX_ENABLE_SKIA)
  add_library(crtgfx_skia_objects OBJECT src/skia_bridge.cc)
  target_link_libraries(crtgfx_skia_objects PRIVATE crt_cxx_build_flags)
  if(CRT_TARGET_OS STREQUAL "windows")
    # SK_BUILD_FOR_UNIX: matches tools/build_skia.py's own -DSK_BUILD_FOR_
    # UNIX for Skia's own GN-driven compile (see that file's own fuller
    # comment) -- needed again here since include/private/base/SkFeatures.h
    # auto-detects purely from _WIN32 (which clang's mingw target predefines
    # too) and has no way to tell tools/crt-cc's mingw-target build apart
    # from real MSVC/clang-cl. Without it, SK_ALWAYS_INLINE (SkAttributes.h)
    # expands to the bare MSVC keyword __forceinline, and SkTypes.h tries to
    # #include <crtdbg.h> under SK_DEBUG -- neither exists/parses under
    # this project's own GNU-ABI clang invocation (confirmed for real,
    # 2026-08-23: `error: unknown type name '__forceinline'` then `fatal
    # error: 'crtdbg.h' file not found`, the same failure build_skia.py's
    # own comment already documents for Skia's own sources). Must match
    # whatever libskia.a itself was actually compiled with for any
    # macro-conditional inline code/struct layout in the shared headers to
    # stay consistent -- Skia's own GN build already picked SK_BUILD_FOR_
    # UNIX (via target_os="linux"), never SK_BUILD_FOR_WIN.
    target_compile_definitions(crtgfx_skia_objects PRIVATE SK_BUILD_FOR_UNIX)
  endif()
  if(CRT_TARGET_OS STREQUAL "linux" AND CRTGFX_LINUX_VULKAN_LIB)
    # Real Ganesh/Vulkan offscreen vertical slice (2026-09-03). Skia's own
    # public include/gpu/ganesh/vk/GrVkTypes.h #includes include/private/
    # gpu/vk/SkiaVulkan.h, which only pulls in Skia's own vendored real
    # Vulkan headers (include/third_party/vulkan/vulkan/vulkan_core.h) when
    # SK_USE_INTERNAL_VULKAN_HEADERS is defined at the *consumer's* own
    # compile time too -- this is normally a GN public_config Skia's own
    # BUILD.gn applies automatically to every real GN dependent
    # (config("use_skia_vulkan_headers"), applied wherever skia_use_vulkan
    # is true), but this project links Skia via a hand-collected .a file
    # with hand-written CMake, not a real GN dependency graph, so that
    # propagation has to be reproduced by hand here -- confirmed by reading
    # both files directly, not assumed. Must match tools/build_skia.py's
    # own Linux-branch skia_use_vulkan=true exactly (same condition), or
    # skia_bridge.cc would silently see incompatible Ganesh Vulkan struct
    # layouts against what libskia.a itself was actually built with.
    target_compile_definitions(crtgfx_skia_objects PRIVATE SK_USE_INTERNAL_VULKAN_HEADERS)
  endif()
  if(CRT_TARGET_OS STREQUAL "windows" AND CRT_USE_IMPORTED_LIBCXX)
    # -isystem${CRT_SYSROOT}/include/c++/v1, via target_compile_options (not
    # target_include_directories) so it lands in <FLAGS>, ahead of
    # crt_cxx_build_flags's own inherited chain -- see the matching, fuller
    # comment on crtgfx_skia_raster_smoke's own libc/include isystem just
    # below for why <INCLUDES> can never win that ordering on this project's
    # custom CMAKE_CXX_COMPILE_OBJECT rule. See the CRTGFX_ENABLE_SKIA guard
    # above (top of this file) for why this project's own imported libc++,
    # not any host substitute, is required here.
    target_compile_options(crtgfx_skia_objects PRIVATE
      "-isystem${CRT_SYSROOT}/include/c++/v1"
    )
  endif()
  target_include_directories(crtgfx_skia_objects SYSTEM BEFORE PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CRTGFX_SKIA_ROOT}"
  )
  target_include_directories(crtgfx_skia_objects SYSTEM BEFORE PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/libc/include"
  )
  set_target_properties(crtgfx_skia_objects PROPERTIES
    POSITION_INDEPENDENT_CODE ON
  )
  set(CRTGFX_SKIA_OBJECTS $<TARGET_OBJECTS:crtgfx_skia_objects>)
endif()
if(CRT_TARGET_OS STREQUAL "windows")
  target_compile_definitions(crtgfx_backend_objects PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
  )

  find_file(CRTGFX_WINDOWS_USER32_LIB
    NAMES user32.lib
    PATHS ${CRT_WINDOWS_SDK_LIB_CANDIDATES}
    NO_DEFAULT_PATH
  )
  find_file(CRTGFX_WINDOWS_GDI32_LIB
    NAMES gdi32.lib
    PATHS ${CRT_WINDOWS_SDK_LIB_CANDIDATES}
    NO_DEFAULT_PATH
  )
  # d3d11.lib/dxgi.lib: real flat-export import libraries for
  # D3D11CreateDevice()/CreateDXGIFactory1() (window_win32.c's own
  # crtgfx_win_create_swap_chain(), added 2026-08-30 for a genuinely
  # asynchronous CRTGFX_EVENT_FRAME_COMPLETE -- see that file's own top
  # comment). dxguid.lib provides the real IID_* GUID *symbols* d3d11.h/
  # dxgi1_2.h/dxgi1_3.h only `EXTERN_C const IID IID_...;` declare --
  # this file defines its own equivalent GUID constants directly instead
  # (crtgfx_iid_idxgi_factory2/crtgfx_iid_id3d11_texture2d/crtgfx_iid_
  # idxgi_swapchain2), matching this file's own no-host-SDK-header style,
  # so dxguid.lib itself is NOT needed here (confirmed via dxgi_probe.c,
  # the real standalone probe this mechanism was validated with before
  # landing here -- that probe DOES need dxguid.lib, since it uses the
  # real SDK's own `IID_IDXGIFactory2`-style extern symbols directly, but
  # this project's own hand-rolled constants sidestep that entirely).
  find_file(CRTGFX_WINDOWS_D3D11_LIB
    NAMES d3d11.lib
    PATHS ${CRT_WINDOWS_SDK_LIB_CANDIDATES}
    NO_DEFAULT_PATH
  )
  find_file(CRTGFX_WINDOWS_DXGI_LIB
    NAMES dxgi.lib
    PATHS ${CRT_WINDOWS_SDK_LIB_CANDIDATES}
    NO_DEFAULT_PATH
  )
  if(NOT CRTGFX_WINDOWS_USER32_LIB)
    set(CRTGFX_WINDOWS_USER32_LIB user32.lib CACHE FILEPATH "Windows user32 import library")
  endif()
  if(NOT CRTGFX_WINDOWS_GDI32_LIB)
    set(CRTGFX_WINDOWS_GDI32_LIB gdi32.lib CACHE FILEPATH "Windows gdi32 import library")
  endif()
  if(NOT CRTGFX_WINDOWS_D3D11_LIB)
    set(CRTGFX_WINDOWS_D3D11_LIB d3d11.lib CACHE FILEPATH "Windows d3d11 import library")
  endif()
  if(NOT CRTGFX_WINDOWS_DXGI_LIB)
    set(CRTGFX_WINDOWS_DXGI_LIB dxgi.lib CACHE FILEPATH "Windows dxgi import library")
  endif()
endif()

# window_cocoa.c drives real AppKit/Foundation/QuartzCore/CoreGraphics
# from plain C via the Objective-C runtime (objc_msgSend/objc_getClass/
# ...) rather than #include-ing any Cocoa header -- see that file's own
# top comment. Those frameworks and libobjc still need to be on the
# link line, the same way Windows needs real user32.lib/gdi32.lib
# above: a static archive member with undefined AppKit/libobjc symbols
# only resolves once something links these in, and crtgfx_shared (an
# actual .dylib) needs them resolved at its own link time regardless.
if(CRT_TARGET_OS STREQUAL "macos")
  set(CRTGFX_MACOS_FRAMEWORKS
    "-framework Foundation"
    "-framework AppKit"
    "-framework QuartzCore"
    "-framework CoreGraphics"
    objc
  )
endif()

add_library(crtgfx STATIC
  $<TARGET_OBJECTS:crtgfx_common_objects>
  $<TARGET_OBJECTS:crtgfx_backend_objects>
  ${CRTGFX_SKIA_OBJECTS}
)
if(CRT_TARGET_OS STREQUAL "linux")
  # CRTGFX_XKBCOMMON_LIBRARIES (further down) references libxkbcommon.a by
  # a plain file path, not a real CMake target -- CMake has no way to know
  # it needs building first without this explicit edge.
  add_dependencies(crtgfx crtgfx-xkbcommon-build)
endif()
target_include_directories(crtgfx PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
if(CRTGFX_ENABLE_SKIA)
  target_include_directories(crtgfx PUBLIC "${CRTGFX_SKIA_ROOT}")
endif()
if(CRT_TARGET_OS STREQUAL "linux" AND CRTGFX_ENABLE_SKIA AND CRTGFX_SKIA_LIBRARIES)
  # --start-group/--end-group: a real GNU-ld circular-archive-resolution
  # gap, confirmed for real (2026-08-23) via the generated link command for
  # crtgfx_skia_raster_smoke -- libskia.a's own undefined needs (`atan2f`/
  # `sinf`/`cosf`/`floor`/`sqrtf` from libm, `operator new`/`delete` from
  # libc++/libc++abi, even `__dso_handle` from libc's own cxa_atexit.c.o)
  # get scanned by ld.bfd's default single left-to-right archive pass
  # *after* libm.a/libc++.a(=cxx)/libc.a already passed, since libskia.a
  # (PUBLIC) is always the last of `crtgfx`'s own dependencies on the final
  # link line regardless of call order here. The exact same class of gap
  # tools/crt-cc's own Linux branch already documents and fixes the
  # identical way for its own, separate link recipes. Wrapped here, in
  # `crtgfx`'s own INTERFACE_LINK_LIBRARIES, rather than on each consumer
  # (crtgfx_skia_raster_smoke, crtgfx_shared, ...): a consumer-side
  # target_link_libraries()/target_link_options() pairing for the group
  # markers does NOT reliably bracket `crtgfx`'s own flattened transitive
  # dependencies -- confirmed for real that CMake re-positions a static
  # library's own propagated INTERFACE_LINK_LIBRARIES independently of
  # where a *consumer's own* separately-added flags land, splitting an
  # attempted --start-group/--end-group pair with `crtgfx`'s own libm.a/
  # libdl.a/libskia.a landing outside it. Declaring the group directly
  # where these libraries are declared avoids relying on that ordering at
  # all.
  target_link_libraries(crtgfx PRIVATE -Wl,--start-group)
  # CRTGFX_XKBCOMMON_LIBRARIES (libxkbcommon.a, src/arch/linux/window_
  # wayland.c's own wl_seat/wl_keyboard wiring, 2026-08-25) before
  # CRTGFX_CRT_STATIC_LIBS: a plain, non-circular need for libc's own
  # strndup/asprintf/mmap/eaccess -- listing it here (ahead of libc.a) is
  # what lets libc.a, scanned after it, satisfy those references, matching
  # every other "dependency before its own provider" ordering already
  # established throughout this file. Empty and therefore a no-op unless
  # this is actually a Linux host with libxkbcommon.a already built (see
  # CRTGFX_XKBCOMMON_LIBRARIES's own definition, near CRTGFX_SKIA_
  # LIBRARIES's, for the auto-detection).
  target_link_libraries(crtgfx PRIVATE ${CRTGFX_XKBCOMMON_LIBRARIES})
  # CRTGFX_LINUX_VULKAN_LIB (src/arch/linux/gpu_vulkan.c, 2026-09-03) --
  # empty and a no-op unless find_library() actually found a real
  # libvulkan.so on this host (see that variable's own find_library()
  # call, near CRTGFX_BACKEND_SOURCES's own linux branch).
  target_link_libraries(crtgfx PRIVATE ${CRTGFX_LINUX_VULKAN_LIB})
  target_link_libraries(crtgfx PRIVATE ${CRTGFX_CRT_STATIC_LIBS})
  target_link_libraries(crtgfx PUBLIC ${CRTGFX_SKIA_LIBRARIES})
  target_link_libraries(crtgfx PRIVATE -Wl,--end-group)
else()
  # Skia first, CRT_STATIC_LIBS (libc++.a on macOS, once CRTGFX_ENABLE_SKIA
  # AND CRT_USE_IMPORTED_LIBCXX swap it in below) second -- reversed from
  # the seemingly more natural "runtime libs, then Skia" order on purpose.
  # Apple ld (this else() branch's only real user: macOS) scans each
  # static archive once, left-to-right, only pulling members needed by
  # symbols already outstanding at that point -- unlike Linux's --start-
  # group/--end-group branch just above, ld64 has no bracket-and-rescan
  # equivalent. libskia.a's own SkSL compiler/parser/debug-trace code
  # makes extensive std::string/iostream/locale/shared_ptr use, so
  # libc++.a must still be "ahead" in scan order when ld reaches it --
  # i.e. mentioned *after* libskia.a on the link line, the reverse of the
  # intuitive "dependencies of a thing come after it" ordering. Confirmed
  # for real (2026-08-23): with CRT_STATIC_LIBS linked first (the original
  # order), crtgfx_skia_raster_smoke/libcrtgfx.dylib both failed with
  # dozens of undefined std::__1::... symbols from libskia.a's own object
  # files, even after fixing which libc++.a was being linked at all (see
  # the CRTGFX_CRT_STATIC_LIBS/SHARED_LIBS macOS substitution above this
  # if/else) and after removing a stray duplicate hardcoded relink on
  # crtgfx_skia_raster_smoke's own target -- only reordering these two
  # calls (no --start-group equivalent, no force_load, no per-consumer
  # override needed) made the link line pass.
  if(CRTGFX_ENABLE_SKIA AND CRTGFX_SKIA_LIBRARIES)
    target_link_libraries(crtgfx PUBLIC ${CRTGFX_SKIA_LIBRARIES})
  endif()
  # See the Linux+Skia branch's own, fuller comment on CRTGFX_XKBCOMMON_
  # LIBRARIES/CRTGFX_LINUX_VULKAN_LIB just above (same file) -- both empty
  # and a no-op on any host other than Linux (and, for the Vulkan one,
  # only non-empty when find_library() actually found a real libvulkan).
  target_link_libraries(crtgfx PRIVATE ${CRTGFX_XKBCOMMON_LIBRARIES})
  target_link_libraries(crtgfx PRIVATE ${CRTGFX_LINUX_VULKAN_LIB})
  target_link_libraries(crtgfx PRIVATE ${CRTGFX_CRT_STATIC_LIBS})
endif()
if(CRT_TARGET_OS STREQUAL "windows")
  target_link_libraries(crtgfx PUBLIC
    "${CRT_WINDOWS_KERNEL32_LIB}"
    "${CRTGFX_WINDOWS_USER32_LIB}"
    "${CRTGFX_WINDOWS_GDI32_LIB}"
    "${CRTGFX_WINDOWS_D3D11_LIB}"
    "${CRTGFX_WINDOWS_DXGI_LIB}"
  )
elseif(CRT_TARGET_OS STREQUAL "macos")
  target_link_libraries(crtgfx PUBLIC ${CRTGFX_MACOS_FRAMEWORKS})
endif()
set_target_properties(crtgfx PROPERTIES
  OUTPUT_NAME crtgfx
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

if(CRT_TARGET_OS STREQUAL "linux" AND CRTGFX_LINUX_VULKAN_LIB)
  # Real, confirmed-for-real bug found bringing up src/arch/linux/
  # gpu_vulkan.c (2026-09-03): the real system libvulkan.so.1 loader
  # itself calls real glibc readdir()/opendir() internally, to scan
  # /usr/share/vulkan/icd.d/*.json and discover the actual GPU driver
  # (ICD) to load. This project's own statically-linked libc exports its
  # own GLOBAL readdir()/opendir()/closedir()/fdopendir() symbols into
  # every executable's dynamic symbol table (confirmed via `readelf
  # --dyn-syms`) -- default ELF symbol interposition rules let the *main
  # executable's* own global symbols silently override the same-named
  # symbol a *shared library* (libvulkan.so.1) resolves at runtime, so
  # libvulkan.so.1's own directory-scanning calls were silently
  # redirected into this project's own readdir(), not glibc's real one.
  # This project's own dirent ABI does not match glibc's real one, so the
  # loader read back truncated/corrupted filenames (confirmed via
  # `strace`: real "gfxstream_vk_icd.json" read back as "k_icd.json"),
  # every real *_icd.json open() failed ENOENT, and vkCreateInstance()
  # correctly, but misleadingly, reported VK_ERROR_INCOMPATIBLE_DRIVER
  # (confirmed by reproducing the exact same real vkCreateInstance() call
  # via plain host gcc + real system headers in the same environment,
  # which succeeded outright -- VK_SUCCESS, 2 real devices -- proving the
  # interop bug lived in this project's own link output, not in the
  # Vulkan environment or gpu_vulkan.c's own hand-rolled bindings).
  #
  # -Wl,--exclude-libs,ALL is the standard, real fix: it keeps symbols
  # that came from a *static archive* (this project's own libc.a/libm.a/
  # libdl.a/libcrtgfx.a, all linked in as .a members, never as a real
  # .so of their own) out of the final executable's/shared-object's own
  # dynamic symbol table, so a real host .so's own internal libc calls
  # resolve to the real glibc it was itself linked against, not to this
  # project's own same-named implementation. Applied as crtgfx's own
  # INTERFACE link option so it reaches every real consumer's own final
  # link line automatically (crtgfx_gpu_test today; the offscreen Ganesh/
  # Vulkan smoke test and any future Linux GPU test tomorrow), without
  # needing this fix repeated on each one by hand -- and on crtgfx_shared
  # directly below (its own real .so link step, not propagated the same
  # way a static archive's INTERFACE options are). Scoped to only apply
  # when a real libvulkan is actually being linked in at all -- no reason
  # to change any other Linux target's own dynamic symbol visibility
  # otherwise.
  target_link_options(crtgfx INTERFACE -Wl,--exclude-libs,ALL)
endif()

set(CRTGFX_SHARED_EXTRA_SOURCES "")
# Windows + Skia only: crtgfx_shared -- unlike the plain `crtgfx` static
# archive, which only records crtgfx_skia_objects's own unresolved real-
# libc++/emutls references for whatever later links it in (crtgfx_skia_
# raster_smoke, already fixed) -- is itself a real DLL, so it must fully
# resolve those same references at its own link time. See crtgfx_skia_
# raster_smoke's own matching comments just above (imported libc++,
# uuid.lib, --allow-multiple-definition, emutls_link_stubs.c) for the full
# story of each piece; this mirrors all of them for the shared/DLL case.
if(CRT_TARGET_OS STREQUAL "windows" AND CRTGFX_ENABLE_SKIA)
  list(APPEND CRTGFX_SHARED_EXTRA_SOURCES
    "${CMAKE_SOURCE_DIR}/libc/src/arch/windows/common/emutls_link_stubs.c")
endif()
add_library(crtgfx_shared SHARED
  $<TARGET_OBJECTS:crtgfx_common_objects>
  $<TARGET_OBJECTS:crtgfx_backend_objects>
  ${CRTGFX_SKIA_OBJECTS}
  ${CRTGFX_SHARED_EXTRA_SOURCES}
)
if(CRT_TARGET_OS STREQUAL "linux")
  add_dependencies(crtgfx_shared crtgfx-xkbcommon-build)
endif()
target_include_directories(crtgfx_shared PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
if(CRTGFX_ENABLE_SKIA)
  target_include_directories(crtgfx_shared PUBLIC "${CRTGFX_SKIA_ROOT}")
endif()
if(CRT_TARGET_OS STREQUAL "windows" AND CRTGFX_ENABLE_SKIA AND CRT_USE_IMPORTED_LIBCXX)
  # Real imported libc++/libc++abi/libunwind DLL import libraries (Itanium
  # ABI, matching Skia itself), not the bootstrap libstdc++/src/ ABI shim
  # (`cxx_shared`) -- swapped in for just the C++ runtime member of
  # CRTGFX_CRT_SHARED_LIBS, keeping c_shared/m_shared/dl_shared as-is.
  target_link_libraries(crtgfx_shared PRIVATE
    c_shared m_shared dl_shared
    "${CRT_SYSROOT}/lib/libc++.dll.a"
    "${CRT_SYSROOT}/lib/libc++abi.dll.a"
    "${CRT_SYSROOT}/lib/libunwind.dll.a"
  )
else()
  set(CRTGFX_SHARED_SKIA_LINUX_GROUPED OFF)
  if(CRT_TARGET_OS STREQUAL "linux" AND CRTGFX_ENABLE_SKIA AND CRTGFX_SKIA_LIBRARIES)
    set(CRTGFX_SHARED_SKIA_LINUX_GROUPED ON)
    # --start-group/--end-group: see `crtgfx`'s own, fuller comment (same
    # file, its CRTGFX_CRT_STATIC_LIBS/CRTGFX_SKIA_LIBRARIES linking just
    # above) for the full story -- the identical GNU-ld circular-archive-
    # resolution gap applies to crtgfx_shared's own link too.
    target_link_libraries(crtgfx_shared PRIVATE -Wl,--start-group)
  endif()
  if(CRTGFX_SHARED_SKIA_LINUX_GROUPED)
    # CRTGFX_SKIA_LIBRARIES (libskia.a + libfreetype.a, see that variable's
    # own comment) BEFORE CRTGFX_CRT_SHARED_LIBS here, not after: found for
    # real (2026-08-24) wiring Skia's own real FreeType font manager in --
    # ld.lld/GNU ld resolve each SHARED library's own undefined-symbol
    # contribution in a single left-to-right scan (unlike a static archive
    # inside --start-group/--end-group, which genuinely does get rescanned
    # on every pass); cxx_shared's own real libc++.so was already "passed"
    # by the time libskia.a's later archive-member extraction discovered
    # NEW undefined libc++ symbols it needed (SkSLString.cpp's own float-
    # to-string conversion via <sstream>/<locale> -- std::locale::classic()
    # and friends), so those stayed unresolved
    # (`ld.lld: error: undefined symbol: std::__1::locale::classic()`)
    # even though libc++.so genuinely exports them (confirmed directly via
    # `nm -D`) and an isolated iostream/locale-only test program links
    # against the exact same libc++.so cleanly. Putting libskia.a/
    # libfreetype.a first means every libc++ symbol THEY need is already
    # a known undefined the following cxx_shared/libc++.so scan resolves
    # in its own single pass -- confirmed fixed by a real manual relink
    # with this exact reordering before applying it here.
    target_link_libraries(crtgfx_shared PUBLIC ${CRTGFX_SKIA_LIBRARIES})
  endif()
  # CRTGFX_XKBCOMMON_LIBRARIES (libxkbcommon.a): a plain, non-circular,
  # pure-C dependency (strndup/asprintf/mmap/eaccess from libc, nothing
  # from libc++/Skia at all) -- unlike CRTGFX_SKIA_LIBRARIES just above,
  # this needs no special single-shared-library-scan ordering care, only
  # "before CRTGFX_CRT_SHARED_LIBS" so libc, listed after it, can satisfy
  # its own libc references. Applies whether or not CRTGFX_SHARED_SKIA_
  # LINUX_GROUPED is set -- window_wayland.c (the one real consumer) is
  # always compiled on Linux, independent of CRTGFX_ENABLE_SKIA. Empty and
  # a no-op on any host other than Linux (see CRTGFX_XKBCOMMON_LIBRARIES's
  # own definition, near CRTGFX_SKIA_LIBRARIES's).
  target_link_libraries(crtgfx_shared PRIVATE ${CRTGFX_XKBCOMMON_LIBRARIES})
  # CRTGFX_LINUX_VULKAN_LIB (src/arch/linux/gpu_vulkan.c, 2026-09-03) --
  # same shape as CRTGFX_XKBCOMMON_LIBRARIES just above: empty and a no-op
  # unless find_library() actually found a real libvulkan.so on this host.
  target_link_libraries(crtgfx_shared PRIVATE ${CRTGFX_LINUX_VULKAN_LIB})
  target_link_libraries(crtgfx_shared PRIVATE ${CRTGFX_CRT_SHARED_LIBS})
  if(CRTGFX_SHARED_SKIA_LINUX_GROUPED)
    target_link_libraries(crtgfx_shared PRIVATE -Wl,--end-group)
  endif()
endif()
target_link_libraries(crtgfx_shared PRIVATE crt_build_flags)
if(NOT CRTGFX_SHARED_SKIA_LINUX_GROUPED AND CRTGFX_ENABLE_SKIA AND CRTGFX_SKIA_LIBRARIES)
  target_link_libraries(crtgfx_shared PUBLIC ${CRTGFX_SKIA_LIBRARIES})
endif()
if(CRT_TARGET_OS STREQUAL "windows")
  target_sources(crtgfx_shared PRIVATE $<TARGET_OBJECTS:crt_windows_dllcrt>)
  target_link_libraries(crtgfx_shared PRIVATE
    "${CRT_WINDOWS_KERNEL32_LIB}"
    "${CRTGFX_WINDOWS_USER32_LIB}"
    "${CRTGFX_WINDOWS_GDI32_LIB}"
    "${CRTGFX_WINDOWS_D3D11_LIB}"
    "${CRTGFX_WINDOWS_DXGI_LIB}"
  )
  if(CRTGFX_ENABLE_SKIA)
    target_link_libraries(crtgfx_shared PRIVATE crt_compiler_rt_builtins)
    find_file(CRTGFX_WINDOWS_UUID_LIB
      NAMES Uuid.Lib uuid.lib
      PATHS ${CRT_WINDOWS_SDK_LIB_CANDIDATES}
      NO_DEFAULT_PATH
    )
    if(NOT CRTGFX_WINDOWS_UUID_LIB)
      set(CRTGFX_WINDOWS_UUID_LIB uuid.lib CACHE FILEPATH "Windows uuid import library")
    endif()
    target_link_libraries(crtgfx_shared PRIVATE "${CRTGFX_WINDOWS_UUID_LIB}")
    target_link_options(crtgfx_shared PRIVATE -Wl,--allow-multiple-definition)
  endif()
elseif(CRT_TARGET_OS STREQUAL "macos")
  target_link_libraries(crtgfx_shared PRIVATE ${CRTGFX_MACOS_FRAMEWORKS})
elseif(CRT_TARGET_OS STREQUAL "linux" AND CRTGFX_ENABLE_SKIA)
  # -fuse-ld=lld: see crtgfx_skia_raster_smoke's own, fuller comment (same
  # file, its Linux branch) for the full ld.bfd-vs-lld story.
  target_link_options(crtgfx_shared PRIVATE -fuse-ld=lld)
endif()
if(CRT_TARGET_OS STREQUAL "linux" AND CRTGFX_LINUX_VULKAN_LIB)
  # See crtgfx's own, fuller comment (same file, right after
  # set_target_properties(crtgfx PROPERTIES ...)) for the real bug this
  # fixes. PRIVATE here, not INTERFACE: crtgfx_shared has its own real
  # .so link step (unlike the plain `crtgfx` static archive), so this
  # only needs to apply directly to it, not propagate further.
  target_link_options(crtgfx_shared PRIVATE -Wl,--exclude-libs,ALL)
endif()
crt_configure_shared_runtime(crtgfx_shared)
set_target_properties(crtgfx_shared PROPERTIES
  OUTPUT_NAME crtgfx
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)
if(CRT_TARGET_OS STREQUAL "windows")
  set_target_properties(crtgfx_shared PROPERTIES
    ARCHIVE_OUTPUT_NAME crtgfx_dll
  )
endif()

install(TARGETS crtgfx crtgfx_shared
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include
  PATTERN ".gitkeep" EXCLUDE
)
if(CRTGFX_ENABLE_SKIA)
  # Deliberately no trailing slash on the source path (unlike the plain
  # `include/` install just above) -- CMake's install(DIRECTORY) keeps the
  # source directory's own basename one level down when there is no
  # trailing slash, which is exactly what is wanted here: every real Skia
  # header, both this project's own consumer code (crtgfx/skia.h, skia_
  # bridge.cc, the Skia test binaries) and Skia's own internal cross-
  # header includes (never patched -- see docs/libcrtgfx_api_policy.md's
  # "use normal Skia headers unmodified" policy), spells this exact same
  # include as `#include "include/core/SkXxx.h"`, matching Skia's own
  # upstream repo-relative path convention verbatim. Since the include
  # search path is plain "${CRT_SYSROOT}/include" everywhere (the same
  # one every other CRT header already resolves through, no Skia-specific
  # -I flag), the only way that literal "include/core/..." prefix
  # resolves is if the real files live one level deeper, at
  # "${CRT_SYSROOT}/include/include/core/...". Renaming this destination
  # (e.g. to "skia") would require rewriting every one of Skia's own
  # internal header includes to match -- impractical, and would have to
  # be redone on every future Skia version bump -- so the nested
  # include/include/ path here is intentional, not a mistake.
  install(DIRECTORY "${CRTGFX_SKIA_ROOT}/include" DESTINATION include)
endif()

# CRT_STARTUP_END_OBJECTS deliberately not listed here -- see
# crt_link_startup_end_objects()'s own comment (top-level CMakeLists.txt).
add_executable(crtgfx_window_smoke
  ${CRT_STARTUP_OBJECTS}
  tests/window_smoke_test.c
)
target_link_libraries(crtgfx_window_smoke PRIVATE crt_build_flags c crtgfx)
target_include_directories(crtgfx_window_smoke PRIVATE
  "${CMAKE_SOURCE_DIR}/libc/include"
)
target_link_libraries(crtgfx_window_smoke PRIVATE crt_compiler_rt_builtins)
target_link_options(crtgfx_window_smoke PRIVATE
  -nostdlib
  -nostartfiles
  -nodefaultlibs
)
if(CRT_TARGET_OS STREQUAL "macos")
  target_link_options(crtgfx_window_smoke PRIVATE
    -Wl,-e,_start
    -Wl,-dead_strip
  )
  target_link_libraries(crtgfx_window_smoke PRIVATE System)
elseif(CRT_TARGET_OS STREQUAL "linux")
  target_link_options(crtgfx_window_smoke PRIVATE
    -Wl,-e,_start
    -Wl,--gc-sections
  )
elseif(CRT_TARGET_OS STREQUAL "windows")
  target_link_options(crtgfx_window_smoke PRIVATE
    -Wl,/entry:mainCRTStartup
    -Wl,/subsystem:console
    -Wl,/OPT:REF
  )
  target_link_libraries(crtgfx_window_smoke PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
endif()
crt_link_startup_end_objects(crtgfx_window_smoke)
add_dependencies(crtgfx_window_smoke c crt1)

add_test(NAME crtgfx_window_smoke_runs COMMAND crtgfx_window_smoke)
set_tests_properties(crtgfx_window_smoke_runs PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PASS_REGULAR_EXPRESSION "crtgfx_window_smoke: ok"
)
set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtgfx_window_smoke_runs)

# crtgfx_synthetic_event: Phase 2 of the window/event API completion plan
# (2026-08-30) -- deterministic event-queue coverage (ordering, overflow/
# drop-newest, multi-window isolation) via crtgfx_window_inject_event(),
# a testing-only hook needing no real OS input delivery. See tests/
# synthetic_event_test.c's own top comment for the full scope and why it
# still needs a real host backend connection (same as crtgfx_window_
# smoke) even though it needs no real *input*. Mirrors crtgfx_window_
# smoke's own target shape exactly -- same startup objects, same per-OS
# link options, same crt_link_startup_end_objects()/add_dependencies()
# calls -- since both are plain C executables linking the same crtgfx
# library the same way.
add_executable(crtgfx_synthetic_event
  ${CRT_STARTUP_OBJECTS}
  tests/synthetic_event_test.c
)
target_link_libraries(crtgfx_synthetic_event PRIVATE crt_build_flags c crtgfx)
target_include_directories(crtgfx_synthetic_event PRIVATE
  "${CMAKE_SOURCE_DIR}/libc/include"
)
target_link_libraries(crtgfx_synthetic_event PRIVATE crt_compiler_rt_builtins)
target_link_options(crtgfx_synthetic_event PRIVATE
  -nostdlib
  -nostartfiles
  -nodefaultlibs
)
if(CRT_TARGET_OS STREQUAL "macos")
  target_link_options(crtgfx_synthetic_event PRIVATE
    -Wl,-e,_start
    -Wl,-dead_strip
  )
  target_link_libraries(crtgfx_synthetic_event PRIVATE System)
elseif(CRT_TARGET_OS STREQUAL "linux")
  target_link_options(crtgfx_synthetic_event PRIVATE
    -Wl,-e,_start
    -Wl,--gc-sections
  )
elseif(CRT_TARGET_OS STREQUAL "windows")
  target_link_options(crtgfx_synthetic_event PRIVATE
    -Wl,/entry:mainCRTStartup
    -Wl,/subsystem:console
    -Wl,/OPT:REF
  )
  target_link_libraries(crtgfx_synthetic_event PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
endif()
crt_link_startup_end_objects(crtgfx_synthetic_event)
add_dependencies(crtgfx_synthetic_event c crt1)

add_test(NAME crtgfx_synthetic_event_runs COMMAND crtgfx_synthetic_event)
set_tests_properties(crtgfx_synthetic_event_runs PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PASS_REGULAR_EXPRESSION "crtgfx_synthetic_event: ok"
)
set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtgfx_synthetic_event_runs)

# crtgfx_gpu_test: real coverage for crtgfx/gpu.h (TODO.md's upper-runtime
# roadmap "Fix the common GPU resource contract" step) -- argument
# validation, crtgfx_gpu_query_capabilities()'s own real, honest
# CRTGFX_GPU_BACKEND_NONE/0-devices report, and a real, working,
# cross-thread crtgfx_gpu_fence wait/signal/timeout (a real
# pthread_create()'d signaler thread). Needs no real host window/display
# connection (unlike crtgfx_synthetic_event above), so no rootfs/backend
# dependency -- same plain-C-executable shape as libcrtmedia's own
# crtmedia_player_test (real pthread_create() usage, no host GPU/display
# resource needed at all).
add_executable(crtgfx_gpu_test
  ${CRT_STARTUP_OBJECTS}
  tests/gpu_test.c
)
target_link_libraries(crtgfx_gpu_test PRIVATE crt_build_flags c crtgfx)
target_include_directories(crtgfx_gpu_test PRIVATE
  "${CMAKE_SOURCE_DIR}/libc/include"
)
target_link_libraries(crtgfx_gpu_test PRIVATE crt_compiler_rt_builtins)
target_link_options(crtgfx_gpu_test PRIVATE
  -nostdlib
  -nostartfiles
  -nodefaultlibs
)
if(CRT_TARGET_OS STREQUAL "macos")
  target_link_options(crtgfx_gpu_test PRIVATE
    -Wl,-e,_start
    -Wl,-dead_strip
  )
  target_link_libraries(crtgfx_gpu_test PRIVATE System)
elseif(CRT_TARGET_OS STREQUAL "linux")
  target_link_options(crtgfx_gpu_test PRIVATE
    -Wl,-e,_start
    -Wl,--gc-sections
  )
elseif(CRT_TARGET_OS STREQUAL "windows")
  target_link_options(crtgfx_gpu_test PRIVATE
    -Wl,/entry:mainCRTStartup
    -Wl,/subsystem:console
    -Wl,/OPT:REF
  )
  target_link_libraries(crtgfx_gpu_test PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
endif()
crt_link_startup_end_objects(crtgfx_gpu_test)
add_dependencies(crtgfx_gpu_test c crt1)

add_test(NAME crtgfx_gpu_test_runs COMMAND crtgfx_gpu_test)
set_tests_properties(crtgfx_gpu_test_runs PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PASS_REGULAR_EXPRESSION "crtgfx_gpu_test: ok"
)
set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtgfx_gpu_test_runs)

# crt_wire_skia_executable(target_name): every Skia-specific compile/link
# flag a real, executable C++ crtgfx consumer needs, extracted into a
# reusable function so a second such consumer (crtgfx_keyboard_interactive,
# further down) can reuse the exact same, already hard-won, platform-
# specific logic instead of a second hand-copy silently drifting out of
# sync with this one over time. The caller is responsible for its own
# add_executable()/crt_link_startup_end_objects()/add_dependencies()/
# add_test() calls -- this function only adds the Skia/FreeType/imported-
# libc++-specific pieces on top of whatever base wiring the target already
# has (matching crtgfx_window_demo's own plain nostdlib/entry-point shape,
# which crtgfx_keyboard_interactive already inherited when it was first
# defined) -- every real fact and every platform-specific bug this
# function's own body works around was found and confirmed exactly once,
# for crtgfx_skia_raster_smoke specifically (see each block's own dated
# comment for the full story); nothing here is guessed for the second
# caller.
function(crt_wire_skia_executable target_name)
  if(CRT_TARGET_OS STREQUAL "windows")
    # SK_BUILD_FOR_UNIX: see crtgfx_skia_objects's own, fuller comment just
    # above (same reasoning; this target also #includes Skia headers
    # directly via crtgfx/skia.h).
    target_compile_definitions(${target_name} PRIVATE SK_BUILD_FOR_UNIX)
    # emutls_link_stubs.c (__acrt_iob_func/__stdio_common_vfprintf link
    # stubs -- see that file's own top comment: needed once -femulated-
    # tls's own compiler-rt implementation, emutls.c, actually gets linked
    # in, which any real Windows+Skia *executable* does once it fully
    # resolves symbols at its own link time -- matches crtgfx_shared's own
    # identical, separately-added copy just above for the DLL case).
    # target_sources(), not a source added at this target's own original
    # add_executable() call: this function is applied to more than one
    # caller's target (crtgfx_skia_raster_smoke, crtgfx_keyboard_
    # interactive, ...), and CMake supports adding sources to an already-
    # defined target exactly this way. Found missing for real (2026-08-25):
    # crtgfx_keyboard_interactive linked against Skia/FreeType fine but
    # failed with `undefined symbol: __acrt_iob_func`/`__stdio_common_
    # vfprintf` -- this function's own extraction from crtgfx_skia_raster_
    # smoke's original inline block (which added this file directly to
    # its own add_executable() sources) had carried over every compile/
    # link *flag* but not this one *source file*, since the file list
    # itself was never part of what got refactored into this function.
    target_sources(${target_name} PRIVATE
      "${CMAKE_SOURCE_DIR}/libc/src/arch/windows/common/emutls_link_stubs.c")
  endif()
  if(CRT_TARGET_OS STREQUAL "linux" AND CRTGFX_LINUX_VULKAN_LIB)
    # See crtgfx_skia_objects's own, fuller comment on SK_USE_INTERNAL_
    # VULKAN_HEADERS just above -- this target also #includes Skia's own
    # public Ganesh Vulkan headers directly (e.g. GrVkTypes.h), so it
    # needs the identical define at its own compile time too.
    target_compile_definitions(${target_name} PRIVATE SK_USE_INTERNAL_VULKAN_HEADERS)
  endif()
  # CRT_SKIA_FONTS_DIR: absolute path to the bundled real font(s) this
  # target's own real-text-rendering code scans via SkFontMgr_New_Custom_
  # Directory() -- Pretendard GOV (the project-wide default as of
  # 2026-08-29) and porting/recipes/freetype.json's own DejaVuSansMono.ttf,
  # see libcrtgfx/assets/fonts/README.md for provenance/license -- same
  # compile-time-path-define convention porting/tests/freetype_glyph_
  # test.c's own CRT_TEST_FONT_PATH already uses. port-build-freetype
  # itself is an explicit dependency the caller adds alongside its own
  # add_dependencies() call (this target also needs libfreetype.a built
  # and installed before it can link at all, via CRTGFX_SKIA_LIBRARIES).
  target_compile_definitions(${target_name} PRIVATE
    CRT_SKIA_FONTS_DIR="${CMAKE_SOURCE_DIR}/libcrtgfx/assets/fonts")
  if(CRT_TARGET_OS STREQUAL "windows" AND CRT_USE_IMPORTED_LIBCXX)
    # -isystem${CRT_SYSROOT}/include/c++/v1, added *before* the libc/include
    # one just below (both via target_compile_options, so both land in
    # <FLAGS> in call order) -- this project's own imported libc++, matching
    # what compiles skia_bridge.cc/Skia itself, not any host substitute. See
    # the CRTGFX_ENABLE_SKIA guard at the top of this file for the fuller
    # story of why this is required on Windows now.
    target_compile_options(${target_name} PRIVATE
      "-isystem${CRT_SYSROOT}/include/c++/v1"
    )
  endif()
  # -isystem<repo>/libc/include, via target_compile_options (NOT target_
  # include_directories -- see the plain CRTGFX_SKIA_ROOT-only call just
  # below for that): confirmed for real (2026-08-23) that this project's
  # own custom CMAKE_CXX_COMPILE_OBJECT rule (top-level CMakeLists.txt,
  # "place C++ flags first so crt_cxx_build_flags can establish the same
  # ordering as crt-c++") emits <FLAGS> before <INCLUDES> on the actual
  # command line, and crt_cxx_build_flags's own real-Windows-SDK isystem
  # chain (CRT_CXX_STANDARD_INCLUDE_DIRS -- ucrt/shared/um/..., deliberately
  # kept reachable there for libc++'s own internal include_next chaining)
  # lives in FLAGS too, inherited via target_link_libraries above -- so a
  # target_include_directories(... SYSTEM BEFORE ...) call (which lands in
  # <INCLUDES>, always emitted *after* <FLAGS> by this same custom rule)
  # can never actually win against it, regardless of the BEFORE keyword.
  # This is a real, previously-latent bug: no earlier C++ test ever did a
  # plain #include <stdio.h>, so nothing had reached ucrt's own inline
  # printf() definition before -- confirmed for real: `lld-link: error:
  # duplicate symbol: printf`, one copy from
  # `.../ucrt/stdio.h:956` (this project's own libc/include/stdio.h was
  # never actually consulted at all) and one from this project's own real
  # `c.lib(printf.c.obj)`. target_compile_options() calls made directly on
  # a target land in <FLAGS> ahead of options inherited via
  # target_link_libraries (confirmed for real by inspecting the
  # regenerated build.ninja: this -isystem now appears before crt_cxx_
  # build_flags's own chain), which is what actually fixes the ordering
  # here -- target_include_directories()'s own BEFORE keyword only orders
  # within <INCLUDES>, a different slot entirely.
  target_compile_options(${target_name} PRIVATE
    "-isystem${CMAKE_SOURCE_DIR}/libc/include"
  )
  target_include_directories(${target_name} SYSTEM BEFORE PRIVATE
    "${CRTGFX_SKIA_ROOT}"
  )
  if(CRT_TARGET_OS STREQUAL "windows" AND CRT_USE_IMPORTED_LIBCXX)
    # Real imported libc++/libc++abi/libunwind (Itanium ABI, matching Skia
    # itself), not the small bootstrap libstdc++/src/ ABI shim (`cxx`) --
    # see the CRTGFX_ENABLE_SKIA guard at the top of this file. Mirrors
    # tools/crt-c++'s own cxx_runtime_libs static-link selection.
    target_link_libraries(${target_name} PRIVATE
      "${CRT_SYSROOT}/lib/libc++.a"
      "${CRT_SYSROOT}/lib/libc++abi.a"
      "${CRT_SYSROOT}/lib/libunwind.a"
      c
    )
  elseif(CRT_TARGET_OS STREQUAL "linux" AND CRTGFX_ENABLE_SKIA AND CRTGFX_SKIA_LIBRARIES)
    # Deliberately NOT an explicit `cxx c` link here, unlike the other
    # branches -- `crtgfx`'s own --start-group/--end-group-wrapped link
    # (see that target's own, fuller comment) already PRIVATE-propagates
    # cxx/c/m/dl transitively, *inside* the group. An explicit mention
    # here would land *outside* it (this call happens before `crtgfx`
    # itself gets flattened into the link line) and CMake's own link-line
    # de-duplication keeps only that first, ungrouped occurrence, silently
    # dropping the grouped one -- confirmed for real (2026-08-23): with
    # this explicit link left in, `lib/libc++.a`(=cxx) landed *before*
    # `-Wl,--start-group`, and libskia.a's own `operator new`/`operator
    # delete` needs (defined in cxx's own new_delete.cc) stayed
    # unresolved even with the group fix in place for libm's symbols.
  elseif(CRT_TARGET_OS STREQUAL "macos" AND CRT_USE_IMPORTED_LIBCXX)
    # No explicit libc++/libc++abi relink needed here at all (unlike the
    # Windows branch above) -- crtgfx's own CRTGFX_CRT_STATIC_LIBS swap
    # (see that swap's own comment, same file) already propagates the
    # real imported libc++.a/libc++abi.a to every consumer, and crtgfx's
    # own build now links Skia's libraries *before* CRTGFX_CRT_STATIC_LIBS
    # (see the plain else() branch of crtgfx's own target_link_libraries
    # calls, above in this file) specifically so libc++.a lands *after*
    # libskia.a on the final link line -- Apple ld's single-pass,
    # left-to-right static-archive scan needs it there, since libskia.a's
    # own extensive std::string/iostream/locale use (SkSL's compiler/
    # parser/debug-trace code) must still be outstanding when ld reaches
    # it. All this branch needs to do is what the `else()` below does for
    # every other platform *except* stop it from also linking the small
    # bootstrap libstdc++/src/ ABI shim (`cxx`) on top of the real one
    # crtgfx already provides -- confirmed for real (2026-08-23): leaving
    # the `else()` branch's `cxx` mention in place put a second, different
    # libc++.a on the link line even after the ordering fix above.
    target_link_libraries(${target_name} PRIVATE c)
  else()
    target_link_libraries(${target_name} PRIVATE cxx c)
  endif()
  target_link_libraries(${target_name} PRIVATE crt_compiler_rt_builtins)
  target_link_options(${target_name} PRIVATE
    -nostdlib
    -nostartfiles
    -nodefaultlibs
  )
  if(CRT_TARGET_OS STREQUAL "macos")
    target_link_options(${target_name} PRIVATE
      -Wl,-e,_start
      -Wl,-dead_strip
    )
    target_link_libraries(${target_name} PRIVATE System)
  elseif(CRT_TARGET_OS STREQUAL "linux")
    target_link_options(${target_name} PRIVATE
      -Wl,-e,_start
      -Wl,--gc-sections
    )
    if(CRTGFX_ENABLE_SKIA)
      # -fuse-ld=lld: this project's regular Linux executables link fine
      # with the system default (ld.bfd) -- this override is scoped to the
      # Skia-enabled targets specifically, not project-wide. Confirmed for
      # real (2026-08-23), bisected down from a genuine runtime failure:
      # `error while loading shared libraries: unexpected PLT reloc type
      # 0x00`. `readelf -r` showed the built executable's `.rela.plt`
      # holding exactly one bogus entry (R_X86_64_NONE, sym 0, offset 0)
      # that glibc's own ld.so refuses to process -- present even in a
      # from-scratch, fully-symbol-resolved relink of the exact same
      # inputs (ruled out: --gc-sections, --unresolved-symbols=ignore-all
      # artifacts, and the -Wl,--start-group/--end-group wrapping alone --
      # none of those reproduced it in isolation; only the *combination*
      # actually used here, through ld.bfd specifically, did). Switching
      # just the linker backend to lld (`-fuse-ld=lld`, otherwise
      # unchanged) produced a clean `.rela.plt` (no bogus entry) from the
      # identical link inputs and flags -- a real, known-ish class of
      # ld.bfd fragility around --start-group/PIE archive linking that lld
      # doesn't share. Requires `lld` installed on the build host when
      # CRTGFX_ENABLE_SKIA=ON on Linux (not a default Ubuntu package).
      target_link_options(${target_name} PRIVATE -fuse-ld=lld)
    endif()
  elseif(CRT_TARGET_OS STREQUAL "windows")
    target_link_options(${target_name} PRIVATE
      -Wl,/entry:mainCRTStartup
      -Wl,/subsystem:console
      -Wl,/OPT:REF
    )
    target_link_libraries(${target_name} PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
    # uuid.lib: something reachable from this target's own link set (bisected
    # for real, 2026-08-23, down to needing *both* crtgfx.a and libskia.a
    # present together -- neither alone triggers it, and no embedded COFF
    # .drectve string naming "uuid" turned up in any linked archive via
    # direct `strings` inspection) makes lld-link, in this mingw-target
    # -Bdynamic mode, go looking for "libuuid.a" -- `ld.lld: error: could
    # not open 'libuuid.a': no such file or directory` otherwise. Supplying
    # the real Windows SDK import library directly resolves it regardless
    # of the exact underlying trigger, matching this file's own existing
    # user32.lib/gdi32.lib find_file() precedent just above.
    find_file(CRTGFX_WINDOWS_UUID_LIB
      NAMES Uuid.Lib uuid.lib
      PATHS ${CRT_WINDOWS_SDK_LIB_CANDIDATES}
      NO_DEFAULT_PATH
    )
    if(NOT CRTGFX_WINDOWS_UUID_LIB)
      set(CRTGFX_WINDOWS_UUID_LIB uuid.lib CACHE FILEPATH "Windows uuid import library")
    endif()
    target_link_libraries(${target_name} PRIVATE "${CRTGFX_WINDOWS_UUID_LIB}")
    # --allow-multiple-definition: clang_rt.builtins-x86_64.lib (this
    # target's own crt_compiler_rt_builtins, needed for __emutls_get_
    # address -- see tools/crt-cc's own -femulated-tls comment) is a real,
    # vendor-shipped MSVC-layout LLVM archive that bundles more than pure
    # arithmetic builtins: at least two of its own object members (emutls.
    # c.obj, enable_execute_stack.c.obj) each carry their own internal
    # fallback `fprintf` (itself calling the real UCRT's __stdio_common_
    # vfprintf, so clearly meant for a real-CRT host environment, not this
    # freestanding one) -- confirmed for real via llvm-nm. Once anything
    # pulls either member in, COFF/lld-link's own archive semantics treat
    # every symbol *that member exports* as now part of the final image, so
    # its own fprintf collides outright with this project's real, correct
    # `fprintf` in libc.a: `ld.lld: error: duplicate symbol: fprintf`. This
    # project's own libc.a is linked well before clang_rt.builtins-x86_64.
    # lib on this target's own link line, and lld's own documented --allow-
    # multiple-definition behavior (matching GNU ld) is "keep the first
    # definition encountered" -- so this reliably keeps our real fprintf,
    # not the vendor archive's unused internal fallback.
    target_link_options(${target_name} PRIVATE
      -Wl,--allow-multiple-definition
    )
  endif()
endfunction()

if(CRTGFX_ENABLE_SKIA)
  # CRT_STARTUP_END_OBJECTS deliberately not listed here -- see
  # crt_link_startup_end_objects()'s own comment (top-level CMakeLists.txt).
  # emutls_link_stubs.c (Windows-only) is added by crt_wire_skia_
  # executable() itself now, via target_sources() -- not this add_
  # executable() call's own source list -- since that function is shared
  # with crtgfx_keyboard_interactive too (see that function's own comment
  # for the real bug this consolidation fixed).
  add_executable(crtgfx_skia_raster_smoke
    ${CRT_STARTUP_OBJECTS}
    tests/skia_raster_smoke.cc
  )
  target_link_libraries(crtgfx_skia_raster_smoke PRIVATE crt_cxx_build_flags crtgfx)
  crt_wire_skia_executable(crtgfx_skia_raster_smoke)
  crt_link_startup_end_objects(crtgfx_skia_raster_smoke)
  add_dependencies(crtgfx_skia_raster_smoke cxx c crt1 crtgfx port-build-freetype)

  add_test(NAME crtgfx_skia_raster_smoke_runs COMMAND crtgfx_skia_raster_smoke)
  set_tests_properties(crtgfx_skia_raster_smoke_runs PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    PASS_REGULAR_EXPRESSION "crtgfx_skia_raster_smoke: ok"
  )
  set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtgfx_skia_raster_smoke_runs)

  # crtgfx_skia_cpu_coverage: deterministic path/transform/clip/save-
  # restore/layer/shader/blend-mode/image-scale/error-path regression
  # coverage (tests/skia_cpu_coverage_test.cc, TODO.md's "Broaden
  # deterministic Skia CPU coverage" item). Unlike crtgfx_skia_raster_
  # smoke just above, this target never calls crtgfx_window_create() at
  # all -- every test builds its own crtgfx_framebuffer directly over a
  # plain malloc'd buffer, so it needs no real host window/display
  # connection and runs identically headless on every CI runner, matching
  # crtgfx_synthetic_event's own "deterministic, no real OS resource
  # needed" property for the window/event side of this API. No port-
  # build-freetype dependency, unlike crtgfx_skia_raster_smoke: this file
  # draws no text at all.
  add_executable(crtgfx_skia_cpu_coverage
    ${CRT_STARTUP_OBJECTS}
    tests/skia_cpu_coverage_test.cc
  )
  target_link_libraries(crtgfx_skia_cpu_coverage PRIVATE crt_cxx_build_flags crtgfx)
  crt_wire_skia_executable(crtgfx_skia_cpu_coverage)
  crt_link_startup_end_objects(crtgfx_skia_cpu_coverage)
  add_dependencies(crtgfx_skia_cpu_coverage cxx c crt1 crtgfx)

  add_test(NAME crtgfx_skia_cpu_coverage_runs COMMAND crtgfx_skia_cpu_coverage)
  set_tests_properties(crtgfx_skia_cpu_coverage_runs PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    PASS_REGULAR_EXPRESSION "crtgfx_skia_cpu_coverage: ok"
  )
  set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtgfx_skia_cpu_coverage_runs)

  # crtgfx_skia_gpu_offscreen_smoke: the real Ganesh/Vulkan offscreen
  # vertical slice's own rendering-correctness coverage (2026-09-03,
  # TODO.md's "Enable Skia GPU rendering" step -- see that file's own top
  # comment for the full design record). Linux + a real libvulkan found at
  # configure time only, matching src/arch/linux/gpu_vulkan.c's own real
  # scope -- Windows/macOS keep their existing Skia CPU-raster-only targets
  # untouched. Same headless shape as crtgfx_skia_cpu_coverage just above
  # (no crtgfx_window/host display connection needed at all).
  if(CRT_TARGET_OS STREQUAL "linux" AND CRTGFX_LINUX_VULKAN_LIB)
    add_executable(crtgfx_skia_gpu_offscreen_smoke
      ${CRT_STARTUP_OBJECTS}
      tests/skia_gpu_offscreen_smoke.cc
    )
    target_link_libraries(crtgfx_skia_gpu_offscreen_smoke PRIVATE crt_cxx_build_flags crtgfx)
    # gpu_internal.h (src/gpu_internal.h, not installed/public) -- this
    # test reaches into crtgfx_gpu_device's own real Vulkan handles
    # directly (crtgfx_skia_make_gpu_context()'s own real caller shape,
    # and the device-loss test's own deliberate vkDestroyDevice() call).
    target_include_directories(crtgfx_skia_gpu_offscreen_smoke PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    crt_wire_skia_executable(crtgfx_skia_gpu_offscreen_smoke)
    crt_link_startup_end_objects(crtgfx_skia_gpu_offscreen_smoke)
    add_dependencies(crtgfx_skia_gpu_offscreen_smoke cxx c crt1 crtgfx)

    add_test(NAME crtgfx_skia_gpu_offscreen_smoke_runs COMMAND crtgfx_skia_gpu_offscreen_smoke)
    set_tests_properties(crtgfx_skia_gpu_offscreen_smoke_runs PROPERTIES
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      PASS_REGULAR_EXPRESSION "crtgfx_skia_gpu_offscreen_smoke: (ok|skipped)"
    )
    set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtgfx_skia_gpu_offscreen_smoke_runs)
  endif()

  # crtmedia_frame_skia_smoke: the real-Skia half of libcrtmedia's CPU
  # frame handoff contract (TODO.md's "Define the libcrtmedia CPU frame
  # handoff contract" item -- source at libcrtmedia/tests/frame_skia_
  # smoke.cc, not under this directory). Registered here, not from
  # libcrtmedia/CMakeLists.txt, because crt_wire_skia_executable() (used
  # just above for crtgfx_skia_cpu_coverage) is a CMake function() defined
  # in *this* file's own directory scope -- CMake function definitions do
  # not propagate to a sibling directory's CMakeLists.txt, only to this
  # directory's own descendants, and add_subdirectory(libcrtmedia) is a
  # sibling of add_subdirectory(libcrtgfx) in the top-level CMakeLists.txt
  # (libcrtmedia is processed after this file, but that only means its own
  # `crtmedia`/`crtmedia_shared` targets are forward-referenced by name
  # below, which CMake resolves fine at generate time -- targets are
  # visible project-wide regardless of definition order, unlike functions/
  # variables). libcrtmedia/tests/frame_test.c's own deterministic,
  # Skia-free coverage of the same contract stays in libcrtmedia/
  # CMakeLists.txt, where it belongs and needs none of this. Headless
  # like crtgfx_skia_cpu_coverage just above (own top comment) -- no
  # FreeType/port-build-freetype dependency either, since this draws no
  # text.
  add_executable(crtmedia_frame_skia_smoke
    ${CRT_STARTUP_OBJECTS}
    "${CMAKE_SOURCE_DIR}/libcrtmedia/tests/frame_skia_smoke.cc"
  )
  target_link_libraries(crtmedia_frame_skia_smoke PRIVATE crt_cxx_build_flags crtgfx crtmedia)
  crt_wire_skia_executable(crtmedia_frame_skia_smoke)
  crt_link_startup_end_objects(crtmedia_frame_skia_smoke)
  add_dependencies(crtmedia_frame_skia_smoke cxx c crt1 crtgfx crtmedia)

  add_test(NAME crtmedia_frame_skia_smoke_runs COMMAND crtmedia_frame_skia_smoke)
  set_tests_properties(crtmedia_frame_skia_smoke_runs PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    PASS_REGULAR_EXPRESSION "crtmedia_frame_skia_smoke: ok"
  )
  set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtmedia_frame_skia_smoke_runs)
endif()

# CRT_STARTUP_END_OBJECTS deliberately not listed here -- see
# crt_link_startup_end_objects()'s own comment (top-level CMakeLists.txt).
add_executable(crtgfx_window_demo
  ${CRT_STARTUP_OBJECTS}
  tools/window_demo.c
)
target_link_libraries(crtgfx_window_demo PRIVATE crt_build_flags c crtgfx)
target_include_directories(crtgfx_window_demo PRIVATE
  "${CMAKE_SOURCE_DIR}/libc/include"
)
target_link_libraries(crtgfx_window_demo PRIVATE crt_compiler_rt_builtins)
target_link_options(crtgfx_window_demo PRIVATE
  -nostdlib
  -nostartfiles
  -nodefaultlibs
)
if(CRT_TARGET_OS STREQUAL "macos")
  target_link_options(crtgfx_window_demo PRIVATE
    -Wl,-e,_start
    -Wl,-dead_strip
  )
  target_link_libraries(crtgfx_window_demo PRIVATE System)
elseif(CRT_TARGET_OS STREQUAL "linux")
  target_link_options(crtgfx_window_demo PRIVATE
    -Wl,-e,_start
    -Wl,--gc-sections
  )
elseif(CRT_TARGET_OS STREQUAL "windows")
  target_link_options(crtgfx_window_demo PRIVATE
    -Wl,/entry:mainCRTStartup
    -Wl,/subsystem:console
    -Wl,/OPT:REF
  )
  target_link_libraries(crtgfx_window_demo PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
endif()
crt_link_startup_end_objects(crtgfx_window_demo)
add_dependencies(crtgfx_window_demo c crt1)

# crtgfx_keyboard_interactive: manual, interactive keyboard-input
# verification (see tests/window_keyboard_interactive_test.c's own
# top-of-file comment for why it's not an automated ctest test -- it
# needs a real human typing into the window it opens, and this sandbox
# has no synthetic-input tool like wtype/ydotool to fake that). Added
# 2026-08-25 as a real CMake target specifically so it can be built the
# normal way (`cmake --build --preset ... --target
# crtgfx_keyboard_interactive`) -- earlier in this port it only ever
# existed as an ad hoc raw-clang compile in a scratch session, which a
# real user on real hardware had no way to reproduce. Follows
# crtgfx_window_demo's own exact shape immediately above (a manual,
# non-ctest-registered demo executable), not crtgfx_window_smoke's
# (which does register a ctest test).
add_executable(crtgfx_keyboard_interactive
  ${CRT_STARTUP_OBJECTS}
  tests/window_keyboard_interactive_test.cc
)
# crt_cxx_build_flags, not crt_build_flags: this source is now a real .cc
# file (see that file's own top comment for why -- optional, compile-time-
# gated Skia+FreeType text rendering, matching crtgfx_skia_raster_smoke's
# own identical choice, same reasoning: a plain C target's own crt_build_
# flags never carries the C++-specific flags/standard tools/crt-c++ needs).
target_link_libraries(crtgfx_keyboard_interactive PRIVATE crt_cxx_build_flags c crtgfx)
target_include_directories(crtgfx_keyboard_interactive PRIVATE
  "${CMAKE_SOURCE_DIR}/libc/include"
)
target_link_libraries(crtgfx_keyboard_interactive PRIVATE crt_compiler_rt_builtins)
target_link_options(crtgfx_keyboard_interactive PRIVATE
  -nostdlib
  -nostartfiles
  -nodefaultlibs
)
if(CRT_TARGET_OS STREQUAL "macos")
  target_link_options(crtgfx_keyboard_interactive PRIVATE
    -Wl,-e,_start
    -Wl,-dead_strip
  )
  target_link_libraries(crtgfx_keyboard_interactive PRIVATE System)
elseif(CRT_TARGET_OS STREQUAL "linux")
  target_link_options(crtgfx_keyboard_interactive PRIVATE
    -Wl,-e,_start
    -Wl,--gc-sections
  )
elseif(CRT_TARGET_OS STREQUAL "windows")
  target_link_options(crtgfx_keyboard_interactive PRIVATE
    -Wl,/entry:mainCRTStartup
    -Wl,/subsystem:console
    -Wl,/OPT:REF
  )
  target_link_libraries(crtgfx_keyboard_interactive PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
endif()
crt_link_startup_end_objects(crtgfx_keyboard_interactive)
add_dependencies(crtgfx_keyboard_interactive c crt1)
if(CRTGFX_ENABLE_SKIA)
  # Real Skia+FreeType text rendering, added 2026-08-25 specifically to
  # verify Linux end to end -- draws the actually-typed text on screen
  # (crtgfx/skia.h's own CRTGFX_HAS_SKIA_HEADERS guard in the source file
  # itself keeps this target buildable with CRTGFX_ENABLE_SKIA=OFF too,
  # falling back to the plain gradient-fill behavior it always had).
  # Reuses crt_wire_skia_executable() (this file, just above) rather than
  # hand-copying crtgfx_skia_raster_smoke's own hard-won platform-specific
  # link wiring a second time.
  crt_wire_skia_executable(crtgfx_keyboard_interactive)
  add_dependencies(crtgfx_keyboard_interactive port-build-freetype)
endif()
