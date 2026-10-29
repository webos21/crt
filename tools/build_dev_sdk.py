#!/usr/bin/env python3
"""Assemble 02-cxx + networking/JSON/SHA-2 utilities; never compile upper stages."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

import build_stage_04_gfx_media as media
from build_stage_05_ui import runtime_env
import crt_dev_profile as profile
from crt_web_stage import publish, run
from create_dist import copy_porting_sdk

ROOT = Path(__file__).resolve().parent.parent


def recipe_build_identity(path):
    value = json.loads(path.read_text(encoding="utf-8"))
    # Replay notes/status do not change the pinned source or build contract.
    return {key: item for key, item in value.items() if key not in ("notes", "status")}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--work-root", required=True, type=Path)
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--packaging-work", type=Path,
                        help="retry metadata only in a completed work directory; runtime and port recipes must match")
    args = parser.parse_args(argv)
    compile_identity = {
        "source_commit": subprocess.check_output(["git", "-C", str(ROOT), "rev-parse", "HEAD"], text=True).strip(),
        "working_tree_dirty": bool(subprocess.check_output(["git", "-C", str(ROOT), "status", "--porcelain"], text=True).strip())}
    base, output, work = args.sdk_root.resolve(), args.output.resolve(), args.work_root.resolve()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    for left, right in ((base, output), (base, work), (work, output)):
        if left == right or left in right.parents or right in left.parents:
            raise ValueError("Development SDK predecessor, output and work-root must be disjoint")
    manifest = json.loads((base / "manifest.json").read_text())
    if manifest.get("stage") != "02-cxx" or (base / profile.SIDECAR).exists():
        raise ValueError("Development SDK requires a plain 02-cxx predecessor")
    inherited = profile.runtime_digests(base)
    if not inherited:
        raise ValueError("02-cxx predecessor contains no runtime payload")
    work.mkdir(parents=True, exist_ok=True)
    if args.packaging_work:
        temp = args.packaging_work.resolve()
        if not temp.is_relative_to(work) or temp == work:
            raise ValueError("packaging-work must be a completed child of work-root")
        staged = temp / "dev"
        previous = json.loads((staged / profile.SIDECAR).read_text())
        compile_identity = previous.get("compile_provenance", {
            "source_commit": previous.get("producer_source_commit"),
            "working_tree_dirty": previous.get("producer_working_tree_dirty")})
        if (previous.get("base_manifest_sha256") != profile.digest(base / "manifest.json")
                or previous.get("runtime_sha256") != inherited
                or profile.runtime_digests(staged) != inherited):
            raise ValueError("packaging retry predecessor/runtime changed; choose a fresh build")
        for name in profile.PORTS | profile.HEADER_PORTS.keys():
            recorded = staged / "share/crt/dependencies" / name / "recipe.json"
            if recipe_build_identity(recorded) != recipe_build_identity(ROOT / "porting/recipes" / (name + ".json")):
                raise ValueError("packaging retry port recipe changed; choose a fresh build")
        if not (temp / "port-build-curl/stamps/curl.installed").is_file():
            raise ValueError("packaging retry requires a completed curl build")
        copy_porting_sdk(ROOT, staged)
        sources = temp / "port-sources"
        print("Packaging-only retry: retained compiled ports, unchanged predecessor and recipes", flush=True)
    else:
        temp = Path(tempfile.mkdtemp(prefix="dev-", dir=work))
        staged = temp / "dev"
        shutil.copytree(base, staged, symlinks=True)
        copy_porting_sdk(ROOT, staged)
        env = runtime_env(staged, manifest)
        sources = media.fetch_port_sources(ROOT, staged, temp, env, ports=("curl", "cjson", "nlohmann-json"), download_cache=work / "downloads")
        media.build_curl_ports(ROOT, sources, staged, temp, manifest, env, args.jobs)
        run([sys.executable, staged / "tools/crt-port-build.py", "--sdk-root", staged,
             "--target-os", manifest["target"]["os"], "--target-arch", manifest["target"]["arch"],
             "--source-root", sources, "--work-root", temp / "port-build-utilities",
             "--install-prefix", staged, "--jobs", str(args.jobs), "--port", "cjson", "--port", "nlohmann-json", "--test"], env=env, cwd=ROOT)
        run([sys.executable, staged / "tools/crt-port-build.py", "--sdk-root", staged,
             "--target-os", manifest["target"]["os"], "--target-arch", manifest["target"]["arch"],
             "--source-root", sources, "--work-root", temp / "port-build-curl",
             "--install-prefix", staged, "--port", "mbedtls", "--test"], env=env, cwd=ROOT)
    dependencies = media.network_dependency_records(staged, ROOT, sources)
    for name, notice, headers, links, patterns in (
            ("cjson", "LICENSE", ["include/cJSON.h"], ["lib/libcjson.a"],
             ("lib/libcjson*.so*", "lib/libcjson*.dylib", "lib/cjson.dll")),
            ("nlohmann-json", "LICENSE.MIT", ["include/nlohmann"], [], ())):
        source = next(sources.glob(name + "-*"))
        entry = media.copy_dependency_record(staged, name, ROOT / "porting/recipes" / (name + ".json"),
                                            source / notice, headers, links, media.existing_relative_files(staged, patterns))
        entry["kind"] = "public-port" if links else "header-only-port"
        dependencies.append(entry)
    mbed = next(entry for entry in dependencies if entry["name"] == "mbedtls")
    notice = "share/licenses/mbedtls/CRT-LICENSE-CHOICE.txt"
    (staged / notice).write_text(media.MBEDTLS_CHOICE, encoding="utf-8")
    mbed["notices"].append(notice)
    manifest["redistributed_dependencies"] = [entry for entry in manifest.get("redistributed_dependencies", [])
                                              if entry.get("name") not in profile.PORTS | profile.HEADER_PORTS.keys()] + dependencies
    if profile.runtime_digests(staged) != inherited:
        raise ValueError("port builds changed inherited runtime bytes")
    # The producer commit is distinct from the predecessor's byte identity.
    # Do not claim an unknown old SDK was built from the current commit.
    commit = subprocess.check_output(["git", "-C", str(ROOT), "rev-parse", "HEAD"], text=True).strip()
    dirty = bool(subprocess.check_output(["git", "-C", str(ROOT), "status", "--porcelain"], text=True).strip())
    data = {"format": 1, "profile": "dev", "capabilities": profile.CAPABILITIES, "target": manifest["target"],
            "version": (base / "VERSION").read_text().strip(), "producer_source_commit": commit,
            "producer_working_tree_dirty": dirty, "base_source_commit": manifest.get("source_commit"),
            "compile_provenance": compile_identity,
            "base_manifest_sha256": profile.digest(base / "manifest.json"),
            "runtime_sha256": inherited, "tls_trust": {"mode": "explicit-ca-file", "bundled": False}}
    data["consumer_compile_options"] = profile.consumer_options(manifest["target"]["os"])
    sidecar = staged / profile.SIDECAR
    sidecar.parent.mkdir(parents=True, exist_ok=True)
    sidecar.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    sbom = {"spdxVersion": "SPDX-2.3", "dataLicense": "CC0-1.0", "SPDXID": "SPDXRef-DOCUMENT",
            "name": "CRT Development SDK", "documentNamespace": "https://github.com/webos21/crt/dev/" + profile.digest(sidecar),
            "creationInfo": {"creators": ["Tool: CRT Development SDK producer"],
                             "created": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")},
            "packages": [{"name": entry["name"], "SPDXID": "SPDXRef-" + entry["name"],
                          "downloadLocation": "NOASSERTION", "filesAnalyzed": False,
                          "licenseConcluded": "NOASSERTION", "licenseDeclared": "NOASSERTION",
                          "copyrightText": "NOASSERTION"}
                         for entry in manifest["redistributed_dependencies"]]}
    (staged / "share/crt/development-sbom.spdx.json").write_text(json.dumps(sbom, indent=2) + "\n", encoding="utf-8")
    (staged / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    if manifest["target"]["os"] == "linux":
        media.remove_staged_absolute_elf_rpaths(staged)
    elif manifest["target"]["os"] == "macos":
        media.portable_macho_dependency_paths(staged)
    remaining = media.relocate_text_prefixes(staged, [staged])
    if remaining:
        raise ValueError("unrelocated Development SDK text: " + ", ".join(map(str, remaining)))
    run([sys.executable, ROOT / "tools/verify_dist.py", "--dist", staged, "--stage", "02-cxx", "--profile", "dev"])
    publish(staged, output)
    print(f"Development SDK ready (not a published release): {output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
