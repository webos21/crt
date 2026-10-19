#!/usr/bin/env python3
"""Windows host ICU pkgdata -> CRT target compiler configuration boundary.

ICU's native MinGW pkgdata executes its compiler commands through Git Bash,
not CRT mksh. Derive an OWNED options file with physical CRT launcher paths;
never edit the upstream source or generated options file, nor select host gcc
for the target data library. This is a source-stage build-host tool only.
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


COMMANDS = {"COMPILE", "GENLIB", "AR", "RANLIB"}


def write_owned_text(path, value):
    # Evidence hashes describe bytes, not a platform-translated CRLF variant.
    path.write_text(value, encoding="utf-8", newline="\n")


def native_arguments(arguments):
    # pkgdata embeds these paths in Bash command strings; native backslashes
    # would be consumed as shell escapes. Do not rewrite arbitrary flags/data.
    return [value.replace("\\", "/") if re.match(r"^[A-Za-z]:[/\\]", value) else value for value in arguments]


def native_config(text, sdk, relays):
    old = "/system/bin/mksh /tools/"
    seen, lines = set(), []
    for line in text.splitlines():
        key, separator, value = line.partition("=")
        if key in COMMANDS:
            if key in seen or not separator or not value.startswith(old):
                raise ValueError(f"unrecognized/duplicate ICU target compiler command: {key}")
            tool = value[len(old):].split()[0]
            if tool not in ("crt-cc", "crt-c++", "crt-native-tool"):
                raise ValueError(f"not a CRT target compiler/tool: {tool}")
            value = (shlex.quote((sdk / "system/bin/mksh.exe").as_posix()) + " " +
                     shlex.quote((relays / (tool + ".sh")).as_posix()) + value[len(old) + len(tool):])
            line = key + "=" + value
            seen.add(key)
        lines.append(line)
    if seen != COMMANDS:
        raise ValueError("incomplete ICU target compiler options")
    return "\n".join(lines) + "\n"


def install_shared_data(arguments, configuration):
    """Complete MinGW pkgdata's omitted shared -I step, not a source patch.

    Static installation works upstream. In shared mode its Win32-only branch
    returns after linking, leaving make install's stub DLL in the target bin.
    Copy only the explicitly named real DLL and its generated import archive.
    """
    def option(name):
        matches = [arguments[index + 1] for index, value in enumerate(arguments[:-1]) if value == name]
        if len(matches) != 1:
            raise ValueError(f"shared ICU install requires one {name}")
        return matches[0]
    if "-I" not in arguments or option("-m") != "dll":
        return {}
    name, revision = option("-L"), option("-r").split(".", 1)[0]
    if not re.fullmatch(r"[A-Za-z0-9_-]+", name) or not revision.isdigit():
        raise ValueError("unsafe ICU library name/version")
    fields = dict(line.split("=", 1) for line in configuration.splitlines() if "=" in line)
    if fields.get("SO") != "dll" or fields.get("LIBPREFIX") not in ("", "lib"):
        raise ValueError("unexpected target ICU DLL naming")
    directory = Path(option("-d")).resolve()
    destination = Path(option("-I")).resolve()
    source = directory / (fields["LIBPREFIX"] + name + revision + ".dll")
    with source.open("rb") as stream:
        if stream.read(2) != b"MZ":
            raise ValueError("target data DLL is not PE")
    destination.mkdir(parents=True, exist_ok=True)
    copies = [(source, destination / source.name)]
    archive = directory / ("lib" + name + ".dll.a")
    if destination.name.lower() == "bin":
        if not archive.is_file():
            raise ValueError("generated target data import archive missing")
        copies.append((archive, destination.parent / "lib" / archive.name))
    installed = {}
    for original, copied in copies:
        copied.parent.mkdir(parents=True, exist_ok=True)
        if original != copied:
            shutil.copy2(original, copied)
        source_hash = hashlib.sha256(original.read_bytes()).hexdigest()
        if hashlib.sha256(copied.read_bytes()).hexdigest() != source_hash:
            raise ValueError("target ICU data installation changed bytes")
        installed[str(copied)] = source_hash
    return installed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("sdk", "host-program", "shell-dir", "compiler-dir"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("arguments", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if os.name != "nt":
        raise ValueError("native Windows host pkgdata only")
    arguments = native_arguments(args.arguments[1:] if args.arguments[:1] == ["--"] else args.arguments[:])
    indices = [index for index, value in enumerate(arguments) if value == "-O"]
    if len(indices) != 1 or indices[0] + 1 >= len(arguments):
        raise ValueError("pkgdata requires exactly one explicit target options file")
    index = indices[0] + 1
    source = Path(arguments[index]).resolve()
    original_bytes = source.read_bytes()
    original = original_bytes.decode("utf-8")
    generated = Path.cwd() / ".crt-host-pkgdata"
    generated.mkdir(exist_ok=True)
    sdk = args.sdk.resolve()
    relay_hashes = {}
    for tool in ("crt-cc", "crt-c++", "crt-native-tool"):
        # Git translates PATH for native programs. Restore the CRT namespace
        # inside CRT mksh, rather than leaking host tools into the wrapper.
        body = ("#!/system/bin/mksh\nset -eu\nexport CRT_ROOTFS=" + shlex.quote(sdk.as_posix()) +
                "\nexport CRT_SYSROOT=" + shlex.quote(sdk.as_posix()) +
                '\nPATH=/system/bin:/bin:/usr/bin\nexport PATH\nexec /tools/' + tool + ' "$@"\n')
        write_owned_text(generated / (tool + ".sh"), body)
        relay_hashes[tool] = hashlib.sha256(body.encode()).hexdigest()
    transformed = native_config(original, sdk, generated)
    options = generated / (hashlib.sha256(transformed.encode()).hexdigest()[:16] + ".inc")
    write_owned_text(options, transformed)
    arguments[index] = options.as_posix()
    env = dict(os.environ)
    env["PATH"] = os.pathsep.join((str(args.compiler_dir.resolve()), str(args.shell_dir.resolve()),
                                   str(args.sdk.resolve() / "bin"), env.get("PATH", "")))
    evidence = dict(source=str(source), source_sha256=hashlib.sha256(original_bytes).hexdigest(),
                    generated=str(options), generated_sha256=hashlib.sha256(transformed.encode()).hexdigest(),
                    host_program=str(args.host_program.resolve()), target_commands=sorted(COMMANDS), relays=relay_hashes)
    status = subprocess.run([str(args.host_program.resolve()), *arguments], env=env, check=False).returncode
    evidence["returncode"] = status
    if status == 0:
        evidence["installed_shared_data"] = install_shared_data(arguments, original)
    evidence["source_unchanged"] = source.read_bytes() == original_bytes
    options.with_suffix(".json").write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
    print("CRT host pkgdata: " + json.dumps(evidence), flush=True)
    return status if evidence["source_unchanged"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
