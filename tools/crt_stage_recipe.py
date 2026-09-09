#!/usr/bin/env python3
"""Shared validation for pinned CRT source-stage recipes."""

import re
from pathlib import PurePosixPath


SCHEMA_VERSION = 2
TARGET_OSES = ("linux", "macos", "windows")
STAGE_SUCCESSORS = {
    "01-c": "02-cxx",
    "02-cxx": "03-gfx-simple",
    "03-gfx-simple": "04-gfx-media",
}


def validate_recipe(recipe: dict) -> None:
    required = (
        "schema_version", "input_stage", "output_stage", "target", "source", "build"
    )
    if (not isinstance(recipe, dict) or any(key not in recipe for key in required) or
            recipe["schema_version"] != SCHEMA_VERSION or
            not isinstance(recipe["input_stage"], str) or
            not re.fullmatch(r"[0-9]{2}-[a-z0-9-]+", recipe["input_stage"]) or
            not isinstance(recipe["output_stage"], str) or
            not re.fullmatch(r"[0-9]{2}-[a-z0-9-]+", recipe["output_stage"]) or
            STAGE_SUCCESSORS.get(recipe["input_stage"]) != recipe["output_stage"] or
            not isinstance(recipe["target"], dict) or
            recipe["target"].get("os") not in TARGET_OSES or
            set(recipe["target"]) != {"os"} or
            not isinstance(recipe["source"], dict) or
            not isinstance(recipe["build"], dict)):
        raise ValueError("invalid or unsupported CRT stage recipe")

    source = recipe["source"]
    archive = source.get("archive", "")
    archive_path = PurePosixPath(archive) if isinstance(archive, str) else None
    if (set(source) != {"url", "archive", "size", "sha256", "source_commit"} or
            not isinstance(source.get("size"), int) or source["size"] <= 0 or
            not re.fullmatch(r"[0-9a-f]{64}", source.get("sha256", "")) or
            not re.fullmatch(r"[0-9a-f]{40}", source.get("source_commit", "")) or
            not isinstance(source.get("url"), str) or not source["url"] or
            archive_path is None or archive_path.name != archive or
            "\\" in archive or not archive.endswith(".tar.xz")):
        raise ValueError("stage recipe source identity is incomplete")

    entrypoint = recipe["build"].get("entrypoint", "")
    entrypoint_path = PurePosixPath(entrypoint) if isinstance(entrypoint, str) else None
    if (set(recipe["build"]) != {"entrypoint"} or entrypoint_path is None or
            not entrypoint.endswith(".py") or entrypoint_path.is_absolute() or
            ".." in entrypoint_path.parts or "\\" in entrypoint):
        raise ValueError("stage recipe build entrypoint is missing")


def recipe_filename_for_stage(stage: str) -> str:
    successor = STAGE_SUCCESSORS.get(stage)
    if not successor:
        raise ValueError(f"stage has no source-stage successor: {stage}")
    return f"{successor}.json"
