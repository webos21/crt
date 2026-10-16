#!/usr/bin/env python3
"""Shared GPU evidence cannot be satisfied by 9A, CPU fallback or missing ownership."""
import unittest
import build_webkit_crt_port as port


class ResourceEvidenceTests(unittest.TestCase):
    def output(self):
        return ("crtweb_gpu_worker: uploads=24 reused=23 retired=3 peak_buffers=4 gpu_readbacks=0\n"
                "crtweb_gpu_resource_test: imports=6 releases=23 live_buffers=3 generation=2\n"
                "crtweb_gpu_resource_test: ok backend=3 source=browser-cpu worker_upload=1 consumer_upload=0 "
                "transport=shared-gpu sync=native-fence-semaphore updates=20 resize=pass ownership=pass cleanup=pass\n")

    def test_linux_only_until_native_backend_replay(self):
        self.assertTrue(port.gpu_resource_evidence(self.output(), "linux")["verified"])
        for target in ("windows", "macos", "android"):
            self.assertFalse(port.gpu_resource_evidence(self.output(), target)["verified"])

    def test_no_skip_fallback_duplicate_failure_or_missing_summary(self):
        for text in ("", "crtweb_gpu_resource_test: fallback mode=cpu resources=0\n", self.output() * 2,
                     self.output() + "skipped\n", self.output() + "FAILED\n", self.output() + "fallback\n"):
            self.assertFalse(port.gpu_resource_evidence(text, "linux")["verified"])

    def test_each_required_field_and_native_ownership_counter(self):
        for field in self.output().split(": ok ")[1].split():
            self.assertFalse(port.gpu_resource_evidence(self.output().replace(field, ""), "linux")["verified"], field)
        for old, new in (("backend=3", "backend=0"), ("consumer_upload=0", "consumer_upload=1"),
                         ("worker_upload=1", "worker_upload=0"), ("imports=6", "imports=0"),
                         ("releases=23", "releases=0"), ("live_buffers=3", "live_buffers=7"),
                         ("generation=2", "generation=1"), ("uploads=24", "uploads=0"),
                         ("reused=23", "reused=0"), ("retired=3", "retired=0"),
                         ("peak_buffers=4", "peak_buffers=7"), ("gpu_readbacks=0", "gpu_readbacks=1"),
                         ("native-fence-semaphore", "cpu-ack")):
            self.assertFalse(port.gpu_resource_evidence(self.output().replace(old, new), "linux")["verified"], old)

    def test_exact_native_frame_count(self):
        valid = self.output() + "crtweb_gpu_resource_test: presented=5 requested=5\n"
        self.assertTrue(port.gpu_resource_evidence(valid, "linux", 5)["verified"])
        self.assertFalse(port.gpu_resource_evidence(valid, "linux")["verified"])
        for text in (self.output(), valid.replace("presented=5", "presented=4"), valid * 2):
            self.assertFalse(port.gpu_resource_evidence(text, "linux", 5)["verified"])


if __name__ == "__main__":
    unittest.main()
