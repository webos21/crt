set(CRT_LIBC_SOURCES
  src/atoi.c
  src/atof.c
  src/abs.c
  src/atol.c
  src/atoll.c
  src/ctype.c
  src/dirent.c
  src/errno.c
  src/env.c
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
  src/setjmp.c
  src/shadow.c
  src/signal.c
  src/socket.c
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
endif()
target_sources(c PRIVATE ${CRT_SIGNAL_BACKEND_FILE})

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
endif()
target_sources(c_shared PRIVATE ${CRT_SIGNAL_BACKEND_FILE})
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
