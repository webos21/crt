# add_crt_test()/add_crt_cxx_test() are defined in the top-level
# CMakeLists.txt (not here) -- this directory and libc/tests both need
# them, and a function defined inside one add_subdirectory()'d file is
# not visible to a sibling add_subdirectory() tree, only to root scope
# and whatever root processes afterward. See the top-level file's own
# comment at their definition site.
#
# imported_libcxx_test.cc/imported_libcxx_string_sdk.cc/
# imported_libcxx_string_abi_test.cc are not built through either helper
# here -- they are compiled and run directly by tools/test_libcxx_
# runtime.py, invoked from this project's own crt-libcxx-smoke target
# (libstdc++/CMakeLists.txt).

# cxx_runtime_test is a plain C translation unit (uses add_crt_test, not
# add_crt_cxx_test) that links against the real cxx runtime to exercise
# the C/C++ ABI boundary -- kept alongside the other libstdc++ tests
# because of what it tests, not how it's compiled.
add_crt_test(cxx_runtime_test cxx_runtime_test.c)
target_link_libraries(cxx_runtime_test PRIVATE cxx)
add_dependencies(cxx_runtime_test cxx)
set_tests_properties(cxx_runtime_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "cxx_runtime_test: ok"
)

add_crt_cxx_test(cxx_frontend_test cxx_frontend_test.cc)
set_tests_properties(cxx_frontend_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "cxx_frontend_test: ok"
)

add_crt_cxx_test(cxx_allocation_test cxx_allocation_test.cc)
set_tests_properties(cxx_allocation_test_runs PROPERTIES
  PASS_REGULAR_EXPRESSION "cxx_allocation_test: ok"
)

# This is the last directory the default host workflow's test tree
# processes (add_subdirectory(libc/tests), then add_subdirectory(
# libstdc++/tests), from the top-level CMakeLists.txt) -- libcrtgfx/
# libcrtjs/libcrtmedia's own tests are registered earlier via their own
# add_subdirectory() calls further up that same file, so this total is
# the full CRT_REGISTERED_TESTS count across the whole configure.
get_property(CRT_REGISTERED_TESTS GLOBAL PROPERTY CRT_REGISTERED_TESTS)
list(LENGTH CRT_REGISTERED_TESTS CRT_REGISTERED_TEST_COUNT)
message(STATUS "CRT registered tests for ${CRT_TARGET_OS}/${CRT_TARGET_ARCH}: ${CRT_REGISTERED_TEST_COUNT}")
