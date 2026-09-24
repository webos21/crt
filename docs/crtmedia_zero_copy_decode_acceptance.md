# Zero-Copy Decoded Textures Acceptance Contract

This document freezes TODO.md's "Zero-copy decoded textures" tranche's scope,
ownership/lifetime model, and per-host contract before implementation work
past Tranche 1 continues, matching the precedent set by
`docs/crtmedia_hardware_decode_acceptance.md` for the tranche this one
follows directly. Nothing in this document changes any existing public ABI;
it records the real structures already in the tree, the additive surface this
tranche adds, and the exact contract each host's work closes against.

## Scope

In scope for this tranche:

- Connecting an already-decoded hardware surface (`CVPixelBuffer` on macOS,
  `ID3D11Texture2D` on Windows, a VA-API surface on Linux) to the native
  graphics path (`libcrtgfx`/Skia) without a mandatory CPU readback, on hosts
  that support it.
- A measured, explicit copy fallback where true zero-copy interop is not
  available (driver, hardware, or FFmpeg build limitation) -- distinguished
  from zero-copy, never silently reported as it.
- One new `libcrtmedia` API, `crtmedia_codec_dequeue_gpu_frame()`, additive
  next to the existing `crtmedia_codec_dequeue_output()`.
- One new, optional bridge component owned jointly by `libcrtgfx` and
  `libcrtmedia`'s public headers (see "Library boundary" below), producing a
  real GPU-backed `SkImage` from a `crtmedia_gpu_frame`.

Explicitly out of scope, deferred to a later tranche or roadmap item:

- Any change to `crtmedia_frame`, `crtmedia_codec_dequeue_output()`, or the
  `CRTMEDIA_FORMAT_KEY_PREFER_HARDWARE_DECODE` opt-in. This tranche is
  strictly additive next to that existing, accepted contract.
- Any public ABI change to `crtmedia_gpu_frame` (`crtmedia/gpu_frame.h`) --
  its layout is frozen as of the "Hardware video decode" tranche; this
  tranche only gives real meaning to fields that were previously always
  zero/NULL.
- Color-range/color-space metadata on `crtmedia_gpu_frame` itself (it has no
  such fields today, unlike `crtmedia_frame`). A real external consumer of
  `crtmedia_gpu_frame` as a general-purpose interchange type would justify
  adding them; none exists yet, matching this project's own "add a field
  when a real consumer needs it" precedent (`crtmedia/frame.h`'s own 10/12-bit
  pixel-format discussion).
- A `crtmedia`→`libcrtgfx` build dependency in either direction. Still not
  introduced; see "Library boundary" below.
- Device-affinity pairing between FFmpeg's own hardware-device choice and
  `crtgfx_gpu_device`'s own choice. Real on multi-GPU Windows and Linux, not
  yet observed as a problem on Apple Silicon (effectively one GPU); addressed
  before the Windows tranche closes, not before.
- Any GPU-fence-based cross-API synchronization through `crtgfx_gpu_fence`
  (`crtgfx/gpu.h`). That type is documented as CPU synchronization only; a
  real device-affine GPU fence contract, if one is ever needed, is separate
  future work, not a reuse of that type.
- RGBA intermediate conversion textures. The accepted path samples the real
  NV12 (or platform-equivalent biplanar) surface directly as YUV in Skia; an
  extra GPU copy/conversion step is exactly the "GPU-copy fallback" case
  above, not true zero-copy, and must be measured and reported as such if it
  is ever the best available path on some host.
- Hardware encode, capture, and every item already listed out of scope by
  `docs/crtmedia_hardware_decode_acceptance.md`.

## Library boundary (frozen)

`libcrtmedia` has no build dependency on `libcrtgfx` today and this tranche
does not add one. The interop point is a **third, optional bridge component**
that depends on both public header trees plus Skia, mirroring `libcrtgfx`'s
own existing `crtgfx_skia*` shared-target split (`libcrtgfx/include/crtgfx/
skia.h`, gated behind `CRTGFX_HAVE_VULKAN`/`_D3D12`/`_METAL`):

```
libcrtmedia (public headers only)      libcrtgfx (public headers + Skia)
              \                                /
               \                              /
                crtgfx_skia_media bridge (new)
```

First version's public surface is one function:

```c
sk_sp<SkImage> crtgfx_skia_import_media_frame(
    GrDirectContext* context,
    const crtgfx_gpu_device* device,
    crtmedia_gpu_frame* frame);
```

Ownership rule: on success, the bridge takes ownership of `*frame` (moves it
into the returned `SkImage`'s own release context, calling
`crtmedia_gpu_frame_release()` only once that `SkImage`'s GPU resource is
actually destroyed); on failure, the caller still owns `frame` and must
release it itself. This mirrors `crtmedia_frame`/`crtmedia_gpu_frame`'s own
existing single-owner release convention and Skia's own
backend-texture-plus-release-callback shape (`SkImages::TextureFromYUVA...`,
`GrYUVABackendTextures`).

## What `native_handle` really is, per host, when `memory_kind == GPU`

