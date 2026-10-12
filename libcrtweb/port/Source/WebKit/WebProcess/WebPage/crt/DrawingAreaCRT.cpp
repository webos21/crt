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

#include "DrawingAreaProxyMessages.h"
#include "MessageSenderInlines.h"
#include "UpdateInfo.h"
#include "WebPage.h"
#include "WebPageCreationParameters.h"
#include "WebPageInlines.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/Page.h>
#include <WebCore/Settings.h>
#include <WebCore/ShareableBitmap.h>
#include <wtf/SetForScope.h>

namespace WebKit {
using namespace WebCore;

DrawingAreaCRT::DrawingAreaCRT(WebPage& webPage, const WebPageCreationParameters& parameters)
    : DrawingArea(parameters.drawingAreaIdentifier, webPage)
    , m_displayTimer(RunLoop::mainSingleton(), "DrawingAreaCRT::DisplayTimer"_s, this, &DrawingAreaCRT::displayTimerFired)
    , m_isPaintingSuspended(!(parameters.activityState & ActivityState::IsVisible))
{
}

DrawingAreaCRT::~DrawingAreaCRT() = default;

void DrawingAreaCRT::setNeedsDisplay()
{
    setNeedsDisplayInRect(m_webPage->bounds());
}

void DrawingAreaCRT::setNeedsDisplayInRect(const IntRect& rect)
{
    IntRect dirtyRect = rect;
    dirtyRect.intersect(m_webPage->bounds());
    if (dirtyRect.isEmpty())
        return;

    m_dirtyRegion.unite(dirtyRect);
    scheduleDisplay();
}

void DrawingAreaCRT::scroll(const IntRect& scrollRect, const IntSize& scrollDelta)
{
    if (scrollRect.isEmpty())
        return;

    if (!m_scrollRect.isEmpty() && scrollRect != m_scrollRect) {
        // Two different scroll rects in one update: repaint the pending one and scroll the new one.
        unsigned scrollArea = scrollRect.width() * scrollRect.height();
        unsigned currentScrollArea = m_scrollRect.width() * m_scrollRect.height();

        if (currentScrollArea >= scrollArea) {
            setNeedsDisplayInRect(scrollRect);
            return;
        }

        setNeedsDisplayInRect(m_scrollRect);
        m_scrollRect = IntRect();
        m_scrollOffset = IntSize();
    }

    // Move the dirty parts that are inside the scroll rect along with the scroll.
    WebCore::Region dirtyRegionInScrollRect = intersect(scrollRect, m_dirtyRegion);
    if (!dirtyRegionInScrollRect.isEmpty()) {
        m_dirtyRegion.subtract(scrollRect);
        m_dirtyRegion.unite(intersect(translate(dirtyRegionInScrollRect, scrollDelta), scrollRect));
    }

    // What the scroll uncovers has to be painted.
    m_dirtyRegion.unite(subtract(scrollRect, translate(scrollRect, scrollDelta)));
    scheduleDisplay();

    m_scrollRect = scrollRect;
    m_scrollOffset += scrollDelta;
}

void DrawingAreaCRT::updateRenderingWithForcedRepaint()
{
    m_isWaitingForDidUpdate = false;
    m_dirtyRegion = m_webPage->bounds();
    display();
}

void DrawingAreaCRT::updateRenderingWithForcedRepaintAsync(WebPage&, CompletionHandler<void()>&& completionHandler)
{
    updateRenderingWithForcedRepaint();
    completionHandler();
}

void DrawingAreaCRT::setLayerTreeStateIsFrozen(bool isFrozen)
{
    if (m_layerTreeStateIsFrozen == isFrozen)
        return;

    m_layerTreeStateIsFrozen = isFrozen;
    if (!isFrozen)
        scheduleDisplay();
}

void DrawingAreaCRT::updatePreferences(const WebPreferencesStore&)
{
    // CPU painting only: never enter accelerated compositing mode.
    Settings& settings = m_webPage->corePage()->settings();
    settings.setAcceleratedCompositingEnabled(false);
    settings.setForceCompositingMode(false);
}

void DrawingAreaCRT::setRootCompositingLayer(Frame&, GraphicsLayer*)
{
}

void DrawingAreaCRT::triggerRenderingUpdate()
{
    if (m_layerTreeStateIsFrozen)
        return;
    scheduleDisplay();
}

void DrawingAreaCRT::activityStateDidChange(OptionSet<ActivityState> changed, ActivityStateChangeID, CompletionHandler<void()>&& completionHandler)
{
    if (changed & ActivityState::IsVisible) {
        if (m_webPage->isVisible()) {
            if (m_isPaintingSuspended) {
                m_isPaintingSuspended = false;
                setNeedsDisplay();
            }
        } else {
            m_isPaintingSuspended = true;
            m_displayTimer.stop();
        }
    }
    completionHandler();
}

void DrawingAreaCRT::updateGeometry(const IntSize& size, CompletionHandler<void()>&& completionHandler)
{
    {
        SetForScope inUpdateGeometry(m_inUpdateGeometry, true);
        Ref webPage = m_webPage.get();
        webPage->setSize(size);
        webPage->layoutIfNeeded();

        m_dirtyRegion = IntRect(IntPoint(), size);
        UpdateInfo updateInfo;
        if (m_isPaintingSuspended) {
            updateInfo.viewSize = webPage->size();
            updateInfo.deviceScaleFactor = webPage->corePage()->deviceScaleFactor();
        } else
            display(updateInfo);
        send(Messages::DrawingAreaProxy::Update(0, WTF::move(updateInfo)));
    }

    completionHandler();
}

bool DrawingAreaCRT::enterAcceleratedCompositingModeIfNeeded()
{
    return false;
}

void DrawingAreaCRT::displayDidRefresh(MonotonicTime)
{
    m_isWaitingForDidUpdate = false;

    if (!m_scheduledWhileWaitingForDidUpdate)
        return;
    m_scheduledWhileWaitingForDidUpdate = false;

    displayTimerFired();
}

void DrawingAreaCRT::forceUpdate()
{
    if (m_isWaitingForDidUpdate)
        return;

    m_dirtyRegion = m_webPage->bounds();
    display();
}

void DrawingAreaCRT::didDiscardBackingStore()
{
    // The UI process threw its backing store away: the next update has to cover the whole view.
    m_dirtyRegion = m_webPage->bounds();
}

void DrawingAreaCRT::scheduleDisplay()
{
    if (m_isWaitingForDidUpdate) {
        m_scheduledWhileWaitingForDidUpdate = true;
        return;
    }

    if (m_isPaintingSuspended || m_displayTimer.isActive())
        return;

    m_displayTimer.startOneShot(0_s);
}

void DrawingAreaCRT::displayTimerFired()
{
    display();
}

void DrawingAreaCRT::display()
{
    if (m_layerTreeStateIsFrozen || m_isPaintingSuspended || m_inUpdateGeometry)
        return;

    UpdateInfo updateInfo;
    display(updateInfo);

    if (updateInfo.updateRectBounds.isEmpty())
        return;

    send(Messages::DrawingAreaProxy::Update(0, WTF::move(updateInfo)));
    m_isWaitingForDidUpdate = true;
    m_scheduledWhileWaitingForDidUpdate = false;
}

static bool shouldPaintBoundsRect(const IntRect& bounds, const Vector<IntRect, 1>& rects)
{
    const size_t rectThreshold = 10;
    const double wastedSpaceThreshold = 0.75;

    if (rects.size() <= 1 || rects.size() > rectThreshold)
        return true;

    // Paint the bounds when few of its pixels would be wasted, the individual rects otherwise.
    unsigned boundsArea = bounds.width() * bounds.height();
    unsigned rectsArea = 0;
    for (auto& rect : rects)
        rectsArea += rect.width() * rect.height();

    return 1 - (static_cast<double>(rectsArea) / boundsArea) <= wastedSpaceThreshold;
}

void DrawingAreaCRT::display(UpdateInfo& updateInfo)
{
    Ref webPage = m_webPage.get();
    webPage->updateRendering();
    webPage->finalizeRenderingUpdate({ });
    webPage->flushPendingEditorStateUpdate();

    if (m_dirtyRegion.isEmpty())
        return;

    updateInfo.viewSize = webPage->size();
    updateInfo.deviceScaleFactor = webPage->corePage()->deviceScaleFactor();

    IntRect bounds = m_dirtyRegion.bounds();
    ASSERT(webPage->bounds().contains(bounds));

    float deviceScaleFactor = webPage->corePage()->deviceScaleFactor();
    IntSize bitmapSize = bounds.size();
    bitmapSize.scale(deviceScaleFactor);
    auto bitmap = ShareableBitmap::create({ bitmapSize });
    if (!bitmap)
        return;

    if (auto handle = bitmap->createHandle())
        updateInfo.bitmapHandle = WTF::move(*handle);
    else
        return;

    auto rects = m_dirtyRegion.rects();
    if (shouldPaintBoundsRect(bounds, rects)) {
        rects.clear();
        rects.append(bounds);
    }

    updateInfo.scrollRect = m_scrollRect;
    updateInfo.scrollOffset = m_scrollOffset;

    m_dirtyRegion = WebCore::Region();
    m_scrollRect = IntRect();
    m_scrollOffset = IntSize();

    if (auto graphicsContext = bitmap->createGraphicsContext()) {
        graphicsContext->applyDeviceScaleFactor(deviceScaleFactor);
        graphicsContext->translate(-bounds.x(), -bounds.y());

        for (auto& rect : rects) {
            webPage->drawRect(*graphicsContext, rect);
            updateInfo.updateRects.append(rect);
        }
    }

    updateInfo.updateRectBounds = bounds;

    webPage->didUpdateRendering();

    // Layout can trigger more setNeedsDisplay calls; they are handled after the UI process
    // has composed this update.
    m_displayTimer.stop();
}

} // namespace WebKit
