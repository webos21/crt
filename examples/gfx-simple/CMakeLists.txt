cmake_minimum_required(VERSION 3.25)
project(crtgfx_window_example C)

if(NOT CMAKE_SYSROOT)
  message(FATAL_ERROR "configure with the CRT distribution's crt-toolchain.cmake")
endif()

find_path(CRTGFX_INCLUDE_DIR crtgfx/window.h
  PATHS "${CMAKE_SYSROOT}/include" NO_DEFAULT_PATH REQUIRED)
if(WIN32)
  find_library(CRTGFX_WINDOW_LIBRARY NAMES crtgfx_dll
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
else()
  if(NOT APPLE)
    # Linux CRT executables embed the static runtime and hide its symbols
    # from real host DSOs.  Prefer the matching static window archive too;
    # the shared library otherwise pulls its libc++ dependency graph into
    # this C-only example and exposes the same host-DSO symbol-preemption
    # boundary already handled by the packaged gfx-gpu example.
    list(INSERT CMAKE_FIND_LIBRARY_SUFFIXES 0 "${CMAKE_STATIC_LIBRARY_SUFFIX}")
  endif()
  find_library(CRTGFX_WINDOW_LIBRARY NAMES crtgfx
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
endif()

add_executable(crtgfx_window_example main.c)
target_include_directories(crtgfx_window_example PRIVATE "${CRTGFX_INCLUDE_DIR}")
target_link_libraries(crtgfx_window_example PRIVATE "${CRTGFX_WINDOW_LIBRARY}")

if(NOT WIN32 AND NOT APPLE)
  # libcrtgfx.a records no transitive dependency metadata.  Reproduce the
  # installed simple-stage window target's link interface explicitly.
  target_link_libraries(crtgfx_window_example PRIVATE
    "${CMAKE_SYSROOT}/lib/libxkbcommon.a")

  # Real host libwayland-client.so (2026-09-22, found rebuilding this exact
  # example from a genuinely extracted Linux 03-gfx-simple release archive
  # for the first time -- docs/release_preview.md's own quick start had
  # never actually been run from a downloaded Linux archive before). The
  # comment this replaced claimed "native Wayland protocol glue first
  # appears in the later media/GPU stage", which does not match
  # libcrtgfx/CMakeLists.txt's own real gate: src/arch/linux/
  # window_wayland_native.c is compiled into crtgfx_window_backend_objects
  # whenever CRTGFX_LINUX_VULKAN_LIB is found at configure time (a Vulkan
  # loader, not a stage choice), which a normal Advanced-Graphics-capable
  # Linux dev host already has, so 03-gfx-simple's own packaged libcrtgfx.a
  # already carries real wl_proxy_.../wl_display_... undefined references --
  # confirmed for real: this example failed to link with exactly those
  # undefined references before this fix. Same NO_CMAKE_FIND_ROOT_PATH +
  # dpkg-architecture multiarch find_file() fallback already established in
  # examples/gfx-gpu/CMakeLists.txt's own CRTGFX_EXAMPLE_WAYLAND_CLIENT_LIB
  # (this project's own crt-toolchain.cmake sets CMAKE_SYSROOT, which skips
  # the executable-link ABI probe that would otherwise populate
  # CMAKE_LIBRARY_ARCHITECTURE).
  find_library(CRTGFX_EXAMPLE_WAYLAND_CLIENT_LIB NAMES wayland-client wayland-client.so.0
    NO_CMAKE_FIND_ROOT_PATH)
  if(NOT CRTGFX_EXAMPLE_WAYLAND_CLIENT_LIB)
    find_program(CRT_DPKG_ARCHITECTURE_EXECUTABLE dpkg-architecture)
    if(CRT_DPKG_ARCHITECTURE_EXECUTABLE)
      execute_process(
        COMMAND "${CRT_DPKG_ARCHITECTURE_EXECUTABLE}" -qDEB_HOST_MULTIARCH
        OUTPUT_VARIABLE CRT_HOST_MULTIARCH_TRIPLET
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
      )
    endif()
    if(CRT_HOST_MULTIARCH_TRIPLET)
      unset(CRTGFX_EXAMPLE_WAYLAND_CLIENT_LIB CACHE)
      find_file(CRTGFX_EXAMPLE_WAYLAND_CLIENT_LIB
        NAMES libwayland-client.so.0
        PATHS /lib /usr/lib /usr/local/lib
        PATH_SUFFIXES "${CRT_HOST_MULTIARCH_TRIPLET}"
      )
    endif()
  endif()
  if(NOT CRTGFX_EXAMPLE_WAYLAND_CLIENT_LIB)
    message(FATAL_ERROR "Could not find a real host libwayland-client.so/.so.0")
  endif()
  get_filename_component(CRT_EXAMPLE_WAYLAND_CLIENT_LIB_DIR "${CRTGFX_EXAMPLE_WAYLAND_CLIENT_LIB}" DIRECTORY)
  target_link_options(crtgfx_window_example PRIVATE
    "-Wl,-rpath-link,${CRT_EXAMPLE_WAYLAND_CLIENT_LIB_DIR}")
  # libxdg-shell-protocol.a: the generated xdg-shell protocol glue
  # window_wayland_native.c also calls into (xdg_wm_base_interface,
  # xdg_surface_ack_configure, ...) -- a separate redistributed archive
  # from libcrtgfx.a itself, same as examples/gfx-gpu/CMakeLists.txt's own
  # identical link list.
  target_link_libraries(crtgfx_window_example PRIVATE
    "${CMAKE_SYSROOT}/lib/libxdg-shell-protocol.a"
    "${CRTGFX_EXAMPLE_WAYLAND_CLIENT_LIB}")

  target_link_options(crtgfx_window_example PRIVATE -Wl,--exclude-libs,ALL)
endif()
