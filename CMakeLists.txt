cmake_minimum_required(VERSION 3.25)

set(CRT_TARGET_OS "macos" CACHE STRING "Target OS personality")
set(CRT_TARGET_ARCH "host" CACHE STRING "Target architecture")
set(CRT_SYSROOT "${CMAKE_BINARY_DIR}/sysroot" CACHE PATH "Project sysroot")

if(CRT_TARGET_OS STREQUAL "windows")
  set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "" CACHE STRING "MSVC runtime library selection" FORCE)
  set(CMAKE_C_STANDARD_LIBRARIES "" CACHE STRING "C standard libraries" FORCE)
  set(CMAKE_CXX_STANDARD_LIBRARIES "" CACHE STRING "C++ standard libraries" FORCE)

  if(NOT CRT_TARGET_ARCH STREQUAL "host")
    # Same-OS cross-arch build: e.g. -DCRT_TARGET_ARCH=x86_64 from a
    # Windows aarch64 host (or vice versa). Clang is a genuine cross-
    # compiler needing no separate per-arch install, so this just means
    # telling it (and, transitively, its -fuse-ld=lld backend) a
    # different target triple -- must be set via CMAKE_<LANG>_COMPILER_
    # TARGET *before* project()/enable_language() for Clang to honor it.
    # "-pc-windows-msvc" (not "-w64-mingw32") matches what CMAKE_C_
    # COMPILER=clang already defaults to with no explicit --target at all
    # (this project's own code never branches on __GNUC__ vs _MSC_VER the
    # way third-party autoconf ports do, so nothing here needs the GNU-
    # compatible macro set tools/crt-cc adds for *port* builds -- see that
    # script's own comment for why *it* needs -w64-mingw32).
    if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
      message(FATAL_ERROR "CRT_TARGET_OS=windows with an explicit CRT_TARGET_ARCH override still requires a Windows host (same-OS cross-arch only; cross-OS is intentionally deferred, same as the CRT_TARGET_ARCH=host case above).")
    endif()
    if(CRT_TARGET_ARCH MATCHES "^(x86_64|AMD64|x64)$")
      set(CRT_CROSS_COMPILER_TARGET "x86_64-pc-windows-msvc")
    elseif(CRT_TARGET_ARCH MATCHES "^(arm64|ARM64|aarch64)$")
      set(CRT_CROSS_COMPILER_TARGET "aarch64-pc-windows-msvc")
    else()
      message(FATAL_ERROR "Unsupported CRT_TARGET_ARCH for a Windows cross-arch build: ${CRT_TARGET_ARCH} (expected x86_64 or arm64/aarch64).")
    endif()
    set(CMAKE_C_COMPILER_TARGET "${CRT_CROSS_COMPILER_TARGET}" CACHE STRING "Cross-arch Clang target triple")
    set(CMAKE_CXX_COMPILER_TARGET "${CRT_CROSS_COMPILER_TARGET}" CACHE STRING "Cross-arch Clang++ target triple")
    set(CMAKE_ASM_COMPILER_TARGET "${CRT_CROSS_COMPILER_TARGET}" CACHE STRING "Cross-arch Clang assembler target triple")
  endif()
elseif(CRT_TARGET_OS STREQUAL "linux")
  # This project's C++ runtime direction is libc++ specifically (README.md's
  # "Stack" section; the CMAKE_CXX_COMPILE_OBJECT rule override just below
  # already assumes libc++'s own include_next relationship with this
  # project's Bionic-shaped C headers), not GNU libstdc++ -- but unlike
  # Apple Clang on macOS, where libc++ is simply the unconditional default,
  # a real Linux Clang install picks whichever standard library the host
  # distro's toolchain defaults to, which is GNU libstdc++ on a stock
  # Debian/Ubuntu system (confirmed directly: `clang++ -E -x c++ -v
  # /dev/null` resolves to /usr/include/c++/13 there, not libc++'s
  # <llvm-prefix>/include/c++/v1, even with libc++-dev installed --
  # ``-stdlib=libc++`` genuinely has to be requested, installing the
  # package alone does not change Clang's own default). GNU libstdc++
  # cannot work here even with a fully correct include path: its own
  # header wrappers (<cstdlib> et al) pull in real glibc-internal headers
  # (<features.h> and friends) this project deliberately never exposes,
  # since it owns its own Bionic-compatible libc, not glibc -- confirmed
  # directly by a real build on a real Ubuntu/Debian aarch64 host, which
  # got exactly that "'features.h' file not found" failure once the
  # (separately real and still-needed) GNU-libstdc++ multiarch-include-dir
  # gap right below was fixed. Setting this here, before project()/
  # enable_language(CXX), is what makes CMake's own compiler-ABI detection
  # (CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES, consumed by crt_cxx_build_
  # flags below) see libc++'s real headers in the first place -- setting
  # it any later (e.g. only on crt_cxx_build_flags's own target_compile_
  # options) would be too late for that one-time detection pass, and this
  # project's real per-target compiles already separately pass
  # -stdlib=libc++ implicitly by using the exact same clang++ invocation
  # style tools/crt-c++ does. Requires libc++-dev (matching the active
  # Clang major version, e.g. `apt install libc++-18-dev libc++abi-18-dev`
  # on Ubuntu) to be installed; if it is not, this still resolves to GNU
  # libstdc++'s own paths (Clang's own fallback behavior for a
  # -stdlib=libc++ request with nothing installed to satisfy it), so the
  # underlying "features.h not found" failure surfaces as before with a
  # clear signal to install it, rather than a new, more confusing failure
  # mode.
  set(CMAKE_CXX_FLAGS "-stdlib=libc++" CACHE STRING "C++ flags" FORCE)
endif()

project(crt LANGUAGES C CXX ASM)

