# Linux Development SDK Preview v0.0.1-preview.20261110.4

This immutable replacement Preview repairs Linux/x86_64 shared C++ startup on
Ubuntu 24.04's glibc 2.39 while retaining the qualified Ubuntu 26.04/glibc 2.43
path. It is stage `02-cxx`, profile `dev`; use **LLVM/Clang/LLD 21** as external
host tools. Older x86 Clang versions may not implement TLSDESC code generation.

[Producer source](https://github.com/webos21/crt/commit/448fa32349ae697f589f0ad8fc60cfdf7a7e195a) ·
[Release page](https://github.com/webos21/crt/releases/tag/v0.0.1-preview.20261110.4).

Linux code uses TLS descriptors instead of a general-dynamic
`__tls_get_addr` import. The SDK no longer ships or links `crt-loader-import.so`:
its explicit loader dependency caused glibc 2.39 to search for
`calloc@GLIBC_2.2.5` before main, whereas CRT exports `calloc@@CRT_1.0`.
The CRT symbol-version firewall is preserved. Linux libc++abi's weak host
thread-exit probe is privately redirected so GNU libc loaded by a host DSO
cannot capture CRT-thread destructors. Upstream source/pins are unchanged;
the adapter is in CRT-owned CMake, using upstream's existing pthread fallback.

## Downloads and trusted pins

Obtain the manifest pin independently from this reviewed repository before
accepting the release's payload hashes. Publisher authentication remains
`not_verified`: this Preview has no publisher signature or automatic channel.

- Manifest SHA-256: **983dbcf143b1fe227bf519f07ec153ce5baf50e190382222995770affe43b2c3** (1683 bytes).
- Installed SDK content: `22107183e5127d44e979f328fa78aaf2a436db802aa9f8821a864a39d492ca81`.

| Asset | Bytes | SHA-256 |
| --- | ---: | --- |
| `crt-v0.0.1-preview.20261110.4-linux-02-cxx-source.tar.xz` | 9115324 | `8162d32545ebb7bb610154baf3ae840b9d3b710d4ce41f71c0837dec0a9d3286` |
| `crt-v0.0.1-preview.20261110.4-linux-03-gfx-simple-source.tar.xz` | 838340 | `d9181cf10fd0686c987fa49a0820bf39a082372ca459e46beec11a0cb164b41f` |
| `crt-v0.0.1-preview.20261110.4-linux-x86_64-dev-toolchain.json` | 403257 | `369dae4fbe6c220a5f451431b2bbc76de947a7d618184abe0520424c4db84cfb` |
| `crt-v0.0.1-preview.20261110.4-linux-x86_64-dev.tar.xz` | 7701048 | `fa1addfb980db8c8e958e7530b7dc71aafdd6bb2727e024e0a05be2da319c775` |

The other files are `release-manifest-dev-linux-x86_64.json` and
`SHA256SUMS-dev-linux-x86_64`. The source tag resolves to the clean producer
above; 02's nested successor recipe matches the accompanying common 03 source
asset. Public C/C++ APIs and manifest/native-policy/recipe formats are unchanged.

## Qualification and migration

The same shared-CRT ELF passes calloc/free, TLS in a DSO and a second thread,
thread-local destruction and cross-DSO exceptions under glibc
`2.39-0ubuntu8.8` and `2.43-2ubuntu2.4`. GNU-libc/Wayland coexistence passes
on both loader/runtime combinations. Unrelated unresolved shared imports still
fail at link time. C tests pass 147/147; static/shared C++ runtime, string ABI,
iostream and the integrated TLS regression pass. Dev HTTPS+inflate and explicit
CA negatives pass, including relocated SDK-only checks with producer inputs
hidden. [Acceptance](../acceptance/sdk_split_acceptance.md#linux-loader-compatibility-repair-2026-11-10)
owns final consumer/public-download receipts and their exact scope.

Install alongside existing SDKs and explicitly select the new identity; existing
project locks are not migrated. Rebuild project objects and dependent ports
against the new policy. Prebuilt GD-TLS libraries using the former loader stub
are not compatible inputs for this path; rebuild them rather than suppressing
undefined-symbol checks. The former `.3` assets/tag and failed results are retained.
The existing published Linux CLI can install/select this SDK; this SDK release
does not publish a replacement CLI executable.

Actual hosted Ubuntu 24.04 (glibc `2.39-0ubuntu8.9`) and Ubuntu 26.04 (glibc
`2.43-2ubuntu2.4`) both pass the complete CLI bootstrap, 913/221/114/142 suites,
second self-build, warm reuse, import audit and SDK-free execution in
[CI run 42805951580](https://github.com/webos21/crt-cli/actions/runs/42805951580).
Both hosts use LLVM 21.1.8; stage 1/2 bytes match within each host. This extends
the local loader tests with real old-LTS/new-host qualification.

This is a Linux/x86_64 **Dev** release. No native Linux/aarch64 replay, other-OS
replay, Full/GFX/Web rebuild, universal glibc compatibility or late `dlopen` TLS
support is implied. Pure C shared SDK-provider mode remains separately
unqualified. Upstream's fallback main-thread destructor ordering and DSO-unload
limits remain. See [installation](../guides/dev_sdk_preview.md) and
[manual trust policy](../design/dev_preview_trust.md#linux-loader-repair-policy-adopted-2026-11-10).
