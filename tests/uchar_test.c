#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <uchar.h>

static int fail(const char* message) {
  fprintf(stderr, "uchar_test: %s\n", message);
  return 1;
}

int main(void) {
  mbstate_t state;
  char16_t c16;
  char32_t c32;
  char buf[8];
  size_t result;
  const char* euro_utf8 = "\xe2\x82\xac"; /* U+20AC EURO SIGN */
  const char* astral_utf8 = "\xf0\x9f\x98\x80"; /* U+1F600, needs a UTF-16 surrogate pair */

  /* --- mbrtoc32 / c32rtomb: direct UTF-8 <-> UTF-32 round trip --- */
  memset(&state, 0, sizeof(state));
  if (mbrtoc32(&c32, "A", 1, &state) != 1 || c32 != (char32_t)'A') {
    return fail("mbrtoc32 ascii");
  }
  memset(&state, 0, sizeof(state));
  if (mbrtoc32(&c32, euro_utf8, 3, &state) != 3 || c32 != (char32_t)0x20ac) {
    return fail("mbrtoc32 bmp");
  }
  memset(&state, 0, sizeof(state));
  if (mbrtoc32(&c32, astral_utf8, 4, &state) != 4 || c32 != (char32_t)0x1f600) {
    return fail("mbrtoc32 astral");
  }
  memset(&state, 0, sizeof(state));
  if (mbrtoc32(&c32, "", 1, &state) != 0 || c32 != 0) {
    return fail("mbrtoc32 nul");
  }
  errno = 0;
  memset(&state, 0, sizeof(state));
  if (mbrtoc32(&c32, "\xff", 1, &state) != (size_t)-1 || errno != EILSEQ) {
    return fail("mbrtoc32 invalid");
  }
  memset(&state, 0, sizeof(state));
  if (mbrtoc32(0, euro_utf8, 2, &state) != (size_t)-2) {
    return fail("mbrtoc32 incomplete");
  }

  memset(&state, 0, sizeof(state));
  memset(buf, 0, sizeof(buf));
  if (c32rtomb(buf, (char32_t)0x20ac, &state) != 3 || strcmp(buf, euro_utf8) != 0) {
    return fail("c32rtomb bmp");
  }
  memset(&state, 0, sizeof(state));
  memset(buf, 0, sizeof(buf));
  if (c32rtomb(buf, (char32_t)0x1f600, &state) != 4 || strcmp(buf, astral_utf8) != 0) {
    return fail("c32rtomb astral");
  }

  /* --- mbrtoc16: BMP codepoints decode in one call, astral codepoints
   * split into a surrogate pair across two calls (the second consumes
   * zero more input bytes and returns (size_t)-3). --- */
  memset(&state, 0, sizeof(state));
  if (mbrtoc16(&c16, "A", 1, &state) != 1 || c16 != (char16_t)'A') {
    return fail("mbrtoc16 ascii");
  }
  memset(&state, 0, sizeof(state));
  if (mbrtoc16(&c16, euro_utf8, 3, &state) != 3 || c16 != (char16_t)0x20ac) {
    return fail("mbrtoc16 bmp");
  }
  memset(&state, 0, sizeof(state));
  result = mbrtoc16(&c16, astral_utf8, 4, &state);
  if (result != 4 || c16 != (char16_t)0xd83d) {
    return fail("mbrtoc16 astral high surrogate");
  }
  result = mbrtoc16(&c16, "", 0, &state);
  if (result != (size_t)-3 || c16 != (char16_t)0xde00) {
    return fail("mbrtoc16 astral low surrogate");
  }

  /* --- c16rtomb: the inverse -- a high surrogate alone produces no
   * output bytes yet; the following low surrogate completes it. --- */
  memset(&state, 0, sizeof(state));
  memset(buf, 0, sizeof(buf));
  if (c16rtomb(buf, (char16_t)'A', &state) != 1 || buf[0] != 'A') {
    return fail("c16rtomb ascii");
  }
  memset(&state, 0, sizeof(state));
  memset(buf, 0, sizeof(buf));
  if (c16rtomb(buf, (char16_t)0x20ac, &state) != 3 || strcmp(buf, euro_utf8) != 0) {
    return fail("c16rtomb bmp");
  }
  memset(&state, 0, sizeof(state));
  memset(buf, 0, sizeof(buf));
  if (c16rtomb(buf, (char16_t)0xd83d, &state) != 0) {
    return fail("c16rtomb high surrogate pending");
  }
  if (c16rtomb(buf, (char16_t)0xde00, &state) != 4 || strcmp(buf, astral_utf8) != 0) {
    return fail("c16rtomb low surrogate completes");
  }

  /* A lone low surrogate (no preceding high one) is invalid. */
  memset(&state, 0, sizeof(state));
  errno = 0;
  if (c16rtomb(buf, (char16_t)0xde00, &state) != (size_t)-1 || errno != EILSEQ) {
    return fail("c16rtomb lone low surrogate");
  }

  printf("uchar_test: ok\n");
  return 0;
}
