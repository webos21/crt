# macOS Development SDK Preview v0.0.1-preview.20261110.macos.1

This prerelease offers the reduced Development SDK for **native macOS/arm64**.
Its manifest stage is `02-cxx`, profile `dev`: CRT C/C++ runtimes, public compiler
policy, networking/JSON/compression dependencies and their notices. The LLVM
toolchain and the macOS SDK are external prerequisites; the exercised toolchain is
Xcode's Apple clang **21.0.0** with its `ld` (ld-27106.1) on macOS **27.2** (SDK 27.2).

Build/source commit: `445f127dc2e1ef4ab538166aa2e374f25773a803` (clean Release build).
[Source tree](https://github.com/webos21/crt/tree/445f127dc2e1ef4ab538166aa2e374f25773a803) ·
[Tagged source archive](https://github.com/webos21/crt/archive/refs/tags/v0.0.1-preview.20261110.macos.1.tar.gz).
The SDK retains dependency recipes, licenses and upstream source pins; the 02/03
source assets below retain the exact integrated sources and successor recipe.

## Downloads and trusted pins

[Release page](https://github.com/webos21/crt/releases/tag/v0.0.1-preview.20261110.macos.1). Verify the manifest against this independently obtained
pin **before** accepting its archive/contract hashes:

- `release-manifest-dev-macos-aarch64.json`: **0981110c84e5d24a903dbd3965dbe92eb2fe405edfa89804e3086dfeb75983de** (1728 bytes).
- Installed SDK content identity: `e3dc7fb1810a3f78ab8ef33882225ccb562d21f6f5977a2037ab92819326bac8`.

| Asset | Bytes | SHA-256 |
| --- | ---: | --- |
| `crt-v0.0.1-preview.20261110.macos.1-macos-aarch64-dev.tar.xz` | 6136776 | `41a1afe701514496a912de83e46481455c9f2c0526d2eadd5b058813c26b2146` |
| `crt-v0.0.1-preview.20261110.macos.1-macos-aarch64-dev-toolchain.json` | 402864 | `d5a100fdd0d38f940fe1c398996f013d57ccc7c4743af2b9ab4988c42b8de464` |
| `crt-v0.0.1-preview.20261110.macos.1-macos-02-cxx-source.tar.xz` | 9005764 | `8a1beeaf48f9e9f25af60ed50f067754b4f6b674b8b43589b4c1f4e7c355fc3a` |
| `crt-v0.0.1-preview.20261110.macos.1-macos-03-gfx-simple-source.tar.xz` | 134120 | `e2d4fc76ca3c5ff378e883ea29565a2a5ba3a5b70d6dc9899c23724fbbf02378` |

The release also carries the original `SHA256SUMS-dev-macos-aarch64` and release
manifest. The tagged CRT tree supplies project sources/build glue; upstream
sources remain identified by the SDK's recorded URLs and digests.

## Trust and supported use

This Preview uses **manual, explicitly pinned metadata** obtained from the
maintainer's `webos21/crt` repository, verified HTTPS with a user-selected CA file,
and exact archive/contract/content checks. It carries no publisher signature;
CLI `publisher_authentication` remains `not_verified`. This is not an automatic
channel, signing-key bootstrap, expiry or rollback-protection protocol.
See [Preview trust policy](../design/dev_preview_trust.md).

GitHub release downloads redirect to GitHub asset storage. Current `crt-cli`
rejects HTTPS redirects; download with a browser or HTTPS client, verify the
fixed manifest pin, then use native `sdk install`. The current public GitHub URL
is not advertised as a working `sdk acquire --source` endpoint.
The [published SDK workflow](../guides/dev_sdk_preview.md#macos-arm64) specifies the
CLI and explicit toolchain inputs. CLI executable publication is a separate step.

C tests pass 146/146; C++ runtime/string-ABI/iostream pass static/shared, Dev
HTTPS/inflate and hidden-source consumers pass. A `crt-cli` built from its current
sources by this SDK alone passes native 922/922, archive 221/221, SDK-store
114/114 and recipe 141/141. Actual public-byte verification is recorded in
[SDK acceptance](../acceptance/sdk_split_acceptance.md#macos-dev-sdk-public-promotion-2026-11-10).

## Scope limits

Only the macOS/arm64 Dev artifact is offered here; the Linux/x86_64 Preview is a
separate release. Full/GFX/Media/UI/Web SDK binaries, other host packages,
independent host-tools archives, general updates and CLI self-upgrade are
separate. The macOS Mach-O images carry only the linker's ad-hoc signatures; nothing is Developer ID signed or notarized. The CLI
compiler policy's pure C shared SDK-provider mode is unqualified; use the
qualified static C/C++ workflows (source-port shared proof does not endorse that
mode). Existing SDKs, project pins and the withdrawn earlier graphics preview are
not replaced. Every future advertised host requires its own final-package proof.
