/*
 * Copyright (C) 2026 The CRT Project. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
