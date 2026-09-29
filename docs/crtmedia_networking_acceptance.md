# Networking & Streaming Acceptance

## Scope and host order

Networking & Streaming extends the accepted `crtmedia_extractor`/
`crtmedia_muxer`/`crtmedia_codec` contracts (local-file only today) with a
CRT-owned transport layer:

```text
libcurl (HTTP/HTTPS, mbedTLS)
  -> CRT transport (bounded byte queue, cancellation, timeout, ownership)
  -> private FFmpeg custom AVIO
  -> existing extractor/muxer/codec pipeline
```

FFmpeg remains built with `--disable-network` and only the `file` protocol
(`porting/recipes/ffmpeg.json`) -- this project owns exactly one networking
implementation, not two. CRT's own sockets/DNS resolver and the
libcurl+mbedTLS HTTP/HTTPS port already pass on all three hosts
(`HISTORY.md`); this tranche is what plugs that already-accepted transport
into `libcrtmedia`, not a new networking stack of its own.

The implementation order is Windows/x64 first for Tranches 1-4, with a quick
Linux/x86_64 and macOS/arm64 replay after each one lands -- not deferred to
a single three-host pass at the end. Windows is first because it is the
most different POSIX-socket/PAL host: CRT's own libcurl port runs on Windows
through this project's portable POSIX socket API (`_WIN32` deliberately
undefined for the compile), not a native Winsock code path, and the
Windows curl bring-up already found and fixed real socket/select/threaded-
resolver/mbedTLS/DLL-symbol-ownership PAL gaps before HTTP/HTTPS passed
there (`HISTORY.md`). Closing the hardest host abstraction boundary first,
the same way Encode & Capture closed Windows last specifically *because* it
was hardest, means a POSIX-only assumption baked into Tranche 1's contract
gets caught immediately rather than discovered while retrofitting Windows
after Tranche 4. Tranches 5 and 6 close all three hosts together, matching
Encode & Capture's own final-closure shape.

**Contract discipline:** even though Windows implements first, Tranches 0
and 1 must be designed host-neutral from the start -- the bounded transport
core itself needs no socket or TLS library at all (a pure producer/consumer
byte queue), and its own acceptance test is fully resource-free. The same
source must pass unmodified on Linux/macOS when replayed; a "Windows
shape" leaking into the Tranche 1 contract is exactly the mistake this
discipline exists to prevent.

## Frozen common contract (Tranche 0)

### Error model

`crtmedia_result` (`crtmedia/frame.h`) currently distinguishes only
`CRTMEDIA_OK`, `CRTMEDIA_ERROR_INVALID_ARGUMENT`, `CRTMEDIA_ERROR_UNSUPPORTED`,
`CRTMEDIA_WOULD_BLOCK` (codec queue backpressure), and `CRTMEDIA_ERROR_IO`
(a present host resource, but the OS operation itself failed). Networking
needs to distinguish several states that all collapse into `UNSUPPORTED`
today, and a caller's reconnect policy cannot be built on top of that
collapse: a wait that timed out, a caller-requested cancellation, and an
unrecoverable transport failure are three different responses (retry with
the same deadline, stop entirely, or reconnect), not one. Three values are
added to the existing enum for this tranche:

- `CRTMEDIA_ERROR_TIMEOUT` -- a bounded wait (connect, read, write) elapsed
  with no progress. Distinct from `WOULD_BLOCK`: `WOULD_BLOCK` means "call
  again, no waiting implied"; `TIMEOUT` means "a real deadline the caller
  asked for was reached."
- `CRTMEDIA_ERROR_CANCELLED` -- the caller's own explicit cancellation
  (never a spontaneous transport decision) unblocked an in-progress
  operation. A cancelled transport must not be reused; release it.
- `CRTMEDIA_ERROR_PROTOCOL` -- the remote peer or its data violated the
  protocol this layer relies on for correctness (an HTTP response that
  cannot be trusted to resume safely -- wrong status for the request,
  `Content-Range` start mismatch, entity-validator mismatch on a `206`;
  TLS verification failure; a malformed response). Distinct from
  `CRTMEDIA_ERROR_IO` (the connection itself failed at the transport
  level, no data to distrust) and from `UNSUPPORTED` (this host/build
  cannot do this at all, independent of any remote peer).

