// Copyright (c) 2026 The CRT Project.
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Force-included into WebCore/crypto/openssl/CryptoKeyRSAOpenSSL.cpp (see PlatformCRT.cmake).
// OpenSSL 3 returns `const RSA*` from EVP_PKEY_get0_RSA(); WebKit's file assigns it to `RSA*`.
#pragma once

#include <openssl/evp.h>
#include <openssl/rsa.h>

#define EVP_PKEY_get0_RSA(key) (const_cast<RSA*>(EVP_PKEY_get0_RSA(key)))
