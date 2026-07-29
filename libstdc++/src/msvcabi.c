#include <sched.h>
#include <stdlib.h>

#include <private/crt_atomic.h>

#define CRT_MSVC_GUARD_UNINITIALIZED 0
#define CRT_MSVC_GUARD_INITIALIZING (-1)

long _Init_global_epoch;
long _Init_thread_epoch;

static crt_spinlock crt_msvc_init_lock = CRT_SPINLOCK_INIT;

void _Init_thread_header(volatile int* guard) {
  for (;;) {
    int state;

    crt_spin_lock(&crt_msvc_init_lock);
    state = *guard;
    if (state == CRT_MSVC_GUARD_UNINITIALIZED) {
      *guard = CRT_MSVC_GUARD_INITIALIZING;
      crt_spin_unlock(&crt_msvc_init_lock);
      return;
    }
    if (state != CRT_MSVC_GUARD_INITIALIZING) {
      crt_spin_unlock(&crt_msvc_init_lock);
      return;
    }
    crt_spin_unlock(&crt_msvc_init_lock);

    sched_yield();
  }
}

void _Init_thread_footer(volatile int* guard) {
  crt_spin_lock(&crt_msvc_init_lock);
  ++_Init_global_epoch;
  if (_Init_global_epoch <= 0) {
    _Init_global_epoch = 1;
  }
  *guard = (int)_Init_global_epoch;
  _Init_thread_epoch = _Init_global_epoch;
  crt_spin_unlock(&crt_msvc_init_lock);
}

void _Init_thread_abort(volatile int* guard) {
  crt_spin_lock(&crt_msvc_init_lock);
  *guard = CRT_MSVC_GUARD_UNINITIALIZED;
  crt_spin_unlock(&crt_msvc_init_lock);
}

int _purecall(void) {
  abort();
  return 0;
}
