"""Shared host-side peak-RSS sampling for the allocator baseline runners.

TODO.md's "Allocator baseline validation before Upper Runtime", tranche 4
("measure fragmentation and resident memory outside the CRT ABI"): CRT's
own `getrusage()` is a compatibility stub (it does not track real resident
memory), so this measurement deliberately lives here, on the host side,
using each host's own real memory-accounting facilities against the
child process -- not added as benchmark-only behavior to the Bionic-facing
API tranche 4's own wording explicitly says to keep out of that boundary.

`run_with_peak_rss()` is the only function callers need: it runs a
subprocess and returns its stdout/stderr/returncode/wall time exactly like
`subprocess.run()` would, plus the child's own peak resident-memory usage
in bytes (or `None` if this host/run could not determine it -- callers
must tolerate that, not require it).

Windows: queries `PeakWorkingSetSize` via `psapi.GetProcessMemoryInfo()`
against the child's own process handle, read once right after the child
exits (Windows maintains this figure for the life of the process; no
polling loop is needed). Uses `subprocess.Popen`, not `subprocess.run()`,
specifically so this module controls the handle's lifetime long enough to
query it before `Popen` closes it.

POSIX (Linux/macOS): uses `resource.getrusage(RUSAGE_CHILDREN).ru_maxrss`,
sampled before and after the child runs. That field is a RUNNING MAXIMUM
across every child this process has ever reaped, not a per-child value --
but since callers run children strictly one at a time (never overlapping),
a genuine increase between the "before" and "after" reads can only be
caused by the child that just ran, and the new value is then exactly that
child's own peak. If the reading did not increase (this child's peak did
not exceed any earlier child's), this module honestly reports `None`
rather than a stale or wrong number -- callers running tiers in increasing
size order (as both allocator baseline runners do) will see this trigger
only in the unlikely case a smaller tier's peak genuinely exceeded a
larger one's, which is itself a fact worth surfacing as "unknown" rather
than guessing.
"""

import platform
import subprocess
import time
from typing import Optional, Tuple


def _windows_peak_working_set_bytes(proc: "subprocess.Popen") -> Optional[int]:
    try:
        import ctypes
        from ctypes import wintypes
    except ImportError:
        return None

    handle = getattr(proc, "_handle", None)
    if handle is None:
        return None

    class _ProcessMemoryCounters(ctypes.Structure):
        _fields_ = [
            ("cb", wintypes.DWORD),
            ("PageFaultCount", wintypes.DWORD),
            ("PeakWorkingSetSize", ctypes.c_size_t),
            ("WorkingSetSize", ctypes.c_size_t),
            ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
            ("QuotaPagedPoolUsage", ctypes.c_size_t),
            ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
            ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
            ("PagefileUsage", ctypes.c_size_t),
            ("PeakPagefileUsage", ctypes.c_size_t),
        ]

    try:
        psapi = ctypes.WinDLL("psapi", use_last_error=True)
        counters = _ProcessMemoryCounters()
        counters.cb = ctypes.sizeof(counters)
        ok = psapi.GetProcessMemoryInfo(wintypes.HANDLE(int(handle)), ctypes.byref(counters), counters.cb)
        if not ok:
            return None
        return int(counters.PeakWorkingSetSize)
    except OSError:
        return None


def _run_windows(args) -> Tuple[int, str, str, float, Optional[int]]:
    start = time.monotonic()
    proc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    stdout, stderr = proc.communicate()
    wall_seconds = time.monotonic() - start
    # Query while this module still holds the Popen object (and therefore
    # the handle) alive -- see this file's own top comment for why that
    # ordering matters and why polling during the run is unnecessary.
    peak_rss_bytes = _windows_peak_working_set_bytes(proc)
    return proc.returncode, stdout, stderr, wall_seconds, peak_rss_bytes


def _run_posix(args) -> Tuple[int, str, str, float, Optional[int]]:
    try:
        import resource
    except ImportError:
        result = subprocess.run(args, capture_output=True, text=True, check=False)
        return result.returncode, result.stdout, result.stderr, 0.0, None

    before = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss
    start = time.monotonic()
    result = subprocess.run(args, capture_output=True, text=True, check=False)
    wall_seconds = time.monotonic() - start
    after = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss

    peak_rss_bytes = None
    if after > before:
        # macOS reports ru_maxrss in bytes already; Linux reports KiB.
        unit_bytes = 1 if platform.system() == "Darwin" else 1024
        peak_rss_bytes = after * unit_bytes

    return result.returncode, result.stdout, result.stderr, wall_seconds, peak_rss_bytes


def run_with_peak_rss(args) -> Tuple[int, str, str, float, Optional[int]]:
    """Run `args` as a subprocess; return (returncode, stdout, stderr, wall_seconds, peak_rss_bytes)."""
    if platform.system() == "Windows":
        return _run_windows(args)
    return _run_posix(args)
