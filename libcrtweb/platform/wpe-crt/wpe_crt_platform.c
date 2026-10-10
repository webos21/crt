/* WPEPlatform "crt" display (Web Tranche 3A).
 *
 * A GIO module that registers a WPEDisplay implementation under the name "crt" (select it with
 * WPE_DISPLAY=crt and WPE_PLATFORMS_PATH=<dir of this module>). It lives *outside* the WebKit tree and
 * needs no WebKit patch: WPE's display is a GIO extension point.
 *
 * It is the Linux prototype boundary of the frame-producer and input contracts, not the product
 * architecture (docs/porting/crtweb_porting.md): the committed WPE shared-memory buffer is copied
 * into a producer-owned pool of memfds that a consumer maps, and the consumer's input comes back as
 * WPE events. All protocol definitions are in ../crtweb_surface_wire.h.
 *
 * Pacing and drop policy (the contract): at most one frame is in flight. WPE reports a frame as
 * rendered only when the consumer acknowledges it (or after a bounded wait, so a stalled consumer
 * cannot freeze page scripts); frames committed meanwhile coalesce, latest wins.
 */
#define _GNU_SOURCE 1
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include <gio/gio.h>
#include <glib-unix.h>
#include <wpe/wpe-platform.h>

#include "../crtweb_surface_wire.h"

#define ACK_TIMEOUT_MS 250
#define CONNECT_TIMEOUT_MS 10000

/* ---- state shared by the display, its toplevel and its view ------------------------------- */

typedef struct {
  uint32_t id;
  int fd;
  uint8_t* map;
  size_t size;
  uint32_t width, height, stride;
  gboolean announced;
  gboolean held; /* the consumer owns it until BUFFER_RELEASE */
} PoolBuffer;

typedef struct {
  int socket;
  guint watch;
  uint32_t width, height, scale_percent;
  PoolBuffer pool[CRTWEB_POOL_SIZE];
  uint32_t next_id;
  uint64_t serial, damage_serial, acked_serial;
  WPEView* view;         /* weak: cleared in the view's dispose */
  WPEToplevel* toplevel; /* weak */
  WPEBuffer* inflight;   /* committed by WebKit, not yet acknowledged */
  uint64_t inflight_serial;
  WPEBuffer* pending;    /* the newest commit that found nothing to do yet */
  guint ack_timer;
  gboolean closed;
} CrtState;

static CrtState state = {.socket = -1};

static uint64_t now_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static guint32 now_ms(void) { return (guint32)(g_get_monotonic_time() / 1000); }

static void send_message(const void* message, size_t size, int fd) {
  if (state.socket < 0) {
    return;
  }
  if (crtweb_wire_send(state.socket, message, size, fd) != 0 && errno != EAGAIN) {
    g_warning("crt platform: send failed: %s", g_strerror(errno));
  }
}

#define SEND(type_value, object)                                  \
  do {                                                            \
    (object).header.type = (type_value);                          \
    (object).header.size = (uint32_t)sizeof(object);              \
    send_message(&(object), sizeof(object), -1);                  \
  } while (0)

/* ---- the buffer pool ---------------------------------------------------------------------- */

static void pool_retire(PoolBuffer* buffer) {
  if (buffer->announced) {
    crtweb_wire_buffer_remove remove = {0};
    remove.buffer_id = buffer->id;
    SEND(CRTWEB_P2C_BUFFER_REMOVE, remove);
  }
  if (buffer->map) {
    munmap(buffer->map, buffer->size);
  }
  if (buffer->fd >= 0) {
    close(buffer->fd);
  }
  memset(buffer, 0, sizeof *buffer);
  buffer->fd = -1;
}

