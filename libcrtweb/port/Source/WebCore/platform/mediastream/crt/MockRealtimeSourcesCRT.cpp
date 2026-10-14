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
