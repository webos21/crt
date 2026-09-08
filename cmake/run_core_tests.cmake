if(NOT DEFINED CRT_BINARY_DIR OR NOT DEFINED CRT_CTEST)
  message(FATAL_ERROR "CRT_BINARY_DIR and CRT_CTEST are required")
endif()

execute_process(
  COMMAND "${CRT_CTEST}" --output-on-failure
          -E "^(crtgfx_|crtmedia_|crtjs_|.*cxx)"
  WORKING_DIRECTORY "${CRT_BINARY_DIR}"
  RESULT_VARIABLE CRT_TEST_RESULT
)
if(NOT CRT_TEST_RESULT EQUAL 0)
  message(FATAL_ERROR "C-stage tests failed: ${CRT_TEST_RESULT}")
endif()
