/* Web Tranche 3A: a web frame producer composed as a crtui SurfaceView.
 *
 * A fake producer (below) speaks the CRT web surface wire protocol over a socketpair; the consumer
 * is crtweb::SurfaceClient, the adapter the WebView will use. The test checks the contract end to end
 * without a browser: the handshake, shared-memory buffers imported as a Skia image behind a crtui
 * SurfaceView, scene order (UI below / frame / UI above), acknowledgement and release, damage and
 * frame serials, resize, and crtui input translated to view-local wire events.
 * The browser itself is exercised by tools/build_webkit_wpe_crt.py (Linux, needs the WPE build). */

#include "crtweb_surface_client.h"

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

int failures = 0;

bool check(bool ok, const char* name) {
  if (!ok) {
    fprintf(stderr, "crtweb_web_surface_test: FAILED %s\n", name);
    ++failures;
  }
  return ok;
}

/* The producer end of the protocol, stepped synchronously by the test. */
class FakeProducer {
 public:
  explicit FakeProducer(int fd) : fd_(fd) {}

  void pump() {
    for (;;) {
      uint8_t buffer[CRTWEB_WIRE_MAX_MESSAGE];
      int fd = -1;
      ssize_t got = crtweb_wire_recv(fd_, buffer, sizeof buffer, &fd);
      if (got <= 0) return;
      const crtweb_wire_header* header = reinterpret_cast<const crtweb_wire_header*>(buffer);
      switch (header->type) {
        case CRTWEB_C2P_HELLO: {
          const crtweb_wire_hello* hello = reinterpret_cast<const crtweb_wire_hello*>(buffer);
          width = hello->width;
          height = hello->height;
          crtweb_wire_config config = {};
          config.header.type = CRTWEB_P2C_CONFIG;
          config.header.size = sizeof config;
          config.version = CRTWEB_WIRE_VERSION;
          config.pool_size = CRTWEB_POOL_SIZE;
          config.pixel_format = CRTWEB_PIXEL_ARGB8888;
          crtweb_wire_send(fd_, &config, sizeof config, -1);
          break;
        }
        case CRTWEB_C2P_RESIZE: {
          const crtweb_wire_resize* resize = reinterpret_cast<const crtweb_wire_resize*>(buffer);
          width = resize->width;
          height = resize->height;
          ++resizes;
          break;
        }
        case CRTWEB_C2P_FRAME_ACK:
          last_ack = reinterpret_cast<const crtweb_wire_frame_ack*>(buffer)->serial;
          ++acks;
          break;
        case CRTWEB_C2P_BUFFER_RELEASE: {
          uint32_t id = reinterpret_cast<const crtweb_wire_buffer_release*>(buffer)->buffer_id;
          for (Slot& slot : slots_) {
            if (slot.id == id) slot.held = false;
          }
          ++releases;
          break;
        }
        case CRTWEB_C2P_POINTER:
          last_pointer = *reinterpret_cast<const crtweb_wire_pointer*>(buffer);
          ++pointers;
          break;
        case CRTWEB_C2P_KEY:
          last_key = *reinterpret_cast<const crtweb_wire_key*>(buffer);
          ++keys;
          break;
        case CRTWEB_C2P_WHEEL:
          last_wheel = *reinterpret_cast<const crtweb_wire_wheel*>(buffer);
          ++wheels;
          break;
        default:
          break;
      }
      if (fd >= 0) close(fd);
    }
  }