if(CRT_TARGET_OS STREQUAL "linux")
  # Only needed to steer the one-time CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES
  # detection just above (already complete by the time project() returns --
  # see "Detecting CXX compiler ABI info" in the configure log) towards
  # libc++ instead of Clang's own Linux default of GNU libstdc++. Every real
  # per-target C++ compile below already gets its own explicit -isystem/
  # -nostdinc++ flags from crt_cxx_build_flags, with no further use for
  # -stdlib= itself -- left in CMAKE_CXX_FLAGS (which every CXX compile
  # picks up via the CMAKE_CXX_COMPILE_OBJECT rule below), Clang's own
  # -Werror,-Wunused-command-line-argument rejects it on every compile-only
  # (-c) translation unit, since -nostdinc++ already fully determines header
  # search there. Reset back to empty now that its one job is done.
  set(CMAKE_CXX_FLAGS "" CACHE STRING "C++ flags" FORCE)
endif()

# CMake's default C++ rule writes target include paths before target compile
# options. That reverses libc++'s required include_next relationship with our
# Bionic-shaped C headers. Keep the C rule untouched, but place C++ flags
# first so crt_cxx_build_flags can establish the same ordering as crt-c++.
set(CMAKE_CXX_COMPILE_OBJECT
  "<CMAKE_CXX_COMPILER> <DEFINES> <FLAGS> <INCLUDES> -o <OBJECT> -c <SOURCE>")

if(CRT_TARGET_ARCH STREQUAL "host")
  if(CRT_TARGET_OS STREQUAL "macos" AND NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    message(FATAL_ERROR "CRT_TARGET_OS=macos can only be used on a macOS host in the current host-build profile. Reconfigure with --preset linux-host-ninja-debug on Linux, or clear the stale build cache.")
  elseif(CRT_TARGET_OS STREQUAL "linux" AND NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "CRT_TARGET_OS=linux can only be used on a Linux host in the current host-build profile. Reconfigure with the matching host preset, or clear the stale build cache.")
  elseif(CRT_TARGET_OS STREQUAL "windows" AND NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    message(FATAL_ERROR "CRT_TARGET_OS=windows can only be used on a Windows host in the current host-build profile. Cross-build support is intentionally deferred.")
  endif()
endif()

if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR "CRT requires Clang as the C compiler because the build uses Clang-specific freestanding ABI flags. Reconfigure with a preset that sets CMAKE_C_COMPILER=clang, or clear the stale build cache.")
endif()

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR "CRT requires Clang++ as the C++ compiler because the build uses Clang-specific freestanding ABI flags. Reconfigure with a preset that sets CMAKE_CXX_COMPILER=clang++, or clear the stale build cache.")
endif()

if(CRT_TARGET_OS STREQUAL "windows")
  set(CMAKE_C_STANDARD_LIBRARIES "")
  set(CMAKE_CXX_STANDARD_LIBRARIES "")
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

message(STATUS "CRT long double ABI follows the compiler target default; no -mlong-double override is applied")

option(CRT_ENABLE_CXX_RUNTIME "Prepare C++ runtime integration points" ON)
option(CRT_USE_COMPILER_RT "Link compiler-rt builtins when the compiler exposes them" ON)

include(GNUInstallDirs)
find_package(Python3 COMPONENTS Interpreter QUIET)
enable_testing()

if(Python3_Interpreter_FOUND)
  add_custom_target(check-import-manifest
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/check_import_manifest.py"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Checking Bionic import manifest"
  )
endif()

add_library(crt_build_flags INTERFACE)
target_compile_options(crt_build_flags INTERFACE
  -ffreestanding
  -fno-builtin
  -Xclang
  -fwchar-type=int
  -ffunction-sections
  -fdata-sections
  -Wall
  -Wextra
  -Werror
)
target_include_directories(crt_build_flags INTERFACE
  "$<$<COMPILE_LANGUAGE:C>:${CMAKE_SOURCE_DIR}/include>"
)
if(CRT_TARGET_OS STREQUAL "macos")
  target_compile_definitions(crt_build_flags INTERFACE CRT_TARGET_OS_MACOS=1)
elseif(CRT_TARGET_OS STREQUAL "linux")
  target_compile_definitions(crt_build_flags INTERFACE CRT_TARGET_OS_LINUX=1)
elseif(CRT_TARGET_OS STREQUAL "windows")
  target_compile_definitions(crt_build_flags INTERFACE CRT_TARGET_OS_WINDOWS=1)
endif()

add_library(crt_cxx_build_flags INTERFACE)
target_link_libraries(crt_cxx_build_flags INTERFACE crt_build_flags)
set(CRT_CXX_STANDARD_INCLUDE_DIRS)
foreach(CRT_CXX_IMPLICIT_INCLUDE_DIR IN LISTS CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES)
  # The first pattern used to be anchored to "include/c++" (e.g. plain
  # "/usr/include/c++/13"), which is what a from-source/non-multiarch GCC or
  # Clang install looks like -- but real Debian/Ubuntu GCC installs split
  # libstdc++ headers into that generic directory *plus* a second,
  # target-triple-specific one carrying the machine-generated
  # bits/c++config.h (e.g. "/usr/include/aarch64-linux-gnu/c++/13" on
  # aarch64, "/usr/include/x86_64-linux-gnu/c++/13" on x86_64), which does
  # not contain the literal substring "include/c++" at all and so was
  # silently dropped -- confirmed directly via `clang++ -E -x c++ -v
  # /dev/null` and CMakeCXXCompiler.cmake's own CMAKE_CXX_IMPLICIT_INCLUDE_
  # DIRECTORIES, both of which do list it. Missing it meant any C++
  # translation unit that reached <cstdlib> (or any other libstdc++ header
  # that itself #includes <bits/c++config.h>) failed with "'bits/c++config.h'
  # file not found" on exactly this class of real Linux host. Broadened to
  # match a "c++" path *component* anywhere (still requiring the trailing
  # version/backward directory or end-of-string, so it doesn't also grab
  # unrelated paths that merely contain the two characters "c++"). The
  # separate libc++ "/c++/v1$" pattern below still matches its own shape
  # too (already covered by the broader one, kept for clarity/history).
  # Windows paths may use backslashes and include MSVC or LLVM SDK headers.
  if(CRT_CXX_IMPLICIT_INCLUDE_DIR MATCHES "/c[+][+](/|$)" OR
     CRT_CXX_IMPLICIT_INCLUDE_DIR MATCHES "/c[+][+]/v1$" OR
     CRT_CXX_IMPLICIT_INCLUDE_DIR MATCHES "VC/Tools/MSVC/.*/include$" OR
     CRT_CXX_IMPLICIT_INCLUDE_DIR MATCHES "VC\\\\Tools\\\\MSVC\\\\.*\\\\include$" OR
     (CRT_TARGET_OS STREQUAL "windows" AND CRT_CXX_IMPLICIT_INCLUDE_DIR MATCHES "/include$"))
    list(APPEND CRT_CXX_STANDARD_INCLUDE_DIRS "${CRT_CXX_IMPLICIT_INCLUDE_DIR}")
  endif()
