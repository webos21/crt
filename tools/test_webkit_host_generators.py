#!/usr/bin/env python3
"""Windows host generator argv-file regression; no target/upstream patch."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
RUNNER = ROOT / "libcrtweb/cmake/crt_webkit_run_generator.py"
HOOK = ROOT / "libcrtweb/cmake/crt_webkit_host_generators.cmake"
FLAGS = {"creationflags": subprocess.CREATE_NO_WINDOW} if sys.platform == "win32" else {}


class HostGeneratorTests(unittest.TestCase):
    def test_invalid_manifest_fails(self):
        with tempfile.TemporaryDirectory() as folder:
            args = Path(folder) / "argv.json"
            args.write_text('{"not": "an argv array"}', encoding="utf-8")
            result = subprocess.run([sys.executable, RUNNER, args], capture_output=True, **FLAGS)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b"nonempty JSON string array", result.stderr)

    @unittest.skipUnless(shutil.which("cmake") and shutil.which("ninja"), "needs host CMake/Ninja")
    def test_oversized_argv_spaces_dependencies_and_following_commands(self):
        with tempfile.TemporaryDirectory(prefix="crt host generator ") as folder:
            root = Path(folder)
            source, build = root / "source with spaces", root / "build with spaces"
            source.mkdir()
            generator = source / "generate-serializers.py"
            generator.write_text(
                "import json,os,sys\nfrom pathlib import Path\nimport nearby_helper\n"
                "Path('result.json').write_text(json.dumps({'args':sys.argv[1:],"
                "'cwd':os.getcwd(),'helper':nearby_helper.VALUE}),encoding='utf-8')\n",
                encoding="utf-8",
            )
            (source / "nearby_helper.py").write_text("VALUE=42\n", encoding="utf-8")
            dependency = source / "input.serialization.in"
            dependency.write_text("first", encoding="utf-8")
            arguments = ["cpp", "--split-by-directory"] + [
                "input directory/유니코드-" + str(i) + "-" + "x" * 100 + ".serialization.in"
                for i in range(400)
            ]
            arguments += ['literal @DO_NOT_EXPAND@', 'quote " and slash \\', 'line\nbreak']
            self.assertGreater(len(subprocess.list2cmdline(arguments)), 32767)
            quoted = "\n".join('  [=[' + value + ']=]' for value in arguments)
            (source / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.20)\nproject(Generator NONE)\n'
                'set(CMAKE_HOST_WIN32 TRUE)\n'
                f'include("{HOOK.as_posix()}")\n'
                'add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/result.json"\n'
                f'  MAIN_DEPENDENCY "{generator.as_posix()}"\n'
                f'  DEPENDS "{dependency.as_posix()}"\n'
                f'  COMMAND "{Path(sys.executable).as_posix()}" "{generator.as_posix()}"\n'
                + quoted + '\n'
                '  COMMAND "${CMAKE_COMMAND}" -E touch following-command.txt\n'
                '  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}" VERBATIM)\n'
                'add_custom_target(generate ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/result.json")\n'
                'add_custom_command(OUTPUT untouched.txt COMMAND "${CMAKE_COMMAND}" -E touch untouched.txt VERBATIM)\n'
                'add_custom_target(unrelated ALL DEPENDS untouched.txt)\n',
                encoding="utf-8",
            )
            def run(command):
                result = subprocess.run(command, capture_output=True, text=True, **FLAGS)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                return result
            run(["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja"])
            run(["cmake", "--build", str(build)])
            result = json.loads((build / "result.json").read_text(encoding="utf-8"))
            self.assertEqual(result["args"], arguments)
            self.assertEqual(Path(result["cwd"]), build)
            self.assertEqual(result["helper"], 42)
            self.assertTrue((build / "following-command.txt").is_file())
            self.assertTrue((build / "untouched.txt").is_file())
            second = run(["cmake", "--build", str(build)])
            self.assertIn("no work to do", second.stdout)
            dependency.write_text("changed dependency", encoding="utf-8")
            rebuilt = run(["cmake", "--build", str(build)])
            self.assertNotIn("no work to do", rebuilt.stdout)
            self.assertEqual(json.loads((build / "result.json").read_text(encoding="utf-8"))["args"], arguments)


if __name__ == "__main__":
    unittest.main()
