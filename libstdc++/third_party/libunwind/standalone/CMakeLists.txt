# Thin driver for a standalone libunwind configure.
#
# AOSP toolchain/llvm-project's libunwind/CMakeLists.txt (unlike the
# sibling libcxx/libcxxabi checkouts, which still carry their own
# standalone-build project() bootstrap) never calls project() itself --
# it unconditionally assumes it is being add_subdirectory()'d from the
# upstream LLVM monorepo's runtimes/CMakeLists.txt driver, which this
# project's fetch (a sparse-checkout of just the libunwind/ subpath, see
# tools/crt-libcxx-build.py's own fetch_recipe()) never has. Configuring
# libunwind's own CMakeLists.txt directly as -S therefore fails before
# even reaching its own option()s (confirmed directly: CMake silently
# injects a placeholder project() with no working C-compiler-detection
# pass behind it, corrupting CMakeCCompiler.cmake's own escaping of a
# Windows compiler path).
#
# This file is that missing driver, kept intentionally minimal: call
# project() for real, then add_subdirectory() the actual fetched source
# (CRT_LIBUNWIND_SOURCE_DIR, passed by tools/crt-libcxx-build.py -- see
# its cmake.driver handling) as a normal subdirectory, giving libunwind's
# own CMakeLists.txt everything it expects from a real parent driver.
cmake_minimum_required(VERSION 3.20.0)
project(crt_libunwind_standalone LANGUAGES C CXX ASM)

if(NOT DEFINED CRT_LIBUNWIND_SOURCE_DIR)
  message(FATAL_ERROR "CRT_LIBUNWIND_SOURCE_DIR must be set to the fetched libunwind source directory")
endif()

add_subdirectory("${CRT_LIBUNWIND_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/libunwind")
