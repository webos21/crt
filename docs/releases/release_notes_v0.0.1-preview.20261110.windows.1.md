# Windows Development SDK Preview v0.0.1-preview.20261110.windows.1

This prerelease offers the reduced Development SDK for **native Windows/x86_64**.
Its manifest stage is `02-cxx`, profile `dev`: CRT C/C++ runtimes, public compiler
policy, networking/JSON/compression dependencies and their notices. The LLVM
toolchain and the Windows SDK import libraries are external prerequisites; the exercised
toolchain is LLVM/Clang/LLD **22.1.8** with the Windows SDK **10.0.28000.0** `um/x64` import libraries on Windows 11 (build 26100).

Build/source commit: `b31276a6acccf0a89731f7bb0aecbdcdc9b45cda` (clean Release build).
[Source tree](https://github.com/webos21/crt/tree/b31276a6acccf0a89731f7bb0aecbdcdc9b45cda) ·
[Tagged source archive](https://github.com/webos21/crt/archive/refs/tags/v0.0.1-preview.20261110.windows.1.tar.gz).
The SDK retains dependency recipes, licenses and upstream source pins; the 02/03
source assets below retain the exact integrated sources and successor recipe.

## Downloads and trusted pins

[Release page](https://github.com/webos21/crt/releases/tag/v0.0.1-preview.20261110.windows.1). Verify the manifest against this independently obtained
pin **before** accepting its archive/contract hashes:

- `release-manifest-dev-windows-x86_64.json`: **b0fca4c81d29b8eee28e0d9fb7d6e26950b094a8be4f2c091e43922366a46976** (1800 bytes).
- Installed SDK content identity: `5bb55372ec10e3cfa24c84de981a7130f56aeb57ef0d43d5da22a65036dec7ef`.

| Asset | Bytes | SHA-256 |
| --- | ---: | --- |
| `crt-v0.0.1-preview.20261110.windows.1-windows-x86_64-dev.zip` | 73323880 | `c71bb91e9f2c623d44e8154c7798a446b850781bdc1cf515eb9675dc04137be1` |
| `crt-v0.0.1-preview.20261110.windows.1-windows-x86_64-dev-toolchain.json` | 440297 | `7762f572b4614b3068e1ea99694ce19360e213d31c5bd01019e4ad6ce005b726` |
| `crt-v0.0.1-preview.20261110.windows.1-windows-02-cxx-source.tar.xz` | 9115212 | `ef301c5e3cc084766ede7a1372c89071f0a3597863a549f6a2dff9610d0e83ab` |
| `crt-v0.0.1-preview.20261110.windows.1-windows-03-gfx-simple-source.tar.xz` | 133720 | `cf3ba715cf764eb374199ded7da0ee0c61277c1d414f0d0cdb538a5246fd660c` |

The release also carries the original `SHA256SUMS-dev-windows-x86_64` (542 bytes, SHA-256
`08e49d83a8ca8b32809c4b411de70c78bc1a02c6b8299231bfcad0bb284c0b6e`) and the release
manifest. The tagged CRT tree supplies project sources/build glue; upstream
sources remain identified by the SDK's recorded URLs and digests.

## Trust and supported use

This Preview uses **manual, explicitly pinned metadata** obtained from the
maintainer's `webos21/crt` repository, verified HTTPS with a user-selected CA input,
and exact archive/contract/content checks. It carries no publisher signature;
CLI `publisher_authentication` remains `not_verified`. This is not an automatic
channel, signing-key bootstrap, expiry or rollback-protection protocol.
See [Preview trust policy](../design/dev_preview_trust.md).

GitHub release downloads redirect to GitHub asset storage. Current `crt-cli`
rejects HTTPS redirects; download with a browser or HTTPS client, verify the
fixed manifest pin, then use native `sdk install`. The current public GitHub URL
is not advertised as a working `sdk acquire --source` endpoint.
The [published SDK workflow](../guides/dev_sdk_preview.md#windowsx86_64) specifies the
CLI and explicit toolchain inputs. CLI executable publication is a separate step.

CTest passes 169/169; C++ runtime/string-ABI/iostream pass static/shared, Dev
HTTPS/inflate and hidden-source consumers pass. A `crt-cli` built from its current
sources by this SDK alone passes native 972/972, archive 214/214, SDK-store
113/113 and recipe 141/141. Actual public-byte verification is recorded in
[SDK acceptance](../acceptance/sdk_split_acceptance.md#windows-dev-sdk-public-promotion-2026-11-10).

## Scope limits

Only the Windows/x86_64 Dev artifact is offered here; the Linux/x86_64 and macOS/arm64 Previews are
separate releases. Full/GFX/Media/UI/Web SDK binaries, other host packages (including Windows/arm64),
independent host-tools archives, general updates and CLI self-upgrade are separate. The PE images are
**not Authenticode-signed**. The Windows SDK import libraries (`kernel32.lib` and friends) are the host's and are never
redistributed or locked: the portable policy names the kind `windows-um-import-libraries`, and each consumer supplies its own
directory (`--os-sdk`). The CLI compiler policy's pure C shared SDK-provider mode is unqualified; use the qualified static
C/C++ workflows (source-port shared proof does not endorse that mode). Existing SDKs, project pins and the withdrawn earlier
graphics preview are not replaced. Every future advertised host requires its own final-package proof.
