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
#include "NativeWebKeyboardEvent.h"
#include "NativeWebMouseEvent.h"
#include "NativeWebWheelEvent.h"
#include "PlayStationWebView.h"
#include "WKAPICast.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include "WebProcessPool.h"
#include "WebsiteDataStore.h"
#include "NetworkProcessProxy.h"
#include <WebCore/IntRect.h>
#include <WebCore/MediaProducer.h>
#include <WebCore/Region.h>
#include <string.h>
#include <time.h>
#include <wpe/wpe.h>
#include <wtf/text/WTFString.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkBitmap.h>
#include <skia/core/SkCanvas.h>
#include <skia/core/SkImageInfo.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

uint64_t WKWebsiteDataStoreCRTProcessToken(WKWebsiteDataStoreRef store)
{
    auto* network = WebKit::toImpl(store)->networkProcessIfExists();
    return network ? network->coreProcessIdentifier().toUInt64() : 0;
}

uint64_t WKViewCRTProcessToken(WKViewRef view)
{
    auto* page = WebKit::toImpl(view)->page();
    return page ? page->legacyMainFrameProcess().coreProcessIdentifier().toUInt64() : 0;
}

size_t WKContextCRTProcessTokens(WKContextRef context, uint64_t* tokens, size_t capacity)
{
    const auto& processes = WebKit::toImpl(context)->processes();
    size_t index = 0;
    for (const auto& process : processes) {
        if (index < capacity) tokens[index] = process->coreProcessIdentifier().toUInt64();
        ++index;
    }
    return index;
}

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

    // A picture of another size is stale (a resize has not been answered by the Web process yet).
    auto storeSize = drawingArea->backingStoreSizeForCRT();
    if (storeSize.width() != static_cast<int>(width) || storeSize.height() != static_cast<int>(height))
        return false;

    SkBitmap bitmap;
    if (!bitmap.installPixels(SkImageInfo::MakeN32Premul(width, height), pixels, strideBytes))
        return false;
    SkCanvas canvas(bitmap);

    WebCore::Region unpainted;
    drawingArea->paint(&canvas, WebCore::IntRect(0, 0, width, height), unpainted);
    return unpainted.isEmpty();
}

void WKViewCRTSetSize(WKViewRef viewRef, uint32_t width, uint32_t height)
{
    auto* view = WebKit::toImpl(viewRef);
    if (!view)
        return;

    WebCore::IntSize size(static_cast<int>(width), static_cast<int>(height));
    view->setViewSize(size);
    if (auto* page = view->page()) {
        if (auto* drawingArea = page->drawingArea())
            drawingArea->setSize(size);
    }
}

// The contract's modifier bits (control 1, shift 2, alt 4, meta 8, caps lock 16) happen to be libwpe's
// keyboard modifier bits for the first four; caps lock is carried by the keysym state in WebKit.
static uint32_t wpeModifiers(uint32_t modifiers)
{
    uint32_t result = 0;
    if (modifiers & 1)
        result |= wpe_input_keyboard_modifier_control;
    if (modifiers & 2)
        result |= wpe_input_keyboard_modifier_shift;
    if (modifiers & 4)
        result |= wpe_input_keyboard_modifier_alt;
    if (modifiers & 8)
        result |= wpe_input_keyboard_modifier_meta;
    return result;
}

static uint32_t eventTime(uint32_t timeMs)
{
    if (timeMs)
        return timeMs;
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<uint32_t>(now.tv_sec * 1000 + now.tv_nsec / 1000000);
}

static WebKit::WebPageProxy* pageFor(WKViewRef viewRef)
{
    auto* view = WebKit::toImpl(viewRef);
    return view ? view->page() : nullptr;
}

