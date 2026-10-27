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
| Linux/aarch64 | Development producer compile/package replay not run; previous Full cross-build is not this profile | Compile-only until exercised natively |
| Windows/arm64 | Development producer compile/package replay not run | Compile-only until exercised natively |

S4's local collector implementation and negative/atomic fixtures are complete.
Actual clean/tagged positive collection remains the independent
[release qualification](../../TODO.md#release-qualification-and-assembly-rehearsal)
gate. No tag, upload, consumer lock or runtime adoption is part of this record.

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
