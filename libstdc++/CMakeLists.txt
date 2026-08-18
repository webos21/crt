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

install(TARGETS cxx cxx_shared
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

# Android keeps the full standard library outside Bionic's small libstdc++ ABI
# shim. Keep those sources outside this checkout too: the metadata below is
# project-owned, while the selected Android revision is fetched under the active
# build preset. The fetch target is deliberately separate from the bootstrap
# `cxx` target until libc++/libc++abi/libunwind can replace it as one coherent
# static-and-shared runtime set.
set(CRT_LIBCXX_ANDROID_REF "refs/heads/main" CACHE STRING "Android ref shared by libc++, libc++abi, and libunwind")
set(CRT_LIBCXX_FETCH_ROOT "${CMAKE_BINARY_DIR}/external/llvm-runtimes" CACHE PATH "Android LLVM runtime source root")
set(CRT_LIBCXX_BUILD_ROOT "${CRT_LIBCXX_FETCH_ROOT}/build" CACHE PATH "Android libc++ CMake build root")
set(CRT_LIBCXX_INSTALL_PREFIX "${CRT_LIBCXX_FETCH_ROOT}/install" CACHE PATH "Android libc++ staged install prefix")
option(CRT_USE_IMPORTED_LIBCXX "Use a successfully staged Android libc++ runtime for dependent external builds" OFF)
if(Python3_Interpreter_FOUND)
  add_custom_target(crt-libcxx-fetch
    COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
      "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/fetch_libcxx_runtimes.py"
      --root "${CRT_LIBCXX_FETCH_ROOT}"
      --ref "${CRT_LIBCXX_ANDROID_REF}"
    COMMENT "Fetching Android libc++, libc++abi, and libunwind sources"
  )
  add_custom_target(crt-libcxx-configure
    COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
      "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/build_libcxx_runtimes.py"
      --root "${CMAKE_SOURCE_DIR}"
      --source-root "${CRT_LIBCXX_FETCH_ROOT}"
      --build-root "${CRT_LIBCXX_BUILD_ROOT}"
      --install-prefix "${CRT_LIBCXX_INSTALL_PREFIX}"
      --sysroot "${CRT_SYSROOT}"
      --target-os "${CRT_TARGET_OS}"
      --phase configure
    DEPENDS crt-libcxx-fetch
    COMMENT "Configuring Android libc++ and libc++abi through the CRT toolchain"
    USES_TERMINAL
  )
  add_custom_target(crt-libcxx-build
    COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
      "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/build_libcxx_runtimes.py"
      --root "${CMAKE_SOURCE_DIR}"
      --source-root "${CRT_LIBCXX_FETCH_ROOT}"
      --build-root "${CRT_LIBCXX_BUILD_ROOT}"
      --install-prefix "${CRT_LIBCXX_INSTALL_PREFIX}"
      --sysroot "${CRT_SYSROOT}"
      --target-os "${CRT_TARGET_OS}"
      --phase build
    DEPENDS crt-libcxx-configure
    COMMENT "Building Android libc++ and libc++abi through the CRT toolchain"
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
endif()
