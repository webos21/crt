#!/usr/bin/env python3
"""Run an installed native browser and observe its owned CPU browser roles.

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
    if sys.platform == "darwin":
        snapshot = subprocess.run(["ps", "-wwaxo", "pid=,ppid=,lstart=,comm="],
                                  check=True, capture_output=True, text=True)
        result = {}
        for line in snapshot.stdout.splitlines():
            fields = line.split(None, 7)
            if len(fields) == 8 and fields[0].isdigit() and fields[1].isdigit():
                result[int(fields[0])] = (int(fields[1]), " ".join(fields[2:7]), Path(fields[7]).name)
        return result
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


def windows_native():
    """Read native process/module snapshots; never inspect unrelated modules."""
    import ctypes as c
    from ctypes import wintypes as w
    class Process(c.Structure):
        _fields_ = [("dwSize", w.DWORD), ("cntUsage", w.DWORD), ("pid", w.DWORD), ("heap", c.c_size_t),
                    ("module", w.DWORD), ("threads", w.DWORD), ("parent", w.DWORD), ("priority", w.LONG),
                    ("flags", w.DWORD), ("name", w.WCHAR * 260)]
    class Module(c.Structure):
        _fields_ = [("dwSize", w.DWORD), ("module", w.DWORD), ("pid", w.DWORD), ("global_refs", w.DWORD),
                    ("process_refs", w.DWORD), ("base", c.c_void_p), ("size", w.DWORD), ("handle", w.HMODULE),
                    ("name", w.WCHAR * 256), ("path", w.WCHAR * 260)]
    kernel = c.WinDLL("kernel32", use_last_error=True)
    for name, arguments, result in (
        ("CreateToolhelp32Snapshot", [w.DWORD, w.DWORD], w.HANDLE),
        ("Process32FirstW", [w.HANDLE, c.POINTER(Process)], w.BOOL),
        ("Process32NextW", [w.HANDLE, c.POINTER(Process)], w.BOOL),
        ("Module32FirstW", [w.HANDLE, c.POINTER(Module)], w.BOOL),
        ("Module32NextW", [w.HANDLE, c.POINTER(Module)], w.BOOL),
        ("OpenProcess", [w.DWORD, w.BOOL, w.DWORD], w.HANDLE),
        ("CloseHandle", [w.HANDLE], w.BOOL),
        ("GetProcessTimes", [w.HANDLE] + [c.POINTER(w.FILETIME)] * 4, w.BOOL),
        ("TerminateProcess", [w.HANDLE, w.UINT], w.BOOL),
    ):
        function = getattr(kernel, name)
        function.argtypes, function.restype = arguments, result
    def snapshot(pid=None):
        handle = kernel.CreateToolhelp32Snapshot(0x18 if pid is not None else 2, pid or 0)
        if handle == c.c_void_p(-1).value:
            raise c.WinError(c.get_last_error())
        item = Module() if pid is not None else Process()
        item.dwSize = c.sizeof(item)
        first, following = ((kernel.Module32FirstW, kernel.Module32NextW) if pid is not None else
                            (kernel.Process32FirstW, kernel.Process32NextW))
        result = [] if pid is not None else {}
        try:
            if not first(handle, c.byref(item)):
                raise c.WinError(c.get_last_error())
            while True:
                if pid is not None:
                    result.append(item.path)
                else:
                    result[item.pid] = (item.parent, item.name)
                if not following(handle, c.byref(item)):
                    if c.get_last_error() != 18:  # ERROR_NO_MORE_FILES
                        raise c.WinError(c.get_last_error())
                    break
        finally:
            kernel.CloseHandle(handle)
        return result
    def identity(pid, terminate=False):
        handle = kernel.OpenProcess(0x1000 | (1 if terminate else 0), False, pid)
        if not handle:
            return None
        times = [w.FILETIME() for _ in range(4)]
        try:
            if not kernel.GetProcessTimes(handle, *[c.byref(item) for item in times]):
                raise c.WinError(c.get_last_error())
            return (times[0].dwHighDateTime << 32) | times[0].dwLowDateTime
        finally:
            kernel.CloseHandle(handle)
    def terminate(pid, expected):
        handle = kernel.OpenProcess(0x1001, False, pid)
        if not handle:
            return
        times = [w.FILETIME() for _ in range(4)]
        try:
            if (kernel.GetProcessTimes(handle, *[c.byref(item) for item in times]) and
                ((times[0].dwHighDateTime << 32) | times[0].dwLowDateTime) == expected):
                kernel.TerminateProcess(handle, 1)
        finally:
            kernel.CloseHandle(handle)
    return snapshot, identity, terminate


def observe_windows(sdk, work, program, env):
    snapshot, identity, terminate = windows_native()
    checkpoint = work / "map-checkpoint"
    ready, release = Path(str(checkpoint) + ".ready"), Path(str(checkpoint) + ".release")
    for file in (ready, release):
        file.unlink(missing_ok=True)
    env = dict(env, CRTWEB_SAMPLE_MAP_CHECKPOINT=str(checkpoint))
    roles = {"crtweb_host.exe": "HostProcess", "WebProcess.exe": "WebProcess", "NetworkProcess.exe": "NetworkProcess"}
    owned, mapped, images, os_maps, violations = {}, set(), {}, set(), set()
    verified = False
    system = Path(os.environ.get("SystemRoot", r"C:\Windows")).resolve()
    with (work / "stdout.log").open("w") as stdout, (work / "stderr.log").open("w") as stderr:
        process = subprocess.Popen([str(program), "--smoke"], cwd=work, env=env, stdout=stdout, stderr=stderr)
        deadline = time.monotonic() + 90
        while process.poll() is None and time.monotonic() < deadline:
            current = snapshot()
            descendants = {process.pid}
            for _ in range(8):
                descendants.update(pid for pid, info in current.items() if info[0] in descendants)
            for pid in descendants - {process.pid}:
                stamp = identity(pid)
                if stamp is not None:
                    owned[pid] = (stamp, current[pid][1])
            if ready.exists():
                verified = ready.read_text().strip() == str(process.pid)
                break
            time.sleep(0.02)
        if verified:
            for pid, (stamp, name) in owned.items():
                role = roles.get(name)
                if role is None or identity(pid) != stamp:
                    continue
                try:
                    paths = snapshot(pid)
                except OSError as error:
                    violations.add(f"{role}: module snapshot failed: {error}")
                    continue
                for path in paths:
                    file = Path(path).resolve()
                    mapped.add((role, str(file)))
                    if file.is_relative_to(system) and file.suffix.lower() == ".dll":
                        os_maps.add((role, str(file)))
                    elif not file.is_relative_to(sdk):
                        violations.add(f"{role}: {path}")
                    for family in ("libc++", "libc++abi", "libunwind"):
                        if file.name.lower() == family + ".dll":
                            images.setdefault((role, family), set()).add(str(file).lower())
        release.write_text("release\n")
        try:
            status = process.wait(timeout=30)
            timed_out = False
        except subprocess.TimeoutExpired:
            process.kill()
            status, timed_out = process.wait(), True
    observed = sorted({role for role, _ in mapped})
    for role in observed:
        for family in ("libc++", "libc++abi", "libunwind"):
            if len(images.get((role, family), set())) != 1:
                violations.add(f"{role}: expected one {family} image")
    remaining = {}
    for _ in range(100):
        remaining = {pid: data for pid, data in owned.items() if identity(pid) == data[0]}
        if not remaining:
            break
        time.sleep(0.02)
    for pid, (stamp, _) in remaining.items():
        terminate(pid, stamp)  # failure cleanup only, never accepted as a pass
    text = (work / "stdout.log").read_text()
    result = {"passed": verified and status == 0 and not timed_out and not violations and not remaining
              and observed == ["HostProcess", "NetworkProcess", "WebProcess"]
              and "crtweb_browser: load=pass pixels=pass script=pass" in text,
              "checkpoint_verified": verified, "returncode": status, "timed_out": timed_out,
              "roles": observed, "violations": sorted(violations), "remaining_owned_processes": sorted(remaining),
              "mapped_files": len(mapped), "single_cxx_runtime_per_role": not any("image" in value for value in violations),
              "os_transitive_mappings": [dict(role=role, path=path) for role, path in sorted(os_maps)]}
    (work / "maps.log").write_text("".join(f"MAPPED {role} {file}\n" for role, file in sorted(mapped)))
    (work / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({key: value for key, value in result.items() if key != "os_transitive_mappings"}, indent=2))
    return 0 if result["passed"] else 1


def observe_macos(sdk, work, program, env, font_cache):
    """Observe checkpointed native processes, with start-time-guarded cleanup."""
    from build_webkit_crt_port import macos_mapped_files
    checkpoint = work / "map-checkpoint"
    ready, release = Path(str(checkpoint) + ".ready"), Path(str(checkpoint) + ".release")
    for file in (ready, release):
        file.unlink(missing_ok=True)
    env = dict(env, CRTWEB_SAMPLE_MAP_CHECKPOINT=str(checkpoint))
    roles = {"crtweb_host": "HostProcess", "WebProcess": "WebProcess", "NetworkProcess": "NetworkProcess"}
    owned, mapped, images, os_maps, violations = {}, set(), {}, set(), set()
    anonymous = []
    verified = False
    with (work / "stdout.log").open("w") as stdout, (work / "stderr.log").open("w") as stderr:
        process = subprocess.Popen([str(program), "--smoke"], cwd=work, env=env, stdout=stdout, stderr=stderr)
        deadline = time.monotonic() + 90
        while process.poll() is None and time.monotonic() < deadline:
            snapshot = processes()
            descendants = {process.pid}
            for _ in range(8):
                descendants.update(pid for pid, info in snapshot.items() if info[0] in descendants)
            for pid in descendants - {process.pid}:
                if pid in snapshot:
                    owned[pid] = snapshot[pid][1]
            if ready.exists():
                try:
                    verified = ready.read_text().strip() == str(process.pid)
                except OSError:
                    pass
                break
            time.sleep(0.02)
        if verified:
            for pid, start in owned.items():
                info = snapshot.get(pid)
                role = roles.get(info[2]) if info else None
                if not role or info[1] != start:
                    continue
                log = work / f"vmmap-{role}-{pid}.log"
                try:
                    ipc = []
                    paths = macos_mapped_files(pid, log, ipc)
                    anonymous.extend(dict(role=role, path=path) for path in ipc)
                except (OSError, subprocess.TimeoutExpired) as error:
                    violations.add(f"{role}: vmmap failed: {error}")
                    continue
                regions = re.findall(r"^.+?\s+[0-9a-fA-F]+-[0-9a-fA-F]+\s+\[[^]]+\]\s+"
                                     r"([rwx-]{3})/[rwx-]{3}\s+.*?\s(/.*)$", log.read_text(), re.M)
                cache_data = {Path(path).resolve() for permissions, path in regions
                              if allowed_cache_mapping(Path(path).resolve(), permissions, font_cache)}
                executable = {Path(path).resolve() for permissions, path in regions if "x" in permissions}
                cache_data -= executable
                for path in paths:
                    file = Path(path).resolve()
                    mapped.add((role, str(file)))
                    if file.suffix.lower() in (".ttf", ".otf", ".ttc", ".dfont") and not file.is_relative_to(sdk):
                        violations.add(f"{role}: host font: {path}")
                        continue
                    # The dyld shared cache is an OS/framework transitive
                    # closure, not the CRT runtime. Direct dependencies remain
                    # subject to the separate strict Mach-O Host ABI audit.
                    if path.startswith(("/usr/lib/", "/System/", "/usr/share/", "/Library/Apple/", "/private/var/db/", "/usr/libexec/")):
                        os_maps.add((role, path))
                        continue
                    if not file.is_relative_to(sdk) and file not in cache_data:
                        violations.add(f"{role}: {path}")
                    for family in ("libc++", "libc++abi", "libunwind"):
                        if re.fullmatch(re.escape(family) + r"(?:\.\d+)*\.dylib", file.name) and file.is_relative_to(sdk):
                            stat = file.stat()
                            images.setdefault((role, family), set()).add((stat.st_dev, stat.st_ino))
        release.write_text("release\n")
        try:
            status = process.wait(timeout=30)
            timed_out = False
        except subprocess.TimeoutExpired:
            process.kill()
            status = process.wait()
            timed_out = True
    observed_roles = sorted({role for role, _ in mapped})
    for role in observed_roles:
        # Darwin's existing runtime contract uses libSystem's unwinder; only
        # libc++/libc++abi are separately distributed CRT images on this host.
        for family in ("libc++", "libc++abi"):
            if len(images.get((role, family), set())) != 1:
                violations.add(f"{role}: expected one {family} image")
    for _ in range(100):
        snapshot = processes()
        remaining = {pid: start for pid, start in owned.items() if pid in snapshot and snapshot[pid][1] == start}
        if not remaining:
            break
        time.sleep(0.02)
    for pid, start in remaining.items():
        snapshot = processes()
        if pid in snapshot and snapshot[pid][1] == start:
            try:
                os.kill(pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
    text = (work / "stdout.log").read_text()
    result = {"passed": verified and status == 0 and not timed_out and not violations and not remaining
              and observed_roles == ["HostProcess", "NetworkProcess", "WebProcess"]
              and "crtweb_browser: load=pass pixels=pass script=pass" in text,
              "checkpoint_verified": verified, "returncode": status, "timed_out": timed_out,
              "roles": observed_roles, "violations": sorted(violations),
              "remaining_owned_processes": sorted(remaining), "mapped_files": len(mapped),
              "single_cxx_runtime_per_role": not any("image" in value for value in violations),
              "anonymous_shared_memory": anonymous,
              "os_transitive_mappings": [dict(role=role, path=path) for role, path in sorted(os_maps)]}
    (work / "maps.log").write_text("".join(f"MAPPED {role} {file}\n" for role, file in sorted(mapped)))
    (work / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    return 0 if result["passed"] else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--program", type=Path, help="external rebuilt sample, instead of the installed one")
    args = parser.parse_args()
    if sys.platform not in ("linux", "darwin", "win32"):
        raise ValueError("native browser observer requires Linux, macOS or Windows")
    sdk, work = args.sdk_root.resolve(), args.output.resolve()
    work.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ)
    for name in ("LD_LIBRARY_PATH", "LD_PRELOAD", "DYLD_LIBRARY_PATH", "DYLD_INSERT_LIBRARIES", "CRTWEB_PROCESS_DIR", "FONTCONFIG_FILE"):
        env.pop(name, None)
    windows = sys.platform == "win32"
    env["CRTWEB_HOST"] = str(sdk / ("libexec/crtweb/crtweb_host.exe" if windows else "libexec/crtweb/crtweb_host"))
    font_config, font_cache = observer_font_config(sdk, work)
    if windows:
        from build_webkit_crt_port import crt_path
        tree = ET.parse(font_config)
        tree.find("dir").text = crt_path(sdk / "share/fonts/crtweb")
        tree.find("cachedir").text = crt_path(font_cache)
        tree.write(font_config, encoding="unicode")
        env["FONTCONFIG_FILE"] = crt_path(font_config)
        # Native PE search policy, package-owned paths only, no build prefix.
        env["PATH"] = str(Path(os.environ.get("SystemRoot", r"C:\Windows")) / "System32")
    else:
        env["FONTCONFIG_FILE"] = str(font_config)
    program = args.program.resolve() if args.program else sdk / ("examples/bin/crtweb_browser.exe" if windows else "examples/bin/crtweb_browser")
    if windows:
        return observe_windows(sdk, work, program, env)
    if sys.platform == "darwin":
        return observe_macos(sdk, work, program, env, font_cache)
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
