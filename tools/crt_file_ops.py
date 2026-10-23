"""Host filesystem operations; keep Win32 extended paths out of tool arguments."""
import os
import shutil
import time


def filesystem_path(path):
    """Opt into long Win32 paths for Python IO, without registry/global changes."""
    value = os.fspath(path)
    if os.name != "nt":
        return value
    value = os.path.abspath(value)
    if value.startswith("\\\\?\\"):
        return value
    if value.startswith("\\\\"):
        return "\\\\?\\UNC\\" + value[2:]
    return "\\\\?\\" + value


def copy_tree(source, destination, **kwargs):
    """Copy every file, including deep upstream metadata; bound failure reports.

    shutil otherwise aggregates an unbounded list of per-file failures into a
    single exception (hundreds of KB for a large checkout). Never skip files to
    make a source cache pass, and never shorten the caller's actual work root.
    """
    try:
        shutil.copytree(filesystem_path(source), filesystem_path(destination), **kwargs)
    except shutil.Error as error:
        failures = error.args[0]
        if not isinstance(failures, (list, tuple)):
            failures = [failures]
        examples = []
        for item in failures[:3]:
            if isinstance(item, (list, tuple)) and len(item) == 3:
                examples.append(str(item[1])[:300] + ": " + str(item[2])[:300])
            else:
                examples.append(str(item)[:600])
        raise OSError(f"source-tree copy failed ({len(failures)} errors; first {len(examples)}): "
                      + " | ".join(examples)) from None
    return destination


def atomic_replace(source, destination):
    """Bound Windows sharing/access contention without weakening publication.

    Keep the prepared tree hidden until the same atomic rename succeeds. A
    persistent permissions failure still raises; no non-atomic fallback, ACL
    change, target deletion or process termination is permitted here.
    """
    for attempt in range(8):
        try:
            os.replace(filesystem_path(source), filesystem_path(destination))
            return
        except OSError as error:
            if os.name != "nt" or getattr(error, "winerror", None) not in (5, 32, 33) or attempt == 7:
                raise
            print(f"atomic publication retry {attempt + 1}/8 (WinError {error.winerror})", flush=True)
            time.sleep(min(0.1 * 2 ** attempt, 1.0))
