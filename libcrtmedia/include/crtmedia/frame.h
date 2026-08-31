#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* CPU video frame handoff contract (TODO.md's "Define the libcrtmedia CPU
 * frame handoff contract" item, the gate before libcrtmedia itself starts
 * decoding anything real). This is deliberately the same shape a future
 * FFmpeg software-decode path will need to fill in (AVFrame's own plane
 * pointers/linesize/format/color metadata map onto this directly) and the
 * same shape libcrtgfx's Skia bridge needs to consume on the other end --
 * see crtmedia_frame_convert_to_rgba() below and tests/frame_skia_smoke.cc
 * for the CPU-only proof that a synthetic frame in each of the three
 * formats below round-trips into a real Skia SkImage/SkSurface.
 *
 * GPU texture handoff is deliberately out of scope here -- see this
 * project's own docs/libcrtgfx_api_policy.md "Open Questions": CPU pixel
 * buffer first, GPU texture interop after the host GPU backend is stable.
 */

typedef enum crtmedia_result {
  CRTMEDIA_OK = 0,
  CRTMEDIA_ERROR_INVALID_ARGUMENT = -1,
  CRTMEDIA_ERROR_UNSUPPORTED = -2,
} crtmedia_result;

/* Every format this contract currently covers: two packed 8-bit-per-
 * channel RGB layouts (the two byte orders real hosts/GPU APIs actually
 * use -- RGBA8888 is OpenGL/most software's own default, BGRA8888 matches
 * libcrtgfx's own crtgfx_framebuffer convention, see crtgfx/window.h),
 * plus one planar YUV layout (I420/YUV420P, 8-bit 4:2:0 -- FFmpeg's own
 * default software-decode output for the overwhelming majority of real
 * H.264/HEVC content). Deliberately not NV12 or any 10/12-bit format yet:
 * nothing in this codebase produces or consumes one today, and this
 * project's own established convention (see docs/libcrtgfx_api_policy.md's
 * window.h split discussion) is to add a real field/value once a real
 * consumer needs it, not speculatively ahead of one. */
typedef enum crtmedia_pixel_format {
  CRTMEDIA_PIXEL_FORMAT_RGBA8888 = 1,
  CRTMEDIA_PIXEL_FORMAT_BGRA8888 = 2,
  CRTMEDIA_PIXEL_FORMAT_YUV420P = 3,
} crtmedia_pixel_format;

/* Luma quantization range. Only meaningful for a YUV frame; a packed RGB
 * frame should leave this CRTMEDIA_COLOR_RANGE_UNSPECIFIED (0, the zero-
 * initialized default) since it does not apply. LIMITED (16-235 luma,
 * 16-240 chroma -- the "TV"/broadcast range) is the de facto default for
 * H.264/HEVC content that does not signal otherwise; FULL (0-255) is
 * common for screen-capture/synthetic content. */
typedef enum crtmedia_color_range {
  CRTMEDIA_COLOR_RANGE_UNSPECIFIED = 0,
  CRTMEDIA_COLOR_RANGE_LIMITED = 1,
  CRTMEDIA_COLOR_RANGE_FULL = 2,
} crtmedia_color_range;

/* YCbCr <-> RGB conversion matrix. Only meaningful for a YUV frame; see
 * crtmedia_color_range's own comment for the packed-RGB case. BT.601 is
 * the SD (<=576 lines) default, BT.709 the HD default and by far the most
 * common real-world value, BT.2020 the UHD/HDR default. crtmedia_frame_
 * convert_to_rgba() below derives its own conversion coefficients from
 * this value's real ITU-R Kr/Kb luma constants rather than hand-picking
 * three separate hardcoded coefficient sets, so adding a fourth space
 * later only needs one more (kr, kb) pair. */
typedef enum crtmedia_color_space {
  CRTMEDIA_COLOR_SPACE_UNSPECIFIED = 0,
  CRTMEDIA_COLOR_SPACE_BT601 = 1,
  CRTMEDIA_COLOR_SPACE_BT709 = 2,
  CRTMEDIA_COLOR_SPACE_BT2020 = 3,
} crtmedia_color_space;

/* No frame this contract describes needs more than 3 planes (YUV420P's Y/
 * U/V) today. Sized for one more (e.g. a future alpha plane, or NV12's
 * interleaved-chroma 2-plane layout) without another ABI break; grow this
 * only when a real format that actually needs it lands, matching this
 * file's own established "no speculative fields" convention. */
#define CRTMEDIA_FRAME_MAX_PLANES 4

/* Sentinel for crtmedia_frame::timestamp_us when a frame carries no
 * meaningful presentation timestamp (e.g. a synthetic test frame, or a
 * still image). INT64_MIN rather than 0 or -1: 0 is a completely valid
 * real timestamp (the first frame of a stream), and -1 is FFmpeg's own
 * AV_NOPTS_VALUE low 32 bits on a 32-bit build in some historical
 * toolchains -- INT64_MIN is unambiguous and cannot collide with any real
 * elapsed-microseconds value a decoder would ever produce. */
#define CRTMEDIA_FRAME_TIMESTAMP_NONE INT64_MIN

/* One plane of pixel data. For a packed format (RGBA8888/BGRA8888) a
 * frame has exactly one plane, sized to the frame's own full width/
 * height. For a planar YUV format each plane may be smaller than the
 * frame's own width/height (chroma subsampling) -- width/height here are
 * this *plane's own* sample dimensions, not the frame's, matching what
 * crtmedia_frame_describe_planes() below fills in and what a decoder's
 * own per-plane linesize/dimensions naturally are. `stride` is real
 * bytes per row (may exceed width * bytes-per-sample for row-alignment
 * padding a decoder applied); every plane walk in this codebase goes
 * through `stride`, never a width-derived row size, for exactly that
 * reason. */
