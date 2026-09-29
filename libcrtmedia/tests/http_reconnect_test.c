// Networking & Streaming Tranche 4 acceptance (docs/crtmedia_networking_
// acceptance.md): reconnect and discontinuity. A repository-owned loopback
// server (tests/http_test_server.h, tests/http_upload_test_server.h) injects
// the real failures -- closing mid-response, changing the resource between
// connections, refusing every resume, dying mid-upload -- and this test
// checks the reconnect contract:
//
//   * resume only with Range + If-Range and all three response checks
//     (206, Content-Range start == offset, same entity validator);
//   * the resumed stream is byte-exact: the extracted samples (track, PTS,
//     size, content) are identical to a clean local-file extraction, none
//     duplicated, none lost;
//   * retries are bounded;
//   * a source with no validator, a changed resource, or a server that
//     cannot resume fails honestly -- never a clean end of stream, never a
//     splice of two versions;
//   * output never auto-resumes: a dropped upload is CRTMEDIA_ERROR_IO and
//     the client opens no second connection.

#include "http_test_server.h"
#include "http_transport.h"
#include "http_upload.h"
#include "http_upload_test_server.h"

#include "crtmedia/extractor.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef CRTMEDIA_TEST_VIDEO_PATH
#error "CRTMEDIA_TEST_VIDEO_PATH must be defined (see libcrtmedia/CMakeLists.txt)"
#endif

static int failures = 0;

#define CHECK(cond, msg)                                             \
  do {                                                               \
    if (!(cond)) {                                                   \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg);  \
      ++failures;                                                    \
    }                                                                \
  } while (0)

typedef struct extraction_summary {
  crtmedia_result open_result;
  crtmedia_result end_result; /* CRTMEDIA_OK == reached a clean EOF */
  uint32_t samples;
  uint64_t hash; /* over track, pts, size and every payload byte, in order */
  int64_t max_pts;
} extraction_summary;

static uint64_t fnv1a(uint64_t hash, const void* data, size_t size) {
  const unsigned char* bytes = (const unsigned char*)data;
  for (size_t i = 0; i < size; ++i) {
    hash ^= bytes[i];
    hash *= 1099511628211ull;
  }
  return hash;
}

static void extract_all(crtmedia_extractor* extractor, extraction_summary* out) {
  uint32_t tracks = crtmedia_extractor_track_count(extractor);
  for (uint32_t i = 0; i < tracks; ++i) {
    crtmedia_extractor_select_track(extractor, i);
  }
  out->hash = 1469598103934665603ull;
  out->max_pts = -1;
  for (;;) {
    crtmedia_sample sample;
    int eof = 0;
    crtmedia_result r = crtmedia_extractor_read_sample(extractor, &sample, &eof);
    if (r != CRTMEDIA_OK) {
      out->end_result = r;
      return;
    }
    if (eof) {
      out->end_result = CRTMEDIA_OK;
      return;
    }
    out->hash = fnv1a(out->hash, &sample.track_index, sizeof(sample.track_index));
    out->hash = fnv1a(out->hash, &sample.pts_us, sizeof(sample.pts_us));
    out->hash = fnv1a(out->hash, &sample.size, sizeof(sample.size));
    out->hash = fnv1a(out->hash, sample.data, sample.size);
    if (sample.pts_us > out->max_pts) {
      out->max_pts = sample.pts_us;
    }
    ++out->samples;
    crtmedia_sample_release(&sample);
  }
}

static void extract_url(const char* url, extraction_summary* out) {
  memset(out, 0, sizeof(*out));
  crtmedia_extractor* extractor = NULL;
  out->open_result = crtmedia_extractor_create_from_url(url, &extractor);
  if (out->open_result != CRTMEDIA_OK) {
    return;
  }
  extract_all(extractor, out);
  crtmedia_extractor_release(extractor);
}

static void* read_file(const char* path, size_t* out_size) {
  FILE* file = fopen(path, "rb");
  if (file == NULL) {
    return NULL;
  }
  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);
  void* data = size >= 0 ? malloc((size_t)size) : NULL;
  if (data == NULL || fread(data, 1, (size_t)size, file) != (size_t)size) {
    free(data);
    fclose(file);
    return NULL;
  }
  fclose(file);
  *out_size = (size_t)size;
  return data;
}

