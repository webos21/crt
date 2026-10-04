/* The executable-memory contract a JIT depends on: map, write machine code, make it
 * executable, run it, make it writable again, patch it, run it again. JavaScriptCore's
 * WTF allocates Linux executable memory in other ways too -- one mmap with
 * PROT_READ|PROT_WRITE|PROT_EXEC, and a large PROT_NONE/MAP_NORESERVE reservation whose
 * pages are committed later with mprotect -- so each shape is checked here, before
 * JavaScriptCore is blamed for an executable-memory defect that belongs to the CRT.
 * x86_64 and aarch64 only (a tiny function returning a constant); other hosts skip. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef int (*int_function)(void);

#if defined(__x86_64__)
/* mov eax, imm32 ; ret */
static void emit_return_constant(unsigned char* code, int value) {
  code[0] = 0xB8;
  memcpy(code + 1, &value, 4);
  code[5] = 0xC3;
}
static size_t code_size(void) { return 6; }
#elif defined(__aarch64__)
/* movz w0, #imm16 ; ret */
static void emit_return_constant(unsigned char* code, int value) {
  uint32_t movz = 0x52800000u | ((uint32_t)(value & 0xFFFF) << 5);
  uint32_t ret = 0xD65F03C0u;
  memcpy(code, &movz, 4);
  memcpy(code + 4, &ret, 4);
}
static size_t code_size(void) { return 8; }
#endif

#if defined(__x86_64__) || defined(__aarch64__)
static int failures;

#define CHECK(condition, message)                                            \
  do {                                                                       \
    if (!(condition)) {                                                      \
      fprintf(stderr, "jit_memory_test: %s (line %d)\n", message, __LINE__); \
      ++failures;                                                            \
    }                                                                        \
  } while (0)

static void flush_instructions(void* begin, size_t size) {
  __builtin___clear_cache((char*)begin, (char*)begin + size);
}

static int call_at(void* code) {
  int_function function;
  memcpy(&function, &code, sizeof(function));
  return function();
}

static void test_rw_then_rx(size_t page) {
  unsigned char* memory = mmap(0, page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  CHECK(memory != MAP_FAILED, "mmap RW");
  if (memory == MAP_FAILED) return;
  emit_return_constant(memory, 42);
  flush_instructions(memory, code_size());
  CHECK(mprotect(memory, page, PROT_READ | PROT_EXEC) == 0, "mprotect RX");
  CHECK(call_at(memory) == 42, "generated code returns 42");
  CHECK(mprotect(memory, page, PROT_READ | PROT_WRITE) == 0, "mprotect back to RW");
  emit_return_constant(memory, 1234);
  flush_instructions(memory, code_size());
  CHECK(mprotect(memory, page, PROT_READ | PROT_EXEC) == 0, "mprotect RX again");
  CHECK(call_at(memory) == 1234, "patched code returns 1234");
  CHECK(munmap(memory, page) == 0, "munmap");
}

static void test_rwx_in_one_call(size_t page) {
  /* What WTF's POSIX allocator does on Linux: one mmap, writable and executable together. */
  unsigned char* memory = mmap(0, page, PROT_READ | PROT_WRITE | PROT_EXEC,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  CHECK(memory != MAP_FAILED, "mmap RWX in one call");
  if (memory == MAP_FAILED) return;
  emit_return_constant(memory, 7);
  flush_instructions(memory, code_size());
  CHECK(call_at(memory) == 7, "code in RWX memory runs");
  emit_return_constant(memory, 8);
  flush_instructions(memory, code_size());
  CHECK(call_at(memory) == 8, "code patched in place in RWX memory runs");
  CHECK(munmap(memory, page) == 0, "munmap RWX");
}

static void test_reserve_then_commit(size_t page) {
  /* A large address-space reservation (WTF's ExecutableAllocator pool), committed in pieces. */
  const size_t reservation = 256UL * 1024 * 1024;
  unsigned char* pool = mmap(0, reservation, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  CHECK(pool != MAP_FAILED, "reserve 256 MiB PROT_NONE|MAP_NORESERVE");
  if (pool == MAP_FAILED) return;
  {
    unsigned char* second = pool + 5 * page;
    CHECK(mprotect(second, page, PROT_READ | PROT_WRITE | PROT_EXEC) == 0, "commit one page RWX");
    emit_return_constant(second, 99);
    flush_instructions(second, code_size());
    CHECK(call_at(second) == 99, "code in a committed page of the reservation runs");
    CHECK(madvise(second, page, MADV_DONTNEED) == 0, "madvise(MADV_DONTNEED) on a committed page");
    CHECK(mprotect(second, page, PROT_NONE) == 0, "decommit back to PROT_NONE");
  }
  CHECK(munmap(pool, reservation) == 0, "munmap the reservation");
}
#endif

int main(void) {
#if defined(__x86_64__) || defined(__aarch64__)
  size_t page = (size_t)sysconf(_SC_PAGESIZE);
  test_rw_then_rx(page);
#if defined(__APPLE__) && defined(__aarch64__)
  /* Apple Silicon refuses a mapping or mprotect that is writable and executable at once unless
   * it was mmap'd with MAP_JIT (and then toggled per thread with pthread_jit_write_protect_np).
   * This libc does not provide MAP_JIT yet -- Darwin's value 0x800 collides with the Bionic
   * MAP_DENYWRITE bit, which mmap() strips -- so the two WTF shapes that need it (one-call RWX
   * and a committed RWX page) cannot run here. Web Tranche 1 macOS replay, docs/crtweb_acceptance.md. */
  if (failures != 0) {
    fprintf(stderr, "jit_memory_test: %d check(s) failed\n", failures);
    return 1;
  }
  printf("jit_memory_test: ok (macOS/arm64: RW->RX only; RWX shapes need MAP_JIT, not provided yet)\n");
  return 0;
#endif
  test_rwx_in_one_call(page);
  test_reserve_then_commit(page);
  if (failures != 0) {
    fprintf(stderr, "jit_memory_test: %d check(s) failed\n", failures);
    return 1;
  }
  printf("jit_memory_test: ok\n");
#else
  printf("jit_memory_test: ok (skipped: no code generator for this architecture)\n");
#endif
  return 0;
}
