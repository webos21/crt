add_library(c STATIC
  src/exit.c
  src/write.c
)
target_link_libraries(c PRIVATE crt_build_flags)
set_target_properties(c PROPERTIES
  OUTPUT_NAME c
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

if(CRT_TARGET_OS STREQUAL "macos")
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
    set(CRT_START_FILE arch/macos/aarch64/crt1.S)
    set(CRT_SYSCALL_FILE arch/macos/aarch64/syscall.S)
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
    set(CRT_START_FILE arch/macos/x86_64/crt1.S)
    set(CRT_SYSCALL_FILE arch/macos/x86_64/syscall.S)
  else()
    message(FATAL_ERROR "Unsupported macOS architecture: ${CMAKE_SYSTEM_PROCESSOR}")
  endif()
else()
  message(FATAL_ERROR "The initial freestanding hello target currently supports CRT_TARGET_OS=macos only")
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
