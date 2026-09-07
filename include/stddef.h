#ifndef CRT_STDDEF_H
#define CRT_STDDEF_H

typedef __PTRDIFF_TYPE__ ptrdiff_t;
typedef __SIZE_TYPE__ size_t;

#ifndef __cplusplus
typedef __WCHAR_TYPE__ wchar_t;
#endif

/* Bionic uses long double for the public C max_align_t spelling. Keeping the
 * same spelling also lets libc++'s C wrapper recognize the compiler type.
 * This is correct on every other target this project builds for: on
 * AArch64 Linux/Android and on x86/x86_64, long double already carries the
 * platform's largest ordinary scalar alignment (128-bit quad precision on
 * AArch64, 80-bit extended --16-byte aligned-- on x86), matching what a
 * general-purpose allocator there actually guarantees.
 *
 * Apple's arm64 ABI is a real, documented exception: long double there is
 * just a plain 64-bit IEEE double (__LDBL_MANT_DIG__ == 53, not AAPCS64's
 * own 113-bit quad precision), so it is only 8-byte aligned -- even though
 * this platform's real allocator guarantee (and __STDCPP_DEFAULT_NEW_
 * ALIGNMENT__) is 16 bytes. Apple's own SDK stddef.h has this exact same
 * understated max_align_t and it is harmless there, because native Apple
 * code never sizes its own hand-rolled allocators off alignof(max_align_t).
 * It is NOT harmless here: confirmed for real (2026-09-06) that Skia's own
 * vendored src/sksl/SkSLMemoryPool.h sets its arena's own internal sub-
 * allocation alignment to exactly alignof(std::max_align_t) -- an 8-byte
 * value here silently handed out only-8-byte-aligned SkSL AST nodes/
 * strings from an arena whose own top-level allocation (this project's
 * malloc()) is itself reliably 16-byte aligned. This alignment policy is
 * independent of the SkSL string corruption: that failure was caused by
 * mismatched libc++ string layouts (see HISTORY.md, 2026-09-07).
 * Diverging from Apple's own
 * value on this one point, rather than patching Skia's own vendored
 * source (this project's standing policy -- see libcrtgfx/third_party/
 * skia/README.md), fixes it for every consumer of max_align_t, not just
 * Skia's. */
#if defined(__LDBL_MANT_DIG__) && __LDBL_MANT_DIG__ <= 53 && \
    (defined(__aarch64__) || defined(__arm64__))
typedef struct {
  long long __crt_max_align_ll;
} __attribute__((aligned(16))) max_align_t;
#else
typedef long double max_align_t;
#endif

#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void*)0)
#endif
#endif

#define offsetof(type, member) __builtin_offsetof(type, member)

#endif
