#!/usr/bin/env python3
"""Relink kit for a shared library that statically folds in an LGPL library (FFmpeg in libcrtmedia).

LGPL lets a recipient replace the LGPL library inside a combined work only if the rest of the work is delivered in a
form that can be relinked. libcrtmedia's shared object is linked from CRT-owned objects plus FFmpeg's static archives;
this tool captures that link from the build (``ninja -t commands``) and writes a kit into the SDK:

    share/crt/relink/<name>/
        objects/*.obj|*.o       the CRT-side objects, exactly as linked
        support/*               export lists the link needs (.def, version scripts)
        relink.json             the link command with @SDK@/@OBJ@/@SUPPORT@/@OUT@ placeholders
        relink.py               stdlib-only runner (--ffmpeg-libdir DIR replaces the FFmpeg archives)
        README.txt

The link line is taken from the build, not written by hand, and any build-tree path that survives the rewrite fails
the kit instead of shipping a command that cannot work elsewhere.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys

OBJECT_SUFFIXES = (".o", ".obj")
SUPPORT_SUFFIXES = (".def", ".map", ".exp", ".ver", ".lds", ".txt")
FFMPEG_ARCHIVES = ("libavcodec.a", "libavformat.a", "libavutil.a", "libswresample.a")
KIT_RELATIVE = Path("share/crt/relink")

RUNNER = r'''#!/usr/bin/env python3
"""Relink {name} against your own build of the LGPL library. Standard library only.

  python relink.py [--ffmpeg-libdir DIR] [--out DIR] [--cc COMPILER] [--print]

Run it in a shell where the SDK is activated (Windows: call <sdk>\\activate.cmd; elsewhere: . <sdk>/activate.sh).
The SDK root is found from this file's location (share/crt/relink/{name}/) unless --sdk is given.
--ffmpeg-libdir replaces libavcodec.a, libavformat.a, libavutil.a and libswresample.a with the files of the same
names in DIR. Host libraries (the Windows SDK import libraries on Windows) are used from the paths recorded in
relink.json and must exist on this machine.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

KIT = Path(__file__).resolve().parent
FFMPEG = {ffmpeg!r}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--sdk", type=Path, default=KIT.parents[3])
    parser.add_argument("--ffmpeg-libdir", type=Path)
    parser.add_argument("--out", type=Path, default=Path.cwd() / "relinked")
    parser.add_argument("--cc", help="compiler driver (default: the recorded one, found on PATH)")
    parser.add_argument("--print", action="store_true", help="show the link command and exit")
    args = parser.parse_args()
    recipe = json.loads((KIT / "relink.json").read_text(encoding="utf-8"))
    if not args.print and not (args.cc or os.environ.get("CRT_SYSROOT")):
        sys.exit("relink: activate the SDK first (Windows: call <sdk>\\activate.cmd, others: . <sdk>/activate.sh), "
                 "or pass --cc with your own compiler driver")
    command = []
    for item in [args.cc or recipe["compiler"]] + recipe["args"]:
        for name, value in (("@SDK@", args.sdk), ("@OBJ@", KIT / "objects"), ("@SUPPORT@", KIT / "support"),
                            ("@OUT@", args.out)):
            item = item.replace(name, str(value))
        if args.ffmpeg_libdir is not None and Path(item).name in FFMPEG:
            replacement = args.ffmpeg_libdir / Path(item).name
            if not replacement.is_file():
                sys.exit(f"relink: {{replacement}} does not exist")
            item = str(replacement)
        command.append(item)
    if args.print:
        print(subprocess.list2cmdline(command))
        return 0
    args.out.mkdir(parents=True, exist_ok=True)
    return subprocess.run(command, cwd=KIT).returncode


if __name__ == "__main__":
    sys.exit(main())
