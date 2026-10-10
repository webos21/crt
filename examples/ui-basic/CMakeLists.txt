cmake_minimum_required(VERSION 3.25)
project(crtui_basic_example C)

if(NOT CMAKE_SYSROOT)
  message(FATAL_ERROR "configure with the CRT distribution's crt-toolchain.cmake")
endif()

# The whole point of this example: an application uses crtui and crtgfx only.
# It includes no LVGL header and links no LVGL library, because LVGL is a private
# implementation detail inside libcrtui (docs/acceptance/crtui_acceptance.md, Tranche 3).
find_path(CRTGFX_INCLUDE_DIR crtgfx/window.h
  PATHS "${CMAKE_SYSROOT}/include" NO_DEFAULT_PATH REQUIRED)
find_path(CRTUI_INCLUDE_DIR crtui/ui.h
  PATHS "${CMAKE_SYSROOT}/include" NO_DEFAULT_PATH REQUIRED)

if(WIN32)
  find_library(CRTGFX_WINDOW_LIBRARY NAMES crtgfx_dll
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
  find_library(CRTUI_LIBRARY NAMES crtui_dll
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
else()
  if(NOT APPLE)
    # Prefer the static archives (same reason as examples/gfx-simple).
    list(INSERT CMAKE_FIND_LIBRARY_SUFFIXES 0 "${CMAKE_STATIC_LIBRARY_SUFFIX}")
  endif()
  find_library(CRTGFX_WINDOW_LIBRARY NAMES crtgfx
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
  find_library(CRTUI_LIBRARY NAMES crtui
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
endif()

add_executable(crtui_basic_example main.c)
target_include_directories(crtui_basic_example PRIVATE "${CRTUI_INCLUDE_DIR}" "${CRTGFX_INCLUDE_DIR}")
target_link_libraries(crtui_basic_example PRIVATE "${CRTUI_LIBRARY}" "${CRTGFX_WINDOW_LIBRARY}")

if(NOT WIN32 AND NOT APPLE)
  # libcrtgfx.a records no transitive dependency metadata: reproduce its window
  # link interface (see examples/gfx-simple/CMakeLists.txt for the full story).
  # libcrtui.a needs nothing extra: LVGL is inside it. Unverified on Linux until
  # the Tranche 3 replay -- the list is copied from the gfx-simple example.
  target_link_libraries(crtui_basic_example PRIVATE
    "${CMAKE_SYSROOT}/lib/libxkbcommon.a")
  find_library(CRTUI_EXAMPLE_WAYLAND_CLIENT_LIB NAMES wayland-client wayland-client.so.0
    NO_CMAKE_FIND_ROOT_PATH)
  if(NOT CRTUI_EXAMPLE_WAYLAND_CLIENT_LIB)
    find_program(CRT_DPKG_ARCHITECTURE_EXECUTABLE dpkg-architecture)
    if(CRT_DPKG_ARCHITECTURE_EXECUTABLE)
      execute_process(
        COMMAND "${CRT_DPKG_ARCHITECTURE_EXECUTABLE}" -qDEB_HOST_MULTIARCH
        OUTPUT_VARIABLE CRT_HOST_MULTIARCH_TRIPLET
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    endif()
    if(CRT_HOST_MULTIARCH_TRIPLET)
      unset(CRTUI_EXAMPLE_WAYLAND_CLIENT_LIB CACHE)
      find_file(CRTUI_EXAMPLE_WAYLAND_CLIENT_LIB NAMES libwayland-client.so.0
        PATHS /lib /usr/lib /usr/local/lib PATH_SUFFIXES "${CRT_HOST_MULTIARCH_TRIPLET}")
    endif()
  endif()
  if(NOT CRTUI_EXAMPLE_WAYLAND_CLIENT_LIB)
    message(FATAL_ERROR "Could not find a real host libwayland-client.so/.so.0")
  endif()
  get_filename_component(CRT_EXAMPLE_WAYLAND_CLIENT_LIB_DIR "${CRTUI_EXAMPLE_WAYLAND_CLIENT_LIB}" DIRECTORY)
  target_link_options(crtui_basic_example PRIVATE
    "-Wl,-rpath-link,${CRT_EXAMPLE_WAYLAND_CLIENT_LIB_DIR}")
  target_link_libraries(crtui_basic_example PRIVATE
    "${CMAKE_SYSROOT}/lib/libxdg-shell-protocol.a"
    "${CRTUI_EXAMPLE_WAYLAND_CLIENT_LIB}")
  target_link_options(crtui_basic_example PRIVATE -Wl,--exclude-libs,ALL)
endif()
