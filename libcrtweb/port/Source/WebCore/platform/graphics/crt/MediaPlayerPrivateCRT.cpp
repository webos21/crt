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
#include "MediaPlayerPrivateCRT.h"

#include "GraphicsContext.h"
#include "MediaPlayer.h"
#include "NativeImage.h"
#include "PlatformMediaResourceLoader.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include <wtf/Condition.h>
#include <wtf/FileHandle.h>
#include <wtf/FileSystem.h>
#include <wtf/Lock.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/Threading.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

#if ENABLE(MEDIA_STREAM)
#include "CRTVideoCaptureSource.h"
#include "MediaStreamPrivate.h"
#include "MediaStreamTrackPrivate.h"
#include "RealtimeMediaSource.h"
#endif

#include <crtmedia/audio.h>
#include <cstdio>
#include <cstdlib>
#include <crtmedia/audio_sink.h>
#include <crtmedia/codec.h>
#include <crtmedia/extractor.h>
#include <crtmedia/format.h>
#include <crtmedia/frame.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkImage.h>
#include <skia/core/SkImageInfo.h>
#include <skia/core/SkPixmap.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

#include <chrono>
#include <cmath>
#include <cstring>
#include <deque>
#include <vector>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaPlayerPrivateCRT);

static int64_t nowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// ---- state shared between the main thread and the worker ----------------------------------------

class MediaPlayerPrivateCRTShared final : public ThreadSafeRefCounted<MediaPlayerPrivateCRTShared> {
public:
    static Ref<MediaPlayerPrivateCRTShared> create() { return adoptRef(*new MediaPlayerPrivateCRTShared); }

    // Main thread only: the player that gets worker notifications; cleared before the player dies.
    MediaPlayerPrivateCRT* owner { nullptr };

    Lock lock;
    Condition condition;

    // Commands (main -> worker), guarded by `lock`.
    std::string path;
    bool quit { false };
    bool playing { false };
    bool seekRequested { false };
    int64_t seekUs { 0 };
    uint64_t seekGeneration { 0 };
    float volume { 1 };
    bool muted { false };

    // Status (worker -> main), guarded by `lock`.
    bool opened { false };
    bool failed { false };
    bool ended { false };
    bool hasVideo { false };
    bool hasAudio { false };
    int64_t durationUs { -1 };
    uint32_t width { 0 };
    uint32_t height { 0 };
    uint64_t completedSeekGeneration { 0 };
    int64_t completedSeekUs { 0 };
    bool prerolled { false };
    uint64_t frameSerial { 0 };
    uint32_t frameWidth { 0 };
    uint32_t frameHeight { 0 };
    std::vector<uint8_t> frame; // RGBA, tightly packed
    // The playback clock: media time `anchorUs` at wall time `anchorNs`; moves only while `clockRunning`.
    int64_t anchorUs { 0 };
    int64_t anchorNs { 0 };
    bool clockRunning { false };
    bool eventPending { false };

    RefPtr<Thread> thread;

    int64_t clockUsLocked() const
    {
        if (!clockRunning)
            return anchorUs;
        return anchorUs + (nowNs() - anchorNs) / 1000;
    }

    void post()
    {
        {
            Locker locker { lock };
            if (eventPending)
                return;
            eventPending = true;
        }
        RunLoop::mainSingleton().dispatch([shared = Ref { *this }] {
            if (shared->owner)
                shared->owner->workerEvent();
        });
    }

    void run();
    void stopAndJoin()
    {
        {
            Locker locker { lock };
            quit = true;
            condition.notifyAll();
        }
        if (RefPtr protectedThread = std::exchange(thread, nullptr))
            protectedThread->waitForCompletion();
    }

private:
    MediaPlayerPrivateCRTShared() = default;
};

// ---- the worker ----------------------------------------------------------------------------------

namespace {

struct Track {
    bool present { false };
    uint32_t index { 0 };
    crtmedia_codec* codec { nullptr };
    bool inputEnded { false };
    bool outputEnded { false };
};

bool findTrack(crtmedia_extractor* extractor, const char* prefix, uint32_t& index, crtmedia_format*& format)
{
    uint32_t count = crtmedia_extractor_track_count(extractor);
    size_t length = strlen(prefix);
    for (uint32_t i = 0; i < count; ++i) {
        crtmedia_format* candidate = nullptr;
        if (crtmedia_extractor_track_format(extractor, i, &candidate) != CRTMEDIA_OK)
            continue;
        const char* mime = nullptr;
        if (crtmedia_format_get_string(candidate, CRTMEDIA_FORMAT_KEY_MIME, &mime) == CRTMEDIA_OK && !strncmp(mime, prefix, length)) {
            index = i;
            format = candidate;
            return true;
        }
        crtmedia_format_release(candidate);
    }
    return false;
}

void applyGain(crtmedia_audio_buffer& buffer, float gain)
{
    if (gain >= 0.999f)
        return;
    size_t count = static_cast<size_t>(buffer.frame_count) * buffer.channels;
    if (buffer.format == CRTMEDIA_SAMPLE_FORMAT_S16) {
        auto* samples = static_cast<int16_t*>(buffer.data);
        for (size_t i = 0; i < count; ++i)
            samples[i] = static_cast<int16_t>(samples[i] * gain);
    } else if (buffer.format == CRTMEDIA_SAMPLE_FORMAT_FLT) {
        auto* samples = static_cast<float*>(buffer.data);
        for (size_t i = 0; i < count; ++i)
            samples[i] *= gain;
    }
}

} // namespace

