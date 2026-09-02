/* crtmedia/audio_sink.h -- Windows backend, driven directly through real
 * WASAPI (`IMMDeviceEnumerator`/`IMMDevice`/`IAudioClient`/
 * `IAudioRenderClient`), shared-mode, default render endpoint.
 *
 * Deliberately does NOT #include <windows.h>/<mmdeviceapi.h>/<Audioclient.h>
 * (or any other host SDK header), matching libcrtgfx/src/arch/windows/
 * window_win32.c's own established convention exactly (see that file's own
 * top-of-vtable-section comment for the full reasoning): every Win32/COM
 * type used below is hand-declared, every real IID/CLSID value is a
 * mechanical transcription of the real SDK's own MIDL_INTERFACE(...)/
 * uuid(...) strings (um/mmdeviceapi.h, um/Audioclient.h,
 * 10.0.28000.0 -- read directly for this transcription, never included),
 * and every COM vtable below is declared only up to and including the last
 * method this file actually calls, with an anonymous `void* reserved[N]`
 * placeholder standing in for every earlier, unused real method -- only
 * the *count* of those matters for correct binary layout, not their
 * individual real signatures, since every vtable slot is a same-size
 * function pointer regardless of signature and this file never calls
 * through a reserved slot. This translation unit is compiled with only
 * libcrtmedia/include and libcrtmedia/src on its own include path (see
 * libcrtmedia/CMakeLists.txt's own crtmedia_backend_objects target,
 * mirroring crtgfx_backend_objects), not this project's own libc headers.
 */

#include "crtmedia/audio_sink.h"

#include <stddef.h>

#if defined(_M_IX86) || defined(__i386__)
#define CRTMEDIA_WINAPI __stdcall
#else
#define CRTMEDIA_WINAPI
#endif

typedef int BOOL;
typedef unsigned short WORD;
typedef unsigned int UINT;
typedef unsigned int UINT32;
typedef unsigned long DWORD;
typedef long HRESULT;
typedef unsigned long ULONG;
typedef long long LONGLONG;
typedef unsigned char BYTE;
typedef void* HANDLE;

typedef struct crtmedia_win_guid {
  uint32_t Data1;
  uint16_t Data2;
  uint16_t Data3;
  unsigned char Data4[8];
} GUID;
typedef GUID IID;
typedef GUID CLSID;
typedef const GUID* REFIID;
typedef const GUID* REFCLSID;
typedef GUID* LPGUID;

#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr) (((HRESULT)(hr)) < 0)

/* Real values (shared/wtypesbase.h, um/objbase.h, um/combaseapi.h,
 * um/AudioSessionTypes.h, shared/mmreg.h -- transcribed, not guessed). */
#define CRTMEDIA_CLSCTX_INPROC_SERVER 0x1
#define CRTMEDIA_COINIT_MULTITHREADED 0x0
#define CRTMEDIA_AUDCLNT_SHAREMODE_SHARED 0
#define CRTMEDIA_WAVE_FORMAT_PCM 0x0001
#define CRTMEDIA_WAVE_FORMAT_IEEE_FLOAT 0x0003
#define CRTMEDIA_E_RENDER 0  /* EDataFlow::eRender */
#define CRTMEDIA_E_CONSOLE 0 /* ERole::eConsole */

/* Real IIDs/CLSID (um/mmdeviceapi.h, um/Audioclient.h,
 * 10.0.28000.0 -- mechanically converted from each interface's own
 * MIDL_INTERFACE("...")/uuid(...) string). */
static const GUID crtmedia_clsid_mmdevice_enumerator = {
    0xbcde0395, 0xe52f, 0x467c, {0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e}};
static const GUID crtmedia_iid_immdevice_enumerator = {
    0xa95664d2, 0x9614, 0x4f35, {0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6}};
static const GUID crtmedia_iid_immdevice = {
    0xd666063f, 0x1587, 0x4e43, {0x81, 0xf1, 0xb9, 0x48, 0xe8, 0x07, 0x36, 0x3f}};
static const GUID crtmedia_iid_iaudio_client = {
    0x1cb9ad4c, 0xdbfa, 0x4c32, {0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2}};
