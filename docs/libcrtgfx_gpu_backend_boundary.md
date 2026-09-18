# libcrtgfx GPU Backend Object Boundary

This document freezes the internal `crtgfx_gpu_device` and
`crtgfx_gpu_surface` ownership boundary before hardware decode and zero-copy
interop add more native objects. It refines the project-wide Host ABI firewall
in `docs/host_abi_firewall.md` for the `libcrtgfx` GPU implementation.

## Problem being removed

`src/gpu_internal.h` originally gave the same C type a different layout under
`CRTGFX_HAVE_VULKAN`, `CRTGFX_HAVE_D3D12`, and `CRTGFX_HAVE_METAL`. The build
therefore relies on every translation unit seeing identical feature macros.
If `gpu.c` allocates the smaller layout while a backend was compiled for the
larger layout, the backend writes beyond the allocation. Directory-wide CMake
definitions reduce that risk but do not make the representation safe.

The backend layout also leaked into `src/skia_bridge.cc`, which read every
native device view and presentation image directly. Before Tranche 3, its
D3D12 branch went further and mutated command-list, fence-value, per-buffer,
submission, and Ganesh-wrap state; before Tranche 4, its Metal branch read
`crtgfx_gpu_device`/`crtgfx_gpu_surface` concrete fields directly the same
way. `tests/skia_gpu_offscreen_smoke.cc` likewise included the private layout
and had a direct Metal-native device-loss/handle-mutation path before
Tranche 4 moved it behind an owner hook. Those callers bypass the object
owner even when their current code is otherwise correct.

## Fixed common representation

The completed representation will be equivalent to:

```c
struct crtgfx_gpu_device {
  atomic_int refcount;
  crtgfx_gpu_backend backend;
  const struct crtgfx_gpu_backend_ops* ops;
  void* backend_state;
};

struct crtgfx_gpu_surface {
  crtgfx_gpu_backend backend;
  const struct crtgfx_gpu_backend_ops* ops;
  void* backend_state;
};
```

The device refcount is common state, not backend state. This preserves the
public atomic shared-ownership contract needed when decode and render threads
hold the same device. A surface remains single-owner and does not retain its
device or window: the existing public rule that both outlive the surface is
unchanged.

No `CRTGFX_HAVE_*` conditional may appear inside either common structure.
Feature macros select an implementation and its declarations; they never
select the allocation size or field offsets of a common object.

Implementation status (2026-09-17): both wrappers now carry only the backend
tag, operations pointer, and opaque state slot (plus the device's common
atomic refcount), and public device/surface entry points dispatch through
that table. Vulkan device/surface state lives only in `gpu_vulkan.c`, D3D12
device/surface state lives only in `gpu_win32.c`, and Metal device/surface
state lives only in `gpu_metal.c` -- no backend concrete fields remain in the
common structures. Vulkan's wrapper/state part of the migration (Tranche 1)
is complete, including owner-local Wayland extraction and injected cleanup
after partial device construction. D3D12's owner migration and Windows
hardware/WARP acceptance are complete in Tranche 3. Metal's owner migration
(Tranche 4) is complete: `gpu_metal.c` owns CAMetalLayer extraction from the
opaque `crtgfx_window*`, all Metal object lifetimes, and the device-loss test
hook; `skia_bridge.cc` and `tests/skia_gpu_offscreen_smoke.cc` use only
borrowed device/surface views. Real device creation, 5-frame window
presentation, a scripted Retina resize (900x520 points correctly reporting a
1800x1040-pixel surface), and Metal Ganesh (offscreen smoke plus the windowed
demo) all pass on macOS/arm64 hardware. Tranche 5 is also complete: generic
tests no longer include this private layout, feature/OS macros are
target-private implementation selectors, and an explicit backend-disabled
configuration builds the fixed common wrappers without a backend object.

## Operations and ownership

