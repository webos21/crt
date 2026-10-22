# Public, offline configuration preflights. Never invoke CMake recursively from
# a Ninja custom target or silently change the user's feature selections.
# These are opt-in; existing reduced-feature diagnostics keep their behavior.
if(Python3_Interpreter_FOUND)
  foreach(crt_config_stage IN ITEMS c libcxx gfx-simple gfx-media ui web)
    add_custom_target(crt-${crt_config_stage}-config
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/crt_stage_config.py"
        --build-dir "${CMAKE_BINARY_DIR}" --stage "${crt_config_stage}"
        --target-os "${CRT_TARGET_OS}" --target-arch "${CRT_EFFECTIVE_ARCH_FOR_FLAGS}"
        --c-compiler "${CMAKE_C_COMPILER}" --cxx-compiler "${CMAKE_CXX_COMPILER}"
        --make-program "${CMAKE_MAKE_PROGRAM}"
      COMMENT "Checking the complete ${crt_config_stage} stage configuration (no build or fetch)"
      VERBATIM)
  endforeach()
endif()
