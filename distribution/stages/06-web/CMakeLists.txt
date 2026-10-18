cmake_minimum_required(VERSION 3.25)
project(crt_stage_06_web C CXX)
foreach(required CRT_STAGE_SOURCE_ROOT CRT_STAGE_TARGET_OS CRT_STAGE_TARGET_ARCH)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "${required} is required")
  endif()
endforeach()
if(NOT CMAKE_SYSROOT)
  message(FATAL_ERROR "use the installed 05-ui SDK toolchain")
endif()
include("${CRT_STAGE_SOURCE_ROOT}/distribution/stages/05-ui/crt_upper_imports.cmake")
add_executable(crtweb_gpu_worker "${CRT_STAGE_SOURCE_ROOT}/libcrtweb/tools/crtweb_gpu_worker.cc")
if(CRT_STAGE_TARGET_OS STREQUAL "linux")
  set_target_properties(crtweb_gpu_worker PROPERTIES
    BUILD_RPATH "$ORIGIN/../../lib;${CMAKE_SYSROOT}/lib" INSTALL_RPATH "$ORIGIN/../../lib")
endif()
target_compile_features(crtweb_gpu_worker PRIVATE cxx_std_17)
target_include_directories(crtweb_gpu_worker PRIVATE
  "${CRT_STAGE_SOURCE_ROOT}/libcrtweb/include" "${CRT_STAGE_SOURCE_ROOT}/libcrtweb/platform")
target_link_libraries(crtweb_gpu_worker PRIVATE crtui_skia crtgfx_skia crtmedia)
if(CRT_STAGE_TARGET_OS STREQUAL "linux")
  target_compile_definitions(crtweb_gpu_worker PRIVATE CRTGFX_HAVE_VULKAN=1 SK_USE_INTERNAL_VULKAN_HEADERS)
elseif(CRT_STAGE_TARGET_OS STREQUAL "windows")
  target_compile_definitions(crtweb_gpu_worker PRIVATE CRTGFX_HAVE_D3D12=1)
else()
  target_compile_definitions(crtweb_gpu_worker PRIVATE CRTGFX_HAVE_METAL=1)
endif()
add_subdirectory("${CRT_STAGE_SOURCE_ROOT}/examples/web-browser" browser)
install(TARGETS crtweb_gpu_worker RUNTIME DESTINATION libexec/crtweb)
install(DIRECTORY "${CRT_STAGE_SOURCE_ROOT}/libcrtweb/include/crtweb" DESTINATION include)
install(FILES
  "${CRT_STAGE_SOURCE_ROOT}/libcrtweb/platform/crtweb_surface_client.h"
  "${CRT_STAGE_SOURCE_ROOT}/libcrtweb/platform/crtweb_surface_control.h"
  "${CRT_STAGE_SOURCE_ROOT}/libcrtweb/platform/crtweb_surface_wire.h"
  "${CRT_STAGE_SOURCE_ROOT}/libcrtweb/platform/crtweb_gpu_wire.h"
  DESTINATION include/crtweb)
install(FILES "${CRT_STAGE_SOURCE_ROOT}/distribution/stages/05-ui/crt_upper_imports.cmake"
  DESTINATION share/crt/cmake)
