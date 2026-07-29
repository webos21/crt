add_library(cxx STATIC
  src/cxxabi.c
)

target_link_libraries(cxx PRIVATE crt_build_flags c)
target_include_directories(cxx PRIVATE
  "${CMAKE_SOURCE_DIR}/libc/include"
)
set_target_properties(cxx PROPERTIES
  OUTPUT_NAME c++
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

install(TARGETS cxx ARCHIVE DESTINATION lib)
