/* crtui core: the CRT-owned widget tree, event routing, focus, and geometry
 * model behind crtui/ui.h (the Tranche 0 contract, docs/crtui_acceptance.md).
 * Headless and host-neutral: no window, GPU or LVGL. Later tranches render
 * this model through a private LVGL backend without changing these rules. */

#include "crtui/ui.h"

#include "render_tree.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define CRTUI_MAX_DEPTH 64
#define CRTUI_MAX_TEXT_INPUT 1024
#define CRTUI_SCROLL_STEP 24

typedef struct crtui_node {
  uint32_t generation; /* never 0 once used; bumped on destroy */
  int in_use;
  crtui_widget_kind kind;
  crtui_widget parent;
  crtui_widget* children;
  size_t child_count;
  size_t child_capacity;
  int32_t x, y, width, height;
  int visible;
  int enabled;
  int focusable;
  char* text;
  int32_t value, min_value, max_value;
  crtui_event_fn callback;
  void* callback_user;
  /* window only */
  int32_t window_width, window_height;
  float dpi_scale;
  int modal;
  uint64_t modal_order;
  crtui_lvgl_backend* backend; /* window only; created on first render */
  /* Layout (Tranche 4). x,y,width,height above are the *resolved* geometry; the
   * request is what the application asked for (CRTUI_SIZE_FILL allowed). */
  int32_t req_width, req_height;
  int32_t margin[4];  /* left, top, right, bottom */
  int32_t padding[4]; /* containers: left, top, right, bottom */
  int32_t gap;
  crtui_main_align main_align;
  int32_t grow;
  crtui_align align_h, align_v;
  int32_t scroll_y, content_height; /* ScrollView / List */
  crtui_style style;
  char* placeholder;   /* TextInput */
  size_t caret;        /* TextInput: byte offset */
  uint8_t* image_pixels; /* Image: tightly packed BGRA8888 */
  int32_t image_width, image_height;
  /* SurfaceView (Tranche 5): producer-independent damage state. The producer
   * itself is deliberately not stored or owned by crtui. */
  int surface_damage_full;
  int surface_has_damage;
  int32_t surface_damage[4]; /* view-local x, y, width, height */
  uint64_t surface_damage_serial;
} crtui_node;

typedef struct crtui_posted {
  crtui_post_fn fn;
  void* user;
} crtui_posted;

struct crtui_context {
  pthread_t ui_thread;

  pthread_mutex_t post_lock;
  crtui_posted* posted;
  size_t posted_count;
  size_t posted_capacity;

  crtui_node* nodes;
  size_t node_count;
  size_t node_capacity;

  crtui_widget focus;
  crtui_widget pressed; /* widget that took the current pointer press */
  crtui_widget capture; /* slider currently capturing the pointer */
  uint64_t version;     /* bumped by every change a renderer could see */
  uint64_t layout_version; /* the version the last layout pass ran for */
  uint64_t modal_counter;

  crtui_input* input_queue;
  size_t input_count;
  size_t input_capacity;
  int processing; /* inside process/drain: send_input queues instead */
};

/* ---- helpers ------------------------------------------------------------ */

static int on_ui_thread(const crtui_context* context) {
  return pthread_equal(pthread_self(), context->ui_thread);
}

static crtui_node* resolve(crtui_context* context, crtui_widget id) {
  if (id == CRTUI_INVALID_WIDGET) {
    return NULL;
  }
  uint32_t index = (uint32_t)(id & 0xffffffffu);
  uint32_t generation = (uint32_t)(id >> 32);
  if (index == 0 || (size_t)index > context->node_count) {
    return NULL;
  }
  crtui_node* node = &context->nodes[index - 1];
  if (!node->in_use || node->generation != generation) {
    return NULL;
  }
  return node;
}

static crtui_widget id_of(const crtui_context* context, const crtui_node* node) {
  return ((uint64_t)node->generation << 32) | (uint64_t)((size_t)(node - context->nodes) + 1);
}

static int is_surface_kind(crtui_widget_kind kind) {
  return kind == CRTUI_WIDGET_SURFACE_VIEW || kind == CRTUI_WIDGET_MEDIA_VIEW;
}

static void mark_surface_full(crtui_node* node) {
  node->surface_damage_full = 1;
  node->surface_has_damage = 1;
  if (++node->surface_damage_serial == 0) ++node->surface_damage_serial;
}

static void touch(crtui_context* context) {
  ++context->version;
  /* Any scene mutation may move, clip, cover or reveal a SurfaceView. Full
   * damage is conservative but correct; producer-only damage uses the focused
   * API below and does not rebuild the LVGL widget plane. */
  for (size_t i = 0; i < context->node_count; ++i) {
    if (context->nodes[i].in_use && is_surface_kind(context->nodes[i].kind)) {
      mark_surface_full(&context->nodes[i]);
    }
  }
}

static int is_container_kind(crtui_widget_kind kind) {
  return kind == CRTUI_WIDGET_WINDOW || kind == CRTUI_WIDGET_CONTAINER || kind == CRTUI_WIDGET_ROW ||
         kind == CRTUI_WIDGET_COLUMN || kind == CRTUI_WIDGET_STACK || kind == CRTUI_WIDGET_SCROLL_VIEW ||
         kind == CRTUI_WIDGET_LIST;
}

static int is_scrolling_kind(crtui_widget_kind kind) {
  return kind == CRTUI_WIDGET_SCROLL_VIEW || kind == CRTUI_WIDGET_LIST;
}

static int32_t max0(int32_t value) { return value < 0 ? 0 : value; }

static void ensure_layout(crtui_context* context);
static void scroll_into_view(crtui_context* context, crtui_widget id);
static int abs_rect(crtui_context* context, crtui_widget id, int32_t* x, int32_t* y, int32_t* w, int32_t* h);

/* Frees a window's private renderer (a no-op in a build without LVGL). */
static void release_backend(crtui_node* node) {
#ifdef CRTUI_HAVE_LVGL
  if (node->backend != NULL) {
    crtui_lvgl_backend_destroy(node->backend);
  }
#endif
  node->backend = NULL;
}

#define CRTUI_CHECK_CONTEXT(context)                       \
  do {                                                     \
    if ((context) == NULL) return CRTUI_ERROR_INVALID_ARGUMENT; \
    if (!on_ui_thread(context)) return CRTUI_ERROR_WRONG_THREAD; \
  } while (0)

static crtui_widget window_of(crtui_context* context, crtui_widget id) {
  for (int depth = 0; depth < CRTUI_MAX_DEPTH + 2; ++depth) {
    crtui_node* node = resolve(context, id);
    if (node == NULL) {
      return CRTUI_INVALID_WIDGET;
    }
    if (node->kind == CRTUI_WIDGET_WINDOW) {
      return id;
    }
    id = node->parent;
  }
  return CRTUI_INVALID_WIDGET;
}

static int chain_visible_enabled(crtui_context* context, crtui_widget id, int need_enabled) {
  for (int depth = 0; depth < CRTUI_MAX_DEPTH + 2 && id != CRTUI_INVALID_WIDGET; ++depth) {
    crtui_node* node = resolve(context, id);
    if (node == NULL || !node->visible || (need_enabled && !node->enabled)) {
      return 0;
    }
    id = node->parent;
  }
  return 1;
}

static crtui_node* topmost_modal(crtui_context* context) {
  crtui_node* best = NULL;
  for (size_t i = 0; i < context->node_count; ++i) {
    crtui_node* node = &context->nodes[i];
    if (node->in_use && node->kind == CRTUI_WIDGET_WINDOW && node->modal &&
        (best == NULL || node->modal_order > best->modal_order)) {
      best = node;
    }
  }
  return best;
}

/* True if input/focus may reach `window` right now (no other modal blocks it). */
static int window_permitted(crtui_context* context, crtui_widget window) {
  crtui_node* modal = topmost_modal(context);
  return modal == NULL || id_of(context, modal) == window;
}

static crtui_result alloc_node(crtui_context* context, crtui_widget_kind kind, crtui_widget parent, crtui_node** out) {
  crtui_node* node = NULL;
  for (size_t i = 0; i < context->node_count; ++i) {
    if (!context->nodes[i].in_use) {
      node = &context->nodes[i];
      break;
    }
  }
  if (node == NULL) {
    if (context->node_count == context->node_capacity) {
      size_t capacity = context->node_capacity == 0 ? 32 : context->node_capacity * 2;
      crtui_node* grown = (crtui_node*)realloc(context->nodes, capacity * sizeof(*grown));
      if (grown == NULL) {
        return CRTUI_ERROR_IO;
      }
      context->nodes = grown;
      context->node_capacity = capacity;
    }
    node = &context->nodes[context->node_count++];
    memset(node, 0, sizeof(*node));
  }
  uint32_t generation = node->generation + 1;
  if (generation == 0) {
    generation = 1;
  }
  memset(node, 0, sizeof(*node));
  node->generation = generation;
  node->in_use = 1;
  node->kind = kind;
  node->parent = parent;
  node->visible = 1;
  node->enabled = 1;
  node->focusable = (kind == CRTUI_WIDGET_BUTTON || kind == CRTUI_WIDGET_SLIDER || kind == CRTUI_WIDGET_SWITCH ||
                     kind == CRTUI_WIDGET_CHECKBOX || kind == CRTUI_WIDGET_TEXT_INPUT);
  node->dpi_scale = 1.0f;
  *out = node;
  return CRTUI_OK;
}

static crtui_result attach_child(crtui_context* context, crtui_widget parent_id, crtui_widget child_id) {
  crtui_node* parent = resolve(context, parent_id);
  if (parent == NULL) {
    return CRTUI_ERROR_INVALID_HANDLE;
  }
  if (parent->child_count == parent->child_capacity) {
    size_t capacity = parent->child_capacity == 0 ? 4 : parent->child_capacity * 2;
    crtui_widget* grown = (crtui_widget*)realloc(parent->children, capacity * sizeof(*grown));
    if (grown == NULL) {
      return CRTUI_ERROR_IO;
    }
    parent->children = grown;
    parent->child_capacity = capacity;
  }
  parent->children[parent->child_count++] = child_id;
  return CRTUI_OK;
}

static crtui_result create_child(
    crtui_context* context, crtui_widget_kind kind, crtui_widget parent_id, crtui_widget* out, crtui_node** out_node) {
  if (out == NULL) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  *out = CRTUI_INVALID_WIDGET;
  crtui_node* parent = resolve(context, parent_id);
  if (parent == NULL) {
    return CRTUI_ERROR_INVALID_HANDLE;
  }
  if (!is_container_kind(parent->kind)) {
    return CRTUI_ERROR_INVALID_ARGUMENT; /* only windows and containers hold children */
  }
  crtui_node* node = NULL;
  crtui_result result = alloc_node(context, kind, parent_id, &node);
  if (result != CRTUI_OK) {
    return result;
  }
  crtui_widget id = id_of(context, node);
  result = attach_child(context, parent_id, id); /* may not move nodes: nodes array untouched here */
  if (result != CRTUI_OK) {
    node->in_use = 0;
    return result;
  }
  *out = id;
  touch(context);
  if (out_node != NULL) {
    *out_node = resolve(context, id);
  }
  return CRTUI_OK;
}

/* ---- focus -------------------------------------------------------------- */

typedef struct crtui_id_list {
  crtui_widget* ids;
  size_t count;
  size_t capacity;
} crtui_id_list;

