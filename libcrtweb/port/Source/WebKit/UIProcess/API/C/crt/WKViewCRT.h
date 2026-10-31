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

// Resizes the view: the size WKViewSetSize stores is the one the page client reports, but nothing
// tells the page's drawing area (the Web process keeps laying out at the old size) in the
// PlayStation-shaped API; this sets both. Logical pixels.
WK_EXPORT void WKViewCRTSetSize(WKViewRef view, uint32_t width, uint32_t height);

// Internal lifecycle bridge: tokens are unique launch identifiers and remain running until reaped.
WK_EXPORT uint64_t WKViewCRTProcessToken(WKViewRef view);
WK_EXPORT bool WKViewCRTProcessIsRunning(uint64_t token);
WK_EXPORT size_t WKViewCRTLiveProcessTokens(uint64_t* tokens, size_t capacity);
WK_EXPORT void WKViewCRTTerminateProcess(uint64_t token);
// Observes an existing network process without starting one as a side effect.
WK_EXPORT uint64_t WKWebsiteDataStoreCRTProcessToken(WKWebsiteDataStoreRef store);
WK_EXPORT size_t WKContextCRTProcessTokens(WKContextRef context, uint64_t* tokens, size_t capacity);

// Input (milestone B6): the web surface protocol's input messages, as the contract states them
// (libcrtweb/platform/crtweb_surface_wire.h), turned into WebKit's native events. Coordinates are
// view-local logical pixels; `modifiers` uses the contract's bits (control 1, shift 2, alt 4, meta 8,
// caps lock 16); `timeMs` 0 means "now". WebKit derives `key` from the keysym and `code` from the
// keycode, so both are required; a keycode of 0 makes WebKit drop the event.
enum {
    kWKViewCRTPointerMove = 1,
    kWKViewCRTPointerDown = 2,
    kWKViewCRTPointerUp = 3,
};

// button: 1 primary, 2 middle, 3 secondary (0 for a move).
WK_EXPORT void WKViewCRTHandlePointer(WKViewRef view, uint32_t action, uint32_t button, double x, double y, uint32_t modifiers, uint32_t timeMs);
// Deltas in logical pixels, as the wire protocol: positive y scrolls the content up.
WK_EXPORT void WKViewCRTHandleWheel(WKViewRef view, double x, double y, double deltaX, double deltaY, uint32_t timeMs);
// keyval: an X11/XKB keysym; keycode: the Linux evdev code + 8.
WK_EXPORT void WKViewCRTHandleKey(WKViewRef view, bool down, bool repeat, uint32_t keyval, uint32_t keycode, uint32_t modifiers, uint32_t timeMs);
// Committed text (UTF-8, one code point used): pressed and released on the key a US keyboard
// layout types it with. A character with no such key is dropped; IME composition is deferred.
WK_EXPORT void WKViewCRTHandleText(WKViewRef view, const char* utf8);

// IME (milestone C8). WKViewCRTInsertText commits UTF-8 text of any length (a Hangul syllable, a
// whole string) at the caret, as an input method's commit does; the composition calls drive the
// page's composition state (`compositionstart/update/end`): WKViewCRTSetComposition shows `utf8` as
// the marked text with the caret at cursorStart (+ cursorLength selected), counted in UTF-16 units of
// the composition string; WKViewCRTConfirmComposition commits what is marked; WKViewCRTCancelComposition
// removes it.
WK_EXPORT void WKViewCRTInsertText(WKViewRef view, const char* utf8);
WK_EXPORT void WKViewCRTSetComposition(WKViewRef view, const char* utf8, uint32_t cursorStart, uint32_t cursorLength);
WK_EXPORT void WKViewCRTConfirmComposition(WKViewRef view);
// Ends the composition with `utf8` as its final text (what an input method's commit string is).
WK_EXPORT void WKViewCRTConfirmCompositionWithText(WKViewRef view, const char* utf8);
WK_EXPORT void WKViewCRTCancelComposition(WKViewRef view);

// Tranche 8C: ends every camera/microphone/screen track of this page and releases its capture
// sources (the page's granted access is reset, so a new request asks the embedder again). Other pages
// are untouched.
WK_EXPORT void WKViewCRTStopMediaCapture(WKViewRef view);

#ifdef __cplusplus
}
#endif
