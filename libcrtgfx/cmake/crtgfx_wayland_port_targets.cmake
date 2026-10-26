# Linux target backends need these producers, including cross builds. Other
# targets expose the standalone port experiments only by explicit opt-in.
option(CRTGFX_ENABLE_WAYLAND_PORT_TARGETS
  "Expose standalone Wayland/xkbcommon port targets on non-Linux targets" OFF)
if(NOT CRT_TARGET_OS STREQUAL "linux" AND NOT CRTGFX_ENABLE_WAYLAND_PORT_TARGETS)
  return()
endif()

# Wayland core (wayland-scanner host tool + wayland-client), 2026-08-24:
# same "external build, recipe.json is the single source of truth for
# source pinning" shape as Skia just above, but with NO external build-
# orchestration tool at all -- tools/build_wayland.py drives every
# compile/link step directly via tools/crt-cc itself (no Meson/Ninja/
# pkg-config), at the user's own explicit direction, after an initial
# Meson-based version of this file was found to need host tooling this
# project's own sandboxed tooling cannot install (see HISTORY.md's
# 2026-08-24 entries for both the Meson attempt and the direct-compile
# rewrite). Scope is also narrower than originally planned: wayland-
# scanner + wayland-client only (no server/cursor/egl yet) -- see
# libcrtgfx/third_party/wayland/recipe.json's own notes for the full
# "why core-client-only, why not wired into libcrtgfx's own backend"
# reasoning. Native Wayland window/GPU consumers depend on this producer;
# the standalone smoke target also remains an explicit porting entry point.
file(READ "${CMAKE_CURRENT_LIST_DIR}/../third_party/wayland/recipe.json" CRTGFX_WAYLAND_RECIPE_JSON)
string(JSON CRTGFX_WAYLAND_RECIPE_VERSION GET "${CRTGFX_WAYLAND_RECIPE_JSON}" source version)
string(JSON CRTGFX_WAYLAND_RECIPE_REF GET "${CRTGFX_WAYLAND_RECIPE_JSON}" source ref)
string(JSON CRTGFX_WAYLAND_RECIPE_REPOSITORY GET "${CRTGFX_WAYLAND_RECIPE_JSON}" source repository)
string(JSON CRTGFX_WAYLAND_RECIPE_EXPECTED_COMMIT GET "${CRTGFX_WAYLAND_RECIPE_JSON}" source expected_commit)

set(CRTGFX_WAYLAND_VERSION "${CRTGFX_WAYLAND_RECIPE_VERSION}" CACHE STRING "Wayland version label to fetch (default: libcrtgfx/third_party/wayland/recipe.json's own source.version)")
set(CRTGFX_WAYLAND_REF "${CRTGFX_WAYLAND_RECIPE_REF}" CACHE STRING "Explicit Wayland git ref/commit to fetch (default: libcrtgfx/third_party/wayland/recipe.json's own source.ref)")
set(CRTGFX_WAYLAND_REPOSITORY "${CRTGFX_WAYLAND_RECIPE_REPOSITORY}" CACHE STRING "Wayland git repository URL (default: libcrtgfx/third_party/wayland/recipe.json's own source.repository)")
set(CRTGFX_WAYLAND_EXPECTED_COMMIT "${CRTGFX_WAYLAND_RECIPE_EXPECTED_COMMIT}" CACHE STRING "Expected Wayland commit hash after fetch (default: libcrtgfx/third_party/wayland/recipe.json's own source.expected_commit)")
# CRTGFX_WAYLAND_FETCH_ROOT/_ROOT/_BUILD_DIR/_INSTALL_PREFIX themselves
# are defined near the top of this file (alongside CRTGFX_XKBCOMMON_
# INSTALL_PREFIX and CRTGFX_WAYLAND_CLIENT_LIBRARIES), matching that
# variable's own precedent -- see the hoisted block's own comment for why.
# CRTGFX_WAYLAND_PORT_PREFIX is also now defined near the top of this
# file, alongside the other hoisted CRTGFX_WAYLAND_* path variables --
# see that block's own comment.