static int list_push(crtui_id_list* list, crtui_widget id) {
  if (list->count == list->capacity) {
    size_t capacity = list->capacity == 0 ? 16 : list->capacity * 2;
    crtui_widget* grown = (crtui_widget*)realloc(list->ids, capacity * sizeof(*grown));
    if (grown == NULL) {
      return -1;
    }
    list->ids = grown;
    list->capacity = capacity;
  }
  list->ids[list->count++] = id;
  return 0;
}

static void collect_focus_order(crtui_context* context, crtui_widget id, int depth, crtui_id_list* list) {
  crtui_node* node = resolve(context, id);
  if (node == NULL || depth > CRTUI_MAX_DEPTH || !node->visible || !node->enabled) {
    return;
  }
  if (node->focusable && node->kind != CRTUI_WIDGET_WINDOW) {
    list_push(list, id);
  }
  for (size_t i = 0; i < node->child_count; ++i) {
    collect_focus_order(context, node->children[i], depth + 1, list);
  }
}

static void deliver(crtui_context* context, crtui_widget target, const crtui_event* event, int bubble, int* out_handled);

static void set_focus_internal(crtui_context* context, crtui_widget next) {
  crtui_widget previous = context->focus;
  if (previous == next) {
    return;
  }
  context->focus = next;
  touch(context);
  if (next != CRTUI_INVALID_WIDGET) {
    scroll_into_view(context, next);
  }
  if (previous != CRTUI_INVALID_WIDGET && resolve(context, previous) != NULL) {
    crtui_event out;
    memset(&out, 0, sizeof(out));
    out.type = CRTUI_EVENT_FOCUS_OUT;
    out.target = previous;
    deliver(context, previous, &out, 0, NULL);
  }
  if (next != CRTUI_INVALID_WIDGET && context->focus == next && resolve(context, next) != NULL) {
    crtui_event in;
    memset(&in, 0, sizeof(in));
    in.type = CRTUI_EVENT_FOCUS_IN;
    in.target = next;
    deliver(context, next, &in, 0, NULL);
  }
}

typedef struct crtui_focus_snapshot {
  crtui_id_list order;
  size_t index; /* position of the focus in `order`, or (size_t)-1 */
  crtui_widget window;
  crtui_widget focus_before;
} crtui_focus_snapshot;

static void focus_snapshot_begin(crtui_context* context, crtui_focus_snapshot* snapshot) {
  memset(snapshot, 0, sizeof(*snapshot));
  snapshot->index = (size_t)-1;
  snapshot->focus_before = context->focus;
  if (context->focus == CRTUI_INVALID_WIDGET) {
    return;
  }
  snapshot->window = window_of(context, context->focus);
  collect_focus_order(context, snapshot->window, 0, &snapshot->order);
  for (size_t i = 0; i < snapshot->order.count; ++i) {
    if (snapshot->order.ids[i] == context->focus) {
      snapshot->index = i;
      break;
    }
  }
}

/* After a mutation: if the focused widget is no longer eligible, move to the
 * next eligible widget after it in the pre-mutation order (wrapping), else clear. */
static void focus_snapshot_end(crtui_context* context, crtui_focus_snapshot* snapshot) {
  crtui_widget focus = snapshot->focus_before;
  /* Only act if the focus we recorded is still what the context holds, or was
   * cleared by the mutation itself (a destroyed widget clears it). */
  if (focus != CRTUI_INVALID_WIDGET && (context->focus == focus || context->focus == CRTUI_INVALID_WIDGET)) {
    crtui_node* node = resolve(context, focus);
    int eligible = context->focus == focus && node != NULL && node->focusable &&
                   chain_visible_enabled(context, focus, 1) &&
                   window_permitted(context, window_of(context, focus));
    if (!eligible) {
      crtui_widget next = CRTUI_INVALID_WIDGET;
      if (snapshot->index != (size_t)-1 && resolve(context, snapshot->window) != NULL) {
        for (size_t step = 1; step <= snapshot->order.count && next == CRTUI_INVALID_WIDGET; ++step) {
          crtui_widget candidate = snapshot->order.ids[(snapshot->index + step) % snapshot->order.count];
          crtui_node* candidate_node = resolve(context, candidate);
          if (candidate != focus && candidate_node != NULL && candidate_node->focusable &&
              chain_visible_enabled(context, candidate, 1) && window_permitted(context, snapshot->window)) {
            next = candidate;
          }
        }
      }
      set_focus_internal(context, next);
    }
  }
  free(snapshot->order.ids);
  memset(snapshot, 0, sizeof(*snapshot));
}

/* ---- event delivery ----------------------------------------------------- */

static void deliver(crtui_context* context, crtui_widget target, const crtui_event* event, int bubble, int* out_handled) {
  crtui_widget chain[CRTUI_MAX_DEPTH];
  size_t count = 0;
  crtui_widget cursor = target;
  while (cursor != CRTUI_INVALID_WIDGET && count < CRTUI_MAX_DEPTH) {
    crtui_node* node = resolve(context, cursor);
    if (node == NULL) {
      break;
    }
    chain[count++] = cursor;
    if (!bubble) {
      break;
    }
    cursor = node->parent;
  }
  int handled = 0;
  for (size_t i = 0; i < count && !handled; ++i) {
    crtui_node* node = resolve(context, chain[i]); /* re-resolve: earlier callbacks may have mutated the tree */
    if (node == NULL || node->callback == NULL) {
      continue;
    }
    crtui_event_fn fn = node->callback;
    void* user = node->callback_user;
    /* No node pointer is used after the callback returns. */
    if (fn(chain[i], event, user) == CRTUI_EVENT_HANDLED) {
      handled = 1;
    }
  }
  if (out_handled != NULL) {
    *out_handled = handled;
  }
}

/* ---- layout ------------------------------------------------------------- */

static void layout_node(crtui_context* context, crtui_widget id, int depth);

/* Free layout (Window, Container): children keep their own x,y; a FILL request
 * snaps that axis to the padded content box. */
static void layout_free(crtui_context* context, crtui_node* node, int32_t cw, int32_t ch) {
  for (size_t i = 0; i < node->child_count; ++i) {
    crtui_node* c = resolve(context, node->children[i]);
    if (c == NULL) continue;
    if (c->req_width == CRTUI_SIZE_FILL) {
      c->width = max0(cw - c->margin[0] - c->margin[2]);
      c->x = node->padding[0] + c->margin[0];
    } else {
      c->width = c->req_width;
    }
    if (c->req_height == CRTUI_SIZE_FILL) {
      c->height = max0(ch - c->margin[1] - c->margin[3]);
      c->y = node->padding[1] + c->margin[1];
    } else {
      c->height = c->req_height;
    }
  }
}

static int32_t align_offset(crtui_align align, int32_t room, int32_t size) {
  if (align == CRTUI_ALIGN_CENTER) return (room - size) / 2;
  if (align == CRTUI_ALIGN_END) return room - size;
  return 0; /* START and STRETCH */
}

static void layout_stack(crtui_context* context, crtui_node* node, int32_t cw, int32_t ch) {
  for (size_t i = 0; i < node->child_count; ++i) {
    crtui_node* c = resolve(context, node->children[i]);
    if (c == NULL || !c->visible) continue;
    int32_t room_w = max0(cw - c->margin[0] - c->margin[2]);
    int32_t room_h = max0(ch - c->margin[1] - c->margin[3]);
    c->width = (c->req_width == CRTUI_SIZE_FILL || c->align_h == CRTUI_ALIGN_STRETCH) ? room_w : max0(c->req_width);
    c->height = (c->req_height == CRTUI_SIZE_FILL || c->align_v == CRTUI_ALIGN_STRETCH) ? room_h : max0(c->req_height);
    c->x = node->padding[0] + c->margin[0] + align_offset(c->align_h, room_w, c->width);
    c->y = node->padding[1] + c->margin[1] + align_offset(c->align_v, room_h, c->height);
  }
}

/* Flex layout along one axis. `scrolling`: the main axis is unbounded (grow is
 * ignored), content_height is recorded and the scroll offset applied. */
static void layout_flex(crtui_context* context, crtui_node* node, int horizontal, int scrolling, int32_t cw, int32_t ch) {
  size_t count = 0;
  int32_t main_avail = horizontal ? cw : ch;
  int32_t cross_avail = horizontal ? ch : cw;
  int32_t pad_main_start = horizontal ? node->padding[0] : node->padding[1];
  int32_t pad_main_end = horizontal ? node->padding[2] : node->padding[3];
  int32_t pad_cross_start = horizontal ? node->padding[1] : node->padding[0];

  int64_t total = 0;
  int64_t grow_sum = 0;
  crtui_widget last_grow = CRTUI_INVALID_WIDGET;
  for (size_t i = 0; i < node->child_count; ++i) {
    crtui_node* c = resolve(context, node->children[i]);
    if (c == NULL || !c->visible) continue;
    ++count;
    int32_t req = horizontal ? c->req_width : c->req_height;
    int32_t grow = c->grow;
    if (req == CRTUI_SIZE_FILL && grow == 0) grow = 1;
    if (scrolling) grow = 0;
    total += max0(req) + (horizontal ? c->margin[0] + c->margin[2] : c->margin[1] + c->margin[3]);
    if (grow > 0) {
      grow_sum += grow;
      last_grow = node->children[i];
    }
  }
  int64_t gaps = count > 1 ? (int64_t)(count - 1) * node->gap : 0;
  int64_t remaining = (int64_t)main_avail - total - gaps;
  if (scrolling || remaining < 0) remaining = 0;
  int64_t used = total + gaps + ((grow_sum > 0 && !scrolling) ? remaining : 0);
  int64_t free_space = scrolling ? 0 : (main_avail > used ? main_avail - used : 0);
  int64_t offset = 0;
  int64_t extra_gap = 0;
  if (node->main_align == CRTUI_MAIN_CENTER) offset = free_space / 2;
  else if (node->main_align == CRTUI_MAIN_END) offset = free_space;
  else if (node->main_align == CRTUI_MAIN_SPACE_BETWEEN && count > 1) extra_gap = free_space / (int64_t)(count - 1);

  if (scrolling) {
    int64_t content = (int64_t)pad_main_start + total + gaps + pad_main_end;
    node->content_height = (int32_t)content;
    int32_t max_scroll = max0(node->content_height - node->height);
    if (node->scroll_y > max_scroll) node->scroll_y = max_scroll;
    if (node->scroll_y < 0) node->scroll_y = 0;
  }
  int64_t pos = (int64_t)pad_main_start + offset;
  int64_t given = 0;
  for (size_t i = 0; i < node->child_count; ++i) {
    crtui_node* c = resolve(context, node->children[i]);
    if (c == NULL || !c->visible) continue;
    int32_t m_main_s = horizontal ? c->margin[0] : c->margin[1];
    int32_t m_main_e = horizontal ? c->margin[2] : c->margin[3];
    int32_t m_cross_s = horizontal ? c->margin[1] : c->margin[0];
    int32_t m_cross_e = horizontal ? c->margin[3] : c->margin[2];
    crtui_align align = horizontal ? c->align_v : c->align_h;
    int32_t req_cross = horizontal ? c->req_height : c->req_width;
    int32_t cross_room = max0(cross_avail - m_cross_s - m_cross_e);
    int32_t cross_size = (align == CRTUI_ALIGN_STRETCH || req_cross == CRTUI_SIZE_FILL) ? cross_room : max0(req_cross);
    int32_t cross_pos = pad_cross_start + m_cross_s + align_offset(align, cross_room, cross_size);
    int32_t main_pos = (int32_t)(pos + m_main_s);
    int32_t req_main = horizontal ? c->req_width : c->req_height;
    int32_t main_size = max0(req_main);
    int32_t grow = c->grow;
    if (req_main == CRTUI_SIZE_FILL && grow == 0) grow = 1;
    if (scrolling) grow = 0;
    if (grow > 0 && remaining > 0) {
      int64_t extra = node->children[i] == last_grow ? remaining - given : remaining * grow / grow_sum;
      main_size += (int32_t)extra;
      given += extra;
    }
    if (scrolling) main_pos -= node->scroll_y;
    if (horizontal) {
      c->x = main_pos;
      c->y = cross_pos;
      c->width = main_size;
      c->height = cross_size;
    } else {
      c->x = cross_pos;
      c->y = main_pos;
      c->width = cross_size;
      c->height = main_size;
    }
    pos += m_main_s + main_size + m_main_e + node->gap + extra_gap;
  }
}

