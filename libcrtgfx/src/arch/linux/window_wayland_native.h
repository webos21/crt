#pragma once

/* Shared glue between the two independent Linux Wayland window backends
 * (2026-09-07, "Finish live GPU presentation everywhere" -- Linux lands
 * first; see docs/libcrtgfx_wayland_plan.md's own dual-backend section for
 * the full design and CRTGFX_WINDOW_GPU_PRESENTATION's own doc comment in
 * crtgfx/window.h for why a window cannot move between them after
 * creation):
 *
 *  - window_wayland.c: the original, frozen, hand-rolled wire-protocol
 *    backend (legacy) -- every window created without CRTGFX_WINDOW_GPU_
 *    PRESENTATION.
 *  - window_wayland_native.c: the new backend (native) -- real
 *    libwayland-client + xdg-shell, owns a Vulkan-presenting window's
 *    entire lifecycle (connection, surface, input) from creation. Every
 *    window created with CRTGFX_WINDOW_GPU_PRESENTATION.
 *
 * Both backends' own private `struct crtgfx_host_window` definitions are
 * opaque outside their own .c file (crtgfx_host_window itself is only
 * ever forward-declared in wayland_weston_internal.h), but window_
 * wayland.c's own five crtgfx_host_window_*() entry points (crtgfx/
 * window.h's real public API surface, wired through wayland_weston.c) are
 * the single place every window of *either* backend actually flows
 * through -- there is no per-backend variant of these five functions
 * anywhere above this layer. window_wayland.c dispatches each call by
 * checking the backend tag below, a small, deliberate exception to "never
 * touch the legacy struct": every real per-backend field stays exactly as
 * it always was, but each struct's own storage now begins with this one
 * shared uint32_t tag (added to struct crtgfx_host_window in window_
 * wayland.c; struct crtgfx_native_wl_window in window_wayland_native.c
 * already had it from the start) precisely so a single dispatcher can
 * safely read *which* backend owns an opaque crtgfx_host_window* before
 * it knows anything else about it.
 *
 * This is well-defined C, not a strict-aliasing gamble: C11 6.7.2.1p15
 * guarantees a pointer to a struct object, suitably converted, points to
 * its own initial member -- true regardless of the two concrete struct
 * types otherwise having nothing in common (the "common initial sequence"
 * rule most people know is the *union*-specific special case of this same
 * general guarantee; a plain read through the first-member's own type is
 * valid independent of that). The same technique is how real POSIX
 * `struct sockaddr`/`sockaddr_in`/`sockaddr_in6` (their shared leading
 * `sa_family_t`) and Win32 OVERLAPPED-family APIs already solve the exact
 * same "which concrete type is this opaque pointer really" problem. */

/* Not crtgfx/window.h directly -- this header's own function signatures
 * below need the real crtgfx_weston_toplevel struct definition (crtgfx_
 * host_window_create()'s own second argument), which only wayland_weston_
 * internal.h actually provides; that header already includes crtgfx/
 * window.h itself, so nothing is lost by including this one instead. */
#include "wayland_weston_internal.h"

#include <stdint.h>

#define CRTGFX_WL_BACKEND_TAG_LEGACY 0x4c454741u /* 'LEGA', arbitrary but stable */
#define CRTGFX_WL_BACKEND_TAG_NATIVE 0x4e415449u /* 'NATI', arbitrary but stable */

/* Safe on a NULL host (returns 0, matching neither real tag) -- every
 * caller below already null-checks host itself, this is just cheap
 * insurance against a stray direct call. */
static inline uint32_t crtgfx_wl_backend_tag(const void* host) {
  if (host == 0) {
    return 0;
  }
  return *(const uint32_t*)host;
}

/* window_wayland_native.c's own real implementation of the five shared
 * crtgfx_host_window_*() entry points, called from window_wayland.c's own
 * versions once the backend tag (or, for _create(), the CRTGFX_WINDOW_
 * GPU_PRESENTATION desc flag) says this call belongs to the native
 * backend. Same names/signatures as wayland_weston_internal.h's own
 * crtgfx_host_window_*() family, `crtgfx_native_wl_` prefixed instead of
 * `crtgfx_host_window_` so both this header and window_wayland.c can see
 * both a given call's dispatcher and its real per-backend implementation
 * without a name collision. */
int crtgfx_native_wl_window_create(const crtgfx_window_desc* desc, crtgfx_weston_toplevel* toplevel);
void crtgfx_native_wl_window_destroy(void* host);
int crtgfx_native_wl_window_show(void* host);
int crtgfx_native_wl_window_get_size(void* host, uint32_t* out_width, uint32_t* out_height);
/* Software presentation is a real, later Phase 3 addition for this
 * backend (see crtgfx/window.h's own CRTGFX_WINDOW_GPU_PRESENTATION
 * comment) -- this always returns CRTGFX_ERROR_UNSUPPORTED today, matching
 * every other "not implemented on this backend yet" case in this project,
 * never a crash/hang. Kept as a real, present symbol (not left for
 * window_wayland.c to special-case away) so window_wayland.c's own
 * dispatcher stays a uniform "tag says native -> call the native
 * function" shape across all five entry points. */
int crtgfx_native_wl_window_present_software(
    void* host, const void* pixels, uint32_t width, uint32_t height, uint32_t stride,
    const crtgfx_damage_rect* damage_rects, uint32_t damage_rect_count);
/* Global, like crtgfx_host_window_dispatch() itself -- pumps the native
 * backend's own one shared connection (if one is currently open; a real,
 * cheap no-op returning CRTGFX_OK immediately if not, so window_wayland.c
 * can always call this unconditionally without first checking whether any
 * native-backend window exists). */
int crtgfx_native_wl_dispatch(uint32_t timeout_ms);
/* True once the native backend's own shared connection is open (at least
 * one native-backend window currently exists) -- window_wayland.c's own
 * crtgfx_host_window_dispatch() uses this (together with its own legacy
 * crtgfx_wl_conn) to decide whether it needs to pump one backend, the
 * other, or both (see that function's own comment for the real, honest
 * "both live at once" budget-splitting policy). */
int crtgfx_native_wl_has_connection(void);

/* For src/arch/linux/gpu_vulkan.c's own vkCreateWaylandSurfaceKHR() call
 * only: returns 1 and fills out_display/out_surface (each a pointer to a
 * pointer) with this window's own real, live wl_display and wl_surface
 * pointers if `toplevel` belongs to a native-backend window with an open
 * connection; returns 0 (leaving the outputs untouched) for anything
 * else, including a legacy-backend window -- a caller does not need to
 * check which backend created a given crtgfx_window* itself before
 * calling this, it is already safe to call unconditionally. Declared
 * `void*` rather than the real wl_display/wl_surface pointer types:
 * gpu_vulkan.c is Linux/Vulkan-only but this header is included from
 * window_wayland.c too (for the dispatch table above), which never
 * includes <wayland-client.h> at all (the legacy backend, deliberately,
 * has never linked real libwayland-client -- see this file's own top
 * comment) -- gpu_vulkan.c casts these back to the real pointer types
 * itself, which is always safe since a real wl_display/wl_surface
 * pointer is exactly what this function actually stored. */
int crtgfx_native_wl_get_surface_handles(
    const crtgfx_weston_toplevel* toplevel, void** out_display, void** out_surface);
