cmake_minimum_required(VERSION 3.25)
project(crt_stage_05_ui C CXX)

# Isolated 04-gfx-media -> 05-ui stage: builds crtui (with its private LVGL
# renderer) and its optional Skia / MediaView companions from this source asset
# against the predecessor SDK alone -- no repository checkout, no in-tree
# libcrtgfx/libcrtmedia sources. crtgfx, crtmedia, Skia, FreeType, FFmpeg and the
# curl chain are consumed as the installed 04 SDK's own headers and archives.
#
# Windows is verified. The macOS and Linux branches below follow the 04 stage's
# own proven link interfaces but have not been run; the replays are expected to
# find real gaps, as every earlier stage's first replay did.

foreach(required CRT_STAGE_SOURCE_ROOT CRT_STAGE_TARGET_OS CRT_STAGE_LVGL_ROOT
                 CRT_STAGE_PYTHON CRT_STAGE_LLVM_BIN)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "${required} is required")
  endif()
endforeach()
if(NOT CMAKE_SYSROOT)
  message(FATAL_ERROR "configure with the predecessor SDK's crt-toolchain.cmake")
endif()

set(CRT_TARGET_OS "${CRT_STAGE_TARGET_OS}")
set(CRTUI_ROOT "${CRT_STAGE_SOURCE_ROOT}/libcrtui")
set(CRTUI_LVGL_ROOT "${CRT_STAGE_LVGL_ROOT}")
include("${CRTUI_ROOT}/cmake/crtui_sources.cmake")

# This stage is option-ON like 04: a missing predecessor artifact is an error,
# never a reason to silently ship a crtui without its Skia/MediaView companions.
foreach(required_file
    "${CRTUI_LVGL_ROOT}/include/lvgl/lvgl.h"
    "${CMAKE_SYSROOT}/include/crtgfx/window.h"
    "${CMAKE_SYSROOT}/include/crtgfx/gpu.h"
    "${CMAKE_SYSROOT}/include/crtgfx/skia.h"
    "${CMAKE_SYSROOT}/include/crtmedia/gpu_frame.h"
    "${CMAKE_SYSROOT}/include/include/core/SkSurface.h"
    "${CMAKE_SYSROOT}/lib/libskia.a"
    "${CMAKE_SYSROOT}/lib/libfreetype.a"
    "${CMAKE_SYSROOT}/lib/libcrtgfx.a"
    "${CMAKE_SYSROOT}/lib/libcrtgfx_gpu.a"
    "${CMAKE_SYSROOT}/lib/libcrtgfx_skia.a"
    "${CMAKE_SYSROOT}/lib/libcrtmedia.a"
    "${CMAKE_SYSROOT}/lib/libavformat.a"
    "${CMAKE_SYSROOT}/lib/libavcodec.a"
    "${CMAKE_SYSROOT}/lib/libswresample.a"
    "${CMAKE_SYSROOT}/lib/libavutil.a"
    "${CMAKE_SYSROOT}/lib/libcurl.a"
    "${CMAKE_SYSROOT}/lib/libmbedtls.a"
    "${CMAKE_SYSROOT}/lib/libmbedx509.a"
    "${CMAKE_SYSROOT}/lib/libmbedcrypto.a"
    "${CMAKE_SYSROOT}/lib/libz.a")
  if(NOT EXISTS "${required_file}")
    message(FATAL_ERROR "required 05-ui predecessor artifact is missing: ${required_file}")
  endif()
endforeach()
if(CRT_TARGET_OS STREQUAL "windows")
  foreach(required_file
      "${CMAKE_SYSROOT}/include/crt-win32-shim/mingw_w64_compat.h"
      "${CMAKE_SYSROOT}/include/crt-mingw-w64-headers/_mingw.h")
    if(NOT EXISTS "${required_file}")
      message(FATAL_ERROR "required 05-ui predecessor artifact is missing: ${required_file}")
    endif()
  endforeach()
endif()

