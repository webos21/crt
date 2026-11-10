# Development Preview Trust and Promotion

## Bounded policy adopted 2026-11-10

The owner authorized source publication, trust-metadata review and SDK promotion
on 2026-11-10. This review adopts the existing **caller-trusted manual pin** model
for the first native Linux/x86_64 Development SDK Preview. It does not introduce
a general/default download authority or a publisher signing key.

| Boundary | Decision |
| --- | --- |
| Publisher location | Public `webos21/crt` repository and the exact release tag `v0.0.1-preview.20261110.3`; no `latest` lookup |
| Build/source pin | `0ab8f8cfbbde19e14dd5861ba176fb8881e06182`; tag must resolve to that exact clean producer |
| Metadata anchor | Manifest SHA-256 `d4211d3298a764ff22f6e262073f834bbb36cb5d07c22e1ecb10f7f83f8fca60` and size 1683, copied from the qualified bytes into reviewed repository source before publication |
| TLS | Explicitly selected trusted CA PEM; peer/hostname checks on, TLS 1.2 or newer, no insecure override |
| Payload acceptance | Exact manifest pin first, then every asset's size/SHA-256; archive install bounds, SDK content and native compiler-policy identity remain checked by CLI |
| Publisher authentication | `not_verified`; no release/publisher signature or key provenance is inferred from a digest or version |
| Existing state | No implicit project SDK migration; install beside existing versions and preserve locks on failure |
| Immutability | No replacement of accepted assets/manifest or tag movement; use a new candidate/release for changed bytes. This is release procedure, not a claim of server-enforced immutability |

The independently trusted input is the reviewed repository commit containing
these pins, obtained through the user's existing trust in the maintainer's
repository. A checksum downloaded beside an untrusted payload is not publisher
authentication. Manual account/repository trust and TLS are this Preview's
explicit bootstrap boundary; unattended channels require a separate authority,
key/expiry/rotation/revocation and recovery decision before adoption.

## Download and promotion procedure

1. Publish the selected producer source, review the clean candidate, source/
   notice correspondence and exact bytes. Keep the failed/unpromoted identities.
2. Freeze these pins in a reviewed commit. Create a source tag at the build
   commit, stage a **prerelease** and upload the unchanged collector outputs.
   Verify names/sizes/digests before making that release public; keep it separate
   from Full SDK delivery and do not designate it a stable/latest release.
3. Download publicly without repository authentication into a fresh directory.
   For GitHub downloads a host-side verifier may accept one verified HTTPS hop
   from the exact release URL to GitHub's asset-storage hosts. Check the location
   before following and never record temporary signed redirect query strings.
   Record only canonical release URLs, final hostname and exact file digests.
4. Consume those downloaded bytes with native `sdk install`, stored portable
   policy, C/C++ first build/test/run and unchanged-input checks. Record rejection
   of unsupported paths rather than labeling them compatible.
5. Record producer promotion and exercised consumer adoption. Consumer defaults/
   production locks and CLI binary publication are later explicit steps. Repeat
   final-package proof for every newly advertised host.

The current CLI intentionally follows no redirects. Its GitHub `sdk acquire`
path remains unsupported; this release's accepted path is manual HTTPS download
plus pinned local installation. A restricted redirect policy would be a CLI
transport/contract change requiring its own failure/native replay, not a flag
silently enabled during producer publication.

No machine trust store, global PATH, account permission or release/security
setting is changed by this policy. Test issuers/keys are not release trust roots.

## macOS/arm64 bounded policy adopted 2026-11-10

The same caller-trusted manual pin model, procedure and limits apply to the native
macOS/arm64 Development SDK Preview. The owner authorized its GitHub prerelease
promotion on 2026-11-10. It is a separate release with its own tag and pins; it does
not alter, replace or extend the Linux Preview's assets or pins.

| Boundary | Decision |
| --- | --- |
| Publisher location | Public `webos21/crt` repository and the exact release tag `v0.0.1-preview.20261110.macos.1`; no `latest` lookup |
| Build/source pin | `445f127dc2e1ef4ab538166aa2e374f25773a803`; tag must resolve to that exact clean producer |
| Metadata anchor | Manifest SHA-256 `0981110c84e5d24a903dbd3965dbe92eb2fe405edfa89804e3086dfeb75983de` and size 1728, copied from the qualified bytes into reviewed repository source before publication |
| TLS, payload acceptance, publisher authentication, existing state, immutability | As in the Linux policy above: explicit trusted CA PEM, exact manifest pin then every asset's size/SHA-256, `not_verified`, install beside existing versions, no replacement of accepted assets or tag movement |
| OS SDK | The portable compiler policy names the kind `macos-xcode-sdk`, not a path: the consumer's own Xcode SDK is discovered with `xcrun` or selected with `--os-sdk` and kept in local state only |
| Code signing | Images carry the linker's ad-hoc signatures only; no Developer ID signature or notarization is claimed |

