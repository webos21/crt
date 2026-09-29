/* FFmpeg-backed container writer for crtmedia/muxer.h. FFmpeg types stay in
 * this translation unit; public timing is always microseconds. */

#include "crtmedia/muxer.h"

#if defined(CRTMEDIA_HAVE_HTTP_TRANSPORT)
#include "http_upload.h"
#endif

#include <libavcodec/codec_id.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/mem.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

struct crtmedia_muxer {
  AVFormatContext* format_context;
  int fragmented;
  int started;
  int finished;
  crtmedia_result finish_result;
  uint32_t capabilities;
#if defined(CRTMEDIA_HAVE_HTTP_TRANSPORT)
  crtmedia_http_upload* upload;
  AVIOContext* upload_io;
#endif
};

crtmedia_result crtmedia_muxer_create(
    const char* path, crtmedia_muxer_output_format output_format,
    crtmedia_muxer** out_muxer) {
  if (path == NULL || out_muxer == NULL ||
      (output_format != CRTMEDIA_MUXER_OUTPUT_MPEG_4 && output_format != CRTMEDIA_MUXER_OUTPUT_MPEG_4_FRAGMENTED)) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  *out_muxer = NULL;
  crtmedia_muxer* muxer = (crtmedia_muxer*)calloc(1, sizeof(*muxer));
  if (muxer == NULL) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  muxer->fragmented = output_format == CRTMEDIA_MUXER_OUTPUT_MPEG_4_FRAGMENTED;
  muxer->capabilities = CRTMEDIA_SINK_WRITABLE | CRTMEDIA_SINK_SEEKABLE;
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

#if defined(CRTMEDIA_HAVE_HTTP_TRANSPORT)
static int http_upload_write_packet(void* opaque, const uint8_t* buffer, int buffer_size) {
  crtmedia_http_upload* upload = (crtmedia_http_upload*)opaque;
  crtmedia_result result = crtmedia_http_upload_write(upload, buffer, (size_t)buffer_size);
  return result == CRTMEDIA_OK ? buffer_size : AVERROR(EIO);
}
#endif

crtmedia_result crtmedia_muxer_create_for_url(
    const char* url, crtmedia_muxer_output_format output_format,
    uint32_t queue_capacity, crtmedia_muxer** out_muxer) {
  if (url == NULL || out_muxer == NULL || output_format != CRTMEDIA_MUXER_OUTPUT_MPEG_4_FRAGMENTED) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  *out_muxer = NULL;
#if !defined(CRTMEDIA_HAVE_HTTP_TRANSPORT)
  (void)queue_capacity;
  return CRTMEDIA_ERROR_UNSUPPORTED;
#else
  crtmedia_muxer* muxer = (crtmedia_muxer*)calloc(1, sizeof(*muxer));
  if (muxer == NULL) {
    return CRTMEDIA_ERROR_IO;
  }
  muxer->fragmented = 1;
  muxer->capabilities = CRTMEDIA_SINK_WRITABLE;
  if (avformat_alloc_output_context2(&muxer->format_context, NULL, "mp4", NULL) < 0 ||
      muxer->format_context == NULL) {
    free(muxer);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  crtmedia_result upload_result = crtmedia_http_upload_open(
      url, (size_t)queue_capacity, &muxer->upload);
  if (upload_result != CRTMEDIA_OK) {
    avformat_free_context(muxer->format_context);
    free(muxer);
    return upload_result;
  }
  unsigned char* avio_buffer = (unsigned char*)av_malloc(32768);
  if (avio_buffer == NULL) {
    crtmedia_http_upload_close(muxer->upload);
    avformat_free_context(muxer->format_context);
    free(muxer);
    return CRTMEDIA_ERROR_IO;
  }
  muxer->upload_io = avio_alloc_context(
      avio_buffer, 32768, 1, muxer->upload, NULL, http_upload_write_packet, NULL);
  if (muxer->upload_io == NULL) {
    av_free(avio_buffer);
    crtmedia_http_upload_close(muxer->upload);
    avformat_free_context(muxer->format_context);
    free(muxer);
    return CRTMEDIA_ERROR_IO;
  }
  muxer->format_context->pb = muxer->upload_io;
  muxer->format_context->flags |= AVFMT_FLAG_CUSTOM_IO;
  *out_muxer = muxer;
  return CRTMEDIA_OK;
#endif
}

uint32_t crtmedia_muxer_get_capabilities(const crtmedia_muxer* muxer) {
  return muxer != NULL ? muxer->capabilities : 0;
}

void crtmedia_muxer_release(crtmedia_muxer* muxer) {
  if (muxer == NULL) {
    return;
  }
  if (muxer->format_context != NULL) {
#if defined(CRTMEDIA_HAVE_HTTP_TRANSPORT)
    if (muxer->upload_io != NULL) {
      muxer->format_context->pb = NULL;
      avio_context_free(&muxer->upload_io);
    } else
#endif
    if (muxer->format_context->pb != NULL) {
      avio_closep(&muxer->format_context->pb);
    }
    avformat_free_context(muxer->format_context);
  }
#if defined(CRTMEDIA_HAVE_HTTP_TRANSPORT)
  crtmedia_http_upload_close(muxer->upload);
#endif
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
  enum AVCodecID codec_id = AV_CODEC_ID_NONE;
  if (crtmedia_format_get_string(format, CRTMEDIA_FORMAT_KEY_MIME, &mime) == CRTMEDIA_OK) {
    if (strcmp(mime, "video/mp4v-es") == 0) {
      codec_id = AV_CODEC_ID_MPEG4;
    } else if (strcmp(mime, "video/avc") == 0) {
      /* VA-API H.264 hardware encode (2026-09-28, "Encode and capture"
       * Tranche 3) -- the encoded bitstream/extradata FFmpeg's own h264_
       * vaapi encoder produces is Annex-B (start-code-prefixed NAL
       * units), not the AVCC/length-prefixed form MP4 normally stores.
       * No conversion is needed here: libavformat's own mov muxer
       * (movenc.c, mov_write_single_packet()) already detects Annex-B
       * H.264 extradata (its own first byte != 1, the avcC
       * configurationVersion) and reformats both the extradata and every
       * packet into avcC form automatically (ff_isom_write_avcc()/
       * ff_nal_parse_units()) -- confirmed by reading movenc.c directly,
       * not assumed. */
      codec_id = AV_CODEC_ID_H264;
    }
  }
  if (codec_id == AV_CODEC_ID_NONE ||
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
  stream->codecpar->codec_id = codec_id;
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
  AVDictionary* options = NULL;
  if (muxer->fragmented) {
    /* frag_keyframe: start a new fragment at every keyframe (this
     * project's own encoders never request non-keyframe-aligned flushes,
     * so this is always safe). empty_moov: write a minimal moov with no
     * sample table before any data, instead of the buffer/rewrite dance
     * mov_flush_fragment() would otherwise need on a non-seekable pb --
     * required for this to work at all when writing straight to a
     * genuinely non-seekable sink (Tranche 3's own HTTP upload). default_
     * base_moof: each moof's own track fragment header carries its base
     * data offset directly, avoiding a dependency on the file's overall
     * moov-relative layout a strict streaming reader might not have. */
    av_dict_set(&options, "movflags", "frag_keyframe+empty_moov+default_base_moof", 0);
  }
  int ret = avformat_write_header(muxer->format_context, &options);
  av_dict_free(&options);
  if (ret < 0) {
#if defined(CRTMEDIA_HAVE_HTTP_TRANSPORT)
    return muxer->upload != NULL ? CRTMEDIA_ERROR_IO : CRTMEDIA_ERROR_UNSUPPORTED;
#else
    return CRTMEDIA_ERROR_UNSUPPORTED;
#endif
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
  if (ret >= 0) {
    return CRTMEDIA_OK;
  }
#if defined(CRTMEDIA_HAVE_HTTP_TRANSPORT)
  return muxer->upload != NULL ? CRTMEDIA_ERROR_IO : CRTMEDIA_ERROR_UNSUPPORTED;
#else
  return CRTMEDIA_ERROR_UNSUPPORTED;
#endif
}

crtmedia_result crtmedia_muxer_finish(crtmedia_muxer* muxer) {
  if (muxer == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  if (muxer->finished) {
    return muxer->finish_result;
  }
  if (!muxer->started) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  int ret = av_write_trailer(muxer->format_context);
  muxer->finished = 1;
#if defined(CRTMEDIA_HAVE_HTTP_TRANSPORT)
  if (muxer->upload != NULL) {
    avio_flush(muxer->format_context->pb);
    if (ret < 0) {
      muxer->finish_result = CRTMEDIA_ERROR_IO;
      return muxer->finish_result;
    }
    muxer->finish_result = crtmedia_http_upload_finish(muxer->upload);
    return muxer->finish_result;
  }
#endif
  if (muxer->format_context->pb != NULL) {
    avio_closep(&muxer->format_context->pb);
  }
  muxer->finish_result = ret < 0 ? CRTMEDIA_ERROR_UNSUPPORTED : CRTMEDIA_OK;
  return muxer->finish_result;
}
