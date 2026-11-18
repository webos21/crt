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


def response_with(body, content_type):
    response = mock.MagicMock()
    response.__enter__.return_value = response
    response.read.side_effect = [body, b""]
    response.headers.get_content_type.return_value = content_type
    return response


class WrongAnswers(unittest.TestCase):
    """A server that answers HTTP 200 with the wrong bytes (the zlib.net block page seen on windows-latest) is rejected, not accepted."""

    GOOD = b"the real archive"
    GOOD_SHA = __import__("hashlib").sha256(GOOD).hexdigest()

    def setUp(self):
        self.tmpdir = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmpdir.cleanup)
        self.archive_path = str(Path(self.tmpdir.name) / "z.tar.gz")

    def run_download(self, answers, sha256=None):
        calls = list(answers)

        def fake_urlopen(url, timeout=None):
            return calls.pop(0)

        with mock.patch.object(fetch_ports.urllib.request, "urlopen", side_effect=fake_urlopen) as urlopen, \
             mock.patch.object(fetch_ports.time, "sleep") as sleep:
            fetch_ports.download("https://example.invalid/z.tar.gz", self.archive_path, sha256)
        return urlopen, sleep

    def test_an_html_page_is_rejected_and_the_next_answer_is_used(self):
        urlopen, sleep = self.run_download([response_with(b"<!DOCTYPE html>", "text/html"), response_with(self.GOOD, "application/x-gzip")])
        self.assertEqual(urlopen.call_count, 2)
        self.assertEqual([c.args[0] for c in sleep.call_args_list], [5])
        self.assertEqual(Path(self.archive_path).read_bytes(), self.GOOD)

    def test_a_digest_mismatch_is_retried_inside_the_download(self):
        self.run_download([response_with(b"garbage", "application/x-gzip"), response_with(self.GOOD, "application/x-gzip")], self.GOOD_SHA)
        self.assertEqual(Path(self.archive_path).read_bytes(), self.GOOD)

    def test_a_persistent_wrong_answer_is_rejected_after_three_short_tries_and_leaves_nothing(self):
        page = lambda: response_with(b"<html>", "text/html")
        with mock.patch.object(fetch_ports.urllib.request, "urlopen", side_effect=[page(), page(), page()]) as urlopen, \
             mock.patch.object(fetch_ports.time, "sleep") as sleep:
            with self.assertRaises(fetch_ports.DownloadRejected) as ctx:
                fetch_ports.download("https://example.invalid/z.tar.gz", self.archive_path, self.GOOD_SHA)
        self.assertEqual(urlopen.call_count, 3)
        self.assertEqual([c.args[0] for c in sleep.call_args_list], [5, 10])
        self.assertIn("HTML page", str(ctx.exception))
        self.assertFalse(Path(self.archive_path).exists())
        self.assertFalse(Path(self.archive_path + ".tmp").exists())

    def test_a_html_asset_that_is_meant_to_be_html_is_not_rejected_for_its_type(self):
        self.archive_path = str(Path(self.tmpdir.name) / "page.html")
        self.run_download([response_with(b"<html>", "text/html")])
        self.assertEqual(Path(self.archive_path).read_bytes(), b"<html>")


class Mirrors(unittest.TestCase):
    def setUp(self):
        self.tmpdir = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmpdir.cleanup)
        self.archive_path = str(Path(self.tmpdir.name) / "z.tar.gz")
        self.good = b"the real archive"
        self.source = {"url": "https://primary.invalid/z.tar.gz", "mirrors": ["https://mirror.invalid/z.tar.gz"], "archive": "z.tar.gz",
                       "sha256": __import__("hashlib").sha256(self.good).hexdigest()}

    def test_a_blocked_primary_falls_back_to_the_mirror_for_the_same_pinned_bytes(self):
        seen = []

        def fake_urlopen(url, timeout=None):
            seen.append(url)
            return response_with(b"<html>", "text/html") if "primary" in url else response_with(self.good, "application/x-gzip")

        with mock.patch.object(fetch_ports.urllib.request, "urlopen", side_effect=fake_urlopen), mock.patch.object(fetch_ports.time, "sleep"):
            fetch_ports.fetch_source(self.source, self.archive_path)
        self.assertEqual(seen, ["https://primary.invalid/z.tar.gz"] * 3 + ["https://mirror.invalid/z.tar.gz"])
        self.assertEqual(Path(self.archive_path).read_bytes(), self.good)

    def test_every_source_failing_names_each_failure(self):
        with mock.patch.object(fetch_ports.urllib.request, "urlopen", side_effect=lambda url, timeout=None: response_with(b"<html>", "text/html")), \
             mock.patch.object(fetch_ports.time, "sleep"):
            with self.assertRaises(SystemExit) as ctx:
                fetch_ports.fetch_source(self.source, self.archive_path)
        message = str(ctx.exception)
        self.assertIn("no source delivered the archive", message)
        self.assertIn("primary.invalid", message)
        self.assertIn("mirror.invalid", message)

    def test_a_mirror_without_a_pinned_digest_is_refused_before_any_download(self):
        self.source["sha256"] = None
        with mock.patch.object(fetch_ports.urllib.request, "urlopen") as urlopen:
            with self.assertRaises(SystemExit):
                fetch_ports.fetch_source(self.source, self.archive_path)
        urlopen.assert_not_called()

    def test_every_recipe_mirror_is_https_distinct_and_pinned(self):
        recipes = fetch_ports.load_recipes(Path(__file__).resolve().parent.parent / "porting" / "recipes")
        found = 0
        for name, recipe in recipes.items():
            source = recipe["source"]
            for mirror in source.get("mirrors", []):
                found += 1
                self.assertTrue(mirror.startswith("https://"), name)
                self.assertNotEqual(mirror, source["url"], name)
                self.assertRegex(source.get("sha256") or "", r"^[0-9a-f]{64}$", name)
        self.assertGreaterEqual(found, 1, "the zlib recipe carries the GitHub mirror")


if __name__ == "__main__":
    unittest.main()
