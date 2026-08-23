# CRT_STARTUP_END_OBJECTS deliberately not listed here -- see
# crt_link_startup_end_objects()'s own comment (top-level CMakeLists.txt).
add_executable(crt_tiny_sh
  ${CRT_STARTUP_OBJECTS}
  tiny_sh/tiny_sh.c
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
crt_link_startup_end_objects(crt_tiny_sh)
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

set(CRT_MKSH_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/mksh")
if(EXISTS "${CRT_MKSH_ROOT}/Android.bp")
  set(CRT_MKSH_SOURCES
    ${CRT_STARTUP_OBJECTS}
    mksh/src/lalloc.c
    mksh/src/edit.c
    mksh/src/eval.c
    mksh/src/exec.c
    mksh/src/expr.c
    mksh/src/funcs.c
    mksh/src/histrap.c
    mksh/src/jobs.c
    mksh/src/lex.c
    mksh/src/main.c
    mksh/src/misc.c
    mksh/src/shf.c
    mksh/src/syn.c
    mksh/src/tree.c
    mksh/src/ulimit.c
    mksh/src/var.c
  )
  if(CRT_TARGET_OS STREQUAL "windows" AND CRT_EFFECTIVE_ARCH_FOR_FLAGS MATCHES "^(arm64|ARM64|aarch64|x86_64|AMD64|x64)$")
    # mksh is the one CRT-built program that actually calls fork() (real
    # subshells, command substitution) -- opts into the startup self-
    # relaunch that memory-copy fork() needs. See that file's own top
    # comment for why this is opt-in rather than linked into every
    # Windows binary.
    list(APPEND CRT_MKSH_SOURCES "${CMAKE_SOURCE_DIR}/libc/src/arch/windows/common/fork_capable_relaunch.c")
  endif()
  # CRT_STARTUP_END_OBJECTS deliberately not appended here -- see
  # crt_link_startup_end_objects()'s own comment (top-level CMakeLists.txt)
  # and this target's own crt_link_startup_end_objects() call below.
  add_executable(crt_mksh ${CRT_MKSH_SOURCES})
  target_link_libraries(crt_mksh PRIVATE crt_build_flags c)
  target_link_libraries(crt_mksh PRIVATE crt_compiler_rt_builtins)
  target_include_directories(crt_mksh PRIVATE
    "${CMAKE_SOURCE_DIR}/libc/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/mksh/src"
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
      MKSH_CRT_WINPATH
      MKSH_WITH_TEXTMODE
    )
    target_link_options(crt_mksh PRIVATE
      -Wl,/entry:mainCRTStartup
      -Wl,/subsystem:console
      -Wl,/OPT:REF
    )
    target_link_libraries(crt_mksh PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
  endif()
  crt_link_startup_end_objects(crt_mksh)
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
    ${CRT_STARTUP_OBJECTS}
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
    toybox/src/toys/pending/stty.c
    toybox/src/toys/pending/tr.c
    toybox/src/toys/posix/basename.c
    toybox/src/toys/posix/cat.c
    toybox/src/toys/posix/chmod.c
    toybox/src/toys/posix/cksum.c
    toybox/src/toys/posix/cmp.c
    toybox/src/toys/posix/comm.c
    toybox/src/toys/posix/cp.c
    toybox/src/toys/posix/cpio.c
    toybox/src/toys/posix/cut.c
    toybox/src/toys/posix/date.c
    toybox/src/toys/posix/dd.c
    toybox/src/toys/posix/df.c
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
    toybox/src/toys/posix/link.c
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
    toybox/src/toys/posix/tsort.c
    toybox/src/toys/posix/tty.c
    toybox/src/toys/posix/uname.c
    toybox/src/toys/posix/uniq.c
    toybox/src/toys/posix/unlink.c
    toybox/src/toys/posix/uuencode.c
    toybox/src/toys/posix/wc.c
    toybox/src/toys/posix/xargs.c
  )
  if(CRT_TARGET_OS STREQUAL "windows" AND CRT_EFFECTIVE_ARCH_FOR_FLAGS MATCHES "^(arm64|ARM64|aarch64|x86_64|AMD64|x64)$")
    # xargs (and potentially other applets later) calls fork()/vfork() to run
    # each batch of arguments -- opts into the startup self-relaunch that
    # memory-copy fork() depends on, same as crt_mksh above and the ctest
    # suite. Without it, every fork() call from any toybox applet fails with
    # "fork_memcopy: stack commit failed" (confirmed hitting this directly:
    # `xargs` crashed this way, and libpng's own `configure` silently
    # swallowed the crash output from its "checking xargs -n works" probe).
    # See that file's own top comment for why this is opt-in rather than
    # linked into every Windows binary.
    list(APPEND CRT_TOYBOX_SOURCES "${CMAKE_SOURCE_DIR}/libc/src/arch/windows/common/fork_capable_relaunch.c")
  endif()
  # CRT_STARTUP_END_OBJECTS deliberately not appended here -- see
  # crt_link_startup_end_objects()'s own comment (top-level CMakeLists.txt)
  # and this target's own crt_link_startup_end_objects() call below.
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
  crt_link_startup_end_objects(crt_toybox)
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

    # Regression coverage for which/readlink/stat: toybox source for these
    # was in CRT_TOYBOX_SOURCES above for a while before anything actually
    # registered them as runnable applets (no shell/toybox/crt/generated/
    # newtoys.h entry, no rootfs alias) -- `which` additionally exposed a
    # real getcwd(NULL, 0) bug once it was reachable (libc/src/fd.c never
    # supported the POSIX.1-2008/GNU "allocate a buffer automatically"
    # form Bionic's own getcwd() does; see that fix's own comment).
    add_test(NAME crt_mksh_rootfs_which_stat_readlink_runs
      COMMAND "${CMAKE_COMMAND}" -E env
        "CRT_ROOTFS=${CMAKE_BINARY_DIR}/rootfs"
        "PATH=/system/bin:/bin:/usr/bin"
        "$<TARGET_FILE:crt_mksh>" -c "which ls; echo wsr-target > /tmp/mksh-ctest-wsr-target.txt; ln -sf /tmp/mksh-ctest-wsr-target.txt /tmp/mksh-ctest-wsr-link.txt; readlink /tmp/mksh-ctest-wsr-link.txt; stat /tmp/mksh-ctest-wsr-target.txt | head -n 1; rm -f /tmp/mksh-ctest-wsr-link.txt /tmp/mksh-ctest-wsr-target.txt"
    )
    set_tests_properties(crt_mksh_rootfs_which_stat_readlink_runs PROPERTIES
      # readlink's own output is whatever real (host-translated, absolute)
      # target string this PAL's symlink() actually stored -- not
      # necessarily byte-identical to the guest-relative string `ln -s`
      # was given -- so this only pins down that it ends in the expected
      # filename, not the exact translated form.
      PASS_REGULAR_EXPRESSION "/system/bin/ls\n[^\n]*mksh-ctest-wsr-target\\.txt\n  File: /tmp/mksh-ctest-wsr-target\\.txt"
      DEPENDS crt_mksh_rootfs_exec_builtin_runs
    )
    set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crt_mksh_rootfs_which_stat_readlink_runs)

    # Regression coverage for the 2026-08-16 LLP64-audit batch (cksum/crc32,
    # tsort, tty, unlink, uuencode): each was newly wired into
    # shell/toybox/crt/generated/newtoys.h and
    # shell/toybox/src/android/linux/generated/config.h's CFG_x/USE_x pair
    # (compiling the .c file into CRT_TOYBOX_SOURCES is not enough on its
    # own -- see the which/readlink/stat comment above, which this
    # reconfirmed for a second, disjoint reason: config.h's USE_x macros
    # gate whether an entry in newtoys.h compiles into toy_list[] at all).
    # tsort.c also had a real Windows LLP64 pointer-truncation bug fixed
    # here (see shell/toybox/PATCHES.md).
    add_test(NAME crt_mksh_rootfs_llp64_batch_runs
      COMMAND "${CMAKE_COMMAND}" -E env
        "CRT_ROOTFS=${CMAKE_BINARY_DIR}/rootfs"
        "PATH=/system/bin:/bin:/usr/bin"
        "$<TARGET_FILE:crt_mksh>" -c "echo hi | cksum; printf 'a b\\nb c\\n' | tsort; echo x | crc32; printf hello | uuencode - u.txt; echo target > /tmp/mksh-ctest-llp64-target.txt && unlink /tmp/mksh-ctest-llp64-target.txt && echo applet-batch-ok"
    )
    set_tests_properties(crt_mksh_rootfs_llp64_batch_runs PROPERTIES
      PASS_REGULAR_EXPRESSION "[0-9]+ 3\na\nb\nc\n[0-9a-f]+\nbegin 744 u.txt\n.*\nend\napplet-batch-ok"
      DEPENDS crt_mksh_rootfs_which_stat_readlink_runs
    )
    set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crt_mksh_rootfs_llp64_batch_runs)

    # `link` needed real linkat()/link() PAL backing, not just an LLP64
    # audit -- libc/src/fd.c's linkat() was an unconditional ENOTSUP stub on
    # every host (not just Windows) until now: CreateHardLinkA on Windows,
    # a new __crt_sys_link raw syscall trampoline in each of
    # libc/src/arch/linux/{x86_64,aarch64}/syscall.S and
    # libc/src/arch/macos/{x86_64,aarch64}/syscall.S. Only the Windows path
    # is actually verified by running this test on real hardware -- the
    # other three are unverified this session (no cross-toolchain
    # available), see HISTORY.md's own dated entry.
    add_test(NAME crt_mksh_rootfs_link_runs
      COMMAND "${CMAKE_COMMAND}" -E env
        "CRT_ROOTFS=${CMAKE_BINARY_DIR}/rootfs"
        "PATH=/system/bin:/bin:/usr/bin"
        "$<TARGET_FILE:crt_mksh>" -c "echo linktest > /tmp/mksh-ctest-link-src.txt && link /tmp/mksh-ctest-link-src.txt /tmp/mksh-ctest-link-dst.txt && cat /tmp/mksh-ctest-link-dst.txt && unlink /tmp/mksh-ctest-link-dst.txt && unlink /tmp/mksh-ctest-link-src.txt && echo link-ok"
    )
    set_tests_properties(crt_mksh_rootfs_link_runs PROPERTIES
      PASS_REGULAR_EXPRESSION "linktest\nlink-ok"
      DEPENDS crt_mksh_rootfs_llp64_batch_runs
    )
    set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crt_mksh_rootfs_link_runs)

    # Regression for a real bug caught only by an actual zlib port build
    # (2026-08-16, see HISTORY.md), not by anything above: inserting CRC32
    # between CKSUM and CP in shell/toybox/crt/generated/newtoys.h left
    # `crc32` sorted before `cp` (`r` > `p`), breaking toy_find()'s
    # required strict-ascending binary-search invariant over toy_list[] --
    # `cp` alone became silently unreachable ("toybox: Unknown command
    # cp"), which zlib's own `make install` hit immediately (its Makefile
    # calls `cp` to install the built library). No test above happened to
    # dispatch `cp`, so this went unnoticed until a real port build tried
    # it. This test asks toybox itself for its own real, current applet
    # list (plain `toybox` with no arguments, not a hardcoded copy that
    # could drift out of sync) and dispatches every single one, catching
    # any future entry that becomes unreachable this same way regardless
    # of the underlying cause (newtoys.h ordering, a missing config.h
    # CFG_x/USE_x pair, or anything else `toy_find()` depends on).
    add_test(NAME crt_mksh_rootfs_toybox_applet_sweep_runs
      COMMAND "${CMAKE_COMMAND}" -E env
        "CRT_ROOTFS=${CMAKE_BINARY_DIR}/rootfs"
        "PATH=/system/bin:/bin:/usr/bin"
        "$<TARGET_FILE:crt_mksh>" -c "bad=0; for cmd in $(toybox); do case \"$(toybox \"$cmd\" 2>&1)\" in *'Unknown command'*) bad=1; echo \"broken:$cmd\";; esac; done; if [ \"$bad\" = 0 ]; then echo applet-sweep-ok; fi"
    )
    set_tests_properties(crt_mksh_rootfs_toybox_applet_sweep_runs PROPERTIES
      PASS_REGULAR_EXPRESSION "applet-sweep-ok"
      DEPENDS crt_mksh_rootfs_link_runs
    )
    set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crt_mksh_rootfs_toybox_applet_sweep_runs)

    # Regression for the real, non-transient Windows rename()-over-open-
    # handle limitation dos2unix/unix2dos hit (2026-08-16, see
    # HISTORY.md): their shared copy_tempfile()/replace_tempfile()
    # implementation keeps the *original* file's own read handle open for
    # the entire conversion, then renames a tempfile over that same path.
    # __crt_sys_rename() now tries SetFileInformationByHandle(
    # FileRenameInfoEx, FILE_RENAME_FLAG_POSIX_SEMANTICS) first (the one
    # Win32 mechanism that can replace a file with another open handle on
    # it), falling back to the pre-existing MoveFileExA()-based retry loop
    # if that's unavailable. Round-trips a file through both directions.
    add_test(NAME crt_mksh_rootfs_dos2unix_runs
      COMMAND "${CMAKE_COMMAND}" -E env
        "CRT_ROOTFS=${CMAKE_BINARY_DIR}/rootfs"
        "PATH=/system/bin:/bin:/usr/bin"
        "$<TARGET_FILE:crt_mksh>" -c "printf 'a\\r\\nb\\r\\n' > /tmp/mksh-ctest-dos2unix.txt && dos2unix /tmp/mksh-ctest-dos2unix.txt && wc -c < /tmp/mksh-ctest-dos2unix.txt && unix2dos /tmp/mksh-ctest-dos2unix.txt && wc -c < /tmp/mksh-ctest-dos2unix.txt && rm -f /tmp/mksh-ctest-dos2unix.txt && echo dos2unix-ok"
    )
    set_tests_properties(crt_mksh_rootfs_dos2unix_runs PROPERTIES
      # "a\r\nb\r\n" (6 bytes) -> dos2unix strips both \r -> 4 bytes ->
      # unix2dos adds them back -> 6 bytes again.
      PASS_REGULAR_EXPRESSION "^4\n6\ndos2unix-ok"
      DEPENDS crt_mksh_rootfs_toybox_applet_sweep_runs
    )
    set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crt_mksh_rootfs_dos2unix_runs)

    # df/stty (2026-08-16, see HISTORY.md): both were re-enabled once their
    # backing PAL surface turned out to already exist (xgetmountlist()'s
    # /proc/mounts + real statvfs()/GetDiskFreeSpaceExA() for df;
    # tcgetattr/tcsetattr/TIOCGWINSZ/TIOCSWINSZ for stty). This also
    # exercises a real, general bug the enablement uncovered: their
    # pre-generated flags.h entries were checked in with df/stty disabled,
    # so every one of their FLAG_x macros used the always-0 FORCED_FLAG
    # multiplier instead of the real 1LL one -- df -h's own FLAG(h) check
    # is exactly the kind of always-false condition that bug caused (`df
    # -h` silently behaved like plain `df`, no error, wrong output), which
    # is why this test greps for the human-readable "Size"/"Avail" header
    # rather than just checking exit code 0.
    add_test(NAME crt_mksh_rootfs_df_runs
      COMMAND "${CMAKE_COMMAND}" -E env
        "CRT_ROOTFS=${CMAKE_BINARY_DIR}/rootfs"
        "PATH=/system/bin:/bin:/usr/bin"
        "$<TARGET_FILE:crt_mksh>" -c "df -h"
    )
    set_tests_properties(crt_mksh_rootfs_df_runs PROPERTIES
      PASS_REGULAR_EXPRESSION "Filesystem +Size +Used +Avail +Use% +Mounted on"
      DEPENDS crt_mksh_rootfs_dos2unix_runs
    )
    set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crt_mksh_rootfs_df_runs)

    # stty -g needs the same FLAG(g)-was-always-false fix as df -h above
    # (see that test's comment). /dev/tty needs a real attached console,
    # which some CI/sandbox environments genuinely don't have (confirmed
    # directly on this machine's own dev environment) -- accept a graceful
    # ENOTTY skip alongside the real success path rather than requiring a
    # console this test can't guarantee exists everywhere it runs.
    add_test(NAME crt_mksh_rootfs_stty_runs
      COMMAND "${CMAKE_COMMAND}" -E env
        "CRT_ROOTFS=${CMAKE_BINARY_DIR}/rootfs"
        "PATH=/system/bin:/bin:/usr/bin"
        "$<TARGET_FILE:crt_mksh>" -c "if out=$(stty -F /dev/tty -g 2>&1); then case \"$out\" in *:*:*:*) echo stty-ok;; *) echo \"stty-bad-output:$out\";; esac; else echo stty-skip-no-console; fi"
    )
    set_tests_properties(crt_mksh_rootfs_stty_runs PROPERTIES
      PASS_REGULAR_EXPRESSION "stty-ok|stty-skip-no-console"
      DEPENDS crt_mksh_rootfs_df_runs
    )
    set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crt_mksh_rootfs_stty_runs)
  endif()
  add_dependencies(crt_shell_artifacts crt_toybox)
  install(TARGETS crt_toybox
    RUNTIME DESTINATION system/bin
  )
