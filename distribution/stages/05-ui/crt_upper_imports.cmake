# Installed 05-ui link interface for standalone WebView consumers. No upstream
# source, LVGL, or build-tree library may be found by fallback here.
foreach(name crtui crtui_skia crtgfx crtgfx_gpu crtgfx_skia crtmedia)
  if(NOT EXISTS "${CMAKE_SYSROOT}/lib/lib${name}.a")
    message(FATAL_ERROR "05-ui SDK is missing lib/lib${name}.a")
  endif()
  add_library(${name} STATIC IMPORTED GLOBAL)
  set_target_properties(${name} PROPERTIES
    IMPORTED_LOCATION "${CMAKE_SYSROOT}/lib/lib${name}.a"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SYSROOT}/include")
endforeach()
set(crt_skia_archives "${CMAKE_SYSROOT}/lib/libskia.a" "${CMAKE_SYSROOT}/lib/libfreetype.a")
set_property(TARGET crtui_skia PROPERTY INTERFACE_LINK_LIBRARIES crtui crtgfx_skia)
set_property(TARGET crtgfx_skia PROPERTY INTERFACE_LINK_LIBRARIES ${crt_skia_archives} crtgfx_gpu)
set_property(TARGET crtgfx_gpu PROPERTY INTERFACE_LINK_LIBRARIES crtgfx)
if(CRT_STAGE_TARGET_OS STREQUAL "linux")
  # Native host libraries are explicit PAL boundaries, never headers/runtime
  # for the app. A cross build must name the target's libraries, not the host's.
  if(CRT_STAGE_TARGET_ARCH STREQUAL "aarch64")
    set(crt_host_triplet aarch64-linux-gnu)
  else()
    set(crt_host_triplet x86_64-linux-gnu)
  endif()
  foreach(pair "vulkan|libvulkan.so.1" "wayland|libwayland-client.so.0"
               "va|libva.so.2" "va_drm|libva-drm.so.2")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 name)
    list(GET parts 1 file)
    find_file(crt_host_${name} NAMES "${file}"
      PATHS "${CRT_LINUX_SYSTEM_LIB_DIR}" "/usr/lib/${crt_host_triplet}" "/lib/${crt_host_triplet}"
      NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH REQUIRED)
  endforeach()
  set_property(TARGET crtgfx APPEND PROPERTY INTERFACE_LINK_LIBRARIES
    "${CMAKE_SYSROOT}/lib/libxkbcommon.a" "${CMAKE_SYSROOT}/lib/libxdg-shell-protocol.a" "${crt_host_wayland}")
  set_property(TARGET crtgfx_gpu APPEND PROPERTY INTERFACE_LINK_LIBRARIES "${crt_host_vulkan}")
  set_property(TARGET crtmedia APPEND PROPERTY INTERFACE_LINK_LIBRARIES
    -Wl,--start-group
    "${CMAKE_SYSROOT}/lib/libavformat.a" "${CMAKE_SYSROOT}/lib/libavcodec.a"
    "${CMAKE_SYSROOT}/lib/libswresample.a" "${CMAKE_SYSROOT}/lib/libavutil.a"
    "${CMAKE_SYSROOT}/lib/libcurl.a" "${CMAKE_SYSROOT}/lib/libmbedtls.a"
    "${CMAKE_SYSROOT}/lib/libmbedx509.a" "${CMAKE_SYSROOT}/lib/libmbedcrypto.a"
    "${CMAKE_SYSROOT}/lib/libz.a" -Wl,--end-group "${crt_host_va}" "${crt_host_va_drm}")
  add_link_options(-Wl,--exclude-libs,ALL -Wl,-Bsymbolic
    "-Wl,-rpath-link,/usr/lib/${crt_host_triplet}")
  if(CMAKE_CROSSCOMPILING AND CRT_LINUX_SYSTEM_LIB_DIR)
    # These are explicit target OS graphics imports, not a target libc SDK.
    # Their transitive host-runtime imports are resolved by the target OS,
    # never by adding a glibc sysroot to CRT's header/library search.
    add_link_options(-Wl,--allow-shlib-undefined)
  endif()
elseif(CRT_STAGE_TARGET_OS STREQUAL "windows")
  set_property(TARGET crtgfx APPEND PROPERTY INTERFACE_LINK_LIBRARIES -Wl,user32.lib -Wl,gdi32.lib -Wl,d3d11.lib -Wl,dxgi.lib)
  set_property(TARGET crtgfx_gpu APPEND PROPERTY INTERFACE_LINK_LIBRARIES -Wl,d3d12.lib)
  set_property(TARGET crtgfx_skia APPEND PROPERTY INTERFACE_LINK_LIBRARIES -Wl,d3dcompiler.lib -Wl,uuid.lib)
  set_property(TARGET crtmedia APPEND PROPERTY INTERFACE_LINK_LIBRARIES
    "${CMAKE_SYSROOT}/lib/libavformat.a" "${CMAKE_SYSROOT}/lib/libavcodec.a"
    "${CMAKE_SYSROOT}/lib/libswresample.a" "${CMAKE_SYSROOT}/lib/libavutil.a"
    "${CMAKE_SYSROOT}/lib/libcurl.a" "${CMAKE_SYSROOT}/lib/libmbedtls.a"
    "${CMAKE_SYSROOT}/lib/libmbedx509.a" "${CMAKE_SYSROOT}/lib/libmbedcrypto.a"
    "${CMAKE_SYSROOT}/lib/libz.a"
    -Wl,ole32.lib -Wl,d3d11.lib -Wl,dxgi.lib -Wl,mfuuid.lib -Wl,strmiids.lib
    -Wl,mfplat.lib -Wl,mfreadwrite.lib -Wl,mf.lib -Wl,uuid.lib)
elseif(CRT_STAGE_TARGET_OS STREQUAL "macos")
  set_property(TARGET crtgfx APPEND PROPERTY INTERFACE_LINK_LIBRARIES
    "-framework Foundation" "-framework AppKit" "-framework QuartzCore" "-framework CoreGraphics" objc)
  set_property(TARGET crtgfx_gpu APPEND PROPERTY INTERFACE_LINK_LIBRARIES "-framework Metal" "-framework IOSurface")
  set_property(TARGET crtgfx_skia APPEND PROPERTY INTERFACE_LINK_LIBRARIES "-framework CoreVideo")
  set_property(TARGET crtmedia APPEND PROPERTY INTERFACE_LINK_LIBRARIES
    "${CMAKE_SYSROOT}/lib/libavformat.a" "${CMAKE_SYSROOT}/lib/libavcodec.a"
    "${CMAKE_SYSROOT}/lib/libswresample.a" "${CMAKE_SYSROOT}/lib/libavutil.a"
    "${CMAKE_SYSROOT}/lib/libcurl.a" "${CMAKE_SYSROOT}/lib/libmbedtls.a"
    "${CMAKE_SYSROOT}/lib/libmbedx509.a" "${CMAKE_SYSROOT}/lib/libmbedcrypto.a"
    "${CMAKE_SYSROOT}/lib/libz.a"
    "-framework AudioToolbox" "-framework VideoToolbox" "-framework CoreMedia"
    "-framework CoreVideo" "-framework CoreFoundation" "-framework AVFoundation"
    "-framework Foundation" objc)
endif()
