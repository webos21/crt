#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Run a build-host Python generator without an OS-sized argument list."""
import json
from pathlib import Path
import runpy
import sys


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: crt_webkit_run_generator.py ARGV.json")
    arguments = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
    if not isinstance(arguments, list) or not arguments or not all(
        isinstance(value, str) for value in arguments
    ):
        raise SystemExit("generator argv must be a nonempty JSON string array")
    script = Path(arguments[0]).resolve(strict=True)
    # Match direct Python invocation, including adjacent imports and cwd.
    # Never launch a second oversized command.
    sys.argv = arguments
    sys.path[0] = str(script.parent)
    runpy.run_path(str(script), run_name="__main__")


if __name__ == "__main__":
    main()
