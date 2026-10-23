"""Real deep-path filesystem fixtures; no global Windows configuration changes."""
import os
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))
from crt_file_ops import copy_tree, filesystem_path


class FileOperations(unittest.TestCase):
    def setUp(self):
        self.root = Path(tempfile.mkdtemp(prefix="crt long path spaces "))
        self.addCleanup(lambda: shutil.rmtree(filesystem_path(self.root)))

    def test_deep_files_are_copied_without_exclusions_or_short_aliases(self):
        source = self.root / "source"
        relative = Path(*["long component " + str(i) + "x" * 48 for i in range(5)]) / "payload.txt"
        original = Path(filesystem_path(source / relative))
        original.parent.mkdir(parents=True)
        original.write_bytes(b"pinned upstream bytes")
        destination = self.root / "deep actual destination"
        self.assertGreater(len(str(destination / relative)), 320)
        self.assertEqual(destination, copy_tree(source, destination, symlinks=True))
        self.assertEqual(b"pinned upstream bytes", Path(filesystem_path(destination / relative)).read_bytes())
        # The caller's requested paths and the source bytes are unchanged.
        self.assertEqual(b"pinned upstream bytes", original.read_bytes())

    def test_hardlink_composition_and_overlay_keep_the_source_pristine(self):
        source = self.root / "source"
        source.mkdir()
        (source / "file").write_text("source")
        destination = self.root / "composition"
        copy_tree(source, destination, copy_function=os.link)
        target = destination / "file"
        value = target.read_bytes()
        target.unlink()  # break the link before applying the project overlay
        target.write_bytes(value)
        overlay = self.root / "overlay"
        overlay.mkdir()
        (overlay / "file").write_text("overlay")
        copy_tree(overlay, destination, dirs_exist_ok=True)
        self.assertEqual("source", (source / "file").read_text())
        self.assertEqual("overlay", target.read_text())

    def test_aggregated_errors_are_bounded_but_still_fail(self):
        failures = [("source", "destination" * 100, "failure" * 1000)] * 1000
        with patch("crt_file_ops.shutil.copytree", side_effect=shutil.Error(failures)):
            with self.assertRaises(OSError) as error:
                copy_tree(self.root / "source", self.root / "destination")
        self.assertIn("1000 errors; first 3", str(error.exception))
        self.assertLess(len(str(error.exception)), 2500)

    @unittest.skipUnless(os.name == "nt", "Win32-only path spelling")
    def test_unc_and_existing_extended_paths(self):
        self.assertEqual("\\\\?\\UNC\\server\\share\\file", filesystem_path("\\\\server\\share\\file"))
        extended = "\\\\?\\C:\\folder\\file"
        self.assertEqual(extended, filesystem_path(extended))


if __name__ == "__main__":
    unittest.main()
