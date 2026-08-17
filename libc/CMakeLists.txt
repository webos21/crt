set(CRT_LIBC_SOURCES
  src/atoi.c
  src/atof.c
  src/abs.c
  src/atol.c
  src/atoll.c
  src/ctype.c
  src/dirent.c
  src/epoll.c
  src/errno.c
  src/env.c
  src/eventfd.c
  src/exit.c
  src/fd.c
  src/file_lock.c
  src/getopt.c
  src/inotify.c
  src/inttypes.c
  src/langinfo.c
  src/libgen.c
  src/locale.c
  src/malloc.c
  src/mman.c
  src/mntent.c
  src/poll.c
  src/printf.c
  src/process.c
  src/pthread.c
  src/qsort.c
  src/fnmatch.c
  src/random.c
  src/reallocarray.c
  src/resource.c
  src/sched.c
  src/scanf.c
  src/semaphore.c
  src/setjmp.c
  src/shadow.c
  src/signal.c
  src/socket.c
  src/stack_protector.c
  src/stdio.c
  src/syslog.c
  src/syscall_public.c
  src/strtol.c
  src/strtoll.c
  src/strtoul.c
  src/strtoull.c
  src/tls.c
  src/termios.c
  src/time.c
  src/timerfd.c
  src/utmp.c
  src/utsname.c
  src/user_group.c
  src/wait.c
  src/wchar.c
  src/wctype.c
  src/xattr.c
  src/string/memchr.c
  src/string/memrchr.c
  src/string/memccpy.c
  src/string/memcmp.c
  src/string/memcpy.c
  src/string/memmove.c
  src/string/memset.c
  src/string/strcat.c
  src/string/stpcpy.c
  src/string/stpncpy.c
  src/string/strcasecmp.c
  src/string/strings_compat.c
  src/string/strcoll.c
  src/string/strcspn.c
  src/string/strdup.c
  src/string/strchr.c
  src/string/strlen.c
  src/string/strcmp.c
  src/string/strcpy.c
  src/string/strndup.c
  src/string/strnlen.c
  src/string/strncmp.c
  src/string/strncpy.c
  src/string/strpbrk.c
  src/string/strrchr.c
  src/string/strspn.c
  src/string/strerror.c
  src/string/strlcat.c
  src/string/strlcpy.c
  src/string/strstr.c
  src/string/strtok_r.c
  src/string/strxfrm.c
)

set(CRT_GDTOA_SOURCES
  src/gdtoa/dmisc.c
  src/gdtoa/dtoa.c
  src/gdtoa/gdtoa.c
  src/gdtoa/gdtoa_support.c
  src/gdtoa/gethex.c
  src/gdtoa/gmisc.c
  src/gdtoa/hd_init.c
  src/gdtoa/hdtoa.c
  src/gdtoa/hexnan.c
  src/gdtoa/ldtoa.c
  src/gdtoa/misc.c
  src/gdtoa/smisc.c
  src/gdtoa/strtod.c
  src/gdtoa/strtodg.c
  src/gdtoa/strtof.c
  src/gdtoa/strtord.c
  src/gdtoa/strtorQ.c
  src/gdtoa/sum.c
  src/gdtoa/ulp.c
)

list(APPEND CRT_LIBC_SOURCES ${CRT_GDTOA_SOURCES})
set_source_files_properties(${CRT_GDTOA_SOURCES} PROPERTIES
  COMPILE_OPTIONS "-include;${CMAKE_CURRENT_SOURCE_DIR}/src/gdtoa/openbsd-compat.h;-Wno-sign-compare;-Wno-uninitialized"
)

# NetBSD/Bionic regex engine import -- see third_party/bionic/README.md for
# provenance and libc/src/regex/netbsd-compat.h for the portability shim
# force-included ahead of every file here. engine.c is deliberately NOT
# listed: it is textually #include'd three times by regexec.c itself (once
# per state representation), never compiled as its own translation unit.
set(CRT_REGEX_SOURCES
  src/regex/regcomp.c
  src/regex/regerror.c
  src/regex/regexec.c
  src/regex/regfree.c
)
list(APPEND CRT_LIBC_SOURCES ${CRT_REGEX_SOURCES})
set_source_files_properties(${CRT_REGEX_SOURCES} PROPERTIES
  COMPILE_OPTIONS "-include;${CMAKE_CURRENT_SOURCE_DIR}/src/regex/netbsd-compat.h;-Wno-sign-compare;-Wno-unused-parameter;-Wno-unused-function;-Wno-missing-field-initializers"
)

