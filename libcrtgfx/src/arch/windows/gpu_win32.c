/* Real Ganesh/D3D12 offscreen vertical slice (TODO.md's "Enable Skia GPU
 * rendering" step, 2026-09-03) -- the Windows sibling of src/arch/linux/
 * gpu_vulkan.c (the Linux/Vulkan slice landed the same week). See crtgfx/
 * gpu.h's own top comment and libcrtgfx/README.md for the full design
 * record.
 *
 * Hand-declares the minimal real D3D12/DXGI subset needed to create a
 * device/command-queue/adapter and enumerate real capabilities -- matching
 * window_win32.c's own established D3D11 hand-declaration convention
 * (crtgfx_dxgi_guid/SUCCEEDED/FAILED, real IIDs transcribed as byte
 * constants, `void* reserved_N_to_M[count]` vtable padding,
 * `__declspec(dllimport)` externs) and this project's own consistent
 * no-host-SDK-header policy elsewhere (gpu_vulkan.c's own hand-rolled
 * Vulkan subset, the ALSA UAPI structs, the Objective-C runtime C ABI).
 * This file itself never includes the real <d3d12.h>/<dxgi1_4.h> -- unlike
 * skia_bridge.cc's own D3D12 branch, which is forced to (Skia's own public
 * include/gpu/ganesh/d3d/GrD3DTypes.h includes them directly; see that
 * file's own top comment and docs/libcrtgfx_api_policy.md's documented
 * "third-party source being ported" exception).
 *
 * Every vtable slot index, struct field, GUID, and enum value below was
 * confirmed for real (2026-09-03) by reading the real Windows SDK headers
 * directly (C:\Program Files (x86)\Windows Kits\10\Include\10.0.28000.0\
 * um\d3d12.h, shared\dxgi.h, shared\dxgi1_4.h) -- including the real,
 * ready-made C-mode vtable structs those headers themselves generate
 * (e.g. ID3D12DeviceVtbl), not hand-counted from the C++ interface
 * declarations alone. GetDeviceRemovedReason is slot 37; descriptor-heap
 * handle returns require the SDK C interface's explicit result pointer. */

#include "gpu_internal.h"

#include <stddef.h>
#include <stdint.h>
/* Deliberately no <stdlib.h> (no calloc/free): this translation unit
 * compiles with only libcrtgfx/include and libcrtgfx/src on its own
 * include path (see libcrtgfx/CMakeLists.txt's own crtgfx_backend_objects
 * target), not this project's own libc headers -- matching window_win32.c's
 * own identical, already-established discipline (that file's own top
 * comment states this explicitly). crtgfx_win32_calloc()/_free() below
 * (2026-09-07, the swap-chain presentation vertical slice's own real
 * per-buffer/per-frame array allocations) wrap the same real Win32 heap
 * API (GetProcessHeap()/HeapAlloc()/HeapFree()) window_win32.c's own
 * crtgfx_host_window_create()/_destroy() already use, rather than
 * introducing a first libc dependency into this file. */

typedef long HRESULT;
typedef unsigned long ULONG;
typedef unsigned int UINT;
typedef int INT;
typedef int BOOL;
typedef unsigned long DWORD;
typedef void* HANDLE;
typedef HANDLE HWND;
typedef unsigned short crtgfx_dxgi_wchar;

#define CRTGFX_WINAPI __stdcall

#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr) (((HRESULT)(hr)) < 0)

/* Real GUID/IID/REFIID -- matching window_win32.c's own crtgfx_dxgi_guid
 * shape exactly (this file is a separate translation unit, so it hand-
 * declares its own copy rather than sharing one via a header neither file
 * currently exposes -- matching the established per-arch-file self-
 * containment convention already used throughout this project). */
typedef struct crtgfx_dxgi_guid {
  uint32_t Data1;
  uint16_t Data2;
  uint16_t Data3;
  unsigned char Data4[8];
} crtgfx_dxgi_guid;
typedef crtgfx_dxgi_guid GUID;
typedef GUID IID;
typedef const GUID* REFIID;

/* Generic IUnknown surface -- QueryInterface/AddRef/Release are always at
 * vtable slots 0/1/2 for every real COM interface, by COM convention, so
 * this one shape safely handles Release() (and QueryInterface(), for
 * upgrading to a more specific real interface) on any real object this
 * file holds, regardless of its own real interface -- matching window_
 * win32.c's own crtgfx_d3d11_device/crtgfx_d3d11_texture2d "opaque,
 * generically Release()'d" precedent. */
typedef struct crtgfx_dxgi_unknown crtgfx_dxgi_unknown;
typedef struct crtgfx_dxgi_unknown_vtbl {
  HRESULT(CRTGFX_WINAPI* QueryInterface)(crtgfx_dxgi_unknown* self, REFIID riid, void** out);
  ULONG(CRTGFX_WINAPI* AddRef)(crtgfx_dxgi_unknown* self);
  ULONG(CRTGFX_WINAPI* Release)(crtgfx_dxgi_unknown* self);
} crtgfx_dxgi_unknown_vtbl;
struct crtgfx_dxgi_unknown {
  const crtgfx_dxgi_unknown_vtbl* lpVtbl;
};

/* SDK D3D12_CPU_DESCRIPTOR_HANDLE: { SIZE_T ptr; }. Handle arguments
 * are passed by value; COM methods returning handles use the explicit
 * result-pointer ABI declared in the SDK's C interface below. */
typedef struct crtgfx_d3d12_cpu_descriptor_handle {
  size_t ptr;
} crtgfx_d3d12_cpu_descriptor_handle;

/* ID3D12Resource -- opaque beyond IUnknown for this file's own real needs
 * (each swap-chain back buffer is one of these; this file only ever
 * passes the pointer around -- into a resource-barrier struct, a render-
 * target-view creation call, Release() on teardown -- never calls a
 * resource-specific method on it). */
typedef crtgfx_dxgi_unknown crtgfx_d3d12_resource;

/* D3D12_COMMAND_QUEUE_DESC -- real field order confirmed directly against
 * um\d3d12.h:1501. */
typedef uint32_t crtgfx_d3d12_command_list_type;
typedef uint32_t crtgfx_d3d12_command_queue_flags;
typedef struct crtgfx_d3d12_command_queue_desc {
  crtgfx_d3d12_command_list_type Type;
  INT Priority;
  crtgfx_d3d12_command_queue_flags Flags;
  UINT NodeMask;
} crtgfx_d3d12_command_queue_desc;

#define CRTGFX_D3D12_COMMAND_LIST_TYPE_DIRECT 0u
#define CRTGFX_D3D12_COMMAND_QUEUE_FLAG_NONE 0u
#define CRTGFX_D3D_FEATURE_LEVEL_12_0 0xc000

/* ID3D12CommandQueue -- real vtable slots confirmed directly against the
 * real ID3D12CommandQueueVtbl (um\d3d12.h): UpdateTileMappings/
 * CopyTileMappings=8-9 (2 methods, real ID3D12CommandQueue-own additions
 * before the two this file actually needs), ExecuteCommandLists=10,
 * SetMarker/BeginEvent/EndEvent=11-13 (3 methods), Signal=14. Extended
 * 2026-09-07 (the Windows swap-chain presentation vertical slice) from
 * this file's own previous plain-IUnknown-only opaque typedef -- every
 * previous consumer of `crtgfx_d3d12_command_queue*` (device_create()/
 * device_destroy() below, and the cast-to-crtgfx_dxgi_unknown* Release()
 * pattern they already use) keeps working unchanged, since QueryInterface/
 * AddRef/Release still sit at the same real slots 0-2 either way. */
/* Forward declarations only -- both real struct bodies are defined later
 * in this file (crtgfx_d3d12_command_allocator alongside its own real
 * vtable, further down; crtgfx_d3d12_resource is opaque-beyond-IUnknown,
 * declared where it's first needed). A pointer to an as-yet-incomplete
 * struct type is enough for ID3D12Device::CreateCommandList's own
 * signature just below, which only ever holds/passes the pointer, never
 * dereferences the pointee here. */
typedef struct crtgfx_d3d12_command_allocator crtgfx_d3d12_command_allocator;

typedef struct crtgfx_d3d12_command_queue crtgfx_d3d12_command_queue;
typedef struct crtgfx_d3d12_command_queue_vtbl {
  HRESULT(CRTGFX_WINAPI* QueryInterface)(crtgfx_d3d12_command_queue* self, REFIID riid, void** out);
  ULONG(CRTGFX_WINAPI* AddRef)(crtgfx_d3d12_command_queue* self);
  ULONG(CRTGFX_WINAPI* Release)(crtgfx_d3d12_command_queue* self);
  void* reserved_3_to_9[7];
  void(CRTGFX_WINAPI* ExecuteCommandLists)(
      crtgfx_d3d12_command_queue* self, UINT num_command_lists, void* const* command_lists);
  void* reserved_11_to_13[3];
  HRESULT(CRTGFX_WINAPI* Signal)(crtgfx_d3d12_command_queue* self, void* fence, uint64_t value);
} crtgfx_d3d12_command_queue_vtbl;
struct crtgfx_d3d12_command_queue {
  const crtgfx_d3d12_command_queue_vtbl* lpVtbl;
};

