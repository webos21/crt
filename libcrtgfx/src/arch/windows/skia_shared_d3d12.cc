#include "crtgfx/skia_shared.h"

/* D3D12 shared-image backend for Web Tranche 9B (Windows). The sibling of
 * src/arch/linux/skia_shared_vulkan.cc behind the same crtgfx_skia_shared_image
 * interface.
 *
 *  - Memory: one committed texture in a shared heap (D3D12_HEAP_FLAG_SHARED),
 *    created with ALLOW_RENDER_TARGET | ALLOW_SIMULTANEOUS_ACCESS so two
 *    processes (two devices) can use it without an ownership-transfer protocol
 *    of image layouts. It is shared as an NT handle (CreateSharedHandle) and
 *    opened by the peer with OpenSharedHandle on the same adapter.
 *  - Synchronization: two shared ID3D12Fence objects, "ready" (producer
 *    signals after writing) and "release" (consumer signals after reading).
 *    A D3D12 fence is both the GPU wait (queue Wait, through Ganesh) and the
 *    CPU wait (SetEventOnCompletion), and its value only grows, so an epoch is
 *    a number and a stale signal cannot satisfy a later epoch: no fence reset
 *    exists to forget. Both sides count epochs in lockstep, one per frame.
 *  - State: Ganesh transitions a wrapped texture as it likes. Before ownership
 *    moves, this file records an explicit barrier from the state Ganesh reports
 *    back to COMMON on the same direct queue and tells Ganesh. D3D12 has no
 *    flush-time mutable state like Vulkan (see skia_bridge.cc).
 *  - Handles cross processes as CRT descriptors: NT handles adopted with
 *    __crt_windows_fd_adopt_handle(), which the CRT's SCM_RIGHTS duplicates into
 *    the peer exactly as it does a file.
 *
 * No D3D12 or Skia object crosses IPC. The producer and consumer never use the
 * texture together: ownership is the fence epochs. */
#if defined(CRTGFX_HAVE_D3D12)
#include "../../gpu_internal.h"

#include "include/core/SkColorSpace.h"
#include "include/gpu/ganesh/GrBackendSemaphore.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkImageGanesh.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/d3d/GrD3DBackendSemaphore.h"
#include "include/gpu/ganesh/d3d/GrD3DBackendSurface.h"
#include "include/gpu/ganesh/d3d/GrD3DTypes.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>

extern "C" {
/* libc/include/private/crt_fd_table.h (Windows-only PAL hooks). */
uintptr_t __crt_windows_fd_get_handle(int fd);
int __crt_windows_fd_adopt_handle(uintptr_t handle);
}

namespace {
constexpr DXGI_FORMAT kFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
constexpr D3D12_RESOURCE_FLAGS kFlags =
    D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;

// IIDs by value: the project does not link dxguid.lib (see skia_bridge.cc).
const GUID kIidResource = {0x696442be, 0xa72e, 0x4059, {0xbc, 0x79, 0x5b, 0x5c, 0x98, 0x04, 0x0f, 0xad}};
const GUID kIidFence = {0x0a753dcf, 0xc4d8, 0x4b91, {0xad, 0xf6, 0xbe, 0x5a, 0x60, 0xd9, 0x5a, 0x76}};
const GUID kIidAllocator = {0x6102dee4, 0xaf59, 0x4b09, {0xb9, 0x99, 0xb4, 0x4d, 0x73, 0xf0, 0x9b, 0x24}};
const GUID kIidCommandList = {0x5b160d0f, 0xac1b, 0x4185, {0x8b, 0xa8, 0xb3, 0xae, 0x42, 0xa5, 0xa4, 0x55}};

struct Device {
  crtgfx_gpu_win32_device_view view {};
  ID3D12Device* device = nullptr;
  ID3D12CommandQueue* queue = nullptr;
  const crtgfx_gpu_device* source = nullptr;
  bool init(const crtgfx_gpu_device* from) {
    source = from;
    if (!crtgfx_gpu_win32_borrow_device(from, &view)) return false;
    device = reinterpret_cast<ID3D12Device*>(view.device);
    queue = reinterpret_cast<ID3D12CommandQueue*>(view.command_queue);
    return device && queue;
  }
};

D3D12_RESOURCE_DESC texture_desc(uint32_t width, uint32_t height) {
  D3D12_RESOURCE_DESC desc = {};
  desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  desc.Width = width;
  desc.Height = height;
  desc.DepthOrArraySize = 1;
  desc.MipLevels = 1;
  desc.Format = kFormat;
  desc.SampleDesc.Count = 1;
  desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  desc.Flags = kFlags;
  return desc;
}

D3D12_HEAP_PROPERTIES default_heap() {
  D3D12_HEAP_PROPERTIES heap = {};
  heap.Type = D3D12_HEAP_TYPE_DEFAULT;
  heap.CreationNodeMask = 1;
  heap.VisibleNodeMask = 1;
  return heap;
}

bool create_shared_texture(ID3D12Device* device, uint32_t width, uint32_t height, gr_cp<ID3D12Resource>* out) {
  D3D12_HEAP_PROPERTIES heap = default_heap();
  D3D12_RESOURCE_DESC desc = texture_desc(width, height);
  return SUCCEEDED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_SHARED, &desc,
      D3D12_RESOURCE_STATE_COMMON, nullptr, kIidResource, reinterpret_cast<void**>(&(*out))));
}

