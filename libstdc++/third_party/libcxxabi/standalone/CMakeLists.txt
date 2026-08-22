# Thin driver for a standalone libcxxabi configure.
#
# AOSP toolchain/llvm-project's libcxxabi/CMakeLists.txt never calls
# project() itself -- it unconditionally assumes it is being
# add_subdirectory()'d from the upstream LLVM monorepo's runtimes/
# CMakeLists.txt driver, which this project's fetch (a sparse-checkout of
# just the libcxxabi/ subpath, see tools/crt-libcxx-build.py's own
# fetch_recipe()) never has. Confirmed by reading the fetched file directly:
# no `project(` call anywhere in it. Same situation as ../libcxx/standalone/
# CMakeLists.txt and ../libunwind/standalone/CMakeLists.txt -- see either
# one's own comment, and ../recipe.json's own notes, for the fuller story.
#
# Unlike libunwind's own driver, this one ALSO add_subdirectory()s libcxx
# (CRT_LIBCXX_SOURCE_DIR) alongside libcxxabi, even though this recipe only
# ever builds/installs libcxxabi's own targets. Confirmed for real
# (2026-08-22) that this is genuinely required, not just convenient: the
# fetched libcxxabi/src/CMakeLists.txt does
# `target_link_libraries(cxxabi_shared_objects PRIVATE cxx-headers ...)`
# (and the same for cxxabi_static_objects) -- "cxx-headers" is an INTERFACE
# target libcxx/CMakeLists.txt itself defines, exposing libcxx's own
# CMake-generated public headers (__config_site, generated via
# configure_file() from __config_site.in at CONFIGURE time -- confirmed
# there is no plain, pre-generated __config_site committed anywhere in the
# fetched source, only the .in template, so this cannot be worked around
# with a raw -I flag at a fixed path the way libunwind's own extra_checkout_
# dirs problem could be). Without libcxx also add_subdirectory()'d into this
# same CMake project graph, "cxx-headers" does not exist as a real target at
# all, and CMake silently treats the unresolved name as a literal linker
# flag rather than failing at configure time -- surfacing much later and
# far more confusingly, as a plain `fatal error: 'stdexcept' file not
# found` compiling cxa_handlers.cpp/stdlib_stdexcept.cpp/
# cxa_exception_storage.cpp (all of which reach std::logic_error/
# std::runtime_error/std::bad_alloc's own libcxx header declarations).
#
# This does NOT create the reverse circular problem (libcxx's own
# HandleLibCXXABI.cmake needing libcxxabi's targets) for this recipe's own
# build: LIBCXX_CXX_ABI is "system-libcxxabi" here (matching ../libcxx/
# recipe.json's own value -- see that file's own notes for why), which
# takes an entirely different, path-based branch in libcxx's HandleLibCXXABI.
# cmake that never references an in-tree cxxabi_shared/cxxabi_static target
# at all, so add_subdirectory()-ing libcxx here never needs libcxxabi's own
# not-yet-built library to already exist. Only cxxabi's own targets
# (cxxabi_shared_objects/cxxabi_static_objects/cxxabi_shared/cxxabi_static)
# are ever requested via `cmake --build --target cxxabi` (tools/crt-libcxx-
# build.py's own per-recipe build_targets selection) -- libcxx's own
# heavier targets (cxx, cxx_shared, ...) are defined here but never built
# from this configure, so nothing here ever needs libcxxabi's own
# not-yet-existing install output.
cmake_minimum_required(VERSION 3.20.0)
project(crt_libcxxabi_standalone LANGUAGES C CXX ASM)

if(NOT DEFINED CRT_LIBCXXABI_SOURCE_DIR)
  message(FATAL_ERROR "CRT_LIBCXXABI_SOURCE_DIR must be set to the fetched libcxxabi source directory")
endif()
if(NOT DEFINED CRT_LIBCXX_SOURCE_DIR)
  message(FATAL_ERROR "CRT_LIBCXX_SOURCE_DIR must be set to the fetched libcxx source directory (needed for its cxx-headers target -- see this file's own comment)")
endif()

# Python3_EXECUTABLE: same gap and same fix as ../../libcxx/standalone/
# CMakeLists.txt's own matching comment -- this driver ALSO
# add_subdirectory()s libcxx (see above), whose include/CMakeLists.txt
# references "${Python3_EXECUTABLE}" in the generate-cxx-headers custom
# command (produces libcxx.imp) without ever calling find_package(Python3)
# itself. Without this, the command silently no-ops on Linux (a bare `cd
# <dir>` with nothing after it, confirmed via `ninja -v`; POSIX happily
# "succeeds" running just `cd`) but fails hard on Windows: confirmed for
# real (2026-08-22) via `cmake --build --target cxxabi` (this recipe's own
# only build_targets entry, which transitively needs cxx-headers ->
# generate-cxx-headers since cxxabi_shared_objects/cxxabi_static_objects
# both target_link_libraries() cxx-headers) -- `CreateProcess failed: The
# system cannot find the file specified` building this exact same
# include/c++/v1/libcxx.imp output, this time under
# build/libcxxabi/libcxx/include rather than build/libcxx/libcxx/include.
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Order matters here, unlike a plain target_link_libraries() forward
# reference: libcxx/CMakeLists.txt's own `include(HandleLibCXXABI)` does a
# real, immediately-evaluated `if (TARGET cxxabi_shared)` (not a generator
# expression deferred to generate time) to decide how to wire up its own
# libcxx-abi-shared/-static targets -- confirmed for real (2026-08-22):
# add_subdirectory()-ing libcxx before libcxxabi left `cxxabi_shared` not
# yet defined at the point HandleLibCXXABI.cmake's own check ran, which
# surfaced later as `CMake Error ... Target "libcxx-abi-shared" not found`
# at libcxx/src/CMakeLists.txt's own add_custom_command generator
# expression. libcxxabi first, libcxx second, fixes it.
add_subdirectory("${CRT_LIBCXXABI_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/libcxxabi")
add_subdirectory("${CRT_LIBCXX_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/libcxx")
