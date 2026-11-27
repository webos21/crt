# Fails unless a PE DLL's export table holds exactly the expected names.
#
#   cmake -DREADOBJ=<llvm-readobj> -DDLL=<path> -DEXPECTED=<name[,name...]> -P check_dll_exports.cmake
#
# Used on a DLL built by tools/crt-cc -shared that calls libc: GNU-ABI DLL
# links export every global they define, so a wrapper that put the static CRT
# archives back on a -shared link line would show up here as extra exports.

foreach(required READOBJ DLL EXPECTED)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "check_dll_exports: -D${required}= is required")
  endif()
endforeach()

execute_process(
  COMMAND "${READOBJ}" --coff-exports "${DLL}"
  OUTPUT_VARIABLE output
  RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "check_dll_exports: ${READOBJ} failed on ${DLL}: ${result}")
endif()

string(REGEX MATCHALL "Name: [^\r\n]+" actual "${output}")
list(TRANSFORM actual REPLACE "^Name: " "")
list(SORT actual)
string(REPLACE "," ";" expected "${EXPECTED}")
list(SORT expected)

if(NOT actual STREQUAL expected)
  list(LENGTH actual count)
  list(SUBLIST actual 0 20 sample)
  message(FATAL_ERROR
    "check_dll_exports: ${DLL} exports ${count} name(s), expected '${expected}'. First names: ${sample}")
endif()

message("check_dll_exports: ok")
