/*
 * freetype_glyph_test.c -- real glyph-rasterization round trip for the
 * freetype port (porting/recipes/freetype.json).
 *
 * Not just "did the API calls return success codes": this loads the real
 * bundled DejaVuSansMono.ttf (libcrtgfx/assets/fonts/, see that directory's
 * own README.md for provenance), rasterizes the glyph for 'A' at a fixed
 * pixel size, and checks the resulting FT_Bitmap has plausible non-zero
 * dimensions AND at least one genuinely non-zero (inked) byte in its
 * buffer -- a real font with a real 'A' glyph cannot rasterize to an
 * all-zero bitmap at 32px, so this is a meaningful correctness check, not
 * just a smoke test that the library links.
 *
 * CRT_TEST_FONT_PATH is supplied by porting/recipes/freetype.json's own
 * test cflags (an absolute path to the bundled .ttf), so this test does
 * not depend on any host font directory.
 */

#include <ft2build.h>
#include FT_FREETYPE_H

#include <stdio.h>
#include <string.h>

#ifndef CRT_TEST_FONT_PATH
#error "CRT_TEST_FONT_PATH must be defined (see porting/recipes/freetype.json)"
#endif

int main(void) {
    FT_Library library;
    FT_Error err = FT_Init_FreeType(&library);
    if (err != 0) {
        fprintf(stderr, "freetype_glyph_test: FT_Init_FreeType failed: %d\n", (int)err);
        return 1;
    }

    FT_Face face;
    err = FT_New_Face(library, CRT_TEST_FONT_PATH, 0, &face);
    if (err != 0) {
        fprintf(stderr, "freetype_glyph_test: FT_New_Face failed: %d\n", (int)err);
        FT_Done_FreeType(library);
        return 1;
    }

    err = FT_Set_Pixel_Sizes(face, 0, 32);
    if (err != 0) {
        fprintf(stderr, "freetype_glyph_test: FT_Set_Pixel_Sizes failed: %d\n", (int)err);
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        return 1;
    }

    /* 'A' (U+0041) -- rasterize directly via FT_LOAD_RENDER, no separate
     * FT_Render_Glyph() call needed. */
    err = FT_Load_Char(face, 'A', FT_LOAD_RENDER);
    if (err != 0) {
        fprintf(stderr, "freetype_glyph_test: FT_Load_Char failed: %d\n", (int)err);
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        return 1;
    }

    FT_Bitmap *bitmap = &face->glyph->bitmap;

    if (bitmap->rows == 0 || bitmap->width == 0) {
        fprintf(stderr,
                "freetype_glyph_test: implausible bitmap dimensions "
                "(rows=%u width=%u)\n",
                bitmap->rows, bitmap->width);
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        return 1;
    }

    /* Sane upper bound: at 32px pixel size, no real glyph should rasterize
     * to a bitmap anywhere near, say, 256 in either dimension. Catches a
     * garbage/uninitialized bitmap struct rather than a real render. */
    if (bitmap->rows > 256 || bitmap->width > 256) {
        fprintf(stderr,
                "freetype_glyph_test: implausibly large bitmap dimensions "
                "(rows=%u width=%u)\n",
                bitmap->rows, bitmap->width);
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        return 1;
    }

    int found_ink = 0;
    if (bitmap->buffer != NULL) {
        /* pitch may be negative for bottom-up bitmaps; use its absolute
         * value as the row stride and scan every row's own width bytes
         * (8-bit grayscale antialiased bitmap_mode is FreeType's default
         * render target for FT_LOAD_RENDER, so one byte per pixel). */
        int pitch = bitmap->pitch;
        unsigned int stride = (pitch < 0) ? (unsigned int)(-pitch) : (unsigned int)pitch;
        for (unsigned int row = 0; row < bitmap->rows && !found_ink; row++) {
            const unsigned char *row_ptr = bitmap->buffer + (size_t)row * stride;
            for (unsigned int col = 0; col < bitmap->width; col++) {
                if (row_ptr[col] != 0) {
                    found_ink = 1;
                    break;
                }
            }
        }
    }

    /* Capture before FT_Done_Face(), which frees face->glyph (and with it
     * the bitmap this pointer refers to) -- reading through `bitmap` after
     * that call would be a use-after-free. */
    unsigned int bitmap_rows = bitmap->rows;
    unsigned int bitmap_width = bitmap->width;

    FT_Done_Face(face);
    FT_Done_FreeType(library);

    if (!found_ink) {
        fprintf(stderr,
                "freetype_glyph_test: rasterized 'A' bitmap (rows=%u width=%u) "
                "has no non-zero (inked) pixels\n",
                bitmap_rows, bitmap_width);
        return 1;
    }

    printf("freetype_glyph_test: ok\n");
    return 0;
}
