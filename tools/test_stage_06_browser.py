#!/usr/bin/env python3
"""Run the installed Linux browser and observe its owned CPU browser roles.

No build-tree loader path, GPU, audio or camera is used. The application can
map its declared window PAL; Host/Web/Network map SDK files, the OS loader and
run-owned non-executable font-cache data only.
"""
import argparse
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import time
import xml.etree.ElementTree as ET


def observer_font_config(sdk, work):
    """Keep mutable font caches owned by this run, not in the user's home."""
    config = ET.parse(sdk / "share/crt/web-fonts.conf")
    dirs = config.findall("dir")
    caches = config.findall("cachedir")
    if len(dirs) != 1 or len(caches) != 1 or dirs[0].get("prefix") != "relative" or dirs[0].text != "../fonts/crtweb":
        raise ValueError("unexpected packaged font configuration")
    dirs[0].attrib.clear()
    dirs[0].text = str(sdk / "share/fonts/crtweb")
    cache = work / "fontcache"
    cache.mkdir(exist_ok=True)
    caches[0].text = str(cache)
    path = work / "fonts.conf"
    config.write(path, encoding="unicode")
    return path, cache


def allowed_cache_mapping(file, permissions, cache):
    # Fontconfig mmaps non-executable data. This is not permission to load a
    # library from an arbitrary host/cache directory.
    return "x" not in permissions and file.is_relative_to(cache) and ".cache-" in file.name


def anonymous_mapping(path, permissions, scratch):
    if "x" in permissions or not path.endswith(" (deleted)"):
        return False
    if path.startswith("/memfd:"):
        return True
    file = Path(path.removesuffix(" (deleted)"))
    return file.parent.resolve() == scratch and bool(re.fullmatch(
        r"crt_shm_WK2SharedMemory\.\d+|crt_memfd_\d+_\d{6}\.tmp", file.name))


def processes():
    result = {}
    for directory in Path("/proc").iterdir():
        if not directory.name.isdigit():
            continue
        try:
            fields = (directory / "stat").read_text().rsplit(")", 1)[1].split()
            result[int(directory.name)] = (int(fields[1]), fields[19], (directory / "comm").read_text().strip())
        except (OSError, ValueError, IndexError):
            continue
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--program", type=Path, help="external rebuilt sample, instead of the installed one")
    args = parser.parse_args()
    if sys.platform != "linux":
        raise ValueError("native /proc observer requires Linux")
    sdk, work = args.sdk_root.resolve(), args.output.resolve()
    work.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ)
    for name in ("LD_LIBRARY_PATH", "LD_PRELOAD", "CRTWEB_PROCESS_DIR", "FONTCONFIG_FILE"):
        env.pop(name, None)
    env["CRTWEB_HOST"] = str(sdk / "libexec/crtweb/crtweb_host")
    font_config, font_cache = observer_font_config(sdk, work)
    env["FONTCONFIG_FILE"] = str(font_config)
    program = args.program.resolve() if args.program else sdk / "examples/bin/crtweb_browser"
    roles = {"crtweb_host": "HostProcess", "WebProcess": "WebProcess", "NetworkProcess": "NetworkProcess"}
    loaders = {file.resolve() for file in (Path("/lib64/ld-linux-x86-64.so.2"),
                                           Path("/lib/ld-linux-aarch64.so.1")) if file.is_file()}
    scratch = Path(next((env[name] for name in ("TMPDIR", "TEMP", "TMP") if env.get(name)), "/tmp")).resolve()
    anonymous = set()
    owned, observed, violations, images = {}, set(), set(), {}
    with (work / "stdout.log").open("w") as stdout, (work / "stderr.log").open("w") as stderr:
        process = subprocess.Popen([str(program), "--smoke"], cwd=work, env=env, stdout=stdout, stderr=stderr)
        deadline = time.monotonic() + 90
        while process.poll() is None and time.monotonic() < deadline:
            snapshot = processes()
            descendants = {process.pid}
            for _ in range(8):
                descendants.update(pid for pid, info in snapshot.items() if info[0] in descendants)
            for pid in descendants - {process.pid}:
                info = snapshot.get(pid)
                if not info:
                    continue
                owned[pid] = info[1]
                role = roles.get(info[2])
                if not role:
                    continue
                try:
                    lines = Path(f"/proc/{pid}/maps").read_text().splitlines()
                except OSError:
                    continue
                for line in lines:
                    fields = line.split(None, 5)
                    if len(fields) < 6 or not fields[5].startswith("/"):
                        continue
                    path = fields[5]
                    if anonymous_mapping(path, fields[1], scratch):
                        anonymous.add((role, path))
                        continue
                    file = Path(path).resolve()
                    observed.add((role, str(file)))
                    if file in loaders:
                        continue
                    if not file.is_relative_to(sdk) and not allowed_cache_mapping(file, fields[1], font_cache):
                        violations.add(f"{role}: {path}")
                    for family in ("libc++.so", "libc++abi.so", "libunwind.so"):
                        if file.name.startswith(family):
                            images.setdefault((role, family), set()).add((fields[3], fields[4]))
            time.sleep(0.005)
        timed_out = process.poll() is None
        if timed_out:
            process.kill()
        status = process.wait()
    observed_roles = sorted({role for role, _ in observed})
    for role in observed_roles:
        for family in ("libc++.so", "libc++abi.so", "libunwind.so"):
            if len(images.get((role, family), set())) != 1:
                violations.add(f"{role}: expected one {family} image")
    remaining = {}
    for _ in range(100):
        snapshot = processes()
        remaining = {pid: start for pid, start in owned.items() if pid in snapshot and snapshot[pid][1] == start}
        if not remaining:
            break
        time.sleep(0.02)
    # On failure retire only descendants this observer actually owned, with
    # matching start time; never kill a process solely by executable name.
    for pid, start in remaining.items():
        snapshot = processes()
        if pid in snapshot and snapshot[pid][1] == start:
            try:
                os.kill(pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
    text = (work / "stdout.log").read_text()
    result = {"passed": status == 0 and not timed_out and not violations and not remaining
              and observed_roles == ["HostProcess", "NetworkProcess", "WebProcess"]
              and "crtweb_browser: load=pass pixels=pass script=pass" in text,
              "returncode": status, "timed_out": timed_out, "roles": observed_roles,
              "violations": sorted(violations), "remaining_owned_processes": sorted(remaining),
              "mapped_files": len(observed), "single_cxx_runtime_per_role": not any("image" in value for value in violations)}
    result["owned_font_cache"] = str(font_cache)
    result["anonymous_shared_memory"] = [dict(role=role, path=path) for role, path in sorted(anonymous)]
    (work / "maps.log").write_text("".join(f"MAPPED {role} {file}\n" for role, file in sorted(observed)))
    (work / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
