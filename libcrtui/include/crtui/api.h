#pragma once

/* Symbol visibility for crtui's public API (Tranche 3). libcrtui is built with
 * hidden visibility by default, and only the functions declared with CRTUI_API
 * are exported from the shared library. That is what keeps LVGL -- compiled into
 * libcrtui, never installed as a header or a library of its own -- a private
 * dependency: none of its symbols (lv_*, or its bundled helpers) is visible to a
 * consumer of libcrtui.so / libcrtui.dylib / libcrtui.dll.
 *
 * CRTUI_BUILD_LIBRARY is defined only while building libcrtui itself; consumers
 * see an empty CRTUI_API (a Windows import library needs no dllimport). */

#if defined(CRTUI_BUILD_LIBRARY)
#if defined(_WIN32) || defined(__MINGW32__) || defined(CRT_TARGET_OS_WINDOWS)
#define CRTUI_API __declspec(dllexport)
#else
#define CRTUI_API __attribute__((visibility("default")))
#endif
#else
#define CRTUI_API
#endif
