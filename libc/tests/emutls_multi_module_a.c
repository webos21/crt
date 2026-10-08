#include "emutls_multi_module.h"

__declspec(dllimport) void* __emutls_get_address(emutls_test_control* control);

static const long shared_initial = 11;
static const long private_initial[2] = {101, 102};

__declspec(dllexport) emutls_test_control emutls_shared_control = {
    sizeof(long), sizeof(long), 0, (void*)&shared_initial};

static emutls_test_control private_controls[2] = {
    {sizeof(long), sizeof(long), 0, (void*)&private_initial[0]},
    {sizeof(long), sizeof(long), 0, (void*)&private_initial[1]},
};

static __thread long compiled_tls = 301;

long emutls_a_shared_get(void) {
  return *(long*)__emutls_get_address(&emutls_shared_control);
}

void emutls_a_shared_set(long value) {
  *(long*)__emutls_get_address(&emutls_shared_control) = value;
}

void emutls_a_touch_private(void) {
  (void)__emutls_get_address(&private_controls[0]);
  (void)__emutls_get_address(&private_controls[1]);
}

uintptr_t emutls_a_private_index(int which) {
  return which >= 0 && which < 2 ? private_controls[which].index : 0;
}

uintptr_t emutls_shared_index(void) {
  return emutls_shared_control.index;
}

long emutls_a_compiled_get(void) { return compiled_tls; }
void emutls_a_compiled_set(long value) { compiled_tls = value; }
