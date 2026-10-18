#!/usr/bin/env python3
"""Fail-closed WPE advisory snapshot for a pinned WebKit development SDK.

This is a version/advisory check, not a vulnerability scanner or proof of a
sandbox. Public release additionally requires the recorded support-window and
native/cross-host packaging gates; a successful snapshot alone cannot grant it.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import hashlib
from html.parser import HTMLParser
import json
from pathlib import Path
import re
import urllib.request

FEED = "https://wpewebkit.org/security/"
RELEASES = "https://wpewebkit.org/releases/"


class Page(HTMLParser):
    def __init__(self):
        super().__init__()
        self.text = []
        self.links = []

    def handle_data(self, data):
        self.text.append(data)

    def handle_starttag(self, tag, attrs):
        if tag == "a":
            self.links.extend(value for key, value in attrs if key == "href")


def version(value):
    if not re.fullmatch(r"\d+\.\d+\.\d+", value):
        raise ValueError(f"unrecognized WebKit version: {value!r}")
    return tuple(map(int, value.split(".")))


def advisory_ids(html):
    page = Page()
    page.feed(html)
    ids = sorted(set(re.findall(r"WSA-\d{4}-\d{4}", " ".join(page.links))), reverse=True)
    if not ids:
        raise ValueError("advisory index contains no WSA links")
    return ids


def evaluate(html, pinned):
    page = Page()
    page.feed(html)
    text = " ".join(" ".join(page.text).split())
    clauses = re.findall(r"Versions affected:\s*(.*?)(?=Versions affected:|$)", text)
    if not clauses:
        raise ValueError("advisory has no affected-version clauses")
    bounds = []
    for clause in clauses:
        # Only the first sentence is the version declaration. Never infer that
        # an unknown range, <=, 'through', or missing bound means unaffected.
        declaration = re.split(r"\.\s", clause, maxsplit=1)[0]
        if re.fullmatch(r"WebKitGTK before \d+\.\d+\.\d+\.?", declaration):
            continue  # explicitly GTK-only, not WPE
        match = re.fullmatch(r"WebKitGTK (?:and |before \d+\.\d+\.\d+ and )WPE WebKit before (\d+\.\d+\.\d+)(?: or earlier)?(?: and \d+\.\d+ branch before \d+\.\d+\.\d+)*(?: if USE_LIBWEBRTC is enabled)?\.?",
                             declaration)
        if not match:
            raise ValueError(f"unsupported advisory range: {declaration!r}")
        bounds.extend(re.findall(r"before (\d+\.\d+\.\d+)", declaration.split("WPE WebKit", 1)[1]))
    return {"fixed_versions": sorted(set(bounds), key=version),
            "affected": any(version(pinned) < version(bound) for bound in bounds),
            "cves": sorted(set(re.findall(r"CVE-\d{4}-\d{4,}", text)))}


def fetch(url):
    with urllib.request.urlopen(url, timeout=60) as response:
        if not response.geturl().startswith((FEED, RELEASES)):
            raise ValueError("security fetch redirected outside official HTTPS origin")
        return response.read().decode("utf-8")


def snapshot(pinned):
    index = fetch(FEED)
    releases = fetch(RELEASES)
    versions = set(re.findall(r'wpewebkit-(\d+\.\d+\.\d+)\.tar\.xz["<]', releases))
    series = [value for value in versions if version(value)[:2] == version(pinned)[:2]]
    if not series:
        raise ValueError("release index contains no archive for the pinned stable series")
    latest_micro = max(series, key=version)
    # Every advisory linked from the first index page is checked, not merely
    # the newest: an older advisory may have a higher fixed-version bound.
    def check(identifier):
        url = FEED + identifier + ".html"
        html = fetch(url)
        return {"id": identifier, "url": url,
                        "sha256": hashlib.sha256(html.encode()).hexdigest(),
                        **evaluate(html, pinned)}
    with ThreadPoolExecutor(max_workers=6) as executor:
        records = list(executor.map(check, advisory_ids(index)))
    return {"format": 1, "version": pinned, "feed": FEED,
            "checked_utc": datetime.now(timezone.utc).isoformat(),
            "index_sha256": hashlib.sha256(index.encode()).hexdigest(),
            "release_index_sha256": hashlib.sha256(releases.encode()).hexdigest(),
            "latest_micro": latest_micro, "pin_is_latest_micro": pinned == latest_micro,
            "latest_advisory": records[0]["id"], "advisories": records,
            "passed": not any(record["affected"] for record in records),
            "scope": "version bounds on current advisory index; not a security certification",
            "public_release_allowed": False,
            "release_blockers": ([f"stable-series pin update required: {pinned} -> {latest_micro}"]
                                 if pinned != latest_micro else []) +
                                ["upstream support window unconfirmed",
                                 "native three-host and cross-target packaging acceptance required"]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recipe", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = snapshot(json.loads(args.recipe.read_text())["version"])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(f"WebKit {result['version']}: advisory gate {'pass' if result['passed'] else 'FAIL'}; "
          f"latest={result['latest_advisory']} (development candidate only)")
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