A caller must be able to answer *retryable? cancelled? timed out?
permanent?* from the returned `crtmedia_result` alone, without inspecting a
side channel. This is frozen now, in Tranche 0, not discovered after the
HTTP implementation exists.

### Source/sink capability

TODO.md's existing rule --  "No public API may imply that a non-seekable
stream supports arbitrary seek" -- is sharpened into an explicit capability
set a transport source/sink reports:

```text
CRTMEDIA_TRANSPORT_READABLE
CRTMEDIA_TRANSPORT_WRITABLE
CRTMEDIA_TRANSPORT_SEEKABLE
CRTMEDIA_TRANSPORT_SIZE_KNOWN
```

HTTP input without a validated `Range` response is `READABLE` only; HTTP
input with a validated `Range` response (Tranche 2) is `READABLE |
SEEKABLE | SIZE_KNOWN`; chunked/live HTTP is `READABLE` only, permanently.
A caller (and the custom AVIO adapter feeding FFmpeg) must consult this set
rather than probe seek support by trying it and handling failure.

The existing local-file API is unchanged and stays the common case:
`crtmedia_extractor_create(path, ...)` and `crtmedia_muxer_create(path,
...)` keep working exactly as today (`crtmedia_extractor.c`'s own direct
`av_seek_frame()` call is fine for the file case it exists for). Streaming
is purely additive:

```text
crtmedia_extractor
       |
       +-- file source        (existing, unchanged)
       |
       +-- stream source      (new, Tranche 2)
               |
               v
          CRT transport -> private custom AVIO

crtmedia_muxer
       |
       +-- file sink          (existing, unchanged: regular MP4)
       |
       +-- stream sink        (new, Tranche 3: fragmented MP4 -> HTTP upload)
```

No curl type, FFmpeg type, or host socket type crosses into a public
`crtmedia/*.h` header. The transport is a private implementation detail
behind the additive stream source/sink entry points, exactly like FFmpeg
and host SDK types already stay private behind `crtmedia_codec`/
`crtmedia_muxer` today.

### Back-pressure

