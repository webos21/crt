/*
 * Copyright (C) 2014 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Adapted from Android Bionic libc/include/ctype.h.
 */

#include <ctype.h>

static int ctype_in_range(unsigned lo, int ch, unsigned hi) {
  return ((unsigned)ch - lo) < (hi - lo + 1);
}

static int tolower_unsafe(int ch) {
  return ch | 0x20;
}

static int toupper_unsafe(int ch) {
  return ch ^ 0x20;
}

int isalpha(int ch) {
  return ctype_in_range('a', tolower_unsafe(ch), 'z');
}

int isblank(int ch) {
  return ch == ' ' || ch == '\t';
}

int iscntrl(int ch) {
  return ((unsigned)ch < ' ') || ch == 0x7f;
}

int isdigit(int ch) {
  return ctype_in_range('0', ch, '9');
}

int isgraph(int ch) {
  return ctype_in_range('!', ch, '~');
}

int islower(int ch) {
  return ctype_in_range('a', ch, 'z');
}

int isprint(int ch) {
  return ctype_in_range(' ', ch, '~');
}

int isspace(int ch) {
  return ch == ' ' || ctype_in_range('\t', ch, '\r');
}

int isupper(int ch) {
  return ctype_in_range('A', ch, 'Z');
}

int isxdigit(int ch) {
  return isdigit(ch) || ctype_in_range('a', tolower_unsafe(ch), 'f');
}

int isalnum(int ch) {
  return isalpha(ch) || isdigit(ch);
}

int ispunct(int ch) {
  return isgraph(ch) && !isalnum(ch);
}

int tolower(int ch) {
  return ctype_in_range('A', ch, 'Z') ? tolower_unsafe(ch) : ch;
}

int toupper(int ch) {
  return ctype_in_range('a', ch, 'z') ? toupper_unsafe(ch) : ch;
}

int isascii(int ch) {
  return (unsigned)ch < 0x80;
}

int toascii(int ch) {
  return ch & 0x7f;
}