void WKViewCRTHandlePointer(WKViewRef viewRef, uint32_t action, uint32_t button, double x, double y, uint32_t modifiers, uint32_t timeMs)
{
    auto* page = pageFor(viewRef);
    if (!page)
        return;

    // libwpe numbers the buttons 1 left, 2 right, 3 middle; the contract 1 primary, 2 middle, 3 secondary.
    uint32_t wpeButton = button == 2 ? 3 : (button == 3 ? 2 : (button == 1 ? 1 : 0));
    uint32_t buttonMask = wpeButton == 1 ? wpe_input_pointer_modifier_button1
        : wpeButton == 2 ? wpe_input_pointer_modifier_button2
        : wpeButton == 3 ? wpe_input_pointer_modifier_button3 : 0;

    wpe_input_pointer_event event = { };
    event.time = eventTime(timeMs);
    event.x = static_cast<int>(x);
    event.y = static_cast<int>(y);
    event.modifiers = wpeModifiers(modifiers);
    switch (action) {
    case kWKViewCRTPointerMove:
        event.type = wpe_input_pointer_event_type_motion;
        break;
    case kWKViewCRTPointerDown:
        event.type = wpe_input_pointer_event_type_button;
        event.state = 1;
        event.button = wpeButton;
        event.modifiers |= buttonMask;
        break;
    case kWKViewCRTPointerUp:
        event.type = wpe_input_pointer_event_type_button;
        event.state = 0;
        event.button = wpeButton;
        break;
    default:
        return;
    }
    page->handleMouseEvent(WebKit::NativeWebMouseEvent(&event, 1));
}

void WKViewCRTHandleWheel(WKViewRef viewRef, double x, double y, double deltaX, double deltaY, uint32_t timeMs)
{
    auto* page = pageFor(viewRef);
    if (!page)
        return;

    wpe_input_axis_2d_event event = { };
    event.base.type = static_cast<wpe_input_axis_event_type>(wpe_input_axis_event_type_motion_smooth | wpe_input_axis_event_type_mask_2d);
    event.base.time = eventTime(timeMs);
    event.base.x = static_cast<int>(x);
    event.base.y = static_cast<int>(y);
    event.x_axis = deltaX;
    event.y_axis = deltaY;
    page->handleNativeWheelEvent(WebKit::NativeWebWheelEvent(&event.base, 1, WebKit::WebWheelEvent::Phase::None, WebKit::WebWheelEvent::Phase::None));
}

void WKViewCRTHandleKey(WKViewRef viewRef, bool down, bool repeat, uint32_t keyval, uint32_t keycode, uint32_t modifiers, uint32_t timeMs)
{
    auto* page = pageFor(viewRef);
    if (!page)
        return;

    wpe_input_keyboard_event event = { };
    event.time = eventTime(timeMs);
    event.key_code = keyval;
    event.hardware_key_code = keycode;
    event.pressed = down;
    event.modifiers = wpeModifiers(modifiers);

    std::optional<Vector<WebCore::CompositionUnderline>> preeditUnderlines;
    std::optional<WebKit::EditingRange> preeditSelectionRange;
    // A null text lets WebKit derive the character from the keysym.
    page->handleKeyboardEvent(WebKit::NativeWebKeyboardEvent(&event, String(), repeat,
        WebKit::NativeWebKeyboardEvent::HandledByInputMethod::No, WTF::move(preeditUnderlines), WTF::move(preeditSelectionRange)));
}

