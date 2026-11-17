# Development SDK Preview {{TAG}} ({{TARGET}})

<!-- Reviewed template for `sdk-release.yml`. Copy to `docs/releases/release_notes_<tag>.md` in a reviewed commit that
     precedes the release commit, replace the prose, and keep the tokens: the build fills them after the bytes exist. Any other
     double-brace token, or a missing MANIFEST_SHA256/CONTENT_SHA256/ASSET_TABLE token, stops the workflow. -->

State in two or three sentences what changed relative to the previous Preview and why a consumer would adopt it. This is a stage
`02-cxx`, profile `dev` SDK; use the external LLVM tools named in the compatibility notes.

[Producer source](https://github.com/webos21/crt/commit/{{COMMIT}}) · [Release page](https://github.com/webos21/crt/releases/tag/{{TAG}}).

## Downloads and trusted pins

Obtain the manifest pin independently from this reviewed repository or the owner's signed announcement before accepting the
payload hashes below. Publisher authentication is **not** part of this publication: the owner signs the archive offline afterwards and
attaches `<archive>.sig`; until that file exists the release is unauthenticated.

- Manifest SHA-256: **{{MANIFEST_SHA256}}** ({{MANIFEST_BYTES}} bytes).
- Installed SDK content: `{{CONTENT_SHA256}}`.

{{ASSET_TABLE}}

The other files are the release manifest and the sums file for this target.

## Qualification and migration

Link the acceptance record for this build (`sdk-integration` run, `sdk-release` run IDs) and state exactly what was and was not
exercised on this target. Existing SDKs and project locks are not migrated; install alongside and select the new identity explicitly.
