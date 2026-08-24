/* Standalone smoke test for the externally-built libwayland-client
 * (libcrtgfx/third_party/wayland/recipe.json, tools/fetch_wayland.py/
 * tools/build_wayland.py, driven by the crtgfx-wayland-configure/-build/
 * -smoke CMake targets in libcrtgfx/CMakeLists.txt).
 *
 * Deliberately independent of libcrtgfx's own real Linux window backend
 * (src/arch/linux/window_wayland.c), which intentionally does NOT link
 * libwayland-client (see docs/libcrtgfx_wayland_plan.md's own "Linux Host
 * Adapter" section) -- this program exists only to prove the external
 * Meson build genuinely produces a working, linkable libwayland-client
 * against this project's own CRT toolchain (libc, epoll, memfd_create,
 * mmap, unix domain sockets), independent of whether/when a future
 * session decides to swap the hand-rolled backend for it.
 *
 * Not run against a live compositor by design: crtgfx-wayland-smoke's own
 * CI/WSL environment has none (matching window_wayland.c's own documented
 * "headless-CI shape" fallback). wl_display_connect() returning NULL (no
 * $WAYLAND_DISPLAY / no compositor socket) is treated as a normal, passing
 * outcome -- the real thing this test verifies is that every referenced
 * wayland-client symbol resolved and linked correctly, which already
 * happened by the time main() starts running at all. If a real compositor
 * happens to be reachable (a real desktop session, not typical for a CI
 * runner), the extra registry round trip below exercises real wire-
 * protocol traffic too, on a best-effort basis.
 */
#include <stdio.h>
#include <string.h>

#include <wayland-client.h>

static int registry_global_count = 0;

static void handle_global(void *data, struct wl_registry *registry, uint32_t name,
                           const char *interface, uint32_t version) {
  (void)data;
  (void)registry;
  (void)name;
  (void)version;
  registry_global_count++;
  printf("wayland_client_smoke: global %s\n", interface);
}

static void handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
  (void)data;
  (void)registry;
  (void)name;
}

static const struct wl_registry_listener kRegistryListener = {
    .global = handle_global,
    .global_remove = handle_global_remove,
};

int main(void) {
  struct wl_display *display = wl_display_connect(NULL);
  if (display == NULL) {
    printf("wayland_client_smoke: no compositor available (WAYLAND_DISPLAY unset or "
           "unreachable) -- library build+link verified, live connect skipped\n");
    printf("wayland_client_smoke: ok\n");
    return 0;
  }

  struct wl_registry *registry = wl_display_get_registry(display);
  if (registry == NULL) {
    printf("wayland_client_smoke: wl_display_get_registry failed\n");
    wl_display_disconnect(display);
    return 1;
  }

  wl_registry_add_listener(registry, &kRegistryListener, NULL);
  if (wl_display_roundtrip(display) < 0) {
    printf("wayland_client_smoke: wl_display_roundtrip failed\n");
    wl_registry_destroy(registry);
    wl_display_disconnect(display);
    return 1;
  }

  wl_registry_destroy(registry);
  wl_display_disconnect(display);

  printf("wayland_client_smoke: ok globals=%d (live compositor round trip)\n", registry_global_count);
  return 0;
}
