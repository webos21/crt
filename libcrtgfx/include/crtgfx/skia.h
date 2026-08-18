#pragma once

#include "crtgfx/window.h"

#ifdef __cplusplus

#if defined(__has_include)
#if __has_include("include/core/SkSurface.h")
#define CRTGFX_HAS_SKIA_HEADERS 1
#else
#define CRTGFX_HAS_SKIA_HEADERS 0
#endif
#else
#define CRTGFX_HAS_SKIA_HEADERS 1
#endif

#if CRTGFX_HAS_SKIA_HEADERS
#include "include/core/SkRefCnt.h"
#include "include/core/SkSurface.h"

sk_sp<SkSurface> crtgfx_skia_make_raster_surface(const crtgfx_framebuffer* framebuffer);
#endif

#endif
