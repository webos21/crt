#pragma once

/* Consumer side of the CRT web surface wire protocol for a crtui application (Web Tranche 3A).
 *
 * C++17, header-only, and the seed of the libcrtweb web-view adapter (Tranche 4): it receives frames
 * into shared memory, exposes the newest one as a Skia image through the crtui SurfaceView provider
 * callbacks (crtui/skia.h), acknowledges what the compositor has drawn, releases buffers when their
 * image is gone, and turns crtui input and size changes into wire messages. It knows no WebKit or
 * GLib type. All semantics are in docs/acceptance/crtweb_acceptance.md.
 *
 *   client.handshake(w, h, scale);
 *   crtui_skia_surface_provider provider = client.surface_provider();
 *   loop: client.poll(0);  crtui_skia_compose(..., &provider);  client.frame_presented();
 */

#include "crtweb_surface_wire.h"

#include "crtui/skia.h"

#include "include/core/SkImage.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"

#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <string>

namespace crtweb {

class SurfaceClient {
 public:
  explicit SurfaceClient(int socket_fd) : socket_(socket_fd) {}
  ~SurfaceClient() {
    image_.reset();
    for (Buffer& buffer : buffers_) {
      if (buffer.map != nullptr) munmap(buffer.map, buffer.size);
    }
  }
  SurfaceClient(const SurfaceClient&) = delete;
  SurfaceClient& operator=(const SurfaceClient&) = delete;

  /* Sends HELLO and waits for CONFIG. Returns false on timeout or protocol violation. */
  bool handshake(uint32_t width, uint32_t height, uint32_t scale_percent, int timeout_ms = 10000) {
    crtweb_wire_hello hello = {};
    hello.version = CRTWEB_WIRE_VERSION;
    hello.width = width;
    hello.height = height;
    hello.scale_percent = scale_percent;
    send(CRTWEB_C2P_HELLO, &hello, sizeof hello);
    while (!configured_ && !closed_) {
      if (!poll(timeout_ms)) return false;
    }
    return configured_;
  }

  /* Reads every message that is available, waiting up to timeout_ms for the first one. Returns
   * true when at least one message was processed. */
  bool poll(int timeout_ms) {
    bool any = false;
    struct pollfd pfd = {socket_, POLLIN, 0};
    int wait = timeout_ms;
    while (!closed_ && ::poll(&pfd, 1, wait) > 0) {
      uint8_t buffer[CRTWEB_WIRE_MAX_MESSAGE];
      int fd = -1;
      ssize_t got = crtweb_wire_recv(socket_, buffer, sizeof buffer, &fd);
      if (got <= 0) {
        if (got < 0 && errno == EAGAIN) break;
        closed_ = true;
        break;
      }
      handle(buffer, fd);
      any = true;
      wait = 0;
    }
    return any;
  }

  /* ---- composition -------------------------------------------------------------------- */

  crtui_skia_surface_provider surface_provider() { return {this, &SurfaceClient::acquire_thunk, &SurfaceClient::release_thunk}; }

  /* Call after crtui_skia_compose() drew the newest frame: acknowledges it (once). */
  void frame_presented() {
    if (frame_serial_ != 0 && frame_serial_ != acked_serial_) {
      crtweb_wire_frame_ack ack = {};
      ack.serial = frame_serial_;
      send(CRTWEB_C2P_FRAME_ACK, &ack, sizeof ack);
      acked_serial_ = frame_serial_;
    }
  }

  bool has_frame() const { return image_ != nullptr; }
  uint64_t frame_serial() const { return frame_serial_; }
  uint64_t damage_serial() const { return damage_serial_; }
  uint32_t frame_width() const { return frame_width_; }
  uint32_t frame_height() const { return frame_height_; }
  const std::string& title() const { return title_; }
  bool closed() const { return closed_; }
  uint32_t frames_received() const { return frames_; }

  /* ---- input and geometry ------------------------------------------------------------- */

  void resize(uint32_t width, uint32_t height, uint32_t scale_percent) {
    crtweb_wire_resize message = {};
    message.width = width;
    message.height = height;
    message.scale_percent = scale_percent;
    send(CRTWEB_C2P_RESIZE, &message, sizeof message);
  }

  void focus(bool focused) {
    crtweb_wire_focus message = {};
    message.focused = focused ? 1 : 0;
    send(CRTWEB_C2P_FOCUS, &message, sizeof message);
  }

