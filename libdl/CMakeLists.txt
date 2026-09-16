if(CRT_TARGET_OS STREQUAL "macos")
  set(CRT_DL_BACKEND_SOURCE src/arch/macos/dl_macos.c)
elseif(CRT_TARGET_OS STREQUAL "linux")
  set(CRT_DL_BACKEND_SOURCE src/arch/linux/dl_linux.c)
elseif(CRT_TARGET_OS STREQUAL "windows")
  set(CRT_DL_BACKEND_SOURCE src/arch/windows/dl_windows.c)
else()
  message(FATAL_ERROR "Unsupported CRT_TARGET_OS: ${CRT_TARGET_OS}")
endif()

set(CRT_DL_SOURCES
  src/dl.c
  ${CRT_DL_BACKEND_SOURCE}
)

add_library(dl STATIC ${CRT_DL_SOURCES})

target_link_libraries(dl PRIVATE crt_build_flags c)
# dl_macos.c reuses libc's private Mach-O export-trie engine (see
# libc/include/private/crt_macho_symbol.h) instead of keeping its own copy;
# libdl already fully depends on libc, so seeing libc's private headers too
# is not a new dependency, just a wider one.
target_include_directories(dl PRIVATE "${CMAKE_SOURCE_DIR}/libc/include")
set_target_properties(dl PROPERTIES
  OUTPUT_NAME dl
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

add_library(dl_shared SHARED ${CRT_DL_SOURCES})
if(CRT_TARGET_OS STREQUAL "windows")
  target_sources(dl_shared PRIVATE $<TARGET_OBJECTS:crt_windows_dllcrt>)
endif()

target_link_libraries(dl_shared PRIVATE crt_build_flags c_shared)
target_include_directories(dl_shared PRIVATE "${CMAKE_SOURCE_DIR}/libc/include")
crt_configure_shared_runtime(dl_shared)
set_target_properties(dl_shared PROPERTIES
  OUTPUT_NAME dl
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)
if(CRT_TARGET_OS STREQUAL "windows")
  set_target_properties(dl_shared PROPERTIES
    ARCHIVE_OUTPUT_NAME dl_dll
  )
endif()
if(CRT_TARGET_OS STREQUAL "linux")
  # CRT_1.0 ELF symbol-version namespace, same fix and same reason as
  # c_shared's own identical treatment (libc/CMakeLists.txt, 2026-09-14):
  # this library's own dlopen/dlsym/dlclose exports previously carried no
  # version information at all (confirmed via `readelf --version-info`/
  # `nm -D` -- plain `T dlopen`, no `@` suffix). A real host consumer that
  # also needs *this same process's* real glibc dlopen -- the Vulkan
  # loader's own internal ICD-loading dlopen() call, versioned
  # `dlopen@GLIBC_2.34` by libvulkan.so.1's own NEEDED record -- can
  # therefore have that versioned lookup incorrectly satisfied by this
  # unversioned library instead of real glibc's own same-named, correctly
  # versioned definition (confirmed for real via `LD_DEBUG=bindings` on
  # the packaged `crtgfx_skia_gpu_window_demo`: `binding file libvulkan.
  # so.1 ... to .../lib/libdl.so: normal symbol 'dlopen' [GLIBC_2.34]`) --
  # the exact same "no version info can satisfy any version request"
  # mechanism already fixed once for libc.so's own strtof_l/readdir, just
  # never applied to this separate DSO. The practical, real-world effect:
  # the host Vulkan loader's dlopen() calls silently no-op through this
  # library's own (non-loader) implementation, so it can never actually
  # load an ICD -- `crtgfx_skia_gpu_window_demo` reports device_count=0
  # even on a host with a real, working Vulkan ICD. A private version
  # node (own SONAME-scoped, independent of c_shared's identically named
  # node) fixes this the same way: real glibc-linked consumers' own
  # `@GLIBC_2.xx` references keep skipping this library's differently
  # versioned symbols regardless of load order, and this project's own
  # CRT_1.0-rebuilt consumers keep resolving correctly to this library.
  set(CRT_DL_VERSION_MAP "${CMAKE_CURRENT_BINARY_DIR}/crt_dl_version.map")
  file(WRITE "${CRT_DL_VERSION_MAP}" [[CRT_1.0 {
  global: *;
};
]])
  set_target_properties(dl_shared PROPERTIES LINK_DEPENDS "${CRT_DL_VERSION_MAP}")
  target_link_options(dl_shared PRIVATE "-Wl,--version-script=${CRT_DL_VERSION_MAP}")
endif()

install(TARGETS dl dl_shared
  ARCHIVE DESTINATION lib COMPONENT crt-c
  LIBRARY DESTINATION lib COMPONENT crt-c
  RUNTIME DESTINATION bin COMPONENT crt-c
)
