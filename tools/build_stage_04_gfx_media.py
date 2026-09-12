#!/usr/bin/env python3
"""Build an option-ON cumulative 04-gfx-media SDK from 03-gfx-simple."""

import argparse
from contextlib import contextmanager
import hashlib
import json
import os
import shutil
import stat
import subprocess
import sys
import tempfile
import time
from pathlib import Path


DEFAULT_DEPENDENCY_JOBS = max(1, min(os.cpu_count() or 2, 4))


def run(command: list[str], env: dict[str, str] | None = None,
        cwd: Path | None = None) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, check=True, env=env, cwd=cwd)


class PhaseTimings:
    """Print and retain coarse stage timings without changing build behavior."""

    def __init__(self) -> None:
        self.started = time.monotonic()
        self.records: list[tuple[str, float, str]] = []

    @contextmanager
    def measure(self, name: str):
        print(f"==> {name}", flush=True)
        started = time.monotonic()
        status = "ok"
        try:
            yield
        except BaseException:
            status = "failed"
            raise
        finally:
            elapsed = time.monotonic() - started
            self.records.append((name, elapsed, status))
            print(f"<== {name}: {status} ({elapsed:.1f}s)", flush=True)

    def report(self) -> None:
        if not self.records:
            return
        print("Stage phase timing summary:", flush=True)
        for name, elapsed, status in self.records:
            print(f"  {elapsed:9.1f}s  {status:6s}  {name}", flush=True)
        print(f"  {time.monotonic() - self.started:9.1f}s  total", flush=True)


def cmake_path(path: Path) -> str:
    return path.resolve().as_posix()


# Mach-O magic numbers (32/64-bit, either endianness) and the fat
# (universal) binary magic -- checked against each file's own first four
# bytes so rewrite_stale_macho_rpaths() below can skip straight past the
# thousands of plain-text files in a real staged tree (Skia's own
# installed include/ alone) without paying for a real `otool` invocation
# on each one.
_MACHO_MAGICS = frozenset((
    b"\xfe\xed\xfa\xce", b"\xce\xfa\xed\xfe",
    b"\xfe\xed\xfa\xcf", b"\xcf\xfa\xed\xfe",
    b"\xca\xfe\xba\xbe", b"\xbe\xba\xfe\xca",
))


def rewrite_stale_macho_rpaths(root: Path, old_root: Path, new_root: Path) -> None:
    """install_name_tool -rpath's every Mach-O file under root whose own
    LC_RPATH still points somewhere under old_root, rewriting it to the
    equivalent path under new_root.

    macOS-only, and needed for real: this stage is built entirely inside a
    throwaway tempfile.mkdtemp() (old_root, see main()'s own temp_root),
    then copied to the real, requested --output (new_root) only once
    finished. The predecessor SDK's own crt-toolchain.cmake computes
    CRT_DISTRIBUTION_ROOT as `get_filename_component(... "${CMAKE_CURRENT_
    LIST_DIR}" ABSOLUTE)` -- deliberately self-relocating ("wherever this
    SDK happens to be unpacked", by design, for the normal "download once,
    build in place" case) -- and both CMAKE_BUILD_RPATH/CMAKE_INSTALL_
    RPATH and tools/crt-c++'s own `-Wl,-rpath,${CRT_SYSROOT}/lib` derive
    from it. So every RPATH-bearing binary built while old_root was still
    "where this SDK lives" gets old_root's own (about-to-be-deleted) path
    baked in, not new_root's -- a real, confirmed bug (2026-09-11): a
    published crtgfx_skia_gpu_window_demo failed outright with "Library
    not loaded: @rpath/libcrtgfx_skia.dylib ... tried:
    '<old_root>/lib/libcrtgfx_skia.dylib' (no such file)" once old_root
    (already deleted by main()'s own `finally: shutil.rmtree(temp_root)`)
    was gone. Note DYLD_LIBRARY_PATH is NOT a workaround here -- pointing
    it at the real lib/ "fixes" that error but immediately reintroduces
    the leaf-name dyld-hijack bug crt_stage_register_test() itself was
    fixed for (distribution/stages/04-gfx-media/CMakeLists.txt's own
    2026-09-11 entry): real Apple frameworks' own libc++.1.dylib gets
    silently swapped for this project's ABI-incompatible one.
    dyld's own @rpath resolution consults every already-loaded image's own
    RPATH entries, so fixing only the top-level executable would usually
    be enough in practice -- every Mach-O file in the tree is rewritten
    here anyway regardless, since it costs little and does not depend on
    that aggregation behavior for a dylib some future consumer might
    dlopen() directly."""
    # Two candidate spellings of old_root, both checked below: ld64 bakes
    # the *real*, symlink-resolved path (/private/var/folders/... on
    # macOS) into some LC_RPATH commands (e.g. CMake's own automatic
    # RPATH) but the raw, unresolved tempfile.mkdtemp() spelling (/var/
    # folders/...) into others (tools/crt-c++'s own -Wl,-rpath uses
    # $CRT_SYSROOT verbatim) -- confirmed for real, 2026-09-11: matching
    # only one form left the other spelling's own stale entry (a second,
    # separate LC_RPATH command in the same file) silently unrewritten,
    # dangling at an already-deleted temp path forever, even though the
    # file also carried a second, correctly-rewritten entry that happened
    # to make the binary still run.
    old_strs = {str(old_root), str(old_root.resolve())}
    new_str = str(new_root.resolve())
    for path in root.rglob("*"):
        if path.is_symlink() or not path.is_file():
            continue
        try:
            with path.open("rb") as handle:
                magic = handle.read(4)
        except OSError:
            continue
        if magic not in _MACHO_MAGICS:
            continue
        listing = subprocess.run(
            ["otool", "-l", str(path)], capture_output=True, text=True, check=False)
        lines = listing.stdout.splitlines()
        existing_rpaths = set()
        for index, line in enumerate(lines):
            if line.strip() != "cmd LC_RPATH" or index + 2 >= len(lines):
                continue
            path_line = lines[index + 2].strip()
            if path_line.startswith("path "):
                existing_rpaths.add(path_line[len("path "):].rsplit(" (offset", 1)[0])
        for value in list(existing_rpaths):
            matched_old = next(
                (old_str for old_str in old_strs
                 if value == old_str or value.startswith(old_str + os.sep)),
                None)
            if matched_old is None:
                continue
            new_value = new_str + value[len(matched_old):]
            if new_value in existing_rpaths:
                # A second, differently-spelled RPATH entry for the exact
                # same real directory already exists in this file (e.g.
                # both the raw and the /private-resolved form of the same
                # TMPDIR path -- confirmed for real, 2026-09-11: dyld
                # tolerates two RPATH strings that happen to resolve to
                # one real directory, but rejects two byte-identical
                # LC_RPATH commands outright ("duplicate LC_RPATH") the
                # instant rewriting both stale spellings to the same new
                # path would have made them identical). Drop this one
                # instead of creating that duplicate; the other spelling
                # is either already correct or gets rewritten to the same
                # new_value by its own turn through this same loop.
                subprocess.run(
                    ["install_name_tool", "-delete_rpath", value, str(path)],
                    check=True)
            else:
                subprocess.run(
                    ["install_name_tool", "-rpath", value, new_value, str(path)],
                    check=True)
                existing_rpaths.add(new_value)


