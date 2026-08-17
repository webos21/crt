#ifndef CRT_UCHAR_H
#define CRT_UCHAR_H

/*
 * C11 <uchar.h>. Real Bionic implements this as a thin conversion layer
 * over the same UTF-8 <-> UTF-32 codepoint logic wchar.h's mbrtowc()/
 * wcrtomb() already use (this project's wchar_t is forced to a 32-bit
 * codepoint on every host via -fwchar-type=int, so mbrtowc()/wcrtomb()
 * already speak UTF-32 directly) -- this header follows the same shape:
 * mbrtoc32()/c32rtomb() are near-trivial wrappers, and mbrtoc16()/
 * c16rtomb() add real UTF-16 surrogate-pair handling on top.
 */
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * char16_t/char32_t are language keywords in C++11 and later (this
 * project's C++ frontend targets C++17) -- typedef'ing them there would
 * conflict with the built-in types. In C (this project builds C code as
 * C99, where they are not keywords), define them from the compiler's own
 * __CHAR16_TYPE__/__CHAR32_TYPE__ builtins, matching real Bionic.
 */
#if !defined(__cplusplus)
typedef __CHAR16_TYPE__ char16_t;
typedef __CHAR32_TYPE__ char32_t;
#endif

size_t mbrtoc16(char16_t* pc16, const char* s, size_t n, mbstate_t* ps);
size_t c16rtomb(char* s, char16_t c16, mbstate_t* ps);
size_t mbrtoc32(char32_t* pc32, const char* s, size_t n, mbstate_t* ps);
size_t c32rtomb(char* s, char32_t c32, mbstate_t* ps);

#ifdef __cplusplus
}
#endif

#endif
