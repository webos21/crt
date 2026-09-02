/* crtmedia/extractor.h -- demux-only (no AVCodecContext at all, see that
 * header's own top comment). Every real libavformat/libavcodec type stays
 * confined to this file, matching src/demux.c's own established
 * discipline -- crtmedia/extractor.h never includes an FFmpeg header or
 * names an FFmpeg type.
 *
 * Seek support is intentionally narrow this first pass: only the default
 * "previous sync sample" mode (AMediaExtractor's own SEEK_PREVIOUS_SYNC),
 * matching this project's own established "narrow now, expand later"
 * pattern -- SEEK_NEXT_SYNC/SEEK_CLOSEST_SYNC are real, useful modes real
 * players eventually want, but no real consumer needs them yet. */

#include "crtmedia/extractor.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include <stdlib.h>
#include <string.h>

struct crtmedia_extractor {
  AVFormatContext* fmt_ctx;
  AVPacket* packet;
  /* One bool per real stream -- crtmedia_extractor_select_track()/
   * _unselect_track() flip these; read_sample()'s own av_read_frame()
   * loop discards any packet whose stream isn't marked here, matching
   * AMediaExtractor's own "only selected tracks produce samples"
   * contract. */
  uint8_t* selected;
};

static const char* mime_for_codec_id(enum AVCodecID codec_id) {
  switch (codec_id) {
    case AV_CODEC_ID_H264:
      return "video/avc";
    case AV_CODEC_ID_AAC:
      return "audio/mp4a-latm";
    case AV_CODEC_ID_MP3:
      return "audio/mpeg";
    case AV_CODEC_ID_PCM_S16LE:
      return "audio/raw";
    default:
      /* A real, if informal, label for a codec this pass's own narrow
       * decode set doesn't cover -- crtmedia_extractor itself is codec-
       * agnostic (this file's own top comment), so a track outside that
       * set is still real, reportable data, not an error. Not a real
       * IANA/RFC 6381-style MIME string for anything unlisted above --
       * good enough for a caller deciding whether it can decode this
       * track at all, which is the only real use this pass has for it. */
      return avcodec_get_name(codec_id);
  }
}

