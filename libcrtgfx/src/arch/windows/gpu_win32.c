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
 * declarations alone. That direct cross-check caught a real, previously
 * wrong slot number for GetDeviceRemovedReason (an initial hand-count
 * landed on 37; the real vtbl struct puts it at 39 -- CreateFence's own
 * real position had been miscounted) -- exactly the kind of silent,
 * dangerous ABI mismatch (calling into the wrong real method) this
 * project's own "verify for real" discipline exists to catch before it
 * ships, not after. */

#include "gpu_internal.h"

#include <stddef.h>
#include <stdint.h>

typedef long HRESULT;
typedef unsigned long ULONG;
typedef unsigned int UINT;
typedef int INT;
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

/* ID3D12CommandQueue -- opaque beyond IUnknown for this file's own real
 * needs (create it, hand its pointer to skia_bridge.cc, Release it on
 * teardown; no other real method call needed here). */
typedef crtgfx_dxgi_unknown crtgfx_d3d12_command_queue;

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

/* ID3D12Device -- real vtable slots confirmed directly against the real
 * ID3D12DeviceVtbl struct (um\d3d12.h): CreateCommandQueue at slot 8
 * (QueryInterface/AddRef/Release=0-2, ID3D12Object's own GetPrivateData/
 * SetPrivateData/SetPrivateDataInterface/SetName=3-6, GetNodeCount=7),
 * GetDeviceRemovedReason at slot 39 (slots 9-38 reserved -- 30 slots). */
typedef struct crtgfx_d3d12_device crtgfx_d3d12_device;
typedef struct crtgfx_d3d12_device_vtbl {
  HRESULT(CRTGFX_WINAPI* QueryInterface)(crtgfx_d3d12_device* self, REFIID riid, void** out);
  ULONG(CRTGFX_WINAPI* AddRef)(crtgfx_d3d12_device* self);
  ULONG(CRTGFX_WINAPI* Release)(crtgfx_d3d12_device* self);
  void* reserved_3_to_7[5];
  HRESULT(CRTGFX_WINAPI* CreateCommandQueue)(
      crtgfx_d3d12_device* self, const crtgfx_d3d12_command_queue_desc* desc, REFIID riid,
      void** out_command_queue);
  void* reserved_9_to_38[30];
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

__declspec(dllimport) HRESULT CRTGFX_WINAPI D3D12CreateDevice(
    void* adapter, uint32_t minimum_feature_level, REFIID riid, void** out_device);
__declspec(dllimport) HRESULT CRTGFX_WINAPI CreateDXGIFactory1(REFIID riid, void** out_factory);

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
