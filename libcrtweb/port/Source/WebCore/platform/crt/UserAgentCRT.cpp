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

// The CRT user agent (Web Tranche 3B). The user agent is fragile (see WebCore/platform/glib/
// UserAgentGLib.cpp's warning): it must say Safari-compatible WebKit, and the version token is the
// frozen WebKit one. The platform token says "CRT" rather than imitating an operating system, as the
// PlayStation port says "PlayStation"; the host OS is deliberately not reported (CRT presents one
// Linux-shaped platform on every host).

#include "config.h"
#include "UserAgent.h"

#include <wtf/NeverDestroyed.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

static constexpr ASCIILiteral versionForUAString()
{
    // https://bugs.webkit.org/show_bug.cgi?id=180365
    return "605.1.15"_s;
}

static String standardUserAgentStatic()
{
    static NeverDestroyed<String> uaStatic(makeString("Mozilla/5.0 (CRT) AppleWebKit/"_s, versionForUAString(), " (KHTML, like Gecko) Version/17.0 Safari/"_s, versionForUAString()));
    return uaStatic;
}

String standardUserAgent(const String& applicationName, const String& applicationVersion)
{
    if (applicationName.isEmpty())
        return standardUserAgentStatic();

    return makeString(standardUserAgentStatic(), ' ', applicationName, '/', applicationVersion.isEmpty() ? versionForUAString() : applicationVersion);
}

String standardUserAgentForURL(const URL&)
{
    return standardUserAgentStatic();
}

} // namespace WebCore
