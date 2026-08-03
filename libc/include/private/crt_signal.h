#ifndef CRT_PRIVATE_CRT_SIGNAL_H
#define CRT_PRIVATE_CRT_SIGNAL_H

#include <signal.h>

void __crt_signal_get_mask(sigset64_t* mask);
void __crt_signal_set_mask(sigset64_t mask);
void __crt_signal_reset_defaults(sigset64_t mask);

/* Monotonically increasing count of how many times a real signal handler has
 * actually been invoked (SIG_IGN and default-disposition deliveries don't
 * count, matching real EINTR semantics: those never "catch" a signal).
 * pselect() (libc/src/poll.c) uses this to detect a signal that was already
 * pending and got delivered synchronously by the very sigprocmask() call
 * that unblocks it -- real kernel signal delivery happens on the way back to
 * userspace from any syscall, so this always happens before sigprocmask()
 * returns, not merely "at some later point" -- which a plain, non-atomic
 * "unblock, then separately call select()" sequence would otherwise miss
 * entirely (the classic pselect() lost-wakeup problem: a signal that arrives
 * or was already pending right as it is unblocked, before the wait actually
 * begins, must still count as an interruption). */
unsigned long __crt_signal_delivery_generation(void);

#endif
