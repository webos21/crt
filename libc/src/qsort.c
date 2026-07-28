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

static void insertion_sort(unsigned char* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*)) {
  size_t i;

  for (i = 1; i < nmemb; ++i) {
    size_t j = i;
    while (j > 0 && compar(base + j * size, base + (j - 1) * size) < 0) {
      swap_bytes(base + j * size, base + (j - 1) * size, size);
      --j;
    }
  }
}

void qsort(void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*)) {
  if (base == 0 || size == 0 || compar == 0 || nmemb < 2) {
    return;
  }
  insertion_sort((unsigned char*)base, nmemb, size, compar);
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