'''

README = """Relink kit: {name}

{output} is linked from CRT-owned objects and the static FFmpeg archives (LGPL). To replace FFmpeg with your own
build and produce a new {output}:

  (activate the SDK first: Windows `call <sdk>\\activate.cmd`, others `. <sdk>/activate.sh`)
  python relink.py --ffmpeg-libdir <your FFmpeg lib directory> --out <output directory>

Build your FFmpeg with the recipe in the corresponding source (porting/recipes/ffmpeg.json) so that its libraries
carry the same names. relink.py --print shows the exact command. The objects here are the ones the SDK's {output}
was linked from (objects/MANIFEST.json has their SHA-256); the host libraries named in relink.json (for example the
Windows SDK import libraries) must exist on your machine. See share/licenses/SOURCE-OFFER.md for the source.
"""


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def link_tokens(commands: str, output: str, windows: bool | None = None) -> list:
    """The tokens of the single link command that produces `output` from ``ninja -t commands`` text."""
    windows = os.name == "nt" if windows is None else windows
    last = [line for line in commands.splitlines() if line.strip()][-1].strip()
    if windows and last.lower().startswith("c:\\windows\\system32\\cmd.exe /c"):
        last = last.split("/C", 1)[1].strip()
        if last.startswith('"') and last.endswith('"'):
            last = last[1:-1]
    tokens = shlex.split(last, posix=not windows)
    tokens = [token[1:-1] if len(token) > 1 and token[0] == token[-1] == '"' else token for token in tokens]
    segments, current = [], []
    for token in tokens:
        if token == "&&":
            segments.append(current)
            current = []
        else:
            current.append(token)
    segments.append(current)
    candidates = [segment for segment in segments
                  if segment and segment[0] not in (":", "cd") and any(Path(t).name == Path(output).name or t.endswith(output)
                                                                         for t in segment)]
    if len(candidates) != 1:
        raise SystemExit(f"cannot find the link command for {output} ({len(candidates)} candidates)")
    return candidates[0]


def under_sdk(token: str, roots: list, windows: bool | None = None) -> str:
    """`token` with the SDK location replaced by @SDK@ (separators normalised, case-insensitive on Windows).

    `roots` holds every spelling of the SDK directory (a build may name it through a symlink such as macOS /var)."""
    text = token.replace(chr(92), "/")
    for root in roots:
        root = root.replace(chr(92), "/").rstrip("/")
        position = text.lower().find(root.lower()) if (os.name == "nt" if windows is None else windows) else text.find(root)
        if position >= 0:
            return text[:position] + "@SDK@" + text[position + len(root):]
    return text


def rewrite(tokens: list, build: Path, sdk_roots: list, objects: dict, support: dict) -> list:
    """Placeholders for everything that depends on where the build and the SDK live."""
    result, skip = [], False
    for index, original in enumerate(tokens):
        if skip:
            skip = False
            continue
        token = under_sdk(original, sdk_roots)
        if token == "-o":
            result += ["-o", f"@OUT@/{Path(tokens[index + 1].replace(chr(92), '/')).name}"]
            skip = True
            continue
        if token.startswith("-Wl,--out-implib,"):
            result.append("-Wl,--out-implib,@OUT@/" + Path(token.split(",", 2)[2].replace(chr(92), "/")).name)
            continue
        if token.startswith("-") or index == 0 or "@SDK@" in token:
            result.append(token)
            continue
        absolute = token.startswith('/') or bool(re.match(r"^[A-Za-z]:[/\\]", token))
        path = Path(token)
        resolved = path if absolute else build / path
        if token.lower().endswith(OBJECT_SUFFIXES):
            # CMake embeds the whole source path in an object's directory (long, host-specific): keep the file name,
            # and add the owning target's directory only when two objects would collide.
            name = resolved.name
            if name in objects and objects[name] != resolved:
                name = f"{resolved.parent.parent.name}__{name}" if resolved.parent.name != resolved.parent.parent.name else f"{len(objects)}__{name}"
            objects[name] = resolved
            result.append(f"@OBJ@/{name}")
        elif token.lower().endswith(SUPPORT_SUFFIXES) and resolved.is_file() and build in resolved.parents:
            support[resolved.name] = resolved
            result.append(f"@SUPPORT@/{resolved.name}")
        elif absolute and build not in resolved.parents:
            result.append(token)  # a host library outside the build and the SDK (recorded as such)
        else:
            raise SystemExit(f"link input {original!r} is neither a CRT object, an export list nor in the SDK; "
                             "the kit would not work elsewhere")
    return result


OUTPUT_CANDIDATES = {"windows": ("bin/libcrtmedia.dll",), "linux": ("lib/libcrtmedia.so", "bin/libcrtmedia.so"),
                     "macos": ("lib/libcrtmedia.dylib", "bin/libcrtmedia.dylib")}


def find_output(build: Path, target_os: str, ninja: str = "ninja") -> str:
    targets = subprocess.run([ninja, "-C", str(build), "-t", "targets", "all"], check=True,
                             capture_output=True, text=True).stdout
    names = {line.split(":", 1)[0].strip() for line in targets.splitlines()}
    for candidate in OUTPUT_CANDIDATES[target_os]:
        if candidate in names:
            return candidate
    raise SystemExit(f"no libcrtmedia shared library target in {build} (looked for {OUTPUT_CANDIDATES[target_os]})")


def build_kit(build: Path, output: str, sdk: Path, name: str, ninja: str = "ninja", kit_root: Path | None = None) -> Path:
    commands = subprocess.run([ninja, "-C", str(build), "-t", "commands", output], check=True,
                              capture_output=True, text=True).stdout
    tokens = link_tokens(commands, output)
    objects, support = {}, {}
    args = rewrite(tokens, build.resolve(), sorted({str(sdk), str(sdk.resolve())}), objects, support)
    kit = (kit_root or sdk) / KIT_RELATIVE / name
    if kit.exists():
        shutil.rmtree(kit)
    (kit / "objects").mkdir(parents=True)
    manifest = {}
    for object_name, source in sorted(objects.items()):
        shutil.copy2(source, kit / "objects" / object_name)
        manifest[object_name] = sha256(source)
    if support:
        (kit / "support").mkdir()
        for support_name, source in support.items():
            shutil.copy2(source, kit / "support" / support_name)
    (kit / "objects" / "MANIFEST.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    recipe = {"format": 1, "name": name, "output": Path(output).name, "compiler": args[0] if "@SDK@" in args[0] else Path(args[0]).name,
              "ffmpeg_archives": list(FFMPEG_ARCHIVES), "args": args[1:]}
    (kit / "relink.json").write_text(json.dumps(recipe, indent=2) + "\n", encoding="utf-8", newline="\n")
    (kit / "relink.py").write_text(RUNNER.format(name=name, ffmpeg=list(FFMPEG_ARCHIVES)), encoding="utf-8", newline="\n")
    (kit / "README.txt").write_text(README.format(name=name, output=Path(output).name), encoding="utf-8", newline="\n")
    return kit


def check_kit(sdk: Path, name: str, kit_root: Path | None = None) -> list:
    """Problems with an installed kit (empty list = fine). Used by verify_dist."""
    kit = (kit_root or sdk) / KIT_RELATIVE / name
    problems = []
    for relative in ("relink.json", "relink.py", "README.txt", "objects/MANIFEST.json"):
        if not (kit / relative).is_file():
            problems.append(f"{(KIT_RELATIVE / name / relative).as_posix()} is missing")
    if problems:
        return problems
    recipe = json.loads((kit / "relink.json").read_text(encoding="utf-8"))
    manifest = json.loads((kit / "objects" / "MANIFEST.json").read_text(encoding="utf-8"))
    if not manifest:
        problems.append("the relink kit has no objects")
    for object_name, digest in manifest.items():
        path = kit / "objects" / object_name
        if not path.is_file() or sha256(path) != digest:
            problems.append(f"relink kit object {object_name} is missing or differs from its manifest")
    for item in [recipe["compiler"]] + recipe["args"]:
        if "@OBJ@/" in item and Path(item.split("@OBJ@/", 1)[1]).name not in manifest:
            problems.append(f"relink.json names an object that is not in the kit: {item}")
        if "@SDK@/" in item and not (sdk / item.split("@SDK@/", 1)[1]).is_file():
            problems.append(f"relink.json names an SDK file that is not installed: {item}")
        if "@SUPPORT@/" in item and not (kit / "support" / Path(item).name).is_file():
            problems.append(f"relink.json names a support file that is not in the kit: {item}")
    for archive in recipe["ffmpeg_archives"]:
        if not (sdk / "lib" / archive).is_file():
            problems.append(f"the FFmpeg archive lib/{archive} is not installed")
    return problems


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output", required=True, help="the linked file as ninja names it, e.g. bin/libcrtmedia.dll")
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--name", default="libcrtmedia")
    parser.add_argument("--ninja", default="ninja")
    parser.add_argument("--kit-root", type=Path, help="write the kit under this directory instead of the SDK (tests)")
    args = parser.parse_args(argv)
    kit = build_kit(args.build_dir, args.output, args.sdk, args.name, args.ninja, args.kit_root)
    problems = check_kit(args.sdk, args.name, args.kit_root)
    if problems:
        print("\n".join(problems), file=sys.stderr)
        return 1
    print(f"relink kit: {kit}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
