#include <string.h>
#include <stdlib.h>
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
  const char* haystack = "hello world";
  const char* alnum = "abc123";
  const char* signal_name;
  char* duplicate;
  char* limited_duplicate;
  char* end;
  char text[16];
  char padded[8];
  char transformed[8];

  if (strlen("") != 0 || strlen("abc") != 3) {
    return fail("strlen");
  }

  if (strnlen("abc", 0) != 0 || strnlen("abc", 2) != 2 || strnlen("abc", 8) != 3) {
    return fail("strnlen");
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
  if (memccpy(buffer, source, 'd', sizeof(source)) != buffer + 4 ||
      !check_bytes(buffer, source, 4)) {
    return fail("memccpy found");
  }
  memset(buffer, 0, sizeof(buffer));
  if (memccpy(buffer, source, 'x', sizeof(source)) != 0 ||
      !check_bytes(buffer, source, sizeof(source))) {
    return fail("memccpy missing");
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
  end = stpcpy(text, "xy");
  if (end != text + 2 || strcmp(text, "xy") != 0) {
    return fail("stpcpy");
  }
  memset(padded, 0x7e, sizeof(padded));
  end = stpncpy(padded, "ab", sizeof(padded));
  if (end != padded + 2 || padded[0] != 'a' || padded[1] != 'b' ||
      padded[2] != 0 || padded[7] != 0) {
    return fail("stpncpy pad");
  }
  memset(padded, 0x7e, sizeof(padded));
  end = stpncpy(padded, "abcdef", 3);
  if (end != padded + 3 || padded[0] != 'a' || padded[1] != 'b' ||
      padded[2] != 'c' || padded[3] != 0x7e) {
    return fail("stpncpy truncate");
  }
  if (strcasecmp("AbC", "aBc") != 0 || strcasecmp("abc", "abd") >= 0 ||
      strncasecmp("abcX", "ABCy", 3) != 0 || strncasecmp("abc", "ABD", 3) >= 0) {
    return fail("strcasecmp");
  }
  if (strcoll("abc", "abc") != 0 || strcoll("abc", "abd") >= 0) {
    return fail("strcoll");
  }
  memset(transformed, 0, sizeof(transformed));
  if (strxfrm(transformed, "abc", sizeof(transformed)) != 3 ||
      strcmp(transformed, "abc") != 0 ||
      strxfrm(transformed, "abcdef", 4) != 6 ||
      strcmp(transformed, "abc") != 0) {
    return fail("strxfrm");
  }
  strcpy(text, "ab");
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
  if (strstr(haystack, "world") != &haystack[6] ||
      strstr(haystack, "") != haystack ||
      strstr(haystack, "missing") != 0) {
    return fail("strstr");
  }
  if (strspn(alnum, "abc") != 3 || strspn(alnum, "") != 0) {
    return fail("strspn");
  }
  if (strcspn(alnum, "123") != 3 || strcspn("abc", "") != 3) {
    return fail("strcspn");
  }
  if (strpbrk(alnum, "31") != &alnum[3] || strpbrk("abc", "xyz") != 0) {
    return fail("strpbrk");
  }

  duplicate = strdup("copy");
  if (duplicate == 0 || strcmp(duplicate, "copy") != 0) {
    return fail("strdup");
  }
  duplicate[0] = 'C';
  if (strcmp(duplicate, "Copy") != 0) {
    return fail("strdup writable");
  }
  free(duplicate);

  limited_duplicate = strndup("abcdef", 3);
  if (limited_duplicate == 0 || strcmp(limited_duplicate, "abc") != 0) {
    return fail("strndup truncate");
  }
  free(limited_duplicate);

  limited_duplicate = strndup("xy", 8);
  if (limited_duplicate == 0 || strcmp(limited_duplicate, "xy") != 0) {
    return fail("strndup short");
  }
  free(limited_duplicate);

  signal_name = strsignal(15);
  if (signal_name == 0 || strstr(signal_name, "Terminated") == 0 ||
      strstr(strsignal(999), "999") == 0) {
    return fail("strsignal");
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
