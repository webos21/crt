add_custom_target(crt_shell_artifacts
  COMMENT "CRT shell artifacts are not imported yet"
)

add_custom_target(crt_shell_inventory
  COMMAND "${CMAKE_COMMAND}" -E echo "mksh/toybox source inventory is pending. Import Android external/mksh first."
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Reporting CRT shell import inventory status"
)