static PoolBuffer* pool_acquire(uint32_t width, uint32_t height) {
  PoolBuffer* free_slot = NULL;
  guint i;

  for (i = 0; i < CRTWEB_POOL_SIZE; ++i) {
    PoolBuffer* buffer = &state.pool[i];
    if (buffer->held) {
      continue;
    }
    if (buffer->map && (buffer->width != width || buffer->height != height)) {
      pool_retire(buffer); /* resized: the old size is no longer wanted */
    }
    if (buffer->map) {
      return buffer;
    }
    if (free_slot == NULL) {
      free_slot = buffer;
    }
  }
  if (free_slot == NULL) {
    return NULL;
  }
  free_slot->fd = memfd_create("crtweb-surface", MFD_CLOEXEC);
  if (free_slot->fd < 0) {
    return NULL;
  }
  free_slot->stride = width * 4u;
  free_slot->size = (size_t)free_slot->stride * height;
  free_slot->width = width;
  free_slot->height = height;
  if (ftruncate(free_slot->fd, (off_t)free_slot->size) != 0 ||
      (free_slot->map = mmap(NULL, free_slot->size, PROT_READ | PROT_WRITE, MAP_SHARED, free_slot->fd, 0)) ==
          MAP_FAILED) {
    free_slot->map = NULL;
    close(free_slot->fd);
    free_slot->fd = -1;
    return NULL;
  }
  free_slot->id = ++state.next_id;
  {
    crtweb_wire_buffer_add add = {0};
    add.header.type = CRTWEB_P2C_BUFFER_ADD;
    add.header.size = sizeof add;
    add.buffer_id = free_slot->id;
    add.width = width;
    add.height = height;
    add.stride = free_slot->stride;
    add.pixel_format = CRTWEB_PIXEL_ARGB8888;
    add.size = free_slot->size;
    send_message(&add, sizeof add, free_slot->fd);
    free_slot->announced = TRUE;
  }
  return free_slot;
}

/* ---- frames ------------------------------------------------------------------------------- */

static void present_pending(void);

static void complete_inflight(void) {
  WPEBuffer* buffer = state.inflight;

  if (state.ack_timer) {
    g_source_remove(state.ack_timer);
    state.ack_timer = 0;
  }
  state.inflight = NULL;
  if (buffer) {
    if (state.view) {
      wpe_view_buffer_rendered(state.view, buffer);
      wpe_view_buffer_released(state.view, buffer);
    }
    g_object_unref(buffer);
  }
  present_pending();
}

static gboolean ack_timeout(gpointer user_data) {
  (void)user_data;
  state.ack_timer = 0;
  g_message("crt platform: consumer did not acknowledge frame %" G_GUINT64_FORMAT " in %d ms; continuing",
            state.inflight_serial, ACK_TIMEOUT_MS);
  complete_inflight();
  return G_SOURCE_REMOVE;
}

/* Copies `buffer` into a free pool buffer and announces it. Returns FALSE when the pool is
 * exhausted (the consumer holds every buffer): the caller keeps the commit pending. */
static gboolean present(WPEBuffer* buffer) {
  GError* error = NULL;
  GBytes* pixels = wpe_buffer_import_to_pixels(buffer, &error); /* (transfer none): owned by `buffer` */
  gsize length = 0;
  const uint8_t* source;
  uint32_t width = (uint32_t)wpe_buffer_get_width(buffer);
  uint32_t height = (uint32_t)wpe_buffer_get_height(buffer);
  guint source_stride = WPE_IS_BUFFER_SHM(buffer) ? wpe_buffer_shm_get_stride(WPE_BUFFER_SHM(buffer)) : width * 4u;
  PoolBuffer* target;
  uint32_t row;
  crtweb_wire_frame frame = {0};

  if (pixels == NULL) {
    g_warning("crt platform: cannot read the committed buffer: %s", error ? error->message : "?");
    g_clear_error(&error);
    return TRUE; /* nothing to present; drop it */
  }
  source = g_bytes_get_data(pixels, &length);
  if (length < (gsize)source_stride * height) {
    return TRUE;
  }
  target = pool_acquire(width, height);
  if (target == NULL) {
    return FALSE;
  }
  for (row = 0; row < height; ++row) {
    memcpy(target->map + (size_t)row * target->stride, source + (size_t)row * source_stride, (size_t)width * 4u);
  }
  target->held = TRUE;
  frame.header.type = CRTWEB_P2C_FRAME;
  frame.header.size = sizeof frame;
  frame.buffer_id = target->id;
  frame.flags = CRTWEB_FRAME_FULL_DAMAGE;
  frame.serial = ++state.serial;
  frame.damage_serial = ++state.damage_serial;
  frame.damage_width = (int32_t)width;
  frame.damage_height = (int32_t)height;
  frame.time_ns = now_ns();
  state.inflight = g_object_ref(buffer);
  state.inflight_serial = frame.serial;
  send_message(&frame, sizeof frame, -1);
  state.ack_timer = g_timeout_add(ACK_TIMEOUT_MS, ack_timeout, NULL);
  return TRUE;
}

