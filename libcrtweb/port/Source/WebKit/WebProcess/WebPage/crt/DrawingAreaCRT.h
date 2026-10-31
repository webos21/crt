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
