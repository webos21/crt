# Development SDK Preview {{TAG}} ({{TARGET}})

This Linux/x86_64 Preview is the first qualification candidate for the SDK's
sign-before-publishing workflow. It is a `02-cxx`, profile `dev` SDK for use
with external LLVM/Clang/LLD 21. This release does not promote a consumer pin
or migrate an existing SDK installation or project lock.

[Producer source](https://github.com/webos21/crt/commit/{{COMMIT}}) ·
[Release page](https://github.com/webos21/crt/releases/tag/{{TAG}}).

## Downloads and trusted pins

The publisher signs every payload file offline, including the toolchain
contract, both source archives, the manifest and the checksum file. The
separately approved finalizer checks each detached `<asset>.sig` and the exact
staged inventory before publication, then checks the public downloads again.
[Verify with the independently pinned publisher key](https://github.com/webos21/crt/blob/{{COMMIT}}/docs/security/publisher_signing.md).

- Manifest SHA-256: **{{MANIFEST_SHA256}}** ({{MANIFEST_BYTES}} bytes).
- Installed SDK content: `{{CONTENT_SHA256}}`.

{{ASSET_TABLE}}

## Qualification and migration

Before staging, a fresh runner re-verifies the package and uses the pinned,
signature-checked Linux/x86_64 CLI Preview to install it and build, test and
run C and C++ projects, including warm rebuilds. This release does not qualify
other hosts, older Linux distributions, or the Full/GFX/Media/UI/Web SDKs.
Install alongside existing SDKs and select the new identity explicitly.
