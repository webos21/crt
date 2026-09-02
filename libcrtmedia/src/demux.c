/* FFmpeg-backed demux/software decode (crtmedia/demux.h). See that header's
 * own top comment and HISTORY.md's matching dated entry for the full scope
 * and design reasoning. Every real libavformat/libavcodec type stays
 * confined to this file -- crtmedia/demux.h never includes an FFmpeg
 * header or names an FFmpeg type. */

#include "crtmedia/demux.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libswresample/swresample.h>

#include <stdlib.h>
#include <string.h>

/* One demuxed+decodable stream this project actually tracks -- a real
 * FFmpeg container can have more streams (subtitles, data, an unsupported
 * codec) than this pass decodes; only streams whose codec is in this
 * pass's own enable list (see ffmpeg.json's own configure_args) get a
 * real AVCodecContext here. */
typedef struct crtmedia_demux_stream {
  crtmedia_stream_type type;
  int avstream_index;
  AVCodecContext* codec_ctx;
  /* Audio only: converts whatever internal format the decoder actually
   * produces (often planar -- AV_SAMPLE_FMT_FLTP for AAC, AV_SAMPLE_FMT_
   * S16P for some PCM paths) into this contract's own interleaved S16/FLT
   * output (crtmedia/audio.h's own documented format list). */
  SwrContext* swr_ctx;
  crtmedia_sample_format out_sample_format;
} crtmedia_demux_stream;

struct crtmedia_demuxer {
  AVFormatContext* fmt_ctx;
  crtmedia_demux_stream* streams;
  uint32_t stream_count;
  AVPacket* packet;
  AVFrame* decode_frame;
  int eof_flushed;
};

static crtmedia_stream_type stream_type_from_codec(const AVCodecParameters* params) {
  if (params->codec_type == AVMEDIA_TYPE_VIDEO) {
    return CRTMEDIA_STREAM_VIDEO;
  }
  if (params->codec_type == AVMEDIA_TYPE_AUDIO) {
    return CRTMEDIA_STREAM_AUDIO;
  }
  return CRTMEDIA_STREAM_UNKNOWN;
}

