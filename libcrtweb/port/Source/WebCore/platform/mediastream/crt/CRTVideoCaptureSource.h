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

// Web Tranche 8, gate 8C: the CRT video capture source. A RealtimeVideoCaptureSource over the installed
// crtmedia capture API (video only: no microphone, no display capture, no WebRTC). Device discovery lists
// devices without opening them; open/start follow an explicit grant (WebKit asks the application first).
// A worker thread owns the crtmedia capture handle; frames are converted to RGBA and handed to observers
// as VideoFrameCRT. The synthetic source selected by CRTWEB_TEST_SYNTHETIC_CAMERA is test-only: it has its
// own label and is never reported as a physical camera.

#pragma once

#if ENABLE(MEDIA_STREAM)

#include "CaptureDeviceManager.h"
#include "RealtimeMediaSourceFactory.h"
#include "RealtimeVideoCaptureSource.h"
#include "VideoFrame.h"
#include <wtf/Vector.h>

namespace WebCore {

constexpr uint32_t kVideoFrameCRTPixelFormat = 0x43525430; // 'CRT0': tightly packed RGBA

// A CPU frame (RGBA, tightly packed) produced by the CRT capture source.
class VideoFrameCRT final : public VideoFrame {
public:
    static Ref<VideoFrameCRT> create(MediaTime presentationTime, IntSize size, Vector<uint8_t>&& rgba)
    {
        return adoptRef(*new VideoFrameCRT(presentationTime, size, WTF::move(rgba)));
    }
    static bool isInstance(const VideoFrame& frame) { return frame.pixelFormat() == kVideoFrameCRTPixelFormat; }

    std::span<const uint8_t> pixels() const { return m_rgba.span(); }
    IntSize presentationSize() const final { return m_size; }
    uint32_t pixelFormat() const final { return kVideoFrameCRTPixelFormat; }

private:
    VideoFrameCRT(MediaTime presentationTime, IntSize size, Vector<uint8_t>&& rgba)
        : VideoFrame(presentationTime, false, VideoFrameRotation::None)
        , m_size(size)
        , m_rgba(WTF::move(rgba))
    {
    }
    Ref<VideoFrame> clone() final { return create(presentationTime(), m_size, Vector<uint8_t>(m_rgba)); }

    IntSize m_size;
    Vector<uint8_t> m_rgba;
};

class CRTCaptureDeviceManager final : public CaptureDeviceManager {
public:
    static CRTCaptureDeviceManager& singleton();
    const Vector<CaptureDevice>& captureDevices() final;
    void computeCaptureDevices(CompletionHandler<void()>&&) final;

private:
    void refresh();
    Vector<CaptureDevice> m_devices;
    bool m_enumerated { false };
};

class CRTVideoCaptureSource final : public RealtimeVideoCaptureSource {
public:
    static CaptureSourceOrError create(const CaptureDevice&, MediaDeviceHashSalts&&, const MediaConstraints*);
    ~CRTVideoCaptureSource();

private:
    CRTVideoCaptureSource(const CaptureDevice&, MediaDeviceHashSalts&&);

    const RealtimeMediaSourceCapabilities& capabilities() final;
    const RealtimeMediaSourceSettings& settings() final;
    void generatePresets() final;
    void startProducingData() final;
    void stopProducingData() final;
    void settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag>) final;
    bool isCaptureSource() const final { return true; }
    CaptureDevice::DeviceType deviceType() const final { return CaptureDevice::DeviceType::Camera; }
#if !RELEASE_LOG_DISABLED
    ASCIILiteral logClassName() const final { return "CRTVideoCaptureSource"_s; }
#endif

    class Worker;
    RefPtr<Worker> m_worker;
    String m_deviceID;
    bool m_synthetic { false };
    std::optional<RealtimeMediaSourceCapabilities> m_capabilities;
    std::optional<RealtimeMediaSourceSettings> m_currentSettings;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