/* ID3D12Device -- real vtable slots confirmed directly against the real
 * ID3D12DeviceVtbl struct (um\d3d12.h): CreateCommandQueue at slot 8
 * (QueryInterface/AddRef/Release=0-2, ID3D12Object's own GetPrivateData/
 * SetPrivateData/SetPrivateDataInterface/SetName=3-6, GetNodeCount=7).
 *
 * **Real, pre-existing bug fixed here (2026-09-07, found while adding the
 * new methods below and cross-checking every slot fresh against this
 * machine's own real local SDK, 10.0.28000.0 -- not touched otherwise):
 * `GetDeviceRemovedReason` was placed at slot 39 (`reserved_9_to_38[30]`
 * immediately before it), but a careful, scoped count of this exact real
 * header (excluding the two `#if !defined(_WIN32)/#else` duplicate
 * declarations for `GetResourceAllocationInfo`/`GetCustomHeapProperties`
 * -- only one branch of each ever actually compiles for any real target,
 * so counting both, as an earlier naive pattern-match plausibly did,
 * over-counts by exactly 2) puts it at the real slot 37 instead --
 * `CreateFence` at 36 immediately before it, `Evict` at 35 before that.
 * The call site (crtgfx_gpu_win32_device_destroy() does not call this;
 * it is called by tests/skia_gpu_offscreen_smoke.cc's own device-loss
 * test) was silently calling through the wrong real vtable slot
 * (`CreateQueryHeap`'s real position) with the wrong argument list --
 * re-verified clean against the existing device-loss/recovery coverage
 * after this fix, see HISTORY.md's own 2026-09-07 entry for the full
 * account.** CreateCommandAllocator=9, CreateCommandList=12, CreateDescriptorHeap=14,
 * GetDescriptorHandleIncrementSize=15, CreateRenderTargetView=20,
 * CreateFence=36, GetDeviceRemovedReason=37 (all newly added/corrected
 * this same date, for the Windows swap-chain presentation vertical
 * slice's own real per-frame resource creation and CPU/GPU
 * synchronization needs). */
typedef struct crtgfx_d3d12_device crtgfx_d3d12_device;
typedef struct crtgfx_d3d12_device_vtbl {
  HRESULT(CRTGFX_WINAPI* QueryInterface)(crtgfx_d3d12_device* self, REFIID riid, void** out);
  ULONG(CRTGFX_WINAPI* AddRef)(crtgfx_d3d12_device* self);
  ULONG(CRTGFX_WINAPI* Release)(crtgfx_d3d12_device* self);
  void* reserved_3_to_7[5];
  HRESULT(CRTGFX_WINAPI* CreateCommandQueue)(
      crtgfx_d3d12_device* self, const crtgfx_d3d12_command_queue_desc* desc, REFIID riid,
      void** out_command_queue);
  HRESULT(CRTGFX_WINAPI* CreateCommandAllocator)(
      crtgfx_d3d12_device* self, crtgfx_d3d12_command_list_type type, REFIID riid, void** out_allocator);
  void* reserved_10_to_11[2];
  HRESULT(CRTGFX_WINAPI* CreateCommandList)(
      crtgfx_d3d12_device* self, UINT node_mask, crtgfx_d3d12_command_list_type type,
      crtgfx_d3d12_command_allocator* command_allocator, void* initial_state, REFIID riid,
      void** out_command_list);
  void* reserved_13[1];
  HRESULT(CRTGFX_WINAPI* CreateDescriptorHeap)(
      crtgfx_d3d12_device* self, const void* desc, REFIID riid, void** out_heap);
  UINT(CRTGFX_WINAPI* GetDescriptorHandleIncrementSize)(crtgfx_d3d12_device* self, uint32_t heap_type);
  void* reserved_16_to_19[4];
  void(CRTGFX_WINAPI* CreateRenderTargetView)(
      crtgfx_d3d12_device* self, crtgfx_d3d12_resource* resource, const void* desc,
      crtgfx_d3d12_cpu_descriptor_handle dest_descriptor);
  void* reserved_21_to_35[15];
  HRESULT(CRTGFX_WINAPI* CreateFence)(
      crtgfx_d3d12_device* self, uint64_t initial_value, uint32_t flags, REFIID riid, void** out_fence);
  HRESULT(CRTGFX_WINAPI* GetDeviceRemovedReason)(crtgfx_d3d12_device* self);
} crtgfx_d3d12_device_vtbl;
struct crtgfx_d3d12_device {
  const crtgfx_d3d12_device_vtbl* lpVtbl;
};

/* ID3D12Device5 -- only reachable via QueryInterface on a real ID3D12Device
 * (RemoveDevice() was added in this later, versioned interface, not the
 * base ID3D12Device -- confirmed directly, the same real way as every
 * other slot in this file). RemoveDevice at real slot 62 (slots 3-61
 * reserved -- 59 slots; CreateLifetimeTracker, ID3D12Device5's own first
 * real method, sits at slot 61 immediately before it). Used only by
 * tests/skia_gpu_offscreen_smoke.cc's own device-loss test, not by this
 * file's own real device/queue creation path -- declared here anyway so
 * that test doesn't need its own second, independent hand-declaration of
 * the same real interface. */
typedef struct crtgfx_d3d12_device5 crtgfx_d3d12_device5;
typedef struct crtgfx_d3d12_device5_vtbl {
  HRESULT(CRTGFX_WINAPI* QueryInterface)(crtgfx_d3d12_device5* self, REFIID riid, void** out);
  ULONG(CRTGFX_WINAPI* AddRef)(crtgfx_d3d12_device5* self);
  ULONG(CRTGFX_WINAPI* Release)(crtgfx_d3d12_device5* self);
  void* reserved_3_to_61[59];
  void(CRTGFX_WINAPI* RemoveDevice)(crtgfx_d3d12_device5* self);
} crtgfx_d3d12_device5_vtbl;
struct crtgfx_d3d12_device5 {
  const crtgfx_d3d12_device5_vtbl* lpVtbl;
};

/* IDXGIAdapter1 -- GetDesc1 at real slot 10 (slots 3-9 reserved -- 7 slots:
 * IDXGIObject's own SetPrivateData/SetPrivateDataInterface/GetPrivateData/
 * GetParent=3-6, IDXGIAdapter's own EnumOutputs/GetDesc/
 * CheckInterfaceSupport=7-9). */
typedef struct crtgfx_dxgi_adapter1 crtgfx_dxgi_adapter1;

typedef struct crtgfx_dxgi_luid {
  unsigned long LowPart;
  long HighPart;
} crtgfx_dxgi_luid;

/* DXGI_ADAPTER_DESC1 -- real field order confirmed directly against
 * shared\dxgi.h:2517. Only ::Flags is actually read by this file (the
 * DXGI_ADAPTER_FLAG_SOFTWARE hardware-vs-WARP/software check), but the
 * whole real struct must be declared at its real size -- GetDesc1() fills
 * the entire thing, and a truncated struct here would let it write past
 * this file's own stack allocation. */
typedef struct crtgfx_dxgi_adapter_desc1 {
  crtgfx_dxgi_wchar Description[128];
  UINT VendorId;
  UINT DeviceId;
  UINT SubSysId;
  UINT Revision;
  size_t DedicatedVideoMemory;
  size_t DedicatedSystemMemory;
  size_t SharedSystemMemory;
  crtgfx_dxgi_luid AdapterLuid;
  UINT Flags;
} crtgfx_dxgi_adapter_desc1;

#define CRTGFX_DXGI_ADAPTER_FLAG_SOFTWARE 2u

typedef struct crtgfx_dxgi_adapter1_vtbl {
  HRESULT(CRTGFX_WINAPI* QueryInterface)(crtgfx_dxgi_adapter1* self, REFIID riid, void** out);
  ULONG(CRTGFX_WINAPI* AddRef)(crtgfx_dxgi_adapter1* self);
  ULONG(CRTGFX_WINAPI* Release)(crtgfx_dxgi_adapter1* self);
  void* reserved_3_to_9[7];
  HRESULT(CRTGFX_WINAPI* GetDesc1)(crtgfx_dxgi_adapter1* self, crtgfx_dxgi_adapter_desc1* out_desc);
} crtgfx_dxgi_adapter1_vtbl;
struct crtgfx_dxgi_adapter1 {
  const crtgfx_dxgi_adapter1_vtbl* lpVtbl;
};

/* IDXGIFactory1 -- EnumAdapters1 at real slot 12 (slots 3-11 reserved --
 * 9 slots: IDXGIObject's own 4 + IDXGIFactory's own EnumAdapters/
 * MakeWindowAssociation/GetWindowAssociation/CreateSwapChain/
 * CreateSoftwareAdapter=5). */
typedef struct crtgfx_dxgi_factory1 crtgfx_dxgi_factory1;
typedef struct crtgfx_dxgi_factory1_vtbl {
  HRESULT(CRTGFX_WINAPI* QueryInterface)(crtgfx_dxgi_factory1* self, REFIID riid, void** out);
  ULONG(CRTGFX_WINAPI* AddRef)(crtgfx_dxgi_factory1* self);
  ULONG(CRTGFX_WINAPI* Release)(crtgfx_dxgi_factory1* self);
  void* reserved_3_to_11[9];
  HRESULT(CRTGFX_WINAPI* EnumAdapters1)(
      crtgfx_dxgi_factory1* self, UINT adapter_index, crtgfx_dxgi_adapter1** out_adapter);
} crtgfx_dxgi_factory1_vtbl;
struct crtgfx_dxgi_factory1 {
  const crtgfx_dxgi_factory1_vtbl* lpVtbl;
};

/* IDXGIFactory4 -- EnumWarpAdapter at real slot 27 (slots 3-26 reserved --
 * 24 slots: IDXGIFactory1's own 10 (EnumAdapters1 + IsCurrent, on top of
 * IDXGIObject/IDXGIFactory's own 9) + IDXGIFactory2's own 11 +
 * IDXGIFactory3's own GetCreationFlags + IDXGIFactory4's own
 * EnumAdapterByLuid). Only ever reached via QueryInterface on the same
 * real IDXGIFactory1 object EnumAdapters1 already used -- WARP is a real,
 * always-installable software D3D12 adapter, matching window_win32.c's
 * own hardware-then-WARP fallback precedent for D3D11. */