void MediaPlayerPrivateCRTShared::run()
{
    const bool traceAudio = getenv("CRTWEB_MEDIA_AUDIO_TRACE") != nullptr;
    if (traceAudio)
        fprintf(stderr, "CRTWEB_AUDIO worker_start\n");
    std::string localPath;
    {
        Locker locker { lock };
        localPath = path;
    }

    auto fail = [this] {
        {
            Locker locker { lock };
            failed = true;
        }
        post();
    };

    crtmedia_extractor* extractor = nullptr;
    if (crtmedia_extractor_create(localPath.c_str(), &extractor) != CRTMEDIA_OK)
        return fail();

    Track video, audio;
    crtmedia_format* videoFormat = nullptr;
    crtmedia_format* audioFormat = nullptr;
    video.present = findTrack(extractor, "video/", video.index, videoFormat);
    audio.present = findTrack(extractor, "audio/", audio.index, audioFormat);
    int32_t width = 0, height = 0;
    int64_t durationUs = 0;
    if (video.present) {
        crtmedia_format_get_int32(videoFormat, CRTMEDIA_FORMAT_KEY_WIDTH, &width);
        crtmedia_format_get_int32(videoFormat, CRTMEDIA_FORMAT_KEY_HEIGHT, &height);
        int64_t value = 0;
        if (crtmedia_format_get_int64(videoFormat, CRTMEDIA_FORMAT_KEY_DURATION_US, &value) == CRTMEDIA_OK)
            durationUs = std::max(durationUs, value);
        if (width <= 0 || height <= 0) {
            video.present = false;
        } else {
            // CPU decode only (GPU decode and presentation are Tranche 9); keep the decoder in software.
            crtmedia_format_set_int32(videoFormat, CRTMEDIA_FORMAT_KEY_PREFER_HARDWARE_DECODE, 0);
            crtmedia_extractor_select_track(extractor, video.index);
            if (crtmedia_codec_create_decoder(videoFormat, &video.codec) != CRTMEDIA_OK)
                video.present = false;
        }
    }
    if (audio.present) {
        int64_t value = 0;
        if (crtmedia_format_get_int64(audioFormat, CRTMEDIA_FORMAT_KEY_DURATION_US, &value) == CRTMEDIA_OK)
            durationUs = std::max(durationUs, value);
        crtmedia_extractor_select_track(extractor, audio.index);
        if (crtmedia_codec_create_decoder(audioFormat, &audio.codec) != CRTMEDIA_OK)
            audio.present = false;
    }
    if (videoFormat)
        crtmedia_format_release(videoFormat);
    if (audioFormat)
        crtmedia_format_release(audioFormat);

    if (!video.present && !audio.present) {
        if (video.codec)
            crtmedia_codec_release(video.codec);
        if (audio.codec)
            crtmedia_codec_release(audio.codec);
        crtmedia_extractor_release(extractor);
        return fail();
    }

    {
        Locker locker { lock };
        opened = true;
        hasVideo = video.present;
        hasAudio = audio.present;
        this->durationUs = durationUs;
        this->width = static_cast<uint32_t>(width);
        this->height = static_cast<uint32_t>(height);
    }
    post();

    // Worker-local state.
    crtmedia_audio_sink* sink = nullptr;
    bool sinkUnavailable = false;
    uint64_t sinkBaseFrames = 0;     // sink position when the clock was last re-based
    int64_t sinkBaseUs = 0;          // media time at that position
    uint32_t sinkRate = 0;
    unsigned audioTraceCount = 0;
    uint64_t sinkFramesWritten = 0;   // diagnostic: CRTWEB_MEDIA_AUDIO_TRACE reports it with the final position
    uint32_t sinkChannels = 0;
    uint64_t nonzeroSamplesWritten = 0;
    uint64_t clockUpdates = 0, clockAdvances = 0, lastSinkPosition = 0;
    unsigned writeFailures = 0;
    bool havePending = false;        // a decoded video frame waiting for its time
    std::deque<crtmedia_audio_buffer> pendingAudio; // paused preroll, including interleaved seek audio
    uint64_t pendingAudioBytes = 0;
    bool decodeFailed = false;
    bool havePendingInput = false;   // retry the same sample after decoder backpressure
    crtmedia_sample pendingInput { };
    crtmedia_frame pending;
    memset(&pending, 0, sizeof pending);
    bool extractorEof = false;
    bool wasPlaying = false;
    bool prerolling = true;          // until the first picture (or audio) is available
    int64_t discardBeforeUs = 0;     // after a seek, decoded media before this is dropped
    uint64_t handledSeek = 0;
    crtmedia_frame rgba;
    memset(&rgba, 0, sizeof rgba);
    std::vector<uint8_t> rgbaStorage;
    bool ended = false;

    auto presentPending = [&]() {
        if (!havePending)
            return;
        uint32_t w = pending.width, h = pending.height;
        rgbaStorage.assign(static_cast<size_t>(w) * h * 4, 0);
        memset(&rgba, 0, sizeof rgba);
        rgba.format = CRTMEDIA_PIXEL_FORMAT_RGBA8888;
        rgba.width = w;
        rgba.height = h;
        rgba.plane_count = 1;
        rgba.planes[0].stride = w * 4;
        rgba.planes[0].width = w;
        rgba.planes[0].height = h;
        rgba.planes[0].data = rgbaStorage.data();
        if (crtmedia_frame_convert_to_rgba(&pending, &rgba) == CRTMEDIA_OK) {
            Locker locker { lock };
            frame.swap(rgbaStorage);
            frameWidth = w;
            frameHeight = h;
            ++frameSerial;
            prerolled = true;
        }
        crtmedia_frame_release(&pending);
        memset(&pending, 0, sizeof pending);
        havePending = false;
        post();
    };

    auto rebaseClock = [&](int64_t mediaUs, bool running) {
        Locker locker { lock };
        anchorUs = mediaUs;
        anchorNs = nowNs();
        clockRunning = running;
        sinkBaseUs = mediaUs;
    };

    for (;;) {
        bool playingNow;
        bool doSeek = false;
        int64_t seekTarget = 0;
        uint64_t seekId = 0;
        float gain;
        {
            Locker locker { lock };
            if (quit)
                break;
            playingNow = playing && !ended;
            if (seekRequested) {
                doSeek = true;
                seekTarget = seekUs;
                seekId = seekGeneration;
                seekRequested = false;
            }
            gain = muted ? 0.f : volume;
        }

        if (doSeek) {
            if (havePendingInput) {
                crtmedia_sample_release(&pendingInput);
                havePendingInput = false;
            }
            for (auto& buffer : pendingAudio)
                crtmedia_audio_buffer_release(&buffer);
            pendingAudio.clear();
            pendingAudioBytes = 0;
            crtmedia_extractor_seek_to(extractor, seekTarget);
            if (video.present)
                crtmedia_codec_flush(video.codec);
            if (audio.present)
                crtmedia_codec_flush(audio.codec);
            video.inputEnded = video.outputEnded = audio.inputEnded = audio.outputEnded = false;
            extractorEof = false;
            ended = false;
            if (havePending) {
                crtmedia_frame_release(&pending);
                memset(&pending, 0, sizeof pending);
                havePending = false;
            }
            discardBeforeUs = seekTarget;
            prerolling = true;
            if (sink) {
                uint64_t frames = 0;
                if (crtmedia_audio_sink_get_position_frames(sink, &frames) == CRTMEDIA_OK)
                    sinkBaseFrames = frames;
            }
            rebaseClock(seekTarget, false);
            handledSeek = seekId;
            wasPlaying = false;
        }

        if (playingNow && !wasPlaying) {
            // Resume: the clock runs from the current media time.
            int64_t current;
            {
                Locker locker { lock };
                current = anchorUs;
            }
            if (sink) {
                uint64_t frames = 0;
                if (crtmedia_audio_sink_get_position_frames(sink, &frames) == CRTMEDIA_OK)
                    sinkBaseFrames = frames;
            }
            rebaseClock(current, true);
            wasPlaying = true;
        } else if (!playingNow && wasPlaying) {
            Locker locker { lock };
            anchorUs = clockUsLocked();
            anchorNs = nowNs();
            clockRunning = false;
            wasPlaying = false;
        }

        bool needData = prerolling || playingNow;
        bool progressed = false;

        // Demux into the decoders.
        if (needData && !extractorEof && !(havePending && video.present)) {
            for (int i = 0; i < 8; ++i) {
                crtmedia_sample sample;
                int eof = 0;
                crtmedia_result rc = CRTMEDIA_OK;
                if (havePendingInput) {
                    sample = pendingInput;
                    memset(&pendingInput, 0, sizeof pendingInput);
                    havePendingInput = false;
                } else
                    rc = crtmedia_extractor_read_sample(extractor, &sample, &eof);
                if (rc != CRTMEDIA_OK) {
                    extractorEof = true;
                    break;
                }
                if (eof) {
                    extractorEof = true;
                    if (video.present && !video.inputEnded) {
                        crtmedia_codec_queue_input(video.codec, nullptr, 0, 0, CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM);
                        video.inputEnded = true;
                    }
                    if (audio.present && !audio.inputEnded) {
                        crtmedia_codec_queue_input(audio.codec, nullptr, 0, 0, CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM);
                        audio.inputEnded = true;
                    }
                    break;
                }
                crtmedia_result queued = CRTMEDIA_OK;
                if (video.present && sample.track_index == video.index)
                    queued = crtmedia_codec_queue_input(video.codec, sample.data, sample.size, sample.pts_us, 0);
                else if (audio.present && sample.track_index == audio.index)
                    queued = crtmedia_codec_queue_input(audio.codec, sample.data, sample.size, sample.pts_us, 0);
                progressed = true;
                if (queued == CRTMEDIA_WOULD_BLOCK) {
                    pendingInput = sample;
                    havePendingInput = true;
                    break;
                }
                crtmedia_sample_release(&sample);
            }
        }

        // Audio: retain the first preroll buffer until play; only seek-stale
        // samples or a missing output device may discard decoded audio.
        if (audio.present && (!audio.outputEnded || !pendingAudio.empty())) {
            for (;;) {
                if (!playingNow && (!prerolling || (!video.present && !pendingAudio.empty())))
                    break;
                crtmedia_audio_buffer buffer;
                int eof = 0;
                memset(&buffer, 0, sizeof buffer);
                crtmedia_result rc = CRTMEDIA_OK;
                if (playingNow && !pendingAudio.empty()) {
                    buffer = pendingAudio.front();
                    pendingAudio.pop_front();
                    pendingAudioBytes -= static_cast<uint64_t>(buffer.frame_count) * buffer.channels
                        * (buffer.format == CRTMEDIA_SAMPLE_FORMAT_S16 ? 2 : 4);
                } else if (audio.outputEnded)
                    break;
                else
                    rc = crtmedia_codec_dequeue_output(audio.codec, nullptr, &buffer, &eof);
                if (rc != CRTMEDIA_OK)
                    break;
                if (eof) {
                    audio.outputEnded = true;
                    break;
                }
                progressed = true;
                bool stale = buffer.timestamp_us != CRTMEDIA_AUDIO_TIMESTAMP_NONE && buffer.timestamp_us + 20000 < discardBeforeUs;
                if (!playingNow && !stale) {
                    // While finding the seek's video frame, interleaved audio
                    // must be drained too or its full decoder queue blocks demux.
                    uint64_t bytes = static_cast<uint64_t>(buffer.frame_count) * buffer.channels
                        * (buffer.format == CRTMEDIA_SAMPLE_FORMAT_S16 ? 2 : 4);
                    if (pendingAudio.size() >= 256 || bytes > 4 * 1024 * 1024 - pendingAudioBytes) {
                        crtmedia_audio_buffer_release(&buffer);
                        { Locker locker { lock }; failed = true; }
                        post();
                        decodeFailed = true;
                        break;
                    }
                    pendingAudio.push_back(buffer);
                    pendingAudioBytes += bytes;
                    continue;
                }
                if (traceAudio && ++audioTraceCount <= 4)
                    fprintf(stderr, "CRTWEB_AUDIO buffer frames=%u ts_us=%lld playing=%d stale=%d sink=%d unavailable=%d\n",
                        static_cast<unsigned>(buffer.frame_count), static_cast<long long>(buffer.timestamp_us),
                        playingNow ? 1 : 0, stale ? 1 : 0, sink ? 1 : 0, sinkUnavailable ? 1 : 0);
                if (playingNow && !stale) {
                    if (!sink && !sinkUnavailable) {
                        crtmedia_audio_sink_desc desc;
                        desc.format = buffer.format;
                        desc.sample_rate = buffer.sample_rate;
                        desc.channels = buffer.channels;
                        if (crtmedia_audio_sink_open(&desc, &sink) != CRTMEDIA_OK) {
                            sink = nullptr;
                            sinkUnavailable = true; // no output device: the wall clock paces playback
                            if (traceAudio)
                                fprintf(stderr, "CRTWEB_AUDIO sink_open_failed rate=%u channels=%u (wall clock paces playback)\n",
                                    static_cast<unsigned>(desc.sample_rate), static_cast<unsigned>(desc.channels));
                        } else {
                            sinkRate = buffer.sample_rate;
                            sinkChannels = buffer.channels;
                            if (traceAudio)
                                fprintf(stderr, "CRTWEB_AUDIO sink_opened rate=%u channels=%u\n",
                                    static_cast<unsigned>(buffer.sample_rate), static_cast<unsigned>(buffer.channels));
                            uint64_t frames = 0;
                            if (crtmedia_audio_sink_get_position_frames(sink, &frames) == CRTMEDIA_OK)
                                sinkBaseFrames = frames;
                            int64_t current;
                            {
                                Locker locker { lock };
                                current = clockUsLocked();
                            }
                            rebaseClock(current, true);
                        }
                    }
                    if (sink) {
                        applyGain(buffer, gain);
                        // The sink contract permits short writes; count only accepted samples.
                        unsigned zeroWrites = 0;
                        uint32_t offset = 0;
                        size_t sampleBytes = buffer.format == CRTMEDIA_SAMPLE_FORMAT_S16 ? sizeof(int16_t) : sizeof(float);
                        while (offset < buffer.frame_count) {
                            auto* data = static_cast<const uint8_t*>(buffer.data) + offset * buffer.channels * sampleBytes;
                            int64_t written = crtmedia_audio_sink_write(sink, data, buffer.frame_count - offset);
                            if (written < 0 || written > buffer.frame_count - offset) {
                                ++writeFailures;
                                break;
                            }
                            if (!written) {
                                Locker locker { lock };
                                if (quit || ++zeroWrites >= 100) {
                                    if (!quit)
                                        ++writeFailures;
                                    break;
                                }
                                condition.waitFor(lock, Seconds(0.001));
                                continue;
                            }
                            zeroWrites = 0;
                            sinkFramesWritten += written;
                            if (traceAudio) {
                                size_t count = static_cast<size_t>(written) * buffer.channels;
                                for (size_t i = 0; i < count; ++i) {
                                    if (buffer.format == CRTMEDIA_SAMPLE_FORMAT_S16)
                                        nonzeroSamplesWritten += reinterpret_cast<const int16_t*>(data)[i] != 0;
                                    else
                                        nonzeroSamplesWritten += reinterpret_cast<const float*>(data)[i] != 0;
                                }
                            }
                            offset += static_cast<uint32_t>(written);
                        }
                        uint64_t frames = 0;
                        if (sinkRate && crtmedia_audio_sink_get_position_frames(sink, &frames) == CRTMEDIA_OK && frames >= sinkBaseFrames) {
                            int64_t mediaUs = sinkBaseUs + static_cast<int64_t>((frames - sinkBaseFrames) * 1000000ull / sinkRate);
                            Locker locker { lock };
                            anchorUs = mediaUs;
                            anchorNs = nowNs();
                            ++clockUpdates;
                            clockAdvances += frames > lastSinkPosition;
                            lastSinkPosition = frames;
                        }
                    }
                }
                crtmedia_audio_buffer_release(&buffer);
                if (!playingNow)
                    break;
            }
        }
        if (decodeFailed)
            break;

        // Video: hold one decoded frame until its time.
        if (video.present && !video.outputEnded) {
            while (!havePending) {
                crtmedia_frame decoded;
                int eof = 0;
                memset(&decoded, 0, sizeof decoded);
                crtmedia_result rc = crtmedia_codec_dequeue_output(video.codec, &decoded, nullptr, &eof);
                if (rc != CRTMEDIA_OK)
                    break;
                if (eof) {
                    video.outputEnded = true;
                    break;
                }
                progressed = true;
                if (decoded.timestamp_us + 1000 < discardBeforeUs && !(extractorEof && video.inputEnded)) {
                    crtmedia_frame_release(&decoded);
                    continue;
                }
                pending = decoded;
                havePending = true;
            }
        }
        if (havePending) {
            int64_t clock;
            {
                Locker locker { lock };
                clock = clockUsLocked();
            }
            bool due = prerolling || clock + 2000 >= pending.timestamp_us;
            if (due) {
                presentPending();
                progressed = true;
                if (prerolling) {
                    prerolling = false;
                    discardBeforeUs = 0;
                }
            }
        } else if (prerolling && !video.present && audio.present) {
            // Audio-only media: ready once the decoder produced anything or ended.
            bool anyAudio = !pendingAudio.empty() || audio.outputEnded;
            if (anyAudio) {
                {
                    Locker locker { lock };
                    prerolled = true;
                }
                prerolling = false;
                discardBeforeUs = 0;
                post();
            }
        }

        // Seek completion is reported once the first picture after it is out.
        {
            bool report = false;
            int64_t reportUs = 0;
            {
                Locker locker { lock };
                if (handledSeek && handledSeek != completedSeekGeneration && !prerolling) {
                    completedSeekGeneration = handledSeek;
                    completedSeekUs = anchorUs;
                    report = true;
                    reportUs = anchorUs;
                }
            }
            if (report) {
                (void)reportUs;
                post();
            }
        }

        // The end: every decoder drained.
        if (!ended && extractorEof && (!video.present || video.outputEnded) && (!audio.present || audio.outputEnded)
            && !havePending && pendingAudio.empty()) {
            ended = true;
            {
                Locker locker { lock };
                anchorUs = durationUs > 0 ? durationUs : clockUsLocked();
                clockRunning = false;
                this->ended = true;
            }
            post();
            wasPlaying = false;
        }

        if (!progressed) {
            Locker locker { lock };
            if (quit)
                break;
            // Wait for a command, or for the next frame's time.
            int64_t waitUs = 4000;
            if (havePending && clockRunning)
                waitUs = std::clamp<int64_t>(pending.timestamp_us - clockUsLocked(), 500, 8000);
            condition.waitFor(lock, Seconds(waitUs / 1e6));
        }
    }

    if (havePending)
        crtmedia_frame_release(&pending);
    for (auto& buffer : pendingAudio)
        crtmedia_audio_buffer_release(&buffer);
    if (havePendingInput)
        crtmedia_sample_release(&pendingInput);
    if (sink) {
        if (traceAudio) {
            uint64_t position = 0;
            crtmedia_result positionResult = crtmedia_audio_sink_get_position_frames(sink, &position);
            fprintf(stderr, "CRTWEB_AUDIO sink_closed sink_id=%llu rate=%u channels=%u written_frames=%llu nonzero_samples=%llu position_frames=%llu position_result=%d clock_updates=%llu clock_advances=%llu write_failures=%u\n",
                static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(sink)), static_cast<unsigned>(sinkRate), static_cast<unsigned>(sinkChannels),
                static_cast<unsigned long long>(sinkFramesWritten), static_cast<unsigned long long>(nonzeroSamplesWritten),
                static_cast<unsigned long long>(position), static_cast<int>(positionResult),
                static_cast<unsigned long long>(clockUpdates), static_cast<unsigned long long>(clockAdvances), writeFailures);
        }
        crtmedia_audio_sink_close(sink);
    }
    if (video.codec)
        crtmedia_codec_release(video.codec);
    if (audio.codec)
        crtmedia_codec_release(audio.codec);
    crtmedia_extractor_release(extractor);
}