`gpu.c` owns only argument validation, wrapper allocation, device refcounting,
dispatch, and wrapper destruction. The selected backend operations own:

- capability query and concrete device-state create/destroy;
- resolving the platform window into the native presentation target;
- concrete surface-state create/destroy;
- size query, acquire, clear, resize, and present;
- all acquired/wrapped/submitted flags, queue transitions, fence values, and
  native object cleanup.

Backend create operations return a fully usable opaque state pointer on
success. On failure they return no state and clean every native object and
allocation created so far. Destroy accepts only a successfully published
state pointer and runs exactly once. The common wrapper is never published to
the caller until backend creation succeeds.

The backend owns platform window extraction too. `gpu.c` must not include
Wayland, Win32, Cocoa, or `wayland_weston_internal.h`; it passes the opaque
public `crtgfx_window*` to the selected backend's surface-create operation.

## Skia interop

Skia may know Vulkan, D3D12, and Metal API types because it must construct the
corresponding Ganesh objects. It must not know a `crtgfx` backend-state layout.

Each backend supplies narrow borrowed descriptors for the native values Skia
needs. A descriptor is a snapshot valid only while its owning CRT device or
surface remains alive and the documented frame phase remains active. Borrowing
does not transfer ownership, add a native reference, or permit destruction.
It is not a generic `get_native_handle()` escape hatch.

The expected information is:

- Vulkan device: instance, physical device, device, queue, queue family;
- Vulkan surface: current image, format, usage, acquire/render semaphores,
  extent, and queue family;
- D3D12 device: adapter, device, command queue;
- D3D12 surface: current back buffer and extent;
- Metal device: device and command queue;
- Metal surface: current drawable texture and extent.

State transitions remain backend operations. In particular:

- beginning a Ganesh wrap validates the acquire phase and records the wrapped
  phase in the backend;
- Vulkan submission semaphore/layout coordination remains Vulkan-owned;
- D3D12's PRESENT barrier, command-list close/execute, monotonic fence value,
  per-buffer fence value, and submitted flag move out of `skia_bridge.cc`;
- Metal's presenting command-buffer preparation remains Metal-owned;
- completing or abandoning a wrap clears backend state through the owner,
  never by writing a common or concrete field from Skia.

## Test boundary

Generic tests, tools, and examples may include public headers plus a narrow
backend-neutral test-control header. They may not include `gpu_internal.h` or
backend state definitions.

Device-loss coverage uses a backend-owned fault/test operation. Vulkan,
D3D12, and Metal choose their own safe simulation and cleanup behavior; the
generic smoke asks for loss, verifies the public/Skia behavior, releases the
device normally, and verifies recreation. No generic test destroys or nulls a
native handle directly.

Construction failure coverage injects failure after successive backend-owned
creation steps. Every case must return an error without publishing a wrapper,
leak no backend state, and leave no object for common code to destroy. These
hooks are test-only and are not added to the public `crtgfx` ABI.

The Vulkan implementation now injects one-shot failures after VkInstance and
after VkDevice/queue creation. Its focused WSL test checks the exact native
destroy mask, the untouched output pointer, and immediate successful device
recreation. The controls live in a private, non-installed header and have
hidden ELF visibility; no native handle or state layout is exposed.

The Windows implementation has a similarly private, non-installed adapter-
selection control that makes capability query and device creation enumerate
only DXGI's real WARP adapter during acceptance. Production enumeration remains
hardware-preferred and uses WARP only as its no-adapter fallback. The shared
Ganesh smoke proves both the ordinary hardware path and the forced-WARP path by
creating a real context, drawing the reference scene, and reading its pixels
back.

## Current direct-access inventory (2026-09-17)

