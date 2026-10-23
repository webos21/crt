"""Real deep-path filesystem fixtures; no global Windows configuration changes."""
import os
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))
from crt_file_ops import atomic_replace, copy_tree, filesystem_path


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

    def test_windows_replace_retries_only_bounded_known_errors(self):
        error = PermissionError("busy")
        error.winerror = 5
        with patch("crt_file_ops.os.name", "nt"), patch("crt_file_ops.filesystem_path", side_effect=str), \
                patch("crt_file_ops.os.replace", side_effect=[error, None]) as replace, \
                patch("crt_file_ops.time.sleep") as sleep:
            atomic_replace("hidden", "final")
            self.assertEqual(2, replace.call_count)
            sleep.assert_called_once_with(0.1)
        with patch("crt_file_ops.os.name", "nt"), patch("crt_file_ops.filesystem_path", side_effect=str), \
                patch("crt_file_ops.os.replace", side_effect=error) as replace, \
                patch("crt_file_ops.time.sleep"):
            with self.assertRaises(PermissionError):
                atomic_replace("hidden", "final")
            self.assertEqual(8, replace.call_count)
        with patch("crt_file_ops.os.name", "posix"), patch("crt_file_ops.os.replace", side_effect=error) as replace:
            with self.assertRaises(PermissionError):
                atomic_replace("hidden", "final")
            self.assertEqual(1, replace.call_count)

    def test_unrelated_replace_error_is_not_retried(self):
        with patch("crt_file_ops.os.replace", side_effect=FileNotFoundError("missing")) as replace, \
                patch("crt_file_ops.time.sleep") as sleep:
            with self.assertRaises(FileNotFoundError):
                atomic_replace(self.root / "hidden", self.root / "final")
            replace.assert_called_once()
            sleep.assert_not_called()

    @unittest.skipUnless(os.name == "nt", "native Windows directory sharing")
    def test_native_directory_handle_is_released_before_atomic_publish(self):
        import ctypes
        from ctypes import wintypes
        import threading
        source, destination = self.root / "hidden", self.root / "final"
        source.mkdir()
        (source / "payload").write_bytes(b"verified")
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.CreateFileW.argtypes = (wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                                      ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE)
        kernel.CreateFileW.restype = wintypes.HANDLE
        kernel.CloseHandle.argtypes = (wintypes.HANDLE,)
        handle = kernel.CreateFileW(str(source), 0x80, 3, None, 3, 0x02000000, None)
        self.assertNotEqual(handle, wintypes.HANDLE(-1).value, ctypes.get_last_error())
        timer = threading.Timer(0.25, lambda: kernel.CloseHandle(handle))
        timer.start()
        try:
            atomic_replace(source, destination)
        finally:
            timer.join()
        self.assertEqual(b"verified", (destination / "payload").read_bytes())
        self.assertFalse(source.exists())

    @unittest.skipUnless(os.name == "nt", "Win32-only path spelling")
    def test_unc_and_existing_extended_paths(self):
        self.assertEqual("\\\\?\\UNC\\server\\share\\file", filesystem_path("\\\\server\\share\\file"))
        extended = "\\\\?\\C:\\folder\\file"
        self.assertEqual(extended, filesystem_path(extended))


if __name__ == "__main__":
    unittest.main()
