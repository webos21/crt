# Shared target-construction logic for crtgfx_gpu/crtgfx_gpu_shared, used by
# both the in-tree libcrtgfx build and the standalone
# distribution/stages/04-gfx-media/CMakeLists.txt. Unlike
# crtgfx_gpu_sources.cmake (pure file-name data), this file contains real
# add_library()/target_link_libraries() calls -- the exact link-order/flag
# fixes below each represent a real bug found and fixed on real hardware
# (see each comment's own date/story), so this file exists specifically so
# there is only ONE place they can be applied, not two that can drift.
#
# Split into two functions, called from two different points in each
# caller's own file (mirroring the in-tree file's own original ordering
# exactly, not reordered here): crt_add_crtgfx_gpu_common_targets() creates
# crtgfx_gpu_common_objects/crtgfx_gpu_backend_objects/crtgfx_gpu (STATIC),
# and must be called once crtgfx_window already exists.
# crt_add_crtgfx_gpu_shared_target() creates crtgfx_gpu_shared, and must be
# called once crtgfx_window_shared already exists -- a separate, later call
# site in both callers, since crtgfx_window_shared is only defined after
# crtgfx_skia in the in-tree file today. Splitting this way avoids any
# question of whether CMake's same-directory forward-reference tolerance
# for target_link_libraries() would have covered a single, earlier,
# consolidated call instead.
#
# Both functions expect these variables already set by the caller before
# they are called:
#   CRT_TARGET_OS                  "windows"/"macos"/"linux"
#   CRTGFX_ROOT                    directory containing include/ and src/
#                                   (libcrtgfx itself in-tree; the bundled
#                                   libcrtgfx copy under CRT_STAGE_SOURCE_ROOT
#                                   for the standalone stage build)
#   CRTGFX_GPU_SOURCES             the "common" GPU sources (in-tree: set to
#                                   CRTGFX_GPU_COMMON_SOURCES from
#                                   crtgfx_gpu_sources.cmake; matches this
#                                   project's own already-established
#                                   variable name, kept as-is here rather
#                                   than renamed to avoid an unnecessary
#                                   alias)
#   CRTGFX_GPU_BACKEND_SOURCES     caller-populated per-OS (see
#                                   crtgfx_gpu_sources.cmake's own top
#                                   comment on why the Linux find_library()
#                                   gating itself stays out of this file)
#   CRTGFX_GPU_BACKEND_ENABLED     true only when a concrete backend is
#                                   selected for this configuration
#   CRTGFX_GPU_BACKEND_DEFINITIONS target-private OS/feature definitions
#                                   needed by common dispatch, the owner, and
#                                   direct Skia consumers
#   CRTGFX_CRT_STATIC_LIBS         real in-tree target names (c m dl cxx) or
#                                   an EMPTY list when the crt-cc/crt-c++
#                                   wrapper already supplies these
#                                   transparently (the standalone stage
#                                   build's own case -- see crt-cc's own
#                                   default link path)
#   CRTGFX_CRT_SHARED_LIBS         same shape, shared-runtime member names
#   CRTGFX_WINDOWS_DXGI_LIB        Windows only; a real absolute path
#                                   (in-tree) or bare "dxgi.lib" (standalone
#                                   -- crt-cc/crt-c++ forward
#                                   CRT_WINDOWS_SDK_LIBPATH as their own
#                                   /libpath, so a bare import-library name
#                                   already resolves)
#   CRTGFX_WINDOWS_D3D12_LIB       same shape, d3d12.lib
#   CRTGFX_LINUX_VULKAN_LIB        Linux only; find_library() result or ""
#
# In-tree-only steps (crt_build_flags, crt_windows_dllcrt,
# crt_configure_shared_runtime()) are each guarded by if(TARGET ...)/
# if(COMMAND ...) so the exact same function safely no-ops them when called
# from the standalone stage project, where none of those exist -- the
# crt-cc/crt-c++ wrapper already supplies their real equivalents (compiler-rt
# builtins, DLL crt objects, startup/end objects) transparently at link time
# there, matching distribution/stages/03-gfx-simple/CMakeLists.txt's own
# existing "the wrapper supplies these" precedent.
function(crt_add_crtgfx_gpu_common_targets)
  add_library(crtgfx_gpu_common_objects OBJECT ${CRTGFX_GPU_SOURCES})
  if(TARGET crt_build_flags)
    target_link_libraries(crtgfx_gpu_common_objects PRIVATE crt_build_flags)
  endif()
  target_include_directories(crtgfx_gpu_common_objects PUBLIC
    "${CRTGFX_ROOT}/include"
  )
  target_include_directories(crtgfx_gpu_common_objects PRIVATE
    "${CRTGFX_ROOT}/src"
  )
  if(CRTGFX_GPU_BACKEND_DEFINITIONS)
    target_compile_definitions(crtgfx_gpu_common_objects PRIVATE
      ${CRTGFX_GPU_BACKEND_DEFINITIONS})
  endif()

  set(CRTGFX_GPU_TARGET_OBJECTS $<TARGET_OBJECTS:crtgfx_gpu_common_objects>)
  if(CRTGFX_GPU_BACKEND_SOURCES)
    add_library(crtgfx_gpu_backend_objects OBJECT ${CRTGFX_GPU_BACKEND_SOURCES})
    if(CRTGFX_GPU_BACKEND_DEFINITIONS)
      target_compile_definitions(crtgfx_gpu_backend_objects PRIVATE
        ${CRTGFX_GPU_BACKEND_DEFINITIONS})
    endif()
    if(CRT_TARGET_OS STREQUAL "linux")
      if(TARGET crt_build_flags)
        target_link_libraries(crtgfx_gpu_backend_objects PRIVATE crt_build_flags)
      endif()
    elseif(CRT_TARGET_OS STREQUAL "windows")
      target_compile_definitions(crtgfx_gpu_backend_objects PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
      )
    endif()
    target_include_directories(crtgfx_gpu_backend_objects PUBLIC
      "${CRTGFX_ROOT}/include"
    )
    target_include_directories(crtgfx_gpu_backend_objects PRIVATE
      "${CRTGFX_ROOT}/src"
    )
    if(CRT_TARGET_OS STREQUAL "linux" AND CRTGFX_LINUX_VULKAN_LIB AND CRTGFX_WAYLAND_INSTALL_PREFIX)
      target_include_directories(crtgfx_gpu_backend_objects PRIVATE
        "${CRTGFX_WAYLAND_INSTALL_PREFIX}/include"
      )
    endif()
    set_target_properties(crtgfx_gpu_backend_objects PROPERTIES
      POSITION_INDEPENDENT_CODE ON
    )
    list(APPEND CRTGFX_GPU_TARGET_OBJECTS $<TARGET_OBJECTS:crtgfx_gpu_backend_objects>)
  endif()

  # --- crtgfx_gpu (STATIC): the GPU device/surface archive.
  add_library(crtgfx_gpu STATIC ${CRTGFX_GPU_TARGET_OBJECTS})
  target_link_libraries(crtgfx_gpu PUBLIC crtgfx_window)
  target_include_directories(crtgfx_gpu PUBLIC "${CRTGFX_ROOT}/include")
  if(CRTGFX_GPU_BACKEND_ENABLED AND CRT_TARGET_OS STREQUAL "linux" AND CRTGFX_LINUX_VULKAN_LIB)
    # find_library() sets a not-found result to the literal string
    # "...-NOTFOUND", not empty -- target_link_libraries() recognizes that
    # suffix and fails CMake's generate step outright if passed
    # unconditionally. Confirmed for real (2026-09-03, no libvulkan-dev on
    # a real CI leg).
    target_link_libraries(crtgfx_gpu PRIVATE ${CRTGFX_LINUX_VULKAN_LIB})
  endif()
  target_link_libraries(crtgfx_gpu PRIVATE ${CRTGFX_CRT_STATIC_LIBS})
  if(CRTGFX_GPU_BACKEND_ENABLED AND CRT_TARGET_OS STREQUAL "windows")
    # d3d12.lib: gpu_win32.c's own real device/queue/adapter creation.
    # dxgi.lib again here (also on crtgfx_window): gpu_win32.c's own real
    # adapter enumeration/swapchain code uses real IDXGIFactory/IDXGIAdapter
    # types too, independent of crtgfx_window's own D3D11-specific need.
    # d3dcompiler.lib deliberately NOT here -- Skia's own D3D12 Ganesh
    # backend is the real D3DCompile() consumer, so that goes on
    # crtgfx_skia instead.
    target_link_libraries(crtgfx_gpu PUBLIC
      "${CRTGFX_WINDOWS_DXGI_LIB}"
      "${CRTGFX_WINDOWS_D3D12_LIB}"
    )
  elseif(CRTGFX_GPU_BACKEND_ENABLED AND CRT_TARGET_OS STREQUAL "macos")
    # gpu_metal.c drives real Metal.framework the same objc_msgSend-direct
    # way window_cocoa.c drives AppKit. CoreGraphics again here (also on
    # crtgfx_window): gpu_metal.c's own CAMetalLayer/drawable sizing code
    # uses real CGRect/CGSize types too.
    target_link_libraries(crtgfx_gpu PUBLIC
      "-framework Metal"
      "-framework CoreGraphics"
    )
  endif()
  set_target_properties(crtgfx_gpu PROPERTIES
    OUTPUT_NAME crtgfx_gpu
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  )

  if(CRTGFX_GPU_BACKEND_ENABLED AND CRT_TARGET_OS STREQUAL "linux" AND CRTGFX_LINUX_VULKAN_LIB)
    # Real, confirmed-for-real bug (2026-09-03): the real system
    # libvulkan.so.1 loader calls real glibc readdir()/opendir()
    # internally to scan /usr/share/vulkan/icd.d/*.json for the actual GPU
    # driver (ICD) to load. This project's own statically-linked libc
    # exports its own GLOBAL readdir()/opendir()/closedir()/fdopendir()
    # symbols into every executable's dynamic symbol table; default ELF
    # symbol interposition rules let the main executable's own global
    # symbols silently override the same-named symbol libvulkan.so.1
    # resolves at runtime, corrupting its directory scan
    # (VK_ERROR_INCOMPATIBLE_DRIVER, misleadingly). -Wl,--exclude-libs,ALL
    # keeps symbols that came from a static archive out of the final
    # link's own dynamic symbol table, so libvulkan.so.1's internal libc
    # calls resolve to the real glibc it was linked against instead.
    # INTERFACE so it reaches every real consumer's own final link line
    # automatically.
    target_link_options(crtgfx_gpu INTERFACE -Wl,--exclude-libs,ALL)
  endif()
