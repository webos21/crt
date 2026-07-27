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
  target_include_directories(${test_name} PRIVATE
    "${CMAKE_SOURCE_DIR}/libc/include"
  )
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
  set_tests_properties(${test_name}_runs PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  )
endfunction()

add_crt_test(string_memory_test string_memory_test.c)
set_tests_properties(string_memory_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "string_memory_test: ok"
)

add_crt_test(fd_errno_test fd_errno_test.c)
set_tests_properties(fd_errno_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "fd_errno_test: ok"
)

add_crt_test(malloc_test malloc_test.c)
set_tests_properties(malloc_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "malloc_test: ok"
)

add_crt_test(stdio_test stdio_test.c)
set_tests_properties(stdio_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "stdio_test: ok"
)

add_crt_test(stdio_file_test stdio_file_test.c)
set_tests_properties(stdio_file_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "stdio_file_test: ok"
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)

add_crt_test(printf_test printf_test.c)
set_tests_properties(printf_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "printf_test: ok 7 ff"
)

add_crt_test(mman_test mman_test.c)
set_tests_properties(mman_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "mman_test: ok"
)

add_crt_test(ctype_test ctype_test.c)
set_tests_properties(ctype_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "ctype_test: ok"
)

add_crt_test(stdlib_test stdlib_test.c)
set_tests_properties(stdlib_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "stdlib_test: ok"
)

add_crt_test(stdint_test stdint_test.c)
set_tests_properties(stdint_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "stdint_test: ok"
)

add_crt_test(stddef_stdarg_test stddef_stdarg_test.c)
set_tests_properties(stddef_stdarg_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "stddef_stdarg_test: ok"
)

add_crt_test(time_test time_test.c)
set_tests_properties(time_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "time_test: ok"
)

add_crt_test(sched_test sched_test.c)
set_tests_properties(sched_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "sched_test: ok"
)

add_crt_test(atomic_test atomic_test.c)
set_tests_properties(atomic_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "atomic_test: ok"
)