// Whether this adapter can create the shapes below and hand them out as NT handles; checks the real
// driver with the smallest real objects instead of trusting a feature bit.
bool probe_sharing(Device& d) {
  gr_cp<ID3D12Resource> texture;
  gr_cp<ID3D12Fence> fence;
  HANDLE handle = nullptr;
  if (!create_shared_texture(d.device, 16, 16, std::addressof(texture))) return false;
  if (FAILED(d.device->CreateSharedHandle(texture.get(), nullptr, GENERIC_ALL, nullptr, &handle)) || !handle) return false;
  CloseHandle(handle);
  handle = nullptr;
  if (FAILED(d.device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, kIidFence, reinterpret_cast<void**>(&fence)))) return false;
  if (FAILED(d.device->CreateSharedHandle(fence.get(), nullptr, GENERIC_ALL, nullptr, &handle)) || !handle) return false;
  CloseHandle(handle);
  return true;
}

uint64_t allocation_size(ID3D12Device* device, uint32_t width, uint32_t height) {
  D3D12_RESOURCE_DESC desc = texture_desc(width, height);
  return device->GetResourceAllocationInfo(0, 1, &desc).SizeInBytes;
}

crtgfx_result query(Device& d, crtgfx_shared_image_desc* out, uint32_t width = 0, uint32_t height = 0) {
  // The adapter identity comes from gpu_win32.c: this translation unit's 4-byte wchar_t would give
  // DXGI_ADAPTER_DESC1 a layout the driver does not fill (all fields read as zero).
  unsigned char device_id[16] = {}, driver_id[16] = {};
  if (!crtgfx_gpu_win32_adapter_identity(d.source, device_id, driver_id)) return CRTGFX_ERROR_UNSUPPORTED;
  if (width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION || height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION)
    return CRTGFX_ERROR_UNSUPPORTED;
  if (!probe_sharing(d)) return CRTGFX_ERROR_UNSUPPORTED;
  *out = {};
  out->version = CRTGFX_SHARED_IMAGE_VERSION;
  out->backend = CRTGFX_GPU_BACKEND_D3D12;
  out->handle_kind = CRTGFX_SHARED_IMAGE_NT_HANDLE;
  out->format = CRTGFX_SHARED_IMAGE_BGRA8;
  // LUID (system-wide until the next boot) + vendor + device id; user-mode driver version + subsystem +
  // revision, so both sides must run the same driver build.
  memcpy(out->device_uuid, device_id, sizeof out->device_uuid);
  memcpy(out->driver_uuid, driver_id, sizeof out->driver_uuid);
  return CRTGFX_OK;
}

