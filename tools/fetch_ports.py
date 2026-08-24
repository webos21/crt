#!/usr/bin/env python3
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import tarfile
import urllib.request
import zipfile


def load_recipes(recipe_dir):
    recipes = {}
    for path in sorted(Path(recipe_dir).glob("*.json")):
        with open(path, "r", encoding="utf-8") as f:
            recipe = json.load(f)
        recipes[recipe["name"]] = recipe

    return recipes


def resolve_recipe_order(recipes, selected):
    ordered = []
    visiting = set()
    visited = set()

    def visit(name):
        if name in visited:
            return
        if name in visiting:
            raise SystemExit(f"dependency cycle includes {name}")
        if name not in recipes:
            raise SystemExit(f"recipe not found: {name}")

        visiting.add(name)
        for dep in recipes[name].get("dependencies", []):
            visit(dep)
        visiting.remove(name)
        visited.add(name)
        ordered.append(recipes[name])

    names = selected or sorted(recipes)
    for name in names:
        visit(name)
    return ordered


def list_recipes(recipes):
    print("name\tversion\tbuild\tautomated\tdependencies\tlinux\tmacos\twindows")
    for name in sorted(recipes):
        recipe = recipes[name]
        build = recipe["build"]
        status = recipe.get("status", {})
        deps = ",".join(recipe.get("dependencies", [])) or "-"
        automated = str(build.get("automated", True)).lower()
        print(
            "\t".join(
                [
                    recipe["name"],
                    recipe["version"],
                    build["system"],
                    automated,
                    deps,
                    status.get("linux", "-"),
                    status.get("macos", "-"),
                    status.get("windows", "-"),
                ]
            )
        )


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def download(url, archive_path):
    if os.path.exists(archive_path):
        return
    tmp_path = archive_path + ".tmp"
    print(f"fetch {url}")
    with urllib.request.urlopen(url) as response, open(tmp_path, "wb") as out:
        shutil.copyfileobj(response, out)
    os.replace(tmp_path, archive_path)


def fix_windows_symlink_targets(root):
    """tarfile.extractall() recreates a POSIX tar's symlink members via
    os.symlink(member.linkname, targetpath) -- passing the raw, forward-
    slash-separated linkname straight through, unmodified. Confirmed for
    real (2026-08-24, building porting/recipes/make.json's own "make"
    dependency from a genuinely from-scratch Windows build directory for
    the first time in this project's history): a *relative* symlink whose
    target crosses a directory via ".." (e.g. Android toolchain/make's own
    build-aux/config.guess -> ../gnulib/build-aux/config.guess) resolves
    fine through os.readlink() and MSYS2/Git-Bash's own `ls`/`readlink`
    (which parse the raw reparse-point print name directly), but fails
    os.stat()/open() -- i.e. every real Win32 CreateFile-based API,
    including copy_source()'s own shutil.copytree() a moment later --
    with `OSError: [WinError 123] The filename, directory name, or volume
    label syntax is incorrect`. Reproduced directly and in isolation: the
    identical relative target recreated with backslash separators
    (`..\\gnulib\\build-aux\\config.guess`) resolves and stats correctly,
    proving this is purely a slash-direction issue in how Windows resolves
    a relative symlink target, not a missing-file or permissions problem
    (Developer Mode was already enabled and os.symlink() itself succeeded
    for every one of these entries -- os.path.islink() is True, os.readlink()
    returns the right string, only the *resolution* through a real file API
    fails). Fixed generally here (not per-recipe) since any future tar-
    sourced recipe with relative symlinks in its archive would hit the same
    bug on Windows the first time crt-port-build.py's own copy_source()
    tries to read through one.

    Windows-only and only relevant to the tarfile path above: zipfile's
    own extractall() never recreates real symlinks at all (a zip's symlink
    entries are Unix-specific external-attribute metadata the stdlib zip
    reader does not act on), so there is nothing to fix for zip archives.
    """
    if os.name != "nt":
        return
    for dirpath, dirnames, filenames in os.walk(root):
        for name in dirnames + filenames:
            path = os.path.join(dirpath, name)
            if not os.path.islink(path):
                continue
            target = os.readlink(path)
            fixed_target = target.replace("/", "\\")
            if fixed_target == target:
                continue
            # Determine the reparse point's own directory-vs-file symlink
            # flag via lstat() (operates on the link itself, no target
            # resolution needed) rather than os.path.isdir(path) (follows
            # the link -- and the whole reason this function exists is
            # that following the link is exactly what fails right now).
            is_dir_target = bool(os.lstat(path).st_file_attributes & stat.FILE_ATTRIBUTE_DIRECTORY)
            if is_dir_target:
                os.rmdir(path)
            else:
                os.remove(path)
            os.symlink(fixed_target, path, target_is_directory=is_dir_target)


def extract(archive_path, dest):
    if os.path.exists(dest):
        return
    tmp_dest = dest + ".tmp"
    if os.path.exists(tmp_dest):
        shutil.rmtree(tmp_dest)
    os.makedirs(tmp_dest, exist_ok=True)
    if zipfile.is_zipfile(archive_path):
        with zipfile.ZipFile(archive_path) as zf:
            zf.extractall(tmp_dest)
    else:
        with tarfile.open(archive_path) as tf:
            tf.extractall(tmp_dest)
        fix_windows_symlink_targets(tmp_dest)
    entries = [os.path.join(tmp_dest, entry) for entry in os.listdir(tmp_dest)]
    dirs = [entry for entry in entries if os.path.isdir(entry)]
    if len(dirs) == 1 and len(entries) == 1:
        os.replace(dirs[0], dest)
        os.rmdir(tmp_dest)
    else:
        os.replace(tmp_dest, dest)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dest", default=None, help="destination source root")
    parser.add_argument("--cache", default=None, help="archive cache directory")
    parser.add_argument("--recipe-dir", default="porting/recipes", help="directory containing porting recipes")
    parser.add_argument("--port", action="append", help="recipe name to fetch; defaults to all recipes")
    parser.add_argument("--list", action="store_true", help="list recipes and exit")
    args = parser.parse_args()

    recipes = load_recipes(args.recipe_dir)
    if args.list:
        list_recipes(recipes)
        return

    if not args.dest:
        raise SystemExit("--dest is required unless --list is used")

    dest = os.path.abspath(args.dest)
    cache = os.path.abspath(args.cache or os.path.join(dest, "..", "downloads"))
    selected_recipes = resolve_recipe_order(recipes, args.port)
    os.makedirs(dest, exist_ok=True)
    os.makedirs(cache, exist_ok=True)

    for recipe in selected_recipes:
        source = recipe["source"]
        archive_path = os.path.join(cache, source["archive"])
        download(source["url"], archive_path)
        actual = sha256_file(archive_path)
        expected = source["sha256"]
        if expected is not None and actual != expected:
            raise SystemExit(
                f"{source['archive']}: sha256 mismatch\n"
                f"expected {expected}\n"
                f"actual   {actual}"
            )
        source_dir = os.path.join(dest, source["source_dir"])
        extract(archive_path, source_dir)
        print(f"{recipe['name']} {recipe['version']}: {source_dir}")


if __name__ == "__main__":
    main()