#if ENABLE(MEDIA_STREAM)
// Shows a MediaStream's camera track: its VideoFrameCRT frames are copied into the player's shared frame
// (observer callbacks run on the capture thread; the player is notified on the main thread).
class CRTStreamBridge final : public RefCounted<CRTStreamBridge>, public MediaStreamTrackPrivateObserver, public RealtimeMediaSource::VideoFrameObserver {
public:
    static Ref<CRTStreamBridge> create(MediaStreamTrackPrivate& track, Ref<MediaPlayerPrivateCRTShared>&& shared) { return adoptRef(*new CRTStreamBridge(track, WTF::move(shared))); }
    ~CRTStreamBridge() { detach(); }

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    void detach()
    {
        RefPtr protectedTrack = std::exchange(m_track, nullptr);
        if (!protectedTrack)
            return;
        protectedTrack->removeObserver(*this);
        protectedTrack->source().removeVideoFrameObserver(*this);
    }

private:
    CRTStreamBridge(MediaStreamTrackPrivate& track, Ref<MediaPlayerPrivateCRTShared>&& shared)
        : m_track(&track)
        , m_shared(WTF::move(shared))
    {
        track.addObserver(*this);
        track.source().addVideoFrameObserver(*this);
    }

    // May run on the capture thread.
    void videoFrameAvailable(VideoFrame& frame, VideoFrameTimeMetadata) final
    {
        if (!VideoFrameCRT::isInstance(frame))
            return;
        auto& crtFrame = static_cast<VideoFrameCRT&>(frame);
        IntSize size = crtFrame.presentationSize();
        auto pixels = crtFrame.pixels();
        {
            Locker locker { m_shared->lock };
            m_shared->frame.assign(pixels.begin(), pixels.end());
            m_shared->frameWidth = size.width();
            m_shared->frameHeight = size.height();
            m_shared->width = size.width();
            m_shared->height = size.height();
            m_shared->hasVideo = true;
            m_shared->opened = true;
            m_shared->prerolled = true;
            ++m_shared->frameSerial;
        }
        m_shared->post();
    }

