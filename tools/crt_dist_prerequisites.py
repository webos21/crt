#!/usr/bin/env python3
"""Canonical external runtime prerequisites for cumulative CRT stages."""

from copy import deepcopy


STAGE_ORDER = {
    "01-c": 1,
    "02-cxx": 2,
    "03-gfx-simple": 3,
    "04-gfx-media": 4,
    "05-js": 5,
}


_BASE = {
    "windows": ({
        "id": "windows-system-runtime",
        "kind": "os-runtime",
        "required_for": ["base-runtime"],
        "components": [
            "KERNEL32.dll",
            "api-ms-win-core-synch-l1-2-0.dll",
        ],
        "bundled": False,
    },),
    "macos": ({
        "id": "macos-system-runtime",
        "kind": "os-runtime",
        "required_for": ["base-runtime"],
        "components": ["libSystem.B.dylib"],
        "bundled": False,
    },),
    "linux": ({
        "id": "linux-kernel-runtime",
        "kind": "os-runtime",
        "required_for": ["base-runtime"],
        "components": ["Linux kernel syscall ABI"],
        "bundled": False,
    },),
}

_CXX = {
    "windows": (),
    "macos": (),
    "linux": ({
        "id": "linux-libatomic-runtime",
        "kind": "host-library",
        "required_for": ["base-runtime"],
        # libatomic.so.1 (2026-09-14, Linux ELF dependency acceptance): a
        # real, confirmed DT_NEEDED entry of libc++.so/libc++.so.1 (`readelf
        # -d` shows it directly, ahead of even libc++abi.so.1) from the
        # earliest stage libc++ exists -- 02-cxx -- onward, so every later
        # cumulative stage (03-gfx-simple, 04-gfx-media, ...) inherits it
        # too. Clang's libc++ uses out-of-line atomic operations for wide
        # (e.g. 16-byte) values on aarch64 and links libatomic.so.1 to
        # provide them; unlike CoreFoundation.framework on macOS, this is
        # not conditional on any of this project's own link flags -- it is
        # baked into libc++'s own build. Found the same way as the macOS
        # CoreFoundation.framework gap: regenerating a fresh SDK (02-cxx
        # through 03-gfx-simple) and diffing its actual binaries' NEEDED
        # entries against this file's own canonical list.
        "components": ["libatomic.so.1"],
        "bundled": False,
    },),
}

_GFX_SIMPLE = {
    "windows": ({
        "id": "windows-desktop-graphics-runtime",
        "kind": "os-runtime",
        "required_for": ["window-system", "software-framebuffer-presentation"],
        "components": ["USER32.dll", "d3d11.dll", "dxgi.dll"],
        "bundled": False,
    },),
    "macos": ({
        "id": "macos-cocoa-window-runtime",
        "kind": "os-framework",
        "required_for": ["window-system", "software-framebuffer-presentation"],
        "components": [
            "Foundation.framework",
            "AppKit.framework",
            "QuartzCore.framework",
            "CoreGraphics.framework",
            # CoreFoundation.framework (2026-09-14, macOS Mach-O dependency
            # acceptance): not one of libcrtgfx's own explicit CRTGFX_MACOS_
            # FRAMEWORKS link flags, but a real, confirmed LC_LOAD_DYLIB
            # entry (not weak) in libcrtgfx.dylib, crtgfx_window_demo, and
            # crtgfx_gpu_window_demo -- `otool -l` shows it as an ordinary
            # required load command. Apple's own linker records it directly
            # because Foundation.framework itself is built on top of
            # CoreFoundation; this project's own window/GPU code never
            # calls a CoreFoundation API directly. Found regenerating a
            # fresh 03-gfx-simple SDK and comparing its actual binaries'
            # dependencies against this file's own canonical list --
            # exactly the gap the comparison was meant to catch.
            "CoreFoundation.framework",
            "libobjc.A.dylib",
        ],
        "bundled": False,
    },),
    "linux": (
        {
            "id": "linux-wayland-compositor",
            "kind": "runtime-service",
            "required_for": ["window-system", "software-framebuffer-presentation"],
            "components": ["Wayland compositor with xdg-shell support"],
            "bundled": False,
        },
        {
            # Moved here from _GFX_MEDIA (2026-09-15, real Linux/aarch64
            # binary-dependency-gate acceptance): a real, confirmed gap the
            # new cumulative `crt_binary_dependencies.py` gate caught the
            # first time it actually ran a native Linux 03-gfx-simple
            # package on this host -- `lib/libcrtgfx.so` and
            # `examples/bin/crtgfx_window_demo` both carry a genuine
            # DT_NEEDED on the real host `libwayland-client.so.0` already
            # at this stage (this project's own hybrid window-management-
            # via-real-Wayland design starts at 03-gfx-simple, not
            # 04-gfx-media), but this component was declared only under
            # _GFX_MEDIA below, one stage too late. `external_
            # prerequisites_for()` builds the cumulative list by stage, so
            # 04-gfx-media still inherits it correctly from here -- do not
            # also list it under _GFX_MEDIA, or its `id` would be
            # declared twice.
            "id": "linux-wayland-client-runtime",
            "kind": "host-library",
            "required_for": ["gpu-presentation"],
            "components": ["libwayland-client.so.0"],
            "bundled": False,
        },
    ),
}

