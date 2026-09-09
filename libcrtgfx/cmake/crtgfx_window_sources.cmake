# Shared, data-only source inventory for the in-tree and isolated
# 03-gfx-simple builds. Target construction and link policy deliberately stay
# in their respective CMakeLists.txt files.
set(CRTGFX_WINDOW_COMMON_SOURCES
  src/window.c
  src/wayland_weston.c
)
set(CRTGFX_WINDOW_BACKEND_WINDOWS_SOURCES
  src/arch/windows/window_win32.c
)
set(CRTGFX_WINDOW_BACKEND_MACOS_SOURCES
  src/arch/macos/window_cocoa.c
)
set(CRTGFX_WINDOW_BACKEND_LINUX_SOURCES
  src/arch/linux/window_wayland.c
)
set(CRTGFX_WINDOW_NATIVE_WAYLAND_SOURCES
  src/arch/linux/window_wayland_native.c
)

# Project-owned source port required by the Linux Simple Graphics backend.
# Windows and macOS use only OS-owned libraries/frameworks at this stage.
set(CRTGFX_SIMPLE_GRAPHICS_LINUX_PORTS xkbcommon)
