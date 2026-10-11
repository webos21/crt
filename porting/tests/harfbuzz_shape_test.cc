// HarfBuzz shaping through FreeType and ICU for the harfbuzz port (porting/recipes/harfbuzz.json):
// shapes "AVA" with the bundled DejaVu Sans Mono (glyph count, advances) and checks that the ICU
// Unicode functions (what WebKit's HarfBuzz font path uses) classify an Arabic letter.
#include <stdio.h>
#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb-ft.h>
#include <hb-icu.h>
#include <hb.h>

#ifndef CRT_TEST_FONT_PATH
#error "CRT_TEST_FONT_PATH must be defined (see porting/recipes/harfbuzz.json)"
#endif

extern "C" int main() {
  FT_Library library;
  FT_Face face;
  if (FT_Init_FreeType(&library) != 0 || FT_New_Face(library, CRT_TEST_FONT_PATH, 0, &face) != 0) {
    fprintf(stderr, "harfbuzz_shape_test: FreeType could not open the font\n");
    return 1;
  }
  FT_Set_Pixel_Sizes(face, 0, 32);
  hb_font_t* font = hb_ft_font_create_referenced(face);
  hb_buffer_t* buffer = hb_buffer_create();
  hb_buffer_add_utf8(buffer, "AVA", -1, 0, -1);
  hb_buffer_guess_segment_properties(buffer);
  hb_shape(font, buffer, nullptr, 0);
  unsigned count = 0;
  hb_glyph_info_t* info = hb_buffer_get_glyph_infos(buffer, &count);
  hb_glyph_position_t* position = hb_buffer_get_glyph_positions(buffer, &count);
  if (count != 3 || info[0].codepoint == 0 || info[0].codepoint != info[2].codepoint) {
    fprintf(stderr, "harfbuzz_shape_test: unexpected shaping result (%u glyphs)\n", count);
    return 1;
  }
  for (unsigned i = 0; i < count; ++i) {
    if (position[i].x_advance <= 0) {
      fprintf(stderr, "harfbuzz_shape_test: glyph %u has no advance\n", i);
      return 1;
    }
  }
  hb_unicode_funcs_t* unicode = hb_icu_get_unicode_funcs();
  if (unicode == nullptr || hb_unicode_script(unicode, 0x0627) != HB_SCRIPT_ARABIC ||
      hb_unicode_script(unicode, 'A') != HB_SCRIPT_LATIN) {
    fprintf(stderr, "harfbuzz_shape_test: ICU unicode functions misclassify scripts\n");
    return 1;
  }
  hb_buffer_destroy(buffer);
  hb_font_destroy(font);
  FT_Done_Face(face);
  FT_Done_FreeType(library);
  printf("harfbuzz_shape_test: ok (HarfBuzz %s)\n", hb_version_string());
  return 0;
}