static void layout_node(crtui_context* context, crtui_widget id, int depth) {
  crtui_node* node = resolve(context, id);
  if (node == NULL || depth > CRTUI_MAX_DEPTH || !is_container_kind(node->kind)) {
    return;
  }
  int32_t w = node->kind == CRTUI_WIDGET_WINDOW ? node->window_width : node->width;
  int32_t h = node->kind == CRTUI_WIDGET_WINDOW ? node->window_height : node->height;
  int32_t cw = max0(w - node->padding[0] - node->padding[2]);
  int32_t ch = max0(h - node->padding[1] - node->padding[3]);
  switch (node->kind) {
    case CRTUI_WIDGET_ROW:
      layout_flex(context, node, 1, 0, cw, ch);
      break;
    case CRTUI_WIDGET_COLUMN:
      layout_flex(context, node, 0, 0, cw, ch);
      break;
    case CRTUI_WIDGET_SCROLL_VIEW:
    case CRTUI_WIDGET_LIST:
      layout_flex(context, node, 0, 1, cw, ch);
      break;
    case CRTUI_WIDGET_STACK:
      layout_stack(context, node, cw, ch);
      break;
    default:
      layout_free(context, node, cw, ch);
      break;
  }
  for (size_t i = 0; i < node->child_count; ++i) {
    layout_node(context, node->children[i], depth + 1);
    node = resolve(context, id); /* nothing reallocates during layout; belt and braces */
    if (node == NULL) return;
  }
}

static void ensure_layout(crtui_context* context) {
  if (context->layout_version == context->version) {
    return;
  }
  context->layout_version = context->version;
  for (size_t i = 0; i < context->node_count; ++i) {
    crtui_node* node = &context->nodes[i];
    if (node->in_use && node->kind == CRTUI_WIDGET_WINDOW) {
      layout_node(context, id_of(context, node), 0);
    }
  }
}

static crtui_widget scroll_ancestor(crtui_context* context, crtui_widget id) {
  for (int depth = 0; depth < CRTUI_MAX_DEPTH + 2 && id != CRTUI_INVALID_WIDGET; ++depth) {
    crtui_node* node = resolve(context, id);
    if (node == NULL) return CRTUI_INVALID_WIDGET;
    if (is_scrolling_kind(node->kind)) return id;
    id = node->parent;
  }
  return CRTUI_INVALID_WIDGET;
}

/* Moves a scroll view's offset by `delta` pixels, clamped to its content. */
static void scroll_by(crtui_context* context, crtui_widget scroller, int32_t delta) {
  crtui_node* node = resolve(context, scroller);
  if (node == NULL || !is_scrolling_kind(node->kind)) return;
  ensure_layout(context);
  node = resolve(context, scroller);
  int32_t max_scroll = max0(node->content_height - node->height);
  int32_t next = node->scroll_y + delta;
  if (next > max_scroll) next = max_scroll;
  if (next < 0) next = 0;
  if (next != node->scroll_y) {
    node->scroll_y = next;
    touch(context);
  }
}

static void scroll_into_view(crtui_context* context, crtui_widget id) {
  crtui_node* node = resolve(context, id);
  if (node == NULL) return;
  crtui_widget scroller = scroll_ancestor(context, node->parent);
  if (scroller == CRTUI_INVALID_WIDGET) return;
  ensure_layout(context);
  int32_t wx, wy, ww, wh, sx, sy, sw, sh;
  if (abs_rect(context, id, &wx, &wy, &ww, &wh) != 0 || abs_rect(context, scroller, &sx, &sy, &sw, &sh) != 0) return;
  if (wy < sy) {
    scroll_by(context, scroller, wy - sy);
  } else if (wy + wh > sy + sh) {
    scroll_by(context, scroller, (wy + wh) - (sy + sh));
  }
}

static int hit_recurse(
    crtui_context* context, crtui_widget id, int32_t px, int32_t py, int32_t origin_x, int32_t origin_y,
    int32_t clip_x0, int32_t clip_y0, int32_t clip_x1, int32_t clip_y1, int depth, crtui_widget* out) {
  crtui_node* node = resolve(context, id);
  if (node == NULL || !node->visible || depth > CRTUI_MAX_DEPTH) {
    return 0;
  }
  int32_t ax = origin_x + node->x;
  int32_t ay = origin_y + node->y;
  int32_t x0 = ax > clip_x0 ? ax : clip_x0;
  int32_t y0 = ay > clip_y0 ? ay : clip_y0;
  int32_t x1 = (ax + node->width) < clip_x1 ? (ax + node->width) : clip_x1;
  int32_t y1 = (ay + node->height) < clip_y1 ? (ay + node->height) : clip_y1;
  if (px < x0 || px >= x1 || py < y0 || py >= y1) {
    return 0; /* outside this widget's clipped area, so outside its subtree too */
  }
  for (size_t i = node->child_count; i > 0; --i) { /* later siblings are on top */
    if (hit_recurse(context, node->children[i - 1], px, py, ax, ay, x0, y0, x1, y1, depth + 1, out)) {
      return 1;
    }
  }
  *out = id;
  return 1;
}

static crtui_widget hit_test_internal(crtui_context* context, crtui_widget window, int32_t x, int32_t y) {
  ensure_layout(context);
  crtui_node* node = resolve(context, window);
  if (node == NULL || node->kind != CRTUI_WIDGET_WINDOW || !node->visible) {
    return CRTUI_INVALID_WIDGET;
  }
  crtui_widget found = CRTUI_INVALID_WIDGET;
  if (x < 0 || y < 0 || x >= node->window_width || y >= node->window_height) {
    return CRTUI_INVALID_WIDGET;
  }
  for (size_t i = node->child_count; i > 0; --i) {
    if (hit_recurse(context, node->children[i - 1], x, y, 0, 0, 0, 0, node->window_width, node->window_height, 1,
                    &found)) {
      return found;
    }
  }
  return window;
}

static void send_simple(crtui_context* context, crtui_event_type type, crtui_widget target, int32_t x, int32_t y,
                        crtui_key key, int32_t value, int bubble, int* out_handled) {
  crtui_event event;
  memset(&event, 0, sizeof(event));
  event.type = type;
  event.target = target;
  event.x = x;
  event.y = y;
  event.key = key;
  event.value = value;
  deliver(context, target, &event, bubble, out_handled);
}

static void focus_move(crtui_context* context, crtui_widget window, int backwards) {
  crtui_id_list order;
  memset(&order, 0, sizeof(order));
  collect_focus_order(context, window, 0, &order);
  if (order.count > 0) {
    size_t index = (size_t)-1;
    for (size_t i = 0; i < order.count; ++i) {
      if (order.ids[i] == context->focus) {
        index = i;
        break;
      }
    }
    size_t next;
    if (index == (size_t)-1) {
      next = backwards ? order.count - 1 : 0;
    } else if (backwards) {
      next = (index + order.count - 1) % order.count;
    } else {
      next = (index + 1) % order.count;
    }
    set_focus_internal(context, order.ids[next]);
  }
  free(order.ids);
}

static void set_pressed(crtui_context* context, crtui_widget id) {
  if (context->pressed != id) {
    context->pressed = id;
    touch(context);
  }
}

/* Absolute (window-space) rectangle of a widget: parent-relative bounds summed
 * up the chain; the window itself contributes no offset. */
static int abs_rect(crtui_context* context, crtui_widget id, int32_t* x, int32_t* y, int32_t* w, int32_t* h) {
  crtui_node* node = resolve(context, id);
  if (node == NULL || node->kind == CRTUI_WIDGET_WINDOW) {
    return -1;
  }
  int32_t ax = 0;
  int32_t ay = 0;
  crtui_widget cursor = id;
  for (int depth = 0; depth < CRTUI_MAX_DEPTH + 2 && cursor != CRTUI_INVALID_WIDGET; ++depth) {
    crtui_node* current = resolve(context, cursor);
    if (current == NULL) {
      return -1;
    }
    if (current->kind != CRTUI_WIDGET_WINDOW) {
      ax += current->x;
      ay += current->y;
    }
    cursor = current->parent;
  }
  *x = ax;
  *y = ay;
  *w = node->width;
  *h = node->height;
  return 0;
}

/* User-driven slider change: clamps, and if the value moved, emits
 * VALUE_CHANGED (bubbling) after the new value is stored. */
static void slider_user_set(crtui_context* context, crtui_widget id, int32_t value) {
  crtui_node* node = resolve(context, id);
  if (node == NULL || node->kind != CRTUI_WIDGET_SLIDER) {
    return;
  }
  if (value < node->min_value) value = node->min_value;
  if (value > node->max_value) value = node->max_value;
  if (value == node->value) {
    return;
  }
  node->value = value;
  touch(context);
  send_simple(context, CRTUI_EVENT_VALUE_CHANGED, id, 0, 0, CRTUI_KEY_NONE, value, 1, NULL);
}

static void slider_from_pointer(crtui_context* context, crtui_widget id, int32_t px) {
  crtui_node* node = resolve(context, id);
  int32_t ax, ay, aw, ah;
  if (node == NULL || abs_rect(context, id, &ax, &ay, &aw, &ah) != 0 || aw <= 0) {
    return;
  }
  int64_t span = (int64_t)node->max_value - (int64_t)node->min_value;
  int64_t offset = (int64_t)px - (int64_t)ax;
  if (offset < 0) offset = 0;
  if (offset > aw) offset = aw;
  int32_t value = (int32_t)((int64_t)node->min_value + (offset * span + aw / 2) / aw);
  slider_user_set(context, id, value);
}

/* Spatial focus navigation: among the eligible focusable widgets of the window,
 * pick the one whose center lies strictly in the requested direction and
 * minimizes (distance along the axis) + 2 * (offset across it). No wrap. */
