# Libm Dependency Map

This document records the planned import map for the next double-precision
`libm` functions. The map is intentionally source-oriented: it lists the public
symbol, candidate upstream source files, and private dependencies that must be
present before the function is imported into `libm/src/freebsd/`.

## Source Policy

Current Bionic `main` uses a mixed strategy:

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

## Imported In This Tranche

| Function | Local source | Upstream source | Ref | Direct private dependencies | Notes |
| --- | --- | --- | --- | --- | --- |
| `sqrt` | `libm/src/basic.c` | `libm/builtins.cpp` | Bionic `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | Clang `__builtin_elementwise_sqrt` | Follows current Bionic builtin policy while avoiding recursive Debug/O0 libcalls on Windows. |
| `sqrtf` | `libm/src/basic.c` | `libm/builtins.cpp` | Bionic `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | Clang `__builtin_elementwise_sqrt` | Same policy as `sqrt`. |

`sqrtl` remains the previous bootstrap long-double implementation. Importing
`e_sqrtl.c` should be handled as a separate long-double tranche because it
involves fenv behavior and long-double ABI details. The older Bionic fdlibm
`libm/src/e_sqrt.c` remains a documented fallback candidate if a future target
cannot use Clang's builtin lowering safely.

`fabs*`, `copysign*`, `fmin`, `fmax`, `fminf`, and `fmaxf` use Clang builtins.
By contrast, `__builtin_floorf`, `__builtin_ceilf`, `__builtin_truncf`, and
`__builtin_roundf` are not used yet because Debug/O0 Linux and Windows builds
lower them to recursive libcalls back into the same exported `*f` symbol.

## Next Double-Precision Tranche

### Trigonometric Core

| Function | Primary source | Required helper sources | Required private header features | Notes |
| --- | --- | --- | --- | --- |
| `sin` | `upstream-freebsd/lib/msun/src/s_sin.c` at Bionic `main` | `e_rem_pio2.c`, `k_rem_pio2.c`, `k_sin.c`, `k_cos.c` | `GET_HIGH_WORD`, `GET_LOW_WORD`, `INSERT_WORDS`, `SET_LOW_WORD`, `STRICT_ASSIGN`, endian-safe double word helpers | `s_sin.c` includes `e_rem_pio2.c` inline when `INLINE_REM_PIO2` is set. Prefer compiling argument reduction once if local symbol collisions appear. |
| `cos` | `upstream-freebsd/lib/msun/src/s_cos.c` at Bionic `main` | `e_rem_pio2.c`, `k_rem_pio2.c`, `k_sin.c`, `k_cos.c` | same as `sin` | Shares the same argument reduction and kernel files as `sin`. |
| `tan` | `upstream-freebsd/lib/msun/src/s_tan.c` at Bionic `main` | `e_rem_pio2.c`, `k_rem_pio2.c`, `k_tan.c` | same as `sin` | `tan` uses the same argument reduction path but only the tangent kernel. |

Recommended import order:

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

`exp`, `log`, `scalbn`, `scalbnf`, and `pow` have been imported. `expf`,
`expl`, `logf`, `logl`, `powf`, and `powl` are currently bootstrap wrappers over
the double implementations; native float and long-double imports remain
separate precision tranches.

Recommended import order:

1. Native float precision for `expf`, `logf`, and `powf`
2. `sin`, `cos`, and `tan`
3. Floating-point exception and `errno` policy
