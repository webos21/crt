# Publisher Signing Key and Roadmap

Owner: CRT. Status: **the public key and fingerprint are published, and selected release archives have detached publisher signatures.**
Publisher authentication applies only to the exact archive bytes whose signature was verified with the pinned key; it does not extend to
unsigned release records, checksums, source assets or other releases. New gated releases sign each of these assets individually;
existing releases retain their original per-file coverage. Keep digest pins alongside signature verification
([Preview trust policy](../design/dev_preview_trust.md)). Dated signing records below and the owning acceptance documents identify the signed assets.

## The publisher key

| Field | Value |
| --- | --- |
| Algorithm | RSA 2048 bits |
| Public key file | [`crt-release-publisher.pub`](crt-release-publisher.pub) (PEM SubjectPublicKeyInfo) |
| **Fingerprint** (SHA-256 of the DER SubjectPublicKeyInfo) | `59218576956e493476bc36f7385d8e01a03ff9b92946a531b013ad3937d43fa2` |
| Published | 2026-11-16 |
| Private key | Held by the owner outside this repository, CI, release assets and every tool session. It is never supplied to an assistant. |

Compute the fingerprint of the file you are about to trust and compare it with the value above **and with every other place the owner
publishes it**; do not trust a key file merely because it sits beside the artifact you are verifying:

```sh
openssl pkey -pubin -in crt-release-publisher.pub -outform DER | sha256sum
```

**Limit of this channel.** This repository and the release assets share a GitHub account, so an account takeover could change both.
The independent publication locations and signed fingerprint notice are recorded in Roadmap, step 2. Record the fingerprint from those
channels and treat any later change as suspect. Detached asset signatures do not imply signed repository commits.

## Signature format (accepted by `crt-cli`)

A detached binary signature over **the exact bytes of the asset**: SHA-256 of the file, then RSA-PSS with SHA-256, MGF1 with SHA-256 and a
32-byte salt. This is what `crt-cli fetch --publisher-key <pem> --signature <file>` verifies; any other padding, salt, a weaker key or a
mismatched signature is rejected with `publisher_signature_invalid` and nothing is published. Signature files are named
`<asset-name>.sig`.

Sign (owner, offline):

```sh
openssl dgst -sha256 -sign crt-release.key \
  -sigopt rsa_padding_mode:pss -sigopt rsa_pss_saltlen:32 -sigopt rsa_mgf1_md:sha256 \
  -out <asset>.sig <asset>
```

Verify without `crt-cli`:

```sh
openssl dgst -sha256 -verify crt-release-publisher.pub \
  -sigopt rsa_padding_mode:pss -sigopt rsa_pss_saltlen:32 -sigopt rsa_mgf1_md:sha256 \
  -signature <asset>.sig <asset>
```

The command pair was exercised against `crt-cli` on 2026-11-16 with a temporary key (never the publisher key): a correct signature is
accepted and reported `publisher_authentication: signature_verified`; a signature by another key and the same signature on a one-bit-changed
file are both rejected as `publisher_signature_invalid` with nothing published.

## Host-tools signatures added on 2026-11-19

