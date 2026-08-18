set(CRTGFX_COMMON_SOURCES
  src/window.c
  src/wayland_weston.c
)

set(CRTGFX_SKIA_VERSION "m148" CACHE STRING "Skia Chrome milestone shorthand to fetch, for example m148")
set(CRTGFX_SKIA_REF "" CACHE STRING "Explicit Skia git ref/commit to fetch; overrides CRTGFX_SKIA_VERSION when set")
set(CRTGFX_SKIA_REPOSITORY "https://skia.googlesource.com/skia.git" CACHE STRING "Skia git repository URL")
set(CRTGFX_SKIA_EXPECTED_COMMIT "" CACHE STRING "Optional full Skia commit hash expected after fetch")
option(CRTGFX_SKIA_SYNC_DEPS "Run Skia tools/git-sync-deps after fetching Skia" ON)
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

add_library(crtgfx_shared SHARED
  $<TARGET_OBJECTS:crtgfx_common_objects>
  $<TARGET_OBJECTS:crtgfx_backend_objects>
  ${CRTGFX_SKIA_OBJECTS}
)
target_include_directories(crtgfx_shared PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
if(CRTGFX_ENABLE_SKIA)
  target_include_directories(crtgfx_shared PUBLIC "${CRTGFX_SKIA_ROOT}")
endif()
target_link_libraries(crtgfx_shared PRIVATE crt_build_flags ${CRTGFX_CRT_SHARED_LIBS})
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

add_executable(crtgfx_window_smoke
  ${CRT_STARTUP_OBJECTS}
  tests/window_smoke_test.c
  ${CRT_STARTUP_END_OBJECTS}
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
add_dependencies(crtgfx_window_smoke c crt1)

add_test(NAME crtgfx_window_smoke_runs COMMAND crtgfx_window_smoke)
set_tests_properties(crtgfx_window_smoke_runs PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PASS_REGULAR_EXPRESSION "crtgfx_window_smoke: ok"
)
set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtgfx_window_smoke_runs)

if(CRTGFX_ENABLE_SKIA)
  add_executable(crtgfx_skia_raster_smoke
    ${CRT_STARTUP_OBJECTS}
    tests/skia_raster_smoke.cc
    ${CRT_STARTUP_END_OBJECTS}
  )
  target_link_libraries(crtgfx_skia_raster_smoke PRIVATE crt_cxx_build_flags crtgfx cxx c)
  target_include_directories(crtgfx_skia_raster_smoke SYSTEM BEFORE PRIVATE
    "${CMAKE_SOURCE_DIR}/libc/include"
    "${CRTGFX_SKIA_ROOT}"
  )
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
  endif()
  add_dependencies(crtgfx_skia_raster_smoke cxx c crt1 crtgfx)

  add_test(NAME crtgfx_skia_raster_smoke_runs COMMAND crtgfx_skia_raster_smoke)
  set_tests_properties(crtgfx_skia_raster_smoke_runs PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    PASS_REGULAR_EXPRESSION "crtgfx_skia_raster_smoke: ok"
  )
  set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtgfx_skia_raster_smoke_runs)
endif()

add_executable(crtgfx_window_demo
  ${CRT_STARTUP_OBJECTS}
  tools/window_demo.c
  ${CRT_STARTUP_END_OBJECTS}
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
add_dependencies(crtgfx_window_demo c crt1)
