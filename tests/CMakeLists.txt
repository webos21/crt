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
  target_link_libraries(hello_c PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
endif()
add_dependencies(hello_c c crt1)

add_test(NAME hello_c_runs COMMAND hello_c)
set_tests_properties(hello_c_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "Hello from freestanding CRT"
)
set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS hello_c_runs)

function(add_crt_test test_name source_file)
  set(CRT_TEST_EXTRA_SOURCES "")
  if(CRT_TARGET_OS STREQUAL "windows" AND CRT_EFFECTIVE_ARCH_FOR_FLAGS MATCHES "^(arm64|ARM64|aarch64|x86_64|AMD64|x64)$")
    # The ctest suite exercises fork() directly (fork_test, fork_signal_test,
    # fork_runtime_reset_test, ...) and needs the startup self-relaunch that
    # memory-copy fork() depends on -- opt in for every test binary rather
    # than auditing which ones call fork() transitively. See that file's own
    # top comment for why this is opt-in rather than linked into every
    # Windows binary.
    set(CRT_TEST_EXTRA_SOURCES "${CMAKE_SOURCE_DIR}/libc/src/arch/windows/common/fork_capable_relaunch.c")
  endif()
  add_executable(${test_name}
    ${source_file}
    ${CRT_TEST_EXTRA_SOURCES}
    $<TARGET_OBJECTS:crt1>
  )
  target_link_libraries(${test_name} PRIVATE crt_build_flags c)
  target_include_directories(${test_name} PRIVATE
    "${CMAKE_SOURCE_DIR}/libc/include"
  )
  if(ARGN)
    target_link_libraries(${test_name} PRIVATE ${ARGN})
  endif()
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
    target_link_libraries(${test_name} PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
  endif()
  add_dependencies(${test_name} c crt1)
  add_test(NAME ${test_name}_runs COMMAND ${test_name})
  set_tests_properties(${test_name}_runs PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  )
  set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS ${test_name}_runs)
endfunction()

function(add_crt_cxx_test test_name source_file)
  add_executable(${test_name}
    ${source_file}
    $<TARGET_OBJECTS:crt1>
  )
  target_link_libraries(${test_name} PRIVATE crt_cxx_build_flags cxx c)
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
    target_link_libraries(${test_name} PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
  endif()
  add_dependencies(${test_name} cxx c crt1)
  add_test(NAME ${test_name}_runs COMMAND ${test_name})
  set_tests_properties(${test_name}_runs PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  )
  set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS ${test_name}_runs)
endfunction()

add_crt_test(string_memory_test string_memory_test.c)
set_tests_properties(string_memory_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "string_memory_test: ok"
)

add_crt_test(regex_test regex_test.c)
set_tests_properties(regex_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "regex_test: ok"
)

add_crt_test(fd_errno_test fd_errno_test.c)
set_tests_properties(fd_errno_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "fd_errno_test: ok"
)

add_crt_test(malloc_test malloc_test.c)
set_tests_properties(malloc_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "malloc_test: ok"
)

