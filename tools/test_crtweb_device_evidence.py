#!/usr/bin/env python3
"""Keep decoded audio or a wall-clock fallback from being reported as real output."""

import unittest

from build_webkit_crt_port import audio_output_evidence, camera_stress_evidence, macos_map_checkpoint


class MapCheckpointTests(unittest.TestCase):
    def test_complete_live_process_checkpoint(self):
        self.assertEqual(macos_map_checkpoint("UIProcess 10\nWebProcess 11\nNetworkProcess 12\n", 10),
                         {"WebProcess": 11, "NetworkProcess": 12})

    def test_incomplete_or_wrong_process_identity_is_rejected(self):
        valid = "UIProcess 10\nWebProcess 11\nNetworkProcess 12\n"
        for invalid in ("", "UIProcess 10\n", valid.replace("UIProcess 10", "UIProcess 20"),
                        valid.replace("WebProcess 11", "WebProcess 12"),
                        valid.replace("WebProcess 11", "WebProcess 0"),
                        valid.replace("WebProcess 11", "WebProcess -1"),
                        valid.replace("WebProcess 11", "OtherProcess 11")):
            self.assertIsNone(macos_map_checkpoint(invalid, 10), invalid)

    def test_malformed_or_duplicate_records_are_rejected(self):
        for invalid in ("WebProcess", "WebProcess nope", "WebProcess 11 extra",
                        "WebProcess \u00b2",
                        "UIProcess 10\nWebProcess 11\nNetworkProcess 12\nWebProcess 13"):
            self.assertIsNone(macos_map_checkpoint(invalid, 10), invalid)


class AudioEvidenceTests(unittest.TestCase):
    def evidence(self, **changes):
        fields = dict(rate=44100, channels=2, written_frames=44100, nonzero_samples=40000,
                      position_frames=40000, position_result=0, clock_updates=40,
                      clock_advances=30, write_failures=0)
        fields.update(changes)
        return "CRTWEB_AUDIO sink_closed " + " ".join(f"{key}={value}" for key, value in fields.items())

    def test_real_pcm_and_progressing_clock(self):
        self.assertTrue(audio_output_evidence(self.evidence())["verified"])

    def test_decode_fallback_or_incomplete_diagnostics_are_not_output(self):
        for text in ("", "CRTWEB_AUDIO buffer frames=1152", "CRTWEB_AUDIO sink_closed rate=44100",
                     "CRTWEB_AUDIO sink_open_failed rate=44100 channels=2"):
            self.assertFalse(audio_output_evidence(text)["verified"])

    def test_silence_stalled_clock_and_failed_writes_are_not_positive_evidence(self):
        for fields in (dict(written_frames=0), dict(nonzero_samples=0), dict(position_frames=0),
                       dict(position_result=-1), dict(clock_advances=0), dict(write_failures=1)):
            self.assertFalse(audio_output_evidence(self.evidence(**fields))["verified"])

    def test_one_good_sink_does_not_hide_another_failed_sink(self):
        text = self.evidence() + "\n" + self.evidence(write_failures=1)
        self.assertFalse(audio_output_evidence(text)["verified"])

    def test_null_or_unknown_pulse_endpoint_is_not_alsa_hardware(self):
        for route in ("", "CRTMEDIA_AUDIO sink_open sink_id=1 backend=pulse rate=48000 channels=2\n",
                      "CRTMEDIA_AUDIO sink_open sink_id=unknown backend=alsa device=/dev/snd/pcmC0D0p\n"):
            text = route + self.evidence(sink_id=1)
            self.assertTrue(audio_output_evidence(text)["verified"])
            self.assertFalse(audio_output_evidence(text)["alsa_hardware_verified"])

    def test_route_identity_is_joined_in_order_and_must_match_nonzero_pcm(self):
        alsa = "CRTMEDIA_AUDIO sink_open sink_id=1 backend=alsa device=/dev/snd/pcmC0D0p rate=48000 channels=2\n"
        pulse = "CRTMEDIA_AUDIO sink_open sink_id=1 backend=pulse rate=48000 channels=2\n"
        self.assertTrue(audio_output_evidence(alsa + self.evidence(sink_id=1))["alsa_hardware_verified"])
        for text in (alsa + self.evidence(sink_id=2), alsa + self.evidence(sink_id=1, nonzero_samples=0),
                     alsa + self.evidence(sink_id=1, write_failures=1)):
            self.assertFalse(audio_output_evidence(text)["alsa_hardware_verified"])
        # An allocator can reuse a closed sink's address; later routing must not relabel old output.
        text = alsa + self.evidence(sink_id=1, nonzero_samples=0) + "\n" + pulse + self.evidence(sink_id=1)
        self.assertFalse(audio_output_evidence(text)["alsa_hardware_verified"])

    def test_capture_and_partial_device_names_are_not_playback(self):
        for name in ("pcmC0D0c", "pcmC0D0p-extra", "pcm", "pcmC0D0"):
            text = f"CRTMEDIA_AUDIO sink_open sink_id=1 backend=alsa device=/dev/snd/{name}\n"
            self.assertFalse(audio_output_evidence(text + self.evidence(sink_id=1))["alsa_hardware_verified"])

    def test_native_fixture_requires_all_frames_not_just_a_preroll_tail(self):
        route = "CRTMEDIA_AUDIO sink_open sink_id=1 backend=alsa device=/dev/snd/pcmC0D0p\n"
        fields = dict(sink_id=1, rate=48000, channels=2, written_frames=14400, nonzero_samples=28320)
        self.assertTrue(audio_output_evidence(route + self.evidence(**fields))["native_fixture_verified"])
        pulse = route.replace("backend=alsa device=/dev/snd/pcmC0D0p", "backend=pulse")
        self.assertTrue(audio_output_evidence(pulse + self.evidence(**fields))["fixture_pcm_complete"])
        self.assertFalse(audio_output_evidence(pulse + self.evidence(**fields))["native_fixture_verified"])
        for changes in (dict(written_frames=2112), dict(nonzero_samples=0), dict(rate=44100),
                        dict(channels=1), dict(clock_advances=0), dict(write_failures=1)):
            self.assertFalse(audio_output_evidence(route + self.evidence(**(fields | changes)))["native_fixture_verified"])


