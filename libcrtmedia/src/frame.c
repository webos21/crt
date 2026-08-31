#include "crtmedia/frame.h"

#include <string.h>

crtmedia_result crtmedia_frame_describe_planes(
    crtmedia_pixel_format format,
    uint32_t width,
    uint32_t height,
    crtmedia_frame_plane out_planes[CRTMEDIA_FRAME_MAX_PLANES],
    uint32_t* out_plane_count) {
  if (out_planes == NULL || out_plane_count == NULL || width == 0 || height == 0) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }

  memset(out_planes, 0, sizeof(crtmedia_frame_plane) * CRTMEDIA_FRAME_MAX_PLANES);

  switch (format) {
    case CRTMEDIA_PIXEL_FORMAT_RGBA8888:
    case CRTMEDIA_PIXEL_FORMAT_BGRA8888:
      out_planes[0].width = width;
      out_planes[0].height = height;
      out_planes[0].stride = width * 4u;
      *out_plane_count = 1;
      return CRTMEDIA_OK;

    case CRTMEDIA_PIXEL_FORMAT_YUV420P: {
      /* 4:2:0 subsampling: each chroma plane is half resolution in both
       * dimensions, rounded up (an odd luma dimension still needs one
       * more chroma sample to cover it -- (width + 1) / 2, not width /
       * 2, matching how every real decoder describes an odd-sized
       * 4:2:0 frame). */
      uint32_t chroma_width = (width + 1u) / 2u;
      uint32_t chroma_height = (height + 1u) / 2u;
      out_planes[0].width = width;
      out_planes[0].height = height;
      out_planes[0].stride = width;
      out_planes[1].width = chroma_width;
      out_planes[1].height = chroma_height;
      out_planes[1].stride = chroma_width;
      out_planes[2].width = chroma_width;
      out_planes[2].height = chroma_height;
      out_planes[2].stride = chroma_width;
      *out_plane_count = 3;
      return CRTMEDIA_OK;
    }

    default:
      return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
}

void crtmedia_frame_release(crtmedia_frame* frame) {
  if (frame == NULL) {
    return;
  }
  if (frame->release != NULL) {
    frame->release(frame, frame->release_context);
  }
  memset(frame, 0, sizeof(*frame));
}
