#ifndef CRT_STRINGS_H
#define CRT_STRINGS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int strcasecmp(const char* s1, const char* s2);
void bcopy(const void* src, void* dst, size_t n);
void bzero(void* s, size_t n);
int bcmp(const void* s1, const void* s2, size_t n);

#ifdef __cplusplus
}
#endif

#endif
