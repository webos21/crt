if(CRT_TARGET_OS STREQUAL "macos")
  set(CRT_DL_BACKEND_SOURCE src/arch/macos/dl_macos.c)
elseif(CRT_TARGET_OS STREQUAL "linux")
  set(CRT_DL_BACKEND_SOURCE src/arch/linux/dl_linux.c)
elseif(CRT_TARGET_OS STREQUAL "windows")
  set(CRT_DL_BACKEND_SOURCE src/arch/windows/dl_windows.c)
else()
  message(FATAL_ERROR "Unsupported CRT_TARGET_OS: ${CRT_TARGET_OS}")
endif()

set(CRT_DL_SOURCES
  src/dl.c
  ${CRT_DL_BACKEND_SOURCE}
)

add_library(dl STATIC ${CRT_DL_SOURCES})

target_link_libraries(dl PRIVATE crt_build_flags c)
set_target_properties(dl PROPERTIES
  OUTPUT_NAME dl
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

add_library(dl_shared SHARED ${CRT_DL_SOURCES})
if(CRT_TARGET_OS STREQUAL "windows")
  target_sources(dl_shared PRIVATE $<TARGET_OBJECTS:crt_windows_dllcrt>)
endif()

target_link_libraries(dl_shared PRIVATE crt_build_flags c_shared)
crt_configure_shared_runtime(dl_shared)
set_target_properties(dl_shared PROPERTIES
  OUTPUT_NAME dl
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)
if(CRT_TARGET_OS STREQUAL "windows")
  set_target_properties(dl_shared PROPERTIES
    ARCHIVE_OUTPUT_NAME dl_dll
  )
endif()

install(TARGETS dl dl_shared
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
