#ifndef CRT_STDIO_EXT_H
#define CRT_STDIO_EXT_H

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FSETLOCKING_QUERY 0
#define FSETLOCKING_INTERNAL 1
#define FSETLOCKING_BYCALLER 2

size_t __fbufsize(FILE* stream);
int __freadable(FILE* stream);
int __freading(FILE* stream);
int __fwritable(FILE* stream);
int __fwriting(FILE* stream);
int __flbf(FILE* stream);
void __fpurge(FILE* stream);
size_t __fpending(FILE* stream);
size_t __freadahead(FILE* stream);
void _flushlbf(void);
void __fseterr(FILE* stream);
int __fsetlocking(FILE* stream, int type);

#ifdef __cplusplus
}
#endif

#endif
