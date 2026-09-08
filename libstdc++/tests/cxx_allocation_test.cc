extern "C" int printf(const char* format, ...);

static int fail(const char* message) {
  printf("cxx_allocation_test: %s\n", message);
  return 1;
}

extern "C" int main() {
  int* single = new int(42);
  int* array = new int[4];

  if (single == nullptr || array == nullptr) {
    return fail("new");
  }
  if (*single != 42) {
    return fail("new value");
  }
  array[0] = 7;
  array[3] = 11;
  if (array[0] != 7 || array[3] != 11) {
    return fail("new array value");
  }

  delete single;
  delete[] array;
  printf("cxx_allocation_test: ok\n");
  return 0;
}
