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

#if defined(CRTGFX_HAVE_VULKAN) || defined(CRTGFX_HAVE_D3D12) || defined(CRTGFX_HAVE_METAL)
#include "crtgfx/gpu.h"
#include "include/gpu/ganesh/GrDirectContext.h"

// Real Ganesh GPU offscreen vertical slice (2026-09-03, TODO.md's "Enable
// Skia GPU rendering" step -- Linux/Vulkan first, Windows/D3D12 the same
// week, macOS/Metal the following week). Deliberately separate from, and
// not implying any stability of, the public crtgfx_gpu_surface contract
// (crtgfx/gpu.h) -- crtgfx_gpu_surface_create() itself still, correctly,
// stays CRTGFX_ERROR_UNSUPPORTED everywhere (no host can yet actually
// present a Ganesh-drawn surface to a real on-screen window). These two
// functions exist purely to prove real Ganesh *rendering* correctness
// offscreen -- see tests/skia_gpu_offscreen_smoke.cc, this vertical
// slice's own real, cross-platform coverage (the same source file, same
// two function names, calls whichever real per-OS implementation below
// CMakeLists.txt actually built).
//
// Only declared when CRTGFX_HAVE_VULKAN, CRTGFX_HAVE_D3D12, or CRTGFX_
// HAVE_METAL is defined (CMakeLists.txt only defines one of these when a
// real backend was actually found/is actually available at configure
// time -- see src/arch/linux/gpu_vulkan.c's, src/arch/windows/gpu_win32.c's,
// and src/arch/macos/gpu_metal.c's own top comments), matching gpu.c's
// own real per-host dispatch. Exactly one of the three real per-OS
// implementations (skia_bridge.cc) is ever compiled into a given build --
// all three share these same two declarations, never more than one
// defined in the same translation unit.
//
// Builds a real GrDirectContext directly from `device`'s own real per-OS
// handles (Vulkan: VkInstance/VkPhysicalDevice/VkDevice/VkQueue/queue
// family; D3D12: ID3D12Device/ID3D12CommandQueue/IDXGIAdapter1 -- see
// src/gpu_internal.h for the real field layout crtgfx_gpu_device_create()
// itself already populated, whichever backend is actually live). `device`
// must outlive the returned context. Deliberately not cached inside
// crtgfx_gpu_device itself (that would need a C-struct-to-C++-object
// ownership bridge this vertical slice does not need yet) -- callers
// construct one context per real use, matching this slice's own
// offscreen-only, non-production scope; a real production caching story
// is exactly what a later, real crtgfx_gpu_surface_create() live-
// presentation implementation will need to add.
sk_sp<GrDirectContext> crtgfx_skia_make_gpu_context(const crtgfx_gpu_device* device);

// Builds a real, GPU-backed (not wrapping any caller-supplied backend
// resource -- Ganesh's own GrResourceProvider allocates and owns the real
// backing GPU memory/resource internally, via each real per-OS
// implementation's own memory allocator) offscreen SkSurface via
// SkSurfaces::RenderTarget() (confirmed backend-agnostic -- the same real
// call works identically for both Vulkan and D3D12, no per-backend code
// needed at this call site at all), BGRA8888-premultiplied to match
// crtgfx_skia_make_raster_surface()'s own established pixel format.
// Returns null for a null context or a zero width/height, matching
// crtgfx_skia_make_raster_surface()'s own validation convention.
sk_sp<SkSurface> crtgfx_skia_make_gpu_offscreen_surface(
    GrDirectContext* context, uint32_t width, uint32_t height);

// Wires the offscreen Ganesh pipeline above onto a live crtgfx_gpu_surface's
// own acquired image (2026-09-07, TODO.md's "Finish live GPU presentation
// everywhere" -- the last remaining piece the Windows/Linux/macOS resize
// work, same day, explicitly deferred). Unlike crtgfx_skia_make_gpu_
// offscreen_surface() above (Ganesh allocates and owns its own backing
// image), this *wraps* the real swapchain/layer image crtgfx_gpu_surface_
// acquire() (crtgfx/gpu.h) already produced -- SkSurfaces::
// WrapBackendRenderTarget(), a render target rather than a sampled
// texture: none of the three real swapchains/layers this project creates
// request sampling usage. `surface` must be between a successful crtgfx_
// gpu_surface_acquire() and crtgfx_skia_gpu_surface_present() (below) --
// never crtgfx_gpu_surface_clear()/crtgfx_gpu_surface_present(), the
// separate, non-Ganesh solid-color path this contract's own vertical
// slice already proved and still supports unchanged (calling either of
// those two directly on a Ganesh-wrapped frame is real, rejected misuse,
// CRTGFX_ERROR_HOST, matching every other out-of-order guard already in
// that contract). Returns null for a null context/surface, an unacquired
// surface, or a surface already wrapped this frame.
sk_sp<SkSurface> crtgfx_skia_wrap_gpu_surface(GrDirectContext* context, crtgfx_gpu_surface* surface);

// Flushes and submits every real Ganesh draw recorded into the SkSurface
// crtgfx_skia_wrap_gpu_surface() returned, performs whatever real, per-
// backend work is needed to bring the image back into a presentable
// state (Vulkan: an explicit VK_IMAGE_LAYOUT_PRESENT_SRC_KHR transition
// requested as part of the same flush, which also signals the exact
// semaphore crtgfx_gpu_surface_present() already waits on; D3D12: one
// small extra resource-barrier command list submitted after Ganesh's own,
// on the same command queue; Metal: no transition needed at all, MTLTexture
// has no layout/state concept, just a fresh presenting command buffer on
// the same command queue), then defers to the existing, unchanged crtgfx_
// gpu_surface_present() (crtgfx/gpu.h) for the actual real present call.
// Must be called exactly once per crtgfx_skia_wrap_gpu_surface() call, in
// place of calling crtgfx_gpu_surface_present() directly. Returns CRTGFX_
// ERROR_INVALID_ARGUMENT for a null argument, CRTGFX_ERROR_HOST if
// `gpu_surface` was not wrapped this frame or any real step fails.
crtgfx_result crtgfx_skia_gpu_surface_present(
    GrDirectContext* context, SkSurface* surface, crtgfx_gpu_surface* gpu_surface);
#endif

#endif

#endif
