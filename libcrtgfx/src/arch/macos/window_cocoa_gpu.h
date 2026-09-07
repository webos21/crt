#pragma once

#include "wayland_weston_internal.h"

/* Borrow the GPU window's CAMetalLayer; software windows are rejected. */
int crtgfx_cocoa_get_metal_layer(const crtgfx_weston_toplevel* toplevel, void** out_layer);