crtmedia_result crtmedia_demuxer_open(const char* path, crtmedia_demuxer** out_demuxer) {
  if (path == NULL || out_demuxer == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  *out_demuxer = NULL;

  AVFormatContext* fmt_ctx = NULL;
  if (avformat_open_input(&fmt_ctx, path, NULL, NULL) < 0) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
    avformat_close_input(&fmt_ctx);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  crtmedia_demuxer* demuxer = (crtmedia_demuxer*)calloc(1, sizeof(crtmedia_demuxer));
  if (demuxer == NULL) {
    avformat_close_input(&fmt_ctx);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  demuxer->fmt_ctx = fmt_ctx;
  demuxer->stream_count = fmt_ctx->nb_streams;
  demuxer->streams = (crtmedia_demux_stream*)calloc(demuxer->stream_count, sizeof(crtmedia_demux_stream));
  demuxer->packet = av_packet_alloc();
  demuxer->decode_frame = av_frame_alloc();
  if (demuxer->streams == NULL || demuxer->packet == NULL || demuxer->decode_frame == NULL) {
    crtmedia_demuxer_close(demuxer);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  for (uint32_t i = 0; i < demuxer->stream_count; ++i) {
    AVStream* avstream = fmt_ctx->streams[i];
    crtmedia_demux_stream* stream = &demuxer->streams[i];
    stream->avstream_index = (int)i;
    stream->type = stream_type_from_codec(avstream->codecpar);
    if (stream->type == CRTMEDIA_STREAM_UNKNOWN) {
      continue;
    }

    const AVCodec* codec = avcodec_find_decoder(avstream->codecpar->codec_id);
    if (codec == NULL) {
      stream->type = CRTMEDIA_STREAM_UNKNOWN;
      continue;
    }
    stream->codec_ctx = avcodec_alloc_context3(codec);
    if (stream->codec_ctx != NULL && stream->type == CRTMEDIA_STREAM_VIDEO) {
      /* Explicit, deliberate multi-threaded decode for video -- not just
       * relying on avcodec_alloc_context3()'s own ambient default (0
       * "auto" thread_count with both FF_THREAD_FRAME/FF_THREAD_SLICE
       * already implicitly allowed, which in practice often does not
       * actually spin up more than one thread for a short/simple/single-
       * slice-per-frame stream like this pass's own small H.264 test
       * fixture). Requesting a real thread_count > 1 here, and letting
       * TODO.md's "verify threaded H.264 decode" step check the decoded
       * output is still correct, is a genuine, real exercise of this
       * project's own Bionic-style pthread implementation cooperating
       * with FFmpeg's internal decode-thread pool under this project's
       * own sysroot -- exactly the kind of real third-party-consumer PAL
       * pressure-test this whole porting effort exists for, not a
       * cosmetic setting. */
      stream->codec_ctx->thread_count = 2;
      stream->codec_ctx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
    }
    if (stream->codec_ctx == NULL ||
        avcodec_parameters_to_context(stream->codec_ctx, avstream->codecpar) < 0 ||
        avcodec_open2(stream->codec_ctx, codec, NULL) < 0) {
      if (stream->codec_ctx != NULL) {
        avcodec_free_context(&stream->codec_ctx);
      }
      stream->type = CRTMEDIA_STREAM_UNKNOWN;
      continue;
    }

    if (stream->type == CRTMEDIA_STREAM_AUDIO) {
      /* This contract only ever produces S16 or FLT (crtmedia/audio.h's
       * own comment) -- prefer S16 unless the decoder's own native
       * format is already float, avoiding a lossy float->int16->float
       * round trip for a codec (like AAC) that decodes to float
       * natively. */
      stream->out_sample_format = (stream->codec_ctx->sample_fmt == AV_SAMPLE_FMT_FLT ||
                                    stream->codec_ctx->sample_fmt == AV_SAMPLE_FMT_FLTP)
                                       ? CRTMEDIA_SAMPLE_FORMAT_FLT
                                       : CRTMEDIA_SAMPLE_FORMAT_S16;
      enum AVSampleFormat out_fmt =
          stream->out_sample_format == CRTMEDIA_SAMPLE_FORMAT_FLT ? AV_SAMPLE_FMT_FLT : AV_SAMPLE_FMT_S16;

      AVChannelLayout out_layout;
      av_channel_layout_default(&out_layout, stream->codec_ctx->ch_layout.nb_channels);
      swr_alloc_set_opts2(
          &stream->swr_ctx, &out_layout, out_fmt, stream->codec_ctx->sample_rate,
          &stream->codec_ctx->ch_layout, stream->codec_ctx->sample_fmt, stream->codec_ctx->sample_rate, 0,
          NULL);
      av_channel_layout_uninit(&out_layout);
      if (stream->swr_ctx == NULL || swr_init(stream->swr_ctx) < 0) {
        avcodec_free_context(&stream->codec_ctx);
        stream->type = CRTMEDIA_STREAM_UNKNOWN;
        continue;
      }
    }
  }

  *out_demuxer = demuxer;
  return CRTMEDIA_OK;
}

uint32_t crtmedia_demuxer_stream_count(const crtmedia_demuxer* demuxer) {
  return demuxer != NULL ? demuxer->stream_count : 0;
}

crtmedia_result crtmedia_demuxer_stream_info(
    const crtmedia_demuxer* demuxer, uint32_t stream_index, crtmedia_stream_info* out_info) {
  if (demuxer == NULL || out_info == NULL || stream_index >= demuxer->stream_count) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  const crtmedia_demux_stream* stream = &demuxer->streams[stream_index];
  const AVCodecParameters* params = demuxer->fmt_ctx->streams[stream_index]->codecpar;

  memset(out_info, 0, sizeof(*out_info));
  out_info->type = stream->type;
  if (stream->type == CRTMEDIA_STREAM_VIDEO) {
    out_info->width = (uint32_t)params->width;
    out_info->height = (uint32_t)params->height;
  } else if (stream->type == CRTMEDIA_STREAM_AUDIO) {
    out_info->sample_rate = (uint32_t)params->sample_rate;
    out_info->channels = (uint32_t)params->ch_layout.nb_channels;
  }
  return CRTMEDIA_OK;
}

/* Ownership context for a decoded video AVFrame handed out through
 * crtmedia_frame -- the AVFrame itself (received fresh from avcodec_
 * receive_frame(), so it owns real, refcounted backing buffers) is kept
 * alive exactly as long as the crtmedia_frame wrapping it, freed by
 * release_video_frame() below. */
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
  out_frame->timestamp_us =
      avframe->pts != AV_NOPTS_VALUE ? avframe->pts : CRTMEDIA_FRAME_TIMESTAMP_NONE;
  out_frame->plane_count = 3;
  uint32_t chroma_width = (out_frame->width + 1u) / 2u;
  uint32_t chroma_height = (out_frame->height + 1u) / 2u;
  out_frame->planes[0] = (crtmedia_frame_plane){avframe->data[0], (uint32_t)avframe->linesize[0],
                                                 out_frame->width, out_frame->height};
  out_frame->planes[1] =
      (crtmedia_frame_plane){avframe->data[1], (uint32_t)avframe->linesize[1], chroma_width, chroma_height};
  out_frame->planes[2] =
      (crtmedia_frame_plane){avframe->data[2], (uint32_t)avframe->linesize[2], chroma_width, chroma_height};
  out_frame->release = release_video_frame;
  out_frame->release_context = avframe;
}

/* Ownership context for a decoded+resampled audio buffer -- unlike the
 * video path, this is a fresh, plain malloc'd buffer (swr_convert()'s own
 * output), not something owned by any AVFrame/AVBufferRef, so release
 * is a plain free(). */
static void release_audio_buffer(crtmedia_audio_buffer* buffer, void* release_context) {
  (void)buffer;
  free(release_context);
}

static crtmedia_result fill_audio_buffer(
    crtmedia_demux_stream* stream, AVFrame* avframe, crtmedia_audio_buffer* out_buffer) {
  int out_sample_size = stream->out_sample_format == CRTMEDIA_SAMPLE_FORMAT_FLT ? 4 : 2;
  int channels = stream->codec_ctx->ch_layout.nb_channels;
  int max_out_samples = (int)av_rescale_rnd(
      swr_get_delay(stream->swr_ctx, stream->codec_ctx->sample_rate) + avframe->nb_samples,
      stream->codec_ctx->sample_rate, stream->codec_ctx->sample_rate, AV_ROUND_UP);

  uint8_t* out_data = (uint8_t*)malloc((size_t)max_out_samples * channels * out_sample_size);
  if (out_data == NULL) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  uint8_t* out_planes[1] = {out_data};
  int converted =
      swr_convert(stream->swr_ctx, out_planes, max_out_samples, (const uint8_t**)avframe->data, avframe->nb_samples);
  if (converted < 0) {
    free(out_data);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  memset(out_buffer, 0, sizeof(*out_buffer));
  out_buffer->format = stream->out_sample_format;
  out_buffer->sample_rate = (uint32_t)stream->codec_ctx->sample_rate;
  out_buffer->channels = (uint32_t)channels;
  out_buffer->frame_count = (uint32_t)converted;
  out_buffer->data = out_data;
  out_buffer->timestamp_us = avframe->pts != AV_NOPTS_VALUE ? avframe->pts : CRTMEDIA_AUDIO_TIMESTAMP_NONE;
  out_buffer->release = release_audio_buffer;
  out_buffer->release_context = out_data;
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_demuxer_read(
    crtmedia_demuxer* demuxer,
    crtmedia_read_status* out_status,
    uint32_t* out_stream_index,
    crtmedia_frame* out_video_frame,
    crtmedia_audio_buffer* out_audio_buffer) {
  if (demuxer == NULL || out_status == NULL || out_stream_index == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }

  for (;;) {
    /* Drain path: pull every already-buffered decoded frame out of the
     * current stream's decoder before reading the next packet -- a
     * decoder may hold more than one frame ready after a single
     * avcodec_send_packet() call. */
    for (uint32_t i = 0; i < demuxer->stream_count; ++i) {
      crtmedia_demux_stream* stream = &demuxer->streams[i];
      if (stream->codec_ctx == NULL) {
        continue;
      }
      int ret = avcodec_receive_frame(stream->codec_ctx, demuxer->decode_frame);
      if (ret == 0) {
        *out_stream_index = i;
        if (stream->type == CRTMEDIA_STREAM_VIDEO) {
          *out_status = CRTMEDIA_READ_VIDEO_FRAME;
          if (out_video_frame != NULL) {
            /* fill_video_frame() takes ownership of a fresh AVFrame it
             * clones from demuxer->decode_frame (the shared, reused
             * decode scratch frame) -- av_frame_alloc()+av_frame_
             * ref(), not a raw pointer into the shared frame, since
             * that frame gets reused/overwritten on the next receive
             * call regardless of which stream it came from. */
            AVFrame* owned = av_frame_alloc();
            av_frame_ref(owned, demuxer->decode_frame);
            fill_video_frame(owned, out_video_frame);
          }
        } else {
          *out_status = CRTMEDIA_READ_AUDIO_BUFFER;
          if (out_audio_buffer != NULL) {
            if (fill_audio_buffer(stream, demuxer->decode_frame, out_audio_buffer) != CRTMEDIA_OK) {
              av_frame_unref(demuxer->decode_frame);
              return CRTMEDIA_ERROR_UNSUPPORTED;
            }
          }
        }
        av_frame_unref(demuxer->decode_frame);
        return CRTMEDIA_OK;
      }
      if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
        return CRTMEDIA_ERROR_UNSUPPORTED;
      }
    }

    /* No decoder had a frame ready -- read and dispatch the next real
     * packet from the container. */
    int read_ret = av_read_frame(demuxer->fmt_ctx, demuxer->packet);
    if (read_ret < 0) {
      /* True end of file: flush every decoder (send a NULL packet) so
       * the drain loop above can pull out whatever each decoder was
       * still holding internally, then report EOF once nothing more
       * comes out. */
      if (!demuxer->eof_flushed) {
        for (uint32_t i = 0; i < demuxer->stream_count; ++i) {
          if (demuxer->streams[i].codec_ctx != NULL) {
            avcodec_send_packet(demuxer->streams[i].codec_ctx, NULL);
          }
        }
        demuxer->eof_flushed = 1;
        continue;
      }
      *out_status = CRTMEDIA_READ_EOF;
      return CRTMEDIA_OK;
    }

    if ((uint32_t)demuxer->packet->stream_index < demuxer->stream_count) {
      crtmedia_demux_stream* stream = &demuxer->streams[demuxer->packet->stream_index];
      if (stream->codec_ctx != NULL) {
        int send_ret = avcodec_send_packet(stream->codec_ctx, demuxer->packet);
        if (send_ret < 0 && send_ret != AVERROR(EAGAIN)) {
          av_packet_unref(demuxer->packet);
          return CRTMEDIA_ERROR_UNSUPPORTED;
        }
      }
    }
    av_packet_unref(demuxer->packet);
  }
}

void crtmedia_demuxer_close(crtmedia_demuxer* demuxer) {
  if (demuxer == NULL) {
    return;
  }
  if (demuxer->streams != NULL) {
    for (uint32_t i = 0; i < demuxer->stream_count; ++i) {
      crtmedia_demux_stream* stream = &demuxer->streams[i];
      if (stream->swr_ctx != NULL) {
        swr_free(&stream->swr_ctx);
      }
      if (stream->codec_ctx != NULL) {
        avcodec_free_context(&stream->codec_ctx);
      }
    }
    free(demuxer->streams);
  }
  if (demuxer->packet != NULL) {
    av_packet_free(&demuxer->packet);
  }
  if (demuxer->decode_frame != NULL) {
    av_frame_free(&demuxer->decode_frame);
  }
  if (demuxer->fmt_ctx != NULL) {
    avformat_close_input(&demuxer->fmt_ctx);
  }
  free(demuxer);
}