typedef struct crtmedia_frame_plane {
  void* data;
  uint32_t stride;
  uint32_t width;
  uint32_t height;
} crtmedia_frame_plane;

typedef struct crtmedia_frame crtmedia_frame;

/* Called exactly once by crtmedia_frame_release() (below) to free
 * whatever backing storage this frame's own planes point into. Receives
 * the frame being released (not yet zeroed) and `release_context`
 * (crtmedia_frame::release_context, opaque to this contract -- e.g. a
 * decoder's own reference-counted buffer handle). */
typedef void (*crtmedia_frame_release_fn)(crtmedia_frame* frame, void* release_context);

/* A single CPU video frame. Ownership: `release`, if non-NULL, is called
 * exactly once by crtmedia_frame_release() to free every plane's `data`
 * (and any other backing storage this frame owns). A frame with
 * `release == NULL` is a non-owning *view* over caller-managed storage
 * (e.g. a stack/static buffer in a test, or a decoder's own frame pool
 * entry the caller is not meant to free) -- crtmedia_frame_release() is
 * then a no-op past zeroing the struct's own contents. This mirrors
 * FFmpeg's own AVFrame/AVBufferRef reference-counting spirit without
 * importing its actual refcounting machinery: exactly one owner decides
 * how (or whether) to free the pixel storage, decided once at frame
 * construction time, not by any shared refcount this contract tracks. */
struct crtmedia_frame {
  crtmedia_pixel_format format;
  uint32_t width;
  uint32_t height;
  crtmedia_color_range color_range;
  crtmedia_color_space color_space;
  int64_t timestamp_us;
  uint32_t plane_count;
  crtmedia_frame_plane planes[CRTMEDIA_FRAME_MAX_PLANES];
  crtmedia_frame_release_fn release;
  void* release_context;
};

/* Fills in `out_planes[0..*out_plane_count)`'s own width/height/stride
 * (data left null -- the caller still owns allocating/pointing at real
 * storage) for `format` at `width`x`height`, and sets `*out_plane_count`.
 * Stride is computed with no row padding (the minimum valid stride for
 * each plane); a caller that needs host/decoder-specific row alignment
 * should widen the returned stride itself before allocating. Useful both
 * to build a crtmedia_frame from raw dimensions (this file's own
 * tests/frame_skia_smoke.cc does exactly that) and to sanity-check an
 * already-built one. Returns CRTMEDIA_ERROR_INVALID_ARGUMENT if
 * out_planes/out_plane_count is null, width or height is 0, or format is
 * not one of the crtmedia_pixel_format values above; CRTMEDIA_ERROR_
 * UNSUPPORTED is never returned by this function (every format this
 * contract defines has a known plane layout). */
crtmedia_result crtmedia_frame_describe_planes(
    crtmedia_pixel_format format,
    uint32_t width,
    uint32_t height,
    crtmedia_frame_plane out_planes[CRTMEDIA_FRAME_MAX_PLANES],
    uint32_t* out_plane_count);

/* Calls frame->release(frame, frame->release_context) if release is non-
 * NULL, then zeroes *frame (format becomes 0, which is not a valid
 * crtmedia_pixel_format value -- a use-after-release that dereferences
 * planes[0].data will now reliably crash on a null pointer instead of
 * silently reading freed memory). A NULL frame is a no-op. */
void crtmedia_frame_release(crtmedia_frame* frame);

/* Converts `src` (any format above) into `dst`, an already-constructed
 * CRTMEDIA_PIXEL_FORMAT_RGBA8888 frame the caller owns (dst->planes[0]
 * must already point at a buffer at least dst->planes[0].stride *
 * dst->height bytes, and dst->width/height must equal src->width/
 * height) -- this function never allocates and never touches dst->
 * release/release_context. Every output pixel's own alpha channel is set
 * to 255 (opaque); no source format this contract defines carries an
 * alpha plane. For CRTMEDIA_PIXEL_FORMAT_YUV420P input, the conversion
 * matrix is derived from src->color_space's own real ITU-R Kr/Kb luma
 * constants (BT.601/BT.709/BT.2020) and src->color_range's limited/full
 * scaling -- CRTMEDIA_COLOR_SPACE_UNSPECIFIED/CRTMEDIA_COLOR_RANGE_
 * UNSPECIFIED default to BT.709/LIMITED (this codebase's most common
 * real-world case, matching typical un-signaled H.264 content) rather
 * than erroring, since a genuinely color-managed pipeline is not this
 * contract's job -- see docs/libcrtgfx_api_policy.md's own crtgfx_
 * pixel_format comment for the matching "no color management applied"
 * policy on the RGB side. Returns CRTMEDIA_ERROR_INVALID_ARGUMENT for a
 * null src/dst, a dst not in CRTMEDIA_PIXEL_FORMAT_RGBA8888, or a width/
 * height mismatch; CRTMEDIA_ERROR_UNSUPPORTED if src->format is not one
 * of the crtmedia_pixel_format values above. */
crtmedia_result crtmedia_frame_convert_to_rgba(const crtmedia_frame* src, crtmedia_frame* dst);

#ifdef __cplusplus
}
#endif
