cmake_minimum_required(VERSION 3.25)
project(crt_stage_04_gfx_media C CXX)

foreach(required CRT_STAGE_SOURCE_ROOT CRT_STAGE_TARGET_OS
                 CRT_STAGE_FFMPEG_PREFIX CRT_STAGE_FREETYPE_PREFIX
                 CRT_STAGE_SKIA_PREFIX)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "${required} is required")
  endif()
endforeach()
if(NOT CMAKE_SYSROOT)
  message(FATAL_ERROR "configure with the predecessor SDK's crt-toolchain.cmake")
endif()

# This stage is intentionally option-ON. Missing third-party artifacts are an
# error rather than a reason to silently produce the ordinary default-OFF 04
# package.
set(CRT_TARGET_OS "${CRT_STAGE_TARGET_OS}")
set(CRTGFX_ENABLE_SKIA ON)
set(CRTMEDIA_ENABLE_FFMPEG ON)
set(CRT_USE_IMPORTED_LIBCXX ON)
set(CRT_LIBCXX_SYSROOT "${CMAKE_SYSROOT}")
set(CRTGFX_ROOT "${CRT_STAGE_SOURCE_ROOT}/libcrtgfx")
set(CRTMEDIA_ROOT "${CRT_STAGE_SOURCE_ROOT}/libcrtmedia")
set(CRTGFX_SKIA_ROOT "${CRT_STAGE_SKIA_PREFIX}/include")
set(CRTGFX_SKIA_LIBRARIES
  "${CRT_STAGE_SKIA_PREFIX}/lib/libskia.a"
  "${CRT_STAGE_FREETYPE_PREFIX}/lib/libfreetype.a"
)
set(CRTGFX_CRT_STATIC_LIBS)
set(CRTGFX_CRT_SHARED_LIBS)
set(CRTMEDIA_CRT_STATIC_LIBS)
set(CRTMEDIA_CRT_SHARED_LIBS)

foreach(required_file
    "${CRTGFX_SKIA_ROOT}/include/core/SkSurface.h"
    "${CRT_STAGE_SKIA_PREFIX}/include/modules/skcms/skcms.h"
    "${CRT_STAGE_SKIA_PREFIX}/include/modules/skcms/src/skcms_public.h"
    "${CRT_STAGE_SKIA_PREFIX}/lib/libskia.a"
    "${CRT_STAGE_FREETYPE_PREFIX}/lib/libfreetype.a"
    "${CRT_STAGE_FFMPEG_PREFIX}/include/libavformat/avformat.h"
    "${CRT_STAGE_FFMPEG_PREFIX}/lib/libavformat.a"
    "${CRT_STAGE_FFMPEG_PREFIX}/lib/libavcodec.a"
    "${CRT_STAGE_FFMPEG_PREFIX}/lib/libswresample.a"
    "${CRT_STAGE_FFMPEG_PREFIX}/lib/libavutil.a")
  if(NOT EXISTS "${required_file}")
    message(FATAL_ERROR "required 04-gfx-media dependency is missing: ${required_file}")
  endif()
endforeach()

set(CRTMEDIA_FFMPEG_PORT_PREFIX "${CRT_STAGE_FFMPEG_PREFIX}")
set(CRTMEDIA_FFMPEG_LIBRARIES
  "${CRT_STAGE_FFMPEG_PREFIX}/lib/libavformat.a"
  "${CRT_STAGE_FFMPEG_PREFIX}/lib/libavcodec.a"
  "${CRT_STAGE_FFMPEG_PREFIX}/lib/libswresample.a"
  "${CRT_STAGE_FFMPEG_PREFIX}/lib/libavutil.a"
)

