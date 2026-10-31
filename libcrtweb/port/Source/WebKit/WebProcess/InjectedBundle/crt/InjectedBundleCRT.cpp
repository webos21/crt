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
