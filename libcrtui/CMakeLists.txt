# libcrtui: the CRT-owned application UI API (stage 05-ui). Tranche 0 ships the
# frozen contract (include/crtui/ui.h) and its headless, resource-free model
# implementation (src/core.c); LVGL arrives privately in Tranche 1 and never
# appears in an installed header. See docs/crtui_acceptance.md.

add_library(crtui STATIC
  src/core.c
)
target_link_libraries(crtui PRIVATE crt_build_flags c m dl cxx)
target_include_directories(crtui PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
set_target_properties(crtui PROPERTIES
  OUTPUT_NAME crtui
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

add_library(crtui_shared SHARED
  src/core.c
)
if(CRT_TARGET_OS STREQUAL "windows")
  target_sources(crtui_shared PRIVATE $<TARGET_OBJECTS:crt_windows_dllcrt>)
endif()
target_link_libraries(crtui_shared PRIVATE crt_build_flags c_shared m_shared dl_shared cxx_shared)
target_include_directories(crtui_shared PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
crt_configure_shared_runtime(crtui_shared)
set_target_properties(crtui_shared PROPERTIES
  OUTPUT_NAME crtui
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)
if(CRT_TARGET_OS STREQUAL "windows")
  set_target_properties(crtui_shared PROPERTIES
    ARCHIVE_OUTPUT_NAME crtui_dll
  )
endif()

install(TARGETS crtui crtui_shared
  ARCHIVE DESTINATION lib COMPONENT crt-ui
  LIBRARY DESTINATION lib COMPONENT crt-ui
  RUNTIME DESTINATION bin COMPONENT crt-ui
)
install(DIRECTORY include/ DESTINATION include
  COMPONENT crt-ui
  PATTERN ".gitkeep" EXCLUDE
)

# crtui_contract_test: the resource-free acceptance test for the frozen
# Tranche 0 contract -- thread ownership, handle lifetime, event dispatch and
# re-entrancy, focus and modal routing, hit-testing and clipping, geometry and
# resize, and error codes. No window, GPU, display, or LVGL.
add_executable(crtui_contract_test
  ${CRT_STARTUP_OBJECTS}
  tests/contract_test.c
)
target_link_libraries(crtui_contract_test PRIVATE crt_build_flags c crtui)
target_include_directories(crtui_contract_test PRIVATE
  "${CMAKE_SOURCE_DIR}/libc/include"
)
target_link_libraries(crtui_contract_test PRIVATE crt_compiler_rt_builtins)
target_link_options(crtui_contract_test PRIVATE
  -nostdlib
  -nostartfiles
  -nodefaultlibs
)
if(CRT_TARGET_OS STREQUAL "macos")
  target_link_options(crtui_contract_test PRIVATE
    -Wl,-e,_start
    -Wl,-dead_strip
  )
  target_link_libraries(crtui_contract_test PRIVATE System)
elseif(CRT_TARGET_OS STREQUAL "linux")
  target_link_options(crtui_contract_test PRIVATE
    -Wl,-e,_start
    -Wl,--gc-sections
  )
elseif(CRT_TARGET_OS STREQUAL "windows")
  target_link_options(crtui_contract_test PRIVATE
    -Wl,/entry:mainCRTStartup
    -Wl,/subsystem:console
    -Wl,/OPT:REF
  )
  target_link_libraries(crtui_contract_test PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
endif()
crt_link_startup_end_objects(crtui_contract_test)
add_dependencies(crtui_contract_test c crt1 crtui)

add_test(NAME crtui_contract_test_runs COMMAND crtui_contract_test)
set_tests_properties(crtui_contract_test_runs PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PASS_REGULAR_EXPRESSION "crtui_contract_test: ok"
  TIMEOUT 60
)
set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtui_contract_test_runs)