static void present_pending(void) {
  WPEBuffer* buffer = state.pending;

  if (buffer == NULL || state.inflight != NULL) {
    return;
  }
  state.pending = NULL;
  if (!present(buffer)) {
    state.pending = buffer; /* still no free pool buffer: wait for a RELEASE */
    return;
  }
  g_object_unref(buffer);
}

/* ---- consumer messages -> WPE ------------------------------------------------------------- */

static WPEModifiers wpe_modifiers(uint32_t wire) {
  WPEModifiers modifiers = 0;
  if (wire & CRTWEB_MOD_CONTROL) modifiers |= WPE_MODIFIER_KEYBOARD_CONTROL;
  if (wire & CRTWEB_MOD_SHIFT) modifiers |= WPE_MODIFIER_KEYBOARD_SHIFT;
  if (wire & CRTWEB_MOD_ALT) modifiers |= WPE_MODIFIER_KEYBOARD_ALT;
  if (wire & CRTWEB_MOD_META) modifiers |= WPE_MODIFIER_KEYBOARD_META;
  if (wire & CRTWEB_MOD_CAPS_LOCK) modifiers |= WPE_MODIFIER_KEYBOARD_CAPS_LOCK;
  return modifiers;
}

static WPEModifiers button_mask(guint button) {
  switch (button) {
    case 1: return WPE_MODIFIER_POINTER_BUTTON1;
    case 2: return WPE_MODIFIER_POINTER_BUTTON2;
    case 3: return WPE_MODIFIER_POINTER_BUTTON3;
    default: return 0;
  }
}

static void dispatch_event(WPEEvent* event) {
  if (event == NULL) {
    return;
  }
  if (state.view) {
    wpe_view_event(state.view, event);
  }
  wpe_event_unref(event);
}

static void handle_pointer(const crtweb_wire_pointer* p) {
  WPEView* view = state.view;
  WPEInputSource source = p->pointer_type == CRTWEB_POINTER_TOUCH   ? WPE_INPUT_SOURCE_TOUCHSCREEN
                          : p->pointer_type == CRTWEB_POINTER_PEN ? WPE_INPUT_SOURCE_PEN
                                                                  : WPE_INPUT_SOURCE_MOUSE;
  WPEModifiers modifiers = wpe_modifiers(p->modifiers);
  guint32 time = p->time_ms ? p->time_ms : now_ms();

  if (view == NULL) {
    return;
  }
  switch (p->action) {
    case CRTWEB_POINTER_MOVE:
      dispatch_event(wpe_event_pointer_move_new(WPE_EVENT_POINTER_MOVE, view, source, time, modifiers, p->x, p->y, 0, 0));
      break;
    case CRTWEB_POINTER_ENTER:
      dispatch_event(wpe_event_pointer_move_new(WPE_EVENT_POINTER_ENTER, view, source, time, modifiers, p->x, p->y, 0, 0));
      break;
    case CRTWEB_POINTER_LEAVE:
      dispatch_event(wpe_event_pointer_move_new(WPE_EVENT_POINTER_LEAVE, view, source, time, modifiers, p->x, p->y, 0, 0));
      break;
    case CRTWEB_POINTER_DOWN:
      dispatch_event(wpe_event_pointer_button_new(WPE_EVENT_POINTER_DOWN, view, source, time,
                                                  modifiers | button_mask(p->button), p->button, p->x, p->y,
                                                  p->press_count ? p->press_count : 1));
      break;
    case CRTWEB_POINTER_UP:
      dispatch_event(wpe_event_pointer_button_new(WPE_EVENT_POINTER_UP, view, source, time, modifiers, p->button,
                                                  p->x, p->y, 0));
      break;
    default:
      break;
  }
}

