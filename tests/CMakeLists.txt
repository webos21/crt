add_executable(hello_c
  hello.c
  $<TARGET_OBJECTS:crt1>
)
target_link_libraries(hello_c PRIVATE crt_build_flags c)
target_link_libraries(hello_c PRIVATE crt_compiler_rt_builtins)
target_link_options(hello_c PRIVATE
  -nostdlib
  -nostartfiles
  -nodefaultlibs
)
if(CRT_TARGET_OS STREQUAL "macos")
  target_link_options(hello_c PRIVATE
    -Wl,-e,_start
    -Wl,-dead_strip
  )
  target_link_libraries(hello_c PRIVATE System)
elseif(CRT_TARGET_OS STREQUAL "linux")
  target_link_options(hello_c PRIVATE
    -Wl,-e,_start
    -Wl,--gc-sections
  )
elseif(CRT_TARGET_OS STREQUAL "windows")
  target_link_options(hello_c PRIVATE
    -Wl,/entry:mainCRTStartup
    -Wl,/subsystem:console
    -Wl,/OPT:REF
  )
  target_link_libraries(hello_c PRIVATE "${CRT_WINDOWS_KERNEL32_LIB}")
endif()
add_dependencies(hello_c c crt1)

add_test(NAME hello_c_runs COMMAND hello_c)
set_tests_properties(hello_c_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "Hello from freestanding CRT"
)

function(add_crt_test test_name source_file)
  add_executable(${test_name}
    ${source_file}
    $<TARGET_OBJECTS:crt1>
  )
  target_link_libraries(${test_name} PRIVATE crt_build_flags c)
  target_link_libraries(${test_name} PRIVATE crt_compiler_rt_builtins)
  target_link_options(${test_name} PRIVATE
    -nostdlib
    -nostartfiles
    -nodefaultlibs
  )
  if(CRT_TARGET_OS STREQUAL "macos")
    target_link_options(${test_name} PRIVATE
      -Wl,-e,_start
      -Wl,-dead_strip
    )
    target_link_libraries(${test_name} PRIVATE System)
  elseif(CRT_TARGET_OS STREQUAL "linux")
    target_link_options(${test_name} PRIVATE
      -Wl,-e,_start
      -Wl,--gc-sections
    )
  elseif(CRT_TARGET_OS STREQUAL "windows")
    target_link_options(${test_name} PRIVATE
      -Wl,/entry:mainCRTStartup
      -Wl,/subsystem:console
      -Wl,/OPT:REF
    )
    target_link_libraries(${test_name} PRIVATE "${CRT_WINDOWS_KERNEL32_LIB}")
  endif()
  add_dependencies(${test_name} c crt1)
  add_test(NAME ${test_name}_runs COMMAND ${test_name})
endfunction()

add_crt_test(string_memory_test string_memory_test.c)
set_tests_properties(string_memory_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "string_memory_test: ok"
)
