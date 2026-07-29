cmake_minimum_required(VERSION 3.25)

set(CRT_TARGET_OS "macos" CACHE STRING "Target OS personality")
set(CRT_TARGET_ARCH "host" CACHE STRING "Target architecture")
set(CRT_SYSROOT "${CMAKE_BINARY_DIR}/sysroot" CACHE PATH "Project sysroot")

if(CRT_TARGET_OS STREQUAL "windows")
  set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "" CACHE STRING "MSVC runtime library selection" FORCE)
  set(CMAKE_C_STANDARD_LIBRARIES "" CACHE STRING "C standard libraries" FORCE)
  set(CMAKE_CXX_STANDARD_LIBRARIES "" CACHE STRING "C++ standard libraries" FORCE)
endif()

project(crt LANGUAGES C CXX ASM)

if(CRT_TARGET_ARCH STREQUAL "host")
  if(CRT_TARGET_OS STREQUAL "macos" AND NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    message(FATAL_ERROR "CRT_TARGET_OS=macos can only be used on a macOS host in the current host-build profile. Reconfigure with --preset linux-host-ninja-debug on Linux, or clear the stale build cache.")
  elseif(CRT_TARGET_OS STREQUAL "linux" AND NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "CRT_TARGET_OS=linux can only be used on a Linux host in the current host-build profile. Reconfigure with the matching host preset, or clear the stale build cache.")
  elseif(CRT_TARGET_OS STREQUAL "windows" AND NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    message(FATAL_ERROR "CRT_TARGET_OS=windows can only be used on a Windows host in the current host-build profile. Cross-build support is intentionally deferred.")
  endif()
endif()

if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR "CRT requires Clang as the C compiler because the build uses Clang-specific freestanding ABI flags. Reconfigure with a preset that sets CMAKE_C_COMPILER=clang, or clear the stale build cache.")
endif()

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR "CRT requires Clang++ as the C++ compiler because the build uses Clang-specific freestanding ABI flags. Reconfigure with a preset that sets CMAKE_CXX_COMPILER=clang++, or clear the stale build cache.")
endif()

if(CRT_TARGET_OS STREQUAL "windows")
  set(CMAKE_C_STANDARD_LIBRARIES "")
  set(CMAKE_CXX_STANDARD_LIBRARIES "")
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

message(STATUS "CRT long double ABI follows the compiler target default; no -mlong-double override is applied")

option(CRT_ENABLE_CXX_RUNTIME "Prepare C++ runtime integration points" ON)
option(CRT_USE_COMPILER_RT "Link compiler-rt builtins when the compiler exposes them" ON)

include(GNUInstallDirs)
find_package(Python3 COMPONENTS Interpreter QUIET)
enable_testing()

if(Python3_Interpreter_FOUND)
  add_custom_target(check-import-manifest
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/check_import_manifest.py"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Checking Bionic import manifest"
  )
endif()

add_library(crt_build_flags INTERFACE)
target_compile_options(crt_build_flags INTERFACE
  -ffreestanding
  -fno-builtin
  -Xclang
  -fwchar-type=int
  -ffunction-sections
  -fdata-sections
  -Wall
  -Wextra
  -Werror
)
target_include_directories(crt_build_flags INTERFACE
  "${CMAKE_SOURCE_DIR}/include"
)
if(CRT_TARGET_OS STREQUAL "macos")
  target_compile_definitions(crt_build_flags INTERFACE CRT_TARGET_OS_MACOS=1)
elseif(CRT_TARGET_OS STREQUAL "linux")
  target_compile_definitions(crt_build_flags INTERFACE CRT_TARGET_OS_LINUX=1)
elseif(CRT_TARGET_OS STREQUAL "windows")
  target_compile_definitions(crt_build_flags INTERFACE CRT_TARGET_OS_WINDOWS=1)
endif()

add_library(crt_cxx_build_flags INTERFACE)
target_link_libraries(crt_cxx_build_flags INTERFACE crt_build_flags)
target_compile_options(crt_cxx_build_flags INTERFACE
  $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
)
if(CRT_TARGET_OS STREQUAL "windows")
  target_compile_options(crt_cxx_build_flags INTERFACE
    $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>
  )
endif()

