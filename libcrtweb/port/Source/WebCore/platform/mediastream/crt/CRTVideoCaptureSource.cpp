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

#include "config.h"
#include "CRTVideoCaptureSource.h"

#if ENABLE(MEDIA_STREAM)

#include "DisplayCaptureManager.h"
#include "MediaConstraints.h"
#include "RealtimeMediaSourceCenter.h"
#include "RealtimeMediaSourceSettings.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/Threading.h>
#include <wtf/text/MakeString.h>

#include <crtmedia/capture.h>
#include <crtmedia/frame.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

namespace WebCore {

static constexpr ASCIILiteral syntheticDeviceID = "crt-test-synthetic-camera"_s;
static constexpr ASCIILiteral syntheticDeviceLabel = "CRT test camera (synthetic)"_s;

static bool syntheticCameraSelected()
{
    const char* value = getenv("CRTWEB_TEST_SYNTHETIC_CAMERA");
    return value && *value;
}

// Test-only: the synthetic source appends its lifecycle to the file named by CRTWEB_TEST_CAMERA_STATE.
static void recordTestState(const char* event)
{
    const char* path = getenv("CRTWEB_TEST_CAMERA_STATE");
    if (!path || !*path)
        return;
    if (FILE* file = fopen(path, "a")) {
        fprintf(file, "%s\n", event);
        fclose(file);
    }
}

// ---- devices ---------------------------------------------------------------------------------------

CRTCaptureDeviceManager& CRTCaptureDeviceManager::singleton()
{
    static NeverDestroyed<CRTCaptureDeviceManager> manager;
    return manager;
}

void CRTCaptureDeviceManager::refresh()
{
    m_devices.clear();
    m_enumerated = true;
    if (syntheticCameraSelected()) {
        m_devices.append(CaptureDevice(String(syntheticDeviceID), CaptureDevice::DeviceType::Camera, String(syntheticDeviceLabel), emptyString(), true, true, false, false));
        return;
    }
    // Metadata only: crtmedia_capture_enumerate neither opens nor starts a device for streaming.
    crtmedia_capture_device_info infos[16];
    size_t count = 0;
    if (crtmedia_capture_enumerate(infos, 16, &count) != CRTMEDIA_OK)
        return;
    if (count > 16)
        count = 16;
    for (size_t i = 0; i < count; ++i)
        m_devices.append(CaptureDevice(String::fromUTF8(infos[i].id), CaptureDevice::DeviceType::Camera, String::fromUTF8(infos[i].name), emptyString(), true, i == 0, false, false));
}

const Vector<CaptureDevice>& CRTCaptureDeviceManager::captureDevices()
{
    if (!m_enumerated)
        refresh();
    return m_devices;
}

void CRTCaptureDeviceManager::computeCaptureDevices(CompletionHandler<void()>&& callback)
{
    refresh();
    callback();
}

// ---- the capture worker ----------------------------------------------------------------------------

class CRTVideoCaptureSource::Worker final : public ThreadSafeRefCounted<Worker> {
public:
    static Ref<Worker> create(String&& deviceID, bool synthetic, uint32_t width, uint32_t height, uint32_t frameRate)
    {
        return adoptRef(*new Worker(WTF::move(deviceID), synthetic, width, height, frameRate));
    }

    // `deliver` runs on the worker thread for every frame; `failed` once on the main run loop.
    void start(Function<void(Ref<VideoFrameCRT>&&)>&& deliver, Function<void(String&&)>&& failed)
    {
        m_deliver = WTF::move(deliver);
        m_failed = WTF::move(failed);
        Ref protectedThis { *this };
        m_thread = Thread::create("CRTCapture"_s, [protectedThis = WTF::move(protectedThis)] { protectedThis->run(); });
    }

    // Stops and joins. After this returns no frame is delivered.
    void stop()
    {
        m_stop = true;
        if (RefPtr thread = std::exchange(m_thread, nullptr))
            thread->waitForCompletion();
    }

private:
    Worker(String&& deviceID, bool synthetic, uint32_t width, uint32_t height, uint32_t frameRate)
        : m_deviceID(WTF::move(deviceID))
        , m_synthetic(synthetic)
        , m_width(width)
        , m_height(height)
        , m_frameRate(frameRate)
    {
    }