    void trackEnded(MediaStreamTrackPrivate&) final { clearAndNotify(); }
    void trackMutedChanged(MediaStreamTrackPrivate&) final { }
    void trackSettingsChanged(MediaStreamTrackPrivate&) final { }
    void trackEnabledChanged(MediaStreamTrackPrivate&) final { }

    void clearAndNotify()
    {
        {
            Locker locker { m_shared->lock };
            m_shared->frame.clear();
            m_shared->frameWidth = 0;
            m_shared->frameHeight = 0;
            ++m_shared->frameSerial;
            m_shared->ended = true;
            m_shared->anchorUs = m_shared->clockUsLocked();
            m_shared->clockRunning = false;
        }
        m_shared->post();
    }

    RefPtr<MediaStreamTrackPrivate> m_track;
    Ref<MediaPlayerPrivateCRTShared> m_shared;
};

void MediaPlayerPrivateCRT::load(MediaStreamPrivate& stream)
{
    cancelLoad();
    m_hasVideo = m_hasAudio = false;
    m_duration = MediaTime::invalidTime();
    m_naturalSize = { };
    m_buffered = { };
    m_lastFrameSerial = 0;
    m_isStream = true;
    RefPtr<MediaStreamTrackPrivate> videoTrack;
    for (auto& track : stream.tracks()) {
        if (track->isVideo() && !track->ended()) {
            videoTrack = track.ptr();
            break;
        }
    }
    if (!videoTrack) {
        setNetworkState(MediaPlayer::NetworkState::FormatError);
        return;
    }
    m_shared = MediaPlayerPrivateCRTShared::create();
    m_shared->owner = this;
    m_stream = CRTStreamBridge::create(*videoTrack, *m_shared);
    setNetworkState(MediaPlayer::NetworkState::Loading);
}

