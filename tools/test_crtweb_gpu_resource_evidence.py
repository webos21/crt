#!/usr/bin/env python3
"""Shared GPU evidence cannot be satisfied by 9A, CPU fallback or missing ownership."""
import unittest
import build_webkit_crt_port as port


class ResourceEvidenceTests(unittest.TestCase):
    def output(self, backend="3", sync="native-fence-semaphore"):
        return ("crtweb_gpu_worker: uploads=24 reused=23 retired=3 peak_buffers=4 gpu_readbacks=0\n"
                "crtweb_gpu_resource_test: imports=6 releases=23 live_buffers=3 generation=2\n"
                f"crtweb_gpu_resource_test: ok backend={backend} source=browser-cpu worker_upload=1 consumer_upload=0 "
                f"transport=shared-gpu sync={sync} updates=20 resize=pass ownership=pass cleanup=pass\n")

    def test_native_backends_are_host_specific(self):
        self.assertTrue(port.gpu_resource_evidence(self.output(), "linux")["verified"])
        self.assertTrue(port.gpu_resource_evidence(self.output("1", "native-shared-fence"), "windows")["verified"])
        self.assertTrue(port.gpu_resource_evidence(self.output("2", "native-shared-event"), "macos")["verified"])
        # No host accepts another host's backend or a CPU completion label.
        for target in ("macos", "android"):
            for text in (self.output(), self.output("1", "native-shared-fence")):
                self.assertFalse(port.gpu_resource_evidence(text, target)["verified"])
        self.assertFalse(port.gpu_resource_evidence(self.output("1", "native-shared-fence"), "linux")["verified"])
        self.assertFalse(port.gpu_resource_evidence(self.output(), "windows")["verified"])
        self.assertFalse(port.gpu_resource_evidence(self.output("1", "native-fence-semaphore"), "windows")["verified"])
        self.assertFalse(port.gpu_resource_evidence(self.output("3", "native-shared-fence"), "windows")["verified"])
        self.assertFalse(port.gpu_resource_evidence(self.output("2", "cpu-ack"), "macos")["verified"])
        self.assertFalse(port.gpu_resource_evidence(self.output("2", "native-shared-event"), "linux")["verified"])
        self.assertFalse(port.gpu_resource_evidence(self.output("2", "native-shared-event"), "windows")["verified"])

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
        # The same counters guard the Windows D3D12 evidence.
        windows = self.output("1", "native-shared-fence")
        for old, new in (("backend=1", "backend=0"), ("consumer_upload=0", "consumer_upload=1"),
                         ("gpu_readbacks=0", "gpu_readbacks=1"), ("imports=6", "imports=0"),
                         ("native-shared-fence", "cpu-ack")):
            self.assertFalse(port.gpu_resource_evidence(windows.replace(old, new), "windows")["verified"], old)
        metal = self.output("2", "native-shared-event")
        for old, new in (("consumer_upload=0", "consumer_upload=1"), ("gpu_readbacks=0", "gpu_readbacks=1"),
                         ("imports=6", "imports=0"), ("native-shared-event", "cpu-ack")):
            self.assertFalse(port.gpu_resource_evidence(metal.replace(old, new), "macos")["verified"], old)

    def test_macos_worker_checkpoint_needs_unique_owned_process_tree(self):
        text = "GPUWorker 20\nHostProcess 30\nWebProcess 40\nNetworkProcess 50\n"
        self.assertEqual(port.macos_gpu_resource_map_checkpoint(text, 10),
                         (20, [("HostProcess", 30), ("WebProcess", 40), ("NetworkProcess", 50)]))
        for bad in ("", text.replace("GPUWorker 20", "GPUWorker 10"), text.replace("GPUWorker 20", "GPUWorker 40"),
                    text.replace("GPUWorker 20", "GPUWorker ０"), text.replace("HostProcess 30\n", ""),
                    text.replace("NetworkProcess 50", "NetworkProcess 40"), text + "GPUWorker 60\n"):
            self.assertIsNone(port.macos_gpu_resource_map_checkpoint(bad, 10), bad)

    def test_exact_native_frame_count(self):
        valid = self.output() + "crtweb_gpu_resource_test: presented=5 requested=5\n"
        self.assertTrue(port.gpu_resource_evidence(valid, "linux", 5)["verified"])
        self.assertFalse(port.gpu_resource_evidence(valid, "linux")["verified"])
        for text in (self.output(), valid.replace("presented=5", "presented=4"), valid * 2):
            self.assertFalse(port.gpu_resource_evidence(text, "linux", 5)["verified"])


if __name__ == "__main__":
    unittest.main()
