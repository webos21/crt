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

add_test(NAME crt_tiny_shell_condition_runs
  COMMAND crt_tiny_sh -c [=[false || echo fallback; true && echo ok; echo status=$?]=]
)
set_tests_properties(crt_tiny_shell_condition_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "fallback\nok\nstatus=0"
)
set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crt_tiny_shell_condition_runs)

add_test(NAME crt_tiny_shell_assignment_runs
  COMMAND crt_tiny_sh -c [=[CRT_TINY_VALUE=works; echo $CRT_TINY_VALUE]=]
)
set_tests_properties(crt_tiny_shell_assignment_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "works"
)
set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crt_tiny_shell_assignment_runs)

add_custom_target(crt_shell_artifacts
  DEPENDS crt_tiny_sh
  COMMENT "Building CRT bootstrap shell artifacts"
)

set(CRT_MKSH_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/mksh/src")
if(EXISTS "${CRT_MKSH_ROOT}/Android.bp")
  set(CRT_MKSH_SOURCES
    mksh/src/src/lalloc.c
    mksh/src/src/edit.c
    mksh/src/src/eval.c
    mksh/src/src/exec.c
    mksh/src/src/expr.c
    mksh/src/src/funcs.c
    mksh/src/src/histrap.c
    mksh/src/src/jobs.c
    mksh/src/src/lex.c
    mksh/src/src/main.c
    mksh/src/src/misc.c
    mksh/src/src/shf.c
    mksh/src/src/syn.c
    mksh/src/src/tree.c
    mksh/src/src/ulimit.c
    mksh/src/src/var.c
    $<TARGET_OBJECTS:crt1>
  )
  add_executable(crt_mksh ${CRT_MKSH_SOURCES})
  target_link_libraries(crt_mksh PRIVATE crt_build_flags c)
  target_link_libraries(crt_mksh PRIVATE crt_compiler_rt_builtins)
  target_include_directories(crt_mksh PRIVATE
    "${CMAKE_SOURCE_DIR}/libc/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/mksh/src/src"
  )
  target_compile_options(crt_mksh PRIVATE
    -Wno-deprecated-declarations
    -Wno-unknown-attributes
    -Wno-unused-parameter
    -fno-asynchronous-unwind-tables
    -fno-strict-aliasing
    -fwrapv
  )
  target_compile_definitions(crt_mksh PRIVATE
    __ANDROID__
    DEBUG_LEAKS
    MKSH_ASSUME_UTF8
    MKSH_DONT_EMIT_IDSTRING
    MKSH_NOPROSPECTOFWORK
    KSH_VERSIONNAME_VENDOR_EXT=" Android"
    MKSH_BUILDSH
    _GNU_SOURCE
    SETUID_CAN_FAIL_WITH_EAGAIN
    HAVE_STRING_POOLING=2
    HAVE_ATTRIBUTE_BOUNDED=1
    HAVE_ATTRIBUTE_FORMAT=1
    HAVE_ATTRIBUTE_NORETURN=1
    HAVE_ATTRIBUTE_PURE=1
    HAVE_ATTRIBUTE_UNUSED=1
    HAVE_ATTRIBUTE_USED=1
    HAVE_SYS_TIME_H=1
    HAVE_TIME_H=1
    HAVE_BOTH_TIME_H=1
    HAVE_SYS_BSDTYPES_H=0
    HAVE_SYS_FILE_H=1
    HAVE_SYS_MKDEV_H=0
    HAVE_SYS_MMAN_H=1
    HAVE_SYS_PARAM_H=1
    HAVE_SYS_RESOURCE_H=1
    HAVE_SYS_SELECT_H=1
    HAVE_SYS_SYSMACROS_H=1
    HAVE_BSTRING_H=0
    HAVE_GRP_H=1
    HAVE_IO_H=0
    HAVE_LIBGEN_H=1
    HAVE_LIBUTIL_H=0
    HAVE_PATHS_H=1
    HAVE_STDINT_H=1
    HAVE_STRINGS_H=1
    HAVE_TERMIOS_H=1
    HAVE_ULIMIT_H=0
    HAVE_VALUES_H=0
    HAVE_CAN_INTTYPES=1
    HAVE_CAN_UCBINTS=1
    HAVE_CAN_INT8TYPE=1
    HAVE_CAN_UCBINT8=1
    HAVE_SIG_T=1
    HAVE_SYS_ERRLIST=0
    HAVE_SYS_SIGNAME=1
    HAVE_SYS_SIGLIST=1
    HAVE_FLOCK=1
    HAVE_LOCK_FCNTL=1
    HAVE_RLIMIT=1
    HAVE_RLIM_T=1
    HAVE_GETRUSAGE=1
    HAVE_GETSID=1
    HAVE_GETTIMEOFDAY=1
    HAVE_KILLPG=1
    HAVE_MEMMOVE=1
    HAVE_MKNOD=0
    HAVE_MMAP=1
    HAVE_FTRUNCATE=1
    HAVE_NICE=1
    HAVE_REVOKE=0
    HAVE_SETLOCALE_CTYPE=1
    HAVE_LANGINFO_CODESET=1
    HAVE_SELECT=1
    HAVE_SETRESUGID=1
    HAVE_SETGROUPS=1
    HAVE_STRERROR=1
    HAVE_STRSIGNAL=1
    HAVE_STRLCPY=1
    HAVE_FLOCK_DECL=1
    HAVE_REVOKE_DECL=1
    HAVE_SYS_ERRLIST_DECL=0
    HAVE_SYS_SIGLIST_DECL=1
    HAVE_ST_MTIM=1
    HAVE_ST_MTIMENSEC=0
    HAVE_PERSISTENT_HISTORY=0
    MKSH_BUILD_R=593
    MKSH_DEFAULT_PROFILEDIR="/system/etc"
    MKSHRC_PATH="/system/etc/mkshrc"
    MKSH_DEFAULT_EXECSHELL="/system/bin/sh"
    MKSH_DEFAULT_TMPDIR="/data/local"
  )
  target_link_options(crt_mksh PRIVATE
    -nostdlib
    -nostartfiles
    -nodefaultlibs
  )
  if(CRT_TARGET_OS STREQUAL "macos")
    target_link_options(crt_mksh PRIVATE
      -Wl,-e,_start
      -Wl,-dead_strip
    )
    target_link_libraries(crt_mksh PRIVATE System)
  elseif(CRT_TARGET_OS STREQUAL "linux")
    target_link_options(crt_mksh PRIVATE
      -Wl,-e,_start
      -Wl,--gc-sections
    )
  elseif(CRT_TARGET_OS STREQUAL "windows")
    target_compile_definitions(crt_mksh PRIVATE
      MKSH_CRT_ALLOW_LLP64
      MKSH_CRT_SHELL_CHILD_SPEC
      MKSH_WITH_TEXTMODE
    )
    target_link_options(crt_mksh PRIVATE
      -Wl,/entry:mainCRTStartup
      -Wl,/subsystem:console
      -Wl,/OPT:REF
    )
    target_link_libraries(crt_mksh PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
  endif()
  add_dependencies(crt_mksh c crt1)
  set_target_properties(crt_mksh PROPERTIES
    OUTPUT_NAME mksh
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/shell"
  )
  add_test(NAME crt_mksh_basic_runs
    COMMAND crt_mksh -c "echo mksh-ok"
  )
  set_tests_properties(crt_mksh_basic_runs PROPERTIES
    PASS_REGULAR_EXPRESSION "mksh-ok"
  )
  set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crt_mksh_basic_runs)

  add_test(NAME crt_mksh_condition_runs
    COMMAND crt_mksh -c [=[x=ok; echo $x; false || echo fallback; true && echo done]=]
  )
  set_tests_properties(crt_mksh_condition_runs PROPERTIES
    PASS_REGULAR_EXPRESSION "ok\nfallback\ndone"
  )
  set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crt_mksh_condition_runs)

  add_dependencies(crt_shell_artifacts crt_mksh)
  install(TARGETS crt_mksh
    RUNTIME DESTINATION system/bin
  )
  install(FILES "${CRT_MKSH_ROOT}/mkshrc"
    DESTINATION system/etc
  )
