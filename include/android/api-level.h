/*
 * Copyright (C) 2008 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES ARE DISCLAIMED.
 *
 * Adapted from Android Bionic libc/include/android/api-level.h. Host PAL
 * builds represent current platform code, so the default API is FUTURE.
 */

#ifndef CRT_ANDROID_API_LEVEL_H
#define CRT_ANDROID_API_LEVEL_H

#include <sys/cdefs.h>

#define __ANDROID_API_FUTURE__ 10000
#ifndef __ANDROID_API__
#define __ANDROID_API__ __ANDROID_API_FUTURE__
#endif

#define __ANDROID_API_G__ 9
#define __ANDROID_API_I__ 14
#define __ANDROID_API_J__ 16
#define __ANDROID_API_J_MR1__ 17
#define __ANDROID_API_J_MR2__ 18
#define __ANDROID_API_K__ 19
#define __ANDROID_API_L__ 21
#define __ANDROID_API_L_MR1__ 22
#define __ANDROID_API_M__ 23
#define __ANDROID_API_N__ 24
#define __ANDROID_API_N_MR1__ 25
#define __ANDROID_API_O__ 26
#define __ANDROID_API_O_MR1__ 27
#define __ANDROID_API_P__ 28
#define __ANDROID_API_Q__ 29
#define __ANDROID_API_R__ 30
#define __ANDROID_API_S__ 31
#define __ANDROID_API_T__ 33
#define __ANDROID_API_U__ 34
#define __ANDROID_API_V__ 35

#if !defined(__ASSEMBLY__)
__BEGIN_DECLS
int android_get_application_target_sdk_version(void);
int android_get_device_api_level(void);
__END_DECLS
#endif

#endif
