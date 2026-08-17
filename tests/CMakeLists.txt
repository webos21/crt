# CRT_STARTUP_OBJECTS (crt1.o, plus crt1_init_array.o's object on Linux,
# or crt1_ctors_begin.o+crt1_ctors_walker.o on Windows -- see
# libc/CMakeLists.txt's own comment) and CRT_STARTUP_END_OBJECTS (empty
# except on Windows, where crt1_ctors_end.o must be the last .ctors/
# .dtors contributor on the whole link line) are both set with
# PARENT_SCOPE by libc/CMakeLists.txt, which add_subdirectory(libc)
# always processes before this file. CRT_STARTUP_OBJECTS must come
# before the real source(s) here (not after, despite that being where it
# used to sit before CRT_STARTUP_END_OBJECTS existed) and
# CRT_STARTUP_END_OBJECTS after -- Linux/macOS don't care either way,
# but Windows' ctors_begin/end sentinels are strictly link-order-
# sensitive (see libc/src/arch/windows/common/ctors_begin.c).
add_executable(hello_c
  ${CRT_STARTUP_OBJECTS}
  hello.c
  ${CRT_STARTUP_END_OBJECTS}
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
    ${CRT_STARTUP_OBJECTS}
    ${source_file}
    ${CRT_TEST_EXTRA_SOURCES}
    ${CRT_STARTUP_END_OBJECTS}
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
    ${CRT_STARTUP_OBJECTS}
    ${source_file}
    ${CRT_STARTUP_END_OBJECTS}
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

add_crt_test(dev_zero_test dev_zero_test.c)
set_tests_properties(dev_zero_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "dev_zero_test: ok"
)

add_crt_test(virtual_proc_test virtual_proc_test.c)
set_tests_properties(virtual_proc_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "virtual_proc_test: ok"
  ENVIRONMENT "CRT_VIRTUAL_PROC_TEST_VAR=hello123"
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

add_crt_test(poll_pipe_write_end_test poll_pipe_write_end_test.c)
set_tests_properties(poll_pipe_write_end_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "poll_pipe_write_end_test: ok"
)

add_crt_test(termios_echo_roundtrip_test termios_echo_roundtrip_test.c)
set_tests_properties(termios_echo_roundtrip_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "termios_echo_roundtrip_test: (ok|skip)"
)

add_crt_test(termios_line_control_test termios_line_control_test.c)
set_tests_properties(termios_line_control_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "termios_line_control_test: (ok|skip)"
)

add_crt_test(socket_network_test socket_network_test.c)
set_tests_properties(socket_network_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "socket_network_test: ok"
)

add_crt_test(dns_resolve_test dns_resolve_test.c)
set_tests_properties(dns_resolve_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "dns_resolve_test: ok"
)

add_crt_test(dns_thread_test dns_thread_test.c)
set_tests_properties(dns_thread_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "dns_thread_test: ok"
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

# TODO.md's/docs/sysroot_ports.md's "Windows mksh subshell status quirk"
# item: a `(subshell); next_command` sequence lost the subshell's real
# exit status on Windows -- see mksh_subshell_status_test.c's own top
# comment for the full root cause (shell/mksh/src/exec.c's execute())
# and the fix.
add_crt_test(mksh_subshell_status_test mksh_subshell_status_test.c)
add_dependencies(mksh_subshell_status_test rootfs)
set_tests_properties(mksh_subshell_status_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "mksh_subshell_status_test: ok"
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  ENVIRONMENT "CRT_ROOTFS=${CRT_ROOTFS}"
)

# TODO.md's "Expand Windows shell smoke tests" item: real mksh
# interpreter-level coverage for fd 3+ redirection, `{ }` grouped
# commands, `&`/`wait` backgrounding, and autoconf-shaped subshell/
# redirection idioms -- see mksh_shell_smoke_test.c's own top comment.
add_crt_test(mksh_shell_smoke_test mksh_shell_smoke_test.c)
add_dependencies(mksh_shell_smoke_test rootfs)
set_tests_properties(mksh_shell_smoke_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "mksh_shell_smoke_test: ok"
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  ENVIRONMENT "CRT_ROOTFS=${CRT_ROOTFS};PATH=/system/bin:/bin:/usr/bin"
  TIMEOUT 30
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

add_crt_test(stdatomic_test stdatomic_test.c)
set_tests_properties(stdatomic_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "stdatomic_test: ok"
)

add_crt_test(semaphore_test semaphore_test.c)
set_tests_properties(semaphore_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "semaphore_test: ok"
)

add_crt_test(memfd_create_test memfd_create_test.c)
set_tests_properties(memfd_create_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "memfd_create_test: ok"
)

add_crt_test(sendmsg_scm_rights_test sendmsg_scm_rights_test.c)
set_tests_properties(sendmsg_scm_rights_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "sendmsg_scm_rights_test: ok"
)

add_crt_test(eventfd_test eventfd_test.c)
set_tests_properties(eventfd_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "eventfd_test: ok"
)

add_crt_test(timerfd_test timerfd_test.c)
set_tests_properties(timerfd_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "timerfd_test: ok"
)

add_crt_test(epoll_test epoll_test.c)
set_tests_properties(epoll_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "epoll_test: ok"
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

# TODO.md's "Windows shell/process stress hardening" item: many
# concurrently-live children sharing one inherited jobserver-style pipe,
# close-on-exec filtering under load, and a full waitpid(-1) drain -- see
# process_stress_test.c's own top comment for the full design and why it
# runs on every host, not just Windows.
add_crt_test(process_stress_test process_stress_test.c)
set_tests_properties(process_stress_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "process_stress_test: ok"
  TIMEOUT 60
)

# TODO.md's "Windows symlink/delete timing verification" item: a real,
# deterministic reproduction of the DeleteFileA()/CreateSymbolicLinkA()
# delete-pending race, and a regression for the retry loops
# __crt_sys_unlink()/__crt_sys_symlink() gained to fix it -- see
# windows_symlink_delete_race_test.c's own top comment for the full
# design and why it runs on every host.
add_crt_test(windows_symlink_delete_race_test windows_symlink_delete_race_test.c)
set_tests_properties(windows_symlink_delete_race_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "windows_symlink_delete_race_test: ok"
  TIMEOUT 60
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

add_crt_test(init_array_test init_array_test.c)
set_tests_properties(init_array_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "init_array_test: ok"
)

if(CRT_TARGET_OS STREQUAL "windows")
  add_executable(windows_export_hygiene_test
    ${CRT_STARTUP_OBJECTS}
    windows_export_hygiene_test.c
    ${CRT_STARTUP_END_OBJECTS}
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

  # Permanent regression test for libc/src/arch/windows/common/
  # pseudo_reloc.c's _pei386_runtime_relocator() -- see that file's own
  # comment and HISTORY.md's dated entry for the full story (found while
  # re-verifying the official port-test-libffi recipe test's shared
  # variant: linking against libffi.dll.a failed with "output image has
  # runtime pseudo relocations, but the function _pei386_runtime_
  # relocator is missing", since libffi's own ffi.h relies on GNU ld's
  # auto-import extension for its exported ffi_type_* globals rather
  # than an explicit dllimport annotation). This exercises the exact
  # same mechanism directly, without needing to download/build
  # libffi -- windows_pseudo_reloc_dll.c/consumer.c are small,
  # project-owned sources reproducing the same "plain global exported
  # from a DLL, referenced with no dllimport annotation" pattern.
  #
  # Deliberately NOT built through CMake's own native
  # add_executable()/add_library(SHARED) path: this project's CMake-
  # native builds default to the MSVC ABI (see the top-level
  # CMakeLists.txt's own comment), which has no auto-import/pseudo-
  # relocation concept at all -- an unannotated cross-DLL data
  # reference there is just a hard link error, not something this
  # mechanism could ever be exercised through. Only tools/crt-cc's GNU-
  # ABI (--target=*-w64-mingw32) path -- the same one every third-party
  # port build (xz, libffi, ...) uses -- can actually trigger this, so
  # this test builds through it directly via add_custom_command,
  # mirroring (a deliberately minimal, no-recipe-system-needed subset
  # of) how tools/crt-port-build.py invokes it for real port tests: the
  # same mksh.exe interpreter (tools/crt-cc has a #! shebang line,
  # which the OS itself can't run directly on Windows), the same
  # CRT_SYSROOT/CRT_TARGET_OS/CRT_TARGET_ARCH/CRT_WINDOWS_SYSTEM_LIBS
  # environment tools/crt-cc itself reads. CRT_TARGET_ARCH must be the
  # normalized "x86_64"/"aarch64" tools/crt-cc's own --target=*-w64-
  # mingw32 triple construction expects, not whatever raw
  # CRT_EFFECTIVE_ARCH_FOR_FLAGS happens to hold ("AMD64"/"ARM64" from
  # CMAKE_SYSTEM_PROCESSOR on a host build) -- tools/crt-cc uses
  # CRT_TARGET_ARCH completely verbatim, with no normalization of its
  # own, unlike crt-port-build.py (which does this same normalization
  # in Python before ever invoking crt-cc).
  if(CRT_EFFECTIVE_ARCH_FOR_FLAGS MATCHES "^(arm64|ARM64|aarch64)$")
    set(CRT_PSEUDO_RELOC_TEST_ARCH "aarch64")
  else()
    set(CRT_PSEUDO_RELOC_TEST_ARCH "x86_64")
  endif()
  # tools/crt-cc is a POSIX shell script that expands $libs (and thus
  # $CRT_WINDOWS_SYSTEM_LIBS, folded into it) UNQUOTED -- an embedded
  # space in a real Windows SDK path ("...\Windows Kits\..." /
  # "Program Files (x86)\...") silently word-splits into multiple,
  # broken arguments. tools/crt-port-build.py works around the identical
  # problem for every real port build via GetShortPathNameW()
  # (windows_short_path()) -- NOT reused here, deliberately: confirmed
  # directly that this breaks on at least one real CI runner, where NTFS
  # 8.3 short-name generation is disabled system-wide (a common Windows
  # Server/cloud-image hardening default: GetShortPathNameW() then just
  # returns the original, still-space-containing path unchanged, with no
  # error). Sidestepped entirely instead: copy just the two needed .lib
  # files into a build-tree-local directory at configure time (CMake's
  # own binary dir is never itself under a spaced path on any host this
  # project targets, unlike the fixed OS-controlled SDK install
  # location), then reference those copies.
  set(CRT_PSEUDO_RELOC_TEST_SDK_LIBDIR "${CMAKE_CURRENT_BINARY_DIR}/windows_sdk_libs")
  file(MAKE_DIRECTORY "${CRT_PSEUDO_RELOC_TEST_SDK_LIBDIR}")
  file(COPY "${CRT_WINDOWS_KERNEL32_LIB}" "${CRT_WINDOWS_SYNCHRONIZATION_LIB}"
    DESTINATION "${CRT_PSEUDO_RELOC_TEST_SDK_LIBDIR}"
  )
  get_filename_component(CRT_WINDOWS_KERNEL32_LIB_NAME "${CRT_WINDOWS_KERNEL32_LIB}" NAME)
  get_filename_component(CRT_WINDOWS_SYNCHRONIZATION_LIB_NAME "${CRT_WINDOWS_SYNCHRONIZATION_LIB}" NAME)
  set(CRT_WINDOWS_KERNEL32_LIB_SHORT "${CRT_PSEUDO_RELOC_TEST_SDK_LIBDIR}/${CRT_WINDOWS_KERNEL32_LIB_NAME}")
  set(CRT_WINDOWS_SYNCHRONIZATION_LIB_SHORT "${CRT_PSEUDO_RELOC_TEST_SDK_LIBDIR}/${CRT_WINDOWS_SYNCHRONIZATION_LIB_NAME}")
  # tools/crt-cc defaults CC to the bare name "clang", relying on PATH --
  # mksh's own rootfs environment doesn't carry this host's LLVM install
  # directory, so it must be set explicitly. Unlike $libs above, every
  # use of $cc in tools/crt-cc is individually double-quoted ("$cc"), so
  # POSIX word-splitting never applies to it regardless of embedded
  # spaces -- confirmed directly against a real verbose invocation trace
  # -- and it can safely be passed as-is, no copy/short-path needed.
  find_program(CRT_PSEUDO_RELOC_TEST_CLANG NAMES clang.exe clang)
  set(CRT_HOST_CC_SHORT "${CRT_PSEUDO_RELOC_TEST_CLANG}")
  set(CRT_PSEUDO_RELOC_TEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/windows_pseudo_reloc_test")
  set(CRT_PSEUDO_RELOC_TEST_DLL "${CRT_PSEUDO_RELOC_TEST_DIR}/libpseudoreloc_regress.dll")
  set(CRT_PSEUDO_RELOC_TEST_IMPLIB "${CRT_PSEUDO_RELOC_TEST_DIR}/libpseudoreloc_regress.dll.a")
  set(CRT_PSEUDO_RELOC_TEST_EXE "${CRT_PSEUDO_RELOC_TEST_DIR}/windows_pseudo_reloc_test.exe")
  set(CRT_PSEUDO_RELOC_TEST_ENV
    "${CMAKE_COMMAND}" -E env
    "CRT_SYSROOT=${CRT_SYSROOT}"
    "CRT_TARGET_OS=windows"
    "CRT_TARGET_ARCH=${CRT_PSEUDO_RELOC_TEST_ARCH}"
    "CRT_WINDOWS_SYSTEM_LIBS=-Wl,${CRT_WINDOWS_KERNEL32_LIB_SHORT} -Wl,${CRT_WINDOWS_SYNCHRONIZATION_LIB_SHORT}"
    "CRT_HOST_CC=${CRT_HOST_CC_SHORT}"
  )
  # DEPENDS below lists the "rootfs"/"sysroot" TARGETS directly (not just
  # via add_dependencies() on the aggregate target further down) --
  # deliberately, after a real CI failure this masked locally: this
  # session's own dev machine already had a fully-built out/ directory
  # (rootfs/sysroot both already up to date) from earlier work, so
  # add_dependencies() on the aggregate target alone was never actually
  # exercised against a genuinely clean tree. On a truly fresh checkout
  # (every CI run), that only orders "rootfs"/"sysroot" before the
  # AGGREGATE target's own completion -- NOT before these two sibling
  # add_custom_command()s specifically, which are independent DAG nodes
  # Ninja is otherwise free to run in parallel with (or before)
  # "rootfs"/"sysroot", racing the mksh.exe/CRT_SYSROOT/lib/* files they
  # actually consume. Listing the targets directly here closes that gap.
  add_custom_command(
    OUTPUT "${CRT_PSEUDO_RELOC_TEST_DLL}" "${CRT_PSEUDO_RELOC_TEST_IMPLIB}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CRT_PSEUDO_RELOC_TEST_DIR}"
    COMMAND ${CRT_PSEUDO_RELOC_TEST_ENV}
      "${CMAKE_BINARY_DIR}/rootfs/system/bin/mksh.exe" "${CMAKE_SOURCE_DIR}/tools/crt-cc"
      -shared "${CMAKE_CURRENT_SOURCE_DIR}/windows_pseudo_reloc_dll.c"
      -o "${CRT_PSEUDO_RELOC_TEST_DLL}"
      "-Wl,--out-implib,${CRT_PSEUDO_RELOC_TEST_IMPLIB}"
    DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/windows_pseudo_reloc_dll.c"
      "${CMAKE_SOURCE_DIR}/tools/crt-cc"
      rootfs sysroot
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Building windows_pseudo_reloc_test's regression DLL (GNU ABI, via tools/crt-cc)"
  )
  add_custom_command(
    OUTPUT "${CRT_PSEUDO_RELOC_TEST_EXE}"
    COMMAND ${CRT_PSEUDO_RELOC_TEST_ENV}
      "${CMAKE_BINARY_DIR}/rootfs/system/bin/mksh.exe" "${CMAKE_SOURCE_DIR}/tools/crt-cc"
      "${CMAKE_CURRENT_SOURCE_DIR}/windows_pseudo_reloc_consumer.c"
      "${CRT_PSEUDO_RELOC_TEST_IMPLIB}"
      -o "${CRT_PSEUDO_RELOC_TEST_EXE}"
    DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/windows_pseudo_reloc_consumer.c"
      "${CRT_PSEUDO_RELOC_TEST_DLL}" "${CRT_PSEUDO_RELOC_TEST_IMPLIB}"
      "${CMAKE_SOURCE_DIR}/tools/crt-cc"
      rootfs sysroot
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Building windows_pseudo_reloc_test's regression consumer (GNU ABI, via tools/crt-cc)"
  )
  add_custom_target(windows_pseudo_reloc_test ALL
    DEPENDS "${CRT_PSEUDO_RELOC_TEST_EXE}"
  )
  add_dependencies(windows_pseudo_reloc_test rootfs sysroot)
  add_test(NAME windows_pseudo_reloc_test_runs
    COMMAND "${CRT_PSEUDO_RELOC_TEST_EXE}"
  )
  set_tests_properties(windows_pseudo_reloc_test_runs PROPERTIES
    PASS_REGULAR_EXPRESSION "windows_pseudo_reloc_test: ok"
  )
  set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS windows_pseudo_reloc_test_runs)

  # Permanent regression test for tools/crt-cc's Windows shared-library
  # link order -- see that file's own Windows/shared_mode comment and
  # windows_dll_symbol_priority_dll.c/_middle.c/consumer.c's own
  # comments for the full story (found chasing an intermittent hang in
  # curl's own Windows port: libcurl-4.dll's own read() calls were
  # silently resolving to a stale, re-exported copy embedded in
  # libmbedcrypto.dll instead of this project's real, current c.lib,
  # because mbedtls's own import library appeared before c.lib on
  # curl's own link line). Three artifacts, chained DLL-to-DLL-to-EXE
  # to match the real bug's shape exactly (an EXE-to-DLL version of
  # this same fixture was tried first and did NOT reproduce the bug --
  # lld-link resolves that shape correctly regardless of link order;
  # only a DLL's *own* link, importing from another DLL that also
  # happens to export a same-named symbol, is affected):
  #   dll.c    ("mbedtls" stand-in)   -> libdllpriority_regress.dll
  #   middle.c ("libcurl" stand-in)   -> libdllpriority_middle.dll
  #                                      (links the fixture DLL's own
  #                                      import library)
  #   consumer.c (the test binary)   -> links only the middle DLL
  # Reuses the same tools/crt-cc-via-mksh.exe build technique (and the
  # same CRT_PSEUDO_RELOC_TEST_ENV/_ARCH/_SDK_LIBDIR-style setup
  # already established just above) rather than CMake's own native
  # add_library(SHARED)/add_executable() path, for the same reason:
  # this needs the real GNU-ABI (--target=*-w64-mingw32) shared-library
  # link tools/crt-cc drives for every actual port build, not this
  # project's own CMake-native MSVC-ABI default.
  #
  # The fixture DLL's own build below passes
  # "-Wl,--exclude-symbols=read"/"-Wl,--exclude-symbols=__crt_sys_read"
  # (one flag per symbol -- clang's "-Wl,A,B" splits on commas into
  # separate linker arguments, so a single comma-joined flag silently
  # mis-parses the second name as a file path) -- the same
  # --exclude-symbols mechanism porting/recipes/mbedtls.json's own
  # Windows LDFLAGS now applies to mbedtls's real DLLs (there via a
  # checked-in linker response file,
  # porting/recipes/mbedtls-windows-exclude-symbols.rsp, one flag per
  # real libc symbol -- the two flags spelled out directly here are
  # this fixture's own much smaller, hand-picked equivalent, not the
  # full list). This makes the permanent regression here about the fix
  # mechanism itself continuing to hold (lld-link's --exclude-symbols
  # keeps the fixture DLL from re-exporting read() at all, so the
  # middle DLL's own read() call can only resolve to the real libc
  # definition), not a standing demonstration of the raw bug -- the raw
  # bug is exercised once, by hand, when this fixture is changed.
  set(CRT_DLL_PRIORITY_TEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/windows_dll_symbol_priority_test")
  set(CRT_DLL_PRIORITY_TEST_DLL "${CRT_DLL_PRIORITY_TEST_DIR}/libdllpriority_regress.dll")
  set(CRT_DLL_PRIORITY_TEST_IMPLIB "${CRT_DLL_PRIORITY_TEST_DIR}/libdllpriority_regress.dll.a")
  set(CRT_DLL_PRIORITY_TEST_MIDDLE_DLL "${CRT_DLL_PRIORITY_TEST_DIR}/libdllpriority_middle.dll")
  set(CRT_DLL_PRIORITY_TEST_MIDDLE_IMPLIB "${CRT_DLL_PRIORITY_TEST_DIR}/libdllpriority_middle.dll.a")
  set(CRT_DLL_PRIORITY_TEST_EXE "${CRT_DLL_PRIORITY_TEST_DIR}/windows_dll_symbol_priority_test.exe")
  add_custom_command(
    OUTPUT "${CRT_DLL_PRIORITY_TEST_DLL}" "${CRT_DLL_PRIORITY_TEST_IMPLIB}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CRT_DLL_PRIORITY_TEST_DIR}"
    COMMAND ${CRT_PSEUDO_RELOC_TEST_ENV}
      "${CMAKE_BINARY_DIR}/rootfs/system/bin/mksh.exe" "${CMAKE_SOURCE_DIR}/tools/crt-cc"
      -shared "${CMAKE_CURRENT_SOURCE_DIR}/windows_dll_symbol_priority_dll.c"
      -o "${CRT_DLL_PRIORITY_TEST_DLL}"
      "-Wl,--exclude-symbols=read"
      "-Wl,--exclude-symbols=__crt_sys_read"
      "-Wl,--out-implib,${CRT_DLL_PRIORITY_TEST_IMPLIB}"
    DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/windows_dll_symbol_priority_dll.c"
      "${CMAKE_SOURCE_DIR}/tools/crt-cc"
      rootfs sysroot
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Building windows_dll_symbol_priority_test's regression DLL (GNU ABI, via tools/crt-cc)"
  )
  add_custom_command(
    OUTPUT "${CRT_DLL_PRIORITY_TEST_MIDDLE_DLL}" "${CRT_DLL_PRIORITY_TEST_MIDDLE_IMPLIB}"
    COMMAND ${CRT_PSEUDO_RELOC_TEST_ENV}
      "${CMAKE_BINARY_DIR}/rootfs/system/bin/mksh.exe" "${CMAKE_SOURCE_DIR}/tools/crt-cc"
      -shared "${CMAKE_CURRENT_SOURCE_DIR}/windows_dll_symbol_priority_middle.c"
      "${CRT_DLL_PRIORITY_TEST_IMPLIB}"
      -o "${CRT_DLL_PRIORITY_TEST_MIDDLE_DLL}"
      "-Wl,--out-implib,${CRT_DLL_PRIORITY_TEST_MIDDLE_IMPLIB}"
    DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/windows_dll_symbol_priority_middle.c"
      "${CRT_DLL_PRIORITY_TEST_DLL}" "${CRT_DLL_PRIORITY_TEST_IMPLIB}"
      "${CMAKE_SOURCE_DIR}/tools/crt-cc"
      rootfs sysroot
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Building windows_dll_symbol_priority_test's regression middle DLL (GNU ABI, via tools/crt-cc)"
  )
  add_custom_command(
    OUTPUT "${CRT_DLL_PRIORITY_TEST_EXE}"
    COMMAND ${CRT_PSEUDO_RELOC_TEST_ENV}
      "${CMAKE_BINARY_DIR}/rootfs/system/bin/mksh.exe" "${CMAKE_SOURCE_DIR}/tools/crt-cc"
      "${CMAKE_CURRENT_SOURCE_DIR}/windows_dll_symbol_priority_consumer.c"
      "${CRT_DLL_PRIORITY_TEST_MIDDLE_IMPLIB}"
      -o "${CRT_DLL_PRIORITY_TEST_EXE}"
    DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/windows_dll_symbol_priority_consumer.c"
      "${CRT_DLL_PRIORITY_TEST_MIDDLE_DLL}" "${CRT_DLL_PRIORITY_TEST_MIDDLE_IMPLIB}"
      "${CMAKE_SOURCE_DIR}/tools/crt-cc"
      rootfs sysroot
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Building windows_dll_symbol_priority_test's regression consumer (GNU ABI, via tools/crt-cc)"
  )
  add_custom_target(windows_dll_symbol_priority_test ALL
    DEPENDS "${CRT_DLL_PRIORITY_TEST_EXE}"
  )
  add_dependencies(windows_dll_symbol_priority_test rootfs sysroot)
  add_test(NAME windows_dll_symbol_priority_test_runs
    COMMAND "${CRT_DLL_PRIORITY_TEST_EXE}"
  )
  set_tests_properties(windows_dll_symbol_priority_test_runs PROPERTIES
    PASS_REGULAR_EXPRESSION "windows_dll_symbol_priority_test: ok"
  )
  set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS windows_dll_symbol_priority_test_runs)
endif()
get_property(CRT_REGISTERED_TESTS GLOBAL PROPERTY CRT_REGISTERED_TESTS)
list(LENGTH CRT_REGISTERED_TESTS CRT_REGISTERED_TEST_COUNT)
message(STATUS "CRT registered tests for ${CRT_TARGET_OS}/${CRT_TARGET_ARCH}: ${CRT_REGISTERED_TEST_COUNT}")
