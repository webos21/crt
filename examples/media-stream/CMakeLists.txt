cmake_minimum_required(VERSION 3.25)
project(crtmedia_stream_example C)

if(NOT CMAKE_SYSROOT)
  message(FATAL_ERROR "configure with the CRT distribution's crt-toolchain.cmake")
endif()

find_path(CRTMEDIA_INCLUDE_DIR crtmedia/extractor.h
  PATHS "${CMAKE_SYSROOT}/include" NO_DEFAULT_PATH REQUIRED)

if(WIN32)
  find_library(CRTMEDIA_LIBRARY NAMES crtmedia_dll
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
else()
  if(NOT APPLE)
    # Prefer the static archives, same reason as examples/media-player.
    list(INSERT CMAKE_FIND_LIBRARY_SUFFIXES 0 "${CMAKE_STATIC_LIBRARY_SUFFIX}")
  endif()
  find_library(CRTMEDIA_LIBRARY NAMES crtmedia
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
endif()

add_executable(crtmedia_stream_example main.c)
target_include_directories(crtmedia_stream_example PRIVATE "${CRTMEDIA_INCLUDE_DIR}")
target_link_libraries(crtmedia_stream_example PRIVATE "${CRTMEDIA_LIBRARY}")

if(NOT WIN32 AND NOT APPLE)
  # libcrtmedia.a records no dependency metadata: list FFmpeg and the
  # curl -> mbedTLS -> zlib chain (all installed beside it) ahead of the C
  # runtime, in dependency order, exactly as examples/media-player does for
  # FFmpeg. On Windows/macOS the shared crtmedia library already embeds them.
  target_link_libraries(crtmedia_stream_example PRIVATE
    "${CMAKE_SYSROOT}/lib/libavformat.a"
    "${CMAKE_SYSROOT}/lib/libavcodec.a"
    "${CMAKE_SYSROOT}/lib/libswresample.a"
    "${CMAKE_SYSROOT}/lib/libavutil.a"
    "${CMAKE_SYSROOT}/lib/libcurl.a"
    "${CMAKE_SYSROOT}/lib/libmbedtls.a"
    "${CMAKE_SYSROOT}/lib/libmbedx509.a"
    "${CMAKE_SYSROOT}/lib/libmbedcrypto.a"
    "${CMAKE_SYSROOT}/lib/libz.a")

  # FFmpeg's own VA-API objects call real host libva entry points (see
  # examples/media-player/CMakeLists.txt for the full story and the
  # dpkg-architecture multiarch fallback).
  find_library(CRTMEDIA_EXAMPLE_VAAPI_LIB va NO_CMAKE_FIND_ROOT_PATH)
  find_library(CRTMEDIA_EXAMPLE_VAAPI_DRM_LIB va-drm NO_CMAKE_FIND_ROOT_PATH)
  if((NOT CRTMEDIA_EXAMPLE_VAAPI_LIB) OR (NOT CRTMEDIA_EXAMPLE_VAAPI_DRM_LIB))
    find_program(CRT_DPKG_ARCHITECTURE_EXECUTABLE dpkg-architecture)
    if(CRT_DPKG_ARCHITECTURE_EXECUTABLE)
      execute_process(
        COMMAND "${CRT_DPKG_ARCHITECTURE_EXECUTABLE}" -qDEB_HOST_MULTIARCH
        OUTPUT_VARIABLE CRT_HOST_MULTIARCH_TRIPLET
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    endif()
    if(CRT_HOST_MULTIARCH_TRIPLET)
      if(NOT CRTMEDIA_EXAMPLE_VAAPI_LIB)
        unset(CRTMEDIA_EXAMPLE_VAAPI_LIB CACHE)
        find_file(CRTMEDIA_EXAMPLE_VAAPI_LIB NAMES libva.so libva.so.2
          PATHS /lib /usr/lib /usr/local/lib PATH_SUFFIXES "${CRT_HOST_MULTIARCH_TRIPLET}")
      endif()
      if(NOT CRTMEDIA_EXAMPLE_VAAPI_DRM_LIB)
        unset(CRTMEDIA_EXAMPLE_VAAPI_DRM_LIB CACHE)
        find_file(CRTMEDIA_EXAMPLE_VAAPI_DRM_LIB NAMES libva-drm.so libva-drm.so.2
          PATHS /lib /usr/lib /usr/local/lib PATH_SUFFIXES "${CRT_HOST_MULTIARCH_TRIPLET}")
      endif()
    endif()
  endif()
  if((NOT CRTMEDIA_EXAMPLE_VAAPI_LIB) OR (NOT CRTMEDIA_EXAMPLE_VAAPI_DRM_LIB))
    message(FATAL_ERROR "Could not find a real host libva.so/libva-drm.so")
  endif()
  get_filename_component(CRT_EXAMPLE_VAAPI_LIB_DIR "${CRTMEDIA_EXAMPLE_VAAPI_LIB}" DIRECTORY)
  get_filename_component(CRT_EXAMPLE_VAAPI_DRM_LIB_DIR "${CRTMEDIA_EXAMPLE_VAAPI_DRM_LIB}" DIRECTORY)
  target_link_options(crtmedia_stream_example PRIVATE
    "-Wl,-rpath-link,${CRT_EXAMPLE_VAAPI_LIB_DIR}"
    "-Wl,-rpath-link,${CRT_EXAMPLE_VAAPI_DRM_LIB_DIR}")
  target_link_libraries(crtmedia_stream_example PRIVATE
    "${CRTMEDIA_EXAMPLE_VAAPI_LIB}" "${CRTMEDIA_EXAMPLE_VAAPI_DRM_LIB}")
  target_link_options(crtmedia_stream_example PRIVATE -Wl,--exclude-libs,ALL)
endif()
