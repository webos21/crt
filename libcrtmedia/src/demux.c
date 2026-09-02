/* crtmedia/demux.h -- the original combined demux+decode convenience
 * API, now a thin wrapper composing crtmedia_extractor (crtmedia/
 * extractor.h) + one crtmedia_codec (crtmedia/codec.h) per decodable
 * track, rather than its own independent FFmpeg integration (TODO.md's
 * "Rebuild crtmedia_demuxer_* over the new core" step, docs/libcrtmedia_
 * api_policy.md's own Decision). No FFmpeg type appears here directly
 * anymore -- not because this file suddenly avoids FFmpeg (it never did
 * so directly even before this rebuild's own predecessor), but because
 * every real FFmpeg call now happens one layer down, inside extractor.c/
 * codec.c.
 *
 * Every stream whose track format's own MIME (crtmedia_extractor_track_
 * format()) starts with "video/" or "audio/" is auto-selected and given
 * a real crtmedia_codec at crtmedia_demuxer_open() time, matching this
 * header's own documented "no separate select-stream step" contract --
 * unlike a caller building directly on crtmedia_extractor/crtmedia_codec,
 * which decides per-track selection itself. A track whose MIME doesn't
 * match either prefix (crtmedia_extractor's own mime_for_codec_id()
 * fallback for a codec outside ffmpeg.json's own narrow --enable-decoder=
 * list returns a bare, unprefixed codec name -- see that file's own
 * comment) is real, reported CRTMEDIA_STREAM_UNKNOWN data, matching this
 * header's own documented behavior for an undecodable stream. */

#include "crtmedia/demux.h"

#include "crtmedia/codec.h"
#include "crtmedia/extractor.h"

#include <stdlib.h>
#include <string.h>

typedef struct crtmedia_demux_stream {
  crtmedia_stream_type type;
  crtmedia_codec* codec; /* NULL for a CRTMEDIA_STREAM_UNKNOWN track */
} crtmedia_demux_stream;

struct crtmedia_demuxer {
  crtmedia_extractor* extractor;
  crtmedia_demux_stream* streams;
  uint32_t stream_count;
  int extractor_eof;
  /* At most one raw sample crtmedia_codec_queue_input() couldn't accept
   * yet (CRTMEDIA_WOULD_BLOCK, a real, if rare for this project's own
   * narrow/low-reordering codec set, backpressure signal -- see codec.h's
   * own doc comment) -- kept alive across crtmedia_demuxer_read() calls
   * rather than discarded, so no real encoded data is ever silently
   * lost waiting for room to free up. */
  int pending_sample_valid;
  crtmedia_sample pending_sample;
};

static crtmedia_stream_type stream_type_from_mime(const char* mime) {
  if (mime == NULL) {
    return CRTMEDIA_STREAM_UNKNOWN;
  }
  if (strncmp(mime, "video/", 6) == 0) {
    return CRTMEDIA_STREAM_VIDEO;
  }
  if (strncmp(mime, "audio/", 6) == 0) {
    return CRTMEDIA_STREAM_AUDIO;
  }
  return CRTMEDIA_STREAM_UNKNOWN;
}

