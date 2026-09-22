cmake_minimum_required(VERSION 3.25)
project(crtmedia_player_example C)

if(NOT CMAKE_SYSROOT)
  message(FATAL_ERROR "configure with the CRT distribution's crt-toolchain.cmake")
endif()

find_path(CRTGFX_INCLUDE_DIR crtgfx/window.h
  PATHS "${CMAKE_SYSROOT}/include" NO_DEFAULT_PATH REQUIRED)
find_path(CRTMEDIA_INCLUDE_DIR crtmedia/player.h
  PATHS "${CMAKE_SYSROOT}/include" NO_DEFAULT_PATH REQUIRED)

if(WIN32)
  find_library(CRTGFX_WINDOW_LIBRARY NAMES crtgfx_dll
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
  find_library(CRTMEDIA_LIBRARY NAMES crtmedia_dll
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
else()
  if(NOT APPLE)
    # Same real reason examples/gfx-simple/CMakeLists.txt's own matching
    # comment documents: prefer the static archives over the identically-
    # named shared libraries, so this executable never pulls in this
    # project's own shared libc.so (and the host-DSO symbol collisions
    # that reintroduces).
    list(INSERT CMAKE_FIND_LIBRARY_SUFFIXES 0 "${CMAKE_STATIC_LIBRARY_SUFFIX}")
  endif()
  find_library(CRTGFX_WINDOW_LIBRARY NAMES crtgfx
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
  find_library(CRTMEDIA_LIBRARY NAMES crtmedia
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
endif()

add_executable(crtmedia_player_example main.c)
target_include_directories(crtmedia_player_example PRIVATE
  "${CRTGFX_INCLUDE_DIR}" "${CRTMEDIA_INCLUDE_DIR}")
target_link_libraries(crtmedia_player_example PRIVATE
  "${CRTMEDIA_LIBRARY}" "${CRTGFX_WINDOW_LIBRARY}")

if(NOT WIN32 AND NOT APPLE)
  # libcrtgfx.a records no transitive dependency metadata -- reproduce the
  # installed window target's own link interface explicitly, same as
  # examples/gfx-simple/CMakeLists.txt (this example links no GPU/Skia
  # backend, so xkbcommon is its only unresolved window dependency).
  #
  # libcrtmedia.a likewise records no dependency on the FFmpeg archives the
  # SDK ships beside it (its extractor/codec objects leave every av*
  # symbol undefined), so they are listed here in the same order the
  # in-tree crtmedia target uses (avformat -> avcodec -> swresample ->
  # avutil). Both this group and libxkbcommon.a must stay *before* the
  # C runtime: crt-cc appends libc/libm/libdl/libc++ in a rescan group after
  # every user input, and libxkbcommon.a's strndup()/asprintf() and
  # FFmpeg's libm/libc calls are only satisfied by archives scanned after
  # them (an earlier libc.a is never revisited for a newly outstanding
  # symbol -- the original "undefined reference to strndup" link failure).
  target_link_libraries(crtmedia_player_example PRIVATE
    "${CMAKE_SYSROOT}/lib/libavformat.a"
    "${CMAKE_SYSROOT}/lib/libavcodec.a"
    "${CMAKE_SYSROOT}/lib/libswresample.a"
    "${CMAKE_SYSROOT}/lib/libavutil.a"
    "${CMAKE_SYSROOT}/lib/libxkbcommon.a")

  # Real host libwayland-client.so (2026-09-22, "Hardware video decode"
  # Tranche 4 fallout): this executable links libcrtgfx.a directly via
  # plain find_library() (see this file's own top comment), bypassing the
  # real crtgfx_window CMake target's own INTERFACE_LINK_LIBRARIES the way
  # examples/gfx-gpu/CMakeLists.txt's own matching comment already
  # documents for CRTGFX_EXAMPLE_WAYLAND_CLIENT_LIB/_VULKAN_LIB -- this
  # standalone project never linked native Wayland at all until now, so
  # this example never needed it before libcrtgfx's own 04-gfx-media
  # window backend started requiring a real host libwayland-client for
  # every consumer, this one included. Same NO_CMAKE_FIND_ROOT_PATH +
  # dpkg-architecture multiarch find_file() fallback as that file, for the
  # identical reason (this project's own crt-toolchain.cmake sets
  # CMAKE_SYSROOT, which skips the executable-link ABI probe that would
  # otherwise populate CMAKE_LIBRARY_ARCHITECTURE).
  find_library(CRTMEDIA_EXAMPLE_WAYLAND_CLIENT_LIB NAMES wayland-client wayland-client.so.0
    NO_CMAKE_FIND_ROOT_PATH)
  if(NOT CRTMEDIA_EXAMPLE_WAYLAND_CLIENT_LIB)
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
      unset(CRTMEDIA_EXAMPLE_WAYLAND_CLIENT_LIB CACHE)
      find_file(CRTMEDIA_EXAMPLE_WAYLAND_CLIENT_LIB
        NAMES libwayland-client.so.0
        PATHS /lib /usr/lib /usr/local/lib
        PATH_SUFFIXES "${CRT_HOST_MULTIARCH_TRIPLET}"
      )
    endif()
  endif()
  if(NOT CRTMEDIA_EXAMPLE_WAYLAND_CLIENT_LIB)
    message(FATAL_ERROR "Could not find a real host libwayland-client.so/.so.0")
  endif()
  get_filename_component(CRT_EXAMPLE_WAYLAND_CLIENT_LIB_DIR "${CRTMEDIA_EXAMPLE_WAYLAND_CLIENT_LIB}" DIRECTORY)
  target_link_options(crtmedia_player_example PRIVATE
    "-Wl,-rpath-link,${CRT_EXAMPLE_WAYLAND_CLIENT_LIB_DIR}")

  # Real host libva.so/libva-drm.so (2026-09-22, same tranche): FFmpeg's
  # own hwcontext_vaapi.o/vaapi_decode.o/vaapi_h264.o (libavutil.a/
  # libavcodec.a above) call real vaInitialize/vaGetDisplayDRM/... entry
  # points, the same real-host-driver-library shape as
  # CRTMEDIA_LINUX_VAAPI_LIBS in libcrtmedia/CMakeLists.txt and
  # distribution/stages/04-gfx-media/CMakeLists.txt -- this standalone
  # project must replicate it by hand exactly like it already does for
  # the FFmpeg archives above, for the same "plain find_library() bypasses
  # CMake's own PRIVATE-dependency propagation" reason.
  find_library(CRTMEDIA_EXAMPLE_VAAPI_LIB va NO_CMAKE_FIND_ROOT_PATH)
  find_library(CRTMEDIA_EXAMPLE_VAAPI_DRM_LIB va-drm NO_CMAKE_FIND_ROOT_PATH)
  if((NOT CRTMEDIA_EXAMPLE_VAAPI_LIB) OR (NOT CRTMEDIA_EXAMPLE_VAAPI_DRM_LIB))
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
      if(NOT CRTMEDIA_EXAMPLE_VAAPI_LIB)
        unset(CRTMEDIA_EXAMPLE_VAAPI_LIB CACHE)
        find_file(CRTMEDIA_EXAMPLE_VAAPI_LIB
          NAMES libva.so libva.so.2
          PATHS /lib /usr/lib /usr/local/lib
          PATH_SUFFIXES "${CRT_HOST_MULTIARCH_TRIPLET}"
        )
      endif()
      if(NOT CRTMEDIA_EXAMPLE_VAAPI_DRM_LIB)
        unset(CRTMEDIA_EXAMPLE_VAAPI_DRM_LIB CACHE)
        find_file(CRTMEDIA_EXAMPLE_VAAPI_DRM_LIB
          NAMES libva-drm.so libva-drm.so.2
          PATHS /lib /usr/lib /usr/local/lib
          PATH_SUFFIXES "${CRT_HOST_MULTIARCH_TRIPLET}"
        )
      endif()
    endif()
  endif()
  if((NOT CRTMEDIA_EXAMPLE_VAAPI_LIB) OR (NOT CRTMEDIA_EXAMPLE_VAAPI_DRM_LIB))
    message(FATAL_ERROR "Could not find a real host libva.so/libva-drm.so")
  endif()
  get_filename_component(CRT_EXAMPLE_VAAPI_LIB_DIR "${CRTMEDIA_EXAMPLE_VAAPI_LIB}" DIRECTORY)
  get_filename_component(CRT_EXAMPLE_VAAPI_DRM_LIB_DIR "${CRTMEDIA_EXAMPLE_VAAPI_DRM_LIB}" DIRECTORY)
  target_link_options(crtmedia_player_example PRIVATE
    "-Wl,-rpath-link,${CRT_EXAMPLE_VAAPI_LIB_DIR}"
    "-Wl,-rpath-link,${CRT_EXAMPLE_VAAPI_DRM_LIB_DIR}")

  target_link_libraries(crtmedia_player_example PRIVATE
    "${CMAKE_SYSROOT}/lib/libxdg-shell-protocol.a"
    "${CRTMEDIA_EXAMPLE_WAYLAND_CLIENT_LIB}"
    "${CRTMEDIA_EXAMPLE_VAAPI_LIB}" "${CRTMEDIA_EXAMPLE_VAAPI_DRM_LIB}")

  target_link_options(crtmedia_player_example PRIVATE -Wl,--exclude-libs,ALL)
endif()