_GFX_MEDIA = {
    "windows": (
        {
            "id": "windows-d3d12-runtime",
            "kind": "os-runtime",
            "required_for": ["gpu-rendering"],
            "components": ["d3d12.dll", "dxgi.dll", "D3DCOMPILER_47.dll"],
            "bundled": False,
        },
        {
            "id": "windows-com-runtime",
            "kind": "os-runtime",
            "required_for": ["media"],
            "components": ["ole32.dll"],
            "bundled": False,
        },
        {
            "id": "windows-gpu-driver",
            "kind": "device-driver",
            "required_for": ["gpu-rendering", "hardware-video"],
            "components": ["D3D11/D3D12-compatible display driver"],
            "bundled": False,
        },
    ),
    "macos": (
        {
            "id": "macos-metal-runtime",
            "kind": "os-framework",
            "required_for": ["gpu-rendering"],
            "components": ["Metal.framework"],
            "bundled": False,
        },
        {
            "id": "macos-media-runtime",
            "kind": "os-framework",
            "required_for": ["media", "hardware-video"],
            "components": [
                "AudioToolbox.framework",
                "VideoToolbox.framework",
                "CoreVideo.framework",
                "CoreMedia.framework",
            ],
            "bundled": False,
        },
    ),
    "linux": (
        # linux-wayland-client-runtime moved to _GFX_SIMPLE above
        # (2026-09-15) -- already inherited here via the cumulative
        # external_prerequisites_for() chain; see that entry's own
        # comment for why.
        {
            "id": "linux-vulkan-loader",
            "kind": "host-library",
            "required_for": ["gpu-rendering"],
            "components": ["libvulkan.so.1"],
            "bundled": False,
        },
        {
            "id": "linux-vulkan-driver",
            "kind": "device-driver",
            "required_for": ["gpu-rendering"],
            "components": ["Vulkan ICD for the target GPU"],
            "bundled": False,
        },
    ),
}


def external_prerequisites_for(target_os: str, stage: str) -> list[dict]:
    """Return the exact cumulative external-runtime contract for one SDK."""
    if target_os not in _BASE:
        raise ValueError(f"unsupported prerequisite target OS: {target_os}")
    if stage not in STAGE_ORDER:
        raise ValueError(f"unsupported prerequisite stage: {stage}")
    prerequisites = list(_BASE[target_os])
    if STAGE_ORDER[stage] >= STAGE_ORDER["02-cxx"]:
        prerequisites.extend(_CXX[target_os])
    if STAGE_ORDER[stage] >= STAGE_ORDER["03-gfx-simple"]:
        prerequisites.extend(_GFX_SIMPLE[target_os])
    if STAGE_ORDER[stage] >= STAGE_ORDER["04-gfx-media"]:
        prerequisites.extend(_GFX_MEDIA[target_os])
    return deepcopy(prerequisites)
