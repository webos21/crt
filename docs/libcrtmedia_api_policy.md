# libcrtmedia API Policy

This document records the public API boundary decision for `libcrtmedia`,
closing `TODO.md`'s "In Progress" step 1.

## Decision

The public media API core is a **project-owned C API modeled on Android
NDK Media's shape** (`AMediaFormat`/`AMediaExtractor`/`AMediaCodec`'s
key-value-format-plus-async-buffer-queue design), not a literal adoption of
those headers and not a direct exposure of FFmpeg's own API.

Three layers, in order of how firmly each is decided:

1. **Core (decided now): `crtmedia_format`/`crtmedia_extractor`/
   `crtmedia_codec`.** An opaque, async, buffer-queue-based C API,
   conceptually shaped like `AMediaFormat`/`AMediaExtractor`/`AMediaCodec`
   -- key-value format description instead of one fixed struct per
   container/codec, and dequeue/queue/release buffer ownership instead of
   a single blocking "give me the next frame" call. FFmpeg (`AVFormatContext`/
   `AVCodecContext`/`AVPacket`/`AVFrame`) stays entirely behind this layer,
   never named in a public header. This is the layer everything else in
   this document builds on.
2. **Convenience (decided now): keep `crtmedia_demuxer_*`.** The existing
   `include/crtmedia/demux.h` (`crtmedia_demuxer_open`/`_read`/...) --
   already implemented, already verified end-to-end on Linux/macOS/Windows
   -- is retained as a simple, synchronous pull-model convenience API for
   callers that just want "decode this file into frames," reimplemented on
   top of the core once the core exists rather than thrown away. Two real,
   working shapes for two real use cases, not competing designs.
3. **Future, explicitly deferred:** a WebCodecs-shaped `libcrtjs` binding
   (JS-facing, wraps the core -- `AMediaCodec`'s own async queue model
   already maps closely onto WebCodecs' `VideoDecoder`/`AudioDecoder`/
   `EncodedChunk` vocabulary, so this is expected to be a thin binding, not
   a redesign) and an optional WebRTC-shaped realtime track/source/sink
   layer (microphone/camera/realtime capture -- unrelated to file demux/
   decode, not a prerequisite for it). Neither is designed yet; recorded
   here only to fix their place in the layering.

**Exact AOSP NDK Media source compatibility is explicitly a later, optional
adapter -- never the core ABI.** The core uses project-owned `crtmedia_*`
naming and a `crtmedia_status`-based error contract (matching every other
`crtmedia_*` type already in this codebase, e.g. `crtmedia_result` in
`demux.h`), not `AMediaCodec_*`/`media_status_t`. A real Android-NDK-
source-compatible shim (literal `AMediaCodec_*` symbols translating to the
`crtmedia_*` core) can be added later if a concrete ported consumer
actually needs it, matching the project's own "no speculative work ahead
of a demonstrated need" convention -- see Non-Goals below for why it is
not the default.

## Rationale

### Why NDK-shaped, not FFmpeg-direct (unlike `libcrtgfx`/Skia)

`libcrtgfx_api_policy.md`'s own decision -- expose Skia's real headers
directly as the public 2D API -- was deliberately considered as the
precedent here and deliberately not repeated, for two concrete reasons
that do not apply to graphics:

- **License boundary.** `libcrtgfx` exposing Skia's own headers costs
  nothing licensing-wise (Skia is BSD). `porting/recipes/ffmpeg.json` is
  built LGPL-only, on purpose (see that recipe's own notes) -- LGPL
  requires a user be able to relink a different version of the LGPL'd
  component. An opaque `crtmedia_*` handle keeps that boundary exactly at
  `libcrtmedia`'s own shared-library edge, where it is trivial to satisfy
  (rebuild/relink one `.so`/`.dll`). Directly exposing `AVFormatContext`/
  `AVCodecContext` -- structs with public fields consumer code would
  routinely reach into -- would push that same relink obligation out into
  every consumer's own translation units instead, a materially different
  and harder-to-satisfy position that Skia's BSD license never created.
- **Audience mismatch.** Skia's own `SkCanvas`/`SkPaint` vocabulary *is*
  the API a drawing application actually wants -- there is no natural
  layer above it a real consumer would prefer instead. FFmpeg's
  `AVFormatContext`/`AVCodecContext` is demux/decode plumbing; real
  consumers (a player, a `<video>`-shaped UI element, this project's own
  eventual WebCodecs-style `libcrtjs` binding) want a queue/callback
  abstraction one level up, not raw packet/frame structs. Android's own
  `AMediaCodec` exists for exactly this reason -- a thin, stable C layer
  over a lower engine (Stagefright, in AOSP's case; FFmpeg, here) -- and
  Chromium's own internal media pipeline is shaped the same way (demuxer
  -> decoder -> renderer with queue-like buffer handoff) even though it
  also uses FFmpeg internally for some codecs. `libcrtgfx` had no
  equivalent "one layer up" abstraction to reach for; `libcrtmedia` does,
  and Android already built and proved it.

### Why the async buffer-queue model, not the existing sync `crtmedia_demuxer_read()` shape

The buffer-queue model (dequeue an empty input buffer, fill it, queue it;
dequeue a full output buffer, consume it, release it) is what lets a
codec's own decode work happen asynchronously and, later, on real hardware
queues -- `TODO.md`'s own roadmap (steps 5-8: GPU frame contract, Skia GPU
rendering, hardware decode phase A/B) needs exactly this shape to connect
decoder-owned buffers to a GPU/presentation pipeline without a redesign at
that point. Designing the core around it now, while the WAV/PCM baseline
is still simple, is cheaper than migrating a synchronous API later once
real consumers depend on it.

