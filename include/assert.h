#ifndef CRT_ASSERT_H
#define CRT_ASSERT_H

#ifdef NDEBUG
#define assert(expr) ((void)0)
#else
#ifdef __cplusplus
extern "C" {
#endif
void abort(void) __attribute__((noreturn));
#ifdef __cplusplus
}
#endif
#define assert(expr) ((expr) ? (void)0 : abort())
#endif

/* C11 static_assert: a compile-time assertion checked entirely by the
 * compiler front end, independent of NDEBUG (NDEBUG only disables the
 * runtime assert() above). _Static_assert is a real C11 keyword every
 * C11-conforming compiler provides; every real C11 libc (glibc, musl,
 * Android Bionic) additionally exposes it under the plain `static_assert`
 * spelling via exactly this macro in <assert.h> -- C11 requires this
 * (C17 7.2p3), and C23 later promoted static_assert to a keyword in its
 * own right (making this macro implicit/redundant there, but still valid
 * to define). Found missing 2026-08-25 while porting libxkbcommon: its
 * src/darray.h includes <assert.h> and relies on getting static_assert
 * from it, exactly like every other real C11 libc -- without this macro,
 * -std=c11 alone does NOT make bare `static_assert(...)` work, since only
 * _Static_assert is an actual C11 keyword; static_assert has always been
 * libc's job, not the compiler's. Guarded so C++ (which has its own
 * static_assert keyword since C++11) and pre-C11 callers are unaffected. */
#if !defined(__cplusplus) && defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(static_assert)
#define static_assert _Static_assert
#endif

#endif