crtmedia_result crtmedia_demuxer_open(const char* path, crtmedia_demuxer** out_demuxer) {
  if (out_demuxer == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  *out_demuxer = NULL;

  crtmedia_extractor* extractor = NULL;
  crtmedia_result r = crtmedia_extractor_create(path, &extractor);
  if (r != CRTMEDIA_OK) {
    return r;
  }

  crtmedia_demuxer* demuxer = (crtmedia_demuxer*)calloc(1, sizeof(crtmedia_demuxer));
  if (demuxer == NULL) {
    crtmedia_extractor_release(extractor);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  demuxer->extractor = extractor;
  demuxer->stream_count = crtmedia_extractor_track_count(extractor);
  demuxer->streams = (crtmedia_demux_stream*)calloc(demuxer->stream_count, sizeof(crtmedia_demux_stream));
  if (demuxer->stream_count > 0 && demuxer->streams == NULL) {
    crtmedia_demuxer_close(demuxer);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  for (uint32_t i = 0; i < demuxer->stream_count; ++i) {
    crtmedia_format* format = NULL;
    if (crtmedia_extractor_track_format(extractor, i, &format) != CRTMEDIA_OK) {
      continue; /* leaves this stream CRTMEDIA_STREAM_UNKNOWN, real but unusual */
    }
    const char* mime = NULL;
    crtmedia_format_get_string(format, CRTMEDIA_FORMAT_KEY_MIME, &mime);
    crtmedia_stream_type type = stream_type_from_mime(mime);
    if (type != CRTMEDIA_STREAM_UNKNOWN) {
      crtmedia_codec* codec = NULL;
      if (crtmedia_codec_create_decoder(format, &codec) == CRTMEDIA_OK) {
        demuxer->streams[i].type = type;
        demuxer->streams[i].codec = codec;
        crtmedia_extractor_select_track(extractor, i);
      }
      /* else: a real MIME with a "video/"/"audio/" prefix that still
       * failed to produce a working decoder (should not happen in
       * practice -- crtmedia_extractor's own mime_for_codec_id() only
       * ever produces those two prefixes for the exact codec IDs
       * crtmedia_codec's own codec_id_for_mime() maps back successfully,
       * see both files' own comments) -- leaves this stream CRTMEDIA_
       * STREAM_UNKNOWN rather than failing crtmedia_demuxer_open()
       * outright, matching this header's own "some streams may simply
       * not be decodable" contract. */
    }
    crtmedia_format_release(format);
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
  memset(out_info, 0, sizeof(*out_info));
  out_info->type = demuxer->streams[stream_index].type;

  crtmedia_format* format = NULL;
  if (crtmedia_extractor_track_format(demuxer->extractor, stream_index, &format) != CRTMEDIA_OK) {
    return CRTMEDIA_OK; /* type is still real; dimensions stay 0 */
  }
  if (out_info->type == CRTMEDIA_STREAM_VIDEO) {
    int32_t width = 0;
    int32_t height = 0;
    crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_WIDTH, &width);
    crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_HEIGHT, &height);
    out_info->width = (uint32_t)width;
    out_info->height = (uint32_t)height;
  } else if (out_info->type == CRTMEDIA_STREAM_AUDIO) {
    int32_t sample_rate = 0;
    int32_t channels = 0;
    crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_SAMPLE_RATE, &sample_rate);
    crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_CHANNEL_COUNT, &channels);
    out_info->sample_rate = (uint32_t)sample_rate;
    out_info->channels = (uint32_t)channels;
  }
  crtmedia_format_release(format);
  return CRTMEDIA_OK;
}

