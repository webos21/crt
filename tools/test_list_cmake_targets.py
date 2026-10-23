#!/usr/bin/env python3
"""Target-catalog and stage-test dependency fixtures; no runtime compilation."""

import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import list_cmake_targets as lt

CATALOG = {"groups": {"stage 01-c": [{"target": "crt-c-build"}, {"target": "crt-c-dist"}],
                      "ports": [{"target": "port-test-recipes"}]},
           "os_specific": {"crt-win-only": ["windows"]},
           "internal_stage_parts": {"crt-gfx-build": "part"}}


class StageTestDependencies(unittest.TestCase):
    @unittest.skipUnless(shutil.which("cmake") and shutil.which("ninja"), "needs host CMake/Ninja")
    def test_selected_registered_targets_are_built_without_default_build(self):
        with tempfile.TemporaryDirectory(prefix="stage test dependencies ") as directory:
            root = Path(directory)
            helper = lt.ROOT / "cmake/crt_test_dependencies.cmake"
            (root / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.20)\nproject(Fixture NONE)\n'
                f'include("{helper.as_posix()}")\n'
                'foreach(stage gfx media ui)\n'
                '  add_custom_target(crt-${stage}-test)\n'
                '  add_custom_target(crt${stage}_feature_test COMMAND "${CMAKE_COMMAND}" -E touch "${stage}.passed")\n'
                '  set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crt${stage}_feature_test_runs)\n'
                'endforeach()\n'
                'set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtgfx_python_unit_runs)\n'
                'foreach(stage gfx media ui)\n'
                '  crt_add_registered_test_dependencies(crt-${stage}-test "^crt${stage}_")\n'
                'endforeach()\n', encoding="utf-8")
            build = root / "build"
            configured = subprocess.run(["cmake", "-S", str(root), "-B", str(build), "-G", "Ninja"],
                                        capture_output=True, text=True)
            self.assertEqual(configured.returncode, 0, configured.stdout + configured.stderr)
            for stage in ("gfx", "media", "ui"):
                result = subprocess.run(["cmake", "--build", str(build), "--target", f"crt-{stage}-test"],
                                        capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertTrue((build / f"{stage}.passed").is_file())
                for other in ({"gfx", "media", "ui"} - {stage}):
                    self.assertFalse((build / f"{other}.passed").exists())
                (build / f"{stage}.passed").unlink()


class Classify(unittest.TestCase):
    def test_listed_targets_are_user_facing_and_the_rest_internal(self) -> None:
        r = lt.classify(["crt-c-build", "crt-c-dist", "port-build-zlib", "crtgfx-skia-build", "crt_stage_x_test", "libc"], CATALOG)
        self.assertEqual({"crt-c-build": "stage 01-c", "crt-c-dist": "stage 01-c"}, r["user"])
        self.assertEqual("port recipes", r["internal"]["port-build-zlib"])
        self.assertEqual("feature preparation", r["internal"]["crtgfx-skia-build"])
        self.assertEqual("libraries and tools", r["internal"]["libc"])

    def test_a_documented_target_that_is_missing_is_reported(self) -> None:
        r = lt.classify(["crt-c-build"], CATALOG)
        self.assertEqual(["crt-c-dist", "port-test-recipes"], r["missing"])

    def test_os_specific_documented_targets_are_not_missing_elsewhere(self) -> None:
        cat = json.loads(json.dumps(CATALOG))
        cat["groups"]["stage 01-c"].append({"target": "crt-win-only"})
        r = lt.classify(["crt-c-build", "crt-c-dist", "port-test-recipes"], cat, "macos")
        self.assertEqual([], r["missing"])

    def test_os_specific_target_is_required_on_its_own_os(self) -> None:
        cat = json.loads(json.dumps(CATALOG))
        cat["groups"]["stage 01-c"].append({"target": "crt-win-only"})
        r = lt.classify(["crt-c-build", "crt-c-dist", "port-test-recipes"], cat, "windows")
        self.assertEqual(["crt-win-only"], r["missing"])

    def test_readme_documents_every_public_target_and_no_planned_one(self) -> None:
        import re
        catalog = lt.load_catalog()
        text = (lt.ROOT / "README.md").read_text(encoding="utf-8")
        build = text.split("## Build\n", 1)[1].split("\n## ", 1)[0]
        documented = set(re.findall(r"--target ([a-z0-9-]+)", build))
        listed = {entry["target"] for entries in catalog["groups"].values() for entry in entries}
        self.assertEqual(listed, documented)
        self.assertTrue(documented.isdisjoint(catalog["planned"]))

    def test_a_new_stage_looking_target_must_be_documented_or_declared_a_part(self) -> None:
        r = lt.classify(["crt-c-build", "crt-c-dist", "port-test-recipes", "crt-gfx-build", "crt-new-dist"], CATALOG)
        self.assertEqual(["crt-new-dist"], r["unlisted_stage_like"])

    def test_the_real_catalog_lists_only_stage_looking_or_known_targets(self) -> None:
        catalog = lt.load_catalog()
        listed = [e["target"] for entries in catalog["groups"].values() for e in entries]
        self.assertEqual(len(listed), len(set(listed)), "a target is listed twice")
        self.assertTrue(set(catalog["planned"]).isdisjoint(listed), "a planned target is also listed as existing")


if __name__ == "__main__":
    unittest.main()