static const GUID crtmedia_iid_iaudio_render_client = {
    0xf294acfc, 0x3146, 0x4483, {0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2}};

/* um/mmreg.h's own tWAVEFORMATEX, field-for-field. */
typedef struct crtmedia_win_waveformatex {
  WORD wFormatTag;
  WORD nChannels;
  DWORD nSamplesPerSec;
  DWORD nAvgBytesPerSec;
  WORD nBlockAlign;
  WORD wBitsPerSample;
  WORD cbSize;
} WAVEFORMATEX;

/* um/mmdeviceapi.h's own EDataFlow/ERole -- only the two real values this
 * file passes (eRender, eConsole; both happen to be integer 0) are ever
 * used, so no full enum type is declared, just the two DWORD-shaped
 * constants above (CRTMEDIA_E_RENDER/CRTMEDIA_E_CONSOLE). */

typedef struct crtmedia_immdevice_enumerator crtmedia_immdevice_enumerator;
typedef struct crtmedia_immdevice_enumerator_vtbl {
  HRESULT(CRTMEDIA_WINAPI* QueryInterface)(crtmedia_immdevice_enumerator* self, REFIID riid, void** out);
  ULONG(CRTMEDIA_WINAPI* AddRef)(crtmedia_immdevice_enumerator* self);
  ULONG(CRTMEDIA_WINAPI* Release)(crtmedia_immdevice_enumerator* self);
  void* reserved_3_enum_audio_endpoints;
  HRESULT(CRTMEDIA_WINAPI* GetDefaultAudioEndpoint)(
      crtmedia_immdevice_enumerator* self, DWORD data_flow, DWORD role, void** out_device);
} crtmedia_immdevice_enumerator_vtbl;
struct crtmedia_immdevice_enumerator {
  const crtmedia_immdevice_enumerator_vtbl* lpVtbl;
};

typedef struct crtmedia_immdevice crtmedia_immdevice;
typedef struct crtmedia_immdevice_vtbl {
  HRESULT(CRTMEDIA_WINAPI* QueryInterface)(crtmedia_immdevice* self, REFIID riid, void** out);
  ULONG(CRTMEDIA_WINAPI* AddRef)(crtmedia_immdevice* self);
  ULONG(CRTMEDIA_WINAPI* Release)(crtmedia_immdevice* self);
  HRESULT(CRTMEDIA_WINAPI* Activate)(
      crtmedia_immdevice* self, REFIID iid, DWORD cls_ctx, void* activation_params, void** out_interface);
} crtmedia_immdevice_vtbl;
struct crtmedia_immdevice {
  const crtmedia_immdevice_vtbl* lpVtbl;
};

typedef struct crtmedia_iaudio_client crtmedia_iaudio_client;
typedef struct crtmedia_iaudio_client_vtbl {
  HRESULT(CRTMEDIA_WINAPI* QueryInterface)(crtmedia_iaudio_client* self, REFIID riid, void** out);
  ULONG(CRTMEDIA_WINAPI* AddRef)(crtmedia_iaudio_client* self);
  ULONG(CRTMEDIA_WINAPI* Release)(crtmedia_iaudio_client* self);
  HRESULT(CRTMEDIA_WINAPI* Initialize)(
      crtmedia_iaudio_client* self, DWORD share_mode, DWORD stream_flags, LONGLONG buffer_duration,
      LONGLONG periodicity, const WAVEFORMATEX* format, const GUID* session_guid);
  HRESULT(CRTMEDIA_WINAPI* GetBufferSize)(crtmedia_iaudio_client* self, UINT32* out_frame_count);
  void* reserved_5_get_stream_latency;
  HRESULT(CRTMEDIA_WINAPI* GetCurrentPadding)(crtmedia_iaudio_client* self, UINT32* out_padding_frames);
  void* reserved_7_to_9[3]; /* IsFormatSupported, GetMixFormat, GetDevicePeriod */
  HRESULT(CRTMEDIA_WINAPI* Start)(crtmedia_iaudio_client* self);
  HRESULT(CRTMEDIA_WINAPI* Stop)(crtmedia_iaudio_client* self);
  void* reserved_12_to_13[2]; /* Reset, SetEventHandle */
  HRESULT(CRTMEDIA_WINAPI* GetService)(crtmedia_iaudio_client* self, REFIID riid, void** out_service);
} crtmedia_iaudio_client_vtbl;
struct crtmedia_iaudio_client {
  const crtmedia_iaudio_client_vtbl* lpVtbl;
};

