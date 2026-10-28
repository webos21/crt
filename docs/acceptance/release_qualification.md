# Release qualification and assembly rehearsal

This record owns the identified artifacts and positive local collection evidence
for the active [release qualification work](../../TODO.md#release-qualification-and-assembly-rehearsal).
The [build-system scope](build_system_acceptance.md#release-qualification-scope-split-2026-10-23)
defines the gate; the completed [SDK split](sdk_split_acceptance.md) does not
qualify old candidates for release. CLI adoption remains consumer-owned.

Linux/x86_64, macOS/arm64 and Windows/x64 local assembly pass below, and Linux/aarch64 passes natively for the Development profile. The
Linux/aarch64 Full `06-web` cross-target packaging (x86_64 host), followed by the combined final review,
remain open. Existing SDK split evidence stays accepted at its original pins.

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

## Linux/x86_64 positive assembly (2026-10-27)

**Accepted locally:** real Development and Full `06-web` collection, from clean
CRT `3741ea515f129483efa631c6d2663d27de9ebc62`, version parameter
`v0.4.0-rehearsal.20261027`, native Linux/x86_64, Clang 21.1.8, Debug. The local
baseline commit freezes the source; selecting the version creates no Git tag.
No dirty allowance, metadata-only repair, predecessor relabeling or old CRT
binary reuse was used. Source-only/download caches were copied to a fresh
build root; `source-caches.json` identifies them. The verified WebKit source
cache contains the new commit and pristine checkout, not a previous Web build.

From that exact clean commit, with a fresh output root and the verified source
cache prepared there, the native entry points are:

```sh
rehearsal_root="$PWD/out/release-qualification-linux-20261027"
cmake --fresh --preset linux-host-ninja-debug -B "$rehearsal_root/build" \
  -DCRT_RELEASE_TAG=v0.4.0-rehearsal.20261027 \
  -DCRT_WEB_SOURCE_CACHE="$rehearsal_root/source-cache" -DCRT_WEB_JOBS=6
cmake --build "$rehearsal_root/build" --target crt-development-sdk-test -j 6
cmake --build "$rehearsal_root/build" \
  --target crt-c-test crt-dev-release crt-full-release -j 6
cmake --build "$rehearsal_root/build" --target crt-cxx-test -j 6
python3 tools/test_sdk_split_s5.py \
  --sdk-root "$rehearsal_root/build/dist/dev" \
  --full-sdk "$rehearsal_root/build/dist/06-web" \
  --output "$rehearsal_root/s5-replay"
```

Choose a different fresh evidence root when repeating; the strict collectors
and S5 driver refuse existing output identities. Documentation added after this
run does not change the producer commit: collect from `3741ea5`, not a newer
documentation commit. The direct external Dev consumer was also run before
Full production (`dev-external/`); S5 repeats it on its extracted package.

The Dev producer/base/compile and Full/collector source identities all match the
fixed clean commit. All five stage-source assets (`02` through `06`) retain
that commit and `working_tree_dirty: false`. Both SDK `VERSION` files, recipe
download URLs and release manifests use the selected rehearsal version.
All **23** inherited runtime digests match across the plain predecessor, Dev
and Full inputs. Original Dev/Full payloads remain unchanged by S5.

| Collected artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `crt-v0.4.0-rehearsal.20261027-linux-x86_64-dev.tar.xz` | 8,939,540 | `beb0783073547cca309b8880542e2332ac46e43a5afa08c95b3dbe3cbf891b24` |
| `crt-v0.4.0-rehearsal.20261027-linux-x86_64-06-web.tar.xz` | 96,953,696 | `0c27d9f89bc78f04baa38000fc09eda00b44157e3092074205f031a8791b99c3` |
| `crt-v0.4.0-rehearsal.20261027-corresponding-source.tar.xz` | 25,235,504 | `b04a5d60a854152381619339fc68350a9ecd4f4e945908546d7ba89bc36710a7` |

The Dev release manifest lists its SDK plus two stage-source assets; Full lists
its SDK, all five stage-source assets and the corresponding-source bundle.
Every listed size/SHA-256 matches both the actual file and the respective
`SHA256SUMS` entry. Release-manifest SHA-256 values:

- Dev: `9cdc805dfc0990c6c4e4d8a7ab07f3e74062aa3e82d711ca0b72425a86298e8c`.
- Full: `e233c834d056a687805c9b46b39e02e86bdd8739288743e0858d66c7222c7165`.

Independent extraction compares **2,579** Dev and **3,641** Full file/symlink
paths exactly to their original SDK inventories. The extracted Full lives in
`Full release rechecked/06-web` (spaces in the path), passes `verify_dist.py`,
the real collector `check_pair` and its pinned clean Web source-asset check.
The Dev manifest SHA-256 is
`f03e27d8c647be57eceeebc008ffef74e07fc83bb9400f22e70d591bc5b001b9`;
Full is `82da1d02090da04ecfa231d96bde8b2abb4a3283acfabf377bca2a41951136fc`.

The corresponding-source bundle has **1,942** tracked source paths, preserving
file/link types and executable modes, plus `CORRESPONDING-SOURCE.json` identifying
the same CRT commit/version and WebKit 2.54.2 archive hash. **1,937** paths match
Git blobs directly. The remaining five are exactly the baseline
`.gitattributes` CRLF conversion: `tools/crt-ar.cmd`, `crt-c++.cmd`, `crt-cc.cmd`,
`crt-env.cmd` and `crt-env.ps1`. Each is checked against the declared text/eol
attributes and exact LF-to-CRLF transformation; no other mismatch is accepted.
The initial raw-blob-only audit stopped at `crt-ar.cmd`; its script/log and
first extraction remain intact. The corrected audit changes no artifact bytes.

Native validation:

- Core CTest **144/144**, zero skips/failures; imported libc++ runtime,
  string-ABI and iostream smokes pass in static and shared linkage (**6**).
- Focused source/release unit suites: **164/168**, four host-conditional skips.
- Source-hidden relocated Dev consumer: static/shared HTTPS+inflate, wrong CA,
  missing explicit CA despite valid ambient trust and malformed CA (**8**).
- S5 packages the new Dev, repeats that consumer and accepts all **8** required
  negatives including stale Full pairing; its receipt keeps the original SDK
  inventories unchanged. It remains a local S5 receipt, not a release promotion.
- New-pin isolated Web build, API/network/trust checks, SDK verification and
  relocated packaged browser pass. The observation sees HostProcess,
  NetworkProcess and WebProcess, **113** mapped files, one C++ runtime per role,
  zero violations and zero remaining owned processes. `test-result.json`
  records the successful `crt-web-test` replay.
- Fresh packaged security report: 2.54.2, newest micro, advisory pass, support
  window pass. The existing owner record yields `public_release_allowed: true`
  with no open blockers. No approval record was changed; this local collector
  result is not authorization to create a tag or publish these assets.

Evidence stays under `out/release-qualification-linux-20261027/`:
`baseline.json`, `source-caches.json`, `configure.log`, `dev-build.log`,
`release-build.log`, `cxx-test.log`, `dev-external/`, `s5-replay/`,
`dev-assembly-result.json`, `full-assembly-result.json` and `final-result.json`.
The final receipt hashes the validation scripts, release manifests and original
receipts. Audit commands are retained as `check_dev_assembly.py` and
`check_full_assembly.py`; the initial diagnostic is in
`check_full_assembly-raw-blob.py` / `full-assembly-check-raw-blob.log`.
Packaged Web evidence is under
`build/isolated-web/test-ik01zxff/packaged-browser-observation/result.json`;
collected artifacts/manifests/checksums are in `build/release/{dev,full}/`.

**Not run on Linux:** new-pin native macOS/arm64 and Windows/x64, new-pin Linux/aarch64
cross-target packaging and the combined release review. Windows/arm64's
accepted S5 Full reference is `05-ui`; it is not a `06-web` collector proof.
Producer promotion, consumer locks/adoption, CRT Git tag creation, upload and
public release are not performed by this rehearsal.

## macOS/arm64 positive assembly (2026-10-28)

**Accepted locally:** real Development and Full `06-web` collection, native
macOS/arm64 (preset `macos-host-ninja-debug`, Debug), from the **same** clean CRT
commit `3741ea515f129483efa631c6d2663d27de9ebc62` and version parameter
`v0.4.0-rehearsal.20261027` as the Linux run. The commit was checked out as a
detached worktree (`git worktree add --detach ../crt-rq-3741ea5 3741ea5`) so the
later documentation commits do not change the producer identity; no Git tag was
created. No source cache was supplied: Skia, ports, LVGL, libc++ and the WebKit
2.54.2 product were fetched and built fresh.

```sh
cd ../crt-rq-3741ea5
rehearsal_root="$PWD/out/release-qualification-macos-20261028"
cmake --fresh --preset macos-host-ninja-debug -B "$rehearsal_root/build" \
  -DCRT_RELEASE_TAG=v0.4.0-rehearsal.20261027 -DCRT_WEB_JOBS=8
cmake --build "$rehearsal_root/build" --target crt-development-sdk-test -j 8
cmake --build "$rehearsal_root/build" --target crt-c-test crt-dev-release crt-full-release -j 8
cmake --build "$rehearsal_root/build" --target crt-cxx-test -j 8
python3 tools/test_sdk_split_s5.py --sdk-root "$rehearsal_root/build/dist/dev" \
  --full-sdk "$rehearsal_root/build/dist/06-web" --output "$rehearsal_root/s5-replay"
```

The build directory must live under the checkout's own `out/`:
`crt-port-build.py --preset` reconstructs `root/out/<name>` from the directory
name, so a `-B` outside it fails at the `make` port with `source not found`
(first attempt, retained as `out/rq-macos-failed-wrongroot/` in the main
checkout). This is the documented `CRT_BUILD_PRESET_NAME` limitation, not a new
defect.

Results (all exit codes 0): Development SDK build and external consumer
(static/shared HTTPS+inflate, trust negatives), `crt-c-test` **143/143**,
imported libc++ string-ABI/iostream smokes, `crt-dev-release`,
`crt-full-release` (including the isolated `crt-web-build`, `crt-web-test` and the
relocated packaged browser: HostProcess/NetworkProcess/WebProcess, one C++
runtime per role, zero violations, zero remaining owned processes).

| Collected artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `crt-v0.4.0-rehearsal.20261027-macos-aarch64-dev.tar.xz` | 6,359,584 | `fa7cd04c6d20a3d6ec8c88c496e3276c823a50df9474b7e787401c59ecacb835` |
| `crt-v0.4.0-rehearsal.20261027-macos-aarch64-06-web.tar.xz` | 83,885,684 | `c718051f330cd55f973dbf82a12db96d45d58750db9035b543a7b00d9d4acb35` |
| `crt-v0.4.0-rehearsal.20261027-corresponding-source.tar.xz` | 25,235,504 | `b04a5d60a854152381619339fc68350a9ecd4f4e945908546d7ba89bc36710a7` |

The corresponding-source bundle is byte-identical to the Linux/x86_64 one, as
expected for the same commit. Release-manifest SHA-256: Dev
`a41b25d095ac6b3ec3c15559d8479f96eb20f4e4eb4e26c0ec475010c8329051`, Full
`6d99761710df41db284d1f9ad6c1713f728232c5ccb4309be217f555a3d037e6`. Both
`SHA256SUMS-*` files verify every listed asset (Dev: SDK + 02/03 source assets;
Full: SDK + 02-06 source assets + corresponding source).

Independent checks: extracted archive inventories equal the original SDK
directories exactly (Dev **2,570**, Full **3,603** files/symlinks, content
hashes included); the **18** `runtime_sha256` files of the Development contract
match in both Dev and Full; the Development contract records the fixed commit
with `producer_working_tree_dirty: false` and base commit equal to it; all five
stage-source assets carry that commit with `working_tree_dirty: false`; the
corresponding-source bundle names the same commit/version. S5 passes natively
(external relocated consumer plus **8** negatives, original payloads
unchanged, no promotion/adoption). The packaged `web-security.json` reports
2.54.2 as the newest micro, advisory `passed`, within the support window; the
raw snapshot keeps `public_release_allowed: false` by design, and
`crt_web_release.decide()` with the existing owner record yields `true` with no
open blockers. No approval record was changed.

Observation: 10 port-library files outside the runtime contract
(`libz`, `libmbedtls/crypto/x509` and `libcurl` archives/dylibs) differ in bytes
between the Dev and Full SDKs on macOS because the Full stage rebuilds them. The
contract compares only the 18 runtime files, so this is not a gate failure, but
it was not examined further here.

Evidence: `<worktree>/out/release-qualification-macos-20261028/`
(`configure.log`, `dev-build.log`, `release-build.log`, `cxx-test.log`,
`s5.log`, `s5-replay/`, `build/release/{dev,full}/`,
`build/isolated-web/test-gxbeqdsh/`); the worktree is a sibling of the main
checkout and is not committed.

**Not run on macOS:** Windows/x64 native, Linux/aarch64 cross-target packaging
and the combined release review. Producer promotion, consumer locks, tag
creation, upload and public release remain out of scope.

## Windows/x64 positive assembly (2026-10-28)

**Accepted locally:** real Development and Full `06-web` collection, native
Windows/x64 (preset `windows-host-ninja-debug`, Debug, Clang 22.1.8, Windows SDK
10.0.28000.0), from the **same** clean CRT commit
`3741ea515f129483efa631c6d2663d27de9ebc62` and version parameter
`v0.4.0-rehearsal.20261027` as the Linux and macOS runs. The commit was a detached
worktree (`git worktree add --detach C:/crtw/rqw 3741ea5`), so later documentation
commits do not change the producer identity; no Git tag was created. The Windows
S5 and release evidence for the previous pin (`2.54.1`) is not reused.

The WebKit 2.54.2 source cache was prepared on this host as in the Linux run:
`tools/fetch_webkit.py` (archive 49,565,492 bytes, SHA-256 `f3284e19…4f56`, matching
the recipe) and `tools/fetch_webkit_commit.py --checkout --correspondence`
(commit `32717fab…`, tree and 462,138-entry listing match the pin; **34,445**
identical files, **130** equal after CRLF-to-LF, **zero** content differences, as
on Linux), validated by the builder's own `validate_source_cache`. The archive
signature file was downloaded but not re-verified here (no public key was fetched);
the signature evidence for this pin is the Linux record. Of the other sources only
the earlier Windows run's `curl`, `ffmpeg`, `freetype`, `mbedtls`, `zlib` and `make`
archives were copied in (re-verified by size and SHA-256 when used); Skia, libc++,
LVGL, mingw-w64 headers and the Web ports were fetched fresh.

```sh
# C:/crtw/rqw, build tree and evidence under out/rq (driver: out/rq/run_rq.py)
cmake --fresh --preset windows-host-ninja-debug -B out/rq/build \
  -DCRT_RELEASE_TAG=v0.4.0-rehearsal.20261027 \
  -DCRT_WEB_SOURCE_CACHE=out/rq/pin/source-cache -DCRT_WEB_JOBS=8
cmake --build out/rq/build --target crt-development-sdk-test --parallel 8
cmake --build out/rq/build --target crt-c-test crt-dev-release crt-full-release --parallel 8
cmake --build out/rq/build --target crt-cxx-test --parallel 8
python tools/test_sdk_split_s5.py --sdk-root out/rq/build/dist/dev \
  --full-sdk out/rq/build/dist/06-web --output out/rq/s5-replay
```

Host environment, as in the earlier Windows run: external Clang/AR/RANLIB, the
Windows SDK `um/x64` import libraries (`CRT_WINDOWS_SDK_LIBPATH`), the `TEMP` of
the producer under the evidence root, and the pure Perl modules Git's POSIX Perl
lacks copied from Strawberry Perl to `host-perl-modules/` (`host-perl-inputs.json`)
for `CRT_WEB_HOST_PERL_LIB`. Two host preconditions mattered:

- **Path length.** The first attempt under `out/release-qualification-windows-20261028`
  failed in the Web stage: unpacking EGL-Registry needs a directory whose path was
  one character beyond the 247-character Win32 directory limit
  (`.../docbook4`). The root was shortened (`out/rq`) and the run restarted from
  `configure`; the failure is retained as `out/attempt1-path-too-long/`.
- **Producer visibility.** The S5 source-hidden consumer uses a restricted token and
  relies on the producer tree not being readable through the public Users/Everyone
  grants (as under the user profile). `C:/crtw` grants `BUILTIN\Users` read, so the
  first S5 attempt correctly failed with "producer/SDK root remains visible"
  (`out/rq/s5-replay-attempt1-visible-root/`). With the owner's approval the ACL of
  that one scratch directory, `C:/crtw/rqw`, was made private (inheritance removed;
  Lee, SYSTEM and Administrators only; the previous ACL is saved in
  `out/rq/acl-before-rqw.txt`). No other path, account or machine policy changed,
  and the S5 test itself is unchanged.

Results (all exit codes 0): Development SDK build and external consumer (static and
shared HTTPS+inflate, trust negatives; `development-consumer/result.json` passed,
two link modes), `crt-c-test` **164/164**, imported libc++ smokes (`crt-cxx-test`),
`crt-dev-release` and `crt-full-release` including the isolated `crt-web-build` and
`crt-web-test`. Durations: development SDK test 836 s, release build 5,149 s,
C++ test 25 s, S5 replay 137 s. The relocated packaged browser sees HostProcess,
NetworkProcess and WebProcess with **141** mapped files, one C++ runtime per role,
zero violations and zero remaining owned processes; the packaged network probe passes
**10** cases and the Host ABI audit reports no violation (28 libraries, 4 known host
libraries).

| Collected artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `crt-v0.4.0-rehearsal.20261027-windows-x86_64-dev.zip` | 144,697,997 | `9bf56d46deaa4c5905b50ae4faacfd0f97f315ad723e6f773f667815a8970fff` |
| `crt-v0.4.0-rehearsal.20261027-windows-x86_64-06-web.zip` | 380,081,238 | `cd9c8ec4e602a1ca25514efb18b6ae57a0084eb17ff6e958d6ec6235fd6dafca` |
| `crt-v0.4.0-rehearsal.20261027-corresponding-source.tar.xz` | 25,235,504 | `b04a5d60a854152381619339fc68350a9ecd4f4e945908546d7ba89bc36710a7` |

The corresponding-source bundle is again byte-identical to the Linux and macOS
bundle (1,942 tracked paths, same commit and version). Release-manifest SHA-256: Dev
`26a9f6ce71962369fe4d5a133436315ac31bd965526b5204afc51a8ac0ffa00a`, Full
`2bd189d84645182075fb4abeb71506916323cfea31ca8ec8b0d3827182edffbb`; SDK manifests:
Dev `f502ebcbe35e356e2480739ffb22ccd1741be779c532bde07c8072453edb4923`, Full
`505504d7bc17e5e7c06f40393bda23b2f8200fdf613832dccc8c12330ebcb590`.

Independent checks (`out/rq/check_assembly.py`, **37** checks, `assembly-result.json`
`passed: true`): every listed size and SHA-256 matches the file and its
`SHA256SUMS-*` entry and no unlisted file exists; ZIP CRCs are good; the extracted
archives equal the original SDK directories exactly (Dev **2,695**, Full **3,673**
paths, bytes included; the two declared zlib aliases `lib/libz.so` and
`lib/libz.so.1` are ZIP-materialized files); the **27** inherited runtime digests
match across plain `02-cxx`, Dev, `05-ui` and `06-web`; all five stage-source
assets and every SDK manifest name the fixed commit, the sources report
`working_tree_dirty: false` and every `VERSION` is the rehearsal version; the
packaged `web-security.json` reports 2.54.2, newest micro, advisory `passed` and
`series_within_grace: true`. The raw snapshot keeps `public_release_allowed: false`
by design; the SDK's recorded release decision from the existing owner approval is
`true` with no open blockers. No approval record was changed.

S5 passes natively on the second attempt (`s5-replay/handoff.json`): relocated
Development SDK with **2,695** payload paths, restricted-token isolation with the
producer source/build and the original SDK hidden, static and shared linkage each with
verified, wrong-CA, missing-CA and malformed-CA cases (**8**), shared closure from the
package only, **8** negative cases, `original_payloads_unchanged: true`,
`promotion: not-promoted`, `release_qualified: false`, `cli_adoption: not-run`.

Focused unit suites on the pinned checkout (fetch, licensing, Web release, source
offer, relink kit, license choices, release assets, verify_dist, create_dist, Web
stage): **173** tests, OK, **8** host-conditional skips. A run with `TEMP` inside the
checkout fails `test_commit_lookup_tolerates_a_source_asset_without_git` because
that fixture's "no Git" directory is then inside the repository; with the ordinary
temporary directory it passes. This is the same fixture/`TEMP` interaction recorded
for the earlier Windows run, not a product failure.

Evidence: `C:/crtw/rqw/out/rq/` (`status.json`, `logs/`, `source-caches.json`,
`pin/{correspondence.json,archive/,source-cache/}`, `assembly-result.json`,
`check_assembly.py`, `s5-replay/`, `build/release/{dev,full}/`,
`build/isolated-web/test-2xjaj4vo/`); the worktree is outside the main checkout and
is not committed.

**Not run on Windows:** Linux/aarch64 cross-target packaging and the combined release
review. Windows/arm64 has no `06-web` collector scope (its Full reference is
`05-ui`). Producer promotion, consumer locks, tag creation, upload and public release
remain out of scope.

## Linux/aarch64 native Development assembly (2026-10-28)

**Accepted locally, Development profile only.** Native Linux/aarch64 (Ubuntu 26.04 QEMU guest, 4 CPU, Clang 21.1.8,
preset `linux-host-ninja-debug`, Debug), from the **same** clean CRT commit
`3741ea515f129483efa631c6d2663d27de9ebc62` and version parameter `v0.4.0-rehearsal.20261027` as the other hosts, in a
detached worktree (`../crt-rq-3741ea5`) so later documentation commits cannot change the producer identity. No Git tag was
created, nothing was uploaded, no source cache or earlier build was reused.

**Scope kept explicit:** the TODO's "Linux/aarch64 cross-target packaging" is the Full `06-web` SDK built on an x86_64
host; `build_stage_06_web.py` accepts Linux/aarch64 only as that cross target and rejects a native aarch64 build by design.
This device is native aarch64, so `crt-full-release`, the Full `06-web` collection and S5 (which needs a Full SDK) were
**not run here**. They remain with an x86_64 Linux host.

```sh
git worktree add --detach ../crt-rq-3741ea5 3741ea5 && cd ../crt-rq-3741ea5
rehearsal_root="$PWD/out/release-qualification-linux-aarch64-20261028"
cmake --fresh --preset linux-host-ninja-debug -B "$rehearsal_root/build" \
  -DCRT_RELEASE_TAG=v0.4.0-rehearsal.20261027
cmake --build "$rehearsal_root/build" --target crt-development-sdk-test -j 3
cmake --build "$rehearsal_root/build" --target crt-c-test crt-dev-release -j 3
cmake --build "$rehearsal_root/build" --target crt-cxx-test -j 3
python3 tools/test_dev_sdk_external.py --sdk-root "<extracted path with spaces>/dev" --output "$rehearsal_root/external"
```

Results (all exit codes 0): Development SDK build and consumer (static/shared HTTPS+inflate, trust negatives), `crt-c-test`
**144/144**, imported libc++ runtime/string-ABI/iostream smokes (static and shared), `crt-dev-release`.

| Collected artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `crt-v0.4.0-rehearsal.20261027-linux-aarch64-dev.tar.xz` | 8,787,100 | `1ae946c26d4d83236b024d5c3793294b6f1b4233407c1e0171666105bb56a925` |
| `crt-v0.4.0-rehearsal.20261027-linux-02-cxx-source.tar.xz` | 8,557,328 | `8fdd7229e65c8e005e15ba66c43aa4f688546b3f30bbf52b42906c69a160aefd` |
| `crt-v0.4.0-rehearsal.20261027-linux-03-gfx-simple-source.tar.xz` | 834,220 | `8d74625ebf9db236960b5b5fb355f1d2714393710f89d3625af0656ca0705adf` |

The Dev release manifest (SHA-256 `b23e2f17d6752bd9fdf97f2f119a63b97c679d33ca99371939ea53325e399495`) lists exactly these three
assets; `SHA256SUMS-dev-linux-aarch64` verifies each. The SDK `manifest.json` SHA-256 is
`8b70ae99417c0154df71a1537e20b606fb2c44b5947cdfd4be722553745f1359`.

Independent checks: the extracted archive's **2,579** paths equal the original SDK directory with identical content hashes
(the same count as the Linux/x86_64 Development SDK); the extracted package, in a path with spaces, passes `verify_dist.py
--stage 02-cxx`; all **23** `runtime_sha256` entries of the Development contract match in both the Dev SDK and the plain
`02-cxx`; the contract records `producer_source_commit` = `base_source_commit` = the fixed commit with
`producer_working_tree_dirty: false`, and both stage-source assets carry that commit with `working_tree_dirty: false`.
The extracted package then passes `tools/test_dev_sdk_external.py` with the producer checkout hidden: relocated static and
shared HTTPS+inflate and the trust negatives.

Not covered: a `06-web` collector/S5 proof for Linux/aarch64, Windows/arm64's `06-web` (it has no collector scope), producer
promotion, consumer locks, tag creation, upload and public release. Evidence:
`../crt-rq-3741ea5/out/release-qualification-linux-aarch64-20261028/` (`configure.log`, `dev-build.log`, `release-build.log`,
`cxx-test.log`, `external.log`, `build/release/dev/`); the worktree is outside the main checkout and is not committed.
