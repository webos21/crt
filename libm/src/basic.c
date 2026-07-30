#include <math.h>

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

double sqrt(double x) {
  return __builtin_elementwise_sqrt(x);
}

float sqrtf(float x) {
  return __builtin_elementwise_sqrt(x);
}
