/* Linux camera capture backend. V4L2 UAPI types and ioctl numbers stay in
 * this private translation unit; crtmedia/capture.h exposes no host SDK type.
 * The isolated CRT build opts this one file into the real Linux UAPI include
 * path with -fcrt-real-linux-sdk, matching gpu_frame_vaapi.c's policy. */
#include "capture_internal.h"
#include "capture_v4l2_test_control.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/* linux/videodev2.h brings asm-generic/ioctl.h's UAPI request macros. The
 * CRT's public sys/ioctl.h intentionally defines the same macros, so including
 * both under -Werror would be a duplicate-definition warning. Keep the libc
 * call declaration local while using the kernel header's request values. */
extern int ioctl(int fd, int request, ...);

#define CRTMEDIA_V4L2_MAX_BUFFERS 8u
#define CRTMEDIA_V4L2_SCAN_LIMIT 64u

typedef struct crtmedia_v4l2_buffer {
  void* address;
  size_t length;
} crtmedia_v4l2_buffer;

typedef struct crtmedia_v4l2_capture {
  int fd;
  int started;
  uint32_t buffer_count;
  crtmedia_v4l2_buffer buffers[CRTMEDIA_V4L2_MAX_BUFFERS];
  uint32_t width;
  uint32_t height;
  uint32_t source_stride;
  crtmedia_v4l2_source_format source_format;
  uint32_t frame_rate;
  int have_timestamp_origin;
  int64_t timestamp_origin_us;
  int64_t last_timestamp_us;
} crtmedia_v4l2_capture;

static int retry_ioctl(int fd, unsigned long request, void* argument) {
  int result;
  do {
    result = ioctl(fd, (int)request, argument);
  } while (result < 0 && errno == EINTR);
  return result;
}

static uint32_t effective_capabilities(const struct v4l2_capability* capability) {
  if ((capability->capabilities & V4L2_CAP_DEVICE_CAPS) != 0) {
    return capability->device_caps;
  }
  return capability->capabilities;
}

static int is_streaming_capture(int fd, struct v4l2_capability* out_capability) {
  uint32_t capabilities;
  memset(out_capability, 0, sizeof(*out_capability));
  if (retry_ioctl(fd, VIDIOC_QUERYCAP, out_capability) < 0) {
    return 0;
  }
  capabilities = effective_capabilities(out_capability);
  return (capabilities & V4L2_CAP_VIDEO_CAPTURE) != 0 &&
         (capabilities & V4L2_CAP_STREAMING) != 0;
}

crtmedia_result crtmedia_capture_backend_enumerate(
    crtmedia_capture_device_info* devices, size_t capacity, size_t* out_count) {
  uint32_t index;
  size_t count = 0;
  for (index = 0; index < CRTMEDIA_V4L2_SCAN_LIMIT; ++index) {
    char path[32];
    struct v4l2_capability capability;
    int fd;
    snprintf(path, sizeof(path), "/dev/video%u", index);
    fd = open(path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
      continue;
    }
    if (is_streaming_capture(fd, &capability)) {
      if (count < capacity) {
        snprintf(devices[count].id, sizeof(devices[count].id), "%s", path);
        snprintf(devices[count].name, sizeof(devices[count].name), "%s",
                 (const char*)capability.card);
      }
      ++count;
    }
    close(fd);
  }
  *out_count = count;
  return CRTMEDIA_OK;
}

static crtmedia_v4l2_source_format source_format_from_fourcc(uint32_t fourcc) {
  switch (fourcc) {
    case V4L2_PIX_FMT_YUV420:
      return CRTMEDIA_V4L2_SOURCE_YUV420;
    case V4L2_PIX_FMT_NV12:
      return CRTMEDIA_V4L2_SOURCE_NV12;
    case V4L2_PIX_FMT_YUYV:
      return CRTMEDIA_V4L2_SOURCE_YUYV;
    default:
      return 0;
  }
}

static void owned_frame_release(crtmedia_frame* frame, void* context) {
  (void)frame;
  free(context);
}

static int multiply_size(size_t a, size_t b, size_t* out) {
  if (a != 0 && b > ((size_t)-1) / a) {
    return 0;
  }
  *out = a * b;
  return 1;
}

