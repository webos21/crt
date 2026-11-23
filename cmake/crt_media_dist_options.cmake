# Which Skia/FFmpeg/curl option combinations the ordinary 04-gfx-media (`crt-media-dist`) and
# 05-ui (`crt-ui-dist`) packages can describe honestly (PAL-04; docs/guides/distribution.md).
#
# The package publishes the Skia, FFmpeg and curl closure (link/runtime artifacts, notices,
# provenance, relink kit and source offer) only as one unit, and only when the in-tree build
# produced all of it. Anything in between would install libraries that need files the package
# does not carry (libcrtgfx_skia without libskia.a; a libcrtmedia.so whose DT_NEEDED libcurl is
# not copied), so it is rejected before the expensive build instead of after it.
#
#   Skia OFF, FFmpeg OFF (curl irrelevant: the HTTP transport is compiled only with FFmpeg)
#   Skia ON,  FFmpeg ON,  curl ON
#
# CMakeLists.txt applies it to Linux targets only. Use as a module:
# crt_media_dist_option_problem(<out-var> <skia> <ffmpeg> <curl>) sets the variable
# to an empty string for a supported combination, else to the reason. Run directly (`cmake
# -DCRTGFX_ENABLE_SKIA=.. -DCRTMEDIA_ENABLE_FFMPEG=.. -DCRTMEDIA_ENABLE_CURL=.. -P <this file>`) it
# fails with that reason, which is how tools/test_media_dist_options.py exercises it.
function(crt_media_dist_option_problem out skia ffmpeg curl)
  set(problem "")
  if(skia AND ffmpeg AND curl)
    # The full path: Skia, FFmpeg and curl are all published.
  elseif(NOT skia AND NOT ffmpeg)
    # The options-off path: nothing optional is published or installed.
  else()
    set(problem
      "crt-media-dist/crt-ui-dist cannot package this option combination "
      "(CRTGFX_ENABLE_SKIA=${skia}, CRTMEDIA_ENABLE_FFMPEG=${ffmpeg}, CRTMEDIA_ENABLE_CURL=${curl}): "
      "the ordinary SDK publishes the Skia, FFmpeg and curl dependency closure only together, so a "
      "partial combination would install libraries whose dependencies the package does not carry "
      "(Skia companions without libskia.a, or a libcrtmedia.so that needs an undeclared libcurl). "
      "Supported combinations: Skia, FFmpeg and curl all OFF (curl has no effect without FFmpeg), or "
      "Skia, FFmpeg and curl all ON. Reconfigure with one of them, or use the isolated 04-gfx-media "
      "stage chain. Other targets still build. Only these dist targets are refused.")
    string(JOIN "" problem ${problem})
  endif()
  set(${out} "${problem}" PARENT_SCOPE)
endfunction()

if(CMAKE_SCRIPT_MODE_FILE AND CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
  crt_media_dist_option_problem(crt_media_dist_problem
    "${CRTGFX_ENABLE_SKIA}" "${CRTMEDIA_ENABLE_FFMPEG}" "${CRTMEDIA_ENABLE_CURL}")
  if(crt_media_dist_problem)
    message(FATAL_ERROR "${crt_media_dist_problem}")
  endif()
endif()