`crtmedia_gpu_frame.native_handle` (`crtmedia/gpu_frame.h`) stays a
type-erased `void*` in the public struct -- no host SDK type appears in that
header, unchanged. This tranche fixes the exact real identity behind it, per
host, as an interop contract between `libcrtmedia`'s producer and
`crtgfx_skia_media`'s consumer (both project-owned, both aware of this
document -- a generic caller must still treat it as opaque):

| Host | Real identity | Lifetime |
| --- | --- | --- |
| macOS | `CVPixelBufferRef` (`avframe->data[3]` on the retained hardware `AVFrame`) | Exactly `frame`'s own lifetime -- not independently retained; the retained `AVFrame` in `release_context` is what actually keeps it alive. |
| Windows | `crtmedia_d3d11_gpu_frame_handle*` (`libcrtmedia/src/gpu_frame_d3d11.h`, private/non-installed) -- a small crtmedia-owned indirection, not a bare `ID3D11Texture2D*` (confirmed necessary while implementing this tranche: `hwcontext_d3d11va.c`'s own real decode output is one array-slice of a shared decode-pool array texture, `frame->data[0]` the `ID3D11Texture2D*` and `frame->data[1]` the array index -- a single pointer cannot carry both). Fields: `void* texture` (the `ID3D11Texture2D*`), `int64_t array_index`. | Exactly `frame`'s own lifetime, same shape as macOS -- the retained `AVFrame` in `release_context` keeps the underlying decode-pool slice alive; the indirection struct itself is heap-allocated alongside that `AVFrame` and freed by the same release function. |
| Linux | `crtmedia_vaapi_gpu_frame_handle*` (`libcrtmedia/src/gpu_frame_vaapi.h`, private/non-installed) -- a crtmedia-owned copy of the real `VADRMPRIMESurfaceDescriptor` returned by `vaExportSurfaceHandle(..., DRM_PRIME_2, READ_ONLY | SEPARATE_LAYERS, ...)`, including the dma-buf objects, DRM modifiers, and the R8/GR88 layer layouts needed to represent NV12. | Exactly `frame`'s own lifetime. The retained `AVFrame` keeps the VA surface/pool slot alive; the indirection owns the exported dma-buf fds, and the same release callback closes those fds before freeing the retained frame. A Vulkan importer duplicates each fd because successful `vkAllocateMemory()` consumes the duplicate. |

On every host, the true owner of the underlying decoder resource is the
retained FFmpeg `AVFrame` kept alive in `frame->release_context`.
`frame->release` frees that frame and any small host indirection/exported
handles owned alongside it. This makes the full chain, on every host, the
same shape (Section-6-style lifetime diagram):

```
decoder hardware surface
      ^ owns
retained AVFrame  (frame->release_context)
      ^ owns
crtmedia_gpu_frame        (frame->native_handle points at the surface above)
      ^ moved into, on success
SkImage release context   (crtgfx_skia_media, real per-host imported texture(s))
      ^
SkImage
```

`SkImage` lifetime >= imported GPU resource lifetime >= decoder surface
lifetime, by construction: nothing above is released until the layer above
it has already released its own reference.

## `crtmedia_codec_dequeue_gpu_frame()` (frozen)

```c
crtmedia_result crtmedia_codec_dequeue_gpu_frame(
    crtmedia_codec* codec, crtmedia_gpu_frame* out_video_frame,
    crtmedia_audio_buffer* out_audio_buffer, int* out_eof);
```

Same calling convention as `crtmedia_codec_dequeue_output()` (matching
argument shapes, `CRTMEDIA_WOULD_BLOCK`/EOF behavior) so a caller already
familiar with that function needs no new mental model. The two are freely
interchangeable frame-by-frame on the same `crtmedia_codec` instance --
each only decides how the *next* already-decoded frame is packaged, not
which frame comes next (`avcodec_receive_frame()` itself is the single
source of truth for stream order either way).

Per decoded video frame, this new function fills `*out_video_frame` as:

| Decoder result | `memory_kind` | `native_handle` | `plane_count` |
| --- | --- | --- | --- |
| Real hardware surface, real zero-copy path implemented for this host | `CRTMEDIA_GPU_MEMORY_GPU` | Real per-host handle (table above) | 0 |
| Real hardware surface, zero-copy not yet implemented for this host | `CRTMEDIA_GPU_MEMORY_CPU` | `NULL` | 2 or 3 (real pixel data, `av_hwframe_transfer_data()` fallback) |
| Software-decoded frame | `CRTMEDIA_GPU_MEMORY_CPU` | `NULL` | 2 or 3 (real pixel data) |

A caller always checks `memory_kind` before touching `native_handle`,
exactly like the existing `crtmedia_gpu_frame` contract already documents;
this table is what makes "hardware decode" and "zero-copy" two independently
observable, honestly-reported outcomes instead of one flag conflated with the
other.

## `crtmedia_codec_is_hardware_accelerated()` (broadened, not redefined)

The existing public promise -- "a real hardware-backed frame was actually
observed and successfully delivered to the caller" -- already covers this
tranche's new delivery path without changing its wording's *intent*, only
correcting an implicit assumption in its previous phrasing ("...and
transferred to CPU memory"): CPU transfer was, until now, the only delivery
path that existed. This tranche adds a second successful-delivery path (the
zero-copy GPU handoff) and the flag becomes true from either one. A caller
using only `crtmedia_codec_dequeue_output()` observes byte-identical
behavior to before -- that path's own success condition is unchanged.

The private diagnostic surface (`libcrtmedia/src/codec_test_control.h`)
gains one new sticky field, `hw_zero_copy_delivered`, additive next to the
existing six-state model, so a test can tell *which* delivery path actually
set the public flag without inferring it from which dequeue function the
test itself happened to call.

## Acceptance host order

`macOS/arm64 → Windows/x64 → Linux`, matching the "Hardware video decode"
tranche's own order and reasoning (Apple Silicon's simple, effectively
single-GPU topology first; Windows' multi-GPU device-affinity question and
Linux's DRM PRIME/dma-buf/Vulkan-version question both deferred past the
first green).

## Per-tranche acceptance gate

Each host tranche is accepted when:

- `crtmedia_codec_dequeue_gpu_frame()` returns `memory_kind ==
  CRTMEDIA_GPU_MEMORY_GPU` for a real hardware-backed frame on this host,
  with `native_handle` non-NULL and `plane_count == 0`.
- `crtgfx_skia_import_media_frame()` returns a non-null, GPU-backed `SkImage`
  (`isTextureBacked() == true`) built from that frame, with no intermediate
  CPU-readback step in the code path that produced it (a structural
  guarantee: the code path taken never calls `av_hwframe_transfer_data()` nor
  any CPU pixel copy of the video plane data).
- That `SkImage` presents through the existing, unchanged Ganesh/`crtgfx_gpu_surface` path, with the same resize-and-pixel-check discipline
  `docs/libcrtgfx_live_presentation_acceptance.md` already established for
  every other Ganesh acceptance case on this host.
- The existing CPU-resident path (`crtmedia_codec_dequeue_output()` and every
  existing hardware-decode test) remains green, unchanged, on the same host.
- Software-only decode remains green through both dequeue functions.
- A decoder instance that never observes a real hardware frame through
  `dequeue_gpu_frame()` reports `memory_kind == CRTMEDIA_GPU_MEMORY_GPU`
  never, and `crtmedia_codec_is_hardware_accelerated()` stays false for its
  whole lifetime, exactly like today.

## Results

| Host | Backend | Status | Evidence |
| --- | --- | --- | --- |
| macOS/arm64 | VideoToolbox `CVPixelBuffer` → `CVMetalTextureCache` → Metal Y/UV textures → `GrYUVABackendTextures` → `SkImage` | See `HISTORY.md` | |
| Windows/x64 | D3D11VA `ID3D11Texture2D` → plane-sliced `ID3D11ShaderResourceView1` → compute-shader copy → NT-handle-shared D3D12 resource → `GrYUVABackendTextures` → `SkImage` (measured GPU-copy fallback, not literal zero-copy -- D3D12/Skia has no multi-plane concept) | See `HISTORY.md` | |
| Linux/x64 | VA-API `VASurfaceID` → `vaSyncSurface()` → `VADRMPRIMESurfaceDescriptor` (`DRM_PRIME_2`, separate R8/GR88 layers) → two directly imported DRM-modifier Vulkan `VkImage`s → `GrYUVABackendTextures` → `SkImage` | Closed 2026-09-24 | Intel iHD VA-API driver + Intel UHD 630/Mesa Vulkan: all 25 real fixture frames presented repeatedly through Wayland with `pixel_check=pass`, `zero_copy=yes`, and the scripted resize passing. See `HISTORY.md`. |

### Linux mapping decision

Linux uses direct DRM PRIME/dma-buf import rather than FFmpeg's Vulkan frame
mapping. The pinned FFmpeg 8.1.2 recipe enables only the narrow VA-API H.264
decode path; its DRM and Vulkan hwcontexts are deliberately not part of the
produced archives. Enabling them only to transport an already-decoded frame
would add libdrm/Vulkan ownership and FFmpeg's runtime `dlopen("libvulkan")`
path to `libcrtmedia`, while this project deliberately keeps graphics-device
ownership in `libcrtgfx` and does not yet provide a general ELF `dlopen()`
backend. Direct export keeps the boundary narrow: `libcrtmedia` owns VA-API
and the exported descriptor, while the optional bridge imports that descriptor
into the already-selected `crtgfx_gpu_device`.

The Vulkan device enables the dma-buf import extension set only when all of
`VK_KHR_external_memory_fd`, `VK_EXT_external_memory_dma_buf`,
`VK_EXT_image_drm_format_modifier`, `VK_KHR_image_format_list`, and
`VK_EXT_queue_family_foreign` are supported. Otherwise the bridge declines the
import. `vaSyncSurface()` is a CPU wait but not a pixel copy; after it returns,
the two images sample the decoder-owned memory directly. The first Vulkan use
acquires ownership from `VK_QUEUE_FAMILY_FOREIGN_EXT`, and Skia's release
callback destroys both imported images/memory allocations before releasing
the retained media frame.
