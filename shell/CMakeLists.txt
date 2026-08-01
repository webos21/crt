add_executable(crt_tiny_sh
  src/tiny_sh.c
  $<TARGET_OBJECTS:crt1>
)
target_link_libraries(crt_tiny_sh PRIVATE crt_build_flags c)
target_include_directories(crt_tiny_sh PRIVATE
  "${CMAKE_SOURCE_DIR}/libc/include"
)
target_link_libraries(crt_tiny_sh PRIVATE crt_compiler_rt_builtins)
target_link_options(crt_tiny_sh PRIVATE
  -nostdlib
  -nostartfiles
  -nodefaultlibs
)
if(CRT_TARGET_OS STREQUAL "macos")
  target_link_options(crt_tiny_sh PRIVATE
    -Wl,-e,_start
    -Wl,-dead_strip
  )
  target_link_libraries(crt_tiny_sh PRIVATE System)
elseif(CRT_TARGET_OS STREQUAL "linux")
  target_link_options(crt_tiny_sh PRIVATE
    -Wl,-e,_start
    -Wl,--gc-sections
  )
elseif(CRT_TARGET_OS STREQUAL "windows")
  target_link_options(crt_tiny_sh PRIVATE
    -Wl,/entry:mainCRTStartup
    -Wl,/subsystem:console
    -Wl,/OPT:REF
  )
  target_link_libraries(crt_tiny_sh PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
endif()
add_dependencies(crt_tiny_sh c crt1)
set_target_properties(crt_tiny_sh PROPERTIES
  OUTPUT_NAME sh
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/shell"
)

add_test(NAME crt_tiny_shell_pipeline_runs
  COMMAND crt_tiny_sh -c "echo tiny | upper"
)
set_tests_properties(crt_tiny_shell_pipeline_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "TINY"
)
set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crt_tiny_shell_pipeline_runs)

add_custom_target(crt_shell_artifacts
  DEPENDS crt_tiny_sh
  COMMENT "Building CRT bootstrap shell artifacts"
)

install(TARGETS crt_tiny_sh
  RUNTIME DESTINATION system/bin
)

add_custom_target(crt_shell_inventory
  COMMAND "${CMAKE_COMMAND}" -E echo "crt_tiny_sh is available. mksh/toybox source inventory is pending."
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Reporting CRT shell import inventory status"
)