set(CRTGFX_WAYLAND_FETCH_ARGS)
if(CRTGFX_WAYLAND_EXPECTED_COMMIT)
  list(APPEND CRTGFX_WAYLAND_FETCH_ARGS --expected-commit "${CRTGFX_WAYLAND_EXPECTED_COMMIT}")
endif()

# tools/build_wayland.py (2026-08-24 rewrite) drives every compile/link
# step itself via tools/crt-cc directly -- no Meson/Ninja/pkg-config
# host-tool dependency at all, see that file's own top-of-file docstring.
set(CRTGFX_WAYLAND_BUILD_ARGS)
if(CRTGFX_WAYLAND_HOST_SCANNER)
  list(APPEND CRTGFX_WAYLAND_BUILD_ARGS --host-scanner "${CRTGFX_WAYLAND_HOST_SCANNER}")
endif()
if(CRT_TARGET_OS STREQUAL "windows")
  # tools/build_wayland.py needs this to set CRT_MKSH_EXE/CRT_ROOTFS for
  # crt-cc.cmd -- exactly matching CRTGFX_SKIA_BUILD_ARGS's own --rootfs
  # wiring just above (same rootfs, same reason: only it carries
  # toybox's applet aliases).
  list(APPEND CRTGFX_WAYLAND_BUILD_ARGS --rootfs "${CMAKE_BINARY_DIR}/rootfs")
  # tools/build_wayland.py links wayland-scanner.exe directly via
  # tools/crt-cc (not through regular CMake target_link_libraries(
  # ${CRT_WINDOWS_SYSTEM_LIBS})), so it needs the real Windows SDK lib
  # directory passed in explicitly -- exactly matching libstdc++/
  # CMakeLists.txt's own CRT_LIBCXX_WINDOWS_SDK_LIBPATH derivation from
  # this same ${CRT_WINDOWS_KERNEL32_LIB} cache variable. Confirmed for
  # real (2026-08-24): without this, crt-cc's own Windows link branch
  # falls back to bare `-Wl,kernel32.lib -Wl,synchronization.lib`, which
  # ld.lld cannot resolve on its own ("could not open 'kernel32.lib'").
  if(CRT_WINDOWS_KERNEL32_LIB)
    get_filename_component(CRTGFX_WAYLAND_WINDOWS_SDK_LIBPATH "${CRT_WINDOWS_KERNEL32_LIB}" DIRECTORY)
    list(APPEND CRTGFX_WAYLAND_BUILD_ARGS --windows-sdk-libpath "${CRTGFX_WAYLAND_WINDOWS_SDK_LIBPATH}")
  endif()
endif()

add_custom_target(crtgfx-wayland-configure
  COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
    "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/fetch_wayland.py"
    --dest "${CRTGFX_WAYLAND_ROOT}"
    --repo "${CRTGFX_WAYLAND_REPOSITORY}"
    --version "${CRTGFX_WAYLAND_VERSION}"
    --ref "${CRTGFX_WAYLAND_REF}"
    ${CRTGFX_WAYLAND_FETCH_ARGS}
  COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
    "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/build_wayland.py"
    --root "${CMAKE_SOURCE_DIR}"
    --source "${CRTGFX_WAYLAND_ROOT}"
    --build-dir "${CRTGFX_WAYLAND_BUILD_DIR}"
    --install-prefix "${CRTGFX_WAYLAND_INSTALL_PREFIX}"
    --sysroot "${CRT_SYSROOT}"
    --port-prefix "${CRTGFX_WAYLAND_PORT_PREFIX}"
    --target-os "${CRT_TARGET_OS}"
    --target-arch "${CRT_EFFECTIVE_ARCH_FOR_FLAGS}"
    ${CRTGFX_WAYLAND_BUILD_ARGS}
    --configure-only
  DEPENDS port-build-expat port-build-libffi
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Fetching Wayland and building the wayland-scanner host tool with the CRT toolchain"
  USES_TERMINAL
)

