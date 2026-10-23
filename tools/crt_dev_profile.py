"""Development SDK packaging profile, not the future general CLI build contract."""
import hashlib
import json
from pathlib import Path
import re
import struct

SIDECAR = "share/crt/contracts/development.json"
PORTS = {"zlib", "mbedtls", "curl"}
STATIC = {"zlib": ("lib/libz.a",), "curl": ("lib/libcurl.a",),
          "mbedtls": ("lib/libmbedtls.a", "lib/libmbedx509.a", "lib/libmbedcrypto.a")}
SHARED = {"zlib": ("libz",), "curl": ("libcurl",),
          "mbedtls": ("libmbedtls", "libmbedx509", "libmbedcrypto")}


def consumer_options(target_os):
    # Match libcrtmedia's existing POSIX curl-header lane; no host winsock
    # headers are part of this Bionic-compatible SDK's public include path.
    return ["-U_WIN32", "-U_WIN32_WCE", "-U__WIN32__", "-UWIN32", "-U__MINGW32__"] if target_os == "windows" else []


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            value.update(chunk)
    return value.hexdigest()


def runtime_digests(sdk):
    result = {}
    for directory in ("lib", "bin"):
        for path in sorted((sdk / directory).glob("*")):
            if path.is_file() and (re.match(r"^lib(?:c\+\+abi|c\+\+|unwind|dl|c|m)(?:[._-]|$)", path.name)
                    or path.name.startswith("crt") and path.suffix == ".o"
                    or path.name in ("dllcrt.o", "libclang_rt.builtins.a")):
                result[path.relative_to(sdk).as_posix()] = digest(path)
    return result


def is_shared_runtime(path, target_os):
    if not path.is_file():
        return False
    if target_os == "windows":
        # zlib deliberately retains its POSIX SONAME spelling on Windows.
        # Test PE's DLL characteristic, not a filename suffix or import archive.
        with path.open("rb") as stream:
            header = stream.read(64)
            if header[:2] != b"MZ" or len(header) < 64:
                return False
            stream.seek(struct.unpack_from("<I", header, 60)[0])
            coff = stream.read(24)
        return len(coff) == 24 and coff[:4] == b"PE\0\0" and bool(struct.unpack_from("<H", coff, 22)[0] & 0x2000)
    return (".dylib" in path.name if target_os == "macos" else ".so" in path.name)


def check(sdk, manifest):
    problems = []
    try:
        profile = json.loads((sdk / SIDECAR).read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return ["Development SDK profile sidecar missing or invalid"]
    if (profile.get("format") != 1 or profile.get("profile") != "dev" or manifest.get("stage") != "02-cxx"
            or profile.get("target") != manifest.get("target")
            or profile.get("version") != (sdk / "VERSION").read_text().strip()):
        problems.append("Development SDK profile/manifest/version identity mismatch")
    if profile.get("tls_trust") != {"mode": "explicit-ca-file", "bundled": False}:
        problems.append("Development SDK must require consumer-supplied explicit CA trust")
    if profile.get("consumer_compile_options") != consumer_options(manifest.get("target", {}).get("os")):
        problems.append("Development SDK missing the SDK-owned portable networking compile options")
    observed = runtime_digests(sdk)
    if not observed or profile.get("runtime_sha256") != observed:
        problems.append("Development SDK inherited runtime digest mismatch")
    dependencies = {entry.get("name"): entry for entry in manifest.get("redistributed_dependencies", [])}
    for name in PORTS:
        entry = dependencies.get(name, {})
        if not entry.get("link_artifacts") or not entry.get("runtime_artifacts"):
            problems.append(f"Development SDK missing static/shared {name} dependency")
        if any(value not in entry.get("link_artifacts", []) or not (sdk / value).is_file() for value in STATIC[name]):
            problems.append(f"Development SDK missing {name} static archive")
        for stem in SHARED[name]:
            if not any(re.match(re.escape(stem) + r"[.\-]", Path(value).name)
                       and is_shared_runtime(sdk / value, manifest.get("target", {}).get("os"))
                       for value in entry.get("runtime_artifacts", [])):
                problems.append(f"Development SDK missing {name} shared runtime: {stem}")
    for name in ("ffmpeg", "skia", "freetype", "lvgl", "webkit"):
        if name in dependencies:
            problems.append(f"Development SDK contains forbidden dependency {name}")
    for relative in ("include/crtgfx", "include/crtmedia", "include/crtui", "include/crtweb", "libexec/crtweb"):
        if (sdk / relative).exists():
            problems.append(f"Development SDK contains upper-stage content: {relative}")
    for directory in ("lib", "bin"):
        if any(re.match(r"^(lib)?(crtgfx|crtmedia|crtui|crtweb|skia|avcodec|avformat|swresample|avutil)", p.name)
               for p in (sdk / directory).glob("*")):
            problems.append("Development SDK contains upper-stage libraries")
    return problems
