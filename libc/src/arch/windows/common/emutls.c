/* Process-wide emulated TLS for Windows.
 *
 * Clang's -femulated-tls lowers __thread/thread_local accesses to this ABI.
 * The implementation must live in libc.dll rather than in compiler-rt (or in
 * each consumer DLL): PE/COFF weak coalescing does not merge the independent
 * runtime state from multiple images.  A control object referenced by two DLLs
 * would otherwise receive one numeric index but address two unrelated per-DLL
 * slot arrays, so the DLLs would not observe the same per-thread object.
 *
 * compiler-rt's Windows implementation also registers process-exit teardown.
 * exit() runs atexit handlers while other threads can still execute, so freeing
 * the TLS key there caused JavaScriptCore compiler threads to abort after the
 * script had completed.  This runtime deliberately has no process-exit
 * teardown; Windows reclaims the key and allocations when the process ends.
 * Per-thread reclamation is tracked separately from this correctness boundary.
 *
 * The control and array layouts follow compiler-rt's public emutls ABI.  An
 * already-numbered control can arrive through another module, so observing one
 * also advances the process-wide high-water mark.  That prevents a later local
 * control from reusing the same index.
 */
#include <malloc.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

__declspec(dllimport) unsigned long __stdcall TlsAlloc(void);
__declspec(dllimport) void* __stdcall TlsGetValue(unsigned long index);
__declspec(dllimport) int __stdcall TlsSetValue(unsigned long index, void* value);
__declspec(dllimport) int __stdcall SwitchToThread(void);

typedef struct {
  size_t size;
  size_t align;
  uintptr_t index; /* 0: not numbered yet */
  void* templ;
} crt_emutls_control;

typedef struct {
  uintptr_t reserved;
  uintptr_t size; /* slots */
  void* data[1];
} crt_emutls_array;

static volatile long crt_emutls_state; /* 0 untouched, 1 initialising, 2 ready */
static unsigned long crt_emutls_key;
static volatile uintptr_t crt_emutls_count;

static void crt_emutls_init(void) {
  long expected = 0;

  if (__atomic_load_n(&crt_emutls_state, __ATOMIC_ACQUIRE) == 2) return;
  if (__atomic_compare_exchange_n(&crt_emutls_state, &expected, 1, 0,
                                  __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
    crt_emutls_key = TlsAlloc();
    if (crt_emutls_key == 0xFFFFFFFFUL) abort();
    __atomic_store_n(&crt_emutls_state, 2, __ATOMIC_RELEASE);
    return;
  }
  while (__atomic_load_n(&crt_emutls_state, __ATOMIC_ACQUIRE) != 2) {
    SwitchToThread();
  }
}

static void crt_emutls_reserve_index(uintptr_t index) {
  uintptr_t count = __atomic_load_n(&crt_emutls_count, __ATOMIC_ACQUIRE);

  while (count < index &&
         !__atomic_compare_exchange_n(&crt_emutls_count, &count, index, 0,
                                      __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
  }
}

void* __emutls_get_address(crt_emutls_control* control) {
  uintptr_t index;
  crt_emutls_array* array;
  void** slot;

  crt_emutls_init();
  index = __atomic_load_n(&control->index, __ATOMIC_ACQUIRE);
  if (index == 0) {
    uintptr_t number = __atomic_add_fetch(&crt_emutls_count, 1, __ATOMIC_SEQ_CST);
    uintptr_t none = 0;

    __atomic_compare_exchange_n(&control->index, &none, number, 0,
                                __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    index = __atomic_load_n(&control->index, __ATOMIC_ACQUIRE);
  } else {
    crt_emutls_reserve_index(index);
  }

  array = (crt_emutls_array*)TlsGetValue(crt_emutls_key);
  if (array == 0 || array->size < index) {
    uintptr_t slots = (index + 15) & ~(uintptr_t)15;
    uintptr_t old_slots = array != 0 ? array->size : 0;
    crt_emutls_array* grown =
        (crt_emutls_array*)realloc(array, sizeof(uintptr_t) * 2 + slots * sizeof(void*));

    if (grown == 0) abort();
    if (array == 0) grown->reserved = 0;
    memset(&grown->data[old_slots], 0, (slots - old_slots) * sizeof(void*));
    grown->size = slots;
    if (!TlsSetValue(crt_emutls_key, grown)) abort();
    array = grown;
  }

  slot = &array->data[index - 1];
  if (*slot == 0) {
    size_t align = control->align < sizeof(void*) ? sizeof(void*) : control->align;
    void* object = _aligned_malloc(control->size, align);

    if (object == 0) abort();
    if (control->templ != 0) {
      memcpy(object, control->templ, control->size);
    } else {
      memset(object, 0, control->size);
    }
    *slot = object;
  }
  return *slot;
}
