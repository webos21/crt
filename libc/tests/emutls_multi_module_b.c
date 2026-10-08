#include "emutls_multi_module.h"

__declspec(dllimport) void* __emutls_get_address(emutls_test_control* control);
__declspec(dllimport) extern emutls_test_control emutls_shared_control;

static const long private_initial[2] = {201, 202};
static emutls_test_control private_controls[2] = {
    {sizeof(long), sizeof(long), 0, (void*)&private_initial[0]},
    {sizeof(long), sizeof(long), 0, (void*)&private_initial[1]},
};

static __thread long compiled_tls = 401;

long emutls_b_shared_get(void) {
  return *(long*)__emutls_get_address(&emutls_shared_control);
}

void emutls_b_shared_set(long value) {
  *(long*)__emutls_get_address(&emutls_shared_control) = value;
}

void emutls_b_touch_private(void) {
  (void)__emutls_get_address(&private_controls[0]);
  (void)__emutls_get_address(&private_controls[1]);
}

uintptr_t emutls_b_private_index(int which) {
  return which >= 0 && which < 2 ? private_controls[which].index : 0;
}

long emutls_b_compiled_get(void) { return compiled_tls; }
void emutls_b_compiled_set(long value) { compiled_tls = value; }
