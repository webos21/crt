#ifndef CRT_STRING_H
#define CRT_STRING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void* memcpy(void* dst, const void* src, size_t n);
void* memmove(void* dst, const void* src, size_t n);
void* memset(void* dst, int c, size_t n);
size_t strlen(const char* s);
int strcmp(const char* s1, const char* s2);

#ifdef __cplusplus
}
#endif

#endif
