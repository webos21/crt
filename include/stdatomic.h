#ifndef CRT_STDATOMIC_H
#define CRT_STDATOMIC_H

/* A real, public C11 <stdatomic.h>, implemented as a thin wrapper over
 * Clang's __c11_atomic_* builtins -- matches real Bionic's own approach
 * (also a thin wrapper over the same class of compiler builtins), and this
 * project's existing private libc/include/private/crt_atomic.h internal
 * layer, just exposed generically and publicly for real C11 consumers
 * (QuickJS/V8/Skia all commonly include <stdatomic.h> directly) instead of
 * that layer's narrow int-only subset. See docs/bionic_libc_gaps.md and
 * HISTORY.md's 2026-08-16 entry.
 *
 * This project builds with -std=gnu99 (see shell/CMakeLists.txt and
 * friends), not -std=c11, so C11's _Generic isn't available -- a portable
 * pure-C11 <stdatomic.h> written against the standard's own text would
 * normally lean on _Generic for its type-generic operation macros. That
 * turns out not to matter here: __c11_atomic_* are themselves already
 * type-generic compiler builtins (they infer the pointee type straight
 * from the _Atomic-qualified pointer argument), so plain object-like/
 * function-like macros are enough. _Atomic itself is a Clang language
 * extension available regardless of -std=gnu99 vs -std=c11 -- verified
 * directly against this project's exact compiler and build flags before
 * writing this file, not assumed from the C11 spec text. */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  memory_order_relaxed = __ATOMIC_RELAXED,
  memory_order_consume = __ATOMIC_CONSUME,
  memory_order_acquire = __ATOMIC_ACQUIRE,
  memory_order_release = __ATOMIC_RELEASE,
  memory_order_acq_rel = __ATOMIC_ACQ_REL,
  memory_order_seq_cst = __ATOMIC_SEQ_CST
} memory_order;

#define ATOMIC_BOOL_LOCK_FREE __CLANG_ATOMIC_BOOL_LOCK_FREE
#define ATOMIC_CHAR_LOCK_FREE __CLANG_ATOMIC_CHAR_LOCK_FREE
#define ATOMIC_CHAR16_T_LOCK_FREE __CLANG_ATOMIC_CHAR16_T_LOCK_FREE
#define ATOMIC_CHAR32_T_LOCK_FREE __CLANG_ATOMIC_CHAR32_T_LOCK_FREE
#define ATOMIC_WCHAR_T_LOCK_FREE __CLANG_ATOMIC_WCHAR_T_LOCK_FREE
#define ATOMIC_SHORT_LOCK_FREE __CLANG_ATOMIC_SHORT_LOCK_FREE
#define ATOMIC_INT_LOCK_FREE __CLANG_ATOMIC_INT_LOCK_FREE
#define ATOMIC_LONG_LOCK_FREE __CLANG_ATOMIC_LONG_LOCK_FREE
#define ATOMIC_LLONG_LOCK_FREE __CLANG_ATOMIC_LLONG_LOCK_FREE
#define ATOMIC_POINTER_LOCK_FREE __CLANG_ATOMIC_POINTER_LOCK_FREE

/* Core scalar types. char16_t/char32_t are intentionally omitted -- they'd
 * need uchar.h, itself a documented, still-open gap (see
 * docs/bionic_libc_gaps.md's "Lower priority" list); add
 * atomic_char16_t/atomic_char32_t alongside whenever that header lands. */
typedef _Atomic(_Bool) atomic_bool;
typedef _Atomic(char) atomic_char;
typedef _Atomic(signed char) atomic_schar;
typedef _Atomic(unsigned char) atomic_uchar;
typedef _Atomic(short) atomic_short;
typedef _Atomic(unsigned short) atomic_ushort;
typedef _Atomic(int) atomic_int;
typedef _Atomic(unsigned int) atomic_uint;
typedef _Atomic(long) atomic_long;
typedef _Atomic(unsigned long) atomic_ulong;
typedef _Atomic(long long) atomic_llong;
typedef _Atomic(unsigned long long) atomic_ullong;
typedef _Atomic(wchar_t) atomic_wchar_t;

/* stdint.h-backed types -- these are the ones real-world code (reference
 * counts, generation counters, atomic pointer swaps) reaches for most. */
