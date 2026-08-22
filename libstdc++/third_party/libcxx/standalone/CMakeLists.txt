# Thin driver for a standalone libcxx configure.
#
# AOSP toolchain/llvm-project's libcxx/CMakeLists.txt never calls project()
# itself -- it unconditionally assumes it is being add_subdirectory()'d from
# the upstream LLVM monorepo's runtimes/CMakeLists.txt driver, which this
# project's fetch (a sparse-checkout of just the libcxx/ subpath, see
# tools/crt-libcxx-build.py's own fetch_recipe()) never has. Confirmed by
# reading the fetched file directly: no `project(` call anywhere in it.
# This is the same situation ../libunwind/standalone/CMakeLists.txt's own
# comment already described for libunwind -- libcxx has since converged onto
# the identical monorepo-only structure (this project's previous source,
# platform/external/libcxx, still carried its own standalone-build project()
# bootstrap; the current source, toolchain/llvm-project's libcxx/, does not;
# see ../recipe.json's own notes for the fuller migration story).
#
# This file is that missing driver, kept intentionally minimal: call
# project() for real, then add_subdirectory() the actual fetched source
# (CRT_LIBCXX_SOURCE_DIR, passed by tools/crt-libcxx-build.py -- see its
# cmake.driver handling) as a normal subdirectory, giving libcxx's own
# CMakeLists.txt everything it expects from a real parent driver.
cmake_minimum_required(VERSION 3.20.0)
project(crt_libcxx_standalone LANGUAGES C CXX ASM)

if(NOT DEFINED CRT_LIBCXX_SOURCE_DIR)
  message(FATAL_ERROR "CRT_LIBCXX_SOURCE_DIR must be set to the fetched libcxx source directory")
endif()

# CRT_LIBC_SHARED_PARENT_DIR: libcxx/src/include/from_chars_floating_point.h
# does `#include "shared/fp_bits.h"` -- a real, unconditional dependency on
# llvm-project's own libc/shared/ directory (a set of header-only utilities
# deliberately meant to be reused by sibling runtime projects like libcxx,
# confirmed by reading the fetched from_chars_floating_point.h directly),
# which is no part of libcxx's own checkout at all. Confirmed for real
# (2026-08-22): `fatal error: 'shared/fp_bits.h' file not found` building
# charconv.cpp, the first libcxx source file that reaches this header.
# Upstream's own real "runtimes" combined build gets this include path for
# free (something in that larger project graph already adds it, since
# libcxx/src/CMakeLists.txt's own target_include_directories() calls never
# reference libc/shared at all); this standalone driver has no such thing,
# so it adds the same include path itself, directory-scoped so it reaches
# every target add_subdirectory(libcxx) below defines. ../recipe.json's own
# source.extra_checkout_dirs now fetches libc/shared as a sibling of
# libcxx/ under the shared source root (matching cmake/ and runtimes/cmake/
# 's own existing treatment), and CRT_LIBC_SHARED_PARENT_DIR (passed via
# -DCRT_LIBC_SHARED_PARENT_DIR=@SOURCE:libc@ in that same recipe.json) is
# that directory's own *parent* -- "shared/fp_bits.h" needs -I<...>/libc,
# not -I<...>/libc/shared, since the #include spells the "shared/" prefix
# itself.
if(DEFINED CRT_LIBC_SHARED_PARENT_DIR)
  include_directories("${CRT_LIBC_SHARED_PARENT_DIR}")
endif()

# Python3_EXECUTABLE: libcxx/include/CMakeLists.txt's own generate-cxx-headers
# custom command (which produces the installed libcxx.imp IWYU-mapping file)
# references "${Python3_EXECUTABLE}" directly but never calls
# find_package(Python3) itself -- confirmed by grepping libcxx/CMakeLists.txt
# and libcxx/include/CMakeLists.txt for both `find_package(Python3` and
# `Python3_EXECUTABLE`: zero hits in either. Upstream's real combined build
# gets this variable for free from runtimes/CMakeLists.txt's own
# `find_package(Python3 REQUIRED COMPONENTS Interpreter)` (confirmed by
# reading that file directly), which this standalone driver never
# add_subdirectory()s. Without it, Python3_EXECUTABLE silently expands to
# nothing, and the generated ninja COMMAND for that custom command truncates
# to just its leading `cd <dir>` with the actual
# `python3 utils/generate_iwyu_mapping.py ...` invocation missing entirely
# -- confirmed for real (2026-08-22) via `ninja -v` on the target directly:
# it "succeeds" (exit 0) without ever creating libcxx.imp, since there was no
# real command left to fail. Matches find_package(Python3)'s own call shape.
find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_subdirectory("${CRT_LIBCXX_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/libcxx")