A slow receiver must propagate bounded back-pressure upstream; no
component may grow memory without bound or lose frame/sample ownership
silently. This is *not* the same as requiring every upstream producer to
be stoppable: a live camera is not a fully throttleable producer (the
existing capture contract already returns native buffers immediately and
hands the caller an owned, already-copied frame -- there is no "pause the
driver" lever once a frame has been delivered). The precise rule:

> A slow receiver must propagate bounded back-pressure upstream; no
> component may grow memory without bound or lose ownership. A live
> producer that cannot itself be stalled must apply an explicit,
> measurable drop policy instead of unbounded queuing or silent ownership
> loss.

So the only three legal outcomes under sustained back-pressure are: the
producer blocks, the producer receives `CRTMEDIA_WOULD_BLOCK` and retries
on its own schedule, or an explicit, counted drop policy discards frames a
live producer cannot hold. An unbounded queue or a frame that vanishes
without a release callback and without being counted as dropped are both
contract violations. This same model is the intended fit for a future
WebRTC producer, not a Tranche-3-only rule.

### Reconnect

Resuming input after a lost connection is safe only when protocol metadata
makes the byte offset provably safe, not merely plausible. For HTTP this
means, concretely:

```text
Range: bytes=<next-byte>-
If-Range: <ETag-or-validated-Last-Modified>
```

and, on the response, *all three* of:

```text
206 Partial Content
Content-Range start == the requested offset
the same entity validator as the original response
```

Missing any one of the three means the server's content may have changed
between the original request and the resume, and resuming anyway risks
splicing bytes from two different versions of the resource into one
decoded stream (an old-first-half/new-second-half corruption, not a clean
failure). Anything short of all three is treated as
`CRTMEDIA_ERROR_PROTOCOL`, not resumed. Retry count and backoff are
bounded; already-delivered samples are never duplicated.

Streaming **output** does not attempt automatic resume in this tranche:
once any bytes have been committed to a connection that is then lost, the
upload fails honestly (`CRTMEDIA_ERROR_IO`/`CRTMEDIA_ERROR_CANCELLED` as
appropriate) rather than guessing at a safe resume point on the write
side. A future tranche may add verified resumable upload; this one does
not claim it.

### TLS trust policy

CRT's own libcurl port is built `--without-ca-bundle --without-ca-path`
(`porting/recipes/curl.json`) -- it vendors no CA trust store, by design.
The existing `porting/tests/curl_http_roundtrip.c` HTTPS coverage runs with
`CURLOPT_SSL_VERIFYPEER`/`CURLOPT_SSL_VERIFYHOST` both explicitly disabled;
it proves the TLS handshake and mbedTLS encrypt/decrypt path work, and
proves nothing about server authentication. Reusing that as this tranche's
"TLS policy" would ship streaming HTTPS that never actually verifies who it
is talking to, so this tranche defines and exercises a real trust policy
instead of pointing at that test:

- Certificate verification is **on by default**. There is no public option
  to silently disable it; a caller who genuinely needs an insecure mode for
  local development must say so explicitly and loudly (a distinctly named
  opt-in, not a default).
- The caller supplies the CA trust material (`CURLOPT_CAINFO`/
  `CURLOPT_CAINFO_BLOB`-equivalent), matching curl.json's own documented
  expectation that a deployment consumer provides its own bundle -- this
  project still vendors none.
- Acceptance runs against a repository-owned loopback TLS fixture, not a
  real external host: a CA cert plus a server cert whose SAN covers
  `IP:127.0.0.1`, generated at test time (never checked in with a
  hardcoded validity window). The matrix:

  ```text
  correct CA                 -> connect succeeds
  wrong/unrelated CA         -> connect fails (verification failure)
  correct CA, wrong SAN/IP   -> connect fails (hostname/IP mismatch)
  ```

Only once this matrix passes can this tranche honestly claim "HTTPS
authentication verified," not merely "HTTPS bytes decrypt."

## Tranches and gates

### 0. Freeze the transport/streaming acceptance contract

Public additions in this tranche are documentation and the error-model
enum values only (`CRTMEDIA_ERROR_TIMEOUT`, `CRTMEDIA_ERROR_CANCELLED`,
`CRTMEDIA_ERROR_PROTOCOL` on `crtmedia_result`) -- no transport code yet.
This document is the deliverable.

### 1. Bounded transport core

A fixed-capacity byte queue (explicit high/low watermark, partial read/
write, EOF, cancellation, timeout, sticky-error state) with no socket,
curl, or TLS dependency at all -- a pure producer/consumer primitive,
fully resource-free and host-neutral by construction. Windows/x64 first,
replayed on Linux/x86_64 and macOS/arm64 shortly after (same source,
unmodified). Deterministic threaded acceptance, each proven, not assumed:

```text
producer faster than consumer -> high watermark reached -> block or
    WOULD_BLOCK, memory never exceeds capacity
consumer faster than producer -> consumer waits, producer wakes it
cancel while a writer is blocked  -> writer wakes immediately, CANCELLED
cancel while a reader is blocked  -> reader wakes immediately, CANCELLED
EOF -> remaining buffered bytes drain first, EOF reported only after
release -> every waiter wakes, every thread joins, no leak
```

**Windows/x64 done 2026-09-28** (`HISTORY.md`): `crtmedia_transport_queue_
test: ok watermark=pass timeout=pass eof=pass stress_producer_faster=pass
stress_consumer_faster=pass cancel_writer=pass cancel_reader=pass
release_wakes_all=pass`, 5 consecutive runs with no flakiness; full `ctest`
156/156.

**macOS/arm64 done 2026-09-29.** Same source, unmodified: identical result
across 5 consecutive runs, no code changes needed anywhere -- confirming
this primitive really is host-neutral by construction, not just by
intention. Full in-tree `ctest`: 146/146 (this host's own total test count
naturally differs from Windows' 156, per-OS test registration, same as
every other cross-host tranche in this project; no test regressed).

**Linux/x86_64 done 2026-09-29.** Same source, unmodified, no code changes
needed: identical result across 5 consecutive runs -- `crtmedia_transport_
queue_test: ok watermark=pass timeout=pass eof=pass stress_producer_
faster=pass stress_consumer_faster=pass cancel_writer=pass
cancel_reader=pass release_wakes_all=pass`. Full in-tree `ctest`: 139/140
(the one failure is the same pre-existing, environment-caused
`crtmedia_playback_pipeline_test_runs` gap this host has had since before
this tranche began -- no sound card at all -- unrelated to the transport
queue). This tranche is now closed on all three hosts.

### 2. Progressive HTTP input and extractor integration

The accepted transport becomes private FFmpeg custom AVIO; the pinned
FFmpeg network configuration (`--disable-network`) is unchanged. A
repository-owned loopback HTTP/1.1 test server exercises this against
**two separate fixtures**, kept deliberately distinct so a failure points
at one cause instead of three:

- **A. Range-capable regular MP4** -- a normal (non-fragmented) MP4 served
  with validated `Range` support: exercises the *seekable* custom AVIO
  path and the *ordinary* MP4 extractor's existing seek requirements
  (`READABLE | SEEKABLE | SIZE_KNOWN`).
- **B. Chunked/no-`Range` delivery** -- served without `Range` support
  (`Transfer-Encoding: chunked` or no `Content-Length`): exercises the
  *non-seekable* custom AVIO path against a **fragmented/streamable** MP4
  fixture that does not need seek at all (`READABLE` only).

Conflating these two into one fixture risks mistaking a `Range`-handling
bug, a custom-AVIO bug, an ordinary-MP4-seek requirement, or an HTTP
chunk-parsing bug for one another. Both fixtures cover short delivery,
`Content-Length` (A only), EOF, cancellation, and redirects within the
frozen policy, ending in a real extractor -> decoder frame-count/PTS/
content round trip.

**Windows/x64 done 2026-09-28.** `crtmedia_extractor_create_from_url()`
(`crtmedia/extractor.h`) is the additive entry point: a private
libcurl-backed producer thread (`libcrtmedia/src/http_transport.c`) feeds
the accepted Tranche 1 bounded queue, which a private FFmpeg custom
`AVIOContext` (`libcrtmedia/src/http_avio.c`) reads from -- seeking
(fixture A only) closes the current transport and opens a brand new one
with a freshly validated `Range` request at the target offset, never a
silent, unvalidated jump. Capability detection always issues a real
`Range: bytes=<offset>-` request, even at offset 0 (proving support, not
inferring it from an `Accept-Ranges` header claim): `206` with a matching
`Content-Range` start means `SEEKABLE | SIZE_KNOWN`; a `200` at offset 0
means the server ignored the range, a legitimate `READABLE`-only outcome
for fixture B.

Real results: `crtmedia_http_input_range_test: ok` (fixture A -- the real
`libcrtmedia/assets/test_video.mp4` fixture, byte-identical decode result
to the existing local-file `crtmedia_extractor_codec_test`, plus a real
seek-via-reconnect exercised end to end) and
`crtmedia_http_input_chunked_test: ok` (fixture B -- a fragmented MP4
generated at test time, `CRTMEDIA_SOURCE_SEEKABLE` correctly absent), each
3 consecutive runs with no flakiness. Full `ctest`: 158/158 (one unrelated,
honest, pre-existing skip -- the same `crtmedia_capture_mf_test` webcam-
not-enumerable environmental gap noted in Tranche 6 of `docs/crtmedia_
encode_capture_acceptance.md`).

Three real findings along the way, each fixed and re-verified:

- **A real, previously-unknown Windows PAL bug**, found the moment two
  fixtures needed a real client and a real server in the same process for
  the first time in this project's history: `poll_socket()` (`libc/src/
  arch/windows/common/syscall.c`) answered `POLLIN` for every socket kind
  using `ioctlsocket(FIONREAD)` -- correct for a connected, data-carrying
  socket, but `FIONREAD` on a *listening* socket always reports 0 on
  Winsock (it has no byte stream, only pending connections), so a real
  client connecting to a real loopback server's listening socket never
  woke that server's own `poll()` loop at all, confirmed with a minimal
  standalone repro (`connect()` returns 0 immediately, `poll(POLLIN)` on
  the listening socket never returns readable across dozens of iterations)
  before touching any real code. `porting/recipes/curl.json`'s own
  extensive Windows history never hit this because every prior curl test
  was a client only, against a real remote server (`example.com`) --
  never this project's own code on both ends of a loopback socket pair.
  Fixed by switching to a zero-timeout `winsock.select()` readfds check
  (the same pattern the POLLOUT branch immediately above it already uses
  for the analogous non-blocking-connect-completion problem), which
  correctly reports both "data available" and "a connection is pending" as
  readable, matching real POSIX `poll()`/`select()` semantics for a
  listening socket. Re-verified against the full `ctest` suite (158/158,
  no regression) since this is a core, shared PAL path.
- **A real gap in Tranche 1's own original design**, found wiring the
  first real producer on top of it: the bounded transport queue had no way
  to distinguish a truncated transfer (a dropped HTTP connection) from a
  clean end of stream -- `crtmedia_transport_queue_write_error()`
  (`libcrtmedia/src/transport_queue.h`) adds a sticky error state,
  additive to the already-closed, already-three-host-replayed Tranche 1
  work rather than amending it. Covered by a new `test_sticky_error_
  drains_before_reporting` case in the existing `crtmedia_transport_
  queue_test`.
- **Fragmented MP4 write support added to the muxer**
  (`CRTMEDIA_MUXER_OUTPUT_MPEG_4_FRAGMENTED`, `crtmedia/muxer.h`), needed
  to generate fixture B's own genuinely forward-streamable fixture (a
  plain MP4's `moov` atom sits after all sample data by default, which a
  non-seekable reader can never reach at all) -- `movflags=frag_keyframe+
  empty_moov+default_base_moof`. This is exactly the same muxer capability
  Tranche 3's own non-seekable HTTP upload sink will need (a live upload
  stream cannot seek back to patch a header the way a local-file
  "faststart" rewrite would), so this was real, correctly-sequenced shared
  infrastructure, not scope creep.

A negative seek check was deliberately *not* added to fixture B beyond the
capability-bit assertion: real, confirmed FFmpeg behavior found while
writing this test makes a seek call's own success/failure an unreliable
signal on its own (`av_seek_frame()` on a fragmented MP4 can report success
without ever touching the `AVIOContext`, and `AVIOContext`'s own internal
read-ahead buffer can silently satisfy a small seek from already-buffered
memory regardless of the transport's real seekability) -- neither is a
contract violation. `CRTMEDIA_SOURCE_SEEKABLE` is the one reliable,
documented signal this contract actually promises, and it is correct.

**macOS/arm64 done 2026-09-29.** The unchanged implementation built against
this project's own FFmpeg, curl, mbedTLS, zlib, libc, libm, libdl, and libc++
artifacts and passed both real loopback fixtures 3 consecutive times:
`crtmedia_http_input_range_test: ok` and
`crtmedia_http_input_chunked_test: ok`. The updated Tranche 1 queue regression
also passed 5 consecutive times, including `sticky_error=pass`, and the local
file extractor regression passed 3 consecutive times.

This replay exposed an independent build-tool regression before the media
tests could be enabled: a 2026-09-16 macOS RPATH cleanup made every
`CRT_PORT_RPATH_DIR` consumer use `@loader_path`, which is correct for a port
dylib installed beside its dependencies but wrong for configure executables
created in an arbitrary work directory. curl's strict runtime-libraries probe
therefore linked but could not load `@rpath/libmbedtls.dylib`. `tools/crt-cc`
and `tools/crt-c++` now retain `@loader_path` for shared-library links and use
the real build-time port library path for executable links; distribution
staging remains responsible for canonicalizing published RPATHs. A fresh CRT
curl dependency build then passed the formerly failing probe (`runtime libs
availability... fine`) and installed both static and shared libcurl; the
recipe's static and shared real-network round trips both returned
`curl_http_roundtrip_test: ok http=200 https=200`.

Full in-tree CTest was 147/148. Both Tranche 2 tests passed in that run; the
one failure was the independent real-camera `crtmedia_capture_encode_
lifecycle_test` (`capture lifecycle cycle`). The same run's standalone
AVFoundation capture, VideoToolbox encode, playback, hardware decode, and
zero-copy tests all passed; repeating the lifecycle test alone reproduced its
device-cycle failure, so it is recorded honestly rather than attributed to
this networking tranche.

Linux/x86_64 replay is next.

### 3. Encoded streaming output

Feed the already-accepted software/hardware encode sample contract into a
non-seekable streaming container (fragmented MP4, per Tranche 0's pinned-
FFmpeg audit) and an HTTP upload sink using the same bounded transport. The
loopback receiver reopens/decodes the captured bytes and verifies exact
sample/frame count, monotonic PTS, EOS drain, and bounded memory -- and the
back-pressure contract above (block, `WOULD_BLOCK`, or an explicit counted
drop for an unstoppable live producer; never unbounded growth, never
silent ownership loss).

### 4. Reconnect and discontinuity

Reproduce a server closing mid-response/mid-upload. Resume input only per
the reconnect contract above (`Range` + `If-Range`, all three response
checks); never duplicate already-delivered samples; bound retry count/
backoff; surface non-resumable output failure honestly
(`CRTMEDIA_ERROR_IO`/`CANCELLED`, no silent resume attempt). Re-run the
already-accepted timestamp-gap (`crtmedia_timing_discontinuity_test`'s own
exact-PTS-preservation contract), flush, lifecycle, and software-fallback
tests around reconnect boundaries specifically, not just in isolation.

### 5. HTTPS and cross-host acceptance

Exercise the TLS trust policy above (verify-on-by-default, caller-supplied
CA, the correct-CA/wrong-CA/wrong-SAN loopback matrix) using the existing
libcurl+mbedTLS stack -- no new host TLS type in any public header. Keep
deterministic protocol tests on loopback with the pinned test certificate/
IP literal so the current minimal synchronous UDP/IPv4/A-record-only DNS
resolver is never mistaken for a streaming prerequisite; separate resolver
coverage exists where a real name lookup is actually required. Replay the
full, unchanged contract on Windows/x64, Linux/x86_64, and macOS/arm64.

### 6. Lifecycle and isolated-package acceptance

Stress repeated connect/stream/cancel/reconnect/destroy cycles (matching
Encode & Capture Tranche 6's own real-device lifecycle-stress precedent,
`HISTORY.md` 2026-09-28), audit sockets/threads/native handles for leaks,
and keep local file playback plus capture/encode green throughout. Rebuild
the isolated `04-gfx-media` stage from its predecessor SDK with every new
private dependency and prerequisite declared (`tools/
create_stage_source.py`'s file registry, `tools/crt_dist_prerequisites.py`
-- both had real, silent staleness gaps found closing Encode & Capture
Tranche 6; do not repeat that gap for networking), rebuild an installed
HTTP streaming consumer, run `verify_dist.py` plus PE/ELF/Mach-O
dependency/RPATH checks, and publish only after all three hosts pass.

## Non-goals for the current tranche

- RTSP, WebRTC, or adaptive streaming (HLS/DASH) -- later consumers of this
  same bounded transport, not completion gates here.
- Resumable streaming **upload** (Tranche 4 defines input resume only;
  output failure is honest, not silently retried).
- A full HTTP client feature surface (proxies, cookies, arbitrary auth
  schemes) beyond what the loopback acceptance fixtures above require.
- Replacing or extending the current minimal DNS resolver; loopback/IP-
  literal testing is deliberate, not a gap being deferred.
