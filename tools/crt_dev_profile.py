"""Development SDK packaging profile, not the future general CLI build contract."""
import hashlib
import json
from pathlib import Path
import re
import struct

SIDECAR = "share/crt/contracts/development.json"
# crt-narrow-encoding-1: crt_get_narrow_encoding() exists and Windows executables linked through the wrappers embed the
# UTF-8 active-code-page manifest (docs/design/windows_narrow_strings.md).
# archive-xz-1: liblzma (static and shared, xz format with CRC64/SHA-256 checks and concatenated streams) beside zlib, so
# a consumer can unpack the tar.xz SDK archives with only SDK-provided code.
# public-header-persona-1: no consumer compile option is needed for the bundled C libraries (`consumer_compile_options` is empty on
# every target); on Windows the headers whose meaning depends on the host platform macros are CRT-owned shims (see PERSONA_HEADERS).
CAPABILITIES = ["crt-platform-queries-1", "json-sha2-1", "crt-narrow-encoding-1", "archive-xz-1", "public-header-persona-1"]
PORTS = {"zlib", "mbedtls", "curl", "cjson", "xz"}
HEADER_PORTS = {"nlohmann-json": "include/nlohmann/json.hpp"}
STATIC = {"zlib": ("lib/libz.a",), "curl": ("lib/libcurl.a",),
          "mbedtls": ("lib/libmbedtls.a", "lib/libmbedx509.a", "lib/libmbedcrypto.a"), "cjson": ("lib/libcjson.a",),
          "xz": ("lib/liblzma.a",)}
SHARED = {"zlib": ("libz",), "curl": ("libcurl",),
          "mbedtls": ("libmbedtls", "libmbedx509", "libmbedcrypto"), "cjson": ("libcjson",), "xz": ("liblzma",)}


def consumer_options(target_os):
    """Compile options every consumer must pass for the bundled libraries: none.

    Earlier Windows profiles returned `-U_WIN32 ...` here. That undefined the platform macros for the whole translation unit, so
    libc++'s `std::filesystem::path` was parsed with `char` while the packaged libc++ is built with `_WIN32` (`wchar_t`): the
    consumer linked and then returned wrong paths. The persona is now scoped to the headers that need it (PERSONA_HEADERS).
    """
    return []


# The C ports are built with the POSIX/Bionic persona (their recipes pass -U_WIN32 ...): sockets are `int`, no winsock, POSIX
# snprintf. libc++ is built with _WIN32 defined. A consumer therefore keeps _WIN32, and the headers of the C ports whose declarations
# change with it are CRT-owned shims that drop the Windows platform macros only while the upstream header is parsed. Only headers whose
# differences affect linking or types are shimmed (measured by preprocessing each public header both ways): curl.h needs winsock2.h and
# SOCKET otherwise, mbedtls/platform.h otherwise refers to mbedtls_snprintf macros the POSIX-built library does not define. zlib.h, lzma.h
# and cJSON.h differ only by an unused wide-character declaration and by calling-convention/dllexport attributes that are ignored on the
# supported x86_64 and aarch64 targets, so they are left unmodified.
PERSONA_MACROS = ("_WIN32", "_WIN32_WCE", "__WIN32__", "WIN32", "__MINGW32__")
PERSONA_HEADERS = {"windows": ("include/curl/curl.h", "include/mbedtls/platform.h")}
SHIM_MARKER = "CRT public-header persona shim 1"


def upstream_name(header):
    path = Path(header)
    return path.with_name(path.stem + "_crt_upstream" + path.suffix).as_posix()


def shim_text(header):
    guard = "CRT_PERSONA_SHIM_" + re.sub(r"[^A-Z0-9]", "_", header.upper())
    push = "".join('#pragma push_macro("%s")\n' % name for name in PERSONA_MACROS)
    undef = "".join("#undef %s\n" % name for name in PERSONA_MACROS)
    pop = "".join('#pragma pop_macro("%s")\n' % name for name in reversed(PERSONA_MACROS))
    return (
        "/* %s: %s\n"
        "   The library behind this header is built with the POSIX/Bionic persona, so the upstream header is parsed without the\n"
        "   Windows platform macros. The consumer keeps them: libc++ (std::filesystem::path, separators) is built for _WIN32.\n"
        "   The upstream header is %s, byte for byte. */\n"
        "#ifndef %s\n#define %s\n"
        "#ifdef __cplusplus\n"
        "#include <version> /* libc++ fixes its platform persona here, while _WIN32 is still defined, before the window below */\n"
        "#endif\n%s%s#include \"%s\"\n%s#endif\n"
    ) % (SHIM_MARKER, header, Path(upstream_name(header)).name, guard, guard, push, undef, Path(upstream_name(header)).name, pop)


def apply_persona_shims(sdk, target_os):
    """Install the shims of one target; idempotent, and refuses a header that is neither upstream nor an exact shim."""
    for header in PERSONA_HEADERS.get(target_os, ()):
        path, upstream = sdk / header, sdk / upstream_name(header)
        text = shim_text(header)
        if upstream.exists():
            if path.read_text(encoding="utf-8") != text:
                raise ValueError("persona shim target is not the expected shim: " + header)
            continue
        if not path.is_file() or SHIM_MARKER in path.read_text(encoding="utf-8", errors="replace"):
            raise ValueError("persona shim input is not an upstream header: " + header)
        path.rename(upstream)
        path.write_text(text, encoding="utf-8", newline="\n")


def persona_records(sdk, target_os):
    """What the profile records about the shims of this SDK; raises when an installed shim is not exactly the expected text."""
    records = []
    for header in PERSONA_HEADERS.get(target_os, ()):
        path, upstream = sdk / header, sdk / upstream_name(header)
        if not path.is_file() or not upstream.is_file() or path.read_text(encoding="utf-8") != shim_text(header):
            raise ValueError("persona shim missing or modified: " + header)
        records.append({"header": header, "upstream": upstream_name(header), "upstream_sha256": digest(upstream),
                        "persona": "posix", "macros": list(PERSONA_MACROS)})
    return records


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
    if profile.get("capabilities") != CAPABILITIES:
        problems.append("Development SDK missing platform-query/JSON/SHA-2/xz capabilities")
    target_os = manifest.get("target", {}).get("os")
    if profile.get("consumer_compile_options") != consumer_options(target_os):
        problems.append("Development SDK consumer compile options must be empty (the platform persona is scoped to shim headers)")
    try:
        if profile.get("public_header_personas") != persona_records(sdk, target_os):
            problems.append("Development SDK public-header persona record does not match the installed shim headers")
    except (OSError, ValueError) as error:
        problems.append("Development SDK public-header persona shims invalid: %s" % error)
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
            pattern = r"(?:lib)?cjson" if name == "cjson" else re.escape(stem)
            if not any(re.match(pattern + r"[.\-]", Path(value).name)
                       and is_shared_runtime(sdk / value, manifest.get("target", {}).get("os"))
                       for value in entry.get("runtime_artifacts", [])):
                problems.append(f"Development SDK missing {name} shared runtime: {stem}")
    for name, header in HEADER_PORTS.items():
        entry = dependencies.get(name, {})
        if not (sdk / header).is_file() or not entry.get("headers") or entry.get("kind") != "header-only-port":
            problems.append(f"Development SDK missing header-only dependency {name}")
    for header in ("include/cJSON.h", "include/mbedtls/sha256.h", "include/mbedtls/sha512.h",
                   "include/crt/platform.h", "include/crt/dl.h", "include/lzma.h"):
        if not (sdk / header).is_file():
            problems.append(f"Development SDK missing public utility/PAL header {header}")
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
