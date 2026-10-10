#!/usr/bin/env python3
"""Build and run the Tranche 3A "crt" WPEPlatform prototype (Linux/x86_64).

Three pieces, in two worlds:

* native (host compiler, GLib, the Tranche 2 WPE reference build): the GIO module
  libcrtweb/platform/wpe-crt (a WPEDisplay named "crt") and a small host program that creates a
  WebKitWebView on it;
* CRT (the installed SDK's crt-cc, no GLib): libcrtweb/tests/platform-crt/surface_probe.c, a consumer
  of the frozen wire protocol (libcrtweb/platform/crtweb_surface_wire.h).

The probe listens, the native host connects; the probe then checks frames (pixels, serials,
acknowledgement/release), drives the page with input, and resizes it. Requires the Tranche 2
reference build (tools/build_webkit_wpe_reference.py) and an installed CRT SDK.
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_webkit_wpe_reference as reference  # noqa: E402  (native env helpers)

PLATFORM = ROOT / "libcrtweb" / "platform"
TESTS = ROOT / "libcrtweb" / "tests" / "platform-crt"


def pkg_flags(build, env, packages):
    pc_env = dict(env)
    pc_env["PKG_CONFIG_PATH"] = os.pathsep.join(filter(None, (str(build), pc_env.get("PKG_CONFIG_PATH", ""))))
    flags = reference.run(["pkg-config", "--cflags", "--libs", *packages], env=pc_env, capture=True).stdout.split()
    if pc_env.get("PKG_CONFIG_SYSROOT_DIR"):
        bad = pc_env["PKG_CONFIG_SYSROOT_DIR"] + str(build)
        flags = [flag.replace(bad, str(build)) for flag in flags]
    return flags, pc_env


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference-work", type=Path, default=ROOT / "out" / "web-reference")
    parser.add_argument("--native-sysroot", type=Path, default=None,
                        help="native dependency lookup root (default: <reference-work>/sysroot if present)")
    parser.add_argument("--sdk-root", type=Path, required=True, help="installed CRT SDK (e.g. out/<preset>/dist/05-ui)")
    parser.add_argument("--work", type=Path, default=ROOT / "out" / "web-crt-platform")
    args = parser.parse_args()
    started = time.monotonic()

    native_sysroot = args.native_sysroot
    if native_sysroot is None and (args.reference_work / "sysroot").is_dir():
        native_sysroot = args.reference_work / "sysroot"
    env = reference.native_environment(native_sysroot)
    build = args.reference_work / "build"
    if not (build / "lib" / "libWPEWebKit-2.0.so").exists():
        raise SystemExit("the Tranche 2 reference build is missing: run tools/build_webkit_wpe_reference.py first")
    args.work.mkdir(parents=True, exist_ok=True)

    # 1. the module and the host (native).
    module_dir = args.work / "modules"
    module_dir.mkdir(exist_ok=True)
    module = module_dir / "libwpe-platform-crt.so"
    flags, pc_env = pkg_flags(build, env, ["wpe-platform-2.0-uninstalled", "gio-2.0", "gmodule-2.0"])
    reference.run(["cc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-shared", "-fPIC", "-O1", "-g",
                   PLATFORM / "wpe-crt" / "wpe_crt_platform.c", "-o", module, *flags], env=pc_env)
    host = args.work / "wpe-crt-host"
    flags, pc_env = pkg_flags(build, env, ["wpe-webkit-2.0-uninstalled"])
    reference.run(["cc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", TESTS / "wpe_crt_host.c", "-o", host, *flags],
                  env=pc_env)

    # 2. the consumer probe (CRT-built).
    probe = args.work / "surface-probe"
    crt_env = dict(os.environ)
    crt_env["CRT_SYSROOT"] = str(args.sdk_root.resolve())
    crt_env["CRT_TARGET_OS"] = "linux"
    reference.run([args.sdk_root / "tools" / "crt-cc", "-std=gnu11", "-Wall", "-Wextra", "-Werror",
                   TESTS / "surface_probe.c", "-o", probe], env=crt_env)

    # 3. run: the probe listens, the host connects.
    socket_path = args.work / "surface.sock"
    run_env = dict(env)
    library_paths = [build / "lib"]
    if env.get("CMAKE_LIBRARY_PATH"):
        library_paths.append(Path(env["CMAKE_LIBRARY_PATH"]))
    run_env.update({
        "LD_LIBRARY_PATH": os.pathsep.join(str(path) for path in library_paths),
        "WEBKIT_EXEC_PATH": str(build / "bin"),
        "WEBKIT_INJECTED_BUNDLE_PATH": str(build / "lib"),
        "WEBKIT_INSPECTOR_RESOURCES_PATH": str(build / "share"),
        "WEBKIT_DISABLE_SANDBOX_THIS_IS_DANGEROUS": "1",
        "GIO_USE_VFS": "local", "GSETTINGS_BACKEND": "memory", "LC_ALL": "C.UTF-8",
        "LIBGL_ALWAYS_SOFTWARE": "1", "MESA_SHADER_CACHE_DISABLE": "true",
        "WPE_DISPLAY": "crt", "WPE_PLATFORMS_PATH": str(module_dir),
        "CRTWEB_SURFACE_SOCKET": str(socket_path),
    })
    if socket_path.exists():
        socket_path.unlink()
    probe_log = (args.work / "probe.log").open("w")
    host_log = (args.work / "host.log").open("w")
    probe_process = subprocess.Popen([str(probe), str(socket_path)], stdout=subprocess.PIPE, text=True)
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline and not socket_path.exists():
        time.sleep(0.05)
    wrapper = os.environ.get("CRTWEB_HOST_WRAPPER", "").split()
    host_process = subprocess.Popen([*wrapper, str(host), str(TESTS / "crt_surface.html")], env=run_env,
                                    stdout=host_log, stderr=subprocess.STDOUT, text=True)
    try:
        output, _ = probe_process.communicate(timeout=180)
    except subprocess.TimeoutExpired:
        probe_process.kill()
        output, _ = probe_process.communicate()
    probe_log.write(output)
    print(output, end="")
    try:
        host_status = host_process.wait(timeout=20)
    except subprocess.TimeoutExpired:
        host_process.kill()
        host_status = host_process.wait()
    probe_ok = probe_process.returncode == 0 and "surface_probe: ok" in output
    print(f"host exit status {host_status}; probe exit status {probe_process.returncode}")
    result = {
        "schema": 1, "status": "pass" if probe_ok else "fail",
        "elapsed_seconds": round(time.monotonic() - started, 1),
        "probe_exit": probe_process.returncode, "host_exit": host_status,
    }
    (args.work / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    if not probe_ok:
        print((args.work / "host.log").read_text(errors="replace")[-3000:])
        raise SystemExit("WPE crt platform prototype: FAIL")
    print("WPE crt platform prototype: PASS")


if __name__ == "__main__":
    main()
