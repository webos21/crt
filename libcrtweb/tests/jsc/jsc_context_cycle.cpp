// Runtime lifecycle acceptance for the JavaScriptCore bring-up (Web Tranche 1B):
// create, use and release a JS global context (and its VM) many times through the
// public C API, run script on worker threads, and check that peak and resident
// memory stay bounded. Linked against libJavaScriptCore and built by
// tools/build_webkit_jsc.py with the CRT compiler wrappers.
#include <JavaScriptCore/JavaScript.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static long resident_kilobytes() {
  FILE* file = fopen("/proc/self/statm", "r");
  long pages = 0, resident = 0;
  if (file == nullptr) return -1;
  if (fscanf(file, "%ld %ld", &pages, &resident) != 2) resident = -1;
  fclose(file);
  return resident < 0 ? -1 : resident * (sysconf(_SC_PAGESIZE) / 1024);
}

static bool run_script(JSGlobalContextRef context, const char* source, double* number_out) {
  JSStringRef script = JSStringCreateWithUTF8CString(source);
  JSValueRef exception = nullptr;
  JSValueRef value = JSEvaluateScript(context, script, nullptr, nullptr, 1, &exception);
  JSStringRelease(script);
  if (exception != nullptr || value == nullptr) {
    static int reported;
    if (exception != nullptr && __atomic_fetch_add(&reported, 1, __ATOMIC_RELAXED) == 0) {
      JSStringRef text = JSValueToStringCopy(context, exception, nullptr);
      char message[256] = "?";
      if (text != nullptr) {
        JSStringGetUTF8CString(text, message, sizeof(message));
        JSStringRelease(text);
      }
      fprintf(stderr, "jsc_context_cycle: script threw: %s\n", message);
    }
    return false;
  }
  if (number_out != nullptr) *number_out = JSValueToNumber(context, value, nullptr);
  return true;
}

static const char* const kWork =
    "var total = 0; var objs = [];"
    "for (var i = 0; i < 4000; i++) { objs.push({ i: i, s: 'v' + i }); total += objs[i].i; }"
    "JSON.parse(JSON.stringify(objs)).length + total";

struct ThreadResult {
  int iterations;
  int failures;
};

static void* worker(void* argument) {
  ThreadResult* result = static_cast<ThreadResult*>(argument);
  for (int i = 0; i < result->iterations; ++i) {
    JSGlobalContextRef context = JSGlobalContextCreate(nullptr);
    double number = 0;
    if (context == nullptr || !run_script(context, kWork, &number) || number != 7998000.0 + 4000) {
      ++result->failures;
    }
    if (context != nullptr) JSGlobalContextRelease(context);
  }
  return nullptr;
}

int main(int argc, char** argv) {
  const int kWarmup = 20;
  const int kCycles = 150;
  int failures = 0;
  long baseline = 0;
  long peak = 0;

  for (int i = 0; i < kWarmup + kCycles; ++i) {
    JSGlobalContextRef context = JSGlobalContextCreate(nullptr);
    double number = 0;
    if (context == nullptr || !run_script(context, kWork, &number) || number != 7998000.0 + 4000) {
      ++failures;
    }
    if (context != nullptr) JSGlobalContextRelease(context);
    JSGarbageCollect(nullptr);
    if (i == kWarmup - 1) baseline = resident_kilobytes();
    long resident = resident_kilobytes();
    if (resident > peak) peak = resident;
  }
  JSGlobalContextRef collector = JSGlobalContextCreate(nullptr);
  JSGarbageCollect(collector);
  JSGlobalContextRelease(collector);
  long after_cycles = resident_kilobytes();

  const int kMaxThreads = 16;
  const int kThreads = argc > 1 ? atoi(argv[1]) : 4;
  pthread_t threads[kMaxThreads];
  ThreadResult results[kMaxThreads];
  for (int t = 0; t < kThreads && t < kMaxThreads; ++t) {
    results[t].iterations = 20;
    results[t].failures = 0;
    // JavaScriptCore wants far more than the 1 MiB default thread stack (as on any
    // platform, an embedder sizes the threads that run script).
    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setstacksize(&attributes, 8u * 1024 * 1024);
    if (pthread_create(&threads[t], &attributes, worker, &results[t]) != 0) ++failures;
    pthread_attr_destroy(&attributes);
  }
  for (int t = 0; t < kThreads && t < kMaxThreads; ++t) {
    pthread_join(threads[t], nullptr);
    failures += results[t].failures;
  }
  long after_threads = resident_kilobytes();

  // Growth after warm-up must stay small: a leaked VM per cycle would add tens of MB
  // each, so 150 cycles would exceed this by a wide margin.
  long growth = after_cycles - baseline;
  bool bounded = baseline > 0 && growth < 64 * 1024;
  printf("jsc_context_cycle: %s cycles=%d threads=%d thread_iterations=%d failures=%d "
         "baseline_kb=%ld after_cycles_kb=%ld growth_kb=%ld peak_kb=%ld after_threads_kb=%ld\n",
         (failures == 0 && bounded) ? "ok" : "FAILED", kCycles, kThreads, 20, failures, baseline,
         after_cycles, growth, peak, after_threads);
  return (failures == 0 && bounded) ? 0 : 1;
}
