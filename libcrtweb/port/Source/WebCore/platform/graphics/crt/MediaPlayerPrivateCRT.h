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
    void load(MediaStreamPrivate&) final { }
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
