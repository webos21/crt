#ifndef CRT_PRIVATE_CRT_SIGNAL_WAIT_H
#define CRT_PRIVATE_CRT_SIGNAL_WAIT_H

/* Windows: blocking waits a thread-directed signal must be able to interrupt (the backend's other
 * delivery path, thread suspension, never touches a thread parked in operating-system wait code).
 * A wait brackets its blocking call with wait_begin()/wait_end(); a sender that finds the thread in a
 * wait wakes it (WakeByAddressAll on `address`, or the thread's wake event) instead, and wait_end()
 * runs the handlers of the signals that arrived, on the waiting thread, with a ucontext_t describing
 * the waiting call (stack pointer and callee-saved registers, which is what a conservative stack scan
 * such as JavaScriptCore's needs). */
#if defined(CRT_TARGET_OS_WINDOWS)
/* Returns nonzero when a signal is already deliverable: do not block, call wait_end() and re-check.
 * `address` is the futex-style address the wait is parked on, or 0 for a handle wait (the caller
 * then also waits on __crt_windows_signal_wake_event()). */
int __crt_windows_signal_wait_begin(volatile void* address);
/* Leaves the wait; returns 1 when a handler ran. */
int __crt_windows_signal_wait_end(void);
void* __crt_windows_signal_wake_event(void);
/* WaitForSingleObject() that a signal's handler interrupts (and resumes afterwards). */
unsigned long __crt_windows_signal_wait_handle(void* handle, unsigned long milliseconds);
/* Sleeps `milliseconds`; returns 1 early when a handler ran. */
int __crt_windows_signal_sleep(unsigned long milliseconds);
#endif

#endif
