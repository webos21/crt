# Use the Development SDK Previews

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

GitHub asset URLs redirect to GitHub storage. The published CLI Previews support
explicitly pinned `sdk acquire --source` with `"redirect_policy": "github-release"`;
the default still refuses redirects. Manual download plus `sdk install` below remains
available. The [CLI source/redirect contract](https://github.com/webos21/crt-cli/blob/main/docs/reference/bootstrap_contract.md#restricted-https-redirects-experimental-cli-owned)
owns the JSON fields and limits. No default-download channel or trust pin is selected implicitly.
The CLI app/build paths themselves require no Python, shell or CMake.

## Install and build without configure

Use the [published Linux/x86_64 CLI Preview](https://github.com/webos21/crt-cli/releases/tag/v0.1.0-preview.20261110.linux-x86_64.1);
[its acceptance](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/preview_acceptance.md#linuxx86_64-cli-preview-release-2026-11-10)
owns the exact executable/package pins. This CLI publication is independent of the SDK version.
The SDK installer validates the pinned manifest before trusting its payload
hashes and validates the portable policy beside the archive. Substitute actual
absolute download/tool paths; SDKs install side by side in the user's store.

```text
crt-cli sdk install --archive /absolute/downloads/crt-v0.0.1-preview.20261110.3-linux-x86_64-dev.tar.xz --release-manifest /absolute/downloads/release-manifest-dev-linux-x86_64.json --release-manifest-sha256 d4211d3298a764ff22f6e262073f834bbb36cb5d07c22e1ecb10f7f83f8fca60
crt-cli new hello-cxx --language cxx
crt-cli build --project hello-cxx --sdk 36623957fbdd5a384cc173991e20253ff83148028ab8072298dcd434a55a2b3f --cc /usr/bin/clang --cxx /usr/bin/clang++ --linker /usr/bin/ld.lld --ar /usr/bin/llvm-ar-21
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

## macOS/arm64

The [macOS Preview release](https://github.com/webos21/crt/releases/tag/v0.0.1-preview.20261110.macos.1) offers the native
macOS/arm64 Dev SDK `v0.0.1-preview.20261110.macos.1` (stage `02-cxx`, profile `dev`), qualified with Xcode's Apple clang 21.0.0 and
its `ld` on macOS 27.2. The Xcode command-line tools provide the compiler, linker, `ar` and the OS SDK; none is bundled.
Download these three original files from the release into a fresh directory with a trusted HTTPS client or browser:

- [SDK archive](https://github.com/webos21/crt/releases/download/v0.0.1-preview.20261110.macos.1/crt-v0.0.1-preview.20261110.macos.1-macos-aarch64-dev.tar.xz)
- [Portable native compiler policy](https://github.com/webos21/crt/releases/download/v0.0.1-preview.20261110.macos.1/crt-v0.0.1-preview.20261110.macos.1-macos-aarch64-dev-toolchain.json)
- [Release manifest](https://github.com/webos21/crt/releases/download/v0.0.1-preview.20261110.macos.1/release-manifest-dev-macos-aarch64.json)

The independently trusted manifest SHA-256 from the reviewed repository is
`0981110c84e5d24a903dbd3965dbe92eb2fe405edfa89804e3086dfeb75983de` (1728 bytes); the
[trust policy](../design/dev_preview_trust.md#macosarm64-bounded-policy-adopted-2026-11-10) defines the boundary. Use a
`crt-cli` built for macOS/arm64 from a source that includes the stored-policy selection (`--sdk`) and the pinned installer;
the [published macOS/arm64 CLI Preview](https://github.com/webos21/crt-cli/releases/tag/v0.1.0-preview.20261110.macos-arm64.1)
provides that executable with notices and its pinned inventory.
Substitute actual absolute paths (the toolchain paths below are Xcode's):

```text
crt-cli sdk install --archive /absolute/downloads/crt-v0.0.1-preview.20261110.macos.1-macos-aarch64-dev.tar.xz --release-manifest /absolute/downloads/release-manifest-dev-macos-aarch64.json --release-manifest-sha256 0981110c84e5d24a903dbd3965dbe92eb2fe405edfa89804e3086dfeb75983de
crt-cli new hello-cxx --language cxx
crt-cli build --project hello-cxx --sdk e3dc7fb1810a3f78ab8ef33882225ccb562d21f6f5977a2037ab92819326bac8 --cc /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang --cxx /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++ --linker /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/ld --ar /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/ar
crt-cli test --project hello-cxx
crt-cli run --project hello-cxx
```

For C sources choose `--language c` and another fresh project name. The first build binds the installed SDK and
discovers the macOS SDK with `xcrun`; pass `--os-sdk <MacOSX.sdk directory>` to select it explicitly. Later plain
build/test/run reuse the project lock. The [macOS CLI release record](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/preview_acceptance.md#macos-cli-preview-release-2026-11-10)
provides a pinned `sdk acquire` source with the explicit GitHub redirect policy; manual installation above remains valid. Qualified scope is the recorded native first-build/installed-selection flow with static
C and C++ consumers; pure C shared SDK-provider mode stays unqualified.

## Windows/x86_64

The [Windows Preview release](https://github.com/webos21/crt/releases/tag/v0.0.1-preview.20261110.windows.1) offers the native
Windows/x86_64 Dev SDK `v0.0.1-preview.20261110.windows.1` (stage `02-cxx`, profile `dev`), qualified with LLVM/Clang/LLD **22.1.8**
and the Windows SDK `10.0.28000.0` import libraries on Windows 11 (build 26100). The LLVM tools and the Windows SDK are external
prerequisites; none is bundled. Download these three original files from the release into a fresh directory with a trusted HTTPS client or browser:

- [SDK archive](https://github.com/webos21/crt/releases/download/v0.0.1-preview.20261110.windows.1/crt-v0.0.1-preview.20261110.windows.1-windows-x86_64-dev.zip)
- [Portable native compiler policy](https://github.com/webos21/crt/releases/download/v0.0.1-preview.20261110.windows.1/crt-v0.0.1-preview.20261110.windows.1-windows-x86_64-dev-toolchain.json)
- [Release manifest](https://github.com/webos21/crt/releases/download/v0.0.1-preview.20261110.windows.1/release-manifest-dev-windows-x86_64.json)

The independently trusted manifest SHA-256 from the reviewed repository is
`b0fca4c81d29b8eee28e0d9fb7d6e26950b094a8be4f2c091e43922366a46976` (1800 bytes); the
[trust policy](../design/dev_preview_trust.md#windowsx86_64-bounded-policy-adopted-2026-11-10) defines the boundary. Use a
`crt-cli` built for Windows/x64 from a source that includes the stored-policy selection (`--sdk`), the pinned installer and the
Windows D3 adapter. The [published Windows/x64 CLI Preview](https://github.com/webos21/crt-cli/releases/tag/v0.1.0-preview.20261110.windows-x86_64.1)
provides the executable with notices and a pinned inventory; [the Windows CLI guide](https://github.com/webos21/crt-cli/blob/main/docs/guides/windows_preview.md)
owns extraction, caller CA export, pinned GitHub source and first-build steps. The SDK archive contains no symbolic links, so installation needs no Developer Mode.
Substitute actual absolute paths (the LLVM and Windows SDK paths below are the exercised ones):

```text
crt-cli sdk install --archive C:\downloads\crt-v0.0.1-preview.20261110.windows.1-windows-x86_64-dev.zip --release-manifest C:\downloads\release-manifest-dev-windows-x86_64.json --release-manifest-sha256 b0fca4c81d29b8eee28e0d9fb7d6e26950b094a8be4f2c091e43922366a46976
crt-cli new hello-cxx --language cxx
crt-cli build --project hello-cxx --sdk 5bb55372ec10e3cfa24c84de981a7130f56aeb57ef0d43d5da22a65036dec7ef --os-sdk "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.28000.0\um\x64" --cc "C:\Program Files\LLVM\bin\clang.exe" --cxx "C:\Program Files\LLVM\bin\clang++.exe" --linker "C:\Program Files\LLVM\bin\ld.lld.exe" --ar "C:\Program Files\LLVM\bin\llvm-ar.exe"
crt-cli test --project hello-cxx
crt-cli run --project hello-cxx
```

For C sources choose `--language c` and another fresh project name. `--os-sdk` names the directory that holds `kernel32.lib` and
`synchronization.lib`; it is kept in local state only and never in the project lock. The first build binds the installed SDK; later
plain build/test/run reuse the project lock. The Windows CLI Preview also qualifies explicit pinned GitHub `sdk acquire`
through its restricted redirect policy; manual installation above remains valid. Qualified scope is the recorded native first-build/installed-selection flow with static C and
C++ consumers; pure C shared SDK-provider mode stays unqualified, and the Windows SDK contract ships no shared `z`.
The PE images are not Authenticode-signed.
