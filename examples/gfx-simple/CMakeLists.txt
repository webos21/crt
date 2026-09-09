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
  find_library(CRTGFX_WINDOW_LIBRARY NAMES crtgfx
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
endif()

add_executable(crtgfx_window_example main.c)
target_include_directories(crtgfx_window_example PRIVATE "${CRTGFX_INCLUDE_DIR}")
target_link_libraries(crtgfx_window_example PRIVATE "${CRTGFX_WINDOW_LIBRARY}")

