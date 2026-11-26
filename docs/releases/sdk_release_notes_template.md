# CRT Development SDK {{TAG}} ({{TARGET}})

<!-- Reviewed template for `sdk-release.yml`. Copy to `docs/releases/release_notes_<tag>.md` in a reviewed commit that
     precedes the release commit, replace the prose, and keep the tokens: the build fills them after the bytes exist. Any other
     double-brace token, or a missing MANIFEST_SHA256/CONTENT_SHA256/ASSET_TABLE token, stops the workflow. -->

State in two or three sentences what changed relative to the previous release and why a consumer would adopt it. This is a stage
`02-cxx`, profile `dev` SDK; use the external LLVM tools named in the compatibility notes.

[Producer source](https://github.com/webos21/crt/commit/{{COMMIT}}) · [Release page](https://github.com/webos21/crt/releases/tag/{{TAG}}).

## Downloads and trusted pins

Obtain the manifest pin independently from this reviewed repository or the
owner's signed announcement. Before publication, the owner signs every
payload file offline and the separate finalizer verifies the detached
`<asset>.sig` files against the staged bytes.
[Verify with the independently pinned publisher key](https://github.com/webos21/crt/blob/main/docs/security/publisher_signing.md).

- Manifest SHA-256: **{{MANIFEST_SHA256}}** ({{MANIFEST_BYTES}} bytes).
- Installed SDK content: `{{CONTENT_SHA256}}`.

{{ASSET_TABLE}}

The manifest and checksum file for this target also have detached signatures.

## Qualification and migration

Link the acceptance record for this build (`sdk-integration` run, `sdk-release` run IDs) and state exactly what was and was not
exercised on this target. State the supported host minimum (macOS releases: macOS 15.0). Existing SDKs and project locks are not migrated; install alongside and select the new identity explicitly.