typedef struct crtgfx_dxgi_factory4 crtgfx_dxgi_factory4;
typedef struct crtgfx_dxgi_factory4_vtbl {
  HRESULT(CRTGFX_WINAPI* QueryInterface)(crtgfx_dxgi_factory4* self, REFIID riid, void** out);
  ULONG(CRTGFX_WINAPI* AddRef)(crtgfx_dxgi_factory4* self);
  ULONG(CRTGFX_WINAPI* Release)(crtgfx_dxgi_factory4* self);
  void* reserved_3_to_26[24];
  HRESULT(CRTGFX_WINAPI* EnumWarpAdapter)(crtgfx_dxgi_factory4* self, REFIID riid, void** out_adapter);
} crtgfx_dxgi_factory4_vtbl;
struct crtgfx_dxgi_factory4 {
  const crtgfx_dxgi_factory4_vtbl* lpVtbl;
};

/* ---- Real D3D12 swap-chain presentation additions, 2026-09-07 -- the
 * Windows leg of "Finish live GPU presentation everywhere", following the
 * Linux native-Wayland-backend vertical slice. Same hand-declaration
 * discipline as everything above: every vtable slot/struct field/GUID/
 * enum value below confirmed directly against this machine's own real
 * local SDK headers (10.0.28000.0), not memory -- see the ID3D12Device
 * vtable's own comment above for the one real, pre-existing bug this
 * same cross-check found and fixed along the way. */

/* ID3D12CommandAllocator -- Reset at real slot 8 (GetDevice at slot 7 is
 * the only real method before it, same "ID3D12Object's own 4 + IUnknown's
 * own 3 + GetDevice" base every ID3D12DeviceChild-derived interface in
 * this file already has). `crtgfx_d3d12_command_allocator` itself was
 * already forward-declared earlier (right before crtgfx_d3d12_command_
 * queue's own vtable, needed there for ID3D12Device::CreateCommandList's
 * signature) -- not repeated here (a real C99 issue, not just style: a
 * duplicate `typedef` of the same name is only valid from C11 onward, and
 * this file compiles at `-std=c99`). */
typedef struct crtgfx_d3d12_command_allocator_vtbl {
  HRESULT(CRTGFX_WINAPI* QueryInterface)(crtgfx_d3d12_command_allocator* self, REFIID riid, void** out);
  ULONG(CRTGFX_WINAPI* AddRef)(crtgfx_d3d12_command_allocator* self);
  ULONG(CRTGFX_WINAPI* Release)(crtgfx_d3d12_command_allocator* self);
  void* reserved_3_to_7[5];
  HRESULT(CRTGFX_WINAPI* Reset)(crtgfx_d3d12_command_allocator* self);
} crtgfx_d3d12_command_allocator_vtbl;
struct crtgfx_d3d12_command_allocator {
  const crtgfx_d3d12_command_allocator_vtbl* lpVtbl;
};

/* ID3D12GraphicsCommandList -- Close=9, Reset=10, ResourceBarrier=26,
 * OMSetRenderTargets=46, ClearRenderTargetView=48 (real slots, the same
 * base-7 + GetDevice=7 + GetType=8 shape every ID3D12CommandList-derived
 * interface starts with). Declared only up to ClearRenderTargetView --
 * this file's own real needs stop there, matching every other interface's
 * own "declared only up to the last method actually called" discipline. */
typedef struct crtgfx_d3d12_graphics_command_list crtgfx_d3d12_graphics_command_list;
typedef struct crtgfx_d3d12_graphics_command_list_vtbl {
  HRESULT(CRTGFX_WINAPI* QueryInterface)(crtgfx_d3d12_graphics_command_list* self, REFIID riid, void** out);
  ULONG(CRTGFX_WINAPI* AddRef)(crtgfx_d3d12_graphics_command_list* self);
  ULONG(CRTGFX_WINAPI* Release)(crtgfx_d3d12_graphics_command_list* self);
  void* reserved_3_to_8[6];
  HRESULT(CRTGFX_WINAPI* Close)(crtgfx_d3d12_graphics_command_list* self);
  HRESULT(CRTGFX_WINAPI* Reset)(
      crtgfx_d3d12_graphics_command_list* self, crtgfx_d3d12_command_allocator* allocator,
      void* initial_state);
  void* reserved_11_to_25[15];
  void(CRTGFX_WINAPI* ResourceBarrier)(
      crtgfx_d3d12_graphics_command_list* self, UINT num_barriers, const void* barriers);
  void* reserved_27_to_45[19];
  void(CRTGFX_WINAPI* OMSetRenderTargets)(
      crtgfx_d3d12_graphics_command_list* self, UINT num_rtv_descriptors,
      const crtgfx_d3d12_cpu_descriptor_handle* rtv_descriptors, BOOL rts_single_handle,
      const crtgfx_d3d12_cpu_descriptor_handle* dsv_descriptor);
  void* reserved_47[1];
  void(CRTGFX_WINAPI* ClearRenderTargetView)(
      crtgfx_d3d12_graphics_command_list* self, crtgfx_d3d12_cpu_descriptor_handle render_target_view,
      const float color_rgba[4], UINT num_rects, const void* rects);
} crtgfx_d3d12_graphics_command_list_vtbl;
struct crtgfx_d3d12_graphics_command_list {
  const crtgfx_d3d12_graphics_command_list_vtbl* lpVtbl;
};

/* ID3D12Fence -- GetCompletedValue=8, SetEventOnCompletion=9 (same real
 * base-7-plus-GetDevice=7 shape as ID3D12CommandAllocator above). */
typedef struct crtgfx_d3d12_fence crtgfx_d3d12_fence;
typedef struct crtgfx_d3d12_fence_vtbl {
  HRESULT(CRTGFX_WINAPI* QueryInterface)(crtgfx_d3d12_fence* self, REFIID riid, void** out);
  ULONG(CRTGFX_WINAPI* AddRef)(crtgfx_d3d12_fence* self);
  ULONG(CRTGFX_WINAPI* Release)(crtgfx_d3d12_fence* self);
  void* reserved_3_to_7[5];
  uint64_t(CRTGFX_WINAPI* GetCompletedValue)(crtgfx_d3d12_fence* self);
  HRESULT(CRTGFX_WINAPI* SetEventOnCompletion)(crtgfx_d3d12_fence* self, uint64_t value, HANDLE event);
} crtgfx_d3d12_fence_vtbl;
struct crtgfx_d3d12_fence {
  const crtgfx_d3d12_fence_vtbl* lpVtbl;
};

/* ID3D12DescriptorHeap slot 9. The SDK C interface uses an explicit
 * result pointer even though the handle contains only one pointer. */
typedef struct crtgfx_d3d12_descriptor_heap crtgfx_d3d12_descriptor_heap;
typedef struct crtgfx_d3d12_descriptor_heap_vtbl {
  HRESULT(CRTGFX_WINAPI* QueryInterface)(crtgfx_d3d12_descriptor_heap* self, REFIID riid, void** out);
  ULONG(CRTGFX_WINAPI* AddRef)(crtgfx_d3d12_descriptor_heap* self);
  ULONG(CRTGFX_WINAPI* Release)(crtgfx_d3d12_descriptor_heap* self);
  void* reserved_3_to_8[6];
  crtgfx_d3d12_cpu_descriptor_handle*(CRTGFX_WINAPI* GetCPUDescriptorHandleForHeapStart)(
      crtgfx_d3d12_descriptor_heap* self, crtgfx_d3d12_cpu_descriptor_handle* result);
} crtgfx_d3d12_descriptor_heap_vtbl;
struct crtgfx_d3d12_descriptor_heap {
  const crtgfx_d3d12_descriptor_heap_vtbl* lpVtbl;
};

/* D3D12_DESCRIPTOR_HEAP_DESC -- real field order confirmed directly
 * against um\d3d12.h:4033-4039. */
typedef struct crtgfx_d3d12_descriptor_heap_desc {
  uint32_t Type;
  UINT NumDescriptors;
  uint32_t Flags;
  UINT NodeMask;
} crtgfx_d3d12_descriptor_heap_desc;

#define CRTGFX_D3D12_DESCRIPTOR_HEAP_TYPE_RTV 2u
#define CRTGFX_D3D12_DESCRIPTOR_HEAP_FLAG_NONE 0u

/* D3D12_RESOURCE_BARRIER -- real Type/Flags fields confirmed directly
 * against um\d3d12.h:3350-3360; the real struct's own anonymous union
 * (Transition/Aliasing/UAV) is simplified to just Transition inline here
 * (no union keyword at all) -- this file only ever constructs the
 * Transition variant, and D3D12_RESOURCE_TRANSITION_BARRIER (pResource +
 * Subresource + StateBefore + StateAfter, 24 bytes on this project's real
 * 64-bit-only targets) is independently confirmed to be the *largest* of
 * the three real union members (Aliasing is 16 bytes, UAV is 8), so this
 * struct's own real total size already matches the real union's -- no
 * padding needed to stay ABI-compatible for a pointer-only consumer that
 * never reads past what Type says is actually populated (this real
 * driver-side contract, not merely "usually true", is why a plain nested
 * struct is safe here instead of a real C union). */
typedef struct crtgfx_d3d12_resource_transition_barrier {
  crtgfx_d3d12_resource* resource;
  UINT subresource;
  uint32_t state_before;
  uint32_t state_after;
} crtgfx_d3d12_resource_transition_barrier;

typedef struct crtgfx_d3d12_resource_barrier {
  uint32_t type;
  uint32_t flags;
  crtgfx_d3d12_resource_transition_barrier transition;
} crtgfx_d3d12_resource_barrier;

#define CRTGFX_D3D12_RESOURCE_BARRIER_TYPE_TRANSITION 0u
#define CRTGFX_D3D12_RESOURCE_BARRIER_FLAG_NONE 0u
#define CRTGFX_D3D12_RESOURCE_STATE_PRESENT 0u
#define CRTGFX_D3D12_RESOURCE_STATE_RENDER_TARGET 0x4u
#define CRTGFX_D3D12_FENCE_FLAG_NONE 0u

