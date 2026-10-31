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

// Web Tranche 8C: the generic (non-Cocoa, non-GStreamer) branch of WebCore's mock capture sources is
// stale upstream (it names classes that no longer exist and leaves pure virtual functions undefined), so
// PlatformCRT does not compile those two files. Mock capture devices are a WebKit-test facility and are
// not part of the CRT product; asking for one fails explicitly.

#include "config.h"

#if ENABLE(MEDIA_STREAM)

#include "MockRealtimeAudioSource.h"
#include "MockRealtimeVideoSource.h"

namespace WebCore {

CaptureSourceOrError MockRealtimeAudioSource::create(String&&, AtomString&&, MediaDeviceHashSalts&&, const MediaConstraints*, std::optional<PageIdentifier>)
{
    return CaptureSourceOrError({ "mock capture devices are not available in the CRT port"_s, MediaAccessDenialReason::HardwareError });
}

CaptureSourceOrError MockRealtimeVideoSource::create(String&&, AtomString&&, MediaDeviceHashSalts&&, const MediaConstraints*, std::optional<PageIdentifier>)
{
    return CaptureSourceOrError({ "mock capture devices are not available in the CRT port"_s, MediaAccessDenialReason::HardwareError });
}

void MockRealtimeAudioSource::setIsInterrupted(bool) { }
void MockRealtimeVideoSource::setIsInterrupted(bool) { }
void MockRealtimeVideoSource::triggerCameraConfigurationChange() { }

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
