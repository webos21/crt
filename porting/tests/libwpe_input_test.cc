/* libwpe's pure functions: key-symbol to Unicode and the pasteboard singleton. The library is
 * static, so the embedder supplies the loader interface; this one provides no backend objects and
 * libwpe falls back to its generic in-process pasteboard. */
#include <stdio.h>
#include <stddef.h>

#include <wpe/wpe.h>

static void* no_object(const char* object_name) {
  (void)object_name;
  return NULL;
}

extern "C" struct wpe_loader_interface _wpe_loader_interface = {no_object, NULL, NULL, NULL, NULL};

extern "C" int main() {
  struct wpe_pasteboard_string_vector types = {nullptr, 0};
  struct wpe_pasteboard* pasteboard;

  if (wpe_key_code_to_unicode(WPE_KEY_a) != 'a' || wpe_key_code_to_unicode(WPE_KEY_Z) != 'Z') {
    fprintf(stderr, "libwpe_input_test: ASCII keysyms\n");
    return 1;
  }
  if (wpe_key_code_to_unicode(WPE_KEY_eacute) != 0xE9) {
    fprintf(stderr, "libwpe_input_test: Latin-1 keysym\n");
    return 1;
  }
  pasteboard = wpe_pasteboard_get_singleton();
  if (pasteboard == NULL) {
    fprintf(stderr, "libwpe_input_test: no pasteboard\n");
    return 1;
  }
  wpe_pasteboard_get_types(pasteboard, &types);
  printf("libwpe_input_test: ok\n");
  return 0;
}
