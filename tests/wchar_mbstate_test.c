#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

static int fail(const char* message) {
  fprintf(stderr, "wchar_mbstate_test: %s\n", message);
  return 1;
}

int main(void) {
  const char euro_utf8[] = "\xe2\x82\xac";
  const char ascii_narrow[] = "ABCD";
  const wchar_t wide_narrow_limit[] = L"A\x20acZ";
  const char* narrow_src;
  const wchar_t* wide_src;
  mbstate_t state = {0, 0, 0};
  wchar_t wide[16];
  wchar_t buffer[16];
  wchar_t moved[8];
  wchar_t line[16];
  wchar_t* end = 0;
  wchar_t* dyn = 0;
  wchar_t* scan_alloc = 0;
  wchar_t* wmem = 0;
  wchar_t* save = 0;
  wchar_t* tok = 0;
  char* mem = 0;
  char narrow[16];
  size_t mem_size = 0;
  size_t wmem_size = 0;
  wchar_t wc;
  FILE* stream;
  size_t result;
  int scanned = 0;
  long double ld = 0.0L;

  if (sizeof(wchar_t) != 4 || (wchar_t)-1 >= (wchar_t)0) {
    return fail("wchar_t data model");
  }
  if (MB_CUR_MAX < 4) {
    return fail("MB_CUR_MAX");
  }
  if (btowc('A') != L'A' || btowc(0x80) != WEOF ||
      wctob(L'Z') != 'Z' || wctob((wint_t)0x20ac) != EOF) {
    return fail("btowc/wctob");
  }

  if (mbrtowc(&wc, "A", 1, &state) != 1 || wc != L'A' || !mbsinit(&state)) {
    return fail("mbrtowc ascii");
  }
  result = mbrtowc(&wc, euro_utf8, 2, &state);
  if (result != (size_t)-2 || mbsinit(&state)) {
    return fail("mbrtowc partial");
  }
  result = mbrtowc(&wc, euro_utf8 + 2, 1, &state);
  if (result != 1 || wc != (wchar_t)0x20ac || !mbsinit(&state)) {
    return fail("mbrtowc complete");
  }
  errno = 0;
  if (mbrtowc(&wc, "\xff", 1, &state) != (size_t)-1 || errno != EILSEQ) {
    return fail("mbrtowc invalid");
  }
  if (mbrtowc(&wc, "", 1, &state) != 0 || wc != 0) {
    return fail("mbrtowc nul");
  }
  if (mbrlen("A", 1, &state) != 1) {
    return fail("mbrlen");
  }

  memset(narrow, 0, sizeof(narrow));
  if (wcrtomb(narrow, (wchar_t)0x20ac, &state) != 3 ||
      strcmp(narrow, euro_utf8) != 0 ||
      wcrtomb(narrow, L'A', &state) != 1 || narrow[0] != 'A') {
    return fail("wcrtomb");
  }

  narrow_src = "A\xe2\x82\xacZ";
  if (mbsrtowcs(wide, &narrow_src, 16, &state) != 3 || narrow_src != 0 ||
      wide[0] != L'A' || wide[1] != (wchar_t)0x20ac || wide[2] != L'Z' || wide[3] != 0) {
    return fail("mbsrtowcs");
  }
  wide_src = wide;
  memset(narrow, 0, sizeof(narrow));
  if (wcsrtombs(narrow, &wide_src, sizeof(narrow), &state) != 5 || wide_src != 0 ||
      strcmp(narrow, "A\xe2\x82\xacZ") != 0) {
    return fail("wcsrtombs");
  }
  narrow_src = ascii_narrow;
  memset(wide, 0, sizeof(wide));
  if (mbsnrtowcs(wide, &narrow_src, 3, 16, &state) != 3 ||
      narrow_src != &ascii_narrow[3] || wcscmp(wide, L"ABC") != 0) {
    return fail("mbsnrtowcs");
  }
  wide_src = wide_narrow_limit;
  memset(narrow, 0, sizeof(narrow));
  if (wcsnrtombs(narrow, &wide_src, 2, sizeof(narrow), &state) != 4 ||
      wide_src != &wide_narrow_limit[2] || strcmp(narrow, "A\xe2\x82\xac") != 0) {
    return fail("wcsnrtombs");
  }
  if (mbstowcs(buffer, "abc", 16) != 3 || wcscmp(buffer, L"abc") != 0 ||
      wcstombs(narrow, L"xy", sizeof(narrow)) != 2 || strcmp(narrow, "xy") != 0) {
    return fail("mbstowcs/wcstombs");
  }
  if (mblen("A", 1) != 1 || mbtowc(&wc, euro_utf8, 3) != 3 ||
      wc != (wchar_t)0x20ac || wctomb(narrow, L'Q') != 1 || narrow[0] != 'Q') {
    return fail("legacy mb");
  }

  if (wcslen(L"abc") != 3 || wcsnlen(L"abc", 2) != 2 ||
      wcscmp(L"abc", L"abd") >= 0 || wcsncmp(L"abc", L"abd", 2) != 0) {
    return fail("wide compare");
  }
  if (wcscpy(buffer, L"ab") != buffer || wcscat(buffer, L"cd") != buffer ||
      wcscmp(buffer, L"abcd") != 0 || wcschr(buffer, L'c') != buffer + 2 ||
      wcsrchr(buffer, L'a') != buffer || wcsstr(buffer, L"bc") != buffer + 1) {
    return fail("wide string");
  }
  if (wcpcpy(buffer, L"copy") != buffer + 4 || wcscmp(buffer, L"copy") != 0 ||
      wcpncpy(buffer, L"xy", 5) != buffer + 2 || buffer[0] != L'x' || buffer[4] != 0) {
    return fail("wcpcpy/wcpncpy");
  }
  wcsncpy(buffer, L"xy", 5);
  if (buffer[0] != L'x' || buffer[1] != L'y' || buffer[2] != 0 || buffer[4] != 0) {
    return fail("wcsncpy");
  }
  wcscpy(buffer, L"ab");
  if (wcsncat(buffer, L"cdef", 2) != buffer || wcscmp(buffer, L"abcd") != 0 ||
      wcspbrk(buffer, L"dx") != buffer + 3 ||
      wcscspn(buffer, L"cd") != 2 || wcsspn(buffer, L"abc") != 3 ||
      wcscasecmp(L"AbC", L"aBc") != 0 || wcsncasecmp(L"AbX", L"aBy", 2) != 0 ||
      wcscoll(L"abc", L"abd") >= 0) {
    return fail("wide string extra");
  }
  wcslcpy(buffer, L"abcdef", 4);
  if (wcscmp(buffer, L"abc") != 0 || wcslcat(buffer, L"XYZ", 6) != 6 ||
      wcscmp(buffer, L"abcXY") != 0) {
    return fail("wcslcpy/wcslcat");
  }
  dyn = wcsdup(L"dup");
  if (dyn == 0 || wcscmp(dyn, L"dup") != 0) {
    free(dyn);
    return fail("wcsdup");
  }
  free(dyn);
  wcscpy(buffer, L"one,two");
  tok = wcstok(buffer, L",", &save);
  if (tok == 0 || wcscmp(tok, L"one") != 0 ||
      (tok = wcstok(0, L",", &save)) == 0 || wcscmp(tok, L"two") != 0 ||
      wcstok(0, L",", &save) != 0) {
    return fail("wcstok");
  }
  if (wcsxfrm(buffer, L"abc", 16) != 3 || wcscmp(buffer, L"abc") != 0 ||
      wcwidth(L'A') != 1 || wcwidth(0) != 0 || wcwidth(0x07) != -1 ||
      wcswidth(L"abc", 16) != 3) {
    return fail("wide collation width");
  }
  if (wcstol(L"123x", &end, 10) != 123 || end == 0 || *end != L'x' ||
      wcstoul(L"77z", &end, 8) != 63 || *end != L'z' ||
      wcstoll(L"-5q", &end, 10) != -5 || *end != L'q' ||
      wcstoull(L"0xff!", &end, 0) != 255 || *end != L'!' ||
      wcstod(L"3.5e1!", &end) != 35.0 || *end != L'!' ||
      wcstof(L"2.5?", &end) != 2.5f || *end != L'?' ||
      wcstold(L"4.25,", &end) != 4.25L || *end != L',') {
    return fail("wide numeric");
  }
  wmemset(moved, L'0', 8);
  if (wmemcpy(moved, L"abcdef", 6) != moved ||
      wmempcpy(moved, L"abcdef", 6) != moved + 6 ||
      wmemchr(moved, L'd', 6) != moved + 3 ||
      wmemcmp(moved, L"abcdef", 6) != 0) {
    return fail("wmemcpy");
  }
  wmemmove(moved + 2, moved, 4);
  if (wmemcmp(moved, L"ababcd", 6) != 0) {
    return fail("wmemmove");
  }

  if (!iswalpha(L'A') || !iswdigit(L'7') || !iswspace(L'\n') ||
      towlower(L'A') != L'a' || towupper(L'z') != L'Z' ||
      !iswctype(L'F', wctype("xdigit")) ||
      towctrans(L'G', wctrans("tolower")) != L'g' ||
      wctype("unknown") != 0 || wctrans("unknown") != 0) {
    return fail("wctype");
  }

  stream = fmemopen((void*)"A\xe2\x82\xac\n", 5, "r");
  if (stream == 0 || fwide(stream, 1) <= 0 ||
      fgetwc(stream) != L'A' ||
      fgetwc(stream) != (wchar_t)0x20ac ||
      ungetwc(L'Z', stream) != L'Z' ||
      fgetwc(stream) != L'Z' ||
      fgetws(line, 16, stream) != line ||
      wcscmp(line, L"\n") != 0) {
    if (stream != 0) {
      fclose(stream);
    }
    return fail("wide input stdio");
  }
  fclose(stream);

  stream = open_memstream(&mem, &mem_size);
  if (stream == 0) {
    return fail("wide output stdio");
  }
  if (fwprintf(stream, L"%ls:%d:%lc", L"wide", 7, L'!') < 0) {
    fclose(stream);
    free(mem);
    return fail("fwprintf");
  }
  if (fputwc((wchar_t)0x20ac, stream) == WEOF) {
    fclose(stream);
    free(mem);
    return fail("fputwc");
  }
  if (fflush(stream) != 0) {
    fclose(stream);
    free(mem);
    return fail("wide fflush");
  }
  if (strcmp(mem, "wide:7:!\xe2\x82\xac") != 0) {
    fclose(stream);
    free(mem);
    return fail("wide output content");
  }
  fclose(stream);
  free(mem);

  stream = open_wmemstream(&wmem, &wmem_size);
  if (stream == 0 || fwide(stream, 0) <= 0 ||
      fwprintf(stream, L"%s:%d", L"wide", 11) != 7 ||
      fputwc((wchar_t)0x20ac, stream) == WEOF ||
      fflush(stream) != 0 ||
      wmem_size != 8 || wcscmp(wmem, L"wide:11\x20ac") != 0) {
    if (stream != 0) {
      fclose(stream);
    }
    free(wmem);
    return fail("open_wmemstream");
  }
  fclose(stream);
  free(wmem);

  if (swprintf(buffer, 16, L"%s:%d%c", L"ok", 9, L'?') != 5 ||
      wcscmp(buffer, L"ok:9?") != 0) {
    return fail("swprintf");
  }
  memset(buffer, 0, sizeof(buffer));
  if (swscanf(L"name 42", L"%s %d", buffer, &scanned) != 2 ||
      wcscmp(buffer, L"name") != 0 || scanned != 42) {
    return fail("swscanf");
  }
  memset(buffer, 0, sizeof(buffer));
  if (swscanf(L"\x20ac done", L"%s", buffer) != 1 ||
      buffer[0] != (wchar_t)0x20ac || wcscmp(buffer + 1, L"") != 0) {
    return fail("swscanf utf8");
  }
  memset(buffer, 0, sizeof(buffer));
  if (swscanf(L"abc123", L"%[abc]", buffer) != 1 || wcscmp(buffer, L"abc") != 0) {
    return fail("swscanf scanset");
  }
  if (swscanf(L"0x1.8p1", L"%La", &ld) != 1 || ld < 2.99L || ld > 3.01L) {
    return fail("swscanf hex float");
  }
  if (swscanf(L"alloc", L"%ms", &scan_alloc) != 1 || wcscmp(scan_alloc, L"alloc") != 0) {
    free(scan_alloc);
    return fail("swscanf allocation");
  }
  free(scan_alloc);

  printf("wchar_mbstate_test: ok\n");
  return 0;
}