/* IDXGIFactory2 -- own, separate copy from window_win32.c's own identical
 * declaration (matching this file's own established "separate
 * translation unit, own copy" convention, see this file's top comment).
 * CreateSwapChainForHwnd at real slot 15 (shared/dxgi1_2.h), confirmed
 * against the same real header window_win32.c's own copy already uses --
 * see that file's own crtgfx_dxgi_factory2_vtbl for the identical
 * slot-count reasoning (SetPrivateData through IsWindowedStereoEnabled,
 * 12 real methods at slots 3-14). Unlike window_win32.c's own copy
 * (created against an ID3D11Device), this file's own CreateSwapChainForHwnd
 * call passes a real ID3D12CommandQueue* as the `device` parameter -- a
 * real, documented D3D12 requirement (the swap chain presents through the
 * queue, not the device, for D3D12 specifically), reasoned from Microsoft's
 * own published D3D12 swap-chain creation samples, not independently
 * re-derivable from the vtable slot layout alone -- flagged here per this
 * project's own "reasoned but flagged unverified" discipline for the one
 * piece of this feature not directly checked against a real local header
 * (there is no real code sample shipped in this SDK install to cross-check
 * against, only the documented convention). */
typedef struct crtgfx_dxgi_factory2 crtgfx_dxgi_factory2;
typedef struct crtgfx_dxgi_factory2_vtbl {
  HRESULT(CRTGFX_WINAPI* QueryInterface)(crtgfx_dxgi_factory2* self, REFIID riid, void** out);
  ULONG(CRTGFX_WINAPI* AddRef)(crtgfx_dxgi_factory2* self);
  ULONG(CRTGFX_WINAPI* Release)(crtgfx_dxgi_factory2* self);
  void* reserved_3_to_14[12];
  HRESULT(CRTGFX_WINAPI* CreateSwapChainForHwnd)(
      crtgfx_dxgi_factory2* self, crtgfx_dxgi_unknown* device, HWND hwnd, const void* desc,
      const void* fullscreen_desc, void* restrict_to_output, void** out_swap_chain);
} crtgfx_dxgi_factory2_vtbl;
struct crtgfx_dxgi_factory2 {
  const crtgfx_dxgi_factory2_vtbl* lpVtbl;
};

/* IDXGISwapChain3 -- Present=8, GetBuffer=9, ResizeBuffers=13,
 * GetCurrentBackBufferIndex=36 (real slots, shared/dxgi1_4.h). Declared
 * only up to the last method this file actually calls, same discipline
 * as every other interface here -- slots between confirmed real methods
 * this file skips are anonymous `void* reserved[N]` padding, only their
 * count matters. */
typedef struct crtgfx_dxgi_swapchain3 crtgfx_dxgi_swapchain3;
typedef struct crtgfx_dxgi_swapchain3_vtbl {
  HRESULT(CRTGFX_WINAPI* QueryInterface)(crtgfx_dxgi_swapchain3* self, REFIID riid, void** out);
  ULONG(CRTGFX_WINAPI* AddRef)(crtgfx_dxgi_swapchain3* self);
  ULONG(CRTGFX_WINAPI* Release)(crtgfx_dxgi_swapchain3* self);
  void* reserved_3_to_7[5];
  HRESULT(CRTGFX_WINAPI* Present)(crtgfx_dxgi_swapchain3* self, UINT sync_interval, UINT flags);
  HRESULT(CRTGFX_WINAPI* GetBuffer)(crtgfx_dxgi_swapchain3* self, UINT buffer, REFIID riid, void** out);
  void* reserved_10_to_12[3];
  HRESULT(CRTGFX_WINAPI* ResizeBuffers)(
      crtgfx_dxgi_swapchain3* self, UINT buffer_count, UINT width, UINT height, UINT new_format,
      UINT flags);
  void* reserved_14_to_35[22];
  UINT(CRTGFX_WINAPI* GetCurrentBackBufferIndex)(crtgfx_dxgi_swapchain3* self);
} crtgfx_dxgi_swapchain3_vtbl;
struct crtgfx_dxgi_swapchain3 {
  const crtgfx_dxgi_swapchain3_vtbl* lpVtbl;
};

typedef struct crtgfx_dxgi_sample_desc {
  UINT Count;
  UINT Quality;
} crtgfx_dxgi_sample_desc;

/* DXGI_SWAP_CHAIN_DESC1 -- real field order, own separate copy from
 * window_win32.c's own identical declaration (same reasoning as
 * crtgfx_dxgi_factory2 above). */
typedef struct crtgfx_dxgi_swap_chain_desc1 {
  UINT Width;
  UINT Height;
  UINT Format;
  BOOL Stereo;
  crtgfx_dxgi_sample_desc SampleDesc;
  UINT BufferUsage;
  UINT BufferCount;
  UINT Scaling;
  UINT SwapEffect;
  UINT AlphaMode;
  UINT Flags;
} crtgfx_dxgi_swap_chain_desc1;

#define CRTGFX_DXGI_FORMAT_B8G8R8A8_UNORM 87u
#define CRTGFX_DXGI_USAGE_RENDER_TARGET_OUTPUT 0x00000020ul
#define CRTGFX_DXGI_SCALING_STRETCH 0u
#define CRTGFX_DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL 3u
#define CRTGFX_DXGI_ALPHA_MODE_IGNORE 3u

/* Real, fixed cap on swap-chain back buffers -- matches gpu_vulkan.c's
 * own CRTGFX_GPU_VULKAN_MAX_SWAPCHAIN_IMAGES precedent (generous for any
 * real host; a real DXGI flip-model swap chain is limited to 2-16 buffers
 * by the spec itself). */
#define CRTGFX_GPU_WIN32_MAX_SWAPCHAIN_BUFFERS 16u

/* Real IIDs, transcribed from each interface's own real MIDL_INTERFACE(...)
 * UUID in the Windows SDK headers -- matching window_win32.c's own
 * dxguid.lib-avoiding precedent exactly (see that file's own top comment
 * on why: this project hand-declares its own GUID constants instead of
 * linking the SDK's own extern IID_* symbols). */
static const GUID crtgfx_iid_id3d12_device = {
    0x189819f1, 0x1db6, 0x4b57, {0xbe, 0x54, 0x18, 0x21, 0x33, 0x9b, 0x85, 0xf7}};
static const GUID crtgfx_iid_id3d12_device5 = {
    0x8b4f173b, 0x2fea, 0x4b80, {0x8f, 0x58, 0x43, 0x07, 0x19, 0x1a, 0xb9, 0x5d}};
static const GUID crtgfx_iid_id3d12_command_queue = {
    0x0ec870a6, 0x5d7e, 0x4c22, {0x8c, 0xfc, 0x5b, 0xaa, 0xe0, 0x76, 0x16, 0xed}};
static const GUID crtgfx_iid_idxgi_factory1 = {
    0x770aae78, 0xf26f, 0x4dba, {0xa8, 0x29, 0x25, 0x3c, 0x83, 0xd1, 0xb3, 0x87}};
static const GUID crtgfx_iid_idxgi_factory4 = {
    0x1bc6ea02, 0xef36, 0x464f, {0xbf, 0x0c, 0x21, 0xca, 0x39, 0xe5, 0x16, 0x8a}};
static const GUID crtgfx_iid_idxgi_adapter1 = {
    0x29038f61, 0x3839, 0x4626, {0x91, 0xfd, 0x08, 0x68, 0x79, 0x01, 0x1a, 0x05}};
static const GUID crtgfx_iid_id3d12_resource = {
    0x696442be, 0xa72e, 0x4059, {0xbc, 0x79, 0x5b, 0x5c, 0x98, 0x04, 0x0f, 0xad}};
static const GUID crtgfx_iid_id3d12_command_allocator = {
    0x6102dee4, 0xaf59, 0x4b09, {0xb9, 0x99, 0xb4, 0x4d, 0x73, 0xf0, 0x9b, 0x24}};
static const GUID crtgfx_iid_id3d12_graphics_command_list = {
    0x5b160d0f, 0xac1b, 0x4185, {0x8b, 0xa8, 0xb3, 0xae, 0x42, 0xa5, 0xa4, 0x55}};
static const GUID crtgfx_iid_id3d12_fence = {
    0x0a753dcf, 0xc4d8, 0x4b91, {0xad, 0xf6, 0xbe, 0x5a, 0x60, 0xd9, 0x5a, 0x76}};
static const GUID crtgfx_iid_id3d12_descriptor_heap = {
    0x8efb471d, 0x616c, 0x4f49, {0x90, 0xf7, 0x12, 0x7b, 0xb7, 0x63, 0xfa, 0x51}};
static const GUID crtgfx_iid_idxgi_factory2 = {
    0x50c83a1c, 0xe072, 0x4c48, {0x87, 0xb0, 0x36, 0x30, 0xfa, 0x36, 0xa6, 0xd0}};
static const GUID crtgfx_iid_idxgi_swapchain3 = {
    0x94d99bdb, 0xf1f8, 0x4ab0, {0xb2, 0x36, 0x7d, 0xa0, 0x17, 0x0e, 0xda, 0xb1}};

__declspec(dllimport) HRESULT CRTGFX_WINAPI D3D12CreateDevice(
    void* adapter, uint32_t minimum_feature_level, REFIID riid, void** out_device);
__declspec(dllimport) HRESULT CRTGFX_WINAPI CreateDXGIFactory1(REFIID riid, void** out_factory);
__declspec(dllimport) HANDLE CRTGFX_WINAPI CreateEventW(
    void* security_attributes, BOOL manual_reset, BOOL initial_state, const void* name);