static void focus_navigate(crtui_context* context, crtui_widget window, crtui_key key) {
  crtui_id_list order;
  memset(&order, 0, sizeof(order));
  collect_focus_order(context, window, 0, &order);
  int32_t fx, fy, fw, fh;
  if (order.count == 0) {
    free(order.ids);
    return;
  }
  if (context->focus == CRTUI_INVALID_WIDGET || abs_rect(context, context->focus, &fx, &fy, &fw, &fh) != 0) {
    set_focus_internal(context, order.ids[0]);
    free(order.ids);
    return;
  }
  int64_t cx = (int64_t)fx * 2 + fw;
  int64_t cy = (int64_t)fy * 2 + fh;
  crtui_widget best = CRTUI_INVALID_WIDGET;
  int64_t best_score = 0;
  for (size_t i = 0; i < order.count; ++i) {
    int32_t x, y, w, h;
    if (order.ids[i] == context->focus || abs_rect(context, order.ids[i], &x, &y, &w, &h) != 0) {
      continue;
    }
    int64_t dx = ((int64_t)x * 2 + w) - cx;
    int64_t dy = ((int64_t)y * 2 + h) - cy;
    int64_t primary;
    int64_t secondary;
    if (key == CRTUI_KEY_RIGHT && dx > 0) {
      primary = dx;
      secondary = dy < 0 ? -dy : dy;
    } else if (key == CRTUI_KEY_LEFT && dx < 0) {
      primary = -dx;
      secondary = dy < 0 ? -dy : dy;
    } else if (key == CRTUI_KEY_DOWN && dy > 0) {
      primary = dy;
      secondary = dx < 0 ? -dx : dx;
    } else if (key == CRTUI_KEY_UP && dy < 0) {
      primary = -dy;
      secondary = dx < 0 ? -dx : dx;
    } else {
      continue;
    }
    int64_t score = primary + 2 * secondary;
    if (best == CRTUI_INVALID_WIDGET || score < best_score) {
      best = order.ids[i];
      best_score = score;
    }
  }
  free(order.ids);
  if (best != CRTUI_INVALID_WIDGET) {
    set_focus_internal(context, best);
  }
}

static void toggle_user_flip(crtui_context* context, crtui_widget id) {
  crtui_node* node = resolve(context, id);
  if (node == NULL || (node->kind != CRTUI_WIDGET_SWITCH && node->kind != CRTUI_WIDGET_CHECKBOX)) return;
  node->value = node->value ? 0 : 1;
  touch(context);
  send_simple(context, CRTUI_EVENT_VALUE_CHANGED, id, 0, 0, CRTUI_KEY_NONE, node->value, 1, NULL);
}

static size_t utf8_prev_index(const char* s, size_t i) {
  if (i == 0) return 0;
  --i;
  while (i > 0 && ((unsigned char)s[i] & 0xC0u) == 0x80u) --i;
  return i;
}

static size_t utf8_next_index(const char* s, size_t length, size_t i) {
  if (i >= length) return length;
  ++i;
  while (i < length && ((unsigned char)s[i] & 0xC0u) == 0x80u) ++i;
  return i;
}

/* Replaces `remove` bytes at `pos` with `insert`; the caret lands after the
 * insertion. Emits TEXT_CHANGED (bubbling) with the new length. */
static void text_input_replace(
    crtui_context* context, crtui_widget id, size_t pos, size_t remove, const char* insert, size_t insert_length) {
  crtui_node* node = resolve(context, id);
  if (node == NULL || node->kind != CRTUI_WIDGET_TEXT_INPUT || node->text == NULL) return;
  size_t length = strlen(node->text);
  if (pos > length || remove > length - pos) return;
  size_t new_length = length - remove + insert_length;
  if (new_length > CRTUI_MAX_TEXT_INPUT) return;
  char* grown = (char*)malloc(new_length + 1);
  if (grown == NULL) return;
  memcpy(grown, node->text, pos);
  if (insert_length > 0) memcpy(grown + pos, insert, insert_length);
  memcpy(grown + pos + insert_length, node->text + pos + remove, length - pos - remove);
  grown[new_length] = '\0';
  free(node->text);
  node->text = grown;
  node->caret = pos + insert_length;
  touch(context);
  send_simple(context, CRTUI_EVENT_TEXT_CHANGED, id, 0, 0, CRTUI_KEY_NONE, (int32_t)new_length, 1, NULL);
}

/* Default key handling for a focused TextInput. Returns 1 if the key was
 * consumed; Tab, Up and Down are left to focus navigation. */
static int text_input_key(crtui_context* context, crtui_widget id, crtui_key key) {
  crtui_node* node = resolve(context, id);
  if (node == NULL || node->text == NULL) return 0;
  size_t length = strlen(node->text);
  switch (key) {
    case CRTUI_KEY_LEFT:
      node->caret = utf8_prev_index(node->text, node->caret);
      touch(context);
      return 1;
    case CRTUI_KEY_RIGHT:
      node->caret = utf8_next_index(node->text, length, node->caret);
      touch(context);
      return 1;
    case CRTUI_KEY_HOME:
      node->caret = 0;
      touch(context);
      return 1;
    case CRTUI_KEY_END:
      node->caret = length;
      touch(context);
      return 1;
    case CRTUI_KEY_BACKSPACE: {
      size_t start = utf8_prev_index(node->text, node->caret);
      if (start < node->caret) text_input_replace(context, id, start, node->caret - start, "", 0);
      return 1;
    }
    case CRTUI_KEY_DELETE: {
      size_t end = utf8_next_index(node->text, length, node->caret);
      if (end > node->caret) text_input_replace(context, id, node->caret, end - node->caret, "", 0);
      return 1;
    }
    case CRTUI_KEY_ENTER:
      send_simple(context, CRTUI_EVENT_ACTIVATE, id, 0, 0, CRTUI_KEY_NONE, 0, 1, NULL);
      return 1;
    default:
      return 0;
  }
}

static void process_input(crtui_context* context, const crtui_input* input) {
  ensure_layout(context);
  crtui_widget window = input->window;
  crtui_node* window_node = resolve(context, window);
  if (window_node == NULL || window_node->kind != CRTUI_WIDGET_WINDOW || !window_permitted(context, window)) {
    return; /* window gone, or a modal window blocks it: dropped */
  }
  switch (input->type) {
    case CRTUI_INPUT_TEXT: {
      crtui_widget focused = context->focus;
      crtui_node* node = resolve(context, focused);
      if (node == NULL || node->kind != CRTUI_WIDGET_TEXT_INPUT || window_of(context, focused) != window ||
          !chain_visible_enabled(context, focused, 1)) {
        return;
      }
      size_t length = 0;
      while (length < sizeof(input->text) && input->text[length] != '\0') ++length;
      if (length == 0 || length >= sizeof(input->text) || (unsigned char)input->text[0] < 0x20u ||
          (unsigned char)input->text[0] == 0x7fu) {
        return; /* empty, unterminated, or a control character: not text */
      }
      text_input_replace(context, focused, node->caret, 0, input->text, length);
      return;
    }
    case CRTUI_INPUT_POINTER_CANCEL:
      context->capture = CRTUI_INVALID_WIDGET;
      set_pressed(context, CRTUI_INVALID_WIDGET);
      return;
    case CRTUI_INPUT_POINTER_DOWN:
    case CRTUI_INPUT_POINTER_UP:
    case CRTUI_INPUT_POINTER_MOVE:
    case CRTUI_INPUT_WHEEL: {
      /* An active slider capture (see crtui/ui.h) takes moves and the release
       * wherever the pointer is; it ends if the slider stopped being eligible. */
      if (context->capture != CRTUI_INVALID_WIDGET) {
        crtui_node* captured = resolve(context, context->capture);
        if (captured == NULL || !chain_visible_enabled(context, context->capture, 1)) {
          context->capture = CRTUI_INVALID_WIDGET;
          set_pressed(context, CRTUI_INVALID_WIDGET);
        } else if (input->type == CRTUI_INPUT_POINTER_MOVE || input->type == CRTUI_INPUT_POINTER_UP) {
          crtui_widget slider = context->capture;
          slider_from_pointer(context, slider, input->x);
          if (input->type == CRTUI_INPUT_POINTER_UP) {
            context->capture = CRTUI_INVALID_WIDGET;
            set_pressed(context, CRTUI_INVALID_WIDGET);
          }
          send_simple(context, input->type == CRTUI_INPUT_POINTER_UP ? CRTUI_EVENT_POINTER_UP : CRTUI_EVENT_POINTER_MOVE,
                      slider, input->x, input->y, CRTUI_KEY_NONE, 0, 1, NULL);
          return;
        }
      }
      crtui_widget target = hit_test_internal(context, window, input->x, input->y);
      if (target == CRTUI_INVALID_WIDGET || !chain_visible_enabled(context, target, 1)) {
        if (input->type == CRTUI_INPUT_POINTER_UP) {
          set_pressed(context, CRTUI_INVALID_WIDGET);
        }
        return;
      }
      if (input->type == CRTUI_INPUT_POINTER_DOWN) {
        crtui_node* node = resolve(context, target);
        if (node != NULL && node->focusable) {
          set_focus_internal(context, target);
        }
        set_pressed(context, target);
        send_simple(context, CRTUI_EVENT_POINTER_DOWN, target, input->x, input->y, CRTUI_KEY_NONE, 0, 1, NULL);
        node = resolve(context, target);
        if (node != NULL && node->kind == CRTUI_WIDGET_SLIDER) {
          context->capture = target;
          slider_from_pointer(context, target, input->x);
        }
      } else if (input->type == CRTUI_INPUT_POINTER_UP) {
        crtui_widget pressed = context->pressed;
        set_pressed(context, CRTUI_INVALID_WIDGET);
        send_simple(context, CRTUI_EVENT_POINTER_UP, target, input->x, input->y, CRTUI_KEY_NONE, 0, 1, NULL);
        crtui_node* node = resolve(context, target);
        if (pressed == target && node != NULL && node->kind == CRTUI_WIDGET_BUTTON) {
          send_simple(context, CRTUI_EVENT_ACTIVATE, target, input->x, input->y, CRTUI_KEY_NONE, 0, 1, NULL);
        } else if (pressed == target && node != NULL &&
                   (node->kind == CRTUI_WIDGET_SWITCH || node->kind == CRTUI_WIDGET_CHECKBOX)) {
          toggle_user_flip(context, target);
        }
      } else if (input->type == CRTUI_INPUT_POINTER_MOVE) {
        send_simple(context, CRTUI_EVENT_POINTER_MOVE, target, input->x, input->y, CRTUI_KEY_NONE, 0, 1, NULL);
      } else {
        int handled = 0;
        send_simple(context, CRTUI_EVENT_WHEEL, target, input->x, input->y, CRTUI_KEY_NONE, input->wheel_delta, 1,
                    &handled);
        crtui_node* node = resolve(context, target);
        if (!handled && node != NULL && node->kind == CRTUI_WIDGET_SLIDER && input->wheel_delta != 0) {
          slider_user_set(context, target, node->value + (input->wheel_delta > 0 ? 1 : -1));
        } else if (!handled && input->wheel_delta != 0) {
          crtui_widget scroller = scroll_ancestor(context, target);
          if (scroller != CRTUI_INVALID_WIDGET) {
            scroll_by(context, scroller, input->wheel_delta > 0 ? -CRTUI_SCROLL_STEP : CRTUI_SCROLL_STEP);
          }
        }
      }
      return;
    }
    case CRTUI_INPUT_KEY_DOWN:
    case CRTUI_INPUT_KEY_UP: {
      crtui_widget target = window;
      if (context->focus != CRTUI_INVALID_WIDGET && window_of(context, context->focus) == window) {
        target = context->focus;
      }
      if (!chain_visible_enabled(context, target, 1)) {
        return;
      }
      int handled = 0;
      send_simple(context,
                  input->type == CRTUI_INPUT_KEY_DOWN ? CRTUI_EVENT_KEY_DOWN : CRTUI_EVENT_KEY_UP, target, 0, 0,
                  input->key, 0, 1, &handled);
      if (handled || input->type == CRTUI_INPUT_KEY_UP) {
        return;
      }
      /* Default actions, only for unhandled KEY_DOWN. */
      crtui_widget focused = context->focus;
      crtui_node* focused_node = resolve(context, focused);
      if (focused_node != NULL && window_of(context, focused) != window) {
        focused_node = NULL;
      }
      if (input->key == CRTUI_KEY_TAB) {
        focus_move(context, window, (input->modifiers & CRTUI_MOD_SHIFT) != 0);
      } else if (focused_node != NULL && focused_node->kind == CRTUI_WIDGET_TEXT_INPUT &&
                 text_input_key(context, focused, input->key)) {
        /* consumed by the text input (caret, editing, submit) */
      } else if ((input->key == CRTUI_KEY_ENTER || input->key == CRTUI_KEY_SPACE) && focused_node != NULL &&
                 focused_node->kind == CRTUI_WIDGET_BUTTON) {
        send_simple(context, CRTUI_EVENT_ACTIVATE, focused, 0, 0, CRTUI_KEY_NONE, 0, 1, NULL);
      } else if ((input->key == CRTUI_KEY_ENTER || input->key == CRTUI_KEY_SPACE) && focused_node != NULL &&
                 (focused_node->kind == CRTUI_WIDGET_SWITCH || focused_node->kind == CRTUI_WIDGET_CHECKBOX)) {
        toggle_user_flip(context, focused);
      } else if (input->key == CRTUI_KEY_LEFT || input->key == CRTUI_KEY_RIGHT || input->key == CRTUI_KEY_UP ||
                 input->key == CRTUI_KEY_DOWN) {
        if (focused_node != NULL && focused_node->kind == CRTUI_WIDGET_SLIDER &&
            (input->key == CRTUI_KEY_LEFT || input->key == CRTUI_KEY_RIGHT)) {
          slider_user_set(context, focused, focused_node->value + (input->key == CRTUI_KEY_LEFT ? -1 : 1));
        } else {
          focus_navigate(context, window, input->key);
        }
      }
      return;
    }
  }
}

