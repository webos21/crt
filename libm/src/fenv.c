#include <fenv.h>

static const fenv_t crt_default_fenv = {0, FE_TONEAREST, 0, 0};

#if defined(__aarch64__) || defined(_M_ARM64)
#define CRT_FENV_AARCH64 1
#elif defined(__x86_64__) || defined(_M_X64)
#define CRT_FENV_X86_64 1
#endif

#if defined(CRT_FENV_X86_64)
#define CRT_MXCSR_EXCEPTION_MASK 0x0000003fU
#define CRT_MXCSR_ROUND_MASK 0x00006000U
#define CRT_MXCSR_DEFAULT 0x00001f80U

static unsigned int read_mxcsr(void) {
  return __builtin_ia32_stmxcsr();
}

static void write_mxcsr(unsigned int value) {
  __builtin_ia32_ldmxcsr(value);
}

static unsigned int exceptions_to_mxcsr(int excepts) {
  unsigned int flags = 0;

  if ((excepts & FE_INVALID) != 0) flags |= 0x01U;
  if ((excepts & FE_DIVBYZERO) != 0) flags |= 0x04U;
  if ((excepts & FE_OVERFLOW) != 0) flags |= 0x08U;
  if ((excepts & FE_UNDERFLOW) != 0) flags |= 0x10U;
  if ((excepts & FE_INEXACT) != 0) flags |= 0x20U;
  return flags;
}

static int mxcsr_to_exceptions(unsigned int status) {
  int flags = 0;

  if ((status & 0x01U) != 0) flags |= FE_INVALID;
  if ((status & 0x04U) != 0) flags |= FE_DIVBYZERO;
  if ((status & 0x08U) != 0) flags |= FE_OVERFLOW;
  if ((status & 0x10U) != 0) flags |= FE_UNDERFLOW;
  if ((status & 0x20U) != 0) flags |= FE_INEXACT;
  return flags;
}

static unsigned int round_to_mxcsr(int round) {
  switch (round) {
    case FE_TONEAREST: return 0x00000000U;
    case FE_DOWNWARD: return 0x00002000U;
    case FE_UPWARD: return 0x00004000U;
    case FE_TOWARDZERO: return 0x00006000U;
    default: return 0xffffffffU;
  }
}

static int mxcsr_to_round(unsigned int mxcsr) {
  switch (mxcsr & CRT_MXCSR_ROUND_MASK) {
    case 0x00000000U: return FE_TONEAREST;
    case 0x00002000U: return FE_DOWNWARD;
    case 0x00004000U: return FE_UPWARD;
    case 0x00006000U: return FE_TOWARDZERO;
    default: return FE_TONEAREST;
  }
}
#elif defined(CRT_FENV_AARCH64)
#define CRT_AARCH64_EXCEPTION_MASK 0x0000009fU
#define CRT_AARCH64_ROUND_MASK 0x00c00000U

static unsigned int read_fpcr(void) {
  unsigned long long value;
  __asm__ volatile("mrs %0, fpcr" : "=r"(value));
  return (unsigned int)value;
}

static void write_fpcr(unsigned int value) {
  unsigned long long fpcr = value;
  __asm__ volatile("msr fpcr, %0" : : "r"(fpcr));
}

static unsigned int read_fpsr(void) {
  unsigned long long value;
  __asm__ volatile("mrs %0, fpsr" : "=r"(value));
  return (unsigned int)value;
}

static void write_fpsr(unsigned int value) {
  unsigned long long fpsr = value;
  __asm__ volatile("msr fpsr, %0" : : "r"(fpsr));
}

static unsigned int exceptions_to_fpsr(int excepts) {
  unsigned int flags = 0;

  if ((excepts & FE_INVALID) != 0) flags |= 0x01U;
  if ((excepts & FE_DIVBYZERO) != 0) flags |= 0x02U;
  if ((excepts & FE_OVERFLOW) != 0) flags |= 0x04U;
  if ((excepts & FE_UNDERFLOW) != 0) flags |= 0x08U;
  if ((excepts & FE_INEXACT) != 0) flags |= 0x10U;
  return flags;
}

static int fpsr_to_exceptions(unsigned int status) {
  int flags = 0;

  if ((status & 0x01U) != 0) flags |= FE_INVALID;
  if ((status & 0x02U) != 0) flags |= FE_DIVBYZERO;
  if ((status & 0x04U) != 0) flags |= FE_OVERFLOW;
  if ((status & 0x08U) != 0) flags |= FE_UNDERFLOW;
  if ((status & 0x10U) != 0) flags |= FE_INEXACT;
  return flags;
}

static unsigned int round_to_fpcr(int round) {
  switch (round) {
    case FE_TONEAREST: return 0x00000000U;
    case FE_UPWARD: return 0x00400000U;
    case FE_DOWNWARD: return 0x00800000U;
    case FE_TOWARDZERO: return 0x00c00000U;
    default: return 0xffffffffU;
  }
}