__declspec(dllimport) DWORD CRTGFX_WINAPI WaitForSingleObject(HANDLE handle, DWORD timeout_ms);
__declspec(dllimport) uint64_t CRTGFX_WINAPI GetTickCount64(void);
__declspec(dllimport) BOOL CRTGFX_WINAPI CloseHandle(HANDLE handle);
__declspec(dllimport) HANDLE CRTGFX_WINAPI GetProcessHeap(void);
__declspec(dllimport) void* CRTGFX_WINAPI HeapAlloc(HANDLE heap, DWORD flags, size_t bytes);
__declspec(dllimport) BOOL CRTGFX_WINAPI HeapFree(HANDLE heap, DWORD flags, void* mem);

#define CRTGFX_HEAP_ZERO_MEMORY 0x00000008ul

/* calloc()-equivalent over the real Win32 process heap -- see this file's
 * own top comment for why (no <stdlib.h> on this translation unit's own
 * include path at all). Real overflow guard on `count * size`, matching
 * this project's own established discipline elsewhere (crtgfx_dxgi_swap_
 * chain_desc1::Width/Height-style bounds checks) rather than trusting the
 * multiplication blindly. */
static void* crtgfx_win32_calloc(size_t count, size_t size) {
  if (size != 0 && count > (size_t)-1 / size) {
    return NULL;
  }
  return HeapAlloc(GetProcessHeap(), CRTGFX_HEAP_ZERO_MEMORY, count * size);
}

static void crtgfx_win32_free(void* mem) {
  if (mem != NULL) {
    HeapFree(GetProcessHeap(), 0, mem);
  }
}

/* Real, fixed cap on enumerated adapters -- matching gpu_vulkan.c's own
 * CRTGFX_GPU_VULKAN_MAX_PHYSICAL_DEVICES precedent (generous for any real
 * host; avoids a dynamic allocation for what is, on every real host this
 * project targets, a small, bounded list). */
#define CRTGFX_GPU_WIN32_MAX_ADAPTERS 16u

/* Enumerates every real adapter and reorders them so hardware-backed
 * adapters (no DXGI_ADAPTER_FLAG_SOFTWARE) come first, falling back to a
 * real WARP adapter only if literally nothing else enumerated at all --
 * matching gpu_vulkan.c's own hardware-preferred device_index ordering
 * and window_win32.c's own existing hardware-then-WARP precedent for
 * D3D11. Every entry in `out_adapters[0..*out_count)` is a real, owned
 * reference the caller must eventually Release(). */
static crtgfx_result crtgfx_gpu_win32_enumerate_ordered(
    crtgfx_dxgi_adapter1** out_adapters, uint32_t* out_count) {
  crtgfx_dxgi_factory1* factory;
  HRESULT hr;
  crtgfx_dxgi_adapter1* raw[CRTGFX_GPU_WIN32_MAX_ADAPTERS];
  uint32_t raw_count = 0;
  uint32_t ordered = 0;
  uint32_t i;

  *out_count = 0;
  hr = CreateDXGIFactory1(&crtgfx_iid_idxgi_factory1, (void**)&factory);
  if (FAILED(hr) || factory == NULL) {
    return CRTGFX_ERROR_UNSUPPORTED;
  }

  for (i = 0; i < CRTGFX_GPU_WIN32_MAX_ADAPTERS; ++i) {
    crtgfx_dxgi_adapter1* adapter = NULL;
    hr = factory->lpVtbl->EnumAdapters1(factory, i, &adapter);
    if (FAILED(hr) || adapter == NULL) {
      break;
    }
    raw[raw_count++] = adapter;
  }

  /* Pass 1: real hardware adapters first. Pass 2: everything else (a
   * software-flagged adapter, or one GetDesc1 itself failed on) -- between
   * the two passes, every real adapter enumerated above ends up in
   * out_adapters exactly once, just reordered, never dropped. */
  for (i = 0; i < raw_count; ++i) {
    crtgfx_dxgi_adapter_desc1 desc;
    hr = raw[i]->lpVtbl->GetDesc1(raw[i], &desc);
    if (SUCCEEDED(hr) && (desc.Flags & CRTGFX_DXGI_ADAPTER_FLAG_SOFTWARE) == 0) {
      out_adapters[ordered++] = raw[i];
    }
  }
  for (i = 0; i < raw_count; ++i) {
    crtgfx_dxgi_adapter_desc1 desc;
    hr = raw[i]->lpVtbl->GetDesc1(raw[i], &desc);
    if (FAILED(hr) || (desc.Flags & CRTGFX_DXGI_ADAPTER_FLAG_SOFTWARE) != 0) {
      out_adapters[ordered++] = raw[i];
    }
  }
  *out_count = ordered;

  /* Real WARP fallback, only when adapter enumeration itself found nothing
   * real to offer at all (raw_count == 0, so ordered == 0 too here). */
  if (*out_count == 0) {
    crtgfx_dxgi_factory4* factory4 = NULL;
    hr = factory->lpVtbl->QueryInterface(factory, &crtgfx_iid_idxgi_factory4, (void**)&factory4);
    if (SUCCEEDED(hr) && factory4 != NULL) {
      crtgfx_dxgi_adapter1* warp = NULL;
      hr = factory4->lpVtbl->EnumWarpAdapter(factory4, &crtgfx_iid_idxgi_adapter1, (void**)&warp);
      if (SUCCEEDED(hr) && warp != NULL) {
        out_adapters[0] = warp;
        *out_count = 1;
      }
      ((crtgfx_dxgi_unknown*)factory4)->lpVtbl->Release((crtgfx_dxgi_unknown*)factory4);
    }
  }

  ((crtgfx_dxgi_unknown*)factory)->lpVtbl->Release((crtgfx_dxgi_unknown*)factory);
  return (*out_count > 0) ? CRTGFX_OK : CRTGFX_ERROR_UNSUPPORTED;
}

crtgfx_result crtgfx_gpu_win32_query_capabilities(crtgfx_gpu_capabilities* out_caps) {
  crtgfx_dxgi_adapter1* adapters[CRTGFX_GPU_WIN32_MAX_ADAPTERS];
  uint32_t count = 0;
  crtgfx_result result = crtgfx_gpu_win32_enumerate_ordered(adapters, &count);
  uint32_t i;

  if (result != CRTGFX_OK) {
    /* Real, honest report: no usable D3D12 adapter (not even WARP) on
     * this host right now -- matches crtgfx_window_create()'s own "no
     * usable host backend right now" contract. */
    out_caps->backend = CRTGFX_GPU_BACKEND_NONE;
    out_caps->device_count = 0;
    return CRTGFX_OK;
  }
  for (i = 0; i < count; ++i) {
    ((crtgfx_dxgi_unknown*)adapters[i])->lpVtbl->Release((crtgfx_dxgi_unknown*)adapters[i]);
  }
  out_caps->backend = CRTGFX_GPU_BACKEND_D3D12;
  out_caps->device_count = count;
  return CRTGFX_OK;
}

crtgfx_result crtgfx_gpu_win32_device_create(uint32_t device_index, struct crtgfx_gpu_device* device) {
  crtgfx_dxgi_adapter1* adapters[CRTGFX_GPU_WIN32_MAX_ADAPTERS];
  uint32_t count = 0;
  crtgfx_result enum_result;
  crtgfx_d3d12_device* d3d_device = NULL;
  crtgfx_d3d12_command_queue* command_queue = NULL;
  crtgfx_d3d12_command_queue_desc queue_desc;
  HRESULT hr;
  uint32_t i;

  enum_result = crtgfx_gpu_win32_enumerate_ordered(adapters, &count);
  if (enum_result != CRTGFX_OK) {
    return CRTGFX_ERROR_UNSUPPORTED;
  }
  if (device_index >= count) {
    for (i = 0; i < count; ++i) {
      ((crtgfx_dxgi_unknown*)adapters[i])->lpVtbl->Release((crtgfx_dxgi_unknown*)adapters[i]);
    }
    /* device_index out of [0, device_count) -- crtgfx_gpu_query_
     * capabilities()'s own real, current report is the only valid source
     * for that range (gpu.h's own documented contract). */
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }

  hr = D3D12CreateDevice(
      adapters[device_index], CRTGFX_D3D_FEATURE_LEVEL_12_0, &crtgfx_iid_id3d12_device,
      (void**)&d3d_device);
  if (FAILED(hr) || d3d_device == NULL) {
    for (i = 0; i < count; ++i) {
      ((crtgfx_dxgi_unknown*)adapters[i])->lpVtbl->Release((crtgfx_dxgi_unknown*)adapters[i]);
    }
    return CRTGFX_ERROR_UNSUPPORTED;
  }

  queue_desc.Type = CRTGFX_D3D12_COMMAND_LIST_TYPE_DIRECT;
  queue_desc.Priority = 0;
  queue_desc.Flags = CRTGFX_D3D12_COMMAND_QUEUE_FLAG_NONE;
  queue_desc.NodeMask = 0;
  hr = d3d_device->lpVtbl->CreateCommandQueue(
      d3d_device, &queue_desc, &crtgfx_iid_id3d12_command_queue, (void**)&command_queue);
  if (FAILED(hr) || command_queue == NULL) {
    d3d_device->lpVtbl->Release(d3d_device);
    for (i = 0; i < count; ++i) {
      ((crtgfx_dxgi_unknown*)adapters[i])->lpVtbl->Release((crtgfx_dxgi_unknown*)adapters[i]);
    }
    return CRTGFX_ERROR_UNSUPPORTED;
  }

  /* Release every enumerated adapter except the one actually selected --
   * unlike gpu_vulkan.c's own device_create() (Vulkan physical devices are
   * never separately ref-counted), real DXGI adapter objects are real,
   * separately-refcounted COM objects that must each be released once
   * this function is done choosing among them. */
  for (i = 0; i < count; ++i) {
    if (i != device_index) {
      ((crtgfx_dxgi_unknown*)adapters[i])->lpVtbl->Release((crtgfx_dxgi_unknown*)adapters[i]);
    }
  }

  device->d3d12_device = (void*)d3d_device;
  device->d3d12_command_queue = (void*)command_queue;
  device->dxgi_adapter = (void*)adapters[device_index];
  return CRTGFX_OK;
}

