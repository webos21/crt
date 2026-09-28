/* crtmedia/codec.h -- see that header's own top comment for the design
 * reasoning. Every real libavcodec/libswresample type stays confined to
 * this file, matching src/demux.c's/src/extractor.c's own established
 * discipline.
 *
 * fill_video_frame()/fill_audio_buffer() below are a deliberate near-
 * duplicate of src/demux.c's own same-named functions, not a shared
 * helper -- both files independently wrap an AVFrame/swr_convert() output
 * into this project's own crtmedia_frame/crtmedia_audio_buffer contract,
 * and demux.c's own functions are `static` to that translation unit.
 * Left duplicated deliberately rather than refactored into a shared
 * internal header this pass: crtmedia_demuxer_* (demux.h) is still its
 * own, separate, already-verified implementation, not yet rebuilt over
 * this new core (see docs/libcrtmedia_api_policy.md's own Decision and
 * TODO.md's own next step) -- sharing code between the two now would mean
 * refactoring already-working demux.c mid-way through landing this new,
 * separate, not-yet-integrated layer, a real regression risk for no
 * benefit until that rebuild actually happens. */

#include "crtmedia/codec.h"

#include "codec_test_control.h"
#if defined(CRT_TARGET_OS_WINDOWS)
#include "gpu_frame_d3d11.h"
#elif defined(CRT_TARGET_OS_LINUX)
#include "gpu_frame_vaapi.h"
#endif

#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
#include <libswresample/swresample.h>

#include <stdlib.h>
#include <string.h>

struct crtmedia_codec {
  AVCodecContext* codec_ctx;
  int is_video;
  int is_encoder;
  AVPacket* packet;
  AVFrame* decode_frame;
  SwrContext* swr_ctx; /* audio only */
  crtmedia_sample_format out_sample_format; /* audio only */
  int eof_signaled;
  int eof_drained;
  int64_t frame_duration_us;
  int64_t encode_pts_queue[256];
  uint32_t encode_pts_head;
  uint32_t encode_pts_count;
  /* Hardware decode, phase A (2026-09-08, TODO.md's "hardware decode,
   * phase A" step) -- video only, and only ever set when the format
   * passed to crtmedia_codec_create_decoder() carried CRTMEDIA_FORMAT_
   * KEY_PREFER_HARDWARE_DECODE and that request actually succeeded (see
   * that function's own real fallback-to-software path otherwise). hw_
   * device_ctx/hw_pix_fmt stay NULL/AV_PIX_FMT_NONE on every other real
   * codec instance, matching this project's own "additive, zero risk to
   * existing behavior" discipline.
   *
   * Six-state diagnostic model (2026-09-18, "Hardware video decode"
   * Tranche 2, docs/crtmedia_hardware_decode_acceptance.md): hw_requested/
   * hw_device_created/hw_pixfmt_offered/hw_frame_observed are private,
   * fine-grained state, each latched true the first time its own real
   * event happens and never reset (matching hardware_accelerated's own
   * existing "sticks true for the lifetime of this instance" contract) --
   * exposed only through codec_test_control.h's own private, test-only
   * crtmedia_codec_test_get_hw_diagnostics(), never through public API.
   * hardware_accelerated itself is the public crtmedia_codec_is_hardware_
   * accelerated() value: true only once a real hardware-backed frame has
   * actually been transferred to CPU memory (crtmedia_codec_dequeue_
   * output()'s own hw-frame branch) -- device creation and pixel-format
   * negotiation alone are deliberately NOT enough to set it (Tranche 0's
   * own gap 2, closed here). */
  int hw_requested;
  int hw_device_created;
  int hw_pixfmt_offered;
  int hw_frame_observed;
  AVBufferRef* hw_device_ctx;
  enum AVPixelFormat hw_pix_fmt;
  int hardware_accelerated;
  /* True only once a real hardware frame was delivered as
   * CRTMEDIA_GPU_MEMORY_GPU. This is the decoder-to-crtmedia handoff, not
   * an end-to-end interop claim: the Windows bridge performs a later GPU
   * copy while macOS/Linux sample decoder storage directly. */
  int hw_gpu_frame_delivered;
  /* Linux VA-API H.264 hardware encode (2026-09-28, "Encode and capture"
   * Tranche 3, docs/crtmedia_encode_capture_acceptance.md). Set only when
   * crtmedia_codec_create_encoder() was asked for mime="video/avc" and a
   * real VA-API encode device+frame pool actually came up -- there is no
   * software H.264 fallback within this codec instance (FFmpeg ships no
   * built-in H.264 encoder; only x264/OpenH264, both external and
   * deliberately not ported here), so creation itself fails honestly
   * (CRTMEDIA_ERROR_UNSUPPORTED) when this cannot be satisfied; a caller
   * wanting a guaranteed-available encoder uses the existing mp4v-es
   * software path instead, exactly like every other "real hardware path,
   * honest failure otherwise" precedent in this file. hw_device_ctx above
   * is reused for the VAAPI device (a crtmedia_codec instance is never
   * simultaneously an encoder and a decoder, so there is no aliasing
   * concern); hw_frames_ctx/hw_encode_frame are additive, encoder-only. */
  int is_vaapi_encoder;
  AVBufferRef* hw_frames_ctx;
  AVFrame* hw_encode_frame;
  /* macOS VideoToolbox H.264 hardware encode (2026-09-28, "Encode and
   * capture" Tranche 4B). Unlike is_vaapi_encoder above, this backend needs
   * no hw_frames_ctx/hw_encode_frame at all: FFmpeg's own libavcodec/
   * videotoolboxenc.c lists AV_PIX_FMT_YUV420P directly in its own
   * avc_pix_fmts[] (confirmed by reading that file), so a plain software
   * AVFrame is handed straight to avcodec_send_frame() exactly like the
   * existing mp4v-es path -- VTCompressionSession copies the planes into
   * its own internally pooled CVPixelBuffer (vtenc_send_frame(), read
   * directly, not assumed). This flag exists only so crtmedia_codec_get_
   * output_format() reports mime="video/avc" for this instance; every
   * other codepath below (queue_frame's plane copy, dequeue_encoded_
   * output) already takes the same route as mp4v-es via the shared
   * `!is_vaapi_encoder` branches. */
  int is_videotoolbox_encoder;
};

static enum AVCodecID codec_id_for_mime(const char* mime) {
  if (strcmp(mime, "video/avc") == 0) {
    return AV_CODEC_ID_H264;
  }
  if (strcmp(mime, "video/mp4v-es") == 0) {
    return AV_CODEC_ID_MPEG4;
  }
  if (strcmp(mime, "audio/mp4a-latm") == 0) {
    return AV_CODEC_ID_AAC;
  }
  if (strcmp(mime, "audio/mpeg") == 0) {
    return AV_CODEC_ID_MP3;
  }
  if (strcmp(mime, "audio/raw") == 0) {
    return AV_CODEC_ID_PCM_S16LE;
  }
  return AV_CODEC_ID_NONE;
}

static void release_encoded_sample(crtmedia_encoded_sample* sample, void* release_context) {
  (void)sample;
  free(release_context);
}

void crtmedia_encoded_sample_release(crtmedia_encoded_sample* sample) {
  if (sample == NULL) {
    return;
  }
  if (sample->release != NULL) {
    sample->release(sample, sample->release_context);
  }
  memset(sample, 0, sizeof(*sample));
}

/* Real per-host hwaccel type (2026-09-08, "hardware decode, phase A") --
 * av_hwdevice_ctx_create() (below) does all real device creation itself
 * (D3D11 device creation, VideoToolbox session setup, VAAPI display
 * connection) once given the right AVHWDeviceType, so this project needs
 * no hand-rolled platform device-creation code the way libcrtgfx's own
 * D3D12/Vulkan/Metal backends did. D3D11VA over D3D12VA on Windows: the
 * more mature, more widely-supported real FFmpeg hwaccel path (TODO.md's
 * own phase-A phrasing already accepts either). AV_HWDEVICE_TYPE_NONE
 * (real, valid "no real accelerator for this host" sentinel) on any other
 * target -- crtmedia_codec_create_decoder() below already treats that the
 * same as "device creation failed," so no separate guard is needed at the
 * call site. */
