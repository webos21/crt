#ifndef CRT_SYS_UCONTEXT_H
#define CRT_SYS_UCONTEXT_H

/* Bionic defines ucontext_t/mcontext_t in <sys/ucontext.h> and <ucontext.h>
 * includes it; CRT keeps the definitions in <ucontext.h>, so this header is the
 * Bionic spelling for code (WTF's signal handling) that includes it directly. */
#include <ucontext.h>

#endif
