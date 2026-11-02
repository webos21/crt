# Host tools are an independently selected native artifact, never a target SDK.
set(CRT_HOST_TOOLS_PREDECESSOR "" CACHE PATH "Native public SDK supplying CRT host-tool binaries")
set(CRT_HOST_TOOLS_OUTPUT "${CMAKE_BINARY_DIR}/host-tools" CACHE PATH "Independent CRT host-tools candidate")
set(CRT_HOST_TOOLS_VERSION "${CRT_RELEASE_TAG}-host-tools" CACHE STRING "Independent local host-tools version")
if(Python3_Interpreter_FOUND)
  set(crt_host_tools_sdk "${CRT_HOST_TOOLS_PREDECESSOR}")
  set(crt_host_tools_depends)
  if(NOT crt_host_tools_sdk)
    set(crt_host_tools_sdk "${CRT_DIST_ROOT}/01-c")
    set(crt_host_tools_depends crt-c-dist)
  endif()
  crt_add_cached_stage_target(crt-host-tools NAME host-tools STAGE c
    DEPENDS ${crt_host_tools_depends}
    INPUTS "${crt_host_tools_sdk}" "${CMAKE_SOURCE_DIR}/third_party/crt-runtime/make.COPYING"
      "${CMAKE_SOURCE_DIR}/third_party/crt-runtime/make.COPYING.source.json"
      "${CMAKE_SOURCE_DIR}/porting/recipes/make.json" "${CMAKE_SOURCE_DIR}/third_party/crt-runtime/components.json"
    OUTPUTS "${CRT_HOST_TOOLS_OUTPUT}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/create_host_tools.py"
      --sdk-root "${crt_host_tools_sdk}" --output "${CRT_HOST_TOOLS_OUTPUT}"
      --version "${CRT_HOST_TOOLS_VERSION}")
  add_custom_target(crt-host-tools-test
    DEPENDS crt-host-tools
    COMMAND "${Python3_EXECUTABLE}" -c
      "import sys; from pathlib import Path; sys.path.insert(0, sys.argv[1]); from create_host_tools import verify_bundle; verify_bundle(Path(sys.argv[2])); print('CRT host-tools inventory/native-image verification passed')"
      "${CMAKE_SOURCE_DIR}/tools" "${CRT_HOST_TOOLS_OUTPUT}"
    VERBATIM USES_TERMINAL)
endif()
