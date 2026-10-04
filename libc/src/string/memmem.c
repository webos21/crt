#include <stddef.h>
#include <string.h>

/* memmem(): Bionic's string.h (API 1) -- the first occurrence of the `needle_len`
 * bytes at `needle` inside the `haystack_len` bytes at `haystack`. An empty
 * needle matches at the start of the haystack (glibc and Bionic behaviour). */
void* memmem(const void* haystack, size_t haystack_len, const void* needle, size_t needle_len) {
  const unsigned char* hay = (const unsigned char*)haystack;
  const unsigned char* pat = (const unsigned char*)needle;
  size_t i;

  if (needle_len == 0) {
    return (void*)haystack;
  }
  if (haystack_len < needle_len) {
    return 0;
  }
  for (i = 0; i + needle_len <= haystack_len;) {
    const unsigned char* hit = memchr(hay + i, pat[0], haystack_len - needle_len + 1 - i);

    if (hit == 0) {
      return 0;
    }
    i = (size_t)(hit - hay);
    if (memcmp(hit, pat, needle_len) == 0) {
      return (void*)hit;
    }
    ++i;
  }
  return 0;
}
