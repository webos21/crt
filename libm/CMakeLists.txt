add_library(m STATIC
  src/basic.c
  src/freebsd/s_ceil.c
  src/freebsd/s_floor.c
  src/freebsd/s_fmax.c
  src/freebsd/s_fmin.c
  src/freebsd/s_round.c
  src/freebsd/s_trunc.c
)

target_link_libraries(m PRIVATE crt_build_flags)
target_include_directories(m PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/freebsd")
target_compile_definitions(m PRIVATE USE_BUILTIN_FMAX USE_BUILTIN_FMIN)
set_target_properties(m PROPERTIES
  OUTPUT_NAME m
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

install(TARGETS m ARCHIVE DESTINATION lib)
