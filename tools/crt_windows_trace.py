"""Restricted-token launch with redirected stdio and a record of every process the run creates.

Producer-side acceptance helper (never a CLI or SDK dependency). It reuses the restricted token of
crt_windows_external -- the producer/CLI trees stay unreadable through the per-run SID -- and adds what
the Linux execve trace and the macOS Seatbelt exec policy give the other hosts: an OS-recorded inventory of
the executables the run really started. A job object reports each new process through a completion port;
the image path is read while the process is alive. A process that exits before it can be opened is listed
with an unknown image, so the allow-list check treats `None` as a failure rather than ignoring it.
This is accounting, not an OS sandbox: it cannot stop a launch, it proves which ones happened.
"""
import ctypes
from ctypes import wintypes as w
import os
from pathlib import Path
import subprocess
import threading
import time

import crt_windows_external as base

JOB_NEW_PROCESS, JOB_EXIT_PROCESS, JOB_ABNORMAL_EXIT, JOB_ACTIVE_ZERO = 6, 7, 8, 4
PROC_THREAD_ATTRIBUTE_HANDLE_LIST = 0x00020002


class StartupInfoEx(ctypes.Structure):
    _fields_ = [('info', base.StartupInfo), ('attributes', ctypes.c_void_p)]


class Security(ctypes.Structure):
    _fields_ = [('length', w.DWORD), ('descriptor', ctypes.c_void_p), ('inherit', w.BOOL)]


class CompletionPort(ctypes.Structure):
    _fields_ = [('key', ctypes.c_void_p), ('port', w.HANDLE)]


def _declare(kernel):
    for function, arguments, result in [
            (kernel.CreateFileW, [w.LPCWSTR, w.DWORD, w.DWORD, ctypes.POINTER(Security), w.DWORD, w.DWORD, w.HANDLE], w.HANDLE),
            (kernel.CreateIoCompletionPort, [w.HANDLE, w.HANDLE, ctypes.c_void_p, w.DWORD], w.HANDLE),
            (kernel.GetQueuedCompletionStatus, [w.HANDLE, ctypes.POINTER(w.DWORD), ctypes.POINTER(ctypes.c_void_p),
                                                ctypes.POINTER(ctypes.c_void_p), w.DWORD], w.BOOL),
            (kernel.OpenProcess, [w.DWORD, w.BOOL, w.DWORD], w.HANDLE),
            (kernel.QueryFullProcessImageNameW, [w.HANDLE, w.DWORD, w.LPWSTR, ctypes.POINTER(w.DWORD)], w.BOOL),
            (kernel.InitializeProcThreadAttributeList, [ctypes.c_void_p, w.DWORD, w.DWORD, ctypes.POINTER(ctypes.c_size_t)], w.BOOL),
            (kernel.UpdateProcThreadAttribute, [ctypes.c_void_p, w.DWORD, ctypes.c_size_t, ctypes.c_void_p, ctypes.c_size_t,
                                                ctypes.c_void_p, ctypes.c_void_p], w.BOOL),
            (kernel.DeleteProcThreadAttributeList, [ctypes.c_void_p], None)]:
        function.argtypes, function.restype = arguments, result


def _open_inheritable(kernel, path, write):
    attributes = Security(ctypes.sizeof(Security), None, True)
    handle = kernel.CreateFileW(str(path), 0x40000000 if write else 0x80000000, 3, ctypes.byref(attributes),
                                2 if write else 3, 0x80, None)
    base.checked(handle not in (None, w.HANDLE(-1).value))
    return handle


def private_directory(path, *sids):
    """Give a directory created by the caller a protected DACL naming only the caller, SYSTEM, Administrators and `sids`.

    Unregistered per-run SIDs cannot be named through icacls (no account mapping), so the DACL goes in as SDDL, as
    crt_windows_external.create_work does for the work root. Only a directory this run created is ever changed."""
    kernel, advapi = base.api()
    descriptor, dacl = ctypes.c_void_p(), ctypes.c_void_p()
    sddl = 'D:P' + ''.join(f'(A;OICI;FA;;;{identity})' for identity in (base.caller_sid(kernel, advapi), 'SY', 'BA', *sids))
    base.checked(advapi.ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, 1, ctypes.byref(descriptor), None))
    try:
        present, defaulted = w.BOOL(), w.BOOL()
        base.checked(advapi.GetSecurityDescriptorDacl(descriptor, ctypes.byref(present), ctypes.byref(dacl), ctypes.byref(defaulted)))
        error = advapi.SetNamedSecurityInfoW(str(path), 1, 4 | 0x80000000, None, None, dacl, None)
        if error:
            raise ctypes.WinError(error)
    finally:
        kernel.LocalFree(descriptor)