static void drain_input(crtui_context* context) {
  context->processing = 1;
  size_t index = 0;
  while (index < context->input_count) {
    crtui_input input = context->input_queue[index++]; /* copy: the queue may grow during dispatch */
    process_input(context, &input);
  }
  context->input_count = 0;
  context->processing = 0;
}

/* ---- destroy ------------------------------------------------------------ */

static void kill_subtree(crtui_context* context, crtui_widget id, int depth) {
  crtui_node* node = resolve(context, id);
  if (node == NULL) {
    return;
  }
  /* Detach first, so nothing can reach the subtree through its parent. */
  size_t child_count = node->child_count;
  crtui_widget* children = node->children;
  node->children = NULL;
  node->child_count = 0;
  node->child_capacity = 0;
  node->in_use = 0; /* ids into this node are invalid from here on */
  release_backend(node);
  free(node->text);
  node->text = NULL;
  free(node->placeholder);
  node->placeholder = NULL;
  free(node->image_pixels);
  node->image_pixels = NULL;
  node->callback = NULL;
  if (context->focus == id) {
    context->focus = CRTUI_INVALID_WIDGET;
  }
  if (context->pressed == id) {
    context->pressed = CRTUI_INVALID_WIDGET;
  }
  if (context->capture == id) {
    context->capture = CRTUI_INVALID_WIDGET;
  }
  for (size_t i = 0; i < child_count && depth < CRTUI_MAX_DEPTH; ++i) {
    kill_subtree(context, children[i], depth + 1);
  }
  free(children);
}

/* ---- context ------------------------------------------------------------ */

crtui_result crtui_context_create(crtui_context** out_context) {
  if (out_context == NULL) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  *out_context = NULL;
  crtui_context* context = (crtui_context*)calloc(1, sizeof(*context));
  if (context == NULL) {
    return CRTUI_ERROR_IO;
  }
  context->ui_thread = pthread_self();
  pthread_mutex_init(&context->post_lock, NULL);
  *out_context = context;
  return CRTUI_OK;
}

crtui_result crtui_context_destroy(crtui_context* context) {
  CRTUI_CHECK_CONTEXT(context);
  for (size_t i = 0; i < context->node_count; ++i) {
    crtui_node* node = &context->nodes[i];
    if (node->in_use) {
      release_backend(node);
      free(node->text);
      free(node->placeholder);
      free(node->image_pixels);
      free(node->children);
      node->in_use = 0;
    }
  }
  free(context->nodes);
  free(context->posted);
  free(context->input_queue);
  pthread_mutex_destroy(&context->post_lock);
  free(context);
  return CRTUI_OK;
}

crtui_result crtui_context_post(crtui_context* context, crtui_post_fn fn, void* user) {
  if (context == NULL || fn == NULL) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  pthread_mutex_lock(&context->post_lock);
  if (context->posted_count == context->posted_capacity) {
    size_t capacity = context->posted_capacity == 0 ? 16 : context->posted_capacity * 2;
    crtui_posted* grown = (crtui_posted*)realloc(context->posted, capacity * sizeof(*grown));
    if (grown == NULL) {
      pthread_mutex_unlock(&context->post_lock);
      return CRTUI_ERROR_IO;
    }
    context->posted = grown;
    context->posted_capacity = capacity;
  }
  context->posted[context->posted_count].fn = fn;
  context->posted[context->posted_count].user = user;
  ++context->posted_count;
  pthread_mutex_unlock(&context->post_lock);
  return CRTUI_OK;
}

crtui_result crtui_context_pump(crtui_context* context, size_t* out_ran) {
  CRTUI_CHECK_CONTEXT(context);
  size_t ran = 0;
  for (;;) {
    crtui_posted next;
    pthread_mutex_lock(&context->post_lock);
    if (context->posted_count == 0) {
      pthread_mutex_unlock(&context->post_lock);
      break;
    }
    next = context->posted[0];
    memmove(context->posted, context->posted + 1, (context->posted_count - 1) * sizeof(*context->posted));
    --context->posted_count;
    pthread_mutex_unlock(&context->post_lock);
    next.fn(next.user); /* may post more; those run in this same pump, after the ones already queued */
    ++ran;
  }
  if (context->input_count > 0 && !context->processing) {
    drain_input(context);
  }
  if (out_ran != NULL) {
    *out_ran = ran;
  }
  return CRTUI_OK;
}

crtui_result crtui_context_send_input(crtui_context* context, const crtui_input* input) {
  CRTUI_CHECK_CONTEXT(context);
  if (input == NULL) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  crtui_node* window = resolve(context, input->window);
  if (window == NULL || window->kind != CRTUI_WIDGET_WINDOW) {
    return CRTUI_ERROR_INVALID_HANDLE;
  }
  if (context->input_count == context->input_capacity) {
    size_t capacity = context->input_capacity == 0 ? 16 : context->input_capacity * 2;
    crtui_input* grown = (crtui_input*)realloc(context->input_queue, capacity * sizeof(*grown));
    if (grown == NULL) {
      return CRTUI_ERROR_IO;
    }
    context->input_queue = grown;
    context->input_capacity = capacity;
  }
  context->input_queue[context->input_count++] = *input;
  if (!context->processing) {
    drain_input(context); /* delivers this input, and any sent from inside callbacks, in order */
  }
  return CRTUI_OK;
}

/* ---- widgets ------------------------------------------------------------ */

crtui_result crtui_window_create(crtui_context* context, crtui_window* out_window) {
  CRTUI_CHECK_CONTEXT(context);
  if (out_window == NULL) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  *out_window = CRTUI_INVALID_WIDGET;
  crtui_node* node = NULL;
  crtui_result result = alloc_node(context, CRTUI_WIDGET_WINDOW, CRTUI_INVALID_WIDGET, &node);
  if (result != CRTUI_OK) {
    return result;
  }
  node->window_width = 0;
  node->window_height = 0;
  *out_window = id_of(context, node);
  touch(context);
  return CRTUI_OK;
}

crtui_result crtui_container_create(crtui_context* context, crtui_widget parent, crtui_widget* out_widget) {
  CRTUI_CHECK_CONTEXT(context);
  return create_child(context, CRTUI_WIDGET_CONTAINER, parent, out_widget, NULL);
}

static crtui_result create_text_like(
    crtui_context* context, crtui_widget_kind kind, crtui_widget parent, const char* utf8, crtui_widget* out) {
  if (out != NULL) {
    *out = CRTUI_INVALID_WIDGET;
  }
  CRTUI_CHECK_CONTEXT(context);
  if (utf8 == NULL || (kind == CRTUI_WIDGET_TEXT_INPUT && strlen(utf8) > CRTUI_MAX_TEXT_INPUT)) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  crtui_node* node = NULL;
  crtui_result result = create_child(context, kind, parent, out, &node);
  if (result != CRTUI_OK) {
    return result;
  }
  node->text = strdup(utf8);
  if (node->text != NULL) {
    node->caret = strlen(node->text); /* TextInput: caret at the end of the initial text */
  }
  if (node->text == NULL) {
    crtui_widget_destroy(context, *out);
    *out = CRTUI_INVALID_WIDGET;
    return CRTUI_ERROR_IO;
  }
  return CRTUI_OK;
}

crtui_result crtui_text_create(crtui_context* context, crtui_widget parent, const char* utf8, crtui_widget* out_widget) {
  return create_text_like(context, CRTUI_WIDGET_TEXT, parent, utf8, out_widget);
}

crtui_result crtui_button_create(crtui_context* context, crtui_widget parent, const char* utf8, crtui_widget* out_widget) {
  return create_text_like(context, CRTUI_WIDGET_BUTTON, parent, utf8, out_widget);
}

crtui_result crtui_slider_create(
    crtui_context* context, crtui_widget parent, int32_t min_value, int32_t max_value, crtui_widget* out_widget) {
  CRTUI_CHECK_CONTEXT(context);
  if (min_value >= max_value) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  crtui_node* node = NULL;
  crtui_result result = create_child(context, CRTUI_WIDGET_SLIDER, parent, out_widget, &node);
  if (result != CRTUI_OK) {
    return result;
  }
  node->min_value = min_value;
  node->max_value = max_value;
  node->value = min_value;
  return CRTUI_OK;
}

crtui_result crtui_surface_view_create(crtui_context* context, crtui_widget parent, crtui_widget* out_widget) {
  CRTUI_CHECK_CONTEXT(context);
  return create_child(context, CRTUI_WIDGET_SURFACE_VIEW, parent, out_widget, NULL);
}

crtui_result crtui_media_view_create(crtui_context* context, crtui_widget parent, crtui_widget* out_widget) {
  CRTUI_CHECK_CONTEXT(context);
  return create_child(context, CRTUI_WIDGET_MEDIA_VIEW, parent, out_widget, NULL);
}