void crtgfx_gpu_win32_device_destroy(struct crtgfx_gpu_device* device) {
  if (device->d3d12_command_queue != NULL) {
    ((crtgfx_dxgi_unknown*)device->d3d12_command_queue)
        ->lpVtbl->Release((crtgfx_dxgi_unknown*)device->d3d12_command_queue);
  }
  if (device->d3d12_device != NULL) {
    ((crtgfx_d3d12_device*)device->d3d12_device)->lpVtbl->Release((crtgfx_d3d12_device*)device->d3d12_device);
  }
  if (device->dxgi_adapter != NULL) {
    ((crtgfx_dxgi_unknown*)device->dxgi_adapter)
        ->lpVtbl->Release((crtgfx_dxgi_unknown*)device->dxgi_adapter);
  }
}

/* Real swap-chain/surface vertical slice (2026-09-07) -- see gpu_internal.h's
 * own comment on struct crtgfx_gpu_surface's Windows fields for the overall
 * shape. `hwnd_handle` is an already-resolved, real, live HWND (gpu.c's own
 * crtgfx_gpu_surface_create() calls crtgfx_win32_get_hwnd() before ever
 * reaching here) -- this function's only job is the real D3D12/DXGI side:
 * a swap chain sized to the window's own requested extent, its real back
 * buffers, an RTV descriptor heap so crtgfx_gpu_win32_surface_clear() can
 * target each one, and the per-frame command allocator/list/fence state
 * crtgfx_gpu_win32_surface_acquire()/_clear()/_present() (below) drive.
 * Single, linear `goto fail` teardown, matching gpu_vulkan.c's own
 * crtgfx_gpu_vulkan_surface_create() -- every handle is zero-initialized
 * up front so the fail: block can safely release exactly what was
 * actually created, in reverse order, regardless of which step failed. */
