add_library(crtmedia STATIC
  src/runtime.c
  src/frame.c
  src/frame_convert.c
)
target_link_libraries(crtmedia PRIVATE crt_build_flags c m dl cxx)
target_include_directories(crtmedia PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
set_target_properties(crtmedia PROPERTIES
  OUTPUT_NAME crtmedia
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

add_library(crtmedia_shared SHARED
  src/runtime.c
  src/frame.c
  src/frame_convert.c
)
if(CRT_TARGET_OS STREQUAL "windows")
  target_sources(crtmedia_shared PRIVATE $<TARGET_OBJECTS:crt_windows_dllcrt>)
endif()
target_link_libraries(crtmedia_shared PRIVATE crt_build_flags c_shared m_shared dl_shared cxx_shared)
target_include_directories(crtmedia_shared PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
crt_configure_shared_runtime(crtmedia_shared)
set_target_properties(crtmedia_shared PROPERTIES
  OUTPUT_NAME crtmedia
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)
if(CRT_TARGET_OS STREQUAL "windows")
  set_target_properties(crtmedia_shared PROPERTIES
    ARCHIVE_OUTPUT_NAME crtmedia_dll
  )
endif()

install(TARGETS crtmedia crtmedia_shared
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include
  PATTERN ".gitkeep" EXCLUDE
)

# crtmedia_frame_test: deterministic, host-resource-free coverage for the
# CPU frame handoff contract (crtmedia/frame.h, TODO.md's "Define the
# libcrtmedia CPU frame handoff contract" item) -- plane geometry,
# ownership/release, and packed-RGB/YUV420P->RGBA conversion. Mirrors
# libcrtgfx's own crtgfx_synthetic_event target shape (tests/synthetic_
# event_test.c, libcrtgfx/CMakeLists.txt): same startup objects, same
# per-OS link options, same crt_link_startup_end_objects()/add_
# dependencies() calls, since both are plain C executables needing no
# real host window/display connection. The real-Skia counterpart (tests/
# frame_skia_smoke.cc) is built and registered from libcrtgfx/CMakeLists.
# txt instead, not here -- see that file's own comment on why (crt_wire_
# skia_executable() is a function() defined in that file's own directory
# scope, and CMake function definitions do not propagate to a sibling
# directory's CMakeLists.txt).
add_executable(crtmedia_frame_test
  ${CRT_STARTUP_OBJECTS}
  tests/frame_test.c
)
target_link_libraries(crtmedia_frame_test PRIVATE crt_build_flags c crtmedia)
target_include_directories(crtmedia_frame_test PRIVATE
  "${CMAKE_SOURCE_DIR}/libc/include"
)
target_link_libraries(crtmedia_frame_test PRIVATE crt_compiler_rt_builtins)
target_link_options(crtmedia_frame_test PRIVATE
  -nostdlib
  -nostartfiles
  -nodefaultlibs
)
if(CRT_TARGET_OS STREQUAL "macos")
  target_link_options(crtmedia_frame_test PRIVATE
    -Wl,-e,_start
    -Wl,-dead_strip
  )
  target_link_libraries(crtmedia_frame_test PRIVATE System)
elseif(CRT_TARGET_OS STREQUAL "linux")
  target_link_options(crtmedia_frame_test PRIVATE
    -Wl,-e,_start
    -Wl,--gc-sections
  )
elseif(CRT_TARGET_OS STREQUAL "windows")
  target_link_options(crtmedia_frame_test PRIVATE
    -Wl,/entry:mainCRTStartup
    -Wl,/subsystem:console
    -Wl,/OPT:REF
  )
  target_link_libraries(crtmedia_frame_test PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
endif()
crt_link_startup_end_objects(crtmedia_frame_test)
add_dependencies(crtmedia_frame_test c crt1 crtmedia)

add_test(NAME crtmedia_frame_test_runs COMMAND crtmedia_frame_test)
set_tests_properties(crtmedia_frame_test_runs PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PASS_REGULAR_EXPRESSION "crtmedia_frame_test: ok"
)
set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtmedia_frame_test_runs)
