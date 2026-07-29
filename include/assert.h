#ifndef CRT_ASSERT_H
#define CRT_ASSERT_H

#ifdef NDEBUG
#define assert(expr) ((void)0)
#else
void abort(void) __attribute__((noreturn));
#define assert(expr) ((expr) ? (void)0 : abort())
#endif

#endif
