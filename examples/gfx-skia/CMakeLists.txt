cmake_minimum_required(VERSION 3.25)
project(crtgfx_skia_example CXX)

if(NOT CMAKE_SYSROOT)
  message(FATAL_ERROR "configure with the CRT distribution's crt-toolchain.cmake")
endif()

find_path(CRTGFX_INCLUDE_DIR crtgfx/skia.h
  PATHS "${CMAKE_SYSROOT}/include" NO_DEFAULT_PATH REQUIRED)
if(WIN32)
  find_library(CRTGFX_SKIA_LIBRARY NAMES crtgfx_skia_dll
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
  find_library(CRTGFX_GPU_LIBRARY NAMES crtgfx_gpu_dll
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
  find_library(CRTGFX_WINDOW_LIBRARY NAMES crtgfx_dll
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
else()
  find_library(CRTGFX_SKIA_LIBRARY NAMES crtgfx_skia
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
  find_library(CRTGFX_GPU_LIBRARY NAMES crtgfx_gpu
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
  find_library(CRTGFX_WINDOW_LIBRARY NAMES crtgfx
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
endif()
# Real, absolute .a paths (not find_library(), which would prefer the
# same-named .dylib/.so on this search path) -- see the fuller comment
# below, at the target_link_libraries() call that actually needs these.
set(CRTGFX_SKIA_STATIC_LIBRARY "${CMAKE_SYSROOT}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}skia${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(CRTGFX_FREETYPE_LIBRARY "${CMAKE_SYSROOT}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}freetype${CMAKE_STATIC_LIBRARY_SUFFIX}")
foreach(required_file "${CRTGFX_SKIA_STATIC_LIBRARY}" "${CRTGFX_FREETYPE_LIBRARY}")
  if(NOT EXISTS "${required_file}")
    message(FATAL_ERROR "required crtgfx_skia_example dependency is missing: ${required_file}")
  endif()
endforeach()

# crtgfx/skia.h only declares crtgfx_skia_make_gpu_context()/_wrap_gpu_
# surface()/_gpu_surface_present() (and only #includes the real Ganesh
# include/gpu/ganesh/GrDirectContext.h at all) behind #if defined(CRTGFX_
# HAVE_VULKAN) || defined(CRTGFX_HAVE_D3D12) || defined(CRTGFX_HAVE_METAL)
# -- see that header's own top comment. distribution/stages/04-gfx-media/
# CMakeLists.txt already defines exactly one of these per host (a real
# fix, see gpu_internal.h's own matching story), but that add_compile_
# definitions() call only reaches targets configured in THAT CMake
# project -- this example is configured as its own, separate `cmake -S
# examples/gfx-skia -B ...` run (see tools/build_stage_04_gfx_media.py's
# own build_example()), so it never saw that definition. A real,
# confirmed bug (2026-09-11), first hit configuring this example against
# a real packaged SDK for the first time: main.cc (== libcrtgfx/tools/
# skia_gpu_window_demo.cc) calls all three of those functions directly,
# and without this define they were never declared at all ("use of
# undeclared identifier 'crtgfx_skia_make_gpu_context'"), while GrImage.h
# forward-declares GrDirectContext regardless (used elsewhere in Skia's
# own public headers) -- so sk_sp<GrDirectContext> parsed far enough to
# look plausible before failing later with "member access into
# incomplete type 'GrDirectContext'" once something tried to actually
# use the (never-included) real class.
if(WIN32)
  add_compile_definitions(CRTGFX_HAVE_D3D12=1)
elseif(APPLE)
  add_compile_definitions(CRTGFX_HAVE_METAL=1)
else()
  add_compile_definitions(CRTGFX_HAVE_VULKAN=1)
endif()

add_executable(crtgfx_skia_example main.cc)
# Skia's own public headers (SkSpan_impl.h's std::is_same_v/std::enable_if_t/
# std::size/std::data use, reached transitively via crtgfx/skia.h) need
# real C++17 -- a real, confirmed bug (2026-09-11), first hit configuring
# this example against a real packaged SDK for the first time (this
# project's own in-repo libcrtgfx/CMakeLists.txt always sets this via its
# own stage_cxx_options/CRT_CXX_STANDARD machinery, which this standalone,
# plain-find_library()-based example CMakeLists.txt never inherits).
# Without it, CMake's own default (no C++ standard requested) leaves
# main.cc's transitively-included Skia headers on whatever ancient
# default clang would otherwise pick, failing outright with "no template
# named 'is_same_v' in namespace 'std'" and friends.
set_target_properties(crtgfx_skia_example PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON)
target_include_directories(crtgfx_skia_example PRIVATE "${CRTGFX_INCLUDE_DIR}")
# CRTGFX_GPU_LIBRARY/CRTGFX_WINDOW_LIBRARY are required alongside
# CRTGFX_SKIA_LIBRARY, not just the latter -- see examples/gfx-gpu/
# CMakeLists.txt's own, fuller comment (same real bug, same root cause:
# a plain find_library() file path carries none of the PUBLIC-link-
# library propagation a real CMake IMPORTED/normal target would, so
# crtgfx_skia_shared's own transitive crtgfx_gpu_shared/crtgfx_window_
# shared PUBLIC dependencies never reach this executable's own link
# line). main.cc (== libcrtgfx/tools/skia_gpu_window_demo.cc) calls
# crtgfx_window_create/_gpu_device_create/... directly, exactly like
# examples/gfx-gpu/main.c does.
#
# CRTGFX_SKIA_STATIC_LIBRARY/CRTGFX_FREETYPE_LIBRARY (libskia.a/
# libfreetype.a themselves, not just libcrtgfx_skia.dylib) are ALSO
# required -- a separate real, confirmed bug (2026-09-11), found right
# after fixing the CRTGFX_HAVE_METAL one above. libcrtgfx_skia.dylib
# only contains whatever specific Skia object-file members skia_bridge.cc
# itself (crtgfx_skia_objects' only source) actually references --
# static-archive linking only pulls in members needed to resolve an
# already-outstanding undefined symbol, so anything skia_bridge.cc itself
# never called (SkPath/SkPaint/SkCanvas::drawRect/SkShaders::
# LinearGradient/... -- this example's own skia_reference_scene.h calls
# these directly) was never pulled into that .dylib at all. In the
# in-repo distribution/stages/04-gfx-media/CMakeLists.txt build, crtgfx_
# skia_raster_smoke (which also calls these directly) only works because
# crt_add_crtgfx_skia_shared_target()'s own `target_link_libraries(
# crtgfx_skia_shared PUBLIC ${CRTGFX_SKIA_LIBRARIES})` is PUBLIC --
# CMake's real target-graph propagation re-adds libskia.a/libfreetype.a
# directly onto every *consumer's* own final link line too, automatically.
# This example's plain find_library()-based CRTGFX_SKIA_LIBRARY is just a
# bare file path, not a real CMake target, so none of that propagation
# happens here -- confirmed directly: "Undefined symbols ...
# SkPathBuilder::.../SkPaint::.../SkCanvas::drawRect(...)/..." linking
# without these.
target_link_libraries(crtgfx_skia_example PRIVATE
  "${CRTGFX_SKIA_LIBRARY}" "${CRTGFX_GPU_LIBRARY}" "${CRTGFX_WINDOW_LIBRARY}"
  "${CRTGFX_SKIA_STATIC_LIBRARY}" "${CRTGFX_FREETYPE_LIBRARY}")
if(NOT WIN32 AND NOT APPLE)
  # Same real, confirmed bug as examples/gfx-gpu/CMakeLists.txt's own
  # matching fix (see that file's fuller comment for the full story):
  # this freestanding executable link defaults to --no-allow-shlib-
  # undefined, so the real host libvulkan.so must be locatable via
  # -rpath-link at link time, and libc++abi.so.1's own `_Unwind_*`
  # symbols need libunwind.so linked directly and explicitly (it carries
  # no NEEDED entry for it at all -- LIBCXXABI_USE_LLVM_UNWINDER=OFF, and
  # this project's own imported libcxx now builds with
  # LIBCXX_ENABLE_ABI_LINKER_SCRIPT=OFF, trading away the upstream ABI
  # linker script that used to auto-add `-lunwind` for any libc++.so
  # consumer -- see that recipe's own notes).
  find_library(CRTGFX_EXAMPLE_VULKAN_LIB NAMES vulkan vulkan.so.1
    NO_CMAKE_FIND_ROOT_PATH)
  if(NOT CRTGFX_EXAMPLE_VULKAN_LIB)
    find_program(CRT_DPKG_ARCHITECTURE_EXECUTABLE dpkg-architecture)
    if(CRT_DPKG_ARCHITECTURE_EXECUTABLE)
      execute_process(
        COMMAND "${CRT_DPKG_ARCHITECTURE_EXECUTABLE}" -qDEB_HOST_MULTIARCH
        OUTPUT_VARIABLE CRT_HOST_MULTIARCH_TRIPLET
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
      )
    endif()
    if(CRT_HOST_MULTIARCH_TRIPLET)
      unset(CRTGFX_EXAMPLE_VULKAN_LIB CACHE)
      find_file(CRTGFX_EXAMPLE_VULKAN_LIB
        NAMES libvulkan.so libvulkan.so.1
        PATHS /lib /usr/lib /usr/local/lib
        PATH_SUFFIXES "${CRT_HOST_MULTIARCH_TRIPLET}"
      )
    endif()
  endif()
  target_link_options(crtgfx_skia_example PRIVATE
    "-Wl,-rpath-link,${CMAKE_SYSROOT}/lib")
  if(CRTGFX_EXAMPLE_VULKAN_LIB)
    get_filename_component(CRT_EXAMPLE_VULKAN_LIB_DIR "${CRTGFX_EXAMPLE_VULKAN_LIB}" DIRECTORY)
    target_link_options(crtgfx_skia_example PRIVATE
      "-Wl,-rpath-link,${CRT_EXAMPLE_VULKAN_LIB_DIR}")
  endif()
  target_link_libraries(crtgfx_skia_example PRIVATE "${CMAKE_SYSROOT}/lib/libunwind.so")
endif()

if(WIN32)
  # main.cc #includes real Skia public headers directly (SkCanvas.h etc.)
  # -- same real "direct Skia consumer" needs as this project's own
  # crtgfx_skia_raster_smoke/skia_gpu_window_demo (distribution/stages/
  # 04-gfx-media/CMakeLists.txt's own fuller comment on both), which this
  # standalone project cannot reach directly, so the SDK bundles the two
  # header sets and the emutls stub archive it needs under these fixed,
  # documented locations instead. Real, confirmed bug (2026-09-12): this
  # example was never actually rebuilt+linked purely from an installed SDK
  # until the 04-gfx-media isolated stage acceptance exercised it for the
  # first time. Windows-only: SK_BUILD_FOR_UNIX and the win32_shim/
  # mingw-w64-headers/emutls-stub bundle only exist to work around this
  # mingw-target clang's own _WIN32 predefine tricking SkFeatures.h into
  # thinking it is real MSVC/clang-cl (see crtgfx_skia_raster_smoke's own
  # matching comment in distribution/stages/04-gfx-media/CMakeLists.txt)
  # -- native Linux/macOS clang never hits this at all, so neither the
  # SDK nor this file bundles/needs any of it on those two hosts.
  find_path(CRT_WIN32_SHIM_DIR mingw_w64_compat.h
    PATHS "${CMAKE_SYSROOT}/include/crt-win32-shim" NO_DEFAULT_PATH REQUIRED)
  find_path(CRT_MINGW_W64_HEADERS_DIR _mingw.h
    PATHS "${CMAKE_SYSROOT}/include/crt-mingw-w64-headers" NO_DEFAULT_PATH REQUIRED)
  find_library(CRT_STAGE_EMUTLS_STUB_LIBRARY NAMES crt_stage_emutls_stub
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
  target_compile_definitions(crtgfx_skia_example PRIVATE SK_BUILD_FOR_UNIX)
  target_compile_options(crtgfx_skia_example PRIVATE
    "-I${CRT_WIN32_SHIM_DIR}"
    "-I${CRT_MINGW_W64_HEADERS_DIR}"
    "-include" "${CRT_WIN32_SHIM_DIR}/mingw_w64_compat.h"
    -Wno-pragma-pack -Wno-unused-value -Wno-ignored-attributes)
  target_link_libraries(crtgfx_skia_example PRIVATE "${CRT_STAGE_EMUTLS_STUB_LIBRARY}")
endif()
