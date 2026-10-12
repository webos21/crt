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
#include "DrawingAreaCRT.h"

#include "WebPage.h"

namespace WebKit {

DrawingAreaCRT::DrawingAreaCRT(WebPage& webPage, const WebPageCreationParameters& parameters)
    : DrawingArea(parameters.drawingAreaIdentifier, webPage)
{
}

DrawingAreaCRT::~DrawingAreaCRT() = default;

void DrawingAreaCRT::setNeedsDisplay()
{
}

void DrawingAreaCRT::setNeedsDisplayInRect(const WebCore::IntRect&)
{
}

void DrawingAreaCRT::scroll(const WebCore::IntRect&, const WebCore::IntSize&)
{
}

void DrawingAreaCRT::updateRenderingWithForcedRepaintAsync(WebPage&, CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

void DrawingAreaCRT::setRootCompositingLayer(WebCore::Frame&, WebCore::GraphicsLayer*)
{
}

void DrawingAreaCRT::triggerRenderingUpdate()
{
}

void DrawingAreaCRT::updateGeometry(const WebCore::IntSize&, CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

bool DrawingAreaCRT::enterAcceleratedCompositingModeIfNeeded()
{
    return false;
}

} // namespace WebKit
