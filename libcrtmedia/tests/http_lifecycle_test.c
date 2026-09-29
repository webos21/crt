// Networking & Streaming Tranche 6 acceptance (docs/crtmedia_networking_
// acceptance.md): repeated connect / stream / cancel / reconnect / destroy
// lifecycle stress across every network path -- the HTTP input transport, the
// URL extractor (plain, reconnecting, and over TLS), and the HTTP upload
// sink -- with a native-resource audit around it. Modeled on Encode &
// Capture Tranche 6's capture_encode_lifecycle_test.c: a single pass rarely
// exposes create/destroy lifetime bugs, so each path is cycled many times in
// one process and the process's own handle/fd/thread population is compared
// before and after.
//
// The audit is the strongest signal this host exposes without a debugger:
//   Windows: GetProcessHandleCount() (every leaked socket, thread, event or
//            file handle raises it);
//   Linux:   entries in /proc/self/fd and /proc/self/task;
//   macOS:   entries in /dev/fd (threads are not observable there, reported
//            as -1; a leaked worker thread still shows up as the unjoined
//            thread's own resources through the fd/handle side and by the
//            test hanging in release()).
// Every path is warmed up once first, so one-time library initialization is
// not mistaken for a leak.

#include "http_test_server.h"
#include "http_transport.h"
#include "http_upload.h"
#include "http_upload_test_server.h"
#include "tls_test_server.h"

#include "crtmedia/extractor.h"
#include "crtmedia/tls.h"

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef CRTMEDIA_TEST_VIDEO_PATH
#error "CRTMEDIA_TEST_VIDEO_PATH must be defined (see libcrtmedia/CMakeLists.txt)"
#endif

#if defined(CRT_TARGET_OS_WINDOWS)
__declspec(dllimport) void* GetCurrentProcess(void);
__declspec(dllimport) int GetProcessHandleCount(void* process, unsigned int* count);
#endif

#define TRANSPORT_CANCEL_CYCLES 40
#define EXTRACTOR_CANCEL_CYCLES 30
#define RECONNECT_CYCLES 8
#define UPLOAD_CANCEL_CYCLES 20
#define TLS_CANCEL_CYCLES 15
#define TLS_REFUSED_CYCLES 15
#define BIG_BODY_BYTES (8 * 1024 * 1024)
#define HANDLE_TOLERANCE 4

static int failures = 0;

#define CHECK(cond, msg)                                             \
  do {                                                               \
    if (!(cond)) {                                                   \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg);  \
      ++failures;                                                    \
    }                                                                \
  } while (0)

typedef struct resources {
  int handles; /* fds (POSIX) or process handles (Windows); -1 unknown */
  int threads; /* -1 unknown */
} resources;

#if !defined(CRT_TARGET_OS_WINDOWS)
static int count_directory(const char* path) {
  DIR* dir = opendir(path);
  if (dir == NULL) {
    return -1;
  }
  int count = 0;
  struct dirent* entry;
  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_name[0] != '.') {
      ++count;
    }
  }
  closedir(dir);
  return count;
}

#endif

static resources sample_resources(void) {
  resources r;
  r.handles = -1;
  r.threads = -1;
#if defined(CRT_TARGET_OS_WINDOWS)
  unsigned int count = 0;
  if (GetProcessHandleCount(GetCurrentProcess(), &count) != 0) {
    r.handles = (int)count;
  }
#else
  r.handles = count_directory("/proc/self/fd");
  if (r.handles < 0) {
    r.handles = count_directory("/dev/fd");
  }
  r.threads = count_directory("/proc/self/task");
#endif
  return r;
}

static uint64_t fnv1a(uint64_t hash, const void* data, size_t size) {
  const unsigned char* bytes = (const unsigned char*)data;
  for (size_t i = 0; i < size; ++i) {
    hash ^= bytes[i];
    hash *= 1099511628211ull;
  }
  return hash;
}

typedef struct extraction_summary {
  crtmedia_result open_result;
  crtmedia_result end_result;
  uint32_t samples;
  uint64_t hash;
} extraction_summary;

