#!/usr/bin/env python3
"""Run the bounded libcrtgfx backend-boundary acceptance tranche.

The calling CMake target builds and verifies a fresh cumulative 02-cxx SDK
before invoking this script.  This runner keeps the remaining checks bounded:
it executes the ABI/ownership/backend tests in the enabled tree, then creates
a clean backend-disabled tree and proves that both static and shared common
GPU wrappers still build and run without a concrete backend object.
"""

import argparse
import shutil
import subprocess
from pathlib import Path


ENABLED_TESTS = (
    "crt_binary_dependencies_unit_runs",
    "crtgfx_gpu_backend_boundary_unit_runs",
    "header_abi_test_runs",
    "host_abi_firewall_test_runs",
    "host_abi_firewall_fault_test_runs",
    "crtgfx_gpu_test_runs",
    "crtgfx_synthetic_event_runs",
)

DISABLED_TESTS = (
    "crtgfx_gpu_backend_boundary_unit_runs",
    "crtgfx_gpu_test_runs",
)


def run(command: list[str], cwd: Path | None = None) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, cwd=cwd, check=True)


def test_regex(names: tuple[str, ...]) -> str:
    return "^(" + "|".join(names) + ")$"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--disabled-build-dir", required=True, type=Path)
    parser.add_argument("--target-os", required=True,
                        choices=("linux", "macos", "windows"))
    parser.add_argument("--target-arch", required=True)
    parser.add_argument("--cmake", required=True)
    parser.add_argument("--ctest", required=True)
    parser.add_argument("--c-compiler", required=True)
    parser.add_argument("--cxx-compiler", required=True)
    args = parser.parse_args()

    root = args.root.resolve()
    build_dir = args.build_dir.resolve()
    disabled = args.disabled_build_dir.resolve()

    # Enabled-tree evidence: the CMake target has already built every binary
    # selected here and crt-libcxx-dist has already run verify_dist.py against
    # dist/02-cxx, including ELF/PE/Mach-O dependency classification.
    run([
        args.ctest, "--test-dir", str(build_dir), "--output-on-failure",
        "-R", test_regex(ENABLED_TESTS),
    ])

    # This must be a genuinely clean configuration.  Reusing a stale cache is
    # precisely how a CRTGFX_HAVE_* definition or backend object could hide a
    # fixed-layout regression.
    if disabled.exists():
        shutil.rmtree(disabled)
    configure = [
        args.cmake, "-G", "Ninja", "-S", str(root), "-B", str(disabled),
        "-DCMAKE_BUILD_TYPE=Debug",
        f"-DCMAKE_C_COMPILER={args.c_compiler}",
        f"-DCMAKE_CXX_COMPILER={args.cxx_compiler}",
        f"-DCRT_TARGET_OS={args.target_os}",
        f"-DCRT_TARGET_ARCH={args.target_arch}",
        "-DCRTGFX_ENABLE_GPU_BACKEND=OFF",
        "-DCRTGFX_ENABLE_SKIA=OFF",
        "-DCRT_USE_IMPORTED_LIBCXX=OFF",
    ]
    if args.target_os == "windows":
        configure.extend((
            "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY",
            "-DCMAKE_MSVC_RUNTIME_LIBRARY=",
            "-DCMAKE_C_STANDARD_LIBRARIES=",
            "-DCMAKE_CXX_STANDARD_LIBRARIES=",
        ))
    run(configure)
    run([
        args.cmake, "--build", str(disabled), "--target",
        "crtgfx_gpu", "crtgfx_gpu_shared", "crtgfx_gpu_test",
    ])
    run([
        args.ctest, "--test-dir", str(disabled), "--output-on-failure",
        "-R", test_regex(DISABLED_TESTS),
    ])
    print("crtgfx boundary acceptance: ok", flush=True)


if __name__ == "__main__":
    main()