static crtmedia_result queue_pending_sample(crtmedia_demuxer* demuxer) {
  crtmedia_codec* target = demuxer->streams[demuxer->pending_sample.track_index].codec;
  crtmedia_result qr = crtmedia_codec_queue_input(
      target, demuxer->pending_sample.data, demuxer->pending_sample.size, demuxer->pending_sample.pts_us,
      CRTMEDIA_CODEC_BUFFER_FLAG_NONE);
  if (qr == CRTMEDIA_WOULD_BLOCK) {
    return CRTMEDIA_WOULD_BLOCK;
  }
  crtmedia_sample_release(&demuxer->pending_sample);
  demuxer->pending_sample_valid = 0;
  return qr;
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
    /* Drain path: check every decodable stream, in index order, for a
     * real decoded output already sitting in its own codec -- matches
     * the original combined implementation's own exact "check every
     * stream's decoder before reading another packet" priority. A
     * per-stream CRTMEDIA_WOULD_BLOCK (nothing ready yet) or a per-
     * stream *out_eof (this codec has fully drained after its own end-
     * of-stream) are both treated the same way here -- move on to the
     * next stream -- matching the original implementation's own real
     * behavior of treating a per-stream AVERROR_EOF exactly like EAGAIN
     * (nothing to return from that stream right now); the *real*
     * termination signal below is extractor_eof plus every stream
     * having nothing left, not a per-stream flag. */
    for (uint32_t i = 0; i < demuxer->stream_count; ++i) {
      crtmedia_demux_stream* stream = &demuxer->streams[i];
      if (stream->codec == NULL) {
        continue;
      }
      crtmedia_frame video_frame;
      crtmedia_audio_buffer audio_buffer;
      int codec_eof = 0;
      crtmedia_result dr = crtmedia_codec_dequeue_output(stream->codec, &video_frame, &audio_buffer, &codec_eof);
      if (dr == CRTMEDIA_WOULD_BLOCK) {
        continue;
      }
      if (dr != CRTMEDIA_OK) {
        return CRTMEDIA_ERROR_UNSUPPORTED;
      }
      if (codec_eof) {
        continue;
      }
      *out_stream_index = i;
      if (stream->type == CRTMEDIA_STREAM_VIDEO) {
        *out_status = CRTMEDIA_READ_VIDEO_FRAME;
        if (out_video_frame != NULL) {
          *out_video_frame = video_frame;
        } else {
          crtmedia_frame_release(&video_frame);
        }
      } else {
        *out_status = CRTMEDIA_READ_AUDIO_BUFFER;
        if (out_audio_buffer != NULL) {
          *out_audio_buffer = audio_buffer;
        } else {
          crtmedia_audio_buffer_release(&audio_buffer);
        }
      }
      return CRTMEDIA_OK;
    }

    /* Nothing ready from any stream -- feed more input. A pending
     * sample from an earlier CRTMEDIA_WOULD_BLOCK takes priority (real
     * encoded data already read out of the container; must not be
     * dropped) over reading a brand new one. */
    if (demuxer->pending_sample_valid) {
      crtmedia_result qr = queue_pending_sample(demuxer);
      if (qr == CRTMEDIA_WOULD_BLOCK) {
        /* Just drained every stream above and still can't make room --
         * a real, if practically unreachable for this project's own
         * bounded codec set/fixtures, stuck condition. Surface it as a
         * real error rather than spinning forever. */
        return CRTMEDIA_ERROR_UNSUPPORTED;
      }
      if (qr != CRTMEDIA_OK) {
        return CRTMEDIA_ERROR_UNSUPPORTED;
      }
      continue;
    }

    if (!demuxer->extractor_eof) {
      crtmedia_sample sample;
      int sample_eof = 0;
      crtmedia_result r = crtmedia_extractor_read_sample(demuxer->extractor, &sample, &sample_eof);
      if (r != CRTMEDIA_OK) {
        return CRTMEDIA_ERROR_UNSUPPORTED;
      }
      if (sample_eof) {
        demuxer->extractor_eof = 1;
        for (uint32_t i = 0; i < demuxer->stream_count; ++i) {
          if (demuxer->streams[i].codec != NULL) {
            crtmedia_codec_queue_input(
                demuxer->streams[i].codec, NULL, 0, CRTMEDIA_FRAME_TIMESTAMP_NONE,
                CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM);
          }
        }
        continue;
      }
      if (demuxer->streams[sample.track_index].codec == NULL) {
        crtmedia_sample_release(&sample); /* an undecodable track's own raw packet -- discard */
        continue;
      }
      crtmedia_result qr = crtmedia_codec_queue_input(
          demuxer->streams[sample.track_index].codec, sample.data, sample.size, sample.pts_us,
          CRTMEDIA_CODEC_BUFFER_FLAG_NONE);
      if (qr == CRTMEDIA_WOULD_BLOCK) {
        demuxer->pending_sample = sample; /* ownership moves here -- released once actually queued */
        demuxer->pending_sample_valid = 1;
        continue;
      }
      crtmedia_sample_release(&sample);
      if (qr != CRTMEDIA_OK) {
        return CRTMEDIA_ERROR_UNSUPPORTED;
      }
      continue;
    }

    /* extractor already at EOF, no pending sample, and the drain pass
     * above found nothing left in any stream's own codec -- genuinely
     * done. */
    *out_status = CRTMEDIA_READ_EOF;
    return CRTMEDIA_OK;
  }
}

void crtmedia_demuxer_close(crtmedia_demuxer* demuxer) {
  if (demuxer == NULL) {
    return;
  }
  if (demuxer->pending_sample_valid) {
    crtmedia_sample_release(&demuxer->pending_sample);
  }
  if (demuxer->streams != NULL) {
    for (uint32_t i = 0; i < demuxer->stream_count; ++i) {
      crtmedia_codec_release(demuxer->streams[i].codec);
    }
    free(demuxer->streams);
  }
  crtmedia_extractor_release(demuxer->extractor);
  free(demuxer);
}