static enum AVHWDeviceType hw_type_for_platform(void) {
#if defined(CRT_TARGET_OS_WINDOWS)
  return AV_HWDEVICE_TYPE_D3D11VA;
#elif defined(CRT_TARGET_OS_MACOS)
  return AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
#elif defined(CRT_TARGET_OS_LINUX)
  return AV_HWDEVICE_TYPE_VAAPI;
#else
  return AV_HWDEVICE_TYPE_NONE;
#endif
}

/* Real AVCodecContext::get_format callback (signature confirmed directly
 * against this project's own vendored libavcodec/avcodec.h) -- offers
 * `codec`'s own real hw_pix_fmt if the decoder is willing to use it,
 * falling through to FFmpeg's own default negotiation otherwise (a real
 * decoder can legitimately decide not to offer the requested hw format at
 * all, e.g. for a profile/level its own hwaccel doesn't support -- this
 * is real, expected fallback, not an error this callback itself needs to
 * detect: crtmedia_codec_dequeue_output() only ever downloads a frame
 * when its own format actually equals hw_pix_fmt, so a silent software
 * fallback here is already handled correctly one layer up). */
static enum AVPixelFormat crtmedia_codec_get_format(
    struct AVCodecContext* ctx, const enum AVPixelFormat* formats) {
  crtmedia_codec* codec = (crtmedia_codec*)ctx->opaque;
  const enum AVPixelFormat* p;
  for (p = formats; *p != AV_PIX_FMT_NONE; ++p) {
    if (*p == codec->hw_pix_fmt) {
      /* hw_pixfmt_offered (Tranche 2): the decoder itself actually
       * selected the hardware format here, not merely "a device exists" --
       * distinct from hw_device_created, which only means av_hwdevice_ctx_
       * create()/avcodec_open2() succeeded. A decoder can legitimately
       * never reach this line at all (this function's own top comment). */
      codec->hw_pixfmt_offered = 1;
      return *p;
    }
  }
  return avcodec_default_get_format(ctx, formats);
}

crtmedia_result crtmedia_codec_create_decoder(const crtmedia_format* format, crtmedia_codec** out_codec) {
  if (format == NULL || out_codec == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  *out_codec = NULL;

  const char* mime = NULL;
  if (crtmedia_format_get_string(format, CRTMEDIA_FORMAT_KEY_MIME, &mime) != CRTMEDIA_OK) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  enum AVCodecID codec_id = codec_id_for_mime(mime);
  if (codec_id == AV_CODEC_ID_NONE) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  const AVCodec* av_codec = avcodec_find_decoder(codec_id);
  if (av_codec == NULL) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  int is_video = av_codec->type == AVMEDIA_TYPE_VIDEO;

  crtmedia_codec* codec = (crtmedia_codec*)calloc(1, sizeof(crtmedia_codec));
  if (codec == NULL) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  codec->is_video = is_video;
  codec->hw_pix_fmt = AV_PIX_FMT_NONE;
  codec->codec_ctx = avcodec_alloc_context3(av_codec);
  codec->packet = av_packet_alloc();
  codec->decode_frame = av_frame_alloc();
  if (codec->codec_ctx == NULL || codec->packet == NULL || codec->decode_frame == NULL) {
    crtmedia_codec_release(codec);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  codec->codec_ctx->opaque = codec;

  int32_t prefer_hardware_decode = 0;
  if (is_video) {
    int32_t width = 0;
    int32_t height = 0;
    crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_WIDTH, &width);
    crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_HEIGHT, &height);
    codec->codec_ctx->width = width;
    codec->codec_ctx->height = height;
    /* Same real, deliberate multi-threaded decode as demux.c's own video
     * path (see that file's own comment for the full reasoning) -- this
     * codec's own decode is exactly as real a pthread-PAL exercise as
     * demux.c's. */
    codec->codec_ctx->thread_count = 2;
    codec->codec_ctx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
    crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_PREFER_HARDWARE_DECODE, &prefer_hardware_decode);
    codec->hw_requested = prefer_hardware_decode != 0;
  } else {
    int32_t sample_rate = 0;
    int32_t channel_count = 0;
    crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_SAMPLE_RATE, &sample_rate);
    crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_CHANNEL_COUNT, &channel_count);
    codec->codec_ctx->sample_rate = sample_rate;
    av_channel_layout_default(&codec->codec_ctx->ch_layout, channel_count);
  }

  const void* csd = NULL;
  size_t csd_size = 0;
  if (crtmedia_format_get_buffer(format, CRTMEDIA_FORMAT_KEY_CSD, &csd, &csd_size) == CRTMEDIA_OK && csd_size > 0) {
    codec->codec_ctx->extradata = (uint8_t*)av_mallocz(csd_size + AV_INPUT_BUFFER_PADDING_SIZE);
    if (codec->codec_ctx->extradata == NULL) {
      crtmedia_codec_release(codec);
      return CRTMEDIA_ERROR_UNSUPPORTED;
    }
    memcpy(codec->codec_ctx->extradata, csd, csd_size);
    codec->codec_ctx->extradata_size = (int)csd_size;
  }

  /* Real hardware decode attempt (2026-09-08, "hardware decode, phase
   * A") -- av_hwdevice_ctx_create() does all real per-host device
   * creation itself (see hw_type_for_platform()'s own comment); this is
   * a real, honest attempt, never a hard requirement. Real recovery:
   * either step below failing (no compatible device at all, or this
   * specific codec/profile genuinely not supported by this host's own
   * hwaccel) falls all the way back to the exact same plain software
   * avcodec_open2() call every codec on this host already used before
   * this feature existed -- codec->hw_pix_fmt stays AV_PIX_FMT_NONE, so
   * crtmedia_codec_get_format()/dequeue_output()'s own hw-frame-download
   * branch never activates for this instance. */
  if (is_video && prefer_hardware_decode != 0) {
    enum AVHWDeviceType hw_type = hw_type_for_platform();
    AVDictionary* hw_device_opts = NULL;
    /* "SHADER" (2026-09-23, Windows Zero-copy decoded textures Tranche 2)
     * -- confirmed by reading FFmpeg's own hwcontext_d3d11va.c directly:
     * av_hwdevice_ctx_create()'s own opts dict is the only way to make the
     * real D3D11VA decode-pool texture D3D11_BIND_SHADER_RESOURCE-capable
     * (device_hwctx->BindFlags |= D3D11_BIND_SHADER_RESOURCE when this key
     * is present, propagated into the frames context's own real texture
     * description at pool-allocation time, hwcontext_d3d11va.c's own
     * d3d11va_frames_init()). Without it the decode-pool texture is
     * BindFlags == D3D11_BIND_DECODER only (set by libavcodec/dxva2.c's
     * own ff_dxva2_common_frame_params()), and crtgfx_skia_media's own
     * D3D12 bridge (skia_bridge.cc) fails every real
     * CreateShaderResourceView1() call with E_INVALIDARG -- confirmed for
     * real on this host's own hardware before this fix. The value itself
     * is never read (av_dict_get(opts, "SHADER", NULL, 0) only checks
     * presence), so "1" is an arbitrary non-empty placeholder. Harmless,
     * ignored no-op on every other real hw_type this function handles
     * (VideoToolbox/VAAPI never look at this key). */
    if (hw_type == AV_HWDEVICE_TYPE_D3D11VA) {
      av_dict_set(&hw_device_opts, "SHADER", "1", 0);
    }
    int hw_device_created = hw_type != AV_HWDEVICE_TYPE_NONE &&
        av_hwdevice_ctx_create(&codec->hw_device_ctx, hw_type, NULL, hw_device_opts, 0) >= 0;
    av_dict_free(&hw_device_opts);
    if (hw_device_created) {
      switch (hw_type) {
        case AV_HWDEVICE_TYPE_D3D11VA:
          codec->hw_pix_fmt = AV_PIX_FMT_D3D11;
          break;
        case AV_HWDEVICE_TYPE_VIDEOTOOLBOX:
          codec->hw_pix_fmt = AV_PIX_FMT_VIDEOTOOLBOX;
          break;
        case AV_HWDEVICE_TYPE_VAAPI:
          codec->hw_pix_fmt = AV_PIX_FMT_VAAPI;
          break;
        default:
          codec->hw_pix_fmt = AV_PIX_FMT_NONE;
          break;
      }
      codec->codec_ctx->hw_device_ctx = av_buffer_ref(codec->hw_device_ctx);
      codec->codec_ctx->get_format = crtmedia_codec_get_format;
    }
  }

  if (avcodec_open2(codec->codec_ctx, av_codec, NULL) < 0) {
    if (codec->hw_device_ctx != NULL) {
      /* Real fallback and recovery: this exact hardware path did not
       * work for this specific codec/profile even though a real device
       * was created -- discard every hw-specific field and retry with a
       * genuinely fresh, plain software AVCodecContext (not the same,
       * possibly partially-configured-by-open2 one -- avcodec_open2()'s
       * own real failure-state contract does not document a failed
       * context as safe to reopen). extradata must be recreated too,
       * since it lived on the now-freed context. */
      av_buffer_unref(&codec->hw_device_ctx);
      codec->hw_pix_fmt = AV_PIX_FMT_NONE;
      avcodec_free_context(&codec->codec_ctx);
      codec->codec_ctx = avcodec_alloc_context3(av_codec);
      if (codec->codec_ctx == NULL) {
        crtmedia_codec_release(codec);
        return CRTMEDIA_ERROR_UNSUPPORTED;
      }
      codec->codec_ctx->opaque = codec;
      {
        int32_t width = 0;
        int32_t height = 0;
        crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_WIDTH, &width);
        crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_HEIGHT, &height);
        codec->codec_ctx->width = width;
        codec->codec_ctx->height = height;
      }
      codec->codec_ctx->thread_count = 2;
      codec->codec_ctx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
      if (csd_size > 0) {
        codec->codec_ctx->extradata = (uint8_t*)av_mallocz(csd_size + AV_INPUT_BUFFER_PADDING_SIZE);
        if (codec->codec_ctx->extradata == NULL) {
          crtmedia_codec_release(codec);
          return CRTMEDIA_ERROR_UNSUPPORTED;
        }
        memcpy(codec->codec_ctx->extradata, csd, csd_size);
        codec->codec_ctx->extradata_size = (int)csd_size;
      }
      if (avcodec_open2(codec->codec_ctx, av_codec, NULL) < 0) {
        crtmedia_codec_release(codec);
        return CRTMEDIA_ERROR_UNSUPPORTED;
      }
    } else {
      crtmedia_codec_release(codec);
      return CRTMEDIA_ERROR_UNSUPPORTED;
    }
  } else if (codec->hw_device_ctx != NULL) {
    /* hw_device_created (Tranche 2): device creation and codec open both
     * succeeded -- this is NOT hardware-decode success and must never set
     * the public hardware_accelerated flag (Tranche 0's own gap 2). A
     * decoder can open cleanly with a hardware device attached and still
     * never actually decode a single frame through it (crtmedia_codec_
     * get_format()'s own comment); hardware_accelerated only becomes true
     * in crtmedia_codec_dequeue_output(), once a real hardware-backed
     * frame has actually been transferred to CPU memory. */
    codec->hw_device_created = 1;
  }

  if (!is_video) {
    /* Same S16-preferred-unless-native-float choice as demux.c's own
     * audio path -- this contract only ever produces S16 or FLT
     * (crtmedia/audio.h's own comment). */
    codec->out_sample_format = (codec->codec_ctx->sample_fmt == AV_SAMPLE_FMT_FLT ||
                                 codec->codec_ctx->sample_fmt == AV_SAMPLE_FMT_FLTP)
                                    ? CRTMEDIA_SAMPLE_FORMAT_FLT
                                    : CRTMEDIA_SAMPLE_FORMAT_S16;
    enum AVSampleFormat out_fmt =
        codec->out_sample_format == CRTMEDIA_SAMPLE_FORMAT_FLT ? AV_SAMPLE_FMT_FLT : AV_SAMPLE_FMT_S16;
    AVChannelLayout out_layout;
    av_channel_layout_default(&out_layout, codec->codec_ctx->ch_layout.nb_channels);
    swr_alloc_set_opts2(
        &codec->swr_ctx, &out_layout, out_fmt, codec->codec_ctx->sample_rate, &codec->codec_ctx->ch_layout,
        codec->codec_ctx->sample_fmt, codec->codec_ctx->sample_rate, 0, NULL);
    av_channel_layout_uninit(&out_layout);
    if (codec->swr_ctx == NULL || swr_init(codec->swr_ctx) < 0) {
      crtmedia_codec_release(codec);
      return CRTMEDIA_ERROR_UNSUPPORTED;
    }
  }

  *out_codec = codec;
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_codec_create_encoder(const crtmedia_format* format, crtmedia_codec** out_codec) {
  if (format == NULL || out_codec == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  *out_codec = NULL;

  const char* mime = NULL;
  int32_t width = 0;
  int32_t height = 0;
  int32_t pixel_format = 0;
  int32_t frame_rate = 0;
  int32_t bit_rate = 0;
  /* want_h264 picks the per-OS real hardware H.264 encoder: h264_vaapi on
   * Linux (Tranche 3), h264_videotoolbox on macOS (Tranche 4B). There is no
   * software H.264 fallback within this function on either OS (this file's
   * own established "real hardware path, honest failure otherwise"
   * discipline -- see is_vaapi_encoder's own struct comment); a caller
   * wanting a guaranteed-available encoder uses the existing mp4v-es
   * software path instead. */
  int want_h264 = 0;
  int mime_ok = 0;
  if (crtmedia_format_get_string(format, CRTMEDIA_FORMAT_KEY_MIME, &mime) == CRTMEDIA_OK) {
    if (strcmp(mime, "video/mp4v-es") == 0) {
      mime_ok = 1;
    } else if (strcmp(mime, "video/avc") == 0) {
      mime_ok = 1;
      want_h264 = 1;
    }
  }
  if (!mime_ok ||
      crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_WIDTH, &width) != CRTMEDIA_OK ||
      crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_HEIGHT, &height) != CRTMEDIA_OK ||
      crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_PIXEL_FORMAT, &pixel_format) != CRTMEDIA_OK ||
      crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_FRAME_RATE, &frame_rate) != CRTMEDIA_OK ||
      width <= 0 || height <= 0 || (width & 1) != 0 || (height & 1) != 0 || frame_rate <= 0 ||
      pixel_format != CRTMEDIA_PIXEL_FORMAT_YUV420P) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