  /* Forwards one crtui input for a view whose bounds (window coordinates) are `view`. Pointer and
   * wheel positions become view-local logical pixels; returns false for an input the web view
   * does not take (a pointer outside the bounds, a key crtui cannot name). */
  bool forward(const crtui_input& input, const crtui_rect& view) {
    switch (input.type) {
      case CRTUI_INPUT_POINTER_DOWN:
      case CRTUI_INPUT_POINTER_UP:
      case CRTUI_INPUT_POINTER_MOVE: {
        if (input.x < view.x || input.y < view.y || input.x >= view.x + view.width || input.y >= view.y + view.height) {
          return false;
        }
        crtweb_wire_pointer message = {};
        message.action = input.type == CRTUI_INPUT_POINTER_DOWN ? CRTWEB_POINTER_DOWN
                         : input.type == CRTUI_INPUT_POINTER_UP ? CRTWEB_POINTER_UP
                                                                : CRTWEB_POINTER_MOVE;
        message.pointer_type = CRTWEB_POINTER_MOUSE;
        message.button = message.action == CRTWEB_POINTER_MOVE ? 0 : 1;
        message.press_count = message.action == CRTWEB_POINTER_DOWN ? 1 : 0;
        message.modifiers = modifiers(input.modifiers);
        message.x = input.x - view.x;
        message.y = input.y - view.y;
        send(CRTWEB_C2P_POINTER, &message, sizeof message);
        return true;
      }
      case CRTUI_INPUT_WHEEL: {
        crtweb_wire_wheel message = {};
        message.modifiers = modifiers(input.modifiers);
        message.x = input.x - view.x;
        message.y = input.y - view.y;
        message.delta_y = input.wheel_delta;
        send(CRTWEB_C2P_WHEEL, &message, sizeof message);
        return true;
      }
      case CRTUI_INPUT_KEY_DOWN:
      case CRTUI_INPUT_KEY_UP: {
        uint32_t keyval = 0, keycode = 0;
        if (!key_map(input.key, &keyval, &keycode)) return false;
        crtweb_wire_key message = {};
        message.down = input.type == CRTUI_INPUT_KEY_DOWN ? 1 : 0;
        message.keyval = keyval;
        message.keycode = keycode;
        message.modifiers = modifiers(input.modifiers);
        send(CRTWEB_C2P_KEY, &message, sizeof message);
        return true;
      }
      case CRTUI_INPUT_TEXT: {
        crtweb_wire_text message = {};
        std::strncpy(message.utf8, input.text, sizeof message.utf8 - 1);
        send(CRTWEB_C2P_TEXT, &message, sizeof message);
        return true;
      }
      default:
        return false;
    }
  }

  void close_connection() {
    crtweb_wire_header bye = {};
    send(CRTWEB_C2P_BYE, &bye, sizeof bye);
  }

 private:
  struct Buffer {
    uint32_t id = 0;
    uint8_t* map = nullptr;
    size_t size = 0;
    uint32_t width = 0, height = 0, stride = 0;
    bool in_use = false;
  };

  struct ImageContext {
    SurfaceClient* client;
    uint32_t buffer_id;
  };

  static uint32_t modifiers(uint32_t crtui_modifiers) { return (crtui_modifiers & CRTUI_MOD_SHIFT) ? CRTWEB_MOD_SHIFT : 0; }

  /* crtui's frozen v1 key enum -> X11 keysym and XKB keycode (evdev + 8). */
  static bool key_map(crtui_key key, uint32_t* keyval, uint32_t* keycode) {
    switch (key) {
      case CRTUI_KEY_TAB: *keyval = 0xff09; *keycode = 15 + 8; return true;
      case CRTUI_KEY_ENTER: *keyval = 0xff0d; *keycode = 28 + 8; return true;
      case CRTUI_KEY_SPACE: *keyval = 0x20; *keycode = 57 + 8; return true;
      case CRTUI_KEY_LEFT: *keyval = 0xff51; *keycode = 105 + 8; return true;
      case CRTUI_KEY_UP: *keyval = 0xff52; *keycode = 103 + 8; return true;
      case CRTUI_KEY_RIGHT: *keyval = 0xff53; *keycode = 106 + 8; return true;
      case CRTUI_KEY_DOWN: *keyval = 0xff54; *keycode = 108 + 8; return true;
      case CRTUI_KEY_ESCAPE: *keyval = 0xff1b; *keycode = 1 + 8; return true;
      case CRTUI_KEY_BACKSPACE: *keyval = 0xff08; *keycode = 14 + 8; return true;
      case CRTUI_KEY_DELETE: *keyval = 0xffff; *keycode = 111 + 8; return true;
      case CRTUI_KEY_HOME: *keyval = 0xff50; *keycode = 102 + 8; return true;
      case CRTUI_KEY_END: *keyval = 0xff57; *keycode = 107 + 8; return true;
      default: return false;
    }
  }

  void send(uint32_t type, void* message, size_t size) {
    crtweb_wire_header* header = static_cast<crtweb_wire_header*>(message);
    header->type = type;
    header->size = static_cast<uint32_t>(size);
    if (!closed_ && crtweb_wire_send(socket_, message, size, -1) != 0 && errno != EAGAIN) closed_ = true;
  }

  Buffer* find(uint32_t id) {
    for (Buffer& buffer : buffers_) {
      if (buffer.map != nullptr && buffer.id == id) return &buffer;
    }
    return nullptr;
  }

