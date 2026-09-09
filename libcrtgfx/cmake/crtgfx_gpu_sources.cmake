# Shared, data-only source inventory for the in-tree and isolated
# 04-gfx-media builds. Mirrors crtgfx_window_sources.cmake's own shape and
# rationale exactly: only the file lists live here, never target
# construction or link policy. Unlike the window sources, GPU backend
# inclusion is also gated by real host-configure-time detection (Linux's
# own find_library(vulkan) -- a real optional dependency, not every host
# has a Vulkan Loader -dev package installed) -- that conditional stays in
# each consuming CMakeLists.txt, matching how CRTGFX_WINDOW_NATIVE_WAYLAND_
# SOURCES's own gating stays local to crtgfx_window_sources.cmake's callers
# too. Each per-OS list is a single file today; kept as a list (not a
# scalar) so a future backend split follows the exact same pattern the
# window sources already established.
set(CRTGFX_GPU_COMMON_SOURCES
  src/gpu.c
)
set(CRTGFX_GPU_BACKEND_WINDOWS_SOURCES
  src/arch/windows/gpu_win32.c
)
set(CRTGFX_GPU_BACKEND_MACOS_SOURCES
  src/arch/macos/gpu_metal.c
)
set(CRTGFX_GPU_BACKEND_LINUX_SOURCES
  src/arch/linux/gpu_vulkan.c
)