    void fail(String&& message)
    {
        RunLoop::mainSingleton().dispatch([protectedThis = Ref { *this }, message = WTF::move(message)]() mutable {
            if (protectedThis->m_stop)
                return;
            if (protectedThis->m_failed)
                protectedThis->m_failed(WTF::move(message));
        });
    }

    void run()
    {
        if (m_synthetic)
            runSynthetic();
        else
            runDevice();
    }

    void runSynthetic()
    {
        recordTestState("open");
        recordTestState("start");
        uint32_t frameNumber = 0;
        auto interval = std::chrono::microseconds(1000000 / std::max<uint32_t>(m_frameRate, 1));
        auto next = std::chrono::steady_clock::now();
        auto origin = next;
        while (!m_stop) {
            next += interval;
            Vector<uint8_t> rgba(static_cast<size_t>(m_width) * m_height * 4);
            // Quadrants: red, green / blue, yellow; a white bar sweeps down so consecutive frames differ.
            uint32_t barRow = (frameNumber * 4) % m_height;
            for (uint32_t y = 0; y < m_height; ++y) {
                for (uint32_t x = 0; x < m_width; ++x) {
                    uint8_t* pixel = &rgba[(static_cast<size_t>(y) * m_width + x) * 4];
                    bool right = x >= m_width / 2, lower = y >= m_height / 2;
                    pixel[0] = ((!lower && !right) || (lower && right)) ? 255 : 0;
                    pixel[1] = (!lower && right) || (lower && right) ? 255 : 0;
                    pixel[2] = (lower && !right) ? 255 : 0;
                    pixel[3] = 255;
                    if (y == barRow || y == (barRow + 1) % m_height)
                        pixel[0] = pixel[1] = pixel[2] = 255;
                }
            }
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - origin).count();
            if (m_deliver)
                m_deliver(VideoFrameCRT::create(MediaTime(elapsed, 1000000), IntSize(m_width, m_height), WTF::move(rgba)));
            ++frameNumber;
            std::this_thread::sleep_until(next);
        }
        recordTestState("stop");
        recordTestState("close");
    }

    void runDevice()
    {
        crtmedia_capture* capture = nullptr;
        crtmedia_capture_config requested = { };
        requested.width = m_width;
        requested.height = m_height;
        requested.frame_rate = m_frameRate;
        crtmedia_capture_config actual = { };
        CString id = m_deviceID.utf8();
        crtmedia_result result = crtmedia_capture_open(id.data(), &requested, &capture, &actual);
        if (result != CRTMEDIA_OK || !capture) {
            fail(makeString("the camera could not be opened (crtmedia result "_s, static_cast<int>(result), ')'));
            return;
        }
        if (crtmedia_capture_start(capture) != CRTMEDIA_OK) {
            crtmedia_capture_release(capture);
            fail("the camera could not be started"_s);
            return;
        }
        int consecutiveFailures = 0;
        while (!m_stop) {
            crtmedia_frame frame;
            memset(&frame, 0, sizeof frame);
            crtmedia_result got = crtmedia_capture_dequeue_frame(capture, 100, &frame);
            if (got == CRTMEDIA_WOULD_BLOCK || got == CRTMEDIA_ERROR_TIMEOUT)
                continue;
            if (got != CRTMEDIA_OK) {
                if (++consecutiveFailures < 5)
                    continue;
                fail("the camera stopped delivering frames"_s);
                break;
            }
            consecutiveFailures = 0;
            Vector<uint8_t> rgba(static_cast<size_t>(frame.width) * frame.height * 4);
            crtmedia_frame target;
            memset(&target, 0, sizeof target);
            target.format = CRTMEDIA_PIXEL_FORMAT_RGBA8888;
            target.width = frame.width;
            target.height = frame.height;
            target.plane_count = 1;
            target.planes[0].stride = frame.width * 4;
            target.planes[0].width = frame.width;
            target.planes[0].height = frame.height;
            target.planes[0].data = rgba.mutableSpan().data();
            bool converted = crtmedia_frame_convert_to_rgba(&frame, &target) == CRTMEDIA_OK;
            IntSize size(frame.width, frame.height);
            MediaTime time(frame.timestamp_us, 1000000);
            crtmedia_frame_release(&frame);
            if (converted && m_deliver && !m_stop)
                m_deliver(VideoFrameCRT::create(time, size, WTF::move(rgba)));
        }
        crtmedia_capture_stop(capture);
        crtmedia_capture_release(capture);
    }

