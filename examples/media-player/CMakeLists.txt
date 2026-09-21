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
  target_link_options(crtmedia_player_example PRIVATE -Wl,--exclude-libs,ALL)
endif()
