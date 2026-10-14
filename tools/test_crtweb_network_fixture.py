#!/usr/bin/env python3
"""Unit tests of the build-host fixture, separate from the CRT browser acceptance gate."""
import http.client
import os
import subprocess
import tempfile
import threading
import unittest
from pathlib import Path
from unittest.mock import patch

import test_crtweb_network as fixture


class FixtureTest(unittest.TestCase):
    def test_http_routes(self):
        server = fixture.http.server.ThreadingHTTPServer(("127.0.0.1", 0), fixture.Handler)
        thread = threading.Thread(target=server.serve_forever)
        thread.start()
        try:
            def request(method, path, body=None, headers=None):
                connection = http.client.HTTPConnection("127.0.0.1", server.server_port, timeout=5)
                connection.request(method, path, body, headers or {})
                response = connection.getresponse()
                result = response.status, dict(response.getheaders()), response.read()
                connection.close()
                return result
            self.assertEqual(request("GET", "/start")[1]["Location"], "/page")
            page = request("GET", "/page")
            self.assertIn("HttpOnly", page[1]["Set-Cookie"])
            self.assertIn(b"probe.js", page[2])
            self.assertIn(b"fetch", request("GET", "/probe.js")[2])
            self.assertEqual(request("POST", "/redirect-post", "crt-body")[0], 307)
            echo = request("POST", "/echo", "crt-body", {"Cookie": "crt=network", "X-CRT-Probe": "yes"})
            self.assertEqual(fixture.json.loads(echo[2]), {"method": "POST", "body": "crt-body",
                                                        "cookie": "crt=network", "header": "yes"})
        finally:
            server.shutdown()
            server.server_close()
            thread.join()

    def test_pki(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            fixture.make_pki(directory, "openssl")
            for name, expected in (("good", True), ("wrong", False), ("expired", False)):
                completed = subprocess.run(["openssl", "verify", "-CAfile", str(directory / "ca.pem"),
                                            "-verify_ip", "127.0.0.1", str(directory / (name + ".pem"))],
                                           capture_output=True, timeout=10)
                self.assertEqual(completed.returncode == 0, expected, name)
            unrelated = subprocess.run(["openssl", "verify", "-CAfile", str(directory / "other.pem"),
                                        str(directory / "good.pem")], capture_output=True, timeout=10)
            self.assertNotEqual(unrelated.returncode, 0)

    def test_gate_records_each_case(self):
        real_run = subprocess.run
        commands = []
        def run(command, **kwargs):
            if command[0] not in ("fake-browser", "fake-webview"):
                return real_run(command, **kwargs)
            commands.append((command, kwargs["env"]))
            marker = "crtweb_network_test" if command[0] == "fake-browser" else "crtweb_webview_test"
            return subprocess.CompletedProcess(command, 0, marker + ": ok\n", "")
        with patch.object(fixture.subprocess, "run", side_effect=run):
            result = fixture.run_gate("fake-browser", webview_program="fake-webview")
        self.assertTrue(result["passed"])
        self.assertEqual(len(commands), 15)
        self.assertEqual(commands[0][0][1], "default")
        self.assertEqual(result["cases"]["https_wrong_san"]["returncode"], 0)
        self.assertEqual(commands[5][0][1], "insecure")
        self.assertEqual(commands[9][0][1], "reset")
        self.assertNotIn("CRTWEB_WEBVIEW_TLS_CA_FILE", commands[10][1])
        self.assertTrue(commands[11][1]["CRTWEB_WEBVIEW_TLS_CA_FILE"].endswith("ca.pem"))
        self.assertEqual(commands[13][1]["CRTWEB_WEBVIEW_TLS_INSECURE"], "1")
        self.assertEqual(commands[14][1]["CRTWEB_WEBVIEW_TLS_EXPECT_START_FAILURE"], "1")

    def test_configuration_is_local_and_caller_environment_is_unchanged(self):
        with tempfile.TemporaryDirectory() as directory, \
                patch.dict(os.environ, {"OPENSSL_CONF": "absent-global-config"}), \
                patch("test_crtweb_network.subprocess.run") as run:
            root = Path(directory).resolve()
            fixture.make_pki(root, "openssl")
            self.assertTrue((root / "openssl.cnf").is_file())
            for call in run.call_args_list:
                self.assertEqual(call.kwargs["env"]["OPENSSL_CONF"], str(root / "openssl.cnf"))
                self.assertEqual(call.kwargs["cwd"], root)
            self.assertEqual(os.environ["OPENSSL_CONF"], "absent-global-config")


if __name__ == "__main__":
    unittest.main()
