# libcrtweb (06-web). Not a library yet: Tranche 3A only adds the consumer-side test of the frozen
# frame-producer/input contracts (docs/acceptance/crtweb_acceptance.md, Tranche 3). The WebKit
# side (the "crt" WPEPlatform module, the JSC and WPE reference builds) is built by
# tools/build_webkit_*.py against the native WPE build, not by this CMake project.
if(TARGET crtui_skia AND TARGET crtgfx_skia AND CRT_TARGET_OS STREQUAL "linux")
  add_executable(crtweb_web_surface_test
    ${CRT_STARTUP_OBJECTS}
    tests/platform-crt/web_surface_test.cc
  )
  target_include_directories(crtweb_web_surface_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/platform")
  target_link_libraries(crtweb_web_surface_test PRIVATE
    crt_cxx_build_flags crtui_skia crtgfx_skia crtmedia)
  target_compile_definitions(crtweb_web_surface_test PRIVATE
    CRTGFX_HAVE_VULKAN=1 SK_USE_INTERNAL_VULKAN_HEADERS)
  crt_wire_skia_executable(crtweb_web_surface_test SKIP_MACOS_DEAD_STRIP)
  crt_link_startup_end_objects(crtweb_web_surface_test)
  add_dependencies(crtweb_web_surface_test cxx c crt1 crtui_skia crtgfx_skia)

  add_test(NAME crtweb_web_surface_test_runs COMMAND crtweb_web_surface_test)
  set_tests_properties(crtweb_web_surface_test_runs PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    PASS_REGULAR_EXPRESSION "crtweb_web_surface_test: (ok|skipped)"
    TIMEOUT 120
  )
  set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtweb_web_surface_test_runs)
endif()
