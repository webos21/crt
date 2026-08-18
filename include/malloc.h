#ifndef CRT_MALLOC_H
#define CRT_MALLOC_H

/* Historical (pre-POSIX) location for the malloc(3) family. POSIX only
 * mandates <stdlib.h>, but a long line of Unix systems (and Android
 * Bionic's own bionic/libc/include/malloc.h, which this header follows)
 * also ship a <malloc.h> that just re-exports the same declarations for
 * source compatibility with code written against that older convention --
 * this project's real malloc()/calloc()/realloc()/free()/reallocarray()
 * declarations live in <stdlib.h> and stay there; this header adds no new
 * symbols of its own except allocator extensions this project's libc
 * actually implements.
 */
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t malloc_usable_size(const void* ptr);

#ifdef __cplusplus
}
#endif

#endif /* CRT_MALLOC_H */