def publish_tree(staged: Path, output: Path) -> None:
    """Copy beside the destination, then expose the final name atomically."""
    publish_root = Path(tempfile.mkdtemp(
        prefix=f".{output.name}-publish-", dir=output.parent
    ))
    candidate = publish_root / output.name
    try:
        shutil.copytree(staged, candidate)
        if sys.platform == "darwin":
            # Rewrite while still under the hidden publish_root, targeting
            # the real final `output` path, so the atomic os.replace()
            # below still exposes either a fully-correct tree or nothing
            # -- never a half-patched one visible at the real output path.
            rewrite_stale_macho_rpaths(candidate, staged, output)
        os.replace(candidate, output)
    finally:
        remove_tree(publish_root, ignore_errors=True)


def touch_tree(root: Path) -> None:
    """Stamp every file under root with the current time.

    Real, confirmed bug (2026-09-12, --reuse-work-root's own first use):
    the extracted stage-source tarball's members (and this project's own
    shutil.copytree(asset, build_asset) copy of them, which preserves
    mtimes via copy2) all carry a normalized, reproducible epoch-0 mtime --
    deliberate, for reproducible packaging, but it defeats CMake's
    install(FILES ...) staleness check (a plain destination-mtime >=
    source-mtime comparison) whenever the freshly-copied predecessor already
    contains the older version of the same file at that exact epoch-0 mtime:
    CMake sees "not older" and skips re-copying the fixed file. Called on
    build_asset right after it exists, before anything reads or installs from
    it, so every file this run actually uses is unconditionally newer than
    the predecessor's installed copy.
    """
    for path in root.rglob("*"):
        if path.is_file():
            os.utime(path, None)  # None -- os.utime's own "stamp with now" form.


