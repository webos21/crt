# Draft Mesa GitLab issue: lavapipe Wayland WSI capabilities deadlock

Not yet submitted upstream (requires a GitLab.com/freedesktop.org account
this session does not have login access to) -- drafted here so it is not
lost, and so the user can submit it from their own account when convenient.
Once actually filed, replace this note with the real issue URL and link it
from `HISTORY.md`'s/`TODO.md`'s own matching entries (2026-09-07, "Linux
on-screen GPU presentation" item).

Root-caused via `lldb` on real Linux aarch64 hardware (this project's own
`crtgfx_gpu_window_demo`, native Wayland + Vulkan swapchain vertical slice):
`vkGetPhysicalDeviceSurfaceCapabilitiesKHR` deadlocks on the main thread,
100% reproducible, confirmed to be entirely inside Mesa's own lavapipe
driver + libwayland-client (`wl_proxy_create_wrapper` self-locking a
non-recursive `wl_display` mutex) -- this project's own code only appears
starting at the frame that makes the (correct, standard) Vulkan API call.
Ruled out: the `VkLayer_MESA_device_select` deadlock class covered by Mesa
issue #15168/MR !38252 (different code path, confirmed via
`NODEVICE_SELECT=1` and `VK_LOADER_LAYERS_DISABLE=~implicit~` not changing
the hang); llvmpipe's own worker-thread pool (`LP_NUM_THREADS=1` no
change); a starved default Wayland event queue (an explicit
`wl_display_roundtrip()` right after `vkCreateWaylandSurfaceKHR`, confirmed
to complete normally, does not unblock the later capabilities call); a
Mesa-loader/ICD-dispatch issue (`VK_LOADER_DEBUG=all` shows clean dispatch
into the driver, nothing more to trace once inside it); and a blanket
"lavapipe + this GNOME session is broken" theory (`vkcube-wayland` on the
identical host/session/Mesa version renders continuously, no hang).

Searched `gitlab.freedesktop.org/mesa/mesa` issues thoroughly first (several
keyword combinations: `wl_proxy_create_wrapper`,
`vkGetPhysicalDeviceSurfaceCapabilitiesKHR`, `lavapipe wayland deadlock`,
`wsi_wl_get_capabilities`, `zwp_linux_dmabuf_feedback wsi capabilities`,
`vulkan wayland hang mutter`, `wayland surface deadlock`, and browsing all
24 `lavapipe wayland` hits with state=Any) -- no existing issue matches this
exact symptom. The closest, #15168 ("Deadlock in vkEnumeratePhysicalDevices
during monitor hotplug on dual-GPU (i915 + nouveau) -- regression in 25.2",
fixed by !38252, Mesa 25.3 only, not backported to 25.2), is a materially
different scenario -- there, the process *is* the Wayland compositor
(mutter/zink) and deadlocks talking to itself; here, this is an ordinary
client (not the compositor), and disabling the device-select layer
entirely does not change the hang, confirming a different code path
(lavapipe's own WSI capabilities query, not device selection).

---

## Title

```
Deadlock in vkGetPhysicalDeviceSurfaceCapabilitiesKHR (lavapipe) via self-locking wl_proxy_create_wrapper, GNOME/Wayland
```

## Body

```
### System information

- OS: Ubuntu 24.04.4 LTS (Noble Numbat), aarch64 (ARM VM guest)
- Kernel: 6.8.0-139-generic
- GPU: virtio-gpu (VM device); no hardware-backed Vulkan ICD available on this
  host (virtio_icd.json enumerates 0 devices), so lavapipe/llvmpipe is the
  only usable Vulkan device
- Mesa: 25.2.8-0ubuntu0.24.04.2 (mesa-vulkan-drivers, libvulkan_lvp.so)
- Vulkan loader: 1.3.275 (libvulkan1)
- Compositor: GNOME Shell 46.0 / Mutter 46.2, native Wayland session
  (WAYLAND_DISPLAY set, no XWayland involved for this client)

### Description

A plain Wayland client (not the compositor itself, no dual-GPU/hotplug
involved) deadlocks inside `vkGetPhysicalDeviceSurfaceCapabilitiesKHR` on the
main thread, on every run, 100% reproducible. The hang happens *after* both
`vkCreateWaylandSurfaceKHR` and `vkGetPhysicalDeviceSurfaceSupportKHR` have
already returned `VK_SUCCESS` for the exact same `VkSurfaceKHR`, so this is
not the compositor-talking-to-itself deadlock covered by #15168 / !38252
(confirmed separately: setting `NODEVICE_SELECT=1` and
`VK_LOADER_LAYERS_DISABLE=~implicit~`, which fully disable
`VkLayer_MESA_device_select`, do not change the hang at all -- the deadlock
is inside the driver's own WSI code, not the device-select layer).

### Steps to reproduce

1. Connect to the compositor: `wl_display_connect(NULL)`, bind
   `wl_compositor`/`xdg_wm_base` via the registry, `wl_display_roundtrip()`.
2. Create a `wl_surface`, get its `xdg_surface`/`xdg_toplevel`, do the
   standard double-commit handshake: initial `wl_surface_commit()` (no
   buffer) -> wait for `xdg_surface::configure` -> `xdg_surface_ack_configure()`
   -> second `wl_surface_commit()`. Block until the first configure has been
   received and acked before proceeding (confirmed this fully completes;
   `has_first_configure` becomes true, the wait loop exits normally).
3. Create a `VkInstance` (apiVersion 1.1, `VK_KHR_surface` +
   `VK_KHR_wayland_surface` + `VK_KHR_swapchain` enabled) and a
   `VkDevice` from the (only) enumerated physical device (llvmpipe).
4. `vkCreateWaylandSurfaceKHR(instance, &{sType, display, surface}, NULL,
   &vk_surface)` -> returns `VK_SUCCESS`.
5. `vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, queue_family,
   vk_surface, &supported)` -> returns `VK_SUCCESS`, `supported = VK_TRUE`.
6. `vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, vk_surface,
   &caps)` -> **never returns**.

No compositor-side changes happen between steps 4-6 (no resize, no
hotplug, no monitor reconfiguration) -- this is the very first, immediate
capabilities query right after creating the surface.

Also independently confirmed on this exact host/compositor/Mesa version that
`vkcube-wayland` (from the `vulkan-tools` package) does **not** hang and
renders continuously -- so this is not a blanket "lavapipe + this GNOME
session is broken" issue, it's specific to some part of our call sequence
or state that vkcube's own initialization avoids.

### Backtrace (lldb, all 11 threads; only the main thread is Wayland-related)

Main thread (only thread that ever touches the Wayland connection):

```
#0 0x... libc.so.6 (syscall: svc #0)
#1 0x... libc.so.6`pthread_mutex_lock + 260
#2 0x... libwayland-client.so.0`wl_proxy_create_wrapper + 52
#3 0x... libvulkan_lvp.so`<mesa internal> + 140
#4 0x... libvulkan_lvp.so`<mesa internal> + 748
#5 0x... libvulkan_lvp.so`<mesa internal> + 116
#6 crtgfx_gpu_vulkan_surface_create(...) [our own call site,
   vkGetPhysicalDeviceSurfaceCapabilitiesKHR]