endforeach()
if(NOT CRT_CXX_STANDARD_INCLUDE_DIRS AND CRT_TARGET_OS STREQUAL "windows")
  # The foreach() above comes back completely empty on Windows, not merely a
  # regex miss like the Linux multiarch gap documented above it -- confirmed
  # directly against this exact machine's generated CMakeCXXCompiler.cmake:
  # CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES is genuinely set to "" for a
  # stock Windows LLVM clang++ install, because CMake's own implicit-
  # include-directory detection is written for GCC/Clang-GNU-style
  # compilers and does not run the same way for a Clang that simulates
  # MSVC (CMAKE_CXX_SIMULATE_ID=MSVC here, the *-pc-windows-msvc default
  # target with no explicit --target override) -- CMake expects MSVC-like
  # compilers to rely on the INCLUDE environment variable instead, which
  # this freestanding build intentionally never sets. The VC/Tools/MSVC/
  # .*/include$ pattern above was already written to handle exactly this
  # path shape but never got real data to test it against. Probe the
  # compiler directly the same way a human would (`clang++ -E -x c++ -v`
  # against an empty translation unit) and parse its own "#include <...>
  # search starts here" listing -- the same information CMake's own
  # detection reads on other platforms, just read here by hand since
  # CMake declines to for this compiler/target combination.
  set(CRT_CXX_STDINC_PROBE_SOURCE "${CMAKE_BINARY_DIR}/CMakeFiles/crt_cxx_stdinc_probe.cpp")
  if(NOT EXISTS "${CRT_CXX_STDINC_PROBE_SOURCE}")
    file(WRITE "${CRT_CXX_STDINC_PROBE_SOURCE}" "")
  endif()
  execute_process(
    COMMAND "${CMAKE_CXX_COMPILER}" -E -x c++ -v "${CRT_CXX_STDINC_PROBE_SOURCE}"
    OUTPUT_QUIET
    ERROR_VARIABLE CRT_CXX_STDINC_PROBE_OUTPUT
    RESULT_VARIABLE CRT_CXX_STDINC_PROBE_RESULT
  )
  if(CRT_CXX_STDINC_PROBE_RESULT EQUAL 0 AND
     CRT_CXX_STDINC_PROBE_OUTPUT MATCHES "search starts here:(.*)End of search list")
    string(REGEX REPLACE "\r?\n" ";" CRT_CXX_STDINC_PROBE_LINES "${CMAKE_MATCH_1}")
    foreach(CRT_CXX_STDINC_PROBE_LINE IN LISTS CRT_CXX_STDINC_PROBE_LINES)
      string(STRIP "${CRT_CXX_STDINC_PROBE_LINE}" CRT_CXX_STDINC_PROBE_LINE)
      # The captured block also contains the unrelated
      # `#include <...> search starts here:` header line (there are two
      # "search starts here:" markers in real clang -v output, one for
      # quoted includes and one for angle-bracket includes, and the regex
      # above has no non-greedy match in CMake to skip past the first) --
      # harmless, since it is not a real filesystem path and EXISTS()
      # filters it out here same as any other non-path noise would be.
      if(CRT_CXX_STDINC_PROBE_LINE AND EXISTS "${CRT_CXX_STDINC_PROBE_LINE}")
        list(APPEND CRT_CXX_STANDARD_INCLUDE_DIRS "${CRT_CXX_STDINC_PROBE_LINE}")
      endif()
    endforeach()
    if(CRT_CXX_STANDARD_INCLUDE_DIRS)
      message(STATUS "CRT located the Clang C++ standard-library include directories on Windows via a direct compiler probe: ${CRT_CXX_STANDARD_INCLUDE_DIRS}")
    endif()
  endif()
endif()
if(NOT CRT_CXX_STANDARD_INCLUDE_DIRS)
  if(CRT_TARGET_OS STREQUAL "windows")
    message(WARNING "CRT could not locate the Clang C++ standard-library include directory on Windows; continuing with limited C++ support")
  else()
    message(FATAL_ERROR "CRT could not locate the Clang C++ standard-library include directory")
  endif()
endif()
set(CRT_CXX_STANDARD_INCLUDE_FLAGS)
foreach(CRT_CXX_STANDARD_INCLUDE_DIR IN LISTS CRT_CXX_STANDARD_INCLUDE_DIRS)
  list(APPEND CRT_CXX_STANDARD_INCLUDE_FLAGS "-isystem${CRT_CXX_STANDARD_INCLUDE_DIR}")
