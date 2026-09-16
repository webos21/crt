#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static void swap_bytes(unsigned char* a, unsigned char* b, size_t size) {
  while (size-- != 0) {
    unsigned char tmp = *a;
    *a++ = *b;
    *b++ = tmp;
  }
}

static void sift_down(
    unsigned char* base,
    size_t root,
    size_t end,
    size_t size,
    int (*compar)(const void*, const void*)) {
  while (root <= (end - 1) / 2) {
    size_t child = root * 2 + 1;
    size_t candidate = root;

    if (compar(base + candidate * size, base + child * size) < 0) {
      candidate = child;
    }
    if (child < end && compar(base + candidate * size, base + (child + 1) * size) < 0) {
      candidate = child + 1;
    }
    if (candidate == root) {
      return;
    }
    swap_bytes(base + root * size, base + candidate * size, size);
    root = candidate;
  }
}

void qsort(void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*)) {
  unsigned char* bytes = (unsigned char*)base;
  size_t start;
  size_t end;

  if (base == 0 || size == 0 || compar == 0 || nmemb < 2) {
    return;
  }

  /* In-place heapsort keeps qsort usable before malloc is initialized and
   * guarantees O(N log N) comparisons even for sorted or hostile input. */
  start = (nmemb - 2) / 2 + 1;
  while (start != 0) {
    --start;
    sift_down(bytes, start, nmemb - 1, size, compar);
  }
  end = nmemb - 1;
  while (end != 0) {
    swap_bytes(bytes, bytes + end * size, size);
    --end;
    if (end != 0) {
      sift_down(bytes, 0, end, size, compar);
    }
  }
}

void* bsearch(
    const void* key,
    const void* base,
    size_t nmemb,
    size_t size,
    int (*compar)(const void*, const void*)) {
  const unsigned char* first = (const unsigned char*)base;

  if (key == 0 || base == 0 || size == 0 || compar == 0) {
    return 0;
  }
  while (nmemb != 0) {
    size_t half = nmemb / 2;
    const unsigned char* middle = first + half * size;
    int result = compar(key, middle);
    if (result == 0) {
      return (void*)middle;
    }
    if (result > 0) {
      first = middle + size;
      nmemb -= half + 1;
    } else {
      nmemb = half;
    }
  }
  return 0;
}
