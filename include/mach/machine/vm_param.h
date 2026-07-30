#ifndef CRT_MACH_MACHINE_VM_PARAM_H
#define CRT_MACH_MACHINE_VM_PARAM_H

/*
 * Minimal Darwin VM page constants required by upstream libffi's arm64
 * trampoline-table source. These headers are CRT-owned compatibility surface;
 * the build must not include host SDK headers through the sysroot wrapper.
 */
#define PAGE_MAX_SHIFT 14
#define PAGE_MAX_SIZE (1 << PAGE_MAX_SHIFT)
#define PAGE_MAX_MASK (PAGE_MAX_SIZE - 1)

#define PAGE_MIN_SHIFT 12
#define PAGE_MIN_SIZE (1 << PAGE_MIN_SHIFT)
#define PAGE_MIN_MASK (PAGE_MIN_SIZE - 1)

#endif