endforeach()
execute_process(
  COMMAND "${CMAKE_CXX_COMPILER}" -print-resource-dir
  OUTPUT_VARIABLE CRT_CXX_RESOURCE_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
if(CRT_CXX_RESOURCE_DIR AND EXISTS "${CRT_CXX_RESOURCE_DIR}/include")
  list(APPEND CRT_CXX_STANDARD_INCLUDE_FLAGS "-isystem${CRT_CXX_RESOURCE_DIR}/include")
endif()
# libc++ C++ wrappers use include_next for their companion C headers. Keep the
# standard headers before the Bionic-compatible C surface and make both system
# paths, so a CMake C++ target follows tools/crt-c++'s include ordering.
target_compile_options(crt_cxx_build_flags INTERFACE
  $<$<COMPILE_LANGUAGE:CXX>:-nostdinc>
  $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<PLATFORM_ID:Windows>>>:-nostdinc++>
  $<$<COMPILE_LANGUAGE:CXX>:${CRT_CXX_STANDARD_INCLUDE_FLAGS}>
  $<$<COMPILE_LANGUAGE:CXX>:-isystem${CMAKE_SOURCE_DIR}/include>
)
target_compile_options(crt_cxx_build_flags INTERFACE
  $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
)
if(CRT_TARGET_OS STREQUAL "windows")
  target_compile_options(crt_cxx_build_flags INTERFACE
    $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>
  )
endif()

function(crt_configure_shared_runtime target_name)
  target_link_options(${target_name} PRIVATE
    -nostdlib
    -nodefaultlibs
  )
  if(CRT_TARGET_OS STREQUAL "macos")
    set_target_properties(${target_name} PROPERTIES
      BUILD_WITH_INSTALL_RPATH TRUE
      INSTALL_RPATH ""
    )
    target_link_libraries(${target_name} PRIVATE System)
  elseif(CRT_TARGET_OS STREQUAL "windows")
    target_link_options(${target_name} PRIVATE
      -fuse-ld=lld
      -Wl,/entry:crtDllMainCRTStartup
      -Wl,/OPT:REF
    )
    target_link_libraries(${target_name} PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
    if(TARGET crt1_pseudo_reloc)
      # dllcrt.c's crtDllMainCRTStartup() now calls _pei386_runtime_
      # relocator() on DLL_PROCESS_ATTACH (see that file's own comment);
      # the symbol itself lives in this OBJECT library, already linked
      # into every executable via CRT_STARTUP_OBJECTS (libc/CMakeLists.
      # txt) but never into a CMake-built shared target until now.
      # Confirmed for real: "lld-link: error: undefined symbol:
      # _pei386_runtime_relocator" building c_shared the first time
      # dllcrt.c's own call was added.
      target_link_libraries(${target_name} PRIVATE $<TARGET_OBJECTS:crt1_pseudo_reloc>)
    endif()
    if(TARGET crt1_dwarf_safety_net)
      # dllcrt.c's crtDllMainCRTStartup() also calls
      # _crt_install_dwarf_unwind_safety_net() on DLL_PROCESS_ATTACH now
      # (see that file's own comment and dwarf_unwind_safety_net.c's own
      # top comment) -- same reasoning and same wiring as
      # crt1_pseudo_reloc above.
      target_link_libraries(${target_name} PRIVATE $<TARGET_OBJECTS:crt1_dwarf_safety_net>)
    endif()
  elseif(CRT_TARGET_OS STREQUAL "linux")
    target_link_options(${target_name} PRIVATE
      -Wl,--gc-sections
    )
  endif()
endfunction()

if(CRT_TARGET_ARCH STREQUAL "host")
  set(CRT_EFFECTIVE_ARCH_FOR_FLAGS "${CMAKE_SYSTEM_PROCESSOR}")
else()
  set(CRT_EFFECTIVE_ARCH_FOR_FLAGS "${CRT_TARGET_ARCH}")
endif()

if(CRT_TARGET_OS STREQUAL "linux" AND CRT_EFFECTIVE_ARCH_FOR_FLAGS MATCHES "^(arm64|ARM64|aarch64)$")
  target_compile_options(crt_build_flags INTERFACE
    $<$<COMPILE_LANGUAGE:C>:-mno-outline-atomics>
  )
endif()

if(CRT_TARGET_OS STREQUAL "windows" AND CRT_EFFECTIVE_ARCH_FOR_FLAGS MATCHES "^(arm64|ARM64|aarch64)$")
  # Windows ARM64's ABI dedicates X18 as the platform register holding the
  # current thread's TEB pointer (NtCurrentTeb() == __getReg(18)); real
  # Windows system code always treats it as reserved. This build must too,
  # applied to every translation unit (not just the one that reads it),
  # since any function anywhere in the process that treats X18 as an
  # ordinary scratch register corrupts it for the rest of the program's
  # lifetime -- found the hard way while wiring up the memory-copy fork()'s
  # stack-bounds lookup (libc/src/arch/windows/aarch64/fork_memcopy.c),
  # which read garbage, differently-valued "TEB" addresses without this.
  target_compile_options(crt_build_flags INTERFACE -ffixed-x18)
endif()

if(CRT_USE_COMPILER_RT)
  execute_process(
    COMMAND "${CMAKE_C_COMPILER}" --print-libgcc-file-name
    OUTPUT_VARIABLE CRT_COMPILER_RT_BUILTINS
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  if(CRT_COMPILER_RT_BUILTINS AND EXISTS "${CRT_COMPILER_RT_BUILTINS}")
    message(STATUS "compiler-rt builtins: ${CRT_COMPILER_RT_BUILTINS}")
    add_library(crt_compiler_rt_builtins INTERFACE)
    target_link_libraries(crt_compiler_rt_builtins INTERFACE "${CRT_COMPILER_RT_BUILTINS}")
    install(FILES "${CRT_COMPILER_RT_BUILTINS}" DESTINATION lib RENAME libclang_rt.builtins.a)
  else()
    message(STATUS "compiler-rt builtins not found by ${CMAKE_C_COMPILER}")
    add_library(crt_compiler_rt_builtins INTERFACE)
  endif()
else()
  add_library(crt_compiler_rt_builtins INTERFACE)
endif()

if(CRT_TARGET_OS STREQUAL "windows")
  if(CRT_TARGET_ARCH STREQUAL "host")
    set(CRT_WINDOWS_SDK_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
  else()
    set(CRT_WINDOWS_SDK_ARCH "${CRT_TARGET_ARCH}")
  endif()

  if(CRT_WINDOWS_SDK_ARCH MATCHES "^(x86_64|AMD64|x64)$")
    set(CRT_WINDOWS_SDK_LIB_ARCH "x64")
  elseif(CRT_WINDOWS_SDK_ARCH MATCHES "^(arm64|ARM64|aarch64)$")
    set(CRT_WINDOWS_SDK_LIB_ARCH "arm64")
  else()
    set(CRT_WINDOWS_SDK_LIB_ARCH "${CRT_WINDOWS_SDK_ARCH}")
  endif()

  set(CRT_WINDOWS_SDK_LIB_CANDIDATES)
  foreach(CRT_WINDOWS_KITS_ROOT
      "$ENV{WindowsSdkDir}"
      "$ENV{ProgramFiles\(x86\)}/Windows Kits/10"
      "$ENV{ProgramFiles}/Windows Kits/10")
    if(CRT_WINDOWS_KITS_ROOT)
      file(GLOB CRT_WINDOWS_SDK_UM_DIRS
        LIST_DIRECTORIES true
        "${CRT_WINDOWS_KITS_ROOT}/Lib/*/um/${CRT_WINDOWS_SDK_LIB_ARCH}")
      list(APPEND CRT_WINDOWS_SDK_LIB_CANDIDATES ${CRT_WINDOWS_SDK_UM_DIRS})
    endif()
  endforeach()
  list(REMOVE_DUPLICATES CRT_WINDOWS_SDK_LIB_CANDIDATES)
  list(SORT CRT_WINDOWS_SDK_LIB_CANDIDATES COMPARE NATURAL ORDER DESCENDING)

  find_file(CRT_WINDOWS_KERNEL32_LIB
    NAMES kernel32.lib
    PATHS ${CRT_WINDOWS_SDK_LIB_CANDIDATES}
    NO_DEFAULT_PATH
  )
  find_file(CRT_WINDOWS_SYNCHRONIZATION_LIB
    NAMES synchronization.lib
    PATHS ${CRT_WINDOWS_SDK_LIB_CANDIDATES}
    NO_DEFAULT_PATH
  )
  if(CRT_WINDOWS_KERNEL32_LIB)
    message(STATUS "Windows kernel32 import library: ${CRT_WINDOWS_KERNEL32_LIB}")
  else()
    message(WARNING "kernel32.lib was not found. Use a Visual Studio Developer shell or set CRT_WINDOWS_KERNEL32_LIB.")
    set(CRT_WINDOWS_KERNEL32_LIB kernel32.lib CACHE FILEPATH "Windows kernel32 import library")
  endif()

  if(CRT_WINDOWS_SYNCHRONIZATION_LIB)
    message(STATUS "Windows synchronization import library: ${CRT_WINDOWS_SYNCHRONIZATION_LIB}")
  else()
    message(WARNING "synchronization.lib was not found. Use a Visual Studio Developer shell or set CRT_WINDOWS_SYNCHRONIZATION_LIB.")
    set(CRT_WINDOWS_SYNCHRONIZATION_LIB synchronization.lib CACHE FILEPATH "Windows synchronization import library")
  endif()

  set(CRT_WINDOWS_SYSTEM_LIBS
    "${CRT_WINDOWS_KERNEL32_LIB}"
    "${CRT_WINDOWS_SYNCHRONIZATION_LIB}"
  )
endif()

add_subdirectory(libc)
add_subdirectory(libm)
add_subdirectory(libdl)
add_subdirectory(libstdc++)
add_subdirectory(shell)
add_subdirectory(libcrtgfx)
add_subdirectory(libcrtjs)
add_subdirectory(libcrtmedia)

# Hoisted ahead of add_subdirectory(tests) below, out of the
# if(Python3_Interpreter_FOUND) block further down that otherwise defines
# it (that block also defines the real "rootfs" custom target itself,
# which genuinely does need targets add_subdirectory(shell) just defined
# above -- only the plain path *variable* needs to exist this early, not
# the target). tests/CMakeLists.txt's mksh-interpreter tests
# (mksh_subshell_status_test.c, mksh_shell_smoke_test.c) reference
# ${CRT_ROOTFS} directly in their own set_tests_properties(... ENVIRONMENT
# ...) calls -- on a from-scratch configure (a genuinely fresh clone, not
# a tree with a CRT_ROOTFS value already sitting in CMakeCache.txt from
# any earlier configure), add_subdirectory(tests) used to run before this
# variable was ever set at all, so those tests' own ENVIRONMENT baked in a
# literal empty "CRT_ROOTFS=" and every case failed outright with
# posix_spawn() ENOENT trying to resolve /bin/sh with no rootfs to
# resolve it against. Reproduced directly from a genuine fresh clone
# (this exact failure is also what broke CI, which always starts from a
# clean checkout) -- a locally re-used out/ directory never surfaced it,
# since CRT_ROOTFS was already cached from a much earlier configure.
if(Python3_Interpreter_FOUND)
  set(CRT_ROOTFS "${CMAKE_BINARY_DIR}/rootfs" CACHE PATH "Android-like runtime root filesystem")
endif()

add_subdirectory(tests)

install(DIRECTORY include/ DESTINATION include)

add_custom_target(sysroot
  COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}" --prefix "${CRT_SYSROOT}"
  DEPENDS c c_shared m m_shared dl dl_shared cxx cxx_shared crtgfx crtgfx_shared crtjs crtjs_shared crtmedia crtmedia_shared crt1 crt_shell_artifacts
  COMMENT "Installing CRT headers, libraries, and startup objects into ${CRT_SYSROOT}"
)
if(TARGET crt-libcxx-configure)
  add_dependencies(crt-libcxx-configure sysroot)
endif()
if(TARGET crtgfx-skia-configure)
  add_dependencies(crtgfx-skia-configure sysroot)
endif()
if(TARGET crtgfx-skia-build)
  add_dependencies(crtgfx-skia-build sysroot)
  if(CRT_USE_IMPORTED_LIBCXX)
    add_dependencies(crtgfx-skia-build crt-libcxx-sysroot)
  endif()
endif()

if(Python3_Interpreter_FOUND)
  # CRT_ROOTFS itself is now set earlier, right after add_subdirectory(shell)
  # above -- see that block's own comment for why. Everything below here
  # still needs this same if(Python3_Interpreter_FOUND) guard.
  # `crt_mksh_rootfs_*_runs` (shell/CMakeLists.txt) are Windows-only ctest
  # entries that exec against this real, on-disk rootfs (not the small
  # self-contained fixtures other tests build). This target has no OUTPUT, so
  # ninja/CMake cannot tell on its own whether it is stale; without ALL, a
  # plain `cmake --build` never regenerates it, and those tests silently run
  # against whatever rootfs happens to already be on disk (or none at all).
  # Windows is the only host where any ctest entry depends on it, so only
  # Windows needs it to build automatically as part of the default target.
  if(CRT_TARGET_OS STREQUAL "windows")
    set(CRT_ROOTFS_ALL_KEYWORD ALL)
  else()
    set(CRT_ROOTFS_ALL_KEYWORD)
  endif()
  set(CRT_ROOTFS_CXX_LIBRARY "$<TARGET_FILE:cxx_shared>")
  set(CRT_ROOTFS_CXX_ABI_LIBRARY "")
  if(CRT_USE_IMPORTED_LIBCXX)
    if(CRT_TARGET_OS STREQUAL "windows")
      set(CRT_ROOTFS_CXX_LIBRARY "${CRT_SYSROOT}/bin/c++.dll")
      set(CRT_ROOTFS_CXX_ABI_LIBRARY "${CRT_SYSROOT}/bin/c++abi.dll")
    elseif(CRT_TARGET_OS STREQUAL "macos")
      set(CRT_ROOTFS_CXX_LIBRARY "${CRT_SYSROOT}/lib/libc++.dylib")
      set(CRT_ROOTFS_CXX_ABI_LIBRARY "${CRT_SYSROOT}/lib/libc++abi.1.dylib")
    else()
      set(CRT_ROOTFS_CXX_LIBRARY "${CRT_SYSROOT}/lib/libc++.so")
      set(CRT_ROOTFS_CXX_ABI_LIBRARY "${CRT_SYSROOT}/lib/libc++abi.so.1")
    endif()
  endif()
  set(CRT_ROOTFS_CXX_ABI_ARGUMENTS "")
  if(CRT_ROOTFS_CXX_ABI_LIBRARY)
    list(APPEND CRT_ROOTFS_CXX_ABI_ARGUMENTS
      --runtime-library "${CRT_ROOTFS_CXX_ABI_LIBRARY}")
  endif()
  add_custom_target(rootfs ${CRT_ROOTFS_ALL_KEYWORD}
    COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
      "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/create_rootfs.py"
      --dest "${CRT_ROOTFS}"
      --target-os "${CRT_TARGET_OS}"
      --shell "$<TARGET_FILE:crt_tiny_sh>"
      --mksh "$<TARGET_FILE:crt_mksh>"
      --mkshrc "${CMAKE_SOURCE_DIR}/shell/mksh/mkshrc"
      --toybox "$<TARGET_FILE:crt_toybox>"
      --awk "$<TARGET_FILE:crt_awk>"
      --runtime-library "$<TARGET_FILE:c_shared>"
      --runtime-library "$<TARGET_FILE:m_shared>"
      --runtime-library "$<TARGET_FILE:dl_shared>"
      --runtime-library "${CRT_ROOTFS_CXX_LIBRARY}"
      ${CRT_ROOTFS_CXX_ABI_ARGUMENTS}
      --runtime-library "$<TARGET_FILE:crtgfx_shared>"
      --runtime-library "$<TARGET_FILE:crtjs_shared>"
      --runtime-library "$<TARGET_FILE:crtmedia_shared>"
    DEPENDS sysroot crt_shell_artifacts
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Creating Android-like CRT rootfs at ${CRT_ROOTFS}"
    USES_TERMINAL
  )
  if(CRT_USE_IMPORTED_LIBCXX)
    add_dependencies(rootfs crt-libcxx-sysroot)
  elseif(CRT_TARGET_OS STREQUAL "windows" AND TARGET crt-libcxx-configure)
    # Windows only, and only the other direction from the
    # CRT_USE_IMPORTED_LIBCXX=ON case just above (mutually exclusive, to
    # avoid a real dependency cycle: rootfs -> crt-libcxx-sysroot ->
    # crt-libcxx-build -> crt-libcxx-configure -> rootfs). Before a
    # successfully staged libc++ is actually in use, crt-libcxx-configure/
    # -build themselves need the rootfs to already exist first --
    # tools/crt-libcxx-build.py launches tools/crt-cc/tools/crt-c++
    # through the *rootfs*'s mksh.exe (see that script's own
    # common_cmake_args() comment for why: the sysroot's own separate
    # mksh.exe copy has no toybox alongside it, so it cannot resolve
    # "printf" and every compile fails outright).
    add_dependencies(crt-libcxx-configure rootfs)
    if(TARGET crt-libcxx-build)
      add_dependencies(crt-libcxx-build rootfs)
    endif()
  endif()
  if(CRT_TARGET_OS STREQUAL "windows" AND TARGET crtgfx-skia-build)
    # tools/build_skia.py's own --rootfs argument (libcrtgfx/CMakeLists.
    # txt's own CRTGFX_SKIA_BUILD_ARGS) needs the rootfs's mksh.exe to
    # exist -- same reasoning as crt-libcxx-configure/-build just above:
    # GN's own gcc_like toolchain invokes crt-cc.cmd/crt-c++.cmd, which
    # need CRT_MKSH_EXE pointed at a real, already-built mksh.exe.
    add_dependencies(crtgfx-skia-build rootfs)
    if(TARGET crtgfx-skia-configure)
      add_dependencies(crtgfx-skia-configure rootfs)
    endif()
  endif()

  get_filename_component(CRT_BUILD_PRESET_NAME "${CMAKE_BINARY_DIR}" NAME)
  set(CRT_PORT_TEST_ROOT "${CMAKE_BINARY_DIR}/port-tests")
  set(CRT_PORT_SOURCE_ROOT "${CRT_PORT_TEST_ROOT}/src")
  set(CRT_PORT_DOWNLOAD_ROOT "${CRT_PORT_TEST_ROOT}/downloads")
  set(CRT_PORT_RECIPE_ROOT "${CMAKE_SOURCE_DIR}/porting/recipes")
  file(GLOB CRT_PORT_RECIPE_FILES CONFIGURE_DEPENDS "${CRT_PORT_RECIPE_ROOT}/*.json")

  add_custom_target(port-list
    COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
      "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/fetch_ports.py"
      --recipe-dir "${CRT_PORT_RECIPE_ROOT}"
      --list
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Listing CRT porting recipes"
    USES_TERMINAL
  )
  add_custom_target(port-fetch
    COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
      "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/fetch_ports.py"
      --dest "${CRT_PORT_SOURCE_ROOT}"
      --cache "${CRT_PORT_DOWNLOAD_ROOT}"
      --recipe-dir "${CRT_PORT_RECIPE_ROOT}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Fetching and extracting all CRT porting recipe sources into ${CRT_PORT_SOURCE_ROOT}"
    USES_TERMINAL
  )

  function(crt_add_port_recipe_target recipe_name)
    add_custom_target(port-fetch-${recipe_name}
      COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
        "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/fetch_ports.py"
        --dest "${CRT_PORT_SOURCE_ROOT}"
        --cache "${CRT_PORT_DOWNLOAD_ROOT}"
        --recipe-dir "${CRT_PORT_RECIPE_ROOT}"
        --port "${recipe_name}"
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      COMMENT "Fetching and extracting ${recipe_name} into ${CRT_PORT_SOURCE_ROOT}"
      USES_TERMINAL
    )
  endfunction()

  function(crt_add_build_port_target recipe_name recipe_build_system)
    crt_add_port_recipe_target(${recipe_name})
    set(CRT_PORT_BUILD_DEPENDS sysroot port-fetch-${recipe_name})
    set(CRT_PORT_BUILD_SHELL_ARGS)
    if((recipe_build_system STREQUAL "configure" OR
        recipe_build_system STREQUAL "android_host_tool" OR
        recipe_build_system STREQUAL "amalgamation") AND
       CRT_TARGET_OS STREQUAL "windows")
      # --use-crt-shell (route configure/make/crt-cc invocations through
      # this project's own rootfs mksh instead of the host's shell) is a
      # native-Windows-only workaround: on native Windows,
      # tools/crt-cc/tools/crt-c++ are themselves shebang shell scripts
      # (no .exe), which CreateProcess cannot launch directly (Windows has
      # no shebang interpretation) -- without --use-crt-shell,
      # crt-port-build.py's make_env() hands the bare script path straight
      # to subprocess.run() as argv[0] and it fails to even start. Applies
      # equally to the amalgamation build system (e.g.
      # sqlite-amalgamation): it never runs a configure script, but it
      # still shells out to crt-cc per translation unit the same way.
      #
      # Deliberately NOT set for macOS/Linux, even though those hosts can
      # also pass --use-crt-shell: those hosts already have a real,
      # complete, natively-shebang-capable shell and coreutils
      # (bash/dash + real GNU/BSD awk/grep/sed/...), which is exactly the
      # environment every one of these upstream configure scripts is
      # actually tested against. --use-crt-shell's PATH (rootfs's
      # system/bin ahead of the host PATH) would instead route macOS/
      # Linux configure probes through this project's own toybox
      # applets -- built deliberately minimal for Windows, where no real
      # POSIX userland exists at all -- for no compensating benefit
      # (nothing in any recipe's macos/linux target_overrides needs it;
      # only tools/crt-cc/tools/crt-c++, unconditionally set as CC/CXX
      # either way, carry the sysroot integration). This also removes a
      # real per-process-spawn overhead source from macOS/Linux configure
      # runs, which invoke the compiler-probe/subshell machinery
      # thousands of times over.
      set(CRT_PORT_BUILD_DEPENDS rootfs port-fetch-${recipe_name})
      set(CRT_PORT_BUILD_SHELL_ARGS --use-crt-shell)
    endif()
    add_custom_target(port-build-${recipe_name}
      COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
        "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/crt-port-build.py"
        --preset "${CRT_BUILD_PRESET_NAME}"
        --target-os "${CRT_TARGET_OS}"
        --target-arch "${CRT_EFFECTIVE_ARCH_FOR_FLAGS}"
        --recipe-dir "${CRT_PORT_RECIPE_ROOT}"
        --port "${recipe_name}"
        ${CRT_PORT_BUILD_SHELL_ARGS}
        --skip-sysroot-build
      DEPENDS ${CRT_PORT_BUILD_DEPENDS}
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      COMMENT "Building ${recipe_name} (${recipe_build_system}) against the CRT sysroot"
      USES_TERMINAL
    )
    add_custom_target(port-rebuild-${recipe_name}
      COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
        "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/crt-port-build.py"
        --preset "${CRT_BUILD_PRESET_NAME}"
        --target-os "${CRT_TARGET_OS}"
        --target-arch "${CRT_EFFECTIVE_ARCH_FOR_FLAGS}"
        --recipe-dir "${CRT_PORT_RECIPE_ROOT}"
        --port "${recipe_name}"
        ${CRT_PORT_BUILD_SHELL_ARGS}
        --rebuild
        --skip-sysroot-build
      DEPENDS ${CRT_PORT_BUILD_DEPENDS}
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      COMMENT "Rebuilding ${recipe_name} (${recipe_build_system}) against the CRT sysroot"
      USES_TERMINAL
    )
    add_custom_target(port-test-${recipe_name}
      COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
        "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/crt-port-build.py"
        --preset "${CRT_BUILD_PRESET_NAME}"
        --target-os "${CRT_TARGET_OS}"
        --target-arch "${CRT_EFFECTIVE_ARCH_FOR_FLAGS}"
        --recipe-dir "${CRT_PORT_RECIPE_ROOT}"
        --port "${recipe_name}"
        ${CRT_PORT_BUILD_SHELL_ARGS}
        --test
        --skip-sysroot-build
      DEPENDS port-build-${recipe_name}
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      COMMENT "Running ${recipe_name} recipe tests against the CRT sysroot"
      USES_TERMINAL
    )
  endfunction()

  set(CRT_CONFIGURE_PORT_TARGETS)
  set(CRT_CONFIGURE_PORT_REBUILD_TARGETS)
  set(CRT_AUTOMATED_PORT_TARGETS)
  set(CRT_AUTOMATED_PORT_REBUILD_TARGETS)
  set(CRT_AUTOMATED_PORT_TEST_TARGETS)
  foreach(CRT_PORT_RECIPE_FILE IN LISTS CRT_PORT_RECIPE_FILES)
    file(READ "${CRT_PORT_RECIPE_FILE}" CRT_PORT_RECIPE_JSON)
    string(JSON CRT_PORT_RECIPE_NAME GET "${CRT_PORT_RECIPE_JSON}" name)
    string(JSON CRT_PORT_RECIPE_BUILD_SYSTEM GET "${CRT_PORT_RECIPE_JSON}" build system)
    set(CRT_PORT_RECIPE_AUTOMATED "true")
    string(JSON CRT_PORT_RECIPE_AUTOMATED_VALUE
      ERROR_VARIABLE CRT_PORT_RECIPE_AUTOMATED_ERROR
      GET "${CRT_PORT_RECIPE_JSON}" build automated)
    if(NOT CRT_PORT_RECIPE_AUTOMATED_ERROR STREQUAL "NOTFOUND")
      set(CRT_PORT_RECIPE_AUTOMATED_VALUE "true")
    endif()
    if((CRT_PORT_RECIPE_BUILD_SYSTEM STREQUAL "configure" OR
        CRT_PORT_RECIPE_BUILD_SYSTEM STREQUAL "amalgamation" OR
        CRT_PORT_RECIPE_BUILD_SYSTEM STREQUAL "android_host_tool") AND
       CRT_PORT_RECIPE_AUTOMATED_VALUE)
      crt_add_build_port_target(${CRT_PORT_RECIPE_NAME} ${CRT_PORT_RECIPE_BUILD_SYSTEM})
      list(APPEND CRT_AUTOMATED_PORT_TARGETS port-build-${CRT_PORT_RECIPE_NAME})
      list(APPEND CRT_AUTOMATED_PORT_REBUILD_TARGETS port-rebuild-${CRT_PORT_RECIPE_NAME})
      string(JSON CRT_PORT_RECIPE_TESTS
        ERROR_VARIABLE CRT_PORT_RECIPE_TESTS_ERROR
        GET "${CRT_PORT_RECIPE_JSON}" tests)
      if(CRT_PORT_RECIPE_TESTS_ERROR STREQUAL "NOTFOUND")
        list(APPEND CRT_AUTOMATED_PORT_TEST_TARGETS port-test-${CRT_PORT_RECIPE_NAME})
      endif()
      if(CRT_PORT_RECIPE_BUILD_SYSTEM STREQUAL "configure")
        list(APPEND CRT_CONFIGURE_PORT_TARGETS port-build-${CRT_PORT_RECIPE_NAME})
        list(APPEND CRT_CONFIGURE_PORT_REBUILD_TARGETS port-rebuild-${CRT_PORT_RECIPE_NAME})
      endif()
    else()
      crt_add_port_recipe_target(${CRT_PORT_RECIPE_NAME})
    endif()
  endforeach()

  add_custom_target(port-build-configure
    DEPENDS ${CRT_CONFIGURE_PORT_TARGETS}
  )
  add_custom_target(port-rebuild-configure
    DEPENDS ${CRT_CONFIGURE_PORT_REBUILD_TARGETS}
  )
  add_custom_target(port-build-recipes
    DEPENDS ${CRT_AUTOMATED_PORT_TARGETS}
  )
  add_custom_target(port-rebuild-recipes
    DEPENDS ${CRT_AUTOMATED_PORT_REBUILD_TARGETS}
  )
  add_custom_target(port-test-recipes
    DEPENDS ${CRT_AUTOMATED_PORT_TEST_TARGETS}
  )
endif()
