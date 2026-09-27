#!/usr/bin/env python3
"""Real coverage for tools/fetch_ports.py's download() retry/backoff logic
(2026-09-27) -- the fix for a real CI break: android.googlesource.com's
Gitiles archive endpoint returned a genuine HTTP 503 fetching porting/
recipes/make.json's own source on three CI legs at once, with no retry at
all before this landed. Mocks urllib/time.sleep so this runs instantly and
needs no real network -- what matters is retry/backoff/give-up decisions,
not any real host's actual current availability.
"""

import sys
import tempfile
import unittest
import urllib.error
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))

import fetch_ports


def http_error(code: str) -> urllib.error.HTTPError:
    return urllib.error.HTTPError(url="https://example.invalid/x", code=code, msg=str(code), hdrs=None, fp=None)


class DownloadRetry(unittest.TestCase):
    def setUp(self):
        self.tmpdir = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmpdir.cleanup)
        self.archive_path = str(Path(self.tmpdir.name) / "archive.tar.gz")

    def test_existing_archive_skips_the_network_entirely(self):
        Path(self.archive_path).write_bytes(b"already here")
        with mock.patch.object(fetch_ports.urllib.request, "urlopen") as urlopen:
            fetch_ports.download("https://example.invalid/x", self.archive_path)
        urlopen.assert_not_called()

    def test_a_definitive_404_fails_immediately_without_retrying(self):
        with mock.patch.object(fetch_ports.urllib.request, "urlopen", side_effect=http_error(404)) as urlopen, \
             mock.patch.object(fetch_ports.time, "sleep") as sleep:
            with self.assertRaises(SystemExit) as ctx:
                fetch_ports.download("https://example.invalid/x", self.archive_path)
        self.assertEqual(urlopen.call_count, 1, "a 404 must not be retried")
        sleep.assert_not_called()
        self.assertIn("HTTP 404", str(ctx.exception))
        self.assertFalse(Path(self.archive_path).exists())

    def test_a_transient_503_then_success_recovers(self):
        response = mock.MagicMock()
        response.__enter__.return_value = response
        response.read.side_effect = [b"payload", b""]
        calls = [http_error(503), response]

        def fake_urlopen(url, timeout=None):
            result = calls.pop(0)
            if isinstance(result, Exception):
                raise result
            return result

        with mock.patch.object(fetch_ports.urllib.request, "urlopen", side_effect=fake_urlopen), \
             mock.patch.object(fetch_ports.time, "sleep") as sleep:
            fetch_ports.download("https://example.invalid/x", self.archive_path)
        self.assertEqual(sleep.call_count, 1)
        self.assertEqual(sleep.call_args[0][0], 5, "the first backoff delay is 5s")
        self.assertTrue(Path(self.archive_path).exists())
        self.assertEqual(Path(self.archive_path).read_bytes(), b"payload")

    def test_persistent_503_gives_up_after_the_last_attempt_with_capped_backoff(self):
        with mock.patch.object(fetch_ports.urllib.request, "urlopen", side_effect=http_error(503)) as urlopen, \
             mock.patch.object(fetch_ports.time, "sleep") as sleep:
            with self.assertRaises(SystemExit) as ctx:
                fetch_ports.download("https://example.invalid/x", self.archive_path)
        self.assertEqual(urlopen.call_count, 8, "8 attempts total, matching this function's own comment")
        # 5, 10, 20, 40, 60, 60, 60 -- 7 sleeps between 8 attempts, capped at
        # max_delay=60 rather than continuing to double indefinitely.
        self.assertEqual(sleep.call_count, 7)
        self.assertEqual([c.args[0] for c in sleep.call_args_list], [5, 10, 20, 40, 60, 60, 60])
        self.assertIn("HTTP 503", str(ctx.exception))

    def test_a_connection_error_is_retried_like_a_5xx(self):
        with mock.patch.object(
            fetch_ports.urllib.request, "urlopen", side_effect=ConnectionError("reset")
        ) as urlopen, mock.patch.object(fetch_ports.time, "sleep"):
            with self.assertRaises(SystemExit):
                fetch_ports.download("https://example.invalid/x", self.archive_path)
        self.assertEqual(urlopen.call_count, 8)


if __name__ == "__main__":
    unittest.main()
