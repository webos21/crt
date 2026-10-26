/* Exercise the shared libdl backend: _DYNAMIC in a DSO is not the main
 * executable's rendezvous table. Both images must be found exactly once. */
#include <elf.h>
#include <link.h>
#include <stdint.h>
#include <stdio.h>

static int main_count;
static int library_count;

int main(void);

static int visit(struct dl_phdr_info* info, size_t size, void* data) {
  unsigned int i;
  (void)data;
  if (size < sizeof(*info)) return 1;
  for (i = 0; i < info->dlpi_phnum; ++i) {
    const Elf64_Phdr* phdr = &info->dlpi_phdr[i];
    uintptr_t start = info->dlpi_addr + phdr->p_vaddr;
    uintptr_t end = start + phdr->p_memsz;
    if (phdr->p_type != PT_LOAD) continue;
    if ((uintptr_t)&main >= start && (uintptr_t)&main < end) ++main_count;
    if ((uintptr_t)&dl_iterate_phdr >= start && (uintptr_t)&dl_iterate_phdr < end) ++library_count;
  }
  return 0;
}

int main(void) {
  if (dl_iterate_phdr(visit, 0) != 0 || main_count != 1 || library_count != 1) {
    fprintf(stderr, "dl_iterate_phdr_shared_test: main=%d shared-libdl=%d\n", main_count, library_count);
    return 1;
  }
  puts("dl_iterate_phdr_shared_test: ok");
  return 0;
}
