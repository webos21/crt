// The interpreter owns TLSDESC; no explicit loader import or host libc is needed.
// Link with the shared CRT runtime without --allow-shlib-undefined.
#include <thread>
#include <cstdio>
#include <atomic>
#include <cstdlib>
extern "C" int crt_shared_tls(int);
extern "C" void crt_shared_throw();
static std::atomic<int> destroyed{0};
struct Tracker { ~Tracker() { ++destroyed; } };
thread_local Tracker tracker;
thread_local int slot = 7;
int main() {
  int *allocation = static_cast<int *>(calloc(4, sizeof(int)));
  if (!allocation || allocation[0] != 0) return 2;
  free(allocation);
  if (crt_shared_tls(19) != 7) return 3;
  try { crt_shared_throw(); return 4; }
  catch (int value) { if (value != 73) return 5; }
  bool ok = false;
  std::thread worker([&] { (void)&tracker; slot = 13; ok = slot == 13 && crt_shared_tls(23) == 7; });
  worker.join();
  if (!ok || slot != 7 || destroyed != 1 || crt_shared_tls(29) != 19) return 1;
  puts("shared_loader_import_test: ok");
  return 0;
}