  /* Paints width x height with four quadrants (top-left colour as given) and sends it. */
  bool produce(uint32_t top_left_bgr, int32_t damage_x = 0, int32_t damage_y = 0, int32_t damage_w = -1, int32_t damage_h = -1) {
    Slot* slot = nullptr;
    for (Slot& candidate : slots_) {
      if (candidate.held) continue;
      if (candidate.map != nullptr && (candidate.width != width || candidate.height != height)) retire(candidate);
      if (slot == nullptr || (slot->map == nullptr && candidate.map != nullptr)) slot = &candidate;
    }
    if (slot == nullptr) return false;
    if (slot->map == nullptr && !allocate(*slot)) return false;
    uint32_t* pixels = reinterpret_cast<uint32_t*>(slot->map);
    for (uint32_t y = 0; y < height; ++y) {
      for (uint32_t x = 0; x < width; ++x) {
        uint32_t color = (x < width / 2) ? ((y < height / 2) ? top_left_bgr : 0x0000FFu)
                                          : ((y < height / 2) ? 0x00FF00u : 0xFFFF00u);
        pixels[y * (slot->stride / 4) + x] = 0xFF000000u | color;
      }
    }
    slot->held = true;
    crtweb_wire_frame frame = {};
    frame.header.type = CRTWEB_P2C_FRAME;
    frame.header.size = sizeof frame;
    frame.buffer_id = slot->id;
    frame.serial = ++serial;
    frame.damage_serial = ++damage_serial;
    frame.damage_x = damage_x;
    frame.damage_y = damage_y;
    frame.damage_width = damage_w < 0 ? static_cast<int32_t>(width) : damage_w;
    frame.damage_height = damage_h < 0 ? static_cast<int32_t>(height) : damage_h;
    if (damage_w < 0) frame.flags = CRTWEB_FRAME_FULL_DAMAGE;
    return crtweb_wire_send(fd_, &frame, sizeof frame, -1) == 0;
  }

  void send_title(const char* text) {
    crtweb_wire_title title = {};
    title.header.type = CRTWEB_P2C_TITLE;
    title.header.size = sizeof title;
    strncpy(title.utf8, text, sizeof title.utf8 - 1);
    crtweb_wire_send(fd_, &title, sizeof title, -1);
  }

  unsigned buffers_held() const {
    unsigned count = 0;
    for (const Slot& slot : slots_) count += slot.held ? 1 : 0;
    return count;
  }

  uint32_t width = 0, height = 0;
  uint64_t serial = 0, damage_serial = 0, last_ack = 0;
  unsigned acks = 0, releases = 0, resizes = 0, pointers = 0, keys = 0, wheels = 0;
  crtweb_wire_pointer last_pointer = {};
  crtweb_wire_key last_key = {};
  crtweb_wire_wheel last_wheel = {};

 private:
  struct Slot {
    uint32_t id = 0;
    int fd = -1;
    uint8_t* map = nullptr;
    size_t size = 0;
    uint32_t width = 0, height = 0, stride = 0;
    bool held = false;
  };

  void retire(Slot& slot) {
    crtweb_wire_buffer_remove remove = {};
    remove.header.type = CRTWEB_P2C_BUFFER_REMOVE;
    remove.header.size = sizeof remove;
    remove.buffer_id = slot.id;
    crtweb_wire_send(fd_, &remove, sizeof remove, -1);
    munmap(slot.map, slot.size);
    close(slot.fd);
    slot = Slot();
  }

  bool allocate(Slot& slot) {
    slot.fd = memfd_create("crtweb-test-surface", MFD_CLOEXEC);
    if (slot.fd < 0) return false;
    slot.width = width;
    slot.height = height;
    slot.stride = width * 4;
    slot.size = static_cast<size_t>(slot.stride) * height;
    if (ftruncate(slot.fd, static_cast<off_t>(slot.size)) != 0) return false;
    void* map = mmap(nullptr, slot.size, PROT_READ | PROT_WRITE, MAP_SHARED, slot.fd, 0);
    if (map == MAP_FAILED) return false;
    slot.map = static_cast<uint8_t*>(map);
    slot.id = ++next_id_;
    crtweb_wire_buffer_add add = {};
    add.header.type = CRTWEB_P2C_BUFFER_ADD;
    add.header.size = sizeof add;
    add.buffer_id = slot.id;
    add.width = width;
    add.height = height;
    add.stride = slot.stride;
    add.pixel_format = CRTWEB_PIXEL_ARGB8888;
    add.size = slot.size;
    return crtweb_wire_send(fd_, &add, sizeof add, slot.fd) == 0;
  }

