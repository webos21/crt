#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include <private/crt_atomic.h>

#define CRT_HEAP_CHUNK_SIZE (64u * 1024u)

typedef union block_header block_header;

union block_header {
  struct {
    size_t size;
    int free;
    block_header* next;
  } block;
  long double align;
};

static block_header* heap_head;
static crt_spinlock heap_lock = CRT_SPINLOCK_INIT;

void __crt_malloc_after_fork_child(void) {
  heap_lock.state.value = 0;
}

/* Windows memory-copy fork() support (docs/windows_fork_emulation.md,
 * "Chosen Direction" superseded by the Cygwin/MSYS-style replacement):
 * tracks the OS-level mmap()/VirtualAlloc() region boundaries separately
 * from the block_header split chain above. The two are NOT the same
 * thing once split_block() has subdivided a chunk: a single 64KB
 * append_chunk() mmap() region can end up linked as several
 * block_header nodes, most of them at addresses that are not 64KB-
 * aligned -- and VirtualAllocEx() on the Windows side requires an
 * explicit lpAddress to be aligned to the system allocation granularity
 * (64KB), so the fork implementation must copy whole OS regions, not
 * individual split blocks. Fixed-size table (not a linked list, to avoid
 * needing malloc() itself to track allocations of the allocator).
 *
 * CRT_MALLOC_MAX_OS_REGIONS was 4096 (== 4096 * CRT_HEAP_CHUNK_SIZE ==
 * exactly 256MB) until a real, reproducible bug was found: append_chunk()
 * silently stopped recording new regions once the table filled up --
 * `mmap()` still succeeded and the memory was perfectly usable within
 * this process, but __crt_malloc_os_region_count()/_base()/_size() (what
 * fork_memcopy.c's copy_heap_chunks() actually walks) never saw anything
 * past the cap, so a fork() from a process that had allocated more than
 * 256MB silently produced a child with that memory simply missing --
 * confirmed directly: a value written past the 256MB mark vanished
 * across fork() and reading it back in the child faulted (a real mksh
 * process interpreting a large, deeply self-recursive libtool script --
 * see the libffi shared-library porting work -- is exactly the kind of
 * long-lived, memory-growing process that can cross this threshold in
 * practice, and the resulting corrupted child is indistinguishable from
 * a hang without knowing to look for this).
 *
 * Fixed two ways: (1) raised the cap from 4096 to 65536 (== 4GB of
 * tracked heap, a still-fixed table -- growing it dynamically would
 * itself need to route through mmap()/munmap() rather than malloc() to
 * avoid the same reentrancy append_chunk()'s own header comment already
 * flags, and would introduce a second, separate "is *this* array's own
 * backing memory visible to fork()?" bookkeeping problem; 4GB is enough
 * headroom that hitting it in practice should now be exceptionally
 * rare). (2) far more importantly, once the table is genuinely full, the
 * allocation itself now fails (ENOMEM) instead of silently succeeding
 * untracked -- turning any future "process legitimately needs more than
 * 4GB of heap and later forks" case into an honest, immediately-visible
 * allocation failure at the point of the oversized malloc(), instead of
 * a correctness time bomb that only detonates later, inside some
 * unrelated fork() call, as memory corruption in the child. */
#define CRT_MALLOC_MAX_OS_REGIONS 65536
static void* heap_os_region_base[CRT_MALLOC_MAX_OS_REGIONS];
static size_t heap_os_region_size[CRT_MALLOC_MAX_OS_REGIONS];
static int heap_os_region_count;

int __crt_malloc_os_region_count(void) {
  return heap_os_region_count;
}

void* __crt_malloc_os_region_base(int index) {
  return index >= 0 && index < heap_os_region_count ? heap_os_region_base[index] : 0;
}

size_t __crt_malloc_os_region_size(int index) {
  return index >= 0 && index < heap_os_region_count ? heap_os_region_size[index] : 0;
}

static size_t align_size(size_t size) {
  size_t alignment = sizeof(block_header);
  return (size + alignment - 1) & ~(alignment - 1);
}

static size_t align_chunk_size(size_t size) {
  return (size + CRT_HEAP_CHUNK_SIZE - 1) & ~(CRT_HEAP_CHUNK_SIZE - 1);
}

