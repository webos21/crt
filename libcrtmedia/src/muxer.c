/* FFmpeg-backed container writer for crtmedia/muxer.h. FFmpeg types stay in
 * this translation unit; public timing is always microseconds. */

#include "crtmedia/muxer.h"

#include <libavcodec/codec_id.h>
#include <libavformat/avformat.h>
#include <libavutil/mem.h>

#include <stdlib.h>
#include <string.h>

struct crtmedia_muxer {
  AVFormatContext* format_context;
  int started;
  int finished;
};

crtmedia_result crtmedia_muxer_create(
    const char* path, crtmedia_muxer_output_format output_format,
    crtmedia_muxer** out_muxer) {
  if (path == NULL || out_muxer == NULL || output_format != CRTMEDIA_MUXER_OUTPUT_MPEG_4) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  *out_muxer = NULL;
  crtmedia_muxer* muxer = (crtmedia_muxer*)calloc(1, sizeof(*muxer));
  if (muxer == NULL) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  if (avformat_alloc_output_context2(&muxer->format_context, NULL, "mp4", path) < 0 ||
      muxer->format_context == NULL) {
    free(muxer);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  if (avio_open(&muxer->format_context->pb, path, AVIO_FLAG_WRITE) < 0) {
    avformat_free_context(muxer->format_context);
    free(muxer);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  *out_muxer = muxer;
  return CRTMEDIA_OK;
}

void crtmedia_muxer_release(crtmedia_muxer* muxer) {
  if (muxer == NULL) {
    return;
  }
  if (muxer->format_context != NULL) {
    if (muxer->format_context->pb != NULL) {
      avio_closep(&muxer->format_context->pb);
    }
    avformat_free_context(muxer->format_context);
  }
  free(muxer);
}

crtmedia_result crtmedia_muxer_add_track(
    crtmedia_muxer* muxer, const crtmedia_format* format,
    uint32_t* out_track_index) {
  if (muxer == NULL || format == NULL || out_track_index == NULL || muxer->started) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  const char* mime = NULL;
  int32_t width = 0;
  int32_t height = 0;
  int32_t bit_rate = 0;
  int32_t frame_rate = 0;
  if (crtmedia_format_get_string(format, CRTMEDIA_FORMAT_KEY_MIME, &mime) != CRTMEDIA_OK ||
      strcmp(mime, "video/mp4v-es") != 0 ||
      crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_WIDTH, &width) != CRTMEDIA_OK ||
      crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_HEIGHT, &height) != CRTMEDIA_OK ||
      width <= 0 || height <= 0) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_BIT_RATE, &bit_rate);
  crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_FRAME_RATE, &frame_rate);

  AVStream* stream = avformat_new_stream(muxer->format_context, NULL);
  if (stream == NULL) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  stream->time_base = AV_TIME_BASE_Q;
  if (frame_rate > 0) {
    stream->avg_frame_rate = (AVRational){frame_rate, 1};
  }
  stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
  stream->codecpar->codec_id = AV_CODEC_ID_MPEG4;
  stream->codecpar->format = AV_PIX_FMT_YUV420P;
  stream->codecpar->width = width;
  stream->codecpar->height = height;
  stream->codecpar->bit_rate = bit_rate;

  const void* csd = NULL;
  size_t csd_size = 0;
  if (crtmedia_format_get_buffer(format, CRTMEDIA_FORMAT_KEY_CSD, &csd, &csd_size) == CRTMEDIA_OK &&
      csd_size > 0) {
    stream->codecpar->extradata = (uint8_t*)av_mallocz(csd_size + AV_INPUT_BUFFER_PADDING_SIZE);
    if (stream->codecpar->extradata == NULL) {
      return CRTMEDIA_ERROR_UNSUPPORTED;
    }
    memcpy(stream->codecpar->extradata, csd, csd_size);
    stream->codecpar->extradata_size = (int)csd_size;
  }
  *out_track_index = stream->index;
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_muxer_start(crtmedia_muxer* muxer) {
  if (muxer == NULL || muxer->started || muxer->format_context->nb_streams == 0) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  if (avformat_write_header(muxer->format_context, NULL) < 0) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  muxer->started = 1;
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_muxer_write_sample(
    crtmedia_muxer* muxer, uint32_t track_index,
    const crtmedia_encoded_sample* sample) {
  if (muxer == NULL || sample == NULL || !muxer->started || muxer->finished ||
      track_index >= muxer->format_context->nb_streams ||
      (sample->data == NULL && sample->size > 0)) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  AVPacket* packet = av_packet_alloc();
  if (packet == NULL || av_new_packet(packet, (int)sample->size) < 0) {
    av_packet_free(&packet);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  memcpy(packet->data, sample->data, sample->size);
  AVStream* stream = muxer->format_context->streams[track_index];
  packet->stream_index = (int)track_index;
  packet->pts = av_rescale_q(sample->pts_us, AV_TIME_BASE_Q, stream->time_base);
  packet->dts = av_rescale_q(sample->dts_us, AV_TIME_BASE_Q, stream->time_base);
  packet->duration = av_rescale_q(sample->duration_us, AV_TIME_BASE_Q, stream->time_base);
  if ((sample->flags & CRTMEDIA_ENCODED_SAMPLE_FLAG_KEY_FRAME) != 0) {
    packet->flags |= AV_PKT_FLAG_KEY;
  }
  int ret = av_interleaved_write_frame(muxer->format_context, packet);
  av_packet_free(&packet);
  return ret < 0 ? CRTMEDIA_ERROR_UNSUPPORTED : CRTMEDIA_OK;
}

crtmedia_result crtmedia_muxer_finish(crtmedia_muxer* muxer) {
  if (muxer == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  if (muxer->finished) {
    return CRTMEDIA_OK;
  }
  if (!muxer->started) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  int ret = av_write_trailer(muxer->format_context);
  muxer->finished = 1;
  if (muxer->format_context->pb != NULL) {
    avio_closep(&muxer->format_context->pb);
  }
  return ret < 0 ? CRTMEDIA_ERROR_UNSUPPORTED : CRTMEDIA_OK;
}
