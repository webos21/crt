#include <float.h>
#include <stdlib.h>

#if LDBL_MANT_DIG == 113
extern int __strtorQ(const char*, char**, int, void*);
#endif

long double strtold(const char* nptr, char** endptr) {
#if LDBL_MANT_DIG == 113
  long double result;
  __strtorQ(nptr, endptr, FLT_ROUNDS, &result);
  return result;
#else
  return (long double)strtod(nptr, endptr);
#endif
}

double atof(const char* s) {
  return strtod(s, 0);
}
