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
  src/resource.c
  src/sched.c
  src/scanf.c
  src/setjmp.c
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
  src/user_group.c
  src/wait.c
  src/wchar.c
  src/wctype.c
  src/xattr.c
  string/memchr.c
  string/memccpy.c
  string/memcmp.c
  string/memcpy.c
  string/memmove.c
  string/memset.c
  string/strcat.c
  string/stpcpy.c
  string/stpncpy.c
  string/strcasecmp.c
  string/strings_compat.c
  string/strcoll.c
  string/strcspn.c
  string/strdup.c
  string/strchr.c
  string/strlen.c
  string/strcmp.c
  string/strcpy.c
  string/strndup.c
  string/strnlen.c
  string/strncmp.c
  string/strncpy.c
  string/strpbrk.c
  string/strrchr.c
  string/strspn.c
  string/strerror.c
  string/strlcat.c
  string/strlcpy.c
  string/strstr.c
  string/strtok_r.c
  string/strxfrm.c
)

set(CRT_GDTOA_SOURCES
  gdtoa/dmisc.c
  gdtoa/dtoa.c
  gdtoa/gdtoa.c
  gdtoa/gdtoa_support.c
  gdtoa/gethex.c
  gdtoa/gmisc.c
  gdtoa/hd_init.c
  gdtoa/hdtoa.c
  gdtoa/hexnan.c
  gdtoa/ldtoa.c
  gdtoa/misc.c
  gdtoa/smisc.c
  gdtoa/strtod.c
  gdtoa/strtodg.c
  gdtoa/strtof.c
  gdtoa/strtord.c
  gdtoa/strtorQ.c
  gdtoa/sum.c
  gdtoa/ulp.c
)

list(APPEND CRT_LIBC_SOURCES ${CRT_GDTOA_SOURCES})
set_source_files_properties(${CRT_GDTOA_SOURCES} PROPERTIES
  COMPILE_OPTIONS "-include;${CMAKE_CURRENT_SOURCE_DIR}/gdtoa/openbsd-compat.h;-Wno-sign-compare;-Wno-uninitialized"
)

add_library(c STATIC ${CRT_LIBC_SOURCES})
target_link_libraries(c PRIVATE crt_build_flags)
target_include_directories(c PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/gdtoa"
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
    set(CRT_START_FILE arch/macos/aarch64/crt1.S)
    set(CRT_SYSCALL_FILE arch/macos/aarch64/syscall.S)
    set(CRT_SETJMP_FILE arch/macos/aarch64/setjmp.S)
  elseif(CRT_EFFECTIVE_ARCH MATCHES "^(x86_64|AMD64)$")
    set(CRT_START_FILE arch/macos/x86_64/crt1.S)
    set(CRT_SYSCALL_FILE arch/macos/x86_64/syscall.S)
    set(CRT_SETJMP_FILE arch/macos/x86_64/setjmp.S)
  else()
    message(FATAL_ERROR "Unsupported macOS architecture: ${CRT_EFFECTIVE_ARCH}")
  endif()
elseif(CRT_TARGET_OS STREQUAL "linux")
  if(CRT_EFFECTIVE_ARCH MATCHES "^(arm64|aarch64)$")
    set(CRT_START_FILE arch/linux/aarch64/crt1.S)
    set(CRT_SYSCALL_FILE arch/linux/aarch64/syscall.S)
    set(CRT_SETJMP_FILE arch/linux/aarch64/setjmp.S)
  elseif(CRT_EFFECTIVE_ARCH MATCHES "^(x86_64|AMD64)$")
    set(CRT_START_FILE arch/linux/x86_64/crt1.S)
    set(CRT_SYSCALL_FILE arch/linux/x86_64/syscall.S)
    set(CRT_SETJMP_FILE arch/linux/x86_64/setjmp.S)
  else()
    message(FATAL_ERROR "Unsupported Linux architecture: ${CRT_EFFECTIVE_ARCH}")
  endif()
elseif(CRT_TARGET_OS STREQUAL "windows")
  if(CRT_EFFECTIVE_ARCH MATCHES "^(x86_64|AMD64)$")
    set(CRT_START_FILE arch/windows/common/crt1.c)
    set(CRT_SYSCALL_FILE arch/windows/common/syscall.c)
    set(CRT_SETJMP_FILE arch/windows/x86_64/setjmp.S)
    set(CRT_WINDOWS_CHKSTK_FILE arch/windows/x86_64/chkstk.S)
  elseif(CRT_EFFECTIVE_ARCH MATCHES "^(arm64|ARM64|aarch64)$")
    set(CRT_START_FILE arch/windows/common/crt1.c)
    set(CRT_SYSCALL_FILE arch/windows/common/syscall.c)
    set(CRT_SETJMP_FILE arch/windows/aarch64/setjmp.S)
    set(CRT_WINDOWS_CHKSTK_FILE arch/windows/aarch64/chkstk.S)
  else()
    message(FATAL_ERROR "Unsupported Windows architecture: ${CRT_EFFECTIVE_ARCH}")
  endif()
else()
  message(FATAL_ERROR "Unsupported CRT_TARGET_OS: ${CRT_TARGET_OS}")
endif()

add_library(crt_syscall OBJECT ${CRT_SYSCALL_FILE})
target_link_libraries(crt_syscall PRIVATE crt_build_flags)
target_include_directories(crt_syscall PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_sources(c PRIVATE $<TARGET_OBJECTS:crt_syscall>)

add_library(crt_setjmp OBJECT ${CRT_SETJMP_FILE})
target_link_libraries(crt_setjmp PRIVATE crt_build_flags)
target_sources(c PRIVATE $<TARGET_OBJECTS:crt_setjmp>)

if(CRT_TARGET_OS STREQUAL "windows")
  add_library(crt_windows_chkstk OBJECT ${CRT_WINDOWS_CHKSTK_FILE})
  target_link_libraries(crt_windows_chkstk PRIVATE crt_build_flags)
  target_sources(c PRIVATE $<TARGET_OBJECTS:crt_windows_chkstk>)

  add_library(crt_windows_compiler_abi OBJECT arch/windows/common/compiler_abi.c)
  target_link_libraries(crt_windows_compiler_abi PRIVATE crt_build_flags)
  target_sources(c PRIVATE $<TARGET_OBJECTS:crt_windows_compiler_abi>)

  add_library(crt_windows_dllcrt OBJECT arch/windows/common/dllcrt.c)
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
  "${CMAKE_CURRENT_SOURCE_DIR}/gdtoa"
)
target_sources(c_shared PRIVATE $<TARGET_OBJECTS:crt_syscall>)
target_sources(c_shared PRIVATE $<TARGET_OBJECTS:crt_setjmp>)
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