static void handle_key(uint32_t down, uint32_t keycode, uint32_t keyval, uint32_t modifiers, uint32_t time_ms) {
  if (g_getenv("CRTWEB_PLATFORM_TRACE")) g_message("crt platform: key down=%u keycode=%u keyval=%#x mods=%u view=%p", down, keycode, keyval, modifiers, (void*)state.view);
  if (state.view == NULL) {
    return;
  }
  dispatch_event(wpe_event_keyboard_new(down ? WPE_EVENT_KEYBOARD_KEY_DOWN : WPE_EVENT_KEYBOARD_KEY_UP, state.view,
                                        WPE_INPUT_SOURCE_KEYBOARD, time_ms ? time_ms : now_ms(),
                                        wpe_modifiers(modifiers), keycode, keyval));
}

/* US-layout evdev key code (+8: the XKB keycode WPE/WebKit expect) for an ASCII character. */
static gboolean us_layout_keycode(gunichar ch, uint32_t* keycode, uint32_t* modifiers) {
  static const char row_q[] = "qwertyuiop";   /* evdev 16..25 */
  static const char row_a[] = "asdfghjkl";    /* evdev 30..38 */
  static const char row_z[] = "zxcvbnm";      /* evdev 44..50 */
  static const char digits[] = "1234567890";  /* evdev 2..11 */
  static const char shifted_digits[] = "!@#$%^&*()";
  const char* hit;
  gunichar lower = ch;
  uint32_t evdev = 0;
  uint32_t shift = 0;

  *modifiers = 0;
  if (ch >= 'A' && ch <= 'Z') {
    lower = ch - 'A' + 'a';
    shift = CRTWEB_MOD_SHIFT;
  }
  if (lower < 0x80 && lower != 0) {
    char c = (char)lower;
    if ((hit = strchr(row_q, c)) != NULL) evdev = 16 + (uint32_t)(hit - row_q);
    else if ((hit = strchr(row_a, c)) != NULL) evdev = 30 + (uint32_t)(hit - row_a);
    else if ((hit = strchr(row_z, c)) != NULL) evdev = 44 + (uint32_t)(hit - row_z);
    else if ((hit = strchr(digits, c)) != NULL) evdev = 2 + (uint32_t)(hit - digits);
    else if ((hit = strchr(shifted_digits, c)) != NULL) { evdev = 2 + (uint32_t)(hit - shifted_digits); shift = CRTWEB_MOD_SHIFT; }
    else if (c == ' ') evdev = 57;
    else if (c == '-') evdev = 12;
    else if (c == '=') evdev = 13;
    else if (c == '.') evdev = 52;
    else if (c == ',') evdev = 51;
    else if (c == '/') evdev = 53;
    else if (c == ';') evdev = 39;
  }
  if (evdev == 0) {
    return FALSE;
  }
  *keycode = evdev + 8;
  *modifiers = shift;
  return TRUE;
}

