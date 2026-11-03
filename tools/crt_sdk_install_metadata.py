#!/usr/bin/env python3
"""Install metadata of a packaged SDK: what a consumer may check before and after it extracts the archive.

The release manifest carries one ``install`` object per SDK archive (format 1). It describes the *extracted* tree, so a
consumer (crt-cli ``sdk install``) can bound extraction before it starts and prove afterwards that exactly the promised
tree arrived:

* ``root``: the single top-level directory the archive extracts to (``dev``); the consumer looks through exactly this one.
* ``content_sha256``: SHA-256 of the compact JSON inventory, the same value as ``sdk_content_sha256`` of the toolchain
  provider (``tools/export_cli_toolchain.py``) and as the CLI's own inventory hash. The inventory lists files (SHA-256)
  and symlinks (link text) only, sorted component-wise and case-sensitively; directories and file modes are not part of
  the identity.
* ``entries``/``files``/``directories``/``symlinks``: counts of the archive entries below ``root`` plus the root itself
  (counted as a directory), i.e. what a safe extractor counts.
* ``uncompressed_bytes``/``max_file_bytes``/``max_path_bytes``/``max_depth``: totals and maxima over the tree, with paths
  measured relative to the archive top (``dev/...``) in UTF-8 bytes and components.

Nothing here is a signature: the archive digest still comes from a trusted manifest digest, and these numbers only make
extraction bounded and verifiable.
"""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path

FORMAT = 1


def file_digest(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def inventory(sdk: Path) -> list[list[str]]:
    """Component-wise, case-sensitive order: the CLI's std::filesystem::path sort on every host (Python's own Path
    ordering is case-insensitive on Windows and would make the same SDK hash differently there)."""
    items = []
    for path in sorted(sdk.rglob("*"), key=lambda item: item.relative_to(sdk).parts):
        relative = path.relative_to(sdk).as_posix()
        if path.is_symlink():
            path.resolve(strict=True).relative_to(sdk.resolve())
            items.append([relative, "symlink", os.readlink(path)])
        elif path.is_file():
            items.append([relative, "file", file_digest(path)])
    return items


def content_sha256(items: list[list[str]]) -> str:
    return hashlib.sha256(json.dumps(items, ensure_ascii=False, separators=(",", ":")).encode()).hexdigest()


def install_metadata(sdk: Path, archive_format: str) -> dict:
    """Metadata of an extracted SDK directory (named ``sdk.name`` in the archive)."""
    manifest = json.loads((sdk / "manifest.json").read_text(encoding="utf-8"))
    profile_path = sdk / "share/crt/contracts/development.json"
    profile = json.loads(profile_path.read_text(encoding="utf-8")).get("profile") if profile_path.is_file() else None
    items = inventory(sdk)
    files = directories = symlinks = total = largest = longest = deepest = 0
    directories = 1  # the root itself
    for path in sdk.rglob("*"):
        archive_path = (Path(sdk.name) / path.relative_to(sdk)).as_posix()
        longest = max(longest, len(archive_path.encode("utf-8")))
        deepest = max(deepest, len(archive_path.split("/")))
        if path.is_symlink():
            symlinks += 1
        elif path.is_dir():
            directories += 1
        elif path.is_file():
            files += 1
            size = path.stat().st_size
            total += size
            largest = max(largest, size)
    return {
        "format": FORMAT, "archive_format": archive_format, "root": sdk.name,
        "version": (sdk / "VERSION").read_text(encoding="utf-8").strip(),
        "stage": manifest["stage"], "profile": profile, "target": manifest["target"],
        "content_sha256": content_sha256(items),
        "entries": files + directories + symlinks, "files": files, "directories": directories, "symlinks": symlinks,
        "uncompressed_bytes": total, "max_file_bytes": largest, "max_path_bytes": longest, "max_depth": deepest,
    }
