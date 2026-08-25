# Libm Dependency Map

This document records the completed portable-source import map and the
remaining long-double/optimization decisions for `libm`. The map is
intentionally source-oriented: it lists public symbols, upstream source files,
and private dependencies used under `libm/src/freebsd/`.

## Source Policy

Current Bionic `main` is the default reference, but it uses a mixed strategy:

- many trigonometric and helper functions come from Bionic's current
  FreeBSD/msun import under `libm/upstream-freebsd/lib/msun/src/`;
- several simple operations, including current `sqrt`, are implemented in
  `libm/builtins.cpp` as compiler builtin wrappers;
- the Bionic `libm` build also pulls in optimized math routines through
  `libarm-optimized-routines-math`;
- older Bionic revisions still contain portable fdlibm C sources for functions
  that current Bionic no longer keeps directly in the FreeBSD/msun source list.

For this project, current Bionic builtin wrappers are acceptable when Clang
provides a stable builtin across the supported 64-bit targets and when compiler
lowering keeps the freestanding boundary explicit. Portable C sources remain
preferred over architecture-specific optimized routines for functions that do
not have a suitable compiler builtin policy.

Older Bionic fdlibm files are allowed only as documented bootstrap exceptions.
They should be used when current Bionic no longer carries a direct portable C
source for the function or when current Bionic's available source would pull in
architecture-specific optimized code, Android-private runtime dependencies, or a
source family that does not match the tranche policy. These exceptions must stay
visible in `third_party/bionic/README.md`.

## Imported In This Tranche