#7 crtgfx_gpu_surface_create(...)
#8 main(...)
```

The other 10 threads are all llvmpipe's own rasterizer thread pool, idle in
`pthread_cond_wait` inside `libvulkan_lvp.so` -- none of them touch Wayland
or hold anything Wayland-related, so this is not worker-thread contention
(also independently confirmed: `LP_NUM_THREADS=1` does not change the
hang).

`wl_proxy_create_wrapper()` is real libwayland-client API used to get a
proxy that can be assigned to a private `wl_event_queue` -- exactly the
mechanism `wsi_common_wayland.c` is expected to use so its own roundtrip
doesn't race with the client's own default-queue dispatch. It's blocked in
`pthread_mutex_lock`, i.e. genuinely waiting on `wl_display`'s own internal
mutex, not spinning/erroring. Since only this one thread ever touches the
Wayland connection, and it is the very thread trying to acquire the lock,
this looks like the driver's own call chain (frames #3-#5, all still on the
stack, so nested rather than sequential) already holds that same
`wl_display` mutex somewhere earlier in this same call and doesn't release
it before a further path also tries to lock it -- a self-deadlock on a
non-recursive mutex.

### What's been ruled out

- `VkLayer_MESA_device_select` involvement: `NODEVICE_SELECT=1` -- no change.
- Any Vulkan implicit layer at all: `VK_LOADER_LAYERS_DISABLE=~implicit~` --
  no change.
- llvmpipe's own worker-thread pool: `LP_NUM_THREADS=1` -- no change.
- A generic "client's default queue is starved" theory: an explicit
  `wl_display_roundtrip()` on the client's own `wl_display`, inserted right
  after `vkCreateWaylandSurfaceKHR` and confirmed to complete normally
  (both before/after log lines print), does not unblock the later
  `vkGetPhysicalDeviceSurfaceCapabilitiesKHR` call.
- Not a Mesa-loader/ICD-selection issue: `VK_LOADER_DEBUG=all` shows the
  loader dispatching cleanly into the driver with no warnings; nothing
  further is logged once inside the driver (expected, since that's
  internal driver code the loader doesn't trace).
- Not specific to this Mesa build being generically broken for Wayland
  presentation: `vkcube-wayland` renders continuously on the identical
  host/session/Mesa version.

Happy to provide the full client source (a small, self-contained ~150-line
C reproducer using hand-declared libwayland-client/Vulkan calls, no third-
party dependencies) if useful -- let me know and I'll attach it.
```
