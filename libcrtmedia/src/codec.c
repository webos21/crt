/* crtmedia/codec.h -- see that header's own top comment for the design
 * reasoning. Every real libavcodec/libswresample type stays confined to
 * this file, matching src/demux.c's/src/extractor.c's own established
 * discipline.
 *
 * fill_video_frame()/fill_audio_buffer() below are a deliberate near-
 * duplicate of src/demux.c's own same-named functions, not a shared
 * helper -- both files independently wrap an AVFrame/swr_convert() output
 * into this project's own crtmedia_frame/crtmedia_audio_buffer contract,
 * and demux.c's own functions are `static` to that translation unit.
 * Left duplicated deliberately rather than refactored into a shared
 * internal header this pass: crtmedia_demuxer_* (demux.h) is still its
 * own, separate, already-verified implementation, not yet rebuilt over
 * this new core (see docs/libcrtmedia_api_policy.md's own Decision and
 * TODO.md's own next step) -- sharing code between the two now would mean
 * refactoring already-working demux.c mid-way through landing this new,
 * separate, not-yet-integrated layer, a real regression risk for no
 * benefit until that rebuild actually happens. */

#include "crtmedia/codec.h"

#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libswresample/swresample.h>

#include <stdlib.h>
#include <string.h>

struct crtmedia_codec {
  AVCodecContext* codec_ctx;
  int is_video;
  AVPacket* packet;
  AVFrame* decode_frame;
  SwrContext* swr_ctx; /* audio only */
  crtmedia_sample_format out_sample_format; /* audio only */
  int eof_signaled;
  int eof_drained;
};

static enum AVCodecID codec_id_for_mime(const char* mime) {
  if (strcmp(mime, "video/avc") == 0) {
    return AV_CODEC_ID_H264;
  }
  if (strcmp(mime, "audio/mp4a-latm") == 0) {
    return AV_CODEC_ID_AAC;
  }
  if (strcmp(mime, "audio/mpeg") == 0) {
    return AV_CODEC_ID_MP3;
  }
  if (strcmp(mime, "audio/raw") == 0) {
    return AV_CODEC_ID_PCM_S16LE;
  }
  return AV_CODEC_ID_NONE;
}

crtmedia_result crtmedia_codec_create_decoder(const crtmedia_format* format, crtmedia_codec** out_codec) {
  if (format == NULL || out_codec == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  *out_codec = NULL;

  const char* mime = NULL;
  if (crtmedia_format_get_string(format, CRTMEDIA_FORMAT_KEY_MIME, &mime) != CRTMEDIA_OK) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  enum AVCodecID codec_id = codec_id_for_mime(mime);
  if (codec_id == AV_CODEC_ID_NONE) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  const AVCodec* av_codec = avcodec_find_decoder(codec_id);
  if (av_codec == NULL) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  int is_video = av_codec->type == AVMEDIA_TYPE_VIDEO;

  crtmedia_codec* codec = (crtmedia_codec*)calloc(1, sizeof(crtmedia_codec));
  if (codec == NULL) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  codec->is_video = is_video;
  codec->codec_ctx = avcodec_alloc_context3(av_codec);
  codec->packet = av_packet_alloc();
  codec->decode_frame = av_frame_alloc();
  if (codec->codec_ctx == NULL || codec->packet == NULL || codec->decode_frame == NULL) {
    crtmedia_codec_release(codec);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  if (is_video) {
    int32_t width = 0;
    int32_t height = 0;
    crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_WIDTH, &width);
    crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_HEIGHT, &height);
    codec->codec_ctx->width = width;
    codec->codec_ctx->height = height;
    /* Same real, deliberate multi-threaded decode as demux.c's own video
     * path (see that file's own comment for the full reasoning) -- this
     * codec's own decode is exactly as real a pthread-PAL exercise as
     * demux.c's. */
    codec->codec_ctx->thread_count = 2;
    codec->codec_ctx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
  } else {
    int32_t sample_rate = 0;
    int32_t channel_count = 0;
    crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_SAMPLE_RATE, &sample_rate);
    crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_CHANNEL_COUNT, &channel_count);
    codec->codec_ctx->sample_rate = sample_rate;
    av_channel_layout_default(&codec->codec_ctx->ch_layout, channel_count);
  }

  const void* csd = NULL;
  size_t csd_size = 0;
  if (crtmedia_format_get_buffer(format, CRTMEDIA_FORMAT_KEY_CSD, &csd, &csd_size) == CRTMEDIA_OK && csd_size > 0) {
    codec->codec_ctx->extradata = (uint8_t*)av_mallocz(csd_size + AV_INPUT_BUFFER_PADDING_SIZE);
    if (codec->codec_ctx->extradata == NULL) {
      crtmedia_codec_release(codec);
      return CRTMEDIA_ERROR_UNSUPPORTED;
    }
    memcpy(codec->codec_ctx->extradata, csd, csd_size);
    codec->codec_ctx->extradata_size = (int)csd_size;
  }

  if (avcodec_open2(codec->codec_ctx, av_codec, NULL) < 0) {
    crtmedia_codec_release(codec);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  if (!is_video) {
    /* Same S16-preferred-unless-native-float choice as demux.c's own
     * audio path -- this contract only ever produces S16 or FLT
     * (crtmedia/audio.h's own comment). */
    codec->out_sample_format = (codec->codec_ctx->sample_fmt == AV_SAMPLE_FMT_FLT ||
                                 codec->codec_ctx->sample_fmt == AV_SAMPLE_FMT_FLTP)
                                    ? CRTMEDIA_SAMPLE_FORMAT_FLT
                                    : CRTMEDIA_SAMPLE_FORMAT_S16;
    enum AVSampleFormat out_fmt =
        codec->out_sample_format == CRTMEDIA_SAMPLE_FORMAT_FLT ? AV_SAMPLE_FMT_FLT : AV_SAMPLE_FMT_S16;
    AVChannelLayout out_layout;
    av_channel_layout_default(&out_layout, codec->codec_ctx->ch_layout.nb_channels);
    swr_alloc_set_opts2(
        &codec->swr_ctx, &out_layout, out_fmt, codec->codec_ctx->sample_rate, &codec->codec_ctx->ch_layout,
        codec->codec_ctx->sample_fmt, codec->codec_ctx->sample_rate, 0, NULL);
    av_channel_layout_uninit(&out_layout);
    if (codec->swr_ctx == NULL || swr_init(codec->swr_ctx) < 0) {
      crtmedia_codec_release(codec);
      return CRTMEDIA_ERROR_UNSUPPORTED;
    }
  }

  *out_codec = codec;
  return CRTMEDIA_OK;
}

