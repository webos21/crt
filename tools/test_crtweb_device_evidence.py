#!/usr/bin/env python3
"""Keep decoded audio or a wall-clock fallback from being reported as real output."""

import unittest

from build_webkit_crt_port import audio_output_evidence


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


if __name__ == "__main__":
    unittest.main()
