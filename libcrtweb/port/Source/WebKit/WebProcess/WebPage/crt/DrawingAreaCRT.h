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
#include <WebCore/IntRect.h>
#include <WebCore/Region.h>
#include <wtf/RunLoop.h>

namespace WebKit {

struct UpdateInfo;

// The CRT Web process drawing area (Web Tranche 3B, milestones B4 and B5).
//
// It paints the page on the CPU and tells the UI process what changed: dirty rectangles are painted
// by WebCore into a ShareableBitmap (BGRA, premultiplied, shared memory) and sent as a
// DrawingAreaProxy::Update message; the UI process composes them into its backing store and
// answers with DisplayDidRefresh, which allows the next update (one update in flight). This is the
// protocol of upstream's non-composited ("classic") drawing area, which the UI-side
// DrawingAreaProxyCoordinatedGraphics still implements for platforms that are neither GTK nor WPE;
// the Web-process half of it (DrawingAreaCoordinatedGraphics.cpp) no longer matches LayerTreeHost
// and the maintained drawing area needs GLib and a GPU, so PlatformCRT carries this one.
//
// There is no accelerated compositing: acceleratedCompositingEnabled is forced off, so WebCore
// paints everything itself. GPU compositing is Tranche 9.
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
    void updateRenderingWithForcedRepaint() override;
    void updateRenderingWithForcedRepaintAsync(WebPage&, CompletionHandler<void()>&&) override;
    void setLayerTreeStateIsFrozen(bool) override;
    bool layerTreeStateIsFrozen() const override { return m_layerTreeStateIsFrozen; }
    void updatePreferences(const WebPreferencesStore&) override;
    void setRootCompositingLayer(WebCore::Frame&, WebCore::GraphicsLayer*) override;
    void triggerRenderingUpdate() override;
    void activityStateDidChange(OptionSet<WebCore::ActivityState>, ActivityStateChangeID, CompletionHandler<void()>&&) override;
    void updateGeometry(const WebCore::IntSize&, CompletionHandler<void()>&&) override;
    bool enterAcceleratedCompositingModeIfNeeded() override;
    void displayDidRefresh(MonotonicTime) override;
    void forceUpdate() override;
    void didDiscardBackingStore() override;

    void scheduleDisplay();
    void displayTimerFired();
    void display();
    void display(UpdateInfo&);

    WebCore::Region m_dirtyRegion;
    WebCore::IntRect m_scrollRect;
    WebCore::IntSize m_scrollOffset;
    RunLoop::Timer m_displayTimer;
    bool m_isWaitingForDidUpdate { false };
    bool m_scheduledWhileWaitingForDidUpdate { false };
    bool m_isPaintingSuspended { false };
    bool m_layerTreeStateIsFrozen { false };
    bool m_inUpdateGeometry { false };
};

} // namespace WebKit
