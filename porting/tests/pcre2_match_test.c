/* Real pcre2_compile()/pcre2_match() round trip against the CRT sysroot,
 * matching the rigor already established for zlib/bzip2/xz's own
 * porting/tests -- exercise a real feature (capture groups), not just
 * "the library loaded". */

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <stdio.h>
#include <string.h>

static const char pattern[] = "(\\w+)@(\\w+)\\.(\\w+)";
static const char subject[] = "contact test@example.com for details";
static const char expected_user[] = "test";
static const char expected_host[] = "example";
static const char expected_tld[] = "com";

static int check_group(pcre2_match_data* match_data, PCRE2_SIZE* ovector, int group, const char* expected) {
  PCRE2_SIZE start = ovector[2 * group];
  PCRE2_SIZE end = ovector[2 * group + 1];
  size_t len = (size_t)(end - start);
  size_t expected_len = strlen(expected);

  (void)match_data;
  if (len != expected_len || memcmp(subject + start, expected, expected_len) != 0) {
    printf("pcre2_match_test: group %d mismatch (got %.*s, want %s)\n", group, (int)len, subject + start, expected);
    return 1;
  }
  return 0;
}

int main(void) {
  int errorcode;
  PCRE2_SIZE erroroffset;
  pcre2_code* re;
  pcre2_match_data* match_data;
  int rc;
  PCRE2_SIZE* ovector;
  char version[64];
  int version_len;

  re = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED, 0, &errorcode, &erroroffset, NULL);
  if (re == NULL) {
    PCRE2_UCHAR buf[256];
    pcre2_get_error_message(errorcode, buf, sizeof(buf));
    printf("pcre2_match_test: compile failed at offset %zu: %s\n", (size_t)erroroffset, (char*)buf);
    return 1;
  }

  match_data = pcre2_match_data_create_from_pattern(re, NULL);
  if (match_data == NULL) {
    printf("pcre2_match_test: match_data_create failed\n");
    pcre2_code_free(re);
    return 1;
  }

  rc = pcre2_match(re, (PCRE2_SPTR)subject, strlen(subject), 0, 0, match_data, NULL);
  if (rc < 0) {
    printf("pcre2_match_test: match failed (%d)\n", rc);
    pcre2_match_data_free(match_data);
    pcre2_code_free(re);
    return 1;
  }
  if (rc != 4) {
    /* whole match + 3 capture groups */
    printf("pcre2_match_test: unexpected capture count (%d)\n", rc);
    pcre2_match_data_free(match_data);
    pcre2_code_free(re);
    return 1;
  }

  ovector = pcre2_get_ovector_pointer(match_data);
  if (check_group(match_data, ovector, 1, expected_user) || check_group(match_data, ovector, 2, expected_host) ||
      check_group(match_data, ovector, 3, expected_tld)) {
    pcre2_match_data_free(match_data);
    pcre2_code_free(re);
    return 1;
  }

  pcre2_match_data_free(match_data);
  pcre2_code_free(re);

  version_len = pcre2_config(PCRE2_CONFIG_VERSION, version);
  if (version_len < 0) {
    printf("pcre2_match_test: config query failed (%d)\n", version_len);
    return 1;
  }

  printf("pcre2_match_test: ok matches=%d version=%s\n", rc, version);
  return 0;
}