add_library(c STATIC ${CRT_LIBC_SOURCES})
target_link_libraries(c PRIVATE crt_build_flags)
target_include_directories(c PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/gdtoa"
)
set_target_properties(c PROPERTIES
  OUTPUT_NAME c
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

if(CRT_TARGET_ARCH STREQUAL "host")
  set(CRT_EFFECTIVE_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
else()
  set(CRT_EFFECTIVE_ARCH "${CRT_TARGET_ARCH}")
endif()

if(CRT_TARGET_OS STREQUAL "macos")
  if(CRT_EFFECTIVE_ARCH MATCHES "^(arm64|aarch64)$")
    set(CRT_START_FILE src/arch/macos/aarch64/crt1.S)
    set(CRT_SYSCALL_FILE src/arch/macos/aarch64/syscall.S)
    set(CRT_SETJMP_FILE src/arch/macos/aarch64/setjmp.S)
  elseif(CRT_EFFECTIVE_ARCH MATCHES "^(x86_64|AMD64)$")
    set(CRT_START_FILE src/arch/macos/x86_64/crt1.S)
    set(CRT_SYSCALL_FILE src/arch/macos/x86_64/syscall.S)
    set(CRT_SETJMP_FILE src/arch/macos/x86_64/setjmp.S)
  else()
    message(FATAL_ERROR "Unsupported macOS architecture: ${CRT_EFFECTIVE_ARCH}")
  endif()
  set(CRT_MACOS_ERRNO_FILE src/arch/macos/common/errno.c)
  set(CRT_SIGNAL_BACKEND_FILE
    src/arch/macos/common/signal_backend.c
    src/arch/macos/common/macho_symbol.c
  )
  # Real ioctl(TIOCGETA/TIOCSETA{,W,F}/...)-backed tcgetattr()/tcsetattr()/
  # tcdrain()/tcflow()/tcflush()/tcsendbreak() -- see that file's own
  # comment for the hardcoded-stub round-trip bug this replaces.
  set(CRT_MACOS_TERMIOS_FILE src/arch/macos/common/termios.c)
elseif(CRT_TARGET_OS STREQUAL "linux")
  if(CRT_EFFECTIVE_ARCH MATCHES "^(arm64|aarch64)$")
    set(CRT_START_FILE src/arch/linux/aarch64/crt1.S)
    set(CRT_SYSCALL_FILE src/arch/linux/aarch64/syscall.S)
    set(CRT_SETJMP_FILE src/arch/linux/aarch64/setjmp.S)
  elseif(CRT_EFFECTIVE_ARCH MATCHES "^(x86_64|AMD64)$")
    set(CRT_START_FILE src/arch/linux/x86_64/crt1.S)
    set(CRT_SYSCALL_FILE src/arch/linux/x86_64/syscall.S)
    set(CRT_SETJMP_FILE src/arch/linux/x86_64/setjmp.S)
  else()
    message(FATAL_ERROR "Unsupported Linux architecture: ${CRT_EFFECTIVE_ARCH}")
  endif()
  set(CRT_SIGNAL_BACKEND_FILE src/arch/linux/common/signal_backend.c)
  # getauxval() (include/sys/auxv.h) -- Linux-only, no macOS/Windows
  # equivalent kernel mechanism exists (see that file's own comment).
  set(CRT_LINUX_AUXV_FILE src/arch/linux/common/auxv.c)
  # Real ioctl(TCGETS/TCSETS*)-backed tcgetattr()/tcsetattr()/etc -- see
  # that file's own comment for the round-trip bug this replaces.
  set(CRT_LINUX_TERMIOS_FILE src/arch/linux/common/termios.c)
  # __crt_run_init_array()/__crt_run_fini_array() (.init_array/.fini_array
  # support for the executable's own static link -- see that file's own
  # comment for why this must be linked directly into every executable,
  # not the general libc.a/libc.so sources: crt1.o is always statically
  # embedded regardless of whether libc itself is linked static or
  # shared, so only code that lives alongside it is guaranteed to see
  # the calling executable's own __init_array_start/__init_array_end
  # boundary symbols rather than some other DSO's). Kept as its own
  # object library/install name (crt1_init_array.o) rather than folded
  # into CRT_START_FILE/crt1.o directly: install(FILES
  # "$<TARGET_OBJECTS:crt1>" ... RENAME crt1.o) below assumes exactly one
  # object, and tools/crt-cc already has an established pattern for
  # referencing more than one fixed-named startup object explicitly
  # (dllcrt.o on Windows) rather than merging objects to preserve a
  # single filename.
  set(CRT_INIT_FINI_ARRAY_FILE src/arch/linux/common/init_fini_array.c)
elseif(CRT_TARGET_OS STREQUAL "windows")
  if(CRT_EFFECTIVE_ARCH MATCHES "^(x86_64|AMD64)$")
    set(CRT_START_FILE src/arch/windows/common/crt1.c)
    set(CRT_SYSCALL_FILE src/arch/windows/common/syscall.c src/arch/windows/x86_64/fork_memcopy.c)
    set(CRT_SETJMP_FILE src/arch/windows/x86_64/setjmp.S)
    set(CRT_WINDOWS_CHKSTK_FILE src/arch/windows/x86_64/chkstk.S)
  elseif(CRT_EFFECTIVE_ARCH MATCHES "^(arm64|ARM64|aarch64)$")
    set(CRT_START_FILE src/arch/windows/common/crt1.c)
    set(CRT_SYSCALL_FILE src/arch/windows/common/syscall.c src/arch/windows/aarch64/fork_memcopy.c)
    set(CRT_SETJMP_FILE src/arch/windows/aarch64/setjmp.S)
    set(CRT_WINDOWS_CHKSTK_FILE src/arch/windows/aarch64/chkstk.S)
  else()
    message(FATAL_ERROR "Unsupported Windows architecture: ${CRT_EFFECTIVE_ARCH}")
  endif()
  set(CRT_SIGNAL_BACKEND_FILE src/arch/windows/common/signal_backend.c)
  # __crt_run_init_array()/__crt_run_fini_array() Windows/PE equivalent --
  # see libc/src/arch/windows/common/init_fini_array.c's own comment for
  # why this needs two extra, position-sensitive sentinel objects
  # (ctors_begin.c/ctors_end.c) rather than the single boundary-symbol
  # trick Linux's ELF linker script provides automatically.
  set(CRT_CTORS_BEGIN_FILE src/arch/windows/common/ctors_begin.c)
  set(CRT_CTORS_END_FILE src/arch/windows/common/ctors_end.c)
  set(CRT_CTORS_WALKER_FILE src/arch/windows/common/init_fini_array.c)
  # _pei386_runtime_relocator() -- see that file's own comment. Not
  # position-sensitive (no custom sentinel objects needed: the linker
  # provides __RUNTIME_PSEUDO_RELOC_LIST__/_END__ automatically), just
  # needs to exist somewhere in the link and be called early at startup.
  set(CRT_PSEUDO_RELOC_FILE src/arch/windows/common/pseudo_reloc.c)
else()
  message(FATAL_ERROR "Unsupported CRT_TARGET_OS: ${CRT_TARGET_OS}")
endif()

add_library(crt_syscall OBJECT ${CRT_SYSCALL_FILE})
target_link_libraries(crt_syscall PRIVATE crt_build_flags)
target_include_directories(crt_syscall PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_sources(c PRIVATE $<TARGET_OBJECTS:crt_syscall>)
if(CRT_TARGET_OS STREQUAL "macos")
  target_sources(c PRIVATE ${CRT_MACOS_ERRNO_FILE})
  target_sources(c PRIVATE ${CRT_MACOS_TERMIOS_FILE})
endif()
target_sources(c PRIVATE ${CRT_SIGNAL_BACKEND_FILE})
if(CRT_TARGET_OS STREQUAL "linux")
  target_sources(c PRIVATE ${CRT_LINUX_AUXV_FILE})
  target_sources(c PRIVATE ${CRT_LINUX_TERMIOS_FILE})
endif()

add_library(crt_setjmp OBJECT ${CRT_SETJMP_FILE})
target_link_libraries(crt_setjmp PRIVATE crt_build_flags)
target_sources(c PRIVATE $<TARGET_OBJECTS:crt_setjmp>)

if(CRT_TARGET_OS STREQUAL "windows")
  add_library(crt_windows_chkstk OBJECT ${CRT_WINDOWS_CHKSTK_FILE})
  target_link_libraries(crt_windows_chkstk PRIVATE crt_build_flags)
  target_sources(c PRIVATE $<TARGET_OBJECTS:crt_windows_chkstk>)

  add_library(crt_windows_compiler_abi OBJECT src/arch/windows/common/compiler_abi.c)
  target_link_libraries(crt_windows_compiler_abi PRIVATE crt_build_flags)
  target_sources(c PRIVATE $<TARGET_OBJECTS:crt_windows_compiler_abi>)

  add_library(crt_windows_dllcrt OBJECT src/arch/windows/common/dllcrt.c)
  target_link_libraries(crt_windows_dllcrt PRIVATE crt_build_flags)
endif()

add_library(crt1 OBJECT ${CRT_START_FILE})
target_link_libraries(crt1 PRIVATE crt_build_flags)
target_include_directories(crt1 PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
set_target_properties(crt1 PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

set(CRT_STARTUP_OBJECTS $<TARGET_OBJECTS:crt1>)
if(CRT_TARGET_OS STREQUAL "linux")
  add_library(crt1_init_array OBJECT ${CRT_INIT_FINI_ARRAY_FILE})
  target_link_libraries(crt1_init_array PRIVATE crt_build_flags)
  target_include_directories(crt1_init_array PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  set_target_properties(crt1_init_array PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  )
  # __crt_run_init_array() (see that file's own comment) must be linked
  # directly into every executable alongside crt1.o itself on Linux.
  # Exposed via PARENT_SCOPE so every subdirectory that builds an
  # executable (shell/, tests/) can list ${CRT_STARTUP_OBJECTS} instead
  # of the bare $<TARGET_OBJECTS:crt1> they'd otherwise need to remember
  # to extend themselves on every executable target.
  list(APPEND CRT_STARTUP_OBJECTS $<TARGET_OBJECTS:crt1_init_array>)
elseif(CRT_TARGET_OS STREQUAL "windows")
  # See libc/src/arch/windows/common/init_fini_array.c's own comment:
  # unlike Linux's single boundary-symbol-driven object, Windows/PE needs
  # THREE separate objects, two of which (begin/end) are POSITION-
  # SENSITIVE on the final link line, not just "linked somewhere" --
  # ctors_begin.o must be the first .ctors/.dtors contributor (right
  # after crt1.o, before any user code), ctors_end.o must be the last
  # (after everything else, including any third-party static libraries a
  # port build links). CRT_STARTUP_OBJECTS below only covers the "must
  # come first" half; CRT_STARTUP_END_OBJECTS (a second, separate
  # PARENT_SCOPE variable) carries the "must come last" half -- every
  # add_executable() call site needs to place sources between the two,
  # not just append CRT_STARTUP_OBJECTS the way Linux/macOS's single
  # variable already could.
  add_library(crt1_ctors_begin OBJECT ${CRT_CTORS_BEGIN_FILE})
  target_link_libraries(crt1_ctors_begin PRIVATE crt_build_flags)
  set_target_properties(crt1_ctors_begin PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  )
  add_library(crt1_ctors_end OBJECT ${CRT_CTORS_END_FILE})
  target_link_libraries(crt1_ctors_end PRIVATE crt_build_flags)
  set_target_properties(crt1_ctors_end PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  )
  add_library(crt1_ctors_walker OBJECT ${CRT_CTORS_WALKER_FILE})
  target_link_libraries(crt1_ctors_walker PRIVATE crt_build_flags)
  set_target_properties(crt1_ctors_walker PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  )
  add_library(crt1_pseudo_reloc OBJECT ${CRT_PSEUDO_RELOC_FILE})
  target_link_libraries(crt1_pseudo_reloc PRIVATE crt_build_flags)
  set_target_properties(crt1_pseudo_reloc PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  )
  # Neither the walker nor the pseudo-reloc object (both plain code, not
  # section-merge-order-sensitive data objects) need any particular link
  # position; bundled into the "begin" group purely for convenience,
  # matching how crt1_init_array's own walker rides along with the
  # "always first" group on Linux.
  list(APPEND CRT_STARTUP_OBJECTS $<TARGET_OBJECTS:crt1_ctors_begin> $<TARGET_OBJECTS:crt1_ctors_walker> $<TARGET_OBJECTS:crt1_pseudo_reloc>)
  set(CRT_STARTUP_END_OBJECTS $<TARGET_OBJECTS:crt1_ctors_end> PARENT_SCOPE)
endif()
if(NOT CRT_TARGET_OS STREQUAL "windows")
  set(CRT_STARTUP_END_OBJECTS "" PARENT_SCOPE)
endif()
set(CRT_STARTUP_OBJECTS "${CRT_STARTUP_OBJECTS}" PARENT_SCOPE)

add_library(c_shared SHARED ${CRT_LIBC_SOURCES})
target_link_libraries(c_shared PRIVATE crt_build_flags crt_compiler_rt_builtins)
target_include_directories(c_shared PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/gdtoa"
)
target_sources(c_shared PRIVATE $<TARGET_OBJECTS:crt_syscall>)
target_sources(c_shared PRIVATE $<TARGET_OBJECTS:crt_setjmp>)
if(CRT_TARGET_OS STREQUAL "macos")
  target_sources(c_shared PRIVATE ${CRT_MACOS_ERRNO_FILE})
  target_sources(c_shared PRIVATE ${CRT_MACOS_TERMIOS_FILE})
endif()
target_sources(c_shared PRIVATE ${CRT_SIGNAL_BACKEND_FILE})
if(CRT_TARGET_OS STREQUAL "linux")
  target_sources(c_shared PRIVATE ${CRT_LINUX_AUXV_FILE})
  target_sources(c_shared PRIVATE ${CRT_LINUX_TERMIOS_FILE})
endif()
if(CRT_TARGET_OS STREQUAL "windows")
  target_sources(c_shared PRIVATE $<TARGET_OBJECTS:crt_windows_chkstk>)
  target_sources(c_shared PRIVATE $<TARGET_OBJECTS:crt_windows_compiler_abi>)
  target_sources(c_shared PRIVATE $<TARGET_OBJECTS:crt_windows_dllcrt>)
endif()
crt_configure_shared_runtime(c_shared)
set_target_properties(c_shared PROPERTIES
  OUTPUT_NAME c
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)
if(CRT_TARGET_OS STREQUAL "windows")
  set_target_properties(c_shared PROPERTIES
    ARCHIVE_OUTPUT_NAME c_dll
  )
endif()

install(TARGETS c c_shared
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES "$<TARGET_OBJECTS:crt1>" DESTINATION lib RENAME crt1.o)
if(CRT_TARGET_OS STREQUAL "linux")
  install(FILES "$<TARGET_OBJECTS:crt1_init_array>" DESTINATION lib RENAME crt1_init_array.o)
endif()
if(CRT_TARGET_OS STREQUAL "windows")
  # crt_windows_dllcrt (crtDllMainCRTStartup, dllcrt.c) has so far only
  # ever been linked directly into this project's own c_shared/m_shared/
  # etc DLL targets above -- nothing installed a standalone, reusable
  # copy into the sysroot the way crt1.o is installed for EXE builds.
  # tools/crt-cc's own -shared support needs exactly that (see its
  # comment on dllcrt.o) to build a DLL for a third-party port.
  install(FILES "$<TARGET_OBJECTS:crt_windows_dllcrt>" DESTINATION lib RENAME dllcrt.o)
  install(FILES "$<TARGET_OBJECTS:crt1_ctors_begin>" DESTINATION lib RENAME crt1_ctors_begin.o)
  install(FILES "$<TARGET_OBJECTS:crt1_ctors_end>" DESTINATION lib RENAME crt1_ctors_end.o)
  install(FILES "$<TARGET_OBJECTS:crt1_ctors_walker>" DESTINATION lib RENAME crt1_ctors_walker.o)
  install(FILES "$<TARGET_OBJECTS:crt1_pseudo_reloc>" DESTINATION lib RENAME crt1_pseudo_reloc.o)
endif()
