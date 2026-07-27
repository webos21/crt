#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CRT_BOOTSTRAP_HEAP_SIZE (1024u * 1024u)

typedef union block_header block_header;

union block_header {
  struct {
    size_t size;
    int free;
    block_header* next;
  } block;
  long double align;
};

static unsigned char heap_storage[CRT_BOOTSTRAP_HEAP_SIZE];
static block_header* heap_head;

static size_t align_size(size_t size) {
  size_t alignment = sizeof(block_header);
  return (size + alignment - 1) & ~(alignment - 1);
}

static void init_heap(void) {
  uintptr_t start = (uintptr_t)heap_storage;
  uintptr_t aligned = (start + sizeof(block_header) - 1) & ~(uintptr_t)(sizeof(block_header) - 1);
  size_t adjustment = (size_t)(aligned - start);

  if (heap_head != 0) {
    return;
  }

  heap_head = (block_header*)aligned;
  heap_head->block.size = CRT_BOOTSTRAP_HEAP_SIZE - adjustment - sizeof(block_header);
  heap_head->block.free = 1;
  heap_head->block.next = 0;
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
    if (current->block.free && current->block.next->block.free) {
      current->block.size += sizeof(block_header) + current->block.next->block.size;
      current->block.next = current->block.next->block.next;
    } else {
      current = current->block.next;
    }
  }
}

void* malloc(size_t size) {
  block_header* current;
  size_t alignment = sizeof(block_header);

  if (size == 0) {
    size = 1;
  }
  if (size > ((size_t)-1) - (alignment - 1) ||
      size > CRT_BOOTSTRAP_HEAP_SIZE - sizeof(block_header)) {
    errno = ENOMEM;
    return 0;
  }
  size = align_size(size);

  init_heap();
  current = heap_head;
  while (current != 0) {
    if (current->block.free && current->block.size >= size) {
      split_block(current, size);
      current->block.free = 0;
      return current + 1;
    }
    current = current->block.next;
  }

  errno = ENOMEM;
  return 0;
}

void free(void* ptr) {
  block_header* header;

  if (ptr == 0) {
    return;
  }

  header = ((block_header*)ptr) - 1;
  header->block.free = 1;
  coalesce_free_blocks();
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

  if (ptr == 0) {
    return malloc(size);
  }
  if (size == 0) {
    free(ptr);
    return 0;
  }

  header = ((block_header*)ptr) - 1;
  size = align_size(size);
  if (header->block.size >= size) {
    split_block(header, size);
    return ptr;
  }

  new_ptr = malloc(size);
  if (new_ptr == 0) {
    return 0;
  }
  copy_size = header->block.size < size ? header->block.size : size;
  memcpy(new_ptr, ptr, copy_size);
  free(ptr);
  return new_ptr;
}
