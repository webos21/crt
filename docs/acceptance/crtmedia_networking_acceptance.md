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

**Windows/x64 done 2026-09-28** ([`archived HISTORY.md`](https://github.com/webos21/crt/blob/4e5eead68048723c37e46c22d80bca43915ac093/HISTORY.md)): `crtmedia_transport_queue_
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
not-enumerable environmental gap noted in Tranche 6 of `docs/acceptance/crtmedia_
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

**Linux/x86_64 done 2026-09-29.** The CRT curl dependency stack built first,
and both its static and shared real-network HTTP/HTTPS round trips passed
(`curl_http_roundtrip_test: ok http=200 https=200`). The two repository-owned
loopback fixtures then passed 3 consecutive runs each:
`crtmedia_http_input_range_test: ok` (validated `206`/`Content-Range`, local
MP4 decode, and real seek-via-reconnect) and
`crtmedia_http_input_chunked_test: ok` (non-seekable fragmented MP4 over
chunked transfer). The local-file extractor regression also passed 3
consecutive times, and the bounded queue regression passed 5 consecutive
times including `sticky_error=pass`.

This replay found and fixed two Linux link-closure defects before the tests
could run as part of a complete build:

- The static curl dependency chain was appended after the existing CRT+
  FFmpeg rescan group. `libcurl.a`'s late `basename()` reference therefore
  could not select the implementation from this project's earlier
  `libc.a`; GNU ld instead diagnosed the host-glibc symbol as `DSO missing
  from command line`. CRT, FFmpeg, curl, mbedTLS, and zlib now share one
  static rescan group, including the player demo's explicit all-static
  closure. The final HTTP test binary defines `basename` from CRT.
- `libcrtmedia.so` had also tried to fold the Linux non-PIC `libz.a` into a
  shared object. The Linux shared target now links the port's shared
  `libcurl.so`, which records its own mbedTLS/zlib dependencies, while the
  static target keeps the all-static dependency chain. The complete build,
  including `libcrtmedia.so` and `crtmedia_player_demo`, succeeds with HTTP
  input enabled.

Full in-tree CTest was 141/142. Both HTTP fixtures passed. The sole failure
was the same pre-existing `crtmedia_playback_pipeline_test_runs` wall-time
pacing check already recorded during Tranche 1 on this Linux host; `aplay -l`
still reports `no soundcards found`, so it remains an environmental playback
gap rather than a networking regression. Tranche 2 is now closed on all
three hosts.

### 3. Encoded streaming output

Feed the already-accepted software/hardware encode sample contract into a
non-seekable streaming container (fragmented MP4, per Tranche 0's pinned-
FFmpeg audit) and an HTTP upload sink using the same bounded transport. The
loopback receiver reopens/decodes the captured bytes and verifies exact
sample/frame count, monotonic PTS, EOS drain, and bounded memory -- and the
back-pressure contract above (block, `WOULD_BLOCK`, or an explicit counted
drop for an unstoppable live producer; never unbounded growth, never
silent ownership loss).

**Windows/x64 done 2026-09-29; macOS/arm64 and Linux/x86_64 replays done the same day (below).** The additive
public entry point is `crtmedia_muxer_create_for_url()` (`crtmedia/muxer.h`).
It accepts fragmented MP4 only, reports `CRTMEDIA_SINK_WRITABLE` without
claiming seekability or a known size, and takes the queue capacity as an
explicit hard memory bound. The existing local-file muxer is unchanged. A
private FFmpeg write-only `AVIOContext` feeds `libcrtmedia/src/http_upload.c`;
that worker consumes the already-accepted Tranche 1 queue through libcurl on
a background pthread and sends an unknown-length HTTP/1.1 PUT with chunked
transfer encoding. Curl, FFmpeg, and socket types remain private. Redirects
and upload resume are disabled; release cancels the queue and curl progress
callback so a blocked writer/network operation can terminate.

The resource-free `crtmedia_http_output_test` uses a repository-owned slow
loopback PUT receiver built on CRT sockets. It software-encodes 90
deterministic 160x120 YUV420P frames, writes every one into fragmented MP4
over a 4 KiB queue, receives a 1,034,884-byte body (well above the hard queue
capacity), requires real `ftyp`/`moof`/`mdat` boxes and chunked transfer, then
reopens and decodes the captured body. Exact result:

```text
crtmedia_http_output_test: ok samples=90 frames=90 bytes=1034884
    queue_capacity=4096 slow_receiver_ms=291
```

Every PTS is checked against the original 33,333-us timeline, EOS drains on
both encoder and decoder, the first/last decoded content is bounded-checked,
and no sample is duplicated or lost. The intentionally slow receiver proves
the synchronous block form of the frozen back-pressure contract: the bounded
queue stalls the encoded-sample writer; it does not pretend a live camera can
be paused and it does not silently drop an owned frame.

Windows validation: the new acceptance passed alone and with the existing
transport queue, HTTP range/chunked input, encode/mux, and timing-
discontinuity tests; static and shared `crtmedia` both rebuilt. Full in-tree
CTest passed 159/159, with the pre-existing no-webcam
`crtmedia_capture_mf_test` classified as the one expected skip.

An exploratory WSL replay was deliberately stopped at the maintainer's
request and is not acceptance evidence: that old build tree first exposed a
stale pre-encode FFmpeg cache, and rebuilding it would still not provide the
real Linux media-device boundary required for this stage. Replay the exact
source on the physical Linux device instead.

**macOS/arm64 done 2026-09-29.** Same, unmodified source. `CRTMEDIA_ENABLE_
CURL=ON` against the curl/mbedTLS/zlib stack already built in this tree
during Tranche 2's own macOS work (no new port build needed). Real result
across 4 consecutive runs, identical every time: `crtmedia_http_output_test:
ok samples=90 frames=90 bytes=1034862 queue_capacity=4096
slow_receiver_ms=...`. The byte count (1,034,862 vs. Windows' 1,034,884) is
an expected, honest per-host difference -- fragmented-MP4 box sizes are not
byte-identical across FFmpeg builds/hosts -- and is not itself part of the
acceptance bar; the sample/frame count (90/90), exact-PTS checks, and
bounded first/last content checks all matched exactly, and the test's own
`ok` line is what actually gates acceptance. Ran together with the existing
transport-queue, HTTP range/chunked input, and extractor regressions with no
interference. Full in-tree `ctest`: 149/149 (one expected `crtmedia_
capture_avfoundation_test` camera-authorization skip, `docs/acceptance/crtmedia_
encode_capture_acceptance.md`'s own Tranche 4A finding, unrelated to
networking).

A real, host-independent packaging gap surfaced running the tooling test
suite (`tools/test_stage_source_closure.py`), not specific to macOS:
`extractor.c`/`muxer.c` now unconditionally `#include "http_avio.h"`/
`"http_upload.h"` (the header/API surface always compiles; only the `.c`
implementation and actual behavior are gated on `CRTMEDIA_ENABLE_CURL`),
but neither those headers, `transport_queue.{c,h}`, `http_transport.{c,h}`,
`http_upload.c`, nor any of the new Tranche 1-3 test files had ever been
added to `tools/create_stage_source.py`'s file registry -- the identical
class of gap Encode & Capture Tranche 6 found repeatedly, now recurring for
Networking's own new files, on all three hosts equally. Fixed by adding all
of them, following the same "bundle unconditionally, let the isolated
stage's own CMake decide whether to actually compile it" precedent already
used for `capture_mf.c`/`capture_avfoundation.c` -- no isolated stage passes
`--enable-curl` yet (that wiring is this tranche's own Tranche 6 item), so
the three `http_*.c` implementation files are currently inert there, not yet
exercised. Full tooling test suite: 77/77 after the fix.

**Linux/x86_64 done 2026-09-29, on the physical Linux acceptance host** (native
Intel UHD 630 -- not WSL, per the stale-cache/no-real-device-boundary note
above). Same, unmodified source, `CRTMEDIA_ENABLE_CURL=ON` against the
curl/mbedTLS stack already built for Tranche 2's own Linux closure (shared
`libcurl.so` for `libcrtmedia.so`); no code, CMake, or recipe change was
needed. Real result across 5 consecutive runs, identical every time:
`crtmedia_http_output_test: ok samples=90 frames=90 bytes=1034884
queue_capacity=4096 slow_receiver_ms=~110`. The byte count matches
Windows' 1,034,884 exactly (macOS' 1,034,862 is the outlier -- box sizes
are per-FFmpeg-build, not part of the acceptance bar; the sample/frame
count, exact-PTS, EOS-drain, and bounded-content checks are). Ran together
with the transport-queue, HTTP range/chunked input, extractor, encode/mux,
timing-discontinuity, and V4L2 capture regressions (8/8, camera present)
with no interference. Full in-tree `ctest`: 142/143 (the one failure is the
pre-existing, environment-caused `crtmedia_playback_pipeline_test_runs` gap
-- no sound card on this host -- unrelated to networking); tooling suite
(including `tools/test_stage_source_closure.py`, which macOS' registry fix
already made pass): 77/77. This tranche is now closed on all three hosts.

### 4. Reconnect and discontinuity

Reproduce a server closing mid-response/mid-upload. Resume input only per
the reconnect contract above (`Range` + `If-Range`, all three response
checks); never duplicate already-delivered samples; bound retry count/
backoff; surface non-resumable output failure honestly
(`CRTMEDIA_ERROR_IO`/`CANCELLED`, no silent resume attempt). Re-run the
already-accepted timestamp-gap (`crtmedia_timing_discontinuity_test`'s own
exact-PTS-preservation contract), flush, lifecycle, and software-fallback
tests around reconnect boundaries specifically, not just in isolation.

**Windows/x64 done 2026-09-29.** Reconnect lives entirely in the private
`libcrtmedia/src/http_avio.c` read path, on top of `http_transport.c`'s new
`crtmedia_http_transport_open_resume()`; no public API changed. When a read
fails with `CRTMEDIA_ERROR_IO`/`_TIMEOUT` (the queue's sticky error, reported
only after every already-buffered byte was delivered) and the original response
gave a usable entity validator (strong ETag, else `Last-Modified`; a weak
`W/` ETag is not a valid `If-Range` validator), it reopens at exactly the count
of bytes already handed to FFmpeg with `Range: bytes=<n>-` plus
`If-Range: <validator>`, and accepts the result only when the response is a
`206`, `Content-Range` starts at `<n>`, and the response carries the same
validator. Anything else is `CRTMEDIA_ERROR_PROTOCOL` and is not retried.
Retries are bounded: 3 consecutive attempts with 25/50/100 ms backoff; any
byte delivered after a resume resets the count. Seeks (`av_seek_frame`) use the
same validated open when a validator is known. The first unrecoverable failure
is sticky and reported by `crtmedia_extractor_read_sample()` and
`crtmedia_extractor_create_from_url()` (`crtmedia_http_avio_last_error()`),
never as a clean end of stream.

`crtmedia_http_reconnect_test` (repository-owned loopback server with fault
injection, `tests/http_test_server.h`; extracted sample streams are compared
by track/PTS/size/payload hash against a clean local-file extraction). Real
result, 3 consecutive identical runs, ~3.3 s:

```text
crtmedia_http_reconnect_test: ok samples=70 resumes=2 bounded_retries=3
    changed_resource=protocol no_validator=io upload_drop=io
```

| Injected fault | Required outcome | Result |
| --- | --- | --- |
| Server closes mid-response twice, validator present | 2 resumes, all with `If-Range`; sample stream identical to local (70/70), max PTS preserved | pass |
| Same drop, server offers no validator | `CRTMEDIA_ERROR_IO`, zero resume requests | pass |
| Resource changed, server answers `200` to `If-Range` | `CRTMEDIA_ERROR_PROTOCOL`, no splice | pass |
| Resource changed, server ignores `If-Range`, `206` with new ETag | `CRTMEDIA_ERROR_PROTOCOL`, no splice | pass |
| Every resume refused | exactly 3 resume attempts, then `CRTMEDIA_ERROR_IO` | pass |
| Transport level: matching validator | 206, resumed bytes equal the body from the offset | pass |
| Receiver dies mid-upload | writer gets `CRTMEDIA_ERROR_IO`; `finish()` not OK; exactly one connection (no auto-resume) | pass |

Regressions run unchanged and green around this work: `crtmedia_timing_
discontinuity_test` (exact-PTS contract), `crtmedia_capture_encode_lifecycle_
test`, `crtmedia_encode_mux_test`, `crtmedia_hw_decode_{,flush_,lifecycle_}
test`, and the Tranche 1-3 tests. The PTS-exactness of a reconnect boundary is
proven directly by the hash comparison above (PTS is part of the hash); the
hardware-decode and software-fallback paths do not touch the network reader, so
their existing tests are the boundary check for them on this host. Full
in-tree `ctest`: 160/160 (one expected `crtmedia_capture_mf_test` no-webcam
skip).

Three real bugs found and fixed (none introduced by this tranche's own new
code):

- `crtmedia_extractor_read_sample()` mapped *every* `av_read_frame()` failure
  to a clean end of stream, so a dropped connection silently truncated the
  media. URL extractors now surface the real error; local-file behavior is
  unchanged.
- `http_transport.c`'s `Content-Range` parser read the word "bytes" as the
  start offset (always 0). A nonzero-offset `206` was effectively unvalidated
  since Tranche 2 (whose tests only ever compared offset 0). It now parses
  `bytes <start>-<end>/<total>`, and any `206` whose start differs from the
  request is `CRTMEDIA_ERROR_PROTOCOL`.
- Windows PAL `poll()` counted one loop iteration as one millisecond while
  `Sleep(1)` sleeps a full ~15.6 ms timer tick, so a 200 ms timeout blocked
  ~3 s (every loopback test server took ~3 s to stop; this test took 31.9 s
  before the fix, 3.3 s after). `__crt_sys_poll()` now measures the timeout
  against `QueryPerformanceCounter`.

Linux/x86_64 and macOS/arm64 replay is next.

**macOS/arm64 done 2026-09-29.** Same, unmodified source (`CRTMEDIA_ENABLE_
CURL=ON`, curl stack already built in this tree). `crtmedia_http_reconnect_
test` passed 3 consecutive runs with the identical result line as Windows --
`ok samples=70 resumes=2 bounded_retries=3 changed_resource=protocol
no_validator=io upload_drop=io` -- covering every row of the fault-injection
table above. The `moov atom not found` lines FFmpeg logs during the run come
from the deliberately truncated/refused cases and are expected. The
regressions named above (`crtmedia_timing_discontinuity_test`, `crtmedia_
capture_encode_lifecycle_test`, `crtmedia_encode_mux_test`, `crtmedia_hw_
decode_{,flush_,lifecycle_}test`, Tranche 1-3 tests) all passed unchanged.
The Windows-only `poll()` timeout fix has no macOS counterpart (macOS uses
the host `poll()`). Full in-tree `ctest`: 150/150 (one expected `crtmedia_
capture_avfoundation_test` camera-authorization skip); tooling tests 77/77.

**Linux/x86_64 done 2026-09-29, on the physical Linux host.** Same,
unmodified source (`CRTMEDIA_ENABLE_CURL=ON`, curl/mbedTLS stack already in
this tree from Tranche 2). `crtmedia_http_reconnect_test` passed 3
consecutive runs with the identical result line as Windows and macOS --
`ok samples=70 resumes=2 bounded_retries=3 changed_resource=protocol
no_validator=io upload_drop=io`, ~2.75 s each -- covering every row of the
fault-injection table. No code, CMake, or recipe change was needed; the
Windows-only `poll()` fix has no Linux counterpart (Linux uses the host
`poll()`, and the ~2.7 s runtime is already the loopback servers' own
bounded shutdown wait, not a timeout-accounting artifact). The regressions
named above ran unchanged and green in one pass (14/14): `crtmedia_timing_
discontinuity_test`, `crtmedia_capture_encode_lifecycle_test` (camera
present), `crtmedia_encode_mux_test`, `crtmedia_encode_vaapi_test`,
`crtmedia_hw_decode_{,flush_,lifecycle_}test`, `crtmedia_zero_copy_test`,
the Tranche 1-3 tests, and the extractor tests. Full in-tree `ctest`:
143/144 (the one failure is the pre-existing no-sound-card
`crtmedia_playback_pipeline_test_runs` gap, unrelated); tooling tests
77/77. This tranche is now closed on all three hosts.

### 5. HTTPS and cross-host acceptance

Exercise the TLS trust policy above (verify-on-by-default, caller-supplied
CA, the correct-CA/wrong-CA/wrong-SAN loopback matrix) using the existing
libcurl+mbedTLS stack -- no new host TLS type in any public header. Keep
deterministic protocol tests on loopback with the pinned test certificate/
IP literal so the current minimal synchronous UDP/IPv4/A-record-only DNS
resolver is never mistaken for a streaming prerequisite; separate resolver
coverage exists where a real name lookup is actually required. Replay the
full, unchanged contract on Windows/x64, Linux/x86_64, and macOS/arm64.

**Windows/x64 done 2026-09-29.** Public, additive API in
`crtmedia/tls.h`: `crtmedia_tls_options { ca_pem,
insecure_skip_verify_for_local_development }`, taken by
`crtmedia_extractor_create_from_url_with_tls()` and
`crtmedia_muxer_create_for_url_with_tls()` (the existing entry points are the
`NULL`-options case, so nothing that worked before changes meaning). Policy as
frozen above: peer and host/IP verification always on
(`CURLOPT_SSL_VERIFYPEER=1`, `CURLOPT_SSL_VERIFYHOST=2`, restated explicitly),
the caller's PEM is the only trust anchor (`CURLOPT_CAINFO_BLOB`), the sole
opt-out is the loudly named `insecure_skip_verify_for_local_development`, only
`http`/`https` are spoken (redirects included), and the options are deep-copied
and reused by every reconnect and seek. A refused certificate is
`CRTMEDIA_ERROR_PROTOCOL`; no curl, mbedTLS or host TLS type is in any public
header.

`crtmedia_https_test` runs against `tests/tls_test_server.c`: an mbedTLS
(3.6.7, the production stack) TLS 1.2 terminator over CRT sockets that
generates, in memory at test time, an ECDSA P-256 CA, an unrelated CA, and a
CA-signed server certificate (SAN `IP:127.0.0.1`, validity now-1d..now+1d),
then relays decrypted bytes to the plain Tranche 2/3 loopback servers -- only
after a completed handshake. Real result, 4 consecutive identical runs, ~1.1 s:

```text
crtmedia_https_test: ok correct_ca=pass wrong_ca=protocol wrong_san=protocol
    default=protocol insecure_opt_in=pass upload_correct_ca=pass
    upload_wrong_ca=protocol samples=70
```

| Case | Required outcome | Result |
| --- | --- | --- |
| correct CA | connects; sample stream identical to the local file | pass (70/70) |
| unrelated CA | `CRTMEDIA_ERROR_PROTOCOL`, zero completed handshakes | pass |
| correct CA, cert covers only `10.255.255.1` | `CRTMEDIA_ERROR_PROTOCOL`, zero completed handshakes | pass |
| `NULL` options / empty options | refused (no implicit trust store) | pass |
| explicit insecure opt-in vs. the mismatching cert | connects | pass |
| `http://` with TLS options set | unaffected | pass |
| upload, correct CA | 2xx, 200 KiB byte-exact after TLS | pass |
| upload, unrelated CA | `CRTMEDIA_ERROR_PROTOCOL`, backend saw 0 connections (body never sent) | pass |

The matrix proves *authentication*, not only decryption: the correct-CA case
shows the TLS path works end to end with the same fixture, so each refusal is
attributable to the CA or SAN check alone. Deterministic tests use the literal
IP `127.0.0.1`; the minimal UDP/IPv4/A-record resolver is not a prerequisite
and no real-name resolution is exercised here. Full in-tree `ctest`: 161/161
(one expected `crtmedia_capture_mf_test` no-webcam skip). Linux/x86_64 and
macOS/arm64 replay of the unchanged contract is next (Tranche 5 closes only
when all three hosts pass).

**macOS/arm64 done 2026-09-29.** Same, unmodified source and fixture (the
in-memory mbedTLS CA/server-certificate generation is host-neutral).
`crtmedia_https_test` passed 4 consecutive runs with the identical result
line as Windows -- `ok correct_ca=pass wrong_ca=protocol wrong_san=protocol
default=protocol insecure_opt_in=pass upload_correct_ca=pass
upload_wrong_ca=protocol samples=70` -- covering every row of the matrix
above, including the refused-certificate upload never sending its body. The
Tranche 1-4 network tests (`transport_queue`, HTTP range/chunked input, HTTP
output, reconnect) passed together unchanged. Full in-tree `ctest`: 151/151
(one expected `crtmedia_capture_avfoundation_test` camera-authorization
skip); tooling tests 77/77.

**Linux/x86_64 done 2026-09-29, on the physical Linux host.** Same
production source (`crtmedia/tls.h`, `http_*`, the mbedTLS-backed curl
stack already in this tree); unlike Windows/macOS it needed one *test-fixture*
fix. First run: `crtmedia_https_test` exited 141 (SIGPIPE) with no output.
`strace` showed the cause exactly: in the deliberate refused-certificate
cases (wrong CA / wrong SAN) libcurl aborts the handshake, and the fixture
(`tls_test_server.c`'s BIO send, and its plain-send fallback) then wrote to
the already-closed socket with `send(..., 0)`, which on Linux raises SIGPIPE
and silently kills the whole test process. libcurl's own sends already
passed `MSG_NOSIGNAL` (visible in the trace), so no production path was
exposed; Windows has no SIGPIPE and macOS did not hit it. Fixed in the
fixtures only: a small shared `tests/test_socket_flags.h` defines
`CRTMEDIA_TEST_SEND_FLAGS` (`MSG_NOSIGNAL`, falling back to the Linux ABI
value 0x4000 because this project's public `<sys/socket.h>` does not
currently expose `MSG_NOSIGNAL` -- a Bionic-surface gap noted here, not
widened as part of this tranche; other hosts keep 0), used at all four
fixture send sites (`tls_test_server.c` x2, `http_test_server.c`,
`http_upload_test_server.c` -- the latter two shared the same latent risk),
and the header was added to `tools/create_stage_source.py`'s registry in the
same change. After the fix, 4 consecutive runs and then 25/25 in a loop gave
the identical result line as Windows and macOS -- `ok correct_ca=pass
wrong_ca=protocol wrong_san=protocol default=protocol insecure_opt_in=pass
upload_correct_ca=pass upload_wrong_ca=protocol samples=70`, ~0.8 s --
covering every row of the matrix. The Tranche 1-4 network tests
(`transport_queue`, HTTP range/chunked input, HTTP output, reconnect) passed
together unchanged (6/6 with HTTPS). Full in-tree `ctest`: 144/145 (the one
failure is the pre-existing no-sound-card `crtmedia_playback_pipeline_
test_runs` gap, unrelated); tooling tests 77/77. This tranche is now closed
on all three hosts.

### 6. Lifecycle and isolated-package acceptance

Stress repeated connect/stream/cancel/reconnect/destroy cycles (matching
Encode & Capture Tranche 6's own real-device lifecycle-stress precedent,
[`archived HISTORY.md`](https://github.com/webos21/crt/blob/4e5eead68048723c37e46c22d80bca43915ac093/HISTORY.md) 2026-09-28), audit sockets/threads/native handles for leaks,
and keep local file playback plus capture/encode green throughout. Rebuild
the isolated `04-gfx-media` stage from its predecessor SDK with every new
private dependency and prerequisite declared (`tools/
create_stage_source.py`'s file registry, `tools/crt_dist_prerequisites.py`
-- both had real, silent staleness gaps found closing Encode & Capture
Tranche 6; do not repeat that gap for networking), rebuild an installed
HTTP streaming consumer, run `verify_dist.py` plus PE/ELF/Mach-O
dependency/RPATH checks, and publish only after all three hosts pass.

**Windows/x64 done 2026-09-29.**

*Lifecycle stress.* `crtmedia_http_lifecycle_test` cycles every network path in
one process and audits native resources around each scenario and overall
(Windows `GetProcessHandleCount()`, Linux `/proc/self/fd` + `/proc/self/task`,
macOS `/dev/fd`; a warm-up pass first so one-time library initialization is not
counted). Real result, 3 identical runs, ~13 s:

```text
crtmedia_http_lifecycle_test: ok transport=40 extractor=30 reconnect=8
    upload=20 tls=15 tls_refused=15 handles=147->139 threads=-1->-1
```

Scenarios: transport open/read/cancel against an 8 MiB body through a 32 KiB
queue (the worker is blocked mid-transfer at every cancel); extractor
open/read/release mid-stream; reconnect cycles (server drops twice, resumes,
result byte-exact vs. the local file, then destroy); upload open/stream/cancel
without `finish()`; TLS authenticated mid-stream release; refused certificates
stay refused every cycle. The audit is meaningful: skipping the transport
`close()` in the test makes it fail (`handle/fd count grew 154 -> 254`). Full
in-tree `ctest` 162/162 (one expected no-webcam skip), which keeps local file
playback, capture, encode, timing-discontinuity and hardware-decode tests green
throughout.

*Isolated `04-gfx-media` stage.* Built from the freshly regenerated Windows
`03-gfx-simple` SDK with `tools/crt-stage-build.py` (curl layer 1446.5 s at
`-j4`; FreeType/FFmpeg 2799.5 s and Skia 212 s on the first run, then reused
from the work-root cache in ~3 s on the rerun, proving the new layer did not
invalidate them). The stage's CMake enables `CRTMEDIA_ENABLE_CURL`
unconditionally, so a missing port is an error rather than a silent
`CRTMEDIA_ERROR_UNSUPPORTED`; it runs the networking suite from the isolated
source asset (17/17 stage tests including transport queue, HTTP input
range/chunked, output, reconnect, HTTPS matrix and lifecycle), installs the new
`examples/media-stream` consumer, and `tools/build_stage_04_gfx_media.py`
rebuilds it externally against the packaged SDK and runs it against a real host
HTTP/1.1 server: `crtmedia_stream_example: samples=70 ... seekable=1
size_known=1`. `verify_dist.py` (extended) requires the curl/mbedtls/zlib
redistributed-dependency records, headers, static libraries, `crtmedia/tls.h`
and the example; the PE dependency scan is clean (`libcrtmedia.dll` imports
ole32/MFPlat/MFReadWrite/MF/KERNEL32/synch only, nothing new). Published
atomically.

Real gaps found and fixed (the class this section warned about):
`tools/create_stage_source.py` bundled only `freetype.json`/`ffmpeg.json`, so
the stage script died reading `curl.json` on the first isolated run; the
`curl`/`mbedtls`/`zlib`/`make` recipes and (Windows) `mbedtls-windows-exclude-
symbols.rsp` are now bundled. `tools/crt_dist_prerequisites.py` needed no new
entry on Windows (curl/mbedTLS/zlib are static, no new runtime library). The
stage ran with checksum-verified tarballs copied from the in-tree download
cache because android.googlesource.com was returning HTTP 503 for the `make`
port at the time.

Linux/x86_64 and macOS/arm64 replay is next, including the Linux (static FFmpeg
+ curl chain + host libva) and macOS branches of
`examples/media-stream/CMakeLists.txt`, which are written but not yet run.
Tranche 6 -- and with it this roadmap item -- closes only when all three hosts
pass.

**macOS/arm64 done 2026-09-29.** Two real macOS-only bugs found and fixed:

- *Fixture SIGPIPE.* `crtmedia_http_lifecycle_test` died with exit 141 and no
  output: its transport-cancel cycles abandon an 8 MiB body mid-send, and the
  loopback fixtures' `send()` raised SIGPIPE. Linux already used `MSG_NOSIGNAL`
  (`tests/test_socket_flags.h`) but macOS has no such flag. The header now
  provides `crtmedia_test_send()` (Linux `MSG_NOSIGNAL`; macOS ignores SIGPIPE
  once, fixture-only; Windows plain `send`), used by all three fixtures.
  Result, 3 identical runs: `ok transport=40 extractor=30 reconnect=8
  upload=20 tls=15 tls_refused=15 handles=6->4` (`/dev/fd` audit, no growth).
- *Non-relocatable curl layer.* The isolated stage got through 17/17 stage
  tests and the installed `examples/media-stream` consumer (`samples=70
  bytes=17375 tracks=2 seekable=1 size_known=1` against a real host HTTP
  server), then `verify_dist.py` rejected the new curl/mbedTLS/zlib dylibs:
  configure/make installed them with absolute `LC_ID_DYLIB`/`LC_LOAD_DYLIB`
  paths into the stage's temporary SDK directory, and `bin/curl` had `@rpath`
  dependencies with no portable RPATH. `tools/build_stage_04_gfx_media.py`
  gained `portable_macho_dependency_paths()`, run after the existing RPATH
  canonicalization: ids/dependencies under the staged root become
  `@rpath/<name>`, `bin/` executables get `@executable_path/../lib`, static
  archives are skipped. The published tree now shows `@rpath/libcurl.4.dylib`
  ids and an `@executable_path/../lib` RPATH on `bin/curl`.

Stage rebuilt from the freshly regenerated `03-gfx-simple` SDK
(FreeType/FFmpeg 383.8 s, Skia 62.1 s, curl/mbedTLS/zlib layer 102.7 s, 612.6 s
total), `verify_dist.py` passed, published atomically. As on Windows,
android.googlesource.com was returning HTTP 503 for the `make` port, so the
checksum-verified tarballs were seeded from the in-tree download cache.
Full in-tree `ctest` 152/152 (one expected camera skip); tooling 77/77.

**Linux/x86_64 done 2026-09-29, on the physical Linux host -- Tranche 6 and
this whole roadmap item are now closed on all three hosts.**

*Lifecycle stress.* `crtmedia_http_lifecycle_test`, unmodified, 3 consecutive
identical runs (~4.5 s each): `ok transport=40 extractor=30 reconnect=8
upload=20 tls=15 tls_refused=15 handles=6->4 threads=3->1` (`/proc/self/fd` +
`/proc/self/task` audit; no fd or thread growth across cycles). No SIGPIPE
problem here -- the Tranche 5 fixture fix (`MSG_NOSIGNAL`) already covers it.

*Isolated `04-gfx-media` stage.* Rebuilt from the freshly regenerated
`03-gfx-simple` SDK with `tools/crt-stage-build.py` (FreeType/FFmpeg
718.4 s at `-j12`, Skia 149.1 s, curl/mbedTLS/zlib layer 109.9 s, 1012.5 s
total). 19/19 stage tests (the whole networking suite plus everything
before it), all packaged examples including the new `examples/media-stream`
consumer -- whose Linux branch (static `libcrtmedia.a` + FFmpeg + the curl
chain + host libva) had been written but never run: `crtmedia_stream_example:
samples=70 bytes=17375 tracks=2 seekable=1 size_known=1`, identical to macOS
-- and the zero-copy bridge (`interop=zero-copy ... cpu_readback=no ...
pixel_check=pass`). `verify_dist.py` passed and the SDK published atomically.
The ELF scan needs no new prerequisite entry: `libcrtmedia.so` still imports
only libva/libva-drm plus the CRT libs (curl/mbedTLS/zlib are linked in
statically), and every packaged runpath is relocatable.

Two real Linux-only packaging gaps found and fixed (the class this section
warned about; neither is visible to an in-tree build):

- *Non-relocatable `bin/curl`.* curl's own `make install` gives the `curl`
  executable `RUNPATH=<temporary stage sdk>/lib` and nothing else, unlike the
  libraries (which keep `$ORIGIN`). The Linux cleanup step
  (`remove_staged_absolute_elf_rpaths()`) correctly refuses to strip the only
  usable runtime path (`ValueError: refusing to remove the only usable runtime
  path from .../bin/curl`), so the first isolated run died there after
  19/19 stage tests. The ELF counterpart of the macOS `@executable_path/../lib`
  fix: new `crt_elf.relocate_absolute_runtime_paths()` rewrites an executable's
  all-absolute runpath in place (never growing the file) to `$ORIGIN/../lib`,
  applied to `bin/` before the existing cleanup; two new unit tests in
  `tools/test_crt_elf.py` (including that the normal cleanup then no-ops and
  that a slot with `$ORIGIN` is left alone). The published `bin/curl` now
  shows `RUNPATH [$ORIGIN/../lib]`.
- *`examples/media-player` no longer linked.* The previously passing example
  links the static `libcrtmedia.a`, whose extractor/muxer now always carry the
  HTTP transport, so the packaged rebuild died with `undefined reference to
  curl_easy_cleanup`/`curl_slist_free_all`. Windows and macOS were unaffected
  (they link the shared library, which embeds curl). Added the same
  libcurl -> mbedTLS -> zlib chain, in the same order, as `examples/media-
  stream` uses to the Linux branch of `examples/media-player/CMakeLists.txt`.
  These were found in two consecutive isolated reruns, each needing the
  shipped tools/examples regenerated (`crt-gfx-simple-dist`) first.

Full in-tree `ctest`: 145/146 (the one failure is the pre-existing
no-sound-card `crtmedia_playback_pipeline_test_runs` gap, unrelated); tooling
79/79 (77 plus the two new ELF tests).

Publication of release assets remains a separate, deliberate, manual step;
nothing was uploaded or tagged.

## Non-goals for the current tranche

- RTSP, WebRTC, or adaptive streaming (HLS/DASH) -- later consumers of this
  same bounded transport, not completion gates here.
- Resumable streaming **upload** (Tranche 4 defines input resume only;
  output failure is honest, not silently retried).
- A full HTTP client feature surface (proxies, cookies, arbitrary auth
  schemes) beyond what the loopback acceptance fixtures above require.
- Replacing or extending the current minimal DNS resolver; loopback/IP-
  literal testing is deliberate, not a gap being deferred.
