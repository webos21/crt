// Real malformed/invalid-input coverage for crtmedia/demux.h -- confirms
// crtmedia_demuxer_open()/crtmedia_demuxer_read() fail cleanly (a real,
// defined crtmedia_result error code, never a crash/hang/silent-garbage-
// success) against inputs no well-formed media file would ever produce.
// TODO.md's upper-runtime roadmap step 1 calls this out explicitly
// ("malformed input") alongside the real-fixture decode coverage in
// demux_decode_video_test.c/demux_decode_mp3_test.c.
//
// The two malformed fixtures here are generated at test run time (not
// committed binary assets -- unlike the real, well-formed fixtures in
// assets/, deliberately-broken input has no meaningful "provenance" to
// document, and generating it from the already-real, already-checked-in
// test_video.mp4 keeps this test self-contained): a byte-for-byte prefix
// truncation of the real MP4 fixture (a real container with a real header
// but a body cut off mid-stream -- the shape a crashed/interrupted
// download or write would actually produce), and a small buffer of
// non-media bytes with a plausible ".mp4" extension (the shape a
// completely wrong file being handed to this API by mistake would
// produce). CRTMEDIA_TEST_VIDEO_PATH/CRTMEDIA_TEST_SCRATCH_DIR are
// compile-time -D defines (libcrtmedia/CMakeLists.txt).

#include "crtmedia/demux.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CRTMEDIA_TEST_VIDEO_PATH
#error "CRTMEDIA_TEST_VIDEO_PATH must be defined (see libcrtmedia/CMakeLists.txt)"
#endif
#ifndef CRTMEDIA_TEST_SCRATCH_DIR
#error "CRTMEDIA_TEST_SCRATCH_DIR must be defined (see libcrtmedia/CMakeLists.txt)"
#endif

static int failures = 0;

#define CHECK(cond, msg)                                                                     \
  do {                                                                                        \
    if (!(cond)) {                                                                            \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg);                           \
      ++failures;                                                                             \
    }                                                                                          \
  } while (0)

// Writes the first `prefix_bytes` of `src_path` (or, if `src_path` is
// NULL, `prefix_bytes` of literal 0x00) to `dst_path`. Returns 1 on
// success, 0 on any real I/O failure (a scratch-file setup problem, not
// itself part of what this test is checking).
static int write_prefix_file(const char* src_path, size_t prefix_bytes, const char* dst_path) {
  unsigned char buffer[4096];
  size_t remaining = prefix_bytes;
  FILE* out = fopen(dst_path, "wb");
  if (out == NULL) {
    return 0;
  }
  FILE* in = src_path != NULL ? fopen(src_path, "rb") : NULL;
  if (src_path != NULL && in == NULL) {
    fclose(out);
    return 0;
  }
  memset(buffer, 0, sizeof(buffer));
  while (remaining > 0) {
    size_t chunk = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
    size_t got = chunk;
    if (in != NULL) {
      got = fread(buffer, 1, chunk, in);
      if (got == 0) {
        break;
      }
    }
    if (fwrite(buffer, 1, got, out) != got) {
      if (in != NULL) {
        fclose(in);
      }
      fclose(out);
      return 0;
    }
    remaining -= got;
  }
  if (in != NULL) {
    fclose(in);
  }
  fclose(out);
  return 1;
}