| Owner/caller | Current access | Required destination |
| --- | --- | --- |
| `src/gpu.c` | fixed wrappers and operations dispatch only | fixed wrappers and operations dispatch only (done) |
| `src/arch/linux/gpu_vulkan.c` | private Vulkan device/surface state, owner-local Wayland extraction, and partial-create fault injection | completed owner |
| `src/arch/windows/gpu_win32.c` | private D3D12 device/surface state, owner-local HWND extraction, borrowed views, submit transition, and private WARP test selection | completed owner |
| `src/arch/macos/gpu_metal.c` | private Metal device/surface state, owner-local CAMetalLayer extraction, and a private device-loss test hook | completed owner |
| `src/skia_bridge.cc` | Vulkan, D3D12, and Metal borrowed views/transitions only | borrowed descriptors plus backend transition operations (done) |
| `tests/skia_gpu_offscreen_smoke.cc` | public GPU/Skia APIs plus private backend-neutral test control; Windows-only WARP adapter selection remains a narrow owner test header | no layout or native-handle access (done) |

No other current test, tool, or example directly accesses these concrete
fields. `tools/test_crtgfx_gpu_backend_boundary.py` turns that inventory into a
seven-case CTest guard: no generic smoke may include `gpu_internal.h`, the
backend-neutral device-loss control must remain the only generic loss path,
backend fields stay owner-local, and no directory-scoped `CRTGFX_HAVE_*`
definition may reopen cross-target coupling. A new privileged caller therefore
cannot quietly appear after the migration.

## Acceptance

- Common device/surface layouts contain zero conditional fields and retain the
  common device atomic-refcount semantics.
- Concrete state definitions are visible only inside their owning backend.
- `gpu.c` is platform-neutral and performs only wrapper lifecycle/dispatch.
- `skia_bridge.cc` uses borrowed descriptors and backend transitions; it never
  reads or writes a concrete `crtgfx` backend-state field.
- Generic tests/tools/examples do not include the private layout.
- Feature-macro mismatch cannot change `sizeof(crtgfx_gpu_device)` or
  `sizeof(crtgfx_gpu_surface)`; a compile-time/focused regression enforces it.
- Injected partial-construction failures clean each backend exactly once.
- Public declarations, return values, and device/surface lifetime semantics do
  not change.
- Backend-enabled and backend-disabled builds pass, followed by Vulkan,
  D3D12, Metal, resize, Skia, static/shared ownership, and distribution-import
  validation on their available hosts.

Tranche 5 Windows evidence (2026-09-17): the seven boundary regressions pass;
the normal configuration builds `crtgfx_gpu`, `crtgfx_gpu_shared`, and
`crtgfx_gpu_test`; its focused GPU tests and real hardware plus forced-WARP
Ganesh smoke pass, including backend-owned D3D12 `RemoveDevice()` and fresh
device/context/draw recovery. A separate configuration with
`CRTGFX_ENABLE_GPU_BACKEND=OFF` creates no backend object target, still builds
both static and shared GPU wrappers plus `crtgfx_gpu_test`, and passes its
focused tests.

Tranche 6 automation (2026-09-17) is now available as
`crtgfx-boundary-acceptance`. It first regenerates and verifies a fresh
`02-cxx` distribution, including its binary-import audit, then runs the
enabled-tree ABI/firewall/source-boundary/GPU/window-event checks. It also
deletes and recreates a dedicated backend-disabled tree, builds both static
and shared common GPU wrappers there, and runs the boundary and public GPU
smokes. Pull-request CI invokes this target on the representative macOS/arm64,
Linux/aarch64, and Windows/x64 jobs. Full Skia/FFmpeg and predecessor-only
distribution audits remain explicit scheduled/release evidence rather than
making every pull request rebuild the complete upper-runtime stack.

The first real target run passed on macOS/arm64: fresh `02-cxx` verification,
all 7 enabled checks, and both clean backend-disabled checks succeeded.
Tranche 6 remains open until the new Linux/aarch64 and Windows/x64 CI lanes
produce their first green evidence; that pending evidence is not inferred from
the earlier host-specific backend runs.
