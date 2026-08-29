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
#endif

#endif
