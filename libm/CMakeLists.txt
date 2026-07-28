add_library(m STATIC
  src/basic.c
)

target_link_libraries(m PRIVATE crt_build_flags)
set_target_properties(m PROPERTIES
  OUTPUT_NAME m
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

install(TARGETS m ARCHIVE DESTINATION lib)
