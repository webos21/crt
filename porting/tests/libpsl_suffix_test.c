/* libpsl public-suffix lookups for the libpsl port (porting/recipes/libpsl.json): the built-in
 * data, ASCII and (through ICU) an internationalised name. WebKit's curl backend asks libpsl for
 * the registrable domain when it stores a cookie. */
#include <stdio.h>
#include <string.h>

#include <libpsl.h>

int main(void) {
  const psl_ctx_t* psl = psl_builtin();
  const char* domain;
  char* lower = NULL;

  if (psl == NULL) {
    fprintf(stderr, "libpsl_suffix_test: no built-in suffix list\n");
    return 1;
  }
  if (!psl_is_public_suffix(psl, "co.uk") || !psl_is_public_suffix(psl, "com") || psl_is_public_suffix(psl, "example.com")) {
    fprintf(stderr, "libpsl_suffix_test: wrong public-suffix answers\n");
    return 1;
  }
  domain = psl_registrable_domain(psl, "www.example.co.uk");
  if (domain == NULL || strcmp(domain, "example.co.uk") != 0) {
    fprintf(stderr, "libpsl_suffix_test: registrable domain is %s\n", domain ? domain : "(null)");
    return 1;
  }
  /* An internationalised name needs the IDNA library libpsl was built with (ICU here). */
  if (psl_str_to_utf8lower("WWW.EXAMPLE.COM", NULL, NULL, &lower) != PSL_SUCCESS || strcmp(lower, "www.example.com") != 0) {
    fprintf(stderr, "libpsl_suffix_test: lower-casing failed\n");
    return 1;
  }
  psl_free_string(lower);
  if (!psl_is_public_suffix(psl, "xn--55qx5d.cn")) { /* 公司.cn */
    fprintf(stderr, "libpsl_suffix_test: the IDNA suffix 公司.cn was not found\n");
    return 1;
  }
  printf("libpsl_suffix_test: ok (libpsl %s, list of %s)\n", psl_get_version(), psl_dist_filename() ? "dist" : "builtin");
  return 0;
}
