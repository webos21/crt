#ifndef CRT_PRIVATE_CRT_THREAD_TLS_H
#define CRT_PRIVATE_CRT_THREAD_TLS_H

#include <stddef.h>

/* Linux: a native ELF TLS block, thread control block and dtv for a thread created
 * by pthread_create() (libc/src/arch/linux/common/thread_tls.c explains the layout
 * and its limits). __crt_linux_thread_tls_size() is the number of bytes to reserve
 * at the top of the new thread's stack mapping, or 0 when the setup is not available
 * (unsupported architecture or loader layout) -- in which case the thread keeps the
 * creator's thread pointer exactly as before. __crt_linux_thread_tls_setup() builds
 * the block in the `size` bytes ending at `region_end` and returns the new thread
 * pointer to pass to clone(CLONE_SETTLS), or 0 on failure. */
size_t __crt_linux_thread_tls_size(void);
void* __crt_linux_thread_tls_setup(void* region_end);

#endif
