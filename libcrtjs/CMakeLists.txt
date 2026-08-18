add_library(crtjs STATIC
  src/runtime.c
)
target_link_libraries(crtjs PRIVATE crt_build_flags c m dl cxx)
target_include_directories(crtjs PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
set_target_properties(crtjs PROPERTIES
  OUTPUT_NAME crtjs
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

add_library(crtjs_shared SHARED
  src/runtime.c
)
if(CRT_TARGET_OS STREQUAL "windows")
  target_sources(crtjs_shared PRIVATE $<TARGET_OBJECTS:crt_windows_dllcrt>)
endif()
target_link_libraries(crtjs_shared PRIVATE crt_build_flags c_shared m_shared dl_shared cxx_shared)
target_include_directories(crtjs_shared PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
crt_configure_shared_runtime(crtjs_shared)
set_target_properties(crtjs_shared PROPERTIES
  OUTPUT_NAME crtjs
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)
if(CRT_TARGET_OS STREQUAL "windows")
  set_target_properties(crtjs_shared PROPERTIES
    ARCHIVE_OUTPUT_NAME crtjs_dll
  )
endif()

install(TARGETS crtjs crtjs_shared
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include
  PATTERN ".gitkeep" EXCLUDE
)
