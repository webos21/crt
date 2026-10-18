// The dynamic loader, not a host libc or an empty import DSO, owns ELF GD TLS.
// Link with the shared CRT runtime without --allow-shlib-undefined.
#include <thread>
#include <cstdio>
#include <atomic>
static std::atomic<int> destroyed{0};
struct Tracker { ~Tracker() { ++destroyed; } };
thread_local Tracker tracker;
thread_local int slot = 7;
int main() {
  bool ok = false;
  std::thread worker([&] { (void)&tracker; slot = 13; ok = slot == 13; });
  worker.join();
  if (!ok || slot != 7 || destroyed != 1) return 1;
  puts("shared_loader_import_test: ok");
  return 0;
}
