"""Build-host Windows file-access isolation for the trusted Dev SDK fixture.

CreateRestrictedToken adds a second ACL check on reads AND writes (not
WRITE_RESTRICTED). Existing public Users/Everyone/logon grants permit OS/tool
reads; an unregistered per-run SID permits the newly created work directory.
No existing producer/tool/user ACL, account or machine policy is modified.
This is a trusted acceptance fixture boundary, not a hostile-code sandbox.
"""
import ctypes
from ctypes import wintypes as w
from pathlib import Path
import os
import subprocess
import tempfile
import uuid


class SidAttributes(ctypes.Structure):
    _fields_ = [('sid', ctypes.c_void_p), ('attributes', w.DWORD)]


class StartupInfo(ctypes.Structure):
    _fields_ = [('cb', w.DWORD), ('reserved', w.LPWSTR), ('desktop', w.LPWSTR),
                ('title', w.LPWSTR), ('x', w.DWORD), ('y', w.DWORD),
                ('xsize', w.DWORD), ('ysize', w.DWORD), ('xchars', w.DWORD),
                ('ychars', w.DWORD), ('fill', w.DWORD), ('flags', w.DWORD),
                ('show', w.WORD), ('reserved_size', w.WORD),
                ('reserved_data', ctypes.c_void_p), ('stdin', w.HANDLE),
                ('stdout', w.HANDLE), ('stderr', w.HANDLE)]


class ProcessInfo(ctypes.Structure):
    _fields_ = [('process', w.HANDLE), ('thread', w.HANDLE),
                ('pid', w.DWORD), ('tid', w.DWORD)]


class BasicLimits(ctypes.Structure):
    _fields_ = [('process_time', ctypes.c_int64), ('job_time', ctypes.c_int64),
                ('flags', w.DWORD), ('min_working_set', ctypes.c_size_t),
                ('max_working_set', ctypes.c_size_t), ('active_processes', w.DWORD),
                ('affinity', ctypes.c_size_t), ('priority', w.DWORD), ('scheduling', w.DWORD)]


class ExtendedLimits(ctypes.Structure):
    _fields_ = [('basic', BasicLimits), ('io', ctypes.c_uint64 * 6),
                ('process_memory', ctypes.c_size_t), ('job_memory', ctypes.c_size_t),
                ('peak_process_memory', ctypes.c_size_t), ('peak_job_memory', ctypes.c_size_t)]


def api():
    if os.name != 'nt':
        raise ValueError('Windows restricted-token acceptance requires native Windows')
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    advapi = ctypes.WinDLL('advapi32', use_last_error=True)
    declarations = [
        (kernel.GetCurrentProcess, [], w.HANDLE),
        (kernel.CloseHandle, [w.HANDLE], w.BOOL),
        (kernel.LocalFree, [ctypes.c_void_p], ctypes.c_void_p),
        (kernel.WaitForSingleObject, [w.HANDLE, w.DWORD], w.DWORD),
        (kernel.GetExitCodeProcess, [w.HANDLE, ctypes.POINTER(w.DWORD)], w.BOOL),
        (kernel.TerminateProcess, [w.HANDLE, w.UINT], w.BOOL),
        (kernel.CreateJobObjectW, [ctypes.c_void_p, w.LPCWSTR], w.HANDLE),
        (kernel.SetInformationJobObject, [w.HANDLE, ctypes.c_int, ctypes.c_void_p, w.DWORD], w.BOOL),
        (kernel.AssignProcessToJobObject, [w.HANDLE, w.HANDLE], w.BOOL),
        (kernel.ResumeThread, [w.HANDLE], w.DWORD),
        (advapi.OpenProcessToken, [w.HANDLE, w.DWORD, ctypes.POINTER(w.HANDLE)], w.BOOL),
        (advapi.GetTokenInformation, [w.HANDLE, ctypes.c_int, ctypes.c_void_p, w.DWORD,
                                     ctypes.POINTER(w.DWORD)], w.BOOL),
        (advapi.SetTokenInformation, [w.HANDLE, ctypes.c_int, ctypes.c_void_p, w.DWORD], w.BOOL),
        (advapi.ConvertStringSidToSidW, [w.LPCWSTR, ctypes.POINTER(ctypes.c_void_p)], w.BOOL),
        (advapi.ConvertSidToStringSidW, [ctypes.c_void_p, ctypes.POINTER(w.LPWSTR)], w.BOOL),
        (advapi.ConvertStringSecurityDescriptorToSecurityDescriptorW,
         [w.LPCWSTR, w.DWORD, ctypes.POINTER(ctypes.c_void_p), ctypes.c_void_p], w.BOOL),
        (advapi.GetSecurityDescriptorDacl,
         [ctypes.c_void_p, ctypes.POINTER(w.BOOL), ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(w.BOOL)], w.BOOL),
        (advapi.SetNamedSecurityInfoW,
         [w.LPWSTR, ctypes.c_int, w.DWORD, ctypes.c_void_p, ctypes.c_void_p,
          ctypes.c_void_p, ctypes.c_void_p], w.DWORD),
        (advapi.CreateRestrictedToken,
         [w.HANDLE, w.DWORD, w.DWORD, ctypes.c_void_p, w.DWORD, ctypes.c_void_p,
          w.DWORD, ctypes.POINTER(SidAttributes), ctypes.POINTER(w.HANDLE)], w.BOOL),
        (advapi.CreateProcessAsUserW,
         [w.HANDLE, w.LPCWSTR, w.LPWSTR, ctypes.c_void_p, ctypes.c_void_p, w.BOOL,
          w.DWORD, ctypes.c_void_p, w.LPCWSTR, ctypes.POINTER(StartupInfo), ctypes.POINTER(ProcessInfo)], w.BOOL),
    ]
    for function, arguments, result in declarations:
        function.argtypes, function.restype = arguments, result
    return kernel, advapi


