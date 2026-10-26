set(CRT_DEV_PREDECESSOR "" CACHE PATH "External plain 02-cxx for the Development SDK (empty builds it in-tree)")
set(CRT_DEV_WORK_ROOT "${CMAKE_BINARY_DIR}/development-sdk-work" CACHE PATH "Development SDK build work root")
set(CRT_DEV_OUTPUT "${CRT_DIST_ROOT}/dev" CACHE PATH "Development SDK output (manifest remains 02-cxx)")
set(CRT_RELEASE_OUTPUT_ROOT "${CMAKE_BINARY_DIR}/release" CACHE PATH "Local prepared release assets; never uploaded")
if(Python3_Interpreter_FOUND)
  set(crt_dev_predecessor "${CRT_DEV_PREDECESSOR}")
  set(crt_dev_depends)
  if(NOT crt_dev_predecessor)
    set(crt_dev_predecessor "${CRT_DIST_ROOT}/02-cxx")
    set(crt_dev_depends crt-cxx-dist)
  endif()
  crt_add_cached_stage_target(crt-development-sdk NAME dev-sdk STAGE cxx
    DEPENDS ${crt_dev_depends}
    INPUTS "${crt_dev_predecessor}" "${CMAKE_SOURCE_DIR}/tools/build_dev_sdk.py"
      "${CMAKE_SOURCE_DIR}/porting/recipes/curl.json" "${CMAKE_SOURCE_DIR}/porting/recipes/mbedtls.json"
      "${CMAKE_SOURCE_DIR}/porting/recipes/zlib.json" "${CMAKE_SOURCE_DIR}/tools/build_stage_04_gfx_media.py"
    OUTPUTS "${CRT_DEV_OUTPUT}"
    COMMAND ${crt_web_env} "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/build_dev_sdk.py"
      --sdk-root "${crt_dev_predecessor}" --work-root "${CRT_DEV_WORK_ROOT}" --output "${CRT_DEV_OUTPUT}")
  crt_add_cached_stage_target(crt-development-sdk-test NAME dev-sdk-test STAGE cxx
    DEPENDS crt-development-sdk
    INPUTS "${CRT_DEV_OUTPUT}" "${CMAKE_SOURCE_DIR}/libstdc++/tests/development_sdk_consumer.cc"
      "${CMAKE_SOURCE_DIR}/tools/test_crtweb_network.py"
    OUTPUTS "${CMAKE_BINARY_DIR}/development-consumer/result.json"
    COMMAND ${crt_web_env} "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/test_dev_sdk_consumer.py"
      --sdk-root "${CRT_DEV_OUTPUT}" --output "${CMAKE_BINARY_DIR}/development-consumer")
  set(crt_release_sdk "${CRT_DIST_ROOT}/06-web")
  if(CRT_WEB_REPLAY_SDK)
    set(crt_release_sdk "${CRT_WEB_REPLAY_SDK}")
  endif()
  set(crt_dev_release_depends)
  set(crt_full_release_depends)
  # Invalid/local tags fail immediately, without paying for a WebKit rebuild.
  if(CRT_RELEASE_TAG MATCHES "^v[0-9]+[.][0-9]+[.][0-9]+(-[0-9A-Za-z][0-9A-Za-z.]*)?$")
    set(crt_dev_release_depends crt-development-sdk-test)
    set(crt_full_release_depends crt-development-sdk-test crt-web-dist)
  endif()
  add_custom_target(crt-dev-release
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/crt_sdk_release.py"
      --profile dev --version "${CRT_RELEASE_TAG}" --development-sdk "${CRT_DEV_OUTPUT}"
      --full-sdk "${crt_dev_predecessor}" --source-assets "${CRT_STAGE_SOURCE_ROOT}"
      --output-dir "${CRT_RELEASE_OUTPUT_ROOT}/dev"
    DEPENDS ${crt_dev_release_depends} VERBATIM USES_TERMINAL)
  add_custom_target(crt-full-release
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/crt_sdk_release.py"
      --profile full --version "${CRT_RELEASE_TAG}" --development-sdk "${CRT_DEV_OUTPUT}"
      --full-sdk "${crt_release_sdk}" --source-assets "${CRT_STAGE_SOURCE_ROOT}"
      --output-dir "${CRT_RELEASE_OUTPUT_ROOT}/full"
    DEPENDS ${crt_full_release_depends} VERBATIM USES_TERMINAL)
endif()
