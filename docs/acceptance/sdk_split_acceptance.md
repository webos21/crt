# Full / Development SDK split acceptance

This record owns the current S2-S5 producer/consumer gates in
[TODO](../../TODO.md#crt-sdk-split-full-sdk-and-development-sdk). Content and
identity policy belong to the [SDK contract](../design/sdk_development_contract.md#full-sdk-and-development-sdk-adopted-direction-2026-10-22);
commands belong to the [distribution guide](../guides/distribution.md#development-sdk-producer-and-external-validation).
Original B4/B5 results remain in [build-system acceptance](build_system_acceptance.md#development-sdk-windows-replay).

## Remaining matrix

| Pair | Development producer / ordinary consumer | Producer-tree-hidden consumer |
| --- | --- | --- |
| Linux/x86_64 | Current producer and static/shared eight-case replay pass below | Pass below |
| macOS/arm64 | Recorded producer/profile and static four-case replay at the [macOS baseline](build_system_acceptance.md#macosarm64-native-replay-b5-2026-10-23); no new shared-consumer acceptance inferred | Not run |
| Windows/x64 | Recorded producer/profile, static/shared eight cases, app-local imports and relocation at the [Windows baseline](build_system_acceptance.md#development-sdk-windows-replay) | Not run |
| Linux/aarch64 | Native producer, archive/extraction, static/shared eight-case replay and port recipe tests pass below | Pass below, using the extracted archive |
| Windows/arm64 | Development producer compile/package replay not run | Compile-only until exercised natively |

S4's local collector implementation and negative/atomic fixtures are complete.
Actual clean/tagged positive collection remains the independent
[release qualification](../../TODO.md#release-qualification-and-assembly-rehearsal)
gate. No tag, upload, consumer lock or runtime adoption is part of this record.

## Linux/aarch64 native producer and package replay (2026-10-27)

The native aarch64 host completed the ordinary Development producer entry point:

```sh
cmake --build --preset linux-host-ninja-debug \
  --target crt-development-sdk-test --parallel 4
```

The existing C/C++ build was updated incrementally, including imported libc++
and newly generated `01-c`/`02-cxx` SDKs; no upper-stage rebuild was needed.
The plain predecessor, producer and retained port compile provenance all identify
clean CRT `805d51012d409121ba27971039278e43087551e4`. The predecessor now declares
the architecture-specific `linux-elf-loader` prerequisite. Fresh port work
`out/linux-host-ninja-debug/development-sdk-work/dev-6pq_eoq6` compiled and
installed zlib 1.3.1, Mbed TLS 3.6.7 and curl 8.21.0 through the shared media
producer functions. Ordinary distribution and Development profile verification
pass, with no graphics/media/UI/Web payload. All **23** inherited runtime-file
digests match plain `02-cxx`.

All **six** recipe tests pass on this device: static/shared zlib round trips,
Mbed TLS SHA-256/AES checks and curl external HTTP/HTTPS round trips
(`http=200 https=200`). The SDK-only C++17 reference consumer additionally
passes **eight** cases: verified HTTPS+inflate and wrong, missing and malformed
explicit CA, for static and shared linkage. The missing-CA case rejects even
with a valid CA supplied through ambient environment variables.

The existing `prepare_release_assets.archive_directory` helper archived the
local `development` candidate without invoking the release collector. Extraction
to `out/dev-sdk-linux-aarch64-20261027/extracted SDK with spaces/dev` passes
`verify_dist.py --stage 02-cxx --profile dev`. Archive identity:

- File: `archives/crt-development-linux-aarch64-dev.tar.xz` under the evidence root below.
- Size: **8,831,520 bytes**.
- SHA-256: `4d3d05a292db88f67290f713b07d0d256f84e5b0da63f60030eec0a6bb6f7be7`.
- Original and extracted inventory: **2,579** files/symlinks, inventory SHA-256
  `b80e906e40877158991f32219596dc21d4e0444bafcd9f35952fcc87c18f5ed1`.

The extracted package then passed the same source-hidden acceptance procedure:

```sh
python3 tools/test_dev_sdk_external.py \
  --sdk-root 'out/dev-sdk-linux-aarch64-20261027/extracted SDK with spaces/dev' \
  --output out/dev-sdk-linux-aarch64-20261027/external-retry
```

Bubblewrap masks the whole producer checkout, including original/extracted
SDKs and build outputs. The visibility probe confirms the hidden root is empty;
both linkage modes build and run from the relocated package outside the checkout,
passing all **eight** trust/inflate cases. Static linkage has no dynamic imports.
The shared recursive closure contains **11 SDK images** and only the declared
`ld-linux-aarch64.so.1` OS boundary. Original, extracted and externally relocated
SDK inventories match and remain unchanged.

Private-copy profile negatives reject missing static curl, missing shared curl
and changed inherited libc bytes. The old local `05-ui` Full SDK is correctly
rejected by the collector's clean-source pair check; it was not regenerated or
relabeled. The existing Development boundary/profile/release unit suite passes
**17/17**. No producer code or public contract/schema change was required, and
CLI/Studio adoption baselines remain those in the review below. This completes
the Linux/aarch64 S2 replay and supplies native S3 evidence, not clean/tagged
release qualification or a matching new Full/Development release pair.

Evidence root: `out/dev-sdk-linux-aarch64-20261027/`, retaining
`producer-entry.log`, `producer-entry-network-retry.log`, `recipe-tests.log`,
`package.log`, `package-result.json`, `external-driver-retry.log`,
`external-retry/{command.json,consumer.log,result.json}`,
`negative-profile{.log,-result.json}` and `unit.log`. The ordinary consumer receipt
is `out/linux-host-ninja-debug/development-consumer/result.json`. The successful
external work directory is retained in its receipt. The initial sandbox attempts
failed at download DNS and user-namespace creation respectively; their logs
remain alongside the successful permission-enabled retries. Isolation was not
bypassed and there was no unisolated fallback.

## Linux source-hidden replay (2026-10-27)

Fresh ports were compiled through the existing producer, reusing the media
stage's networking build and dependency-record functions:

```sh
python3 tools/build_dev_sdk.py \
  --sdk-root out/linux-host-ninja-debug/dist/02-cxx \
  --output out/dev-full-sdk-20261027/dev \
  --work-root out/dev-full-sdk-20261027/producer-work --jobs 6
python3 tools/test_dev_sdk_external.py \
  --sdk-root out/dev-full-sdk-20261027/dev \
  --output out/dev-full-sdk-20261027/external
```

`producer.log` exits 0 and ordinary plus Development profile verification pass.
Compile and producer identity are clean CRT
`181f74b2aaf644c82e21a57e4325355964ede3c0`. The supplied plain `02-cxx`
predecessor retains its own source identity
`857f8313199bd4e9dc1c6d8cc98c01743c80fe40`; no metadata was relabeled.

The new Linux-only acceptance driver copies the SDK and the reference consumer
to a fresh directory outside the checkout. Bubblewrap masks the original SDK
and the whole producer checkout, including its `out/` build roots, with empty
mounts. A child probe checks that the masked roots are empty before validating
the relocated profile, compiling or executing the consumer. Build-host Clang,
Python, OpenSSL and OS tools remain available. An eleven-script validation
capsule supplies only the reference fixture's local imports, with no CRT
headers/runtime objects, `build_dev_sdk.py` or `crt-libcxx-build.py`. The receipt hashes those scripts.

With the producer and original SDK inaccessible, fresh static and shared C++17
consumers compile using the relocated SDK wrappers, headers and libraries. Each
passes verified HTTPS+inflate, wrong CA, missing explicit CA despite ambient
valid-CA variables, and malformed CA: **eight cases**. PKI is generated fresh
for the local loopback server. The static consumer has no dynamic imports; the
shared consumer's actual recursive import closure contains **11 SDK images**
and only the declared `ld-linux-x86-64.so.2` OS boundary.

Both the original and relocated SDK retain all **2,579** file/symlink inventory
entries, with inventory SHA-256
`89e659fe1b257129fddc9979bdedc319ee6b2a30c18fe8e464449e8bac1490e7`.
Evidence: `out/dev-full-sdk-20261027/{producer.log,external-driver.log,
external/command.json,external/consumer.log,external/result.json}`. The result
also retains the external work location and confirms SDK payloads are unchanged.
Missing bubblewrap or failed namespace creation fails this gate; there is no
unisolated fallback. Cross-target SDKs are rejected before execution.

### Runtime correspondence and negatives

`pair-and-negative-results.json` records equality of all **23** inherited
runtime files between Dev, plain `02-cxx`, Full `05-ui` and Full `06-web`.
Private SDK copies exercise profile rejection of missing static curl, missing
shared curl and changed inherited libc bytes. Original artifacts are untouched.
The actual Dev/Full pair above is also rejected by the collector's pair check
because producer and Full/base source commits differ, despite equal runtime
bytes. This is a local development candidate, not a matching clean release pair.

The existing CI tooling suite includes regressions for foreign-target execution
rejection, hidden/work-tree overlap, stale-success receipt removal and the
bounded validation capsule. All **17** Development profile/release/boundary unit
tests pass (`unit.log`); the full existing CI tooling selection passes
**115/119**, with four conditional skips (`ci-tooling-tests.log`).
Linux/amd64 PR CI now builds/tests the Dev SDK and
runs the source-hidden driver; other matrix hosts keep bounded offline fixtures.
The CI step is implemented; a hosted GitHub run is **Not run** in this replay.

## Producer/consumer review

The producer owner is CRT's `build_dev_sdk.py`, profile verifier, reference
consumer and external acceptance driver. Review baseline: CRT `181f74b`;
consumer baselines remain CLI `5fcdc995d231fe2f9dbab4292000a81e0b4fce93` and
Studio `27548e1c6d29b5a63d7eb6adce8aaf47b2f5d807`, with no approved runtime
combination in CLI's `docs/compatibility.md`. This adds an acceptance procedure,
not a new SDK/CLI wire contract: SDK manifest/profile format 1, port recipe
version 1 and isolated-stage recipe version 2 remain unchanged. The general
SDK development-contract v1 remains proposed. CLI/Studio live integration is
**Not run**; artifact promotion precedes any later consumer lock update.
