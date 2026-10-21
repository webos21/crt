#!/usr/bin/env python3
"""Run the Tranche 8 device-facing gates against an installed 06-web SDK.

The media playback gate (HTML <video>/<audio> through the CRT playback engine) and the camera capture gate (permission
lifecycle and capture from the labelled synthetic camera; no physical camera is opened) are written against the public
crtweb C API only, so they are built here with the SDK's own wrapper and run with the SDK's own processes -- no
WebKit build directory. The checks and the pass criteria are the driver's (tools/build_webkit_crt_port.py
run_media_gate/run_capture_gate), including the audit that the Web process maps nothing outside the SDK and exactly one C++
runtime. Linux and macOS/Windows hosts use their native toolchain; only Linux has been run.

    test_stage_06_devices.py --sdk-root <06-web SDK> --output <work dir> [--require-audio-output]
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))

import build_webkit_crt_port as driver  # noqa: E402
from test_stage_06_browser import observer_font_config  # noqa: E402

TESTS = ("crtweb_media_test", "crtweb_capture_test")


def build_tests(sdk: Path, build: Path, env: dict) -> None:
    exe = ".exe" if sys.platform == "win32" else ""
    (build / "bin").mkdir(parents=True, exist_ok=True)
    (build / "lib").mkdir(exist_ok=True)
    wrapper = sdk / "tools" / ("crt-c++.cmd" if sys.platform == "win32" else "crt-c++")
    link = [] if sys.platform == "win32" else ["-Wl,-rpath," + str(sdk / "lib")]
    for name in TESTS:
        subprocess.run([str(wrapper), str(ROOT / "libcrtweb/tests/webkit" / (name + ".cpp")),
                        "-I" + str(sdk / "include"), "-L" + str(sdk / "lib"), "-lcrtweb", *link,
                        "-o", str(build / "bin" / (name + exe))], env=env, check=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--sdk-root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--require-audio-output", action="store_true",
                        help="require real PCM delivery and a progressing sink clock (needs a sound device)")
    args = parser.parse_args()
    sdk, work = args.sdk_root.resolve(), args.output.resolve()
    if work.exists():
        shutil.rmtree(work)
    work.mkdir(parents=True)
    manifest = json.loads((sdk / "manifest.json").read_text(encoding="utf-8"))
    target_os = manifest["target"]["os"]
    if target_os != "linux":
        raise SystemExit(f"the installed-SDK device gates were only written and run for Linux, not {target_os}")

    env = dict(os.environ)
    for name in ("LD_LIBRARY_PATH", "LD_PRELOAD", "CRTWEB_PROCESS_DIR", "FONTCONFIG_FILE"):
        env.pop(name, None)
    env.update(CRT_SYSROOT=str(sdk), CRT_ROOTFS=str(sdk), CRT_TARGET_OS=target_os,
               CRT_TARGET_ARCH=manifest["target"]["arch"], CRT_CXX_RUNTIME_LINKAGE="shared", CRT_CXX_HOSTED="1")
    private = sdk / "libexec" / "crtweb"
    env["CRTWEB_PROCESS_DIR"] = str(private)
    env["CRTWEB_HOST"] = str(private / "crtweb_host")
    observer_font_config(sdk, work)  # writes work/fonts.conf, which the driver's gate environment names

    build = work / "build"
    build_tests(sdk, build, env)
    # The driver's loader path would put the build and port directories first; here the SDK is both.
    results = {"media_playback": driver.run_media_gate(build, sdk, sdk, work, env, target_os,
                                                        require_audio_output=args.require_audio_output),
               "camera_capture": driver.run_capture_gate(build, sdk, sdk, work, env, target_os)}
    summary = {name: {"passed": result["passed"], "returncode": result["returncode"]}
               for name, result in results.items()}
    (work / "devices.json").write_text(json.dumps({"summary": summary, "results": results}, indent=2) + "\n",
                                       encoding="utf-8")
    print("device gates against the installed SDK:", json.dumps(summary))
    return 0 if all(item["passed"] for item in summary.values()) else 1


if __name__ == "__main__":
    sys.exit(main())
