# Linux Development SDK Preview v0.0.1-preview.20261110.3

This prerelease offers the reduced Development SDK for **native Linux/x86_64**.
Its manifest stage is `02-cxx`, profile `dev`: CRT C/C++ runtimes, public compiler
policy, networking/JSON/compression dependencies and their notices. LLVM is an
external prerequisite; the exercised toolchain is LLVM/Clang/LLD **21.1.8**.

Build/source commit: `0ab8f8cfbbde19e14dd5861ba176fb8881e06182` (clean Release build).
[Source tree](https://github.com/webos21/crt/tree/0ab8f8cfbbde19e14dd5861ba176fb8881e06182) ·
[Tagged source archive](https://github.com/webos21/crt/archive/refs/tags/v0.0.1-preview.20261110.3.tar.gz).
The SDK retains dependency recipes, licenses and upstream source pins; the 02/03
source assets below retain the exact integrated sources and successor recipe.

## Downloads and trusted pins

[Release page](https://github.com/webos21/crt/releases/tag/v0.0.1-preview.20261110.3). Verify the manifest against this independently obtained
pin **before** accepting its archive/contract hashes:

- `release-manifest-dev-linux-x86_64.json`: **d4211d3298a764ff22f6e262073f834bbb36cb5d07c22e1ecb10f7f83f8fca60** (1683 bytes).
- Installed SDK content identity: `36623957fbdd5a384cc173991e20253ff83148028ab8072298dcd434a55a2b3f`.

| Asset | Bytes | SHA-256 |
| --- | ---: | --- |
| `crt-v0.0.1-preview.20261110.3-linux-x86_64-dev.tar.xz` | 7697544 | `e858c1b8b22697e11551de44e1b5c3aba3cbdc93679787e33d7ae6241d6f2217` |
| `crt-v0.0.1-preview.20261110.3-linux-x86_64-dev-toolchain.json` | 402958 | `90b3bb59aaac7df481becbad4e2b87473e2fefcb803123332689d7f1135e4e30` |
| `crt-v0.0.1-preview.20261110.3-linux-02-cxx-source.tar.xz` | 9114436 | `cb937e55acd86af008d2695fa826156f9c1defb300fdeb928168b953692f91d9` |
| `crt-v0.0.1-preview.20261110.3-linux-03-gfx-simple-source.tar.xz` | 839084 | `20479ccb00deffbbd9d35bddb5d0fafddf297c01ccc51bf4c9adff0c345d8270` |

The release also carries the original `SHA256SUMS-dev-linux-x86_64` and release
manifest. The tagged CRT tree supplies project sources/build glue; upstream
sources remain identified by the SDK's recorded URLs and digests.

## Trust and supported use

This first Preview uses **manual, explicitly pinned metadata** obtained from the
maintainer's `webos21/crt` repository, verified HTTPS with a user-selected CA file,
and exact archive/contract/content checks. It carries no publisher signature;
CLI `publisher_authentication` remains `not_verified`. This is not an automatic
channel, signing-key bootstrap, expiry or rollback-protection protocol.
See [Preview trust policy](../design/dev_preview_trust.md).

GitHub release downloads redirect to GitHub asset storage. Current `crt-cli`
rejects HTTPS redirects; download with a browser or HTTPS client, verify the
fixed manifest pin, then use native `sdk install`. The current public GitHub URL
is not advertised as a working `sdk acquire --source` endpoint.
The [published SDK workflow](../guides/dev_sdk_preview.md) specifies the qualified
CLI and explicit LLVM inputs. CLI executable publication is a separate step.

C tests pass 147/147; C++ runtime/string-ABI/iostream pass static/shared, Dev
HTTPS/inflate and hidden-source consumers pass. The exact local CLI candidate
also passes native 881/881, port/provider 81/81, input gates 19/19, controlled
HTTPS SDK acquisition 73/73, resource/environment 30/30 and HTTPS/signature
32/32 at their recorded inputs. Actual public-byte verification is recorded in
[SDK acceptance](../acceptance/sdk_split_acceptance.md#linux-dev-sdk-public-promotion-2026-11-10).

## Scope limits

Only the Linux/x86_64 Dev artifact is offered here. Full/GFX/Media/UI/Web SDK
binaries, other host packages, independent host-tools archives, general updates
and CLI self-upgrade are separate. The CLI compiler policy's pure C shared
SDK-provider mode remains unqualified (it can map host libz); use the qualified
static-C/C++ workflows. Source-port shared proof does not endorse that SDK mode.
Existing SDKs, project pins and the withdrawn earlier graphics preview are not
replaced. Every future advertised host requires its own final-package proof.