static http_test_server* start_server(
    const void* body, size_t size, const http_test_server_options* options, char* url, size_t url_size) {
  http_test_server* server = NULL;
  int port = 0;
  if (http_test_server_start_ex(body, size, HTTP_TEST_SERVER_RANGE_CAPABLE, options, &server, &port) != 0) {
    return NULL;
  }
  snprintf(url, url_size, "http://127.0.0.1:%d/test.mp4", port);
  return server;
}

static void sleep_ms(int ms) {
  struct timespec delay;
  delay.tv_sec = ms / 1000;
  delay.tv_nsec = (long)(ms % 1000) * 1000000L;
  nanosleep(&delay, NULL);
}

static int transport_read_all(crtmedia_http_transport* transport, unsigned char* out, size_t capacity, size_t* out_size) {
  size_t total = 0;
  for (;;) {
    size_t n = 0;
    int eof = 0;
    crtmedia_result r = crtmedia_http_transport_read(transport, out + total, capacity - total, 5000, &n, &eof);
    if (r != CRTMEDIA_OK) {
      return -1;
    }
    if (eof) {
      *out_size = total;
      return 0;
    }
    total += n;
  }
}

int main(void) {
  size_t body_size = 0;
  unsigned char* body = (unsigned char*)read_file(CRTMEDIA_TEST_VIDEO_PATH, &body_size);
  if (body == NULL) {
    fprintf(stderr, "crtmedia_http_reconnect_test: cannot load fixture\n");
    return 1;
  }
  char url[128];
  http_test_server_stats stats;
  http_test_server_options options;
  const size_t drop_after = body_size / 4;

  /* Reference: the same fixture read from local disk. */
  extraction_summary local;
  memset(&local, 0, sizeof(local));
  {
    crtmedia_extractor* extractor = NULL;
    CHECK(crtmedia_extractor_create(CRTMEDIA_TEST_VIDEO_PATH, &extractor) == CRTMEDIA_OK, "open local reference");
    if (extractor == NULL) {
      free(body);
      return 1;
    }
    extract_all(extractor, &local);
    crtmedia_extractor_release(extractor);
    CHECK(local.end_result == CRTMEDIA_OK && local.samples > 0, "local reference extracts cleanly");
  }

  /* A. No faults, with a validator: identical to local, no resume. */
  memset(&options, 0, sizeof(options));
  options.etag = "v1";
  http_test_server* server = start_server(body, body_size, &options, url, sizeof(url));
  CHECK(server != NULL, "start clean server");
  extraction_summary clean;
  extract_url(url, &clean);
  http_test_server_get_stats(server, &stats);
  http_test_server_stop(server);
  CHECK(clean.open_result == CRTMEDIA_OK && clean.end_result == CRTMEDIA_OK, "clean HTTP extraction succeeds");
  CHECK(clean.samples == local.samples && clean.hash == local.hash, "clean HTTP samples == local samples");
  const int clean_requests = stats.requests;
  CHECK(stats.resume_requests == 0, "a clean stream needs no resume");

  /* B. Server closes mid-response twice; validator present -> both are
   * resumed and the sample stream is byte-exact. */
  memset(&options, 0, sizeof(options));
  options.etag = "v1";
  options.truncate_connections = 2;
  options.truncate_after_bytes = drop_after;
  server = start_server(body, body_size, &options, url, sizeof(url));
  CHECK(server != NULL, "start truncating server");
  extraction_summary resumed;
  extract_url(url, &resumed);
  http_test_server_get_stats(server, &stats);
  http_test_server_stop(server);
  CHECK(resumed.open_result == CRTMEDIA_OK && resumed.end_result == CRTMEDIA_OK, "resumed extraction reaches a clean EOF");
  CHECK(resumed.samples == local.samples, "no sample lost or duplicated across two reconnects");
  CHECK(resumed.hash == local.hash, "resumed sample stream is byte- and PTS-exact vs local");
  CHECK(resumed.max_pts == local.max_pts, "PTS timeline preserved across reconnects");
  const int resumed_extra_requests = stats.requests - clean_requests;
  CHECK(resumed_extra_requests == 2, "exactly the two drops forced exactly two resume connections");
  CHECK(stats.resume_requests > 0 && stats.resume_requests == stats.resume_requests_with_if_range,
        "every resume request carried If-Range");

  /* C. No validator at all -> a drop cannot be proven safe to resume. */
  memset(&options, 0, sizeof(options));
  server = start_server(body, body_size, &options, url, sizeof(url));
  extraction_summary no_validator_clean;
  extract_url(url, &no_validator_clean);
  http_test_server_get_stats(server, &stats);
  http_test_server_stop(server);
  CHECK(stats.requests == clean_requests, "a validator-less server serves the clean stream in the same requests");
  CHECK(no_validator_clean.end_result == CRTMEDIA_OK && no_validator_clean.hash == local.hash,
        "validator-less clean extraction still works");

  memset(&options, 0, sizeof(options));
  options.truncate_connections = 1;
  options.truncate_after_bytes = drop_after;
  server = start_server(body, body_size, &options, url, sizeof(url));
  extraction_summary no_validator;
  extract_url(url, &no_validator);
  http_test_server_get_stats(server, &stats);
  http_test_server_stop(server);
  CHECK(no_validator.open_result == CRTMEDIA_ERROR_IO || no_validator.end_result == CRTMEDIA_ERROR_IO,
        "a drop with no validator is surfaced as CRTMEDIA_ERROR_IO, not a clean EOF or a bogus 'unsupported'");
  CHECK(stats.resume_requests == 0, "no resume attempted without a validator");

  /* D. Resource changed, server honors If-Range (answers 200): refuse. */
  memset(&options, 0, sizeof(options));
  options.etag = "v1";
  options.truncate_connections = 1;
  options.truncate_after_bytes = drop_after;
  options.change_etag_after_first = 1;
  options.honor_if_range = 1;
  server = start_server(body, body_size, &options, url, sizeof(url));
  extraction_summary changed_200;
  extract_url(url, &changed_200);
  http_test_server_stop(server);
  CHECK(changed_200.open_result == CRTMEDIA_ERROR_PROTOCOL || changed_200.end_result == CRTMEDIA_ERROR_PROTOCOL,
        "changed resource (200 to If-Range) is CRTMEDIA_ERROR_PROTOCOL, never spliced");

  /* E. Resource changed, server ignores If-Range (206 with a new ETag). */
  options.honor_if_range = 0;
  server = start_server(body, body_size, &options, url, sizeof(url));
  extraction_summary changed_206;
  extract_url(url, &changed_206);
  http_test_server_stop(server);
  CHECK(changed_206.open_result == CRTMEDIA_ERROR_PROTOCOL || changed_206.end_result == CRTMEDIA_ERROR_PROTOCOL,
        "changed resource (206 with a different ETag) is CRTMEDIA_ERROR_PROTOCOL");

  /* F. Every resume is refused: bounded retries, then an honest error. */
  int refused_retry_requests = 0;
  memset(&options, 0, sizeof(options));
  options.etag = "v1";
  options.truncate_connections = 1;
  options.truncate_after_bytes = drop_after;
  options.refuse_after_first = 1;
  server = start_server(body, body_size, &options, url, sizeof(url));
  extraction_summary refused;
  extract_url(url, &refused);
  http_test_server_get_stats(server, &stats);
  http_test_server_stop(server);
  CHECK(refused.open_result == CRTMEDIA_ERROR_IO || refused.end_result == CRTMEDIA_ERROR_IO,
        "exhausted retries surface CRTMEDIA_ERROR_IO");
  refused_retry_requests = stats.requests - clean_requests;
  CHECK(refused_retry_requests == 3, "retries are bounded: exactly 3 resume attempts, then give up");

  /* Transport-level contract, exact status combinations. */
  {
    memset(&options, 0, sizeof(options));
    options.etag = "v1";
    server = start_server(body, body_size, &options, url, sizeof(url));
    crtmedia_http_transport* t = NULL;
    crtmedia_http_transport_info info;
    CHECK(crtmedia_http_transport_open(url, 0, 4096, NULL, &t, &info) == CRTMEDIA_OK, "plain open");
    CHECK(strcmp(info.validator, "\"v1\"") == 0, "strong ETag captured as the entity validator");
    crtmedia_http_transport_close(t);
    t = NULL;
    CHECK(crtmedia_http_transport_open_resume(url, 1000, info.validator, 4096, NULL, &t, &info) == CRTMEDIA_OK,
          "resume with the matching validator succeeds (206 + Content-Range + same ETag)");
    unsigned char* tail = (unsigned char*)malloc(body_size);
    size_t tail_size = 0;
    if (t != NULL && tail != NULL) {
      CHECK(transport_read_all(t, tail, body_size, &tail_size) == 0, "read resumed body");
      CHECK(tail_size == body_size - 1000 && memcmp(tail, body + 1000, tail_size) == 0,
            "resumed bytes are exactly the bytes from the requested offset");
    }
    free(tail);
    crtmedia_http_transport_close(t);
    http_test_server_stop(server);
  }
  {
    memset(&options, 0, sizeof(options));
    options.etag = "v1";
    options.change_etag_after_first = 1;
    options.honor_if_range = 0;
    server = start_server(body, body_size, &options, url, sizeof(url));
    crtmedia_http_transport* t = NULL;
    crtmedia_http_transport_info info;
    CHECK(crtmedia_http_transport_open(url, 0, 4096, NULL, &t, &info) == CRTMEDIA_OK, "plain open (changing server)");
    crtmedia_http_transport_close(t);
    t = NULL;
    CHECK(crtmedia_http_transport_open_resume(url, 1000, info.validator, 4096, NULL, &t, &info) == CRTMEDIA_ERROR_PROTOCOL,
          "206 with a different ETag is refused");
    CHECK(t == NULL, "no transport handed out for a refused resume");
    http_test_server_stop(server);
  }

  /* Output never auto-resumes. */
  {
    http_upload_test_server* upload_server = NULL;
    int port = 0;
    CHECK(http_upload_test_server_start_dropping(48 * 1024, &upload_server, &port) == 0, "start dying receiver");
    char upload_url[128];
    snprintf(upload_url, sizeof(upload_url), "http://127.0.0.1:%d/upload.mp4", port);
    crtmedia_http_upload* upload = NULL;
    CHECK(crtmedia_http_upload_open(upload_url, 4096, NULL, &upload) == CRTMEDIA_OK, "open upload");
    unsigned char block[8192];
    memset(block, 0x5a, sizeof(block));
    crtmedia_result write_result = CRTMEDIA_OK;
    for (int i = 0; i < 1024 && write_result == CRTMEDIA_OK; ++i) {
      write_result = crtmedia_http_upload_write(upload, block, sizeof(block));
    }
    CHECK(write_result == CRTMEDIA_ERROR_IO, "a receiver dying mid-upload surfaces CRTMEDIA_ERROR_IO to the writer");
    CHECK(crtmedia_http_upload_finish(upload) != CRTMEDIA_OK, "finish never reports success after a lost upload");
    sleep_ms(500); /* longer than the server's accept poll: a silent resume would have connected by now */
    CHECK(http_upload_test_server_connection_count(upload_server) == 1,
          "no automatic resume: the client opened exactly one connection");
    crtmedia_http_upload_close(upload);
    http_upload_test_server_stop(upload_server);
  }

  free(body);
  if (failures != 0) {
    fprintf(stderr, "crtmedia_http_reconnect_test: %d failure(s)\n", failures);
    return 1;
  }
  printf(
      "crtmedia_http_reconnect_test: ok samples=%u resumes=%d bounded_retries=%d changed_resource=protocol "
      "no_validator=io upload_drop=io\n",
      local.samples, resumed_extra_requests, refused_retry_requests);
  return 0;
}
