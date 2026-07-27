cmake_minimum_required(VERSION 3.25)

project(crt LANGUAGES C ASM)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CRT_TARGET_OS "macos" CACHE STRING "Target OS personality")
set(CRT_TARGET_ARCH "${CMAKE_SYSTEM_PROCESSOR}" CACHE STRING "Target architecture")
set(CRT_SYSROOT "${CMAKE_BINARY_DIR}/sysroot" CACHE PATH "Project sysroot")

option(CRT_ENABLE_CXX_RUNTIME "Prepare C++ runtime integration points" ON)
option(CRT_USE_COMPILER_RT "Link compiler-rt builtins when the compiler exposes them" ON)

include(GNUInstallDirs)
enable_testing()

add_library(crt_build_flags INTERFACE)
target_compile_options(crt_build_flags INTERFACE
  -ffreestanding
  -fno-builtin
  -Wall
  -Wextra
  -Werror
)
target_include_directories(crt_build_flags INTERFACE
  "${CMAKE_SOURCE_DIR}/include"
)

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

add_subdirectory(libc)
add_subdirectory(tests)

install(DIRECTORY include/ DESTINATION include)

add_custom_target(sysroot
  COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}" --prefix "${CRT_SYSROOT}"
  DEPENDS c crt1
  COMMENT "Installing CRT headers, libraries, and startup objects into ${CRT_SYSROOT}"
)
