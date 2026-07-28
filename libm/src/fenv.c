#include <fenv.h>

static const fenv_t crt_default_fenv = {0, FE_TONEAREST};
static fenv_t crt_current_fenv = {0, FE_TONEAREST};

const fenv_t* __crt_fe_dfl_env(void) {
  return &crt_default_fenv;
}

int feclearexcept(int excepts) {
  (void)excepts;
  crt_current_fenv.flags = 0;
  return 0;
}

int fegetexceptflag(fexcept_t* flagp, int excepts) {
  *flagp = crt_current_fenv.flags & (unsigned int)excepts;
  return 0;
}

int feraiseexcept(int excepts) {
  (void)excepts;
  return 0;
}

int fesetexceptflag(const fexcept_t* flagp, int excepts) {
  crt_current_fenv.flags =
      (crt_current_fenv.flags & ~(unsigned int)excepts) | (*flagp & (unsigned int)excepts);
  return 0;
}

int fetestexcept(int excepts) {
  (void)excepts;
  return 0;
}

int fegetround(void) {
  return crt_current_fenv.round;
}

int fesetround(int round) {
  if (round != FE_TONEAREST) {
    return -1;
  }
  crt_current_fenv.round = round;
  return 0;
}

int fegetenv(fenv_t* envp) {
  *envp = crt_current_fenv;
  return 0;
}

int feholdexcept(fenv_t* envp) {
  *envp = crt_current_fenv;
  crt_current_fenv.flags = 0;
  return 0;
}

int fesetenv(const fenv_t* envp) {
  if (envp == FE_DFL_ENV) {
    crt_current_fenv = crt_default_fenv;
  } else {
    crt_current_fenv = *envp;
  }
  return 0;
}

int feupdateenv(const fenv_t* envp) {
  return fesetenv(envp);
}
