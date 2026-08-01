#ifndef CRT_SYS_SWAP_H
#define CRT_SYS_SWAP_H

#define SWAP_FLAG_PREFER 0x8000
#define SWAP_FLAG_PRIO_MASK 0x7fff
#define SWAP_FLAG_PRIO_SHIFT 0

#ifdef __cplusplus
extern "C" {
#endif

int swapon(const char* path, int flags);
int swapoff(const char* path);

#ifdef __cplusplus
}
#endif

#endif
