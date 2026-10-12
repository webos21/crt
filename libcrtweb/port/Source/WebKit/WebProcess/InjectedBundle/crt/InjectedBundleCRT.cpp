// Copyright (c) 2026 The CRT Project. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in
//    the documentation and/or other materials provided with the
//    distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
// OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
// AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
// OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.

// The CRT injected bundle loader (Web Tranche 3B, milestone B4): a bundle is a shared object that
// exports WKBundleInitialize, loaded with dlopen() into the Web process. PlayStation uses its SDK's
// LibraryBundle; the generic POSIX loader is all CRT needs. On a host whose libdl cannot load ELF
// objects (the CRT Linux libdl, docs/design/dynamic_loading.md) the load fails and the Web process
// continues without a bundle: injected bundles are not part of the CRT product API.

#include "config.h"
#include "InjectedBundle.h"

#include "WKBundleAPICast.h"
#include "WKBundleInitialize.h"
#include <dlfcn.h>
#include <stdio.h>

namespace WebKit {

bool InjectedBundle::initialize(const WebProcessCreationParameters&, RefPtr<API::Object>&& initializationUserData)
{
    if (m_path.isEmpty())
        return false;

    void* handle = dlopen(m_path.utf8().data(), RTLD_NOW);
    if (!handle) {
        fprintf(stderr, "crtweb: could not load the injected bundle %s: %s\n", m_path.utf8().data(), dlerror());
        return false;
    }
    m_platformBundle = handle;

    auto initializeFunction = reinterpret_cast<WKBundleInitializeFunctionPtr>(dlsym(handle, "WKBundleInitialize"));
    if (!initializeFunction) {
        fprintf(stderr, "crtweb: %s does not export WKBundleInitialize\n", m_path.utf8().data());
        return false;
    }
    initializeFunction(toAPI(this), toAPI(initializationUserData.get()));
    return true;
}

void InjectedBundle::setBundleParameter(const String&, std::span<const uint8_t>)
{
}

void InjectedBundle::setBundleParameters(std::span<const uint8_t>)
{
}

} // namespace WebKit
