set(CRTGFX_COMMON_SOURCES
  src/window.c
  src/wayland_weston.c
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
if(NOT CRTGFX_SKIA_LIBRARIES AND EXISTS "${CRTGFX_SKIA_INSTALL_PREFIX}/lib/libskia.a")
  set(CRTGFX_SKIA_LIBRARIES "${CRTGFX_SKIA_INSTALL_PREFIX}/lib/libskia.a" CACHE STRING "Libraries needed to link Skia CPU raster support" FORCE)
else()
  set(CRTGFX_SKIA_LIBRARIES "" CACHE STRING "Libraries needed to link Skia CPU raster support")
endif()
if(EXISTS "${CRTGFX_SKIA_ROOT}/include/core/SkSurface.h" AND CRTGFX_SKIA_LIBRARIES)
  set(CRTGFX_SKIA_AVAILABLE_DEFAULT ON)
else()
  set(CRTGFX_SKIA_AVAILABLE_DEFAULT OFF)
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
elseif(CRT_TARGET_OS STREQUAL "linux")
  set(CRTGFX_BACKEND_SOURCES src/arch/linux/window_wayland.c)
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
    DEPENDS crtgfx-skia-fetch
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
    DEPENDS crtgfx-skia-fetch
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Building Skia CPU raster with the CRT toolchain"
    USES_TERMINAL
  )
endif()

