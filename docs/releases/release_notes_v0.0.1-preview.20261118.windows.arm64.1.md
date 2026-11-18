# Development SDK Preview {{TAG}} ({{TARGET}})

This is the **first Development SDK Preview for native Windows/arm64** (`windows-11-arm`). It was built from a clean commit on `main` by
GitHub Actions (`sdk-release.yml`), verified from its bytes on a second runner, and published only after a reviewer approved the `release`
environment. It is stage `02-cxx`, profile `dev`; the external host tools are **LLVM/Clang/LLD 22.1.8** (the `woa64` build), the MSVC
developer environment and the Windows SDK import libraries for `arm64`.

**There is no earlier Windows/arm64 Preview, so there is nothing to migrate from.** The CRT libraries, headers, recipes and shipped tools are
those of the `main` commit named below; this Preview adds the Windows/arm64 target, not new API.

**This is a first-host bootstrap release.** No published `crt-cli` Preview exists for Windows/arm64 yet, and the first one has to be built from
this SDK, so the consumer smoke by a published CLI could not run. It is recorded, not skipped silently:

Consumer smoke by the published CLI: NOT RUN (first-host bootstrap)

In its place the workflow re-derived the asset set from its bytes on a second runner and ran the source-hidden native C/C++ consumer
against the extracted archive with the producer checkout inaccessible. That shows the archive is complete and usable without CRT's
sources; it does **not** show that `crt-cli` installs it. The next Windows/arm64 Preview runs the normal smoke once a CLI Preview is pinned.

[Producer source](https://github.com/webos21/crt/commit/{{COMMIT}}) · [Release page](https://github.com/webos21/crt/releases/tag/{{TAG}}) ·
[Release procedure](https://github.com/webos21/crt/blob/{{COMMIT}}/docs/guides/sdk_ci.md#first-host-bootstrap-a-host-without-a-published-cli).

## Downloads and trusted pins

Obtain the manifest pin independently from this reviewed repository or the owner's signed announcement before accepting the payload hashes
below. **Publisher authentication is not part of this publication.** The owner signs the archive offline after publication and attaches
`<archive>.sig`; until that file exists this release is unauthenticated, and no consumer pin (crt-cli) is created before then.

- Manifest SHA-256: **{{MANIFEST_SHA256}}** ({{MANIFEST_BYTES}} bytes).
- Installed SDK content: `{{CONTENT_SHA256}}`.

{{ASSET_TABLE}}

The other files are `release-manifest-dev-windows-aarch64.json` and `SHA256SUMS-dev-windows-aarch64`.

## Qualification and limits

Before publication the workflow re-verified the packaged bytes (sizes and digests, manifest identity, safe extraction, install metadata,
portable toolchain contract bound to the content identity). The same build path passes the Windows/arm64 row of the SDK integration,
including the external-consumer proof with the producer checkout hidden. Run identifiers and what was and was not exercised are in the
[acceptance record](https://github.com/webos21/crt/blob/{{COMMIT}}/docs/acceptance/release_qualification.md).

Not exercised for this Preview: installation by `crt-cli` (above), publisher signature verification, Full/GFX/Web SDKs (Windows/arm64 has no
`06-web` collector scope), and other targets. Install it alongside existing SDKs and select the new identity explicitly; existing project
locks are not migrated.
