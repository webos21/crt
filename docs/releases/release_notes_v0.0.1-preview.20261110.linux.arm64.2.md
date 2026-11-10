# Linux/aarch64 Development SDK Preview v0.0.1-preview.20261110.linux.arm64.2

This prerelease offers the reduced Development SDK for **native Linux/aarch64** (arm64).
Its manifest stage is `02-cxx`, profile `dev`: CRT C/C++ runtimes, public compiler
policy, networking/JSON/compression dependencies and their notices. The LLVM
toolchain is an external prerequisite; the exercised toolchain is Ubuntu's LLVM
**21.1.8** (`clang`, `ld.lld`, `llvm-ar-21`) on Ubuntu 26.04 with glibc **2.43**.
LLVM 21 is required for the TLS descriptor code generation the SDK policy captures.

Build/source commit: `e6a94ab9e69b05497b3804328971d24d6071a53d` (clean Release build).
[Source tree](https://github.com/webos21/crt/tree/e6a94ab9e69b05497b3804328971d24d6071a53d) ·
[Tagged source archive](https://github.com/webos21/crt/archive/refs/tags/v0.0.1-preview.20261110.linux.arm64.2.tar.gz).
The SDK retains dependency recipes, licenses and upstream source pins; the 02/03
source assets below retain the exact integrated sources and successor recipe.

## Downloads and trusted pins

[Release page](https://github.com/webos21/crt/releases/tag/v0.0.1-preview.20261110.linux.arm64.2). Verify the manifest against this independently obtained
pin **before** accepting its archive/contract hashes:

- `release-manifest-dev-linux-aarch64.json`: **ba77b26edfa0413df5e8c17309506d7ae6ecf134ae7e9908e3742bc562067be3** (1772 bytes).
- Installed SDK content identity: `cd2ab657615c0fa52a1804f301a57dfa21b1067e3e5ed5a9f9a07d86ae050cc6`.

| Asset | Bytes | SHA-256 |
| --- | ---: | --- |
| `crt-v0.0.1-preview.20261110.linux.arm64.2-linux-aarch64-dev.tar.xz` | 7632648 | `5c99dc59b747cb31cb623fa057264c3a4e7d5715f960203073dc1b34f664a1c4` |
| `crt-v0.0.1-preview.20261110.linux.arm64.2-linux-aarch64-dev-toolchain.json` | 403294 | `a3df2fbd6b4d14d316b691a1d2831d0692aaba133e3b168076ee0ec7cb6434d4` |
| `crt-v0.0.1-preview.20261110.linux.arm64.2-linux-02-cxx-source.tar.xz` | 9114820 | `39f7de2ace7154fde45b187fafeae9d10f8d7772f70ecab438d7591f1c29ccbe` |
| `crt-v0.0.1-preview.20261110.linux.arm64.2-linux-03-gfx-simple-source.tar.xz` | 838332 | `1fec38dbc1ff2e7fa886723b1e9327ebd55b65fffd714b1d2b3d9c619a3b746c` |

The release also carries the original `SHA256SUMS-dev-linux-aarch64` (SHA-256 `0be55a0fb39d9b5fc351e849012fd1cf22d6b779be95580a19f5b3071eedb35a`, 551 bytes) and the release
manifest. The tagged CRT tree supplies project sources/build glue; upstream
sources remain identified by the SDK's recorded URLs and digests.

## Trust and supported use

This Preview uses **manual, explicitly pinned metadata** obtained from the
maintainer's `webos21/crt` repository, verified HTTPS with a user-selected CA file,
and exact archive/contract/content checks. It carries no publisher signature;
CLI `publisher_authentication` remains `not_verified`. This is not an automatic
channel, signing-key bootstrap, expiry or rollback-protection protocol.
See [Preview trust policy](../design/dev_preview_trust.md).

GitHub release downloads redirect to GitHub asset storage. Download with a browser or
HTTPS client, verify the fixed manifest pin, then use native `sdk install` (a CLI with
the explicit GitHub redirect policy can also acquire it from a pinned source).
The [published SDK workflow](../guides/dev_sdk_preview.md#linuxaarch64) specifies the
CLI and explicit toolchain inputs. **No Linux/aarch64 `crt-cli` executable is published
with this release**; the qualified CLI was built from `crt-cli` sources by this SDK alone.

Producer evidence: C tests pass 147/147; C++ runtime/string-ABI/iostream pass static/shared;
the shared-loader regression (calloc, TLS descriptors, a second thread, thread-local destruction and
a cross-DSO exception) passes and rejects an unrelated missing import; Dev static/shared HTTPS/inflate
and hidden-source consumers pass. A `crt-cli` built from its sources by this SDK passes native 913/913,
archive 221/221, SDK-store 114/114 and port-recipe 142/142 with the executable reproduced from the
SDK alone. The actual public-byte verification is recorded in
[SDK acceptance](../acceptance/sdk_split_acceptance.md#linuxaarch64-dev-sdk-public-promotion-2026-11-10).

## Scope limits

Only the Linux/aarch64 Dev artifact is offered here; the Linux/x86_64, macOS and Windows Previews
are separate releases. Qualification used glibc 2.43 only: the older-loader (glibc 2.39) check
that the x86_64 repair used was not run on aarch64. Full/GFX/Media/UI/Web SDK binaries, other host packages,
independent host-tools archives, general updates and CLI self-upgrade are separate. The CLI compiler
policy's pure C shared SDK-provider mode is unqualified; use the qualified static C/C++ workflows
(source-port shared evidence does not endorse that mode). Existing SDKs, project pins and the earlier
Previews are not replaced. Every future advertised host requires its own final-package proof.