void MediaPlayerPrivateCRT::streamEnded()
{
    if (auto player = m_player.get())
        player->timeChanged();
}
#else
void MediaPlayerPrivateCRT::streamEnded() { }
#endif // ENABLE(MEDIA_STREAM)

// ---- the player ------------------------------------------------------------------------------------

MediaPlayerPrivateCRT::MediaPlayerPrivateCRT(MediaPlayer& player)
    : m_player(player)
{
}

MediaPlayerPrivateCRT::~MediaPlayerPrivateCRT()
{
    cancelLoad();
}

void MediaPlayerPrivateCRT::setNetworkState(MediaPlayer::NetworkState state)
{
    if (m_networkState == state)
        return;
    m_networkState = state;
    if (auto player = m_player.get())
        player->networkStateChanged();
}

void MediaPlayerPrivateCRT::setReadyState(MediaPlayer::ReadyState state)
{
    if (m_readyState == state)
        return;
    m_readyState = state;
    if (auto player = m_player.get())
        player->readyStateChanged();
}

void MediaPlayerPrivateCRT::loadingFailed()
{
    cancelLoad();
    setNetworkState(MediaPlayer::NetworkState::FormatError);
    setReadyState(MediaPlayer::ReadyState::HaveNothing);
}

void MediaPlayerPrivateCRT::cancelLoad()
{
    if (m_resource) {
        m_resource->shutdown();
        m_resource->setClient(nullptr);
        m_resource = nullptr;
    }
#if ENABLE(MEDIA_STREAM)
    if (RefPtr stream = std::exchange(m_stream, nullptr))
        stream->detach();
#endif
    m_isStream = false;
    if (m_shared) {
        m_shared->owner = nullptr;
        m_shared->stopAndJoin();
        m_shared = nullptr;
    }
    if (!m_temporaryPath.isEmpty()) {
        FileSystem::deleteFile(m_temporaryPath);
        m_temporaryPath = String();
    }
    m_seekPromise = std::nullopt;
    m_seeking = false;
}

