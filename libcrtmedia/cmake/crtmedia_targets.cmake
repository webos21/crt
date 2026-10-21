# Shared target-construction logic for crtmedia/crtmedia_shared. The in-tree
# build and the isolated 04-gfx-media project must use the same archive order,
# platform libraries, and shared-runtime policy.
#
# The caller provides CRTMEDIA_ROOT, CRTMEDIA_BACKEND_SOURCES,
# CRTMEDIA_ENABLE_FFMPEG, CRTMEDIA_FFMPEG_PORT_PREFIX,
# CRTMEDIA_FFMPEG_LIBRARIES, CRTMEDIA_ENABLE_CURL,
# CRTMEDIA_CURL_PORT_PREFIX, CRTMEDIA_CURL_LIBRARIES,
# CRTMEDIA_CURL_SHARED_LIBRARIES, CRTMEDIA_CRT_STATIC_LIBS,
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
    # capture_avfoundation.c alone drives AVFoundation/CoreVideo purely
    # through objc_msgSend from plain C (src/arch/macos/capture_
    # avfoundation.c's own top comment) -- the same real, intentional
    # function-pointer-cast pattern libcrtgfx/CMakeLists.txt already
    # suppresses this same Clang diagnostic for (window_cocoa.c/gpu_metal.c).
    # Shared here, not duplicated per caller (this exact block used to live
    # only in libcrtmedia/CMakeLists.txt, so every isolated stage's own
    # crtmedia_backend_objects never got it and failed to link with real
    # undefined _crtmedia_capture_backend_* symbols -- found for real
    # 2026-09-28 running the isolated 04-gfx-media stage on macOS for the
    # first time since this file was added), exactly like capture_mf.c
    # above.
    if(CRT_TARGET_OS STREQUAL "macos")
      foreach(crtmedia_backend_source IN LISTS crtmedia_backend_sources)
        if(crtmedia_backend_source MATCHES "/capture_avfoundation\\.c$")
          set_source_files_properties(${crtmedia_backend_source} PROPERTIES
            COMPILE_OPTIONS "-Wno-cast-function-type-mismatch;-Wno-unknown-warning-option")
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
    # Networking & Streaming Tranche 1 (docs/acceptance/crtmedia_networking_
    # acceptance.md): no socket/curl/TLS/FFmpeg dependency at all, so this
    # is always built, not gated behind CRTMEDIA_ENABLE_FFMPEG like demux.c/
    # extractor.c/codec.c/muxer.c below.
    "${CRTMEDIA_ROOT}/src/transport_queue.c"
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

  # GNU ld needs the mutually-dependent CRT, FFmpeg, and curl dependency
  # archives in one rescan group. In particular, libcurl.a can introduce a
  # late basename() reference that must rescan this project's libc.a rather
  # than leak through to the host libc. Other hosts retain their already-
  # verified plain ordering.
  if(CRT_TARGET_OS STREQUAL "linux" AND
     (CRTMEDIA_ENABLE_FFMPEG OR CRTMEDIA_ENABLE_CURL))
    target_link_libraries(crtmedia PRIVATE -Wl,--start-group)
    target_link_libraries(crtmedia PRIVATE ${CRTMEDIA_CRT_STATIC_LIBS})
    if(CRTMEDIA_ENABLE_FFMPEG)
      target_link_libraries(crtmedia PRIVATE ${CRTMEDIA_FFMPEG_LIBRARIES})
    endif()
    if(CRTMEDIA_ENABLE_CURL)
      target_link_libraries(crtmedia PRIVATE ${CRTMEDIA_CURL_LIBRARIES})
    endif()
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
    if(CRTMEDIA_ENABLE_CURL)
      target_link_libraries(crtmedia PRIVATE ${CRTMEDIA_CURL_LIBRARIES})
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
    "${CRTMEDIA_ROOT}/src/transport_queue.c"
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
    if(CRT_TARGET_OS STREQUAL "linux")
      # FFmpeg's static archives are linked into this shared object. On aarch64 its NEON assembly
      # (libavutil/aarch64/tx_float_neon.S) reaches global tables such as ff_tx_tab_*_float with
      # adrp+add, which ld refuses for a symbol that "may bind externally" in a shared object (x86_64
      # builds no FFmpeg assembly, so it never meets this). FFmpeg's own shared build hides these with
      # a version script, which a static archive never gets; -Bsymbolic binds the references to the
      # definitions in this same link instead. The isolated 04/05 stages already pass it globally
      # (distribution/stages/*/CMakeLists.txt); the in-tree build did not, so an option-ON in-tree
      # SDK could not be built on Linux/aarch64. See porting/recipes/ffmpeg.json (2026-09-11 note).
      target_link_options(crtmedia_shared PRIVATE "-Wl,-Bsymbolic")
    endif()

    # libcurl-backed HTTP transport (Networking & Streaming Tranche 2,
    # docs/acceptance/crtmedia_networking_acceptance.md) -- additive, only makes sense
    # together with FFmpeg's extractor (the custom AVIOContext this feeds).
    if(CRTMEDIA_ENABLE_CURL)
      set(CRTMEDIA_HTTP_SOURCES
        "${CRTMEDIA_ROOT}/src/http_transport.c"
        "${CRTMEDIA_ROOT}/src/http_avio.c"
        "${CRTMEDIA_ROOT}/src/http_upload.c"
      )
      # curl/curl.h #includes <winsock2.h> whenever _WIN32 is defined
      # (this PAL has no such header): the same -U_WIN32 family curl.json's
      # own http-roundtrip-static/-shared tests already need to compile
      # against these installed headers, routing curl's own public header
      # onto its portable-POSIX-host branch instead of native Windows.
      if(CRT_TARGET_OS STREQUAL "windows")
        set_source_files_properties(${CRTMEDIA_HTTP_SOURCES} PROPERTIES
          COMPILE_OPTIONS "-U_WIN32;-U_WIN32_WCE;-U__WIN32__;-UWIN32;-U__MINGW32__")
      endif()
      # extractor.c's own crtmedia_extractor_create_from_url() is gated on
      # this compile definition, not just CMake source selection -- it is
      # always *compiled* (unconditionally part of extractor.c), but only
      # *implemented* (rather than a plain CRTMEDIA_ERROR_UNSUPPORTED stub)
      # when this is defined, so extractor.c itself never needs a curl/
      # libavformat AVIOContext #include unless this whole feature is on.
      target_compile_definitions(crtmedia PRIVATE CRTMEDIA_HAVE_HTTP_TRANSPORT=1)
      target_compile_definitions(crtmedia_shared PRIVATE CRTMEDIA_HAVE_HTTP_TRANSPORT=1)
      target_sources(crtmedia PRIVATE ${CRTMEDIA_HTTP_SOURCES})
      target_include_directories(crtmedia PRIVATE "${CRTMEDIA_CURL_PORT_PREFIX}/include")
      target_sources(crtmedia_shared PRIVATE ${CRTMEDIA_HTTP_SOURCES})
      target_include_directories(crtmedia_shared PRIVATE "${CRTMEDIA_CURL_PORT_PREFIX}/include")
      if(CRT_TARGET_OS STREQUAL "linux")
        target_link_libraries(crtmedia_shared PRIVATE ${CRTMEDIA_CURL_SHARED_LIBRARIES})
      else()
        target_link_libraries(crtmedia_shared PRIVATE ${CRTMEDIA_CURL_LIBRARIES})
      endif()
    endif()
  endif()

  if(COMMAND crt_configure_shared_runtime)
    crt_configure_shared_runtime(crtmedia_shared)
  endif()
  set_target_properties(crtmedia_shared PROPERTIES
    OUTPUT_NAME crtmedia
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
  )
  if(CRT_TARGET_OS STREQUAL "windows")
    # Export the owned C API, not the thousands of FFmpeg/curl/GUID globals
    # pulled in from static archives. The same list drives in-tree and isolated
    # builds. Prefix filtering alone would also export private crtmedia_* helpers.
    set(_crtmedia_def "${CMAKE_CURRENT_BINARY_DIR}/crtmedia.def")
    add_custom_command(OUTPUT "${_crtmedia_def}"
      COMMAND "${CMAKE_COMMAND}" "-DNM=${CMAKE_NM}"
        "-DARCHIVE=$<TARGET_FILE:crtmedia>"
        "-DPUBLIC_SYMBOLS=${CRTMEDIA_ROOT}/cmake/crtmedia_public_symbols.txt"
        "-DOUTPUT=${_crtmedia_def}"
        -P "${CRTMEDIA_ROOT}/cmake/crtmedia_exports.cmake"
      DEPENDS crtmedia "${CRTMEDIA_ROOT}/cmake/crtmedia_public_symbols.txt"
        "${CRTMEDIA_ROOT}/cmake/crtmedia_exports.cmake"
      VERBATIM)
    target_sources(crtmedia_shared PRIVATE "${_crtmedia_def}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
      "${CRTMEDIA_ROOT}/cmake/crtmedia_public_symbols.txt")
    set_target_properties(crtmedia_shared PROPERTIES
      ARCHIVE_OUTPUT_NAME crtmedia_dll
    )
    install(FILES "${CRTMEDIA_ROOT}/cmake/crtmedia_public_symbols.txt"
      DESTINATION share/crt COMPONENT crt-media)
  endif()
  if(CRT_TARGET_OS STREQUAL "macos")
    # Mach-O exports every global by default (an ELF consumer is protected by link order and soname
    # rules, two-level namespace binds to the first dylib that defines the name): restrict to the API.
    # Two-level namespace binds an undefined symbol to the FIRST dylib on the link line that defines it.
    # CMake orders libc.dylib after the frameworks (its dependents come first), and the frameworks
    # re-export libSystem, so FFmpeg's pthread_once/pthread_mutex_* bound to libsystem_pthread: a Darwin
    # function writing into a Bionic-layout pthread_once_t (bus error in avcodec_find_decoder_by_name).
    # The CRT libc goes first, so every libc symbol of libcrtmedia is the CRT's.
    # An isolated stage has no c_shared target: the predecessor SDK's installed libc.dylib is the same file.
    if(TARGET c_shared)
      target_link_options(crtmedia_shared BEFORE PRIVATE "$<TARGET_FILE:c_shared>")
    else()
      target_link_options(crtmedia_shared BEFORE PRIVATE "${CMAKE_SYSROOT}/lib/libc.dylib")
    endif()
    set(_crtmedia_exports "${CRTMEDIA_ROOT}/cmake/crtmedia_exports_macos.txt")
    target_link_options(crtmedia_shared PRIVATE "LINKER:-exported_symbols_list,${_crtmedia_exports}")
    set_property(TARGET crtmedia_shared APPEND PROPERTY LINK_DEPENDS "${_crtmedia_exports}")
  endif()
endfunction()