class SharedImage final : public crtgfx_skia_shared_image {
 public:
  Device d;
  crtgfx_shared_image_desc desc {};
  gr_cp<ID3D12Resource> resource;
  gr_cp<ID3D12Fence> ready, release;
  gr_cp<ID3D12CommandAllocator> allocator;
  gr_cp<ID3D12GraphicsCommandList> list;
  GrBackendTexture texture;
  GrDirectContext* context = nullptr;
  uint64_t epochs = 0;           // producer: completed writes; consumer: acquired reads
  uint64_t pending_value = 0;    // last barrier-list signal, until which the allocator is busy
  ID3D12Fence* pending_fence = nullptr;
  bool producer = false, writing = false, reading = false, poisoned = false;

  ~SharedImage() override {
    if (!d.device) return;
    // Callers drop their Skia references first. Wait (bounded) for our own last queue signal so the
    // objects are not released under work still using them; the peer holds independent references.
    if (pending_fence) cpu_wait(pending_fence, pending_value, 2000);
    if (producer && ready && epochs) cpu_wait(ready.get(), epochs, 2000);
  }
  const crtgfx_shared_image_desc& description() const override { return desc; }

  // Fresh event per wait: an event left registered by a timed-out wait could fire into a later one.
  crtgfx_result cpu_wait(ID3D12Fence* fence, uint64_t value, uint32_t timeout_ms) {
    uint64_t completed = fence->GetCompletedValue();
    if (completed == UINT64_MAX) return CRTGFX_ERROR_HOST;  // device removed
    if (completed >= value) return CRTGFX_OK;
    if (!timeout_ms) return CRTGFX_ERROR_TIMEOUT;
    HANDLE event = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    if (!event) return CRTGFX_ERROR_HOST;
    crtgfx_result result = CRTGFX_ERROR_HOST;
    if (SUCCEEDED(fence->SetEventOnCompletion(value, event))) {
      DWORD waited = WaitForSingleObject(event, timeout_ms);
      result = waited == WAIT_OBJECT_0 ? CRTGFX_OK : waited == WAIT_TIMEOUT ? CRTGFX_ERROR_TIMEOUT : CRTGFX_ERROR_HOST;
    }
    CloseHandle(event);
    return result;
  }

