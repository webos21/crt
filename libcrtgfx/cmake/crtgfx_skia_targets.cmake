# Shared target construction for the Skia bridge. Keep the object compile
# policy, static archive ordering, and shared-library link fixes in one place
# for both the in-tree build and isolated 04-gfx-media stage.
#
# The caller supplies CRTGFX_ROOT plus the Skia, CRT runtime, platform-library,
# imported-libc++, Win32 shim, emutls, and SDK-header variables referenced by
# the original in-tree logic below.
function(crt_add_crtgfx_skia_object_target)
  set(CRTGFX_SKIA_OBJECTS)
  if(CRTGFX_ENABLE_SKIA)
    add_library(crtgfx_skia_objects OBJECT "${CRTGFX_ROOT}/src/skia_bridge.cc")
    if(TARGET crt_cxx_build_flags)
      target_link_libraries(crtgfx_skia_objects PRIVATE crt_cxx_build_flags)
    endif()
    if(CRT_TARGET_OS STREQUAL "windows")
      # SK_BUILD_FOR_UNIX: matches tools/build_skia.py's own -DSK_BUILD_FOR_
      # UNIX for Skia's own GN-driven compile (see that file's own fuller
      # comment) -- needed again here since include/private/base/SkFeatures.h
      # auto-detects purely from _WIN32 (which clang's mingw target predefines
      # too) and has no way to tell tools/crt-cc's mingw-target build apart
      # from real MSVC/clang-cl. Without it, SK_ALWAYS_INLINE (SkAttributes.h)
      # expands to the bare MSVC keyword __forceinline, and SkTypes.h tries to
      # #include <crtdbg.h> under SK_DEBUG -- neither exists/parses under
      # this project's own GNU-ABI clang invocation (confirmed for real,
      # 2026-08-23: `error: unknown type name '__forceinline'` then `fatal
      # error: 'crtdbg.h' file not found`, the same failure build_skia.py's
      # own comment already documents for Skia's own sources). Must match
      # whatever libskia.a itself was actually compiled with for any
      # macro-conditional inline code/struct layout in the shared headers to
      # stay consistent -- Skia's own GN build already picked SK_BUILD_FOR_
      # UNIX (via target_os="linux"), never SK_BUILD_FOR_WIN.
      target_compile_definitions(crtgfx_skia_objects PRIVATE SK_BUILD_FOR_UNIX)
    endif()
    if(CRT_TARGET_OS STREQUAL "linux" AND CRTGFX_LINUX_VULKAN_LIB)
      # Real Ganesh/Vulkan offscreen vertical slice (2026-09-03). Skia's own
      # public include/gpu/ganesh/vk/GrVkTypes.h #includes include/private/
      # gpu/vk/SkiaVulkan.h, which only pulls in Skia's own vendored real
      # Vulkan headers (include/third_party/vulkan/vulkan/vulkan_core.h) when
      # SK_USE_INTERNAL_VULKAN_HEADERS is defined at the *consumer's* own
      # compile time too -- this is normally a GN public_config Skia's own
      # BUILD.gn applies automatically to every real GN dependent
      # (config("use_skia_vulkan_headers"), applied wherever skia_use_vulkan
      # is true), but this project links Skia via a hand-collected .a file
      # with hand-written CMake, not a real GN dependency graph, so that
      # propagation has to be reproduced by hand here -- confirmed by reading
      # both files directly, not assumed. Must match tools/build_skia.py's
      # own Linux-branch skia_use_vulkan=true exactly (same condition), or
      # skia_bridge.cc would silently see incompatible Ganesh Vulkan struct
      # layouts against what libskia.a itself was actually built with.
      target_compile_definitions(crtgfx_skia_objects PRIVATE SK_USE_INTERNAL_VULKAN_HEADERS)
    endif()
    if(CRT_TARGET_OS STREQUAL "windows")
      # Windows/D3D12 offscreen vertical slice (2026-09-03, real headers
      # switched 2026-09-04) -- Skia's own public include/gpu/ganesh/d3d/
      # GrD3DTypes.h unconditionally #includes <d3d12.h>/<dxgi1_4.h>, a
      # real, forced exception to this project's own no-host-header policy
      # (see docs/libcrtgfx_api_policy.md). skia_bridge.cc's own D3D12
      # branch transitively includes it, so this target needs the same
      # real flags tools/build_skia.py's own sibling call already adds for
      # Skia's *own* GrD3D*.cpp sources -- see that file's own comments
      # for the full "why" behind each one (mingw-w64's header set, not
      # the raw Microsoft Windows SDK's -- a real, confirmed dead end
      # under this project's mingw-target clang; and win32_shim/
      # mingw_w64_compat.h's own real, narrow compatibility fixes).
      #
      # Both -I dirs, in this exact order: win32_shim FIRST, matching
      # tools/build_skia.py's own real order, is not optional -- win32_shim
      # provides excpt.h/malloc.h/DXProgrammableCapture.h (real files
      # mingw-w64-headers does not, needed unconditionally by mingw's own
      # real windows.h/rpc.h/GrD3DGpu.cpp chains -- see each shim file's
      # own top comment), AND its own windows.h/winerror.h/winioctl.h/
      # psapi.h/ntverp.h #include_next-forward to mingw-w64-headers' real
      # versions (see win32_shim/windows.h's own top comment) -- reversing
      # this order would either lose that forwarding (files found in
      # mingw-w64-headers directly, never reaching win32_shim's own
      # #include_next logic) or make win32_shim's exclusive files
      # (excpt.h/malloc.h/DXProgrammableCapture.h) unreachable. A first
      # attempt at this target's own flags omitted win32_shim entirely
      # (mistakenly assumed here to be irrelevant to this specific
      # target) -- confirmed for real (2026-09-04) this breaks: `fatal
      # error: 'excpt.h' file not found` from mingw's own windows.h.
      # target_compile_options(), not target_include_directories(): lands
      # in <FLAGS>, matching this file's own established
      # -isystem${CRT_LIBCXX_SYSROOT}/include/c++/v1 precedent just below (see
      # that block's own fuller comment on why <INCLUDES> alone cannot
      # reliably win the real ordering this project's own custom
      # CMAKE_CXX_COMPILE_OBJECT rule needs).
      target_compile_options(crtgfx_skia_objects PRIVATE
        "-I${CRTGFX_WIN32_SHIM_ROOT}"
        "-I${CRT_MINGW_W64_HEADERS_INCLUDE_ROOT}"
        # -include mingw_w64_compat.h: see tools/build_skia.py's own
        # matching comment (same file's own fuller reasoning) -- this
        # target #includes the same real mingw-w64 headers, transitively,
        # and needs the same real, narrow compatibility declarations
        # forced ahead of them.
        "-include" "${CRTGFX_WIN32_SHIM_ROOT}/mingw_w64_compat.h"
        # -Wno-pragma-pack (2026-09-04, real, confirmed necessary): this
        # project's own -Wall -Wextra -Werror (crt_cxx_build_flags) turns
        # mingw-w64's own real, expected, harmless #pragma pack(push,N)/
        # pack() nesting (winnt.h/wingdi.h/... intentionally narrowing/
        # restoring struct alignment for real Win32 ABI structs, exactly
        # as every other real Windows consumer's build does) into hard
        # errors -- confirmed for real: `error: the current #pragma pack
        # alignment value is modified in the included file
        # [-Werror,-Wpragma-pack]`, 20 of them, hit only once this
        # project's own code (not just Skia's, which already builds these
        # headers warning-suppressed via tools/build_skia.py's own -w)
        # started #including real mingw-w64 headers directly.
        "-Wno-pragma-pack"
        # -Wno-unused-value/-Wno-ignored-attributes (2026-09-04, real,
        # confirmed necessary, same real cause as -Wno-pragma-pack just
        # above): mingw-w64's own real combaseapi.h (reached transitively
        # via <d3d12.h> -> ... -> objbase.h) has a `static_cast<IUnknown
        # *>(*pp);` (a real, deliberate no-op cast, part of its own
        # `IID_PPV_ARGS`-equivalent macro machinery's own compile-time
        # type check) and two `__forceinline` functions (CoCreateInstance/
        # CoCreateInstanceEx) that intentionally redeclare a `dllimport`
        # declaration inline -- both real, harmless, expected mingw-w64
        # patterns, confirmed via `error: expression result unused
        # [-Werror,-Wunused-value]` / `error: 'CoCreateInstance'
        # redeclared inline; 'dllimport' attribute ignored
        # [-Werror,-Wignored-attributes]`.
        "-Wno-unused-value"
        "-Wno-ignored-attributes"
      )
      if(TARGET crtgfx-mingw-w64-headers-fetch)
        add_dependencies(crtgfx_skia_objects crtgfx-mingw-w64-headers-fetch)
      endif()
    endif()
    if(CRT_TARGET_OS STREQUAL "macos")
      # macOS/Metal offscreen vertical slice (2026-09-04) -- Skia's own
      # public include/gpu/ganesh/mtl/GrMtlBackendContext.h (transitively,
      # via GrMtlTypes.h/include/ports/SkCFObject.h) #includes real
      # <TargetConditionals.h>/<CoreFoundation/CoreFoundation.h>/<Metal/
      # Metal.h>, a real, forced exception to this project's own no-host-
      # SDK-header policy (see docs/libcrtgfx_api_policy.md), exactly like
      # the D3D12 branch's own GrD3DTypes.h above.
      #
      # Two real calling contexts now share this function (2026-09-11):
      # the in-tree build (plain CMAKE_CXX_COMPILER=clang++, no wrapper --
      # see CMakePresets.json) and the isolated distribution/stages/
      # 04-gfx-media project, which compiles through tools/crt-c++
      # instead (crt-toolchain.cmake's own CMAKE_CXX_COMPILER). Detected
      # via `if(TARGET crt_cxx_build_flags)` -- that INTERFACE library only
      # ever exists in the in-tree build, matching this same file's own
      # existing use of the identical check just above for the Windows
      # branch. The two need genuinely different fixes, not just different
      # flag spellings:
      if(TARGET crt_cxx_build_flags)
        # In-tree build: plain clang++ predefines __APPLE__ on its own and
        # has no -U__APPLE__/-isystem${CRT_SYSROOT}/include of its own to
        # fight with, so the manual, hand-ordered fix below (confirmed
        # necessary and correctly ordered for real, 2026-09-04) is enough:
        # -isysroot/-F<Frameworks>/-isystem<usr/include>, with
        # CRT_CXX_STANDARD_INCLUDE_FLAGS re-added ahead of the new
        # <usr/include> entry so libc++'s own <cstdio>/<cerrno>/... headers
        # still find their own thin wrapper first via #include_next
        # (confirmed for real: without this re-add, "<cstdio> tried
        # including <stdio.h> but didn't find libc++'s <stdio.h> header").
        # "own PRIVATE target_compile_options land in <FLAGS> ahead of
        # crt_cxx_build_flags's own inherited chain" is what makes this
        # ordering hold in the in-tree build specifically.
        execute_process(
          COMMAND xcrun --sdk macosx --show-sdk-path
          OUTPUT_VARIABLE CRTGFX_MACOS_REAL_SDK_PATH
          OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(NOT CRTGFX_MACOS_REAL_SDK_PATH)
          message(FATAL_ERROR "crtgfx_skia_objects (macOS/Metal): 'xcrun --sdk macosx --show-sdk-path' failed")
        endif()
        target_compile_options(crtgfx_skia_objects PRIVATE
          -isysroot "${CRTGFX_MACOS_REAL_SDK_PATH}"
          "-F${CRTGFX_MACOS_REAL_SDK_PATH}/System/Library/Frameworks"
          ${CRT_CXX_STANDARD_INCLUDE_FLAGS}
          "-isystem${CRTGFX_MACOS_REAL_SDK_PATH}/usr/include"
        )
      else()
        # Isolated stage build (tools/crt-c++): the identical manual
        # reproduction above does NOT work here -- a real, confirmed bug
        # (2026-09-11), found completing the first-ever real macOS build
        # of this standalone project. CMake's own default compile rule
        # places <INCLUDES> (target_include_directories, carrying this
        # target's own -isystem${CMAKE_SYSROOT}/include from this
        # function's PRIVATE include dirs below) *before* <FLAGS>
        # (target_compile_options, carrying the manual -isysroot/-F/
        # -isystem<usr/include> above) in the actual generated command
        # line -- the opposite order the in-tree build's own <FLAGS>-vs-
        # <FLAGS> comment above relies on, since that's a different axis
        # (ordering *within* <FLAGS>, not <INCLUDES> vs <FLAGS>). This
        # project's own minimal sys/cdefs.h (found first via <INCLUDES>)
        # therefore still shadowed the real one real Darwin headers like
        # Block.h/malloc.h need to reach <ptrcheck.h>'s own real
        # __single/__sized_by_or_null/... bounds-safety macros --
        # confirmed for real: "error: expected ';' after top level
        # declarator" on Block.h's own real, unmodified declarations, the
        # exact failure class tools/crt-c++'s own -fcrt-real-apple-sdk
        # sentinel exists specifically to fix. That sentinel already gets
        # this exact ordering right internally (its own apple_sdk_search_
        # flags is placed ahead of its own -isystem${CRT_SYSROOT}/include
        # inside common_flags, a single combined string this rule's own
        # <INCLUDES>-before-<FLAGS> split can't pull apart) and already
        # restores -D__APPLE__=1 (needed here too: Skia's own public
        # include/ports/SkCFObject.h, transitively required by
        # GrMtlBackendContext.h, is itself `#ifdef __APPLE__`-gated, and
        # crt-c++ -U__APPLE__'s every macOS compile by default) -- so the
        # real fix for this calling context is to hand it the sentinel
        # directly instead of reproducing (and, as it turns out here,
        # mis-ordering) the same exception by hand a second time.
        target_compile_options(crtgfx_skia_objects PRIVATE -fcrt-real-apple-sdk)
      endif()
    endif()
    if(CRT_TARGET_OS STREQUAL "windows" AND CRT_USE_IMPORTED_LIBCXX)
      # -isystem${CRT_LIBCXX_SYSROOT}/include/c++/v1 (dist/02-cxx, not
      # CRT_SYSROOT/dist/01-c -- see crtgfx-skia-configure's own, fuller
      # comment further up this file for why), via target_compile_options
      # (not target_include_directories) so it lands in <FLAGS>, ahead of
      # crt_cxx_build_flags's own inherited chain -- see the matching, fuller
      # comment on crtgfx_skia_raster_smoke's own libc/include isystem just
      # below for why <INCLUDES> can never win that ordering on this project's
      # custom CMAKE_CXX_COMPILE_OBJECT rule. See the CRTGFX_ENABLE_SKIA guard
      # above (top of this file) for why this project's own imported libc++,
      # not any host substitute, is required here.
      target_compile_options(crtgfx_skia_objects PRIVATE
        "-isystem${CRT_LIBCXX_SYSROOT}/include/c++/v1"
      )
    endif()
    target_include_directories(crtgfx_skia_objects SYSTEM BEFORE PUBLIC
      "${CRTGFX_ROOT}/include"
      "${CRTGFX_SKIA_ROOT}"
    )
    target_include_directories(crtgfx_skia_objects SYSTEM BEFORE PRIVATE
      "${CRTGFX_ROOT}/src"
      "${CRTGFX_LIBC_INCLUDE_DIR}"
    )
    set_target_properties(crtgfx_skia_objects PROPERTIES
      POSITION_INDEPENDENT_CODE ON
    )
    set(CRTGFX_SKIA_OBJECTS $<TARGET_OBJECTS:crtgfx_skia_objects>)
  endif()
endfunction()

function(crt_add_crtgfx_skia_static_target)
  # --- crtgfx_skia (STATIC): the Skia CPU/GPU bridge (crtgfx/skia.h).
  # Links crtgfx_gpu PUBLIC (transitively pulls crtgfx_window too) --
  # skia_bridge.cc needs gpu_internal.h's struct layout and (on macOS)
  # calls a real function gpu_metal.c defines
  # (crtgfx_gpu_metal_surface_prepare_ganesh_present()).
  if(CRTGFX_ENABLE_SKIA)
    add_library(crtgfx_skia STATIC $<TARGET_OBJECTS:crtgfx_skia_objects>)
    target_link_libraries(crtgfx_skia PUBLIC crtgfx_gpu)
    if(CRT_TARGET_OS STREQUAL "macos" AND CRT_USE_IMPORTED_LIBCXX)
      # Frameworks load Apple's libc++ too. Keep this embedded runtime's weak
      # std::__1 definitions local, so dyld cannot mix locale/facet instances.
      # Scoped to crtgfx_skia now (the only one of the three that actually
      # links libc++.a at all) rather than the old monolithic crtgfx.
      target_link_options(crtgfx_skia INTERFACE "LINKER:-unexported_symbols_list,${CRTGFX_LIBCXX_UNEXPORTED}")
      set_property(TARGET crtgfx_skia APPEND PROPERTY INTERFACE_LINK_DEPENDS "${CRTGFX_LIBCXX_UNEXPORTED}")
    endif()
    target_include_directories(crtgfx_skia PUBLIC
      "${CRTGFX_ROOT}/include"
      "${CRTGFX_SKIA_ROOT}"
    )
    if(CRT_TARGET_OS STREQUAL "linux" AND CRTGFX_SKIA_LIBRARIES)
      # --start-group/--end-group: a real GNU-ld circular-archive-resolution
      # gap, confirmed for real (2026-08-23) via the generated link command for
      # crtgfx_skia_raster_smoke -- libskia.a's own undefined needs (`atan2f`/
      # `sinf`/`cosf`/`floor`/`sqrtf` from libm, `operator new`/`delete` from
      # libc++/libc++abi, even `__dso_handle` from libc's own cxa_atexit.c.o)
      # get scanned by ld.bfd's default single left-to-right archive pass
      # *after* libm.a/libc++.a(=cxx)/libc.a already passed, since libskia.a
      # (PUBLIC) is always the last of `crtgfx_skia`'s own dependencies on
      # the final link line regardless of call order here. The exact same
      # class of gap tools/crt-cc's own Linux branch already documents and
      # fixes the identical way for its own, separate link recipes. Wrapped
      # here, in `crtgfx_skia`'s own INTERFACE_LINK_LIBRARIES, rather than
      # on each consumer: a consumer-side target_link_libraries()/target_
      # link_options() pairing for the group markers does NOT reliably
      # bracket `crtgfx_skia`'s own flattened transitive dependencies --
      # confirmed for real that CMake re-positions a static library's own
      # propagated INTERFACE_LINK_LIBRARIES independently of where a
      # *consumer's own* separately-added flags land. Declaring the group
      # directly where these libraries are declared avoids relying on that
      # ordering at all.
      target_link_libraries(crtgfx_skia PRIVATE -Wl,--start-group)
      target_link_libraries(crtgfx_skia PRIVATE ${CRTGFX_CRT_STATIC_LIBS})
      target_link_libraries(crtgfx_skia PUBLIC ${CRTGFX_SKIA_LIBRARIES})
      target_link_libraries(crtgfx_skia PRIVATE -Wl,--end-group)
    else()
      # Skia first, CRT_STATIC_LIBS (libc++.a on macOS, once CRT_USE_
      # IMPORTED_LIBCXX swaps it in below) second -- reversed from the
      # seemingly more natural "runtime libs, then Skia" order on purpose.
      # Apple ld (this else() branch's own real user: macOS) scans each
      # static archive once, left-to-right, only pulling members needed by
      # symbols already outstanding at that point -- unlike Linux's
      # --start-group/--end-group branch just above, ld64 has no
      # bracket-and-rescan equivalent. libskia.a's own SkSL compiler/
      # parser/debug-trace code makes extensive std::string/iostream/
      # locale/shared_ptr use, so libc++.a must still be "ahead" in scan
      # order when ld reaches it -- i.e. mentioned *after* libskia.a on the
      # link line, the reverse of the intuitive "dependencies of a thing
      # come after it" ordering. Confirmed for real (2026-08-23): with
      # CRT_STATIC_LIBS linked first (the original order),
      # crtgfx_skia_raster_smoke/libcrtgfx.dylib both failed with dozens of
      # undefined std::__1::... symbols from libskia.a's own object files,
      # even after fixing which libc++.a was being linked at all (see the
      # CRTGFX_CRT_STATIC_LIBS/SHARED_LIBS macOS substitution further up)
      # and after removing a stray duplicate hardcoded relink on crtgfx_
      # skia_raster_smoke's own target -- only reordering these two calls
      # (no --start-group equivalent, no force_load, no per-consumer
      # override needed) made the link line pass.
      if(CRTGFX_SKIA_LIBRARIES)
        target_link_libraries(crtgfx_skia PUBLIC ${CRTGFX_SKIA_LIBRARIES})
      endif()
      target_link_libraries(crtgfx_skia PRIVATE ${CRTGFX_CRT_STATIC_LIBS})
    endif()
    if(CRT_TARGET_OS STREQUAL "windows")
      # d3dcompiler.lib (2026-09-04, real, confirmed necessary): Skia's own
      # real src/gpu/ganesh/d3d/GrD3DPipelineStateBuilder.cpp calls the
      # real, standard D3DCompile() (compiles Ganesh's own SkSL-generated
      # HLSL source into real shader bytecode at runtime) -- confirmed via
      # `ld.lld: error: undefined symbol: D3DCompile`.
      target_link_libraries(crtgfx_skia PUBLIC "${CRTGFX_WINDOWS_D3DCOMPILER_LIB}")
    endif()
    set_target_properties(crtgfx_skia PROPERTIES
      OUTPUT_NAME crtgfx_skia
      ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    )
  endif()
endfunction()

function(crt_add_crtgfx_skia_shared_target)
  # --- crtgfx_skia_shared (SHARED): the Skia bridge DLL/.so/.dylib.
  if(CRTGFX_ENABLE_SKIA)
    set(CRTGFX_SHARED_EXTRA_SOURCES "")
    # Windows + Skia only: crtgfx_skia_shared -- unlike the plain `crtgfx_
    # skia` static archive, which only records crtgfx_skia_objects's own
    # unresolved real-libc++/emutls references for whatever later links it
    # in (crtgfx_skia_raster_smoke, already fixed) -- is itself a real DLL,
    # so it must fully resolve those same references at its own link time.
    # See crtgfx_skia_raster_smoke's own matching comments (imported
    # libc++, uuid.lib, --allow-multiple-definition, emutls_link_stubs.c)
    # for the full story of each piece; this mirrors all of them for the
    # shared/DLL case.
    if(CRT_TARGET_OS STREQUAL "windows")
      list(APPEND CRTGFX_SHARED_EXTRA_SOURCES
        "${CRTGFX_EMUTLS_STUB_SOURCE}")
    endif()
    add_library(crtgfx_skia_shared SHARED
      $<TARGET_OBJECTS:crtgfx_skia_objects>
      ${CRTGFX_SHARED_EXTRA_SOURCES}
    )
    target_link_libraries(crtgfx_skia_shared PUBLIC crtgfx_gpu_shared)
    target_include_directories(crtgfx_skia_shared PUBLIC
      "${CRTGFX_ROOT}/include"
      "${CRTGFX_SKIA_ROOT}"
    )
    if(CRT_TARGET_OS STREQUAL "windows" AND CRT_USE_IMPORTED_LIBCXX)
      # Real imported libc++/libc++abi/libunwind DLL import libraries
      # (Itanium ABI, matching Skia itself), not the bootstrap libstdc++/
      # src/ ABI shim (`cxx_shared`) -- swapped in for just the C++
      # runtime member of CRTGFX_CRT_SHARED_LIBS, keeping c_shared/
      # m_shared/dl_shared as-is. CRT_LIBCXX_SYSROOT (dist/02-cxx), not
      # CRT_SYSROOT -- see crtgfx-skia-configure's own, fuller comment
      # further up this file for why.
      target_link_libraries(crtgfx_skia_shared PRIVATE
        c_shared m_shared dl_shared
        "${CRT_LIBCXX_SYSROOT}/lib/libc++.dll.a"
        "${CRT_LIBCXX_SYSROOT}/lib/libc++abi.dll.a"
        "${CRT_LIBCXX_SYSROOT}/lib/libunwind.dll.a"
      )
    else()
      set(CRTGFX_SHARED_SKIA_LINUX_GROUPED OFF)
      if(CRT_TARGET_OS STREQUAL "linux" AND CRTGFX_SKIA_LIBRARIES)
        set(CRTGFX_SHARED_SKIA_LINUX_GROUPED ON)
        # --start-group/--end-group: see `crtgfx_skia`'s own, fuller
        # comment (same file, its CRTGFX_CRT_STATIC_LIBS/CRTGFX_SKIA_
        # LIBRARIES linking) for the full story -- the identical GNU-ld
        # circular-archive-resolution gap applies to crtgfx_skia_shared's
        # own link too.
        target_link_libraries(crtgfx_skia_shared PRIVATE -Wl,--start-group)
      endif()
      if(CRTGFX_SHARED_SKIA_LINUX_GROUPED)
        # CRTGFX_SKIA_LIBRARIES (libskia.a + libfreetype.a, see that
        # variable's own comment) BEFORE CRTGFX_CRT_SHARED_LIBS here, not
        # after: found for real (2026-08-24) wiring Skia's own real
        # FreeType font manager in -- ld.lld/GNU ld resolve each SHARED
        # library's own undefined-symbol contribution in a single
        # left-to-right scan (unlike a static archive inside --start-
        # group/--end-group, which genuinely does get rescanned on every
        # pass); cxx_shared's own real libc++.so was already "passed" by
        # the time libskia.a's later archive-member extraction discovered
        # NEW undefined libc++ symbols it needed (SkSLString.cpp's own
        # float-to-string conversion via <sstream>/<locale> --
        # std::locale::classic() and friends), so those stayed unresolved
        # (`ld.lld: error: undefined symbol: std::__1::locale::classic()`)
        # even though libc++.so genuinely exports them (confirmed directly
        # via `nm -D`) and an isolated iostream/locale-only test program
        # links against the exact same libc++.so cleanly. Putting
        # libskia.a/libfreetype.a first means every libc++ symbol THEY
        # need is already a known undefined the following cxx_shared/
        # libc++.so scan resolves in its own single pass -- confirmed
        # fixed by a real manual relink with this exact reordering before
        # landing here.
        target_link_libraries(crtgfx_skia_shared PUBLIC ${CRTGFX_SKIA_LIBRARIES})
      endif()
      # Unconditional (matches this block's own pre-split shape exactly):
      # a real DLL/.so/.dylib link step, on every host, needs this
      # resolved at its own link time.
      target_link_libraries(crtgfx_skia_shared PRIVATE ${CRTGFX_CRT_SHARED_LIBS})
      if(CRTGFX_SHARED_SKIA_LINUX_GROUPED)
        target_link_libraries(crtgfx_skia_shared PRIVATE -Wl,--end-group)
      endif()
    endif()
    if(TARGET crt_build_flags)
      target_link_libraries(crtgfx_skia_shared PRIVATE crt_build_flags)
    endif()
    if(NOT CRTGFX_SHARED_SKIA_LINUX_GROUPED AND CRTGFX_SKIA_LIBRARIES AND
       NOT (CRT_TARGET_OS STREQUAL "windows" AND CRT_USE_IMPORTED_LIBCXX))
      target_link_libraries(crtgfx_skia_shared PUBLIC ${CRTGFX_SKIA_LIBRARIES})
    endif()
    if(CRT_TARGET_OS STREQUAL "windows")
      if(TARGET crt_windows_dllcrt)
        target_sources(crtgfx_skia_shared PRIVATE $<TARGET_OBJECTS:crt_windows_dllcrt>)
      endif()
      target_link_libraries(crtgfx_skia_shared PRIVATE "${CRTGFX_WINDOWS_D3DCOMPILER_LIB}")
      if(TARGET crt_compiler_rt_builtins)
        target_link_libraries(crtgfx_skia_shared PRIVATE crt_compiler_rt_builtins)
      endif()
      find_file(CRTGFX_WINDOWS_UUID_LIB
        NAMES Uuid.Lib uuid.lib
        PATHS ${CRT_WINDOWS_SDK_LIB_CANDIDATES}
        NO_DEFAULT_PATH
      )
      if(NOT CRTGFX_WINDOWS_UUID_LIB)
        set(CRTGFX_WINDOWS_UUID_LIB uuid.lib CACHE FILEPATH "Windows uuid import library")
      endif()
      target_link_libraries(crtgfx_skia_shared PRIVATE "${CRTGFX_WINDOWS_UUID_LIB}")
      target_link_options(crtgfx_skia_shared PRIVATE -Wl,--allow-multiple-definition)
    elseif(CRT_TARGET_OS STREQUAL "linux")
      # -fuse-ld=lld: see crtgfx_skia_raster_smoke's own, fuller comment
      # (same file, its Linux branch) for the full ld.bfd-vs-lld story.
      target_link_options(crtgfx_skia_shared PRIVATE -fuse-ld=lld)
    elseif(CRT_TARGET_OS STREQUAL "macos")
      # objc: Skia's own Metal/Ganesh backend (GrMtlCommandBuffer.cpp,
      # GrMtlGpu.cpp, GrMtlOpsRenderPass.cpp, ...), already compiled into
      # CRTGFX_SKIA_LIBRARIES' libskia.a above, is real Objective-C++
      # under ARC -- its object code calls the ARC runtime directly
      # (objc_storeWeak/objc_loadWeakRetained/objc_initWeak/objc_
      # storeStrong/objc_msgSend/...), independent of and in addition to
      # crtgfx_gpu_shared's own separate, already-resolved objc need
      # (gpu_metal.c's plain objc_msgSend calls, see crt_add_crtgfx_gpu_
      # shared_target()'s own comment). That PRIVATE objc on crtgfx_gpu_
      # shared does not help here: it only resolved crtgfx_gpu_shared's
      # own link, and PRIVATE link libraries of a SHARED library never
      # propagate to a further consumer's own link -- crtgfx_skia_shared
      # is itself a second, independent real .dylib link, pulling
      # libskia.a's ARC object code in directly, so it must resolve
      # those symbols itself. A real, confirmed bug (2026-09-11), first
      # hit building the isolated 03-gfx-simple -> 04-gfx-media stage
      # upgrade (CRTGFX_ENABLE_SKIA is opt-in and OFF by default in the
      # in-tree build, so this is also the first time this project's own
      # crtgfx_skia_shared has ever actually been linked for macOS with
      # a real Metal-enabled libskia.a at all): "ld: symbol(s) not found
      # for architecture arm64" on every _objc_storeWeak/_objc_
      # loadWeakRetained/_objc_initWeak/... reference.
      #
      # CoreFoundation/Foundation/Metal: same real-first-link discovery,
      # one rebuild later -- once the objc *runtime* itself resolved,
      # libskia.a's own Metal backend and skia_bridge.cc's own SkCFObject.h
      # use (see that header's own #ifdef __APPLE__ story) still left
      # _CFRetain/_CFRelease/___CFConstantStringClassReference
      # (CoreFoundation), _NSLog/_OBJC_CLASS_$_NSString (Foundation), and
      # _OBJC_CLASS_$_MTL* -- every Cocoa class reference GrMtlCommand
      # Buffer.cpp/GrMtlUtil.cpp/GrMtlDepthStencil.cpp/... instantiate --
      # (Metal) undefined. Same root cause as objc just above: these are
      # crtgfx_skia_shared's own direct, first-ever real link-time need,
      # not something crtgfx_gpu_shared's own (PRIVATE, non-propagating)
      # framework list could ever have covered here.
      target_link_libraries(crtgfx_skia_shared PRIVATE
        objc
        "-framework CoreFoundation"
        "-framework Foundation"
        "-framework Metal"
      )
    endif()
    if(COMMAND crt_configure_shared_runtime)
      crt_configure_shared_runtime(crtgfx_skia_shared)
    endif()
    set_target_properties(crtgfx_skia_shared PROPERTIES
      OUTPUT_NAME crtgfx_skia
      LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
      ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
      WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
    if(CRT_TARGET_OS STREQUAL "windows")
      set_target_properties(crtgfx_skia_shared PROPERTIES
        ARCHIVE_OUTPUT_NAME crtgfx_skia_dll
      )
    endif()
  endif()
endfunction()
