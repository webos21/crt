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

#endif
