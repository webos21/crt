# Use the Linux Development SDK Preview

The [Preview release](https://github.com/webos21/crt/releases/tag/v0.0.1-preview.20261110.3) contains the native
Linux/x86_64 Dev SDK `v0.0.1-preview.20261110.3`. LLVM/Clang/LLD are
external inputs; this combination is qualified with LLVM **21.1.8**. The SDK
retains stage `02-cxx`, profile `dev`. Full and other-host packages are separate.

## Download and trusted metadata

Use a trusted HTTPS client/browser and an explicitly selected trusted CA input.
Keep these three original files together in a fresh directory:

- [SDK archive](https://github.com/webos21/crt/releases/download/v0.0.1-preview.20261110.3/crt-v0.0.1-preview.20261110.3-linux-x86_64-dev.tar.xz)
- [Portable native compiler policy](https://github.com/webos21/crt/releases/download/v0.0.1-preview.20261110.3/crt-v0.0.1-preview.20261110.3-linux-x86_64-dev-toolchain.json)
- [Release manifest](https://github.com/webos21/crt/releases/download/v0.0.1-preview.20261110.3/release-manifest-dev-linux-x86_64.json)

The independently trusted manifest SHA-256 from the reviewed repository is
`d4211d3298a764ff22f6e262073f834bbb36cb5d07c22e1ecb10f7f83f8fca60` (1683 bytes). Obtain that pin through
your trust in the maintainer's repository; do not adopt a checksum merely because
it was served beside an untrusted payload. The [trust policy](../design/dev_preview_trust.md)
defines the Preview's manual pin/TLS boundary. Publisher signatures/default
channels are not supplied. Component notices and source pins are inside the SDK;
the release tag and 02/03 assets retain the corresponding project/upstream inputs.

GitHub asset URLs redirect to GitHub storage. The current CLI intentionally
rejects redirects, so use manual download plus `sdk install` below. This release
does not advertise GitHub `sdk acquire --source` or automatic first-build download.
The CLI app/build paths themselves require no Python, shell or CMake.

## Install and build without configure

Use the CLI baseline from [consumer acceptance](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/preview_acceptance.md#linux-local-candidate-2026-11-10).
The qualified executable SHA-256 is
`6ba046e5baeae24b86e407ad04353ec9d98b989c21921160413408ca0f71dab2`;
CLI executable publication is a separate step from this SDK release.
The SDK installer validates the pinned manifest before trusting its payload
hashes and validates the portable policy beside the archive. Substitute actual
absolute download/tool paths; SDKs install side by side in the user's store.

```text
crt-cli sdk install --archive /absolute/downloads/crt-v0.0.1-preview.20261110.3-linux-x86_64-dev.tar.xz --release-manifest /absolute/downloads/release-manifest-dev-linux-x86_64.json --release-manifest-sha256 d4211d3298a764ff22f6e262073f834bbb36cb5d07c22e1ecb10f7f83f8fca60
crt-cli new hello-cxx --language cxx
crt-cli build --project hello-cxx --sdk-id 36623957fbdd5a384cc173991e20253ff83148028ab8072298dcd434a55a2b3f --cc /usr/bin/clang --cxx /usr/bin/clang++ --linker /usr/bin/ld.lld --ar /usr/bin/llvm-ar-21
crt-cli test --project hello-cxx
crt-cli run --project hello-cxx
```

For C sources choose `--language c` and another fresh project name. `new` only
generates source/tests; the first build binds the installed SDK. Later plain
build/test/run reuse that project lock and selected tools. Installing another SDK
does not silently migrate existing project pins. Keep the compiler-policy asset
and manifest unchanged; failure leaves previous installed SDKs/selections intact.

Qualified scope is the recorded native first-build/installed-selection flow,
static C and C++ consumers. Pure C shared SDK-provider mode is unqualified;
source-port shared evidence is separate and requires independently selected CRT
host tools. General profiles/Full, updates, self-upgrade and registry resolution
remain later contracts. See [producer acceptance](../acceptance/sdk_split_acceptance.md#linux-dev-sdk-public-promotion-2026-11-10)
for the exact public-byte and installed-consumer evidence.