### Why keep `crtmedia_demuxer_*` at all, instead of one API

It already exists, is already verified end-to-end on all three hosts
(`crtmedia_demux_test`), and a plain "decode this whole file" call is a
real, common enough use shape (this project's own current test is exactly
that) that forcing every caller through the full queue/callback machinery
would be needless ceremony for the simple case. Reimplementing it as a
thin wrapper over the new core, rather than deleting it, means the
existing, already-passing test coverage keeps validating the core
transitively instead of becoming dead code to throw away.

### Why not literal AOSP `NdkMedia*.h` header adoption

Two reasons, both already-established project policy rather than new
judgment calls:

- `docs/runtime_roadmap.md`'s own Non-Goals already state: "Do not claim
  Android framework or APK compatibility." Shipping the literal
  `AMediaCodec_*`/`AMediaExtractor_*` symbol names, backed by FFmpeg
  instead of AOSP's real Stagefright/mediaserver stack, would silently
  make exactly that claim to any code written against real Android NDK
  media -- present real API surface, absent real Android platform
  behavior (extractor quirks, codec availability, `MediaDrm`/`MediaCrypto`
  integration, `ANativeWindow` output surfaces, and so on).
- Unlike Skia (whose real upstream source this project actually rebuilds,
  so "the header" and "the implementation" are the same upstream project),
  AOSP's NDK Media headers describe an API whose real reference
  implementation this project is *not* rebuilding -- FFmpeg is a
  different, if conceptually similar, engine underneath. Matching the
  *shape* transfers real, proven design value; matching the *literal
  symbol names* would transfer a compatibility promise this project isn't
  actually keeping.

## Non-Goals

- Do not expose FFmpeg headers or types (`AVFormatContext`, `AVCodecContext`,
  `AVPacket`, `AVFrame`, ...) in any public `crtmedia` header. If a real,
  demonstrated consumer need shows up for lower-level access than the
  queue-based core provides, add a narrow, explicitly-labeled escape hatch
  (e.g. a getter returning an opaque `void*`/documented-unstable pointer)
  rather than exposing FFmpeg's own headers -- do not design one
  preemptively.
- Do not ship literal `AMediaCodec_*`/`AMediaExtractor_*`/`AMediaFormat_*`
  symbol names or claim Android NDK source compatibility as the default.
  A real compatibility adapter is a later, separate, opt-in addition for a
  concrete ported consumer, not the core ABI -- see Decision above.
- Do not design the WebRTC-shaped realtime track layer or the WebCodecs
  `libcrtjs` binding yet. Both are real future work (`TODO.md` steps 9-12
  and step 4 above); this document only fixes where they sit relative to
  the core once they exist.
- Do not require every simple "decode this file" caller to use the full
  async queue/callback core. `crtmedia_demuxer_*` stays a first-class,
  supported convenience API, not a deprecated legacy path.
- Do not treat this decision as gating `TODO.md` step 2 (H.264+AAC MP4/MP3
  fixture evidence). Step 2's own fixture/threading/EOF work can and
  should continue against the existing `crtmedia_demuxer_*` shape; the
  core API design in this document is step 3's own concern.

## Public API Shape

```text
libcrtmedia/include/
  crtmedia/
    frame.h          # crtmedia_frame -- CPU video frame contract (existing, unchanged)
    audio.h          # crtmedia_audio_buffer -- PCM audio buffer contract (existing, unchanged)
    demux.h          # crtmedia_demuxer_* -- convenience/pull-model API, now a thin wrapper over extractor.h/codec.h (rebuilt and verified, 2026-09-02)
    format.h         # crtmedia_format -- key-value format description (implemented and verified, 2026-09-02)
    extractor.h      # crtmedia_extractor -- demux-only, no decode (implemented and verified, 2026-09-02)
    codec.h          # crtmedia_codec -- async buffer-queue decode (implemented and verified, 2026-09-02; encode remains future work)
```

`format.h`/`extractor.h`/`codec.h` are implemented and verified end to end
on Linux and Windows (`crtmedia_format_test`, `crtmedia_extractor_codec_
test` -- see `HISTORY.md`'s 2026-09-02 entry for the full trail), and
`demux.h` is now rebuilt over this new core too -- `crtmedia_demuxer_*` is
a real, thin wrapper composing `crtmedia_extractor` + one `crtmedia_codec`
per decodable track, no longer its own independent FFmpeg integration.
Every existing `crtmedia_demux_*_test` passes completely unchanged after
the rebuild, confirming it as a transparent internal swap. `frame.h`/
`audio.h` are unaffected either way: decoded output, from either API
layer, keeps landing in the same
`crtmedia_frame`/`crtmedia_audio_buffer` contracts already verified against
Skia and FFmpeg.
