#include <stdlib.h>

div_t div(int numerator, int denominator) {
  div_t result;
  result.quot = numerator / denominator;
  result.rem = numerator % denominator;
  return result;
}

ldiv_t ldiv(long numerator, long denominator) {
  ldiv_t result;
  result.quot = numerator / denominator;
  result.rem = numerator % denominator;
  return result;
}

lldiv_t lldiv(long long numerator, long long denominator) {
  lldiv_t result;
  result.quot = numerator / denominator;
  result.rem = numerator % denominator;
  return result;
}
