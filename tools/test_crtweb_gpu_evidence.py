#!/usr/bin/env python3
"""Strict GPU composition evidence is not a skip, CPU target or zero-copy claim."""
import unittest
from unittest import mock
from pathlib import Path
import build_webkit_crt_port as port


class GpuEvidenceTests(unittest.TestCase):
    def output(self, backend=3):
        return (f"crtweb_webview_gpu_test: ok backend={backend} source=cpu-shm interop=cpu-upload "
                "gpu_target=pass pixels=pass clip=pass opacity=pass ordering=pass input=pass "
                "resize=pass updates=20 recovery=pass cleanup=pass\n")

    def test_expected_backend_on_each_host(self):
        for host, backend in (("linux", 3), ("macos", 2), ("windows", 1)):
            self.assertTrue(port.gpu_composition_evidence(self.output(backend), host)["verified"])
            self.assertFalse(port.gpu_composition_evidence(self.output(0), host)["verified"])
        self.assertFalse(port.gpu_composition_evidence(self.output(), "android")["verified"])

    def test_skip_failure_missing_and_duplicate_summary(self):
        for text in ("", "crtweb_webview_gpu_test: skipped\n", self.output() * 2,
                     self.output() + "FAILED pixels\n", self.output() + "skipped\n"):
            self.assertFalse(port.gpu_composition_evidence(text, "linux")["verified"])

    def test_every_required_observation(self):
        for field in self.output().split(": ok ")[1].split():
            self.assertFalse(port.gpu_composition_evidence(self.output().replace(field, ""), "linux")["verified"], field)
        for before, after in (("gpu_target=pass", "gpu_target=cpu"), ("interop=cpu-upload", "interop=zero-copy"),
                              ("pixels=pass", "pixels=fail"), ("updates=20", "updates=0")):
            self.assertFalse(port.gpu_composition_evidence(self.output().replace(before, after), "linux")["verified"])

    def test_live_count_is_exact(self):
        for presented, requested in ((0, 5), (4, 5), (5, 4), (6, 5)):
            text = self.output() + f"crtweb_webview_gpu_test: presented={presented} requested={requested}\n"
            self.assertFalse(port.gpu_composition_evidence(text, "linux", 5)["verified"])
        text = self.output() + "crtweb_webview_gpu_test: presented=5 requested=5\n"
        self.assertTrue(port.gpu_composition_evidence(text, "linux", 5)["verified"])
        self.assertFalse(port.gpu_composition_evidence(text, "linux")["verified"])
        self.assertFalse(port.gpu_composition_evidence(self.output(), "linux", 5)["verified"])


class BrowserMapTests(unittest.TestCase):
    def evidence(self, text, target="linux"):
        return port.gpu_browser_map_evidence(text, Path("/sdk"), Path("/deps"), Path("/build"), Path("/run"), target)

    def maps(self):
        return ("MAPPED HostProcess /build/lib/libcrtweb.so\n"
                "MAPPED WebProcess /sdk/lib/libc.so\nMAPPED NetworkProcess /deps/lib/libcurl.so\n")

    def test_all_roles_required(self):
        self.assertTrue(self.evidence(self.maps())["verified"])
        self.assertFalse(self.evidence("")["verified"])
        self.assertFalse(self.evidence(self.maps().replace("MAPPED NetworkProcess", "IGNORED NetworkProcess"))["verified"])

    def test_host_library_and_gpu_loader_rejected_in_browser(self):
        for library in ("/usr/lib/libc.so.6", "/sdk/lib/libvulkan.so.1", "/sdk/lib/libEGL.so.1"):
            self.assertFalse(self.evidence(self.maps() + f"MAPPED WebProcess {library}\n")["verified"])
        self.assertTrue(self.evidence(self.maps() + "MAPPED HostProcess /usr/lib/ld-linux-x86-64.so.2\n")["verified"])

    def test_existing_crt_gl_stubs_are_not_native_gpu_drivers(self):
        evidence = self.evidence(self.maps() + "MAPPED WebProcess /deps/lib/libEGL.so\n")
        self.assertTrue(evidence["verified"])
        self.assertEqual(evidence["crt_gl_stubs"], ["libEGL.so"])

    def test_other_hosts_are_explicitly_unobserved_not_clean(self):
        self.assertIsNone(self.evidence("", "macos")["verified"])

    def test_windows_requires_all_browser_roles(self):
        self.assertFalse(self.evidence("", "windows")["verified"])
        maps = self.maps().replace(".so", ".dll")
        self.assertTrue(self.evidence(maps, "windows")["verified"])
        self.assertFalse(self.evidence(maps.replace("MAPPED NetworkProcess", "IGNORED NetworkProcess"),
                                       "windows")["verified"])

    @mock.patch.dict(port.os.environ, {"SystemRoot": "/os/Windows"})
    def test_windows_os_closure_and_crt_stubs(self):
        maps = self.maps().replace(".so", ".dll")
        maps += ("MAPPED HostProcess /os/Windows/System32/KERNEL32.DLL\n"
                 "MAPPED WebProcess /os/Windows/System32/ucrtbase.dll\n"
                 "MAPPED WebProcess /os/Windows/System32/d3d11.dll\n"
                 "MAPPED WebProcess /os/Windows/System32/dxgi.dll\n"
                 "MAPPED WebProcess /deps/lib/libGLESv2.dll\n")
        result = self.evidence(maps, "windows")
        self.assertTrue(result["verified"])
        self.assertEqual(result["crt_gl_stubs"], ["libGLESv2.dll"])
        self.assertIn("ucrtbase.dll", result["os_runtime_dlls"])
        self.assertIn("d3d11.dll", result["os_runtime_dlls"])

    @mock.patch.dict(port.os.environ, {"SystemRoot": "/os/Windows"})
    def test_windows_foreign_and_final_compositor_clients_rejected(self):
        maps = self.maps().replace(".so", ".dll")
        for path in ("/foreign/ucrtbase.dll", "/sdk/lib/d3d12.dll", "/os/Windows/System32/D3D12Core.dll",
                     "/os/Windows/System32/D3DCOMPILER_47.dll", "/os/Windows/System32/vulkan-1.dll",
                     "/os/Windows/System32/opengl32.dll", "/sdk/lib/libEGL.dll"):
            self.assertFalse(self.evidence(maps + f"MAPPED WebProcess {path}\n", "windows")["verified"], path)


if __name__ == "__main__":
    unittest.main()