/* Reads up to `limit` samples (0 = to EOF). */
static void extract_some(crtmedia_extractor* extractor, uint32_t limit, extraction_summary* out) {
  uint32_t tracks = crtmedia_extractor_track_count(extractor);
  for (uint32_t i = 0; i < tracks; ++i) {
    crtmedia_extractor_select_track(extractor, i);
  }
  out->hash = 1469598103934665603ull;
  while (limit == 0 || out->samples < limit) {
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
    ++out->samples;
    crtmedia_sample_release(&sample);
  }
  out->end_result = CRTMEDIA_OK;
}

static void extract_url(const char* url, const crtmedia_tls_options* tls, uint32_t limit,
                        extraction_summary* out) {
  memset(out, 0, sizeof(*out));
  crtmedia_extractor* extractor = NULL;
  out->open_result = crtmedia_extractor_create_from_url_with_tls(url, tls, &extractor);
  if (out->open_result != CRTMEDIA_OK) {
    return;
  }
  extract_some(extractor, limit, out);
  crtmedia_extractor_release(extractor); /* mid-stream destroy when limit != 0 */
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

typedef struct scenario_audit {
  const char* name;
  resources before;
  resources after;
} scenario_audit;

static void audit_begin(scenario_audit* audit, const char* name) {
  audit->name = name;
  audit->before = sample_resources();
}

static void audit_end(scenario_audit* audit) {
  audit->after = sample_resources();
  if (audit->before.handles >= 0) {
    int delta = audit->after.handles - audit->before.handles;
    if (delta > HANDLE_TOLERANCE) {
      fprintf(stderr, "FAIL %s: handle/fd count grew %d -> %d\n", audit->name, audit->before.handles,
              audit->after.handles);
      ++failures;
    }
  }
  if (audit->before.threads >= 0 && audit->after.threads > audit->before.threads) {
    fprintf(stderr, "FAIL %s: thread count grew %d -> %d\n", audit->name, audit->before.threads,
            audit->after.threads);
    ++failures;
  }
}

/* One transport open, a partial read of a body far larger than the bounded
 * queue (so the worker is genuinely blocked mid-transfer), then close. */
static void transport_cancel_cycle(const char* url) {
  crtmedia_http_transport* transport = NULL;
  crtmedia_http_transport_info info;
  crtmedia_result r = crtmedia_http_transport_open(url, 0, 32 * 1024, NULL, &transport, &info);
  CHECK(r == CRTMEDIA_OK, "transport open");
  if (r != CRTMEDIA_OK) {
    return;
  }
  unsigned char buffer[16384];
  size_t got = 0;
  int eof = 0;
  CHECK(crtmedia_http_transport_read(transport, buffer, sizeof(buffer), 5000, &got, &eof) == CRTMEDIA_OK && got > 0,
        "transport delivers data before cancel");
  crtmedia_http_transport_close(transport);
}

int main(void) {
  size_t mp4_size = 0;
  unsigned char* mp4 = (unsigned char*)read_file(CRTMEDIA_TEST_VIDEO_PATH, &mp4_size);
  if (mp4 == NULL) {
    fprintf(stderr, "crtmedia_http_lifecycle_test: cannot load fixture\n");
    return 1;
  }
  unsigned char* big = (unsigned char*)malloc(BIG_BODY_BYTES);
  if (big == NULL) {
    return 1;
  }
  for (size_t i = 0; i < BIG_BODY_BYTES; ++i) {
    big[i] = (unsigned char)(i * 131 + (i >> 8));
  }

  extraction_summary local;
  memset(&local, 0, sizeof(local));
  {
    crtmedia_extractor* extractor = NULL;
    CHECK(crtmedia_extractor_create(CRTMEDIA_TEST_VIDEO_PATH, &extractor) == CRTMEDIA_OK, "open local reference");
    if (extractor == NULL) {
      return 1;
    }
    extract_some(extractor, 0, &local);
    crtmedia_extractor_release(extractor);
  }

  http_test_server* big_server = NULL;
  http_test_server* mp4_server = NULL;
  int big_port = 0;
  int mp4_port = 0;
  CHECK(http_test_server_start(big, BIG_BODY_BYTES, HTTP_TEST_SERVER_RANGE_CAPABLE, &big_server, &big_port) == 0,
        "start big-body server");
  CHECK(http_test_server_start(mp4, mp4_size, HTTP_TEST_SERVER_RANGE_CAPABLE, &mp4_server, &mp4_port) == 0,
        "start mp4 server");
  if (big_server == NULL || mp4_server == NULL) {
    return 1;
  }
  char big_url[128];
  char mp4_url[128];
  snprintf(big_url, sizeof(big_url), "http://127.0.0.1:%d/big.bin", big_port);
  snprintf(mp4_url, sizeof(mp4_url), "http://127.0.0.1:%d/test.mp4", mp4_port);

  /* Warm-up: one pass of every path, so lazy one-time initialization (curl
   * global state, thread-local buffers) is not reported as a leak. */
  extraction_summary summary;
  transport_cancel_cycle(big_url);
  extract_url(mp4_url, NULL, 3, &summary);

  resources start = sample_resources();
  scenario_audit audit;

  /* 1. Transport: connect, stream, cancel mid-transfer, destroy. */
  audit_begin(&audit, "transport cancel");
  for (int i = 0; i < TRANSPORT_CANCEL_CYCLES; ++i) {
    transport_cancel_cycle(big_url);
  }
  audit_end(&audit);

  /* 2. Extractor: open, read a few samples, release mid-stream. */
  audit_begin(&audit, "extractor mid-stream release");
  for (int i = 0; i < EXTRACTOR_CANCEL_CYCLES; ++i) {
    extract_url(mp4_url, NULL, 3, &summary);
    CHECK(summary.open_result == CRTMEDIA_OK && summary.samples == 3, "extractor delivers before release");
  }
  audit_end(&audit);

  /* 3. Reconnect: server drops mid-response twice, the read resumes, the
   * result is still byte-exact, then everything is destroyed. */
  audit_begin(&audit, "reconnect");
  for (int i = 0; i < RECONNECT_CYCLES; ++i) {
    http_test_server_options options;
    memset(&options, 0, sizeof(options));
    options.etag = "v1";
    options.truncate_connections = 2;
    options.truncate_after_bytes = mp4_size / 4;
    http_test_server* flaky = NULL;
    int flaky_port = 0;
    CHECK(http_test_server_start_ex(mp4, mp4_size, HTTP_TEST_SERVER_RANGE_CAPABLE, &options, &flaky, &flaky_port) == 0,
          "start flaky server");
    if (flaky == NULL) {
      continue;
    }
    char flaky_url[128];
    snprintf(flaky_url, sizeof(flaky_url), "http://127.0.0.1:%d/test.mp4", flaky_port);
    extract_url(flaky_url, NULL, 0, &summary);
    CHECK(summary.open_result == CRTMEDIA_OK && summary.end_result == CRTMEDIA_OK, "reconnect cycle completes");
    CHECK(summary.samples == local.samples && summary.hash == local.hash, "reconnect cycle is byte-exact");
    http_test_server_stop(flaky);
  }
  audit_end(&audit);

  /* 4. Upload sink: open, stream, cancel without finishing, destroy. The
   * receiver never drops (huge threshold) but keeps accepting, so one
   * server serves every cycle. */
  {
    http_upload_test_server* upload_server = NULL;
    int upload_port = 0;
    CHECK(http_upload_test_server_start_dropping((size_t)1 << 30, &upload_server, &upload_port) == 0,
          "start upload receiver");
    char upload_url[128];
    snprintf(upload_url, sizeof(upload_url), "http://127.0.0.1:%d/up.bin", upload_port);
    unsigned char block[8192];
    memset(block, 0x33, sizeof(block));
    audit_begin(&audit, "upload cancel");
    for (int i = 0; i < UPLOAD_CANCEL_CYCLES; ++i) {
      crtmedia_http_upload* upload = NULL;
      CHECK(crtmedia_http_upload_open(upload_url, 8192, NULL, &upload) == CRTMEDIA_OK, "open upload");
      if (upload == NULL) {
        continue;
      }
      for (int w = 0; w < 8; ++w) {
        CHECK(crtmedia_http_upload_write(upload, block, sizeof(block)) == CRTMEDIA_OK, "upload write");
      }
      crtmedia_http_upload_close(upload); /* cancel: no finish() */
    }
    audit_end(&audit);
    CHECK(http_upload_test_server_connection_count(upload_server) >= UPLOAD_CANCEL_CYCLES / 2,
          "the receiver really saw the cancelled uploads");
    http_upload_test_server_stop(upload_server);
  }

  /* 5. TLS: authenticated open + mid-stream release, and refused opens. */
  {
    tls_test_proxy* proxy = NULL;
    int tls_port = 0;
    const char* ca_pem = NULL;
    const char* other_ca_pem = NULL;
    CHECK(tls_test_proxy_start(mp4_port, TLS_TEST_SAN_LOOPBACK, &proxy, &tls_port, &ca_pem, &other_ca_pem) == 0,
          "start TLS terminator");
    char tls_url[128];
    snprintf(tls_url, sizeof(tls_url), "https://127.0.0.1:%d/test.mp4", tls_port);
    crtmedia_tls_options good;
    memset(&good, 0, sizeof(good));
    good.ca_pem = ca_pem;
    crtmedia_tls_options bad;
    memset(&bad, 0, sizeof(bad));
    bad.ca_pem = other_ca_pem;
    extract_url(tls_url, &good, 3, &summary); /* warm-up */

    audit_begin(&audit, "tls mid-stream release");
    for (int i = 0; i < TLS_CANCEL_CYCLES; ++i) {
      extract_url(tls_url, &good, 3, &summary);
      CHECK(summary.open_result == CRTMEDIA_OK && summary.samples == 3, "TLS extractor delivers before release");
    }
    audit_end(&audit);

    audit_begin(&audit, "tls refused");
    for (int i = 0; i < TLS_REFUSED_CYCLES; ++i) {
      extract_url(tls_url, &bad, 0, &summary);
      CHECK(summary.open_result == CRTMEDIA_ERROR_PROTOCOL, "refused certificate stays refused every cycle");
    }
    audit_end(&audit);
    tls_test_proxy_stop(proxy);
  }

  /* After all that churn a clean stream is still exactly the media. */
  extract_url(mp4_url, NULL, 0, &summary);
  CHECK(summary.open_result == CRTMEDIA_OK && summary.end_result == CRTMEDIA_OK && summary.samples == local.samples &&
            summary.hash == local.hash,
        "after the stress, a clean HTTP extraction is still byte-exact");

  http_test_server_stop(big_server);
  http_test_server_stop(mp4_server);
  resources end = sample_resources();
  if (start.handles >= 0 && end.handles - start.handles > HANDLE_TOLERANCE) {
    fprintf(stderr, "FAIL overall: handle/fd count grew %d -> %d\n", start.handles, end.handles);
    ++failures;
  }
  if (start.threads >= 0 && end.threads > start.threads) {
    fprintf(stderr, "FAIL overall: thread count grew %d -> %d\n", start.threads, end.threads);
    ++failures;
  }
  free(big);
  free(mp4);
  if (failures != 0) {
    fprintf(stderr, "crtmedia_http_lifecycle_test: %d failure(s)\n", failures);
    return 1;
  }
  printf(
      "crtmedia_http_lifecycle_test: ok transport=%d extractor=%d reconnect=%d upload=%d tls=%d tls_refused=%d "
      "handles=%d->%d threads=%d->%d\n",
      TRANSPORT_CANCEL_CYCLES, EXTRACTOR_CANCEL_CYCLES, RECONNECT_CYCLES, UPLOAD_CANCEL_CYCLES,
      TLS_CANCEL_CYCLES, TLS_REFUSED_CYCLES, start.handles, end.handles, start.threads, end.threads);
  return 0;
}
