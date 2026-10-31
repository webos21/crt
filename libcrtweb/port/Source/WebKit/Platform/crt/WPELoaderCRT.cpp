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

// The libwpe loader interface of the CRT port. libwpe is linked statically (porting/recipes/
// libwpe.json) and asks the embedder for its backend objects through this symbol. WebKit, as the
// embedder here, provides none: the pasteboard falls back to libwpe's generic in-process one and
// views are created with wpe_view_backend_create_with_backend_interface() by whoever owns the view
// (libcrtweb, Tranche 4). A different embedder replaces this object file.

#include "config.h"

#include <wpe/wpe.h>

extern "C" {

static void* crtLoadObject(const char*)
{
    return nullptr;
}

struct wpe_loader_interface _wpe_loader_interface = { crtLoadObject, nullptr, nullptr, nullptr, nullptr };

}