Promotion follows the procedure above (draft prerelease, unchanged collector outputs,
digest check before publishing, unauthenticated public download, native consumer
proof on the downloaded bytes). The record is in
[SDK acceptance](../acceptance/sdk_split_acceptance.md#macos-dev-sdk-public-promotion-2026-11-10).

## Windows/x86_64 bounded policy adopted 2026-11-10

The same caller-trusted manual pin model, procedure and limits apply to the native
Windows/x86_64 Development SDK Preview. The owner requested its GitHub prerelease
promotion on 2026-11-10. It is a separate release with its own tag and pins; it does
not alter, replace or extend the Linux or macOS Previews' assets or pins.

| Boundary | Decision |
| --- | --- |
| Publisher location | Public `webos21/crt` repository and the exact release tag `v0.0.1-preview.20261110.windows.1`; no `latest` lookup |
| Build/source pin | `b31276a6acccf0a89731f7bb0aecbdcdc9b45cda`; tag must resolve to that exact clean producer |
| Metadata anchor | Manifest SHA-256 `b0fca4c81d29b8eee28e0d9fb7d6e26950b094a8be4f2c091e43922366a46976` and size 1800, copied from the qualified bytes into reviewed repository source before publication |
| TLS, payload acceptance, publisher authentication, existing state, immutability | As in the Linux policy above: user-selected trusted CA input, exact manifest pin then every asset's size/SHA-256, `not_verified`, install beside existing versions, no replacement of accepted assets or tag movement |
| OS SDK | The portable compiler policy names the kind `windows-um-import-libraries`, not a path: the consumer's own Windows SDK import-library directory is selected with `--os-sdk` (or `CRT_WINDOWS_SDK_LIBPATH`) and kept in local state only; it is neither redistributed nor locked |
| Code signing | The PE images are not Authenticode-signed; nothing is claimed about SmartScreen or other reputation checks |
| Installation | The ZIP archive contains no symbolic links; installation needs no Developer Mode and no elevation |

Promotion follows the procedure above (draft prerelease, unchanged collector outputs,
digest check before publishing, unauthenticated public download, native consumer
proof on the downloaded bytes). The record is in
[SDK acceptance](../acceptance/sdk_split_acceptance.md#windows-dev-sdk-public-promotion-2026-11-10).

## Linux loader-repair policy adopted 2026-11-10

The owner authorized repair and a new Linux SDK release. Extend the existing
caller-trusted manual-pin procedure to `v0.0.1-preview.20261110.4` only, with clean producer
`448fa32349ae697f589f0ad8fc60cfdf7a7e195a`, manifest SHA-256 `983dbcf143b1fe227bf519f07ec153ce5baf50e190382222995770affe43b2c3`
and size 1683. Freeze these pins in source before staging a draft;
the tag must resolve to that producer and all six collector files must remain
unchanged. [Release notes](../releases/release_notes_v0.0.1-preview.20261110.4.md)
list every payload pin and migration limit. `.3` is retained; its glibc 2.39
shared-C++ failure is not relabeled as passing.

Keep verified TLS, exact asset/content/policy checks, publisher authentication
`not_verified`, non-latest prerelease status and explicit side-by-side selection.
No signer, default SDK migration or Full/other-host promotion is introduced.
The published CLI's reviewed GitHub redirect policy can consume this release;
manual pinned installation remains valid. Public-byte proof and CLI adoption
are recorded after promotion, independently of local qualification.

## Linux/aarch64 bounded policy adopted 2026-11-10

The same caller-trusted manual pin model, procedure and limits apply to the native
Linux/aarch64 Development SDK Preview. The owner authorized its GitHub prerelease
promotion on 2026-11-10. It is a separate release with its own tag and pins; it does
not alter, replace or extend the other Previews' assets or pins.

| Boundary | Decision |
| --- | --- |
| Publisher location | Public `webos21/crt` repository and the exact release tag `v0.0.1-preview.20261110.linux.arm64.2`; no `latest` lookup |
| Build/source pin | `e6a94ab9e69b05497b3804328971d24d6071a53d`; tag must resolve to that exact clean producer |
| Metadata anchor | Manifest SHA-256 `ba77b26edfa0413df5e8c17309506d7ae6ecf134ae7e9908e3742bc562067be3` and size 1772, copied from the qualified bytes into reviewed repository source before publication |
| TLS, payload acceptance, publisher authentication, existing state, immutability | As in the Linux policy above: explicit trusted CA PEM, exact manifest pin then every asset's size/SHA-256, `not_verified`, install beside existing versions, no replacement of accepted assets or tag movement |
| OS SDK | None: the Linux portable compiler policy has no OS SDK entry; the consumer supplies LLVM 21 (`clang`, `ld.lld`, `llvm-ar`) |
| Loader boundary | The system ELF interpreter; TLS uses descriptors (AArch64's default code generation). Qualified with glibc 2.43 only |

Promotion follows the procedure above (draft prerelease, unchanged collector outputs,
digest check before publishing, unauthenticated public download, native consumer
proof on the downloaded bytes). The record is in
[SDK acceptance](../acceptance/sdk_split_acceptance.md#linuxaarch64-dev-sdk-public-promotion-2026-11-10).