void crtmedia_codec_release(crtmedia_codec* codec) {
  if (codec == NULL) {
    return;
  }
  if (codec->swr_ctx != NULL) {
    swr_free(&codec->swr_ctx);
  }
  if (codec->decode_frame != NULL) {
    av_frame_free(&codec->decode_frame);
  }
  if (codec->packet != NULL) {
    av_packet_free(&codec->packet);
  }
  if (codec->codec_ctx != NULL) {
    avcodec_free_context(&codec->codec_ctx);
  }
  free(codec);
}

crtmedia_result crtmedia_codec_queue_input(
    crtmedia_codec* codec, const void* data, uint32_t size, int64_t pts_us, uint32_t flags) {
  if (codec == NULL || (data == NULL && size > 0)) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }

  if (size > 0) {
    av_packet_unref(codec->packet);
    if (av_new_packet(codec->packet, (int)size) < 0) {
      return CRTMEDIA_ERROR_UNSUPPORTED;
    }
    memcpy(codec->packet->data, data, size);
    codec->packet->pts = pts_us != CRTMEDIA_FRAME_TIMESTAMP_NONE ? pts_us : AV_NOPTS_VALUE;

    int send_ret = avcodec_send_packet(codec->codec_ctx, codec->packet);
    av_packet_unref(codec->packet);
    if (send_ret == AVERROR(EAGAIN)) {
      return CRTMEDIA_WOULD_BLOCK;
    }
    if (send_ret < 0) {
      return CRTMEDIA_ERROR_UNSUPPORTED;
    }
  }

  if ((flags & CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM) != 0 && !codec->eof_signaled) {
    avcodec_send_packet(codec->codec_ctx, NULL);
    codec->eof_signaled = 1;
  }
  return CRTMEDIA_OK;
}

static void release_video_frame(crtmedia_frame* frame, void* release_context) {
  (void)frame;
  AVFrame* avframe = (AVFrame*)release_context;
  av_frame_free(&avframe);
}

