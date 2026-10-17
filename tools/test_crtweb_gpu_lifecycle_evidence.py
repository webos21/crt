#!/usr/bin/env python3
"""9C requires actual fault coverage and bounded complete lifetimes, not 9B replay."""
import unittest
import build_webkit_crt_port as port


class LifecycleEvidenceTests(unittest.TestCase):
    def output(self, backend="3"):
        names = ("normal", "consumer-disconnect", "worker-kill", "host-kill", "web-kill", "context-loss", "held-resize")
        cases = list(names) + [names[i % 7] for i in range(20)] + ["normal"]
        return ("crtweb_gpu_lifecycle_test: queued_transfers=3 imported=0 cleanup=pass\n"
                + "".join(f"crtweb_gpu_lifecycle_test: case={name} result=pass elapsed_ms=100\n" for name in cases)
                + "crtweb_gpu_lifecycle_test: cycles=20 rss=+32 KiB threads=+0 descriptors=+0\n"
                + "".join(f"crtweb_gpu_lifecycle_test: coverage={name} count={3 if i < 6 else 2}\n" for i, name in enumerate(names))
                + f"crtweb_gpu_lifecycle_test: ok backend={backend} cycles=20 device_loss=controlled-context-and-owner "
                  "worker_loss=pass browser_loss=pass disconnect=pass held_resize=pass recovery=pass cleanup=pass\n")

    def test_each_native_backend(self):
        for host, backend in (("linux", "3"), ("windows", "1"), ("macos", "2")):
            self.assertTrue(port.gpu_lifecycle_evidence(self.output(backend), host)["verified"])
        self.assertFalse(port.gpu_lifecycle_evidence(self.output(), "macos")["verified"])
        self.assertFalse(port.gpu_lifecycle_evidence(self.output(), "android")["verified"])

    def test_every_observation_and_summary_is_required(self):
        text = self.output()
        for line in text.splitlines():
            self.assertFalse(port.gpu_lifecycle_evidence(text.replace(line + "\n", "", 1), "linux")["verified"], line)
        self.assertFalse(port.gpu_lifecycle_evidence(text * 2, "linux")["verified"])
        for field in text.split(": ok ")[1].split():
            self.assertFalse(port.gpu_lifecycle_evidence(text.replace(field, "", 1 if field != "cycles=20" else 2), "linux")["verified"], field)

    def test_failed_cases_limits_and_fake_loss_are_rejected(self):
        for old, new in (("result=pass", "result=fail"), ("elapsed_ms=100", "elapsed_ms=30001"),
                         ("rss=+32", "rss=+24577"), ("threads=+0", "threads=+3"),
                         ("descriptors=+0", "descriptors=+3"), ("count=3", "count=0"),
                         ("device_loss=controlled-context-and-owner", "device_loss=physical-reset"),
                         ("recovery=pass", "recovery=not-tested")):
            self.assertFalse(port.gpu_lifecycle_evidence(self.output().replace(old, new, 1), "linux")["verified"], old)
        for tail in ("crtweb_gpu_resource_test: FAILED cleanup\n", "skipped\n", "fallback\n"):
            self.assertFalse(port.gpu_lifecycle_evidence(self.output() + tail, "linux")["verified"])

    def test_controlled_host_kill_is_not_a_failed_assertion(self):
        self.assertTrue(port.gpu_lifecycle_evidence(self.output() + "crtweb_gpu_worker: FAILED\n", "linux")["verified"])


if __name__ == "__main__":
    unittest.main()
