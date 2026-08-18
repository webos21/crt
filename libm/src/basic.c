#include <float.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

double fabs(double x) {
  return __builtin_fabs(x);
}

float fabsf(float x) {
  return __builtin_fabsf(x);
}

long double fabsl(long double x) {
  return __builtin_fabsl(x);
}

double copysign(double x, double y) {
  return __builtin_copysign(x, y);
}

float copysignf(float x, float y) {
  return __builtin_copysignf(x, y);
}

long double copysignl(long double x, long double y) {
  return __builtin_copysignl(x, y);
}

double fma(double x, double y, double z) {
  return __builtin_fma(x, y, z);
}

float fmaf(float x, float y, float z) {
  return __builtin_fmaf(x, y, z);
}

long double fmal(long double x, long double y, long double z) {
  return __builtin_fmal(x, y, z);
}

float fminf(float x, float y) {
  if (isnan(x)) return y;
  if (isnan(y)) return x;
  if (x == 0.0f && y == 0.0f) {
    return (signbit(x) || signbit(y)) ? -0.0f : 0.0f;
  }
  return x < y ? x : y;
}

long double fminl(long double x, long double y) {
  if (isnan(x)) return y;
  if (isnan(y)) return x;
  if (x == 0.0L && y == 0.0L) {
    return (signbit(x) || signbit(y)) ? -0.0L : 0.0L;
  }
  return x < y ? x : y;
}

float fmaxf(float x, float y) {
  if (isnan(x)) return y;
  if (isnan(y)) return x;
  if (x == 0.0f && y == 0.0f) {
    return (signbit(x) && signbit(y)) ? -0.0f : 0.0f;
  }
  return x > y ? x : y;
}

long double fmaxl(long double x, long double y) {
  if (isnan(x)) return y;
  if (isnan(y)) return x;
  if (x == 0.0L && y == 0.0L) {
    return (signbit(x) && signbit(y)) ? -0.0L : 0.0L;
  }
  return x > y ? x : y;
}

float truncf(float x) {
  return (float)trunc((double)x);
}

float floorf(float x) {
  return (float)floor((double)x);
}

float ceilf(float x) {
  return (float)ceil((double)x);
}

float roundf(float x) {
  return (float)round((double)x);
}

double nextafter(double x, double y) {
  uint64_t bits;

  if (isnan(x) || isnan(y)) return x + y;
  if (x == y) return y;
  if (x == 0.0) return copysign(0x1p-1074, y);

  memcpy(&bits, &x, sizeof(bits));
  if ((x > 0.0) == (x < y)) {
    ++bits;
  } else {
    --bits;
  }
  memcpy(&x, &bits, sizeof(x));
  return x;
}

float nextafterf(float x, float y) {
  uint32_t bits;

  if (isnan(x) || isnan(y)) return x + y;
  if (x == y) return y;
  if (x == 0.0f) return copysignf(0x1p-149f, y);

  memcpy(&bits, &x, sizeof(bits));
  if ((x > 0.0f) == (x < y)) {
    ++bits;
  } else {
    --bits;
  }
  memcpy(&x, &bits, sizeof(x));
  return x;
}

long double nextafterl(long double x, long double y) {
  return (long double)nextafter((double)x, (double)y);
}

long lrint(double x) {
  return __builtin_lrint(x);
}

long lrintf(float x) {
  return __builtin_lrintf(x);
}

long lrintl(long double x) {
  return __builtin_lrintl(x);
}

long lround(double x) {
  return __builtin_lround(x);
}

long lroundf(float x) {
  return __builtin_lroundf(x);
}

long lroundl(long double x) {
  return __builtin_lroundl(x);
}

long long llround(double x) {
  return __builtin_llround(x);
}

long long llroundf(float x) {
  return __builtin_llroundf(x);
}

long long llroundl(long double x) {
  return __builtin_llroundl(x);
}

double asin(double x) {
  if (isnan(x)) return x;
  if (x > 1.0 || x < -1.0) return NAN;
  if (x == 1.0) return atan2(1.0, 0.0);
  if (x == -1.0) return -atan2(1.0, 0.0);
  return atan2(x, sqrt(1.0 - x * x));
}

float asinf(float x) {
  return (float)asin((double)x);
}

long double asinl(long double x) {
  if (isnan(x)) return x;
  if (x > 1.0L || x < -1.0L) return NAN;
  if (x == 1.0L) return atan2l(1.0L, 0.0L);
  if (x == -1.0L) return -atan2l(1.0L, 0.0L);
  return atan2l(x, sqrtl(1.0L - x * x));
}

double acos(double x) {
  if (isnan(x)) return x;
  if (x > 1.0 || x < -1.0) return NAN;
  return atan2(sqrt(1.0 - x * x), x);
}

float acosf(float x) {
  return (float)acos((double)x);
}

long double acosl(long double x) {
  if (isnan(x)) return x;
  if (x > 1.0L || x < -1.0L) return NAN;
  return atan2l(sqrtl(1.0L - x * x), x);
}

double sqrt(double x) {
  return __builtin_elementwise_sqrt(x);
}

float sqrtf(float x) {
  return __builtin_elementwise_sqrt(x);
}

double cbrt(double x) {
  if (isnan(x) || isinf(x) || x == 0.0) return x;
  return x < 0.0 ? -pow(-x, 1.0 / 3.0) : pow(x, 1.0 / 3.0);
}

float cbrtf(float x) {
  return (float)cbrt((double)x);
}

long double cbrtl(long double x) {
  if (isnan(x) || isinf(x) || x == 0.0L) return x;
  return x < 0.0L ? -powl(-x, 1.0L / 3.0L) : powl(x, 1.0L / 3.0L);
}