crtmedia_result crtmedia_v4l2_convert_to_yuv420p(
    const void* source, size_t source_size,
    crtmedia_v4l2_source_format source_format,
    uint32_t width, uint32_t height, uint32_t source_stride,
    crtmedia_frame* out_frame) {
  const uint8_t* input = (const uint8_t*)source;
  uint8_t* storage;
  uint8_t* y_plane;
  uint8_t* u_plane;
  uint8_t* v_plane;
  uint32_t chroma_width;
  uint32_t chroma_height;
  size_t y_size;
  size_t chroma_size;
  size_t storage_size;
  size_t required_size;
  uint32_t row;

  if (source == NULL || out_frame == NULL || width == 0 || height == 0 ||
      width == UINT32_MAX || height == UINT32_MAX || source_stride == 0) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  memset(out_frame, 0, sizeof(*out_frame));
  chroma_width = (width + 1u) / 2u;
  chroma_height = (height + 1u) / 2u;
  if (!multiply_size((size_t)width, height, &y_size) ||
      !multiply_size((size_t)chroma_width, chroma_height, &chroma_size) ||
      chroma_size > (((size_t)-1) - y_size) / 2u) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  storage_size = y_size + chroma_size * 2u;
  storage = (uint8_t*)malloc(storage_size);
  if (storage == NULL) {
    return CRTMEDIA_ERROR_IO;
  }
  y_plane = storage;
  u_plane = y_plane + y_size;
  v_plane = u_plane + chroma_size;

  if (source_format == CRTMEDIA_V4L2_SOURCE_YUV420) {
    uint32_t chroma_stride = (source_stride + 1u) / 2u;
    const uint8_t* source_u;
    const uint8_t* source_v;
    size_t source_y_size;
    size_t source_chroma_size;
    if (source_stride < width || chroma_stride < chroma_width ||
        !multiply_size(source_stride, height, &source_y_size) ||
        !multiply_size(chroma_stride, chroma_height, &source_chroma_size) ||
        source_chroma_size > (((size_t)-1) - source_y_size) / 2u) {
      free(storage);
      return CRTMEDIA_ERROR_INVALID_ARGUMENT;
    }
    required_size = source_y_size + source_chroma_size * 2u;
    if (source_size < required_size) {
      free(storage);
      return CRTMEDIA_ERROR_INVALID_ARGUMENT;
    }
    source_u = input + source_y_size;
    source_v = source_u + source_chroma_size;
    for (row = 0; row < height; ++row) {
      memcpy(y_plane + (size_t)row * width,
             input + (size_t)row * source_stride, width);
    }
    for (row = 0; row < chroma_height; ++row) {
      memcpy(u_plane + (size_t)row * chroma_width,
             source_u + (size_t)row * chroma_stride, chroma_width);
      memcpy(v_plane + (size_t)row * chroma_width,
             source_v + (size_t)row * chroma_stride, chroma_width);
    }
  } else if (source_format == CRTMEDIA_V4L2_SOURCE_NV12) {
    const uint8_t* source_uv;
    size_t source_y_size;
    size_t source_uv_size;
    uint32_t column;
    if (chroma_width > UINT32_MAX / 2u || source_stride < width ||
        source_stride < chroma_width * 2u ||
        !multiply_size(source_stride, height, &source_y_size) ||
        !multiply_size(source_stride, chroma_height, &source_uv_size) ||
        source_uv_size > (size_t)-1 - source_y_size) {
      free(storage);
      return CRTMEDIA_ERROR_INVALID_ARGUMENT;
    }
    required_size = source_y_size + source_uv_size;
    if (source_size < required_size) {
      free(storage);
      return CRTMEDIA_ERROR_INVALID_ARGUMENT;
    }
    source_uv = input + source_y_size;
    for (row = 0; row < height; ++row) {
      memcpy(y_plane + (size_t)row * width,
             input + (size_t)row * source_stride, width);
    }
    for (row = 0; row < chroma_height; ++row) {
      for (column = 0; column < chroma_width; ++column) {
        u_plane[(size_t)row * chroma_width + column] =
            source_uv[(size_t)row * source_stride + column * 2u];
        v_plane[(size_t)row * chroma_width + column] =
            source_uv[(size_t)row * source_stride + column * 2u + 1u];
      }
    }
  } else if (source_format == CRTMEDIA_V4L2_SOURCE_YUYV) {
    uint32_t chroma_row;
    uint32_t column;
    if ((width & 1u) != 0 || width > UINT32_MAX / 2u ||
        source_stride < width * 2u ||
        !multiply_size(source_stride, height, &required_size) ||
        source_size < required_size) {
      free(storage);
      return CRTMEDIA_ERROR_INVALID_ARGUMENT;
    }
    for (row = 0; row < height; ++row) {
      const uint8_t* source_row = input + (size_t)row * source_stride;
      for (column = 0; column < width; column += 2u) {
        y_plane[(size_t)row * width + column] = source_row[column * 2u];
        y_plane[(size_t)row * width + column + 1u] =
            source_row[column * 2u + 2u];
      }
    }
    for (chroma_row = 0; chroma_row < chroma_height; ++chroma_row) {
      uint32_t row0 = chroma_row * 2u;
      uint32_t row1 = row0 + 1u < height ? row0 + 1u : row0;
      const uint8_t* source_row0 = input + (size_t)row0 * source_stride;
      const uint8_t* source_row1 = input + (size_t)row1 * source_stride;
      for (column = 0; column < chroma_width; ++column) {
        size_t pair_offset = (size_t)column * 4u;
        u_plane[(size_t)chroma_row * chroma_width + column] =
            (uint8_t)(((uint32_t)source_row0[pair_offset + 1u] +
                       source_row1[pair_offset + 1u] + 1u) /
                      2u);
        v_plane[(size_t)chroma_row * chroma_width + column] =
            (uint8_t)(((uint32_t)source_row0[pair_offset + 3u] +
                       source_row1[pair_offset + 3u] + 1u) /
                      2u);
      }
    }
  } else {
    free(storage);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  out_frame->format = CRTMEDIA_PIXEL_FORMAT_YUV420P;
  out_frame->width = width;
  out_frame->height = height;
  out_frame->color_range = CRTMEDIA_COLOR_RANGE_LIMITED;
  out_frame->color_space =
      height <= 576u ? CRTMEDIA_COLOR_SPACE_BT601 : CRTMEDIA_COLOR_SPACE_BT709;
  out_frame->timestamp_us = CRTMEDIA_FRAME_TIMESTAMP_NONE;
  out_frame->plane_count = 3;
  out_frame->planes[0].data = y_plane;
  out_frame->planes[0].stride = width;
  out_frame->planes[0].width = width;
  out_frame->planes[0].height = height;
  out_frame->planes[1].data = u_plane;
  out_frame->planes[1].stride = chroma_width;
  out_frame->planes[1].width = chroma_width;
  out_frame->planes[1].height = chroma_height;
  out_frame->planes[2].data = v_plane;
  out_frame->planes[2].stride = chroma_width;
  out_frame->planes[2].width = chroma_width;
  out_frame->planes[2].height = chroma_height;
  out_frame->release = owned_frame_release;
  out_frame->release_context = storage;
  return CRTMEDIA_OK;
}

static void release_backend(crtmedia_v4l2_capture* capture) {
  uint32_t index;
  if (capture == NULL) {
    return;
  }
  if (capture->started) {
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    retry_ioctl(capture->fd, VIDIOC_STREAMOFF, &type);
  }
  for (index = 0; index < capture->buffer_count; ++index) {
    if (capture->buffers[index].address != NULL &&
        capture->buffers[index].address != MAP_FAILED) {
      munmap(capture->buffers[index].address, capture->buffers[index].length);
    }
  }
  if (capture->fd >= 0) {
    close(capture->fd);
  }
  free(capture);
}

crtmedia_result crtmedia_capture_backend_open(
    const char* device_id, const crtmedia_capture_config* requested,
    void** out_backend, crtmedia_capture_config* out_actual) {
  static const uint32_t preferred_formats[] = {
      V4L2_PIX_FMT_YUV420, V4L2_PIX_FMT_NV12, V4L2_PIX_FMT_YUYV};
  crtmedia_v4l2_capture* capture;
  struct v4l2_capability capability;
  struct v4l2_format format;
  struct v4l2_requestbuffers request_buffers;
  uint32_t requested_width = requested != NULL && requested->width != 0
                                 ? requested->width : 640u;
  uint32_t requested_height = requested != NULL && requested->height != 0
                                  ? requested->height : 480u;
  uint32_t requested_rate = requested != NULL && requested->frame_rate != 0
                                ? requested->frame_rate : 30u;
  uint32_t format_index;
  int negotiated = 0;

  if (out_backend == NULL || out_actual == NULL ||
      (requested != NULL && requested->format != 0 &&
       requested->format != CRTMEDIA_PIXEL_FORMAT_YUV420P)) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  *out_backend = NULL;
  capture = (crtmedia_v4l2_capture*)calloc(1, sizeof(*capture));
  if (capture == NULL) {
    return CRTMEDIA_ERROR_IO;
  }
  capture->fd = -1;
  capture->last_timestamp_us = -1;
  capture->fd = open(device_id, O_RDWR | O_NONBLOCK | O_CLOEXEC);
  if (capture->fd < 0) {
    release_backend(capture);
    return CRTMEDIA_ERROR_IO;
  }
  if (!is_streaming_capture(capture->fd, &capability)) {
    release_backend(capture);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  for (format_index = 0;
       format_index < sizeof(preferred_formats) / sizeof(preferred_formats[0]);
       ++format_index) {
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = requested_width;
    format.fmt.pix.height = requested_height;
    format.fmt.pix.pixelformat = preferred_formats[format_index];
    format.fmt.pix.field = V4L2_FIELD_ANY;
    if (retry_ioctl(capture->fd, VIDIOC_S_FMT, &format) == 0 &&
        source_format_from_fourcc(format.fmt.pix.pixelformat) != 0) {
      negotiated = 1;
      break;
    }
  }
  if (!negotiated || format.fmt.pix.width == 0 || format.fmt.pix.height == 0) {
    release_backend(capture);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  capture->width = format.fmt.pix.width;
  capture->height = format.fmt.pix.height;
  capture->source_stride = format.fmt.pix.bytesperline;
  capture->source_format = source_format_from_fourcc(format.fmt.pix.pixelformat);
  if (capture->source_stride == 0) {
    capture->source_stride = capture->source_format == CRTMEDIA_V4L2_SOURCE_YUYV
                                 ? capture->width * 2u : capture->width;
  }
  capture->frame_rate = requested_rate;

  {
    struct v4l2_streamparm parameters;
    memset(&parameters, 0, sizeof(parameters));
    parameters.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parameters.parm.capture.timeperframe.numerator = 1;
    parameters.parm.capture.timeperframe.denominator = requested_rate;
    if (retry_ioctl(capture->fd, VIDIOC_S_PARM, &parameters) == 0 &&
        parameters.parm.capture.timeperframe.numerator != 0 &&
        parameters.parm.capture.timeperframe.denominator != 0) {
      capture->frame_rate =
          (parameters.parm.capture.timeperframe.denominator +
           parameters.parm.capture.timeperframe.numerator / 2u) /
          parameters.parm.capture.timeperframe.numerator;
      if (capture->frame_rate == 0) {
        capture->frame_rate = requested_rate;
      }
    }
  }

  memset(&request_buffers, 0, sizeof(request_buffers));
  request_buffers.count = 4;
  request_buffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  request_buffers.memory = V4L2_MEMORY_MMAP;
  if (retry_ioctl(capture->fd, VIDIOC_REQBUFS, &request_buffers) < 0 ||
      request_buffers.count < 2) {
    release_backend(capture);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  capture->buffer_count = request_buffers.count > CRTMEDIA_V4L2_MAX_BUFFERS
                              ? CRTMEDIA_V4L2_MAX_BUFFERS : request_buffers.count;
  for (format_index = 0; format_index < capture->buffer_count; ++format_index) {
    struct v4l2_buffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = format_index;
    if (retry_ioctl(capture->fd, VIDIOC_QUERYBUF, &buffer) < 0) {
      release_backend(capture);
      return CRTMEDIA_ERROR_IO;
    }
    capture->buffers[format_index].length = buffer.length;
    capture->buffers[format_index].address =
        mmap(NULL, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED,
             capture->fd, (off_t)buffer.m.offset);
    if (capture->buffers[format_index].address == MAP_FAILED) {
      release_backend(capture);
      return CRTMEDIA_ERROR_IO;
    }
  }

  out_actual->width = capture->width;
  out_actual->height = capture->height;
  out_actual->frame_rate = capture->frame_rate;
  out_actual->format = CRTMEDIA_PIXEL_FORMAT_YUV420P;
  *out_backend = capture;
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_capture_backend_start(void* backend) {
  crtmedia_v4l2_capture* capture = (crtmedia_v4l2_capture*)backend;
  uint32_t index;
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (capture->started) {
    return CRTMEDIA_OK;
  }
  for (index = 0; index < capture->buffer_count; ++index) {
    struct v4l2_buffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = index;
    if (retry_ioctl(capture->fd, VIDIOC_QBUF, &buffer) < 0) {
      return CRTMEDIA_ERROR_IO;
    }
  }
  if (retry_ioctl(capture->fd, VIDIOC_STREAMON, &type) < 0) {
    return CRTMEDIA_ERROR_IO;
  }
  capture->started = 1;
  capture->have_timestamp_origin = 0;
  capture->last_timestamp_us = -1;
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_capture_backend_dequeue(
    void* backend, int timeout_ms, crtmedia_frame* out_frame) {
  crtmedia_v4l2_capture* capture = (crtmedia_v4l2_capture*)backend;
  struct pollfd descriptor;
  struct v4l2_buffer buffer;
  crtmedia_result result;
  int poll_result;
  int64_t native_timestamp_us;
  int64_t timestamp_us;
  int64_t nominal_step = capture->frame_rate != 0
                             ? 1000000 / (int64_t)capture->frame_rate : 1;
  if (!capture->started) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  memset(out_frame, 0, sizeof(*out_frame));
  descriptor.fd = capture->fd;
  descriptor.events = POLLIN | POLLPRI;
  descriptor.revents = 0;
  do {
    poll_result = poll(&descriptor, 1, timeout_ms);
  } while (poll_result < 0 && errno == EINTR);
  if (poll_result == 0) {
    return CRTMEDIA_WOULD_BLOCK;
  }
  if (poll_result < 0 || (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
    return CRTMEDIA_ERROR_IO;
  }
  memset(&buffer, 0, sizeof(buffer));
  buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buffer.memory = V4L2_MEMORY_MMAP;
  if (retry_ioctl(capture->fd, VIDIOC_DQBUF, &buffer) < 0) {
    return errno == EAGAIN ? CRTMEDIA_WOULD_BLOCK : CRTMEDIA_ERROR_IO;
  }
  if (buffer.index >= capture->buffer_count) {
    return CRTMEDIA_ERROR_IO;
  }
  result = crtmedia_v4l2_convert_to_yuv420p(
      capture->buffers[buffer.index].address,
      buffer.bytesused != 0 ? buffer.bytesused : capture->buffers[buffer.index].length,
      capture->source_format, capture->width, capture->height,
      capture->source_stride, out_frame);
  if (retry_ioctl(capture->fd, VIDIOC_QBUF, &buffer) < 0) {
    if (result == CRTMEDIA_OK) {
      crtmedia_frame_release(out_frame);
    }
    return CRTMEDIA_ERROR_IO;
  }
  if (result != CRTMEDIA_OK) {
    return result;
  }
  native_timestamp_us = (int64_t)buffer.timestamp.tv_sec * 1000000 +
                        (int64_t)buffer.timestamp.tv_usec;
  if (!capture->have_timestamp_origin) {
    capture->timestamp_origin_us = native_timestamp_us;
    capture->have_timestamp_origin = 1;
    timestamp_us = 0;
  } else {
    timestamp_us = native_timestamp_us - capture->timestamp_origin_us;
    if (timestamp_us <= capture->last_timestamp_us) {
      timestamp_us = capture->last_timestamp_us + (nominal_step > 0 ? nominal_step : 1);
    }
  }
  capture->last_timestamp_us = timestamp_us;
  out_frame->timestamp_us = timestamp_us;
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_capture_backend_stop(void* backend) {
  crtmedia_v4l2_capture* capture = (crtmedia_v4l2_capture*)backend;
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (!capture->started) {
    return CRTMEDIA_OK;
  }
  if (retry_ioctl(capture->fd, VIDIOC_STREAMOFF, &type) < 0) {
    return CRTMEDIA_ERROR_IO;
  }
  capture->started = 0;
  return CRTMEDIA_OK;
}

void crtmedia_capture_backend_release(void* backend) {
  release_backend((crtmedia_v4l2_capture*)backend);
}
