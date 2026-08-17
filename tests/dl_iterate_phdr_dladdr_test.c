/* dl_iterate_phdr()/dladdr() -- see docs/dynamic_loading.md and
 * docs/bionic_libc_gaps.md/HISTORY.md's 2026-08-17 entry.
 *
 * elf.h's struct sizes are checked directly (a fixed, documented binary
 * spec, not host-dependent -- meaningful on every host this project
 * builds for, not just Linux/ELF ones, since the header itself is
 * declared everywhere).
 *
 * dladdr() against this test binary's own main() is real, verifiable
 * behavior on every host: Windows (GetModuleHandleExA), macOS (dyld image
 * walk), and Linux (the main executable's own AT_PHDR-derived PT_LOAD
 * ranges) should all find *some* containing image for an address that is
 * definitely inside this running process's own main executable.
 *
 * dl_iterate_phdr() legitimately behaves differently per host (real
 * ELF-shaped data with exactly one entry on Linux; zero entries, a
 * documented honest result, on macOS/Windows -- see link.h's own
 * comment), so this test accepts either shape rather than asserting one
 * specific host's behavior, while still checking internal consistency
 * whenever an entry *is* reported. */
#include <dlfcn.h>
#include <elf.h>
#include <link.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int fail(const char* message) {
  fprintf(stderr, "dl_iterate_phdr_dladdr_test: %s\n", message);
  return 1;
}

static int phdr_callback_count;
static int phdr_callback_bad;

static int phdr_callback(struct dl_phdr_info* info, size_t size, void* data) {
  (void)data;
  ++phdr_callback_count;
  if (size < sizeof(*info)) {
    phdr_callback_bad = 1;
    return 0;
  }
  if (info->dlpi_phdr == 0 || info->dlpi_phnum == 0) {
    phdr_callback_bad = 1;
    return 0;
  }
  return 0;
}

int main(void) {
  Dl_info info;
  int result;
  int iterate_result;

  if (sizeof(Elf64_Ehdr) != 64) {
    return fail("sizeof(Elf64_Ehdr) must be 64 -- real ELF64 ABI, not host-dependent");
  }
  if (sizeof(Elf64_Phdr) != 56) {
    return fail("sizeof(Elf64_Phdr) must be 56 -- real ELF64 ABI, not host-dependent");
  }
  if (sizeof(Elf64_Shdr) != 64) {
    return fail("sizeof(Elf64_Shdr) must be 64 -- real ELF64 ABI, not host-dependent");
  }
  if (sizeof(Elf64_Sym) != 24) {
    return fail("sizeof(Elf64_Sym) must be 24 -- real ELF64 ABI, not host-dependent");
  }

  iterate_result = dl_iterate_phdr(phdr_callback, 0);
  if (phdr_callback_bad) {
    return fail("dl_iterate_phdr callback saw inconsistent info");
  }
  if (phdr_callback_count == 0 && iterate_result != 0) {
    return fail("dl_iterate_phdr returned nonzero without ever invoking the callback");
  }

  memset(&info, 0xAA, sizeof(info)); /* poison, so a no-op backend can't accidentally "pass" */
  result = dladdr((const void*)&main, &info);
  if (result == 0) {
    return fail("dladdr could not find the image containing this test binary's own main()");
  }
  if (info.dli_fname == 0) {
    return fail("dladdr found an image but did not report dli_fname");
  }
  if (info.dli_fbase == 0) {
    return fail("dladdr found an image but did not report dli_fbase");
  }
  /* dli_sname/dli_saddr are documented to legitimately stay NULL/0 -- see
   * dlfcn.h's own comment -- so no assertion on those two fields. */

  /* A NULL addr/info must fail cleanly, not crash. */
  if (dladdr(0, &info) != 0) {
    return fail("dladdr(NULL, ...) should fail");
  }
  if (dladdr((const void*)&main, 0) != 0) {
    return fail("dladdr(addr, NULL) should fail");
  }

  /* An address nowhere near any loaded image should not be reported as
   * found. */
  memset(&info, 0, sizeof(info));
  result = dladdr((const void*)(uintptr_t)1, &info);
  if (result != 0) {
    return fail("dladdr should not find an image for address 1");
  }

  printf("dl_iterate_phdr_dladdr_test: ok\n");
  return 0;
}
