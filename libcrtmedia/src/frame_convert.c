#include "crtmedia/frame.h"

#include <stddef.h>

/* Real ITU-R BT.601/BT.709/BT.2020 luma coefficients (Kr, Kb -- the
 * fraction of red/blue in luma; Kg = 1 - Kr - Kb is derived, not stored).
 * CRTMEDIA_COLOR_SPACE_UNSPECIFIED defaults to BT.709: this codebase's
 * most common real-world case (typical un-signaled H.264/HEVC content),
 * matching crtmedia/frame.h's own crtmedia_frame_convert_to_rgba()
 * comment. */
static void yuv_kr_kb(crtmedia_color_space space, double* kr, double* kb) {
  switch (space) {
    case CRTMEDIA_COLOR_SPACE_BT601:
      *kr = 0.299;
      *kb = 0.114;
      return;
    case CRTMEDIA_COLOR_SPACE_BT2020:
      *kr = 0.2627;
      *kb = 0.0593;
      return;
    case CRTMEDIA_COLOR_SPACE_BT709:
    case CRTMEDIA_COLOR_SPACE_UNSPECIFIED:
    default:
      *kr = 0.2126;
      *kb = 0.0722;
      return;
  }
}

static uint8_t clamp_u8(double v) {
  if (v <= 0.0) return 0;
  if (v >= 255.0) return 255;
  return (uint8_t)(v + 0.5);
}

/* Converts one Y/Cb/Cr triple (already range-scaled to 0-255 by the
 * caller -- see convert_yuv420p_to_rgba()'s own luma_scale/chroma_scale)
 * into RGB using the standard inverse-YCbCr matrix parameterized by
 * (kr, kb): Cb = (B - Y) / (2 * (1 - kb)), Cr = (R - Y) / (2 * (1 - kr)),
 * inverted here. */
static void ycbcr_to_rgb(double y, double cb, double cr, double kr, double kb, uint8_t* r,
                          uint8_t* g, uint8_t* b) {
  double kg = 1.0 - kr - kb;
  double cb_c = cb - 128.0;
  double cr_c = cr - 128.0;
  *r = clamp_u8(y + (2.0 * (1.0 - kr)) * cr_c);
  *b = clamp_u8(y + (2.0 * (1.0 - kb)) * cb_c);
  *g = clamp_u8(y - (2.0 * kr * (1.0 - kr) / kg) * cr_c - (2.0 * kb * (1.0 - kb) / kg) * cb_c);
}

static crtmedia_result convert_yuv420p_to_rgba(const crtmedia_frame* src, crtmedia_frame* dst) {
  if (src->plane_count < 3 || src->planes[0].data == NULL || src->planes[1].data == NULL ||
      src->planes[2].data == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }

  double kr, kb;
  yuv_kr_kb(src->color_space, &kr, &kb);

  /* LIMITED range (the default -- UNSPECIFIED maps here too, matching
   * this file's own/frame.h's top comment): luma's real swing is
   * [16, 235] (219 raw units), chroma's is [16, 240] (224 raw units),
   * both centered so raw 128 chroma means "no color". Scale each up to
   * the full [0, 255] this function's own ycbcr_to_rgb() expects,
   * around each channel's own true center (0 for luma, 128 for chroma)
   * -- FULL range is already exactly that, scale 1.0. */
  int limited = src->color_range != CRTMEDIA_COLOR_RANGE_FULL;
  double luma_scale = limited ? 255.0 / 219.0 : 1.0;
  double chroma_scale = limited ? 255.0 / 224.0 : 1.0;

  const uint8_t* y_plane = (const uint8_t*)src->planes[0].data;
  const uint8_t* u_plane = (const uint8_t*)src->planes[1].data;
  const uint8_t* v_plane = (const uint8_t*)src->planes[2].data;
  uint8_t* dst_base = (uint8_t*)dst->planes[0].data;

  for (uint32_t row = 0; row < src->height; ++row) {
    const uint8_t* y_row = y_plane + (size_t)row * src->planes[0].stride;
    const uint8_t* u_row = u_plane + (size_t)(row / 2) * src->planes[1].stride;
    const uint8_t* v_row = v_plane + (size_t)(row / 2) * src->planes[2].stride;
    uint8_t* dst_row = dst_base + (size_t)row * dst->planes[0].stride;

    for (uint32_t col = 0; col < src->width; ++col) {
      double y = limited ? ((double)y_row[col] - 16.0) * luma_scale : (double)y_row[col];
      double cb = 128.0 + ((double)u_row[col / 2] - 128.0) * chroma_scale;
      double cr = 128.0 + ((double)v_row[col / 2] - 128.0) * chroma_scale;

      uint8_t r, g, b;
      ycbcr_to_rgb(y, cb, cr, kr, kb, &r, &g, &b);
      dst_row[col * 4 + 0] = r;
      dst_row[col * 4 + 1] = g;
      dst_row[col * 4 + 2] = b;
      dst_row[col * 4 + 3] = 255;
    }
  }
  return CRTMEDIA_OK;
}

static crtmedia_result convert_packed_rgb_to_rgba(const crtmedia_frame* src, crtmedia_frame* dst) {
  if (src->planes[0].data == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  int swap_rb = src->format == CRTMEDIA_PIXEL_FORMAT_BGRA8888;
  const uint8_t* src_base = (const uint8_t*)src->planes[0].data;
  uint8_t* dst_base = (uint8_t*)dst->planes[0].data;

  for (uint32_t row = 0; row < src->height; ++row) {
    const uint8_t* src_row = src_base + (size_t)row * src->planes[0].stride;
    uint8_t* dst_row = dst_base + (size_t)row * dst->planes[0].stride;
    for (uint32_t col = 0; col < src->width; ++col) {
      uint8_t c0 = src_row[col * 4 + 0];
      uint8_t c1 = src_row[col * 4 + 1];
      uint8_t c2 = src_row[col * 4 + 2];
      uint8_t c3 = src_row[col * 4 + 3];
      dst_row[col * 4 + 0] = swap_rb ? c2 : c0;
      dst_row[col * 4 + 1] = c1;
      dst_row[col * 4 + 2] = swap_rb ? c0 : c2;
      dst_row[col * 4 + 3] = c3;
    }
  }
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_frame_convert_to_rgba(const crtmedia_frame* src, crtmedia_frame* dst) {
  if (src == NULL || dst == NULL || dst->format != CRTMEDIA_PIXEL_FORMAT_RGBA8888 ||
      dst->width != src->width || dst->height != src->height || dst->planes[0].data == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }

  switch (src->format) {
    case CRTMEDIA_PIXEL_FORMAT_RGBA8888:
    case CRTMEDIA_PIXEL_FORMAT_BGRA8888:
      return convert_packed_rgb_to_rgba(src, dst);
    case CRTMEDIA_PIXEL_FORMAT_YUV420P:
      return convert_yuv420p_to_rgba(src, dst);
    default:
      return CRTMEDIA_ERROR_UNSUPPORTED;
  }
}
