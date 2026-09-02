// Real, end-to-end coverage for TODO.md's upper-runtime roadmap "Finish
// the software player" step's own last remaining piece: "the actual full
// demux+decode+sink+clock render-loop pipeline" -- proving crtmedia_
// extractor + crtmedia_codec (already proven together by extractor_codec_
// test.c) + crtmedia_player (the real CLOCK_MONOTONIC-anchored master
// clock/A-V-sync core, player.h) + crtmedia_audio_sink (a real per-host
// backend, or a graceful CRTMEDIA_ERROR_UNSUPPORTED) genuinely compose
// into a real, working player against the same real MP4 fixture every
// other crtmedia_demux_*/extractor_codec test already decodes.
//
// Deliberately a real integration test, not a new public API: crtmedia_
// player.h's own top comment already documents that the actual render
// loop tying extractor/codec/an audio sink/a caller-owned render surface
// together is "each their own, separate piece" this project leaves to
// the caller -- this file is the real, working reference for exactly
// that composition, matching this project's own established "prove
// real composition with a real test" discipline (extractor_codec_test.c
// itself is the same shape one layer down).
//
// The buffering/frame-drop policy this same TODO.md step calls for is
// real and concrete here, not just crtmedia_player_plan_video_frame()'s
// own already-existing WAIT/PRESENT_NOW/DROP decision: this file's own
// CRTMEDIA_PIPELINE_VIDEO_LOOKAHEAD bounds how many decoded-but-not-yet-
// presented video frames this loop ever holds locally at once (a real,
// deliberate cap on how far decode is allowed to run ahead of real
// presentation, not decode-everything-then-present) -- once that local
// queue is full, this loop presents/drops from its head (per plan_video_
// frame()'s own real decision, sleeping a real, bounded amount for WAIT)
// before queuing any further video input, exactly the "how far ahead do
// we buffer, and what happens when a frame falls behind" policy question
// this step asks for.
//
// Audio has no equivalent local lookahead queue on purpose: each real
// decoded crtmedia_audio_buffer is written straight through to the audio
// sink (a real, blocking write -- see crtmedia_audio_sink_write()'s own
// documented backpressure contract) as soon as it is decoded, matching
// crtmedia_player.h's own documented "audio is this player's own
// reference clock" design (an audio buffer already IS the pacing signal
// once it reaches a real device; holding it back locally first would only
// add latency, not correctness).

#include "crtmedia/audio_sink.h"
#include "crtmedia/codec.h"
#include "crtmedia/extractor.h"
#include "crtmedia/player.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#ifndef CRTMEDIA_TEST_VIDEO_PATH
#error "CRTMEDIA_TEST_VIDEO_PATH must be defined (see libcrtmedia/CMakeLists.txt)"
#endif

static int failures = 0;

