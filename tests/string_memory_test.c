#include <string.h>
#include <unistd.h>

static int fail(const char* message) {
  static const char prefix[] = "string_memory_test: ";
  static const char suffix[] = "\n";
  write(2, prefix, sizeof(prefix) - 1);
  write(2, message, strlen(message));
  write(2, suffix, sizeof(suffix) - 1);
  return 1;
}

static int check_bytes(const unsigned char* data, const unsigned char* expected, size_t n) {
  size_t i;
  for (i = 0; i < n; ++i) {
    if (data[i] != expected[i]) {
      return 0;
    }
  }
  return 1;
}

int main(void) {
  unsigned char buffer[16];
  unsigned char source[8] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};
  unsigned char expected_memcpy[8] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};
  unsigned char expected_overlap_forward[8] = {'0', '1', '0', '1', '2', '3', '4', '7'};
  unsigned char expected_overlap_backward[8] = {'2', '3', '4', '5', '6', '5', '6', '7'};
  const char* repeated = "ababa";
  char text[16];
  char padded[8];

  if (strlen("") != 0 || strlen("abc") != 3) {
    return fail("strlen");
  }

  if (strcmp("abc", "abc") != 0 || strcmp("abc", "abd") >= 0 || strcmp("abd", "abc") <= 0) {
    return fail("strcmp");
  }

  if (strncmp("abc", "abd", 2) != 0 || strncmp("abc", "abd", 3) >= 0 ||
      strncmp("abc", "abc", 0) != 0) {
    return fail("strncmp");
  }

  memset(buffer, 0x5a, sizeof(buffer));
  if (buffer[0] != 0x5a || buffer[15] != 0x5a) {
    return fail("memset");
  }

  if (memchr(buffer, 0x5a, sizeof(buffer)) != buffer ||
      memchr(buffer, 0x11, sizeof(buffer)) != 0 ||
      memchr(buffer, 0x5a, 0) != 0) {
    return fail("memchr");
  }

  memset(buffer, 0, sizeof(buffer));
  if (memcpy(buffer, source, sizeof(source)) != buffer) {
    return fail("memcpy return");
  }
  if (!check_bytes(buffer, expected_memcpy, sizeof(expected_memcpy))) {
    return fail("memcpy bytes");
  }

  if (memcmp(buffer, source, sizeof(source)) != 0 ||
      memcmp("abc", "abd", 3) >= 0 ||
      memcmp("abd", "abc", 3) <= 0 ||
      memcmp("\xff", "\x00", 1) <= 0 ||
      memcmp("abc", "xyz", 0) != 0) {
    return fail("memcmp");
  }

  memcpy(buffer, "01234567", 8);
  if (memmove(buffer + 2, buffer, 5) != buffer + 2) {
    return fail("memmove return forward");
  }
  if (!check_bytes(buffer, expected_overlap_forward, sizeof(expected_overlap_forward))) {
    return fail("memmove overlap forward");
  }

  memcpy(buffer, "01234567", 8);
  memmove(buffer, buffer + 2, 5);
  if (!check_bytes(buffer, expected_overlap_backward, sizeof(expected_overlap_backward))) {
    return fail("memmove overlap backward");
  }

  if (strcpy(text, "ab") != text || strcmp(text, "ab") != 0) {
    return fail("strcpy");
  }
  if (strcat(text, "cd") != text || strcmp(text, "abcd") != 0) {
    return fail("strcat");
  }
  if (strchr(text, 'b') != text + 1 || strchr(text, '\0') != text + 4 ||
      strchr(text, 'x') != 0) {
    return fail("strchr");
  }
  if (strrchr(repeated, 'a') != &repeated[4] || strrchr(repeated, 'x') != 0 ||
      strrchr(repeated, '\0') != &repeated[5]) {
    return fail("strrchr");
  }

  memset(padded, 0x7e, sizeof(padded));
  if (strncpy(padded, "xy", sizeof(padded)) != padded || padded[0] != 'x' ||
      padded[1] != 'y' || padded[2] != 0 || padded[7] != 0) {
    return fail("strncpy pad");
  }
  memset(padded, 0x7e, sizeof(padded));
  strncpy(padded, "abcdef", 3);
  if (padded[0] != 'a' || padded[1] != 'b' || padded[2] != 'c' || padded[3] != 0x7e) {
    return fail("strncpy truncate");
  }

  write(1, "string_memory_test: ok\n", 23);
  return 0;
}
