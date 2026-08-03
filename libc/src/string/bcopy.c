/*	$OpenBSD: bcopy.c,v 1.5 2005/08/08 08:05:37 espie Exp $ */
/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdint.h>
#include <string.h>

/*
 * Derived from Android Bionic ics-mr0 libc/string/bcopy.c.
 * Local adaptation: use uintptr_t for pointer-width arithmetic so the code is
 * correct on Windows LLP64, where long is 32-bit.
 */
typedef uintptr_t word;

#define wsize sizeof(word)
#define wmask (wsize - 1)

#ifdef MEMCOPY
void* memcpy(void* dst0, const void* src0, size_t length)
#elif defined(MEMMOVE)
void* memmove(void* dst0, const void* src0, size_t length)
#else
void bcopy(const void* src0, void* dst0, size_t length)
#endif
{
  char* dst = dst0;
  const char* src = src0;
  size_t t;

  if (length == 0 || dst == src) {
    goto done;
  }

#define TLOOP(s) \
  if (t) TLOOP1(s)
#define TLOOP1(s) \
  do {             \
    s;             \
  } while (--t)

  if ((uintptr_t)dst < (uintptr_t)src) {
    t = (uintptr_t)src;
    if ((t | (uintptr_t)dst) & wmask) {
      if ((t ^ (uintptr_t)dst) & wmask || length < wsize) {
        t = length;
      } else {
        t = wsize - (t & wmask);
      }
      length -= t;
      TLOOP1(*dst++ = *src++);
    }
    t = length / wsize;
    TLOOP(*(word*)dst = *(const word*)src; src += wsize; dst += wsize);
    t = length & wmask;
    TLOOP(*dst++ = *src++);
  } else {
    src += length;
    dst += length;
    t = (uintptr_t)src;
    if ((t | (uintptr_t)dst) & wmask) {
      if ((t ^ (uintptr_t)dst) & wmask || length <= wsize) {
        t = length;
      } else {
        t &= wmask;
      }
      length -= t;
      TLOOP1(*--dst = *--src);
    }
    t = length / wsize;
    TLOOP(src -= wsize; dst -= wsize; *(word*)dst = *(const word*)src);
    t = length & wmask;
    TLOOP(*--dst = *--src);
  }

done:
#if defined(MEMCOPY) || defined(MEMMOVE)
  return dst0;
#else
  return;
#endif
}
