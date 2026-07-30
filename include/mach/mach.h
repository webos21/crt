#ifndef CRT_MACH_MACH_H
#define CRT_MACH_MACH_H

#include <stdint.h>
#include <mach/vm_param.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int kern_return_t;
typedef int boolean_t;
typedef uint32_t natural_t;
typedef uint32_t mach_port_t;
typedef mach_port_t mach_port_name_t;
typedef mach_port_t task_t;
typedef mach_port_t task_name_t;
typedef mach_port_t vm_map_t;
typedef uintptr_t vm_offset_t;
typedef uintptr_t vm_address_t;
typedef uintptr_t vm_size_t;
typedef int vm_prot_t;
typedef unsigned int vm_inherit_t;

#define KERN_SUCCESS 0

#define TRUE 1
#define FALSE 0

#define VM_PROT_NONE ((vm_prot_t)0x00)
#define VM_PROT_READ ((vm_prot_t)0x01)
#define VM_PROT_WRITE ((vm_prot_t)0x02)
#define VM_PROT_EXECUTE ((vm_prot_t)0x04)
#define VM_PROT_DEFAULT (VM_PROT_READ | VM_PROT_WRITE)

#define VM_FLAGS_ANYWHERE 0x00000001
#define VM_FLAGS_OVERWRITE 0x00004000

#define VM_INHERIT_SHARE ((vm_inherit_t)0)

extern mach_port_t mach_task_self_;
#define mach_task_self() mach_task_self_
#define current_task() mach_task_self()

kern_return_t vm_allocate(vm_map_t target_task, vm_address_t* address, vm_size_t size, int flags);
kern_return_t vm_deallocate(vm_map_t target_task, vm_address_t address, vm_size_t size);
kern_return_t vm_protect(vm_map_t target_task, vm_address_t address, vm_size_t size,
                         boolean_t set_maximum, vm_prot_t new_protection);
kern_return_t vm_remap(vm_map_t target_task, vm_address_t* target_address, vm_size_t size,
                       vm_address_t mask, int flags, vm_map_t src_task,
                       vm_address_t src_address, boolean_t copy, vm_prot_t* cur_protection,
                       vm_prot_t* max_protection, vm_inherit_t inheritance);

#ifdef __cplusplus
}
#endif

#endif