static int fpcr_to_round(unsigned int fpcr) {
  switch (fpcr & CRT_AARCH64_ROUND_MASK) {
    case 0x00000000U: return FE_TONEAREST;
    case 0x00400000U: return FE_UPWARD;
    case 0x00800000U: return FE_DOWNWARD;
    case 0x00c00000U: return FE_TOWARDZERO;
    default: return FE_TONEAREST;
  }
}
#endif

const fenv_t* __crt_fe_dfl_env(void) {
  return &crt_default_fenv;
}

int feclearexcept(int excepts) {
#if defined(CRT_FENV_X86_64)
  unsigned int mxcsr = read_mxcsr();
  mxcsr &= ~exceptions_to_mxcsr(excepts);
  write_mxcsr(mxcsr);
  return 0;
#elif defined(CRT_FENV_AARCH64)
  unsigned int fpsr = read_fpsr();
  fpsr &= ~exceptions_to_fpsr(excepts);
  write_fpsr(fpsr);
  return 0;
#else
  (void)excepts;
  return 0;
#endif
}

int fegetexceptflag(fexcept_t* flagp, int excepts) {
  if (flagp == 0) {
    return -1;
  }
  *flagp = (fexcept_t)fetestexcept(excepts);
  return 0;
}

int feraiseexcept(int excepts) {
#if defined(CRT_FENV_X86_64)
  unsigned int mxcsr = read_mxcsr();
  mxcsr |= exceptions_to_mxcsr(excepts);
  write_mxcsr(mxcsr);
  return 0;
#elif defined(CRT_FENV_AARCH64)
  unsigned int fpsr = read_fpsr();
  fpsr |= exceptions_to_fpsr(excepts);
  write_fpsr(fpsr);
  return 0;
#else
  (void)excepts;
  return 0;
#endif
}

int fesetexceptflag(const fexcept_t* flagp, int excepts) {
  if (flagp == 0) {
    return -1;
  }
  feclearexcept(excepts);
  return feraiseexcept(*flagp & excepts);
}

int fetestexcept(int excepts) {
#if defined(CRT_FENV_X86_64)
  return mxcsr_to_exceptions(read_mxcsr()) & excepts;
#elif defined(CRT_FENV_AARCH64)
  return fpsr_to_exceptions(read_fpsr()) & excepts;
#else
  (void)excepts;
  return 0;
#endif
}

int fegetround(void) {
#if defined(CRT_FENV_X86_64)
  return mxcsr_to_round(read_mxcsr());
#elif defined(CRT_FENV_AARCH64)
  return fpcr_to_round(read_fpcr());
#else
  return FE_TONEAREST;
#endif
}

int fesetround(int round) {
#if defined(CRT_FENV_X86_64)
  unsigned int round_bits = round_to_mxcsr(round);
  unsigned int mxcsr;

  if (round_bits == 0xffffffffU) {
    return -1;
  }
  mxcsr = read_mxcsr();
  mxcsr = (mxcsr & ~CRT_MXCSR_ROUND_MASK) | round_bits;
  write_mxcsr(mxcsr);
  return 0;
#elif defined(CRT_FENV_AARCH64)
  unsigned int round_bits = round_to_fpcr(round);
  unsigned int fpcr;

  if (round_bits == 0xffffffffU) {
    return -1;
  }
  fpcr = read_fpcr();
  fpcr = (fpcr & ~CRT_AARCH64_ROUND_MASK) | round_bits;
  write_fpcr(fpcr);
  return 0;
#else
  return round == FE_TONEAREST ? 0 : -1;
#endif
}

int fegetenv(fenv_t* envp) {
  if (envp == 0) {
    return -1;
  }
#if defined(CRT_FENV_X86_64)
  envp->control = read_mxcsr();
  envp->status = envp->control & CRT_MXCSR_EXCEPTION_MASK;
  envp->flags = (unsigned int)mxcsr_to_exceptions(envp->status);
  envp->round = mxcsr_to_round(envp->control);
#elif defined(CRT_FENV_AARCH64)
  envp->control = read_fpcr();
  envp->status = read_fpsr();
  envp->flags = (unsigned int)fpsr_to_exceptions(envp->status);
  envp->round = fpcr_to_round(envp->control);
#else
  *envp = crt_default_fenv;
#endif
  return 0;
}

int feholdexcept(fenv_t* envp) {
  if (fegetenv(envp) != 0) {
    return -1;
  }
  return feclearexcept(FE_ALL_EXCEPT);
}

int fesetenv(const fenv_t* envp) {
  fenv_t env;

  if (envp == 0) {
    return -1;
  }
  env = envp == FE_DFL_ENV ? crt_default_fenv : *envp;
#if defined(CRT_FENV_X86_64)
  if (env.control == 0) {
    env.control = CRT_MXCSR_DEFAULT;
  }
  write_mxcsr(env.control);
  return 0;
#elif defined(CRT_FENV_AARCH64)
  write_fpcr(env.control);
  write_fpsr(env.status);
  return 0;
#else
  (void)env;
  return 0;
#endif
}

int feupdateenv(const fenv_t* envp) {
  int excepts = fetestexcept(FE_ALL_EXCEPT);

  if (fesetenv(envp) != 0) {
    return -1;
  }
  return feraiseexcept(excepts);
}
