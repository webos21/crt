// Signal-based VM traps (Web Tranche 1C). With the JIT on, JavaScriptCore interrupts compiled
// code from another thread with a signal whose handler edits the interrupted ucontext_t. This
// runs an infinite loop that the JIT compiles almost at once, with an execution-time limit set
// through the C API, and requires that the loop is terminated -- on the main thread and on a
// worker thread, several times -- instead of hanging or crashing. Run with the JIT options.
#include <JavaScriptCore/JavaScript.h>

#include <dlfcn.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ucontext.h>
#include <sys/syscall.h>
#include <unistd.h>

// Declared in JSContextRefPrivate.h, which the build does not install; exported by the library.
typedef bool (*JSShouldTerminateCallback)(JSContextRef ctx, void* context);
extern "C" void JSContextGroupSetExecutionTimeLimit(JSContextGroupRef group, double limit,
                                                    JSShouldTerminateCallback callback, void* context);
extern "C" void JSContextGroupClearExecutionTimeLimit(JSContextGroupRef group);

static int callback_calls;

// A fatal-signal reporter, so a crash in the signal-based trap path names the faulting
// instruction and thread instead of dying silently. It uses the real siginfo_t and
// ucontext_t the CRT forwards to SA_SIGINFO handlers (and so also checks that they arrive).
static void report_crash(int sig, siginfo_t* info, void* context) {
  char line[512];
  int n = 0;
  const ucontext_t* uc = static_cast<const ucontext_t*>(context);
  unsigned long pc = 0, sp = 0, bp = 0;
#if defined(__x86_64__)
  if (uc) { pc = uc->uc_mcontext.gregs[REG_RIP]; sp = uc->uc_mcontext.gregs[REG_RSP]; bp = uc->uc_mcontext.gregs[REG_RBP]; }
#elif defined(__aarch64__)
  if (uc) { pc = uc->uc_mcontext.pc; sp = uc->uc_mcontext.sp; bp = uc->uc_mcontext.regs[29]; }
#endif
  Dl_info where;
  const char* name = "?";
  const char* module = "?";
  unsigned long offset = 0;
  if (dladdr(reinterpret_cast<void*>(pc), &where)) {
    if (where.dli_sname) name = where.dli_sname;
    if (where.dli_fname) module = where.dli_fname;
    offset = pc - reinterpret_cast<unsigned long>(where.dli_saddr ? where.dli_saddr : where.dli_fbase);
  }
  n = snprintf(line, sizeof line, "jsc_watchdog_test: CRASH signal=%d code=%d addr=%p pc=%#lx sp=%#lx in %s+%#lx (%s) tid=%ld\n",
               sig, info ? info->si_code : -1, info ? info->si_addr : nullptr, pc, sp, name, offset, module,
               static_cast<long>(syscall(SYS_gettid)));
  write(2, line, n);
#if defined(__x86_64__)
  if (uc) {
    static const char* const names[] = {"r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", "rdi", "rsi",
                                        "rbp", "rbx", "rdx", "rax", "rcx", "rsp", "rip"};
    n = 0;
    for (int i = 0; i < 17; ++i)
      n += snprintf(line + n, sizeof line - n, "%s=%#lx ", names[i], (unsigned long)uc->uc_mcontext.gregs[i]);
    n += snprintf(line + n, sizeof line - n, "\n");
    write(2, line, n);
    n = snprintf(line, sizeof line, "  code bytes before/after pc:");
    const unsigned char* code = reinterpret_cast<const unsigned char*>(pc);
    for (int i = -24; i < 8; ++i) n += snprintf(line + n, sizeof line - n, " %02x", code[i]);
    n += snprintf(line + n, sizeof line - n, "\n");
    write(2, line, n);
  }
#endif
  unsigned long* frame = reinterpret_cast<unsigned long*>(bp);
  for (int i = 0; i < 12 && frame && (reinterpret_cast<unsigned long>(frame) & 7) == 0 &&
                  reinterpret_cast<unsigned long>(frame) > sp; ++i) {
    unsigned long ret = frame[1];
    Dl_info f;
    const char* fname = dladdr(reinterpret_cast<void*>(ret), &f) && f.dli_sname ? f.dli_sname : "?";
    n = snprintf(line, sizeof line, "  frame %d: %#lx %s\n", i, ret, fname);
    write(2, line, n);
    frame = reinterpret_cast<unsigned long*>(frame[0]);
  }
  _exit(139);
}

static bool should_terminate(JSContextRef, void*) {
  __atomic_fetch_add(&callback_calls, 1, __ATOMIC_RELAXED);
  return true;
}

static double now_seconds() {
  timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec / 1e9;
}

// Returns true when the loop was terminated by the time limit.
static bool run_one(const char* label) {
  JSContextGroupRef group = JSContextGroupCreate();
  JSGlobalContextRef context = JSGlobalContextCreateInGroup(group, nullptr);
  JSContextGroupSetExecutionTimeLimit(group, 0.25, should_terminate, nullptr);
  // The loop runs inside a function that is entered many times first, so the JIT has
  // compiled it before it is made to spin.
  const char* source =
      "function spin(limit) { var n = 0; for (var i = 0; i < limit; i++) n += i & 3; return n; }"
      "for (var warm = 0; warm < 3000; warm++) spin(200);"
      "var counter = 0; for (;;) counter += spin(1000);";
  JSStringRef script = JSStringCreateWithUTF8CString(source);
  JSValueRef exception = nullptr;
  double started = now_seconds();
  JSValueRef value = JSEvaluateScript(context, script, nullptr, nullptr, 1, &exception);
  double elapsed = now_seconds() - started;
  JSStringRelease(script);
  bool terminated = value == nullptr && exception != nullptr && elapsed < 10.0;
  printf("jsc_watchdog_test: %s terminated=%d elapsed=%.2fs\n", label, terminated ? 1 : 0, elapsed);
  JSContextGroupClearExecutionTimeLimit(group);
  JSGlobalContextRelease(context);
  JSContextGroupRelease(group);
  return terminated;
}

static void* worker(void* argument) {
  bool* ok = static_cast<bool*>(argument);
  *ok = run_one("worker");
  return nullptr;
}

int main() {
  struct sigaction crash;
  memset(&crash, 0, sizeof crash);
  crash.sa_sigaction = report_crash;
  crash.sa_flags = SA_SIGINFO;
  sigaction(SIGSEGV, &crash, nullptr);
  sigaction(SIGBUS, &crash, nullptr);
  bool ok = true;
  for (int i = 0; i < 3; ++i) ok = run_one("main") && ok;
  for (int i = 0; i < 3; ++i) {
    pthread_t thread;
    pthread_attr_t attributes;
    bool thread_ok = false;
    pthread_attr_init(&attributes);
    pthread_attr_setstacksize(&attributes, 8u * 1024 * 1024);
    pthread_create(&thread, &attributes, worker, &thread_ok);
    pthread_join(thread, nullptr);
    pthread_attr_destroy(&attributes);
    ok = thread_ok && ok;
  }
  printf("jsc_watchdog_test: %s callback_calls=%d\n", ok ? "ok" : "FAILED", callback_calls);
  return ok ? 0 : 1;
}
