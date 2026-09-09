cmake_minimum_required(VERSION 3.25)
project(crtgfx_skia_example CXX)

if(NOT CMAKE_SYSROOT)
  message(FATAL_ERROR "configure with the CRT distribution's crt-toolchain.cmake")
endif()

find_path(CRTGFX_INCLUDE_DIR crtgfx/skia.h
  PATHS "${CMAKE_SYSROOT}/include" NO_DEFAULT_PATH REQUIRED)
if(WIN32)
  find_library(CRTGFX_SKIA_LIBRARY NAMES crtgfx_skia_dll
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
else()
  find_library(CRTGFX_SKIA_LIBRARY NAMES crtgfx_skia
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
endif()

add_executable(crtgfx_skia_example main.cc)
target_include_directories(crtgfx_skia_example PRIVATE "${CRTGFX_INCLUDE_DIR}")
target_link_libraries(crtgfx_skia_example PRIVATE "${CRTGFX_SKIA_LIBRARY}")

