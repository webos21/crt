#!/usr/bin/env python3
import argparse
import hashlib
import os
import shutil
import tarfile
import urllib.request
import zipfile


PORTS = [
    {
        "name": "zlib",
        "version": "1.3.1",
        "url": "https://zlib.net/fossils/zlib-1.3.1.tar.gz",
        "archive": "zlib-1.3.1.tar.gz",
        "sha256": "9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23",
    },
    {
        "name": "libpng",
        "version": "1.6.57",
        "url": "https://download.sourceforge.net/libpng/libpng-1.6.57.tar.xz",
        "archive": "libpng-1.6.57.tar.xz",
        "sha256": None,
    },
    {
        "name": "sqlite-amalgamation",
        "version": "3.53.4",
        "url": "https://www.sqlite.org/2026/sqlite-amalgamation-3530400.zip",
        "archive": "sqlite-amalgamation-3530400.zip",
        "sha256": None,
    },
    {
        "name": "libffi",
        "version": "3.4.5",
        "url": "https://github.com/libffi/libffi/releases/download/v3.4.5/libffi-3.4.5.tar.gz",
        "archive": "libffi-3.4.5.tar.gz",
        "sha256": "96fff4e589e3b239d888d9aa44b3ff30693c2ba1617f953925a70ddebcc102b2",
    },
]


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
    parser.add_argument("--dest", required=True, help="destination source root")
    parser.add_argument("--cache", default=None, help="archive cache directory")
    args = parser.parse_args()

    dest = os.path.abspath(args.dest)
    cache = os.path.abspath(args.cache or os.path.join(dest, "..", "downloads"))
    os.makedirs(dest, exist_ok=True)
    os.makedirs(cache, exist_ok=True)

    for port in PORTS:
        archive_path = os.path.join(cache, port["archive"])
        download(port["url"], archive_path)
        actual = sha256_file(archive_path)
        expected = port["sha256"]
        if expected is not None and actual != expected:
            raise SystemExit(
                f"{port['archive']}: sha256 mismatch\n"
                f"expected {expected}\n"
                f"actual   {actual}"
            )
        source_dir = os.path.join(dest, f"{port['name']}-{port['version']}")
        extract(archive_path, source_dir)
        print(f"{port['name']} {port['version']}: {source_dir}")


if __name__ == "__main__":
    main()
