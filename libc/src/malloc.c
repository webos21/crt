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
  mapping = mmap(0, chunk_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mapping == MAP_FAILED) {
    return 0;
  }

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