class CameraEvidenceTests(unittest.TestCase):
    def evidence(self, warmups=3):
        lines = ["CAMERA_STRESS baseline pid=123 ui_rss=1000 ui_threads=4 ui_descriptors=8 web_rss=50000 web_threads=12 web_descriptors=20"]
        for cycle in range(1, 21):
            lines.append(f"CAMERA_STRESS cycle={cycle} mode={(warmups + cycle - 1) % 4} pid=123 samples=3 distinct=2 camera_fds=0 "
                         "ui_rss_delta=8 ui_threads_delta=0 ui_descriptors_delta=0 "
                         "web_rss_delta=100 web_threads_delta=0 web_descriptors_delta=0")
        lines += [f"CAMERA_STRESS completed=20 warmups={warmups} same_process=1 rss_limit_kib=24576 thread_limit=2 descriptor_limit=2",
                  f"CAMERA_STRESS permission_requests={1 + warmups + 20}"]
        return "\n".join(lines)

    def test_same_process_bounds_and_device_release(self):
        self.assertTrue(camera_stress_evidence(self.evidence(), require_device_fds=True)["verified"])

    def test_all_four_release_paths_can_be_warmed_before_baseline(self):
        self.assertTrue(camera_stress_evidence(self.evidence(warmups=4), require_device_fds=True)["verified"])

    def test_resource_settling_has_a_fixed_deadline(self):
        text = self.evidence(warmups=4).replace("camera_fds=0 ", "camera_fds=0 settled_ms=1200 ")
        text = text.replace("descriptor_limit=2", "descriptor_limit=2 settle_timeout_ms=2000")
        self.assertTrue(camera_stress_evidence(text)["verified"])
        for invalid in (text.replace("settled_ms=1200", "settled_ms=2001"),
                        text.replace("settled_ms=1200", "settled_ms=-1"),
                        text.replace("settle_timeout_ms=2000", "settle_timeout_ms=3000")):
            self.assertFalse(camera_stress_evidence(invalid)["verified"])

    def test_separate_runs_or_incomplete_output_do_not_prove_stress(self):
        for text in ("crtweb_camera_probe: ok\n" * 3, "", self.evidence().split("CAMERA_STRESS completed")[0],
                     self.evidence().replace("cycle=20 ", "cycle=19 "),
                     self.evidence().replace("permission_requests=24", "permission_requests=23")):
            self.assertFalse(camera_stress_evidence(text)["verified"])

    def test_replacement_process_unknown_observations_and_resource_growth_fail(self):
        for key, value in (("pid=123 samples", "pid=124 samples"), ("ui_rss=1000", "ui_rss=-1"),
                           ("web_rss_delta=100", "web_rss_delta=24577"),
                           ("web_threads_delta=0", "web_threads_delta=3"),
                           ("ui_descriptors_delta=0", "ui_descriptors_delta=3"),
                           ("ui_threads_delta=0", "ui_threads_delta=-5"),
                           ("samples=3", "samples=0"), ("distinct=2", "distinct=1")):
            self.assertFalse(camera_stress_evidence(self.evidence().replace(key, value))["verified"], key)

    def test_native_camera_fd_observer_is_explicit_not_invented(self):
        text = self.evidence().replace("camera_fds=0", "camera_fds=-1")
        self.assertTrue(camera_stress_evidence(text)["verified"])
        self.assertFalse(camera_stress_evidence(text, require_device_fds=True)["verified"])
        self.assertFalse(camera_stress_evidence(self.evidence().replace("camera_fds=0", "camera_fds=1"),
                                              require_device_fds=True)["verified"])


if __name__ == "__main__":
    unittest.main()
