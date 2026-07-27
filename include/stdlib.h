#ifndef CRT_STDLIB_H
#define CRT_STDLIB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void* malloc(size_t size);
void free(void* ptr);
void* calloc(size_t nmemb, size_t size);
void* realloc(void* ptr, size_t size);
int atoi(const char* s);
long atol(const char* s);
long strtol(const char* nptr, char** endptr, int base);
unsigned long strtoul(const char* nptr, char** endptr, int base);

#ifdef __cplusplus
}
#endif

#endif