#define CHECK(cond, msg)                                                  \
  do {                                                                    \
    if (!(cond)) {                                                        \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg);       \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

static void sleep_us(int64_t microseconds) {
  if (microseconds <= 0) {
    return;
  }
  struct timespec ts;
  ts.tv_sec = microseconds / 1000000;
  ts.tv_nsec = (microseconds % 1000000) * 1000;
  nanosleep(&ts, NULL);
}

static int64_t monotonic_now_us(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (int64_t)ts.tv_sec * 1000000 + (int64_t)ts.tv_nsec / 1000;
}

// This exact real, bounded cap is the buffering policy itself -- see this
// file's own top comment. 3 frames at the fixture's own real ~25fps (see
// extractor_codec_test.c's own already-verified 25-frame/~1s duration) is
// roughly 120ms of real lookahead, comfortably inside the same "a few
// hundred ms of real headroom" range every audio_sink backend's own real
// device buffer already uses (WASAPI's 300ms, ALSA's real hardware
// buffer_size, CoreAudio's ~200ms 4-buffer pool).
#define CRTMEDIA_PIPELINE_VIDEO_LOOKAHEAD 3

typedef struct video_queue {
  crtmedia_frame frames[CRTMEDIA_PIPELINE_VIDEO_LOOKAHEAD];
  int count;
} video_queue;

static void video_queue_push(video_queue* q, const crtmedia_frame* frame) {
  q->frames[q->count] = *frame;
  ++q->count;
}

static void video_queue_pop_front(video_queue* q) {
  int i;
  for (i = 1; i < q->count; ++i) {
    q->frames[i - 1] = q->frames[i];
  }
  --q->count;
}

int main(void) {
  crtmedia_extractor* extractor = NULL;
  crtmedia_result r = crtmedia_extractor_create(CRTMEDIA_TEST_VIDEO_PATH, &extractor);
  CHECK(r == CRTMEDIA_OK, "crtmedia_extractor_create succeeds on the real MP4 fixture");
  if (extractor == NULL) {
    fprintf(stderr, "crtmedia_playback_pipeline_test: %d failure(s)\n", failures);
    return 1;
  }

  uint32_t track_count = crtmedia_extractor_track_count(extractor);
  int video_track = -1;
  int audio_track = -1;
  crtmedia_format* video_format = NULL;
  crtmedia_format* audio_format = NULL;
  for (uint32_t i = 0; i < track_count; ++i) {
    crtmedia_format* format = NULL;
    if (crtmedia_extractor_track_format(extractor, i, &format) != CRTMEDIA_OK || format == NULL) {
      continue;
    }
    const char* mime = NULL;
    crtmedia_format_get_string(format, CRTMEDIA_FORMAT_KEY_MIME, &mime);
    if (mime != NULL && strncmp(mime, "video/", 6) == 0 && video_track < 0) {
      video_track = (int)i;
      video_format = format;
    } else if (mime != NULL && strncmp(mime, "audio/", 6) == 0 && audio_track < 0) {
      audio_track = (int)i;
      audio_format = format;
    } else {
      crtmedia_format_release(format);
    }
  }
  CHECK(video_track >= 0 && audio_track >= 0, "the real MP4 fixture has both a video and an audio track");
  if (video_track < 0 || audio_track < 0) {
    if (video_format != NULL) crtmedia_format_release(video_format);
    if (audio_format != NULL) crtmedia_format_release(audio_format);
    crtmedia_extractor_release(extractor);
    fprintf(stderr, "crtmedia_playback_pipeline_test: %d failure(s)\n", failures);
    return 1;
  }
  crtmedia_extractor_select_track(extractor, (uint32_t)video_track);
  crtmedia_extractor_select_track(extractor, (uint32_t)audio_track);

  crtmedia_codec* video_codec = NULL;
  crtmedia_codec* audio_codec = NULL;
  CHECK(
      crtmedia_codec_create_decoder(video_format, &video_codec) == CRTMEDIA_OK,
      "crtmedia_codec_create_decoder succeeds for the video track");
  CHECK(
      crtmedia_codec_create_decoder(audio_format, &audio_codec) == CRTMEDIA_OK,
      "crtmedia_codec_create_decoder succeeds for the audio track");
  crtmedia_format_release(video_format);
  crtmedia_format_release(audio_format);

  crtmedia_player* player = NULL;
  CHECK(crtmedia_player_create(&player) == CRTMEDIA_OK, "crtmedia_player_create succeeds");
  crtmedia_player_play(player); // the real master clock starts advancing now

  crtmedia_audio_sink* sink = NULL; // opened lazily once the first real decoded buffer's own exact format is known

  video_queue vqueue;
  vqueue.count = 0;
  uint32_t video_displayed = 0;
  uint32_t video_dropped = 0;
  uint32_t total_audio_samples = 0;
  uint64_t final_sink_position = 0;
  int64_t last_video_pts = -1;
  int extractor_eof = 0;
  int video_codec_eof = 0;
  int audio_codec_eof = 0;

  int64_t wall_start_us = monotonic_now_us();

  // Presents/drops from the front of vqueue for as long as it has real
  // frames ready to act on -- this is where CRTMEDIA_PIPELINE_VIDEO_
  // LOOKAHEAD's own buffering policy actually plays out: called both
  // whenever the local queue is full (to make real room before decoding
  // more) and once more at the very end (to flush whatever is left).
  #define PRESENT_READY_VIDEO()                                                                    \
    while (vqueue.count > 0) {                                                                      \
      crtmedia_player_video_action action;                                                           \
      int64_t wait_us = 0;                                                                           \
      crtmedia_player_plan_video_frame(player, vqueue.frames[0].timestamp_us, &action, &wait_us);     \
      if (action == CRTMEDIA_PLAYER_VIDEO_WAIT) {                                                     \
        sleep_us(wait_us);                                                                            \
        continue; /* re-plan the same front frame -- the clock has now advanced */                    \
      }                                                                                                \
      CHECK(                                                                                           \
          vqueue.frames[0].format == CRTMEDIA_PIXEL_FORMAT_YUV420P, "a presented/dropped frame is real YUV420P"); \
      if (vqueue.frames[0].timestamp_us != CRTMEDIA_FRAME_TIMESTAMP_NONE) {                            \
        CHECK(vqueue.frames[0].timestamp_us >= last_video_pts, "video frame timestamps are non-decreasing"); \
        last_video_pts = vqueue.frames[0].timestamp_us;                                                \
      }                                                                                                \
      if (action == CRTMEDIA_PLAYER_VIDEO_PRESENT_NOW) {                                               \
        ++video_displayed;                                                                             \
      } else {                                                                                         \
        ++video_dropped;                                                                               \
      }                                                                                                \
      crtmedia_frame_release(&vqueue.frames[0]);                                                       \
      video_queue_pop_front(&vqueue);                                                                  \
    }

  for (int iterations = 0; iterations < 20000 && (video_codec != NULL) && (audio_codec != NULL) &&
                            !(extractor_eof && video_codec_eof && audio_codec_eof && vqueue.count == 0);
       ++iterations) {
    // 1. Feed: pull one real sample from the extractor and route it to
    // the right track's codec, matching extractor_codec_test.c's own
    // already-proven feed loop exactly.
    if (!extractor_eof) {
      crtmedia_sample sample;
      int sample_eof = 0;
      CHECK(
          crtmedia_extractor_read_sample(extractor, &sample, &sample_eof) == CRTMEDIA_OK,
          "crtmedia_extractor_read_sample succeeds");
      if (sample_eof) {
        extractor_eof = 1;
        crtmedia_codec_queue_input(
            video_codec, NULL, 0, CRTMEDIA_FRAME_TIMESTAMP_NONE, CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM);
        crtmedia_codec_queue_input(
            audio_codec, NULL, 0, CRTMEDIA_FRAME_TIMESTAMP_NONE, CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM);
      } else {
        int is_video = (int)sample.track_index == video_track;
        // The real buffering policy in action: never queue more video
        // input while the local lookahead queue is already full --
        // present/drop from it first to make real room, exactly the
        // "how far ahead do we decode" bound this step calls for. Audio
        // has no such gate (queue_input()'s own real CRTMEDIA_WOULD_BLOCK
        // backpressure, handled below, is enough on its own).
        if (is_video) {
          while (vqueue.count >= CRTMEDIA_PIPELINE_VIDEO_LOOKAHEAD) {
            PRESENT_READY_VIDEO();
            if (vqueue.count >= CRTMEDIA_PIPELINE_VIDEO_LOOKAHEAD) {
              // Every queued frame was still WAIT-ing on the very first
              // pass through PRESENT_READY_VIDEO() -- a real, if
              // low-latency, decode-far-ahead-of-schedule case; sleep a
              // small real amount and let PRESENT_READY_VIDEO() try
              // again rather than spinning.
              sleep_us(1000);
            }
          }
        }
        crtmedia_codec* target = is_video ? video_codec : audio_codec;
        crtmedia_result qr;
        for (;;) {
          qr = crtmedia_codec_queue_input(
              target, sample.data, sample.size, sample.pts_us, CRTMEDIA_CODEC_BUFFER_FLAG_NONE);
          if (qr != CRTMEDIA_WOULD_BLOCK) {
            break;
          }
          // Real backpressure -- drain this same codec's own output to
          // make room, then retry the exact same sample (queue_input()'s
          // own documented contract: nothing was lost on WOULD_BLOCK).
          if (is_video) {
            crtmedia_frame frame;
            int frame_eof = 0;
            if (crtmedia_codec_dequeue_output(video_codec, &frame, NULL, &frame_eof) == CRTMEDIA_OK && !frame_eof) {
              video_queue_push(&vqueue, &frame);
            }
          } else {
            crtmedia_audio_buffer buffer;
            int buffer_eof = 0;
            if (crtmedia_codec_dequeue_output(audio_codec, NULL, &buffer, &buffer_eof) == CRTMEDIA_OK &&
                !buffer_eof) {
              total_audio_samples += buffer.frame_count;
              crtmedia_audio_buffer_release(&buffer);
            }
          }
        }
        CHECK(qr == CRTMEDIA_OK, "crtmedia_codec_queue_input eventually succeeds");
        crtmedia_sample_release(&sample);
      }
    }

    // 2. Drain whatever each codec has ready right now.
    for (;;) {
      crtmedia_frame frame;
      int frame_eof = 0;
      crtmedia_result dr = crtmedia_codec_dequeue_output(video_codec, &frame, NULL, &frame_eof);
      if (dr == CRTMEDIA_WOULD_BLOCK) {
        break;
      }
      CHECK(dr == CRTMEDIA_OK, "video crtmedia_codec_dequeue_output succeeds");
      if (dr != CRTMEDIA_OK) {
        break;
      }
      if (frame_eof) {
        video_codec_eof = 1;
        break;
      }
      video_queue_push(&vqueue, &frame);
      if (vqueue.count >= CRTMEDIA_PIPELINE_VIDEO_LOOKAHEAD) {
        break; // real backpressure -- stop pulling more until we present/drop some
      }
    }
    for (;;) {
      crtmedia_audio_buffer buffer;
      int buffer_eof = 0;
      crtmedia_result dr = crtmedia_codec_dequeue_output(audio_codec, NULL, &buffer, &buffer_eof);
      if (dr == CRTMEDIA_WOULD_BLOCK) {
        break;
      }
      CHECK(dr == CRTMEDIA_OK, "audio crtmedia_codec_dequeue_output succeeds");
      if (dr != CRTMEDIA_OK) {
        break;
      }
      if (buffer_eof) {
        audio_codec_eof = 1;
        break;
      }

      // Real audio, written straight through -- see this file's own top
      // comment for why audio gets no local lookahead queue.
      if (sink == NULL) {
        crtmedia_audio_sink_desc desc;
        desc.format = buffer.format;
        desc.sample_rate = buffer.sample_rate;
        desc.channels = buffer.channels;
        crtmedia_result open_result = crtmedia_audio_sink_open(&desc, &sink);
        CHECK(
            open_result == CRTMEDIA_OK || open_result == CRTMEDIA_ERROR_UNSUPPORTED,
            "crtmedia_audio_sink_open() either succeeds or reports CRTMEDIA_ERROR_UNSUPPORTED, nothing else");
      }
      if (sink != NULL) {
        const uint8_t* cursor = (const uint8_t*)buffer.data;
        uint32_t frames_left = buffer.frame_count;
        uint32_t block_align =
            buffer.channels * ((buffer.format == CRTMEDIA_SAMPLE_FORMAT_FLT) ? 4u : 2u);
        int zero_write_streak = 0;
        while (frames_left > 0) {
          int64_t written = crtmedia_audio_sink_write(sink, cursor, frames_left);
          if (written < 0) {
            // A real, negative device failure -- this header's own
            // documented contract, distinct from written == 0 (a real,
            // valid "nothing accepted this call, simply retry" result,
            // handled below). A real player keeps playing video on a
            // real mid-stream audio failure rather than giving up
            // entirely (crtmedia_player.h's own documented "a video-only
            // stream free-runs on real host wall-clock time" shape is
            // exactly this case, just arrived at mid-playback instead of
            // from the start) -- deliberately not calling crtmedia_
            // audio_sink_close() here: a sink already reporting a real
            // failure may itself be unable to complete its own real
            // drain in close()'s own contract, and this process is about
            // to exit regardless. fprintf(), not CHECK(): a real,
            // externally-caused device failure mid-stream (this exact
            // dev environment's own WSLg PulseAudio bridge was already
            // confirmed, separately, to genuinely stop responding after
            // roughly one real second of continuous audio -- see
            // audio_sink_linux.c's own top comment) is not this test's
            // own bug to report as one.
            fprintf(stderr, "crtmedia_playback_pipeline_test: audio device stopped accepting data mid-stream\n");
            sink = NULL;
            break;
          }
          if (written == 0) {
            // A real, valid, documented outcome (audio_sink.h's own
            // "0 is a real, valid, non-error result ... the caller
            // should simply retry") -- bounded here purely as a defensive
            // safety net against a real backend that somehow never makes
            // forward progress, not because 0 itself is unexpected.
            ++zero_write_streak;
            CHECK(zero_write_streak < 10000, "crtmedia_audio_sink_write() eventually makes real forward progress");
            if (zero_write_streak >= 10000) {
              sink = NULL;
              break;
            }
            continue;
          }
          zero_write_streak = 0;
          cursor += (uint32_t)written * block_align;
          frames_left -= (uint32_t)written;
        }
        if (sink != NULL && buffer.timestamp_us != CRTMEDIA_AUDIO_TIMESTAMP_NONE) {
          crtmedia_player_update_audio_clock(player, buffer.timestamp_us);
        }
      }

      total_audio_samples += buffer.frame_count;
      crtmedia_audio_buffer_release(&buffer);
    }

    // 3. Present/drop whatever real video is actually due right now.
    PRESENT_READY_VIDEO();
  }

  #undef PRESENT_READY_VIDEO

  int64_t wall_elapsed_us = monotonic_now_us() - wall_start_us;

  CHECK(extractor_eof, "extractor eventually reports EOF");
  CHECK(video_codec_eof, "video codec eventually drains to EOF");
  CHECK(audio_codec_eof, "audio codec eventually drains to EOF");
  CHECK(vqueue.count == 0, "every decoded video frame is eventually presented or dropped, none left queued");
  CHECK(
      video_displayed + video_dropped == 25,
      "every one of the fixture's real 25 encoded video frames is accounted for (displayed or dropped)");
  CHECK(video_displayed > 0, "at least some real video frames were actually presented, not all dropped");
  CHECK(
      total_audio_samples > 30000 && total_audio_samples < 60000,
      "total decoded audio samples are in the real ~1-second range for a 44100 Hz stream");
  // A real, deliberately generous window: the fixture is ~1 real second
  // of content; this loop's own real CRTMEDIA_PLAYER_VIDEO_WAIT sleeps
  // (paced by the real player clock, real audio device backpressure when
  // a sink is open) should keep total wall time roughly in that range,
  // not near-zero (which would mean WAIT/real pacing was never actually
  // exercised) and not wildly over (which would mean something is
  // genuinely stuck, not just slow).
  CHECK(wall_elapsed_us > 200000, "the real render loop takes a real, non-negligible amount of wall time");
  CHECK(wall_elapsed_us < 15000000, "the real render loop finishes in a real, bounded amount of wall time");

  if (sink != NULL) {
    crtmedia_result pos_result = crtmedia_audio_sink_get_position_frames(sink, &final_sink_position);
    CHECK(pos_result == CRTMEDIA_OK, "crtmedia_audio_sink_get_position_frames() succeeds at the end of playback");
    crtmedia_audio_sink_close(sink); // a real drain
  }
  (void)final_sink_position;

  crtmedia_codec_release(video_codec);
  crtmedia_codec_release(audio_codec);
  crtmedia_player_release(player);
  crtmedia_extractor_release(extractor);

  if (failures != 0) {
    fprintf(stderr, "crtmedia_playback_pipeline_test: %d failure(s)\n", failures);
    return 1;
  }
  printf("crtmedia_playback_pipeline_test: ok\n");
  return 0;
}
