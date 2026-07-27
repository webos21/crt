add_library(c STATIC
  src/exit.c
  src/write.c
  string/memcpy.c
  string/memmove.c
  string/memset.c
  string/strlen.c
  string/strcmp.c
)
target_link_libraries(c PRIVATE crt_build_flags)
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
  elseif(CRT_EFFECTIVE_ARCH MATCHES "^(x86_64|AMD64)$")
    set(CRT_START_FILE arch/macos/x86_64/crt1.S)
    set(CRT_SYSCALL_FILE arch/macos/x86_64/syscall.S)
  else()
    message(FATAL_ERROR "Unsupported macOS architecture: ${CRT_EFFECTIVE_ARCH}")
  endif()
elseif(CRT_TARGET_OS STREQUAL "linux")
  if(CRT_EFFECTIVE_ARCH MATCHES "^(arm64|aarch64)$")
    set(CRT_START_FILE arch/linux/aarch64/crt1.S)
    set(CRT_SYSCALL_FILE arch/linux/aarch64/syscall.S)
  elseif(CRT_EFFECTIVE_ARCH MATCHES "^(x86_64|AMD64)$")
    set(CRT_START_FILE arch/linux/x86_64/crt1.S)
    set(CRT_SYSCALL_FILE arch/linux/x86_64/syscall.S)
  else()
    message(FATAL_ERROR "Unsupported Linux architecture: ${CRT_EFFECTIVE_ARCH}")
  endif()
elseif(CRT_TARGET_OS STREQUAL "windows")
  if(CRT_EFFECTIVE_ARCH MATCHES "^(x86_64|AMD64|arm64|ARM64|aarch64)$")
    set(CRT_START_FILE arch/windows/common/crt1.c)
    set(CRT_SYSCALL_FILE arch/windows/common/syscall.c)
  else()
    message(FATAL_ERROR "Unsupported Windows architecture: ${CRT_EFFECTIVE_ARCH}")
  endif()
else()
  message(FATAL_ERROR "Unsupported CRT_TARGET_OS: ${CRT_TARGET_OS}")
endif()

add_library(crt_syscall OBJECT ${CRT_SYSCALL_FILE})
target_link_libraries(crt_syscall PRIVATE crt_build_flags)
target_sources(c PRIVATE $<TARGET_OBJECTS:crt_syscall>)

add_library(crt1 OBJECT ${CRT_START_FILE})
target_link_libraries(crt1 PRIVATE crt_build_flags)
set_target_properties(crt1 PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

install(TARGETS c ARCHIVE DESTINATION lib)
install(FILES "$<TARGET_OBJECTS:crt1>" DESTINATION lib RENAME crt1.o)
