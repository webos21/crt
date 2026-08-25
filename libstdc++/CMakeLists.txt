add_library(cxx STATIC
  src/cxxabi.c
  src/msvcabi.c
  src/new_delete.cc
)

target_link_libraries(cxx PRIVATE crt_cxx_build_flags c)
target_include_directories(cxx PRIVATE
  "${CMAKE_SOURCE_DIR}/libc/include"
)
set_target_properties(cxx PROPERTIES
  OUTPUT_NAME c++
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

add_library(cxx_shared SHARED
  src/cxxabi.c
  src/msvcabi.c
  src/new_delete.cc
)
if(CRT_TARGET_OS STREQUAL "windows")
  target_sources(cxx_shared PRIVATE $<TARGET_OBJECTS:crt_windows_dllcrt>)
endif()

target_link_libraries(cxx_shared PRIVATE crt_cxx_build_flags c_shared)
target_include_directories(cxx_shared PRIVATE
  "${CMAKE_SOURCE_DIR}/libc/include"
)
crt_configure_shared_runtime(cxx_shared)
set_target_properties(cxx_shared PROPERTIES
  OUTPUT_NAME c++
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)
if(CRT_TARGET_OS STREQUAL "windows")
  set_target_properties(cxx_shared PROPERTIES
    ARCHIVE_OUTPUT_NAME c++_dll
  )
endif()

# option() hoisted up from its previous spot further down this same file
# (where CRT_LIBCXX_RECIPE_ROOT/etc. are declared) so it exists before this
# install() call needs to read it -- see that if(NOT CRT_USE_IMPORTED_LIBCXX)
# guard's own comment for why.
option(CRT_USE_IMPORTED_LIBCXX "Use a successfully staged Android libc++ runtime for dependent external builds" OFF)
# Skipped entirely once a real imported libc++ is in play: the top-level
# `sysroot` custom target (CMakeLists.txt) has no declared OUTPUT/
# BYPRODUCTS, so ninja always considers it "out of date" and reruns its
# `cmake --install` step on *every* build invocation that reaches it
# (crt-libcxx-configure and every configure-system port's own port-build-*
# target both depend on it directly) -- not just the first time. Since
# this install(TARGETS ...) uses cxx/cxx_shared's own OUTPUT_NAME (`c++`),
# it writes to the exact same ${CRT_SYSROOT}/lib/libc++.a/.dylib path
# tools/install_libcxx_runtimes.py's own staging step (the crt-libcxx-
# sysroot target) writes the real, full imported libc++ to -- so leaving
# this unconditional meant *any* later build activity that happened to
# reach `sysroot` again (an ordinary `cmake --build --target crtgfx_skia_
# raster_smoke`/`port-build-freetype`/etc, not just an explicit `--target
# sysroot`) would silently overwrite an already-correctly-staged real
# libc++.a back to this small bootstrap ABI shim (operator new/delete/
# __cxa_* stubs only) -- confirmed for real, twice (2026-08-23 and
# 2026-08-25, on two different build directories): dozens of undefined
# std::__1::... symbols linking crtgfx_skia_raster_smoke/libcrtgfx.dylib
# against libskia.a, in a tree where crt-libcxx-sysroot had *already* run
# successfully and produced the real 9MB+ archive shortly before. A
# reconfigure/rebuild alone never fixed it, because the clobbering install
# step kept re-running on the very next build invocation that needed
# `sysroot` for any other reason. Guarding it here (skip staging the
# bootstrap's own libc++.a/.dylib into the sysroot at all, once imported
# libc++ is the active choice) is the real fix, not another one-off
# manual re-stage: whichever install runs last, real or bootstrap, no
# longer matters, because the bootstrap one is never in the running.
if(NOT CRT_USE_IMPORTED_LIBCXX)
  install(TARGETS cxx cxx_shared
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
  )
endif()

# Android keeps the full standard library outside Bionic's small libstdc++ ABI
# shim. Keep those sources outside this checkout too: each component's own
# provenance/build recipe is the project-owned, checked-in
# third_party/<name>/recipe.json right next to it (see tools/crt-libcxx-
# build.py's own docstring for the schema), while the selected source
# revision is fetched under the active build preset. The fetch target is
# deliberately separate from the bootstrap `cxx` target until libc++/
# libc++abi/libunwind can replace it as one coherent static-and-shared
# runtime set.
set(CRT_LIBCXX_RECIPE_ROOT "${CMAKE_SOURCE_DIR}/libstdc++/third_party" CACHE PATH "libcxx/libcxxabi/libunwind recipe.json root")
set(CRT_LIBCXX_FETCH_ROOT "${CMAKE_BINARY_DIR}/external/llvm-runtimes" CACHE PATH "Android LLVM runtime source root")
set(CRT_LIBCXX_BUILD_ROOT "${CRT_LIBCXX_FETCH_ROOT}/build" CACHE PATH "Android libc++ CMake build root")
set(CRT_LIBCXX_INSTALL_PREFIX "${CRT_LIBCXX_FETCH_ROOT}/install" CACHE PATH "Android libc++ staged install prefix")
# CRT_USE_IMPORTED_LIBCXX itself is declared earlier in this same file now
# (right before the install(TARGETS cxx cxx_shared ...) call that needs to
# read it) -- see that option()'s own comment for why.
set(CRT_LIBCXX_PLATFORM_ARGUMENTS "")
if(CRT_TARGET_OS STREQUAL "windows" AND CRT_WINDOWS_KERNEL32_LIB)
  get_filename_component(CRT_LIBCXX_WINDOWS_SDK_LIBPATH "${CRT_WINDOWS_KERNEL32_LIB}" DIRECTORY)
  list(APPEND CRT_LIBCXX_PLATFORM_ARGUMENTS
    --windows-sdk-libpath "${CRT_LIBCXX_WINDOWS_SDK_LIBPATH}")
endif()
if(CRT_TARGET_OS STREQUAL "windows")
  # A native Windows process cannot execute tools/crt-cc/tools/crt-c++'s
  # shebang lines directly, so CMake's compiler launcher is this
  # project's own mksh.exe with the real wrapper passed as ARG1 (see
  # tools/crt-libcxx-build.py's common_cmake_args()). That mksh.exe must
  # come from the *rootfs* (${CMAKE_BINARY_DIR}/rootfs), not the plainer
  # sysroot copy the C library install stages for other purposes: the
  # rootfs is the only tree with toybox's applet aliases actually
  # installed (system/bin/printf and friends), which tools/crt-cc itself
  # needs for argument handling -- confirmed for real: launching via the
  # sysroot's own mksh.exe copy fails every single compile with "printf:
  # inaccessible or not found", since that tree only carries mksh.exe
  # itself, no toybox alongside it. Computed inline as the same literal
  # path the "rootfs" custom target further down uses for its own output
  # (that target -- and the CRT_ROOTFS cache variable mirroring it -- is
  # not defined yet at this point in the top-level CMakeLists.txt, but
  # the plain path expression needs no forward reference); the actual
  # build-order dependency on the real "rootfs" target is wired later,
  # in the same if(TARGET crt-libcxx-configure) block that already wires
  # the sysroot dependency.
  list(APPEND CRT_LIBCXX_PLATFORM_ARGUMENTS
    --rootfs "${CMAKE_BINARY_DIR}/rootfs")
endif()
# --host-cc/--host-cxx: tools/crt-libcxx-build.py's own bootstrap (every
# recipe below is built through tools/crt-cc/tools/crt-c++, which fall
# back to a bare "clang"/"clang++" resolved via whatever the ambient PATH
# happens to symlink that name to, whenever CRT_HOST_CC/CRT_HOST_CXX are
# unset) previously had no way to know which compiler THIS project's own
# top-level configure was actually told to use -- confirmed for real
# (2026-08-22) that this is a genuine gap, not just a style preference:
# a host whose own default clang++ resolves to an older release than the
# CMAKE_CXX_COMPILER this configure explicitly selected (e.g. a stock
# Ubuntu 20.04's own apt-default clang++ 10, coexisting with a separately
# installed, explicitly-requested clang++-18) silently bootstrapped
# libcxx/libcxxabi with the *wrong*, too-old compiler, only surfacing as
# a confusing, much-later `#error "remove_reference not implemented!"`
# deep in libcxx's own <type_traits> once its source was current enough
# to require compiler builtins that old clang genuinely does not have.
# Passing this project's own already-resolved CMAKE_C_COMPILER/
# CMAKE_CXX_COMPILER through directly means the bootstrap always uses
# the exact same compiler the rest of the project was told to use, on
# every host, rather than re-deriving its own separate (and possibly
# different) guess.
list(APPEND CRT_LIBCXX_PLATFORM_ARGUMENTS
  --host-cc "${CMAKE_C_COMPILER}"
  --host-cxx "${CMAKE_CXX_COMPILER}")
if(Python3_Interpreter_FOUND)
  add_custom_target(crt-libcxx-fetch
    COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
      "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/crt-libcxx-build.py"
      --root "${CMAKE_SOURCE_DIR}"
      --recipe-root "${CRT_LIBCXX_RECIPE_ROOT}"
      --source-root "${CRT_LIBCXX_FETCH_ROOT}"
      --build-root "${CRT_LIBCXX_BUILD_ROOT}"
      --install-prefix "${CRT_LIBCXX_INSTALL_PREFIX}"
      --sysroot "${CRT_SYSROOT}"
      --target-os "${CRT_TARGET_OS}"
      --phase fetch
    COMMENT "Fetching libcxx/libcxxabi/libunwind recipe sources"
  )
  add_custom_target(crt-libcxx-configure
    COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
      "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/crt-libcxx-build.py"
      --root "${CMAKE_SOURCE_DIR}"
      --recipe-root "${CRT_LIBCXX_RECIPE_ROOT}"
      --source-root "${CRT_LIBCXX_FETCH_ROOT}"
      --build-root "${CRT_LIBCXX_BUILD_ROOT}"
      --install-prefix "${CRT_LIBCXX_INSTALL_PREFIX}"
      --sysroot "${CRT_SYSROOT}"
      --target-os "${CRT_TARGET_OS}"
      ${CRT_LIBCXX_PLATFORM_ARGUMENTS}
      --phase configure
    DEPENDS crt-libcxx-fetch
    COMMENT "Configuring libcxx/libcxxabi/libunwind recipes through the CRT toolchain"
    USES_TERMINAL
  )
  add_custom_target(crt-libcxx-build
    COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
      "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/crt-libcxx-build.py"
      --root "${CMAKE_SOURCE_DIR}"
      --recipe-root "${CRT_LIBCXX_RECIPE_ROOT}"
      --source-root "${CRT_LIBCXX_FETCH_ROOT}"
      --build-root "${CRT_LIBCXX_BUILD_ROOT}"
      --install-prefix "${CRT_LIBCXX_INSTALL_PREFIX}"
      --sysroot "${CRT_SYSROOT}"
      --target-os "${CRT_TARGET_OS}"
      ${CRT_LIBCXX_PLATFORM_ARGUMENTS}
      --phase build
    DEPENDS crt-libcxx-configure
    COMMENT "Building libcxx/libcxxabi/libunwind recipes through the CRT toolchain"
    USES_TERMINAL
  )
  add_custom_target(crt-libcxx-sysroot
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/install_libcxx_runtimes.py"
      --install-prefix "${CRT_LIBCXX_INSTALL_PREFIX}"
      --sysroot "${CRT_SYSROOT}"
    DEPENDS crt-libcxx-build
    COMMENT "Staging Android libc++ runtime into the CRT sysroot"
    USES_TERMINAL
  )
  set(CRT_LIBCXX_SMOKE_BINARY
    "${CMAKE_BINARY_DIR}/external/llvm-runtimes/smoke/imported_libcxx_test${CMAKE_EXECUTABLE_SUFFIX}")
  add_custom_target(crt-libcxx-smoke
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/test_libcxx_runtime.py"
      --root "${CMAKE_SOURCE_DIR}"
      --sysroot "${CRT_SYSROOT}"
      --target-os "${CRT_TARGET_OS}"
      --output "${CRT_LIBCXX_SMOKE_BINARY}"
      ${CRT_LIBCXX_PLATFORM_ARGUMENTS}
    DEPENDS crt-libcxx-sysroot
    COMMENT "Linking and running static/shared Android libc++ through the CRT sysroot"
    USES_TERMINAL
  )
endif()
