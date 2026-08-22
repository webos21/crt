#include <inttypes.h>
#include <stdlib.h>
#include <wchar.h>

intmax_t strtoimax(const char* nptr, char** endptr, int base) {
  return (intmax_t)strtoll(nptr, endptr, base);
}

uintmax_t strtoumax(const char* nptr, char** endptr, int base) {
  return (uintmax_t)strtoull(nptr, endptr, base);
}

intmax_t wcstoimax(const wchar_t* nptr, wchar_t** endptr, int base) {
  return (intmax_t)wcstoll(nptr, endptr, base);
}

uintmax_t wcstoumax(const wchar_t* nptr, wchar_t** endptr, int base) {
  return (uintmax_t)wcstoull(nptr, endptr, base);
}

intmax_t imaxabs(intmax_t j) {
  return j < 0 ? -j : j;
}

/* intmax_t is `long` on LP64 hosts (Linux/macOS) but `long long` on this
 * project's Windows target (LLP64, confirmed via a real compiler probe:
 * `clang --target=x86_64-w64-mingw32 -dM -E` reports `__INTMAX_TYPE__`
 * as `long long int`, vs plain `long int` on Linux) -- so this is
 * implemented directly in terms of intmax_t's own division/modulo
 * (defined by C99 as truncating toward zero, exactly matching div_t/
 * ldiv_t/lldiv_t's own documented semantics) rather than delegating to
 * ldiv()/lldiv() based on an assumption about which one intmax_t
 * actually is on a given host. */
imaxdiv_t imaxdiv(intmax_t numer, intmax_t denom) {
  imaxdiv_t result;

  result.quot = numer / denom;
  result.rem = numer % denom;
  return result;
}
