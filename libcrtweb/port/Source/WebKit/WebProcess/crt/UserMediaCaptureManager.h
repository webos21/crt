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