namespace {

class RemoteClient final : public PlatformMediaResourceClient {
public:
    static Ref<RemoteClient> create(MediaPlayerPrivateCRT& player, FileSystem::FileHandle&& file) { return adoptRef(*new RemoteClient(player, WTF::move(file))); }
    void responseReceived(PlatformMediaResource&, const ResourceResponse& response, CompletionHandler<void(ShouldContinuePolicyCheck)>&& completion) final
    {
        m_ok = response.isSuccessful() || response.httpStatusCode() == 0;
        completion(m_ok ? ShouldContinuePolicyCheck::Yes : ShouldContinuePolicyCheck::No);
    }
    void dataReceived(PlatformMediaResource&, const SharedBuffer& buffer) final
    {
        if (!m_ok || !m_file)
            return;
        buffer.forEachSegment([this](std::span<const uint8_t> segment) { m_file.write(segment); });
        m_bytes += buffer.size();
    }
    void loadFailed(PlatformMediaResource&, const ResourceError&) final { m_failed = true; notify(); }
    void loadFinished(PlatformMediaResource&, const NetworkLoadMetrics&) final { m_finished = true; notify(); }
    std::function<void(bool ok, uint64_t bytes)> onDone;

private:
    RemoteClient(MediaPlayerPrivateCRT&, FileSystem::FileHandle&& file)
        : m_file(WTF::move(file))
    {
    }
    void notify()
    {
        m_file = { };
        if (onDone)
            onDone(m_ok && !m_failed, m_bytes);
    }
    FileSystem::FileHandle m_file;
    bool m_ok { true };
    bool m_failed { false };
    bool m_finished { false };
    uint64_t m_bytes { 0 };
};

} // namespace

void MediaPlayerPrivateCRT::startWorker(const String& path)
{
    m_shared = MediaPlayerPrivateCRTShared::create();
    m_shared->owner = this;
    m_shared->path = std::string(path.utf8().data());
    m_shared->volume = m_volume;
    m_shared->muted = m_muted;
    Ref shared = *m_shared;
    m_shared->thread = Thread::create("CRTMediaPlayer"_s, [shared = WTF::move(shared)] {
        shared->run();
    });
    setNetworkState(MediaPlayer::NetworkState::Loading);
    setReadyState(MediaPlayer::ReadyState::HaveNothing);
}

void MediaPlayerPrivateCRT::load(const String& urlString)
{
    cancelLoad();
    m_hasVideo = m_hasAudio = false;
    m_duration = MediaTime::invalidTime();
    m_naturalSize = { };
    m_buffered = { };
    m_lastFrameSerial = 0;
    m_didLoadingProgress = false;

    URL url { urlString };
    if (url.protocolIsFile()) {
        startWorker(url.fileSystemPath());
        return;
    }
    if (!url.protocolIsInHTTPFamily()) {
        loadingFailed();
        return;
    }
    loadRemote(url);
}

// The browser's own loader fetches the resource (so TLS policy, cookies, cache and redirects are the
// browser's, not a second HTTP stack); the engine spools it to a temporary file and plays that. The
// resource is therefore played once complete: seeking and progressive start inside an unfinished
// transfer are not supported in this gate.
void MediaPlayerPrivateCRT::loadRemote(const URL& url)
{
    auto player = m_player.get();
    if (!player)
        return;
    auto [path, handle] = FileSystem::openTemporaryFile("crtmedia"_s, ".media"_s);
    if (!handle) {
        loadingFailed();
        return;
    }
    m_temporaryPath = path;
    auto client = RemoteClient::create(*this, WTF::move(handle));
    client->onDone = [weakThis = WeakPtr { *this }](bool ok, uint64_t bytes) {
        RefPtr self = weakThis.get();
        if (!self)
            return;
        if (!ok || !bytes) {
            self->loadingFailed();
            return;
        }
        self->m_totalBytes = bytes;
        self->m_didLoadingProgress = true;
        String temporary = self->m_temporaryPath;
        self->m_resource = nullptr;
        self->startWorker(temporary);
    };
    Ref loader = player->mediaResourceLoader();
    m_resource = loader->requestResource(ResourceRequest(URL { url }), PlatformMediaResourceLoader::LoadOption::BufferData);
    if (!m_resource) {
        loadingFailed();
        return;
    }
    m_resource->setClient(WTF::move(client));
    setNetworkState(MediaPlayer::NetworkState::Loading);
}