def checked(result):
    if not result:
        raise ctypes.WinError(ctypes.get_last_error())


def caller_sid(kernel, advapi):
    token, size = w.HANDLE(), w.DWORD()
    checked(advapi.OpenProcessToken(kernel.GetCurrentProcess(), 8, ctypes.byref(token)))
    try:
        advapi.GetTokenInformation(token, 1, None, 0, ctypes.byref(size))
        buffer = ctypes.create_string_buffer(size.value)
        checked(advapi.GetTokenInformation(token, 1, buffer, size, ctypes.byref(size)))
        sid = ctypes.cast(buffer, ctypes.POINTER(SidAttributes)).contents.sid
        string = w.LPWSTR()
        checked(advapi.ConvertSidToStringSidW(sid, ctypes.byref(string)))
        try:
            return string.value
        finally:
            kernel.LocalFree(ctypes.cast(string, ctypes.c_void_p))
    finally:
        kernel.CloseHandle(token)


def logon_sid(kernel, advapi, token):
    size = w.DWORD()
    advapi.GetTokenInformation(token, 2, None, 0, ctypes.byref(size))
    buffer = ctypes.create_string_buffer(size.value)
    checked(advapi.GetTokenInformation(token, 2, buffer, size, ctypes.byref(size)))
    count = ctypes.cast(buffer, ctypes.POINTER(w.DWORD)).contents.value
    offset = ctypes.sizeof(ctypes.c_void_p)
    groups = (SidAttributes * count).from_address(ctypes.addressof(buffer) + offset)
    for group in groups:
        if group.attributes & 0xC0000000 == 0xC0000000:
            string = w.LPWSTR()
            checked(advapi.ConvertSidToStringSidW(group.sid, ctypes.byref(string)))
            try:
                return string.value
            finally:
                kernel.LocalFree(ctypes.cast(string, ctypes.c_void_p))
    raise ValueError('native acceptance requires the current logon SID')


def create_work(path=None):
    """Only apply an ACL to a directory created here, never an existing path."""
    kernel, advapi = api()
    if path is None:
        work = Path(tempfile.mkdtemp(prefix='crt dev external ')).resolve()
    else:
        work = Path(path).absolute()
        work.mkdir(parents=False, exist_ok=False)
        work = work.resolve()
    # Unique, unregistered SID: no account or global membership is created.
    parts = [int.from_bytes(uuid.uuid4().bytes[i:i + 4], 'little') for i in (0, 4, 8, 12)]
    sid = 'S-1-5-21-' + '-'.join(map(str, parts))
    descriptor, dacl = ctypes.c_void_p(), ctypes.c_void_p()
    sddl = 'D:P' + ''.join(f'(A;OICI;FA;;;{identity})' for identity in
                           (caller_sid(kernel, advapi), 'SY', 'BA', sid))
    checked(advapi.ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, 1, ctypes.byref(descriptor), None))
    try:
        present, defaulted = w.BOOL(), w.BOOL()
        checked(advapi.GetSecurityDescriptorDacl(descriptor, ctypes.byref(present), ctypes.byref(dacl), ctypes.byref(defaulted)))
        if not present.value or not dacl.value:
            raise ValueError('private work directory requires a non-null DACL')
        error = advapi.SetNamedSecurityInfoW(str(work), 1, 4 | 0x80000000, None, None, dacl, None)
        if error:
            raise ctypes.WinError(error)
    finally:
        kernel.LocalFree(descriptor)
    return work, sid