crtgfx_result crtgfx_gpu_win32_surface_create(
    struct crtgfx_gpu_device* device, void* hwnd_handle, uint32_t width, uint32_t height,
    struct crtgfx_gpu_surface* surface) {
  crtgfx_d3d12_device* d3d_device = (crtgfx_d3d12_device*)device->d3d12_device;
  crtgfx_d3d12_command_queue* command_queue = (crtgfx_d3d12_command_queue*)device->d3d12_command_queue;
  HWND hwnd = (HWND)hwnd_handle;
  /* Fixed at 2 -- matches window_win32.c's own D3D11 swap chain's own
   * BufferCount, a real, ordinary double-buffered flip-model swap chain;
   * no real reason for this vertical slice to make it configurable yet. */
  uint32_t buffer_count = 2u;
  crtgfx_dxgi_swap_chain_desc1 swap_desc;
  crtgfx_d3d12_descriptor_heap_desc heap_desc;
  crtgfx_d3d12_cpu_descriptor_handle rtv_start;
  size_t rtv_descriptor_size;
  HRESULT hr;
  uint32_t i;

  crtgfx_dxgi_factory2* factory = NULL;
  void* swap_chain_raw = NULL;
  crtgfx_dxgi_swapchain3* swap_chain3 = NULL;
  void** buffer_array = NULL;
  crtgfx_d3d12_descriptor_heap* rtv_heap = NULL;
  crtgfx_d3d12_command_allocator* command_allocator = NULL;
  crtgfx_d3d12_graphics_command_list* command_list = NULL;
  crtgfx_d3d12_fence* fence = NULL;
  uint64_t* fence_values = NULL;
  HANDLE fence_event = NULL;

  hr = CreateDXGIFactory1(&crtgfx_iid_idxgi_factory2, (void**)&factory);
  if (FAILED(hr) || factory == NULL) {
    goto fail;
  }

  swap_desc = (crtgfx_dxgi_swap_chain_desc1){0};
  swap_desc.Width = width;
  swap_desc.Height = height;
  swap_desc.Format = CRTGFX_DXGI_FORMAT_B8G8R8A8_UNORM;
  swap_desc.SampleDesc.Count = 1;
  swap_desc.BufferUsage = CRTGFX_DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swap_desc.BufferCount = buffer_count;
  swap_desc.Scaling = CRTGFX_DXGI_SCALING_STRETCH;
  swap_desc.SwapEffect = CRTGFX_DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  swap_desc.AlphaMode = CRTGFX_DXGI_ALPHA_MODE_IGNORE;
  swap_desc.Flags = 0;

  /* Real D3D12 requirement: `device` here is the command queue, not the
   * ID3D12Device -- see crtgfx_dxgi_factory2's own comment above. */
  hr = factory->lpVtbl->CreateSwapChainForHwnd(
      factory, (crtgfx_dxgi_unknown*)command_queue, hwnd, &swap_desc, NULL, NULL, &swap_chain_raw);
  if (FAILED(hr) || swap_chain_raw == NULL) {
    goto fail;
  }
  hr = ((crtgfx_dxgi_unknown*)swap_chain_raw)
           ->lpVtbl->QueryInterface(
               (crtgfx_dxgi_unknown*)swap_chain_raw, &crtgfx_iid_idxgi_swapchain3, (void**)&swap_chain3);
  ((crtgfx_dxgi_unknown*)swap_chain_raw)->lpVtbl->Release((crtgfx_dxgi_unknown*)swap_chain_raw);
  swap_chain_raw = NULL;
  if (FAILED(hr) || swap_chain3 == NULL) {
    goto fail;
  }

  buffer_array = (void**)crtgfx_win32_calloc(buffer_count, sizeof(void*));
  if (buffer_array == NULL) {
    goto fail;
  }
  for (i = 0; i < buffer_count; ++i) {
    crtgfx_d3d12_resource* buffer = NULL;
    hr = swap_chain3->lpVtbl->GetBuffer(swap_chain3, i, &crtgfx_iid_id3d12_resource, (void**)&buffer);
    if (FAILED(hr) || buffer == NULL) {
      goto fail;
    }
    buffer_array[i] = (void*)buffer;
  }

  heap_desc.Type = CRTGFX_D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  heap_desc.NumDescriptors = buffer_count;
  heap_desc.Flags = CRTGFX_D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  heap_desc.NodeMask = 0;
  hr = d3d_device->lpVtbl->CreateDescriptorHeap(
      d3d_device, &heap_desc, &crtgfx_iid_id3d12_descriptor_heap, (void**)&rtv_heap);
  if (FAILED(hr) || rtv_heap == NULL) {
    goto fail;
  }
  rtv_descriptor_size = (size_t)d3d_device->lpVtbl->GetDescriptorHandleIncrementSize(
      d3d_device, CRTGFX_D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  rtv_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(rtv_heap, &rtv_start);
  for (i = 0; i < buffer_count; ++i) {
    crtgfx_d3d12_cpu_descriptor_handle handle;
    handle.ptr = rtv_start.ptr + (size_t)i * rtv_descriptor_size;
    /* pDesc = NULL: real, documented D3D12 shortcut valid whenever the
     * resource's own real format is already fully typed (never
     * TYPELESS) -- true here, this swap chain was created with a real,
     * concrete DXGI_FORMAT_B8G8R8A8_UNORM, not a typeless one. */
    d3d_device->lpVtbl->CreateRenderTargetView(d3d_device, (crtgfx_d3d12_resource*)buffer_array[i], NULL, handle);
  }

  hr = d3d_device->lpVtbl->CreateCommandAllocator(
      d3d_device, CRTGFX_D3D12_COMMAND_LIST_TYPE_DIRECT, &crtgfx_iid_id3d12_command_allocator,
      (void**)&command_allocator);
  if (FAILED(hr) || command_allocator == NULL) {
    goto fail;
  }
  hr = d3d_device->lpVtbl->CreateCommandList(
      d3d_device, 0, CRTGFX_D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocator, NULL,
      &crtgfx_iid_id3d12_graphics_command_list, (void**)&command_list);
  if (FAILED(hr) || command_list == NULL) {
    goto fail;
  }
  /* Real D3D12 convention: a freshly-created command list starts OPEN
   * (recording) -- close it immediately so crtgfx_gpu_win32_surface_
   * acquire()'s own Reset() call (which requires a closed list) is
   * correct on the very first frame too, not just subsequent ones. */
  hr = command_list->lpVtbl->Close(command_list);
  if (FAILED(hr)) {
    goto fail;
  }

  hr = d3d_device->lpVtbl->CreateFence(
      d3d_device, 0, CRTGFX_D3D12_FENCE_FLAG_NONE, &crtgfx_iid_id3d12_fence, (void**)&fence);
  if (FAILED(hr) || fence == NULL) {
    goto fail;
  }
  fence_event = CreateEventW(NULL, 0, 0, NULL);
  if (fence_event == NULL) {
    goto fail;
  }
  fence_values = (uint64_t*)crtgfx_win32_calloc(buffer_count, sizeof(uint64_t));
  if (fence_values == NULL) {
    goto fail;
  }

  surface->device = device;
  surface->dxgi_swapchain = (void*)swap_chain3;
  surface->d3d12_back_buffers = buffer_array;
  surface->d3d12_rtv_heap = (void*)rtv_heap;
  surface->d3d12_rtv_descriptor_size = rtv_descriptor_size;
  surface->d3d12_buffer_count = buffer_count;
  surface->width = width;
  surface->height = height;
  surface->d3d12_command_allocator = (void*)command_allocator;
  surface->d3d12_command_list = (void*)command_list;
  surface->d3d12_fence = (void*)fence;
  surface->d3d12_fence_values = fence_values;
  surface->d3d12_fence_next_value = 1;
  surface->d3d12_fence_event = fence_event;
  surface->d3d12_current_buffer_index = 0;
  surface->d3d12_image_acquired = 0;

  ((crtgfx_dxgi_unknown*)factory)->lpVtbl->Release((crtgfx_dxgi_unknown*)factory);
  return CRTGFX_OK;

fail:
  if (fence_values != NULL) {
    crtgfx_win32_free(fence_values);
  }
  if (fence_event != NULL) {
    CloseHandle(fence_event);
  }
  if (fence != NULL) {
    ((crtgfx_dxgi_unknown*)fence)->lpVtbl->Release((crtgfx_dxgi_unknown*)fence);
  }
  if (command_list != NULL) {
    ((crtgfx_dxgi_unknown*)command_list)->lpVtbl->Release((crtgfx_dxgi_unknown*)command_list);
  }
  if (command_allocator != NULL) {
    ((crtgfx_dxgi_unknown*)command_allocator)->lpVtbl->Release((crtgfx_dxgi_unknown*)command_allocator);
  }
  if (rtv_heap != NULL) {
    ((crtgfx_dxgi_unknown*)rtv_heap)->lpVtbl->Release((crtgfx_dxgi_unknown*)rtv_heap);
  }
  if (buffer_array != NULL) {
    for (i = 0; i < buffer_count; ++i) {
      if (buffer_array[i] != NULL) {
        ((crtgfx_dxgi_unknown*)buffer_array[i])->lpVtbl->Release((crtgfx_dxgi_unknown*)buffer_array[i]);
      }
    }
    crtgfx_win32_free(buffer_array);
  }
  if (swap_chain3 != NULL) {
    ((crtgfx_dxgi_unknown*)swap_chain3)->lpVtbl->Release((crtgfx_dxgi_unknown*)swap_chain3);
  } else if (swap_chain_raw != NULL) {
    ((crtgfx_dxgi_unknown*)swap_chain_raw)->lpVtbl->Release((crtgfx_dxgi_unknown*)swap_chain_raw);
  }
  if (factory != NULL) {
    ((crtgfx_dxgi_unknown*)factory)->lpVtbl->Release((crtgfx_dxgi_unknown*)factory);
  }
  return CRTGFX_ERROR_UNSUPPORTED;
}

void crtgfx_gpu_win32_surface_destroy(struct crtgfx_gpu_surface* surface) {
  uint32_t i;

  if (surface->device == NULL) {
    return;
  }
  /* Real requirement, same reasoning as gpu_vulkan.c's own crtgfx_gpu_
   * vulkan_surface_destroy(): every one of these real GPU objects must
   * not be destroyed while the GPU may still be using them. D3D12 has no
   * single "wait idle" call the way Vulkan's vkDeviceWaitIdle()/D3D11's
   * ID3D11DeviceContext::Flush() do -- the real, standard idiom is a
   * fence signal immediately followed by a synchronous wait on it. */
  if (surface->d3d12_fence != NULL && surface->device->d3d12_command_queue != NULL) {
    crtgfx_d3d12_fence* fence = (crtgfx_d3d12_fence*)surface->d3d12_fence;
    crtgfx_d3d12_command_queue* queue = (crtgfx_d3d12_command_queue*)surface->device->d3d12_command_queue;
    uint64_t wait_value = surface->d3d12_fence_next_value;

    if (SUCCEEDED(queue->lpVtbl->Signal(queue, fence, wait_value)) &&
        fence->lpVtbl->GetCompletedValue(fence) < wait_value && surface->d3d12_fence_event != NULL) {
      if (SUCCEEDED(fence->lpVtbl->SetEventOnCompletion(fence, wait_value, (HANDLE)surface->d3d12_fence_event))) {
        while (fence->lpVtbl->GetCompletedValue(fence) < wait_value) {
          if (WaitForSingleObject((HANDLE)surface->d3d12_fence_event, 0xffffffffu) != 0u) break;
        }
      }
    }
  }

  if (surface->d3d12_fence_values != NULL) {
    crtgfx_win32_free(surface->d3d12_fence_values);
  }
  if (surface->d3d12_fence_event != NULL) {
    CloseHandle((HANDLE)surface->d3d12_fence_event);
  }
  if (surface->d3d12_fence != NULL) {
    ((crtgfx_dxgi_unknown*)surface->d3d12_fence)->lpVtbl->Release((crtgfx_dxgi_unknown*)surface->d3d12_fence);
  }
  if (surface->d3d12_command_list != NULL) {
    ((crtgfx_dxgi_unknown*)surface->d3d12_command_list)
        ->lpVtbl->Release((crtgfx_dxgi_unknown*)surface->d3d12_command_list);
  }
  if (surface->d3d12_command_allocator != NULL) {
    ((crtgfx_dxgi_unknown*)surface->d3d12_command_allocator)
        ->lpVtbl->Release((crtgfx_dxgi_unknown*)surface->d3d12_command_allocator);
  }
  if (surface->d3d12_rtv_heap != NULL) {
    ((crtgfx_dxgi_unknown*)surface->d3d12_rtv_heap)->lpVtbl->Release((crtgfx_dxgi_unknown*)surface->d3d12_rtv_heap);
  }
  if (surface->d3d12_back_buffers != NULL) {
    for (i = 0; i < surface->d3d12_buffer_count; ++i) {
      if (surface->d3d12_back_buffers[i] != NULL) {
        ((crtgfx_dxgi_unknown*)surface->d3d12_back_buffers[i])
            ->lpVtbl->Release((crtgfx_dxgi_unknown*)surface->d3d12_back_buffers[i]);
      }
    }
    crtgfx_win32_free(surface->d3d12_back_buffers);
  }
  if (surface->dxgi_swapchain != NULL) {
    ((crtgfx_dxgi_unknown*)surface->dxgi_swapchain)->lpVtbl->Release((crtgfx_dxgi_unknown*)surface->dxgi_swapchain);
  }
}

crtgfx_result crtgfx_gpu_win32_surface_acquire(struct crtgfx_gpu_surface* surface, uint64_t timeout_us) {
  crtgfx_dxgi_swapchain3* swap_chain = (crtgfx_dxgi_swapchain3*)surface->dxgi_swapchain;
  crtgfx_d3d12_fence* fence = (crtgfx_d3d12_fence*)surface->d3d12_fence;
  crtgfx_d3d12_command_allocator* allocator = (crtgfx_d3d12_command_allocator*)surface->d3d12_command_allocator;
  crtgfx_d3d12_graphics_command_list* command_list =
      (crtgfx_d3d12_graphics_command_list*)surface->d3d12_command_list;
  uint32_t index;
  uint64_t wait_value;
  HRESULT hr;

  if (surface->d3d12_image_acquired) {
    /* Same real, honest misuse guard as gpu_vulkan.c's own crtgfx_gpu_
     * vulkan_surface_acquire(). */
    return CRTGFX_ERROR_HOST;
  }

  /* DXGI's current index is nonblocking. Allocator reuse is gated by
   * the shared submission fence below, regardless of the image index. */
  index = swap_chain->lpVtbl->GetCurrentBackBufferIndex(swap_chain);
  if (index >= surface->d3d12_buffer_count) {
    return CRTGFX_ERROR_HOST;
  }

  /* All images share one allocator: wait for its latest submission, not
   * just the last use of the newly selected back buffer. */
  wait_value = surface->d3d12_fence_next_value - 1;
  if (fence->lpVtbl->GetCompletedValue(fence) == UINT64_MAX) {
    return CRTGFX_ERROR_HOST;
  }
  if (wait_value != 0 && fence->lpVtbl->GetCompletedValue(fence) < wait_value) {
    if (surface->d3d12_fence_event == NULL) {
      return CRTGFX_ERROR_HOST;
    }
    hr = fence->lpVtbl->SetEventOnCompletion(fence, wait_value, (HANDLE)surface->d3d12_fence_event);
    if (FAILED(hr)) {
      return CRTGFX_ERROR_HOST;
    }
    {
      uint64_t ms = timeout_us / 1000u + (timeout_us % 1000u != 0);
      uint64_t start = GetTickCount64();
      for (;;) {
        uint64_t completed = fence->lpVtbl->GetCompletedValue(fence);
        uint64_t elapsed = GetTickCount64() - start;
        uint64_t remaining;
        DWORD wait_rc;
        if (completed == UINT64_MAX) return CRTGFX_ERROR_HOST;
        if (completed >= wait_value) break;
        if (elapsed >= ms) return CRTGFX_ERROR_TIMEOUT;
        remaining = ms - elapsed;
        wait_rc = WaitForSingleObject((HANDLE)surface->d3d12_fence_event,
            (DWORD)(remaining > 0xfffffffeu ? 0xfffffffeu : remaining));
        if (wait_rc != 0u && wait_rc != 258u) return CRTGFX_ERROR_HOST;
        /* A timed-out earlier acquire can leave a pending event signal.
         * Only fence completion authorizes allocator reuse, not a wakeup. */
      }
    }
  }

  hr = allocator->lpVtbl->Reset(allocator);
  if (FAILED(hr)) {
    return CRTGFX_ERROR_HOST;
  }
  hr = command_list->lpVtbl->Reset(command_list, allocator, NULL);
  if (FAILED(hr)) {
    return CRTGFX_ERROR_HOST;
  }

  surface->d3d12_current_buffer_index = index;
  surface->d3d12_image_acquired = 1;
  surface->d3d12_frame_submitted = 0;
  return CRTGFX_OK;
}

crtgfx_result crtgfx_gpu_win32_surface_clear(struct crtgfx_gpu_surface* surface, float r, float g, float b, float a) {
  crtgfx_d3d12_graphics_command_list* command_list =
      (crtgfx_d3d12_graphics_command_list*)surface->d3d12_command_list;
  crtgfx_d3d12_command_queue* queue = (crtgfx_d3d12_command_queue*)surface->device->d3d12_command_queue;
  crtgfx_d3d12_fence* fence = (crtgfx_d3d12_fence*)surface->d3d12_fence;
  uint32_t index = surface->d3d12_current_buffer_index;
  crtgfx_d3d12_resource* buffer = (crtgfx_d3d12_resource*)surface->d3d12_back_buffers[index];
  crtgfx_d3d12_cpu_descriptor_handle rtv;
  crtgfx_d3d12_resource_barrier to_render_target;
  crtgfx_d3d12_resource_barrier to_present;
  float color[4];
  uint64_t signal_value;
  HRESULT hr;

  if (!surface->d3d12_image_acquired || surface->d3d12_frame_submitted) {
    return CRTGFX_ERROR_HOST;
  }

  {
    crtgfx_d3d12_descriptor_heap* heap = (crtgfx_d3d12_descriptor_heap*)surface->d3d12_rtv_heap;
    crtgfx_d3d12_cpu_descriptor_handle heap_start;
    heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(heap, &heap_start);
    rtv.ptr = heap_start.ptr + (size_t)index * surface->d3d12_rtv_descriptor_size;
  }

  color[0] = r;
  color[1] = g;
  color[2] = b;
  color[3] = a;

  to_render_target.type = CRTGFX_D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  to_render_target.flags = CRTGFX_D3D12_RESOURCE_BARRIER_FLAG_NONE;
  to_render_target.transition.resource = buffer;
  to_render_target.transition.subresource = 0;
  to_render_target.transition.state_before = CRTGFX_D3D12_RESOURCE_STATE_PRESENT;
  to_render_target.transition.state_after = CRTGFX_D3D12_RESOURCE_STATE_RENDER_TARGET;

  to_present.type = CRTGFX_D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  to_present.flags = CRTGFX_D3D12_RESOURCE_BARRIER_FLAG_NONE;
  to_present.transition.resource = buffer;
  to_present.transition.subresource = 0;
  to_present.transition.state_before = CRTGFX_D3D12_RESOURCE_STATE_RENDER_TARGET;
  to_present.transition.state_after = CRTGFX_D3D12_RESOURCE_STATE_PRESENT;

  command_list->lpVtbl->ResourceBarrier(command_list, 1, &to_render_target);
  command_list->lpVtbl->OMSetRenderTargets(command_list, 1, &rtv, 0, NULL);
  command_list->lpVtbl->ClearRenderTargetView(command_list, rtv, color, 0, NULL);
  command_list->lpVtbl->ResourceBarrier(command_list, 1, &to_present);

  hr = command_list->lpVtbl->Close(command_list);
  if (FAILED(hr)) {
    return CRTGFX_ERROR_HOST;
  }

  queue->lpVtbl->ExecuteCommandLists(queue, 1, (void* const*)&command_list);

  signal_value = surface->d3d12_fence_next_value++;
  hr = queue->lpVtbl->Signal(queue, fence, signal_value);
  if (FAILED(hr)) {
    return CRTGFX_ERROR_HOST;
  }
  surface->d3d12_fence_values[index] = signal_value;
  surface->d3d12_frame_submitted = 1;
  return CRTGFX_OK;
}

/* Real swap-chain recreation (2026-09-07, closing the "resize on all GPU
 * hosts" gap this vertical slice's own follow-up work left open -- see
 * crtgfx/gpu.h's own crtgfx_gpu_surface_resize() comment for the full,
 * host-independent contract). D3D12/DXGI's own real, documented resize
 * idiom: every real reference to a back buffer (this file's own
 * ID3D12Resource* array and the RTV descriptor heap's own views onto
 * them) must be released before IDXGISwapChain3::ResizeBuffers() is
 * called -- unlike Vulkan's own vkCreateSwapchainKHR(..., oldSwapchain),
 * there is no "hand the old swap chain in, get a new one back" option
 * here; the *same* swap chain object is resized in place, so this
 * function reuses `surface->dxgi_swapchain`/`d3d12_rtv_heap` (already
 * sized to `d3d12_buffer_count`, which never changes) rather than
 * recreating either. Same real fence-signal-then-wait GPU idle pattern
 * crtgfx_gpu_win32_surface_destroy() already uses above (D3D12 has no
 * single wait-idle call) -- every real back-buffer resource must not be
 * released while the GPU may still be using it. */
crtgfx_result crtgfx_gpu_win32_surface_resize(struct crtgfx_gpu_surface* surface, uint32_t width, uint32_t height) {
  crtgfx_dxgi_swapchain3* swap_chain = (crtgfx_dxgi_swapchain3*)surface->dxgi_swapchain;
  crtgfx_d3d12_device* d3d_device = (crtgfx_d3d12_device*)surface->device->d3d12_device;
  crtgfx_d3d12_fence* fence = (crtgfx_d3d12_fence*)surface->d3d12_fence;
  crtgfx_d3d12_command_queue* queue = (crtgfx_d3d12_command_queue*)surface->device->d3d12_command_queue;
  crtgfx_d3d12_descriptor_heap* rtv_heap = (crtgfx_d3d12_descriptor_heap*)surface->d3d12_rtv_heap;
  crtgfx_d3d12_cpu_descriptor_handle rtv_start;
  size_t rtv_descriptor_size = surface->d3d12_rtv_descriptor_size;
  uint32_t buffer_count = surface->d3d12_buffer_count;
  uint32_t i;
  HRESULT hr;

  if (surface->d3d12_image_acquired) {
    /* Same real, honest misuse guard as every other out-of-order call this
     * contract already rejects -- recreating the swap chain while an
     * image is acquired is not real, defined D3D12 behavior. */
    return CRTGFX_ERROR_HOST;
  }
  if (width == surface->width && height == surface->height) {
    /* Real, cheap no-op -- see crtgfx/gpu.h's own comment on this
     * function. */
    return CRTGFX_OK;
  }

  /* Real GPU idle wait -- same fence-signal-then-wait pattern as crtgfx_
   * gpu_win32_surface_destroy() above (D3D12 has no single wait-idle
   * call). Every back buffer below must not be released while the GPU
   * may still be using it. */
  {
    uint64_t wait_value = surface->d3d12_fence_next_value++;
    if (FAILED(queue->lpVtbl->Signal(queue, fence, wait_value))) {
      return CRTGFX_ERROR_HOST;
    }
    if (fence->lpVtbl->GetCompletedValue(fence) < wait_value && surface->d3d12_fence_event != NULL) {
      if (FAILED(fence->lpVtbl->SetEventOnCompletion(fence, wait_value, (HANDLE)surface->d3d12_fence_event))) {
        return CRTGFX_ERROR_HOST;
      }
      while (fence->lpVtbl->GetCompletedValue(fence) < wait_value) {
        if (WaitForSingleObject((HANDLE)surface->d3d12_fence_event, 0xffffffffu) != 0u) break;
      }
    }
  }

  /* Release every real back-buffer reference (the RTV heap's own views
   * onto them go stale but need no explicit release -- CreateRenderTarget
   * View() below simply overwrites each descriptor in place, same as
   * crtgfx_gpu_win32_surface_create()'s own first-time fill). Leaves
   * `surface->d3d12_back_buffers[i]` NULL until re-fetched just below --
   * a real ResizeBuffers() failure after this point leaves the surface in
   * the same "safe to release, not otherwise usable" state crtgfx/gpu.h's
   * own crtgfx_gpu_surface_resize() comment documents, matching a failed
   * crtgfx_gpu_surface_create()'s own contract. */
  for (i = 0; i < buffer_count; ++i) {
    if (surface->d3d12_back_buffers[i] != NULL) {
      ((crtgfx_dxgi_unknown*)surface->d3d12_back_buffers[i])
          ->lpVtbl->Release((crtgfx_dxgi_unknown*)surface->d3d12_back_buffers[i]);
      surface->d3d12_back_buffers[i] = NULL;
    }
  }

  hr = swap_chain->lpVtbl->ResizeBuffers(
      swap_chain, buffer_count, width, height, CRTGFX_DXGI_FORMAT_B8G8R8A8_UNORM, 0);
  if (FAILED(hr)) {
    return CRTGFX_ERROR_HOST;
  }

  for (i = 0; i < buffer_count; ++i) {
    crtgfx_d3d12_resource* buffer = NULL;
    hr = swap_chain->lpVtbl->GetBuffer(swap_chain, i, &crtgfx_iid_id3d12_resource, (void**)&buffer);
    if (FAILED(hr) || buffer == NULL) {
      return CRTGFX_ERROR_HOST;
    }
    surface->d3d12_back_buffers[i] = (void*)buffer;
  }

  rtv_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(rtv_heap, &rtv_start);
  for (i = 0; i < buffer_count; ++i) {
    crtgfx_d3d12_cpu_descriptor_handle handle;
    handle.ptr = rtv_start.ptr + (size_t)i * rtv_descriptor_size;
    d3d_device->lpVtbl->CreateRenderTargetView(
        d3d_device, (crtgfx_d3d12_resource*)surface->d3d12_back_buffers[i], NULL, handle);
  }

  surface->width = width;
  surface->height = height;
  return CRTGFX_OK;
}

crtgfx_result crtgfx_gpu_win32_surface_present(struct crtgfx_gpu_surface* surface) {
  crtgfx_dxgi_swapchain3* swap_chain = (crtgfx_dxgi_swapchain3*)surface->dxgi_swapchain;
  HRESULT hr;

  if (!surface->d3d12_image_acquired || !surface->d3d12_frame_submitted) {
    return CRTGFX_ERROR_HOST;
  }
  /* SyncInterval=1, matching window_win32.c's own D3D11 swap chain's own
   * convention -- see that file's own crtgfx_host_window_present_
   * software() comment on DXGI_STATUS_OCCLUDED and friends being real,
   * normal, non-error outcomes, not failures. */
  hr = swap_chain->lpVtbl->Present(swap_chain, 1, 0);
  surface->d3d12_image_acquired = 0;
  if (FAILED(hr)) {
    return CRTGFX_ERROR_HOST;
  }
  return CRTGFX_OK;
}
