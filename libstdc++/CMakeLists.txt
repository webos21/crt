add_library(cxx STATIC
  src/cxxabi.c
  src/msvcabi.c
  src/new_delete.cc
)

target_link_libraries(cxx PRIVATE crt_cxx_build_flags c)
target_include_directories(cxx PRIVATE
  "${CMAKE_SOURCE_DIR}/libc/include"
)
set_target_properties(cxx PROPERTIES
  OUTPUT_NAME c++
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

add_library(cxx_shared SHARED
  src/cxxabi.c
  src/msvcabi.c
  src/new_delete.cc
)
if(CRT_TARGET_OS STREQUAL "windows")
  target_sources(cxx_shared PRIVATE $<TARGET_OBJECTS:crt_windows_dllcrt>)
endif()

target_link_libraries(cxx_shared PRIVATE crt_cxx_build_flags c_shared)
target_include_directories(cxx_shared PRIVATE
  "${CMAKE_SOURCE_DIR}/libc/include"
)
crt_configure_shared_runtime(cxx_shared)
set_target_properties(cxx_shared PROPERTIES
  OUTPUT_NAME c++
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)
if(CRT_TARGET_OS STREQUAL "windows")
  set_target_properties(cxx_shared PROPERTIES
    ARCHIVE_OUTPUT_NAME c++_dll
  )
endif()

install(TARGETS cxx cxx_shared
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
