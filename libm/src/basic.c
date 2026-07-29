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

float logf(float x) {
  return (float)log((double)x);
}

long double logl(long double x) {
  return (long double)log((double)x);
}

long double log10l(long double x) {
  return (long double)log10((double)x);
}

double log2(double x) {
  return log(x) * 1.4426950408889634074;
}

float log2f(float x) {
  return (float)log2((double)x);
}

long double log2l(long double x) {
  return (long double)log2((double)x);
}

long double expm1l(long double x) {
  return (long double)expm1((double)x);
}

long double log1pl(long double x) {
  return (long double)log1p((double)x);
}

long double scalbnl(long double x, int n) {
  return (long double)scalbn((double)x, n);
}

long double ldexpl(long double x, int n) {
  return scalbnl(x, n);
}

float powf(float x, float y) {
  return (float)pow((double)x, (double)y);
}

long double powl(long double x, long double y) {
  return (long double)pow((double)x, (double)y);
}

float sinf(float x) {
  return (float)sin((double)x);
}

long double sinl(long double x) {
  return (long double)sin((double)x);
}

float cosf(float x) {
  return (float)cos((double)x);
}

long double cosl(long double x) {
  return (long double)cos((double)x);
}

long double tanl(long double x) {
  return (long double)tan((double)x);
}

long double frexpl(long double x, int* exp) {
  return (long double)frexp((double)x, exp);
}

long double modfl(long double x, long double* iptr) {
  double integral;
  double fractional;

  fractional = modf((double)x, &integral);
  *iptr = (long double)integral;
  return (long double)fractional;
}

long double fmodl(long double x, long double y) {
  return (long double)fmod((double)x, (double)y);
}

long double remainderl(long double x, long double y) {
  return (long double)remainder((double)x, (double)y);
}

long double remquol(long double x, long double y, int* quo) {
  return (long double)remquo((double)x, (double)y, quo);
}
