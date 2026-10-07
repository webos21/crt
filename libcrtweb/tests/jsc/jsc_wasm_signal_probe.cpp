// Counts the signal handlers the process ran while a script executed. Tranche 1D-C proves that
// WebAssembly's fast memory turns every out-of-bounds access into a hardware fault that JSC's
// handler converts into a RuntimeError, and that the bounds-checked mode takes no signal at all.
// On Linux the harness counts rt_sigreturn calls with strace; where that does not exist (Windows,
// macOS) this program reads the CRT's own delivery counter before and after the script, which is
// incremented once per handler invocation on every host.
//
//   jsc_wasm_signal_probe <script.js>      prints "jsc_wasm_signal_probe: handled=<n> ok=<0|1>"
#include <JavaScriptCore/JavaScript.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// private/crt_signal.h; exported by libc on every host.
extern "C" unsigned long __crt_signal_delivery_generation(void);

static JSValueRef print_callback(JSContextRef ctx, JSObjectRef, JSObjectRef, size_t argument_count,
                                 const JSValueRef arguments[], JSValueRef*) {
  for (size_t i = 0; i < argument_count; ++i) {
    JSStringRef text = JSValueToStringCopy(ctx, arguments[i], nullptr);
    if (text == nullptr) {
      continue;
    }
    size_t size = JSStringGetMaximumUTF8CStringSize(text);
    char* buffer = static_cast<char*>(malloc(size));
    JSStringGetUTF8CString(text, buffer, size);
    printf("%s%s", i ? " " : "", buffer);
    free(buffer);
    JSStringRelease(text);
  }
  printf("\n");
  return JSValueMakeUndefined(ctx);
}

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: jsc_wasm_signal_probe <script.js>\n");
    return 2;
  }
  FILE* file = fopen(argv[1], "rb");
  if (file == nullptr) {
    fprintf(stderr, "jsc_wasm_signal_probe: cannot open %s\n", argv[1]);
    return 2;
  }
  fseek(file, 0, SEEK_END);
  long length = ftell(file);
  fseek(file, 0, SEEK_SET);
  char* source = static_cast<char*>(malloc(static_cast<size_t>(length) + 1));
  size_t got = fread(source, 1, static_cast<size_t>(length), file);
  fclose(file);
  source[got] = 0;

  JSGlobalContextRef context = JSGlobalContextCreate(nullptr);
  JSObjectRef global = JSContextGetGlobalObject(context);
  JSStringRef print_name = JSStringCreateWithUTF8CString("print");
  JSObjectSetProperty(context, global, print_name,
                      JSObjectMakeFunctionWithCallback(context, print_name, print_callback), 0, nullptr);
  JSStringRelease(print_name);

  JSStringRef script = JSStringCreateWithUTF8CString(source);
  JSValueRef exception = nullptr;
  unsigned long before = __crt_signal_delivery_generation();
  JSEvaluateScript(context, script, nullptr, nullptr, 1, &exception);
  unsigned long after = __crt_signal_delivery_generation();
  JSStringRelease(script);
  JSGlobalContextRelease(context);
  free(source);

  printf("jsc_wasm_signal_probe: handled=%lu ok=%d\n", after - before, exception == nullptr ? 1 : 0);
  return exception == nullptr ? 0 : 1;
}
