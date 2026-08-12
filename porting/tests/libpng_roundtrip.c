#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include <png.h>

struct mem_io {
  unsigned char data[4096];
  size_t size;
  size_t offset;
};

static unsigned char image[4][4] = {
  { 255, 0, 0, 255 },
  { 0, 255, 0, 255 },
  { 0, 0, 255, 255 },
  { 255, 255, 255, 255 },
};

static void png_write_mem(png_structp png_ptr, png_bytep data, png_size_t length) {
  struct mem_io* io = (struct mem_io*)png_get_io_ptr(png_ptr);
  if (length > sizeof(io->data) || io->size > sizeof(io->data) - length) {
    png_error(png_ptr, "write overflow");
  }
  memcpy(io->data + io->size, data, length);
  io->size += length;
}

static void png_flush_mem(png_structp png_ptr) {
  (void)png_ptr;
}

static void png_read_mem(png_structp png_ptr, png_bytep data, png_size_t length) {
  struct mem_io* io = (struct mem_io*)png_get_io_ptr(png_ptr);
  if (length > io->size || io->offset > io->size - length) {
    png_error(png_ptr, "read overflow");
  }
  memcpy(data, io->data + io->offset, length);
  io->offset += length;
}

int main(void) {
  struct mem_io io;
  memset(&io, 0, sizeof(io));

  png_structp writer = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (writer == NULL) {
    printf("libpng_roundtrip_test: create writer failed\n");
    return 1;
  }
  png_infop write_info = png_create_info_struct(writer);
  if (write_info == NULL) {
    png_destroy_write_struct(&writer, NULL);
    printf("libpng_roundtrip_test: create write info failed\n");
    return 1;
  }
  if (setjmp(png_jmpbuf(writer))) {
    png_destroy_write_struct(&writer, &write_info);
    printf("libpng_roundtrip_test: write failed\n");
    return 1;
  }
  png_set_write_fn(writer, &io, png_write_mem, png_flush_mem);
  png_set_IHDR(writer, write_info, 2, 2, 8, PNG_COLOR_TYPE_RGBA,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
  png_write_info(writer, write_info);
  png_bytep rows[2] = { image[0], image[2] };
  png_write_image(writer, rows);
  png_write_end(writer, write_info);
  png_destroy_write_struct(&writer, &write_info);

  unsigned char decoded[4][4];
  memset(decoded, 0, sizeof(decoded));
  png_structp reader = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (reader == NULL) {
    printf("libpng_roundtrip_test: create reader failed\n");
    return 1;
  }
  png_infop read_info = png_create_info_struct(reader);
  if (read_info == NULL) {
    png_destroy_read_struct(&reader, NULL, NULL);
    printf("libpng_roundtrip_test: create read info failed\n");
    return 1;
  }
  if (setjmp(png_jmpbuf(reader))) {
    png_destroy_read_struct(&reader, &read_info, NULL);
    printf("libpng_roundtrip_test: read failed\n");
    return 1;
  }
  io.offset = 0;
  png_set_read_fn(reader, &io, png_read_mem);
  png_read_info(reader, read_info);
  if (png_get_image_width(reader, read_info) != 2 ||
      png_get_image_height(reader, read_info) != 2 ||
      png_get_color_type(reader, read_info) != PNG_COLOR_TYPE_RGBA ||
      png_get_bit_depth(reader, read_info) != 8) {
    png_destroy_read_struct(&reader, &read_info, NULL);
    printf("libpng_roundtrip_test: header mismatch\n");
    return 1;
  }
  png_bytep read_rows[2] = { decoded[0], decoded[2] };
  png_read_image(reader, read_rows);
  png_read_end(reader, read_info);
  png_destroy_read_struct(&reader, &read_info, NULL);

  if (memcmp(image, decoded, sizeof(image)) != 0) {
    printf("libpng_roundtrip_test: data mismatch\n");
    return 1;
  }

  printf("libpng_roundtrip_test: ok size=%zu version=%lu\n",
         io.size, (unsigned long)png_access_version_number());
  return 0;
}
