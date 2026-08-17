#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct crtgfx_window crtgfx_window;

typedef enum crtgfx_result {
  CRTGFX_OK = 0,
  CRTGFX_ERROR_INVALID_ARGUMENT = -1,
  CRTGFX_ERROR_UNSUPPORTED = -2,
  CRTGFX_ERROR_HOST = -3,
} crtgfx_result;

enum {
  CRTGFX_WINDOW_VISIBLE = 1u << 0,
};

typedef struct crtgfx_window_desc {
  const char* title;
  uint32_t width;
  uint32_t height;
  uint32_t flags;
} crtgfx_window_desc;

typedef enum crtgfx_pixel_format {
  CRTGFX_PIXEL_FORMAT_BGRA8888_PREMULTIPLIED = 1,
} crtgfx_pixel_format;

typedef struct crtgfx_framebuffer {
  void* pixels;
  uint32_t width;
  uint32_t height;
  uint32_t stride;
  crtgfx_pixel_format format;
} crtgfx_framebuffer;

int crtgfx_window_create(const crtgfx_window_desc* desc, crtgfx_window** out_window);
void crtgfx_window_destroy(crtgfx_window* window);
int crtgfx_window_show(crtgfx_window* window);
int crtgfx_window_pump_events(uint32_t timeout_ms);
int crtgfx_window_get_size(crtgfx_window* window, uint32_t* out_width, uint32_t* out_height);
int crtgfx_window_should_close(crtgfx_window* window);
int crtgfx_window_begin_frame(crtgfx_window* window, crtgfx_framebuffer* out_framebuffer);
int crtgfx_window_end_frame(crtgfx_window* window);

#ifdef __cplusplus
}
#endif
