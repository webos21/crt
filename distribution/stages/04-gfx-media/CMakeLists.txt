cmake_minimum_required(VERSION 3.25)
project(crt_stage_04_gfx_media C CXX)

foreach(required CRT_STAGE_SOURCE_ROOT CRT_STAGE_TARGET_OS
                 CRT_STAGE_FFMPEG_PREFIX CRT_STAGE_FREETYPE_PREFIX
                 CRT_STAGE_SKIA_PREFIX)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "${required} is required")
  endif()
endforeach()
if(NOT CMAKE_SYSROOT)
  message(FATAL_ERROR "configure with the predecessor SDK's crt-toolchain.cmake")
endif()

# This stage is intentionally option-ON. Missing third-party artifacts are an
# error rather than a reason to silently produce the ordinary default-OFF 04
# package.
set(CRT_TARGET_OS "${CRT_STAGE_TARGET_OS}")
set(CRTGFX_ENABLE_SKIA ON)
set(CRTMEDIA_ENABLE_FFMPEG ON)
set(CRT_USE_IMPORTED_LIBCXX ON)
set(CRT_LIBCXX_SYSROOT "${CMAKE_SYSROOT}")
set(CRTGFX_ROOT "${CRT_STAGE_SOURCE_ROOT}/libcrtgfx")
set(CRTMEDIA_ROOT "${CRT_STAGE_SOURCE_ROOT}/libcrtmedia")
set(CRTGFX_SKIA_ROOT "${CRT_STAGE_SKIA_PREFIX}/include")
set(CRTGFX_SKIA_LIBRARIES
  "${CRT_STAGE_SKIA_PREFIX}/lib/libskia.a"
  "${CRT_STAGE_FREETYPE_PREFIX}/lib/libfreetype.a"
)
set(CRTGFX_CRT_STATIC_LIBS)
set(CRTGFX_CRT_SHARED_LIBS)
set(CRTMEDIA_CRT_STATIC_LIBS)
set(CRTMEDIA_CRT_SHARED_LIBS)

foreach(required_file
    "${CRTGFX_SKIA_ROOT}/include/core/SkSurface.h"
    "${CRT_STAGE_SKIA_PREFIX}/include/modules/skcms/skcms.h"
    "${CRT_STAGE_SKIA_PREFIX}/include/modules/skcms/src/skcms_public.h"
    "${CRT_STAGE_SKIA_PREFIX}/lib/libskia.a"
    "${CRT_STAGE_FREETYPE_PREFIX}/lib/libfreetype.a"
    "${CRT_STAGE_FFMPEG_PREFIX}/include/libavformat/avformat.h"
    "${CRT_STAGE_FFMPEG_PREFIX}/lib/libavformat.a"
    "${CRT_STAGE_FFMPEG_PREFIX}/lib/libavcodec.a"
    "${CRT_STAGE_FFMPEG_PREFIX}/lib/libswresample.a"
    "${CRT_STAGE_FFMPEG_PREFIX}/lib/libavutil.a")
  if(NOT EXISTS "${required_file}")
    message(FATAL_ERROR "required 04-gfx-media dependency is missing: ${required_file}")
  endif()
endforeach()

set(CRTMEDIA_FFMPEG_PORT_PREFIX "${CRT_STAGE_FFMPEG_PREFIX}")
set(CRTMEDIA_FFMPEG_LIBRARIES
  "${CRT_STAGE_FFMPEG_PREFIX}/lib/libavformat.a"
  "${CRT_STAGE_FFMPEG_PREFIX}/lib/libavcodec.a"
  "${CRT_STAGE_FFMPEG_PREFIX}/lib/libswresample.a"
  "${CRT_STAGE_FFMPEG_PREFIX}/lib/libavutil.a"
)

