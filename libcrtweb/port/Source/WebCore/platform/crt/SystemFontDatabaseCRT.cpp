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
