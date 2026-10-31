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

// Web Tranche 8, gate 8B: the CRT playback engine. A WebCore MediaPlayerPrivate that demuxes, decodes
// and times media through the installed crtmedia public C API only (extractor, codec, frame, audio
// sink). FFmpeg and the host audio API stay private to libcrtmedia. CPU video/audio only; GPU decode and
// presentation belong to Tranche 9, and MSE/WebCodecs/WebAudio/DRM are not part of this engine.
//
// Threading: one worker thread per player owns every crtmedia handle (blocking demux, decode and audio
// write); the WebCore main thread owns the MediaPlayer notifications and painting. The two share one
// ThreadSafeRefCounted state object; the player detaches from it on the main thread before it dies, and
// the worker is joined by that state object's last reference.

#pragma once

#include "MediaPlayerPrivate.h"
#include "PlatformTimeRanges.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class MediaPlayerPrivateCRTShared;
class CRTStreamBridge;
class PlatformMediaResource;

class MediaPlayerPrivateCRT final
    : public MediaPlayerPrivateInterface
    , public CanMakeWeakPtr<MediaPlayerPrivateCRT>
    , public RefCounted<MediaPlayerPrivateCRT> {
    WTF_MAKE_TZONE_ALLOCATED(MediaPlayerPrivateCRT);
public:
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    explicit MediaPlayerPrivateCRT(MediaPlayer&);
    ~MediaPlayerPrivateCRT();

    static void registerMediaEngine(MediaEngineRegistrar);
    // Registers the engine with WebCore once per process (Web process start-up).
    WEBCORE_EXPORT static void ensureRegistered();

    // The player kind and engine slot: WebCore's enumerations have no neutral CRT value, so the engine
    // uses the one no CRT build can otherwise produce (see docs/porting/crtweb_porting.md).
    constexpr MediaPlayerType mediaPlayerType() const final { return MediaPlayerType::MediaFoundation; }

    void load(const String& url) final;
#if ENABLE(MEDIA_SOURCE)
    void load(const URL&, const LoadOptions&, MediaSourcePrivateClient&) final { }
#endif
#if ENABLE(MEDIA_STREAM)
    // Tranche 8C: a MediaStream (a camera track) is shown by observing its video frames.
    void load(MediaStreamPrivate&) final;
#endif
    void cancelLoad() final;

    void play() final;
    void pause() final;

    FloatSize naturalSize() const final { return m_naturalSize; }
    bool hasVideo() const final { return m_hasVideo; }
    bool hasAudio() const final { return m_hasAudio; }

    void setPageIsVisible(bool) final { }

    MediaTime duration() const final { return m_duration; }
    MediaTime currentTime() const final;
    bool timeIsProgressing() const final;
    Ref<MediaTimePromise> seekToTarget(const SeekTarget&) final;
    bool seeking() const { return m_seeking; }

    void setRate(float) final;
    double rate() const final { return m_rate; }

    bool paused() const final { return m_paused; }
    void setVolume(float) final;
    float volume() const { return m_volume; }
    void setMuted(bool) final;

    MediaPlayer::NetworkState networkState() const final { return m_networkState; }
    MediaPlayer::ReadyState readyState() const final { return m_readyState; }

    const PlatformTimeRanges& buffered() const final { return m_buffered; }
    MediaTime maxTimeSeekable() const final { return m_duration; }
    bool didLoadingProgress() const final;
    unsigned long long totalBytes() const final { return m_totalBytes; }

    void setPresentationSize(const IntSize&) final { }
    void paint(GraphicsContext&, const FloatRect&) final;
    RefPtr<NativeImage> nativeImageForCurrentTime() final;
    DestinationColorSpace colorSpace() final;
    bool hasAvailableVideoFrame() const final;


    // Called on the main thread by the worker's dispatch.
    void workerEvent();
    void streamEnded();

private:
    friend class MediaPlayerFactoryCRT;
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);
    static void getSupportedTypes(HashSet<String>&);

    void startWorker(const String& path);
    void loadRemote(const URL&);
    void setNetworkState(MediaPlayer::NetworkState);
    void setReadyState(MediaPlayer::ReadyState);
    void loadingFailed();

    ThreadSafeWeakPtr<MediaPlayer> m_player;
    RefPtr<MediaPlayerPrivateCRTShared> m_shared;
    RefPtr<PlatformMediaResource> m_resource;
    String m_temporaryPath;
    bool m_isStream { false };
#if ENABLE(MEDIA_STREAM)
    RefPtr<CRTStreamBridge> m_stream;
#endif
    int m_temporaryHandle { -1 };

    MediaPlayer::NetworkState m_networkState { MediaPlayer::NetworkState::Empty };
    MediaPlayer::ReadyState m_readyState { MediaPlayer::ReadyState::HaveNothing };
    PlatformTimeRanges m_buffered;
    MediaTime m_duration { MediaTime::invalidTime() };
    FloatSize m_naturalSize;
    unsigned long long m_totalBytes { 0 };
    bool m_hasVideo { false };
    bool m_hasAudio { false };
    bool m_paused { true };
    bool m_seeking { false };
    bool m_didLoadingProgress { false };
    float m_volume { 1 };
    bool m_muted { false };
    double m_rate { 1 };
    uint64_t m_lastFrameSerial { 0 };
    std::optional<MediaTimePromise::AutoRejectProducer> m_seekPromise;
};

} // namespace WebCore
