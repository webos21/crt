/* fontconfig match for the fontconfig port (porting/recipes/fontconfig.json): an in-memory FcConfig
 * that scans one directory, then a family match and a character-coverage query. */
#include <stdio.h>
#include <string.h>

#include <fontconfig/fontconfig.h>

#ifndef CRT_TEST_FONT_DIR
#error "CRT_TEST_FONT_DIR must name the bundled font directory"
#endif

int main(void) {
  static const char config_text[] =
      "<?xml version=\"1.0\"?>\n"
      "<!DOCTYPE fontconfig SYSTEM \"urn:fontconfig:fonts.dtd\">\n"
      "<fontconfig><dir>" CRT_TEST_FONT_DIR "</dir><cachedir>/tmp/crt-fontconfig-test-cache</cachedir></fontconfig>\n";
  FcConfig* config = FcConfigCreate();
  FcPattern* pattern;
  FcPattern* match;
  FcResult result;
  FcChar8* file = NULL;
  FcChar8* family = NULL;
  FcCharSet* charset = NULL;

  if (config == NULL || !FcConfigParseAndLoadFromMemory(config, (const FcChar8*)config_text, FcTrue)) {
    fprintf(stderr, "fontconfig_match_test: configuration did not load\n");
    return 1;
  }
  if (!FcConfigBuildFonts(config)) {
    fprintf(stderr, "fontconfig_match_test: font scan failed\n");
    return 1;
  }
  if (FcConfigGetFonts(config, FcSetSystem) == NULL || FcConfigGetFonts(config, FcSetSystem)->nfont < 2) {
    fprintf(stderr, "fontconfig_match_test: fewer than two fonts found in %s\n", CRT_TEST_FONT_DIR);
    return 1;
  }

  pattern = FcNameParse((const FcChar8*)"DejaVu Sans Mono");
  FcConfigSubstitute(config, pattern, FcMatchPattern);
  FcDefaultSubstitute(pattern);
  match = FcFontMatch(config, pattern, &result);
  if (match == NULL || FcPatternGetString(match, FC_FILE, 0, &file) != FcResultMatch ||
      FcPatternGetString(match, FC_FAMILY, 0, &family) != FcResultMatch) {
    fprintf(stderr, "fontconfig_match_test: no match\n");
    return 1;
  }
  if (strstr((const char*)file, "DejaVuSansMono.ttf") == NULL) {
    fprintf(stderr, "fontconfig_match_test: matched %s (%s)\n", file, family);
    return 1;
  }
  if (FcPatternGetCharSet(match, FC_CHARSET, 0, &charset) != FcResultMatch || !FcCharSetHasChar(charset, 'A')) {
    fprintf(stderr, "fontconfig_match_test: coverage missing\n");
    return 1;
  }
  FcPatternDestroy(match);
  FcPatternDestroy(pattern);
  FcConfigDestroy(config);
  printf("fontconfig_match_test: ok\n");
  return 0;
}