  crtgfx_result make_objects() {
    if (FAILED(d.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, kIidAllocator,
            reinterpret_cast<void**>(&allocator))))
      return CRTGFX_ERROR_HOST;
    if (FAILED(d.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.get(), nullptr,
            kIidCommandList, reinterpret_cast<void**>(&list))))
      return CRTGFX_ERROR_HOST;
    list->Close();
    return CRTGFX_OK;
  }

  crtgfx_result create(uint32_t width, uint32_t height) {
    desc.width = width;
    desc.height = height;
    if (!create_shared_texture(d.device, width, height, std::addressof(resource))) return CRTGFX_ERROR_UNSUPPORTED;
    desc.allocation_size = allocation_size(d.device, width, height);
    if (!crtgfx_shared_image_desc_valid(desc)) return CRTGFX_ERROR_UNSUPPORTED;
    gr_cp<ID3D12Fence>* fences[2] = {std::addressof(ready), std::addressof(release)};
    for (gr_cp<ID3D12Fence>* fence : fences)
      if (FAILED(d.device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, kIidFence, reinterpret_cast<void**>(&(*fence)))))
        return CRTGFX_ERROR_UNSUPPORTED;
    return make_objects();
  }

  // The native objects of an exporting peer, opened on this device. The caller's descriptors stay open.
  crtgfx_result open(const int* fds) {
    HANDLE handles[CRTGFX_SHARED_IMAGE_D3D12_HANDLE_COUNT];
    for (unsigned i = 0; i < CRTGFX_SHARED_IMAGE_D3D12_HANDLE_COUNT; ++i) {
      handles[i] = reinterpret_cast<HANDLE>(__crt_windows_fd_get_handle(fds[i]));
      if (!handles[i]) return CRTGFX_ERROR_INVALID_ARGUMENT;
    }
    if (FAILED(d.device->OpenSharedHandle(handles[0], kIidResource, reinterpret_cast<void**>(&resource))))
      return CRTGFX_ERROR_UNSUPPORTED;
    D3D12_RESOURCE_DESC opened = resource->GetDesc();
    if (opened.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || opened.Width != desc.width
        || opened.Height != desc.height || opened.Format != kFormat || opened.MipLevels != 1
        || opened.DepthOrArraySize != 1 || opened.SampleDesc.Count != 1 || (opened.Flags & kFlags) != kFlags)
      return CRTGFX_ERROR_INVALID_ARGUMENT;
    if (FAILED(d.device->OpenSharedHandle(handles[1], kIidFence, reinterpret_cast<void**>(&ready)))
        || FAILED(d.device->OpenSharedHandle(handles[2], kIidFence, reinterpret_cast<void**>(&release))))
      return CRTGFX_ERROR_UNSUPPORTED;
    return make_objects();
  }

  crtgfx_result export_handles(int out[CRTGFX_SHARED_IMAGE_HANDLE_COUNT]) override {
    if (!out) return CRTGFX_ERROR_INVALID_ARGUMENT;
    for (unsigned i = 0; i < CRTGFX_SHARED_IMAGE_HANDLE_COUNT; ++i) out[i] = -1;
    if (!producer) return CRTGFX_ERROR_INVALID_ARGUMENT;
    ID3D12DeviceChild* objects[CRTGFX_SHARED_IMAGE_D3D12_HANDLE_COUNT] = {resource.get(), ready.get(), release.get()};
    bool ok = true;
    for (unsigned i = 0; ok && i < CRTGFX_SHARED_IMAGE_D3D12_HANDLE_COUNT; ++i) {
      HANDLE handle = nullptr;
      ok = SUCCEEDED(d.device->CreateSharedHandle(objects[i], nullptr, GENERIC_ALL, nullptr, &handle)) && handle;
      if (!ok) break;
      out[i] = __crt_windows_fd_adopt_handle(reinterpret_cast<uintptr_t>(handle));
      if (out[i] < 0) { CloseHandle(handle); ok = false; }
    }
    if (!ok) {
      for (unsigned i = 0; i < CRTGFX_SHARED_IMAGE_HANDLE_COUNT; ++i) {
        if (out[i] >= 0) CloseHandle(reinterpret_cast<HANDLE>(__crt_windows_fd_get_handle(out[i])));
        out[i] = -1;
      }
      return CRTGFX_ERROR_HOST;
    }
    return CRTGFX_OK;
  }

  // Producer: has the consumer released every epoch written so far? Consumer: has the producer made the
  // next epoch ready? A fence that has never been signalled is "not yet" in both, which is why the target
  // is never zero.
  crtgfx_result wait_ready(uint32_t timeout_ms) override {
    return cpu_wait(ready.get(), producer ? std::max<uint64_t>(epochs, 1) : epochs + 1, timeout_ms);
  }
  crtgfx_result wait_released(uint32_t timeout_ms) override {
    return cpu_wait(release.get(), std::max<uint64_t>(epochs, 1), timeout_ms);
  }

  void make_texture() {
    // Default-construct and retain(): the pointer constructor ADOPTS its argument without an AddRef, which
    // released our resource when the info went out of scope.
    GrD3DTextureResourceInfo info;
    info.fResource.retain(resource.get());
    info.fResourceState = D3D12_RESOURCE_STATE_COMMON;
    info.fFormat = kFormat;
    info.fSampleCount = 1;
    info.fLevelCount = 1;
    texture = GrBackendTextures::MakeD3D(static_cast<int>(desc.width), static_cast<int>(desc.height), info);
  }
  bool bind_context(GrDirectContext* ctx) {
    if (!ctx || ctx->abandoned() || poisoned || (context && context != ctx)) return false;
    context = ctx;
    return true;
  }
  bool gpu_wait(GrDirectContext* ctx, ID3D12Fence* fence, uint64_t value) {
    GrD3DFenceInfo info;
    info.fFence.retain(fence);
    info.fValue = value;
    GrBackendSemaphore semaphore = GrBackendSemaphores::MakeD3D(info);
    return ctx->wait(1, &semaphore, false);
  }

  // After Ganesh's work is submitted: move the texture to COMMON on the same queue, then signal. Ownership
  // reaches the peer in the state the peer's own device assumes.
  crtgfx_result hand_over(ID3D12Fence* fence, uint64_t value) {
    if (pending_fence && cpu_wait(pending_fence, pending_value, 2000) != CRTGFX_OK) { poisoned = true; return CRTGFX_ERROR_HOST; }
    D3D12_RESOURCE_STATES before = GrBackendTextures::GetD3DTextureResourceInfo(texture).fResourceState;
    if (before != D3D12_RESOURCE_STATE_COMMON) {
      D3D12_RESOURCE_BARRIER barrier = {};
      barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      barrier.Transition.pResource = resource.get();
      barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
      barrier.Transition.StateBefore = before;
      barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
      if (FAILED(allocator->Reset()) || FAILED(list->Reset(allocator.get(), nullptr))) { poisoned = true; return CRTGFX_ERROR_HOST; }
      list->ResourceBarrier(1, &barrier);
      if (FAILED(list->Close())) { poisoned = true; return CRTGFX_ERROR_HOST; }
      ID3D12CommandList* lists[] = {list.get()};
      d.queue->ExecuteCommandLists(1, lists);
      GrBackendTextures::SetD3DResourceState(&texture, D3D12_RESOURCE_STATE_COMMON);
    }
    if (FAILED(d.queue->Signal(fence, value))) { poisoned = true; return CRTGFX_ERROR_HOST; }
    pending_fence = fence;
    pending_value = value;
    return CRTGFX_OK;
  }

  crtgfx_result begin_write(GrDirectContext* ctx, uint32_t timeout_ms, sk_sp<SkSurface>* out) override {
    if (!out) return CRTGFX_ERROR_INVALID_ARGUMENT;
    out->reset();
    if (!producer || writing || reading || !bind_context(ctx)) return CRTGFX_ERROR_INVALID_ARGUMENT;
    if (epochs) {
      // The previous epoch must have been released by the consumer's GPU work before it is overwritten.
      crtgfx_result result = cpu_wait(release.get(), epochs, timeout_ms);
      if (result != CRTGFX_OK) return result;
      if (!gpu_wait(ctx, release.get(), epochs)) return CRTGFX_ERROR_HOST;
    }
    make_texture();
    *out = SkSurfaces::WrapBackendTexture(ctx, texture, kTopLeft_GrSurfaceOrigin, 1, kBGRA_8888_SkColorType,
        nullptr, nullptr);
    writing = *out != nullptr;
    return writing ? CRTGFX_OK : CRTGFX_ERROR_HOST;
  }
  crtgfx_result end_write(GrDirectContext* ctx, SkSurface* surface) override {
    if (!writing || !surface || surface->recordingContext() != ctx || !bind_context(ctx)) return CRTGFX_ERROR_INVALID_ARGUMENT;
    ctx->flush(surface);
    writing = false;
    if (!ctx->submit(GrSyncCpu::kNo)) { poisoned = true; return CRTGFX_ERROR_HOST; }
    crtgfx_result result = hand_over(ready.get(), epochs + 1);
    if (result == CRTGFX_OK) ++epochs;
    return result;
  }
  crtgfx_result begin_read(GrDirectContext* ctx, uint32_t timeout_ms, sk_sp<SkImage>* out) override {
    if (!out) return CRTGFX_ERROR_INVALID_ARGUMENT;
    out->reset();
    if (producer || writing || reading || !bind_context(ctx)) return CRTGFX_ERROR_INVALID_ARGUMENT;
    crtgfx_result result = cpu_wait(ready.get(), epochs + 1, timeout_ms);
    if (result != CRTGFX_OK) return result;
    ++epochs;  // this epoch is now ours; a second begin_read needs a newer signal
    if (!gpu_wait(ctx, ready.get(), epochs)) { poisoned = true; return CRTGFX_ERROR_HOST; }
    make_texture();
    *out = SkImages::BorrowTextureFrom(ctx, texture, kTopLeft_GrSurfaceOrigin, kBGRA_8888_SkColorType,
        kPremul_SkAlphaType, nullptr);
    reading = *out != nullptr;
    return reading ? CRTGFX_OK : CRTGFX_ERROR_HOST;
  }
  crtgfx_result end_read(GrDirectContext* ctx) override {
    if (!reading || !bind_context(ctx)) return CRTGFX_ERROR_INVALID_ARGUMENT;
    ctx->flush();
    reading = false;
    if (!ctx->submit(GrSyncCpu::kNo)) { poisoned = true; return CRTGFX_ERROR_HOST; }
    return hand_over(release.get(), epochs);
  }
};
}  // namespace