void MediaPlayerPrivateCRT::play()
{
    m_paused = false;
    if (!m_shared)
        return;
    Locker locker { m_shared->lock };
    m_shared->playing = true;
    // File players start/freeze their clock in the decode worker. MediaStream has no such worker:
    // its camera callback supplies frames only. Own its play/pause wall-clock anchors here, as the
    // upstream MediaStream player does; otherwise live camera currentTime stays at zero forever.
    if (m_isStream && !m_shared->ended && !m_shared->clockRunning) {
        m_shared->anchorNs = nowNs();
        m_shared->clockRunning = true;
    }
    if (m_shared->ended && !m_isStream) {
        // Replaying from the end starts over.
        m_shared->ended = false;
        m_shared->seekRequested = true;
        m_shared->seekUs = 0;
        m_shared->seekGeneration++;
    }
    m_shared->condition.notifyAll();
}

void MediaPlayerPrivateCRT::pause()
{
    m_paused = true;
    if (!m_shared)
        return;
    Locker locker { m_shared->lock };
    if (m_isStream) {
        m_shared->anchorUs = m_shared->clockUsLocked();
        m_shared->clockRunning = false;
    }
    m_shared->playing = false;
    m_shared->condition.notifyAll();
}

void MediaPlayerPrivateCRT::setRate(float rate)
{
    // Only 1x is implemented; other rates are not claimed (rate() reports what is applied).
    m_rate = rate > 0 ? 1 : 0;
}

void MediaPlayerPrivateCRT::setVolume(float volume)
{
    m_volume = volume;
    if (!m_shared)
        return;
    Locker locker { m_shared->lock };
    m_shared->volume = volume;
}

void MediaPlayerPrivateCRT::setMuted(bool muted)
{
    m_muted = muted;
    if (!m_shared)
        return;
    Locker locker { m_shared->lock };
    m_shared->muted = muted;
}

MediaTime MediaPlayerPrivateCRT::currentTime() const
{
    if (!m_shared)
        return MediaTime::zeroTime();
    Locker locker { m_shared->lock };
    int64_t us = m_shared->clockUsLocked();
    if (m_shared->durationUs > 0 && us > m_shared->durationUs)
        us = m_shared->durationUs;
    return MediaTime(us, 1000000);
}

bool MediaPlayerPrivateCRT::timeIsProgressing() const
{
    if (!m_shared)
        return false;
    Locker locker { m_shared->lock };
    return m_shared->clockRunning;
}

Ref<MediaTimePromise> MediaPlayerPrivateCRT::seekToTarget(const SeekTarget& target)
{
    if (!m_shared)
        return MediaTimePromise::createAndReject(PlatformMediaError::Cancelled);
    if (m_seekPromise)
        m_seekPromise->reject(PlatformMediaError::Cancelled);
    m_seekPromise.emplace(PlatformMediaError::Cancelled);
    Ref promise = m_seekPromise->promise();
    m_seeking = true;
    int64_t us = std::max<int64_t>(0, target.time.toMicroseconds());
    {
        Locker locker { m_shared->lock };
        m_shared->seekRequested = true;
        m_shared->seekUs = us;
        m_shared->seekGeneration++;
        m_shared->ended = false;
        m_shared->condition.notifyAll();
    }
    return promise;
}

bool MediaPlayerPrivateCRT::didLoadingProgress() const
{
    return std::exchange(const_cast<MediaPlayerPrivateCRT*>(this)->m_didLoadingProgress, false);
}

void MediaPlayerPrivateCRT::workerEvent()
{
    if (!m_shared)
        return;
    bool opened, failed, ended, prerolled, hasVideo, hasAudio;
    int64_t durationUs;
    uint32_t width, height;
    uint64_t serial, seekDone;
    int64_t seekUs;
    {
        Locker locker { m_shared->lock };
        m_shared->eventPending = false;
        opened = m_shared->opened;
        failed = m_shared->failed;
        ended = m_shared->ended;
        prerolled = m_shared->prerolled;
        hasVideo = m_shared->hasVideo;
        hasAudio = m_shared->hasAudio;
        durationUs = m_shared->durationUs;
        width = m_shared->width;
        height = m_shared->height;
        serial = m_shared->frameSerial;
        seekDone = m_shared->completedSeekGeneration;
        seekUs = m_shared->completedSeekUs;
    }
    auto player = m_player.get();
    if (!player)
        return;
    if (failed) {
        loadingFailed();
        return;
    }
    if (opened && m_duration.isInvalid()) {
        m_hasVideo = hasVideo;
        m_hasAudio = hasAudio;
        m_duration = m_isStream ? MediaTime::positiveInfiniteTime() : MediaTime(durationUs, 1000000);
        m_naturalSize = FloatSize(width, height);
        if (!m_isStream)
            m_buffered.add(MediaTime::zeroTime(), m_duration);
        player->characteristicChanged();
        player->durationChanged();
        if (hasVideo)
            player->sizeChanged();
        setReadyState(MediaPlayer::ReadyState::HaveMetadata);
    }
    if (prerolled && m_readyState < MediaPlayer::ReadyState::HaveEnoughData && opened) {
        setReadyState(MediaPlayer::ReadyState::HaveEnoughData);
        setNetworkState(MediaPlayer::NetworkState::Loaded);
    }
    if (m_isStream && width && height && FloatSize(width, height) != m_naturalSize) {
        m_naturalSize = FloatSize(width, height);
        player->sizeChanged();
    }
    if (serial != m_lastFrameSerial) {
        m_lastFrameSerial = serial;
        player->repaint();
    }
    if (m_seeking && m_seekPromise && seekDone && prerolled) {
        // A seek finishes once the worker has re-prerolled at the target.
        m_seeking = false;
        if (auto promise = std::exchange(m_seekPromise, std::nullopt))
            promise->resolve(MediaTime(seekUs, 1000000));
        player->timeChanged();
    }
    if (ended && !m_isStream)
        player->timeChanged();
    if (ended && m_isStream)
        streamEnded();
}