static block_header* append_chunk(size_t size) {
  size_t needed;
  size_t chunk_size;
  void* mapping;
  block_header* header;
  block_header* current;

  if (size > ((size_t)-1) - sizeof(block_header) - (CRT_HEAP_CHUNK_SIZE - 1)) {
    errno = ENOMEM;
    return 0;
  }

  needed = size + sizeof(block_header);
  chunk_size = align_chunk_size(needed);
  if (heap_os_region_count >= CRT_MALLOC_MAX_OS_REGIONS) {
    /* See CRT_MALLOC_MAX_OS_REGIONS's own comment above: silently
     * handing out memory we can no longer make visible to fork() is
     * what caused the original bug. Fail the allocation instead of the
     * mmap() below ever running. */
    errno = ENOMEM;
    return 0;
  }
  mapping = mmap(0, chunk_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mapping == MAP_FAILED) {
    return 0;
  }
  heap_os_region_base[heap_os_region_count] = mapping;
  heap_os_region_size[heap_os_region_count] = chunk_size;
  heap_os_region_count++;

  header = (block_header*)mapping;
  header->block.size = chunk_size - sizeof(block_header);
  header->block.free = 1;
  header->block.next = 0;

  if (heap_head == 0) {
    heap_head = header;
    return header;
  }

  current = heap_head;
  while (current->block.next != 0) {
    current = current->block.next;
  }
  current->block.next = header;
  return header;
}

static void split_block(block_header* header, size_t size) {
  block_header* next;

  if (header->block.size < size + sizeof(block_header) + sizeof(block_header)) {
    return;
  }

  next = (block_header*)((unsigned char*)(header + 1) + size);
  next->block.size = header->block.size - size - sizeof(block_header);
  next->block.free = 1;
  next->block.next = header->block.next;

  header->block.size = size;
  header->block.next = next;
}

static void coalesce_free_blocks(void) {
  block_header* current = heap_head;

  while (current != 0 && current->block.next != 0) {
    unsigned char* current_end = (unsigned char*)(current + 1) + current->block.size;
    if (current->block.free && current->block.next->block.free &&
        current_end == (unsigned char*)current->block.next) {
      current->block.size += sizeof(block_header) + current->block.next->block.size;
      current->block.next = current->block.next->block.next;
    } else {
      current = current->block.next;
    }
  }
}

static void* malloc_unlocked(size_t size) {
  block_header* current;
  size_t alignment = sizeof(block_header);

  if (size == 0) {
    size = 1;
  }
  if (size > ((size_t)-1) - (alignment - 1)) {
    errno = ENOMEM;
    return 0;
  }
  size = align_size(size);

  current = heap_head;
  while (current != 0) {
    if (current->block.free && current->block.size >= size) {
      split_block(current, size);
      current->block.free = 0;
      return current + 1;
    }
    current = current->block.next;
  }

  current = append_chunk(size);
  if (current == 0) {
    return 0;
  }
  split_block(current, size);
  current->block.free = 0;
  return current + 1;
}

static void free_unlocked(void* ptr) {
  block_header* header;

  if (ptr == 0) {
    return;
  }

  header = ((block_header*)ptr) - 1;
  header->block.free = 1;
  coalesce_free_blocks();
}

void* malloc(size_t size) {
  void* ptr;

  crt_spin_lock(&heap_lock);
  ptr = malloc_unlocked(size);
  crt_spin_unlock(&heap_lock);
  return ptr;
}

void free(void* ptr) {
  crt_spin_lock(&heap_lock);
  free_unlocked(ptr);
  crt_spin_unlock(&heap_lock);
}

void* calloc(size_t nmemb, size_t size) {
  void* ptr;

  if (size != 0 && nmemb > ((size_t)-1) / size) {
    errno = ENOMEM;
    return 0;
  }

  size *= nmemb;
  ptr = malloc(size);
  if (ptr == 0) {
    return 0;
  }
  memset(ptr, 0, size);
  return ptr;
}

void* realloc(void* ptr, size_t size) {
  block_header* header;
  void* new_ptr;
  size_t copy_size;
  size_t alignment = sizeof(block_header);

  if (ptr == 0) {
    return malloc(size);
  }
  if (size == 0) {
    free(ptr);
    return 0;
  }

  crt_spin_lock(&heap_lock);
  if (size > ((size_t)-1) - (alignment - 1)) {
    errno = ENOMEM;
    crt_spin_unlock(&heap_lock);
    return 0;
  }
  header = ((block_header*)ptr) - 1;
  size = align_size(size);
  if (header->block.size >= size) {
    split_block(header, size);
    crt_spin_unlock(&heap_lock);
    return ptr;
  }

  new_ptr = malloc_unlocked(size);
  if (new_ptr == 0) {
    crt_spin_unlock(&heap_lock);
    return 0;
  }
  copy_size = header->block.size < size ? header->block.size : size;
  memcpy(new_ptr, ptr, copy_size);
  free_unlocked(ptr);
  crt_spin_unlock(&heap_lock);
  return new_ptr;
}
