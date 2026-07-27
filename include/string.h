#ifndef CRT_STRING_H
#define CRT_STRING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void* memcpy(void* dst, const void* src, size_t n);
void* memchr(const void* s, int c, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);
void* memmove(void* dst, const void* src, size_t n);
void* memset(void* dst, int c, size_t n);
char* strcat(char* dst, const char* src);
size_t strcspn(const char* s1, const char* s2);
char* strdup(const char* s);
char* strchr(const char* s, int c);
int strcmp(const char* s1, const char* s2);
char* strcpy(char* dst, const char* src);
size_t strlen(const char* s);
char* strndup(const char* s, size_t n);
size_t strnlen(const char* s, size_t n);
int strncmp(const char* s1, const char* s2, size_t n);
char* strncpy(char* dst, const char* src, size_t n);
char* strpbrk(const char* s1, const char* s2);
char* strrchr(const char* s, int c);
size_t strspn(const char* s1, const char* s2);
char* strstr(const char* s, const char* find);

#ifdef __cplusplus
}
#endif

#endif
