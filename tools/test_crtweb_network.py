#!/usr/bin/env python3
"""Build-host-only loopback HTTP/TLS fixture for Tranche 8A; never installed in the target SDK.

Fresh, two-day PKI per run: no public Internet, checked-in private key, or date-sensitive fixture.
The browser executable uses CRT sockets/curl/OpenSSL; Python ssl is only the test server.
"""
import argparse
from datetime import datetime, timedelta, timezone
import http.server
import json
import os
from pathlib import Path
import ssl
import subprocess
import tempfile
import threading


class Handler(http.server.BaseHTTPRequestHandler):
    def setup(self):
        self.request.settimeout(10)
        super().setup()

    def handle(self):
        try:
            super().handle()
        except (ConnectionResetError, BrokenPipeError, TimeoutError, ssl.SSLError):
            # A browser rejecting a certificate may close immediately after the handshake.
            pass

    def log_message(self, *_args):
        pass

    def response(self, status, body=b"", headers=()):
        self.send_response(status)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        for key, value in headers:
            self.send_header(key, value)
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/start":
            self.response(302, headers=[("Location", "/page")])
        elif self.path == "/page":
            self.response(200, b'<!doctype html><title>network:pending</title><script src="/probe.js"></script>',
                          [("Content-Type", "text/html"), ("Set-Cookie", "crt=network; Path=/; HttpOnly")])
        elif self.path == "/probe.js":
            self.response(200, b"fetch('/redirect-post',{method:'POST',headers:{'X-CRT-Probe':'yes'},body:'crt-body'})"
                          b".then(r=>r.json()).then(r=>{document.title=r.method==='POST'&&r.body==='crt-body'&&"
                          b"r.header==='yes'&&r.cookie==='crt=network'?'network:ok':'network:bad';})"
                          b".catch(()=>{document.title='network:error';});", [("Content-Type", "text/javascript")])
        else:
            self.response(404)

    def do_POST(self):
        body = self.rfile.read(int(self.headers.get("Content-Length", 0)))
        if self.path == "/redirect-post":
            self.response(307, headers=[("Location", "/echo")])
        elif self.path == "/echo":
            result = {"method": self.command, "body": body.decode(), "header": self.headers.get("X-CRT-Probe"),
                      "cookie": self.headers.get("Cookie")}
            self.response(200, json.dumps(result).encode(), [("Content-Type", "application/json")])
        else:
            self.response(404)


def make_pki(directory, openssl):
    def run(*args):
        subprocess.run([openssl, *args], cwd=directory, check=True, capture_output=True, timeout=60)
    for name in ("ca", "other"):
        run("req", "-x509", "-newkey", "rsa:2048", "-nodes", "-keyout", name + ".key",
            "-out", name + ".pem", "-days", "2", "-subj", "/CN=CRT test " + name,
            "-addext", "basicConstraints=critical,CA:TRUE")
    for name, ip in (("good", "127.0.0.1"), ("wrong", "10.255.255.1"), ("expired", "127.0.0.1")):
        run("req", "-new", "-newkey", "rsa:2048", "-nodes", "-keyout", name + ".key",
            "-out", name + ".csr", "-subj", "/CN=CRT loopback")
        (directory / (name + ".ext")).write_text(
            f"subjectAltName=IP:{ip}\nbasicConstraints=critical,CA:FALSE\nextendedKeyUsage=serverAuth\n")
        if name == "expired":
            (directory / "index").touch()
            (directory / "serial").write_text("1000\n")
            (directory / "ca.cnf").write_text(
                "[ca]\ndefault_ca=issuer\n[issuer]\ndatabase=index\nserial=serial\n"
                "new_certs_dir=.\ncertificate=ca.pem\nprivate_key=ca.key\ndefault_md=sha256\n"
                "policy=names\n[names]\ncommonName=supplied\n")
            now = datetime.now(timezone.utc)
            run("ca", "-batch", "-config", "ca.cnf", "-in", name + ".csr", "-out", name + ".pem",
                "-startdate", (now - timedelta(days=2)).strftime("%y%m%d%H%M%SZ"),
                "-enddate", (now - timedelta(days=1)).strftime("%y%m%d%H%M%SZ"),
                "-extfile", name + ".ext", "-notext")
        else:
            run("x509", "-req", "-in", name + ".csr", "-CA", "ca.pem", "-CAkey", "ca.key",
                "-CAcreateserial", "-out", name + ".pem", "-days", "2", "-extfile", name + ".ext")
    (directory / "malformed.pem").write_text("not a PEM trust anchor\n")