endif()

set(CRT_TOYBOX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/toybox/src")
if(EXISTS "${CRT_TOYBOX_ROOT}/Android.bp")
  set(CRT_TOYBOX_SOURCES
    toybox/src/lib/args.c
    toybox/src/lib/commas.c
    toybox/src/lib/dirtree.c
    toybox/src/lib/elf.c
    toybox/src/lib/env.c
    toybox/src/lib/hash.c
    toybox/src/lib/lib.c
    toybox/src/lib/llist.c
    toybox/src/lib/net.c
    toybox/src/lib/portability.c
    toybox/src/lib/tty.c
    toybox/src/lib/utf8.c
    toybox/src/lib/xwrap.c
    toybox/src/main.c
    toybox/src/toys/lsb/hostname.c
    toybox/src/toys/lsb/md5sum.c
    toybox/src/toys/lsb/mktemp.c
    toybox/src/toys/lsb/seq.c
    toybox/src/toys/net/microcom.c
    toybox/src/toys/other/dos2unix.c
    toybox/src/toys/other/getopt.c
    toybox/src/toys/other/readlink.c
    toybox/src/toys/other/setsid.c
    toybox/src/toys/other/stat.c
    toybox/src/toys/other/timeout.c
    toybox/src/toys/other/truncate.c
    toybox/src/toys/other/which.c
    toybox/src/toys/other/xxd.c
    toybox/src/toys/other/yes.c
    toybox/src/toys/pending/diff.c
    toybox/src/toys/pending/expr.c
    toybox/src/toys/pending/tr.c
    toybox/src/toys/posix/basename.c
    toybox/src/toys/posix/cat.c
    toybox/src/toys/posix/chmod.c
    toybox/src/toys/posix/cmp.c
    toybox/src/toys/posix/comm.c
    toybox/src/toys/posix/cp.c
    toybox/src/toys/posix/cpio.c
    toybox/src/toys/posix/cut.c
    toybox/src/toys/posix/date.c
    toybox/src/toys/posix/dd.c
    toybox/src/toys/posix/dirname.c
    toybox/src/toys/posix/du.c
    toybox/src/toys/posix/echo.c
    toybox/src/toys/posix/env.c
    toybox/src/toys/posix/file.c
    toybox/src/toys/posix/false.c
    toybox/src/toys/posix/find.c
    toybox/src/toys/posix/getconf.c
    toybox/src/toys/posix/grep.c
    toybox/src/toys/posix/head.c
    toybox/src/toys/posix/id.c
    toybox/src/toys/posix/ln.c
    toybox/src/toys/posix/ls.c
    toybox/src/toys/posix/mkdir.c
    toybox/src/toys/posix/nl.c
    toybox/src/toys/posix/od.c
    toybox/src/toys/posix/paste.c
    toybox/src/toys/posix/patch.c
    toybox/src/toys/posix/printf.c
    toybox/src/toys/posix/pwd.c
    toybox/src/toys/posix/rm.c
    toybox/src/toys/posix/rmdir.c
    toybox/src/toys/posix/sed.c
    toybox/src/toys/posix/sleep.c
    toybox/src/toys/posix/sort.c
    toybox/src/toys/posix/tail.c
    toybox/src/toys/posix/tar.c
    toybox/src/toys/posix/tee.c
    toybox/src/toys/posix/test.c
    toybox/src/toys/posix/touch.c
    toybox/src/toys/posix/true.c
    toybox/src/toys/posix/uname.c
    toybox/src/toys/posix/uniq.c
    toybox/src/toys/posix/wc.c
    toybox/src/toys/posix/xargs.c
    $<TARGET_OBJECTS:crt1>
  )
  add_executable(crt_toybox ${CRT_TOYBOX_SOURCES})
  target_link_libraries(crt_toybox PRIVATE c m)
  target_link_libraries(crt_toybox PRIVATE crt_compiler_rt_builtins)
  target_include_directories(crt_toybox PRIVATE
    "${CMAKE_SOURCE_DIR}/libc/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/toybox/crt"
    "${CMAKE_CURRENT_SOURCE_DIR}/toybox/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/toybox/src/android/linux"
    "${CMAKE_SOURCE_DIR}/include"
  )
  target_compile_options(crt_toybox PRIVATE
    -ffreestanding
    -fno-builtin
    -ffunction-sections
    -fdata-sections
    $<$<C_COMPILER_ID:Clang>:SHELL:-Xclang -fwchar-type=int>
    -U__APPLE__
    -Wno-char-subscripts
    -Wno-deprecated-declarations
    -Wno-invalid-source-encoding
    -Wno-missing-field-initializers
    -Wno-pointer-arith
    -Wno-sign-compare
    -Wno-string-plus-int
    -Wno-unused-parameter
    -Wno-unused-variable
    -Wno-error=sign-compare
    -Wno-error=char-subscripts
    -Wno-error=string-plus-int
    -Wno-error=unused-function
    -Wno-error=unused-parameter
    -funsigned-char
    -fno-asynchronous-unwind-tables
    -fno-strict-aliasing
    -std=gnu99
  )
  target_compile_definitions(crt_toybox PRIVATE
    __BIONIC__
    __linux__
    TOYBOX_VENDOR="-crt"
  )
  target_link_options(crt_toybox PRIVATE
    -nostdlib
    -nostartfiles
    -nodefaultlibs
  )
  if(CRT_TARGET_OS STREQUAL "macos")
    target_link_options(crt_toybox PRIVATE
      -Wl,-e,_start
      -Wl,-dead_strip
    )
    target_link_libraries(crt_toybox PRIVATE System)
  elseif(CRT_TARGET_OS STREQUAL "linux")
    target_link_options(crt_toybox PRIVATE
      -Wl,-e,_start
      -Wl,--gc-sections
    )
  elseif(CRT_TARGET_OS STREQUAL "windows")
    target_link_options(crt_toybox PRIVATE
      -Wl,/entry:mainCRTStartup
      -Wl,/subsystem:console
      -Wl,/OPT:REF
    )
    target_link_libraries(crt_toybox PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
  endif()
  add_dependencies(crt_toybox c m crt1)
  set_target_properties(crt_toybox PROPERTIES
    OUTPUT_NAME toybox
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/shell"
  )
  add_test(NAME crt_toybox_basic_runs
    COMMAND crt_toybox echo toybox-ok
  )
  set_tests_properties(crt_toybox_basic_runs PROPERTIES
    PASS_REGULAR_EXPRESSION "toybox-ok"
  )
  set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crt_toybox_basic_runs)
  if(CRT_TARGET_OS STREQUAL "windows" AND TARGET crt_mksh)
    add_test(NAME crt_mksh_rootfs_external_runs
      COMMAND "${CMAKE_COMMAND}" -E env
        "CRT_ROOTFS=${CMAKE_BINARY_DIR}/rootfs"
        "PATH=/system/bin:/bin:/usr/bin"
        "$<TARGET_FILE:crt_mksh>" -c "ls -al /system/bin | head -n 2"
    )
    set_tests_properties(crt_mksh_rootfs_external_runs PROPERTIES
      PASS_REGULAR_EXPRESSION "total"
      DEPENDS crt_toybox_basic_runs
    )
    set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crt_mksh_rootfs_external_runs)

    add_test(NAME crt_mksh_rootfs_pipeline_runs
      COMMAND "${CMAKE_COMMAND}" -E env
        "CRT_ROOTFS=${CMAKE_BINARY_DIR}/rootfs"
        "PATH=/system/bin:/bin:/usr/bin"
        "$<TARGET_FILE:crt_mksh>" -c "echo hello | tr a-z A-Z"
    )
    set_tests_properties(crt_mksh_rootfs_pipeline_runs PROPERTIES
      PASS_REGULAR_EXPRESSION "HELLO"
      DEPENDS crt_mksh_rootfs_external_runs
    )
    set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crt_mksh_rootfs_pipeline_runs)

    add_test(NAME crt_mksh_rootfs_redirection_runs
      COMMAND "${CMAKE_COMMAND}" -E env
        "CRT_ROOTFS=${CMAKE_BINARY_DIR}/rootfs"
        "PATH=/system/bin:/bin:/usr/bin"
        "$<TARGET_FILE:crt_mksh>" -c "echo redir > /tmp/mksh-ctest-redir.txt; cat < /tmp/mksh-ctest-redir.txt"
    )
    set_tests_properties(crt_mksh_rootfs_redirection_runs PROPERTIES
      PASS_REGULAR_EXPRESSION "redir"
      DEPENDS crt_mksh_rootfs_pipeline_runs
    )
    set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crt_mksh_rootfs_redirection_runs)

    add_test(NAME crt_mksh_rootfs_command_substitution_runs
      COMMAND "${CMAKE_COMMAND}" -E env
        "CRT_ROOTFS=${CMAKE_BINARY_DIR}/rootfs"
        "PATH=/system/bin:/bin:/usr/bin"
        "$<TARGET_FILE:crt_mksh>" -c "echo subfile > /tmp/mksh-ctest-sub.txt; echo $(cat /tmp/mksh-ctest-sub.txt)"
    )
    set_tests_properties(crt_mksh_rootfs_command_substitution_runs PROPERTIES
      PASS_REGULAR_EXPRESSION "subfile"
      DEPENDS crt_mksh_rootfs_redirection_runs
    )
    set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crt_mksh_rootfs_command_substitution_runs)

    add_test(NAME crt_mksh_rootfs_exec_builtin_runs
      COMMAND "${CMAKE_COMMAND}" -E env
        "CRT_ROOTFS=${CMAKE_BINARY_DIR}/rootfs"
        "PATH=/system/bin:/bin:/usr/bin"
        "$<TARGET_FILE:crt_mksh>" -c "exec echo exec-ok"
    )
    set_tests_properties(crt_mksh_rootfs_exec_builtin_runs PROPERTIES
      PASS_REGULAR_EXPRESSION "exec-ok"
      DEPENDS crt_mksh_rootfs_command_substitution_runs
    )
    set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crt_mksh_rootfs_exec_builtin_runs)
  endif()
  add_dependencies(crt_shell_artifacts crt_toybox)
  install(TARGETS crt_toybox
    RUNTIME DESTINATION system/bin
  )
endif()

install(TARGETS crt_tiny_sh
  RUNTIME DESTINATION system/bin
)

add_custom_target(crt_shell_inventory
  COMMAND "${CMAKE_COMMAND}" -E echo "crt_tiny_sh is available. mksh/toybox source inventory is pending."
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Reporting CRT shell import inventory status"
)