int main(void) {
  crtmedia_demuxer* demuxer = NULL;

  // 1. Null-argument handling (CRTMEDIA_ERROR_INVALID_ARGUMENT, not a
  //    crash) -- demux.h's own documented contract.
  crtmedia_result r = crtmedia_demuxer_open(NULL, &demuxer);
  CHECK(r == CRTMEDIA_ERROR_INVALID_ARGUMENT, "null path returns CRTMEDIA_ERROR_INVALID_ARGUMENT");
  r = crtmedia_demuxer_open(CRTMEDIA_TEST_VIDEO_PATH, NULL);
  CHECK(r == CRTMEDIA_ERROR_INVALID_ARGUMENT, "null out_demuxer returns CRTMEDIA_ERROR_INVALID_ARGUMENT");

  // 2. A path that does not exist at all.
  demuxer = NULL;
  r = crtmedia_demuxer_open("this/path/does/not/exist/crtmedia_malformed_test.mp4", &demuxer);
  CHECK(r == CRTMEDIA_ERROR_UNSUPPORTED, "a nonexistent path returns CRTMEDIA_ERROR_UNSUPPORTED");
  CHECK(demuxer == NULL, "a failed open leaves *out_demuxer NULL");

  // 3. A file that is not a real media container at all (plausible
  //    extension, garbage content) -- avformat's own format probe should
  //    reject it outright.
  char garbage_path[512];
  snprintf(garbage_path, sizeof(garbage_path), "%s/crtmedia_malformed_test_garbage.mp4", CRTMEDIA_TEST_SCRATCH_DIR);
  CHECK(write_prefix_file(NULL, 256, garbage_path), "garbage scratch fixture writes successfully");
  demuxer = NULL;
  r = crtmedia_demuxer_open(garbage_path, &demuxer);
  CHECK(r == CRTMEDIA_ERROR_UNSUPPORTED, "256 zero bytes with an .mp4 name returns CRTMEDIA_ERROR_UNSUPPORTED");
  CHECK(demuxer == NULL, "a failed open leaves *out_demuxer NULL (garbage case)");

  // 4. A real container header truncated well before its real end -- a
  //    real MP4's moov/mdat structure means a short-enough prefix may
  //    still successfully *open* (the header/track description can be
  //    fully present before the truncation point) but must never produce
  //    a false-complete decode: either open itself fails, or read()
  //    eventually reports a real error (or, at worst, a real, bounded EOF
  //    short of the full known-good frame count) -- never a crash, never
  //    silently reporting the untruncated fixture's own full result.
  char truncated_path[512];
  snprintf(truncated_path, sizeof(truncated_path), "%s/crtmedia_malformed_test_truncated.mp4",
           CRTMEDIA_TEST_SCRATCH_DIR);
  CHECK(write_prefix_file(CRTMEDIA_TEST_VIDEO_PATH, 512, truncated_path),
        "truncated scratch fixture writes successfully");
  demuxer = NULL;
  r = crtmedia_demuxer_open(truncated_path, &demuxer);
  if (r == CRTMEDIA_OK && demuxer != NULL) {
    uint32_t video_frames = 0;
    int hit_defined_outcome = 0;
    for (int iterations = 0; iterations < 10000 && !hit_defined_outcome; ++iterations) {
      crtmedia_read_status status;
      uint32_t stream_index;
      crtmedia_frame video_frame;
      crtmedia_audio_buffer audio_buffer;
      crtmedia_result read_r =
          crtmedia_demuxer_read(demuxer, &status, &stream_index, &video_frame, &audio_buffer);
      if (read_r != CRTMEDIA_OK) {
        hit_defined_outcome = 1; // a real, defined decode-error return -- acceptable.
        break;
      }
      if (status == CRTMEDIA_READ_EOF) {
        hit_defined_outcome = 1; // a real, defined (short) EOF -- acceptable.
        break;
      }
      if (status == CRTMEDIA_READ_VIDEO_FRAME) {
        ++video_frames;
        crtmedia_frame_release(&video_frame);
      } else {
        crtmedia_audio_buffer_release(&audio_buffer);
      }
    }
    CHECK(hit_defined_outcome, "reading a truncated fixture reaches a real, defined outcome (error or EOF)");
    CHECK(video_frames < 25, "a truncated fixture never reports the full untruncated frame count");
    crtmedia_demuxer_close(demuxer);
  } else {
    CHECK(r != CRTMEDIA_OK ? demuxer == NULL : 1, "a failed truncated-fixture open leaves *out_demuxer NULL");
  }

  if (failures != 0) {
    fprintf(stderr, "crtmedia_demux_malformed_test: %d failure(s)\n", failures);
    return 1;
  }
  printf("crtmedia_demux_malformed_test: ok\n");
  return 0;
}
