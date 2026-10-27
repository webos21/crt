# Release qualification and assembly rehearsal

This record owns the identified artifacts and positive local collection evidence
for the active [release qualification work](../../TODO.md#release-qualification-and-assembly-rehearsal).
The [build-system scope](build_system_acceptance.md#release-qualification-scope-split-2026-10-23)
defines the gate; the completed [SDK split](sdk_split_acceptance.md) does not
qualify old candidates for release. CLI adoption remains consumer-owned.

## Execution scope

Start with native Linux/x86_64. Use `v0.4.0-rehearsal.20261027` as the local
rehearsal version parameter; no Git tag or release URL is created by selecting
that string. Freeze one clean CRT commit after the pin/tooling changes pass,
then generate the plain predecessor, Development SDK, cumulative Full `06-web`
SDK and corresponding stage-source assets from that identity. Existing
`development` SDKs remain evidence of their original builds and are not inputs
to positive release collection.

Run the actual `crt-dev-release` and `crt-full-release` targets with fresh
output directories. Require ordinary/profile verification, extracted archive
checks, manifests/checksums, known clean source assets, matching
producer/base/compile/collector identities and inherited runtime bytes.
Retain the Web advisory/support-window and source-offer checks. Review the
resulting artifacts before closing the tranche. Other-host native acceptance
of a new pin is recorded separately; patch application on Linux cannot grant it.

The existing owner Web approval is unchanged. Local assembly does not create a
tag, upload assets, publish a release, promote a CLI lock or claim SDK/CLI/Studio
compatibility. Those actions remain separate from this rehearsal.

## WebKit 2.54.2 source gate (2026-10-27)

Starting CRT baseline: `2dfd37c`. The previous Windows/x64 candidate correctly
reported a stale `2.54.1` micro pin. The official
[2.54.2 release](https://wpewebkit.org/release/wpewebkit-2.54.2.html), dated
2026-10-23, and the [release index](https://wpewebkit.org/releases/) confirm the
replacement. Local evidence root: `out/release-qualification-linux-20261027/pin/`.

- Archive: `wpewebkit-2.54.2.tar.xz`, **49,565,492 bytes**, SHA-256
  `f3284e19e47fcb5ac8c63c2e1c113dbd65b2423eb8a545f5b345707829004f56`,
  matching the published checksum.
- Both the detached archive signature and annotated tag signature verify with
  fingerprint `5AA3BC334FD7E3369E7C77B291C559DBE4C9123B` in a private GnuPG home.
  This verifies signatures under that key, not independent signer identity.
- Tag object `6c294e37d744618c3f9983b44c7759b8265ecf63` peels to commit
  `32717fabcb9dcad9aa00a54ef1f23e41ee64e849`; tree
  `952d1a2ab51b8c902861f5f3da1a3726b6b106b8`, **462,138** entries, recursive
  listing SHA-256 `67328e85b8da67d9355498a5aa4301bf9630b0c5ae9f301b0d66b5d40d9ba7c5`.
- Complete tarball/commit correspondence: **34,445** identical regular files,
  **130** ANGLE files matching after CRLF-to-LF normalization, **zero** content
  differences. Only the archive has **4,525** generated Documentation files and
  `NEWS`; only the commit has **35,810** additional `Source/` files.
- Regenerated license inventories: **64** unchanged license/notice files;
  **69,600** product-tree files classified. GPL/MPL/unknown classes remain
  explicit; their counts do not increase. This scan describes source files,
  not the binary's compiled-object license composition.
- Reviewed all seven changed upstream files touched by carried patches. The
  **23 patch files are unchanged**; **16** before/after hashes were refreshed
  for those files, including the stacked WebPage patch. Linux/macOS/Windows
  selections apply with **zero fuzz** (17/22/18 patches). Patch-manifest license
  and pristine-byte checks pass against the new product tree.
- Fresh official advisory snapshot: `passed`, `pin_is_latest_micro` and
  `series_within_grace` are true; latest listed advisory `WSA-2026-0006`.
  The snapshot alone keeps `public_release_allowed: false`; the existing owner
  decision and native packaging evidence are separate inputs.

Receipts: `identity.json`, `archive-signature.log`, `tag-signature.log`,
`correspondence.json`, `license-inventory.json`, `license-scan.json`,
`patch-replay.json`, `patch-{linux,macos,windows}.log` and `security.json`.
The pristine source checkout and original archive/signature remain retained.
Native compile/run of the new pin and real release collection are not yet
accepted by this source gate.

The normal patch harness also verifies all selected before/after hashes on
fresh disposable trees (`hash-verified-{linux,macos,windows}.log`); the pristine
product checkout remains clean. Focused unit suites: fetch **6/6**, licensing
**15/15**, JSC build helpers **13/13**, Web stage **47/49** with two
host-conditional skips. This does not claim native other-host execution.