def run(command, *, work, sid, env, cwd, stdout, stderr, timeout=600):
    """Run `command` under the restricted token; returns exit code, timeout flag and the process inventory."""
    kernel, advapi = base.api()
    _declare(kernel)
    original, restricted = w.HANDLE(), w.HANDLE()
    allocations, handles = [], []
    process = base.ProcessInfo()
    job = port = w.HANDLE()
    processes, stop = {}, threading.Event()
    base.checked(advapi.OpenProcessToken(kernel.GetCurrentProcess(), 0x8B, ctypes.byref(original)))
    try:
        identities = ('S-1-1-0', 'S-1-5-32-545', base.logon_sid(kernel, advapi, original), sid)
        restrictions = (base.SidAttributes * len(identities))()
        for item, identity in zip(restrictions, identities):
            pointer = ctypes.c_void_p()
            base.checked(advapi.ConvertStringSidToSidW(identity, ctypes.byref(pointer)))
            allocations.append(pointer)
            item.sid, item.attributes = pointer.value, 0
        base.checked(advapi.CreateRestrictedToken(original, 1, 0, None, 0, None, len(identities), restrictions,
                                                  ctypes.byref(restricted)))
        descriptor, dacl = ctypes.c_void_p(), ctypes.c_void_p()
        sddl = 'D:' + ''.join(f'(A;;GA;;;{identity})' for identity in (base.caller_sid(kernel, advapi), 'SY', sid))
        base.checked(advapi.ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, 1, ctypes.byref(descriptor), None))
        allocations.append(descriptor)
        present, defaulted = w.BOOL(), w.BOOL()
        base.checked(advapi.GetSecurityDescriptorDacl(descriptor, ctypes.byref(present), ctypes.byref(dacl), ctypes.byref(defaulted)))
        base.checked(advapi.SetTokenInformation(restricted, 6, ctypes.byref(dacl), ctypes.sizeof(dacl)))

        null_in = _open_inheritable(kernel, 'NUL', False)
        out_handle, err_handle = _open_inheritable(kernel, stdout, True), _open_inheritable(kernel, stderr, True)
        handles += [null_in, out_handle, err_handle]
        inherit = (w.HANDLE * 3)(null_in, out_handle, err_handle)
        size = ctypes.c_size_t()
        kernel.InitializeProcThreadAttributeList(None, 1, 0, ctypes.byref(size))
        attribute_list = ctypes.create_string_buffer(size.value)
        base.checked(kernel.InitializeProcThreadAttributeList(attribute_list, 1, 0, ctypes.byref(size)))
        base.checked(kernel.UpdateProcThreadAttribute(attribute_list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit,
                                                      ctypes.sizeof(inherit), None, None))
        startup = StartupInfoEx()
        startup.info.cb = ctypes.sizeof(startup)
        startup.info.flags, startup.info.show = 0x101, 0
        startup.info.stdin, startup.info.stdout, startup.info.stderr = null_in, out_handle, err_handle
        startup.attributes = ctypes.cast(attribute_list, ctypes.c_void_p)

        job = kernel.CreateJobObjectW(None, None)
        base.checked(job)
        port = kernel.CreateIoCompletionPort(w.HANDLE(-1), None, None, 1)
        base.checked(port)
        limits = base.ExtendedLimits()
        limits.basic.flags = 0x2000  # kill on job close
        base.checked(kernel.SetInformationJobObject(job, 9, ctypes.byref(limits), ctypes.sizeof(limits)))
        association = CompletionPort(None, port)
        base.checked(kernel.SetInformationJobObject(job, 7, ctypes.byref(association), ctypes.sizeof(association)))

        def watch():
            message, key, overlapped = w.DWORD(), ctypes.c_void_p(), ctypes.c_void_p()
            while not stop.is_set():
                if not kernel.GetQueuedCompletionStatus(port, ctypes.byref(message), ctypes.byref(key),
                                                        ctypes.byref(overlapped), 50):
                    continue
                if message.value == JOB_NEW_PROCESS and overlapped.value:
                    pid = overlapped.value
                    image = None
                    handle = kernel.OpenProcess(0x1000, False, pid)
                    if handle:
                        buffer, length = ctypes.create_unicode_buffer(32768), w.DWORD(32768)
                        if kernel.QueryFullProcessImageNameW(handle, 0, buffer, ctypes.byref(length)):
                            image = buffer.value
                        kernel.CloseHandle(handle)
                    processes[pid] = {'pid': pid, 'image': image}
        watcher = threading.Thread(target=watch, daemon=True)
        watcher.start()

        line = ctypes.create_unicode_buffer(subprocess.list2cmdline([str(p) for p in command]))
        environment = ctypes.create_unicode_buffer('\0'.join(f'{k}={v}' for k, v in sorted(env.items())) + '\0\0')
        # CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT
        base.checked(advapi.CreateProcessAsUserW(restricted, str(command[0]), line, None, None, True,
                                                0x08000000 | 0x400 | 4 | 0x80000, environment, str(cwd),
                                                ctypes.cast(ctypes.byref(startup), ctypes.POINTER(base.StartupInfo)),
                                                ctypes.byref(process)))
        try:
            base.checked(kernel.AssignProcessToJobObject(job, process.process))
            if kernel.ResumeThread(process.thread) == 0xFFFFFFFF:
                raise ctypes.WinError(ctypes.get_last_error())
        except BaseException:
            kernel.TerminateProcess(process.process, 1)
            raise
        timed_out = False
        waited = kernel.WaitForSingleObject(process.process, int(timeout * 1000))
        if waited == 0x102:
            timed_out = True
            kernel.TerminateProcess(process.process, 124)
            kernel.WaitForSingleObject(process.process, 5000)
        elif waited != 0:
            raise ctypes.WinError(ctypes.get_last_error())
        code = w.DWORD()
        base.checked(kernel.GetExitCodeProcess(process.process, ctypes.byref(code)))
        time.sleep(0.3)  # let the completion port deliver the last exit/new-process messages
        stop.set()
        watcher.join(5)
        kernel.DeleteProcThreadAttributeList(attribute_list)
        return {'exit_code': code.value, 'timed_out': timed_out,
                'processes': sorted(processes.values(), key=lambda item: item['pid'])}
    finally:
        stop.set()
        for handle in (job, port, process.thread, process.process, restricted, original, *handles):
            if handle:
                kernel.CloseHandle(handle)
        for pointer in allocations:
            kernel.LocalFree(pointer)
