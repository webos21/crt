# Development SDK Preview {{TAG}} ({{TARGET}})

This is the **first Development SDK Preview produced by the automated release workflow** (`sdk-release.yml`): it was built from a
clean commit on `main` by GitHub Actions, verified from its bytes on a second runner, installed and exercised by the published
CRT-CLI Preview, and published only after a reviewer approved the `release` environment. Earlier Previews were built and published
by hand. It is stage `02-cxx`, profile `dev`; use **LLVM/Clang/LLD 21** as external host tools.

**Changes relative to `v0.0.1-preview.20261110.4`.** None intended in the Linux/x86_64 SDK's sources, ABI or native policy: the
CRT libraries, headers, recipes and the shipped tools are unchanged by the commits between the two. What differs is how it was
produced and verified (above), the tag and content identity that follow from a new build, and, for other targets only, macOS test-environment and
Linux/aarch64 configure fixes that do not alter this archive's inputs. Treat it as a re-issue of `.4` with automated provenance, not as a
functional upgrade; adopt it only if you want that provenance.

[Producer source](https://github.com/webos21/crt/commit/{{COMMIT}}) · [Release page](https://github.com/webos21/crt/releases/tag/{{TAG}}) ·
[Release procedure](https://github.com/webos21/crt/blob/{{COMMIT}}/docs/guides/sdk_ci.md#releasing-a-dev-sdk-preview-sdk-releaseyml).

## Downloads and trusted pins

Obtain the manifest pin independently from this reviewed repository or the owner's signed announcement before accepting the payload hashes
below. **Publisher authentication is not part of this publication.** The owner signs the archive offline after publication and attaches
`<archive>.sig`; until that file exists this release is unauthenticated, and the consumer pin (crt-cli) is not updated before then.

- Manifest SHA-256: **{{MANIFEST_SHA256}}** ({{MANIFEST_BYTES}} bytes).
- Installed SDK content: `{{CONTENT_SHA256}}`.

{{ASSET_TABLE}}

The other files are `release-manifest-dev-linux-x86_64.json` and `SHA256SUMS-dev-linux-x86_64`.

## Qualification and limits

Before publication the workflow re-derived the asset set from its bytes (sizes and digests, manifest identity, safe extraction, install
metadata, portable toolchain contract bound to the content identity) and, on a separate runner, the pinned published
`crt-cli` Preview `v0.1.0-preview.20261116.linux-x86_64.1` installed the archive from the manifest pin and built, tested and ran a C and a
C++ project, cold and warm. The same build path passes the four-host SDK integration, including the external-consumer proof with the
producer checkout hidden. Run identifiers and what was and was not exercised are in the
[acceptance record](https://github.com/webos21/crt/blob/{{COMMIT}}/docs/acceptance/release_qualification.md).

Not exercised for this Preview: Ubuntu 24.04/glibc 2.39 (the repair target of `.4`; its evidence was not re-run here), Linux/aarch64, macOS,
Windows, Full/GFX/Web SDKs, and publisher signature verification. Install it alongside existing SDKs and select the new identity explicitly;
existing project locks are not migrated.
