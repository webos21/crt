#!/usr/bin/env python3
"""Real SDK-only C++17 HTTPS/inflate consumer; a build-host loopback fixture."""
import argparse
import http.server
import json
import os
from pathlib import Path
import shutil
import ssl
import subprocess
import sys
import tempfile
import threading
import zlib

import audit_macos_bindings as macos_bindings
import crt_binary_dependencies as binaries
from crt_dev_profile import digest
from build_stage_05_ui import runtime_env
from test_crtweb_network import make_pki

ROOT = Path(__file__).resolve().parent.parent


def target_path(path, windows):
    text = path.as_posix()
    return f'/{text[0].lower()}{text[2:]}' if windows and len(text) > 2 and text[1] == ':' else text


def closure(program, sdk, manifest):
    target_os = manifest["target"]["os"]
    norm = str.lower if target_os == "windows" else str
    index = {}
    for directory in (sdk / "bin", sdk / "lib"):
        for file in directory.glob("*"):
            if file.is_file() and binaries._is_target_binary(file, target_os):
                name = norm(file.name)
                if name in index and digest(index[name]) != digest(file):
                    raise ValueError(f"ambiguous runtime import: {name}")
                index[name] = file
    external = {norm(name) for entry in manifest.get("external_prerequisites", []) for name in entry.get("components", [])}
    pending, seen, owned, os_imports = [program], set(), {}, set()
    while pending:
        file = pending.pop()
        if file in seen:
            continue
        seen.add(file)
        for name in binaries.binary_dependencies(file, target_os):
            leaf = binaries._macho_component(name) if target_os == "macos" else name
            leaf = norm(leaf)
            if leaf in index:
                path = index[leaf]
                owned[path.relative_to(sdk).as_posix()] = digest(path)
                pending.append(path)
            elif leaf in external:
                os_imports.add(leaf)
            else:
                raise ValueError(f"undeclared consumer runtime: {name}")
    return {"packaged": owned, "external": sorted(os_imports)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    sdk, work = args.sdk_root.resolve(), args.output.resolve()
    work.mkdir(parents=True, exist_ok=True)
    (work / "result.json").unlink(missing_ok=True)
    manifest = json.loads((sdk / "manifest.json").read_text())
    from crt_dev_profile import SIDECAR
    profile = json.loads((sdk / SIDECAR).read_text())
    env = runtime_env(sdk, manifest)
    windows = manifest["target"]["os"] == "windows"
    macos = manifest["target"]["os"] == "macos"
    owned = macos_bindings.owned_symbols(sdk) if macos else {}
    flags = subprocess.CREATE_NO_WINDOW if windows else 0
    openssl = shutil.which("openssl")
    if not openssl:
        raise ValueError("native OpenSSL build-host tool required for the fresh test PKI")
    pki = Path(tempfile.mkdtemp(prefix="pki-", dir=work))
    make_pki(pki, openssl)
    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass
        def do_GET(self):
            body = zlib.compress(b"crt-development-sdk")
            self.send_response(200)
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(pki / "good.pem", pki / "good.key")
    server.socket = context.wrap_socket(server.socket, server_side=True)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    source = work / "consumer.cc"
    shutil.copy2(ROOT / "libstdc++/tests/development_sdk_consumer.cc", source)
    results = {}
    try:
        for mode in ("static", "shared"):
            compile_env = dict(env, CRT_CXX_RUNTIME_LINKAGE=mode)
            program = work / ("consumer-" + mode + (".exe" if windows else ""))
            libraries = [str(sdk / "lib" / name) for name in (
                "libcurl.a", "libmbedtls.a", "libmbedx509.a", "libmbedcrypto.a", "libz.a")] if mode == "static" else [
                "-L" + str(sdk / "lib"), "-lcurl", "-lmbedtls", "-lmbedx509", "-lmbedcrypto", "-lz"]
            if mode == "shared" and windows:
                libraries = [str(sdk / "lib" / name) for name in (
                    "libcurl.dll.a", "libmbedtls.dll.a", "libmbedx509.dll.a", "libmbedcrypto.dll.a", "libz.so")]
            command = [str(sdk / ("tools/crt-c++.cmd" if windows else "tools/crt-c++")),
                       "-std=c++17", *profile["consumer_compile_options"], str(source), *libraries, "-o", str(program)]
            compiled = subprocess.run(command, env=compile_env, capture_output=True, text=True,
                                      timeout=120, creationflags=flags)
            if compiled.returncode:
                raise ValueError(f"{mode} consumer compile failed: {compiled.stdout} {compiled.stderr}")
            imports = closure(program, sdk, manifest)
            binding_evidence = None
            if macos:
                image = macos_bindings.collect_image(program, program.name, owned)
                if image["kind"] != "exe" or not owned:
                    raise ValueError("macOS consumer audit requires an executable and SDK-owned symbols")
                notes = []
                failures = macos_bindings.problems([image], notes=notes)
                if failures:
                    raise ValueError("macOS consumer binding audit: " + "; ".join(failures))
                binding_evidence = {**image, "bound": {key: sorted(value) for key, value in image["bound"].items()},
                                    "math_only": sorted(image["math_only"]), "notes": notes, "passed": True}
            if windows:
                for relative in imports["packaged"]:
                    shutil.copy2(sdk / relative, work / Path(relative).name)
            run_env = dict(compile_env)
            if windows:
                run_env["PATH"] = str(Path(os.environ.get("SystemRoot", "C:/Windows")) / "System32")
            else:
                run_env["LD_LIBRARY_PATH" if manifest["target"]["os"] == "linux" else "DYLD_LIBRARY_PATH"] = str(sdk / "lib")
            cases = {}
            # No globally installed CA bundle may grant trust. The valid CA is
            # deliberately present in ambient variables during the negative.
            run_env.update(CURL_CA_BUNDLE=str(pki / "ca.pem"), SSL_CERT_FILE=str(pki / "ca.pem"))
            for name, ca, expected in (("verified", "ca.pem", "ok"), ("wrong-ca", "other.pem", "reject"),
                                       ("missing-ca", None, "reject"), ("malformed-ca", "malformed.pem", "reject")):
                command = [str(program), f"https://127.0.0.1:{server.server_port}/payload",
                           target_path(pki / ca, windows) if ca else "-", expected]
                completed = subprocess.run(command, env=run_env, capture_output=True, text=True,
                                           timeout=30, creationflags=flags)
                marker = "development-sdk: HTTPS+inflate ok" if expected == "ok" else "development-sdk: rejected"
                if completed.returncode or marker not in completed.stdout:
                    raise ValueError(f"{mode}/{name}: exit {completed.returncode}; {completed.stdout} {completed.stderr}")
                cases[name] = True
            results[mode] = {"cases": cases, "imports": imports}
            if binding_evidence is not None:
                results[mode]["macos_binding_audit"] = binding_evidence
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)
    (work / "result.json").write_text(json.dumps({"passed": True, "link_modes": results}, indent=2) + "\n")
    print("Development SDK consumer: static/shared HTTPS+inflate and trust negatives pass")


if __name__ == "__main__":
    main()
