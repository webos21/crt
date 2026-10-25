#!/usr/bin/env python3
"""Input-hash cache for the build-tree stage steps (config, test, dist).

A step is skipped when the SHA-256 of everything it depends on equals the hash recorded by its last successful
run and its declared outputs still match their recorded content/type/link inventory. The hash covers:

* the stage's source roots and files (tools/stage_cache.json), hashed by content (with a per-file cache keyed by
  path, size, mtime and ctime so unchanged files are not read again),
* the hashes of the upstream steps it names with --after (so editing libc invalidates every later stage, while
  editing libcrtui invalidates only 05-ui and later),
* the --key values (configuration that changes the result: compiler, options, release tag, ...),
* the command line, executed scripts (including test drivers), explicit artifact/configuration inputs,
  the identity of the resolved tools (not the PATH string) and build-affecting environment. A source/input edit during execution prevents caching.

Usage:
  crt_stage_cache.py run --name NAME --stage STAGE --state-dir DIR [--after NAME ...] [--key K=V ...]
                         [--output PATH ...] [--stamp FILE] -- COMMAND [++ COMMAND ...]
  crt_stage_cache.py hash --stage STAGE --root ROOT [--after-hash H ...]     (print the input hash)
  crt_stage_cache.py status --state-dir DIR                                    (list recorded steps)

``run`` exits with the command's status, prints "[stage-cache] NAME: up to date (HASH)" when it skipped, and never
records a failed run. CRT_STAGE_CACHE=off disables skipping (the command always runs and is recorded).
"""

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CONFIG = ROOT / "tools" / "stage_cache.json"
SKIP_DIRS = {".git", "out", "__pycache__", "node_modules", ".claude"}
SKIP_SUFFIXES = (".pyc", ".o", ".obj", ".a", ".dylib", ".so", ".dll", ".log")


def load_config(path: Path | None = None) -> dict:
    return json.loads((path or CONFIG).read_text(encoding="utf-8"))


def expand(root: Path, entries: list) -> list:
    """Sorted, de-duplicated file list for directories and files given relative to root."""
    files = {}
    for entry in entries:
        if any(ch in entry for ch in "*?["):
            for match in sorted(root.glob(entry)):
                if match.is_file() and not match.is_symlink() and not (
                        match.name.startswith("test_") and match.suffix == ".py"):
                    files[match.relative_to(root).as_posix()] = match
            continue
        target = root / entry
        if target.is_file():
            files[target.relative_to(root).as_posix()] = target
        elif target.is_dir():
            for current, dirs, names in os.walk(target):
                dirs[:] = sorted(d for d in dirs if d not in SKIP_DIRS)
                for name in sorted(names):
                    if name.endswith(SKIP_SUFFIXES) or (name.startswith("test_") and name.endswith(".py")):
                        continue
                    path = Path(current) / name
                    if path.is_file() and not path.is_symlink():
                        files[path.relative_to(root).as_posix()] = path
    return [(name, files[name]) for name in sorted(files)]


class FileHashes:
    """sha256 of file contents with a (size, mtime_ns) keyed cache stored next to the step state."""

    def __init__(self, cache_path: Path | None) -> None:
        self.path = cache_path
        self.entries = {}
        self.dirty = False
        if cache_path and cache_path.is_file():
            try:
                self.entries = json.loads(cache_path.read_text(encoding="utf-8"))
            except (OSError, ValueError):
                self.entries = {}

    def digest(self, key: str, path: Path) -> str:
        stat = path.stat()
        stamp = [stat.st_size, stat.st_mtime_ns, stat.st_ctime_ns]
        cached = self.entries.get(key)
        if cached and cached[:3] == stamp:
            return cached[3]
        sha = hashlib.sha256()
        with path.open("rb") as stream:
            for chunk in iter(lambda: stream.read(1 << 20), b""):
                sha.update(chunk)
        self.entries[key] = stamp + [sha.hexdigest()]
        self.dirty = True
        return sha.hexdigest()

    def save(self) -> None:
        if self.path and self.dirty:
            self.path.parent.mkdir(parents=True, exist_ok=True)
            self.path.write_text(json.dumps(self.entries), encoding="utf-8")


def stage_inputs(stages: dict, name: str, seen: tuple = ()) -> list:
    """Own inputs of `name` followed by those of every included (upstream) stage, without duplicates."""
    if name in seen:
        raise SystemExit(f"stage include cycle: {' -> '.join(seen + (name,))}")
    entry = stages[name]
    result = list(entry.get("inputs", []))
    for upstream in entry.get("includes", []):
        for item in stage_inputs(stages, upstream, seen + (name,)):
            if item not in result:
                result.append(item)
    return result


