/* examples/media-stream: an SDK consumer that reads a real HTTP(S) media
 * stream through crtmedia's public API only (crtmedia/extractor.h and
 * crtmedia/tls.h) -- no curl, TLS, socket, or FFmpeg type appears here.
 *
 *   crtmedia_stream_example <url> [ca.pem]
 *
 * https:// URLs are authenticated: pass the PEM file of the CA that signed
 * the server certificate as the second argument (CRT ships no CA store).
 * Prints one summary line and exits 0 only if at least one sample arrived. */

#include <crtmedia/extractor.h>
#include <crtmedia/tls.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static char* read_text_file(const char* path) {
  FILE* file = fopen(path, "rb");
  if (file == NULL) {
    return NULL;
  }
  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);
  char* text = size >= 0 ? (char*)malloc((size_t)size + 1) : NULL;
  if (text == NULL || fread(text, 1, (size_t)size, file) != (size_t)size) {
    free(text);
    fclose(file);
    return NULL;
  }
  text[size] = '\0';
  fclose(file);
  return text;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s <url> [ca.pem]\n", argv[0]);
    return 2;
  }
  crtmedia_tls_options tls = {0};
  char* ca_pem = NULL;
  if (argc >= 3) {
    ca_pem = read_text_file(argv[2]);
    if (ca_pem == NULL) {
      fprintf(stderr, "crtmedia_stream_example: cannot read %s\n", argv[2]);
      return 2;
    }
    tls.ca_pem = ca_pem;
  }

  crtmedia_extractor* extractor = NULL;
  crtmedia_result result = crtmedia_extractor_create_from_url_with_tls(argv[1], &tls, &extractor);
  if (result != CRTMEDIA_OK) {
    fprintf(stderr, "crtmedia_stream_example: open failed (%d)\n", (int)result);
    free(ca_pem);
    return 1;
  }
  uint32_t capabilities = crtmedia_extractor_get_capabilities(extractor);
  uint32_t tracks = crtmedia_extractor_track_count(extractor);
  for (uint32_t i = 0; i < tracks; ++i) {
    crtmedia_extractor_select_track(extractor, i);
  }

  uint32_t samples = 0;
  unsigned long long bytes = 0;
  for (;;) {
    crtmedia_sample sample;
    int eof = 0;
    result = crtmedia_extractor_read_sample(extractor, &sample, &eof);
    if (result != CRTMEDIA_OK) {
      fprintf(stderr, "crtmedia_stream_example: read failed (%d) after %u samples\n", (int)result, samples);
      crtmedia_extractor_release(extractor);
      free(ca_pem);
      return 1;
    }
    if (eof) {
      break;
    }
    ++samples;
    bytes += sample.size;
    crtmedia_sample_release(&sample);
  }
  crtmedia_extractor_release(extractor);
  free(ca_pem);

  printf("crtmedia_stream_example: samples=%u bytes=%llu tracks=%u seekable=%d size_known=%d\n", samples, bytes,
         tracks, (capabilities & CRTMEDIA_SOURCE_SEEKABLE) != 0, (capabilities & CRTMEDIA_SOURCE_SIZE_KNOWN) != 0);
  return samples > 0 ? 0 : 1;
}
