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
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkSurface.h"
#include "include/core/SkTypeface.h"

sk_sp<SkSurface> crtgfx_skia_make_raster_surface(const crtgfx_framebuffer* framebuffer);

// Resolves this project's own bundled default typeface out of a font
// manager already created over libcrtgfx/assets/fonts/ (normally via
// SkFontMgr_New_Custom_Directory(CRT_SKIA_FONTS_DIR)). Tries, in order:
// "Pretendard GOV" (the project-wide default as of 2026-08-29, see that
// directory's README.md), then "DejaVu Sans Mono" (kept for any consumer
// that specifically wants a monospace fallback), then whatever
// legacyMakeTypeface(nullptr, ...) returns as a last resort so this never
// regresses to "no font at all" if either bundled family is ever removed.
// Centralized here instead of duplicated per call site (crtgfx_skia_
// raster_smoke.cc, window_keyboard_interactive_test.cc) specifically
// because matchFamilyStyle() needs an exact family-name string --
// SkFontMgr_Custom::onMatchFamilyStyle() has no null-means-"any"/default
// fallback (confirmed by reading src/ports/SkFontMgr_custom.cpp), and
// onLegacyMakeTypeface(nullptr, ...)'s own fDefaultFamily fallback is
// whichever family the directory scan happened to register first --
// alphabetical on every host observed so far, which would silently keep
// resolving to "DejaVu Sans Mono" now that both bundled families sort
// with DejaVu first. font_mgr may be null (mirrors SkFontMgr_New_Custom_
// Directory()'s own possible-null return); returns null if every attempt
// fails.
sk_sp<SkTypeface> crtgfx_skia_default_typeface(SkFontMgr* font_mgr, const SkFontStyle& style);

#if defined(CRTGFX_HAVE_VULKAN)
#include "crtgfx/gpu.h"
#include "include/gpu/ganesh/GrDirectContext.h"

// Real Ganesh/Vulkan offscreen vertical slice (2026-09-03, TODO.md's
// "Enable Skia GPU rendering" step). Deliberately separate from, and not
// implying any stability of, the public crtgfx_gpu_surface contract
// (crtgfx/gpu.h) -- crtgfx_gpu_surface_create() itself still, correctly,
// stays CRTGFX_ERROR_UNSUPPORTED everywhere (no host can yet actually
// present a Ganesh-drawn surface to a real on-screen window). These two
// functions exist purely to prove real Ganesh/Vulkan *rendering*
// correctness offscreen -- see tests/skia_gpu_offscreen_smoke.cc, this
// vertical slice's own real coverage.
//
// Only declared when CRTGFX_HAVE_VULKAN is defined (CMakeLists.txt only
// defines it when a real libvulkan was found at configure time -- see
// src/arch/linux/gpu_vulkan.c's own top comment) -- Windows/macOS builds
// never see these declarations at all today, matching gpu.c's own real
// per-host dispatch.
//
// Builds a real GrDirectContext directly from `device`'s own real Vulkan
// handles (VkInstance/VkPhysicalDevice/VkDevice/VkQueue/queue family --
// see src/gpu_internal.h for the real field layout crtgfx_gpu_device_
// create() itself already populated). `device` must outlive the returned
// context. Deliberately not cached inside crtgfx_gpu_device itself (that
// would need a C-struct-to-C++-object ownership bridge this vertical
// slice does not need yet) -- callers construct one context per real use,
// matching this slice's own offscreen-only, non-production scope; a real
// production caching story is exactly what a later, real crtgfx_gpu_
// surface_create() live-presentation implementation will need to add.
sk_sp<GrDirectContext> crtgfx_skia_make_gpu_context(const crtgfx_gpu_device* device);

// Builds a real, GPU-backed (not wrapping any caller-supplied VkImage --
// Ganesh's own GrResourceProvider allocates and owns the real backing
// VkImage/VkDeviceMemory internally) offscreen SkSurface via SkSurfaces::
// RenderTarget(), BGRA8888-premultiplied to match crtgfx_skia_make_raster_
// surface()'s own established pixel format. Returns null for a null
// context or a zero width/height, matching crtgfx_skia_make_raster_
// surface()'s own validation convention.
sk_sp<SkSurface> crtgfx_skia_make_gpu_offscreen_surface(
    GrDirectContext* context, uint32_t width, uint32_t height);
#endif

#endif

#endif