    String m_deviceID;
    bool m_synthetic;
    uint32_t m_width, m_height, m_frameRate;
    std::atomic<bool> m_stop { false };
    Function<void(Ref<VideoFrameCRT>&&)> m_deliver;
    Function<void(String&&)> m_failed;
    RefPtr<Thread> m_thread;
};

// ---- the source ------------------------------------------------------------------------------------

CaptureSourceOrError CRTVideoCaptureSource::create(const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints)
{
    if (!device || device.type() != CaptureDevice::DeviceType::Camera)
        return CaptureSourceOrError({ "the CRT capture source supports video cameras only"_s, MediaAccessDenialReason::PermissionDenied });
    auto source = adoptRef(*new CRTVideoCaptureSource(device, WTF::move(hashSalts)));
    if (constraints) {
        if (auto result = source->applyConstraints(*constraints))
            return CaptureSourceOrError(CaptureSourceError { result->invalidConstraint });
    }
    return CaptureSourceOrError(WTF::move(source));
}

CRTVideoCaptureSource::CRTVideoCaptureSource(const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts)
    : RealtimeVideoCaptureSource(device, WTF::move(hashSalts), { })
    , m_deviceID(device.persistentId())
    , m_synthetic(device.persistentId() == syntheticDeviceID && syntheticCameraSelected())
{
}

CRTVideoCaptureSource::~CRTVideoCaptureSource()
{
    if (RefPtr worker = std::exchange(m_worker, nullptr))
        worker->stop();
}

void CRTVideoCaptureSource::generatePresets()
{
    Vector<VideoPresetData> presets;
    for (IntSize size : { IntSize(320, 240), IntSize(640, 480), IntSize(1280, 720) })
        presets.append({ size, { { 1, 30 } }, 1, 1, false });
    setSupportedPresets(WTF::move(presets));
}

const RealtimeMediaSourceCapabilities& CRTVideoCaptureSource::capabilities()
{
    if (!m_capabilities) {
        RealtimeMediaSourceCapabilities capabilities(settings().supportedConstraints());
        capabilities.setDeviceId(hashedId());
        updateCapabilities(capabilities);
        capabilities.addFacingMode(VideoFacingMode::Unknown);
        m_capabilities = WTF::move(capabilities);
    }
    return *m_capabilities;
}

const RealtimeMediaSourceSettings& CRTVideoCaptureSource::settings()
{
    if (!m_currentSettings) {
        RealtimeMediaSourceSettings settings;
        settings.setDeviceId(hashedId());
        settings.setGroupId(hashedGroupId());
        settings.setLabel(name());
        RealtimeMediaSourceSupportedConstraints supportedConstraints;
        supportedConstraints.setSupportsDeviceId(true);
        supportedConstraints.setSupportsGroupId(true);
        supportedConstraints.setSupportsWidth(true);
        supportedConstraints.setSupportsHeight(true);
        supportedConstraints.setSupportsAspectRatio(true);
        supportedConstraints.setSupportsFrameRate(true);
        settings.setSupportedConstraints(supportedConstraints);
        m_currentSettings = WTF::move(settings);
    }
    m_currentSettings->setWidth(size().width());
    m_currentSettings->setHeight(size().height());
    m_currentSettings->setFrameRate(frameRate());
    return *m_currentSettings;
}

void CRTVideoCaptureSource::settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag>)
{
    m_currentSettings = std::nullopt;
    m_capabilities = std::nullopt;
}

