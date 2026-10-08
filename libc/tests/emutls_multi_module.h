#ifndef CRT_TESTS_EMUTLS_MULTI_MODULE_H
#define CRT_TESTS_EMUTLS_MULTI_MODULE_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
  size_t size;
  size_t align;
  uintptr_t index;
  void* templ;
} emutls_test_control;

#if defined(emutls_module_a_EXPORTS)
#define EMUTLS_A_API __declspec(dllexport)
#else
#define EMUTLS_A_API __declspec(dllimport)
#endif
#if defined(emutls_module_b_EXPORTS)
#define EMUTLS_B_API __declspec(dllexport)
#else
#define EMUTLS_B_API __declspec(dllimport)
#endif

EMUTLS_A_API long emutls_a_shared_get(void);
EMUTLS_A_API void emutls_a_shared_set(long value);
EMUTLS_B_API long emutls_b_shared_get(void);
EMUTLS_B_API void emutls_b_shared_set(long value);
EMUTLS_A_API void emutls_a_touch_private(void);
EMUTLS_B_API void emutls_b_touch_private(void);
EMUTLS_A_API uintptr_t emutls_a_private_index(int which);
EMUTLS_B_API uintptr_t emutls_b_private_index(int which);
EMUTLS_A_API uintptr_t emutls_shared_index(void);
EMUTLS_A_API long emutls_a_compiled_get(void);
EMUTLS_A_API void emutls_a_compiled_set(long value);
EMUTLS_B_API long emutls_b_compiled_get(void);
EMUTLS_B_API void emutls_b_compiled_set(long value);

#endif
