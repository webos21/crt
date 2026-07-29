/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <stddef.h>
#include <string.h>

#ifndef __BEGIN_DECLS
#ifdef __cplusplus
#define __BEGIN_DECLS extern "C" {
#define __END_DECLS }
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif
#endif

#define __BEGIN_HIDDEN_DECLS _Pragma("GCC visibility push(hidden)")
#define __END_HIDDEN_DECLS _Pragma("GCC visibility pop")

extern const char* __progname;

/* Ignore all DEF_STRONG/DEF_WEAK in OpenBSD. */
#define DEF_STRONG(sym)
#define DEF_WEAK(sym)
#define __weak_alias __strong_alias

/* Ignore all __warn_references in OpenBSD. */
#define __warn_references(sym,msg)

#define PROTO_NORMAL(x)

#ifndef __LIBC_HIDDEN__
#define __LIBC_HIDDEN__ __attribute__((visibility("hidden")))
#endif

#ifndef memset_explicit
#define explicit_bzero(p, s) memset((p), 0, (s))
#endif
