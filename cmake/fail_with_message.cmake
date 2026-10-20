# `cmake -DCRT_FAIL_MESSAGE=... -P fail_with_message.cmake`: stop a build step with a readable
# message (used by dist targets whose prerequisites were configured out).
if(NOT DEFINED CRT_FAIL_MESSAGE)
  set(CRT_FAIL_MESSAGE "build step is not available in this configuration")
endif()
message(FATAL_ERROR "${CRT_FAIL_MESSAGE}")