set(CRTGFX_CRT_STATIC_LIBS c m dl cxx)
set(CRTGFX_CRT_SHARED_LIBS c_shared m_shared dl_shared cxx_shared)

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
  if(NOT CRTGFX_WINDOWS_USER32_LIB)
    set(CRTGFX_WINDOWS_USER32_LIB user32.lib CACHE FILEPATH "Windows user32 import library")
  endif()
  if(NOT CRTGFX_WINDOWS_GDI32_LIB)
    set(CRTGFX_WINDOWS_GDI32_LIB gdi32.lib CACHE FILEPATH "Windows gdi32 import library")
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
target_include_directories(crtgfx PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
if(CRTGFX_ENABLE_SKIA)
  target_include_directories(crtgfx PUBLIC "${CRTGFX_SKIA_ROOT}")
endif()
target_link_libraries(crtgfx PRIVATE ${CRTGFX_CRT_STATIC_LIBS})
if(CRTGFX_ENABLE_SKIA AND CRTGFX_SKIA_LIBRARIES)
  target_link_libraries(crtgfx PUBLIC ${CRTGFX_SKIA_LIBRARIES})
endif()
if(CRT_TARGET_OS STREQUAL "windows")
  target_link_libraries(crtgfx PUBLIC
    "${CRT_WINDOWS_KERNEL32_LIB}"
    "${CRTGFX_WINDOWS_USER32_LIB}"
    "${CRTGFX_WINDOWS_GDI32_LIB}"
  )
elseif(CRT_TARGET_OS STREQUAL "macos")
  target_link_libraries(crtgfx PUBLIC ${CRTGFX_MACOS_FRAMEWORKS})
endif()
set_target_properties(crtgfx PROPERTIES
  OUTPUT_NAME crtgfx
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

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
  target_link_libraries(crtgfx_shared PRIVATE ${CRTGFX_CRT_SHARED_LIBS})
endif()
target_link_libraries(crtgfx_shared PRIVATE crt_build_flags)
if(CRTGFX_ENABLE_SKIA AND CRTGFX_SKIA_LIBRARIES)
  target_link_libraries(crtgfx_shared PUBLIC ${CRTGFX_SKIA_LIBRARIES})
endif()
if(CRT_TARGET_OS STREQUAL "windows")
  target_sources(crtgfx_shared PRIVATE $<TARGET_OBJECTS:crt_windows_dllcrt>)
  target_link_libraries(crtgfx_shared PRIVATE
    "${CRT_WINDOWS_KERNEL32_LIB}"
    "${CRTGFX_WINDOWS_USER32_LIB}"
    "${CRTGFX_WINDOWS_GDI32_LIB}"
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

if(CRTGFX_ENABLE_SKIA)
  # CRT_STARTUP_END_OBJECTS deliberately not listed here -- see
  # crt_link_startup_end_objects()'s own comment (top-level CMakeLists.txt).
  set(CRTGFX_SKIA_RASTER_SMOKE_EXTRA_SOURCES "")
  if(CRT_TARGET_OS STREQUAL "windows")
    # __acrt_iob_func/__stdio_common_vfprintf link stubs -- see that file's
    # own top comment (needed once -femulated-tls's own compiler-rt
    # implementation, emutls.c, actually gets linked in).
    list(APPEND CRTGFX_SKIA_RASTER_SMOKE_EXTRA_SOURCES
      "${CMAKE_SOURCE_DIR}/libc/src/arch/windows/common/emutls_link_stubs.c")
  endif()
  add_executable(crtgfx_skia_raster_smoke
    ${CRT_STARTUP_OBJECTS}
    tests/skia_raster_smoke.cc
    ${CRTGFX_SKIA_RASTER_SMOKE_EXTRA_SOURCES}
  )
  target_link_libraries(crtgfx_skia_raster_smoke PRIVATE crt_cxx_build_flags crtgfx)
  if(CRT_TARGET_OS STREQUAL "windows")
    # SK_BUILD_FOR_UNIX: see crtgfx_skia_objects's own, fuller comment just
    # above (same reasoning; this target also #includes Skia headers
    # directly via crtgfx/skia.h).
    target_compile_definitions(crtgfx_skia_raster_smoke PRIVATE SK_BUILD_FOR_UNIX)
  endif()
  if(CRT_TARGET_OS STREQUAL "windows" AND CRT_USE_IMPORTED_LIBCXX)
    # -isystem${CRT_SYSROOT}/include/c++/v1, added *before* the libc/include
    # one just below (both via target_compile_options, so both land in
    # <FLAGS> in call order) -- this project's own imported libc++, matching
    # what compiles skia_bridge.cc/Skia itself, not any host substitute. See
    # the CRTGFX_ENABLE_SKIA guard at the top of this file for the fuller
    # story of why this is required on Windows now.
    target_compile_options(crtgfx_skia_raster_smoke PRIVATE
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
  target_compile_options(crtgfx_skia_raster_smoke PRIVATE
    "-isystem${CMAKE_SOURCE_DIR}/libc/include"
  )
  target_include_directories(crtgfx_skia_raster_smoke SYSTEM BEFORE PRIVATE
    "${CRTGFX_SKIA_ROOT}"
  )
  if(CRT_TARGET_OS STREQUAL "windows" AND CRT_USE_IMPORTED_LIBCXX)
    # Real imported libc++/libc++abi/libunwind (Itanium ABI, matching Skia
    # itself), not the small bootstrap libstdc++/src/ ABI shim (`cxx`) --
    # see the CRTGFX_ENABLE_SKIA guard at the top of this file. Mirrors
    # tools/crt-c++'s own cxx_runtime_libs static-link selection.
    target_link_libraries(crtgfx_skia_raster_smoke PRIVATE
      "${CRT_SYSROOT}/lib/libc++.a"
      "${CRT_SYSROOT}/lib/libc++abi.a"
      "${CRT_SYSROOT}/lib/libunwind.a"
      c
    )
  else()
    target_link_libraries(crtgfx_skia_raster_smoke PRIVATE cxx c)
  endif()
  target_link_libraries(crtgfx_skia_raster_smoke PRIVATE crt_compiler_rt_builtins)
  target_link_options(crtgfx_skia_raster_smoke PRIVATE
    -nostdlib
    -nostartfiles
    -nodefaultlibs
  )
  if(CRT_TARGET_OS STREQUAL "macos")
    target_link_options(crtgfx_skia_raster_smoke PRIVATE
      -Wl,-e,_start
      -Wl,-dead_strip
    )
    target_link_libraries(crtgfx_skia_raster_smoke PRIVATE System)
  elseif(CRT_TARGET_OS STREQUAL "linux")
    target_link_options(crtgfx_skia_raster_smoke PRIVATE
      -Wl,-e,_start
      -Wl,--gc-sections
    )
  elseif(CRT_TARGET_OS STREQUAL "windows")
    target_link_options(crtgfx_skia_raster_smoke PRIVATE
      -Wl,/entry:mainCRTStartup
      -Wl,/subsystem:console
      -Wl,/OPT:REF
    )
    target_link_libraries(crtgfx_skia_raster_smoke PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
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
    target_link_libraries(crtgfx_skia_raster_smoke PRIVATE "${CRTGFX_WINDOWS_UUID_LIB}")
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
    target_link_options(crtgfx_skia_raster_smoke PRIVATE
      -Wl,--allow-multiple-definition
    )
  endif()
  crt_link_startup_end_objects(crtgfx_skia_raster_smoke)
  add_dependencies(crtgfx_skia_raster_smoke cxx c crt1 crtgfx)

  add_test(NAME crtgfx_skia_raster_smoke_runs COMMAND crtgfx_skia_raster_smoke)
  set_tests_properties(crtgfx_skia_raster_smoke_runs PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    PASS_REGULAR_EXPRESSION "crtgfx_skia_raster_smoke: ok"
  )
  set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtgfx_skia_raster_smoke_runs)
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
