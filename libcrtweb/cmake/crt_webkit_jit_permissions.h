// Force-included (-include) into JavaScriptCore's C++ translation units by tools/build_webkit_jsc.py on
// macOS in baseline-jit mode. It uses the one extension point JavaScriptCore offers for W^X
// (JavaScriptCore/assembler/FastJITPermissions.h): defining both OS_THREAD_SELF_RESTRICT and
// OS_THREAD_SELF_RESTRICT_SUPPORTED. On Apple Silicon JIT memory is writable or executable per thread, and
// the CRT libc exposes that toggle as __crt_jit_write_protect(); JavaScriptCore then flips it around every
// write to executable memory exactly as it does for Darwin's own pthread_jit_write_protect_np.
// No WebKit source is involved. See libc/src/arch/macos/common/jit_permissions.c.
#pragma once

extern "C" int __crt_jit_write_protect_supported(void);
extern "C" void __crt_jit_write_protect(int enabled);

// `restriction` is a MemoryRestriction value (an enum declared in FastJITPermissions.h before either macro is
// used). Only RWX-to-RW and RWX-to-RX exist on this host.
#define OS_THREAD_SELF_RESTRICT_SUPPORTED(restriction) \
    (((restriction) == MemoryRestriction::kRwxToRw || (restriction) == MemoryRestriction::kRwxToRx) \
        && __crt_jit_write_protect_supported())
#define OS_THREAD_SELF_RESTRICT(restriction) \
    __crt_jit_write_protect((restriction) == MemoryRestriction::kRwxToRx)