crtui_result crtui_surface_view_damage(
    crtui_context* context, crtui_widget surface_view, int32_t x, int32_t y, int32_t width, int32_t height) {
  CRTUI_CHECK_CONTEXT(context);
  ensure_layout(context);
  crtui_node* node = resolve(context, surface_view);
  if (node == NULL) return CRTUI_ERROR_INVALID_HANDLE;
  if (!is_surface_kind(node->kind) || x < 0 || y < 0 || width <= 0 || height <= 0 ||
      x >= node->width || y >= node->height) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  int64_t x1_64 = (int64_t)x + width;
  int64_t y1_64 = (int64_t)y + height;
  int32_t x1 = x1_64 > node->width ? node->width : (int32_t)x1_64;
  int32_t y1 = y1_64 > node->height ? node->height : (int32_t)y1_64;
  if (node->surface_damage_full) {
    /* Full damage already contains this rectangle. Still advance the serial so
     * a compositor can observe a newly submitted producer frame. */
  } else if (!node->surface_has_damage) {
    node->surface_damage[0] = x;
    node->surface_damage[1] = y;
    node->surface_damage[2] = x1 - x;
    node->surface_damage[3] = y1 - y;
    node->surface_has_damage = 1;
  } else {
    int32_t old_x1 = node->surface_damage[0] + node->surface_damage[2];
    int32_t old_y1 = node->surface_damage[1] + node->surface_damage[3];
    int32_t nx0 = x < node->surface_damage[0] ? x : node->surface_damage[0];
    int32_t ny0 = y < node->surface_damage[1] ? y : node->surface_damage[1];
    int32_t nx1 = x1 > old_x1 ? x1 : old_x1;
    int32_t ny1 = y1 > old_y1 ? y1 : old_y1;
    node->surface_damage[0] = nx0;
    node->surface_damage[1] = ny0;
    node->surface_damage[2] = nx1 - nx0;
    node->surface_damage[3] = ny1 - ny0;
  }
  if (++node->surface_damage_serial == 0) ++node->surface_damage_serial;
  return CRTUI_OK;
}

crtui_result crtui_surface_view_clear_damage(crtui_context* context, crtui_widget surface_view) {
  CRTUI_CHECK_CONTEXT(context);
  crtui_node* node = resolve(context, surface_view);
  if (node == NULL) return CRTUI_ERROR_INVALID_HANDLE;
  if (!is_surface_kind(node->kind)) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  node->surface_damage_full = 0;
  node->surface_has_damage = 0;
  memset(node->surface_damage, 0, sizeof(node->surface_damage));
  return CRTUI_OK;
}

static crtui_rect rect_intersection(crtui_rect a, crtui_rect b) {
  int64_t ax1 = (int64_t)a.x + a.width;
  int64_t ay1 = (int64_t)a.y + a.height;
  int64_t bx1 = (int64_t)b.x + b.width;
  int64_t by1 = (int64_t)b.y + b.height;
  int32_t x0 = a.x > b.x ? a.x : b.x;
  int32_t y0 = a.y > b.y ? a.y : b.y;
  int64_t x1 = ax1 < bx1 ? ax1 : bx1;
  int64_t y1 = ay1 < by1 ? ay1 : by1;
  crtui_rect result = {x0, y0, x1 > x0 ? (int32_t)(x1 - x0) : 0, y1 > y0 ? (int32_t)(y1 - y0) : 0};
  return result;
}

static void collect_surface_layers(
    crtui_context* context, crtui_widget id, int32_t origin_x, int32_t origin_y, crtui_rect parent_clip,
    uint32_t parent_opacity, int parent_visible, int depth, uint32_t* z_order,
    crtui_surface_layer* layers, size_t* count) {
  crtui_node* node = resolve(context, id);
  if (node == NULL || depth > CRTUI_MAX_DEPTH) return;
  crtui_rect bounds;
  if (node->kind == CRTUI_WIDGET_WINDOW) {
    bounds = (crtui_rect){0, 0, node->window_width, node->window_height};
  } else {
    bounds = (crtui_rect){origin_x + node->x, origin_y + node->y, node->width, node->height};
  }
  uint32_t z = (*z_order)++;
  crtui_rect clip = rect_intersection(parent_clip, bounds);
  int visible = parent_visible && node->visible && clip.width > 0 && clip.height > 0;
  uint32_t own_opacity = (node->style.mask & CRTUI_STYLE_OPACITY) != 0 ? node->style.opacity : 255u;
  uint32_t opacity = (parent_opacity * own_opacity + 127u) / 255u;
  if (visible && is_surface_kind(node->kind)) {
    crtui_surface_layer* layer = layers != NULL ? &layers[*count] : NULL;
    if (layer != NULL) {
      memset(layer, 0, sizeof(*layer));
      layer->view = id;
      layer->bounds = bounds;
      layer->clip = clip;
      layer->z_order = z;
      layer->opacity = (uint8_t)opacity;
      layer->damage_serial = node->surface_damage_serial;
      if (node->surface_has_damage) {
        crtui_rect local_damage = node->surface_damage_full
                                      ? (crtui_rect){0, 0, node->width, node->height}
                                      : (crtui_rect){node->surface_damage[0], node->surface_damage[1],
                                                     node->surface_damage[2], node->surface_damage[3]};
        local_damage.x += bounds.x;
        local_damage.y += bounds.y;
        layer->damage = rect_intersection(clip, local_damage);
        layer->has_damage = layer->damage.width > 0 && layer->damage.height > 0;
      }
    }
    ++*count;
  }
  for (size_t i = 0; i < node->child_count; ++i) {
    collect_surface_layers(context, node->children[i], bounds.x, bounds.y, clip, opacity, visible, depth + 1,
                           z_order, layers, count);
    node = resolve(context, id);
    if (node == NULL) return;
  }
}

crtui_result crtui_window_get_surface_layers(
    crtui_context* context, crtui_window window, crtui_surface_layer* layers, size_t capacity, size_t* out_count) {
  CRTUI_CHECK_CONTEXT(context);
  crtui_node* node = resolve(context, window);
  if (node == NULL) return CRTUI_ERROR_INVALID_HANDLE;
  if (node->kind != CRTUI_WIDGET_WINDOW || out_count == NULL || (layers == NULL && capacity != 0)) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  ensure_layout(context);
  node = resolve(context, window);
  crtui_rect window_clip = {0, 0, node->window_width, node->window_height};
  size_t required = 0;
  uint32_t z_order = 0;
  collect_surface_layers(context, window, 0, 0, window_clip, 255u, 1, 0, &z_order, NULL, &required);
  *out_count = required;
  if (layers == NULL && capacity == 0) return CRTUI_OK;
  if (capacity < required) return CRTUI_WOULD_BLOCK;
  if (required == 0) return CRTUI_OK;
  size_t written = 0;
  z_order = 0;
  collect_surface_layers(context, window, 0, 0, window_clip, 255u, 1, 0, &z_order, layers, &written);
  return CRTUI_OK;
}

crtui_result crtui_widget_destroy(crtui_context* context, crtui_widget widget) {
  CRTUI_CHECK_CONTEXT(context);
  crtui_node* node = resolve(context, widget);
  if (node == NULL) {
    return CRTUI_ERROR_INVALID_HANDLE;
  }
  crtui_focus_snapshot snapshot;
  focus_snapshot_begin(context, &snapshot);
  crtui_widget parent_id = node->parent;
  crtui_node* parent = resolve(context, parent_id);
  if (parent != NULL) {
    for (size_t i = 0; i < parent->child_count; ++i) {
      if (parent->children[i] == widget) {
        memmove(parent->children + i, parent->children + i + 1, (parent->child_count - i - 1) * sizeof(*parent->children));
        --parent->child_count;
        break;
      }
    }
  }
  kill_subtree(context, widget, 0);
  touch(context);
  focus_snapshot_end(context, &snapshot);
  return CRTUI_OK;
}

#define CRTUI_RESOLVE(context, widget, node)                    \
  crtui_node* node = resolve((context), (widget));              \
  if ((node) == NULL) return CRTUI_ERROR_INVALID_HANDLE

crtui_result crtui_widget_get_kind(crtui_context* context, crtui_widget widget, crtui_widget_kind* out_kind) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, widget, node);
  if (out_kind == NULL) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  *out_kind = node->kind;
  return CRTUI_OK;
}

crtui_result crtui_widget_get_parent(crtui_context* context, crtui_widget widget, crtui_widget* out_parent) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, widget, node);
  if (out_parent == NULL) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  *out_parent = node->parent;
  return CRTUI_OK;
}

crtui_result crtui_widget_set_bounds(
    crtui_context* context, crtui_widget widget, int32_t x, int32_t y, int32_t width, int32_t height) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, widget, node);
  if ((width < 0 && width != CRTUI_SIZE_FILL) || (height < 0 && height != CRTUI_SIZE_FILL) ||
      node->kind == CRTUI_WIDGET_WINDOW) {
    return CRTUI_ERROR_INVALID_ARGUMENT; /* windows are sized with crtui_window_set_size() */
  }
  node->x = x;
  node->y = y;
  node->req_width = width;
  node->req_height = height;
  node->width = width < 0 ? 0 : width;
  node->height = height < 0 ? 0 : height;
  touch(context);
  return CRTUI_OK;
}

crtui_result crtui_widget_get_bounds(
    crtui_context* context, crtui_widget widget, int32_t* out_x, int32_t* out_y, int32_t* out_width,
    int32_t* out_height) {
  CRTUI_CHECK_CONTEXT(context);
  ensure_layout(context);
  CRTUI_RESOLVE(context, widget, node);
  if (out_x == NULL || out_y == NULL || out_width == NULL || out_height == NULL) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  *out_x = node->x;
  *out_y = node->y;
  *out_width = node->kind == CRTUI_WIDGET_WINDOW ? node->window_width : node->width;
  *out_height = node->kind == CRTUI_WIDGET_WINDOW ? node->window_height : node->height;
  return CRTUI_OK;
}

crtui_result crtui_widget_set_visible(crtui_context* context, crtui_widget widget, int visible) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, widget, node);
  crtui_focus_snapshot snapshot;
  focus_snapshot_begin(context, &snapshot);
  node->visible = visible != 0;
  touch(context);
  focus_snapshot_end(context, &snapshot);
  return CRTUI_OK;
}

crtui_result crtui_widget_set_enabled(crtui_context* context, crtui_widget widget, int enabled) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, widget, node);
  crtui_focus_snapshot snapshot;
  focus_snapshot_begin(context, &snapshot);
  node->enabled = enabled != 0;
  touch(context);
  focus_snapshot_end(context, &snapshot);
  return CRTUI_OK;
}

crtui_result crtui_widget_set_focusable(crtui_context* context, crtui_widget widget, int focusable) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, widget, node);
  if (node->kind == CRTUI_WIDGET_WINDOW) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  crtui_focus_snapshot snapshot;
  focus_snapshot_begin(context, &snapshot);
  node->focusable = focusable != 0;
  focus_snapshot_end(context, &snapshot);
  return CRTUI_OK;
}

crtui_result crtui_widget_set_text(crtui_context* context, crtui_widget widget, const char* utf8) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, widget, node);
  if (utf8 == NULL || (node->kind != CRTUI_WIDGET_TEXT && node->kind != CRTUI_WIDGET_BUTTON &&
                       node->kind != CRTUI_WIDGET_CHECKBOX && node->kind != CRTUI_WIDGET_TEXT_INPUT)) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  if (node->kind == CRTUI_WIDGET_TEXT_INPUT && strlen(utf8) > CRTUI_MAX_TEXT_INPUT) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  char* copy = strdup(utf8);
  if (copy == NULL) {
    return CRTUI_ERROR_IO;
  }
  free(node->text);
  node->text = copy;
  node->caret = strlen(copy);
  touch(context);
  return CRTUI_OK;
}

crtui_result crtui_widget_get_text(
    crtui_context* context, crtui_widget widget, char* buffer, size_t capacity, size_t* out_length) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, widget, node);
  if (node->text == NULL || (buffer == NULL && capacity > 0)) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  size_t length = strlen(node->text);
  if (capacity > 0) {
    size_t copy = length < capacity - 1 ? length : capacity - 1;
    memcpy(buffer, node->text, copy);
    buffer[copy] = '\0';
  }
  if (out_length != NULL) {
    *out_length = length;
  }
  return CRTUI_OK;
}