// ---- painting --------------------------------------------------------------------------------------

static RefPtr<NativeImage> imageFromShared(MediaPlayerPrivateCRTShared& shared)
{
    sk_sp<SkImage> image;
    {
        Locker locker { shared.lock };
        if (!shared.frameSerial || shared.frame.empty() || !shared.frameWidth || !shared.frameHeight)
            return nullptr;
        SkImageInfo info = SkImageInfo::Make(shared.frameWidth, shared.frameHeight, kRGBA_8888_SkColorType, kOpaque_SkAlphaType);
        SkPixmap pixmap(info, shared.frame.data(), static_cast<size_t>(shared.frameWidth) * 4);
        image = SkImages::RasterFromPixmapCopy(pixmap);
    }
    if (!image)
        return nullptr;
    return NativeImage::create(WTF::move(image));
}

void MediaPlayerPrivateCRT::paint(GraphicsContext& context, const FloatRect& rect)
{
    if (!m_shared || context.paintingDisabled() || rect.isEmpty())
        return;
    RefPtr image = imageFromShared(*m_shared);
    if (!image)
        return;
    FloatSize size = image->size();
    context.drawNativeImage(*image, rect, FloatRect(FloatPoint(), size));
}

RefPtr<NativeImage> MediaPlayerPrivateCRT::nativeImageForCurrentTime()
{
    return m_shared ? imageFromShared(*m_shared) : nullptr;
}

DestinationColorSpace MediaPlayerPrivateCRT::colorSpace()
{
    return DestinationColorSpace::SRGB();
}

bool MediaPlayerPrivateCRT::hasAvailableVideoFrame() const
{
    if (!m_shared)
        return false;
    Locker locker { m_shared->lock };
    return m_shared->frameSerial > 0;
}

// ---- registration ------------------------------------------------------------------------------------

namespace {

const ASCIILiteral supportedTypes[] = {
    "video/mp4"_s, "video/x-m4v"_s, "audio/mp4"_s, "audio/x-m4a"_s, "audio/mpeg"_s, "audio/mp3"_s,
    "audio/wav"_s, "audio/x-wav"_s, "audio/wave"_s,
};

} // namespace

void MediaPlayerPrivateCRT::getSupportedTypes(HashSet<String>& types)
{
    for (auto type : supportedTypes)
        types.add(type);
}

MediaPlayer::SupportsType MediaPlayerPrivateCRT::supportsType(const MediaEngineSupportParameters& parameters)
{
#if ENABLE(MEDIA_STREAM)
    if (parameters.platformType == PlatformMediaDecodingType::MediaStream)
        return MediaPlayer::SupportsType::IsSupported;
#endif
    if (parameters.platformType != PlatformMediaDecodingType::FileOrHLS)
        return MediaPlayer::SupportsType::IsNotSupported;
    String container = parameters.type.containerType();
    if (container.isEmpty())
        return MediaPlayer::SupportsType::IsNotSupported;
    bool known = false;
    for (auto type : supportedTypes) {
        if (equalIgnoringASCIICase(container, type)) {
            known = true;
            break;
        }
    }
    if (!known)
        return MediaPlayer::SupportsType::IsNotSupported;
    // A codecs list is only accepted when every entry is one of the codecs this build of the media
    // library decodes (H.264, AAC, MP3, PCM).
    auto codecs = parameters.type.codecs();
    if (codecs.isEmpty())
        return MediaPlayer::SupportsType::MayBeSupported;
    for (auto& codec : codecs) {
        bool codecKnown = codec.startsWith("avc1"_s) || codec.startsWith("mp4a"_s) || codec == "mp3"_s || codec == "1"_s;
        if (!codecKnown)
            return MediaPlayer::SupportsType::IsNotSupported;
    }
    return MediaPlayer::SupportsType::IsSupported;
}

class MediaPlayerFactoryCRT final : public MediaPlayerFactory {
    WTF_MAKE_TZONE_ALLOCATED(MediaPlayerFactoryCRT);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(MediaPlayerFactoryCRT);
private:
    MediaPlayerEnums::MediaEngineIdentifier identifier() const final { return MediaPlayerEnums::MediaEngineIdentifier::MediaFoundation; }
    Ref<MediaPlayerPrivateInterface> createMediaEnginePlayer(MediaPlayer& player) const final { return adoptRef(*new MediaPlayerPrivateCRT(player)); }
    void getSupportedTypes(HashSet<String>& types) const final { MediaPlayerPrivateCRT::getSupportedTypes(types); }
    MediaPlayer::SupportsType supportsTypeAndCodecs(const MediaEngineSupportParameters& parameters) const final { return MediaPlayerPrivateCRT::supportsType(parameters); }
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaPlayerFactoryCRT);

void MediaPlayerPrivateCRT::registerMediaEngine(MediaEngineRegistrar registrar)
{
    registrar(makeUnique<MediaPlayerFactoryCRT>());
}

void MediaPlayerPrivateCRT::ensureRegistered()
{
    static std::once_flag once;
    std::call_once(once, [] {
        MediaPlayerFactorySupport::callRegisterMediaEngine(MediaPlayerPrivateCRT::registerMediaEngine);
    });
}

} // namespace WebCore