def input_hash(root: Path, stage: dict, after: list, keys: dict, command: list, hashes: FileHashes) -> str:
    sha = hashlib.sha256()

    def feed(label: str, value: str) -> None:
        sha.update(label.encode() + b"\0" + value.encode() + b"\0")

    for name, path in expand(root, stage["resolved_inputs"] if "resolved_inputs" in stage else stage.get("inputs", [])):
        feed("file", name + ":" + hashes.digest(name, path))
    for value in sorted(after):
        feed("after", value)
    for key in sorted(keys):
        feed("key", f"{key}={keys[key]}")
    feed("command", "\0".join(command))
    return sha.hexdigest()


def state_file(state_dir: Path, name: str) -> Path:
    return state_dir / f"{name}.json"


def read_state(state_dir: Path, name: str) -> dict:
    try:
        return json.loads(state_file(state_dir, name).read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}


def path_inventory(paths, hashes):
    """Content/type/link/execute-bit identity, not directory existence alone.

    Ignore Git bookkeeping and Python bytecode, neither of which is a target
    payload. Never follow directory symlinks (cycles); record their link text.
    """
    result = []
    for value in sorted(map(str, paths)):
        root = Path(value).resolve() if not Path(value).is_symlink() else Path(value).absolute()
        def visit(path):
            name = str(path)
            if path.is_symlink():
                result.append((name, "link", os.readlink(path),
                               hashes.digest("artifact:" + name, path) if path.is_file() else ""))
            elif path.is_file():
                result.append((name, "file", path.stat().st_mode & 0o111,
                               hashes.digest("artifact:" + name, path)))
            elif path.is_dir():
                result.append((name, "dir"))
                for child in sorted(path.iterdir()):
                    if child.name not in (".git", "__pycache__"):
                        visit(child)
            else:
                result.append((name, "missing"))
        visit(root)
    return hashlib.sha256(json.dumps(result, separators=(",", ":")).encode()).hexdigest()


def command_files(command, root):
    """Hash executed scripts even when their test_* names are not source globs."""
    result = []
    for index, token in enumerate(command):
        if token == "++":
            continue
        path = Path(token)
        if not path.is_absolute():
            local = root / path
            path = local if local.is_file() else path
        if path.is_file() and (path.suffix in (".py", ".cmake") or index == 0):
            result.append(path)
    return result


def tool_identity(programs, path=None):
    """Identity of the programs a step may run, resolved through PATH.

    The key is what was found (real path, size, modification time), never the PATH string: another shell, a
    reordered PATH or an unrelated directory must not invalidate a step whose tools are the same files.
    """
    found = []
    for name in sorted(programs):
        located = shutil.which(name, path=path)
        if not located:
            found.append((name, None))
            continue
        real = os.path.realpath(located)
        try:
            status = os.stat(real)
            found.append((name, real, status.st_size, status.st_mtime_ns))
        except OSError:
            found.append((name, real, None, None))
    return hashlib.sha256(json.dumps(found, separators=(",", ":")).encode()).hexdigest()


def producer_commit(root):
    if not (root / ".git").exists():
        return None
    return subprocess.run(["git", "-C", str(root), "rev-parse", "HEAD"], capture_output=True, text=True,
                          check=True, creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0).stdout.strip()