set(stage_c_options
  -ffreestanding -fno-builtin -Xclang -fwchar-type=int
  -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
set(stage_cxx_options ${stage_c_options} -std=c++17)

# 03-gfx-simple is the predecessor SDK, so consume its installed window
# libraries rather than rebuilding them from this source asset.
add_library(crtgfx_window INTERFACE IMPORTED GLOBAL)
add_library(crtgfx_window_shared INTERFACE IMPORTED GLOBAL)
target_include_directories(crtgfx_window INTERFACE "${CMAKE_SYSROOT}/include")
target_include_directories(crtgfx_window_shared INTERFACE "${CMAKE_SYSROOT}/include")
if(CRT_TARGET_OS STREQUAL "windows")
  set_property(TARGET crtgfx_window PROPERTY INTERFACE_LINK_LIBRARIES
    "${CMAKE_SYSROOT}/lib/libcrtgfx.a")
  set_property(TARGET crtgfx_window_shared PROPERTY INTERFACE_LINK_LIBRARIES
    "${CMAKE_SYSROOT}/lib/libcrtgfx_dll.dll.a")
elseif(CRT_TARGET_OS STREQUAL "macos")
  # crtgfx_window here is only an IMPORTED stub around the predecessor
  # SDK's already-built libcrtgfx.a, not the real distribution/stages/
  # 03-gfx-simple/CMakeLists.txt STATIC target -- so none of that file's
  # own PRIVATE "-framework Foundation/AppKit/QuartzCore/CoreGraphics
  # objc" entries (needed to resolve window_cocoa.c's real AppKit/objc
  # symbols baked into that .a) travel with it: PRIVATE link libraries of
  # a STATIC library are a CMake build-graph property of that library's
  # own target, not something recorded inside the .a file itself, so a
  # plain IMPORTED wrapper around the archive path has no way to know
  # about them. A real, confirmed bug (2026-09-11): crtgfx_gpu_test and
  # crtgfx_gpu_window_demo -- both link crtgfx_gpu PUBLIC, which links
  # this crtgfx_window PUBLIC -- failed outright with dozens of undefined
  # objc_msgSend/objc_getClass/kCAGravityResize/... symbols, all from
  # window_cocoa.c.o already inside the imported archive. Re-listing the
  # same frameworks/objc here (this time as part of the IMPORTED target's
  # own INTERFACE_LINK_LIBRARIES, the only kind of link info an IMPORTED
  # target can carry) fixes it. libcrtgfx.dylib below needs none of this:
  # a real shared library resolves its own dependencies at its own link
  # time and records them as load commands the dynamic linker follows
  # automatically, unlike a static archive.
  set_property(TARGET crtgfx_window PROPERTY INTERFACE_LINK_LIBRARIES
    "${CMAKE_SYSROOT}/lib/libcrtgfx.a"
    "-framework Foundation" "-framework AppKit" "-framework QuartzCore"
    "-framework CoreGraphics" objc)
  set_property(TARGET crtgfx_window_shared PROPERTY INTERFACE_LINK_LIBRARIES
    "${CMAKE_SYSROOT}/lib/libcrtgfx.dylib")
elseif(CRT_TARGET_OS STREQUAL "linux")
  # Same class of bug as the macOS branch above, for Linux's own missing
  # transitive dependency instead of Objective-C frameworks (2026-09-11):
  # window_wayland.c.o (already inside the predecessor SDK's libcrtgfx.a)
  # calls real libxkbcommon symbols (xkb_context_new/xkb_state_*/
  # xkb_keymap_*) that 03-gfx-simple's own real STATIC target links
  # PRIVATE (libcrtgfx/CMakeLists.txt's CRTGFX_XKBCOMMON_LIBRARIES) --
  # again, a build-graph property of that target, not recorded inside the
  # .a itself. crtgfx_gpu_test happened not to hit this (it never
  # exercises the keyboard/xkb code path), but crtgfx_gpu_window_demo
  # (creates a real window with input handling) failed outright with
  # undefined references to every xkb_* symbol above. The predecessor SDK
  # already carries the pinned static libxkbcommon.a as a declared
  # redistributed dependency (03-gfx-simple's own manifest.json
  # redistributed_dependencies entry) at this exact path -- re-listing it
  # here, the same way the macOS branch re-lists its frameworks, fixes it.
  set_property(TARGET crtgfx_window PROPERTY INTERFACE_LINK_LIBRARIES
    "${CMAKE_SYSROOT}/lib/libcrtgfx.a" "${CMAKE_SYSROOT}/lib/libxkbcommon.a")
  set_property(TARGET crtgfx_window_shared PROPERTY INTERFACE_LINK_LIBRARIES
    "${CMAKE_SYSROOT}/lib/libcrtgfx.so")
else()
  message(FATAL_ERROR "unsupported CRT_STAGE_TARGET_OS=${CRT_STAGE_TARGET_OS}")
endif()

include("${CRTGFX_ROOT}/cmake/crtgfx_gpu_sources.cmake")
include("${CRTGFX_ROOT}/cmake/crtgfx_gpu_targets.cmake")
include("${CRTGFX_ROOT}/cmake/crtgfx_skia_targets.cmake")
include("${CRTMEDIA_ROOT}/cmake/crtmedia_targets.cmake")

set(CRTGFX_GPU_SOURCES ${CRTGFX_GPU_COMMON_SOURCES})
if(CRT_TARGET_OS STREQUAL "windows")
  set(CRTGFX_GPU_BACKEND_SOURCES ${CRTGFX_GPU_BACKEND_WINDOWS_SOURCES})
  set(CRTGFX_WINDOWS_DXGI_LIB dxgi.lib)
  set(CRTGFX_WINDOWS_D3D12_LIB d3d12.lib)
  set(CRTGFX_WINDOWS_D3DCOMPILER_LIB d3dcompiler.lib)
  set(CRTGFX_WINDOWS_UUID_LIB uuid.lib)
  set(CRTGFX_WIN32_SHIM_ROOT
    "${CRT_STAGE_SOURCE_ROOT}/libstdc++/third_party/win32_shim")
  set(CRT_MINGW_W64_HEADERS_INCLUDE_ROOT "${CRT_STAGE_MINGW_W64_HEADERS_ROOT}")
  if(NOT CRT_STAGE_MINGW_W64_HEADERS_ROOT)
    message(FATAL_ERROR "Windows 04-gfx-media requires CRT_STAGE_MINGW_W64_HEADERS_ROOT")
  endif()
elseif(CRT_TARGET_OS STREQUAL "macos")
  set(CRTGFX_GPU_BACKEND_SOURCES ${CRTGFX_GPU_BACKEND_MACOS_SOURCES})
  set(CRTGFX_LIBCXX_UNEXPORTED
    "${CRT_STAGE_SOURCE_ROOT}/libstdc++/third_party/libcxx/libc++.unexported.exp")
elseif(CRT_TARGET_OS STREQUAL "linux")
  # Plain find_library() here comes up empty on a real multiarch Debian/
  # Ubuntu host (confirmed for real, 2026-09-11, running this isolated
  # stage against a real Ubuntu aarch64 host that genuinely has
  # libvulkan-dev installed: "Could not find CRTGFX_LINUX_VULKAN_LIB").
  # Root cause: this stage's own crt-toolchain.cmake sets
  # CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY (needed so CMake's
  # compiler-identification probe doesn't try to fully link against this
  # freestanding sysroot the normal way), which makes CMake's compiler-ABI
  # detection skip the executable-link probe entirely ("Check for working
  # C compiler ... - skipped") -- and that link probe (parsing a verbose
  # `-L.../lib/aarch64-linux-gnu` line) is the *only* thing that ever
  # populates CMAKE_LIBRARY_ARCHITECTURE/CMAKE_C_IMPLICIT_LINK_DIRECTORIES,
  # both confirmed empty here via direct inspection. Without a populated
  # CMAKE_LIBRARY_ARCHITECTURE, find_library() only searches plain
  # `/usr/lib`, `/lib`, etc, never the real
  # `/usr/lib/aarch64-linux-gnu/libvulkan.so` a `libvulkan-dev` install
  # actually uses -- the in-tree libcrtgfx/CMakeLists.txt's own identical
  # find_library(CRTGFX_LINUX_VULKAN_LIB vulkan) call never hits this,
  # since the main build's own ABI probe is a normal executable link.
  # Fixed the same way that file's own CRTGFX_LINUX_WAYLAND_CLIENT_LIB
  # fallback already handles a versioned-only install: retry via
  # find_file() with the real Debian/Ubuntu multiarch directory name, read
  # from dpkg-architecture directly rather than trusting CMake's own
  # (here, broken) derivation of it. dpkg-architecture is Debian/Ubuntu-
  # only by design (this project's own Linux ports are only exercised on
  # Debian/Ubuntu hosts -- WSL/Ubuntu, real Ubuntu -- per every other
  # Linux-specific comment in this codebase); find_program() failing
  # cleanly on a non-Debian host just leaves this fallback a no-op, same
  # as before this fix on such a host.
  find_library(CRTGFX_LINUX_VULKAN_LIB NAMES vulkan vulkan.so.1
    NO_CMAKE_FIND_ROOT_PATH)
  if(NOT CRTGFX_LINUX_VULKAN_LIB)
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
      unset(CRTGFX_LINUX_VULKAN_LIB CACHE)
      find_file(CRTGFX_LINUX_VULKAN_LIB
        NAMES libvulkan.so libvulkan.so.1
        PATHS /lib /usr/lib /usr/local/lib
        PATH_SUFFIXES "${CRT_HOST_MULTIARCH_TRIPLET}"
      )
    endif()
  endif()
  if(NOT CRTGFX_LINUX_VULKAN_LIB)
    message(FATAL_ERROR "Could not find CRTGFX_LINUX_VULKAN_LIB using the "
      "following names: vulkan, vulkan.so.1")
  endif()
  # A second, separate real bug found immediately after fixing the above
  # (2026-09-11): every executable linking CRTGFX_LINUX_VULKAN_LIB (e.g.
  # crtgfx_gpu_test) failed with dozens of `undefined reference to
  # 'strcmp@GLIBC_2.17'`/etc, straight out of libvulkan.so itself, plus
  # `warning: libc.so.6, needed by .../libvulkan.so, not found (try using
  # -rpath or -rpath-link)`. Root cause: this stage links every executable
  # fully freestanding (crt-toolchain.cmake's own --sysroot=/-nostdinc/no
  # default -lc), so GNU ld's own default for an EXECUTABLE (unlike a
  # `-shared` library, which defaults to --allow-shlib-undefined) is
  # --no-allow-shlib-undefined -- it insists on verifying every symbol
  # libvulkan.so's own NEEDED entries (libc.so.6/libm.so.6/
  # ld-linux-aarch64.so.1) would provide, and can only do that by actually
  # locating those files at link time. The `-rpath` already set on these
  # targets (crtgfx_gpu_targets.cmake) only affects the *runtime* loader,
  # not this link-time verification pass -- confirmed for real: adding
  # `-rpath-link` pointing at libvulkan.so's own directory (where the
  # real host libc.so.6/libm.so.6/ld-linux-aarch64.so.1 already live
  # alongside it, Ubuntu's usrmerged /lib -> /usr/lib multiarch layout)
  # is exactly the linker's own suggested fix and made a standalone
  # reproduction of the identical failing link succeed immediately, with
  # no effect on the resulting binary (-rpath-link is link-time-only,
  # never embedded). Directory-scoped so every current and future target
  # in this stage that links CRTGFX_LINUX_VULKAN_LIB picks it up
  # automatically, matching how CRTGFX_HAVE_VULKAN below is also set
  # once, project-wide, rather than repeated per target.
  get_filename_component(CRT_LINUX_VULKAN_LIB_DIR "${CRTGFX_LINUX_VULKAN_LIB}" DIRECTORY)
  add_link_options("-Wl,-rpath-link,${CRT_LINUX_VULKAN_LIB_DIR}")
  # A third, unrelated real bug found linking `lib/libcrtmedia.so` (2026-09-11):
  # `libavutil.a(tx_float_neon.o): relocation R_AARCH64_ADR_PREL_PG_HI21
  # against symbol 'ff_tx_tab_32_float' which may bind externally can not
  # be used when making a shared object; recompile with -fPIC`. Root
  # cause is narrower than the message suggests: `libavutil/aarch64/
  # asm.S`'s own `movrel` macro already emits the correct PIC-safe
  # `adrp`+`add :lo12:` sequence for this project's own `--enable-pic`
  # FFmpeg build (porting/recipes/ffmpeg.json) -- ld's complaint is about
  # the *symbol's* ordinary global (non-hidden) ELF visibility, not the
  # instruction sequence: it cannot assume an in-page-relative reference
  # to a symbol that could still be preempted by another shared object
  # stays valid. FFmpeg's own real `--enable-shared` build sidesteps this
  # with a linker version script demoting non-public symbols to local
  # scope at *its own* `.so` link -- never triggered here, since this
  # recipe only ever builds FFmpeg static and it is this project's own,
  # separate `libcrtmedia.so` link that first exposes it. `-Wl,-Bsymbolic`
  # forces every default-visibility symbol reference resolved within this
  # project's own final shared-library link to bind to the definition
  # already present in that same link -- exactly the guarantee ld was
  # refusing to assume -- without needing to patch FFmpeg's own visibility
  # annotations or Makefile (see porting/recipes/ffmpeg.json's own notes
  # for the two dead ends tried first: `--enable-pic` alone does not touch
  # this, and `--disable-neon` trades it for a *worse*, unconditional
  # undefined-reference failure from a genuine upstream Makefile gap).
  # Harmless for the executables linked in this same directory (`-Bsymbolic`
  # only changes shared-object symbol binding; ld accepts and no-ops it
  # for a plain executable link).
  add_link_options("-Wl,-Bsymbolic")
  set(CRTGFX_GPU_BACKEND_SOURCES ${CRTGFX_GPU_BACKEND_LINUX_SOURCES})
endif()
# libcrtgfx/src/gpu_internal.h's own struct crtgfx_gpu_device/crtgfx_gpu_
# surface field sets (and gpu_metal.c/gpu_vulkan.c/gpu_win32.c's own real
# field accesses into them) are all behind a per-backend #if defined(CRT_
# TARGET_OS_<os>) && defined(CRTGFX_HAVE_<backend>) guard -- the in-tree
# libcrtgfx/CMakeLists.txt defines CRTGFX_HAVE_METAL/_D3D12 unconditionally
# per host and CRTGFX_HAVE_VULKAN once a real libvulkan is found, but this
# standalone stage project never included that file, only the shared
# crtgfx_gpu_*.cmake *source list* modules -- a real, confirmed bug
# (2026-09-11): without this, gpu_internal.h's own structs fell through to
# their bodyless "#else int reserved;" branch while gpu_metal.c's real
# field accesses (device->mtl_device, surface->height, ...) were compiled
# unconditionally, failing outright with "no member named 'height' in
# 'struct crtgfx_gpu_surface'" the first time this stage's own macOS build
# actually reached that file.
if(CRT_TARGET_OS STREQUAL "windows")
  add_compile_definitions(CRTGFX_HAVE_D3D12=1)
elseif(CRT_TARGET_OS STREQUAL "macos")
  add_compile_definitions(CRTGFX_HAVE_METAL=1)
elseif(CRT_TARGET_OS STREQUAL "linux")
  add_compile_definitions(CRTGFX_HAVE_VULKAN=1)
endif()
list(TRANSFORM CRTGFX_GPU_SOURCES PREPEND "${CRTGFX_ROOT}/")
list(TRANSFORM CRTGFX_GPU_BACKEND_SOURCES PREPEND "${CRTGFX_ROOT}/")
set(CRTGFX_LIBC_INCLUDE_DIR "${CMAKE_SYSROOT}/include")
set(CRTGFX_EMUTLS_STUB_SOURCE
  "${CRT_STAGE_SOURCE_ROOT}/libc/src/arch/windows/common/emutls_link_stubs.c")

crt_add_crtgfx_gpu_common_targets()
target_compile_options(crtgfx_gpu_common_objects PRIVATE ${stage_c_options})
if(CRT_TARGET_OS STREQUAL "linux")
  target_compile_options(crtgfx_gpu_backend_objects PRIVATE ${stage_c_options})
endif()
crt_add_crtgfx_skia_object_target()
target_compile_options(crtgfx_skia_objects PRIVATE ${stage_cxx_options})
crt_add_crtgfx_skia_static_target()
crt_add_crtgfx_gpu_shared_target()
crt_add_crtgfx_skia_shared_target()

if(CRT_TARGET_OS STREQUAL "windows")
  set(CRTMEDIA_BACKEND_SOURCES src/arch/windows/audio_sink_wasapi.c)
  set(CRTMEDIA_WINDOWS_OLE32_LIB ole32.lib)
  set(CRTMEDIA_WINDOWS_D3D11_LIB d3d11.lib)
  set(CRTMEDIA_WINDOWS_DXGI_LIB dxgi.lib)
elseif(CRT_TARGET_OS STREQUAL "macos")
  set(CRTMEDIA_BACKEND_SOURCES src/arch/macos/audio_sink_coreaudio.c)
  set(CRTMEDIA_MACOS_FRAMEWORKS
    "-framework AudioToolbox" "-framework VideoToolbox"
    "-framework CoreVideo" "-framework CoreMedia")
else()
  set(CRTMEDIA_BACKEND_SOURCES src/arch/linux/audio_sink_linux.c)
endif()
crt_add_crtmedia_targets()
target_compile_options(crtmedia PRIVATE ${stage_c_options})
target_compile_options(crtmedia_shared PRIVATE ${stage_c_options})
if(TARGET crtmedia_backend_objects AND NOT CRT_TARGET_OS STREQUAL "windows")
  target_compile_options(crtmedia_backend_objects PRIVATE ${stage_c_options})
endif()

enable_testing()
function(crt_stage_register_test name target expected)
  set(predecessor_runtime "")
  if(CRT_TARGET_OS STREQUAL "windows")
    set(runtime_variable PATH)
    # Windows DLLs (this stage's own crtgfx_gpu_dll.dll/crtmedia_dll.dll
    # included) land in RUNTIME_OUTPUT_DIRECTORY, i.e. .../bin -- unlike
    # the macOS/Linux LIBRARY_OUTPUT_DIRECTORY case just below.
    set(own_runtime "${CMAKE_BINARY_DIR}/bin")
    set(predecessor_runtime "${CMAKE_SYSROOT}/bin")
  elseif(CRT_TARGET_OS STREQUAL "macos")
    set(runtime_variable DYLD_LIBRARY_PATH)
    set(own_runtime "${CMAKE_BINARY_DIR}/lib")
    # No predecessor_runtime (${CMAKE_SYSROOT}/lib) here, unlike Windows/
    # Linux just above/below -- a real, confirmed bug (2026-09-11): every
    # one of this stage's own 8 ctest binaries failed (Bus error/
    # Segmentation fault, or crtgfx_skia_raster_smoke's own "Symbol not
    # found: __ZnamSt19__type_descriptor_t, expected as weak-def export
    # from some loaded dylib") the first time this function's own env
    # wrapping was ever actually exercised end-to-end (isolated 03-gfx-
    # simple -> 04-gfx-media stage upgrade). Root-caused directly (ran
    # each failing ctest COMMAND by hand, then bisected by env var):
    # dyld's DYLD_LIBRARY_PATH override resolves a dependency by *leaf
    # filename* against every directory listed, ahead of that
    # dependency's own recorded path -- even an already-absolute one.
    # ${CMAKE_SYSROOT}/lib (the predecessor SDK) contains this project's
    # own imported libc++.1.dylib/libc++abi.1.dylib, under the exact same
    # leaf names as the real system copies. Every one of these test
    # executables links at least one real Apple framework (Foundation/
    # AppKit/CoreAudio/AudioToolboxCore/...), and those frameworks
    # transitively load the *real* /usr/lib/libc++.1.dylib themselves --
    # DYLD_LIBRARY_PATH silently swaps in this project's own, ABI-
    # incompatible libc++.1.dylib for that unrelated real framework's own
    # internal use instead, corrupting it. Confirmed directly: every
    # affected binary here ran perfectly (`<name>: ok`, exit 0) with
    # DYLD_LIBRARY_PATH unset entirely, resolving 100% of its own real
    # CRT dependencies (libcrtgfx.dylib/libcrtgfx_gpu.dylib/libcrtgfx_
    # skia.dylib/the imported libc++.dylib) via already-embedded absolute
    # paths or @rpath + this stage's own linker-baked -Wl,-rpath entries
    # (see tools/crt-cc's/tools/crt-c++'s own shared-link "entry" logic
    # and CMAKE_BUILD_RPATH/CMAKE_INSTALL_RPATH in the generated crt-
    # toolchain.cmake) -- so, unlike Windows/Linux above/below, nothing
    # here ever actually needed this predecessor_runtime override, only
    # ever amounted to a live landmine once a real Apple framework was in
    # the picture (which no prior consumer of this shared cmake helper --
    # this function only exists in this stage -- had ever exercised
    # before Skia/Metal made that the norm here).
  else()
    set(runtime_variable LD_LIBRARY_PATH)
    set(own_runtime "${CMAKE_BINARY_DIR}/lib")
    set(predecessor_runtime "${CMAKE_SYSROOT}/lib")
  endif()
  # Scope the newly-built and predecessor shared-library paths to each test;
  # never poison the host cmake/ctest process itself with CRT libc++.
  set(env_modify --modify "${runtime_variable}=path_list_prepend:${own_runtime}")
  if(predecessor_runtime)
    list(APPEND env_modify --modify "${runtime_variable}=path_list_prepend:${predecessor_runtime}")
  endif()
  add_test(NAME ${name} COMMAND "${CMAKE_COMMAND}" -E env
    ${env_modify}
    "$<TARGET_FILE:${target}>")
  set_tests_properties(${name} PROPERTIES PASS_REGULAR_EXPRESSION "${expected}")
endfunction()

function(crt_stage_add_c_test target source library expected)
  add_executable(${target} "${source}")
  target_compile_options(${target} PRIVATE ${stage_c_options})
  target_include_directories(${target} PRIVATE
    "${CRTGFX_ROOT}/include" "${CRTMEDIA_ROOT}/include")
  target_link_libraries(${target} PRIVATE ${library})
  crt_stage_register_test(${target}_runs ${target} "${expected}")
endfunction()

crt_stage_add_c_test(crtgfx_gpu_test "${CRTGFX_ROOT}/tests/gpu_test.c"
  crtgfx_gpu "crtgfx_gpu_test: ok")
crt_stage_add_c_test(crtmedia_frame_test "${CRTMEDIA_ROOT}/tests/frame_test.c"
  crtmedia "crtmedia_frame_test: ok")
crt_stage_add_c_test(crtmedia_gpu_frame_test "${CRTMEDIA_ROOT}/tests/gpu_frame_test.c"
  crtmedia "crtmedia_gpu_frame_test: ok")
crt_stage_add_c_test(crtmedia_format_test "${CRTMEDIA_ROOT}/tests/format_test.c"
  crtmedia "crtmedia_format_test: ok")
crt_stage_add_c_test(crtmedia_player_test "${CRTMEDIA_ROOT}/tests/player_test.c"
  crtmedia "crtmedia_player_test: ok")
crt_stage_add_c_test(crtmedia_audio_sink_test "${CRTMEDIA_ROOT}/tests/audio_sink_test.c"
  crtmedia "crtmedia_audio_sink_test: ok")
crt_stage_add_c_test(crtmedia_demux_test "${CRTMEDIA_ROOT}/tests/demux_decode_test.c"
  crtmedia "crtmedia_demux_test: ok")
target_compile_definitions(crtmedia_demux_test PRIVATE
  "CRTMEDIA_TEST_WAV_PATH=\"${CRTMEDIA_ROOT}/assets/test_tone.wav\"")

add_executable(crtgfx_skia_raster_smoke "${CRTGFX_ROOT}/tests/skia_raster_smoke.cc")
target_compile_options(crtgfx_skia_raster_smoke PRIVATE ${stage_cxx_options})
target_compile_definitions(crtgfx_skia_raster_smoke PRIVATE
  "CRT_SKIA_FONTS_DIR=\"${CRTGFX_ROOT}/assets/fonts\"")
target_include_directories(crtgfx_skia_raster_smoke PRIVATE
  "${CRTGFX_ROOT}/include" "${CRTGFX_SKIA_ROOT}")
target_link_libraries(crtgfx_skia_raster_smoke PRIVATE crtgfx_skia_shared)
crt_stage_register_test(crtgfx_skia_raster_smoke_runs
  crtgfx_skia_raster_smoke "crtgfx_skia_raster_smoke: ok")

add_executable(crtgfx_gpu_window_demo "${CRTGFX_ROOT}/tools/gpu_window_demo.c")
target_compile_options(crtgfx_gpu_window_demo PRIVATE ${stage_c_options})
target_link_libraries(crtgfx_gpu_window_demo PRIVATE crtgfx_gpu)
add_executable(crtgfx_skia_gpu_window_demo
  "${CRTGFX_ROOT}/tools/skia_gpu_window_demo.cc")
target_compile_options(crtgfx_skia_gpu_window_demo PRIVATE ${stage_cxx_options})
target_include_directories(crtgfx_skia_gpu_window_demo PRIVATE
  "${CRTGFX_ROOT}/tests" "${CRTGFX_SKIA_ROOT}")
target_link_libraries(crtgfx_skia_gpu_window_demo PRIVATE crtgfx_skia_shared)

install(TARGETS crtgfx_gpu crtgfx_gpu_shared crtgfx_skia crtgfx_skia_shared
                crtmedia crtmedia_shared
  ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install(DIRECTORY "${CRTGFX_ROOT}/include/" DESTINATION include)
install(DIRECTORY "${CRTMEDIA_ROOT}/include/" DESTINATION include)
install(DIRECTORY "${CRTGFX_SKIA_ROOT}/include" DESTINATION include)
install(TARGETS crtgfx_gpu_window_demo crtgfx_skia_gpu_window_demo
  RUNTIME DESTINATION examples/bin)
install(FILES "${CRTGFX_ROOT}/tools/gpu_window_demo.c"
  DESTINATION examples/gfx-gpu RENAME main.c)
install(FILES "${CRTGFX_ROOT}/tools/skia_gpu_window_demo.cc"
  DESTINATION examples/gfx-skia RENAME main.cc)
install(FILES "${CRTGFX_ROOT}/tests/skia_reference_scene.h"
  DESTINATION examples/gfx-skia)
install(FILES "${CRT_STAGE_SOURCE_ROOT}/examples/gfx-gpu/CMakeLists.txt"
  DESTINATION examples/gfx-gpu)
install(FILES "${CRT_STAGE_SOURCE_ROOT}/examples/gfx-skia/CMakeLists.txt"
  DESTINATION examples/gfx-skia)
