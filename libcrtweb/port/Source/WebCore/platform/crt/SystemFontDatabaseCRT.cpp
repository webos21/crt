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

// The CRT system font shorthands (`font: caption`, `-webkit-small-control`, ... that form controls
// and the UA stylesheet use). PlayStation's file answers the standard family at 16 px for every
// shorthand and logs "not implemented"; here the family is the generic sans-serif, which the fontconfig
// layer of the Skia font cache resolves against the embedder's font set, with the sizes the other
// ports use (13 px controls, 11 px mini controls). A control whose font cannot be resolved has a line
// height of zero and collapses to its border, which is what the first B6 run showed.

#include "config.h"
#include "SystemFontDatabase.h"

#include <wtf/NeverDestroyed.h>

namespace WebCore {

SystemFontDatabase& SystemFontDatabase::singleton()
{
    static NeverDestroyed<SystemFontDatabase> database = SystemFontDatabase();
    return database.get();
}

auto SystemFontDatabase::platformSystemFontShorthandInfo(FontShorthand fontShorthand) -> SystemFontShorthandInfo
{
    float size = 13;
    switch (fontShorthand) {
    case FontShorthand::WebkitMiniControl:
        size = 11;
        break;
    case FontShorthand::WebkitSmallControl:
        size = 13;
        break;
    default:
        break;
    }
    return { AtomString { "sans-serif"_s }, size, normalWeightValue() };
}

void SystemFontDatabase::platformInvalidate()
{
}

} // namespace WebCore
