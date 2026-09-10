# Shared target-construction logic for crtmedia/crtmedia_shared. The in-tree
# build and the isolated 04-gfx-media project must use the same archive order,
# platform libraries, and shared-runtime policy.
#
# The caller provides CRTMEDIA_ROOT, CRTMEDIA_BACKEND_SOURCES,
# CRTMEDIA_ENABLE_FFMPEG, CRTMEDIA_FFMPEG_PORT_PREFIX,
# CRTMEDIA_FFMPEG_LIBRARIES, CRTMEDIA_CRT_STATIC_LIBS,
# CRTMEDIA_CRT_SHARED_LIBS, and the platform library variables used below.
function(crt_add_crtmedia_targets)
  set(CRTMEDIA_BACKEND_OBJECTS)
  if(CRTMEDIA_BACKEND_SOURCES)
    set(crtmedia_backend_sources ${CRTMEDIA_BACKEND_SOURCES})
    list(TRANSFORM crtmedia_backend_sources PREPEND "${CRTMEDIA_ROOT}/")
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
    )
  elseif(CRT_TARGET_OS STREQUAL "macos")
    target_link_libraries(crtmedia_shared PRIVATE ${CRTMEDIA_MACOS_FRAMEWORKS})
  endif()
  target_include_directories(crtmedia_shared PUBLIC "${CRTMEDIA_ROOT}/include")

  if(CRTMEDIA_ENABLE_FFMPEG)
    target_sources(crtmedia PRIVATE
      "${CRTMEDIA_ROOT}/src/demux.c"
      "${CRTMEDIA_ROOT}/src/extractor.c"
      "${CRTMEDIA_ROOT}/src/codec.c"
    )
    target_include_directories(crtmedia PRIVATE
      "${CRTMEDIA_FFMPEG_PORT_PREFIX}/include"
    )
    target_sources(crtmedia_shared PRIVATE
      "${CRTMEDIA_ROOT}/src/demux.c"
      "${CRTMEDIA_ROOT}/src/extractor.c"
      "${CRTMEDIA_ROOT}/src/codec.c"
    )
    target_include_directories(crtmedia_shared PRIVATE
      "${CRTMEDIA_FFMPEG_PORT_PREFIX}/include"
    )
    target_link_libraries(crtmedia_shared PRIVATE ${CRTMEDIA_FFMPEG_LIBRARIES})
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
