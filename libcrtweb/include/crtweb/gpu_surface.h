#pragma once

/* GPU resource consumer, separate from the v1 CPU WebView. The worker owns
 * native production; the application owns its SDK Skia context. This class
 * never copies GPU pixels through CPU memory. UI/GPU-owner thread only.
 * device/context must be the same pair and outlive this object/Skia references.
 * Return provider references before poll/resize/destruction; do not retain
 * them to schedule later GPU draws after releasing the current frame.
 */
#include "crtweb_gpu_wire.h"
#include "crtgfx/skia_shared.h"
#include "crtui/skia.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include <array>
#include <chrono>
#include <poll.h>
#include <string>

namespace crtweb {
class GpuSurfaceClient {
 public:
  GpuSurfaceClient(int fd, const crtgfx_gpu_device* device, GrDirectContext* context)
      : fd_(fd), device_(device), context_(context) {}
  GpuSurfaceClient(const GpuSurfaceClient&) = delete;
  GpuSurfaceClient& operator=(const GpuSurfaceClient&) = delete;
  ~GpuSurfaceClient() { shutdown(); }

  /* Stop the owned channel, draining transfer descriptors without importing
   * new images. This is essential on Windows, where SCM_RIGHTS duplicates a
   * handle into the receiver before its record is read. The process owner
   * still waits/reaps the worker separately. No reconnect reuses this pool.
   * A lost context cannot acknowledge/release GPU work; BYE retires the whole
   * session instead. A timeout is a failed shutdown, not a completion signal. */
  bool shutdown(uint32_t timeout_ms = 6000) {
    if (fd_ < 0) return shutdown_complete_;
    bool lost = !context_ || context_->abandoned();
    bool retired = retire_current();
    if (!peer_eof_) crtweb_gpu_send(fd_, &bye_packet(), nullptr, 0);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (!peer_eof_ && std::chrono::steady_clock::now() < deadline) {
      struct pollfd ready {fd_, POLLIN, 0};
      int result = ::poll(&ready, 1, 10);
      if (result < 0 && errno != EINTR) break;
      if (result <= 0) continue;
      crtweb_gpu_packet packet; int handles[CRTGFX_SHARED_IMAGE_HANDLE_COUNT];
      int got = crtweb_gpu_recv(fd_, &packet, handles);
      crtweb_gpu_close_handles(handles);
      if (got == 1 && packet.type == CRTWEB_GPU_BUFFER_ADD) ++discarded_imports_;
      if (!got || (got < 0 && errno != EPROTO)) peer_eof_ = true;
    }
    image_.reset();
    if (!lost) context_->flushAndSubmit(GrSyncCpu::kYes);
    for (auto& buffer : buffers_) buffer = Buffer();
    close(fd_); fd_ = -1; closed_ = true;
    shutdown_complete_ = peer_eof_ && (retired || lost);
    return shutdown_complete_;
  }

  bool handshake(uint32_t width, uint32_t height, bool require_shared, int timeout_ms = 10000) {
    auto hello = crtweb_gpu_message(CRTWEB_GPU_HELLO);
    crtgfx_skia_shared_query(device_, &hello.payload.image);
    hello.payload.image.width = width; hello.payload.image.height = height;
    hello.flags = require_shared ? CRTWEB_GPU_REQUIRE_SHARED : 0;
    if (!send(hello) || !poll(timeout_ms) || !configured_) return false;
    return mode_ == CRTWEB_GPU_MODE_SHARED || (!require_shared && mode_ == CRTWEB_GPU_MODE_CPU);
  }
  bool poll(int timeout_ms = 0) {
    if (!gpu_usable()) return false;
    bool any = false;
    while (!closed_) {
      struct pollfd pfd {fd_, POLLIN, 0};
      int ready = ::poll(&pfd, 1, any ? 0 : timeout_ms);
      if (ready <= 0) break;
      crtweb_gpu_packet packet;
      int handles[CRTGFX_SHARED_IMAGE_HANDLE_COUNT];
      int received = crtweb_gpu_recv(fd_, &packet, handles);
      if (received <= 0) { closed_ = true; peer_eof_ = received == 0; break; }
      bool ok = handle(packet, handles);
      crtweb_gpu_close_handles(handles);
      any = true;
      if (!ok) { closed_ = true; break; }
    }
    return any;
  }
  uint32_t mode() const { return mode_; }
  bool closed() const { return closed_; }
  bool gpu_lost() const { return gpu_lost_; }
  bool has_frame() const { return image_ != nullptr && context_ && !context_->abandoned(); }
  uint64_t frame_serial() const { return serial_; }
  uint32_t frame_width() const { return width_; }
  uint32_t frame_height() const { return height_; }
  uint32_t generation() const { return generation_; }
  uint32_t imports() const { return imports_; }
  uint32_t releases() const { return releases_; }
  uint32_t discarded_imports() const { return discarded_imports_; }
  uint32_t live_buffers() const {
    uint32_t count = 0; for (const auto& b : buffers_) if (b.resource) ++count; return count;
  }
  crtui_skia_surface_provider surface_provider() { return {this, acquire, release}; }
  bool frame_presented() {
    if (!gpu_usable()) return false;
    if (!current_ || serial_ == acked_) return true;
    auto ack = crtweb_gpu_message(CRTWEB_GPU_ACK);
    ack.serial = serial_; ack.generation = generation_; ack.buffer_id = current_;
    if (!send(ack)) return false;
    acked_ = serial_;
    return true;
  }
  bool command(uint32_t code, const std::string& text) {
    auto packet = crtweb_gpu_message(CRTWEB_GPU_COMMAND);
    if (text.size() >= sizeof packet.payload.command.text) return false;
    packet.payload.command.code = code;
    packet.payload.command.length = static_cast<uint32_t>(text.size());
    memcpy(packet.payload.command.text, text.data(), text.size());
    return send(packet);
  }
  bool resize(uint32_t width, uint32_t height) {
    if (!width || !height || width > CRTGFX_SHARED_IMAGE_MAX_EXTENT || height > CRTGFX_SHARED_IMAGE_MAX_EXTENT) return false;
    return command(CRTWEB_GPU_RESIZE, std::to_string(width) + " " + std::to_string(height));
  }

