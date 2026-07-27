cmake_minimum_required(VERSION 3.25)

set(CRT_TARGET_OS "macos" CACHE STRING "Target OS personality")
set(CRT_TARGET_ARCH "host" CACHE STRING "Target architecture")
set(CRT_SYSROOT "${CMAKE_BINARY_DIR}/sysroot" CACHE PATH "Project sysroot")

if(CRT_TARGET_OS STREQUAL "windows")
  set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "" CACHE STRING "MSVC runtime library selection" FORCE)
  set(CMAKE_C_STANDARD_LIBRARIES "" CACHE STRING "C standard libraries" FORCE)
endif()

project(crt LANGUAGES C ASM)

if(CRT_TARGET_OS STREQUAL "windows")
  set(CMAKE_C_STANDARD_LIBRARIES "")
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(CRT_ENABLE_CXX_RUNTIME "Prepare C++ runtime integration points" ON)
option(CRT_USE_COMPILER_RT "Link compiler-rt builtins when the compiler exposes them" ON)

include(GNUInstallDirs)
enable_testing()

add_library(crt_build_flags INTERFACE)
target_compile_options(crt_build_flags INTERFACE
  -ffreestanding
  -fno-builtin
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
add_subdirectory(tests)

install(DIRECTORY include/ DESTINATION include)

add_custom_target(sysroot
  COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}" --prefix "${CRT_SYSROOT}"
  DEPENDS c crt1
  COMMENT "Installing CRT headers, libraries, and startup objects into ${CRT_SYSROOT}"
)
