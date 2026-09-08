extern "C" int printf(const char* format, ...);
extern "C" void __cxa_finalize(void* dso);
extern "C" void* __dso_handle;

static int constructor_count;
static int destructor_count;

class LocalStatic {
 public:
  LocalStatic() : value_(41) {
    ++constructor_count;
  }

  ~LocalStatic() {
    ++destructor_count;
  }

  int value() const {
    return value_;
  }

 private:
  int value_;
};

static LocalStatic& local_static() {
  static LocalStatic object;
  return object;
}

static int fail(const char* message) {
  printf("cxx_frontend_test: %s\n", message);
  return 1;
}

extern "C" int main() {
  if (local_static().value() != 41) {
    return fail("first local static");
  }
  if (local_static().value() != 41) {
    return fail("second local static");
  }
  if (constructor_count != 1) {
    return fail("constructor count");
  }

#if !defined(CRT_TARGET_OS_WINDOWS)
  __cxa_finalize(&__dso_handle);
  if (destructor_count != 1) {
    return fail("destructor finalize");
  }
#else
  (void)__cxa_finalize;
  (void)__dso_handle;
  if (destructor_count != 0) {
    return fail("unexpected destructor count");
  }
#endif

  printf("cxx_frontend_test: ok\n");
  return 0;
}