def fingerprint(paths: list[Path]) -> str:
    """Return a stable content hash for an ordered collection of files."""
    digest = hashlib.sha256()
    for index, path in enumerate(paths):
        # Cache identity follows content, not the temporary directory where a
        # source asset happened to be extracted for this invocation.
        digest.update(str(index).encode("ascii"))
        digest.update(b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()[:32]


def file_digest(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def path_identity(path: Path) -> str:
    if path.is_symlink():
        return "symlink:" + os.readlink(path)
    if path.is_file():
        return "file:" + file_digest(path)
    raise SystemExit(f"stage cache entry is not a file or symlink: {path}")


def tree_inventory(root: Path, normalize_manifest: bool = False) -> dict[str, str]:
    """Describe every file/symlink by relative path and stable content.

    The predecessor manifest's creation time records when an archive was
    packaged, not an SDK input that can affect dependency binaries. Excluding
    only that field prevents timestamp-only repacks from discarding a valid
    multi-hour cache while all actual SDK files remain covered.
    """
    inventory: dict[str, str] = {}
    for path in sorted(root.rglob("*"), key=lambda item: item.as_posix()):
        relative = path.relative_to(root).as_posix()
        if path.is_symlink():
            inventory[relative] = path_identity(path)
        elif path.is_file():
            if normalize_manifest and relative == "manifest.json":
                manifest = json.loads(path.read_text(encoding="utf-8"))
                manifest.pop("created_utc", None)
                content = json.dumps(
                    manifest, sort_keys=True, separators=(",", ":")
                ).encode("utf-8")
                inventory[relative] = (
                    "file:" + hashlib.sha256(content).hexdigest())
            else:
                inventory[relative] = path_identity(path)
    return inventory


def inventory_fingerprint(inventory: dict[str, str]) -> str:
    digest = hashlib.sha256()
    for relative, identity in sorted(inventory.items()):
        digest.update(relative.encode("utf-8"))
        digest.update(b"\0")
        digest.update(identity.encode("utf-8"))
        digest.update(b"\0")
    return digest.hexdigest()[:32]


def toolchain_identity(target_os: str) -> str:
    """Fingerprint the external compiler tools that produce cached objects."""
    defaults = {
        "CRT_CC": "clang",
        "CRT_CXX": "clang++",
        "CRT_AR": "llvm-ar" if target_os == "windows" else "ar",
        "CRT_RANLIB": "llvm-ranlib" if target_os == "windows" else "ranlib",
    }
    digest = hashlib.sha256()
    digest.update(target_os.encode("utf-8"))
    digest.update(b"\0")
    if target_os == "windows":
        # The versioned SDK library directory is part of the effective linker
        # toolchain even though the CRT SDK deliberately does not redistribute
        # Microsoft's import libraries.
        digest.update(os.environ.get(
            "CRT_WINDOWS_SDK_LIBPATH", "<unset>").encode("utf-8"))
        digest.update(b"\0")
    for variable, default in defaults.items():
        configured = os.environ.get(variable, default)
        resolved = shutil.which(configured) or configured
        try:
            result = subprocess.run(
                [configured, "--version"], capture_output=True, text=True,
                check=False, timeout=30)
            version = result.stdout + result.stderr
        except (OSError, subprocess.TimeoutExpired) as exc:
            version = f"unavailable:{type(exc).__name__}:{exc}"
        digest.update(variable.encode("utf-8"))
        digest.update(b"\0")
        digest.update(str(Path(resolved).resolve()).encode("utf-8"))
        digest.update(b"\0")
        digest.update(version.encode("utf-8", errors="replace"))
        digest.update(b"\0")
    return digest.hexdigest()[:32]


def remove_readonly_and_retry(function, path: str, _exc_info) -> None:
    """Permit removal of read-only files left by Windows source checkouts."""
    os.chmod(path, stat.S_IWRITE)
    function(path)


def remove_tree(path: Path, ignore_errors: bool = False) -> None:
    shutil.rmtree(
        path, ignore_errors=ignore_errors,
        onerror=None if ignore_errors else remove_readonly_and_retry)


def remove_path(path: Path) -> None:
    if path.is_symlink() or path.is_file():
        path.unlink()
    elif path.is_dir():
        remove_tree(path)


def safe_layer_path(root: Path, relative: str) -> Path:
    path = Path(relative)
    if path.is_absolute() or not path.parts or ".." in path.parts:
        raise SystemExit(f"unsafe stage cache layer path: {relative!r}")
    candidate = root / path
    try:
        candidate.parent.resolve().relative_to(root.resolve())
    except ValueError as exc:
        raise SystemExit(
            f"stage cache layer path escapes through a symlink: {relative!r}") from exc
    return candidate


def copy_layer_entry(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists() or destination.is_symlink():
        remove_path(destination)
    if source.is_symlink():
        destination.symlink_to(
            os.readlink(source), target_is_directory=source.is_dir())
    else:
        shutil.copy2(source, destination)


def capture_install_layer(staged: Path, before: dict[str, str],
                          layer: Path) -> None:
    """Atomically snapshot only files changed by one dependency build tier."""
    after = tree_inventory(staged)
    changed = {
        relative: identity for relative, identity in sorted(after.items())
        if before.get(relative) != identity
    }
    removed = sorted(set(before) - set(after))
    candidate = layer.with_name(layer.name + ".new")
    if candidate.exists():
        remove_tree(candidate)
    candidate.mkdir(parents=True)
    for relative in changed:
        copy_layer_entry(
            safe_layer_path(staged, relative),
            safe_layer_path(candidate, relative))
    (candidate / ".layer.json").write_text(json.dumps({
        "format": 2,
        "changed": changed,
        "removed": removed,
    }, indent=2) + "\n", encoding="utf-8")
    if layer.exists():
        remove_tree(layer)
    os.replace(candidate, layer)


def validate_install_layer(layer: Path) -> dict:
    metadata = json.loads(
        (layer / ".layer.json").read_text(encoding="utf-8"))
    if metadata.get("format") != 2:
        raise SystemExit(f"unsupported stage cache layer format: {layer}")
    if (not isinstance(metadata.get("changed"), dict) or
            not isinstance(metadata.get("removed"), list)):
        raise SystemExit(f"malformed stage cache layer metadata: {layer}")
    for relative, expected_identity in metadata["changed"].items():
        if not isinstance(relative, str) or not isinstance(expected_identity, str):
            raise SystemExit(f"malformed stage cache layer entry: {layer}")
        source = safe_layer_path(layer, relative)
        actual = path_identity(source)
        if actual != expected_identity:
            raise SystemExit(
                f"stage cache layer content mismatch: {source} "
                f"(expected {expected_identity}, got {actual})")
    for relative in metadata["removed"]:
        if not isinstance(relative, str):
            raise SystemExit(f"malformed stage cache removal entry: {layer}")
        safe_layer_path(layer, relative)
    return metadata


def install_layer_valid(layer: Path) -> bool:
    try:
        validate_install_layer(layer)
        return True
    except (OSError, ValueError, KeyError, TypeError, SystemExit) as exc:
        print(f"+ [work-root cache] rejecting invalid layer {layer}: {exc}",
              flush=True)
        return False


def apply_install_layer(layer: Path, staged: Path) -> None:
    # Validate every cached byte before changing the clean predecessor copy.
    metadata = validate_install_layer(layer)
    for relative in metadata["removed"]:
        destination = safe_layer_path(staged, relative)
        if destination.exists() or destination.is_symlink():
            remove_path(destination)
    for relative in metadata["changed"]:
        source = safe_layer_path(layer, relative)
        copy_layer_entry(source, safe_layer_path(staged, relative))


def runtime_env(sdk: Path, manifest: dict) -> dict[str, str]:
    env = os.environ.copy()
    target = manifest["target"]
    required = manifest.get("external_toolchain_environment", [])
    missing = [name for name in required if not os.environ.get(name)]
    if missing:
        raise SystemExit(
            "external toolchain environment is incomplete; set before "
            "starting the stage build: " + ", ".join(missing))
    env.update({
        "CRT_SYSROOT": str(sdk),
        "CRT_ROOTFS": str(sdk),
        "CRT_TARGET_OS": target["os"],
        "CRT_TARGET_ARCH": target["arch"],
        # The predecessor may have been packaged before crt-c++ learned to
        # infer the cumulative SDK's canonical libc++ include directory.
        # Preserve the wrapper's required C++-before-C include order while
        # bootstrapping the next stage from any otherwise-valid 03 SDK.
        "CRT_CXX_STANDARD_INCLUDE_FLAGS":
            f"-isystem{sdk / 'include' / 'c++' / 'v1'}",
    })
    if os.environ.get("CRT_CC"):
        env["CRT_HOST_CC"] = Path(os.environ["CRT_CC"]).as_posix()
    if os.environ.get("CRT_CXX"):
        env["CRT_HOST_CXX"] = Path(os.environ["CRT_CXX"]).as_posix()
    if os.environ.get("CRT_AR"):
        env["CRT_HOST_AR"] = Path(os.environ["CRT_AR"]).as_posix()
    if target["os"] == "windows":
        env["CRT_MKSH_EXE"] = str(sdk / "system" / "bin" / "mksh.exe")
        env["CRT_HOST_PYTHON"] = sys.executable
        env["PATH"] = str(sdk / "bin") + os.pathsep + env.get("PATH", "")
    return env


def first_existing(root: Path, relatives: tuple[str, ...]) -> Path:
    for relative in relatives:
        candidate = root / relative
        if candidate.is_file():
            return candidate
    raise SystemExit(f"none of the required notice files exist under {root}: {relatives}")


def copy_dependency_record(staged: Path, name: str, recipe: Path,
                           notice: Path, headers: list[str],
                           links: list[str], runtimes: list[str]) -> dict:
    notice_relative = Path("share") / "licenses" / name / notice.name
    notice_dest = staged / notice_relative
    notice_dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(notice, notice_dest)

    provenance_relative = Path("share") / "crt" / "dependencies" / name / "recipe.json"
    provenance_dest = staged / provenance_relative
    provenance_dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(recipe, provenance_dest)
    return {
        "name": name,
        "kind": "private-static-port",
        "headers": headers,
        "link_artifacts": links,
        "runtime_artifacts": runtimes,
        "notices": [notice_relative.as_posix()],
        "provenance": provenance_relative.as_posix(),
    }


def existing_relative_files(root: Path, patterns: tuple[str, ...]) -> list[str]:
    found: list[str] = []
    for pattern in patterns:
        found.extend(path.relative_to(root).as_posix()
                     for path in root.glob(pattern) if path.is_file())
    return sorted(set(found))


def fetch_port_sources(asset: Path, staged: Path, temp_root: Path,
                       env: dict[str, str]) -> Path:
    """Only the fetch+checksum-verify step -- always run, even when the
    slow configure/make/install below is skipped by the --work-root cache,
    because add_redistributed_dependencies() still needs a real verified
    source checkout to read each port's LICENSE file out of. Cheap: this is
    a download+extract, not a build."""
    source_root = temp_root / "port-sources"
    run([
        sys.executable, str(staged / "tools" / "fetch_ports.py"),
        "--dest", str(source_root),
        "--cache", str(Path(asset).parent / "port-downloads"),
        "--port", "freetype",
        "--port", "ffmpeg",
    ], env, asset)
    return source_root


def build_ports(asset: Path, source_root: Path, staged: Path, temp_root: Path,
                manifest: dict, env: dict[str, str], jobs: int) -> None:
    driver = staged / "tools" / "crt-port-build.py"
    command = [
        sys.executable, str(driver),
        "--sdk-root", str(staged),
        "--target-os", manifest["target"]["os"],
        "--target-arch", manifest["target"]["arch"],
        "--source-root", str(source_root),
        "--work-root", str(temp_root / "port-build"),
        "--install-prefix", str(staged),
        # NOT a fix for the "can't fork - try again" failure this stage hit
        # here -- confirmed directly (2026-09-10) by reproducing the exact
        # same failure with --jobs 1 already in effect. Root-caused instead:
        # a real Windows-PAL handle leak in __crt_sys_posix_spawn()'s fd-
        # snapshot export/child-duplicate path (libc/src/arch/windows/
        # common/syscall.c), proportional to *pipeline* (`cmd1 | cmd2`)
        # execution specifically -- confirmed with a minimal, deterministic
        # repro (`echo hi | sed ... >/dev/null` in a loop leaks exactly +1
        # real Windows handle per iteration; the same loop with a plain
        # external command or a redirected subshell, no pipe, leaks
        # nothing). FreeType's real autoconf `configure` is pipeline-heavy
        # (the AS_LINENO self-test's `sed | sed` chain, repeated `` `expr
        # ...` `` substitutions) and is the first real port in this stage
        # chain to run enough of that to exhaust CRT_FD_TABLE_SIZE (64).
        # See HISTORY.md for the full repro and fix. This stage now uses a
        # measured, bounded parallel default; this is not a workaround for
        # that already-fixed handle leak.
        "--jobs", str(jobs),
        "--port", "freetype",
        "--port", "ffmpeg",
    ]
    run(command, env, asset)


def fetch_skia_source(asset: Path, temp_root: Path,
                      env: dict[str, str]) -> Path:
    """Only the fetch+expected-commit-verify step -- always run for the
    same license-lookup reason as fetch_port_sources() above, even when the
    slow ninja build below is skipped by the --work-root cache."""
    recipe_path = asset / "libcrtgfx" / "third_party" / "skia" / "recipe.json"
    recipe = json.loads(recipe_path.read_text(encoding="utf-8"))
    source_spec = recipe["source"]
    source = temp_root / "skia-source"
    fetch = [
        sys.executable, str(asset / "tools" / "fetch_skia.py"),
        "--dest", str(source),
        "--repo", source_spec["repository"],
        "--version", source_spec["version"],
        "--ref", source_spec["ref"],
        "--expected-commit", source_spec["expected_commit"],
    ]
    for sparse_path in source_spec.get("sparse_paths", []):
        fetch.extend(["--sparse-path", sparse_path])
    if source_spec.get("sync_deps"):
        fetch.append("--sync-deps")
    run(fetch, env, asset)
    return source


def fetch_mingw_w64_headers(asset: Path, temp_root: Path,
                            env: dict[str, str]) -> Path:
    """Always run (not cached alongside the Skia build): the final stage
    CMakeLists.txt configure step below needs CRT_STAGE_MINGW_W64_HEADERS_
    ROOT on every run regardless of whether the Skia *build* itself was
    skipped by the --work-root cache -- it is what
    crtgfx_skia_raster_smoke/crtgfx_skia_gpu_window_demo's own win32_shim
    compile flags need, not something build_skia.py consumes and discards.
    Cheap: a headers-only sparse checkout, not a build."""
    mingw_checkout = temp_root / "mingw-w64"
    run([
        sys.executable,
        str(asset / "tools" / "fetch_mingw_w64_headers.py"),
        "--dest", str(mingw_checkout),
    ], env, asset)
    return mingw_checkout


def build_skia(asset: Path, source: Path, mingw_checkout: Path | None,
               staged: Path, temp_root: Path, manifest: dict,
               env: dict[str, str]) -> None:
    build = [
        sys.executable, str(asset / "tools" / "build_skia.py"),
        "--root", str(asset),
        "--source", str(source),
        "--build-dir", str(temp_root / "skia-build"),
        "--install-prefix", str(staged),
        "--sysroot", str(staged),
        "--target-os", manifest["target"]["os"],
        "--target-arch", manifest["target"]["arch"],
        "--freetype-prefix", str(staged),
    ]
    if manifest["target"]["os"] == "windows":
        assert mingw_checkout is not None
        build.extend([
            "--rootfs", str(staged),
            "--mingw-w64-headers-root",
            str(mingw_checkout / "mingw-w64-headers" / "include"),
        ])
    run(build, env, asset)


def add_redistributed_dependencies(asset: Path, staged: Path,
                                   port_sources: Path, skia_source: Path,
                                   manifest: dict) -> None:
    freetype_source = next(port_sources.glob("freetype-*"), None)
    ffmpeg_source = next(port_sources.glob("ffmpeg-*"), None)
    if freetype_source is None or ffmpeg_source is None:
        raise SystemExit("the port driver did not leave verified FreeType/FFmpeg sources")

    dependencies = [
        copy_dependency_record(
            staged, "freetype", asset / "porting" / "recipes" / "freetype.json",
            first_existing(freetype_source, ("LICENSE.TXT", "LICENSE", "docs/LICENSE.TXT")),
            ["include/freetype2"], ["lib/libfreetype.a"],
            existing_relative_files(staged, (
                "lib/libfreetype.so*", "lib/libfreetype*.dylib",
                "bin/libfreetype*.dll", "lib/libfreetype*.dll.a"))),
        copy_dependency_record(
            staged, "ffmpeg", asset / "porting" / "recipes" / "ffmpeg.json",
            first_existing(ffmpeg_source, ("COPYING.LGPLv2.1", "COPYING.LGPLv3", "LICENSE.md")),
            ["include/libavformat", "include/libavcodec",
             "include/libswresample", "include/libavutil"],
            ["lib/libavformat.a", "lib/libavcodec.a",
             "lib/libswresample.a", "lib/libavutil.a"],
            existing_relative_files(staged, (
                "lib/libavformat.so*", "lib/libavcodec.so*",
                "lib/libswresample.so*", "lib/libavutil.so*",
                "lib/libavformat*.dylib", "lib/libavcodec*.dylib",
                "lib/libswresample*.dylib", "lib/libavutil*.dylib",
                "bin/avformat*.dll", "bin/avcodec*.dll",
                "bin/swresample*.dll", "bin/avutil*.dll"))),
        copy_dependency_record(
            staged, "skia", asset / "libcrtgfx" / "third_party" / "skia" / "recipe.json",
            first_existing(skia_source, ("LICENSE",)),
            ["include/include", "include/modules/skcms"],
            ["lib/libskia.a"], []),
    ]
    retained = [item for item in manifest.get("redistributed_dependencies", [])
                if item.get("name") not in {"freetype", "ffmpeg", "skia"}]
    manifest["redistributed_dependencies"] = retained + dependencies


def build_example(staged: Path, temp_root: Path, name: str,
                  executable_name: str, env: dict[str, str]) -> None:
    build_dir = temp_root / f"example-{name}"
    if build_dir.exists():
        remove_tree(build_dir)
    run([
        "cmake", "-S", str(staged / "examples" / name),
        "-B", str(build_dir), "-G", "Ninja",
        f"-DCMAKE_TOOLCHAIN_FILE={cmake_path(staged / 'crt-toolchain.cmake')}",
    ], env)
    run(["cmake", "--build", str(build_dir)], env)
    suffix = ".exe" if env["CRT_TARGET_OS"] == "windows" else ""
    run([str(build_dir / f"{executable_name}{suffix}"), "1"], env)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-root", required=True, type=Path)
    parser.add_argument("--asset-root", required=True, type=Path)
    parser.add_argument("--work-root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--recipe-location", required=True)
    parser.add_argument("--source-sha256", required=True)
    # Off by default: a plain --work-root with no flag reproduces this
    # script's original from-scratch-every-time behavior exactly (still the
    # right default for a genuine, unconditional final acceptance run).
    # Pass this during iterative debugging of the *stage CMakeLists.txt/
    # cmake modules* themselves -- FreeType/FFmpeg/Skia are pinned, external,
    # and unaffected by that kind of fix, so re-running their real
    # multi-hour configure/make/ninja for every single fix-and-retry cycle
    # was pure waste (confirmed for real, 2026-09-11: 9 isolated reruns in
    # one session, each paying that same ~90 minute tax to re-verify a
    # one-line CMakeLists.txt change).
    parser.add_argument("--reuse-work-root", action="store_true",
                        help="Keep verified dependency install layers under "
                             "--work-root and apply them to a clean predecessor "
                             "SDK when its content, recipes, build tools and "
                             "external toolchain identity have not changed.")
    parser.add_argument(
        "--dependency-jobs", type=int, default=DEFAULT_DEPENDENCY_JOBS,
        help="make -jN used for FreeType/FFmpeg (default: CPU count capped "
             f"at 4; resolves to {DEFAULT_DEPENDENCY_JOBS} on this host)")
    args = parser.parse_args()
    if args.dependency_jobs < 1:
        parser.error("--dependency-jobs must be at least 1")
    print(f"Dependency build parallelism: -j{args.dependency_jobs}",
          flush=True)

    timings = PhaseTimings()

    sdk = args.sdk_root.resolve()
    asset = args.asset_root.resolve()
    output = args.output.resolve()
    if output.exists():
        raise SystemExit(f"output already exists: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    work_root = args.work_root.resolve()
    work_root.mkdir(parents=True, exist_ok=True)

    if args.reuse_work_root:
        temp_root = work_root
    else:
        # Own private scratch dir, discarded in `finally` below -- the
        # original, still-default, always-genuinely-from-scratch behavior.
        temp_root = Path(tempfile.mkdtemp(
            prefix="crt-stage-04-gfx-media-", dir=work_root))
    try:
        staged = temp_root / "sdk"
        manifest_path_probe = sdk / "manifest.json"
        manifest = json.loads(manifest_path_probe.read_text(encoding="utf-8"))
        if manifest.get("stage") != "03-gfx-simple":
            raise SystemExit("04-gfx-media requires a 03-gfx-simple predecessor SDK")
        target_os = manifest["target"]["os"]
        if target_os == "windows" and not os.environ.get("CRT_WINDOWS_SDK_LIBPATH"):
            raise SystemExit("CRT_WINDOWS_SDK_LIBPATH is required on Windows")

        # Preserve the path-with-spaces acceptance input, while retaining the
        # same explicit Windows wrapper workaround as stages 02 and 03.
        build_asset = asset
        if target_os == "windows":
            build_asset = temp_root / "source"
            if build_asset.exists():
                remove_tree(build_asset)
            shutil.copytree(asset, build_asset)
        touch_tree(build_asset)

        # Hash the predecessor's real content, not its volatile packaging
        # timestamp. This covers installed headers, libraries, wrappers and
        # toolchain files that the dependency builds actually consume.
        with timings.measure("fingerprint predecessor SDK and toolchain"):
            predecessor_key = inventory_fingerprint(
                tree_inventory(sdk, normalize_manifest=True))
            compiler_key = toolchain_identity(target_os)
        ports_key = predecessor_key + "-" + compiler_key + "-" + fingerprint([
            build_asset / "porting" / "recipes" / "freetype.json",
            build_asset / "porting" / "recipes" / "ffmpeg.json",
        ])
        skia_inputs_key = fingerprint([
            build_asset / "libcrtgfx" / "third_party" / "skia" / "recipe.json",
            build_asset / "tools" / "build_skia.py",
        ] + ([build_asset / "tools" / "fetch_mingw_w64_headers.py"]
             if target_os == "windows" else []))
        cache_dir = temp_root / ".crt-stage-cache"
        ports_marker = cache_dir / "ports.key"
        skia_marker = cache_dir / "skia.key"
        ports_layer = cache_dir / "ports-layer"
        skia_layer = cache_dir / "skia-layer"

        ports_cached = (
            args.reuse_work_root and ports_layer.is_dir() and
            (ports_layer / ".layer.json").is_file() and
            ports_marker.is_file() and
            ports_marker.read_text(encoding="utf-8").strip() == ports_key and
            install_layer_valid(ports_layer))
        # Always start from the declared predecessor. Cache reuse applies
        # immutable dependency deltas to this clean copy; it never treats a
        # previous run's mutable 04-stage tree as a trustworthy cache.
        with timings.measure("prepare clean predecessor SDK copy"):
            if staged.exists():
                remove_tree(staged)
            shutil.copytree(sdk, staged)
        env = runtime_env(staged, manifest)

        with timings.measure("fetch and verify FreeType/FFmpeg sources"):
            port_sources = fetch_port_sources(
                build_asset, staged, temp_root, env)
        if ports_cached:
            with timings.measure("reuse cached FreeType/FFmpeg install"):
                apply_install_layer(ports_layer, staged)
                print(f"+ [work-root cache] reusing FreeType/FFmpeg install "
                      f"layer in {staged} (key {ports_key})", flush=True)
        else:
            before_ports = tree_inventory(staged) if args.reuse_work_root else {}
            port_build_root = temp_root / "port-build"
            if port_build_root.exists():
                remove_tree(port_build_root)
            with timings.measure(
                    f"build and install FreeType/FFmpeg (-j{args.dependency_jobs})"):
                build_ports(build_asset, port_sources, staged, temp_root,
                            manifest, env, args.dependency_jobs)
            if args.reuse_work_root:
                with timings.measure("capture FreeType/FFmpeg install layer"):
                    cache_dir.mkdir(parents=True, exist_ok=True)
                    capture_install_layer(staged, before_ports, ports_layer)
                    ports_marker.write_text(ports_key, encoding="utf-8")

        # Skia consumes the installed FreeType result, so key it from the
        # actual post-port staged content rather than assuming equal recipes
        # necessarily produced equal archives on every toolchain revision.
        with timings.measure("fingerprint installed FreeType/FFmpeg layer"):
            skia_key = inventory_fingerprint(
                tree_inventory(staged)) + "-" + skia_inputs_key

        with timings.measure("fetch and verify Skia source"):
            skia_source = fetch_skia_source(build_asset, temp_root, env)
        mingw_checkout = None
        if target_os == "windows":
            with timings.measure("fetch and verify MinGW-w64 headers"):
                mingw_checkout = fetch_mingw_w64_headers(
                    build_asset, temp_root, env)
        skia_cached = (
            args.reuse_work_root and skia_layer.is_dir() and
            (skia_layer / ".layer.json").is_file() and
            skia_marker.is_file() and
            skia_marker.read_text(encoding="utf-8").strip() == skia_key and
            install_layer_valid(skia_layer))
        if skia_cached:
            with timings.measure("reuse cached Skia install"):
                apply_install_layer(skia_layer, staged)
                print(f"+ [work-root cache] reusing Skia build already in "
                      f"{staged} (key {skia_key})", flush=True)
        else:
            before_skia = tree_inventory(staged) if args.reuse_work_root else {}
            skia_build_root = temp_root / "skia-build"
            if skia_build_root.exists():
                remove_tree(skia_build_root)
            with timings.measure("build and install Skia"):
                build_skia(build_asset, skia_source, mingw_checkout, staged,
                           temp_root, manifest, env)
            if args.reuse_work_root:
                with timings.measure("capture Skia install layer"):
                    cache_dir.mkdir(parents=True, exist_ok=True)
                    capture_install_layer(staged, before_skia, skia_layer)
                    skia_marker.write_text(skia_key, encoding="utf-8")

        build_dir = temp_root / "gfx-media-build"
        if build_dir.exists():
            remove_tree(build_dir)
        configure = [
            "cmake", "-S", str(build_asset / "distribution" / "stages" / "04-gfx-media"),
            "-B", str(build_dir), "-G", "Ninja",
            f"-DCMAKE_TOOLCHAIN_FILE={cmake_path(staged / 'crt-toolchain.cmake')}",
            f"-DCMAKE_INSTALL_PREFIX={cmake_path(staged)}",
            f"-DCRT_STAGE_SOURCE_ROOT={cmake_path(build_asset)}",
            f"-DCRT_STAGE_TARGET_OS={target_os}",
            f"-DCRT_STAGE_SKIA_PREFIX={cmake_path(staged)}",
            f"-DCRT_STAGE_FREETYPE_PREFIX={cmake_path(staged)}",
            f"-DCRT_STAGE_FFMPEG_PREFIX={cmake_path(staged)}",
        ]
        if mingw_checkout is not None:
            configure.append(
                "-DCRT_STAGE_MINGW_W64_HEADERS_ROOT=" +
                cmake_path(mingw_checkout / "mingw-w64-headers" / "include"))
        with timings.measure("configure 04-gfx-media"):
            run(configure, env)
        with timings.measure("build 04-gfx-media"):
            run(["cmake", "--build", str(build_dir)], env)
        with timings.measure("test 04-gfx-media"):
            run(["ctest", "--test-dir", str(build_dir),
                 "--output-on-failure"], env)
        with timings.measure("install 04-gfx-media"):
            run(["cmake", "--install", str(build_dir)], env)

        with timings.measure("write dependency provenance and manifest"):
            manifest["stage"] = "04-gfx-media"
            manifest["built_from"] = {
                "stage": "03-gfx-simple",
                "source_recipe": args.recipe_location,
                "source_sha256": args.source_sha256,
            }
            add_redistributed_dependencies(
                build_asset, staged, port_sources, skia_source, manifest)
            (staged / "manifest.json").write_text(
                json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

        with timings.measure("rebuild and run installed gfx-gpu example"):
            build_example(
                staged, temp_root, "gfx-gpu", "crtgfx_gpu_example", env)
        with timings.measure("rebuild and run installed gfx-skia example"):
            build_example(
                staged, temp_root, "gfx-skia", "crtgfx_skia_example", env)
        with timings.measure("verify 04-gfx-media distribution"):
            run([sys.executable, str(build_asset / "tools" / "verify_dist.py"),
                 "--dist", str(staged), "--stage", "04-gfx-media"], env)
        with timings.measure("publish 04-gfx-media atomically"):
            publish_tree(staged, output)
        print(f"CRT stage ready: {output}")
    finally:
        # CRT_STAGE_KEEP_TMP=1: skip cleanup and keep temp_root (the build
        # tree, staged SDK copy, and port/Skia build directories) around for
        # post-mortem inspection after a failure -- e.g. re-running a
        # crashing ctest binary directly under lldb/otool with the exact
        # same on-disk layout it just failed with. Without this, temp_root
        # (a fresh tempfile.mkdtemp() every run) is unconditionally deleted
        # here even when main() above raised, so a CalledProcessError from
        # `ctest`/`cmake --build` leaves nothing behind to look at. Added
        # 2026-09-11 debugging the isolated 03-gfx-simple -> 04-gfx-media
        # stage upgrade's own real ctest crashes. --reuse-work-root (its
        # own, complementary reason to keep temp_root: reusing the
        # FreeType/FFmpeg/Skia build across runs, not post-mortem
        # debugging) already keeps it via args.reuse_work_root below; that
        # work root now holds immutable dependency layers plus this run's
        # disposable staged copy, so either reason still suffices.
        if os.environ.get("CRT_STAGE_KEEP_TMP"):
            print(f"CRT_STAGE_KEEP_TMP set: keeping {temp_root}")
        elif not args.reuse_work_root:
            remove_tree(temp_root, ignore_errors=True)
        timings.report()


if __name__ == "__main__":
    main()
