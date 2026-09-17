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
native device view and presentation image directly. Its D3D12 branch still goes
further and mutates command-list, fence-value, per-buffer, submission, and
Ganesh-wrap state. `tests/skia_gpu_offscreen_smoke.cc` likewise includes the
private layout and tears native device objects down itself for device-loss
coverage. Those callers bypass the object owner even when their current code
is otherwise correct.

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

Implementation status (2026-09-17): both wrappers now carry the backend tag,
operations pointer, and opaque state slot, and public device/surface entry
points dispatch through that table. Vulkan device/surface state now lives only
in `gpu_vulkan.c`, while D3D12 and Metal concrete fields remain temporarily
unconditional behind the same fixed layout. The wrappers are therefore still
an intermediate representation, not the accepted final one. Vulkan's wrapper/
state part of the migration (Tranche 1) is complete, including owner-local
Wayland extraction and injected cleanup after partial device construction.

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

## Current direct-access inventory (2026-09-17)

| Owner/caller | Current access | Required destination |
| --- | --- | --- |
| `src/gpu.c` | fixed wrappers/operations dispatch; temporary D3D12/Metal window extraction adapters remain | fixed wrappers and operations dispatch only |
| `src/arch/linux/gpu_vulkan.c` | private Vulkan device/surface state and owner-local Wayland extraction | completed owner; add partial-create failure injection |
| `src/arch/windows/gpu_win32.c` | D3D12/DXGI device/surface fields | private D3D12 device/surface state |
| `src/arch/macos/gpu_metal.c` | Metal device/surface fields | private Metal device/surface state |
| `src/skia_bridge.cc` | Vulkan borrowed views; direct D3D12 fence/state machine and Metal handles remain | borrowed descriptors plus backend transition operations |
| `tests/skia_gpu_offscreen_smoke.cc` | Vulkan owner hook; direct D3D12/Metal loss and handle mutation remain | backend-neutral test-control operation |

No other current test, tool, or example directly accesses these concrete
fields. This inventory is a migration checklist, not permission to add another
caller while the work is in progress. `tools/test_crtgfx_gpu_backend_boundary.py`
turns that inventory into a CTest guard: the known Skia/generic-test exceptions
are an exact allowlist that each migration tranche must shrink, so a new
privileged caller cannot quietly appear during the refactor.

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
