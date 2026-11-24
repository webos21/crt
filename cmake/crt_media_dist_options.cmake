# Which Skia/FFmpeg/curl option combinations the ordinary 04-gfx-media (`crt-media-dist`) and
# 05-ui (`crt-ui-dist`) packages can describe honestly (PAL-04; docs/guides/distribution.md).
#
# The package publishes each optional dependency closure (link/runtime artifacts, notices, provenance, relink
# kit and source offer) only when the in-tree build produced all of it. The closures are:
#
#   Skia    FreeType and Skia itself
#   FFmpeg  FFmpeg with curl, Mbed TLS and zlib (curl is required: the HTTP transport is compiled only with FFmpeg)
#
# Combinations, by (Skia, FFmpeg, curl):
#
#   OFF, OFF, any   nothing optional is published (curl has no effect without FFmpeg)
#   ON,  ON,  ON    both closures
#   OFF, ON,  ON    the FFmpeg closure only (Skia-less)      -- needs PARTIAL
#   ON,  OFF, any   the Skia closure only (FFmpeg-less)      -- needs PARTIAL
#
# Anything else (FFmpeg without curl) would install libraries whose dependencies the package does not carry, so
# it is rejected before the expensive build instead of after it. The two partial closures are accepted only
# where `partial` is true: CMakeLists.txt passes it for the hosts whose packaging has been replayed
# (Windows/x64); Linux and macOS keep refusing them until their own replay. The rule itself applies to every
# target OS (Linux, Windows and macOS).
#
# Use as a module: crt_media_dist_option_problem(<out-var> <skia> <ffmpeg> <curl> <partial>) sets the variable
# to an empty string for a supported combination, else to the reason. Run directly (`cmake
# -DCRTGFX_ENABLE_SKIA=.. -DCRTMEDIA_ENABLE_FFMPEG=.. -DCRTMEDIA_ENABLE_CURL=.. [-DCRT_MEDIA_DIST_PARTIAL=ON] -P
# <this file>`) it fails with that reason, which is how tools/test_media_dist_options.py exercises it.

function(crt_media_dist_option_problem out skia ffmpeg curl partial)
  set(problem "")
  if(skia AND ffmpeg AND curl)
    # Both closures are published.
  elseif(NOT skia AND NOT ffmpeg)
    # The options-off path: nothing optional is published or installed.
  elseif(partial AND ffmpeg AND NOT curl)
    set(problem
      "crt-media-dist/crt-ui-dist cannot package this option combination "
      "(CRTGFX_ENABLE_SKIA=${skia}, CRTMEDIA_ENABLE_FFMPEG=${ffmpeg}, CRTMEDIA_ENABLE_CURL=${curl}): "
      "FFmpeg media needs curl, Mbed TLS and zlib in its dependency closure. Enable CRTMEDIA_ENABLE_CURL, "
      "or turn FFmpeg off. Other targets still build. Only these dist targets are refused.")
  elseif(partial)
    # Skia-less (FFmpeg and curl, no Skia) or FFmpeg-less (Skia, no FFmpeg): one complete closure.
  else()
    set(problem
      "crt-media-dist/crt-ui-dist cannot package this option combination "
      "(CRTGFX_ENABLE_SKIA=${skia}, CRTMEDIA_ENABLE_FFMPEG=${ffmpeg}, CRTMEDIA_ENABLE_CURL=${curl}): "
      "on this host the ordinary SDK publishes the Skia, FFmpeg and curl dependency closure only together, so a "
      "partial combination would install libraries whose dependencies the package does not carry "
      "(Skia companions without libskia.a, or FFmpeg-backed media without dependency records/link artifacts). "
      "Supported combinations: Skia, FFmpeg and curl all OFF (curl has no effect without FFmpeg), or "
      "Skia, FFmpeg and curl all ON. Reconfigure with one of them, or use the isolated 04-gfx-media "
      "stage chain. Other targets still build. Only these dist targets are refused.")
  endif()
  if(problem)
    string(JOIN "" problem ${problem})
  endif()
  set(${out} "${problem}" PARENT_SCOPE)
endfunction()

if(CMAKE_SCRIPT_MODE_FILE AND CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
  crt_media_dist_option_problem(crt_media_dist_problem
    "${CRTGFX_ENABLE_SKIA}" "${CRTMEDIA_ENABLE_FFMPEG}" "${CRTMEDIA_ENABLE_CURL}" "${CRT_MEDIA_DIST_PARTIAL}")
  if(crt_media_dist_problem)
    message(FATAL_ERROR "${crt_media_dist_problem}")
  endif()
endif()
