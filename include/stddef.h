#ifndef CRT_STDDEF_H
#define CRT_STDDEF_H

typedef __PTRDIFF_TYPE__ ptrdiff_t;
typedef __SIZE_TYPE__ size_t;

#ifndef __cplusplus
typedef __WCHAR_TYPE__ wchar_t;
#endif

typedef struct {
  long long __ll;
  long double __ld;
} max_align_t;

#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void*)0)
#endif
#endif

#define offsetof(type, member) __builtin_offsetof(type, member)

#endif
