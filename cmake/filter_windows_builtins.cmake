# libc owns emulated TLS; the vendor eprintf member embeds UCRT forwarding
# symbols too. Filter those two members without touching an unchanged output:
# needless mtime changes otherwise relink libc.dll and invalidate every nested
# C++ runtime build through its (correct) predecessor-byte fingerprint.
function(crt_filter_windows_builtins source output archiver)
  get_filename_component(directory "${output}" DIRECTORY)
  file(MAKE_DIRECTORY "${directory}")
  set(candidate "${output}.candidate")
  file(COPY_FILE "${source}" "${candidate}")
  execute_process(COMMAND "${archiver}" t "${candidate}"
    OUTPUT_VARIABLE members OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE result)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "cannot inspect compiler-rt builtins with ${archiver}")
  endif()
  string(REPLACE "\r\n" ";" members "${members}")
  string(REPLACE "\n" ";" members "${members}")
  foreach(member IN LISTS members)
    if(member MATCHES "(^|[\\\\/])(emutls|eprintf)\\.c\\.(obj|o)$")
      execute_process(COMMAND "${archiver}" d "${candidate}" "${member}" RESULT_VARIABLE result)
      if(NOT result EQUAL 0)
        message(FATAL_ERROR "cannot remove ${member} from compiler-rt builtins")
      endif()
    endif()
  endforeach()
  file(COPY_FILE "${candidate}" "${output}" ONLY_IF_DIFFERENT)
  file(REMOVE "${candidate}")
endfunction()

if(CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
  crt_filter_windows_builtins("${INPUT}" "${OUTPUT}" "${AR}")
endif()
