#ifndef CRT_PRIVATE_CRT_SIGNAL_H
#define CRT_PRIVATE_CRT_SIGNAL_H

#include <signal.h>

void __crt_signal_get_mask(sigset64_t* mask);
void __crt_signal_set_mask(sigset64_t mask);
void __crt_signal_reset_defaults(sigset64_t mask);

#endif