endfunction()

# --- crtgfx_gpu_shared (SHARED): the GPU device/surface DLL/.so/.dylib.
# Called once crtgfx_window_shared already exists (see this file's own top
# comment on why this is a separate function/call site from
# crt_add_crtgfx_gpu_common_targets() above, not a consolidated one).
function(crt_add_crtgfx_gpu_shared_target)
  set(CRTGFX_GPU_SHARED_OBJECTS $<TARGET_OBJECTS:crtgfx_gpu_common_objects>)
  if(TARGET crtgfx_gpu_backend_objects)
    list(APPEND CRTGFX_GPU_SHARED_OBJECTS $<TARGET_OBJECTS:crtgfx_gpu_backend_objects>)
  endif()
  add_library(crtgfx_gpu_shared SHARED ${CRTGFX_GPU_SHARED_OBJECTS})
  target_link_libraries(crtgfx_gpu_shared PUBLIC crtgfx_window_shared)
  target_include_directories(crtgfx_gpu_shared PUBLIC "${CRTGFX_ROOT}/include")
  if(CRTGFX_GPU_BACKEND_ENABLED AND CRT_TARGET_OS STREQUAL "linux" AND CRTGFX_LINUX_VULKAN_LIB)
    target_link_libraries(crtgfx_gpu_shared PRIVATE ${CRTGFX_LINUX_VULKAN_LIB})
  endif()
  # CRTGFX_CRT_SHARED_LIBS: unconditional on every real host -- a real
  # DLL/.so/.dylib link step must resolve its own libc references itself.
  # Confirmed for real (2026-09-08): scoping this to Linux only broke the
  # Windows crtgfx_window_shared link outright (undefined calloc/free/...).
  target_link_libraries(crtgfx_gpu_shared PRIVATE ${CRTGFX_CRT_SHARED_LIBS})
  if(TARGET crt_build_flags)
    target_link_libraries(crtgfx_gpu_shared PRIVATE crt_build_flags)
  endif()
  if(CRT_TARGET_OS STREQUAL "windows")
    if(TARGET crt_windows_dllcrt)
      target_sources(crtgfx_gpu_shared PRIVATE $<TARGET_OBJECTS:crt_windows_dllcrt>)
    endif()
    if(CRTGFX_GPU_BACKEND_ENABLED)
      target_link_libraries(crtgfx_gpu_shared PRIVATE
        "${CRTGFX_WINDOWS_DXGI_LIB}"
        "${CRTGFX_WINDOWS_D3D12_LIB}"
      )
    endif()
  elseif(CRTGFX_GPU_BACKEND_ENABLED AND CRT_TARGET_OS STREQUAL "macos")
    # objc: gpu_metal.c drives real Metal.framework via direct objc_
    # msgSend/objc_getClass/sel_registerName calls. The plain static
    # crtgfx_gpu never needed this listed explicitly (links crtgfx_window
    # PUBLIC, whose own PUBLIC macOS framework list already includes objc);
    # crtgfx_gpu_shared links crtgfx_window_shared PUBLIC instead, but that
    # target's own framework list is PRIVATE, so it never propagates here.
    # A real, confirmed bug (2026-09-09): crtgfx_gpu_shared's own real
    # .dylib link failed outright the first time anything actually forced
    # this shared library to build standalone (crt-gfx-media-dist).
    target_link_libraries(crtgfx_gpu_shared PRIVATE
      "-framework Metal"
      "-framework CoreGraphics"
      objc
    )
  endif()
  if(CRTGFX_GPU_BACKEND_ENABLED AND CRT_TARGET_OS STREQUAL "linux" AND CRTGFX_LINUX_VULKAN_LIB)
    # See crtgfx_gpu's own comment above for the real bug this fixes.
    # PRIVATE here, not INTERFACE: crtgfx_gpu_shared has its own real .so
    # link step (unlike the plain crtgfx_gpu static archive), so this only
    # needs to apply directly to it, not propagate further.
    target_link_options(crtgfx_gpu_shared PRIVATE -Wl,--exclude-libs,ALL)
  endif()
  if(CRTGFX_GPU_BACKEND_ENABLED AND CRT_TARGET_OS STREQUAL "windows")
    # Real, confirmed-for-real bug (2026-09-11/12, standalone 04-gfx-media
    # isolated stage build): WINDOWS_EXPORT_ALL_SYMBOLS ON (set below) makes
    # CMake pass lld's own --export-all-symbols, which exports every GLOBAL
    # symbol in this target's link -- including ones pulled in from a
    # static archive, not just this target's own real source objects.
    # In-tree, CRTGFX_CRT_SHARED_LIBS (c_shared/m_shared/dl_shared/
    # cxx_shared) is what crtgfx_gpu_shared links for libc, so any libc
    # symbol it needs resolves against a real shared libc DLL via an import
    # thunk, never a static-archive body -- nothing to over-export. The
    # standalone stage project has no such shared libc DLL to link
    # (CRTGFX_CRT_SHARED_LIBS is empty there, distribution/stages/
    # 04-gfx-media/CMakeLists.txt), but tools/crt-c++'s own wrapper
    # *unconditionally* appends the real static ${CRT_SYSROOT}/lib/libc.a
    # to every link line regardless of what CMake explicitly requested --
    # so libcrtgfx_gpu.dll ends up with real static
    # fopen/ftell/fseek/fwrite/fflush/fclose bodies baked in (pulled in by
    # whatever crtgfx_gpu's own sources reference from libc/src/stdio.c's
    # single translation unit) and then auto-exported. Any consumer that
    # also references the same names against its own implicit libc.a link
    # (every consumer, via that same wrapper -- here, Skia/FreeType inside
    # crtgfx_skia_raster_smoke.exe/crtgfx_skia_gpu_window_demo.exe, the only
    # two executables that link the SHARED crtgfx_gpu_shared transitively
    # via crtgfx_skia_shared PUBLIC crtgfx_gpu_shared, not the plain static
    # crtgfx_gpu archive crtgfx_gpu_window_demo.exe uses instead) sees two
    # real definitions of each -- one from libc.a directly, one from
    # libcrtgfx_gpu_dll.dll.a's own auto-generated import thunk -- and lld
    # correctly refuses to pick one ("duplicate symbol: fopen").
    #
    # -Wl,--exclude-libs,ALL (the Linux/Vulkan case's own fix, right above)
    # is an ELF-only lld option -- confirmed for real: lld's MinGW/PE
    # driver (what this project's Windows target actually links with)
    # rejects it outright ("unknown argument: --exclude-libs"; verified
    # directly via `ld.lld -m i386pep --help`, which lists
    # --exclude-symbols=<symbol[,...]> as the PE-target equivalent instead,
    # a real, narrower, per-symbol form of the same "don't auto-export
    # something pulled from a static archive" fix -- not a guess). One
    # -Wl,--exclude-symbols=<name> per name, not a single comma-joined
    # -Wl,--exclude-symbols=a,b,c: Clang's -Wl, splits its whole argument on
    # *every* comma before forwarding to the linker (confirmed directly via
    # `clang -###`), which would tear a comma-joined list into bogus
    # trailing positional arguments. Matches this file's/crt-cc's own
    # existing one-value-per-Wl, convention (kernel32.lib, dxgi.lib, ...).
    # Only the six names actually observed colliding are listed -- if a
    # future consumer pulls a different libc.a symbol through both paths,
    # extend this list the same evidence-driven way this one was found,
    # rather than pre-guessing a longer list now.
    target_link_options(crtgfx_gpu_shared PRIVATE
      -Wl,--exclude-symbols=fopen
      -Wl,--exclude-symbols=fclose
      -Wl,--exclude-symbols=fwrite
      -Wl,--exclude-symbols=fseek
      -Wl,--exclude-symbols=ftell
      -Wl,--exclude-symbols=fflush
    )
  endif()
  if(COMMAND crt_configure_shared_runtime)
    crt_configure_shared_runtime(crtgfx_gpu_shared)
  endif()
  set_target_properties(crtgfx_gpu_shared PROPERTIES
    OUTPUT_NAME crtgfx_gpu
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    WINDOWS_EXPORT_ALL_SYMBOLS ON
  )
  if(CRT_TARGET_OS STREQUAL "windows")
    set_target_properties(crtgfx_gpu_shared PROPERTIES
      ARCHIVE_OUTPUT_NAME crtgfx_gpu_dll
    )
  endif()
endfunction()
