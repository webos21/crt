cmake_minimum_required(VERSION 3.25)
project(crtgfx_window_example C)

if(NOT CMAKE_SYSROOT)
  message(FATAL_ERROR "configure with the CRT distribution's crt-toolchain.cmake")
endif()

find_path(CRTGFX_INCLUDE_DIR crtgfx/window.h
  PATHS "${CMAKE_SYSROOT}/include" NO_DEFAULT_PATH REQUIRED)
if(WIN32)
  find_library(CRTGFX_WINDOW_LIBRARY NAMES crtgfx_dll
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
else()
  if(NOT APPLE)
    # Linux CRT executables embed the static runtime and hide its symbols
    # from real host DSOs.  Prefer the matching static window archive too;
    # the shared library otherwise pulls its libc++ dependency graph into
    # this C-only example and exposes the same host-DSO symbol-preemption
    # boundary already handled by the packaged gfx-gpu example.
    list(INSERT CMAKE_FIND_LIBRARY_SUFFIXES 0 "${CMAKE_STATIC_LIBRARY_SUFFIX}")
  endif()
  find_library(CRTGFX_WINDOW_LIBRARY NAMES crtgfx
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
endif()

add_executable(crtgfx_window_example main.c)
target_include_directories(crtgfx_window_example PRIVATE "${CRTGFX_INCLUDE_DIR}")
target_link_libraries(crtgfx_window_example PRIVATE "${CRTGFX_WINDOW_LIBRARY}")

if(NOT WIN32 AND NOT APPLE)
  # libcrtgfx.a records no transitive dependency metadata.  Reproduce the
  # installed simple-stage window target's link interface explicitly.  Its
  # only unresolved window dependency is the redistributed xkbcommon archive;
  # native Wayland protocol glue first appears in the later media/GPU stage.
  target_link_libraries(crtgfx_window_example PRIVATE
    "${CMAKE_SYSROOT}/lib/libxkbcommon.a")
  target_link_options(crtgfx_window_example PRIVATE -Wl,--exclude-libs,ALL)
endif()
