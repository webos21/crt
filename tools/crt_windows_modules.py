"""Test-host-only module snapshots of owned native Windows processes."""
import ctypes
from ctypes import wintypes as w
from pathlib import Path
import time


class ModuleEntry(ctypes.Structure):
    _fields_ = [('size', w.DWORD), ('module_id', w.DWORD), ('pid', w.DWORD), ('global_count', w.DWORD),
                ('process_count', w.DWORD), ('base', ctypes.c_void_p), ('image_size', w.DWORD),
                ('module', w.HMODULE), ('name', w.WCHAR * 256), ('path', w.WCHAR * 260)]


def snapshot(pid):
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.CreateToolhelp32Snapshot.argtypes, kernel.CreateToolhelp32Snapshot.restype = [w.DWORD, w.DWORD], w.HANDLE
    kernel.Module32FirstW.argtypes = kernel.Module32NextW.argtypes = [w.HANDLE, ctypes.POINTER(ModuleEntry)]
    kernel.Module32FirstW.restype = kernel.Module32NextW.restype = w.BOOL
    kernel.CloseHandle.argtypes, kernel.CloseHandle.restype = [w.HANDLE], w.BOOL
    handle = kernel.CreateToolhelp32Snapshot(0x8 | 0x10, pid)
    if handle in (None, w.HANDLE(-1).value):
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        entry = ModuleEntry(); entry.size = ctypes.sizeof(entry)
        if not kernel.Module32FirstW(handle, ctypes.byref(entry)):
            raise ctypes.WinError(ctypes.get_last_error())
        result = []
        while True:
            result.append(str(Path(entry.path).resolve()))
            if not kernel.Module32NextW(handle, ctypes.byref(entry)):
                error = ctypes.get_last_error()
                if error != 18:
                    raise ctypes.WinError(error)
                return result
    finally:
        kernel.CloseHandle(handle)


def wait_for_modules(pid, expected, timeout=2):
    deadline, last = time.monotonic() + timeout, 'module initialization not observed'
    while time.monotonic() < deadline:
        try:
            paths = snapshot(pid)
            if {name.casefold() for name in expected} <= {Path(path).name.casefold() for path in paths}:
                return {'pid': pid, 'paths': paths}
            last = 'expected private DLLs not all mapped'
        except OSError as error:
            last = str(error)
        time.sleep(0.01)
    return {'pid': pid, 'paths': [], 'error': last}
