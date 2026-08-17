#ifndef CRT_PRIVATE_WAIT_H
#define CRT_PRIVATE_WAIT_H

struct timespec;

int __crt_wait32(int* addr, int expected);
int __crt_wait32_timed(int* addr, int expected, const struct timespec* timeout);
int __crt_wake32_one(int* addr);
int __crt_wake32_all(int* addr);

/*
 * Process-shared variants: same contract as the four functions above, but
 * for an address that lives in memory mapped PTHREAD_PROCESS_SHARED across
 * multiple processes rather than just multiple threads within one process.
 *
 * Per-host reality (see wait.c for the full reasoning):
 *   - Linux: real and cross-process. Uses the non-private FUTEX_WAIT/
 *     FUTEX_WAKE operations, which key off physical backing (inode+offset)
 *     rather than (mm_struct, virtual address) the way the private
 *     operations above do, so two unrelated processes mapping the same
 *     shared memory correctly rendezvous on the same futex.
 *   - macOS: real, using os_sync_wait_on_address's documented SHARED flag.
 *     Reasoned carefully but not yet verified against real Apple hardware
 *     in this Windows-only dev session -- flagged unverified in wait.c.
 *   - Windows: ENOTSUP. WaitOnAddress/WakeByAddressSingle/WakeByAddressAll
 *     are documented as operating on the calling process's own virtual
 *     address space only -- an architectural limitation, not a missing
 *     flag -- so there is no honest way to make these cross-process here.
 */
int __crt_wait32_shared(int* addr, int expected);
int __crt_wait32_timed_shared(int* addr, int expected, const struct timespec* timeout);
int __crt_wake32_one_shared(int* addr);
int __crt_wake32_all_shared(int* addr);

#endif
