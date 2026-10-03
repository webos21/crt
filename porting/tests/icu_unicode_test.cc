// Exercises the three ICU components JavaScriptCore needs -- uc (case mapping,
// normalization), i18n (collation) and data (the embedded locale/collation data) --
// through the C API, and checks real results rather than a version string.
#include <stdio.h>
#include <string.h>

#include <unicode/ucol.h>
#include <unicode/unorm2.h>
#include <unicode/ustring.h>
#include <unicode/utypes.h>

static int fail(const char* what, UErrorCode status) {
  printf("icu_unicode_test: %s failed (%s)\n", what, u_errorName(status));
  return 1;
}

int main() {
  UErrorCode status = U_ZERO_ERROR;

  // uc: full case mapping, including the length-changing sharp s.
  UChar upper[16];
  const UChar sharp_s[] = {'s', 't', 'r', 'a', 0x00DF, 'e', 0};
  int32_t upper_length = u_strToUpper(upper, 16, sharp_s, -1, "de", &status);
  if (U_FAILURE(status)) return fail("u_strToUpper", status);
  const UChar expected_upper[] = {'S', 'T', 'R', 'A', 'S', 'S', 'E', 0};
  if (upper_length != 7 || u_strcmp(upper, expected_upper) != 0) {
    printf("icu_unicode_test: wrong upper-case mapping length=%d\n", (int)upper_length);
    return 2;
  }

  // uc: NFD decomposes e-acute into e + combining acute.
  const UNormalizer2* nfd = unorm2_getNFDInstance(&status);
  if (U_FAILURE(status)) return fail("unorm2_getNFDInstance", status);
  const UChar eacute[] = {0x00E9, 0};
  UChar decomposed[8];
  int32_t decomposed_length = unorm2_normalize(nfd, eacute, -1, decomposed, 8, &status);
  if (U_FAILURE(status)) return fail("unorm2_normalize", status);
  if (decomposed_length != 2 || decomposed[0] != 'e' || decomposed[1] != 0x0301) {
    printf("icu_unicode_test: wrong NFD result length=%d\n", (int)decomposed_length);
    return 3;
  }

  // i18n + data: locale-sensitive collation. In Swedish 'z' < 'a with ring above'
  // (U+00E5), the reverse of the root order -- this needs the collation data.
  const UChar zed[] = {'z', 0};
  const UChar aring[] = {0x00E5, 0};
  UCollator* swedish = ucol_open("sv", &status);
  if (U_FAILURE(status)) return fail("ucol_open(sv)", status);
  UCollationResult swedish_order = ucol_strcoll(swedish, zed, -1, aring, -1);
  ucol_close(swedish);
  UCollator* root = ucol_open("en", &status);
  if (U_FAILURE(status)) return fail("ucol_open(en)", status);
  UCollationResult root_order = ucol_strcoll(root, zed, -1, aring, -1);
  ucol_close(root);
  if (swedish_order != UCOL_LESS || root_order != UCOL_GREATER) {
    printf("icu_unicode_test: wrong collation order sv=%d en=%d\n", (int)swedish_order,
           (int)root_order);
    return 4;
  }

  char version[U_MAX_VERSION_STRING_LENGTH];
  UVersionInfo info;
  u_getVersion(info);
  u_versionToString(info, version);
  printf("icu_unicode_test: ok icu=%s\n", version);
  return 0;
}