function(crt_configure_shared_runtime target_name)
  target_link_options(${target_name} PRIVATE
    -nostdlib
    -nodefaultlibs
  )
  if(CRT_TARGET_OS STREQUAL "macos")
    target_link_libraries(${target_name} PRIVATE System)
  elseif(CRT_TARGET_OS STREQUAL "windows")
    target_link_options(${target_name} PRIVATE
      -fuse-ld=lld
      -Wl,/OPT:REF
    )
    target_link_libraries(${target_name} PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
  elseif(CRT_TARGET_OS STREQUAL "linux")
    target_link_options(${target_name} PRIVATE
      -Wl,--gc-sections
    )
  endif()
endfunction()

if(CRT_TARGET_ARCH STREQUAL "host")
  set(CRT_EFFECTIVE_ARCH_FOR_FLAGS "${CMAKE_SYSTEM_PROCESSOR}")
else()
  set(CRT_EFFECTIVE_ARCH_FOR_FLAGS "${CRT_TARGET_ARCH}")
endif()

if(CRT_TARGET_OS STREQUAL "linux" AND CRT_EFFECTIVE_ARCH_FOR_FLAGS MATCHES "^(arm64|ARM64|aarch64)$")
  target_compile_options(crt_build_flags INTERFACE
    $<$<COMPILE_LANGUAGE:C>:-mno-outline-atomics>
  )
endif()

if(CRT_USE_COMPILER_RT)
  execute_process(
    COMMAND "${CMAKE_C_COMPILER}" --print-libgcc-file-name
    OUTPUT_VARIABLE CRT_COMPILER_RT_BUILTINS
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  if(CRT_COMPILER_RT_BUILTINS AND EXISTS "${CRT_COMPILER_RT_BUILTINS}")
    message(STATUS "compiler-rt builtins: ${CRT_COMPILER_RT_BUILTINS}")
    add_library(crt_compiler_rt_builtins INTERFACE)
    target_link_libraries(crt_compiler_rt_builtins INTERFACE "${CRT_COMPILER_RT_BUILTINS}")
    install(FILES "${CRT_COMPILER_RT_BUILTINS}" DESTINATION lib RENAME libclang_rt.builtins.a)
  else()
    message(STATUS "compiler-rt builtins not found by ${CMAKE_C_COMPILER}")
    add_library(crt_compiler_rt_builtins INTERFACE)
  endif()
else()
  add_library(crt_compiler_rt_builtins INTERFACE)
endif()

if(CRT_TARGET_OS STREQUAL "windows")
  if(CRT_TARGET_ARCH STREQUAL "host")
    set(CRT_WINDOWS_SDK_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
  else()
    set(CRT_WINDOWS_SDK_ARCH "${CRT_TARGET_ARCH}")
  endif()

  if(CRT_WINDOWS_SDK_ARCH MATCHES "^(x86_64|AMD64|x64)$")
    set(CRT_WINDOWS_SDK_LIB_ARCH "x64")
  elseif(CRT_WINDOWS_SDK_ARCH MATCHES "^(arm64|ARM64|aarch64)$")
    set(CRT_WINDOWS_SDK_LIB_ARCH "arm64")
  else()
    set(CRT_WINDOWS_SDK_LIB_ARCH "${CRT_WINDOWS_SDK_ARCH}")
  endif()

  set(CRT_WINDOWS_SDK_LIB_CANDIDATES)
  foreach(CRT_WINDOWS_KITS_ROOT
      "$ENV{WindowsSdkDir}"
      "$ENV{ProgramFiles\(x86\)}/Windows Kits/10"
      "$ENV{ProgramFiles}/Windows Kits/10")
    if(CRT_WINDOWS_KITS_ROOT)
      file(GLOB CRT_WINDOWS_SDK_UM_DIRS
        LIST_DIRECTORIES true
        "${CRT_WINDOWS_KITS_ROOT}/Lib/*/um/${CRT_WINDOWS_SDK_LIB_ARCH}")
      list(APPEND CRT_WINDOWS_SDK_LIB_CANDIDATES ${CRT_WINDOWS_SDK_UM_DIRS})
    endif()
  endforeach()
  list(REMOVE_DUPLICATES CRT_WINDOWS_SDK_LIB_CANDIDATES)
  list(SORT CRT_WINDOWS_SDK_LIB_CANDIDATES COMPARE NATURAL ORDER DESCENDING)

  find_file(CRT_WINDOWS_KERNEL32_LIB
    NAMES kernel32.lib
    PATHS ${CRT_WINDOWS_SDK_LIB_CANDIDATES}
    NO_DEFAULT_PATH
  )
  find_file(CRT_WINDOWS_SYNCHRONIZATION_LIB
    NAMES synchronization.lib
    PATHS ${CRT_WINDOWS_SDK_LIB_CANDIDATES}
    NO_DEFAULT_PATH
  )
  if(CRT_WINDOWS_KERNEL32_LIB)
    message(STATUS "Windows kernel32 import library: ${CRT_WINDOWS_KERNEL32_LIB}")
  else()
    message(WARNING "kernel32.lib was not found. Use a Visual Studio Developer shell or set CRT_WINDOWS_KERNEL32_LIB.")
    set(CRT_WINDOWS_KERNEL32_LIB kernel32.lib CACHE FILEPATH "Windows kernel32 import library")
  endif()

  if(CRT_WINDOWS_SYNCHRONIZATION_LIB)
    message(STATUS "Windows synchronization import library: ${CRT_WINDOWS_SYNCHRONIZATION_LIB}")
  else()
    message(WARNING "synchronization.lib was not found. Use a Visual Studio Developer shell or set CRT_WINDOWS_SYNCHRONIZATION_LIB.")
    set(CRT_WINDOWS_SYNCHRONIZATION_LIB synchronization.lib CACHE FILEPATH "Windows synchronization import library")
  endif()

  set(CRT_WINDOWS_SYSTEM_LIBS
    "${CRT_WINDOWS_KERNEL32_LIB}"
    "${CRT_WINDOWS_SYNCHRONIZATION_LIB}"
  )
endif()

add_subdirectory(libc)
add_subdirectory(libm)
add_subdirectory(libdl)
add_subdirectory(libstdc++)
add_subdirectory(tests)

install(DIRECTORY include/ DESTINATION include)

add_custom_target(sysroot
  COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}" --prefix "${CRT_SYSROOT}"
  DEPENDS c c_shared m m_shared dl dl_shared cxx cxx_shared crt1
  COMMENT "Installing CRT headers, libraries, and startup objects into ${CRT_SYSROOT}"
)
