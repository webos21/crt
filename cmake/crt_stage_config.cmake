# Public, offline configuration preflights. Never invoke CMake recursively from
# a Ninja custom target or silently change the user's feature selections.
# These are opt-in; existing reduced-feature diagnostics keep their behavior.
if(Python3_Interpreter_FOUND)
  # Public command names are separate from the existing preflight/cache stage
  # IDs and SDK directory names.
  foreach(crt_config_pair IN ITEMS c:c cxx:libcxx gfx:gfx-simple media:gfx-media ui:ui web:web)
    string(REPLACE ":" ";" crt_config_parts "${crt_config_pair}")
    list(GET crt_config_parts 0 crt_config_name)
    list(GET crt_config_parts 1 crt_config_stage)
    add_custom_target(crt-${crt_config_name}-config
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/crt_stage_config.py"
        --build-dir "${CMAKE_BINARY_DIR}" --stage "${crt_config_stage}"
        --target-os "${CRT_TARGET_OS}" --target-arch "${CRT_EFFECTIVE_ARCH_FOR_FLAGS}"
        --c-compiler "${CMAKE_C_COMPILER}" --cxx-compiler "${CMAKE_CXX_COMPILER}"
        --make-program "${CMAKE_MAKE_PROGRAM}"
      COMMENT "Checking the complete ${crt_config_stage} stage configuration (no build or fetch)"
      VERBATIM)
  endforeach()
endif()