typedef struct crtmedia_iaudio_render_client crtmedia_iaudio_render_client;
typedef struct crtmedia_iaudio_render_client_vtbl {
  HRESULT(CRTMEDIA_WINAPI* QueryInterface)(crtmedia_iaudio_render_client* self, REFIID riid, void** out);
  ULONG(CRTMEDIA_WINAPI* AddRef)(crtmedia_iaudio_render_client* self);
  ULONG(CRTMEDIA_WINAPI* Release)(crtmedia_iaudio_render_client* self);
  HRESULT(CRTMEDIA_WINAPI* GetBuffer)(crtmedia_iaudio_render_client* self, UINT32 frames_requested, BYTE** out_data);
  HRESULT(CRTMEDIA_WINAPI* ReleaseBuffer)(
      crtmedia_iaudio_render_client* self, UINT32 frames_written, DWORD flags);
} crtmedia_iaudio_render_client_vtbl;
struct crtmedia_iaudio_render_client {
  const crtmedia_iaudio_render_client_vtbl* lpVtbl;
};

/* Real, flat dllimport declarations (ole32.dll/kernel32.dll) -- same style
 * as window_win32.c's own kernel32/user32/gdi32 declarations just above
 * that file's own vtable section. */
__declspec(dllimport) HRESULT CRTMEDIA_WINAPI CoInitializeEx(void* reserved, DWORD coinit);
__declspec(dllimport) HRESULT CRTMEDIA_WINAPI CoCreateInstance(
    REFCLSID clsid, void* outer, DWORD cls_ctx, REFIID riid, void** out);
__declspec(dllimport) void CRTMEDIA_WINAPI CoUninitialize(void);
__declspec(dllimport) void CRTMEDIA_WINAPI Sleep(DWORD milliseconds);
/* No libc on this translation unit's own include path (see this file's own
 * top comment) -- GetProcessHeap()/HeapAlloc()/HeapFree() stand in for
 * malloc()/free(), matching window_win32.c's own identical precedent. */
__declspec(dllimport) HANDLE CRTMEDIA_WINAPI GetProcessHeap(void);
__declspec(dllimport) void* CRTMEDIA_WINAPI HeapAlloc(HANDLE heap, DWORD flags, size_t bytes);
__declspec(dllimport) BOOL CRTMEDIA_WINAPI HeapFree(HANDLE heap, DWORD flags, void* mem);

struct crtmedia_audio_sink {
  crtmedia_iaudio_client* audio_client;
  crtmedia_iaudio_render_client* render_client;
  UINT32 buffer_frame_count;
  UINT32 block_align;
  uint64_t frames_written_total;
  int com_initialized; /* 1 if this sink's own CoInitializeEx() call was the
                         * one that must be balanced by CoUninitialize() --
                         * S_FALSE (already initialized by someone else on
                         * this thread) still counts, matching real COM
                         * ref-counted init/uninit balancing rules. */
};

static void crtmedia_copy_bytes(void* dst, const void* src, size_t count) {
  unsigned char* d = (unsigned char*)dst;
  const unsigned char* s = (const unsigned char*)src;
  size_t i;
  for (i = 0; i < count; ++i) {
    d[i] = s[i];
  }
}