add_custom_target(crtgfx-wayland-build
  COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
    "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/build_wayland.py"
    --root "${CMAKE_SOURCE_DIR}"
    --source "${CRTGFX_WAYLAND_ROOT}"
    --build-dir "${CRTGFX_WAYLAND_BUILD_DIR}"
    --install-prefix "${CRTGFX_WAYLAND_INSTALL_PREFIX}"
    --sysroot "${CRT_SYSROOT}"
    --port-prefix "${CRTGFX_WAYLAND_PORT_PREFIX}"
    --target-os "${CRT_TARGET_OS}"
    --target-arch "${CRT_EFFECTIVE_ARCH_FOR_FLAGS}"
    ${CRTGFX_WAYLAND_BUILD_ARGS}
  DEPENDS crtgfx-wayland-configure
  BYPRODUCTS "${CRTGFX_WAYLAND_XDG_SHELL_LIBRARY}"
    "${CRTGFX_WAYLAND_INSTALL_PREFIX}/include/xdg-shell-client-protocol.h"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Building and installing Wayland core (wayland-scanner + wayland-client) with the CRT toolchain"
  USES_TERMINAL
)

# crtgfx-wayland-smoke: self-sufficient like crtgfx-skia-smoke/
# crt-libcxx-smoke -- `cmake --build <dir> --target crtgfx-wayland-smoke`
# alone drives a dedicated shadow build directory (sysroot/rootfs, then
# port-build-expat/port-build-libffi, then crtgfx-wayland-build) and
# finally compiles+runs libcrtgfx/tests/wayland_client_smoke.c directly
# against the freshly installed libwayland-client, entirely independent
# of *this* (the calling) build directory's own cache/state. See
# tools/test_crtgfx_wayland_smoke.py's own docstring.
set(CRTGFX_WAYLAND_SMOKE_ARGS)
if(CRT_TARGET_OS STREQUAL "windows" AND CRTGFX_WAYLAND_WINDOWS_SDK_LIBPATH)
  # Same reason as CRTGFX_WAYLAND_BUILD_ARGS's own --windows-sdk-libpath
  # just above: tools/test_crtgfx_wayland_smoke.py also links an
  # executable (wayland_client_smoke.exe) directly via tools/crt-cc.
  list(APPEND CRTGFX_WAYLAND_SMOKE_ARGS --windows-sdk-libpath "${CRTGFX_WAYLAND_WINDOWS_SDK_LIBPATH}")
endif()

