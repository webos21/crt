"""One macOS deployment target across the build, the wrappers and the release gate."""
from pathlib import Path
import re
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import crt_macho
import crt_sdk_release as release

ROOT = Path(__file__).resolve().parent.parent
TARGET = crt_macho.MACOS_DEPLOYMENT_TARGET


def image(minos):
    """A minimal arm64 Mach-O with one LC_BUILD_VERSION command."""
    major, minor = (int(part) for part in minos.split("."))
    command = struct.pack("<IIIIII", crt_macho.LC_BUILD_VERSION, 24, 1, major << 16 | minor << 8, 0, 0)
    return struct.pack("<IIIIIIII", crt_macho.MH_MAGIC_64, 0x0100000c, 0, 2, 1, len(command), 0, 0) + command


def policy(flag=True):
    vector = ["-ffreestanding"] + (["-mmacos-version-min=" + TARGET] if flag else [])
    return {"target": {"os": "macos"}, "commands": {"compile_c": vector},
            "modes": {"static-library": {"compile_c": vector, "archive": ["rcs"]}}}


class OneValue(unittest.TestCase):
    def test_cmake_and_both_wrappers_name_the_same_target(self):
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertEqual([TARGET], re.findall(r'set\(CMAKE_OSX_DEPLOYMENT_TARGET "([0-9.]+)"', cmake))
        for wrapper in ("crt-cc", "crt-c++"):
            text = (ROOT / "tools" / wrapper).read_text(encoding="utf-8")
            self.assertEqual([TARGET], re.findall(r"-mmacos-version-min=\$\{CRT_MACOS_DEPLOYMENT_TARGET:-([0-9.]+)\}", text))


class Gate(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        (self.root / "bin").mkdir()
        (self.root / "bin/tool").write_bytes(image(TARGET))
        (self.root / "lib.a").write_bytes(b"!<arch>\n")

    def check(self, document=None, archive=frozenset({"11.0", TARGET})):
        release.check_macos_deployment_target(self.root, document or policy(), lambda path: set(archive))

    def test_the_target_and_older_archive_members_pass(self):
        self.assertEqual(TARGET, crt_macho.minimum_os(self.root / "bin/tool"))
        self.check()

    def test_a_newer_or_an_older_image_is_refused(self):
        for other in ("27.2", "14.0"):
            (self.root / "bin/tool").write_bytes(image(other))
            with self.assertRaisesRegex(release.release.ReleaseError, "declares minimum macOS " + other):
                self.check()

    def test_a_newer_archive_member_is_refused(self):
        with self.assertRaisesRegex(release.release.ReleaseError, "archive members require macOS 27.2"):
            self.check(archive={"27.2"})

    def test_a_policy_without_the_flag_is_refused(self):
        with self.assertRaisesRegex(release.release.ReleaseError, "lacks -mmacos-version-min"):
            self.check(policy(flag=False))

    def test_a_tree_without_images_is_refused(self):
        (self.root / "bin/tool").unlink()
        with self.assertRaisesRegex(release.release.ReleaseError, "no Mach-O image"):
            self.check()

    def test_other_hosts_are_not_checked(self):
        release.check_macos_deployment_target(self.root / "absent", {"target": {"os": "linux"}})


if __name__ == "__main__":
    unittest.main()
