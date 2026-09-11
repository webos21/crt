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
  # A third real, confirmed bug (2026-09-11), found right after the two
  # above: this executable's own freestanding link (crt-toolchain.cmake's
  # --sysroot=/-nostdinc/no default -lc) makes GNU ld default to
  # --no-allow-shlib-undefined (unlike a `-shared` library, which
  # defaults the other way) -- it insists on verifying every symbol each
  # linked .so's own transitive NEEDED entries would provide, and can
  # only do that by actually locating those files or (for a symbol with
  # no NEEDED entry pointing anywhere at all) by that symbol's provider
  # being explicitly present on this same link line. CRTGFX_GPU_LIBRARY
  # (libcrtgfx_gpu.so) itself needs the real host libvulkan.so --
  # `-rpath-link` (below) is the standard, sufficient fix for that, since
  # ld only needs to *locate* a real NEEDED entry it already knows about.
  # CRTGFX_WINDOW_LIBRARY (libcrtgfx.so) additionally needs libc++.so ->
  # libc++abi.so.1's own `_Unwind_*` symbols -- and `-rpath-link` alone
  # does NOT fix this half: `libstdc++/third_party/libcxxabi/recipe.json`
  # deliberately builds libc++abi against the portable Itanium `_Unwind_*`
  # ABI surface without linking a concrete unwinder itself
  # (LIBCXXABI_USE_LLVM_UNWINDER=OFF, see that recipe's own notes) --
  # confirmed directly via `readelf -d`: libc++abi.so.1 carries no NEEDED
  # entry for libunwind.so at all, so there is nothing for `-rpath-link`
  # to even locate on this symbol's behalf. Upstream's usual answer is
  # libc++'s own ABI linker script (`INPUT(libc++.so.1 -lc++abi
  # -lunwind)`) auto-adding `-lunwind` for any `-lc++`/libc++.so
  # consumer at link time -- but this project's own imported libcxx
  # recipe now builds with `LIBCXX_ENABLE_ABI_LINKER_SCRIPT=OFF` (see
  # that recipe's own notes: the script itself is not a real, loadable
  # ELF, which broke *runtime* loading for anything with libc++.so as a
  # literal NEEDED entry) -- trading that auto-add away as a direct
  # consequence. Fixed the same way any consumer of a libc++abi built
  # this way normally must: link libunwind.so directly and explicitly,
  # matching CRTGFX_GPU_LIBRARY/CRTGFX_WINDOW_LIBRARY's own already-
  # established explicit-.so-path convention in this same file.
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
  target_link_libraries(crtgfx_gpu_example PRIVATE "${CMAKE_SYSROOT}/lib/libunwind.so")
endif()

