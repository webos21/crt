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

#include "config.h"
#include "WKViewCRT.h"

#include "DrawingAreaProxyCoordinatedGraphics.h"
#include "PlayStationWebView.h"
#include "WKAPICast.h"
#include "WebPageProxy.h"
#include <WebCore/IntRect.h>
#include <WebCore/Region.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkBitmap.h>
#include <skia/core/SkCanvas.h>
#include <skia/core/SkImageInfo.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

bool WKViewCRTCopyPixels(WKViewRef viewRef, void* pixels, uint32_t width, uint32_t height, size_t strideBytes)
{
    auto* view = WebKit::toImpl(viewRef);
    if (!view || !pixels || !width || !height || strideBytes < static_cast<size_t>(width) * 4)
        return false;

    auto* page = view->page();
    if (!page)
        return false;

    // The CRT process model always uses the Coordinated Graphics proxy (PageClientImpl).
    auto* drawingArea = static_cast<WebKit::DrawingAreaProxyCoordinatedGraphics*>(page->drawingArea());
    if (!drawingArea)
        return false;

    SkBitmap bitmap;
    if (!bitmap.installPixels(SkImageInfo::MakeN32Premul(width, height), pixels, strideBytes))
        return false;
    SkCanvas canvas(bitmap);

    WebCore::Region unpainted;
    drawingArea->paint(&canvas, WebCore::IntRect(0, 0, width, height), unpainted);
    return unpainted.isEmpty();
}