  int fd_;
  Slot slots_[CRTWEB_POOL_SIZE];
  uint32_t next_id_ = 0;
};

/* Scene: window 320x240 with a red container below, the web SurfaceView at (40,20) 200x150, and a
 * blue container above its lower-right corner. */
struct Scene {
  crtui_context* ui = nullptr;
  crtui_window window = CRTUI_INVALID_WIDGET;
  crtui_widget surface = CRTUI_INVALID_WIDGET;
};

void set_background(crtui_context* ui, crtui_widget widget, uint32_t rgb) {
  crtui_style style = {};
  style.mask = CRTUI_STYLE_BACKGROUND;
  style.background_rgb = rgb;
  check(crtui_widget_set_style(ui, widget, &style) == CRTUI_OK, "set background");
}

bool make_scene(Scene* scene) {
  crtui_widget below = CRTUI_INVALID_WIDGET, above = CRTUI_INVALID_WIDGET;
  if (!check(crtui_context_create(&scene->ui) == CRTUI_OK, "context") ||
      !check(crtui_window_create(scene->ui, &scene->window) == CRTUI_OK, "window") ||
      !check(crtui_window_set_size(scene->ui, scene->window, 320, 240, 1.0f) == CRTUI_OK, "window size") ||
      !check(crtui_container_create(scene->ui, scene->window, &below) == CRTUI_OK, "below") ||
      !check(crtui_widget_set_bounds(scene->ui, below, 0, 0, 320, 240) == CRTUI_OK, "below bounds") ||
      !check(crtui_surface_view_create(scene->ui, scene->window, &scene->surface) == CRTUI_OK, "surface") ||
      !check(crtui_widget_set_bounds(scene->ui, scene->surface, 40, 20, 200, 150) == CRTUI_OK, "surface bounds") ||
      !check(crtui_container_create(scene->ui, scene->window, &above) == CRTUI_OK, "above") ||
      !check(crtui_widget_set_bounds(scene->ui, above, 200, 130, 60, 60) == CRTUI_OK, "above bounds")) {
    return false;
  }
  set_background(scene->ui, below, 0xFF0000u);
  set_background(scene->ui, above, 0xFFFFFFu);
  return true;
}

crtui_rect surface_bounds(Scene* scene) {
  crtui_surface_layer layer = {};
  size_t count = 0;
  crtui_rect none = {0, 0, 0, 0};
  if (crtui_window_get_surface_layers(scene->ui, scene->window, &layer, 1, &count) != CRTUI_OK || count != 1) return none;
  return layer.bounds;
}

bool near(int value, int expected) { return value >= expected - 3 && value <= expected + 3; }

/* Pixel of the composed target as {r,g,b}. */
struct Rgb { int r, g, b; };

Rgb read_pixel(const std::vector<uint8_t>& pixels, int stride, int x, int y) {
  const uint8_t* p = pixels.data() + y * stride + x * 4;
  return {p[2], p[1], p[0]};  // BGRA
}

bool is(Rgb c, int r, int g, int b) { return near(c.r, r) && near(c.g, g) && near(c.b, b); }

}  // namespace

