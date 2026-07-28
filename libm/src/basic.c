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
  return __builtin_fminf(x, y);
}

long double fminl(long double x, long double y) {
  if (isnan(x)) return y;
  if (isnan(y)) return x;
  return x < y ? x : y;
}

float fmaxf(float x, float y) {
  return __builtin_fmaxf(x, y);
}

long double fmaxl(long double x, long double y) {
  if (isnan(x)) return y;
  if (isnan(y)) return x;
  return x > y ? x : y;
}

float truncf(float x) {
  return (float)trunc((double)x);
}

long double truncl(long double x) {
  if (!isfinite(x) || x == 0.0L) {
    return x;
  }
  return (long double)(long long)x;
}

float floorf(float x) {
  return (float)floor((double)x);
}

long double floorl(long double x) {
  long double t;
  if (!isfinite(x) || x == 0.0L) {
    return x;
  }
  t = truncl(x);
  if (t > x) {
    t -= 1.0L;
  }
  return t;
}

float ceilf(float x) {
  return (float)ceil((double)x);
}

long double ceill(long double x) {
  long double t;
  if (!isfinite(x) || x == 0.0L) {
    return x;
  }
  t = truncl(x);
  if (t < x) {
    t += 1.0L;
  }
  return t;
}

float roundf(float x) {
  return (float)round((double)x);
}

long double roundl(long double x) {
  if (!isfinite(x) || x == 0.0L) {
    return x;
  }
  return x < 0.0L ? ceill(x - 0.5L) : floorl(x + 0.5L);
}

double sqrt(double x) {
  return __builtin_elementwise_sqrt(x);
}

float sqrtf(float x) {
  return __builtin_elementwise_sqrt(x);
}

long double sqrtl(long double x) {
  long double guess;
  int i;

  if (isnan(x) || x == 0.0L || x == (long double)INFINITY) {
    return x;
  }
  if (x < 0.0L) {
    return (long double)NAN;
  }
  guess = x >= 1.0L ? x : 1.0L;
  for (i = 0; i < 40; ++i) {
    guess = 0.5L * (guess + x / guess);
  }
  return guess;
}

float expf(float x) {
  return (float)exp((double)x);
}

long double expl(long double x) {
  return (long double)exp((double)x);
}
