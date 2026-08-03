#ifndef CRT_SYS_CDEFS_H
#define CRT_SYS_CDEFS_H

#define __BIONIC__ 1

#ifdef __cplusplus
#define __BEGIN_DECLS extern "C" {
#define __END_DECLS }
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif

/* Old K&R-style prototype wrapper; full ANSI C is always available here. */
#define __P(protos) protos

#define __CONCAT1(x, y) x##y
#define __CONCAT(x, y) __CONCAT1(x, y)
#define ___CONCAT(x, y) __CONCAT(x, y)

#define __STRING(x) #x
#define ___STRING(x) __STRING(x)

#define __unused __attribute__((__unused__))
#define __used __attribute__((__used__))
#define __packed __attribute__((__packed__))
#define __dead __attribute__((__noreturn__))
#define __noreturn __attribute__((__noreturn__))
#define __always_inline __attribute__((__always_inline__))

#define __printflike(x, y) __attribute__((__format__(printf, x, y)))
#define __scanflike(x, y) __attribute__((__format__(scanf, x, y)))

#define __predict_true(exp) __builtin_expect((exp) != 0, 1)
#define __predict_false(exp) __builtin_expect((exp) != 0, 0)

#endif