  void handle(const void* data, int fd) {
    const crtweb_wire_header* header = static_cast<const crtweb_wire_header*>(data);
    switch (header->type) {
      case CRTWEB_P2C_CONFIG: {
        const crtweb_wire_config* config = static_cast<const crtweb_wire_config*>(data);
        configured_ = config->version == CRTWEB_WIRE_VERSION && config->pixel_format == CRTWEB_PIXEL_ARGB8888;
        if (!configured_) closed_ = true;
        break;
      }
      case CRTWEB_P2C_BUFFER_ADD: {
        const crtweb_wire_buffer_add* add = static_cast<const crtweb_wire_buffer_add*>(data);
        if (fd < 0 || add->pixel_format != CRTWEB_PIXEL_ARGB8888 || add->stride < add->width * 4u ||
            add->size < static_cast<uint64_t>(add->stride) * add->height) {
          closed_ = true;
          break;
        }
        for (Buffer& buffer : buffers_) {
          if (buffer.map != nullptr) continue;
          void* map = mmap(nullptr, static_cast<size_t>(add->size), PROT_READ, MAP_SHARED, fd, 0);
          if (map == MAP_FAILED) {
            closed_ = true;
            break;
          }
          buffer.map = static_cast<uint8_t*>(map);
          buffer.size = static_cast<size_t>(add->size);
          buffer.id = add->buffer_id;
          buffer.width = add->width;
          buffer.height = add->height;
          buffer.stride = add->stride;
          buffer.in_use = false;
          break;
        }
        break;
      }
      case CRTWEB_P2C_BUFFER_REMOVE: {
        const crtweb_wire_buffer_remove* remove = static_cast<const crtweb_wire_buffer_remove*>(data);
        Buffer* buffer = find(remove->buffer_id);
        /* The producer retires a buffer only after it was released, so no image can still use it. */
        if (buffer != nullptr && !buffer->in_use) {
          munmap(buffer->map, buffer->size);
          *buffer = Buffer();
        }
        break;
      }
      case CRTWEB_P2C_FRAME: {
        const crtweb_wire_frame* frame = static_cast<const crtweb_wire_frame*>(data);
        Buffer* buffer = find(frame->buffer_id);
        if (buffer == nullptr || buffer->in_use || frame->serial <= frame_serial_) {
          closed_ = true; /* a protocol violation */
          break;
        }
        buffer->in_use = true;
        ImageContext* context = new ImageContext{this, buffer->id};
        SkImageInfo info = SkImageInfo::Make(static_cast<int>(buffer->width), static_cast<int>(buffer->height),
                                             kBGRA_8888_SkColorType, kPremul_SkAlphaType);
        SkPixmap pixmap(info, buffer->map, buffer->stride);
        /* The image reads the shared memory in place; its release proc returns the buffer. */
        sk_sp<SkImage> image = SkImages::RasterFromPixmap(pixmap, &SurfaceClient::image_released, context);
        image_ = std::move(image);
        frame_serial_ = frame->serial;
        damage_serial_ = frame->damage_serial;
        frame_width_ = buffer->width;
        frame_height_ = buffer->height;
        ++frames_;
        break;
      }
      case CRTWEB_P2C_TITLE: {
        const crtweb_wire_title* title = static_cast<const crtweb_wire_title*>(data);
        title_.assign(title->utf8, strnlen(title->utf8, sizeof title->utf8));
        break;
      }
      case CRTWEB_P2C_BYE:
        closed_ = true;
        break;
      default:
        break;
    }
    if (fd >= 0) ::close(fd);
  }

  static void image_released(const void*, void* raw) {
    ImageContext* context = static_cast<ImageContext*>(raw);
    SurfaceClient* client = context->client;
    Buffer* buffer = client->find(context->buffer_id);
    if (buffer != nullptr) {
      buffer->in_use = false;
      crtweb_wire_buffer_release release = {};
      release.buffer_id = buffer->id;
      client->send(CRTWEB_C2P_BUFFER_RELEASE, &release, sizeof release);
    }
    delete context;
  }

  static SkImage* acquire_thunk(void* user, const crtui_surface_layer*) {
    SurfaceClient* client = static_cast<SurfaceClient*>(user);
    if (client->image_ == nullptr) return nullptr;
    client->image_->ref(); /* the compositor's release() consumes this reference */
    return client->image_.get();
  }

  static void release_thunk(void*, const crtui_surface_layer*, SkImage* image) { image->unref(); }

  int socket_;
  bool configured_ = false;
  bool closed_ = false;
  std::array<Buffer, 8> buffers_;
  sk_sp<SkImage> image_;
  uint64_t frame_serial_ = 0;
  uint64_t acked_serial_ = 0;
  uint64_t damage_serial_ = 0;
  uint32_t frame_width_ = 0, frame_height_ = 0;
  uint32_t frames_ = 0;
  std::string title_;
};

}  // namespace crtweb
