#ifndef CRT_MATH_H
#define CRT_MATH_H

#ifdef __cplusplus
extern "C" {
#endif

#define FP_NAN 0
#define FP_INFINITE 1
#define FP_ZERO 2
#define FP_SUBNORMAL 3
#define FP_NORMAL 4

#define HUGE_VAL (__builtin_huge_val())
#define HUGE_VALF (__builtin_huge_valf())
#define HUGE_VALL (__builtin_huge_vall())
#define INFINITY (__builtin_inff())
#define NAN (__builtin_nanf(""))

#define fpclassify(x) __builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, (x))
#define isfinite(x) __builtin_isfinite(x)
#define isinf(x) __builtin_isinf(x)
#define isnan(x) __builtin_isnan(x)
#define isnormal(x) __builtin_isnormal(x)
#define signbit(x) __builtin_signbit(x)

double fabs(double x);
float fabsf(float x);
long double fabsl(long double x);

double copysign(double x, double y);
float copysignf(float x, float y);
long double copysignl(long double x, long double y);

double fmin(double x, double y);
float fminf(float x, float y);
long double fminl(long double x, long double y);

double fmax(double x, double y);
float fmaxf(float x, float y);
long double fmaxl(long double x, long double y);

double floor(double x);
float floorf(float x);
long double floorl(long double x);

double ceil(double x);
float ceilf(float x);
long double ceill(long double x);

double trunc(double x);
float truncf(float x);
long double truncl(long double x);

double round(double x);
float roundf(float x);
long double roundl(long double x);

double sqrt(double x);
float sqrtf(float x);
long double sqrtl(long double x);

double exp(double x);
float expf(float x);
long double expl(long double x);

double log(double x);
float logf(float x);
long double logl(long double x);

double scalbn(double x, int n);
float scalbnf(float x, int n);
long double scalbnl(long double x, int n);

double ldexp(double x, int n);
float ldexpf(float x, int n);
long double ldexpl(long double x, int n);

double pow(double x, double y);
float powf(float x, float y);
long double powl(long double x, long double y);

#ifdef __cplusplus
}
#endif

#endif
