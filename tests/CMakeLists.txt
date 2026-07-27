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
if(APPLE)
  target_link_options(hello_c PRIVATE
    -Wl,-e,_start
  )
  target_link_libraries(hello_c PRIVATE System)
endif()
add_dependencies(hello_c c crt1)

add_test(NAME hello_c_runs COMMAND hello_c)
set_tests_properties(hello_c_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "Hello from freestanding CRT"
)