crtui_result crtui_slider_set_value(crtui_context* context, crtui_widget slider, int32_t value) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, slider, node);
  if (node->kind != CRTUI_WIDGET_SLIDER) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  if (value < node->min_value) value = node->min_value;
  if (value > node->max_value) value = node->max_value;
  node->value = value;
  touch(context);
  return CRTUI_OK;
}

crtui_result crtui_slider_get_value(crtui_context* context, crtui_widget slider, int32_t* out_value) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, slider, node);
  if (node->kind != CRTUI_WIDGET_SLIDER || out_value == NULL) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  *out_value = node->value;
  return CRTUI_OK;
}

crtui_result crtui_widget_set_callback(crtui_context* context, crtui_widget widget, crtui_event_fn fn, void* user) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, widget, node);
  node->callback = fn;
  node->callback_user = user;
  return CRTUI_OK;
}

crtui_result crtui_progress_create(crtui_context* context, crtui_widget parent, crtui_widget* out_widget) {
  CRTUI_CHECK_CONTEXT(context);
  crtui_node* node = NULL;
  crtui_result result = create_child(context, CRTUI_WIDGET_PROGRESS, parent, out_widget, &node);
  if (result != CRTUI_OK) {
    return result;
  }
  node->min_value = 0;
  node->max_value = 100;
  node->value = 0;
  return CRTUI_OK;
}

crtui_result crtui_progress_set_value(crtui_context* context, crtui_widget progress, int32_t value) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, progress, node);
  if (node->kind != CRTUI_WIDGET_PROGRESS) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  if (value < node->min_value) value = node->min_value;
  if (value > node->max_value) value = node->max_value;
  node->value = value;
  touch(context);
  return CRTUI_OK;
}

crtui_result crtui_progress_get_value(crtui_context* context, crtui_widget progress, int32_t* out_value) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, progress, node);
  if (node->kind != CRTUI_WIDGET_PROGRESS || out_value == NULL) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  *out_value = node->value;
  return CRTUI_OK;
}

/* ---- rendering ---------------------------------------------------------- */

#ifdef CRTUI_HAVE_LVGL
typedef struct crtui_item_list {
  crtui_render_item* items;
  size_t count;
  size_t capacity;
} crtui_item_list;

static int build_items(
    crtui_context* context, crtui_widget id, int32_t parent_index, int parent_enabled, int depth,
    crtui_item_list* list) {
  crtui_node* node = resolve(context, id);
  if (node == NULL || depth > CRTUI_MAX_DEPTH) {
    return 0;
  }
  if (list->count == list->capacity) {
    size_t capacity = list->capacity == 0 ? 32 : list->capacity * 2;
    crtui_render_item* grown = (crtui_render_item*)realloc(list->items, capacity * sizeof(*grown));
    if (grown == NULL) {
      return -1;
    }
    list->items = grown;
    list->capacity = capacity;
  }
  int32_t index = (int32_t)list->count++;
  crtui_render_item* item = &list->items[index];
  memset(item, 0, sizeof(*item));
  item->kind = node->kind;
  item->parent = parent_index;
  item->visible = node->visible;
  item->enabled = parent_enabled && node->enabled;
  item->text = node->text;
  item->value = node->value;
  item->min_value = node->min_value;
  item->max_value = node->max_value;
  item->focused = context->focus == id;
  item->pressed = context->pressed == id;
  item->style = node->style;
  item->placeholder = node->placeholder;
  item->caret = (int32_t)node->caret;
  item->image_pixels = node->image_pixels;
  item->image_width = node->image_width;
  item->image_height = node->image_height;
  if (node->kind == CRTUI_WIDGET_WINDOW) {
    item->x = 0;
    item->y = 0;
    item->width = node->window_width;
    item->height = node->window_height;
  } else {
    item->x = node->x;
    item->y = node->y;
    item->width = node->width;
    item->height = node->height;
  }
  int enabled = item->enabled;
  for (size_t i = 0; i < node->child_count; ++i) {
    /* `item` may move if the list grows, so it is not used past this point. */
    if (build_items(context, node->children[i], index, enabled, depth + 1, list) != 0) {
      return -1;
    }
    node = resolve(context, id);
  }
  return 0;
}
#endif

crtui_result crtui_window_render(
    crtui_context* context, crtui_window window, void* pixels, size_t stride_bytes, int32_t width, int32_t height) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, window, node);
  if (node->kind != CRTUI_WIDGET_WINDOW || pixels == NULL || width <= 0 || height <= 0 ||
      width != node->window_width || height != node->window_height || stride_bytes < (size_t)width * 4u) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
#ifndef CRTUI_HAVE_LVGL
  return CRTUI_ERROR_UNSUPPORTED;
#else
  ensure_layout(context);
  crtui_item_list list;
  memset(&list, 0, sizeof(list));
  if (build_items(context, window, -1, 1, 0, &list) != 0) {
    free(list.items);
    return CRTUI_ERROR_IO;
  }
  node = resolve(context, window);
  crtui_result result = crtui_lvgl_render(&node->backend, list.items, list.count, context->version, pixels, stride_bytes);
  free(list.items);
  return result;
#endif
}

crtui_result crtui_window_render_plane(
    crtui_context* context, crtui_window window, uint32_t z_begin, uint32_t z_end, int transparent,
    void* pixels, size_t stride_bytes, int32_t width, int32_t height) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, window, node);
  if (node->kind != CRTUI_WIDGET_WINDOW || z_begin >= z_end || (transparent != 0 && transparent != 1) ||
      pixels == NULL || width <= 0 || height <= 0 || width != node->window_width ||
      height != node->window_height || stride_bytes < (size_t)width * 4u) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
#ifndef CRTUI_HAVE_LVGL
  return CRTUI_ERROR_UNSUPPORTED;
#else
  ensure_layout(context);
  crtui_item_list list;
  memset(&list, 0, sizeof(list));
  if (build_items(context, window, -1, 1, 0, &list) != 0) {
    free(list.items);
    return CRTUI_ERROR_IO;
  }
  node = resolve(context, window);
  crtui_result result = crtui_lvgl_render_plane(
      &node->backend, list.items, list.count, context->version, z_begin, z_end, transparent,
      pixels, stride_bytes);
  free(list.items);
  return result;
#endif
}

/* ---- layout, style and the v1 widget set (Tranche 4) ---------------------- */

crtui_result crtui_row_create(crtui_context* context, crtui_widget parent, crtui_widget* out_widget) {
  CRTUI_CHECK_CONTEXT(context);
  return create_child(context, CRTUI_WIDGET_ROW, parent, out_widget, NULL);
}

crtui_result crtui_column_create(crtui_context* context, crtui_widget parent, crtui_widget* out_widget) {
  CRTUI_CHECK_CONTEXT(context);
  return create_child(context, CRTUI_WIDGET_COLUMN, parent, out_widget, NULL);
}

crtui_result crtui_stack_create(crtui_context* context, crtui_widget parent, crtui_widget* out_widget) {
  CRTUI_CHECK_CONTEXT(context);
  return create_child(context, CRTUI_WIDGET_STACK, parent, out_widget, NULL);
}

crtui_result crtui_widget_set_size(crtui_context* context, crtui_widget widget, int32_t width, int32_t height) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, widget, node);
  if ((width < 0 && width != CRTUI_SIZE_FILL) || (height < 0 && height != CRTUI_SIZE_FILL) ||
      node->kind == CRTUI_WIDGET_WINDOW) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  node->req_width = width;
  node->req_height = height;
  node->width = width < 0 ? 0 : width;
  node->height = height < 0 ? 0 : height;
  touch(context);
  return CRTUI_OK;
}

crtui_result crtui_widget_set_margin(
    crtui_context* context, crtui_widget widget, int32_t left, int32_t top, int32_t right, int32_t bottom) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, widget, node);
  if (left < 0 || top < 0 || right < 0 || bottom < 0 || node->kind == CRTUI_WIDGET_WINDOW) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  node->margin[0] = left;
  node->margin[1] = top;
  node->margin[2] = right;
  node->margin[3] = bottom;
  touch(context);
  return CRTUI_OK;
}

crtui_result crtui_container_set_padding(
    crtui_context* context, crtui_widget container, int32_t left, int32_t top, int32_t right, int32_t bottom) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, container, node);
  if (left < 0 || top < 0 || right < 0 || bottom < 0 || !is_container_kind(node->kind)) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  node->padding[0] = left;
  node->padding[1] = top;
  node->padding[2] = right;
  node->padding[3] = bottom;
  touch(context);
  return CRTUI_OK;
}

crtui_result crtui_container_set_gap(crtui_context* context, crtui_widget container, int32_t gap) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, container, node);
  if (gap < 0 || !is_container_kind(node->kind)) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  node->gap = gap;
  touch(context);
  return CRTUI_OK;
}

crtui_result crtui_container_set_main_align(crtui_context* context, crtui_widget container, crtui_main_align main_align) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, container, node);
  if (main_align < CRTUI_MAIN_START || main_align > CRTUI_MAIN_SPACE_BETWEEN || !is_container_kind(node->kind)) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  node->main_align = main_align;
  touch(context);
  return CRTUI_OK;
}

crtui_result crtui_widget_set_alignment(
    crtui_context* context, crtui_widget widget, crtui_align horizontal, crtui_align vertical) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, widget, node);
  if (horizontal < CRTUI_ALIGN_START || horizontal > CRTUI_ALIGN_STRETCH || vertical < CRTUI_ALIGN_START ||
      vertical > CRTUI_ALIGN_STRETCH || node->kind == CRTUI_WIDGET_WINDOW) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  node->align_h = horizontal;
  node->align_v = vertical;
  touch(context);
  return CRTUI_OK;
}

crtui_result crtui_widget_set_grow(crtui_context* context, crtui_widget widget, int32_t grow) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, widget, node);
  if (grow < 0 || node->kind == CRTUI_WIDGET_WINDOW) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  node->grow = grow;
  touch(context);
  return CRTUI_OK;
}

crtui_result crtui_widget_set_style(crtui_context* context, crtui_widget widget, const crtui_style* style) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, widget, node);
  if (style == NULL) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  uint32_t known = CRTUI_STYLE_BACKGROUND | CRTUI_STYLE_FOREGROUND | CRTUI_STYLE_BORDER_COLOR |
                   CRTUI_STYLE_BORDER_WIDTH | CRTUI_STYLE_RADIUS | CRTUI_STYLE_OPACITY | CRTUI_STYLE_FONT |
                   CRTUI_STYLE_TEXT_ALIGN;
  if ((style->mask & ~known) != 0 || ((style->mask & CRTUI_STYLE_BORDER_WIDTH) != 0 && style->border_width < 0) ||
      ((style->mask & CRTUI_STYLE_RADIUS) != 0 && style->radius < 0) ||
      ((style->mask & CRTUI_STYLE_FONT) != 0 && (style->font < CRTUI_FONT_DEFAULT || style->font > CRTUI_FONT_LARGE)) ||
      ((style->mask & CRTUI_STYLE_TEXT_ALIGN) != 0 &&
       (style->text_align < CRTUI_TEXT_ALIGN_START || style->text_align > CRTUI_TEXT_ALIGN_END))) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  if ((style->mask & CRTUI_STYLE_BACKGROUND) != 0) node->style.background_rgb = style->background_rgb & 0xFFFFFFu;
  if ((style->mask & CRTUI_STYLE_FOREGROUND) != 0) node->style.foreground_rgb = style->foreground_rgb & 0xFFFFFFu;
  if ((style->mask & CRTUI_STYLE_BORDER_COLOR) != 0) node->style.border_rgb = style->border_rgb & 0xFFFFFFu;
  if ((style->mask & CRTUI_STYLE_BORDER_WIDTH) != 0) node->style.border_width = style->border_width;
  if ((style->mask & CRTUI_STYLE_RADIUS) != 0) node->style.radius = style->radius;
  if ((style->mask & CRTUI_STYLE_OPACITY) != 0) node->style.opacity = style->opacity;
  if ((style->mask & CRTUI_STYLE_FONT) != 0) node->style.font = style->font;
  if ((style->mask & CRTUI_STYLE_TEXT_ALIGN) != 0) node->style.text_align = style->text_align;
  node->style.mask |= style->mask;
  touch(context);
  return CRTUI_OK;
}