 private:
  struct Buffer {
    uint32_t generation = 0;
    uint64_t serial = 0;
    bool read = false, ever_read = false;
    std::unique_ptr<crtgfx_skia_shared_image> resource;
  };
  bool send(const crtweb_gpu_packet& packet) {
    if (!gpu_usable() || closed_ || crtweb_gpu_send(fd_, &packet, nullptr, 0) != 0) { closed_ = true; return false; }
    return true;
  }
  static const crtweb_gpu_packet& bye_packet() {
    static const auto bye = crtweb_gpu_message(CRTWEB_GPU_BYE);
    return bye;
  }
  bool gpu_usable() {
    if (context_ && !context_->abandoned()) return true;
    gpu_lost_ = closed_ = true;
    image_.reset();
    return false;
  }
  bool retire_current() {
    if (!current_) return true;
    Buffer& buffer = buffers_[current_ - 1];
    bool ok = buffer.resource->end_read(context_) == CRTGFX_OK;
    image_.reset();
    if (ok && !closed_) {
      auto released = crtweb_gpu_message(CRTWEB_GPU_RELEASE);
      released.buffer_id = current_; released.generation = buffer.generation; released.serial = buffer.serial;
      ok = send(released);
    }
    buffer.read = false; current_ = 0; ++releases_;
    return ok;
  }
  bool handle(const crtweb_gpu_packet& p, const int* handles) {
    if (p.type == CRTWEB_GPU_CONFIG) {
      if (configured_) return false;
      mode_ = p.flags;
      if (mode_ == CRTWEB_GPU_MODE_SHARED) {
        crtgfx_shared_image_desc identity {};
        if (crtgfx_skia_shared_query(device_, &identity) != CRTGFX_OK
            || identity.version != p.payload.image.version || identity.backend != p.payload.image.backend
            || identity.handle_kind != p.payload.image.handle_kind || identity.format != p.payload.image.format
            || memcmp(identity.device_uuid, p.payload.image.device_uuid, 16)
            || memcmp(identity.driver_uuid, p.payload.image.driver_uuid, 16)) return false;
      }
      configured_ = true;
      return true;
    }
    if (p.type == CRTWEB_GPU_BYE) { closed_ = true; return true; }
    if (!configured_ || mode_ != CRTWEB_GPU_MODE_SHARED) return false;
    if (p.type == CRTWEB_GPU_BUFFER_ADD) {
      Buffer& buffer = buffers_[p.buffer_id - 1];
      if (buffer.resource || p.generation < generation_) return false;
      if (crtgfx_skia_shared_import(device_, p.payload.image, handles, &buffer.resource) != CRTGFX_OK) return false;
      buffer.generation = p.generation; ++imports_;
      return true;
    }
    if (p.type == CRTWEB_GPU_FRAME) {
      Buffer& buffer = buffers_[p.buffer_id - 1];
      if (!buffer.resource || buffer.read || buffer.generation != p.generation
          || p.generation < generation_ || p.serial <= serial_ || p.serial <= buffer.serial) return false;
      if (!retire_current()) return false;
      if (buffer.resource->begin_read(context_, 2000, &image_) != CRTGFX_OK || !image_->isTextureBacked()) return false;
      buffer.read = buffer.ever_read = true; buffer.serial = p.serial;
      current_ = p.buffer_id; serial_ = p.serial; generation_ = p.generation;
      width_ = buffer.resource->description().width; height_ = buffer.resource->description().height;
      return true;
    }
    if (p.type == CRTWEB_GPU_BUFFER_REMOVE) {
      Buffer& buffer = buffers_[p.buffer_id - 1];
      if (!buffer.resource || buffer.read || buffer.generation != p.generation) return false;
      if (buffer.ever_read && buffer.resource->wait_released(0) != CRTGFX_OK) return false;
      buffer = Buffer();
      return true;
    }
    return false;
  }
  static SkImage* acquire(void* user, const crtui_surface_layer*) {
    auto* self = static_cast<GpuSurfaceClient*>(user);
    if (!self->gpu_usable() || !self->image_) return nullptr;
    self->image_->ref(); return self->image_.get();
  }
  static void release(void*, const crtui_surface_layer*, SkImage* image) { image->unref(); }
  int fd_;
  const crtgfx_gpu_device* device_;
  GrDirectContext* context_;
  std::array<Buffer, CRTWEB_GPU_MAX_BUFFERS> buffers_;
  sk_sp<SkImage> image_;
  bool configured_ = false, closed_ = false;
  bool peer_eof_ = false, gpu_lost_ = false, shutdown_complete_ = false;
  uint32_t discarded_imports_ = 0;
  uint32_t mode_ = 0, current_ = 0, generation_ = 0, width_ = 0, height_ = 0, imports_ = 0, releases_ = 0;
  uint64_t serial_ = 0, acked_ = 0;
};
}  // namespace crtweb
