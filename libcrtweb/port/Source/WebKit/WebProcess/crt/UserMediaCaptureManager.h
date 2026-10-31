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

// Web Tranche 8C: PlatformCRT captures in the Web process through RealtimeMediaSourceCenter, and the UI
// process lists devices and validates constraints itself (the non-GLib path of
// UserMediaPermissionRequestManagerProxy), so no capture IPC supplement is registered. WebProcess.cpp
// still names the type for its accessor; this header provides it, as Cocoa and GLib provide theirs.

#pragma once

#if ENABLE(MEDIA_STREAM)

#include "WebProcessSupplement.h"
#include <wtf/TZoneMalloc.h>

namespace WebKit {

class WebProcess;

class UserMediaCaptureManager : public WebProcessSupplement {
    WTF_MAKE_TZONE_ALLOCATED(UserMediaCaptureManager);
    WTF_MAKE_NONCOPYABLE(UserMediaCaptureManager);
public:
    static ASCIILiteral supplementName() { return "UserMediaCaptureManager"_s; }
};

} // namespace WebKit

#endif // ENABLE(MEDIA_STREAM)
