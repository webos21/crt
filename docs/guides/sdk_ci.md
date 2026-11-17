# Dev SDK CI and Release Workflows

This guide owns how GitHub Actions builds, verifies and (later) publishes the **Development SDK** of this repository. The
contract of the SDK itself is the [SDK development contract](../design/sdk_development_contract.md); evidence of what actually ran
is in [release qualification](../acceptance/release_qualification.md) and the [SDK acceptance](../acceptance/sdk_split_acceptance.md).
Open work and order are in [TODO](../../TODO.md#dev-sdk-github-cicd-integration-and-release-workflows). Full SDK delivery is a later,
separate step that reuses this flow.

## Roles and who builds what

| Repository | Final artifact | CI builds | CI consumes |
| --- | --- | --- | --- |
| `crt` (this one) | the Dev SDK asset set (archive, portable toolchain contract, source assets, release manifest, sums) | the SDK, once per host | published CLI Preview (consumer smoke, S2; pinned in `.github/ci/sdk-pins.json`) |
| `crt-cli` | the `crt-cli` executable | the CLI | the *published* Dev SDK, pinned by digest and publisher signature |

Neither repository rebuilds the other's source. The producer publishes and signs; the consumer adopts through its own pin promotion
([policy](https://github.com/webos21/crt-cli/blob/main/docs/guides/development.md#ci-pin-promotion)). The order is always
publish -> sign -> update the consumer lock.

## Workflows

| Workflow | Trigger | Does | Publishes |
| --- | --- | --- | --- |
| `ci.yml` | push, pull request | the required 5-host build and test gate; **stays the required check** | nothing |
| `host-tools.yml` | manual | assembles and replays the independent host-tools bundle | nothing (artifacts only) |
| `sdk-integration.yml` | SDK-relevant push to `main`, weekly, manual | builds `crt-dev-release` on Linux x86_64/aarch64, macOS/arm64 and Windows/x64 and arm64, verifies the asset set from its bytes, runs the extracted archive as an external consumer with the producer checkout hidden | nothing (14-day artifacts) |
| `sdk-release.yml` | manual, `dry_run` default true | plan -> package -> verify -> approval-gated publish (below) | a non-latest prerelease, only after a `release` environment reviewer approves |

`sdk-integration` is deliberately **not** a required check: it is long (a full SDK build per host) and ci.yml already gates every
change. A red scheduled or path-triggered run is a regression or a changed external input, not noise.

## What `sdk-integration` proves

1. **Verifier tests** (`tools/test_ci_sdk_release.py`, `tools/test_sdk_workflows.py`): the verifier accepts a faithful set and rejects each
   tampering; workflow pin rules hold (below).
2. **Build** with the CI-only tag `v0.0.1-ci.<run id>`, which is a valid release tag but never a Preview name and is never published.
   Same configuration as the Previews: Release, imported libc++, Skia/FFmpeg/curl/LVGL off.
3. **`tools/ci_sdk_release.py verify`** re-derives from the bytes, importing none of the producer's code: the directory holds exactly the
   manifest's assets plus manifest and sums; sizes and SHA-256 agree between files, manifest and sums; the manifest names the expected
   profile, version, target and producer commit (`$GITHUB_SHA`); the archive extracts safely and boundedly to one `dev/` root; the install
   metadata (counts, sizes and the content identity) is true of the extracted tree; `VERSION`/`manifest.json` agree; the portable toolchain
   contract is bound to that content identity and commit and contains neither `external_os_sdk` nor the runner's workspace/temp paths.
   It prints the **manifest pin** (SHA-256 and size of the release manifest) that notes and consumer pins quote. Checked against the
   published Linux Preview `v0.0.1-preview.20261110.4`, it reproduces the documented pin `983dbcf1...b2c3` and content identity.
4. **External consumer** (`tools/test_dev_sdk_external.py`) on the *extracted* archive, the shipped bytes, not the build tree: static and
   shared C++ consumers build and run with the producer checkout and original SDK inaccessible (bubblewrap, Seatbelt, restricted token).

5. **Consumer smoke** (`tools/ci_sdk_cli_smoke.py`, S2): the published CLI Preview pinned in `.github/ci/sdk-pins.json` is downloaded; its size,
   SHA-256 and the detached publisher signature (RSA-PSS, checked with `openssl`, key fingerprint pinned) must match before it runs. It then
   `sdk install`s the archive just built using the manifest SHA-256 computed from those assets, the installed content identity must equal
   the manifest's, and a C and a C++ project run `new`, `build`, `test`, `run` and a warm rebuild that must reuse the whole cache. The only
   external inputs are the compiler/linker tools (LLVM 21 on Linux, Xcode tools on macOS, LLVM 22.1.8 plus the Windows import libraries).

Not proven here: Full SDK content, GPU/window/media behaviour, publisher authenticity
(the signature is made offline by the owner after publication; see [publisher signing](../security/publisher_signing.md)).

## Pin rules (enforced by `tools/test_sdk_workflows.py`)

- Every `uses:` is a full 40-hex commit with its version in a trailing `# vX.Y.Z` comment; never a moving tag.
- `sdk-integration.yml` has `permissions: contents: read` only, no secrets, no `gh release`.
- LLVM is pinned in the workflow: **21** from the Ubuntu 26.04 archive on Linux (TLS descriptor code generation needs it), **22.1.8**
  (`llvmorg-22.1.8`) on Windows, Apple clang on macOS. Changing one is a reviewed change with its own evidence.
- Every script a workflow names and every push trigger path must exist.
- Unsupported OS/architecture pairs (macOS/x86_64, Linux/riscv) are **unverified**, not silently skipped: they have no matrix row and the
  guide above says so.
- Windows/arm64 (`windows-aarch64`, `windows-11-arm`) has a matrix row, but no published crt-cli Preview exists for it, so
  `.github/ci/sdk-pins.json` has no pin and the consumer smoke (S2) cannot run. Its row says `cli_preview: none`: the run logs a notice
  and a "NOT RUN" summary line instead of silently passing, and `tools/test_sdk_workflows.py` fails if a host has neither a pin nor that
  field, or both. Adopt the host by adding its pin and deleting the field together.
- The Windows isolation proof (the extracted archive consumed under a restricted token with the producer hidden) needs the producer checkout
  and the extracted SDK to be private to the runner user. On `windows-11-arm` the workspace is under `C:` and keeps `Users` and
  `Authenticated Users` entries after `icacls /inheritance:r` (which copies inherited entries) and `/grant:r` (which replaces only the accounts it
  names), so the first hosted run reported the checkout visible; the workflow now also removes those groups with `/remove:g` and prints the
  resulting ACL. The windows-latest workspace on `D:` was already private. The proof's denial check was not changed.

## Running the pieces locally

```bash
python3 tools/ci_sdk_release.py verify --assets <release/dev dir> --os linux --arch x86_64 \
    --version <tag> --commit <full sha> --forbid "$PWD" --json verification.json
CRT_SDK_VERIFY_REAL_ASSETS=<dir with a published Preview's assets> python3 -m unittest tools.test_ci_sdk_release
```

The build directory must be `<checkout>/out/<name>` (the workflow uses `out/sdk-ci`): `tools/crt-port-build.py` derives its port source
root from that shape, and an external `-B` fails with "source not found". The release collector requires a clean tree and a checkout equal to the SDK's producer commit, so a local build for `crt-dev-release`
uses a clean `git worktree` of the commit under test.

### Replaying a macOS port-test failure

After the build reaches a port test, reuse the retained `dev-*` directory under
`out/sdk-ci/development-sdk-work/` instead of rebuilding libc++ and the network ports.
On the retained staging SDK (not a previously published archive):

```bash
python3 tools/crt-port-build.py --sdk-root "$work/dev" --target-os macos --target-arch aarch64 \
    --source-root "$work/port-sources" --work-root "$work/port-build-utilities" \
    --install-prefix "$work/dev" --port nlohmann-json --test
otool -L "$work/port-build-utilities/tests/nlohmann-json/roundtrip-static"
otool -L "$work/port-build-utilities/tests/nlohmann-json/roundtrip-shared"
```

Use the actual retained directory for `work`. `otool -L` reports declared imports;
`DYLD_PRINT_LIBRARIES=1` on direct execution shows the mapped images. Do not set
`DYLD_LIBRARY_PATH` to the SDK globally: even a static CRT consumer can load Apple
libc++ through libSystem. Check curl's `port-build-curl/work/curl/config.log` for
the actual mbedTLS `conftest` link and execution. See the
[macOS replay](../acceptance/release_qualification.md#macos-dyld-investigation-and-local-replay-2026-11-17)
for the native versus hosted scope.

## Releasing a Dev SDK Preview (`sdk-release.yml`)

CI success is necessary, never publication authority. The workflow is manual (`workflow_dispatch`) and `dry_run` defaults to **true**.

| Job | Runner | Permission | Does |
| --- | --- | --- | --- |
| `plan` | ubuntu-24.04 | read (+ `actions: read`) | refuses the request unless: host/date/serial are valid, the commit is a full SHA that is an ancestor of `origin/main` with a successful `CI` run, no release of that tag exists, the notes are a `docs/releases/*.md` file at that commit that renders, and the `release` environment **has required reviewers** (GitHub auto-creates a missing environment with no approval, so its absence must fail, not pass) |
| `package` | the host's runner | read | builds exactly that commit with `crt-dev-release` under the real tag; `ci_sdk_release.py verify` on the result; uploads the six assets |
| `verify` | a fresh runner of the same host | read | downloads the artifact, re-verifies it, and runs the S2 consumer smoke (pinned CLI Preview installs the packaged archive; C and C++ build/test/run, cold and warm) |
| `publish` | ubuntu-24.04 | **write**, behind environment `release` | waits for a reviewer, re-verifies the bytes, stages a draft, downloads it back and compares every byte, then (not in a dry run) publishes a non-latest prerelease. Never rebuilds, never overwrites |

A dry run goes through the same approval and stops at the verified draft; delete that draft by hand. The tag is
`v0.0.1-preview.<date>.<os>.<arch>.<serial>` (for example `v0.0.1-preview.20261118.linux.x64.1`; a prerelease identifier cannot contain `_`).
Host keys: `linux-x64`, `linux-arm64`, `macos-arm64`, `windows-x64`; Windows/arm64 has no consumer smoke yet and is not offered.

**Notes** are a reviewed file: copy `docs/releases/sdk_release_notes_template.md` to `docs/releases/release_notes_<tag>.md` in a commit that precedes the release
commit. The workflow fills `{{TAG}} {{COMMIT}} {{TARGET}} {{MANIFEST_SHA256}} {{MANIFEST_BYTES}} {{CONTENT_SHA256}} {{ASSET_TABLE}}` from the verified bytes; any other token,
or a template without the pins, stops it.

**One-time setup** (repository settings, done by the owner): create the `release` environment with the owner as required reviewer.
Publication does not authenticate the publisher: the owner signs the archive offline and attaches `.sig`
([procedure](../security/publisher_signing.md)); the consumer pin moves only after that.
