add_library(dl STATIC
  src/dl.c
)

target_link_libraries(dl PRIVATE crt_build_flags c)
set_target_properties(dl PROPERTIES
  OUTPUT_NAME dl
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

install(TARGETS dl ARCHIVE DESTINATION lib)
