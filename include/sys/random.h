#ifndef CRT_SYS_RANDOM_H
#define CRT_SYS_RANDOM_H

#include <stddef.h>
#include <linux/random.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

int getentropy(void* buffer, size_t buffer_size);
ssize_t getrandom(void* buffer, size_t buffer_size, unsigned int flags);

#ifdef __cplusplus
}
#endif

#endif