add_crt_test(malloc_contention_test malloc_contention_test.c)
set_tests_properties(malloc_contention_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "malloc_contention_test: ok"
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

add_crt_test(stdio_memory_test stdio_memory_test.c)
set_tests_properties(stdio_memory_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "stdio_memory_test: ok"
)

add_crt_test(stdio_ext_test stdio_ext_test.c)
set_tests_properties(stdio_ext_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "stdio_ext_test: ok"
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)

add_crt_test(stdio_edge_test stdio_edge_test.c)
set_tests_properties(stdio_edge_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "stdio_edge_test: ok"
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)

add_crt_test(temp_name_test temp_name_test.c)
set_tests_properties(temp_name_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "temp_name_test: ok"
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)

add_crt_test(file_path_test file_path_test.c)
set_tests_properties(file_path_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "file_path_test: ok"
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)

add_crt_test(sysconf_test sysconf_test.c)
set_tests_properties(sysconf_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "sysconf_test: ok"
)

add_crt_test(ioctl_test ioctl_test.c)
set_tests_properties(ioctl_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "ioctl_test: ok"
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)

add_crt_test(poll_select_test poll_select_test.c)
set_tests_properties(poll_select_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "poll_select_test: ok"
)

add_crt_test(socket_network_test socket_network_test.c)
set_tests_properties(socket_network_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "socket_network_test: ok"
)

add_crt_test(dl_test dl_test.c)
target_link_libraries(dl_test PRIVATE dl)
add_dependencies(dl_test dl)
set_tests_properties(dl_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "dl_test: ok"
)

add_crt_test(cxx_runtime_test cxx_runtime_test.c)
target_link_libraries(cxx_runtime_test PRIVATE cxx)
add_dependencies(cxx_runtime_test cxx)
set_tests_properties(cxx_runtime_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "cxx_runtime_test: ok"
)

add_crt_cxx_test(cxx_frontend_test cxx_frontend_test.cc)
set_tests_properties(cxx_frontend_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "cxx_frontend_test: ok"
)

add_crt_test(printf_test printf_test.c m)
add_dependencies(printf_test m)
set_tests_properties(printf_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "printf_test: ok 7 ff"
)

add_crt_test(scanf_test scanf_test.c)
set_tests_properties(scanf_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "scanf_test: ok"
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)

add_crt_test(scanf_edge_matrix_test scanf_edge_matrix_test.c)
set_tests_properties(scanf_edge_matrix_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "scanf_edge_matrix_test: ok"
)

add_crt_test(mman_test mman_test.c)
set_tests_properties(mman_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "mman_test: ok"
)

add_crt_test(math_test math_test.c m)
add_dependencies(math_test m)
set_tests_properties(math_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "math_test: ok"
)

add_crt_test(ctype_test ctype_test.c)
set_tests_properties(ctype_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "ctype_test: ok"
)

add_crt_test(locale_test locale_test.c)
set_tests_properties(locale_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "locale_test: ok"
)

add_crt_test(wchar_mbstate_test wchar_mbstate_test.c)
set_tests_properties(wchar_mbstate_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "wchar_mbstate_test: ok"
)

add_crt_test(stdlib_test stdlib_test.c)
set_tests_properties(stdlib_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "stdlib_test: ok"
)

add_crt_test(setjmp_test setjmp_test.c)
set_tests_properties(setjmp_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "setjmp_test: ok"
)

add_crt_test(stdlib_string_extra_test stdlib_string_extra_test.c)
set_tests_properties(stdlib_string_extra_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "stdlib_string_extra_test: ok"
  ENVIRONMENT "CRT_HOST_ENV_TEST=visible"
)

add_crt_test(stdint_test stdint_test.c)
set_tests_properties(stdint_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "stdint_test: ok"
)

add_crt_test(data_model_test data_model_test.c)
set_tests_properties(data_model_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "data_model_test: ok"
)

add_crt_test(header_abi_test header_abi_test.c)
set_tests_properties(header_abi_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "header_abi_test: ok"
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

add_crt_test(process_signal_test process_signal_test.c)
set_tests_properties(process_signal_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "process_signal_test: ok"
)

add_crt_test(job_control_test job_control_test.c)
set_tests_properties(job_control_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "job_control_test: ok"
)

add_crt_test(rootfs_process_test rootfs_process_test.c)
set_tests_properties(rootfs_process_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "rootfs_process_test: ok"
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)

add_crt_test(shell_smoke_test shell_smoke_test.c)
set_tests_properties(shell_smoke_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "shell_smoke_test: ok"
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)

add_crt_test(mntent_test mntent_test.c)
set_tests_properties(mntent_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "mntent_test: ok"
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)

add_crt_test(atomic_test atomic_test.c)
set_tests_properties(atomic_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "atomic_test: ok"
)

add_crt_test(wait_test wait_test.c)
set_tests_properties(wait_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "wait_test: ok"
)

add_crt_test(fork_test fork_test.c)
set_tests_properties(fork_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "fork_test: ok"
)

add_crt_test(fork_signal_test fork_signal_test.c)
set_tests_properties(fork_signal_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "fork_signal_test: ok"
)

add_crt_test(pselect_sigchld_test pselect_sigchld_test.c)
set_tests_properties(pselect_sigchld_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "pselect_sigchld_test: ok"
  # The scenario under test is a real fork() + blocked-SIGCHLD +
  # pselect() lost-wakeup pattern (docs/signal_delivery.md); the test
  # itself uses a bounded 5s pselect() timeout so a regression fails
  # fast instead of hanging, but this TIMEOUT is the outer safety net.
  TIMEOUT 30
)

add_crt_test(fork_runtime_reset_test fork_runtime_reset_test.c)
set_tests_properties(fork_runtime_reset_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "fork_runtime_reset_test: ok"
)

add_crt_test(windows_fd_snapshot_test windows_fd_snapshot_test.c)
set_tests_properties(windows_fd_snapshot_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "windows_fd_snapshot_test: ok"
)

add_crt_test(pthread_basic_test pthread_basic_test.c)
set_tests_properties(pthread_basic_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "pthread_basic_test: ok"
)

add_crt_test(pthread_once_contention_test pthread_once_contention_test.c)
set_tests_properties(pthread_once_contention_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "pthread_once_contention_test: ok"
)

add_crt_test(pthread_mutexattr_test pthread_mutexattr_test.c)
set_tests_properties(pthread_mutexattr_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "pthread_mutexattr_test: ok"
)

add_crt_test(pthread_mutex_contention_test pthread_mutex_contention_test.c)
set_tests_properties(pthread_mutex_contention_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "pthread_mutex_contention_test: ok"
)

add_crt_test(pthread_rwlock_test pthread_rwlock_test.c)
set_tests_properties(pthread_rwlock_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "pthread_rwlock_test: ok"
)

add_crt_test(pthread_rwlock_contention_test pthread_rwlock_contention_test.c)
set_tests_properties(pthread_rwlock_contention_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "pthread_rwlock_contention_test: ok"
)

add_crt_test(pthread_spin_test pthread_spin_test.c)
set_tests_properties(pthread_spin_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "pthread_spin_test: ok"
)

add_crt_test(pthread_barrier_test pthread_barrier_test.c)
set_tests_properties(pthread_barrier_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "pthread_barrier_test: ok"
)

add_crt_test(pthread_policy_test pthread_policy_test.c)
set_tests_properties(pthread_policy_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "pthread_policy_test: ok"
)

add_crt_test(pthread_bionic_api_test pthread_bionic_api_test.c)
set_tests_properties(pthread_bionic_api_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "pthread_bionic_api_test: ok"
)

add_crt_test(pthread_cond_test pthread_cond_test.c)
set_tests_properties(pthread_cond_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "pthread_cond_test: ok"
)

add_crt_test(pthread_key_test pthread_key_test.c)
set_tests_properties(pthread_key_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "pthread_key_test: ok"
)

add_crt_test(pthread_tls_test pthread_tls_test.c)
set_tests_properties(pthread_tls_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "pthread_tls_test: ok"
)

add_crt_test(pthread_key_destructor_test pthread_key_destructor_test.c)
set_tests_properties(pthread_key_destructor_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "pthread_key_destructor_test: ok"
)

add_crt_test(pthread_thread_test pthread_thread_test.c)
set_tests_properties(pthread_thread_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "pthread_thread_test: ok"
)

add_crt_test(pthread_attr_test pthread_attr_test.c)
set_tests_properties(pthread_attr_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "pthread_attr_test: ok"
)

add_crt_test(pthread_detach_test pthread_detach_test.c)
set_tests_properties(pthread_detach_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "pthread_detach_test: ok"
)

add_crt_test(pthread_type_test pthread_type_test.c)
set_tests_properties(pthread_type_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "pthread_type_test: ok"
)

if(CRT_TARGET_OS STREQUAL "windows")
  add_executable(windows_export_hygiene_test
    windows_export_hygiene_test.c
    $<TARGET_OBJECTS:crt1>
  )
  target_link_libraries(windows_export_hygiene_test PRIVATE crt_build_flags c)
  target_link_libraries(windows_export_hygiene_test PRIVATE crt_compiler_rt_builtins)
  target_link_options(windows_export_hygiene_test PRIVATE
    -nostdlib
    -nostartfiles
    -nodefaultlibs
    -Wl,/entry:mainCRTStartup
    -Wl,/subsystem:console
    -Wl,/OPT:REF
  )
  target_link_libraries(windows_export_hygiene_test PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
  add_dependencies(windows_export_hygiene_test c crt1 c_shared m_shared dl_shared cxx_shared)
  add_test(NAME windows_export_hygiene_runs
    COMMAND windows_export_hygiene_test
      "$<TARGET_FILE:c_shared>"
      "$<TARGET_FILE:m_shared>"
      "$<TARGET_FILE:dl_shared>"
      "$<TARGET_FILE:cxx_shared>"
  )
  set_tests_properties(windows_export_hygiene_runs PROPERTIES
    PASS_REGULAR_EXPRESSION "windows_export_hygiene_test: ok"
  )
  set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS windows_export_hygiene_runs)
endif()
get_property(CRT_REGISTERED_TESTS GLOBAL PROPERTY CRT_REGISTERED_TESTS)
list(LENGTH CRT_REGISTERED_TESTS CRT_REGISTERED_TEST_COUNT)
message(STATUS "CRT registered tests for ${CRT_TARGET_OS}/${CRT_TARGET_ARCH}: ${CRT_REGISTERED_TEST_COUNT}")
