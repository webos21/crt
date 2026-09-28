# Shared target-construction logic for crtmedia/crtmedia_shared. The in-tree
# build and the isolated 04-gfx-media project must use the same archive order,
# platform libraries, and shared-runtime policy.
#
# The caller provides CRTMEDIA_ROOT, CRTMEDIA_BACKEND_SOURCES,
# CRTMEDIA_ENABLE_FFMPEG, CRTMEDIA_FFMPEG_PORT_PREFIX,
# CRTMEDIA_FFMPEG_LIBRARIES, CRTMEDIA_CRT_STATIC_LIBS,
# CRTMEDIA_CRT_SHARED_LIBS, CRTMEDIA_LINUX_VAAPI_LIBS, and the platform
# library variables used below.
function(crt_add_crtmedia_targets)
  set(CRTMEDIA_BACKEND_OBJECTS)
  if(CRTMEDIA_BACKEND_SOURCES)
    set(crtmedia_backend_sources ${CRTMEDIA_BACKEND_SOURCES})
    list(TRANSFORM crtmedia_backend_sources PREPEND "${CRTMEDIA_ROOT}/")
    # capture_v4l2.c alone consumes the host Linux UAPI. Isolated stages use
    # crt-cc -nostdinc, so opt this private boundary file into the real SDK;
    # public capture headers remain host-neutral.
    if(CRT_TARGET_OS STREQUAL "linux" AND CMAKE_C_COMPILER MATCHES "crt-cc")
      foreach(crtmedia_backend_source IN LISTS crtmedia_backend_sources)
        if(crtmedia_backend_source MATCHES "/capture_v4l2\\.c$")
          set_source_files_properties(${crtmedia_backend_source}
            PROPERTIES COMPILE_OPTIONS "-fcrt-real-linux-sdk")
        endif()
      endforeach()
    endif()
    # capture_mf.c alone #includes real mingw-w64 Media Foundation headers
    # instead of this library's own usual hand-declared-COM-vtable
    # convention (src/arch/windows/capture_mf.c's own top comment has the
    # full "why"). Needs its own win32_shim-first-then-real-headers include
    # order plus this project's own libc include dir (winnt.h reaches for
    # <ctype.h>, which mingw-w64-headers does not itself provide -- see that
    # file's own top comment) -- shared here, not duplicated per caller,
    # exactly like the Linux capture_v4l2.c case above, since both the
    # in-tree build and every isolated stage need the identical treatment,
    # just with different real path *values* for CRTMEDIA_WIN32_SHIM_ROOT/
    # CRTMEDIA_LIBC_INCLUDE_DIR (the caller's own job to set correctly).
    if(CRT_TARGET_OS STREQUAL "windows")
      foreach(crtmedia_backend_source IN LISTS crtmedia_backend_sources)
        if(crtmedia_backend_source MATCHES "/capture_mf\\.c$")
          if(NOT CRTMEDIA_WIN32_SHIM_ROOT OR NOT CRTMEDIA_LIBC_INCLUDE_DIR)
            message(FATAL_ERROR "crt_add_crtmedia_targets(): capture_mf.c requires CRTMEDIA_WIN32_SHIM_ROOT and CRTMEDIA_LIBC_INCLUDE_DIR")
          endif()
          set_source_files_properties(${crtmedia_backend_source} PROPERTIES
            COMPILE_OPTIONS
              "-I${CRTMEDIA_WIN32_SHIM_ROOT};-I${CRT_MINGW_W64_HEADERS_INCLUDE_ROOT};-I${CRTMEDIA_LIBC_INCLUDE_DIR};-include;${CRTMEDIA_WIN32_SHIM_ROOT}/mingw_w64_compat.h;-Wno-pragma-pack;-Wno-unused-value;-Wno-ignored-attributes;-Wno-extern-c-compat;-Wno-class-conversion"
          )
        endif()
      endforeach()
    endif()
    add_library(crtmedia_backend_objects OBJECT ${crtmedia_backend_sources})
    if((CRT_TARGET_OS STREQUAL "linux" OR CRT_TARGET_OS STREQUAL "macos") AND
       TARGET crt_build_flags)
      target_link_libraries(crtmedia_backend_objects PRIVATE crt_build_flags)
    endif()
    target_include_directories(crtmedia_backend_objects PUBLIC
      "${CRTMEDIA_ROOT}/include"
    )
    target_include_directories(crtmedia_backend_objects PRIVATE
      "${CRTMEDIA_ROOT}/src"
    )
    set_target_properties(crtmedia_backend_objects PROPERTIES
      POSITION_INDEPENDENT_CODE ON
    )
    if(CRT_TARGET_OS STREQUAL "linux" AND CRTMEDIA_ENABLE_FFMPEG)
      # V4L2 MJPEG-only webcams (2026-09-28, Encode and capture Tranche 2
      # real-hardware gate -- capture_v4l2_test_control.h's own CRTMEDIA_
      # V4L2_SOURCE_MJPEG comment has the full "why"): capture_v4l2.c's
      # MJPEG branch decodes through FFmpeg's own built-in avcodec MJPEG
      # decoder. crtmedia_backend_objects only needs the FFmpeg include
      # dir here -- the undefined avcodec_*/av_frame_*/av_packet_* symbols
      # this pulls in are resolved once this OBJECT library's outputs are
      # folded into the `crtmedia`/`crtmedia_shared` targets below, which
      # already link CRTMEDIA_FFMPEG_LIBRARIES when this flag is on.
      target_compile_definitions(crtmedia_backend_objects PRIVATE CRTMEDIA_CAPTURE_HAVE_MJPEG=1)
      target_include_directories(crtmedia_backend_objects PRIVATE "${CRTMEDIA_FFMPEG_PORT_PREFIX}/include")
    endif()
    set(CRTMEDIA_BACKEND_OBJECTS $<TARGET_OBJECTS:crtmedia_backend_objects>)
  endif()

  add_library(crtmedia STATIC
    "${CRTMEDIA_ROOT}/src/runtime.c"
    "${CRTMEDIA_ROOT}/src/frame.c"
    "${CRTMEDIA_ROOT}/src/frame_convert.c"
    "${CRTMEDIA_ROOT}/src/audio.c"
    "${CRTMEDIA_ROOT}/src/format.c"
    "${CRTMEDIA_ROOT}/src/player.c"
    "${CRTMEDIA_ROOT}/src/gpu_frame.c"
    "${CRTMEDIA_ROOT}/src/capture.c"
    ${CRTMEDIA_BACKEND_OBJECTS}
  )
  if(TARGET crt_build_flags)
    target_link_libraries(crtmedia PRIVATE crt_build_flags)
  endif()
  if(CRT_TARGET_OS STREQUAL "windows")
    target_link_libraries(crtmedia PUBLIC
      "${CRTMEDIA_WINDOWS_OLE32_LIB}"
      "${CRTMEDIA_WINDOWS_D3D11_LIB}"
      "${CRTMEDIA_WINDOWS_DXGI_LIB}"
      "${CRTMEDIA_WINDOWS_MFUUID_LIB}"
      "${CRTMEDIA_WINDOWS_STRMIIDS_LIB}"
      "${CRTMEDIA_WINDOWS_MFPLAT_LIB}"
      "${CRTMEDIA_WINDOWS_MFREADWRITE_LIB}"
      "${CRTMEDIA_WINDOWS_MF_LIB}"
      "${CRTMEDIA_WINDOWS_UUID_LIB}"
    )
  elseif(CRT_TARGET_OS STREQUAL "macos")
    target_link_libraries(crtmedia PUBLIC ${CRTMEDIA_MACOS_FRAMEWORKS})
  endif()

  # GNU ld needs the CRT and FFmpeg static archives in one rescan group.
  # Other hosts retain their already-verified plain ordering.
  if(CRT_TARGET_OS STREQUAL "linux" AND CRTMEDIA_ENABLE_FFMPEG)
    target_link_libraries(crtmedia PRIVATE -Wl,--start-group)
    target_link_libraries(crtmedia PRIVATE ${CRTMEDIA_CRT_STATIC_LIBS})
    target_link_libraries(crtmedia PRIVATE ${CRTMEDIA_FFMPEG_LIBRARIES})
    target_link_libraries(crtmedia PRIVATE -Wl,--end-group)
    # Real host .so's, not static archives -- no archive-member-selection
    # ordering concern the --start-group/--end-group rescan above exists
    # for, so linked plainly afterward (see CRTMEDIA_LINUX_VAAPI_LIBS's own
    # top comment, libcrtmedia/CMakeLists.txt).
    target_link_libraries(crtmedia PUBLIC ${CRTMEDIA_LINUX_VAAPI_LIBS})
  else()
    target_link_libraries(crtmedia PRIVATE ${CRTMEDIA_CRT_STATIC_LIBS})
    if(CRTMEDIA_ENABLE_FFMPEG)
      target_link_libraries(crtmedia PRIVATE ${CRTMEDIA_FFMPEG_LIBRARIES})
    endif()
  endif()
  target_include_directories(crtmedia PUBLIC "${CRTMEDIA_ROOT}/include")
  set_target_properties(crtmedia PROPERTIES
    OUTPUT_NAME crtmedia
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  )

  add_library(crtmedia_shared SHARED
    "${CRTMEDIA_ROOT}/src/runtime.c"
    "${CRTMEDIA_ROOT}/src/frame.c"
    "${CRTMEDIA_ROOT}/src/frame_convert.c"
    "${CRTMEDIA_ROOT}/src/audio.c"
    "${CRTMEDIA_ROOT}/src/format.c"
    "${CRTMEDIA_ROOT}/src/player.c"
    "${CRTMEDIA_ROOT}/src/gpu_frame.c"
    "${CRTMEDIA_ROOT}/src/capture.c"
    ${CRTMEDIA_BACKEND_OBJECTS}
  )
  if(CRT_TARGET_OS STREQUAL "windows" AND TARGET crt_windows_dllcrt)
    target_sources(crtmedia_shared PRIVATE $<TARGET_OBJECTS:crt_windows_dllcrt>)
  endif()
  if(TARGET crt_build_flags)
    target_link_libraries(crtmedia_shared PRIVATE crt_build_flags)
  endif()
  target_link_libraries(crtmedia_shared PRIVATE ${CRTMEDIA_CRT_SHARED_LIBS})
  if(CRT_TARGET_OS STREQUAL "windows")
    target_link_libraries(crtmedia_shared PRIVATE
      "${CRTMEDIA_WINDOWS_OLE32_LIB}"
      "${CRTMEDIA_WINDOWS_D3D11_LIB}"
      "${CRTMEDIA_WINDOWS_DXGI_LIB}"
      "${CRTMEDIA_WINDOWS_MFUUID_LIB}"
      "${CRTMEDIA_WINDOWS_STRMIIDS_LIB}"
      "${CRTMEDIA_WINDOWS_MFPLAT_LIB}"
      "${CRTMEDIA_WINDOWS_MFREADWRITE_LIB}"
      "${CRTMEDIA_WINDOWS_MF_LIB}"
      "${CRTMEDIA_WINDOWS_UUID_LIB}"
    )
  elseif(CRT_TARGET_OS STREQUAL "macos")
    target_link_libraries(crtmedia_shared PRIVATE ${CRTMEDIA_MACOS_FRAMEWORKS})
  endif()
  target_include_directories(crtmedia_shared PUBLIC "${CRTMEDIA_ROOT}/include")

  if(CRTMEDIA_ENABLE_FFMPEG)
    # Linux zero-copy (Tranche 3): the libva-header-using export source is
    # separate so -fcrt-real-linux-sdk (host /usr/include, lowest priority)
    # applies to this one file only.
    set(CRTMEDIA_LINUX_VAAPI_EXPORT_SOURCES)
    if(CRT_TARGET_OS STREQUAL "linux")
      set(CRTMEDIA_LINUX_VAAPI_EXPORT_SOURCES "${CRTMEDIA_ROOT}/src/gpu_frame_vaapi.c")
      # Only the isolated stages compile through tools/crt-cc (-nostdinc,
      # which understands the sentinel); the in-tree build uses plain clang,
      # which already sees /usr/include.
      if(CMAKE_C_COMPILER MATCHES "crt-cc")
        set_source_files_properties(${CRTMEDIA_LINUX_VAAPI_EXPORT_SOURCES}
          PROPERTIES COMPILE_OPTIONS "-fcrt-real-linux-sdk")
      endif()
    endif()
    target_sources(crtmedia PRIVATE
      "${CRTMEDIA_ROOT}/src/demux.c"
      "${CRTMEDIA_ROOT}/src/extractor.c"
      "${CRTMEDIA_ROOT}/src/codec.c"
      "${CRTMEDIA_ROOT}/src/muxer.c"
      ${CRTMEDIA_LINUX_VAAPI_EXPORT_SOURCES}
    )
    target_include_directories(crtmedia PRIVATE
      "${CRTMEDIA_FFMPEG_PORT_PREFIX}/include"
    )
    target_sources(crtmedia_shared PRIVATE
      "${CRTMEDIA_ROOT}/src/demux.c"
      "${CRTMEDIA_ROOT}/src/extractor.c"
      "${CRTMEDIA_ROOT}/src/codec.c"
      "${CRTMEDIA_ROOT}/src/muxer.c"
      ${CRTMEDIA_LINUX_VAAPI_EXPORT_SOURCES}
    )
    target_include_directories(crtmedia_shared PRIVATE
      "${CRTMEDIA_FFMPEG_PORT_PREFIX}/include"
    )
    target_link_libraries(crtmedia_shared PRIVATE ${CRTMEDIA_FFMPEG_LIBRARIES})
    target_link_libraries(crtmedia_shared PRIVATE ${CRTMEDIA_LINUX_VAAPI_LIBS})
  endif()

  if(COMMAND crt_configure_shared_runtime)
    crt_configure_shared_runtime(crtmedia_shared)
  endif()
  set_target_properties(crtmedia_shared PROPERTIES
    OUTPUT_NAME crtmedia
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    WINDOWS_EXPORT_ALL_SYMBOLS ON
  )
  if(CRT_TARGET_OS STREQUAL "windows")
    set_target_properties(crtmedia_shared PROPERTIES
      ARCHIVE_OUTPUT_NAME crtmedia_dll
    )
  endif()
endfunction()