set(stage_c_options
  -ffreestanding -fno-builtin -Xclang -fwchar-type=int
  -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
set(stage_cxx_options ${stage_c_options} -std=c++17)

# 03-gfx-simple is the predecessor SDK, so consume its installed window
# libraries rather than rebuilding them from this source asset.
add_library(crtgfx_window INTERFACE IMPORTED GLOBAL)
add_library(crtgfx_window_shared INTERFACE IMPORTED GLOBAL)
target_include_directories(crtgfx_window INTERFACE "${CMAKE_SYSROOT}/include")
target_include_directories(crtgfx_window_shared INTERFACE "${CMAKE_SYSROOT}/include")
if(CRT_TARGET_OS STREQUAL "windows")
  set_property(TARGET crtgfx_window PROPERTY INTERFACE_LINK_LIBRARIES
    "${CMAKE_SYSROOT}/lib/libcrtgfx.a")
  set_property(TARGET crtgfx_window_shared PROPERTY INTERFACE_LINK_LIBRARIES
    "${CMAKE_SYSROOT}/lib/libcrtgfx_dll.dll.a")
elseif(CRT_TARGET_OS STREQUAL "macos")
  # crtgfx_window here is only an IMPORTED stub around the predecessor
  # SDK's already-built libcrtgfx.a, not the real distribution/stages/
  # 03-gfx-simple/CMakeLists.txt STATIC target -- so none of that file's
  # own PRIVATE "-framework Foundation/AppKit/QuartzCore/CoreGraphics
  # objc" entries (needed to resolve window_cocoa.c's real AppKit/objc
  # symbols baked into that .a) travel with it: PRIVATE link libraries of
  # a STATIC library are a CMake build-graph property of that library's
  # own target, not something recorded inside the .a file itself, so a
  # plain IMPORTED wrapper around the archive path has no way to know
  # about them. A real, confirmed bug (2026-09-11): crtgfx_gpu_test and
  # crtgfx_gpu_window_demo -- both link crtgfx_gpu PUBLIC, which links
  # this crtgfx_window PUBLIC -- failed outright with dozens of undefined
  # objc_msgSend/objc_getClass/kCAGravityResize/... symbols, all from
  # window_cocoa.c.o already inside the imported archive. Re-listing the
  # same frameworks/objc here (this time as part of the IMPORTED target's
  # own INTERFACE_LINK_LIBRARIES, the only kind of link info an IMPORTED
  # target can carry) fixes it. libcrtgfx.dylib below needs none of this:
  # a real shared library resolves its own dependencies at its own link
  # time and records them as load commands the dynamic linker follows
  # automatically, unlike a static archive.
  set_property(TARGET crtgfx_window PROPERTY INTERFACE_LINK_LIBRARIES
    "${CMAKE_SYSROOT}/lib/libcrtgfx.a"
    "-framework Foundation" "-framework AppKit" "-framework QuartzCore"
    "-framework CoreGraphics" objc)
  set_property(TARGET crtgfx_window_shared PROPERTY INTERFACE_LINK_LIBRARIES
    "${CMAKE_SYSROOT}/lib/libcrtgfx.dylib")
elseif(CRT_TARGET_OS STREQUAL "linux")
  set_property(TARGET crtgfx_window PROPERTY INTERFACE_LINK_LIBRARIES
    "${CMAKE_SYSROOT}/lib/libcrtgfx.a")
  set_property(TARGET crtgfx_window_shared PROPERTY INTERFACE_LINK_LIBRARIES
    "${CMAKE_SYSROOT}/lib/libcrtgfx.so")
else()
  message(FATAL_ERROR "unsupported CRT_STAGE_TARGET_OS=${CRT_STAGE_TARGET_OS}")
endif()

include("${CRTGFX_ROOT}/cmake/crtgfx_gpu_sources.cmake")
include("${CRTGFX_ROOT}/cmake/crtgfx_gpu_targets.cmake")
include("${CRTGFX_ROOT}/cmake/crtgfx_skia_targets.cmake")
include("${CRTMEDIA_ROOT}/cmake/crtmedia_targets.cmake")

set(CRTGFX_GPU_SOURCES ${CRTGFX_GPU_COMMON_SOURCES})
if(CRT_TARGET_OS STREQUAL "windows")
  set(CRTGFX_GPU_BACKEND_SOURCES ${CRTGFX_GPU_BACKEND_WINDOWS_SOURCES})
  set(CRTGFX_WINDOWS_DXGI_LIB dxgi.lib)
  set(CRTGFX_WINDOWS_D3D12_LIB d3d12.lib)
  set(CRTGFX_WINDOWS_D3DCOMPILER_LIB d3dcompiler.lib)
  set(CRTGFX_WINDOWS_UUID_LIB uuid.lib)
  set(CRTGFX_WIN32_SHIM_ROOT
    "${CRT_STAGE_SOURCE_ROOT}/libstdc++/third_party/win32_shim")
  set(CRT_MINGW_W64_HEADERS_INCLUDE_ROOT "${CRT_STAGE_MINGW_W64_HEADERS_ROOT}")
  if(NOT CRT_STAGE_MINGW_W64_HEADERS_ROOT)
    message(FATAL_ERROR "Windows 04-gfx-media requires CRT_STAGE_MINGW_W64_HEADERS_ROOT")
  endif()
elseif(CRT_TARGET_OS STREQUAL "macos")
  set(CRTGFX_GPU_BACKEND_SOURCES ${CRTGFX_GPU_BACKEND_MACOS_SOURCES})
  set(CRTGFX_LIBCXX_UNEXPORTED
    "${CRT_STAGE_SOURCE_ROOT}/libstdc++/third_party/libcxx/libc++.unexported.exp")
elseif(CRT_TARGET_OS STREQUAL "linux")
  find_library(CRTGFX_LINUX_VULKAN_LIB NAMES vulkan vulkan.so.1
    NO_CMAKE_FIND_ROOT_PATH REQUIRED)
  set(CRTGFX_GPU_BACKEND_SOURCES ${CRTGFX_GPU_BACKEND_LINUX_SOURCES})
endif()
# libcrtgfx/src/gpu_internal.h's own struct crtgfx_gpu_device/crtgfx_gpu_
# surface field sets (and gpu_metal.c/gpu_vulkan.c/gpu_win32.c's own real
# field accesses into them) are all behind a per-backend #if defined(CRT_
# TARGET_OS_<os>) && defined(CRTGFX_HAVE_<backend>) guard -- the in-tree
# libcrtgfx/CMakeLists.txt defines CRTGFX_HAVE_METAL/_D3D12 unconditionally
# per host and CRTGFX_HAVE_VULKAN once a real libvulkan is found, but this
# standalone stage project never included that file, only the shared
# crtgfx_gpu_*.cmake *source list* modules -- a real, confirmed bug
# (2026-09-11): without this, gpu_internal.h's own structs fell through to
# their bodyless "#else int reserved;" branch while gpu_metal.c's real
# field accesses (device->mtl_device, surface->height, ...) were compiled
# unconditionally, failing outright with "no member named 'height' in
# 'struct crtgfx_gpu_surface'" the first time this stage's own macOS build
# actually reached that file.
if(CRT_TARGET_OS STREQUAL "windows")
  add_compile_definitions(CRTGFX_HAVE_D3D12=1)
elseif(CRT_TARGET_OS STREQUAL "macos")
  add_compile_definitions(CRTGFX_HAVE_METAL=1)
elseif(CRT_TARGET_OS STREQUAL "linux")
  add_compile_definitions(CRTGFX_HAVE_VULKAN=1)
endif()
list(TRANSFORM CRTGFX_GPU_SOURCES PREPEND "${CRTGFX_ROOT}/")
list(TRANSFORM CRTGFX_GPU_BACKEND_SOURCES PREPEND "${CRTGFX_ROOT}/")
set(CRTGFX_LIBC_INCLUDE_DIR "${CMAKE_SYSROOT}/include")
set(CRTGFX_EMUTLS_STUB_SOURCE
  "${CRT_STAGE_SOURCE_ROOT}/libc/src/arch/windows/common/emutls_link_stubs.c")

crt_add_crtgfx_gpu_common_targets()
target_compile_options(crtgfx_gpu_common_objects PRIVATE ${stage_c_options})
if(CRT_TARGET_OS STREQUAL "linux")
  target_compile_options(crtgfx_gpu_backend_objects PRIVATE ${stage_c_options})
endif()
crt_add_crtgfx_skia_object_target()
target_compile_options(crtgfx_skia_objects PRIVATE ${stage_cxx_options})
crt_add_crtgfx_skia_static_target()
crt_add_crtgfx_gpu_shared_target()
crt_add_crtgfx_skia_shared_target()

if(CRT_TARGET_OS STREQUAL "windows")
  set(CRTMEDIA_BACKEND_SOURCES src/arch/windows/audio_sink_wasapi.c)
  set(CRTMEDIA_WINDOWS_OLE32_LIB ole32.lib)
  set(CRTMEDIA_WINDOWS_D3D11_LIB d3d11.lib)
  set(CRTMEDIA_WINDOWS_DXGI_LIB dxgi.lib)
elseif(CRT_TARGET_OS STREQUAL "macos")
  set(CRTMEDIA_BACKEND_SOURCES src/arch/macos/audio_sink_coreaudio.c)
  set(CRTMEDIA_MACOS_FRAMEWORKS
    "-framework AudioToolbox" "-framework VideoToolbox"
    "-framework CoreVideo" "-framework CoreMedia")
else()
  set(CRTMEDIA_BACKEND_SOURCES src/arch/linux/audio_sink_linux.c)
endif()
crt_add_crtmedia_targets()
target_compile_options(crtmedia PRIVATE ${stage_c_options})
target_compile_options(crtmedia_shared PRIVATE ${stage_c_options})
if(TARGET crtmedia_backend_objects AND NOT CRT_TARGET_OS STREQUAL "windows")
  target_compile_options(crtmedia_backend_objects PRIVATE ${stage_c_options})
endif()

enable_testing()
function(crt_stage_register_test name target expected)
  set(predecessor_runtime "")
  if(CRT_TARGET_OS STREQUAL "windows")
    set(runtime_variable PATH)
    # Windows DLLs (this stage's own crtgfx_gpu_dll.dll/crtmedia_dll.dll
    # included) land in RUNTIME_OUTPUT_DIRECTORY, i.e. .../bin -- unlike
    # the macOS/Linux LIBRARY_OUTPUT_DIRECTORY case just below.
    set(own_runtime "${CMAKE_BINARY_DIR}/bin")
    set(predecessor_runtime "${CMAKE_SYSROOT}/bin")
  elseif(CRT_TARGET_OS STREQUAL "macos")
    set(runtime_variable DYLD_LIBRARY_PATH)
    set(own_runtime "${CMAKE_BINARY_DIR}/lib")
    # No predecessor_runtime (${CMAKE_SYSROOT}/lib) here, unlike Windows/
    # Linux just above/below -- a real, confirmed bug (2026-09-11): every
    # one of this stage's own 8 ctest binaries failed (Bus error/
    # Segmentation fault, or crtgfx_skia_raster_smoke's own "Symbol not
    # found: __ZnamSt19__type_descriptor_t, expected as weak-def export
    # from some loaded dylib") the first time this function's own env
    # wrapping was ever actually exercised end-to-end (isolated 03-gfx-
    # simple -> 04-gfx-media stage upgrade). Root-caused directly (ran
    # each failing ctest COMMAND by hand, then bisected by env var):
    # dyld's DYLD_LIBRARY_PATH override resolves a dependency by *leaf
    # filename* against every directory listed, ahead of that
    # dependency's own recorded path -- even an already-absolute one.
    # ${CMAKE_SYSROOT}/lib (the predecessor SDK) contains this project's
    # own imported libc++.1.dylib/libc++abi.1.dylib, under the exact same
    # leaf names as the real system copies. Every one of these test
    # executables links at least one real Apple framework (Foundation/
    # AppKit/CoreAudio/AudioToolboxCore/...), and those frameworks
    # transitively load the *real* /usr/lib/libc++.1.dylib themselves --
    # DYLD_LIBRARY_PATH silently swaps in this project's own, ABI-
    # incompatible libc++.1.dylib for that unrelated real framework's own
    # internal use instead, corrupting it. Confirmed directly: every
    # affected binary here ran perfectly (`<name>: ok`, exit 0) with
    # DYLD_LIBRARY_PATH unset entirely, resolving 100% of its own real
    # CRT dependencies (libcrtgfx.dylib/libcrtgfx_gpu.dylib/libcrtgfx_
    # skia.dylib/the imported libc++.dylib) via already-embedded absolute
    # paths or @rpath + this stage's own linker-baked -Wl,-rpath entries
    # (see tools/crt-cc's/tools/crt-c++'s own shared-link "entry" logic
    # and CMAKE_BUILD_RPATH/CMAKE_INSTALL_RPATH in the generated crt-
    # toolchain.cmake) -- so, unlike Windows/Linux above/below, nothing
    # here ever actually needed this predecessor_runtime override, only
    # ever amounted to a live landmine once a real Apple framework was in
    # the picture (which no prior consumer of this shared cmake helper --
    # this function only exists in this stage -- had ever exercised
    # before Skia/Metal made that the norm here).
  else()
    set(runtime_variable LD_LIBRARY_PATH)
    set(own_runtime "${CMAKE_BINARY_DIR}/lib")
    set(predecessor_runtime "${CMAKE_SYSROOT}/lib")
  endif()
  # Scope the newly-built and predecessor shared-library paths to each test;
  # never poison the host cmake/ctest process itself with CRT libc++.
  set(env_modify --modify "${runtime_variable}=path_list_prepend:${own_runtime}")
  if(predecessor_runtime)
    list(APPEND env_modify --modify "${runtime_variable}=path_list_prepend:${predecessor_runtime}")
  endif()
  add_test(NAME ${name} COMMAND "${CMAKE_COMMAND}" -E env
    ${env_modify}
    "$<TARGET_FILE:${target}>")
  set_tests_properties(${name} PROPERTIES PASS_REGULAR_EXPRESSION "${expected}")
endfunction()

function(crt_stage_add_c_test target source library expected)
  add_executable(${target} "${source}")
  target_compile_options(${target} PRIVATE ${stage_c_options})
  target_include_directories(${target} PRIVATE
    "${CRTGFX_ROOT}/include" "${CRTMEDIA_ROOT}/include")
  target_link_libraries(${target} PRIVATE ${library})
  crt_stage_register_test(${target}_runs ${target} "${expected}")
endfunction()

crt_stage_add_c_test(crtgfx_gpu_test "${CRTGFX_ROOT}/tests/gpu_test.c"
  crtgfx_gpu "crtgfx_gpu_test: ok")
crt_stage_add_c_test(crtmedia_frame_test "${CRTMEDIA_ROOT}/tests/frame_test.c"
  crtmedia "crtmedia_frame_test: ok")
crt_stage_add_c_test(crtmedia_gpu_frame_test "${CRTMEDIA_ROOT}/tests/gpu_frame_test.c"
  crtmedia "crtmedia_gpu_frame_test: ok")
crt_stage_add_c_test(crtmedia_format_test "${CRTMEDIA_ROOT}/tests/format_test.c"
  crtmedia "crtmedia_format_test: ok")
crt_stage_add_c_test(crtmedia_player_test "${CRTMEDIA_ROOT}/tests/player_test.c"
  crtmedia "crtmedia_player_test: ok")
crt_stage_add_c_test(crtmedia_audio_sink_test "${CRTMEDIA_ROOT}/tests/audio_sink_test.c"
  crtmedia "crtmedia_audio_sink_test: ok")
crt_stage_add_c_test(crtmedia_demux_test "${CRTMEDIA_ROOT}/tests/demux_decode_test.c"
  crtmedia "crtmedia_demux_test: ok")
target_compile_definitions(crtmedia_demux_test PRIVATE
  "CRTMEDIA_TEST_WAV_PATH=\"${CRTMEDIA_ROOT}/assets/test_tone.wav\"")

add_executable(crtgfx_skia_raster_smoke "${CRTGFX_ROOT}/tests/skia_raster_smoke.cc")
target_compile_options(crtgfx_skia_raster_smoke PRIVATE ${stage_cxx_options})
target_compile_definitions(crtgfx_skia_raster_smoke PRIVATE
  "CRT_SKIA_FONTS_DIR=\"${CRTGFX_ROOT}/assets/fonts\"")
target_include_directories(crtgfx_skia_raster_smoke PRIVATE
  "${CRTGFX_ROOT}/include" "${CRTGFX_SKIA_ROOT}")
target_link_libraries(crtgfx_skia_raster_smoke PRIVATE crtgfx_skia_shared)
crt_stage_register_test(crtgfx_skia_raster_smoke_runs
  crtgfx_skia_raster_smoke "crtgfx_skia_raster_smoke: ok")

add_executable(crtgfx_gpu_window_demo "${CRTGFX_ROOT}/tools/gpu_window_demo.c")
target_compile_options(crtgfx_gpu_window_demo PRIVATE ${stage_c_options})
target_link_libraries(crtgfx_gpu_window_demo PRIVATE crtgfx_gpu)
add_executable(crtgfx_skia_gpu_window_demo
  "${CRTGFX_ROOT}/tools/skia_gpu_window_demo.cc")
target_compile_options(crtgfx_skia_gpu_window_demo PRIVATE ${stage_cxx_options})
target_include_directories(crtgfx_skia_gpu_window_demo PRIVATE
  "${CRTGFX_ROOT}/tests" "${CRTGFX_SKIA_ROOT}")
target_link_libraries(crtgfx_skia_gpu_window_demo PRIVATE crtgfx_skia_shared)

install(TARGETS crtgfx_gpu crtgfx_gpu_shared crtgfx_skia crtgfx_skia_shared
                crtmedia crtmedia_shared
  ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install(DIRECTORY "${CRTGFX_ROOT}/include/" DESTINATION include)
install(DIRECTORY "${CRTMEDIA_ROOT}/include/" DESTINATION include)
install(DIRECTORY "${CRTGFX_SKIA_ROOT}/include" DESTINATION include)
install(TARGETS crtgfx_gpu_window_demo crtgfx_skia_gpu_window_demo
  RUNTIME DESTINATION examples/bin)
install(FILES "${CRTGFX_ROOT}/tools/gpu_window_demo.c"
  DESTINATION examples/gfx-gpu RENAME main.c)
install(FILES "${CRTGFX_ROOT}/tools/skia_gpu_window_demo.cc"
  DESTINATION examples/gfx-skia RENAME main.cc)
install(FILES "${CRTGFX_ROOT}/tests/skia_reference_scene.h"
  DESTINATION examples/gfx-skia)
install(FILES "${CRT_STAGE_SOURCE_ROOT}/examples/gfx-gpu/CMakeLists.txt"
  DESTINATION examples/gfx-gpu)
install(FILES "${CRT_STAGE_SOURCE_ROOT}/examples/gfx-skia/CMakeLists.txt"
  DESTINATION examples/gfx-skia)