crtmedia_result crtmedia_audio_sink_open(const crtmedia_audio_sink_desc* desc, crtmedia_audio_sink** out_sink) {
  if (desc == NULL || out_sink == NULL || desc->sample_rate == 0 || desc->channels == 0) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  WORD bits_per_sample;
  WORD format_tag;
  if (desc->format == CRTMEDIA_SAMPLE_FORMAT_S16) {
    bits_per_sample = 16;
    format_tag = (WORD)CRTMEDIA_WAVE_FORMAT_PCM;
  } else if (desc->format == CRTMEDIA_SAMPLE_FORMAT_FLT) {
    bits_per_sample = 32;
    format_tag = (WORD)CRTMEDIA_WAVE_FORMAT_IEEE_FLOAT;
  } else {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }

  HRESULT hr = CoInitializeEx(NULL, CRTMEDIA_COINIT_MULTITHREADED);
  /* S_OK (0) or S_FALSE (1) both mean this thread's COM apartment is now
   * usable; only a real negative HRESULT (e.g. RPC_E_CHANGED_MODE if some
   * other component on this same thread already chose apartment-threaded)
   * is a real failure worth degrading gracefully for. */
  if (FAILED(hr)) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  int com_initialized = 1;

  crtmedia_immdevice_enumerator* enumerator = NULL;
  hr = CoCreateInstance(
      &crtmedia_clsid_mmdevice_enumerator, NULL, CRTMEDIA_CLSCTX_INPROC_SERVER, &crtmedia_iid_immdevice_enumerator,
      (void**)&enumerator);
  if (FAILED(hr) || enumerator == NULL) {
    if (com_initialized) {
      CoUninitialize();
    }
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  crtmedia_immdevice* device = NULL;
  hr = enumerator->lpVtbl->GetDefaultAudioEndpoint(enumerator, CRTMEDIA_E_RENDER, CRTMEDIA_E_CONSOLE, (void**)&device);
  enumerator->lpVtbl->Release(enumerator);
  if (FAILED(hr) || device == NULL) {
    CoUninitialize();
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  crtmedia_iaudio_client* audio_client = NULL;
  hr = device->lpVtbl->Activate(
      device, &crtmedia_iid_iaudio_client, CRTMEDIA_CLSCTX_INPROC_SERVER, NULL, (void**)&audio_client);
  device->lpVtbl->Release(device);
  if (FAILED(hr) || audio_client == NULL) {
    CoUninitialize();
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  WAVEFORMATEX wfx;
  wfx.wFormatTag = format_tag;
  wfx.nChannels = (WORD)desc->channels;
  wfx.nSamplesPerSec = (DWORD)desc->sample_rate;
  wfx.wBitsPerSample = bits_per_sample;
  wfx.nBlockAlign = (WORD)(desc->channels * (bits_per_sample / 8));
  wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
  wfx.cbSize = 0;

  /* 3,000,000 hundred-nanosecond units == 300ms -- a real, deliberate,
   * generous shared-mode buffer (WASAPI's own shared-mode engine period is
   * typically ~10ms; 300ms just bounds how much real audio this sink can
   * ever have in flight at once, trading a little latency for headroom
   * against real scheduler jitter in crtmedia_audio_sink_write()'s own
   * caller). hnsPeriodicity == 0 is the documented "let the shared-mode
   * engine pick" value. */
  hr = audio_client->lpVtbl->Initialize(
      audio_client, CRTMEDIA_AUDCLNT_SHAREMODE_SHARED, 0, 3000000, 0, &wfx, NULL);
  if (FAILED(hr)) {
    audio_client->lpVtbl->Release(audio_client);
    CoUninitialize();
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  UINT32 buffer_frame_count = 0;
  hr = audio_client->lpVtbl->GetBufferSize(audio_client, &buffer_frame_count);
  if (FAILED(hr)) {
    audio_client->lpVtbl->Release(audio_client);
    CoUninitialize();
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  crtmedia_iaudio_render_client* render_client = NULL;
  hr = audio_client->lpVtbl->GetService(audio_client, &crtmedia_iid_iaudio_render_client, (void**)&render_client);
  if (FAILED(hr) || render_client == NULL) {
    audio_client->lpVtbl->Release(audio_client);
    CoUninitialize();
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  hr = audio_client->lpVtbl->Start(audio_client);
  if (FAILED(hr)) {
    render_client->lpVtbl->Release(render_client);
    audio_client->lpVtbl->Release(audio_client);
    CoUninitialize();
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  crtmedia_audio_sink* sink =
      (crtmedia_audio_sink*)HeapAlloc(GetProcessHeap(), 0, sizeof(crtmedia_audio_sink));
  if (sink == NULL) {
    audio_client->lpVtbl->Stop(audio_client);
    render_client->lpVtbl->Release(render_client);
    audio_client->lpVtbl->Release(audio_client);
    CoUninitialize();
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  sink->audio_client = audio_client;
  sink->render_client = render_client;
  sink->buffer_frame_count = buffer_frame_count;
  sink->block_align = wfx.nBlockAlign;
  sink->frames_written_total = 0;
  sink->com_initialized = com_initialized;
  *out_sink = sink;
  return CRTMEDIA_OK;
}

void crtmedia_audio_sink_close(crtmedia_audio_sink* sink) {
  if (sink == NULL) {
    return;
  }
  /* A real drain: block until every frame already handed to
   * crtmedia_audio_sink_write() has actually, audibly played (this file's
   * own documented crtmedia_audio_sink_close() contract) -- polls the same
   * real GetCurrentPadding() crtmedia_audio_sink_get_position_frames() and
   * crtmedia_audio_sink_write() both already use, not a fixed sleep. */
  UINT32 padding = 0;
  while (SUCCEEDED(sink->audio_client->lpVtbl->GetCurrentPadding(sink->audio_client, &padding)) && padding > 0) {
    Sleep(1);
  }
  sink->audio_client->lpVtbl->Stop(sink->audio_client);
  sink->render_client->lpVtbl->Release(sink->render_client);
  sink->audio_client->lpVtbl->Release(sink->audio_client);
  if (sink->com_initialized) {
    CoUninitialize();
  }
  HeapFree(GetProcessHeap(), 0, sink);
}

int64_t crtmedia_audio_sink_write(crtmedia_audio_sink* sink, const void* data, uint32_t frame_count) {
  if (sink == NULL || data == NULL) {
    return (int64_t)CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  if (frame_count == 0) {
    return 0;
  }

  UINT32 padding = 0;
  HRESULT hr = sink->audio_client->lpVtbl->GetCurrentPadding(sink->audio_client, &padding);
  if (FAILED(hr)) {
    return (int64_t)CRTMEDIA_ERROR_UNSUPPORTED;
  }
  /* Real, genuine backpressure (this file's own documented "blocking until
   * the host device has real room for at least one full frame" contract) --
   * paced by real playback time via GetCurrentPadding() draining as the
   * device actually consumes already-queued audio, not merely an internal
   * buffer-full check. */
  while (padding >= sink->buffer_frame_count) {
    Sleep(1);
    hr = sink->audio_client->lpVtbl->GetCurrentPadding(sink->audio_client, &padding);
    if (FAILED(hr)) {
      return (int64_t)CRTMEDIA_ERROR_UNSUPPORTED;
    }
  }

  UINT32 available = sink->buffer_frame_count - padding;
  UINT32 to_write = (frame_count < available) ? frame_count : available;

  BYTE* device_buffer = NULL;
  hr = sink->render_client->lpVtbl->GetBuffer(sink->render_client, to_write, &device_buffer);
  if (FAILED(hr) || device_buffer == NULL) {
    return (int64_t)CRTMEDIA_ERROR_UNSUPPORTED;
  }
  crtmedia_copy_bytes(device_buffer, data, (size_t)to_write * (size_t)sink->block_align);
  hr = sink->render_client->lpVtbl->ReleaseBuffer(sink->render_client, to_write, 0);
  if (FAILED(hr)) {
    return (int64_t)CRTMEDIA_ERROR_UNSUPPORTED;
  }

  sink->frames_written_total += to_write;
  return (int64_t)to_write;
}

crtmedia_result crtmedia_audio_sink_get_position_frames(const crtmedia_audio_sink* sink, uint64_t* out_frames) {
  if (sink == NULL || out_frames == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  UINT32 padding = 0;
  HRESULT hr = sink->audio_client->lpVtbl->GetCurrentPadding(sink->audio_client, &padding);
  if (FAILED(hr)) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  /* Frames actually, audibly reached so far == everything ever handed to
   * crtmedia_audio_sink_write() minus whatever is still queued ahead of
   * the real playback position right now (this file's own documented
   * "distinct from how many frames have been written" contract). */
  uint64_t queued = (uint64_t)padding;
  *out_frames = (sink->frames_written_total >= queued) ? (sink->frames_written_total - queued) : 0;
  return CRTMEDIA_OK;
}