static void handle_message(const void* data, int fd) {
  const crtweb_wire_header* header = data;

  if (fd >= 0) {
    close(fd); /* the consumer sends no descriptors */
  }
  switch (header->type) {
    case CRTWEB_C2P_FRAME_ACK: {
      const crtweb_wire_frame_ack* ack = data;
      if (state.inflight && ack->serial == state.inflight_serial) {
        state.acked_serial = ack->serial;
        complete_inflight();
      }
      break;
    }
    case CRTWEB_C2P_BUFFER_RELEASE: {
      const crtweb_wire_buffer_release* release = data;
      guint i;
      for (i = 0; i < CRTWEB_POOL_SIZE; ++i) {
        if (state.pool[i].map && state.pool[i].id == release->buffer_id) {
          state.pool[i].held = FALSE;
        }
      }
      present_pending();
      break;
    }
    case CRTWEB_C2P_RESIZE: {
      const crtweb_wire_resize* resize = data;
      state.width = resize->width;
      state.height = resize->height;
      if (resize->scale_percent && resize->scale_percent != state.scale_percent) {
        state.scale_percent = resize->scale_percent;
        if (state.toplevel) {
          wpe_toplevel_scale_changed(state.toplevel, state.scale_percent / 100.0);
        }
      }
      if (state.toplevel) {
        wpe_toplevel_resize(state.toplevel, (int)resize->width, (int)resize->height);
      }
      break;
    }
    case CRTWEB_C2P_FOCUS: {
      const crtweb_wire_focus* focus = data;
      if (state.view) {
        if (focus->focused) {
          wpe_view_focus_in(state.view);
        } else {
          wpe_view_focus_out(state.view);
        }
      }
      break;
    }
    case CRTWEB_C2P_POINTER:
      handle_pointer(data);
      break;
    case CRTWEB_C2P_WHEEL: {
      const crtweb_wire_wheel* wheel = data;
      if (state.view) {
        dispatch_event(wpe_event_scroll_new(state.view, WPE_INPUT_SOURCE_MOUSE, wheel->time_ms ? wheel->time_ms : now_ms(),
                                            wpe_modifiers(wheel->modifiers), wheel->delta_x, wheel->delta_y, TRUE,
                                            FALSE, wheel->x, wheel->y));
      }
      break;
    }
    case CRTWEB_C2P_KEY: {
      const crtweb_wire_key* key = data;
      handle_key(key->down, key->keycode, key->keyval, key->modifiers, key->time_ms);
      break;
    }
    case CRTWEB_C2P_TEXT: {
      /* Committed text, Latin only until the IME/composition work (deferred past Web v1): one code
       * point becomes a press/release of its keysym on the key a US layout types it with. WebKit
       * drops key events without a hardware key code, so a character with no such key is dropped. */
      const crtweb_wire_text* text = data;
      gunichar ch = g_utf8_get_char_validated(text->utf8, -1);
      if (ch != (gunichar)-1 && ch != (gunichar)-2 && ch != 0) {
        uint32_t keycode = 0, modifiers = 0;
        if (us_layout_keycode(ch, &keycode, &modifiers)) {
          handle_key(1, keycode, ch, modifiers, 0);
          handle_key(0, keycode, ch, modifiers, 0);
        } else {
          g_message("crt platform: no US-layout key for U+%04X; text input needs the IME path", ch);
        }
      }
      break;
    }
    case CRTWEB_C2P_BYE:
      state.closed = TRUE;
      if (state.view) {
        wpe_view_closed(state.view);
      }
      break;
    default:
      g_warning("crt platform: unknown message %u", header->type);
      break;
  }
}

static gboolean socket_readable(gint fd, GIOCondition condition, gpointer user_data) {
  (void)user_data;
  for (;;) {
    uint8_t buffer[CRTWEB_WIRE_MAX_MESSAGE];
    int received_fd = -1;
    ssize_t got = crtweb_wire_recv(fd, buffer, sizeof buffer, &received_fd);

    if (got > 0) {
      handle_message(buffer, received_fd);
      continue;
    }
    if (got < 0 && errno == EAGAIN) {
      return G_SOURCE_CONTINUE;
    }
    if (got == 0 || (condition & (G_IO_HUP | G_IO_ERR)) || errno != EAGAIN) {
      g_message("crt platform: consumer connection closed");
      state.closed = TRUE;
      state.watch = 0;
      if (state.view) {
        wpe_view_closed(state.view);
      }
      return G_SOURCE_REMOVE;
    }
  }
}

/* ---- toplevel ----------------------------------------------------------------------------- */

typedef struct { WPEToplevel parent_instance; } WPEToplevelCRT;
typedef struct { WPEToplevelClass parent_class; } WPEToplevelCRTClass;
G_DEFINE_TYPE(WPEToplevelCRT, wpe_toplevel_crt, WPE_TYPE_TOPLEVEL)

static void toplevel_constructed(GObject* object) {
  G_OBJECT_CLASS(wpe_toplevel_crt_parent_class)->constructed(object);
  state.toplevel = WPE_TOPLEVEL(object);
  g_object_add_weak_pointer(object, (gpointer*)&state.toplevel);
  wpe_toplevel_state_changed(WPE_TOPLEVEL(object), WPE_TOPLEVEL_STATE_ACTIVE);
  if (state.width && state.height) {
    wpe_toplevel_resized(WPE_TOPLEVEL(object), (int)state.width, (int)state.height);
  }
  if (state.scale_percent && state.scale_percent != 100) {
    wpe_toplevel_scale_changed(WPE_TOPLEVEL(object), state.scale_percent / 100.0);
  }
}

static gboolean resize_view(WPEToplevel* toplevel, WPEView* view, gpointer user_data) {
  int width, height;
  (void)user_data;
  wpe_toplevel_get_size(toplevel, &width, &height);
  wpe_view_resized(view, width, height);
  return FALSE;
}

