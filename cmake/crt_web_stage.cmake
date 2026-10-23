# Web remains built by the pinned isolated 05-ui -> 06-web driver, not by
# duplicating PlatformCRT/WebKit's source graph in the top-level project.
set(CRT_WEB_PREDECESSOR "" CACHE PATH "External 05-ui predecessor (empty builds the cumulative in-tree SDK)")
set(CRT_WEB_STAGE_RECIPE "" CACHE FILEPATH "Pinned 06-web source recipe (empty uses the generated recipe)")
set(CRT_WEB_STAGE_ASSET "" CACHE FILEPATH "Local pinned 06-web source archive")
set(CRT_WEB_SOURCE_CACHE "" CACHE PATH "Verified source-only WebKit cache")
set(CRT_WEB_RETRY_RUN "" CACHE PATH "Explicit failed run-* child for guarded isolated recovery (empty starts fresh)")
set(CRT_WEB_PREVIOUS_SOURCE_ASSET "" CACHE FILEPATH "With retry-run: verified old source archive for packaging-only repair")
set(CRT_WEB_REPLAY_SDK "" CACHE PATH "Explicit existing 06-web SDK for test/dist replay only; does not validate a new build")
set(CRT_WEB_WORK_ROOT "${CMAKE_BINARY_DIR}/isolated-web" CACHE PATH "Web build and test work roots")
set(CRT_WEB_JOBS "4" CACHE STRING "Bounded isolated Web dependency/build parallelism")
set(CRT_WEB_HOST_PERL_LIB "$ENV{CRT_WEB_HOST_PERL_LIB}" CACHE PATH "Prepared Windows POSIX build-host Perl modules; never target SDK content")
if(Python3_Interpreter_FOUND)
  set(crt_web_host_inputs)
  foreach(crt_web_host_program IN ITEMS cmake ninja git patch perl ruby openssl clang clang++ cc c++ gcc g++ make gmake mingw32-make)
    unset(crt_web_host_found)
    find_program(crt_web_host_found NAMES "${crt_web_host_program}" NO_CACHE)
    if(crt_web_host_found)
      list(APPEND crt_web_host_inputs "${crt_web_host_found}")
    endif()
  endforeach()
  set(crt_web_build_depends)
  set(crt_web_predecessor "${CRT_WEB_PREDECESSOR}")
  if(NOT crt_web_predecessor)
    set(crt_web_predecessor "${CRT_DIST_ROOT}/05-ui")
    list(APPEND crt_web_build_depends crt-ui-dist)
  endif()
  set(crt_web_recipe "${CRT_WEB_STAGE_RECIPE}")
  if(NOT crt_web_recipe)
    if(CRT_WEB_PREDECESSOR)
      set(crt_web_recipe "${crt_web_predecessor}/stages/recipes/06-web.json")
    else()
      set(crt_web_recipe "${CRT_STAGE_SOURCE_ROOT}/06-web.json")
      list(APPEND crt_web_build_depends crt-stage-06-source)
    endif()
  endif()
  set(crt_web_optional)
  if(CRT_WEB_STAGE_ASSET)
    list(APPEND crt_web_optional --asset "${CRT_WEB_STAGE_ASSET}")
  endif()
  if(CRT_WEB_SOURCE_CACHE)
    list(APPEND crt_web_optional --source-cache "${CRT_WEB_SOURCE_CACHE}")
  endif()
  if(CRT_WEB_RETRY_RUN)
    list(APPEND crt_web_optional --retry-run "${CRT_WEB_RETRY_RUN}")
  endif()
  if(CRT_WEB_PREVIOUS_SOURCE_ASSET)
    list(APPEND crt_web_optional --previous-source-asset "${CRT_WEB_PREVIOUS_SOURCE_ASSET}")
  endif()
  # Pass the actual selected host tools instead of relying on the ambient PATH.
  set(crt_web_env "${CMAKE_COMMAND}" -E env
    "CRT_CC=${CMAKE_C_COMPILER}" "CRT_CXX=${CMAKE_CXX_COMPILER}"
    "CRT_AR=${CMAKE_AR}" "CRT_RANLIB=${CMAKE_RANLIB}")
  if(CRT_TARGET_OS STREQUAL "windows")
    get_filename_component(crt_web_sdk_libpath "${CRT_WINDOWS_KERNEL32_LIB}" DIRECTORY)
    list(APPEND crt_web_env "CRT_WINDOWS_SDK_LIBPATH=${crt_web_sdk_libpath}")
    list(APPEND crt_web_env "CRT_WEB_HOST_PERL_LIB=${CRT_WEB_HOST_PERL_LIB}")
    if(CRT_WEB_HOST_PERL_LIB)
      list(APPEND crt_web_host_inputs "${CRT_WEB_HOST_PERL_LIB}")
    endif()
    foreach(crt_web_git_program IN ITEMS bin/bash.exe usr/bin/sh.exe usr/bin/perl.exe)
      list(APPEND crt_web_host_inputs "$ENV{ProgramFiles}/Git/${crt_web_git_program}")
    endforeach()
  endif()
  # Canonicalize CMake's AMD64/ARM64 spellings for manifest pair comparison.
  string(TOLOWER "${CRT_EFFECTIVE_ARCH_FOR_FLAGS}" crt_web_arch)
  if(crt_web_arch MATCHES "^(amd64|x64)$")
    set(crt_web_arch x86_64)
  elseif(crt_web_arch STREQUAL "arm64")
    set(crt_web_arch aarch64)
  endif()
  if((CRT_TARGET_OS STREQUAL "linux" AND NOT crt_web_arch STREQUAL "x86_64") OR
     (CRT_TARGET_OS STREQUAL "windows" AND NOT crt_web_arch STREQUAL "x86_64") OR
     (CRT_TARGET_OS STREQUAL "macos" AND NOT crt_web_arch STREQUAL "aarch64"))
    # Unsupported pairs must fail before building a costly predecessor.
    set(crt_web_build_depends)
  endif()
  set(crt_web_command ${crt_web_env} "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/crt_web_stage.py"
    --target-os "${CRT_TARGET_OS}" --target-arch "${crt_web_arch}"
    --work-root "${CRT_WEB_WORK_ROOT}" --jobs "${CRT_WEB_JOBS}")
  crt_add_cached_stage_target(crt-web-build NAME web-build STAGE web
    DEPENDS ${crt_web_build_depends}
    INPUTS "${crt_web_predecessor}" "${crt_web_recipe}" ${CRT_WEB_STAGE_ASSET} ${CRT_WEB_PREVIOUS_SOURCE_ASSET} ${crt_web_host_inputs}
    OUTPUTS "${CRT_DIST_ROOT}/06-web"
    COMMAND ${crt_web_command} --phase build --sdk "${CRT_DIST_ROOT}/06-web"
      --predecessor "${crt_web_predecessor}" --recipe "${crt_web_recipe}" ${crt_web_optional})
  set(crt_web_sdk "${CRT_DIST_ROOT}/06-web")
  set(crt_web_test_depends crt-web-build)
  if(CRT_WEB_REPLAY_SDK)
    set(crt_web_sdk "${CRT_WEB_REPLAY_SDK}")
    set(crt_web_test_depends)
  endif()
  crt_add_cached_stage_target(crt-web-test NAME web-test STAGE web
    DEPENDS ${crt_web_test_depends}
    INPUTS "${crt_web_sdk}" ${crt_web_host_inputs}
    OUTPUTS "${CRT_WEB_WORK_ROOT}/test-result.json"
    COMMAND ${crt_web_command} --phase test --sdk "${crt_web_sdk}")
  # Keep dist verification/archive explicit even if a prior test cache hit.
  add_custom_target(crt-web-dist
    COMMAND ${crt_web_command} --phase dist --sdk "${crt_web_sdk}" --archive-dir "${CRT_DIST_ROOT}"
    DEPENDS crt-web-test VERBATIM USES_TERMINAL)
endif()
