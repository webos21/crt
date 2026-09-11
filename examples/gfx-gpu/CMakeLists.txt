cmake_minimum_required(VERSION 3.25)
project(crtgfx_gpu_example C)

if(NOT CMAKE_SYSROOT)
  message(FATAL_ERROR "configure with the CRT distribution's crt-toolchain.cmake")
endif()

find_path(CRTGFX_INCLUDE_DIR crtgfx/gpu.h
  PATHS "${CMAKE_SYSROOT}/include" NO_DEFAULT_PATH REQUIRED)
if(WIN32)
  find_library(CRTGFX_GPU_LIBRARY NAMES crtgfx_gpu_dll
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
  find_library(CRTGFX_WINDOW_LIBRARY NAMES crtgfx_dll
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
else()
  find_library(CRTGFX_GPU_LIBRARY NAMES crtgfx_gpu
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
  find_library(CRTGFX_WINDOW_LIBRARY NAMES crtgfx
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
endif()

add_executable(crtgfx_gpu_example main.c)
target_include_directories(crtgfx_gpu_example PRIVATE "${CRTGFX_INCLUDE_DIR}")
# CRTGFX_WINDOW_LIBRARY (crtgfx_window_create/_destroy/_poll_event/...) is
# required too, not just CRTGFX_GPU_LIBRARY -- a real, confirmed bug
# (2026-09-11), first hit building this example against a real packaged
# SDK for the first time (the isolated 03-gfx-simple -> 04-gfx-media
# stage upgrade's own build_example() step). find_library()'s NAMES
# crtgfx_gpu here resolves to the *shared* libcrtgfx_gpu.dylib/.so
# (matched ahead of the static libcrtgfx_gpu.a by CMAKE_FIND_LIBRARY_
# SUFFIXES' default ordering), which itself depends on libcrtgfx.dylib/
# .so at ITS OWN link time (crtgfx_gpu_shared links crtgfx_window_shared
# PUBLIC) -- but that is an ordinary load-time dependency, not a
# re-export, so ld64/ld.lld's two-level-namespace symbol resolution does
# not search into it when resolving *this* executable's own undefined
# crtgfx_window_* symbols: only libraries listed directly on this link's
# own command line count. Confirmed directly: "Undefined symbols ...
# _crtgfx_window_create/_crtgfx_window_destroy/_crtgfx_window_poll_
# event/..." building crtgfx_gpu_example this way for the first time.
target_link_libraries(crtgfx_gpu_example PRIVATE
  "${CRTGFX_GPU_LIBRARY}" "${CRTGFX_WINDOW_LIBRARY}")

