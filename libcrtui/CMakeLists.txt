# libcrtui: the CRT-owned application UI API (stage 05-ui). Tranche 0 ships the
# frozen contract (include/crtui/ui.h) and its headless, resource-free model
# implementation (src/core.c); LVGL arrives privately in Tranche 1 and never
# appears in an installed header. See docs/crtui_acceptance.md.

# Networking-style optional dependency gate (like CRTMEDIA_ENABLE_CURL): the
# LVGL-backed renderer is compiled only when CRTUI_ENABLE_LVGL is ON, so an
# ordinary configure never needs the ~28 MB extracted LVGL tree. Without it
# crtui_window_render() returns CRTUI_ERROR_UNSUPPORTED and the headless model
# is unchanged. Fetch the pinned source with the crtui-lvgl-fetch target.
option(CRTUI_ENABLE_LVGL "Build crtui's private LVGL software renderer" OFF)
set(CRTUI_LVGL_RECIPE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lvgl/recipe.json")
set(CRTUI_LVGL_FETCH_DIR "${CMAKE_BINARY_DIR}/lvgl" CACHE PATH "Where tools/fetch_lvgl.py extracts LVGL")
set(CRTUI_LVGL_ROOT "${CRTUI_LVGL_FETCH_DIR}/lvgl-9.6.0" CACHE PATH "Extracted LVGL source root")
set(CRTUI_LVGL_SOURCES)
set(CRTUI_LVGL_PRIVATE_SOURCES)
if(Python3_Interpreter_FOUND)
  add_custom_target(crtui-lvgl-fetch
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/fetch_lvgl.py"
      --recipe "${CRTUI_LVGL_RECIPE}" --dest "${CRTUI_LVGL_FETCH_DIR}"
      --cache "${CRTUI_LVGL_FETCH_DIR}/.downloads"
    COMMENT "Fetching the pinned LVGL release (libcrtui/third_party/lvgl/recipe.json)"
    USES_TERMINAL
  )
endif()
if(CRTUI_ENABLE_LVGL)
  if(NOT EXISTS "${CRTUI_LVGL_ROOT}/include/lvgl/lvgl.h")
    message(FATAL_ERROR "CRTUI_ENABLE_LVGL=ON requires the extracted LVGL source at ${CRTUI_LVGL_ROOT}. Run the crtui-lvgl-fetch target, then reconfigure.")
  endif()
  file(GLOB_RECURSE CRTUI_LVGL_SOURCES CONFIGURE_DEPENDS "${CRTUI_LVGL_ROOT}/src/*.c")
  set(CRTUI_LVGL_PRIVATE_SOURCES src/lvgl_backend.c)
  # Third-party code: no warnings-as-errors, LVGL reads only crtui's lv_conf.h.
  set(CRTUI_LVGL_COMPILE_OPTIONS -w -DLV_CONF_INCLUDE_SIMPLE=1 -fvisibility=hidden)
  if(CRT_TARGET_OS STREQUAL "windows")
    # Same reason as crtmedia's curl sources: this sysroot has no <windows.h>,
    # so the mingw predefined Win32 macros must not steer LVGL into it.
    list(APPEND CRTUI_LVGL_COMPILE_OPTIONS -U_WIN32 -U_WIN32_WCE -U__WIN32__ -UWIN32 -U__MINGW32__)
  endif()
  set_source_files_properties(${CRTUI_LVGL_SOURCES} PROPERTIES
    COMPILE_OPTIONS "${CRTUI_LVGL_COMPILE_OPTIONS}"
    INCLUDE_DIRECTORIES "${CRTUI_LVGL_ROOT};${CRTUI_LVGL_ROOT}/include;${CRTUI_LVGL_ROOT}/src;${CMAKE_CURRENT_SOURCE_DIR}/src/lvgl_conf"
  )
  set_source_files_properties(${CRTUI_LVGL_PRIVATE_SOURCES} PROPERTIES
    COMPILE_OPTIONS "-DLV_CONF_INCLUDE_SIMPLE=1"
    INCLUDE_DIRECTORIES "${CRTUI_LVGL_ROOT}/include;${CMAKE_CURRENT_SOURCE_DIR}/src/lvgl_conf"
  )
endif()

add_library(crtui STATIC
  src/core.c
  src/crtgfx_adapter.c
  ${CRTUI_LVGL_SOURCES}
  ${CRTUI_LVGL_PRIVATE_SOURCES}
)
if(CRTUI_ENABLE_LVGL)
  target_compile_definitions(crtui PRIVATE CRTUI_HAVE_LVGL=1)
endif()
# Private LVGL (Tranche 3): hidden by default, only CRTUI_API functions are
# exported (see include/crtui/api.h). On Windows the dllexport attribute is
# only wanted on the DLL, never on objects that land in a consumer's executable.
target_compile_options(crtui PRIVATE -fvisibility=hidden)
if(NOT CRT_TARGET_OS STREQUAL "windows")
  target_compile_definitions(crtui PRIVATE CRTUI_BUILD_LIBRARY=1)
endif()
target_link_libraries(crtui PRIVATE crt_build_flags c m dl cxx)
target_include_directories(crtui PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
# crtui/crtgfx.h reads crtgfx_event, so the adapter needs crtgfx's headers (no link).
target_include_directories(crtui PUBLIC "${CMAKE_SOURCE_DIR}/libcrtgfx/include")
set_target_properties(crtui PROPERTIES
  OUTPUT_NAME crtui
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

add_library(crtui_shared SHARED
  src/core.c
  src/crtgfx_adapter.c
  ${CRTUI_LVGL_SOURCES}
  ${CRTUI_LVGL_PRIVATE_SOURCES}
)
if(CRTUI_ENABLE_LVGL)
  target_compile_definitions(crtui_shared PRIVATE CRTUI_HAVE_LVGL=1)
endif()
target_compile_options(crtui_shared PRIVATE -fvisibility=hidden)
target_compile_definitions(crtui_shared PRIVATE CRTUI_BUILD_LIBRARY=1)
if(CRT_TARGET_OS STREQUAL "windows")
  target_sources(crtui_shared PRIVATE $<TARGET_OBJECTS:crt_windows_dllcrt>)
endif()
target_link_libraries(crtui_shared PRIVATE crt_build_flags c_shared m_shared dl_shared cxx_shared)
target_include_directories(crtui_shared PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_include_directories(crtui_shared PUBLIC "${CMAKE_SOURCE_DIR}/libcrtgfx/include")
crt_configure_shared_runtime(crtui_shared)
if(CRT_TARGET_OS STREQUAL "macos")
  # Bind libc symbols (pthread_*, malloc, ...) to the CRT's libc.dylib, not Apple's
  # libSystem. crt_configure_shared_runtime() leaves -lSystem ahead of libc.dylib,
  # and two-level namespace binding takes the first dylib that defines a symbol,
  # so libcrtui would call Apple's 64-byte pthread_mutex_init() on the CRT's
  # 40-byte pthread_mutex_t embedded in a crtui context: a heap overflow that
  # aborted crtui_context_destroy() in an externally built sample (Tranche 3
  # macOS replay). Naming libc.dylib first fixes the binding; libc.dylib still
  # loads libSystem itself.
  target_link_options(crtui_shared PRIVATE "$<TARGET_FILE:c_shared>")
endif()
set_target_properties(crtui_shared PROPERTIES
  OUTPUT_NAME crtui
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  # Exports come from CRTUI_API only: exporting everything would publish every
  # LVGL symbol from the DLL.
  WINDOWS_EXPORT_ALL_SYMBOLS OFF
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

# crtui_layout_test: Tranche 4 acceptance -- the CRT-owned layout engine (Row,
# Column, Stack, free containers, grow/align/margin/padding/gap/main_align,
# ScrollView and List), the style properties, and the v1 widget behaviors
# (Switch, Checkbox, TextInput, Image), against the headless model. LVGL-free.
add_executable(crtui_layout_test
  ${CRT_STARTUP_OBJECTS}
  tests/layout_test.c
)
target_link_libraries(crtui_layout_test PRIVATE crt_build_flags c crtui)
target_include_directories(crtui_layout_test PRIVATE
  "${CMAKE_SOURCE_DIR}/libc/include"
)
target_link_libraries(crtui_layout_test PRIVATE crt_compiler_rt_builtins)
target_link_options(crtui_layout_test PRIVATE
  -nostdlib
  -nostartfiles
  -nodefaultlibs
)
if(CRT_TARGET_OS STREQUAL "macos")
  target_link_options(crtui_layout_test PRIVATE
    -Wl,-e,_start
    -Wl,-dead_strip
  )
  target_link_libraries(crtui_layout_test PRIVATE System)
elseif(CRT_TARGET_OS STREQUAL "linux")
  target_link_options(crtui_layout_test PRIVATE
    -Wl,-e,_start
    -Wl,--gc-sections
  )
elseif(CRT_TARGET_OS STREQUAL "windows")
  target_link_options(crtui_layout_test PRIVATE
    -Wl,/entry:mainCRTStartup
    -Wl,/subsystem:console
    -Wl,/OPT:REF
  )
  target_link_libraries(crtui_layout_test PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
endif()
crt_link_startup_end_objects(crtui_layout_test)
add_dependencies(crtui_layout_test c crt1 crtui)

add_test(NAME crtui_layout_test_runs COMMAND crtui_layout_test)
set_tests_properties(crtui_layout_test_runs PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PASS_REGULAR_EXPRESSION "crtui_layout_test: ok"
  TIMEOUT 60
)
set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtui_layout_test_runs)

# crtui_input_test: Tranche 2 acceptance -- synthesized crtgfx_event values ->
# crtui through the crtgfx adapter (keyboard, pointer, wheel, slider drag, resize
# and re-layout, focus loss and recovery). Headless and LVGL-free, so it runs on
# every host; it only needs crtgfx's headers, which crtui already exports.
add_executable(crtui_input_test
  ${CRT_STARTUP_OBJECTS}
  tests/input_test.c
)
target_link_libraries(crtui_input_test PRIVATE crt_build_flags c crtui)
target_include_directories(crtui_input_test PRIVATE
  "${CMAKE_SOURCE_DIR}/libc/include"
)
target_link_libraries(crtui_input_test PRIVATE crt_compiler_rt_builtins)
target_link_options(crtui_input_test PRIVATE
  -nostdlib
  -nostartfiles
  -nodefaultlibs
)
if(CRT_TARGET_OS STREQUAL "macos")
  target_link_options(crtui_input_test PRIVATE
    -Wl,-e,_start
    -Wl,-dead_strip
  )
  target_link_libraries(crtui_input_test PRIVATE System)
elseif(CRT_TARGET_OS STREQUAL "linux")
  target_link_options(crtui_input_test PRIVATE
    -Wl,-e,_start
    -Wl,--gc-sections
  )
elseif(CRT_TARGET_OS STREQUAL "windows")
  target_link_options(crtui_input_test PRIVATE
    -Wl,/entry:mainCRTStartup
    -Wl,/subsystem:console
    -Wl,/OPT:REF
  )
  target_link_libraries(crtui_input_test PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
endif()
crt_link_startup_end_objects(crtui_input_test)
add_dependencies(crtui_input_test c crt1 crtui)

add_test(NAME crtui_input_test_runs COMMAND crtui_input_test)
set_tests_properties(crtui_input_test_runs PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PASS_REGULAR_EXPRESSION "crtui_input_test: ok"
  TIMEOUT 60
)
set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtui_input_test_runs)

# crtui_contract_shared_test (macOS): the same contract test, linked against the
# installed-style libcrtui.dylib instead of libcrtui.a. The static-link tests
# above cannot notice which libc a *shared* crtui binds to; this one aborted with
# a heap overflow before the link-order fix above (docs/crtui_acceptance.md,
# Tranche 3 macOS replay), and it proves the dylib exports the whole contract API.
if(CRT_TARGET_OS STREQUAL "macos")
  add_executable(crtui_contract_shared_test
    ${CRT_STARTUP_OBJECTS}
    tests/contract_test.c
  )
  target_link_libraries(crtui_contract_shared_test PRIVATE crt_build_flags c crtui_shared)
  target_include_directories(crtui_contract_shared_test PRIVATE
    "${CMAKE_SOURCE_DIR}/libc/include"
  )
  target_link_libraries(crtui_contract_shared_test PRIVATE crt_compiler_rt_builtins)
  target_link_options(crtui_contract_shared_test PRIVATE
    -nostdlib
    -nostartfiles
    -nodefaultlibs
    -Wl,-e,_start
    -Wl,-dead_strip
  )
  target_link_libraries(crtui_contract_shared_test PRIVATE System)
  crt_link_startup_end_objects(crtui_contract_shared_test)
  add_dependencies(crtui_contract_shared_test c crt1 crtui_shared)

  add_test(NAME crtui_contract_shared_test_runs COMMAND crtui_contract_shared_test)
  set_tests_properties(crtui_contract_shared_test_runs PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    PASS_REGULAR_EXPRESSION "crtui_contract_test: ok"
    TIMEOUT 60
  )
  set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtui_contract_shared_test_runs)
endif()

# crtui_render_test: Tranche 1 acceptance -- renders a Window > Container >
# Label/Button/Slider/Progress scene through the private LVGL software renderer
# into memory and checks real pixels, then cycles create/render/destroy with a
# native handle/thread audit. Built only when the renderer is (CRTUI_ENABLE_LVGL).
if(CRTUI_ENABLE_LVGL)
  add_executable(crtui_render_test
    ${CRT_STARTUP_OBJECTS}
    tests/render_test.c
  )
  target_link_libraries(crtui_render_test PRIVATE crt_build_flags c crtui)
  target_include_directories(crtui_render_test PRIVATE
    "${CMAKE_SOURCE_DIR}/libc/include"
  )
  target_link_libraries(crtui_render_test PRIVATE crt_compiler_rt_builtins)
  target_link_options(crtui_render_test PRIVATE
    -nostdlib
    -nostartfiles
    -nodefaultlibs
  )
  if(CRT_TARGET_OS STREQUAL "macos")
    target_link_options(crtui_render_test PRIVATE
      -Wl,-e,_start
      -Wl,-dead_strip
    )
    target_link_libraries(crtui_render_test PRIVATE System)
  elseif(CRT_TARGET_OS STREQUAL "linux")
    target_link_options(crtui_render_test PRIVATE
      -Wl,-e,_start
      -Wl,--gc-sections
    )
  elseif(CRT_TARGET_OS STREQUAL "windows")
    target_link_options(crtui_render_test PRIVATE
      -Wl,/entry:mainCRTStartup
      -Wl,/subsystem:console
      -Wl,/OPT:REF
    )
    target_link_libraries(crtui_render_test PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
  endif()
  crt_link_startup_end_objects(crtui_render_test)
  add_dependencies(crtui_render_test c crt1 crtui)

  add_test(NAME crtui_render_test_runs COMMAND crtui_render_test)
  set_tests_properties(crtui_render_test_runs PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    PASS_REGULAR_EXPRESSION "crtui_render_test: ok"
    TIMEOUT 120
  )
  set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtui_render_test_runs)
endif()

# crtui_window_demo: Tranche 1 present path -- the same scene rendered into a
# real crtgfx window's software framebuffer. Not a ctest (it needs a display);
# run it directly with a frame limit: `crtui_window_demo 30`.
if(CRTUI_ENABLE_LVGL)
  add_executable(crtui_window_demo
    ${CRT_STARTUP_OBJECTS}
    tools/window_demo.c
  )
  # crtgfx_window *before* crtui (and a trailing `c`): on Linux crtgfx_window
  # pulls libxkbcommon.a, which needs libc's strndup/asprintf/mmap, and static
  # archive order matters -- with crtui first, CMake hoisted crtui's c/m/dl/cxx
  # ahead of libxkbcommon.a, so the linker fell through to the host libc
  # (Linux Tranche 1 replay: undefined 'strndup@@GLIBC_2.2.5'). This order puts
  # the final libc.a after it, as crtgfx_window_demo's own link does.
  target_link_libraries(crtui_window_demo PRIVATE crt_build_flags crtgfx_window crtui c)
  target_include_directories(crtui_window_demo PRIVATE
    "${CMAKE_SOURCE_DIR}/libc/include"
  )
  target_link_libraries(crtui_window_demo PRIVATE crt_compiler_rt_builtins)
  target_link_options(crtui_window_demo PRIVATE
    -nostdlib
    -nostartfiles
    -nodefaultlibs
  )
  if(CRT_TARGET_OS STREQUAL "macos")
    target_link_options(crtui_window_demo PRIVATE -Wl,-e,_start -Wl,-dead_strip)
    target_link_libraries(crtui_window_demo PRIVATE System)
  elseif(CRT_TARGET_OS STREQUAL "linux")
    target_link_options(crtui_window_demo PRIVATE -Wl,-e,_start -Wl,--gc-sections)
  elseif(CRT_TARGET_OS STREQUAL "windows")
    target_link_options(crtui_window_demo PRIVATE
      -Wl,/entry:mainCRTStartup -Wl,/subsystem:console -Wl,/OPT:REF)
    target_link_libraries(crtui_window_demo PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
  endif()
  crt_link_startup_end_objects(crtui_window_demo)
  add_dependencies(crtui_window_demo c crt1 crtui)
endif()

# Installed sample (Tranche 3): examples/ui-basic is the application-level view
# of crtui -- this same demo source, a standalone project that includes only
# crtui and crtgfx headers, and the prebuilt binary.
if(CRTUI_ENABLE_LVGL)
  install(TARGETS crtui_window_demo
    RUNTIME DESTINATION examples/bin COMPONENT crt-ui
  )
  install(FILES tools/window_demo.c
    DESTINATION examples/ui-basic RENAME main.c COMPONENT crt-ui
  )
  install(FILES "${CMAKE_SOURCE_DIR}/examples/ui-basic/CMakeLists.txt"
    DESTINATION examples/ui-basic COMPONENT crt-ui
  )
endif()

# crtui_privacy_test: Tranche 3 -- LVGL stays private. Headers name no LVGL, the
# shared library exports only crtui_* functions (every declared one), and the
# sample application's own objects and source reference no lv_* symbol. Needs
# llvm-nm/llvm-readobj from the compiler's LLVM directory.
if(CRTUI_ENABLE_LVGL AND Python3_Interpreter_FOUND)
  get_filename_component(CRTUI_LLVM_BIN "${CMAKE_C_COMPILER}" DIRECTORY)
  add_test(NAME crtui_privacy_test_runs COMMAND
    "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/check_crtui_privacy.py"
      --llvm-bin "${CRTUI_LLVM_BIN}"
      --headers "${CMAKE_CURRENT_SOURCE_DIR}/include"
      --shared "$<TARGET_FILE:crtui_shared>"
      --object "$<TARGET_OBJECTS:crtui_window_demo>"
      --example-source "${CMAKE_CURRENT_SOURCE_DIR}/tools/window_demo.c"
  )
  set_tests_properties(crtui_privacy_test_runs PROPERTIES
    PASS_REGULAR_EXPRESSION "crtui_privacy: ok"
    TIMEOUT 60
  )
  set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtui_privacy_test_runs)
endif()
