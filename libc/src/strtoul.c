/*	$OpenBSD: strtoul.c,v 1.7 2005/08/08 08:05:37 espie Exp $	*/

/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
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
 *
 * Adapted from Android Bionic libc/stdlib/strtoul.c.
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

unsigned long strtoul(const char* nptr, char** endptr, int base) {
  const char* s = nptr;
  unsigned long acc = 0;
  unsigned long cutoff;
  int c;
  int neg = 0;
  int any = 0;
  int cutlim;

  do {
    c = (unsigned char)*s++;
  } while (isspace(c));

  if (c == '-') {
    neg = 1;
    c = (unsigned char)*s++;
  } else if (c == '+') {
    c = (unsigned char)*s++;
  }

  if ((base == 0 || base == 16) && c == '0' && (*s == 'x' || *s == 'X')) {
    c = (unsigned char)s[1];
    s += 2;
    base = 16;
  }

  if (base == 0) {
    base = c == '0' ? 8 : 10;
  }
  if (base < 2 || base > 36) {
    errno = EINVAL;
    if (endptr != 0) {
      *endptr = (char*)nptr;
    }
    return 0;
  }

  cutoff = ULONG_MAX / (unsigned long)base;
  cutlim = (int)(ULONG_MAX % (unsigned long)base);

  for (;; c = (unsigned char)*s++) {
    if (isdigit(c)) {
      c -= '0';
    } else if (isalpha(c)) {
      c -= isupper(c) ? 'A' - 10 : 'a' - 10;
    } else {
      break;
    }
    if (c >= base) {
      break;
    }
    if (any < 0) {
      continue;
    }
    if (acc > cutoff || (acc == cutoff && c > cutlim)) {
      any = -1;
      acc = ULONG_MAX;
      errno = ERANGE;
    } else {
      any = 1;
      acc *= (unsigned long)base;
      acc += (unsigned long)c;
    }
  }

  if (neg && any > 0) {
    acc = (unsigned long)(0UL - acc);
  }
  if (endptr != 0) {
    *endptr = (char*)(any ? s - 1 : nptr);
  }
  return acc;
}