static void fill_video_frame(AVFrame* avframe, crtmedia_frame* out_frame) {
  memset(out_frame, 0, sizeof(*out_frame));
  out_frame->format = CRTMEDIA_PIXEL_FORMAT_YUV420P;
  out_frame->width = (uint32_t)avframe->width;
  out_frame->height = (uint32_t)avframe->height;
  out_frame->color_range =
      avframe->color_range == AVCOL_RANGE_JPEG ? CRTMEDIA_COLOR_RANGE_FULL : CRTMEDIA_COLOR_RANGE_LIMITED;
  switch (avframe->colorspace) {
    case AVCOL_SPC_BT709:
      out_frame->color_space = CRTMEDIA_COLOR_SPACE_BT709;
      break;
    case AVCOL_SPC_BT470BG:
    case AVCOL_SPC_SMPTE170M:
      out_frame->color_space = CRTMEDIA_COLOR_SPACE_BT601;
      break;
    case AVCOL_SPC_BT2020_NCL:
    case AVCOL_SPC_BT2020_CL:
      out_frame->color_space = CRTMEDIA_COLOR_SPACE_BT2020;
      break;
    default:
      out_frame->color_space = CRTMEDIA_COLOR_SPACE_UNSPECIFIED;
      break;
  }
  out_frame->timestamp_us = avframe->pts != AV_NOPTS_VALUE ? avframe->pts : CRTMEDIA_FRAME_TIMESTAMP_NONE;
  out_frame->plane_count = 3;
  uint32_t chroma_width = (out_frame->width + 1u) / 2u;
  uint32_t chroma_height = (out_frame->height + 1u) / 2u;
  out_frame->planes[0] =
      (crtmedia_frame_plane){avframe->data[0], (uint32_t)avframe->linesize[0], out_frame->width, out_frame->height};
  out_frame->planes[1] =
      (crtmedia_frame_plane){avframe->data[1], (uint32_t)avframe->linesize[1], chroma_width, chroma_height};
  out_frame->planes[2] =
      (crtmedia_frame_plane){avframe->data[2], (uint32_t)avframe->linesize[2], chroma_width, chroma_height};
  out_frame->release = release_video_frame;
  out_frame->release_context = avframe;
}

static void release_audio_buffer(crtmedia_audio_buffer* buffer, void* release_context) {
  (void)buffer;
  free(release_context);
}

static crtmedia_result fill_audio_buffer(crtmedia_codec* codec, AVFrame* avframe, crtmedia_audio_buffer* out_buffer) {
  int out_sample_size = codec->out_sample_format == CRTMEDIA_SAMPLE_FORMAT_FLT ? 4 : 2;
  int channels = codec->codec_ctx->ch_layout.nb_channels;
  int max_out_samples = (int)av_rescale_rnd(
      swr_get_delay(codec->swr_ctx, codec->codec_ctx->sample_rate) + avframe->nb_samples,
      codec->codec_ctx->sample_rate, codec->codec_ctx->sample_rate, AV_ROUND_UP);

  uint8_t* out_data = (uint8_t*)malloc((size_t)max_out_samples * channels * out_sample_size);
  if (out_data == NULL) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  uint8_t* out_planes[1] = {out_data};
  int converted =
      swr_convert(codec->swr_ctx, out_planes, max_out_samples, (const uint8_t**)avframe->data, avframe->nb_samples);
  if (converted < 0) {
    free(out_data);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  memset(out_buffer, 0, sizeof(*out_buffer));
  out_buffer->format = codec->out_sample_format;
  out_buffer->sample_rate = (uint32_t)codec->codec_ctx->sample_rate;
  out_buffer->channels = (uint32_t)channels;
  out_buffer->frame_count = (uint32_t)converted;
  out_buffer->data = out_data;
  out_buffer->timestamp_us = avframe->pts != AV_NOPTS_VALUE ? avframe->pts : CRTMEDIA_AUDIO_TIMESTAMP_NONE;
  out_buffer->release = release_audio_buffer;
  out_buffer->release_context = out_data;
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_codec_dequeue_output(
    crtmedia_codec* codec, crtmedia_frame* out_video_frame, crtmedia_audio_buffer* out_audio_buffer, int* out_eof) {
  if (codec == NULL || out_eof == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  *out_eof = 0;
  if (codec->eof_drained) {
    *out_eof = 1;
    return CRTMEDIA_OK;
  }

  int ret = avcodec_receive_frame(codec->codec_ctx, codec->decode_frame);
  if (ret == AVERROR(EAGAIN)) {
    return CRTMEDIA_WOULD_BLOCK;
  }
  if (ret == AVERROR_EOF) {
    codec->eof_drained = 1;
    *out_eof = 1;
    return CRTMEDIA_OK;
  }
  if (ret < 0) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  if (codec->is_video) {
    if (out_video_frame != NULL) {
      AVFrame* owned = av_frame_alloc();
      av_frame_ref(owned, codec->decode_frame);
      fill_video_frame(owned, out_video_frame);
    }
  } else {
    if (out_audio_buffer != NULL) {
      if (fill_audio_buffer(codec, codec->decode_frame, out_audio_buffer) != CRTMEDIA_OK) {
        av_frame_unref(codec->decode_frame);
        return CRTMEDIA_ERROR_UNSUPPORTED;
      }
    }
  }
  av_frame_unref(codec->decode_frame);
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_codec_flush(crtmedia_codec* codec) {
  if (codec == NULL) {
    return CRTMEDIA_OK;
  }
  avcodec_flush_buffers(codec->codec_ctx);
  codec->eof_signaled = 0;
  codec->eof_drained = 0;
  return CRTMEDIA_OK;
}