// The key a US layout types an ASCII character with: evdev + 8 (the XKB keycode WebKit expects).
static bool usLayoutKey(uint32_t character, uint32_t& keycode, uint32_t& modifiers)
{
    static const char rowQ[] = "qwertyuiop"; // evdev 16..25
    static const char rowA[] = "asdfghjkl"; // evdev 30..38
    static const char rowZ[] = "zxcvbnm"; // evdev 44..50
    static const char digits[] = "1234567890"; // evdev 2..11
    static const char shiftedDigits[] = "!@#$%^&*()";

    uint32_t lower = character;
    uint32_t evdev = 0;
    uint32_t shift = 0;
    if (character >= 'A' && character <= 'Z') {
        lower = character - 'A' + 'a';
        shift = 2;
    }
    if (lower && lower < 0x80) {
        char c = static_cast<char>(lower);
        const char* hit;
        if ((hit = strchr(rowQ, c)))
            evdev = 16 + static_cast<uint32_t>(hit - rowQ);
        else if ((hit = strchr(rowA, c)))
            evdev = 30 + static_cast<uint32_t>(hit - rowA);
        else if ((hit = strchr(rowZ, c)))
            evdev = 44 + static_cast<uint32_t>(hit - rowZ);
        else if ((hit = strchr(digits, c)))
            evdev = 2 + static_cast<uint32_t>(hit - digits);
        else if ((hit = strchr(shiftedDigits, c))) {
            evdev = 2 + static_cast<uint32_t>(hit - shiftedDigits);
            shift = 2;
        } else if (c == ' ')
            evdev = 57;
        else if (c == '-')
            evdev = 12;
        else if (c == '=')
            evdev = 13;
        else if (c == '.')
            evdev = 52;
        else if (c == ',')
            evdev = 51;
        else if (c == '/')
            evdev = 53;
        else if (c == ';')
            evdev = 39;
    }
    if (!evdev)
        return false;
    keycode = evdev + 8;
    modifiers = shift;
    return true;
}

void WKViewCRTHandleText(WKViewRef view, const char* utf8)
{
    if (!view || !utf8 || !*utf8)
        return;

    // One code point: decode the first UTF-8 sequence.
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(utf8);
    uint32_t character = bytes[0];
    if (character >= 0xC0) {
        unsigned extra = character >= 0xF0 ? 3 : (character >= 0xE0 ? 2 : 1);
        character &= (0x3F >> extra);
        for (unsigned i = 1; i <= extra && bytes[i]; ++i)
            character = (character << 6) | (bytes[i] & 0x3F);
    }

    uint32_t keycode = 0;
    uint32_t modifiers = 0;
    if (!usLayoutKey(character, keycode, modifiers))
        return;
    WKViewCRTHandleKey(view, true, false, character, keycode, modifiers, 0);
    WKViewCRTHandleKey(view, false, false, character, keycode, modifiers, 0);
}

// IME (Web Tranche 4, milestone C8): the embedder's input method owns composition; these entry points
// hand its result to the focused frame's editor through the CRT messages of patch 0021 (the
// non-Cocoa Web process has no composition entry point of its own), and bypass the US-layout key
// mapping of WKViewCRTHandleText so any UTF-8 text can be committed.
template<typename Message>
static void sendToPage(WKViewRef viewRef, Message&& message)
{
    auto* view = WebKit::toImpl(viewRef);
    if (!view || !view->page() || !view->page()->hasRunningProcess())
        return;
    auto* page = view->page();
    page->legacyMainFrameProcess().send(WTF::move(message), page->webPageIDInMainFrameProcess());
}

void WKViewCRTInsertText(WKViewRef view, const char* utf8)
{
    if (utf8 && *utf8)
        sendToPage(view, Messages::WebPage::InsertTextCRT(String::fromUTF8(utf8)));
}

void WKViewCRTSetComposition(WKViewRef view, const char* utf8, uint32_t cursorStart, uint32_t cursorLength)
{
    if (utf8)
        sendToPage(view, Messages::WebPage::SetCompositionCRT(String::fromUTF8(utf8), cursorStart, cursorLength));
}

void WKViewCRTConfirmComposition(WKViewRef view)
{
    sendToPage(view, Messages::WebPage::ConfirmCompositionCRT(String()));
}

void WKViewCRTConfirmCompositionWithText(WKViewRef view, const char* utf8)
{
    if (utf8)
        sendToPage(view, Messages::WebPage::ConfirmCompositionCRT(String::fromUTF8(utf8)));
}

void WKViewCRTCancelComposition(WKViewRef view)
{
    sendToPage(view, Messages::WebPage::SetCompositionCRT(String(), 0, 0));
}

void WKViewCRTStopMediaCapture(WKViewRef viewRef)
{
    auto* view = WebKit::toImpl(viewRef);
    if (!view || !view->page())
        return;
#if ENABLE(MEDIA_STREAM)
    view->page()->stopMediaCapture(WebCore::MediaProducerMediaCaptureKind::EveryKind);
#endif
}