| Function | Local source | Upstream source | Ref | Direct private dependencies | Notes |
| --- | --- | --- | --- | --- | --- |
| `sqrt` | `libm/src/basic.c` | `libm/builtins.cpp` | Bionic `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | Clang `__builtin_elementwise_sqrt` | Follows current Bionic builtin policy while avoiding recursive Debug/O0 libcalls on Windows. |
| `sqrtf` | `libm/src/basic.c` | `libm/builtins.cpp` | Bionic `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | Clang `__builtin_elementwise_sqrt` | Same policy as `sqrt`. |
| `expf` | `libm/src/freebsd/e_expf.c` | `libm/src/e_expf.c` | Bionic `884e4f8` | word helpers | Native float fdlibm source replaces the bootstrap double wrapper. |
| `logf` | `libm/src/freebsd/e_logf.c` | `libm/src/e_logf.c` | Bionic `884e4f8` | word helpers | Native float fdlibm source replaces the bootstrap double wrapper. |
| `powf` | `libm/src/freebsd/e_powf.c` | `libm/src/e_powf.c` | Bionic `884e4f8` | word helpers, `sqrtf`, `scalbnf` | Native float fdlibm source replaces the bootstrap double wrapper. |
| `sinf` | `libm/src/freebsd/s_sincosf.c` | project-owned wrapper over Bionic main `e_rem_pio2f.c`, `k_sinf.c`, and `k_cosf.c` | Bionic `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` plus project wrapper | inline float reduction and kernels | Bionic main exposes helper files but not a standalone public `s_sinf.c` in the observed tree. |
| `cosf` | `libm/src/freebsd/s_sincosf.c` | project-owned wrapper over Bionic main `e_rem_pio2f.c`, `k_sinf.c`, and `k_cosf.c` | Bionic `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` plus project wrapper | inline float reduction and kernels | Bionic main exposes helper files but not a standalone public `s_cosf.c` in the observed tree. |
| `log10` | `libm/src/freebsd/e_log10.c` | `upstream-freebsd/lib/msun/src/e_log10.c` | Bionic `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | `k_log.h`, word helpers | Replaces the bootstrap `log(x) * log10(e)` wrapper. |
| `log10f` | `libm/src/freebsd/e_log10f.c` | `upstream-freebsd/lib/msun/src/e_log10f.c` | Bionic `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | `k_logf.h`, float word helpers | Native float source. |
| `log2` | `libm/src/freebsd/e_log2.c` | `lib/msun/src/e_log2.c` | FreeBSD upstream main | `k_log.h`, word helpers | Explicit exception because observed Bionic refs did not expose direct `e_log2.c`. |
| `log2f` | `libm/src/freebsd/e_log2f.c` | `lib/msun/src/e_log2f.c` | FreeBSD upstream main | `k_logf.h`, float word helpers | Explicit exception because observed Bionic refs did not expose direct `e_log2f.c`. |
| `expm1` | `libm/src/freebsd/s_expm1.c` | `upstream-freebsd/lib/msun/src/s_expm1.c` | Bionic `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | word helpers, `STRICT_ASSIGN` | Replaces the bootstrap `exp(x) - 1` wrapper. |
| `expm1f` | `libm/src/freebsd/s_expm1f.c` | `upstream-freebsd/lib/msun/src/s_expm1f.c` | Bionic `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | float word helpers | Native float source. |
| `log1p` | `libm/src/freebsd/s_log1p.c` | `upstream-freebsd/lib/msun/src/s_log1p.c` | Bionic `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | word helpers, `STRICT_ASSIGN` | Replaces the bootstrap `log(1 + x)` wrapper. |
| `log1pf` | `libm/src/freebsd/s_log1pf.c` | `upstream-freebsd/lib/msun/src/s_log1pf.c` | Bionic `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | float word helpers | Native float source. |
| `fmod` | `libm/src/freebsd/e_fmod.c` | `upstream-freebsd/lib/msun/src/e_fmod.c` | Bionic `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | word helpers, `nan_mix_op` | Replaces the quotient/trunc bootstrap implementation. |
| `fmodf` | `libm/src/freebsd/e_fmodf.c` | `upstream-freebsd/lib/msun/src/e_fmodf.c` | Bionic `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | float word helpers, `nan_mix_op` | Native float source. |
| `remainder` | `libm/src/freebsd/e_remainder.c` | `upstream-freebsd/lib/msun/src/e_remainder.c` | Bionic `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | `fmod`, `nan_mix_op` | Replaces the bootstrap ties-to-even implementation. |
| `remainderf` | `libm/src/freebsd/e_remainderf.c` | `upstream-freebsd/lib/msun/src/e_remainderf.c` | Bionic `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | `fmodf`, `nan_mix_op` | Native float source. |
| `remquo` | `libm/src/freebsd/s_remquo.c` | `upstream-freebsd/lib/msun/src/s_remquo.c` | Bionic `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | word helpers, `nan_mix_op` | Replaces the bootstrap quotient-bit implementation. |
| `remquof` | `libm/src/freebsd/s_remquof.c` | `upstream-freebsd/lib/msun/src/s_remquof.c` | Bionic `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | float word helpers, `nan_mix_op` | Native float source. |
| `frexp` | `libm/src/freebsd/s_frexp.c` | `upstream-freebsd/lib/msun/src/s_frexp.c` | Bionic `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | word helpers | Replaces the local IEEE decomposition bootstrap. |
| `frexpf` | `libm/src/freebsd/s_frexpf.c` | `upstream-freebsd/lib/msun/src/s_frexpf.c` | Bionic `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | float word helpers | Native float source. |
| `modf` | `libm/src/freebsd/s_modf.c` | `upstream-freebsd/lib/msun/src/s_modf.c` | Bionic `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | word helpers | Replaces the trunc/subtract bootstrap implementation. |
| `modff` | `libm/src/freebsd/s_modff.c` | `upstream-freebsd/lib/msun/src/s_modff.c` | Bionic `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | float word helpers | Native float source. |
| `tanf` | `libm/src/freebsd/s_tanf.c` | `upstream-freebsd/lib/msun/src/s_tanf.c` | Bionic `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | inline `e_rem_pio2f.c`, inline `k_tanf.c`, `M_PI_2` | Native float tangent path. |
| `fenv` | `libm/src/fenv.c` | project-owned architecture backend | project-owned | x86_64 MXCSR plus x87 control/status, or AArch64 FPCR/FPSR | Provides observable exception flags and rounding mode storage for supported 64-bit CPU families; full trap/tag-word environment images are still deferred. |

`sqrtl` remains the previous bootstrap long-double implementation. Importing
`e_sqrtl.c` should be handled as a separate long-double tranche because it
involves fenv behavior and long-double ABI details. The older Bionic fdlibm
`libm/src/e_sqrt.c` remains a documented fallback candidate if a future target
cannot use Clang's builtin lowering safely.

`fabs*` and `copysign*` use Clang builtins. The `fmin`/`fmax` family uses the
FreeBSD/msun or project-owned portable paths so NaN and signed-zero behavior is
not target-builtin dependent. By contrast, `__builtin_floorf`,
`__builtin_ceilf`, `__builtin_truncf`, and
`__builtin_roundf` are not used yet because Debug/O0 Linux and Windows builds
lower them to recursive libcalls back into the same exported `*f` symbol.

## Double-Precision Import Record

### Trigonometric Core

| Function | Primary source | Required helper sources | Required private header features | Notes |
| --- | --- | --- | --- | --- |
| `sin` | `upstream-freebsd/lib/msun/src/s_sin.c` at Bionic `main` | `e_rem_pio2.c`, `k_rem_pio2.c`, `k_sin.c`, `k_cos.c` | `GET_HIGH_WORD`, `GET_LOW_WORD`, `INSERT_WORDS`, `SET_LOW_WORD`, `STRICT_ASSIGN`, endian-safe double word helpers | Imported. `s_sin.c` includes `e_rem_pio2.c` inline with `INLINE_REM_PIO2`; `k_rem_pio2.c` is compiled once. |
| `cos` | `upstream-freebsd/lib/msun/src/s_cos.c` at Bionic `main` | `e_rem_pio2.c`, `k_rem_pio2.c`, `k_sin.c`, `k_cos.c` | same as `sin` | Imported. Shares the same argument reduction and kernel files as `sin`. |
| `tan` | `upstream-freebsd/lib/msun/src/s_tan.c` at Bionic `main` | `e_rem_pio2.c`, `k_rem_pio2.c`, `k_tan.c` | same as `sin` | Imported. Uses the shared argument reduction path and the tangent kernel. |

Completed import order:

1. Expand local `math_private.h` with the missing word helpers and
   `STRICT_ASSIGN`.
2. Import `k_sin.c`, `k_cos.c`, `k_tan.c`, `k_rem_pio2.c`, and `e_rem_pio2.c`.
3. Import `s_sin.c`, `s_cos.c`, and `s_tan.c`.
4. Add tests for small values, quadrant reduction, infinities, NaNs, and signed
   zero behavior.

### Exponential And Logarithmic Core

Current Bionic `main` does not list direct double `e_exp.c`, `e_log.c`, or
`e_pow.c` files in `libm/Android.bp`. For portable C import, the practical
candidate is the older Bionic fdlibm source set.

| Function | Portable candidate source | Ref | Direct private dependencies | Other libc/libm dependencies | Notes |
| --- | --- | --- | --- | --- | --- |
| `exp` | `libm/src/e_exp.c` | Bionic `884e4f8` | `GET_HIGH_WORD`, `GET_LOW_WORD`, `SET_HIGH_WORD`, `u_int32_t` | none beyond `math.h` | Standalone enough for an early import after `math_private.h` expands. |
| `log` | `libm/src/e_log.c` | Bionic `884e4f8` | `EXTRACT_WORDS`, `GET_HIGH_WORD`, `SET_HIGH_WORD`, `u_int32_t` | none beyond `math.h` | Good next candidate after `exp`; no kernel source dependency. |
| `scalbn` | `libm/src/s_scalbn.c` | Bionic `884e4f8` | `EXTRACT_WORDS`, `GET_HIGH_WORD`, `SET_HIGH_WORD`, `u_int32_t` | `copysign` | Imported before `pow`; `ldexp` is provided as a wrapper. |
| `scalbnf` | `libm/src/s_scalbnf.c` | Bionic `884e4f8` | `GET_FLOAT_WORD`, `SET_FLOAT_WORD` | `copysignf` | Imported before `powf`; `ldexpf` is provided as a wrapper. |
| `pow` | `libm/src/e_pow.c` | Bionic `884e4f8` | `EXTRACT_WORDS`, `GET_HIGH_WORD`, `SET_HIGH_WORD`, `SET_LOW_WORD`, `u_int32_t` | `fabs`, `sqrt`, `scalbn` | Needs `scalbn` before full import. It can use the current builtin-backed `sqrt`. |

`exp`, `log`, `scalbn`, `scalbnf`, `pow`, native `expf`, native `logf`, native
`powf`, and the common trigonometric float functions have been imported or
replaced with native-kernel wrappers. The public long-double APIs now live in a
project-owned portable bootstrap file, `libm/src/long_double.c`, so they no
longer collapse through double wrappers on targets where `long double` is wider
than `double`. The project has decided to follow the compiler target's native
`long double` ABI instead of forcing Bionic's 64-bit Android 128-bit long double
model across every host OS.

Remaining order:

1. Replace project-owned long-double bootstrap functions with representation
   appropriate sources selected by `LDBL_MANT_DIG`: fake/double-sized wrappers
   for `53`, ld80-style sources for `64`, and ld128-style sources for `113`.
2. Revisit `math_errhandling` only if a future POSIX compatibility mode requires
   libm functions to set `errno` or raise strict per-function fenv exceptions.
   The default policy remains `math_errhandling == 0`.
3. Evaluate Bionic optimized-routine imports after correctness and provenance
   are stable.
