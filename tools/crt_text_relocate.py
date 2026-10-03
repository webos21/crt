#!/usr/bin/env python3
"""Make text files installed into a staged SDK independent of the staging path.

An isolated stage installs its ports (curl, FreeType, FFmpeg, zlib, ...) with
`--install-prefix <temporary staged dir>` and then moves the tree to its final
location. Autotools/pkg-config bake that temporary prefix into text files:
`lib/pkgconfig/*.pc`, libtool `*.la` archives and `bin/*-config` scripts. The
binary equivalents (Mach-O/ELF RPATHs) were already made relocatable; this does
the same for text files so the published SDK names no build-host path.

* `*.la` archives are removed: libtool archives are not part of the CRT
  consumer interface and hold only stale absolute `libdir`/dependency paths.
* `.pc` files get `prefix=${pcfiledir}/<up to the root>`; every other
  occurrence of the old path becomes `${prefix}`.
* `bin/*` shell scripts get a prefix computed from the script's own location;
  other occurrences become `${prefix}`, closing and reopening single quotes where
  needed.

Anything else that still contains an old root is left untouched and returned, so
verify_dist.py's text check reports it instead of this tool guessing.
"""

from __future__ import annotations

import re
from pathlib import Path

SHELL_PREFIX_VARIABLES = ("prefix", "exec_prefix")


def root_spellings(*roots: Path | str) -> list[str]:
    """Every spelling of the staging roots that may be baked into a file."""
    spellings: set[str] = set()
    for root in roots:
        candidates = {str(root)}
        try:
            candidates.add(str(Path(root).resolve()))
        except OSError:
            pass
        for candidate in list(candidates):
            candidates.add(candidate.replace("\\", "/"))
            candidates.add(candidate.replace("/", "\\"))
        spellings.update(c.rstrip("/\\") for c in candidates if c)
    # Longest first so a path that contains another is replaced whole.
    return sorted(spellings, key=len, reverse=True)


def _read_text(path: Path) -> str | None:
    try:
        data = path.read_bytes()
    except OSError:
        return None
    if b"\0" in data[:4096]:
        return None
    try:
        return data.decode("utf-8")
    except UnicodeDecodeError:
        return None


def _contains_root(text: str, roots: list[str]) -> bool:
    return any(root in text for root in roots)


def _replace_roots(text: str, roots: list[str], replacement: str) -> str:
    for root in roots:
        text = text.replace(root, replacement)
    return text


def relocate_pkgconfig(text: str, roots: list[str], up: str) -> str:
    """Rewrite one .pc file (`up` is the relative path from its directory to the root)."""
    prefix_value = "${pcfiledir}/" + up if up else "${pcfiledir}"
    out: list[str] = []
    has_prefix = False
    for line in text.splitlines(keepends=True):
        if re.match(r"\s*prefix\s*=", line):
            has_prefix = True
            if _contains_root(line, roots):
                line = _replace_roots(line, roots, prefix_value)
        elif _contains_root(line, roots):
            line = _replace_roots(line, roots, "${prefix}")
        out.append(line)
    result = "".join(out)
    if not has_prefix and result != text:
        result = f"prefix={prefix_value}\n" + result
    return result


def _quote_state_before(line: str, index: int) -> str:
    """'single', 'double' or '' (unquoted) at `index` in a shell line."""
    state = ""
    escaped = False
    for char in line[:index]:
        if escaped:
            escaped = False
        elif char == "\\" and state != "single":
            escaped = True
        elif char == "'" and state != "double":
            state = "" if state == "single" else "single"
        elif char == '"' and state != "single":
            state = "" if state == "double" else "double"
    return state


def _replace_in_shell_line(line: str, roots: list[str]) -> str:
    for root in roots:
        position = line.find(root)
        while position != -1:
            state = _quote_state_before(line, position)
            replacement = {"single": "'\"${prefix}\"'",
                           "double": "${prefix}"}.get(state, "\"${prefix}\"")
            line = line[:position] + replacement + line[position + len(root):]
            position = line.find(root, position + len(replacement))
    return line


def relocate_shell_script(text: str, roots: list[str], up: str) -> str:
    """Rewrite a `bin/*-config` style script so its prefix follows the script."""
    computed = ('prefix="$(cd "$(dirname "$0")/' + (up or ".") +
                '" >/dev/null 2>&1 && pwd)"')
    out: list[str] = []
    for line in text.splitlines(keepends=True):
        newline = "\n" if line.endswith("\n") else ""
        body = line[:-1] if newline else line
        if re.match(r"\s*prefix=", body) and _contains_root(body, roots):
            body = computed
        elif _contains_root(body, roots):
            body = _replace_in_shell_line(body, roots)
        out.append(body + newline)
    return "".join(out)


def _up_path(root: Path, path: Path) -> str:
    depth = len(path.relative_to(root).parts) - 1
    return "/".join([".."] * depth)


def relocate_text_prefixes(root: Path, old_roots: list[Path | str]) -> list[Path]:
    """Relocate staged-path text files under `root`; return files left untouched.

    The return value lists text files that still contain an old root after the
    rewrite (an unknown file type), so the caller/verifier can surface them.
    """
    roots = root_spellings(*old_roots)
    unhandled: list[Path] = []
    for path in sorted(root.rglob("*")):
        if path.is_symlink() or not path.is_file():
            continue
        if path.suffix == ".la":
            text = _read_text(path)
            if text is not None and "libtool library" in text:
                path.unlink()
                continue
        text = _read_text(path)
        if text is None or not _contains_root(text, roots):
            continue
        relative = path.relative_to(root)
        if path.suffix == ".pc":
            rewritten = relocate_pkgconfig(text, roots, _up_path(root, path))
        elif relative.parts[0] == "bin" and text.startswith("#!"):
            rewritten = relocate_shell_script(text, roots, _up_path(root, path))
        else:
            unhandled.append(path)
            continue
        if _contains_root(rewritten, roots):
            unhandled.append(path)
            continue
        mode = path.stat().st_mode
        path.write_text(rewritten, encoding="utf-8", newline="")
        path.chmod(mode)
    return unhandled