void CRTVideoCaptureSource::startProducingData()
{
    if (m_worker)
        return;
    IntSize requested = size();
    if (requested.isEmpty())
        requested = IntSize(640, 480);
    double rate = frameRate();
    m_worker = Worker::create(String(m_deviceID), m_synthetic, requested.width(), requested.height(), rate > 0 ? static_cast<uint32_t>(rate) : 30);
    m_worker->start(
        [weakThis = ThreadSafeWeakPtr { *this }](Ref<VideoFrameCRT>&& frame) {
            RefPtr source = weakThis.get();
            if (!source || !source->isProducingData() || source->muted())
                return;
            source->dispatchVideoFrameToObservers(frame.get(), { });
        },
        [weakThis = ThreadSafeWeakPtr { *this }](String&&) {
            if (RefPtr source = weakThis.get())
                source->captureFailed();
        });
}

void CRTVideoCaptureSource::stopProducingData()
{
    if (RefPtr worker = std::exchange(m_worker, nullptr))
        worker->stop();
}

// ---- factories -----------------------------------------------------------------------------------------

namespace {

class CRTVideoCaptureFactory final : public VideoCaptureFactory {
public:
    CaptureSourceOrError createVideoCaptureSource(const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints, std::optional<PageIdentifier>) final
    {
        return CRTVideoCaptureSource::create(device, WTF::move(hashSalts), constraints);
    }
    CaptureDeviceManager& videoCaptureDeviceManager() final { return CRTCaptureDeviceManager::singleton(); }
};

// Audio and display capture are not provided by the CRT port: no devices, and a request fails honestly.
class CRTEmptyDeviceManager final : public CaptureDeviceManager {
public:
    const Vector<CaptureDevice>& captureDevices() final { return m_none; }
private:
    Vector<CaptureDevice> m_none;
};

class CRTAudioCaptureFactory final : public AudioCaptureFactory {
public:
    CaptureSourceOrError createAudioCaptureSource(const CaptureDevice&, MediaDeviceHashSalts&&, const MediaConstraints*, std::optional<PageIdentifier>) final
    {
        return CaptureSourceOrError({ "audio capture is not supported by the CRT port"_s, MediaAccessDenialReason::HardwareError });
    }
    CaptureDeviceManager& audioCaptureDeviceManager() final
    {
        static NeverDestroyed<CRTEmptyDeviceManager> manager;
        return manager;
    }
    const Vector<CaptureDevice>& speakerDevices() const final
    {
        static NeverDestroyed<Vector<CaptureDevice>> none;
        return none;
    }
};

class CRTEmptyDisplayManager final : public DisplayCaptureManager {
public:
    const Vector<CaptureDevice>& captureDevices() final { return m_none; }
    std::optional<CaptureDevice> captureDeviceWithPersistentID(CaptureDevice::DeviceType, const String&) final { return std::nullopt; }
private:
    Vector<CaptureDevice> m_none;
};

class CRTDisplayCaptureFactory final : public DisplayCaptureFactory {
public:
    CaptureSourceOrError createDisplayCaptureSource(const CaptureDevice&, MediaDeviceHashSalts&&, const MediaConstraints*, std::optional<PageIdentifier>) final
    {
        return CaptureSourceOrError({ "display capture is not supported by the CRT port"_s, MediaAccessDenialReason::HardwareError });
    }
    DisplayCaptureManager& displayCaptureDeviceManager() final
    {
        static NeverDestroyed<CRTEmptyDisplayManager> manager;
        return manager;
    }
};

} // namespace

AudioCaptureFactory& RealtimeMediaSourceCenter::defaultAudioCaptureFactory()
{
    static NeverDestroyed<CRTAudioCaptureFactory> factory;
    return factory;
}

VideoCaptureFactory& RealtimeMediaSourceCenter::defaultVideoCaptureFactory()
{
    static NeverDestroyed<CRTVideoCaptureFactory> factory;
    return factory;
}

DisplayCaptureFactory& RealtimeMediaSourceCenter::defaultDisplayCaptureFactory()
{
    static NeverDestroyed<CRTDisplayCaptureFactory> factory;
    return factory;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