def run(command, *, work, sid, env, timeout=600):
    """Launch without inherited handles or an unrestricted fallback."""
    kernel, advapi = api()
    original, restricted = w.HANDLE(), w.HANDLE()
    allocations = []
    process = ProcessInfo()
    job = w.HANDLE()
    checked(advapi.OpenProcessToken(kernel.GetCurrentProcess(), 0x8B, ctypes.byref(original)))
    try:
        identities = ('S-1-1-0', 'S-1-5-32-545', logon_sid(kernel, advapi, original), sid)
        restrictions = (SidAttributes * len(identities))()
        for item, identity in zip(restrictions, identities):
            pointer = ctypes.c_void_p()
            checked(advapi.ConvertStringSidToSidW(identity, ctypes.byref(pointer)))
            allocations.append(pointer)
            item.sid, item.attributes = pointer.value, 0
        # DISABLE_MAX_PRIVILEGE, deliberately NOT WRITE_RESTRICTED.
        checked(advapi.CreateRestrictedToken(original, 1, 0, None, 0, None, len(identities),
                                            restrictions, ctypes.byref(restricted)))
        # Child-created process/thread/file objects must be accessible to that
        # same restricted token. This changes only the new token's default DACL.
        descriptor, dacl = ctypes.c_void_p(), ctypes.c_void_p()
        sddl = 'D:' + ''.join(f'(A;;GA;;;{identity})' for identity in
                              (caller_sid(kernel, advapi), 'SY', sid))
        checked(advapi.ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, 1, ctypes.byref(descriptor), None))
        allocations.append(descriptor)
        present, defaulted = w.BOOL(), w.BOOL()
        checked(advapi.GetSecurityDescriptorDacl(descriptor, ctypes.byref(present), ctypes.byref(dacl), ctypes.byref(defaulted)))
        if not present.value or not dacl.value:
            raise ValueError('restricted child objects require a non-null default DACL')
        checked(advapi.SetTokenInformation(restricted, 6, ctypes.byref(dacl), ctypes.sizeof(dacl)))
        startup = StartupInfo()
        startup.cb, startup.flags, startup.show = ctypes.sizeof(startup), 1, 0
        line = ctypes.create_unicode_buffer(subprocess.list2cmdline([str(p) for p in command]))
        environment = ctypes.create_unicode_buffer('\0'.join(f'{k}={v}' for k, v in sorted(env.items())) + '\0\0')
        job = kernel.CreateJobObjectW(None, None)
        checked(job)
        limits = ExtendedLimits()
        limits.basic.flags = 0x2000  # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
        checked(kernel.SetInformationJobObject(job, 9, ctypes.byref(limits), ctypes.sizeof(limits)))
        checked(advapi.CreateProcessAsUserW(restricted, str(command[0]), line, None, None,
                                           False, 0x08000000 | 0x400 | 4, environment, str(work),
                                           ctypes.byref(startup), ctypes.byref(process)))
        try:
            checked(kernel.AssignProcessToJobObject(job, process.process))
            if kernel.ResumeThread(process.thread) == 0xFFFFFFFF:
                raise ctypes.WinError(ctypes.get_last_error())
        except BaseException:
            kernel.TerminateProcess(process.process, 1)
            raise
        waited = kernel.WaitForSingleObject(process.process, int(timeout * 1000))
        if waited == 0x102:
            kernel.TerminateProcess(process.process, 1)
            kernel.WaitForSingleObject(process.process, 5000)
            raise subprocess.TimeoutExpired(command, timeout)
        if waited != 0:
            raise ctypes.WinError(ctypes.get_last_error())
        code = w.DWORD()
        checked(kernel.GetExitCodeProcess(process.process, ctypes.byref(code)))
        if code.value:
            raise subprocess.CalledProcessError(code.value, command)
    finally:
        for handle in (job, process.thread, process.process, restricted, original):
            if handle:
                kernel.CloseHandle(handle)
        for pointer in allocations:
            kernel.LocalFree(pointer)
