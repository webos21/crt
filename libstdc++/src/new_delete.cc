#include <stddef.h>
#include <stdlib.h>

namespace std {
struct nothrow_t;
}

void* operator new(size_t size) {
  return malloc(size);
}

void* operator new[](size_t size) {
  return malloc(size);
}

void* operator new(size_t size, const std::nothrow_t&) noexcept {
  return malloc(size);
}

void* operator new[](size_t size, const std::nothrow_t&) noexcept {
  return malloc(size);
}

void operator delete(void* ptr) noexcept {
  free(ptr);
}

void operator delete[](void* ptr) noexcept {
  free(ptr);
}

void operator delete(void* ptr, size_t) noexcept {
  free(ptr);
}

void operator delete[](void* ptr, size_t) noexcept {
  free(ptr);
}

void operator delete(void* ptr, const std::nothrow_t&) noexcept {
  free(ptr);
}

void operator delete[](void* ptr, const std::nothrow_t&) noexcept {
  free(ptr);
}
