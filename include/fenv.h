#ifndef CRT_FENV_H
#define CRT_FENV_H

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int fexcept_t;
typedef struct {
  unsigned int flags;
  int round;
  unsigned int status;
  unsigned int control;
  unsigned int mxcsr;
  unsigned int x87_control;
  unsigned int x87_status;
} fenv_t;

#define FE_INVALID 0x01
#define FE_DIVBYZERO 0x02
#define FE_OVERFLOW 0x04
#define FE_UNDERFLOW 0x08
#define FE_INEXACT 0x10
#define FE_ALL_EXCEPT (FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW | FE_INEXACT)

#define FE_TONEAREST 0
#define FE_DOWNWARD 1
#define FE_UPWARD 2
#define FE_TOWARDZERO 3

extern const fenv_t* __crt_fe_dfl_env(void);
#define FE_DFL_ENV (__crt_fe_dfl_env())

int feclearexcept(int excepts);
int fegetexceptflag(fexcept_t* flagp, int excepts);
int feraiseexcept(int excepts);
int fesetexceptflag(const fexcept_t* flagp, int excepts);
int fetestexcept(int excepts);

int fegetround(void);
int fesetround(int round);

int fegetenv(fenv_t* envp);
int feholdexcept(fenv_t* envp);
int fesetenv(const fenv_t* envp);
int feupdateenv(const fenv_t* envp);

#ifdef __cplusplus
}
#endif

#endif
