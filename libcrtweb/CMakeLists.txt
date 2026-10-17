# libcrtweb (06-web). This in-tree project owns the wire/consumer tests for the external-surface
# contract. libcrtweb, crtweb_host and the WebKit-side gates are built separately by
# tools/build_webkit_crt_port.py against the installed CRT SDK; the native WPE reference and
# prototype have their own tools/build_webkit_wpe_*.py entry points.
# The frozen wire contract's sizes, offsets and numbers are checked on every host (plain C, no socket).
add_crt_test(crtweb_wire_layout_test tests/platform-crt/wire_layout_test.c)
target_include_directories(crtweb_wire_layout_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/platform")
set_tests_properties(crtweb_wire_layout_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "wire_layout_test: ok")

add_crt_test(crtweb_gpu_wire_test tests/platform-crt/gpu_wire_test.c)
target_include_directories(crtweb_gpu_wire_test PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/platform" "${CMAKE_SOURCE_DIR}/libcrtgfx/include")
set_tests_properties(crtweb_gpu_wire_test_runs PROPERTIES TIMEOUT 30
  PASS_REGULAR_EXPRESSION "gpu_wire_test: ok")

# The same contract over its real transport (SOCK_SEQPACKET, SCM_RIGHTS, a memfd), in one process and between
# two. Needs no browser, window or Skia; on Windows it runs over the CRT's own AF_UNIX layer.
add_crt_test(crtweb_wire_transport_test tests/platform-crt/wire_transport_test.c)
target_include_directories(crtweb_wire_transport_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/platform")
set_tests_properties(crtweb_wire_transport_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "wire_transport_test: ok"
  TIMEOUT 120)

# Linux, macOS and Windows: the transport is SOCK_SEQPACKET (native on Linux, CRT-framed on macOS and
# on Windows), and each host builds the consumer against its own Skia tree.
if(TARGET crtui_skia AND TARGET crtgfx_skia AND
   (CRT_TARGET_OS STREQUAL "linux" OR CRT_TARGET_OS STREQUAL "macos" OR CRT_TARGET_OS STREQUAL "windows"))
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
  if(CRT_TARGET_OS STREQUAL "macos" AND CRT_USE_IMPORTED_LIBCXX)
    # The host SDK's libc++ headers are what in-tree C++ compiles see by default, but this test links
    # the imported libc++.a: with the SDK headers `std::string` is laid out differently in the
    # inline accessors than in the archive's out-of-line members (assign() then size() read 0).
    # Use the imported headers, as Windows does for Skia code.
    target_compile_options(crtweb_web_surface_test PRIVATE
      "-isystem${CRT_LIBCXX_SYSROOT}/include/c++/v1"
      "-isystem${CMAKE_SOURCE_DIR}/libc/include"
      "-isystem${CMAKE_SOURCE_DIR}/include")
    add_dependencies(crtweb_web_surface_test crt-libcxx-sysroot)
  endif()
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
  # Web Tranche 4: the crtui WebView (crtweb::WebView) against a real crtweb_host. The host is built by
  # tools/build_webkit_crt_port.py (the PlatformCRT port); without CRTWEB_HOST the test reports
  # "skipped" and the driver's webview gate runs it with the host.
  add_executable(crtweb_webview_test
    ${CRT_STARTUP_OBJECTS}
    tests/platform-crt/webview_test.cc
  )
  target_include_directories(crtweb_webview_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/platform")
  target_link_libraries(crtweb_webview_test PRIVATE
    crt_cxx_build_flags crtui_skia crtgfx_skia crtmedia)
  target_compile_definitions(crtweb_webview_test PRIVATE
    CRTGFX_HAVE_VULKAN=1 SK_USE_INTERNAL_VULKAN_HEADERS)
  if(CRT_TARGET_OS STREQUAL "macos" AND CRT_USE_IMPORTED_LIBCXX)
    target_compile_options(crtweb_webview_test PRIVATE
      "-isystem${CRT_LIBCXX_SYSROOT}/include/c++/v1"
      "-isystem${CMAKE_SOURCE_DIR}/libc/include"
      "-isystem${CMAKE_SOURCE_DIR}/include")
    add_dependencies(crtweb_webview_test crt-libcxx-sysroot)
  endif()
  crt_wire_skia_executable(crtweb_webview_test SKIP_MACOS_DEAD_STRIP)
  crt_link_startup_end_objects(crtweb_webview_test)
  add_dependencies(crtweb_webview_test cxx c crt1 crtui_skia crtgfx_skia)
  add_test(NAME crtweb_webview_test_runs COMMAND crtweb_webview_test)
  set_tests_properties(crtweb_webview_test_runs PROPERTIES
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    PASS_REGULAR_EXPRESSION "crtweb_webview_test: (ok|skipped)"
    TIMEOUT 180
  )
  set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtweb_webview_test_runs)

  # Tranche 9A: real browser frames through the GPU final compositor. CTest
  # records unavailable prerequisites as a skip, never as GPU acceptance; the
  # web driver supplies CRTWEB_HOST and --require-gpu for strict evidence.
  if(CRTGFX_ENABLE_GPU_BACKEND)
    add_executable(crtweb_webview_gpu_test
      ${CRT_STARTUP_OBJECTS}
      tests/platform-crt/webview_gpu_test.cc)
    target_include_directories(crtweb_webview_gpu_test PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/platform")
    target_link_libraries(crtweb_webview_gpu_test PRIVATE
      crt_cxx_build_flags crtui_skia crtgfx_skia crtmedia)
    if(CRT_TARGET_OS STREQUAL "windows")
      target_compile_definitions(crtweb_webview_gpu_test PRIVATE CRTGFX_HAVE_D3D12=1)
    elseif(CRT_TARGET_OS STREQUAL "linux")
      target_compile_definitions(crtweb_webview_gpu_test PRIVATE CRTGFX_HAVE_VULKAN=1 SK_USE_INTERNAL_VULKAN_HEADERS)
    elseif(CRT_TARGET_OS STREQUAL "macos")
      target_compile_definitions(crtweb_webview_gpu_test PRIVATE CRTGFX_HAVE_METAL=1)
    endif()
    if(CRT_TARGET_OS STREQUAL "macos" AND CRT_USE_IMPORTED_LIBCXX)
      target_compile_options(crtweb_webview_gpu_test PRIVATE
        "-isystem${CRT_LIBCXX_SYSROOT}/include/c++/v1"
        "-isystem${CMAKE_SOURCE_DIR}/libc/include" "-isystem${CMAKE_SOURCE_DIR}/include")
      add_dependencies(crtweb_webview_gpu_test crt-libcxx-sysroot)
    endif()
    crt_wire_skia_executable(crtweb_webview_gpu_test SKIP_MACOS_DEAD_STRIP)
    crt_link_startup_end_objects(crtweb_webview_gpu_test)
    add_dependencies(crtweb_webview_gpu_test cxx c crt1 crtui_skia crtgfx_skia)
    add_test(NAME crtweb_webview_gpu_test_runs COMMAND crtweb_webview_gpu_test)
    set_tests_properties(crtweb_webview_gpu_test_runs PROPERTIES
      WORKING_DIRECTORY "${CMAKE_BINARY_DIR}" SKIP_RETURN_CODE 77 TIMEOUT 240)
    set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtweb_webview_gpu_test_runs)

    # Tranche 9B: a CRT-owned GPU producer, deliberately not linked to WebKit.
    # Browser CPU pixels are uploaded here; only worker -> app uses shared GPU memory.
    foreach(gpu_target IN ITEMS crtweb_gpu_worker crtweb_gpu_resource_test)
      if(gpu_target STREQUAL "crtweb_gpu_worker")
        set(gpu_source tools/crtweb_gpu_worker.cc)
      else()
        set(gpu_source tests/platform-crt/gpu_resource_test.cc)
      endif()
      add_executable(${gpu_target} ${CRT_STARTUP_OBJECTS} ${gpu_source})
      target_include_directories(${gpu_target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/platform")
      target_link_libraries(${gpu_target} PRIVATE
        crt_cxx_build_flags crtui_skia crtgfx_skia crtmedia)
      if(CRT_TARGET_OS STREQUAL "linux")
        target_compile_definitions(${gpu_target} PRIVATE CRTGFX_HAVE_VULKAN=1 SK_USE_INTERNAL_VULKAN_HEADERS)
      elseif(CRT_TARGET_OS STREQUAL "windows")
        target_compile_definitions(${gpu_target} PRIVATE CRTGFX_HAVE_D3D12=1)
      elseif(CRT_TARGET_OS STREQUAL "macos")
        target_compile_definitions(${gpu_target} PRIVATE CRTGFX_HAVE_METAL=1)
      endif()
      if(CRT_TARGET_OS STREQUAL "macos" AND CRT_USE_IMPORTED_LIBCXX)
        target_compile_options(${gpu_target} PRIVATE
          "-isystem${CRT_LIBCXX_SYSROOT}/include/c++/v1"
          "-isystem${CMAKE_SOURCE_DIR}/libc/include" "-isystem${CMAKE_SOURCE_DIR}/include")
        add_dependencies(${gpu_target} crt-libcxx-sysroot)
      endif()
      crt_wire_skia_executable(${gpu_target} SKIP_MACOS_DEAD_STRIP)
      crt_link_startup_end_objects(${gpu_target})
      add_dependencies(${gpu_target} cxx c crt1 crtui_skia crtgfx_skia)
    endforeach()
    add_dependencies(crtweb_gpu_resource_test crtweb_gpu_worker)
    add_test(NAME crtweb_gpu_resource_test_runs COMMAND crtweb_gpu_resource_test)
    set_tests_properties(crtweb_gpu_resource_test_runs PROPERTIES
      WORKING_DIRECTORY "${CMAKE_BINARY_DIR}" SKIP_RETURN_CODE 77 TIMEOUT 240)
    set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtweb_gpu_resource_test_runs)
    add_test(NAME crtweb_gpu_lifecycle_test_runs COMMAND crtweb_gpu_resource_test --lifecycle)
    set_tests_properties(crtweb_gpu_lifecycle_test_runs PROPERTIES
      WORKING_DIRECTORY "${CMAKE_BINARY_DIR}" SKIP_RETURN_CODE 77 TIMEOUT 300)
    set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtweb_gpu_lifecycle_test_runs)
  endif()
endif()