def run_gate(program, env=None, cwd=None, openssl="openssl", path_for_target=str,
             webview_program=None, webview_env=None):
    cases = {}
    servers = []
    threads = []
    with tempfile.TemporaryDirectory(prefix="crtweb-network-") as temp:
        directory = Path(temp)
        make_pki(directory, openssl)
        try:
            for name in ("http", "good", "wrong", "expired"):
                server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
                if name != "http":
                    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
                    context.load_cert_chain(directory / (name + ".pem"), directory / (name + ".key"))
                    server.socket = context.wrap_socket(server.socket, server_side=True)
                servers.append(server)
                thread = threading.Thread(target=server.serve_forever, daemon=True)
                thread.start()
                threads.append(thread)
            urls = [("http" if i == 0 else "https") + "://127.0.0.1:" + str(s.server_port) + "/start"
                    for i, s in enumerate(servers)]
            fixtures = [
                ("http_default", "default", "-", urls[0], "success"),
                ("https_no_anchors", "default", "-", urls[1], "failure"),
                ("https_correct_ca", "verify", "ca.pem", urls[1], "success"),
                ("https_wrong_ca", "verify", "other.pem", urls[1], "failure"),
                ("https_wrong_san", "verify", "ca.pem", urls[2], "failure"),
                ("https_explicit_insecure", "insecure", "-", urls[2], "success"),
                ("https_empty_explicit", "verify", "-", urls[1], "failure"),
                ("https_expired", "verify", "ca.pem", urls[3], "failure"),
                ("https_malformed_ca", "verify", "malformed.pem", urls[1], "failure"),
                ("https_reset_defaults", "reset", "ca.pem", urls[1], "failure"),
            ]
            # Both environment conventions are ambient; none may grant trust to the target.
            run_env = dict(os.environ if env is None else env)
            run_env.update(CURL_CA_BUNDLE=str(directory / "ca.pem"), SSL_CERT_FILE=str(directory / "ca.pem"))
            for key in ("http_proxy", "https_proxy", "all_proxy", "HTTP_PROXY", "HTTPS_PROXY", "ALL_PROXY"):
                run_env.pop(key, None)
            run_env["NO_PROXY"] = run_env["no_proxy"] = "127.0.0.1,localhost"
            for name, mode, ca, url, expected in fixtures:
                ca_path = "-" if ca == "-" else path_for_target(directory / ca)
                completed = subprocess.run([str(program), mode, ca_path, url, expected], env=run_env,
                                           cwd=cwd, capture_output=True, text=True, timeout=90)
                ok = completed.returncode == 0 and "crtweb_network_test: ok" in completed.stdout
                cases[name] = {"passed": ok, "returncode": completed.returncode,
                               "stdout": completed.stdout, "stderr": completed.stderr}
                print(name + ": " + ("PASSED" if ok else "FAILED"), flush=True)
                if not ok:
                    print(completed.stdout + completed.stderr, flush=True)
            if webview_program is not None:
                for name, ca, url, success, insecure in (
                    ("webview_default", None, urls[1], False, False),
                    ("webview_correct_ca", "ca.pem", urls[1], True, False),
                    ("webview_wrong_san", "ca.pem", urls[2], False, False),
                    ("webview_explicit_insecure", None, urls[2], True, True),
                    ("webview_missing_ca_file", "missing.pem", urls[1], False, False),
                ):
                    launch_env = dict(run_env if webview_env is None else webview_env)
                    for key in ("http_proxy", "https_proxy", "all_proxy", "HTTP_PROXY", "HTTPS_PROXY", "ALL_PROXY"):
                        launch_env.pop(key, None)
                    launch_env["NO_PROXY"] = launch_env["no_proxy"] = "127.0.0.1,localhost"
                    for key in ("CA_FILE", "INSECURE", "EXPECT_SUCCESS", "EXPECT_START_FAILURE"):
                        launch_env.pop("CRTWEB_WEBVIEW_TLS_" + key, None)
                    launch_env["CRTWEB_WEBVIEW_TLS_URL"] = url
                    if ca: launch_env["CRTWEB_WEBVIEW_TLS_CA_FILE"] = path_for_target(directory / ca)
                    if success: launch_env["CRTWEB_WEBVIEW_TLS_EXPECT_SUCCESS"] = "1"
                    if insecure: launch_env["CRTWEB_WEBVIEW_TLS_INSECURE"] = "1"
                    if ca == "missing.pem": launch_env["CRTWEB_WEBVIEW_TLS_EXPECT_START_FAILURE"] = "1"
                    completed = subprocess.run([str(webview_program)], env=launch_env, cwd=cwd,
                                               capture_output=True, text=True, timeout=90)
                    ok = completed.returncode == 0 and "crtweb_webview_test: ok" in completed.stdout
                    cases[name] = {"passed": ok, "returncode": completed.returncode,
                                   "stdout": completed.stdout, "stderr": completed.stderr}
                    print(name + ": " + ("PASSED" if ok else "FAILED"), flush=True)
                    if not ok: print(completed.stdout + completed.stderr, flush=True)
        finally:
            for server in servers:
                server.shutdown()
                server.server_close()
            for thread in threads:
                thread.join()
    return {"passed": all(c["passed"] for c in cases.values()) and len(cases) == (15 if webview_program else 10),
            "cases": cases}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--program", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--openssl", default="openssl")
    args = parser.parse_args()
    result = run_gate(args.program.resolve(), openssl=args.openssl)
    if args.output:
        args.output.write_text(json.dumps(result, indent=2) + "\n")
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
