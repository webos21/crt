set(CRT_LIBM_SOURCES
  src/basic.c
  src/fenv.c
  src/long_double.c
  src/freebsd/e_fmod.c
  src/freebsd/e_fmodf.c
  src/freebsd/e_exp.c
  src/freebsd/e_expf.c
  src/freebsd/e_log.c
  src/freebsd/e_logf.c
  src/freebsd/e_log2.c
  src/freebsd/e_log2f.c
  src/freebsd/e_log10.c
  src/freebsd/e_log10f.c
  src/freebsd/e_pow.c
  src/freebsd/e_powf.c
  src/freebsd/k_cos.c
  src/freebsd/k_rem_pio2.c
  src/freebsd/k_sin.c
  src/freebsd/k_tan.c
  src/freebsd/e_remainder.c
  src/freebsd/e_remainderf.c
  src/freebsd/s_ceil.c
  src/freebsd/s_cos.c
  src/freebsd/s_expm1.c
  src/freebsd/s_expm1f.c
  src/freebsd/s_floor.c
  src/freebsd/s_fmax.c
  src/freebsd/s_fmin.c
  src/freebsd/s_frexp.c
  src/freebsd/s_frexpf.c
  src/freebsd/s_log1p.c
  src/freebsd/s_log1pf.c
  src/freebsd/s_modf.c
  src/freebsd/s_modff.c
  src/freebsd/s_remquo.c
  src/freebsd/s_remquof.c
  src/freebsd/s_round.c
  src/freebsd/s_scalbn.c
  src/freebsd/s_scalbnf.c
  src/freebsd/s_sin.c
  src/freebsd/s_sincosf.c
  src/freebsd/s_tan.c
  src/freebsd/s_tanf.c
  src/freebsd/s_trunc.c
)

add_library(m STATIC ${CRT_LIBM_SOURCES})
if(CRT_TARGET_OS STREQUAL "windows")
  target_sources(m PRIVATE $<TARGET_OBJECTS:crt_windows_compiler_abi>)
endif()
target_link_libraries(m PRIVATE crt_build_flags)
target_include_directories(m PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/freebsd")
set_target_properties(m PROPERTIES
  OUTPUT_NAME m
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

add_library(m_shared SHARED ${CRT_LIBM_SOURCES})
if(CRT_TARGET_OS STREQUAL "windows")
  target_sources(m_shared PRIVATE $<TARGET_OBJECTS:crt_windows_dllcrt>)
  target_sources(m_shared PRIVATE $<TARGET_OBJECTS:crt_windows_compiler_abi>)
endif()
target_link_libraries(m_shared PRIVATE crt_build_flags crt_compiler_rt_builtins)
target_include_directories(m_shared PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/freebsd")
crt_configure_shared_runtime(m_shared)
set_target_properties(m_shared PROPERTIES
  OUTPUT_NAME m
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)
if(CRT_TARGET_OS STREQUAL "windows")
  set_target_properties(m_shared PROPERTIES
    ARCHIVE_OUTPUT_NAME m_dll
  )
endif()

install(TARGETS m m_shared
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
