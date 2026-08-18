#ifndef CRT_MATH_H
#define CRT_MATH_H

#include <limits.h>

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

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MATH_ERRNO 1
#define MATH_ERREXCEPT 2
#define math_errhandling 0
#define FP_ILOGB0 INT_MIN
#define FP_ILOGBNAN INT_MIN

#define fpclassify(x) __builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, (x))
#define isfinite(x) __builtin_isfinite(x)
#define isinf(x) __builtin_isinf(x)
#define isnan(x) __builtin_isnan(x)
#define isnormal(x) __builtin_isnormal(x)
#define signbit(x) __builtin_signbit(x)
#define isgreater(x, y) __builtin_isgreater((x), (y))
#define isgreaterequal(x, y) __builtin_isgreaterequal((x), (y))
#define isless(x, y) __builtin_isless((x), (y))
#define islessequal(x, y) __builtin_islessequal((x), (y))
#define islessgreater(x, y) __builtin_islessgreater((x), (y))
#define isunordered(x, y) __builtin_isunordered((x), (y))

#if FLT_EVAL_METHOD == 0
typedef float float_t;
typedef double double_t;
#elif FLT_EVAL_METHOD == 1
typedef double float_t;
typedef double double_t;
#else
typedef long double float_t;
typedef long double double_t;
#endif

double fabs(double x);
float fabsf(float x);
long double fabsl(long double x);

double copysign(double x, double y);
float copysignf(float x, float y);
long double copysignl(long double x, long double y);

double fma(double x, double y, double z);
float fmaf(float x, float y, float z);
long double fmal(long double x, long double y, long double z);

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

double nextafter(double x, double y);
float nextafterf(float x, float y);
long double nextafterl(long double x, long double y);

double nexttoward(double x, long double y);
float nexttowardf(float x, long double y);
long double nexttowardl(long double x, long double y);

double nan(const char* tagp);
float nanf(const char* tagp);
long double nanl(const char* tagp);

long lrint(double x);
long lrintf(float x);
long lrintl(long double x);

long long llrint(double x);
long long llrintf(float x);
long long llrintl(long double x);

long lround(double x);
long lroundf(float x);
long lroundl(long double x);

long long llround(double x);
long long llroundf(float x);
long long llroundl(long double x);

double rint(double x);
float rintf(float x);
long double rintl(long double x);

double nearbyint(double x);
float nearbyintf(float x);
long double nearbyintl(long double x);

double sqrt(double x);
float sqrtf(float x);
long double sqrtl(long double x);

double cbrt(double x);
float cbrtf(float x);
long double cbrtl(long double x);

double exp(double x);
float expf(float x);
long double expl(long double x);

double exp2(double x);
float exp2f(float x);
long double exp2l(long double x);

double log(double x);
float logf(float x);
long double logl(long double x);

double log10(double x);
float log10f(float x);
long double log10l(long double x);

double log2(double x);
float log2f(float x);
long double log2l(long double x);

double logb(double x);
float logbf(float x);
long double logbl(long double x);

double expm1(double x);
float expm1f(float x);
long double expm1l(long double x);

double log1p(double x);
float log1pf(float x);
long double log1pl(long double x);

double scalbn(double x, int n);
float scalbnf(float x, int n);
long double scalbnl(long double x, int n);

double scalbln(double x, long n);
float scalblnf(float x, long n);
long double scalblnl(long double x, long n);

double ldexp(double x, int n);
float ldexpf(float x, int n);
long double ldexpl(long double x, int n);

double pow(double x, double y);
float powf(float x, float y);
long double powl(long double x, long double y);

double sin(double x);
float sinf(float x);
long double sinl(long double x);

double cos(double x);
float cosf(float x);
long double cosl(long double x);

double tan(double x);
float tanf(float x);
long double tanl(long double x);

double cosh(double x);
float coshf(float x);
long double coshl(long double x);
double sinh(double x);
float sinhf(float x);
long double sinhl(long double x);
double tanh(double x);
float tanhf(float x);
long double tanhl(long double x);
double acosh(double x);
float acoshf(float x);
long double acoshl(long double x);
double asinh(double x);
float asinhf(float x);
long double asinhl(long double x);
double atanh(double x);
float atanhf(float x);
long double atanhl(long double x);

double atan(double x);
float atanf(float x);
long double atanl(long double x);

double atan2(double y, double x);
float atan2f(float y, float x);
long double atan2l(long double y, long double x);

double asin(double x);
float asinf(float x);
long double asinl(long double x);

double acos(double x);
float acosf(float x);
long double acosl(long double x);

double frexp(double x, int* exp);
float frexpf(float x, int* exp);
long double frexpl(long double x, int* exp);

double modf(double x, double* iptr);
float modff(float x, float* iptr);
long double modfl(long double x, long double* iptr);

double fmod(double x, double y);
float fmodf(float x, float y);
long double fmodl(long double x, long double y);

double remainder(double x, double y);
float remainderf(float x, float y);
long double remainderl(long double x, long double y);

double remquo(double x, double y, int* quo);
float remquof(float x, float y, int* quo);
long double remquol(long double x, long double y, int* quo);

double erf(double x);
float erff(float x);
long double erfl(long double x);
double erfc(double x);
float erfcf(float x);
long double erfcl(long double x);

double fdim(double x, double y);
float fdimf(float x, float y);
long double fdiml(long double x, long double y);

double hypot(double x, double y);
float hypotf(float x, float y);
long double hypotl(long double x, long double y);

int ilogb(double x);
int ilogbf(float x);
int ilogbl(long double x);

extern int signgam;
double lgamma(double x);
float lgammaf(float x);
long double lgammal(long double x);
double lgamma_r(double x, int* signgamp);
float lgammaf_r(float x, int* signgamp);
long double lgammal_r(long double x, int* signgamp);

double tgamma(double x);
float tgammaf(float x);
long double tgammal(long double x);

double sinpi(double x);
double cospi(double x);

#ifdef __cplusplus
}
#endif

#endif
