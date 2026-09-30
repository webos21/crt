/* CRT-owned LVGL configuration for libcrtui's private LVGL backend.
 *
 * Only the choices crtui cares about are set here; every other option takes
 * LVGL's own default from include/lvgl/config/lv_conf_internal.h. This file is
 * the only "patch" LVGL receives (a configuration file, never source).
 *
 * - Software rendering only: no GPU backend, no OS/thread integration, no
 *   drivers. Rendering goes into a CPU buffer that crtui hands to the CRT
 *   display adapter (crtui_window_render()).
 * - Allocation uses the CRT C library's malloc; string/sprintf use LVGL's own
 *   builtin implementations so no libc formatting quirk can affect widgets.
 * - The default theme is disabled: crtui styles every widget explicitly, so
 *   the look is defined by CRT code, not by an LVGL theme version. */

#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_BUILTIN

#define LV_USE_OS LV_OS_NONE
#define LV_DEF_REFR_PERIOD 16

#define LV_USE_DRAW_SW 1
#define LV_USE_DRAW_VG_LITE 0
#define LV_USE_DRAW_PXP 0
#define LV_USE_DRAW_G2D 0
#define LV_USE_DRAW_DMA2D 0
#define LV_USE_DRAW_OPENGLES 0
#define LV_USE_THORVG 0
#define LV_USE_VECTOR_GRAPHIC 0

#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_SIMPLE 0
#define LV_USE_THEME_MONO 0

#define LV_USE_LOG 0
#define LV_USE_SYSMON 0
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0

/* An LVGL assertion is a programming error in crtui, never a hang. */
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
#define LV_ASSERT_HANDLER __builtin_trap();

#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#endif /* LV_CONF_H */
