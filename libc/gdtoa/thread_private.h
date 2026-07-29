/*
 * Minimal OpenBSD thread-private adapter for the imported gdtoa sources.
 *
 * Bionic builds these files with Android's OpenBSD compatibility layer.  This
 * project keeps the gdtoa source shape but routes its internal locks through
 * the local pthread implementation.
 */
#pragma once

#include <pthread.h>

__BEGIN_DECLS

void __crt_gdtoa_mutex_lock(void* lock_slot);
void __crt_gdtoa_mutex_unlock(void* lock_slot);

#define _MUTEX_LOCK(lock_slot) __crt_gdtoa_mutex_lock((void*)(lock_slot))
#define _MUTEX_UNLOCK(lock_slot) __crt_gdtoa_mutex_unlock((void*)(lock_slot))

__END_DECLS
