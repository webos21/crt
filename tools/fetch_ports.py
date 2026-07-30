#!/usr/bin/env python3
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
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
