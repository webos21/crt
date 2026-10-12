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

#pragma once

#include <WebKit/WKBase.h>

#ifdef __cplusplus
extern "C" {
#endif

// CRT extension of the C API (Web Tranche 3B, milestone B5). The UI process keeps the page's pixels
// in a backing store (Skia, premultiplied BGRA); the PlayStation-shaped API only says "this region
// needs display" (WKViewSetViewNeedsDisplayCallback). An embedder that owns the screen, or the
// shared-memory buffer of the web surface protocol, copies the view out with this call.
//
// Copies the `width` x `height` top-left part of the view into `pixels` (rows `strideBytes` apart,
// bytes B, G, R, A, alpha premultiplied, origin top-left). Returns true when every pixel of the
// rectangle came from the backing store, false when part of it has not been painted yet (that part
// is left untouched, so clear the buffer first if that matters).
WK_EXPORT bool WKViewCRTCopyPixels(WKViewRef view, void* pixels, uint32_t width, uint32_t height, size_t strideBytes);

#ifdef __cplusplus
}
#endif