The owner supplied detached signatures for the `20261119` macOS/aarch64, Windows/x86_64 and Windows/aarch64 host-tools archives.
Each was verified against its public archive before attachment, uploaded as a new `<archive>.sig` asset without replacing accepted bytes,
then downloaded back, compared byte-for-byte with the supplied signature and verified again. All three use the public key above and the
specified RSA-PSS parameters (256-byte signatures). Archive/signature digests, native macOS replay and the Windows verification limit are in
[host-tools acceptance](../acceptance/host_tools_acceptance.md#macoswindows-host-tools-publication-signatures-and-macos-replay-2026-11-19).
The original release records remain unchanged: their `not_verified` field describes the pre-signing packaging step, not a failed check of
the subsequently attached signature. The records and checksum files themselves are not signed.

2026-11-19, later: the Windows/x86_64 signature was verified again natively on Windows from the unauthenticated public download, with `openssl` (9/9 including four
refusals) and with the public `crt-cli` Preview's `fetch --publisher-key --signature` (`signature_verified`; a swapped, a bit-changed and a wrong-digest control are refused with no output). Record:
[host-tools acceptance](../acceptance/host_tools_acceptance.md#windowsx86_64-host-tools-public-signature-and-native-replay-2026-11-19). The fingerprint-copy limit above is unchanged.

2026-11-19, Windows/aarch64: native ARM64 verification also passes OpenSSL's nine checks and the published ARM64 CLI's four public-URL
`fetch` cases (`signature_verified`; x64-signature, changed-signature and wrong-digest refusals publish no output). The CLI archive's own
signature verifies with the same pinned key. [Record](../acceptance/host_tools_acceptance.md#windowsaarch64-host-tools-public-signature-and-native-replay-2026-11-19).
This does not change fingerprint independence, sign the release metadata or provide Authenticode.

## What a signature does and does not establish

- It establishes that the holder of the private key signed exactly those bytes, provided the verifier obtained the public key independently.
- It does not establish that the bytes are safe or reviewed, who the key holder is beyond the fingerprint, or that the key was never
  compromised. It is not Authenticode, Developer ID notarization or any OS reputation signal; those remain separate and unclaimed.
- A signature does not replace the digest pins: keep pinning size and SHA-256, and add the signature check.

## Signing procedure (owner)

Signing is a deliberate, manual, offline step. CI never holds the private key.

For new Dev SDK and CLI Previews, stage on `main` with `dry_run: false` and
approve the `release` environment. Staging never publishes, even with
`dry_run: false`. Rehearsals (`dry_run: true`) do not issue a finalization
receipt and cannot be finalized; delete the rehearsal draft or use a new tag.

1. Obtain the stage run ID and its immutable Actions receipt. Download the
   draft payload and compare its exact names, sizes and SHA-256 values with the
   receipt from that successful run. Actions build artifacts are not releases;
   this gate does not make them private.
2. On the offline machine, sign **every** payload file, including
   `SHA256SUMS-*`. For an SDK that means the archive, toolchain contract, two
   stage-source archives, manifest and checksums; for the CLI it means the
   archive and checksums. Use the format above and verify with the public key.
   Archive signatures keep their existing names and consumer contract.
3. Upload each `<asset>.sig` to the same draft. Do not replace payload bytes,
   edit the draft or attach the internal staging receipt. Dispatch
   `finalize-release.yml` on `main` with the **stage workflow run ID** and
   approve the separate `release` deployment after checking the plan summary.
   The finalizer uses only the receipt from the successful workflow's current
   attempt (14-day retention), re-downloads the draft, checks the exact
   payload and signature set using the pinned SPKI fingerprint, and makes
   that draft a non-latest prerelease. It downloads and checks the assets again
   after publication. It does not rebuild, overwrite, or republish.
4. Verify the public files independently in a fresh workspace before changing
   any consumer pins. Record the run IDs, signer fingerprint, digests and
   verification result in acceptance. Check every file whose authenticity is
   required; verifying only the archive does not authenticate other files.

**Setup and recovery.** Configure required reviewers and a main-only deployment
branch policy on the `release` environment in both repositories. Do not edit
the draft or tag while finalization is running. GitHub's
[release update API](https://docs.github.com/en/rest/releases/releases#update-a-release)
does not atomically compare an asset inventory and publish: the finalizer
rechecks the release, asset IDs and tag immediately before publication and
again afterwards. Limit release editors accordingly. If staging failed or
its receipt expired, stage a fresh candidate under an unused tag (or first
remove a confirmed unpublished draft under owner control). If finalization
was interrupted, check the release's state before retrying; an already-public
release is never edited by a retry. A post-publication audit failure is not
proof the release stayed private: stop pin promotion and investigate.

**Independent host tools** remain artifact-only in `host-tools.yml`. For
new manual Host Tools Previews, stage a draft from the verified archive,
release record and checksum file. Apply the same sign-each-file rule;
download the signed draft, verify its exact inventory, hashes and signatures
against the original verified files, then have the owner approve and publish
manually. Verify the public downloads again. This path has no Actions
finalization receipt; do not dispatch the SDK finalizer for it.

## Roadmap

Ordered; each step needs its own approval. The owner decides keys, custody and which releases to sign.

1. **Publish the key and fingerprint** (this document). Done 2026-11-16.
2. **Publish the fingerprint on an independent channel.** At least one place that is not the CRT repository: for example GitHub Pages of the
   organization and one location outside GitHub. Record the locations here. The limit above stands until then; it is narrowed by the Naver blog post below.
   - 2026-11-16: published on the project site `https://webos21.github.io/` ("배포 서명 공개 키 지문"; the same SHA-256 value, verified by
     fetching the page). This is a second *place*, but it is served from the same GitHub account, so it does not protect against an account
     takeover. 2026-11-16: also published in the project blog post `https://blog.naver.com/webos21/224479549664` (Naver Blog, "Release signing
     public key fingerprint", SHA-256 of the DER SubjectPublicKeyInfo; the same value, verified by fetching the post). This is the first location
     outside the GitHub account: an attacker would need the Naver account as well to make the published copies agree. It is still the owner's
     own account (a post can be edited later, and it is not signed), so users should record the fingerprint once, from two places, and treat later
     changes as suspect. The limit above is narrowed, not removed.
   - 2026-11-17: the owner attached the signed notice `fingerprint-notice-2026-11-16.txt` and its `.sig` to the same blog post. The files downloaded from the
     post are byte-identical to the repository copies (notice sha256 `6e761192...45a7`) and the signature verifies with the published key, so the
     blog copy is checkable against the key itself, not only against the post text.
3. **Sign the two published host-tools bundles** (`v0.1.0-hosttools.20261116.linux-x86_64.1`, `...linux-aarch64.1`) as `.sig` files added to
   their releases, then the Dev SDK Preview archives and the `crt-cli` Preview, using the procedure above. Adding signature files is not a
   replacement of accepted assets; record it as such.
   - 2026-11-16: signed and attached as `crt-<tag>.tar.xz.sig` (256 bytes, RSA-2048 PSS) to both host-tools releases
     (`...linux-x86_64.1`, `...linux-aarch64.1`); no existing asset was changed. Verified from the public download URLs with `openssl` and
     with `crt-cli fetch --publisher-key --signature` (`publisher_authentication: signature_verified` for both; the x86_64 signature
     against the aarch64 archive is refused with `publisher_signature_invalid` and leaves no output). Only these two assets are signed; the
     Dev SDK and `crt-cli` Previews, the release record and `SHA256SUMS` are not. Consumer pins and `sdk acquire` do not require the
     signature yet (step 4).
   - 2026-11-16: the main archive of each Dev SDK Preview (`crt` releases `v0.0.1-preview.20261110.3`, `.4`, `.linux.arm64.2`, `.macos.1`,
     `.windows.1`) and each `crt-cli` Preview (`crt-cli` releases `v0.1.0-preview.20261110.linux-x86_64.1`, `...macos-arm64.1`,
     `...windows-x86_64.1`, `v0.1.0-preview.20261111.linux-arm64.1`) was signed the same way; `<archive>.sig` was added to each release
     (9 files, no existing asset changed). Each was verified with `openssl` before upload (and matched the release `SHA256SUMS`), then from
     the public URL with `crt-cli fetch --publisher-key --signature` (`signature_verified` for all nine). Not signed: the `*-source.tar.xz`
     stage archives, `*-toolchain.json`, release manifests and `SHA256SUMS` files of those releases.
   - 2026-11-17: the two `crt-cli` Previews published by `release.yml` (`v0.1.0-preview.20261116.linux-x86_64.1`, `...linux-arm64.1`) were signed the same
     way and `<archive>.sig` was added to each (2 files, no existing asset changed); verified with `openssl` and from the public URL with
     `crt-cli fetch --publisher-key --signature` (`signature_verified` for both; a swapped signature is refused). The `SHA256SUMS-*` files of those
     releases are not signed.
4. **Teach consumers to require it.** `crt-cli`'s `sdk acquire` source file and the CI host-tools pin have no field for the key or the
   signature yet (only `fetch` takes them). Add `publisher_key` and signature locations to those inputs, report `signature_verified` in the
   acquisition record and let a pin require it. This is CLI work, not CRT's.
5. **Sign commits and tags** of release-bearing refs so the repository history that carries pins is authenticated by a key as well.
   - 2026-11-16: a **signed fingerprint notice** was issued first: [`fingerprint-notice-2026-11-16.txt`](fingerprint-notice-2026-11-16.txt) with its detached
     signature [`.sig`](fingerprint-notice-2026-11-16.txt.sig) (same RSA-PSS format; verification commands are in the notice). It states the fingerprint, the
     publication places, that releases are signed only when a `.sig` exists, and that a key change is valid only if announced by a notice signed with the
     previous key. Verified from the committed bytes with `openssl` and the stdlib verifier; the same signature fails on a one-byte-changed copy. It lets a
     user who recorded the fingerprint check this statement without trusting any web page, and lets a later rotation be chained to this key; it does not by
     itself prove who the key holder is, and signed commits/tags remain open.
6. **Key lifecycle policy.** Decide and document, before the first non-test signature is relied on:
   - *Expiry/review*: a review date for the key (for example two years) after which it is rotated or consciously renewed.
   - *Rotation*: publish the new fingerprint through every channel in step 2, sign the rotation notice with the old key, and overlap so older
     releases stay verifiable; new releases are signed by the new key.
   - *Revocation*: a revocation notice in this directory and in release notes naming the fingerprint and the earliest untrusted date; a
     consumer rejects signatures after that date and re-signs are issued only after the cause is understood.
   - *Recovery*: where the encrypted backup lives, who may restore it, and how loss is announced (a lost key means a new key and a full
     re-signing, not a silent substitution).
7. **Optional build provenance.** GitHub artifact attestations can state which workflow and commit produced an artifact. They complement
   the publisher signature and do not replace it: they prove where bytes were built, not that the project approved them.

## Verification record

| Date | Event |
| --- | --- |
| 2026-11-16 | Public key and fingerprint published in this repository; the fingerprint was recomputed from the published file and matches the owner's value. A temporary-key trial of the signing/verification commands passed against `crt-cli` (3/3). No release signed. |
