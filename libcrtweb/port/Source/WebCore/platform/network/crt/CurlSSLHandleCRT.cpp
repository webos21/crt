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

#include "config.h"
#include "CurlSSLHandle.h"
#include <cstdlib>
#include <cstring>

namespace WebCore {

// CRT trust policy, not a host certificate-store adapter. The public libcrtweb configure API freezes
// these launch parameters before WebKit starts; each process copies the PEM into Curl's CA blob.
// An empty bundle intentionally supplies invalid certificate data rather than letting libcurl fall
// back to a build-time CA path. The CRT curl port also disables default CA bundle/path discovery.
void CurlSSLHandle::platformInitialize()
{
    const char* ca = std::getenv("CRTWEB_INTERNAL_CA_PEM");
    if (!ca || !*ca)
        ca = "CRT: no trust anchors configured";
    auto bytes = std::span(reinterpret_cast<const uint8_t*>(ca), std::strlen(ca));
    setCACertData(CertificateInfo::makeCertificate(bytes));
    const char* insecure = std::getenv("CRTWEB_INTERNAL_INSECURE_TLS");
    setIgnoreSSLErrors(insecure && !std::strcmp(insecure, "1"));
}

} // namespace WebCore
