/* libjpeg-turbo round trip for the libjpeg-turbo port (porting/recipes/libjpeg-turbo.json).
 *
 * Compresses a 64x48 gradient in memory (jpeg_mem_dest), decompresses it with the BGRA output
 * extension WebKit's JPEG decoder asks for when JCS_ALPHA_EXTENSIONS is defined, and checks the
 * size, the alpha channel and that the gradient survived (lossy, so within a tolerance). */
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jpeglib.h>

enum { W = 64, H = 48 };

static jmp_buf failure;
static void on_error(j_common_ptr info) {
  char message[JMSG_LENGTH_MAX];
  (*info->err->format_message)(info, message);
  fprintf(stderr, "libjpeg_roundtrip_test: libjpeg error: %s\n", message);
  longjmp(failure, 1);
}

int main(void) {
  unsigned char* source = malloc((size_t)W * H * 3);
  unsigned char* encoded = NULL;
  unsigned long encoded_size = 0;
  unsigned char* decoded = NULL;
  struct jpeg_compress_struct compress;
  struct jpeg_decompress_struct decompress;
  struct jpeg_error_mgr error;
  int x, y;

  for (y = 0; y < H; ++y) {
    for (x = 0; x < W; ++x) {
      unsigned char* p = source + ((size_t)y * W + x) * 3;
      p[0] = (unsigned char)(x * 4);       /* R */
      p[1] = (unsigned char)(y * 5);       /* G */
      p[2] = 128;                          /* B */
    }
  }
  jpeg_std_error(&error);
  error.error_exit = on_error;
  if (setjmp(failure)) {
    return 1;
  }
  compress.err = &error;
  jpeg_create_compress(&compress);
  jpeg_mem_dest(&compress, &encoded, &encoded_size);
  compress.image_width = W;
  compress.image_height = H;
  compress.input_components = 3;
  compress.in_color_space = JCS_RGB;
  jpeg_set_defaults(&compress);
  jpeg_set_quality(&compress, 95, TRUE);
  jpeg_start_compress(&compress, TRUE);
  while (compress.next_scanline < compress.image_height) {
    JSAMPROW row = source + (size_t)compress.next_scanline * W * 3;
    jpeg_write_scanlines(&compress, &row, 1);
  }
  jpeg_finish_compress(&compress);
  jpeg_destroy_compress(&compress);
  if (encoded_size < 100 || encoded[0] != 0xFF || encoded[1] != 0xD8) {
    fprintf(stderr, "libjpeg_roundtrip_test: not a JPEG stream (%lu bytes)\n", encoded_size);
    return 1;
  }

  decompress.err = &error;
  jpeg_create_decompress(&decompress);
  jpeg_mem_src(&decompress, encoded, encoded_size);
  jpeg_read_header(&decompress, TRUE);
#if defined(JCS_ALPHA_EXTENSIONS)
  decompress.out_color_space = JCS_EXT_BGRA;
#else
  fprintf(stderr, "libjpeg_roundtrip_test: JCS_ALPHA_EXTENSIONS missing (not libjpeg-turbo?)\n");
  return 1;
#endif
  jpeg_start_decompress(&decompress);
  if (decompress.output_width != W || decompress.output_height != H || decompress.output_components != 4) {
    fprintf(stderr, "libjpeg_roundtrip_test: unexpected output %ux%u x%d\n", decompress.output_width,
            decompress.output_height, decompress.output_components);
    return 1;
  }
  decoded = malloc((size_t)W * H * 4);
  while (decompress.output_scanline < decompress.output_height) {
    JSAMPROW row = decoded + (size_t)decompress.output_scanline * W * 4;
    jpeg_read_scanlines(&decompress, &row, 1);
  }
  jpeg_finish_decompress(&decompress);
  jpeg_destroy_decompress(&decompress);

  for (y = 4; y < H - 4; y += 8) {
    for (x = 4; x < W - 4; x += 8) {
      const unsigned char* got = decoded + ((size_t)y * W + x) * 4;        /* B G R A */
      const unsigned char* want = source + ((size_t)y * W + x) * 3;        /* R G B */
      if (got[3] != 255 || abs(got[2] - want[0]) > 12 || abs(got[1] - want[1]) > 12 || abs(got[0] - want[2]) > 12) {
        fprintf(stderr, "libjpeg_roundtrip_test: pixel (%d,%d) is B%d G%d R%d A%d, want R%d G%d B%d\n", x, y, got[0],
                got[1], got[2], got[3], want[0], want[1], want[2]);
        return 1;
      }
    }
  }
  printf("libjpeg_roundtrip_test: ok (%lu bytes, libjpeg %d)\n", encoded_size, JPEG_LIB_VERSION);
  return 0;
}