static gboolean toplevel_resize(WPEToplevel* toplevel, int width, int height) {
  wpe_toplevel_resized(toplevel, width, height);
  wpe_toplevel_foreach_view(toplevel, resize_view, NULL);
  return TRUE;
}

static void toplevel_set_title(WPEToplevel* toplevel, const char* title) {
  crtweb_wire_title message = {0};
  (void)toplevel;
  if (title == NULL) {
    return;
  }
  g_strlcpy(message.utf8, title, sizeof message.utf8);
  SEND(CRTWEB_P2C_TITLE, message);
}

static void wpe_toplevel_crt_class_init(WPEToplevelCRTClass* klass) {
  G_OBJECT_CLASS(klass)->constructed = toplevel_constructed;
  WPE_TOPLEVEL_CLASS(klass)->resize = toplevel_resize;
  WPE_TOPLEVEL_CLASS(klass)->set_title = toplevel_set_title;
}

static void wpe_toplevel_crt_init(WPEToplevelCRT* self) { (void)self; }

/* ---- view --------------------------------------------------------------------------------- */

typedef struct { WPEView parent_instance; } WPEViewCRT;
typedef struct { WPEViewClass parent_class; } WPEViewCRTClass;
G_DEFINE_TYPE(WPEViewCRT, wpe_view_crt, WPE_TYPE_VIEW)

static void view_toplevel_changed(WPEView* view, GParamSpec* spec, gpointer user_data) {
  WPEToplevel* toplevel = wpe_view_get_toplevel(view);
  (void)spec;
  (void)user_data;
  if (toplevel == NULL) {
    wpe_view_unmap(view);
    return;
  }
  {
    int width, height;
    wpe_toplevel_get_size(toplevel, &width, &height);
    if (width && height) {
      wpe_view_resized(view, width, height);
    }
  }
  wpe_view_map(view);
}

static void view_constructed(GObject* object) {
  G_OBJECT_CLASS(wpe_view_crt_parent_class)->constructed(object);
  state.view = WPE_VIEW(object);
  g_object_add_weak_pointer(object, (gpointer*)&state.view);
  g_signal_connect(object, "notify::toplevel", G_CALLBACK(view_toplevel_changed), NULL);
}

static void view_dispose(GObject* object) {
  if (state.inflight) {
    g_clear_object(&state.inflight);
  }
  if (state.pending) {
    g_clear_object(&state.pending);
  }
  if (state.ack_timer) {
    g_source_remove(state.ack_timer);
    state.ack_timer = 0;
  }
  G_OBJECT_CLASS(wpe_view_crt_parent_class)->dispose(object);
}

static gboolean view_render_buffer(WPEView* view, WPEBuffer* buffer, const WPERectangle* damage, guint n_damage,
                                   GError** error) {
  (void)view;
  (void)damage;
  (void)n_damage;
  (void)error;
  if (state.closed) {
    return TRUE;
  }
  if (state.pending) {
    /* Latest wins: the older, never-shown commit goes straight back to WebKit. */
    wpe_view_buffer_released(view, state.pending);
    g_object_unref(state.pending);
    state.pending = NULL;
  }
  state.pending = g_object_ref(buffer);
  present_pending();
  return TRUE;
}

static void wpe_view_crt_class_init(WPEViewCRTClass* klass) {
  GObjectClass* object_class = G_OBJECT_CLASS(klass);
  object_class->constructed = view_constructed;
  object_class->dispose = view_dispose;
  WPE_VIEW_CLASS(klass)->render_buffer = view_render_buffer;
}

static void wpe_view_crt_init(WPEViewCRT* self) { (void)self; }

/* ---- display ------------------------------------------------------------------------------ */

typedef struct { WPEDisplay parent_instance; } WPEDisplayCRT;
typedef struct { WPEDisplayClass parent_class; } WPEDisplayCRTClass;
G_DEFINE_TYPE(WPEDisplayCRT, wpe_display_crt, WPE_TYPE_DISPLAY)

