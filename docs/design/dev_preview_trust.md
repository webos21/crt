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
