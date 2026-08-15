# Hello Bring-Up

**This document is a historical snapshot of the first executable milestone,
not a description of current libc/PAL capability.** Every "currently"/
"deferred" statement below is dated to that milestone. In particular:
`mprotect()` is implemented (not deferred); file-backed `mmap()` is
implemented (`libc/src/mman.c`); the stdio layer has grown a real `FILE`
ABI, buffering, memory streams, `stdio_ext`, and a gdtoa-backed
`printf`/`scanf` formatter family, not just the small bootstrap formatter
described here; and libc now covers process/fd/signal/pthread/shell surface
far beyond this initial subset list. For current status, see
`../porting_status.md` (third-party library porting) and
`../sysroot_ports.md` (sysroot/PAL policy and behavior-difference tables).
This file stays as a record of what the *first* milestone looked like, not
because it's wrong to read, just because reading it as "current" would be.

This document records the first executable milestone.

The current bring-up builds a minimal C hello program using:

- C99 source.
- `-ffreestanding`.
- `-fno-builtin`.
- `-ffunction-sections`.
- `-fdata-sections`.
- `-nostdlib`.
- `-nostartfiles`.
- `-nodefaultlibs`.
- linker section garbage collection:
  - `-Wl,--gc-sections` on ELF/Linux.
  - `-Wl,-dead_strip` on Mach-O/macOS.
  - `-Wl,/OPT:REF` on PE/COFF Windows.
- project-provided `crt1.o`.
- project-provided `libc.a`.
- project-provided `unistd.h`.
- compiler-rt builtins when the active Clang exposes them.
- a generated project sysroot under the build directory.

The current libc subset includes:

- `_exit`
- `errno`
- `read`
- `write`
- `open`
- `close`
- `lseek`
- `mmap`
- `munmap`
- `malloc`
- `free`
- `calloc`
- `realloc`
- `fputc`
- `fputs`
- `fopen`
- `fclose`
- `fseek`
- `ftell`
- `printf`
- `fprintf`
- `snprintf`
- `vsnprintf`
- `puts`
- `putchar`
- `fread`
- `fwrite`
- `fflush`
- `memcpy`
- `memmove`
- `memset`
- `strlen`
- `strcmp`

On Windows, `errno` uses a Win32 TLS slot instead of compiler PE TLS. This keeps
the freestanding startup path independent from `_tls_index` while still giving
each thread a separate `errno` storage location.

The allocator is currently a VM-backed bootstrap heap with a locked free list.
It grows with anonymous `mmap` chunks. This is enough for early cross-OS
libc/PAL tests, but it is not the final allocator design.

The VM layer currently supports anonymous private mappings. Linux and macOS use
direct syscalls. Windows uses `VirtualAlloc` and `VirtualFree` for anonymous
mappings. File-backed mappings are deferred.

The stdio layer is currently limited to standard streams, fd-backed file streams,
direct read/write backing, EOF/error state helpers, remove/rename, and a small
bootstrap formatter. A final `FILE` ABI, buffering model, and complete `printf`
formatter are deferred.

On macOS, the executable still links `libSystem.dylib` explicitly. This is a
platform loader requirement for normal Mach-O executables. The hello path itself
uses this project's `_start`, `write`, `_exit`, and direct Darwin syscall
wrappers rather than hosted libc startup.

The initial verified flow is:

```sh
cmake --preset macos-host-ninja-debug
cmake --build --preset macos-host-ninja-debug
ctest --preset macos-host-ninja-debug
cmake --build --preset macos-host-ninja-debug --target sysroot
```

The sysroot currently contains:

```text
sysroot/
  include/
    errno.h
    fcntl.h
    stdio.h
    string.h
    stdlib.h
    sys/mman.h
    sys/types.h
    unistd.h
  lib/
    crt1.o
    libc.a
    libclang_rt.builtins.a
```

The manual sysroot-consumer link shape is:

```sh
clang \
  -ffreestanding \
  -fno-builtin \
  -nostdlib \
  -nostartfiles \
  -nodefaultlibs \
  -I out/macos-host-ninja-debug/sysroot/include \
  out/macos-host-ninja-debug/sysroot/lib/crt1.o \
  tests/hello.c \
  out/macos-host-ninja-debug/sysroot/lib/libc.a \
  out/macos-host-ninja-debug/sysroot/lib/libclang_rt.builtins.a \
  -Wl,-e,_start \
  -lSystem \
  -o out/macos-host-ninja-debug/hello_sysroot
```

Future Linux bring-up should remove the macOS `libSystem` exception and verify a
fully project-owned startup/libc path for ELF executables.