static gboolean display_connect(WPEDisplay* display, GError** error) {
  const char* path = g_getenv("CRTWEB_SURFACE_SOCKET");
  struct sockaddr_un address;
  gint64 deadline = g_get_monotonic_time() + (gint64)CONNECT_TIMEOUT_MS * 1000;
  int fd = -1;
  crtweb_wire_hello hello;
  int received_fd = -1;
  crtweb_wire_config config = {0};

  (void)display;
  if (state.socket >= 0) {
    return TRUE;
  }
  if (path == NULL || *path == '\0' || strlen(path) >= sizeof address.sun_path) {
    g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED,
                        "CRTWEB_SURFACE_SOCKET is not set to a socket path");
    return FALSE;
  }
  memset(&address, 0, sizeof address);
  address.sun_family = AF_UNIX;
  strcpy(address.sun_path, path);
  for (;;) {
    fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if (fd >= 0 && connect(fd, (struct sockaddr*)&address, sizeof address) == 0) {
      break;
    }
    if (fd >= 0) {
      close(fd);
      fd = -1;
    }
    if (g_get_monotonic_time() > deadline) {
      g_set_error(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED,
                  "no CRT surface consumer at %s", path);
      return FALSE;
    }
    g_usleep(50 * 1000);
  }
  /* The consumer speaks first: size and scale of the window that will show the frames. */
  if (crtweb_wire_recv(fd, &hello, sizeof hello, &received_fd) != (ssize_t)sizeof hello ||
      hello.header.type != CRTWEB_C2P_HELLO || hello.version != CRTWEB_WIRE_VERSION) {
    g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED,
                        "the CRT surface consumer sent no valid HELLO");
    close(fd);
    return FALSE;
  }
  if (received_fd >= 0) {
    close(received_fd);
  }
  state.width = hello.width;
  state.height = hello.height;
  state.scale_percent = hello.scale_percent ? hello.scale_percent : 100;
  config.header.type = CRTWEB_P2C_CONFIG;
  config.header.size = sizeof config;
  config.version = CRTWEB_WIRE_VERSION;
  config.pool_size = CRTWEB_POOL_SIZE;
  config.pixel_format = CRTWEB_PIXEL_ARGB8888;
  if (crtweb_wire_send(fd, &config, sizeof config, -1) != 0) {
    g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "CONFIG send failed");
    close(fd);
    return FALSE;
  }
  {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  }
  state.socket = fd;
  for (guint i = 0; i < CRTWEB_POOL_SIZE; ++i) {
    state.pool[i].fd = -1;
  }
  state.watch = g_unix_fd_add(fd, G_IO_IN | G_IO_HUP | G_IO_ERR, socket_readable, NULL);
  return TRUE;
}

static WPEView* display_create_view(WPEDisplay* display) {
  return WPE_VIEW(g_object_new(wpe_view_crt_get_type(), "display", display, NULL));
}

static WPEToplevel* display_create_toplevel(WPEDisplay* display, guint max_views) {
  (void)max_views;
  return WPE_TOPLEVEL(g_object_new(wpe_toplevel_crt_get_type(), "display", display, NULL));
}

static void wpe_display_crt_class_init(WPEDisplayCRTClass* klass) {
  WPEDisplayClass* display_class = WPE_DISPLAY_CLASS(klass);
  display_class->connect = display_connect;
  display_class->create_view = display_create_view;
  display_class->create_toplevel = display_create_toplevel;
  /* No get_egl_display / get_drm_device: WebKit falls back to shared-memory buffers, which is the
   * 3A output path. GPU buffers arrive with the GPU-buffer contract (Tranche 9). */
}

static void wpe_display_crt_init(WPEDisplayCRT* self) { (void)self; }

/* ---- GIO module entry points -------------------------------------------------------------- */

G_MODULE_EXPORT void g_io_module_load(GIOModule* module) {
  g_type_module_use(G_TYPE_MODULE(module)); /* keep resident: the types are static */
  g_io_extension_point_implement(WPE_DISPLAY_EXTENSION_POINT_NAME, wpe_display_crt_get_type(), "crt", 100);
}

G_MODULE_EXPORT void g_io_module_unload(GIOModule* module) { (void)module; }

G_MODULE_EXPORT char** g_io_module_query(void) {
  char* eps[] = {(char*)WPE_DISPLAY_EXTENSION_POINT_NAME, NULL};
  return g_strdupv(eps);
}
