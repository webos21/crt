add_library(crtmedia STATIC
  src/common/runtime.c
)
target_link_libraries(crtmedia PRIVATE crt_build_flags c m dl cxx)
target_include_directories(crtmedia PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
set_target_properties(crtmedia PROPERTIES
  OUTPUT_NAME crtmedia
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

add_library(crtmedia_shared SHARED
  src/common/runtime.c
)
if(CRT_TARGET_OS STREQUAL "windows")
  target_sources(crtmedia_shared PRIVATE $<TARGET_OBJECTS:crt_windows_dllcrt>)
endif()
target_link_libraries(crtmedia_shared PRIVATE crt_build_flags c_shared m_shared dl_shared cxx_shared)
target_include_directories(crtmedia_shared PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
crt_configure_shared_runtime(crtmedia_shared)
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

install(TARGETS crtmedia crtmedia_shared
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include
  PATTERN ".gitkeep" EXCLUDE
)
