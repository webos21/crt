/* libwebp round trip for the libwebp port (porting/recipes/libwebp.json): a lossless encode and
 * decode of a 32x24 RGBA image must be bit exact, and the demux library (WebKit's WebP decoder
 * links libwebpdemux) must report the canvas size from the same bitstream. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <webp/decode.h>
#include <webp/demux.h>
#include <webp/encode.h>

enum { W = 32, H = 24 };

int main(void) {
  uint8_t* rgba = malloc((size_t)W * H * 4);
  uint8_t* encoded = NULL;
  uint8_t* decoded;
  size_t encoded_size;
  int width = 0, height = 0, x, y;
  WebPData data;
  WebPDemuxer* demux;

  for (y = 0; y < H; ++y) {
    for (x = 0; x < W; ++x) {
      uint8_t* p = rgba + ((size_t)y * W + x) * 4;
      p[0] = (uint8_t)(x * 8);
      p[1] = (uint8_t)(y * 10);
      p[2] = (uint8_t)((x ^ y) * 9);
      p[3] = (uint8_t)(x < 16 ? 255 : 128);
    }
  }
  encoded_size = WebPEncodeLosslessRGBA(rgba, W, H, W * 4, &encoded);
  if (encoded_size == 0 || encoded == NULL) {
    fprintf(stderr, "libwebp_roundtrip_test: encode failed\n");
    return 1;
  }
  decoded = WebPDecodeRGBA(encoded, encoded_size, &width, &height);
  if (decoded == NULL || width != W || height != H) {
    fprintf(stderr, "libwebp_roundtrip_test: decode failed (%d x %d)\n", width, height);
    return 1;
  }
  if (memcmp(decoded, rgba, (size_t)W * H * 4) != 0) {
    fprintf(stderr, "libwebp_roundtrip_test: lossless round trip is not bit exact\n");
    return 1;
  }
  data.bytes = encoded;
  data.size = encoded_size;
  demux = WebPDemux(&data);
  if (demux == NULL || WebPDemuxGetI(demux, WEBP_FF_CANVAS_WIDTH) != W ||
      WebPDemuxGetI(demux, WEBP_FF_CANVAS_HEIGHT) != H || WebPDemuxGetI(demux, WEBP_FF_FRAME_COUNT) != 1) {
    fprintf(stderr, "libwebp_roundtrip_test: demux disagrees\n");
    return 1;
  }
  WebPDemuxDelete(demux);
  WebPFree(decoded);
  WebPFree(encoded);
  printf("libwebp_roundtrip_test: ok (%zu bytes, decoder %x)\n", encoded_size, WebPGetDecoderVersion());
  return 0;
}
