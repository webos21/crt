cmake_minimum_required(VERSION 3.25)
project(crt_stage_03_gfx_simple C)

if(NOT DEFINED CRT_STAGE_SOURCE_ROOT OR NOT DEFINED CRT_STAGE_TARGET_OS)
  message(FATAL_ERROR "CRT_STAGE_SOURCE_ROOT and CRT_STAGE_TARGET_OS are required")
endif()
if(NOT CMAKE_SYSROOT)
  message(FATAL_ERROR "configure with the predecessor SDK's crt-toolchain.cmake")
endif()

set(CRTGFX_ROOT "${CRT_STAGE_SOURCE_ROOT}/libcrtgfx")
include("${CRTGFX_ROOT}/cmake/crtgfx_window_sources.cmake")

set(window_sources ${CRTGFX_WINDOW_COMMON_SOURCES})
if(CRT_STAGE_TARGET_OS STREQUAL "windows")
  list(APPEND window_sources ${CRTGFX_WINDOW_BACKEND_WINDOWS_SOURCES})
elseif(CRT_STAGE_TARGET_OS STREQUAL "macos")
  list(APPEND window_sources ${CRTGFX_WINDOW_BACKEND_MACOS_SOURCES})
elseif(CRT_STAGE_TARGET_OS STREQUAL "linux")
  list(APPEND window_sources ${CRTGFX_WINDOW_BACKEND_LINUX_SOURCES})
else()
  message(FATAL_ERROR "unsupported CRT_STAGE_TARGET_OS=${CRT_STAGE_TARGET_OS}")
endif()
list(TRANSFORM window_sources PREPEND "${CRTGFX_ROOT}/")

set(common_compile_options
  -ffreestanding -fno-builtin -Xclang -fwchar-type=int
  -ffunction-sections -fdata-sections -Wall -Wextra -Werror
)

add_library(crtgfx_window STATIC ${window_sources})
add_library(crtgfx_window_shared SHARED ${window_sources})
foreach(target crtgfx_window crtgfx_window_shared)
  target_compile_options(${target} PRIVATE ${common_compile_options})
  target_include_directories(${target} PUBLIC "${CRTGFX_ROOT}/include")
  target_include_directories(${target} PRIVATE "${CRTGFX_ROOT}/src")
  set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
endforeach()

if(CRT_STAGE_TARGET_OS STREQUAL "linux")
  if(NOT DEFINED CRT_STAGE_XKBCOMMON_PREFIX)
    message(FATAL_ERROR "Linux Simple Graphics requires CRT_STAGE_XKBCOMMON_PREFIX")
  endif()
  foreach(target crtgfx_window crtgfx_window_shared)
    target_include_directories(${target} PRIVATE "${CRT_STAGE_XKBCOMMON_PREFIX}/include")
    target_link_libraries(${target} PRIVATE "${CRT_STAGE_XKBCOMMON_PREFIX}/lib/libxkbcommon.a")
  endforeach()
elseif(CRT_STAGE_TARGET_OS STREQUAL "windows")
  foreach(target crtgfx_window crtgfx_window_shared)
    # crt-cc forwards CRT_WINDOWS_SDK_LIBPATH as one quoted /libpath
    # argument. Absolute paths here would hit the known mksh argv issue.
    target_link_libraries(${target} PUBLIC
      -Wl,user32.lib -Wl,gdi32.lib -Wl,d3d11.lib -Wl,dxgi.lib)
  endforeach()
elseif(CRT_STAGE_TARGET_OS STREQUAL "macos")
  foreach(target crtgfx_window crtgfx_window_shared)
    target_compile_options(${target} PRIVATE
      -Wno-cast-function-type-mismatch -Wno-unknown-warning-option)
    target_link_libraries(${target} PRIVATE
      "-framework Foundation" "-framework AppKit" "-framework QuartzCore"
      "-framework CoreGraphics" objc)
  endforeach()
endif()

set_target_properties(crtgfx_window PROPERTIES OUTPUT_NAME crtgfx)
set_target_properties(crtgfx_window_shared PROPERTIES
  OUTPUT_NAME crtgfx WINDOWS_EXPORT_ALL_SYMBOLS ON)
if(CRT_STAGE_TARGET_OS STREQUAL "windows")
  set_target_properties(crtgfx_window_shared PROPERTIES ARCHIVE_OUTPUT_NAME crtgfx_dll)
endif()

enable_testing()
add_executable(crtgfx_window_smoke "${CRTGFX_ROOT}/tests/window_smoke_test.c")
add_executable(crtgfx_synthetic_event "${CRTGFX_ROOT}/tests/synthetic_event_test.c")
add_executable(crtgfx_window_demo "${CRTGFX_ROOT}/tools/window_demo.c")
foreach(target crtgfx_window_smoke crtgfx_synthetic_event crtgfx_window_demo)
  target_compile_options(${target} PRIVATE ${common_compile_options})
  target_include_directories(${target} PRIVATE "${CRTGFX_ROOT}/include")
  target_link_libraries(${target} PRIVATE crtgfx_window)
endforeach()
add_test(NAME crtgfx_window_smoke_runs COMMAND crtgfx_window_smoke)
add_test(NAME crtgfx_synthetic_event_runs COMMAND crtgfx_synthetic_event)

install(TARGETS crtgfx_window crtgfx_window_shared
  ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install(FILES "${CRTGFX_ROOT}/include/crtgfx/window.h" DESTINATION include/crtgfx)
install(TARGETS crtgfx_window_demo RUNTIME DESTINATION examples/bin)
install(FILES "${CRTGFX_ROOT}/tools/window_demo.c"
  DESTINATION examples/gfx-simple RENAME main.c)
install(FILES "${CRT_STAGE_SOURCE_ROOT}/examples/README.md" DESTINATION examples)
install(FILES "${CRT_STAGE_SOURCE_ROOT}/examples/gfx-simple/CMakeLists.txt"
  DESTINATION examples/gfx-simple)