crtui_result crtui_widget_get_style(crtui_context* context, crtui_widget widget, crtui_style* out_style) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, widget, node);
  if (out_style == NULL) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  *out_style = node->style;
  return CRTUI_OK;
}

crtui_result crtui_widget_clear_style(crtui_context* context, crtui_widget widget) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, widget, node);
  memset(&node->style, 0, sizeof(node->style));
  touch(context);
  return CRTUI_OK;
}

crtui_result crtui_image_create(crtui_context* context, crtui_widget parent, crtui_widget* out_widget) {
  CRTUI_CHECK_CONTEXT(context);
  return create_child(context, CRTUI_WIDGET_IMAGE, parent, out_widget, NULL);
}

crtui_result crtui_image_set_pixels(
    crtui_context* context, crtui_widget image, const void* bgra, int32_t width, int32_t height, size_t stride_bytes) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, image, node);
  if (node->kind != CRTUI_WIDGET_IMAGE || bgra == NULL || width <= 0 || height <= 0 || width > 4096 || height > 4096 ||
      stride_bytes < (size_t)width * 4u) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  size_t row = (size_t)width * 4u;
  uint8_t* copy = (uint8_t*)malloc(row * (size_t)height);
  if (copy == NULL) {
    return CRTUI_ERROR_IO;
  }
  for (int32_t y = 0; y < height; ++y) {
    memcpy(copy + (size_t)y * row, (const uint8_t*)bgra + (size_t)y * stride_bytes, row);
  }
  free(node->image_pixels);
  node->image_pixels = copy;
  node->image_width = width;
  node->image_height = height;
  if (node->req_width == 0 && node->req_height == 0) {
    node->req_width = width;
    node->req_height = height;
    node->width = width;
    node->height = height;
  }
  touch(context);
  return CRTUI_OK;
}

crtui_result crtui_switch_create(crtui_context* context, crtui_widget parent, crtui_widget* out_widget) {
  CRTUI_CHECK_CONTEXT(context);
  return create_child(context, CRTUI_WIDGET_SWITCH, parent, out_widget, NULL);
}

crtui_result crtui_checkbox_create(
    crtui_context* context, crtui_widget parent, const char* utf8, crtui_widget* out_widget) {
  return create_text_like(context, CRTUI_WIDGET_CHECKBOX, parent, utf8, out_widget);
}

crtui_result crtui_toggle_set_checked(crtui_context* context, crtui_widget toggle, int checked) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, toggle, node);
  if (node->kind != CRTUI_WIDGET_SWITCH && node->kind != CRTUI_WIDGET_CHECKBOX) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  node->value = checked ? 1 : 0;
  touch(context);
  return CRTUI_OK;
}

crtui_result crtui_toggle_get_checked(crtui_context* context, crtui_widget toggle, int* out_checked) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, toggle, node);
  if ((node->kind != CRTUI_WIDGET_SWITCH && node->kind != CRTUI_WIDGET_CHECKBOX) || out_checked == NULL) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  *out_checked = node->value != 0;
  return CRTUI_OK;
}

crtui_result crtui_scroll_view_create(crtui_context* context, crtui_widget parent, crtui_widget* out_widget) {
  CRTUI_CHECK_CONTEXT(context);
  return create_child(context, CRTUI_WIDGET_SCROLL_VIEW, parent, out_widget, NULL);
}

crtui_result crtui_list_create(crtui_context* context, crtui_widget parent, crtui_widget* out_widget) {
  CRTUI_CHECK_CONTEXT(context);
  return create_child(context, CRTUI_WIDGET_LIST, parent, out_widget, NULL);
}

crtui_result crtui_list_add_item(crtui_context* context, crtui_widget list, const char* utf8, crtui_widget* out_item) {
  if (out_item != NULL) {
    *out_item = CRTUI_INVALID_WIDGET;
  }
  CRTUI_CHECK_CONTEXT(context);
  crtui_node* parent = resolve(context, list);
  if (parent == NULL) {
    return CRTUI_ERROR_INVALID_HANDLE;
  }
  if (parent->kind != CRTUI_WIDGET_LIST) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  crtui_result result = create_text_like(context, CRTUI_WIDGET_BUTTON, list, utf8, out_item);
  if (result != CRTUI_OK) {
    return result;
  }
  crtui_node* item = resolve(context, *out_item);
  item->req_height = 32;
  item->height = 32;
  item->align_h = CRTUI_ALIGN_STRETCH;
  touch(context);
  return CRTUI_OK;
}

crtui_result crtui_scroll_view_set_offset(crtui_context* context, crtui_widget scroll_view, int32_t offset) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, scroll_view, node);
  if (!is_scrolling_kind(node->kind)) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  ensure_layout(context);
  node = resolve(context, scroll_view);
  int32_t max_scroll = max0(node->content_height - node->height);
  if (offset > max_scroll) offset = max_scroll;
  if (offset < 0) offset = 0;
  node->scroll_y = offset;
  touch(context);
  return CRTUI_OK;
}

crtui_result crtui_scroll_view_get_offset(
    crtui_context* context, crtui_widget scroll_view, int32_t* out_offset, int32_t* out_content_height) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, scroll_view, node);
  if (!is_scrolling_kind(node->kind) || out_offset == NULL) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  ensure_layout(context);
  node = resolve(context, scroll_view);
  *out_offset = node->scroll_y;
  if (out_content_height != NULL) *out_content_height = node->content_height;
  return CRTUI_OK;
}

crtui_result crtui_text_input_create(
    crtui_context* context, crtui_widget parent, const char* initial_utf8, crtui_widget* out_widget) {
  return create_text_like(context, CRTUI_WIDGET_TEXT_INPUT, parent, initial_utf8, out_widget);
}

crtui_result crtui_text_input_set_placeholder(crtui_context* context, crtui_widget text_input, const char* utf8) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, text_input, node);
  if (node->kind != CRTUI_WIDGET_TEXT_INPUT) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  char* copy = utf8 != NULL ? strdup(utf8) : NULL;
  if (utf8 != NULL && copy == NULL) {
    return CRTUI_ERROR_IO;
  }
  free(node->placeholder);
  node->placeholder = copy;
  touch(context);
  return CRTUI_OK;
}

crtui_result crtui_text_input_set_caret(crtui_context* context, crtui_widget text_input, size_t byte_offset) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, text_input, node);
  if (node->kind != CRTUI_WIDGET_TEXT_INPUT || node->text == NULL) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  size_t length = strlen(node->text);
  if (byte_offset > length) byte_offset = length;
  while (byte_offset > 0 && byte_offset < length && ((unsigned char)node->text[byte_offset] & 0xC0u) == 0x80u) {
    --byte_offset; /* never inside a UTF-8 character */
  }
  node->caret = byte_offset;
  touch(context);
  return CRTUI_OK;
}

crtui_result crtui_text_input_get_caret(crtui_context* context, crtui_widget text_input, size_t* out_byte_offset) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, text_input, node);
  if (node->kind != CRTUI_WIDGET_TEXT_INPUT || out_byte_offset == NULL) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  *out_byte_offset = node->caret;
  return CRTUI_OK;
}

/* ---- windows ------------------------------------------------------------ */

crtui_result crtui_window_set_size(
    crtui_context* context, crtui_window window, int32_t width, int32_t height, float dpi_scale) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, window, node);
  if (node->kind != CRTUI_WIDGET_WINDOW || width < 0 || height < 0 || !(dpi_scale > 0.0f)) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  node->window_width = width;
  node->window_height = height;
  node->dpi_scale = dpi_scale;
  touch(context);
  send_simple(context, CRTUI_EVENT_RESIZED, window, width, height, CRTUI_KEY_NONE, (int32_t)(dpi_scale * 100.0f + 0.5f),
              0, NULL);
  return CRTUI_OK;
}

crtui_result crtui_window_get_size(
    crtui_context* context, crtui_window window, int32_t* out_width, int32_t* out_height, float* out_dpi_scale) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, window, node);
  if (node->kind != CRTUI_WIDGET_WINDOW || out_width == NULL || out_height == NULL || out_dpi_scale == NULL) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  *out_width = node->window_width;
  *out_height = node->window_height;
  *out_dpi_scale = node->dpi_scale;
  return CRTUI_OK;
}

crtui_result crtui_window_set_modal(crtui_context* context, crtui_window window, int modal) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, window, node);
  if (node->kind != CRTUI_WIDGET_WINDOW) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  crtui_focus_snapshot snapshot;
  focus_snapshot_begin(context, &snapshot);
  node->modal = modal != 0;
  node->modal_order = node->modal ? ++context->modal_counter : 0;
  if (node->modal && context->focus != CRTUI_INVALID_WIDGET && window_of(context, context->focus) != window) {
    /* Focus inside a now-blocked window moves into the modal window. */
    crtui_widget first = CRTUI_INVALID_WIDGET;
    crtui_id_list order;
    memset(&order, 0, sizeof(order));
    collect_focus_order(context, window, 0, &order);
    if (order.count > 0) {
      first = order.ids[0];
    }
    free(order.ids);
    set_focus_internal(context, first);
  }
  focus_snapshot_end(context, &snapshot);
  return CRTUI_OK;
}

/* ---- focus and hit-testing ---------------------------------------------- */

crtui_result crtui_widget_focus(crtui_context* context, crtui_widget widget) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, widget, node);
  if (!node->focusable || !chain_visible_enabled(context, widget, 1) ||
      !window_permitted(context, window_of(context, widget))) {
    return CRTUI_ERROR_STATE;
  }
  set_focus_internal(context, widget);
  return CRTUI_OK;
}

crtui_result crtui_context_get_focus(crtui_context* context, crtui_widget* out_widget) {
  CRTUI_CHECK_CONTEXT(context);
  if (out_widget == NULL) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  *out_widget = context->focus;
  return context->focus == CRTUI_INVALID_WIDGET ? CRTUI_ERROR_STATE : CRTUI_OK;
}

crtui_result crtui_context_hit_test(
    crtui_context* context, crtui_window window, int32_t x, int32_t y, crtui_widget* out_widget) {
  CRTUI_CHECK_CONTEXT(context);
  CRTUI_RESOLVE(context, window, node);
  if (node->kind != CRTUI_WIDGET_WINDOW || out_widget == NULL) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  *out_widget = hit_test_internal(context, window, x, y);
  return CRTUI_OK;
}