add_custom_target(crtgfx-wayland-smoke
  COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
    "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/test_crtgfx_wayland_smoke.py"
    --root "${CMAKE_SOURCE_DIR}"
    --build-dir "${CMAKE_BINARY_DIR}/crtgfx-wayland-smoke"
    --cmake-c-compiler "${CMAKE_C_COMPILER}"
    --cmake-cxx-compiler "${CMAKE_CXX_COMPILER}"
    --target-os "${CRT_TARGET_OS}"
    --target-arch "${CRT_TARGET_ARCH}"
    ${CRTGFX_WAYLAND_SMOKE_ARGS}
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Building and running wayland_client_smoke in a dedicated build directory (this directory's own cache untouched)"
  USES_TERMINAL
)

# libxkbcommon core (xkb_context/xkb_keymap/xkb_state), 2026-08-25: same
# "direct-compile, no Meson/Ninja/pkg-config" shape as Wayland core just
# above -- see libcrtgfx/third_party/xkbcommon/recipe.json's own notes
# for the one real difference (a pre-generated, committed bison output
# instead of a bison host-tool port) and the scope decision (core keymap/
# state API only, no xkbregistry, no Compose). Unlike Wayland, this DOES
# feed a real libcrtgfx backend directly (src/arch/linux/window_wayland.c
# -- Phase 3 of the "notepad-capability" plan, real keyboard input), so
# there is no separate crtgfx-xkbcommon-smoke target the way crtgfx-
# wayland-smoke exists standalone: the real wl_seat/wl_keyboard wiring
# itself, tested via a real smoke test elsewhere in this file, is what
# actually exercises this build end to end.
file(READ "${CMAKE_CURRENT_LIST_DIR}/../third_party/xkbcommon/recipe.json" CRTGFX_XKBCOMMON_RECIPE_JSON)
string(JSON CRTGFX_XKBCOMMON_RECIPE_VERSION GET "${CRTGFX_XKBCOMMON_RECIPE_JSON}" source version)
string(JSON CRTGFX_XKBCOMMON_RECIPE_REF GET "${CRTGFX_XKBCOMMON_RECIPE_JSON}" source ref)
string(JSON CRTGFX_XKBCOMMON_RECIPE_REPOSITORY GET "${CRTGFX_XKBCOMMON_RECIPE_JSON}" source repository)
string(JSON CRTGFX_XKBCOMMON_RECIPE_EXPECTED_COMMIT GET "${CRTGFX_XKBCOMMON_RECIPE_JSON}" source expected_commit)

set(CRTGFX_XKBCOMMON_VERSION "${CRTGFX_XKBCOMMON_RECIPE_VERSION}" CACHE STRING "libxkbcommon version label to fetch (default: libcrtgfx/third_party/xkbcommon/recipe.json's own source.version)")
set(CRTGFX_XKBCOMMON_REF "${CRTGFX_XKBCOMMON_RECIPE_REF}" CACHE STRING "Explicit libxkbcommon git ref/commit to fetch (default: libcrtgfx/third_party/xkbcommon/recipe.json's own source.ref)")
set(CRTGFX_XKBCOMMON_REPOSITORY "${CRTGFX_XKBCOMMON_RECIPE_REPOSITORY}" CACHE STRING "libxkbcommon git repository URL (default: libcrtgfx/third_party/xkbcommon/recipe.json's own source.repository)")
set(CRTGFX_XKBCOMMON_EXPECTED_COMMIT "${CRTGFX_XKBCOMMON_RECIPE_EXPECTED_COMMIT}" CACHE STRING "Expected libxkbcommon commit hash after fetch (default: libcrtgfx/third_party/xkbcommon/recipe.json's own source.expected_commit)")
# CRTGFX_XKBCOMMON_FETCH_ROOT/_ROOT/_BUILD_DIR/_INSTALL_PREFIX themselves
# are defined near the top of this file (alongside CRTGFX_SKIA_INSTALL_
# PREFIX), matching that variable's own precedent: crtgfx_backend_
# objects's own target_include_directories/target_link_libraries calls
# (further down this file) need CRTGFX_XKBCOMMON_INSTALL_PREFIX
# regardless of whether Python3_Interpreter_FOUND is true, the same
# "path math works even without the tool that populates it" reasoning
# CRTGFX_SKIA_INSTALL_PREFIX already established.

set(CRTGFX_XKBCOMMON_FETCH_ARGS)
if(CRTGFX_XKBCOMMON_EXPECTED_COMMIT)
  list(APPEND CRTGFX_XKBCOMMON_FETCH_ARGS --expected-commit "${CRTGFX_XKBCOMMON_EXPECTED_COMMIT}")
endif()

set(CRTGFX_XKBCOMMON_BUILD_ARGS)
if(CRT_TARGET_OS STREQUAL "windows")
  list(APPEND CRTGFX_XKBCOMMON_BUILD_ARGS --rootfs "${CMAKE_BINARY_DIR}/rootfs")
  if(CRT_WINDOWS_KERNEL32_LIB)
    get_filename_component(CRTGFX_XKBCOMMON_WINDOWS_SDK_LIBPATH "${CRT_WINDOWS_KERNEL32_LIB}" DIRECTORY)
    list(APPEND CRTGFX_XKBCOMMON_BUILD_ARGS --windows-sdk-libpath "${CRTGFX_XKBCOMMON_WINDOWS_SDK_LIBPATH}")
  endif()
endif()

add_custom_target(crtgfx-xkbcommon-configure
  COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
    "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/fetch_xkbcommon.py"
    --dest "${CRTGFX_XKBCOMMON_ROOT}"
    --repo "${CRTGFX_XKBCOMMON_REPOSITORY}"
    --version "${CRTGFX_XKBCOMMON_VERSION}"
    --ref "${CRTGFX_XKBCOMMON_REF}"
    ${CRTGFX_XKBCOMMON_FETCH_ARGS}
  COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
    "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/build_xkbcommon.py"
    --root "${CMAKE_SOURCE_DIR}"
    --source "${CRTGFX_XKBCOMMON_ROOT}"
    --build-dir "${CRTGFX_XKBCOMMON_BUILD_DIR}"
    --install-prefix "${CRTGFX_XKBCOMMON_INSTALL_PREFIX}"
    --sysroot "${CRT_SYSROOT}"
    --target-os "${CRT_TARGET_OS}"
    --target-arch "${CRT_EFFECTIVE_ARCH_FOR_FLAGS}"
    ${CRTGFX_XKBCOMMON_BUILD_ARGS}
    --configure-only
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Fetching libxkbcommon and writing its config.h with the CRT toolchain"
  USES_TERMINAL
)

# OUTPUT-based (not a plain add_custom_target COMMAND) specifically so
# ninja has a real rule that produces this *exact* file path -- required
# because CRTGFX_XKBCOMMON_LIBRARIES (just above) now unconditionally
# feeds this literal path into target_link_libraries(crtgfx/crtgfx_shared
# PRIVATE ...). A raw file path passed to target_link_libraries becomes a
# real ninja build-graph *input*, not just an order-only target
# dependency -- if nothing in the graph claims to produce that path (a
# plain add_custom_target's COMMAND has no OUTPUT, so it doesn't count),
# ninja refuses to even start the build: real GitHub Actions CI failure
# (2026-08-25, aarch64), from a from-scratch checkout: "ninja: error:
# 'external/xkbcommon/install/lib/libxkbcommon.a', needed by
# 'lib/libcrtgfx.so', missing and no known rule to make it". This is
# exactly the ninja limitation CRTGFX_SKIA_LIBRARIES's own EXISTS-gating
# (above) was *also* silently working around by simply never listing a
# not-yet-existing file as a link input in the first place -- xkbcommon
# can't use that same trick (see CRTGFX_XKBCOMMON_LIBRARIES's own
# comment: it's a non-optional, always-built dependency, and an
# EXISTS-gated value evaluated once at configure time can never become
# true later within one single-pass `cmake --workflow` invocation). An
# OUTPUT-based add_custom_command is the correct, general fix instead:
# ninja then knows precisely how to produce this path on demand,
# regardless of configure-vs-build-time ordering.
add_custom_command(
  OUTPUT "${CRTGFX_XKBCOMMON_INSTALL_PREFIX}/lib/libxkbcommon.a"
  COMMAND "${CMAKE_COMMAND}" -E env PYTHONUNBUFFERED=1
    "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/build_xkbcommon.py"
    --root "${CMAKE_SOURCE_DIR}"
    --source "${CRTGFX_XKBCOMMON_ROOT}"
    --build-dir "${CRTGFX_XKBCOMMON_BUILD_DIR}"
    --install-prefix "${CRTGFX_XKBCOMMON_INSTALL_PREFIX}"
    --sysroot "${CRT_SYSROOT}"
    --target-os "${CRT_TARGET_OS}"
    --target-arch "${CRT_EFFECTIVE_ARCH_FOR_FLAGS}"
    ${CRTGFX_XKBCOMMON_BUILD_ARGS}
  DEPENDS crtgfx-xkbcommon-configure
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Building and installing libxkbcommon's core keymap/state API with the CRT toolchain"
  USES_TERMINAL
)
add_custom_target(crtgfx-xkbcommon-build
  DEPENDS "${CRTGFX_XKBCOMMON_INSTALL_PREFIX}/lib/libxkbcommon.a"
)