set(stage_c_options
  -ffreestanding -fno-builtin -Xclang -fwchar-type=int
  -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
set(stage_cxx_options ${stage_c_options} -std=c++17)

# ---- Predecessor SDK libraries as imported targets ------------------------
# A static archive records no dependency metadata, so each imported target
# reproduces the link interface of the real target in libcrtgfx/libcrtmedia
# (the same "stage keeps its own copy" arrangement the 04 stage documents).
add_library(crtgfx_window INTERFACE IMPORTED GLOBAL)
add_library(crtgfx_gpu INTERFACE IMPORTED GLOBAL)
add_library(crtgfx_skia INTERFACE IMPORTED GLOBAL)
add_library(crtmedia INTERFACE IMPORTED GLOBAL)
foreach(imported crtgfx_window crtgfx_gpu crtgfx_skia crtmedia)
  target_include_directories(${imported} INTERFACE "${CMAKE_SYSROOT}/include")
endforeach()

set(CRTMEDIA_FFMPEG_ARCHIVES
  "${CMAKE_SYSROOT}/lib/libavformat.a" "${CMAKE_SYSROOT}/lib/libavcodec.a"
  "${CMAKE_SYSROOT}/lib/libswresample.a" "${CMAKE_SYSROOT}/lib/libavutil.a")
# curl -> mbedtls -> mbedx509 -> mbedcrypto -> z, as in libcrtmedia/CMakeLists.txt.
set(CRTMEDIA_CURL_ARCHIVES
  "${CMAKE_SYSROOT}/lib/libcurl.a" "${CMAKE_SYSROOT}/lib/libmbedtls.a"
  "${CMAKE_SYSROOT}/lib/libmbedx509.a" "${CMAKE_SYSROOT}/lib/libmbedcrypto.a"
  "${CMAKE_SYSROOT}/lib/libz.a")
set(CRTGFX_SKIA_ARCHIVES
  "${CMAKE_SYSROOT}/lib/libskia.a" "${CMAKE_SYSROOT}/lib/libfreetype.a")

if(CRT_TARGET_OS STREQUAL "windows")
  # "-Wl,<name>.lib", not a bare "<name>.lib": CMake would turn a bare word into
  # "-l<name>", which lld's MinGW driver cannot resolve (see the 04 stage).
  set_property(TARGET crtgfx_window PROPERTY INTERFACE_LINK_LIBRARIES
    "${CMAKE_SYSROOT}/lib/libcrtgfx.a"
    -Wl,user32.lib -Wl,gdi32.lib -Wl,d3d11.lib -Wl,dxgi.lib)
  set_property(TARGET crtgfx_gpu PROPERTY INTERFACE_LINK_LIBRARIES
    "${CMAKE_SYSROOT}/lib/libcrtgfx_gpu.a" crtgfx_window
    -Wl,dxgi.lib -Wl,d3d12.lib)
  set_property(TARGET crtgfx_skia PROPERTY INTERFACE_LINK_LIBRARIES
    "${CMAKE_SYSROOT}/lib/libcrtgfx_skia.a" crtgfx_gpu
    ${CRTGFX_SKIA_ARCHIVES} -Wl,d3dcompiler.lib -Wl,uuid.lib)
  set_property(TARGET crtmedia PROPERTY INTERFACE_LINK_LIBRARIES
    "${CMAKE_SYSROOT}/lib/libcrtmedia.a"
    ${CRTMEDIA_FFMPEG_ARCHIVES} ${CRTMEDIA_CURL_ARCHIVES}
    -Wl,ole32.lib -Wl,d3d11.lib -Wl,dxgi.lib -Wl,mfuuid.lib -Wl,strmiids.lib
    -Wl,mfplat.lib -Wl,mfreadwrite.lib -Wl,Mf.lib -Wl,uuid.lib)
elseif(CRT_TARGET_OS STREQUAL "macos")
  set_property(TARGET crtgfx_window PROPERTY INTERFACE_LINK_LIBRARIES
    "${CMAKE_SYSROOT}/lib/libcrtgfx.a"
    "-framework Foundation" "-framework AppKit" "-framework QuartzCore"
    "-framework CoreGraphics" objc)
  set_property(TARGET crtgfx_gpu PROPERTY INTERFACE_LINK_LIBRARIES
    "${CMAKE_SYSROOT}/lib/libcrtgfx_gpu.a" crtgfx_window
    "-framework Metal" "-framework CoreGraphics")
  set_property(TARGET crtgfx_skia PROPERTY INTERFACE_LINK_LIBRARIES
    "${CMAKE_SYSROOT}/lib/libcrtgfx_skia.a" crtgfx_gpu
    ${CRTGFX_SKIA_ARCHIVES} "-framework CoreVideo")
  # Keep in step with CRTMEDIA_MACOS_FRAMEWORKS in libcrtmedia/CMakeLists.txt.
  set_property(TARGET crtmedia PROPERTY INTERFACE_LINK_LIBRARIES
    "${CMAKE_SYSROOT}/lib/libcrtmedia.a"
    ${CRTMEDIA_FFMPEG_ARCHIVES} ${CRTMEDIA_CURL_ARCHIVES}
    "-framework AudioToolbox" "-framework VideoToolbox" "-framework CoreVideo"
    "-framework CoreMedia" "-framework CoreFoundation" "-framework AVFoundation"
    "-framework Foundation" objc)
elseif(CRT_TARGET_OS STREQUAL "linux")
  # Real host libraries, found the way the 04 stage finds them: the stage
  # toolchain skips CMake's executable ABI probe, so CMAKE_LIBRARY_ARCHITECTURE
  # is empty and plain find_library() misses Debian/Ubuntu multiarch paths.
  find_program(CRT_DPKG_ARCHITECTURE_EXECUTABLE dpkg-architecture)
  if(CRT_DPKG_ARCHITECTURE_EXECUTABLE)
    execute_process(
      COMMAND "${CRT_DPKG_ARCHITECTURE_EXECUTABLE}" -qDEB_HOST_MULTIARCH
      OUTPUT_VARIABLE CRT_HOST_MULTIARCH_TRIPLET
      OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  endif()
  function(crt_stage_find_host_library variable library_name file_names)
    find_library(${variable} NAMES ${library_name} NO_CMAKE_FIND_ROOT_PATH)
    if(NOT ${variable} AND CRT_HOST_MULTIARCH_TRIPLET)
      unset(${variable} CACHE)
      find_file(${variable} NAMES ${file_names}
        PATHS /lib /usr/lib /usr/local/lib
        PATH_SUFFIXES "${CRT_HOST_MULTIARCH_TRIPLET}")
    endif()
    if(NOT ${variable})
      message(FATAL_ERROR "Could not find a real host ${library_name} library")
    endif()
    get_filename_component(directory "${${variable}}" DIRECTORY)
    add_link_options("-Wl,-rpath-link,${directory}")
  endfunction()
  crt_stage_find_host_library(CRT_STAGE_VULKAN_LIB vulkan "libvulkan.so;libvulkan.so.1")
  crt_stage_find_host_library(CRT_STAGE_WAYLAND_CLIENT_LIB wayland-client
    "libwayland-client.so;libwayland-client.so.0")
  crt_stage_find_host_library(CRT_STAGE_VAAPI_LIB va "libva.so;libva.so.2")
  crt_stage_find_host_library(CRT_STAGE_VAAPI_DRM_LIB va-drm "libva-drm.so;libva-drm.so.2")
  # See the 04 stage: FFmpeg's static archives need -Bsymbolic to link into a
  # shared object, harmless for executables.
  add_link_options("-Wl,-Bsymbolic")
  set_property(TARGET crtgfx_window PROPERTY INTERFACE_LINK_LIBRARIES
    "${CMAKE_SYSROOT}/lib/libcrtgfx.a" "${CMAKE_SYSROOT}/lib/libxkbcommon.a"
    "${CMAKE_SYSROOT}/lib/libxdg-shell-protocol.a" "${CRT_STAGE_WAYLAND_CLIENT_LIB}")
  set_property(TARGET crtgfx_gpu PROPERTY INTERFACE_LINK_LIBRARIES
    "${CMAKE_SYSROOT}/lib/libcrtgfx_gpu.a" crtgfx_window "${CRT_STAGE_VULKAN_LIB}")
  # The real host libvulkan must not pick up this project's same-named libc
  # symbols (readdir/opendir): the static archives are scrubbed from the dynamic
  # symbol table, as every Vulkan-linking consumer in the 04 stage does.
  set_property(TARGET crtgfx_gpu PROPERTY INTERFACE_LINK_OPTIONS -Wl,--exclude-libs,ALL)
  set_property(TARGET crtgfx_skia PROPERTY INTERFACE_LINK_LIBRARIES
    "${CMAKE_SYSROOT}/lib/libcrtgfx_skia.a" crtgfx_gpu
    -Wl,--start-group ${CRTGFX_SKIA_ARCHIVES} -Wl,--end-group)
  set_property(TARGET crtmedia PROPERTY INTERFACE_LINK_LIBRARIES
    "${CMAKE_SYSROOT}/lib/libcrtmedia.a"
    -Wl,--start-group ${CRTMEDIA_FFMPEG_ARCHIVES} ${CRTMEDIA_CURL_ARCHIVES} -Wl,--end-group
    "${CRT_STAGE_VAAPI_LIB}" "${CRT_STAGE_VAAPI_DRM_LIB}")
else()
  message(FATAL_ERROR "unsupported CRT_STAGE_TARGET_OS=${CRT_STAGE_TARGET_OS}")
endif()

# ---- crtui: static + shared, LVGL private ---------------------------------
file(GLOB_RECURSE CRTUI_LVGL_SOURCES CONFIGURE_DEPENDS "${CRTUI_LVGL_ROOT}/src/*.c")
list(TRANSFORM CRTUI_CORE_SOURCES PREPEND "${CRTUI_ROOT}/")
list(TRANSFORM CRTUI_LVGL_BACKEND_SOURCES PREPEND "${CRTUI_ROOT}/")
list(TRANSFORM CRTUI_SKIA_SOURCES PREPEND "${CRTUI_ROOT}/")
list(TRANSFORM CRTUI_SKIA_MEDIA_SOURCES PREPEND "${CRTUI_ROOT}/")

# Third-party code: no warnings-as-errors, LVGL reads only crtui's lv_conf.h.
set(CRTUI_LVGL_COMPILE_OPTIONS -w -DLV_CONF_INCLUDE_SIMPLE=1 -fvisibility=hidden)
if(CRT_TARGET_OS STREQUAL "windows")
  # This sysroot has no <windows.h>: the mingw predefined Win32 macros must not
  # steer LVGL into it (same reason as crtmedia's curl sources).
  list(APPEND CRTUI_LVGL_COMPILE_OPTIONS -U_WIN32 -U_WIN32_WCE -U__WIN32__ -UWIN32 -U__MINGW32__)
endif()
set_source_files_properties(${CRTUI_LVGL_SOURCES} PROPERTIES
  COMPILE_OPTIONS "${CRTUI_LVGL_COMPILE_OPTIONS}"
  INCLUDE_DIRECTORIES "${CRTUI_LVGL_ROOT};${CRTUI_LVGL_ROOT}/include;${CRTUI_LVGL_ROOT}/src;${CRTUI_ROOT}/src/lvgl_conf")
set_source_files_properties(${CRTUI_LVGL_BACKEND_SOURCES} PROPERTIES
  COMPILE_OPTIONS "-DLV_CONF_INCLUDE_SIMPLE=1"
  INCLUDE_DIRECTORIES "${CRTUI_LVGL_ROOT}/include;${CRTUI_ROOT}/src/lvgl_conf")

set(crtui_sources ${CRTUI_CORE_SOURCES} ${CRTUI_LVGL_SOURCES} ${CRTUI_LVGL_BACKEND_SOURCES})
add_library(crtui STATIC ${crtui_sources})
add_library(crtui_shared SHARED ${crtui_sources})
foreach(target crtui crtui_shared)
  target_compile_definitions(${target} PRIVATE CRTUI_HAVE_LVGL=1)
  # Private LVGL: hidden by default, only CRTUI_API functions are exported
  # (crtui/api.h). On Windows dllexport is wanted on the DLL only, never on
  # objects that land in a consumer's executable.
  target_compile_options(${target} PRIVATE ${stage_c_options} -fvisibility=hidden)
  target_include_directories(${target} PUBLIC "${CRTUI_ROOT}/include")
  set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON OUTPUT_NAME crtui)
endforeach()
target_compile_definitions(crtui_shared PRIVATE CRTUI_BUILD_LIBRARY=1)
if(NOT CRT_TARGET_OS STREQUAL "windows")
  target_compile_definitions(crtui PRIVATE CRTUI_BUILD_LIBRARY=1)
endif()
# Exports come from CRTUI_API only: exporting everything would publish every LVGL
# symbol from the DLL.
set_target_properties(crtui_shared PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS OFF)
if(CRT_TARGET_OS STREQUAL "windows")
  set_target_properties(crtui_shared PROPERTIES ARCHIVE_OUTPUT_NAME crtui_dll)
elseif(CRT_TARGET_OS STREQUAL "macos")
  # Bind libc symbols to the CRT's libc.dylib, not Apple's libSystem: two-level
  # namespace binding takes the first dylib that defines a symbol, so without
  # this libcrtui calls Apple's 64-byte pthread_mutex_init() on the CRT's
  # 40-byte pthread_mutex_t (docs/acceptance/crtui_acceptance.md, Tranche 3 macOS replay).
  # UNVERIFIED in this isolated stage until the macOS replay.
  target_link_options(crtui_shared PRIVATE "${CMAKE_SYSROOT}/lib/libc.dylib")
  set_target_properties(crtui_shared PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE INSTALL_RPATH "")
elseif(CRT_TARGET_OS STREQUAL "linux")
  set_target_properties(crtui_shared PROPERTIES BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN")
endif()

# ---- Tests (headless, then LVGL render) -----------------------------------
enable_testing()
function(crt_stage_add_ui_test target source expected timeout)
  add_executable(${target} "${CRTUI_ROOT}/tests/${source}")
  target_compile_options(${target} PRIVATE ${stage_c_options})
  target_link_libraries(${target} PRIVATE crtui)
  add_test(NAME ${target}_runs COMMAND ${target})
  set_tests_properties(${target}_runs PROPERTIES
    PASS_REGULAR_EXPRESSION "${expected}" TIMEOUT ${timeout})
endfunction()
crt_stage_add_ui_test(crtui_contract_test contract_test.c "crtui_contract_test: ok" 60)
crt_stage_add_ui_test(crtui_layout_test layout_test.c "crtui_layout_test: ok" 60)
crt_stage_add_ui_test(crtui_surface_test surface_test.c "crtui_surface_test: ok" 60)
crt_stage_add_ui_test(crtui_input_test input_test.c "crtui_input_test: ok" 60)
crt_stage_add_ui_test(crtui_render_test render_test.c "crtui_render_test: ok" 120)
# crtui_window_demo: the present path (a real crtgfx window); not a ctest, the
# stage entrypoint runs the installed copy with a frame limit. crtgfx_window
# before crtui: on Linux it pulls libxkbcommon.a, whose strndup()/asprintf()
# the wrapper's trailing libc group must still see.
add_executable(crtui_window_demo "${CRTUI_ROOT}/tools/window_demo.c")
target_compile_options(crtui_window_demo PRIVATE ${stage_c_options})
target_link_libraries(crtui_window_demo PRIVATE crtgfx_window crtui)
if(CRT_TARGET_OS STREQUAL "macos")
  set_target_properties(crtui_window_demo PROPERTIES
    BUILD_RPATH "@loader_path/../../lib;${CMAKE_BUILD_RPATH}"
    INSTALL_RPATH "@loader_path/../../lib")
elseif(CRT_TARGET_OS STREQUAL "linux")
  set_target_properties(crtui_window_demo PROPERTIES
    BUILD_RPATH "$ORIGIN/../../lib;${CMAKE_BUILD_RPATH}"
    INSTALL_RPATH "$ORIGIN/../../lib")
  target_link_options(crtui_window_demo PRIVATE -Wl,--exclude-libs,ALL)
endif()

# LVGL stays private: headers name no LVGL, the shared library exports only
# crtui_* functions (every declared one), and the sample references no lv_*.
add_test(NAME crtui_privacy_test_runs COMMAND
  "${CRT_STAGE_PYTHON}" "${CRT_STAGE_SOURCE_ROOT}/tools/check_crtui_privacy.py"
    --llvm-bin "${CRT_STAGE_LLVM_BIN}"
    --headers "${CRTUI_ROOT}/include"
    --shared "$<TARGET_FILE:crtui_shared>"
    --object "$<TARGET_OBJECTS:crtui_window_demo>"
    --example-source "${CRTUI_ROOT}/tools/window_demo.c")
set_tests_properties(crtui_privacy_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "crtui_privacy: ok" TIMEOUT 60)

# ---- Optional Skia / MediaView companions ---------------------------------
# Same compile policy as the in-tree crtui_configure_skia_compile(): the per-OS
# GPU backend selects declarations in crtgfx/skia.h, and the mingw/shim header
# set comes from the installed 04 SDK (it bundles both under include/).
function(crt_stage_configure_skia_consumer target)
  target_compile_options(${target} PRIVATE ${stage_cxx_options})
  target_include_directories(${target} PRIVATE "${CRTUI_ROOT}/include" "${CMAKE_SYSROOT}/include")
  if(CRT_TARGET_OS STREQUAL "windows")
    target_compile_definitions(${target} PRIVATE SK_BUILD_FOR_UNIX CRTGFX_HAVE_D3D12=1)
    target_compile_options(${target} PRIVATE
      "-I${CMAKE_SYSROOT}/include/crt-win32-shim"
      "-I${CMAKE_SYSROOT}/include/crt-mingw-w64-headers"
      "-include" "${CMAKE_SYSROOT}/include/crt-win32-shim/mingw_w64_compat.h"
      -Wno-pragma-pack -Wno-unused-value -Wno-ignored-attributes)
  elseif(CRT_TARGET_OS STREQUAL "linux")
    target_compile_definitions(${target} PRIVATE
      CRTGFX_HAVE_VULKAN=1 SK_USE_INTERNAL_VULKAN_HEADERS)
  elseif(CRT_TARGET_OS STREQUAL "macos")
    target_compile_definitions(${target} PRIVATE CRTGFX_HAVE_METAL=1)
  endif()
endfunction()

add_library(crtui_skia STATIC ${CRTUI_SKIA_SOURCES})
target_link_libraries(crtui_skia PUBLIC crtui crtgfx_skia)
crt_stage_configure_skia_consumer(crtui_skia)
set_target_properties(crtui_skia PROPERTIES OUTPUT_NAME crtui_skia POSITION_INDEPENDENT_CODE ON)

add_library(crtui_skia_media STATIC ${CRTUI_SKIA_MEDIA_SOURCES})
target_link_libraries(crtui_skia_media PUBLIC crtui_skia crtgfx_skia crtmedia)
crt_stage_configure_skia_consumer(crtui_skia_media)
set_target_properties(crtui_skia_media PROPERTIES OUTPUT_NAME crtui_skia_media POSITION_INDEPENDENT_CODE ON)

add_executable(crtui_surface_compositor_test "${CRTUI_ROOT}/tests/surface_compositor_test.cc")
target_link_libraries(crtui_surface_compositor_test PRIVATE crtui_skia crtgfx_skia crtmedia)
crt_stage_configure_skia_consumer(crtui_surface_compositor_test)
add_test(NAME crtui_surface_compositor_test_runs COMMAND crtui_surface_compositor_test)
set_tests_properties(crtui_surface_compositor_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "crtui_surface_compositor_test: (ok|skipped)" TIMEOUT 120)

add_executable(crtui_media_view_test "${CRTUI_ROOT}/tests/media_view_test.cc")
target_link_libraries(crtui_media_view_test PRIVATE crtui_skia_media crtui_skia crtgfx_skia crtmedia)
crt_stage_configure_skia_consumer(crtui_media_view_test)
target_compile_definitions(crtui_media_view_test PRIVATE
  "CRTMEDIA_TEST_VIDEO_PATH=\"${CRT_STAGE_SOURCE_ROOT}/libcrtmedia/assets/test_video.mp4\"")
add_test(NAME crtui_media_view_test_runs COMMAND crtui_media_view_test)
set_tests_properties(crtui_media_view_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "crtui_media_view_test: (ok|skipped)" TIMEOUT 120)
# ---- Install --------------------------------------------------------------
install(TARGETS crtui crtui_shared crtui_skia crtui_skia_media
  ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install(DIRECTORY "${CRTUI_ROOT}/include/" DESTINATION include PATTERN ".gitkeep" EXCLUDE)
install(TARGETS crtui_window_demo RUNTIME DESTINATION examples/bin)
install(FILES "${CRTUI_ROOT}/tools/window_demo.c"
  DESTINATION examples/ui-basic RENAME main.c)
install(FILES "${CRT_STAGE_SOURCE_ROOT}/examples/ui-basic/CMakeLists.txt"
  DESTINATION examples/ui-basic)