typedef _Atomic(int_least8_t) atomic_int_least8_t;
typedef _Atomic(uint_least8_t) atomic_uint_least8_t;
typedef _Atomic(int_least16_t) atomic_int_least16_t;
typedef _Atomic(uint_least16_t) atomic_uint_least16_t;
typedef _Atomic(int_least32_t) atomic_int_least32_t;
typedef _Atomic(uint_least32_t) atomic_uint_least32_t;
typedef _Atomic(int_least64_t) atomic_int_least64_t;
typedef _Atomic(uint_least64_t) atomic_uint_least64_t;
typedef _Atomic(int_fast8_t) atomic_int_fast8_t;
typedef _Atomic(uint_fast8_t) atomic_uint_fast8_t;
typedef _Atomic(int_fast16_t) atomic_int_fast16_t;
typedef _Atomic(uint_fast16_t) atomic_uint_fast16_t;
typedef _Atomic(int_fast32_t) atomic_int_fast32_t;
typedef _Atomic(uint_fast32_t) atomic_uint_fast32_t;
typedef _Atomic(int_fast64_t) atomic_int_fast64_t;
typedef _Atomic(uint_fast64_t) atomic_uint_fast64_t;
typedef _Atomic(intptr_t) atomic_intptr_t;
typedef _Atomic(uintptr_t) atomic_uintptr_t;
typedef _Atomic(size_t) atomic_size_t;
typedef _Atomic(ptrdiff_t) atomic_ptrdiff_t;
typedef _Atomic(intmax_t) atomic_intmax_t;
typedef _Atomic(uintmax_t) atomic_uintmax_t;

#define atomic_init(object, desired) __c11_atomic_init(object, desired)

#define atomic_thread_fence(order) __c11_atomic_thread_fence(order)
#define atomic_signal_fence(order) __c11_atomic_signal_fence(order)

#define atomic_is_lock_free(object) __c11_atomic_is_lock_free(sizeof(*(object)))

#define atomic_store_explicit(object, desired, order) \
  __c11_atomic_store(object, desired, order)
#define atomic_store(object, desired) \
  atomic_store_explicit(object, desired, memory_order_seq_cst)

#define atomic_load_explicit(object, order) __c11_atomic_load(object, order)
#define atomic_load(object) atomic_load_explicit(object, memory_order_seq_cst)

#define atomic_exchange_explicit(object, desired, order) \
  __c11_atomic_exchange(object, desired, order)
#define atomic_exchange(object, desired) \
  atomic_exchange_explicit(object, desired, memory_order_seq_cst)

#define atomic_compare_exchange_strong_explicit( \
    object, expected, desired, success, failure) \
  __c11_atomic_compare_exchange_strong(object, expected, desired, success, failure)
#define atomic_compare_exchange_strong(object, expected, desired) \
  atomic_compare_exchange_strong_explicit( \
      object, expected, desired, memory_order_seq_cst, memory_order_seq_cst)

#define atomic_compare_exchange_weak_explicit( \
    object, expected, desired, success, failure) \
  __c11_atomic_compare_exchange_weak(object, expected, desired, success, failure)
#define atomic_compare_exchange_weak(object, expected, desired) \
  atomic_compare_exchange_weak_explicit( \
      object, expected, desired, memory_order_seq_cst, memory_order_seq_cst)

#define atomic_fetch_add_explicit(object, operand, order) \
  __c11_atomic_fetch_add(object, operand, order)
#define atomic_fetch_add(object, operand) \
  atomic_fetch_add_explicit(object, operand, memory_order_seq_cst)

#define atomic_fetch_sub_explicit(object, operand, order) \
  __c11_atomic_fetch_sub(object, operand, order)
#define atomic_fetch_sub(object, operand) \
  atomic_fetch_sub_explicit(object, operand, memory_order_seq_cst)

#define atomic_fetch_or_explicit(object, operand, order) \
  __c11_atomic_fetch_or(object, operand, order)
#define atomic_fetch_or(object, operand) \
  atomic_fetch_or_explicit(object, operand, memory_order_seq_cst)

#define atomic_fetch_xor_explicit(object, operand, order) \
  __c11_atomic_fetch_xor(object, operand, order)
#define atomic_fetch_xor(object, operand) \
  atomic_fetch_xor_explicit(object, operand, memory_order_seq_cst)

#define atomic_fetch_and_explicit(object, operand, order) \
  __c11_atomic_fetch_and(object, operand, order)
#define atomic_fetch_and(object, operand) \
  atomic_fetch_and_explicit(object, operand, memory_order_seq_cst)

/* Deprecated (C17) but still common in the wild. */
#define ATOMIC_VAR_INIT(value) (value)

typedef struct {
  atomic_bool __flag;
} atomic_flag;

#define ATOMIC_FLAG_INIT \
  { 0 }

static inline _Bool atomic_flag_test_and_set_explicit(
    volatile atomic_flag* object, memory_order order) {
  return __c11_atomic_exchange(&object->__flag, 1, order);
}

static inline _Bool atomic_flag_test_and_set(volatile atomic_flag* object) {
  return atomic_flag_test_and_set_explicit(object, memory_order_seq_cst);
}

static inline void atomic_flag_clear_explicit(
    volatile atomic_flag* object, memory_order order) {
  __c11_atomic_store(&object->__flag, 0, order);
}

static inline void atomic_flag_clear(volatile atomic_flag* object) {
  atomic_flag_clear_explicit(object, memory_order_seq_cst);
}

#ifdef __cplusplus
}
#endif

#endif
