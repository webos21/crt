#pragma once

/* Internal HWND accessor; host SDK types stay inside the Windows PAL. */
#include "wayland_weston_internal.h"

/* Returns 1 only for a live GPU_PRESENTATION window, otherwise 0.
 * Software windows already own a D3D11 swapchain and are rejected. */
int crtgfx_win32_get_hwnd(const crtgfx_weston_toplevel* toplevel, void** out_hwnd);
