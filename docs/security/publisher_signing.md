# Publisher Signing Key and Roadmap

Owner: CRT. Status: **the key exists and its public half and fingerprint are published here; no release has been signed yet.**
Every published CRT and `crt-cli` artifact is therefore still `publisher_authentication: not_verified` and trusted only through digests
a caller pins ([Preview trust policy](../design/dev_preview_trust.md)). Publishing the key does not change that; the first signed
release does, and only for the assets it signs.

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

**Limit of this channel.** For now this repository document is the only publication of the fingerprint. It lives under the same GitHub
account as the releases, so an account takeover could change both. The fingerprint should also be published on at least one location the
GitHub account does not control (see Roadmap, step 2); until then a careful user should record the fingerprint once, today, from an
independent copy of this document, and treat any later change as suspect. The repository's commits are not signed either.

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

## What a signature does and does not establish

- It establishes that the holder of the private key signed exactly those bytes, provided the verifier obtained the public key independently.
- It does not establish that the bytes are safe or reviewed, who the key holder is beyond the fingerprint, or that the key was never
  compromised. It is not Authenticode, Developer ID notarization or any OS reputation signal; those remain separate and unclaimed.
- A signature does not replace the digest pins: keep pinning size and SHA-256, and add the signature check.

## Signing procedure (owner)

Signing is a deliberate, manual, offline step. CI never holds the private key.

1. Produce the release candidate in the workflow, download its artifacts and compare them with the candidate recorded in acceptance.
2. On the offline machine, check each asset's SHA-256 against the release record; sign each asset; verify each signature with the **public**
   key and the command above.
3. Upload the `.sig` files to the same release (new files only; accepted assets are never replaced) and record the signer's key fingerprint,
   the date and the assets signed in the acceptance record.
4. From a fresh workspace, download the public release and verify with `crt-cli fetch --publisher-key` or the `openssl` command.

## Roadmap

Ordered; each step needs its own approval. The owner decides keys, custody and which releases to sign.

1. **Publish the key and fingerprint** (this document). Done 2026-11-16.
2. **Publish the fingerprint on an independent channel.** At least one place that is not the CRT repository: for example GitHub Pages of the
   organization and one location outside GitHub. Record the locations here. Until a location outside the GitHub account exists, the limit
   above stands.
   - 2026-11-16: published on the project site `https://webos21.github.io/` ("배포 서명 공개 키 지문"; the same SHA-256 value, verified by
     fetching the page). This is a second *place*, but it is served from the same GitHub account, so it does not protect against an account
     takeover. A location outside GitHub (for example another domain or a signed announcement) is still open.
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
4. **Teach consumers to require it.** `crt-cli`'s `sdk acquire` source file and the CI host-tools pin have no field for the key or the
   signature yet (only `fetch` takes them). Add `publisher_key` and signature locations to those inputs, report `signature_verified` in the
   acquisition record and let a pin require it. This is CLI work, not CRT's.
5. **Sign commits and tags** of release-bearing refs so the repository history that carries pins is authenticated by a key as well.
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
