set(CRTGFX_COMMON_SOURCES
  src/common/window.c
  src/common/wayland_weston.c
)

if(CRT_TARGET_OS STREQUAL "windows")
  set(CRTGFX_BACKEND_SOURCES src/arch/windows/window_win32.c)
elseif(CRT_TARGET_OS STREQUAL "macos")
  set(CRTGFX_BACKEND_SOURCES src/arch/macos/window_stub.c)
elseif(CRT_TARGET_OS STREQUAL "linux")
  set(CRTGFX_BACKEND_SOURCES src/arch/linux/window_stub.c)
endif()

set(CRTGFX_CRT_STATIC_LIBS c m dl cxx)
set(CRTGFX_CRT_SHARED_LIBS c_shared m_shared dl_shared cxx_shared)

add_library(crtgfx_common_objects OBJECT ${CRTGFX_COMMON_SOURCES})
target_link_libraries(crtgfx_common_objects PRIVATE crt_build_flags)
target_include_directories(crtgfx_common_objects PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_include_directories(crtgfx_common_objects PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src/common"
)
set_target_properties(crtgfx_common_objects PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

# Host backends are intentionally outside crt_build_flags: this is the narrow
# layer allowed to speak native window/GPU APIs while the common runtime above
# stays on this project's CRT surface.
add_library(crtgfx_backend_objects OBJECT ${CRTGFX_BACKEND_SOURCES})
target_include_directories(crtgfx_backend_objects PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_include_directories(crtgfx_backend_objects PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src/common"
)
set_target_properties(crtgfx_backend_objects PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)
if(CRT_TARGET_OS STREQUAL "windows")
  target_compile_definitions(crtgfx_backend_objects PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
  )

  find_file(CRTGFX_WINDOWS_USER32_LIB
    NAMES user32.lib
    PATHS ${CRT_WINDOWS_SDK_LIB_CANDIDATES}
    NO_DEFAULT_PATH
  )
  find_file(CRTGFX_WINDOWS_GDI32_LIB
    NAMES gdi32.lib
    PATHS ${CRT_WINDOWS_SDK_LIB_CANDIDATES}
    NO_DEFAULT_PATH
  )
  if(NOT CRTGFX_WINDOWS_USER32_LIB)
    set(CRTGFX_WINDOWS_USER32_LIB user32.lib CACHE FILEPATH "Windows user32 import library")
  endif()
  if(NOT CRTGFX_WINDOWS_GDI32_LIB)
    set(CRTGFX_WINDOWS_GDI32_LIB gdi32.lib CACHE FILEPATH "Windows gdi32 import library")
  endif()
endif()

add_library(crtgfx STATIC
  $<TARGET_OBJECTS:crtgfx_common_objects>
  $<TARGET_OBJECTS:crtgfx_backend_objects>
)
target_include_directories(crtgfx PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(crtgfx PRIVATE ${CRTGFX_CRT_STATIC_LIBS})
if(CRT_TARGET_OS STREQUAL "windows")
  target_link_libraries(crtgfx PUBLIC
    "${CRT_WINDOWS_KERNEL32_LIB}"
    "${CRTGFX_WINDOWS_USER32_LIB}"
    "${CRTGFX_WINDOWS_GDI32_LIB}"
  )
endif()
set_target_properties(crtgfx PROPERTIES
  OUTPUT_NAME crtgfx
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

add_library(crtgfx_shared SHARED
  $<TARGET_OBJECTS:crtgfx_common_objects>
  $<TARGET_OBJECTS:crtgfx_backend_objects>
)
target_include_directories(crtgfx_shared PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(crtgfx_shared PRIVATE crt_build_flags ${CRTGFX_CRT_SHARED_LIBS})
if(CRT_TARGET_OS STREQUAL "windows")
  target_sources(crtgfx_shared PRIVATE $<TARGET_OBJECTS:crt_windows_dllcrt>)
  target_link_libraries(crtgfx_shared PRIVATE
    "${CRT_WINDOWS_KERNEL32_LIB}"
    "${CRTGFX_WINDOWS_USER32_LIB}"
    "${CRTGFX_WINDOWS_GDI32_LIB}"
  )
endif()
crt_configure_shared_runtime(crtgfx_shared)
set_target_properties(crtgfx_shared PROPERTIES
  OUTPUT_NAME crtgfx
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)
if(CRT_TARGET_OS STREQUAL "windows")
  set_target_properties(crtgfx_shared PROPERTIES
    ARCHIVE_OUTPUT_NAME crtgfx_dll
  )
endif()

install(TARGETS crtgfx crtgfx_shared
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include
  PATTERN ".gitkeep" EXCLUDE
)

add_executable(crtgfx_window_smoke
  ${CRT_STARTUP_OBJECTS}
  tests/window_smoke_test.c
  ${CRT_STARTUP_END_OBJECTS}
)
target_link_libraries(crtgfx_window_smoke PRIVATE crt_build_flags c crtgfx)
target_include_directories(crtgfx_window_smoke PRIVATE
  "${CMAKE_SOURCE_DIR}/libc/include"
)
target_link_libraries(crtgfx_window_smoke PRIVATE crt_compiler_rt_builtins)
target_link_options(crtgfx_window_smoke PRIVATE
  -nostdlib
  -nostartfiles
  -nodefaultlibs
)
if(CRT_TARGET_OS STREQUAL "macos")
  target_link_options(crtgfx_window_smoke PRIVATE
    -Wl,-e,_start
    -Wl,-dead_strip
  )
  target_link_libraries(crtgfx_window_smoke PRIVATE System)
elseif(CRT_TARGET_OS STREQUAL "linux")
  target_link_options(crtgfx_window_smoke PRIVATE
    -Wl,-e,_start
    -Wl,--gc-sections
  )
elseif(CRT_TARGET_OS STREQUAL "windows")
  target_link_options(crtgfx_window_smoke PRIVATE
    -Wl,/entry:mainCRTStartup
    -Wl,/subsystem:console
    -Wl,/OPT:REF
  )
  target_link_libraries(crtgfx_window_smoke PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
endif()
add_dependencies(crtgfx_window_smoke c crt1)

add_test(NAME crtgfx_window_smoke_runs COMMAND crtgfx_window_smoke)
set_tests_properties(crtgfx_window_smoke_runs PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PASS_REGULAR_EXPRESSION "crtgfx_window_smoke: ok"
)
set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtgfx_window_smoke_runs)

add_executable(crtgfx_window_demo
  ${CRT_STARTUP_OBJECTS}
  tools/window_demo.c
  ${CRT_STARTUP_END_OBJECTS}
)
target_link_libraries(crtgfx_window_demo PRIVATE crt_build_flags c crtgfx)
target_include_directories(crtgfx_window_demo PRIVATE
  "${CMAKE_SOURCE_DIR}/libc/include"
)
target_link_libraries(crtgfx_window_demo PRIVATE crt_compiler_rt_builtins)
target_link_options(crtgfx_window_demo PRIVATE
  -nostdlib
  -nostartfiles
  -nodefaultlibs
)
if(CRT_TARGET_OS STREQUAL "macos")
  target_link_options(crtgfx_window_demo PRIVATE
    -Wl,-e,_start
    -Wl,-dead_strip
  )
  target_link_libraries(crtgfx_window_demo PRIVATE System)
elseif(CRT_TARGET_OS STREQUAL "linux")
  target_link_options(crtgfx_window_demo PRIVATE
    -Wl,-e,_start
    -Wl,--gc-sections
  )
elseif(CRT_TARGET_OS STREQUAL "windows")
  target_link_options(crtgfx_window_demo PRIVATE
    -Wl,/entry:mainCRTStartup
    -Wl,/subsystem:console
    -Wl,/OPT:REF
  )
  target_link_libraries(crtgfx_window_demo PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
endif()
add_dependencies(crtgfx_window_demo c crt1)
