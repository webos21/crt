"""Validate pinned runtime caches and produce canonical source-stage inputs.

Git objects supply the upstream truth, never a previously patched worktree.
Only pristine or exactly recipe-patched file bytes are accepted. Packaging
always emits the patched form, independent of a cache's configure history.
"""

import hashlib
import json
import subprocess
from pathlib import Path


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def validate_patch_inputs(recipe, checkout):
    for relative, checks in recipe.get("source_checks", {}).items():
        target = checkout / relative
        if target.is_symlink() or not target.is_file() or sha256(target.read_bytes()) not in checks.values():
            raise SystemExit(f"{recipe['name']}: noncanonical source cache: {relative}; "
                             "preserve the cache and fetch pristine pinned sources in a new directory")


def patch_text(recipe, relative, text):
    for patch in recipe.get("patches", []):
        if patch["file"] != relative:
            continue
        find, replacement = patch["find"], patch["replace"]
        if replacement and replacement in text:
            continue
        if find not in text:
            if not replacement:
                continue
            raise SystemExit(f"{recipe['name']}: patch target text not found in {relative}")
        text = text.replace(find, replacement, 1)
    return text


def apply_patches(recipe, checkout):
    # Validate all guarded files before writing anything, including extra files.
    validate_patch_inputs(recipe, checkout)
    outputs = {}
    for relative in dict.fromkeys(patch["file"] for patch in recipe.get("patches", [])):
        target = checkout / relative
        original = target.read_text(encoding="utf-8")
        patched = patch_text(recipe, relative, original)
        if patched != original:
            outputs[relative] = patched
    for extra in recipe.get("extra_files", []):
        target = checkout / extra["path"]
        if target.exists() and target.read_text(encoding="utf-8") != extra["content"]:
            raise SystemExit(f"{recipe['name']}: noncanonical extra file: {extra['path']}")
        if not target.exists():
            outputs[extra["path"]] = extra["content"]
    for relative, text in outputs.items():
        target = checkout / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text, encoding="utf-8", newline="\n")
    validate_patch_inputs(recipe, checkout)


def canonical_runtime_sources(root, source_root, target_os):
    """Return archive overrides and a host-independent provenance receipt.

    Every bundled upstream file is compared to the pinned Git object or its
    approved patch result; unknown files and partial/drifted patches fail.
    No cache files are rewritten. Git caches must contain the requested pin.
    """
    originals, canonical, modes, records = {}, {}, {}, []
    for recipe_path in sorted((root / "libstdc++/third_party").glob("*/recipe.json")):
        recipe = json.loads(recipe_path.read_text(encoding="utf-8"))
        if target_os not in recipe.get("target_os", [target_os]):
            continue
        source = recipe["source"]
        ref = source["ref"]
        clone = source_root / (".clone-" + recipe["name"])
        subdir = source["checkout_subdir"]
        paths = [subdir, *source.get("extra_checkout_dirs", [])]
        listing = subprocess.check_output(["git", "ls-tree", "-rz", ref, "--", *paths], cwd=clone)
        entries = []
        for entry in listing.split(b'\0'):
            if not entry:
                continue
            header, name = entry.split(b'\t', 1)
            mode, kind, oid = header.split()
            if kind != b'blob' or mode not in (b'100644', b'100755'):
                raise SystemExit(f"runtime source pin contains unsupported member: {name!r}")
            entries.append((name.decode('utf-8'), oid, 0o755 if mode == b'100755' else 0o644))
        # Read exactly the selected blob IDs. git archive may additionally fetch
        # attributes outside sparse paths, which are neither bundled nor inputs.
        payload = subprocess.check_output(["git", "cat-file", "--batch"],
                                          input=b''.join(oid + b'\n' for _, oid, _ in entries), cwd=clone)
        recipe_originals = {}
        offset = 0
        for name, oid, mode in entries:
            end = payload.index(b'\n', offset)
            header = payload[offset:end].split()
            if len(header) != 3 or header[:2] != [oid, b'blob']:
                raise SystemExit(f"cannot read pinned runtime blob: {name}")
            size = int(header[2])
            data = payload[end + 1:end + 1 + size]
            offset = end + size + 2
            if name.startswith(subdir + "/"):
                relative = recipe["name"] + name[len(subdir):]
            elif any(name.startswith(extra + "/") for extra in paths[1:]):
                relative = name
            else:
                continue
            recipe_originals[relative] = data
            modes[relative] = mode
        recipe_canonical = dict(recipe_originals)
        for relative in dict.fromkeys(p["file"] for p in recipe.get("patches", [])):
            name = recipe["name"] + "/" + relative
            recipe_canonical[name] = patch_text(recipe, relative, recipe_originals[name].decode("utf-8")).encode("utf-8")
        for extra in recipe.get("extra_files", []):
            name = recipe["name"] + "/" + extra["path"]
            recipe_canonical[name] = extra["content"].encode("utf-8")
            modes[name] = 0o644
        for name, data in recipe_canonical.items():
            if name in canonical and canonical[name] != data:
                raise SystemExit(f"conflicting runtime source pins: {name}")
            canonical[name] = data
        originals.update(recipe_originals)
        records.append({"recipe": recipe["name"], "repository": source["repository"], "commit": ref,
                        "recipe_sha256": sha256(recipe_path.read_bytes())})

    roots = {name.split("/")[0] for name in canonical}
    actual = {}
    for name in sorted(roots):
        for path in (source_root / name).rglob("*"):
            if any(part in (".git", "__pycache__") for part in path.relative_to(source_root).parts):
                continue
            if path.is_symlink():
                raise SystemExit(f"noncanonical runtime source symlink: {path}")
            if path.is_file():
                actual[path.relative_to(source_root).as_posix()] = path
    missing = set(originals) - set(actual)
    unexpected = set(actual) - set(canonical)
    if missing or unexpected:
        raise SystemExit(f"noncanonical runtime source inventory: missing={sorted(missing)[:5]}, "
                         f"unexpected={sorted(unexpected)[:5]}")
    for name, path in actual.items():
        data = path.read_bytes()
        if data != canonical[name] and data != originals.get(name):
            raise SystemExit(f"noncanonical runtime source cache: {name}; regenerate from pinned Git objects")
    inventory = [{"path": name, "mode": modes[name], "size": len(data), "sha256": sha256(data)}
                 for name, data in sorted(canonical.items())]
    return canonical, {"format": 1, "policy": "pinned-git-plus-recipe-patches", "recipes": records,
                       "files": inventory}
