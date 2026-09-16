cmake_minimum_required(VERSION 3.25)
project(crtgfx_gpu_example C)

if(NOT CMAKE_SYSROOT)
  message(FATAL_ERROR "configure with the CRT distribution's crt-toolchain.cmake")
endif()

find_path(CRTGFX_INCLUDE_DIR crtgfx/gpu.h
  PATHS "${CMAKE_SYSROOT}/include" NO_DEFAULT_PATH REQUIRED)
if(WIN32)
  find_library(CRTGFX_GPU_LIBRARY NAMES crtgfx_gpu_dll
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
  find_library(CRTGFX_WINDOW_LIBRARY NAMES crtgfx_dll
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
else()
  if(NOT APPLE)
    # Linux-only: prefer the static archives (.a) over the identically-
    # named shared libraries also installed under ${CMAKE_SYSROOT}/lib --
    # a sixth real, confirmed bug (2026-09-13), found chasing the fifth
    # one's own `-Wl,--exclude-libs,ALL` fix (below) failing to actually
    # resolve a real GPU device in the full stage build despite fixing
    # the standalone repro. crt-cc's own default (non-`-shared`) link
    # mode is built entirely around embedding everything statically and
    # scrubbing this executable's own dynamic symbol table via that flag
    # -- but find_library()'s default suffix order (.so before .a) picks
    # the *shared* libcrtgfx(_gpu).so here instead, which carries its own
    # NEEDED entry on this project's own SHARED libc.so (confirmed via
    # `readelf -d libcrtgfx.so`). That shared libc.so re-introduces the
    # exact readdir()/opendir() collision `--exclude-libs,ALL` was
    # supposed to prevent, in a form the flag cannot reach: it only
    # suppresses symbols contributed by STATIC ARCHIVES linked into
    # *this* link command, never symbols a separate, already-built
    # shared object exports on its own -- confirmed directly: with the
    # flag applied, `readelf --dyn-syms` on this project's own libc.so
    # still shows readdir/opendir as GLOBAL DEFAULT. Further confirmed
    # via `readelf -V` that libc.so carries *no* symbol version
    # information at all, while the real host libvulkan.so.1 looks up
    # the versioned `readdir@GLIBC_2.17` -- glibc's dynamic linker treats
    # an unversioned definition as satisfying *any* versioned lookup, so
    # this project's own libc.so still wins over the real, correctly-
    # versioned host libc.so.6. Preferring the static archives avoids
    # linking any shared libcrtgfx*.so at all, so no transitive shared
    # libc.so is ever pulled in, and the executable's own combined
    # static-archive symbols are fully covered by `--exclude-libs,ALL`
    # (below), matching crt-cc's intended default-mode design.
    list(INSERT CMAKE_FIND_LIBRARY_SUFFIXES 0 "${CMAKE_STATIC_LIBRARY_SUFFIX}")
  endif()
  find_library(CRTGFX_GPU_LIBRARY NAMES crtgfx_gpu
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
  find_library(CRTGFX_WINDOW_LIBRARY NAMES crtgfx
    PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH REQUIRED)
endif()

add_executable(crtgfx_gpu_example main.c)
target_include_directories(crtgfx_gpu_example PRIVATE "${CRTGFX_INCLUDE_DIR}")
# CRTGFX_WINDOW_LIBRARY (crtgfx_window_create/_destroy/_poll_event/...) is
# required too, not just CRTGFX_GPU_LIBRARY -- a real, confirmed bug
# (2026-09-11), first hit building this example against a real packaged
# SDK for the first time (the isolated 03-gfx-simple -> 04-gfx-media
# stage upgrade's own build_example() step). find_library()'s NAMES
# crtgfx_gpu here resolves to the *shared* libcrtgfx_gpu.dylib/.so
# (matched ahead of the static libcrtgfx_gpu.a by CMAKE_FIND_LIBRARY_
# SUFFIXES' default ordering), which itself depends on libcrtgfx.dylib/
# .so at ITS OWN link time (crtgfx_gpu_shared links crtgfx_window_shared
# PUBLIC) -- but that is an ordinary load-time dependency, not a
# re-export, so ld64/ld.lld's two-level-namespace symbol resolution does
# not search into it when resolving *this* executable's own undefined
# crtgfx_window_* symbols: only libraries listed directly on this link's
# own command line count. Confirmed directly: "Undefined symbols ...
# _crtgfx_window_create/_crtgfx_window_destroy/_crtgfx_window_poll_
# event/..." building crtgfx_gpu_example this way for the first time.
target_link_libraries(crtgfx_gpu_example PRIVATE
  "${CRTGFX_GPU_LIBRARY}" "${CRTGFX_WINDOW_LIBRARY}")
if(NOT WIN32 AND NOT APPLE)
  # Linux selects the static crtgfx archives above, and tools/crt-cc adds
  # the static CRT closure. Keep this executable free of direct shared CRT
  # dependencies; only the host Vulkan and Wayland boundaries below should
  # survive in DT_NEEDED.
  find_library(CRTGFX_EXAMPLE_VULKAN_LIB NAMES vulkan vulkan.so.1
    NO_CMAKE_FIND_ROOT_PATH)
  if(NOT CRTGFX_EXAMPLE_VULKAN_LIB)
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
      unset(CRTGFX_EXAMPLE_VULKAN_LIB CACHE)
      find_file(CRTGFX_EXAMPLE_VULKAN_LIB
        NAMES libvulkan.so libvulkan.so.1
        PATHS /lib /usr/lib /usr/local/lib
        PATH_SUFFIXES "${CRT_HOST_MULTIARCH_TRIPLET}"
      )
    endif()
  endif()
  target_link_options(crtgfx_gpu_example PRIVATE
    "-Wl,-rpath-link,${CMAKE_SYSROOT}/lib")
  if(CRTGFX_EXAMPLE_VULKAN_LIB)
    get_filename_component(CRT_EXAMPLE_VULKAN_LIB_DIR "${CRTGFX_EXAMPLE_VULKAN_LIB}" DIRECTORY)
    target_link_options(crtgfx_gpu_example PRIVATE
      "-Wl,-rpath-link,${CRT_EXAMPLE_VULKAN_LIB_DIR}")
  endif()
  if(NOT CRTGFX_EXAMPLE_VULKAN_LIB)
    message(FATAL_ERROR "Could not find a real host libvulkan.so/.so.1")
  endif()
  # An eighth real, confirmed bug (2026-09-13), found immediately after
  # fixing the sixth/seventh (CMAKE_FIND_LIBRARY_SUFFIXES/Vulkan+
  # libxkbcommon.a, above) once CRTGFX_HAVE_NATIVE_WAYLAND was actually
  # enabled for the predecessor SDK's own libcrtgfx.a for the first time:
  # window_wayland_native.c.o (also already inside libcrtgfx.a) calls real
  # libwayland-client symbols (wl_display_connect/wl_proxy_marshal_flags/
  # .../xdg_surface_interface/...) this standalone project's own plain
  # find_library()-based link never supplies, for the exact same
  # propagation-bypass reason as the seventh bug -- see
  # distribution/stages/04-gfx-media/CMakeLists.txt's own matching
  # crtgfx_window INTERFACE_LINK_LIBRARIES fix for the fuller story,
  # including why the *real host* libwayland-client.so.0 (not a project-
  # own freestanding-built one) is required: Vulkan WSI drivers are host
  # shared objects using the host Wayland client's own private wl_display
  # layout, which the native presentation backend must match. libxdg-
  # shell-protocol.a (generated protocol-marshaling glue, no ABI
  # sensitivity, unlike libwayland-client.so itself) is a real
  # redistributed SDK dependency exactly like libxkbcommon.a (see
  # create_dist.py's own copy_wayland_xdg_shell()).
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
  target_link_options(crtgfx_gpu_example PRIVATE
    "-Wl,-rpath-link,${CRT_EXAMPLE_WAYLAND_CLIENT_LIB_DIR}")
  # A seventh real, confirmed bug (2026-09-13), found right after the
  # sixth (CMAKE_FIND_LIBRARY_SUFFIXES, above): once CRTGFX_GPU_LIBRARY/
  # CRTGFX_WINDOW_LIBRARY resolve to the *static* archives instead, the
  # link fails outright with dozens of undefined references --
  # vkCreateInstance/vkEnumeratePhysicalDevices/.../gpu_vulkan.c.o's own
  # real Vulkan calls, and xkb_context_new/xkb_state_.../window_wayland.
  # c.o's own real libxkbcommon calls. Both symbol sets used to resolve
  # "for free" while linking the *shared* libcrtgfx(_gpu).so, because a
  # shared library's own real NEEDED entries (recorded at ITS OWN link
  # time) get silently walked and satisfied by ld's default `--allow-
  # shlib-undefined`-for-transitive-needs handling -- but a static
  # archive records no such thing: its members are just object code with
  # plain undefined symbol references, which this standalone project's
  # own plain find_library()-based link (bypassing the real `crtgfx_gpu`/
  # `crtgfx_window` CMake targets' own INTERFACE_LINK_LIBRARIES entirely,
  # see this file's fifth-bug comment above) never supplies. The in-tree/
  # in-stage crtgfx_gpu CMake target already carries both as real link
  # dependencies (crtgfx_gpu_targets.cmake's own `target_link_libraries(
  # crtgfx_gpu PRIVATE ${CRTGFX_LINUX_VULKAN_LIB})`, and distribution/
  # stages/04-gfx-media/CMakeLists.txt's own crtgfx_window INTERFACE_
  # LINK_LIBRARIES re-listing libxkbcommon.a) -- CMake's own PRIVATE-
  # dependency-of-a-STATIC-library propagation forwards both onto every
  # real in-tree/in-stage consumer's link line automatically, which is
  # exactly the propagation this standalone project's plain find_library()
  # bypasses and must replicate by hand, same as every other fix in this
  # file. Order matters here (ld only pulls a static archive member to
  # satisfy an *already outstanding* undefined symbol): libxkbcommon.a
  # must come after libcrtgfx.a (CRTGFX_WINDOW_LIBRARY, linked above) on
  # the command line, which appending here achieves; libvulkan.so, being
  # a real shared library, has no such ordering constraint.
  target_link_libraries(crtgfx_gpu_example PRIVATE
    "${CRTGFX_EXAMPLE_VULKAN_LIB}" "${CMAKE_SYSROOT}/lib/libxkbcommon.a"
    "${CMAKE_SYSROOT}/lib/libxdg-shell-protocol.a" "${CRTGFX_EXAMPLE_WAYLAND_CLIENT_LIB}")
  # A fifth real bug (2026-09-13) -- turned out to be the SAME one
  # libcrtgfx/cmake/crtgfx_gpu_targets.cmake's own `crtgfx_gpu` INTERFACE
  # target already found and fixed on 2026-09-03, just never reachable
  # here: this example resolves CRTGFX_GPU_LIBRARY/CRTGFX_WINDOW_LIBRARY
  # via plain find_library() file paths, not the real `crtgfx_gpu` CMake
  # target, so that target's own INTERFACE `-Wl,--exclude-libs,ALL`
  # never propagates onto this executable's own link line the way it
  # does for every in-tree/in-stage consumer. Symptom looked identical
  # to (and was originally misdiagnosed here as) a separate, deeper
  # "real host library corrupts its own glibc calls" architecture
  # problem: `crtgfx_gpu_window_demo: no usable GPU backend on this host
  # (rc=0, device_count=0)` even with confirmed working `/dev/dri`
  # permissions. Confirmed for real with a minimal standalone repro
  # (a bare C program calling only vkCreateInstance/
  # vkEnumeratePhysicalDevices against this same real
  # `/lib/aarch64-linux-gnu/libvulkan.so.1`): built via crt-cc without
  # this flag, `vkCreateInstance` returns `VK_ERROR_INCOMPATIBLE_DRIVER`
  # (-9); `readelf --dyn-syms` on that binary shows `readdir`/`opendir`
  # exported as GLOBAL DEFAULT symbols (crt-cc's own default, non-
  # `-shared` link mode statically embeds libc.a's own implementations
  # of them, unhidden); `LD_DEBUG=bindings` running it shows
  # `libvulkan.so.1`'s own internal calls to `readdir`/`opendir`
  # (`[GLIBC_2.17]`-versioned) binding to *this executable itself*
  # instead of real glibc -- corrupting the real loader's own
  # `/usr/share/vulkan/icd.d` manifest scan (confirmed via `strace`:
  # the filenames it then tries to open are corrupted fragments, the
  # signature of a `struct dirent` read at the wrong offsets). Adding
  # this exact flag to the same standalone repro made both calls
  # succeed (`rc=0, count=1`, the real `llvmpipe` device `vulkaninfo`
  # also reports) -- matches `crtgfx_gpu_targets.cmake`'s own fix and
  # comment for the identical root cause exactly. Necessary but NOT
  # sufficient on its own here, though: see the sixth bug's own comment
  # above (CMAKE_FIND_LIBRARY_SUFFIXES) for the transitive shared libc.so
  # wrinkle this flag cannot reach by itself.
  target_link_options(crtgfx_gpu_example PRIVATE -Wl,--exclude-libs,ALL)
endif()
