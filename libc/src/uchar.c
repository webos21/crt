#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <uchar.h>
#include <wchar.h>

/*
 * mbrtoc16()/c16rtomb() reuse mbrtowc()/wcrtomb() (UTF-8 <-> UTF-32
 * codepoint, since this project's wchar_t is a 32-bit codepoint on every
 * host) for the UTF-8 half of the conversion, and add real UTF-16
 * surrogate-pair handling on top for codepoints outside the Basic
 * Multilingual Plane (> 0xffff).
 *
 * The extra surrogate-pair state (a pending low surrogate waiting to be
 * emitted by the next mbrtoc16() call, or a pending high surrogate
 * waiting to be completed by the next c16rtomb() call) is stashed in the
 * same mbstate_t object mbrtowc()/wcrtomb() already use, via two sentinel
 * values in mbstate_t's `expected` byte that UTF-8 decoding itself never
 * produces (UTF-8 continuation-byte counts are always 0/2/3/4).
 */
#define CRT_MBSTATE_PENDING_LOW_SURROGATE 0xfeU
#define CRT_MBSTATE_PENDING_HIGH_SURROGATE 0xffU

static mbstate_t internal_mbrtoc16_state;
static mbstate_t internal_c16rtomb_state;
static mbstate_t internal_mbrtoc32_state;
static mbstate_t internal_c32rtomb_state;

size_t mbrtoc16(char16_t* pc16, const char* s, size_t n, mbstate_t* ps) {
  mbstate_t* state = ps != 0 ? ps : &internal_mbrtoc16_state;
  wchar_t wc;
  size_t result;
  uint32_t codepoint;

  if (s == 0) {
    state->codepoint = 0;
    state->expected = 0;
    state->seen = 0;
    return 0;
  }

  if (state->expected == CRT_MBSTATE_PENDING_LOW_SURROGATE) {
    if (pc16 != 0) {
      *pc16 = (char16_t)state->codepoint;
    }
    state->codepoint = 0;
    state->expected = 0;
    state->seen = 0;
    return (size_t)-3;
  }

  result = mbrtowc(&wc, s, n, state);
  if (result == (size_t)-1 || result == (size_t)-2) {
    return result;
  }

  codepoint = (uint32_t)wc;
  if (codepoint <= 0xffffU) {
    if (pc16 != 0) {
      *pc16 = (char16_t)codepoint;
    }
    return result;
  }

  /* Astral codepoint: emit the high surrogate now, stash the low one. */
  codepoint -= 0x10000U;
  if (pc16 != 0) {
    *pc16 = (char16_t)(0xd800U + (codepoint >> 10));
  }
  state->codepoint = 0xdc00U + (codepoint & 0x3ffU);
  state->expected = CRT_MBSTATE_PENDING_LOW_SURROGATE;
  state->seen = 0;
  return result;
}

size_t c16rtomb(char* s, char16_t c16, mbstate_t* ps) {
  mbstate_t* state = ps != 0 ? ps : &internal_c16rtomb_state;
  uint32_t codepoint;

  if (s == 0) {
    state->codepoint = 0;
    state->expected = 0;
    state->seen = 0;
    return 1;
  }

  if (state->expected == CRT_MBSTATE_PENDING_HIGH_SURROGATE) {
    uint32_t high = state->codepoint;

    state->codepoint = 0;
    state->expected = 0;
    state->seen = 0;
    if (c16 < 0xdc00U || c16 > 0xdfffU) {
      errno = EILSEQ;
      return (size_t)-1;
    }
    codepoint = 0x10000U + ((high - 0xd800U) << 10) + (c16 - 0xdc00U);
    return wcrtomb(s, (wchar_t)codepoint, state);
  }

  if (c16 >= 0xd800U && c16 <= 0xdbffU) {
    /* High surrogate: stash it, wait for the matching low half. */
    state->codepoint = c16;
    state->expected = CRT_MBSTATE_PENDING_HIGH_SURROGATE;
    state->seen = 0;
    return 0;
  }
  if (c16 >= 0xdc00U && c16 <= 0xdfffU) {
    /* Lone low surrogate with no preceding high one. */
    errno = EILSEQ;
    return (size_t)-1;
  }

  return wcrtomb(s, (wchar_t)c16, state);
}

size_t mbrtoc32(char32_t* pc32, const char* s, size_t n, mbstate_t* ps) {
  mbstate_t* state = ps != 0 ? ps : &internal_mbrtoc32_state;
  wchar_t wc;
  size_t result = mbrtowc(&wc, s, n, state);

  if (pc32 != 0 && result != (size_t)-1 && result != (size_t)-2) {
    *pc32 = (char32_t)wc;
  }
  return result;
}

size_t c32rtomb(char* s, char32_t c32, mbstate_t* ps) {
  mbstate_t* state = ps != 0 ? ps : &internal_c32rtomb_state;

  return wcrtomb(s, (wchar_t)c32, state);
}