endif()

# See shell/awk/README.md: a real AWK, built against this project's own CRT
# like mksh/toybox, rather than either enabling toybox's own disabled-by-
# default toys/pending/awk.c or depending on whatever awk (if any) a given
# host machine happens to already have installed.
set(CRT_AWK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/awk")
if(EXISTS "${CRT_AWK_ROOT}/src/awk.h")
  set(CRT_AWK_SOURCES
    awk/src/awkgram.tab.c
    awk/src/b.c
    awk/src/lex.c
    awk/src/lib.c
    awk/src/main.c
    awk/src/parse.c
    awk/src/proctab.c
    awk/src/run.c
    awk/src/tran.c
  )
  # Several dispatch-table-shaped functions across this import legitimately
  # ignore one of their (uniform-signature) parameters. A plain
  # target_compile_options(-Wno-unused-parameter) is not reliable here: CMake
  # places crt_build_flags' linked-INTERFACE -Wall/-Wextra/-Werror *after*
  # this target's own target_compile_options on the actual command line, and
  # -Wall/-Wextra re-enable -Wunused-parameter there, so -Werror would still
  # fault -- set_source_files_properties' COMPILE_OPTIONS are appended at the
  # very end instead, so this reliably wins the "last flag wins" ordering
  # (same reason libm/CMakeLists.txt and libc/CMakeLists.txt's gdtoa/regex
  # imports use the same mechanism).
  set_source_files_properties(${CRT_AWK_SOURCES} PROPERTIES
    COMPILE_OPTIONS "-Wno-unused-parameter;-Wno-unused-but-set-variable"
  )
  # CRT_STARTUP_END_OBJECTS deliberately not listed here -- see
  # crt_link_startup_end_objects()'s own comment (top-level CMakeLists.txt).
  add_executable(crt_awk
    ${CRT_STARTUP_OBJECTS}
    ${CRT_AWK_SOURCES}
  )
  target_link_libraries(crt_awk PRIVATE crt_build_flags c m)
  target_link_libraries(crt_awk PRIVATE crt_compiler_rt_builtins)
  target_include_directories(crt_awk PRIVATE
    "${CMAKE_SOURCE_DIR}/libc/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/awk/src"
  )
  target_link_options(crt_awk PRIVATE
    -nostdlib
    -nostartfiles
    -nodefaultlibs
  )
  if(CRT_TARGET_OS STREQUAL "macos")
    target_link_options(crt_awk PRIVATE
      -Wl,-e,_start
      -Wl,-dead_strip
    )
    target_link_libraries(crt_awk PRIVATE System)
  elseif(CRT_TARGET_OS STREQUAL "linux")
    target_link_options(crt_awk PRIVATE
      -Wl,-e,_start
      -Wl,--gc-sections
    )
  elseif(CRT_TARGET_OS STREQUAL "windows")
    target_link_options(crt_awk PRIVATE
      -Wl,/entry:mainCRTStartup
      -Wl,/subsystem:console
      -Wl,/OPT:REF
    )
    target_link_libraries(crt_awk PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
  endif()
  crt_link_startup_end_objects(crt_awk)
  add_dependencies(crt_awk c m crt1)
  set_target_properties(crt_awk PROPERTIES
    OUTPUT_NAME awk
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/shell"
  )
  add_test(NAME crt_awk_basic_runs
    COMMAND crt_awk "BEGIN { print \"awk-ok\" }"
  )
  set_tests_properties(crt_awk_basic_runs PROPERTIES
    PASS_REGULAR_EXPRESSION "awk-ok"
  )
  set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crt_awk_basic_runs)

  add_dependencies(crt_shell_artifacts crt_awk)
  install(TARGETS crt_awk
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