crtmedia_result crtmedia_extractor_create(const char* path, crtmedia_extractor** out_extractor) {
  if (path == NULL || out_extractor == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  *out_extractor = NULL;

  AVFormatContext* fmt_ctx = NULL;
  if (avformat_open_input(&fmt_ctx, path, NULL, NULL) < 0) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
    avformat_close_input(&fmt_ctx);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  crtmedia_extractor* extractor = (crtmedia_extractor*)calloc(1, sizeof(crtmedia_extractor));
  if (extractor == NULL) {
    avformat_close_input(&fmt_ctx);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  extractor->fmt_ctx = fmt_ctx;
  extractor->packet = av_packet_alloc();
  extractor->selected = (uint8_t*)calloc(fmt_ctx->nb_streams, sizeof(uint8_t));
  if (extractor->packet == NULL || (fmt_ctx->nb_streams > 0 && extractor->selected == NULL)) {
    crtmedia_extractor_release(extractor);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  *out_extractor = extractor;
  return CRTMEDIA_OK;
}

void crtmedia_extractor_release(crtmedia_extractor* extractor) {
  if (extractor == NULL) {
    return;
  }
  free(extractor->selected);
  if (extractor->packet != NULL) {
    av_packet_free(&extractor->packet);
  }
  if (extractor->fmt_ctx != NULL) {
    avformat_close_input(&extractor->fmt_ctx);
  }
  free(extractor);
}

uint32_t crtmedia_extractor_track_count(const crtmedia_extractor* extractor) {
  return extractor != NULL ? extractor->fmt_ctx->nb_streams : 0;
}

crtmedia_result crtmedia_extractor_track_format(
    const crtmedia_extractor* extractor, uint32_t track_index, crtmedia_format** out_format) {
  if (extractor == NULL || out_format == NULL || track_index >= extractor->fmt_ctx->nb_streams) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  const AVStream* stream = extractor->fmt_ctx->streams[track_index];
  const AVCodecParameters* params = stream->codecpar;

  crtmedia_format* format = NULL;
  crtmedia_result r = crtmedia_format_create(&format);
  if (r != CRTMEDIA_OK) {
    return r;
  }
  crtmedia_format_set_string(format, CRTMEDIA_FORMAT_KEY_MIME, mime_for_codec_id(params->codec_id));
  if (params->codec_type == AVMEDIA_TYPE_VIDEO) {
    crtmedia_format_set_int32(format, CRTMEDIA_FORMAT_KEY_WIDTH, params->width);
    crtmedia_format_set_int32(format, CRTMEDIA_FORMAT_KEY_HEIGHT, params->height);
  } else if (params->codec_type == AVMEDIA_TYPE_AUDIO) {
    crtmedia_format_set_int32(format, CRTMEDIA_FORMAT_KEY_SAMPLE_RATE, params->sample_rate);
    crtmedia_format_set_int32(format, CRTMEDIA_FORMAT_KEY_CHANNEL_COUNT, params->ch_layout.nb_channels);
  }
  if (stream->duration != AV_NOPTS_VALUE) {
    int64_t duration_us = av_rescale_q(stream->duration, stream->time_base, AV_TIME_BASE_Q);
    crtmedia_format_set_int64(format, CRTMEDIA_FORMAT_KEY_DURATION_US, duration_us);
  }
  if (params->extradata != NULL && params->extradata_size > 0) {
    /* Real codec-config data (H.264's SPS/PPS in avcC form, AAC's
     * AudioSpecificConfig, ...) a decoder needs before it can decode any
     * real sample -- crtmedia_codec_create_decoder() (crtmedia/codec.h)
     * reads this back to configure AVCodecContext.extradata the same
     * way. Silently dropped (not a real error) if it exceeds crtmedia_
     * format's own fixed buffer-value limit (format.c) -- no codec this
     * pass's own narrow decode set targets has ever come close to that
     * limit in practice; a future codec that genuinely needs more would
     * need that limit raised, not this call failing quietly. */
    crtmedia_format_set_buffer(format, CRTMEDIA_FORMAT_KEY_CSD, params->extradata, (size_t)params->extradata_size);
  }

  *out_format = format;
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_extractor_select_track(crtmedia_extractor* extractor, uint32_t track_index) {
  if (extractor == NULL || track_index >= extractor->fmt_ctx->nb_streams) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  extractor->selected[track_index] = 1;
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_extractor_unselect_track(crtmedia_extractor* extractor, uint32_t track_index) {
  if (extractor == NULL || track_index >= extractor->fmt_ctx->nb_streams) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  extractor->selected[track_index] = 0;
  return CRTMEDIA_OK;
}

static void release_sample(crtmedia_sample* sample, void* release_context) {
  (void)sample;
  free(release_context);
}

crtmedia_result crtmedia_extractor_read_sample(
    crtmedia_extractor* extractor, crtmedia_sample* out_sample, int* out_eof) {
  if (extractor == NULL || out_sample == NULL || out_eof == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  *out_eof = 0;

  for (;;) {
    int read_ret = av_read_frame(extractor->fmt_ctx, extractor->packet);
    if (read_ret < 0) {
      *out_eof = 1;
      return CRTMEDIA_OK;
    }
    uint32_t stream_index = (uint32_t)extractor->packet->stream_index;
    if (stream_index >= extractor->fmt_ctx->nb_streams || !extractor->selected[stream_index]) {
      av_packet_unref(extractor->packet);
      continue;
    }

    /* A real, owned copy -- extractor->packet itself gets reused/
     * unref'd by the next av_read_frame() call regardless of which
     * stream it came from, matching demux.c's own decode_frame reuse
     * discipline for the same reason. */
    void* data = malloc((size_t)extractor->packet->size > 0 ? (size_t)extractor->packet->size : 1);
    if (data == NULL) {
      av_packet_unref(extractor->packet);
      return CRTMEDIA_ERROR_UNSUPPORTED;
    }
    memcpy(data, extractor->packet->data, (size_t)extractor->packet->size);

    const AVStream* stream = extractor->fmt_ctx->streams[stream_index];
    memset(out_sample, 0, sizeof(*out_sample));
    out_sample->data = data;
    out_sample->size = (uint32_t)extractor->packet->size;
    out_sample->track_index = stream_index;
    out_sample->pts_us = extractor->packet->pts != AV_NOPTS_VALUE
                              ? av_rescale_q(extractor->packet->pts, stream->time_base, AV_TIME_BASE_Q)
                              : CRTMEDIA_FRAME_TIMESTAMP_NONE;
    out_sample->flags =
        (extractor->packet->flags & AV_PKT_FLAG_KEY) != 0 ? CRTMEDIA_SAMPLE_FLAG_KEY_FRAME : CRTMEDIA_SAMPLE_FLAG_NONE;
    out_sample->release = release_sample;
    out_sample->release_context = data;

    av_packet_unref(extractor->packet);
    return CRTMEDIA_OK;
  }
}

crtmedia_result crtmedia_extractor_seek_to(crtmedia_extractor* extractor, int64_t seek_pos_us) {
  if (extractor == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  if (av_seek_frame(extractor->fmt_ctx, -1, seek_pos_us, AVSEEK_FLAG_BACKWARD) < 0) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  return CRTMEDIA_OK;
}

void crtmedia_sample_release(crtmedia_sample* sample) {
  if (sample == NULL || sample->release == NULL) {
    return;
  }
  sample->release(sample, sample->release_context);
  memset(sample, 0, sizeof(*sample));
}