crtgfx_result crtgfx_skia_shared_query(const crtgfx_gpu_device* device, crtgfx_shared_image_desc* out) {
  if (!device || !out) return CRTGFX_ERROR_INVALID_ARGUMENT;
  *out = {};
  Device d;
  return d.init(device) ? query(d, out) : CRTGFX_ERROR_UNSUPPORTED;
}

crtgfx_result crtgfx_skia_shared_create(const crtgfx_gpu_device* device, uint32_t width, uint32_t height,
    std::unique_ptr<crtgfx_skia_shared_image>* out) {
  if (!out) return CRTGFX_ERROR_INVALID_ARGUMENT;
  out->reset();
  if (!device || !width || !height || width > CRTGFX_SHARED_IMAGE_MAX_EXTENT || height > CRTGFX_SHARED_IMAGE_MAX_EXTENT)
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  std::unique_ptr<SharedImage> image(new SharedImage);
  image->producer = true;
  if (!image->d.init(device)) return CRTGFX_ERROR_UNSUPPORTED;
  crtgfx_result result = query(image->d, &image->desc, width, height);
  if (result != CRTGFX_OK) return result;
  result = image->create(width, height);
  if (result == CRTGFX_OK) *out = std::move(image);
  return result;
}

crtgfx_result crtgfx_skia_shared_import(const crtgfx_gpu_device* device, const crtgfx_shared_image_desc& desc,
    const int fds[CRTGFX_SHARED_IMAGE_HANDLE_COUNT], std::unique_ptr<crtgfx_skia_shared_image>* out) {
  if (!out) return CRTGFX_ERROR_INVALID_ARGUMENT;
  out->reset();
  if (!device || !fds || !crtgfx_shared_image_desc_valid(desc)) return CRTGFX_ERROR_INVALID_ARGUMENT;
  if (desc.backend != CRTGFX_GPU_BACKEND_D3D12) return CRTGFX_ERROR_UNSUPPORTED;
  const unsigned count = crtgfx_shared_image_handle_count(&desc);
  for (unsigned i = 0; i < count; ++i)
    if (fds[i] < 0 || !__crt_windows_fd_get_handle(fds[i])) return CRTGFX_ERROR_INVALID_ARGUMENT;
  std::unique_ptr<SharedImage> image(new SharedImage);
  if (!image->d.init(device)) return CRTGFX_ERROR_UNSUPPORTED;
  crtgfx_shared_image_desc identity {};
  crtgfx_result result = query(image->d, &identity, desc.width, desc.height);
  if (result != CRTGFX_OK) return result;
  if (memcmp(desc.device_uuid, identity.device_uuid, sizeof desc.device_uuid)
      || memcmp(desc.driver_uuid, identity.driver_uuid, sizeof desc.driver_uuid)) return CRTGFX_ERROR_UNSUPPORTED;
  // The same shape this device would allocate; a descriptor that claims another size is refused before any
  // handle is opened.
  if (desc.allocation_size != allocation_size(image->d.device, desc.width, desc.height)) return CRTGFX_ERROR_INVALID_ARGUMENT;
  image->desc = desc;
  result = image->open(fds);
  if (result == CRTGFX_OK) *out = std::move(image);
  return result;
}
#endif
