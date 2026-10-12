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

#include "DrawingArea.h"

namespace WebKit {

// The CRT Web process drawing area (Web Tranche 3B). Milestone B4 only needs the process model, so
// this area accepts every call and paints nothing; milestone B5 makes it paint on the CPU into a
// ShareableBitmap and hand the frame to the UI process. It exists because the drawing areas
// upstream builds for this platform shape are the GLib/GL ones (DrawingAreaCoordinatedGraphicsGLib
// with LayerTreeHost, ThreadedCompositor, AcceleratedSurface), and the PlayStation-era one
// (DrawingAreaCoordinatedGraphics.cpp) no longer matches LayerTreeHost's interface.
class DrawingAreaCRT final : public DrawingArea {
public:
    static Ref<DrawingAreaCRT> create(WebPage& webPage, const WebPageCreationParameters& parameters)
    {
        return adoptRef(*new DrawingAreaCRT(webPage, parameters));
    }

    virtual ~DrawingAreaCRT();

private:
    DrawingAreaCRT(WebPage&, const WebPageCreationParameters&);

    // DrawingArea
    void setNeedsDisplay() override;
    void setNeedsDisplayInRect(const WebCore::IntRect&) override;
    void scroll(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollDelta) override;
    void updateRenderingWithForcedRepaintAsync(WebPage&, CompletionHandler<void()>&&) override;
    void setRootCompositingLayer(WebCore::Frame&, WebCore::GraphicsLayer*) override;
    void triggerRenderingUpdate() override;
    void updateGeometry(const WebCore::IntSize&, CompletionHandler<void()>&&) override;
    bool enterAcceleratedCompositingModeIfNeeded() override;
};

} // namespace WebKit