extern "C" int main() {
  int sockets[2];
  if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sockets) != 0) {
    printf("crtweb_web_surface_test: skipped (no AF_UNIX SOCK_SEQPACKET on this host)\n");
    return 0;
  }
  {
    int flags = fcntl(sockets[0], F_GETFL, 0);
    fcntl(sockets[0], F_SETFL, flags | O_NONBLOCK);
    flags = fcntl(sockets[1], F_GETFL, 0);
    fcntl(sockets[1], F_SETFL, flags | O_NONBLOCK);
  }
  FakeProducer producer(sockets[1]);
  Scene scene;
  if (!make_scene(&scene)) return 1;
  crtui_rect bounds = surface_bounds(&scene);
  check(bounds.x == 40 && bounds.y == 20 && bounds.width == 200 && bounds.height == 150, "layer bounds");

  /* Handshake: HELLO carries the view's logical size; the producer answers CONFIG. The test steps
   * the producer between the client's reads (the real producer runs concurrently). */
  crtweb::SurfaceClient client(sockets[0]);
  {
    crtweb_wire_hello hello = {};
    hello.header.type = CRTWEB_C2P_HELLO;
    hello.header.size = sizeof hello;
    hello.version = CRTWEB_WIRE_VERSION;
    hello.width = 200;
    hello.height = 150;
    hello.scale_percent = 100;
    crtweb_wire_send(sockets[0], &hello, sizeof hello, -1);
    producer.pump();
    check(producer.width == 200 && producer.height == 150, "producer learned the view size");
  }
  check(client.poll(1000), "CONFIG received");
  (void)client;

  sk_sp<SkSurface> target = SkSurfaces::Raster(SkImageInfo::Make(320, 240, kBGRA_8888_SkColorType, kPremul_SkAlphaType));
  if (!check(target != nullptr, "raster target")) return 1;
  std::vector<uint8_t> pixels(320 * 240 * 4);
  SkImageInfo readback = SkImageInfo::Make(320, 240, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
  crtui_skia_surface_provider provider = client.surface_provider();

  auto compose = [&]() {
    check(crtui_skia_compose(scene.ui, scene.window, target.get(), &provider) == CRTUI_OK ||
              client.frames_received() == 0,
          "compose");
    check(target->readPixels(readback, pixels.data(), 320 * 4, 0, 0), "read back");
    client.frame_presented();
    producer.pump();
  };

  /* No frame yet: compose reports WOULD_BLOCK and the UI below shows through. */
  check(crtui_skia_compose(scene.ui, scene.window, target.get(), &provider) == CRTUI_WOULD_BLOCK, "no frame: WOULD_BLOCK");

  /* Frame 1: 200x150; quadrants top-left (as given), green, blue, yellow (0xRRGGBB). */
  check(producer.produce(0xFF0000u), "produce frame 1");
  check(client.poll(1000) && client.has_frame(), "frame 1 received");
  check(client.frame_width() == 200 && client.frame_height() == 150, "frame size");
  compose();
  check(producer.last_ack == 1 && producer.acks == 1, "frame 1 acknowledged");
  /* View-local (50,40) is in the top-left quadrant; window coordinates add (40,20). */
  check(is(read_pixel(pixels, 320 * 4, 40 + 50, 20 + 40), 255, 0, 0), "top-left quadrant shows the producer's colour");
  check(is(read_pixel(pixels, 320 * 4, 40 + 50, 20 + 110), 0, 0, 255), "bottom-left quadrant is the producer's blue");
  check(is(read_pixel(pixels, 320 * 4, 40 + 150, 20 + 40), 0, 255, 0), "top-right quadrant is the producer's green");
  check(is(read_pixel(pixels, 320 * 4, 5, 5), 255, 0, 0), "UI below shows around the view");
  check(is(read_pixel(pixels, 320 * 4, 220, 150), 255, 255, 255), "UI above the view stays on top");
  check(is(read_pixel(pixels, 320 * 4, 40 + 150, 20 + 110), 255, 255, 0), "bottom-right quadrant is the producer's yellow");

  /* Frame 2 replaces frame 1: the old buffer goes back only when its image is gone. */
  check(producer.produce(0xFF00FFu), "produce frame 2");
  check(client.poll(1000), "frame 2 received");
  compose();
  producer.pump();
  check(client.frame_serial() == 2 && producer.last_ack == 2, "frame 2 acknowledged");
  check(producer.releases >= 1, "frame 1's buffer was released");
  check(producer.buffers_held() == 1, "the producer sees one buffer in use");
  check(client.damage_serial() == 2, "damage serial follows the frame");

  /* A frame the consumer has not composed yet is dropped in favour of the next one, and its buffer
   * returns (latest wins). */
  check(producer.produce(0x00FF00u), "produce frame 3");
  check(producer.produce(0x808080u), "produce frame 4");
  check(client.poll(1000), "frames 3 and 4 received");
  compose();
  check(client.frame_serial() == 4 && client.frames_received() == 4, "newest frame wins");
  check(producer.last_ack == 4, "only the shown frame is acknowledged");
  producer.pump();
  check(producer.buffers_held() == 1, "dropped frame's buffer was released");

  /* Title. */
  producer.send_title("hello|1");
  check(client.poll(1000) && client.title() == "hello|1", "title message");

  /* Input: window coordinates -> view-local logical pixels. */
  {
    crtui_input input = {};
    input.type = CRTUI_INPUT_POINTER_DOWN;
    input.window = scene.window;
    input.x = 40 + 70;
    input.y = 20 + 30;
    check(client.forward(input, surface_bounds(&scene)), "pointer inside the view is forwarded");
    producer.pump();
    check(producer.pointers == 1 && producer.last_pointer.action == CRTWEB_POINTER_DOWN &&
              producer.last_pointer.button == 1 && producer.last_pointer.press_count == 1 &&
              producer.last_pointer.x == 70.0 && producer.last_pointer.y == 30.0,
          "pointer position is view-local");
    input.x = 5;
    input.y = 5;
    check(!client.forward(input, surface_bounds(&scene)), "pointer outside the view is not forwarded");
    crtui_input key = {};
    key.type = CRTUI_INPUT_KEY_DOWN;
    key.key = CRTUI_KEY_ENTER;
    key.modifiers = CRTUI_MOD_SHIFT;
    check(client.forward(key, surface_bounds(&scene)), "key forwarded");
    producer.pump();
    check(producer.keys == 1 && producer.last_key.keyval == 0xff0d && producer.last_key.keycode == 36 &&
              producer.last_key.down == 1 && (producer.last_key.modifiers & CRTWEB_MOD_SHIFT) != 0,
          "Enter -> keysym 0xff0d, XKB keycode 36, shift");
    crtui_input wheel = {};
    wheel.type = CRTUI_INPUT_WHEEL;
    wheel.x = 40 + 10;
    wheel.y = 20 + 10;
    wheel.wheel_delta = 3;
    check(client.forward(wheel, surface_bounds(&scene)), "wheel forwarded");
    producer.pump();
    check(producer.wheels == 1 && producer.last_wheel.delta_y == 3.0 && producer.last_wheel.x == 10.0, "wheel position");
  }

  /* Resize: the view grows to 240x180; the producer answers with buffers of the new size. */
  check(crtui_widget_set_bounds(scene.ui, scene.surface, 40, 20, 240, 180) == CRTUI_OK, "grow the view");
  client.resize(240, 180, 100);
  producer.pump();
  check(producer.width == 240 && producer.height == 180 && producer.resizes == 1, "producer saw the resize");
  check(producer.produce(0x00FFFFu), "produce at the new size");
  check(client.poll(1000), "resized frame received");
  check(client.frame_width() == 240 && client.frame_height() == 180, "frame has the new size");
  compose();
  check(is(read_pixel(pixels, 320 * 4, 40 + 20, 20 + 20), 0, 255, 255), "resized frame composed (cyan top-left)");

  client.close_connection();
  crtui_context_destroy(scene.ui);
  if (failures != 0) {
    fprintf(stderr, "crtweb_web_surface_test: %d check(s) failed\n", failures);
    return 1;
  }
  printf("crtweb_web_surface_test: ok\n");
  return 0;
}