#if !defined(CRT_TARGET_OS_LINUX) && !defined(CRT_TARGET_OS_MACOS)
  if (want_h264) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
#endif
  if (crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_BIT_RATE, &bit_rate) != CRTMEDIA_OK ||
      bit_rate <= 0) {
    bit_rate = 1000000;
  }

  const AVCodec* av_codec = NULL;
  if (want_h264) {
#if defined(CRT_TARGET_OS_LINUX)
    av_codec = avcodec_find_encoder_by_name("h264_vaapi");
#elif defined(CRT_TARGET_OS_MACOS)
    av_codec = avcodec_find_encoder_by_name("h264_videotoolbox");
#endif
  } else {
    av_codec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
  }
  if (av_codec == NULL) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  crtmedia_codec* codec = (crtmedia_codec*)calloc(1, sizeof(*codec));
  if (codec == NULL) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  codec->is_video = 1;
  codec->is_encoder = 1;
  codec->hw_pix_fmt = AV_PIX_FMT_NONE;
  codec->frame_duration_us = 1000000 / frame_rate;
  codec->codec_ctx = avcodec_alloc_context3(av_codec);
  codec->packet = av_packet_alloc();
  codec->decode_frame = av_frame_alloc();
  if (codec->codec_ctx == NULL || codec->packet == NULL || codec->decode_frame == NULL) {
    crtmedia_codec_release(codec);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  codec->codec_ctx->width = width;
  codec->codec_ctx->height = height;
  codec->codec_ctx->time_base = (AVRational){1, 30000};
  codec->codec_ctx->framerate = (AVRational){frame_rate, 1};
  codec->codec_ctx->bit_rate = bit_rate;
  codec->codec_ctx->gop_size = frame_rate;
  codec->codec_ctx->max_b_frames = 0;
  codec->codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

#if defined(CRT_TARGET_OS_LINUX)
  if (want_h264) {
    /* Real VA-API hardware encode device (this project's own libva/
     * libva-drm, already required by porting/recipes/ffmpeg.json's
     * --enable-vaapi for decode -- Tranche 3 adds no new host
     * dependency). No sw_frames fallback exists for this mime inside
     * this function -- a host without a usable VA-API display fails
     * creation itself, honestly, rather than silently downgrading to a
     * different codec the caller did not ask for. */
    if (av_hwdevice_ctx_create(&codec->hw_device_ctx, AV_HWDEVICE_TYPE_VAAPI, NULL, NULL, 0) < 0) {
      crtmedia_codec_release(codec);
      return CRTMEDIA_ERROR_UNSUPPORTED;
    }
    AVBufferRef* frames_ref = av_hwframe_ctx_alloc(codec->hw_device_ctx);
    if (frames_ref == NULL) {
      crtmedia_codec_release(codec);
      return CRTMEDIA_ERROR_UNSUPPORTED;
    }
    AVHWFramesContext* frames_ctx = (AVHWFramesContext*)frames_ref->data;
    frames_ctx->format = AV_PIX_FMT_VAAPI;
    frames_ctx->sw_format = AV_PIX_FMT_NV12;
    frames_ctx->width = width;
    frames_ctx->height = height;
    /* A small, fixed margin above "one in flight" -- this codec's own
     * max_b_frames=0 means no reference reordering, but the VAAPI driver
     * still wants a short real pipeline (reconstructed reference + the
     * surface currently being read back), matching FFmpeg's own vaapi_
     * encode.c default pool sizing discipline for a B-frame-less config. */
    frames_ctx->initial_pool_size = 4;
    if (av_hwframe_ctx_init(frames_ref) < 0) {
      av_buffer_unref(&frames_ref);
      crtmedia_codec_release(codec);
      return CRTMEDIA_ERROR_UNSUPPORTED;
    }
    codec->hw_frames_ctx = frames_ref;
    codec->codec_ctx->hw_frames_ctx = av_buffer_ref(codec->hw_frames_ctx);
    codec->codec_ctx->pix_fmt = AV_PIX_FMT_VAAPI;
    codec->hw_encode_frame = av_frame_alloc();
    if (codec->codec_ctx->hw_frames_ctx == NULL || codec->hw_encode_frame == NULL) {
      crtmedia_codec_release(codec);
      return CRTMEDIA_ERROR_UNSUPPORTED;
    }
    codec->is_vaapi_encoder = 1;
  } else
#endif
  {
    /* Plain software-frame path: the existing mp4v-es software encoder
     * (want_h264 == 0 everywhere), and macOS's h264_videotoolbox (want_h264
     * == 1 on macOS -- see is_videotoolbox_encoder's own struct comment for
     * why this hardware backend takes the same plain-YUV420P route as
     * software, unlike VA-API above). */
    codec->codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
#if defined(CRT_TARGET_OS_MACOS)
    if (want_h264) {
      codec->is_videotoolbox_encoder = 1;
    }
#endif
  }

  if (avcodec_open2(codec->codec_ctx, av_codec, NULL) < 0) {
    crtmedia_codec_release(codec);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  /* The encoder-input scratch frame: NV12 for the VA-API path (matching
   * frames_ctx->sw_format above -- crtmedia_codec_queue_frame() writes the
   * caller's YUV420P planes into it that way before uploading), plain
   * YUV420P for every other path (existing software mp4v-es, and macOS
   * VideoToolbox), unchanged. */
  codec->decode_frame->format = codec->is_vaapi_encoder ? AV_PIX_FMT_NV12 : AV_PIX_FMT_YUV420P;
  codec->decode_frame->width = width;
  codec->decode_frame->height = height;
  if (av_frame_get_buffer(codec->decode_frame, 32) < 0) {
    crtmedia_codec_release(codec);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  *out_codec = codec;
  return CRTMEDIA_OK;
}

void crtmedia_codec_release(crtmedia_codec* codec) {
  if (codec == NULL) {
    return;
  }
  if (codec->hw_device_ctx != NULL) {
    av_buffer_unref(&codec->hw_device_ctx);
  }
  if (codec->hw_frames_ctx != NULL) {
    av_buffer_unref(&codec->hw_frames_ctx);
  }
  if (codec->hw_encode_frame != NULL) {
    av_frame_free(&codec->hw_encode_frame);
  }
  if (codec->swr_ctx != NULL) {
    swr_free(&codec->swr_ctx);
  }
  if (codec->decode_frame != NULL) {
    av_frame_free(&codec->decode_frame);
  }
  if (codec->packet != NULL) {
    av_packet_free(&codec->packet);
  }
  if (codec->codec_ctx != NULL) {
    avcodec_free_context(&codec->codec_ctx);
  }
  free(codec);
}

crtmedia_result crtmedia_codec_queue_input(
    crtmedia_codec* codec, const void* data, uint32_t size, int64_t pts_us, uint32_t flags) {
  if (codec == NULL || codec->is_encoder || (data == NULL && size > 0)) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }

  if (size > 0) {
    av_packet_unref(codec->packet);
    if (av_new_packet(codec->packet, (int)size) < 0) {
      return CRTMEDIA_ERROR_UNSUPPORTED;
    }
    memcpy(codec->packet->data, data, size);
    codec->packet->pts = pts_us != CRTMEDIA_FRAME_TIMESTAMP_NONE ? pts_us : AV_NOPTS_VALUE;

    int send_ret = avcodec_send_packet(codec->codec_ctx, codec->packet);
    av_packet_unref(codec->packet);
    if (send_ret == AVERROR(EAGAIN)) {
      return CRTMEDIA_WOULD_BLOCK;
    }
    if (send_ret < 0) {
      return CRTMEDIA_ERROR_UNSUPPORTED;
    }
  }

  if ((flags & CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM) != 0 && !codec->eof_signaled) {
    avcodec_send_packet(codec->codec_ctx, NULL);
    codec->eof_signaled = 1;
  }
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_codec_queue_frame(
    crtmedia_codec* codec, const crtmedia_frame* frame, uint32_t flags) {
  if (codec == NULL || !codec->is_encoder) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  if (frame == NULL) {
    if ((flags & CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM) == 0) {
      return CRTMEDIA_ERROR_INVALID_ARGUMENT;
    }
    if (!codec->eof_signaled) {
      int ret = avcodec_send_frame(codec->codec_ctx, NULL);
      if (ret == AVERROR(EAGAIN)) {
        return CRTMEDIA_WOULD_BLOCK;
      }
      if (ret < 0) {
        return CRTMEDIA_ERROR_UNSUPPORTED;
      }
      codec->eof_signaled = 1;
    }
    return CRTMEDIA_OK;
  }
  if ((flags & CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM) != 0 || codec->eof_signaled ||
      frame->format != CRTMEDIA_PIXEL_FORMAT_YUV420P ||
      frame->width != (uint32_t)codec->codec_ctx->width ||
      frame->height != (uint32_t)codec->codec_ctx->height || frame->plane_count < 3 ||
      frame->timestamp_us == CRTMEDIA_FRAME_TIMESTAMP_NONE ||
      codec->encode_pts_count == 256) {
    return codec->encode_pts_count == 256 ? CRTMEDIA_WOULD_BLOCK : CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  if (av_frame_make_writable(codec->decode_frame) < 0) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  for (uint32_t plane = 0; plane < 3; ++plane) {
    uint32_t row_bytes = plane == 0 ? frame->width : (frame->width + 1u) / 2u;
    uint32_t rows = plane == 0 ? frame->height : (frame->height + 1u) / 2u;
    if (frame->planes[plane].data == NULL || frame->planes[plane].stride < row_bytes ||
        frame->planes[plane].width < row_bytes || frame->planes[plane].height < rows) {
      return CRTMEDIA_ERROR_INVALID_ARGUMENT;
    }
    if (!codec->is_vaapi_encoder) {
      /* Direct planar YUV420P copy -- the existing software path,
       * byte-identical to before this tranche. */
      for (uint32_t row = 0; row < rows; ++row) {
        memcpy(codec->decode_frame->data[plane] + row * codec->decode_frame->linesize[plane],
               (const uint8_t*)frame->planes[plane].data + row * frame->planes[plane].stride,
               row_bytes);
      }
    } else if (plane == 0) {
      /* Y plane: still a direct copy, decode_frame->data[0] either way. */
      for (uint32_t row = 0; row < rows; ++row) {
        memcpy(codec->decode_frame->data[0] + row * codec->decode_frame->linesize[0],
               (const uint8_t*)frame->planes[0].data + row * frame->planes[0].stride, row_bytes);
      }
    } else {
      /* U/V -> interleaved NV12 UV (decode_frame->data[1]) -- the VA-API
       * driver's own sw_format above (frames_ctx->sw_format = NV12,
       * crtmedia_codec_create_encoder()). U writes even bytes, V writes
       * odd bytes; this plane==1 (U) and plane==2 (V) pass both target
       * the same NV12 plane. */
      uint32_t dst_offset = plane == 1 ? 0u : 1u;
      const uint8_t* source = (const uint8_t*)frame->planes[plane].data;
      for (uint32_t row = 0; row < rows; ++row) {
        uint8_t* dst_row = codec->decode_frame->data[1] + (size_t)row * codec->decode_frame->linesize[1];
        const uint8_t* src_row = source + (size_t)row * frame->planes[plane].stride;
        for (uint32_t column = 0; column < row_bytes; ++column) {
          dst_row[column * 2u + dst_offset] = src_row[column];
        }
      }
    }
  }
  AVFrame* send_frame = codec->decode_frame;
  if (codec->is_vaapi_encoder) {
    av_frame_unref(codec->hw_encode_frame);
    if (av_hwframe_get_buffer(codec->codec_ctx->hw_frames_ctx, codec->hw_encode_frame, 0) < 0 ||
        av_hwframe_transfer_data(codec->hw_encode_frame, codec->decode_frame, 0) < 0) {
      return CRTMEDIA_ERROR_UNSUPPORTED;
    }
    send_frame = codec->hw_encode_frame;
  }
  send_frame->pts = av_rescale_q(frame->timestamp_us, AV_TIME_BASE_Q, codec->codec_ctx->time_base);
  int ret = avcodec_send_frame(codec->codec_ctx, send_frame);
  if (ret == AVERROR(EAGAIN)) {
    return CRTMEDIA_WOULD_BLOCK;
  }
  if (ret < 0) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  uint32_t tail = (codec->encode_pts_head + codec->encode_pts_count) % 256;
  codec->encode_pts_queue[tail] = frame->timestamp_us;
  ++codec->encode_pts_count;
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_codec_dequeue_encoded_output(
    crtmedia_codec* codec, crtmedia_encoded_sample* out_sample, int* out_eof) {
  if (codec == NULL || !codec->is_encoder || out_sample == NULL || out_eof == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  *out_eof = 0;
  if (codec->eof_drained) {
    *out_eof = 1;
    return CRTMEDIA_OK;
  }
  av_packet_unref(codec->packet);
  int ret = avcodec_receive_packet(codec->codec_ctx, codec->packet);
  if (ret == AVERROR(EAGAIN)) {
    return CRTMEDIA_WOULD_BLOCK;
  }
  if (ret == AVERROR_EOF) {
    codec->eof_drained = 1;
    *out_eof = 1;
    return CRTMEDIA_OK;
  }
  if (ret < 0 || codec->encode_pts_count == 0) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  void* data = malloc(codec->packet->size > 0 ? (size_t)codec->packet->size : 1u);
  if (data == NULL) {
    av_packet_unref(codec->packet);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  memcpy(data, codec->packet->data, (size_t)codec->packet->size);
  int64_t pts_us = codec->encode_pts_queue[codec->encode_pts_head];
  codec->encode_pts_head = (codec->encode_pts_head + 1) % 256;
  --codec->encode_pts_count;

  memset(out_sample, 0, sizeof(*out_sample));
  out_sample->data = data;
  out_sample->size = (uint32_t)codec->packet->size;
  out_sample->pts_us = pts_us;
  out_sample->dts_us = pts_us;
  out_sample->duration_us = codec->frame_duration_us;
  out_sample->flags = (codec->packet->flags & AV_PKT_FLAG_KEY) != 0
                          ? CRTMEDIA_ENCODED_SAMPLE_FLAG_KEY_FRAME
                          : CRTMEDIA_ENCODED_SAMPLE_FLAG_NONE;
  out_sample->release = release_encoded_sample;
  out_sample->release_context = data;
  av_packet_unref(codec->packet);
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_codec_get_output_format(
    const crtmedia_codec* codec, crtmedia_format** out_format) {
  if (codec == NULL || !codec->is_encoder || out_format == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  *out_format = NULL;
  crtmedia_format* format = NULL;
  if (crtmedia_format_create(&format) != CRTMEDIA_OK) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  if (crtmedia_format_set_string(
          format, CRTMEDIA_FORMAT_KEY_MIME,
          (codec->is_vaapi_encoder || codec->is_videotoolbox_encoder) ? "video/avc" : "video/mp4v-es") != CRTMEDIA_OK ||
      crtmedia_format_set_int32(format, CRTMEDIA_FORMAT_KEY_WIDTH, codec->codec_ctx->width) != CRTMEDIA_OK ||
      crtmedia_format_set_int32(format, CRTMEDIA_FORMAT_KEY_HEIGHT, codec->codec_ctx->height) != CRTMEDIA_OK ||
      crtmedia_format_set_int32(format, CRTMEDIA_FORMAT_KEY_PIXEL_FORMAT, CRTMEDIA_PIXEL_FORMAT_YUV420P) != CRTMEDIA_OK ||
      crtmedia_format_set_int32(format, CRTMEDIA_FORMAT_KEY_FRAME_RATE, codec->codec_ctx->framerate.num) != CRTMEDIA_OK ||
      crtmedia_format_set_int32(format, CRTMEDIA_FORMAT_KEY_BIT_RATE, (int32_t)codec->codec_ctx->bit_rate) != CRTMEDIA_OK) {
    crtmedia_format_release(format);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  if (codec->codec_ctx->extradata != NULL && codec->codec_ctx->extradata_size > 0 &&
      crtmedia_format_set_buffer(format, CRTMEDIA_FORMAT_KEY_CSD, codec->codec_ctx->extradata,
                                 (size_t)codec->codec_ctx->extradata_size) != CRTMEDIA_OK) {
    crtmedia_format_release(format);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  *out_format = format;
  return CRTMEDIA_OK;
}

static void release_video_frame(crtmedia_frame* frame, void* release_context) {
  (void)frame;
  AVFrame* avframe = (AVFrame*)release_context;
  av_frame_free(&avframe);
}

static void fill_video_frame(AVFrame* avframe, crtmedia_frame* out_frame) {
  memset(out_frame, 0, sizeof(*out_frame));
  /* NV12 (2026-09-08, "hardware decode, phase A"): avframe here is always
   * a real, CPU-resident software frame by this point -- crtmedia_codec_
   * dequeue_output() already downloaded it via av_hwframe_transfer_data()
   * if the raw decode output was still hardware-resident, so avframe->
   * format is a genuine software pixel format either way, never one of
   * the hw formats (AV_PIX_FMT_D3D11/VIDEOTOOLBOX/VAAPI) themselves. */
  out_frame->format =
      avframe->format == AV_PIX_FMT_NV12 ? CRTMEDIA_PIXEL_FORMAT_NV12 : CRTMEDIA_PIXEL_FORMAT_YUV420P;
  out_frame->width = (uint32_t)avframe->width;
  out_frame->height = (uint32_t)avframe->height;
  out_frame->color_range =
      avframe->color_range == AVCOL_RANGE_JPEG ? CRTMEDIA_COLOR_RANGE_FULL : CRTMEDIA_COLOR_RANGE_LIMITED;
  switch (avframe->colorspace) {
    case AVCOL_SPC_BT709:
      out_frame->color_space = CRTMEDIA_COLOR_SPACE_BT709;
      break;
    case AVCOL_SPC_BT470BG:
    case AVCOL_SPC_SMPTE170M:
      out_frame->color_space = CRTMEDIA_COLOR_SPACE_BT601;
      break;
    case AVCOL_SPC_BT2020_NCL:
    case AVCOL_SPC_BT2020_CL:
      out_frame->color_space = CRTMEDIA_COLOR_SPACE_BT2020;
      break;
    default:
      out_frame->color_space = CRTMEDIA_COLOR_SPACE_UNSPECIFIED;
      break;
  }
  out_frame->timestamp_us = avframe->pts != AV_NOPTS_VALUE ? avframe->pts : CRTMEDIA_FRAME_TIMESTAMP_NONE;
  uint32_t chroma_width = (out_frame->width + 1u) / 2u;
  uint32_t chroma_height = (out_frame->height + 1u) / 2u;
  out_frame->planes[0] =
      (crtmedia_frame_plane){avframe->data[0], (uint32_t)avframe->linesize[0], out_frame->width, out_frame->height};
  if (out_frame->format == CRTMEDIA_PIXEL_FORMAT_NV12) {
    /* One interleaved-UV plane -- real NV12 layout, matching crtmedia_
     * frame_describe_planes()'s own NV12 branch (frame.c). */
    out_frame->plane_count = 2;
    out_frame->planes[1] =
        (crtmedia_frame_plane){avframe->data[1], (uint32_t)avframe->linesize[1], chroma_width, chroma_height};
  } else {
    out_frame->plane_count = 3;
    out_frame->planes[1] =
        (crtmedia_frame_plane){avframe->data[1], (uint32_t)avframe->linesize[1], chroma_width, chroma_height};
    out_frame->planes[2] =
        (crtmedia_frame_plane){avframe->data[2], (uint32_t)avframe->linesize[2], chroma_width, chroma_height};
  }
  out_frame->release = release_video_frame;
  out_frame->release_context = avframe;
}

static void release_gpu_video_frame_owned_avframe(crtmedia_gpu_frame* frame, void* release_context) {
  (void)frame;
  AVFrame* avframe = (AVFrame*)release_context;
  av_frame_free(&avframe);
}

/* CPU branch of crtmedia_codec_dequeue_gpu_frame() -- a real, honest
 * crtmedia_gpu_frame(memory_kind == CRTMEDIA_GPU_MEMORY_CPU) built from
 * `avframe`'s own real pixel planes, the same real layout fill_video_
 * frame() above already produces for crtmedia_frame. Deliberately does
 * not carry color_range/color_space: crtmedia_gpu_frame has no such
 * fields today (docs/crtmedia_zero_copy_decode_acceptance.md's own Scope
 * section -- no real external consumer needs them yet). Takes ownership
 * of `avframe` (release frees it), matching fill_video_frame()'s own
 * ownership contract. */
static void fill_gpu_video_frame_cpu(AVFrame* avframe, crtmedia_gpu_frame* out_frame) {
  memset(out_frame, 0, sizeof(*out_frame));
  out_frame->format =
      avframe->format == AV_PIX_FMT_NV12 ? CRTMEDIA_PIXEL_FORMAT_NV12 : CRTMEDIA_PIXEL_FORMAT_YUV420P;
  out_frame->width = (uint32_t)avframe->width;
  out_frame->height = (uint32_t)avframe->height;
  out_frame->memory_kind = CRTMEDIA_GPU_MEMORY_CPU;
  out_frame->timestamp_us = avframe->pts != AV_NOPTS_VALUE ? avframe->pts : CRTMEDIA_FRAME_TIMESTAMP_NONE;
  out_frame->device_id = 0;
  out_frame->native_handle = NULL;
  uint32_t chroma_width = (out_frame->width + 1u) / 2u;
  uint32_t chroma_height = (out_frame->height + 1u) / 2u;
  out_frame->planes[0] =
      (crtmedia_frame_plane){avframe->data[0], (uint32_t)avframe->linesize[0], out_frame->width, out_frame->height};
  if (out_frame->format == CRTMEDIA_PIXEL_FORMAT_NV12) {
    out_frame->plane_count = 2;
    out_frame->planes[1] =
        (crtmedia_frame_plane){avframe->data[1], (uint32_t)avframe->linesize[1], chroma_width, chroma_height};
  } else {
    out_frame->plane_count = 3;
    out_frame->planes[1] =
        (crtmedia_frame_plane){avframe->data[1], (uint32_t)avframe->linesize[1], chroma_width, chroma_height};
    out_frame->planes[2] =
        (crtmedia_frame_plane){avframe->data[2], (uint32_t)avframe->linesize[2], chroma_width, chroma_height};
  }
  out_frame->release = release_gpu_video_frame_owned_avframe;
  out_frame->release_context = avframe;
}

/* Real zero-copy branch for macOS/VideoToolbox (Tranche 1 -- docs/crtmedia_
 * zero_copy_decode_acceptance.md's own native_handle table): `avframe` here
 * is still genuinely hardware-resident (its own
 * format equals AV_PIX_FMT_VIDEOTOOLBOX), never downloaded.
 * FFmpeg's own hwcontext_videotoolbox.c places the real CVPixelBufferRef
 * at avframe->data[3] and that AVFrame's own reference is exactly what
 * keeps the pixel buffer alive -- confirmed by reading that file
 * directly, not assumed -- so native_handle is a real, live
 * CVPixelBufferRef for as long as `avframe` (kept alive in
 * release_context) is not freed. No CVPixelBufferRetain()/Release() is
 * used or needed: this project does not link against real CoreVideo
 * headers anywhere (matching gpu_metal.c's own no-host-SDK-header
 * policy), and none is required since ownership rides entirely on the
 * AVFrame reference already held. crtgfx_skia_media (the real GPU-
 * texture-import bridge) is the one real consumer that
 * interprets this pointer; this function itself never touches CoreVideo/
 * Metal. Video is always reported as NV12 here (this project's own
 * established "every real hardware H.264 decoder produces NV12" fact,
 * crtmedia/frame.h's own comment) since there is no downloaded sw_frame
 * to inspect an actual pixel format on. */
static void fill_gpu_video_frame_videotoolbox(AVFrame* avframe, crtmedia_gpu_frame* out_frame) {
  memset(out_frame, 0, sizeof(*out_frame));
  out_frame->format = CRTMEDIA_PIXEL_FORMAT_NV12;
  out_frame->width = (uint32_t)avframe->width;
  out_frame->height = (uint32_t)avframe->height;
  out_frame->memory_kind = CRTMEDIA_GPU_MEMORY_GPU;
  out_frame->timestamp_us = avframe->pts != AV_NOPTS_VALUE ? avframe->pts : CRTMEDIA_FRAME_TIMESTAMP_NONE;
  out_frame->device_id = 0;
  out_frame->native_handle = avframe->data[3];
  out_frame->plane_count = 0;
  out_frame->release = release_gpu_video_frame_owned_avframe;
  out_frame->release_context = avframe;
}

#if defined(CRT_TARGET_OS_WINDOWS)
/* Backing for the Windows zero-copy branch (Tranche 2, 2026-09-23) -- see
 * gpu_frame_d3d11.h's own top comment for the full "why". Combines the
 * retained AVFrame with its own crtmedia_d3d11_gpu_frame_handle in one
 * allocation so a single release() call frees both together, matching
 * every other fill_gpu_video_frame_*() sibling's "one owner, one release"
 * shape even though this one owns two logically distinct things. */
typedef struct crtmedia_gpu_frame_d3d11_backing {
  AVFrame* avframe;
  crtmedia_d3d11_gpu_frame_handle handle;
} crtmedia_gpu_frame_d3d11_backing;

static void release_gpu_video_frame_d3d11(crtmedia_gpu_frame* frame, void* release_context) {
  (void)frame;
  crtmedia_gpu_frame_d3d11_backing* backing = (crtmedia_gpu_frame_d3d11_backing*)release_context;
  av_frame_free(&backing->avframe);
  free(backing);
}

/* Real zero-copy branch (Windows/D3D11VA -- docs/crtmedia_zero_copy_decode_
 * acceptance.md's own native_handle table). `avframe` here is still
 * genuinely hardware-resident (its own format equals AV_PIX_FMT_D3D11,
 * never downloaded). FFmpeg's own hwcontext_d3d11va.c places the real
 * ID3D11Texture2D pointer/array-index pair at avframe->data[0]/data[1] --
 * confirmed by reading that file directly (wrap_texture_buf(), d3d11va_
 * transfer_get()), not assumed; gpu_frame_d3d11.h's own top comment has the
 * full citation. Returns CRTMEDIA_ERROR_UNSUPPORTED on a real allocation
 * failure, unlike fill_gpu_video_frame_videotoolbox()'s own unchecked
 * style -- this function's own extra heap allocation (the combined backing
 * above) gives it a real failure mode that one does not have. Video is
 * always reported as NV12 here, matching the macOS branch's own identical
 * "every real hardware H.264 decoder produces NV12" reasoning -- there is
 * no downloaded sw_frame to inspect an actual pixel format on. */
static crtmedia_result fill_gpu_video_frame_d3d11(AVFrame* avframe, crtmedia_gpu_frame* out_frame) {
  crtmedia_gpu_frame_d3d11_backing* backing =
      (crtmedia_gpu_frame_d3d11_backing*)malloc(sizeof(*backing));
  if (backing == NULL) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  backing->avframe = avframe;
  backing->handle.texture = avframe->data[0];
  backing->handle.array_index = (int64_t)(intptr_t)avframe->data[1];

  memset(out_frame, 0, sizeof(*out_frame));
  out_frame->format = CRTMEDIA_PIXEL_FORMAT_NV12;
  out_frame->width = (uint32_t)avframe->width;
  out_frame->height = (uint32_t)avframe->height;
  out_frame->memory_kind = CRTMEDIA_GPU_MEMORY_GPU;
  out_frame->timestamp_us = avframe->pts != AV_NOPTS_VALUE ? avframe->pts : CRTMEDIA_FRAME_TIMESTAMP_NONE;
  out_frame->device_id = 0;
  out_frame->native_handle = &backing->handle;
  out_frame->plane_count = 0;
  out_frame->release = release_gpu_video_frame_d3d11;
  out_frame->release_context = backing;
  return CRTMEDIA_OK;
}
#endif

#if defined(CRT_TARGET_OS_LINUX)
/* Backing for the Linux zero-copy branch (Tranche 3, 2026-09-24) -- see
 * gpu_frame_vaapi.h's own top comment. Retains the VAAPI AVFrame (keeps the
 * VASurface/pool slot alive) together with the exported dma-buf descriptor
 * so one release() call closes the fds and frees both. */
typedef struct crtmedia_gpu_frame_vaapi_backing {
  AVFrame* avframe;
  crtmedia_vaapi_gpu_frame_handle handle;
} crtmedia_gpu_frame_vaapi_backing;

static void release_gpu_video_frame_vaapi(crtmedia_gpu_frame* frame, void* release_context) {
  (void)frame;
  crtmedia_gpu_frame_vaapi_backing* backing = (crtmedia_gpu_frame_vaapi_backing*)release_context;
  crtmedia_vaapi_gpu_frame_handle_close(&backing->handle);
  av_frame_free(&backing->avframe);
  free(backing);
}

/* Takes ownership of `avframe` only on CRTMEDIA_OK. Returns
 * CRTMEDIA_ERROR_UNSUPPORTED when the surface cannot be exported as DRM PRIME
 * (allocation failure, driver without DRM_PRIME_2 export, unexpected
 * layout); the caller then falls back to the honest CPU download. Video is
 * NV12 -- the only layout the consumer imports (two-layer R8 + GR88). */
static crtmedia_result fill_gpu_video_frame_vaapi(AVFrame* avframe, crtmedia_gpu_frame* out_frame) {
  crtmedia_gpu_frame_vaapi_backing* backing =
      (crtmedia_gpu_frame_vaapi_backing*)calloc(1, sizeof(*backing));
  if (backing == NULL) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  if (crtmedia_vaapi_export_frame(avframe, &backing->handle) != 0) {
    free(backing);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  backing->avframe = avframe;

  memset(out_frame, 0, sizeof(*out_frame));
  out_frame->format = CRTMEDIA_PIXEL_FORMAT_NV12;
  out_frame->width = (uint32_t)avframe->width;
  out_frame->height = (uint32_t)avframe->height;
  out_frame->memory_kind = CRTMEDIA_GPU_MEMORY_GPU;
  out_frame->timestamp_us = avframe->pts != AV_NOPTS_VALUE ? avframe->pts : CRTMEDIA_FRAME_TIMESTAMP_NONE;
  out_frame->device_id = 0;
  out_frame->native_handle = &backing->handle;
  out_frame->plane_count = 0;
  out_frame->release = release_gpu_video_frame_vaapi;
  out_frame->release_context = backing;
  return CRTMEDIA_OK;
}
#endif

static void release_audio_buffer(crtmedia_audio_buffer* buffer, void* release_context) {
  (void)buffer;
  free(release_context);
}

static crtmedia_result fill_audio_buffer(crtmedia_codec* codec, AVFrame* avframe, crtmedia_audio_buffer* out_buffer) {
  int out_sample_size = codec->out_sample_format == CRTMEDIA_SAMPLE_FORMAT_FLT ? 4 : 2;
  int channels = codec->codec_ctx->ch_layout.nb_channels;
  int max_out_samples = (int)av_rescale_rnd(
      swr_get_delay(codec->swr_ctx, codec->codec_ctx->sample_rate) + avframe->nb_samples,
      codec->codec_ctx->sample_rate, codec->codec_ctx->sample_rate, AV_ROUND_UP);

  uint8_t* out_data = (uint8_t*)malloc((size_t)max_out_samples * channels * out_sample_size);
  if (out_data == NULL) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  uint8_t* out_planes[1] = {out_data};
  int converted =
      swr_convert(codec->swr_ctx, out_planes, max_out_samples, (const uint8_t**)avframe->data, avframe->nb_samples);
  if (converted < 0) {
    free(out_data);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  memset(out_buffer, 0, sizeof(*out_buffer));
  out_buffer->format = codec->out_sample_format;
  out_buffer->sample_rate = (uint32_t)codec->codec_ctx->sample_rate;
  out_buffer->channels = (uint32_t)channels;
  out_buffer->frame_count = (uint32_t)converted;
  out_buffer->data = out_data;
  out_buffer->timestamp_us = avframe->pts != AV_NOPTS_VALUE ? avframe->pts : CRTMEDIA_AUDIO_TIMESTAMP_NONE;
  out_buffer->release = release_audio_buffer;
  out_buffer->release_context = out_data;
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_codec_dequeue_output(
    crtmedia_codec* codec, crtmedia_frame* out_video_frame, crtmedia_audio_buffer* out_audio_buffer, int* out_eof) {
  if (codec == NULL || codec->is_encoder || out_eof == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  *out_eof = 0;
  if (codec->eof_drained) {
    *out_eof = 1;
    return CRTMEDIA_OK;
  }

  int ret = avcodec_receive_frame(codec->codec_ctx, codec->decode_frame);
  if (ret == AVERROR(EAGAIN)) {
    return CRTMEDIA_WOULD_BLOCK;
  }
  if (ret == AVERROR_EOF) {
    codec->eof_drained = 1;
    *out_eof = 1;
    return CRTMEDIA_OK;
  }
  if (ret < 0) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  if (codec->is_video) {
    if (out_video_frame != NULL) {
      /* Real hw-frame download (2026-09-08, "hardware decode, phase A"):
       * codec->decode_frame->format equals hw_pix_fmt exactly when this
       * frame is still hardware-resident (a real decoder can legitimately
       * decide not to use the offered hw format at all -- crtmedia_codec_
       * get_format()'s own comment -- so this check, not just "hw_pix_fmt
       * != NONE", is what actually decides whether a download is needed
       * this frame). av_hwframe_transfer_data() downloads to real CPU
       * memory (this pass's own explicit scope -- zero-copy interop is
       * phase B); av_frame_copy_props() carries pts/color metadata across
       * since the fresh sw frame starts with none of its own.
       *
       * Tranche 2 (2026-09-18): this is the one real point where the
       * public hardware_accelerated flag is allowed to become true --
       * closing Tranche 0's own gap 2 (it used to latch true as soon as
       * device creation succeeded, before any frame was ever observed).
       * hw_frame_observed latches the instant a hardware-resident frame is
       * actually seen, independent of whether the subsequent CPU transfer
       * below succeeds; hardware_accelerated latches only after that
       * transfer actually succeeds, matching crtmedia_codec_is_hardware_
       * accelerated()'s own public promise ("a real hardware-backed frame
       * was actually observed and downloaded", not merely offered). A
       * failed transfer returns the existing decode error without setting
       * either flag's own already-true value back to false -- both are
       * sticky for this decoder instance's whole lifetime once set. */
      if (codec->hw_pix_fmt != AV_PIX_FMT_NONE && codec->decode_frame->format == codec->hw_pix_fmt) {
        AVFrame* sw_frame = av_frame_alloc();
        codec->hw_frame_observed = 1;
        if (sw_frame == NULL || av_hwframe_transfer_data(sw_frame, codec->decode_frame, 0) < 0) {
          if (sw_frame != NULL) {
            av_frame_free(&sw_frame);
          }
          av_frame_unref(codec->decode_frame);
          return CRTMEDIA_ERROR_UNSUPPORTED;
        }
        codec->hardware_accelerated = 1;
        av_frame_copy_props(sw_frame, codec->decode_frame);
        fill_video_frame(sw_frame, out_video_frame);
      } else {
        AVFrame* owned = av_frame_alloc();
        av_frame_ref(owned, codec->decode_frame);
        fill_video_frame(owned, out_video_frame);
      }
    }
  } else {
    if (out_audio_buffer != NULL) {
      if (fill_audio_buffer(codec, codec->decode_frame, out_audio_buffer) != CRTMEDIA_OK) {
        av_frame_unref(codec->decode_frame);
        return CRTMEDIA_ERROR_UNSUPPORTED;
      }
    }
  }
  av_frame_unref(codec->decode_frame);
  return CRTMEDIA_OK;
}

/* crtmedia/codec.h's own top comment has the full frozen contract
 * (docs/crtmedia_zero_copy_decode_acceptance.md). Shares avcodec_
 * receive_frame()'s single decode-order source of truth with dequeue_
 * output() above -- the two are freely interchangeable frame-by-frame on
 * the same codec instance. */
crtmedia_result crtmedia_codec_dequeue_gpu_frame(
    crtmedia_codec* codec, crtmedia_gpu_frame* out_video_frame, crtmedia_audio_buffer* out_audio_buffer,
    int* out_eof) {
  if (codec == NULL || codec->is_encoder || out_eof == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  *out_eof = 0;
  if (codec->eof_drained) {
    *out_eof = 1;
    return CRTMEDIA_OK;
  }

  int ret = avcodec_receive_frame(codec->codec_ctx, codec->decode_frame);
  if (ret == AVERROR(EAGAIN)) {
    return CRTMEDIA_WOULD_BLOCK;
  }
  if (ret == AVERROR_EOF) {
    codec->eof_drained = 1;
    *out_eof = 1;
    return CRTMEDIA_OK;
  }
  if (ret < 0) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  if (codec->is_video) {
    if (out_video_frame != NULL) {
      if (codec->hw_pix_fmt != AV_PIX_FMT_NONE && codec->decode_frame->format == codec->hw_pix_fmt) {
        codec->hw_frame_observed = 1;
        if (codec->hw_pix_fmt == AV_PIX_FMT_VIDEOTOOLBOX) {
          /* Real zero-copy path (macOS today -- see fill_gpu_video_frame_
           * videotoolbox()'s own comment). av_frame_alloc()/av_frame_ref()
           * failure is real, checked recovery, not the existing dequeue_
           * output()'s own unchecked style: a failed ref here must never
           * leave a dangling native_handle observable by a caller. */
          AVFrame* owned = av_frame_alloc();
          if (owned == NULL || av_frame_ref(owned, codec->decode_frame) < 0) {
            if (owned != NULL) {
              av_frame_free(&owned);
            }
            av_frame_unref(codec->decode_frame);
            return CRTMEDIA_ERROR_UNSUPPORTED;
          }
          fill_gpu_video_frame_videotoolbox(owned, out_video_frame);
          codec->hardware_accelerated = 1;
          codec->hw_gpu_frame_delivered = 1;
#if defined(CRT_TARGET_OS_WINDOWS)
        } else if (codec->hw_pix_fmt == AV_PIX_FMT_D3D11) {
          /* GPU-resident D3D11VA handoff (Windows -- see fill_gpu_video_
           * frame_d3d11()). The downstream D3D11 -> D3D12 bridge performs
           * one GPU copy, so this must not be treated as end-to-end
           * zero-copy. */
          AVFrame* owned = av_frame_alloc();
          if (owned == NULL || av_frame_ref(owned, codec->decode_frame) < 0) {
            if (owned != NULL) {
              av_frame_free(&owned);
            }
            av_frame_unref(codec->decode_frame);
            return CRTMEDIA_ERROR_UNSUPPORTED;
          }
          if (fill_gpu_video_frame_d3d11(owned, out_video_frame) != CRTMEDIA_OK) {
            av_frame_free(&owned);
            av_frame_unref(codec->decode_frame);
            return CRTMEDIA_ERROR_UNSUPPORTED;
          }
          codec->hardware_accelerated = 1;
          codec->hw_gpu_frame_delivered = 1;
#endif
        } else {
#if defined(CRT_TARGET_OS_LINUX)
          if (codec->hw_pix_fmt == AV_PIX_FMT_VAAPI) {
            /* Real zero-copy path (Linux -- see fill_gpu_video_frame_vaapi()).
             * On any export failure fall through to the honest CPU download
             * below; hw_gpu_frame_delivered stays 0 for that frame. */
            AVFrame* owned = av_frame_alloc();
            if (owned != NULL && av_frame_ref(owned, codec->decode_frame) == 0) {
              if (fill_gpu_video_frame_vaapi(owned, out_video_frame) == CRTMEDIA_OK) {
                codec->hardware_accelerated = 1;
                codec->hw_gpu_frame_delivered = 1;
                av_frame_unref(codec->decode_frame);
                return CRTMEDIA_OK;
              }
            }
            if (owned != NULL) {
              av_frame_free(&owned);
            }
          }
#endif
          /* No GPU-frame handoff for this frame -- same real CPU download
           * dequeue_output()'s own hw branch already uses, so this API stays
           * fully usable everywhere. */
          AVFrame* sw_frame = av_frame_alloc();
          if (sw_frame == NULL || av_hwframe_transfer_data(sw_frame, codec->decode_frame, 0) < 0) {
            if (sw_frame != NULL) {
              av_frame_free(&sw_frame);
            }
            av_frame_unref(codec->decode_frame);
            return CRTMEDIA_ERROR_UNSUPPORTED;
          }
          codec->hardware_accelerated = 1;
          av_frame_copy_props(sw_frame, codec->decode_frame);
          fill_gpu_video_frame_cpu(sw_frame, out_video_frame);
        }
      } else {
        AVFrame* owned = av_frame_alloc();
        av_frame_ref(owned, codec->decode_frame);
        fill_gpu_video_frame_cpu(owned, out_video_frame);
      }
    }
  } else {
    if (out_audio_buffer != NULL) {
      if (fill_audio_buffer(codec, codec->decode_frame, out_audio_buffer) != CRTMEDIA_OK) {
        av_frame_unref(codec->decode_frame);
        return CRTMEDIA_ERROR_UNSUPPORTED;
      }
    }
  }
  av_frame_unref(codec->decode_frame);
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_codec_flush(crtmedia_codec* codec) {
  if (codec == NULL) {
    return CRTMEDIA_OK;
  }
  avcodec_flush_buffers(codec->codec_ctx);
  codec->eof_signaled = 0;
  codec->eof_drained = 0;
  codec->encode_pts_head = 0;
  codec->encode_pts_count = 0;
  /* Deliberately does not touch hardware_accelerated or any of the private
   * hw_* diagnostic fields above (Tranche 2): crtmedia_codec_is_hardware_
   * accelerated() answers whether this decoder instance has ever actually
   * used hardware, not whether the immediately-previous frame did -- a
   * seek-then-flush-then-resume within the same instance must not make an
   * already-true flag lie back to false. */
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_codec_is_hardware_accelerated(const crtmedia_codec* codec, int* out_is_hardware) {
  if (codec == NULL || codec->is_encoder || out_is_hardware == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  *out_is_hardware = codec->hardware_accelerated;
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_codec_test_get_hw_diagnostics(
    const crtmedia_codec* codec, crtmedia_codec_hw_diagnostics* out_diagnostics) {
  if (codec == NULL || out_diagnostics == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  out_diagnostics->hw_requested = codec->hw_requested;
  out_diagnostics->hw_device_created = codec->hw_device_created;
  out_diagnostics->hw_pixfmt_offered = codec->hw_pixfmt_offered;
  out_diagnostics->hw_frame_observed = codec->hw_frame_observed;
  out_diagnostics->hw_frame_transferred = codec->hardware_accelerated;
  out_diagnostics->hw_gpu_frame_delivered = codec->hw_gpu_frame_delivered;
  return CRTMEDIA_OK;
}