def run_step(args: argparse.Namespace, command: list) -> int:
    config = load_config()
    if args.stage not in config["stages"]:
        raise SystemExit(f"unknown stage {args.stage!r} in {CONFIG}")
    stage = {"resolved_inputs": stage_inputs(config["stages"], args.stage)}
    state_dir = Path(args.state_dir)
    hashes = FileHashes(state_dir / "file-hashes.json")
    after = []
    for upstream in args.after:
        previous = read_state(state_dir, upstream)
        after.append(f"{upstream}={previous.get('hash', 'never-ran')}:{previous.get('outputs', '')}")
    keys = dict(item.split("=", 1) for item in args.key)
    if args.name.endswith("-dist") or args.name.startswith("stage-") or args.name in ("dev-sdk", "web-build"):
        # Packaging/source correspondence must move to the merged commit even
        # when that commit changed documentation only. Compilation/tests remain
        # source-driven and do not pay for a doc-only commit.
        keys["producer_commit"] = producer_commit(Path(args.root)) or "source-asset"
    extra_inputs = [*getattr(args, "input", []), *getattr(args, "config_file", []),
                    *command_files(command, Path(args.root))]
    keys["artifact_inputs"] = path_inventory(extra_inputs, hashes)
    # Wrapper/tool selection and environment-only compile/link settings must
    # not survive a changed invocation just because CMake's options match.
    for name, content in sorted(os.environ.items()):
        if name.startswith("CRT_") and name != "CRT_STAGE_CACHE" or name in (
                "CC", "CXX", "AR", "RANLIB", "CFLAGS", "CXXFLAGS", "CPPFLAGS", "LDFLAGS"):
            keys["env:" + name] = content
    # PATH itself is not a key (see tool_identity): the programs it resolves are.
    keys["tools"] = tool_identity(config.get("tool_identity", {}).get("programs", ()), os.environ.get("PATH"))
    value = input_hash(Path(args.root), stage, after, keys, command, hashes)
    recorded = read_state(state_dir, args.name)
    outputs_exist = all(Path(path).exists() for path in args.output)
    outputs = path_inventory(args.output, hashes)
    hashes.save()
    disabled = os.environ.get("CRT_STAGE_CACHE", "").lower() in ("off", "0", "false")
    if (not disabled and recorded.get("hash") == value and recorded.get("status") == "ok"
            and outputs_exist and recorded.get("outputs") == outputs):
        print(f"[stage-cache] {args.name}: up to date ({value[:12]})", flush=True)
        if args.stamp:
            Path(args.stamp).touch()
        return 0
    reason = "disabled" if disabled else ("first run" if not recorded else
             "outputs changed or missing" if recorded.get("hash") == value else "inputs changed")
    print(f"[stage-cache] {args.name}: running ({reason}, {value[:12]})", flush=True)
    # Invalidate the old success before execution, including interruptions or
    # spawn errors, not just ordinary nonzero process exits.
    state_file(state_dir, args.name).unlink(missing_ok=True)
    result = 0
    sequence, current = [], []
    for token in command:
        if token == "++":
            sequence.append(current)
            current = []
        else:
            current.append(token)
    sequence.append(current)
    for part in sequence:  # "A ++ B": run sequentially, stop at the first failure
        result = subprocess.run(part, stdout=sys.stdout if hasattr(sys.stdout, "buffer") else None,
                                stderr=sys.stderr if hasattr(sys.stderr, "buffer") else None,
                                creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0).returncode
        if result != 0:
            break
    if result == 0:
        if not all(Path(path).exists() for path in args.output):
            print(f"[stage-cache] {args.name}: successful command did not create its declared outputs", file=sys.stderr)
            return 1
        # Commands can change their inputs (the nested runtime driver owns
        # configure stamps); never record a success against pre-run payloads.
        keys["artifact_inputs"] = path_inventory(extra_inputs, hashes)
        after_value = input_hash(Path(args.root), stage, after, keys, command, hashes)
        if after_value != value:
            print(f"[stage-cache] {args.name}: inputs changed during execution; success is not cached", flush=True)
            hashes.save()
            return 0
        outputs = path_inventory(args.output, hashes)
        hashes.save()
        state_dir.mkdir(parents=True, exist_ok=True)
        state_file(state_dir, args.name).write_text(
            json.dumps({"name": args.name, "stage": args.stage, "hash": value, "outputs": outputs, "status": "ok"}, indent=1),
            encoding="utf-8")
        if args.stamp:
            Path(args.stamp).touch()
    elif state_file(state_dir, args.name).is_file():
        state_file(state_dir, args.name).unlink()  # a failed run must not leave an older success to skip against
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="mode", required=True)
    run = sub.add_parser("run")
    run.add_argument("--name", required=True)
    run.add_argument("--stage", required=True)
    run.add_argument("--state-dir", required=True)
    run.add_argument("--root", default=str(ROOT))
    run.add_argument("--after", action="append", default=[])
    run.add_argument("--key", action="append", default=[])
    run.add_argument("--output", action="append", default=[])
    run.add_argument("--input", action="append", default=[], help="content-pinned external file or directory")
    run.add_argument("--config-file", action="append", default=[], help="resolved build configuration input")
    run.add_argument("--stamp", help="file touched on success or skip (the Ninja output of the step)")
    show = sub.add_parser("hash")
    show.add_argument("--stage", required=True)
    show.add_argument("--root", default=str(ROOT))
    show.add_argument("--after-hash", action="append", default=[])
    status = sub.add_parser("status")
    status.add_argument("--state-dir", required=True)
    args, rest = parser.parse_known_args()
    if args.mode == "run":
        command = rest[1:] if rest[:1] == ["--"] else rest
        if not command:
            parser.error("run needs a command after --")
        return run_step(args, command)
    if args.mode == "hash":
        stages = load_config()["stages"]
        stage = {"resolved_inputs": stage_inputs(stages, args.stage)}
        print(input_hash(Path(args.root), stage, args.after_hash, {}, [], FileHashes(None)))
        return 0
    for path in sorted(Path(args.state_dir).glob("*.json")):
        if path.name != "file-hashes.json":
            item = json.loads(path.read_text(encoding="utf-8"))
            print(f"{item['name']:<28} {item['status']:<4} {item['hash'][:12]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
