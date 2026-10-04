/* Native ELF thread-local storage for CRT-created threads on Linux.
 *
 * pthread_create() starts a thread with a raw clone(2). Without CLONE_SETTLS the
 * child keeps the creator's thread pointer, so every `thread_local`/`__thread`
 * variable (compiled with the native TLS models) is shared by all CRT threads:
 * one address, values bleeding between threads. This file gives each new thread a
 * thread pointer of its own, laid out the way the process's dynamic loader laid out
 * the initial thread's, so code compiled for the initial-exec/local-exec models
 * (and the dtv-based general-dynamic model for the modules loaded at startup) works
 * unchanged in the new thread.
 *
 * It is deliberately built from public, documented structure only -- the SVR4
 * r_debug/link_map rendezvous list, ELF PT_TLS program headers, and the TLS ABI's
 * thread pointer/dtv rules (Drepper, "ELF Handling For Thread-Local Storage",
 * x86-64 variant II) -- not from loader internals:
 *   - the modules that own a TLS segment are the PT_TLS-bearing entries of the
 *     link-map chain, in chain (= load) order, which is their module-id order: the
 *     k-th such module is dtv[k], and dtv[k].pointer.val is the address of its block
 *     in the initial thread -- so each module's exact thread-pointer offset is read,
 *     not recomputed;
 *   - each block is initialised from the module's own PT_TLS image (p_filesz bytes
 *     of .tdata, then zeroes up to p_memsz), never from the initial thread's current
 *     (possibly modified) copy;
 *   - the thread control block header (tcb/dtv/self pointers, stack-guard canary, ...)
 *     is copied from the initial thread and its self and dtv pointers are re-aimed.
 * Anything that does not look exactly as expected (a missing r_debug, a module
 * count that disagrees with the dtv, a block outside the static area) makes the setup
 * decline, and the thread then starts as it always did.
 *
 * The glibc dynamic loader (/lib64/ld-linux-x86-64.so.2 or /lib/ld-linux-aarch64.so.1,
 * the loader every Linux CRT executable uses today) on x86_64 and aarch64.
 *
 * x86_64 is TLS variant II: the thread pointer addresses the control block, which sits
 * ABOVE the static TLS blocks (blocks are at negative offsets, dtv at tp+8).
 *
 * aarch64 is TLS variant I (tpidr_el0): the thread pointer addresses a 16-byte header
 * (dtv, private) and the static blocks follow it at POSITIVE offsets, while the loader's
 * thread descriptor (glibc's struct pthread) lies BELOW the thread pointer. Block offsets
 * are still read from the initial thread's dtv rather than recomputed. The descriptor's
 * size is a loader internal that is not exported, so the window below the thread pointer
 * is copied at the same thread-pointer-relative offsets the loader's own code uses (a
 * fixed upper bound, clamped to the pages that are actually mapped); every field the
 * loader reads there keeps its offset from tp whatever the size is.
 *
 * Modules loaded later with dlopen are not covered: CRT's dlopen is a
 * stub and a dynamically-added TLS module has no static block to clone. */

#include <elf.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <private/crt_atomic.h>
#include <private/crt_thread_tls.h>

#if defined(__x86_64__) || defined(__aarch64__)

#define CRT_TLS_MAX_MODULES 32
/* Bytes of the control block header copied from the initial thread. The loader's
 * control block (glibc's struct pthread) is 2304 bytes and is always followed by
 * nothing the new thread needs; copying through offset 0x700 covers every field
 * code in a CRT process reads (tcb/dtv/self, the stack-guard canary, the pointer
 * guard, feature flags, the tid fields). */
#define CRT_TLS_TCB_COPY 0x700
#define CRT_TLS_ALIGN 64

#if defined(__aarch64__)
/* The 16-byte variant I header at the thread pointer (dtv, private). */
#define CRT_TLS_TCB_SIZE 16
/* Upper bound of the bytes below the thread pointer copied from the initial thread
 * (glibc's struct pthread is 0x720 there today; see the file comment). */
#define CRT_TLS_PRE_TCB_MAX 0x1000
/* Fewer mapped bytes than this below the thread pointer cannot hold the descriptor. */
#define CRT_TLS_PRE_TCB_MIN 0x100
#endif

typedef union {
  size_t counter;
  struct {
    void* val;
    void* to_free;
  } pointer;
} crt_dtv_t;

struct crt_link_map {
  Elf64_Addr l_addr;
  const char* l_name;
  const Elf64_Dyn* l_ld;
  struct crt_link_map* l_next;
  struct crt_link_map* l_prev;
};

struct crt_r_debug {
  int r_version;
  struct crt_link_map* r_map;
};

struct crt_tls_module {
  const unsigned char* image;
  size_t filesz;
  size_t memsz;
  size_t distance; /* x86_64: tp minus the block address; aarch64: block address minus tp. */
};

static crt_spinlock tls_lock = CRT_SPINLOCK_INIT;
static int tls_state; /* 0 = not tried, 1 = ready, -1 = declined. */
static struct crt_tls_module tls_modules[CRT_TLS_MAX_MODULES];
static size_t tls_module_count;
static size_t tls_dtv_slots;
static size_t tls_dtv_generation;
static size_t tls_static_size;
#if defined(__aarch64__)
static size_t tls_pre_size;   /* bytes copied from below the initial thread's pointer. */
static size_t tls_align = CRT_TLS_ALIGN; /* thread-pointer alignment (>= every p_align). */
#endif

unsigned long getauxval(unsigned long type);
int mincore(void* addr, size_t length, unsigned char* vector);

static uintptr_t current_thread_pointer(void) {
  uintptr_t tp;

#if defined(__aarch64__)
  __asm__ volatile("mrs %0, tpidr_el0" : "=r"(tp));
#else
  __asm__ volatile("movq %%fs:0, %0" : "=r"(tp));
#endif
  return tp;
}

/* The dtv pointer of the thread whose thread pointer is `tp`. */
static const crt_dtv_t* thread_dtv(uintptr_t tp) {
#if defined(__aarch64__)
  return *(const crt_dtv_t* const*)tp;
#else
  return *(const crt_dtv_t* const*)(tp + 8);
#endif
}

static size_t round_up(size_t value, size_t alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}

/* PT_TLS of the object whose program headers are at `phdr`, or 0. */
static const Elf64_Phdr* find_tls_phdr(const Elf64_Phdr* phdr, size_t count) {
  size_t i;

  for (i = 0; i < count; ++i) {
    if (phdr[i].p_type == PT_TLS) {
      return &phdr[i];
    }
  }
  return 0;
}

static const struct crt_r_debug* find_r_debug(const Elf64_Phdr* exe_phdr, size_t exe_count,
                                              uintptr_t exe_phdr_addr) {
  size_t i;
  uintptr_t bias = 0;
  const Elf64_Dyn* dyn = 0;

  for (i = 0; i < exe_count; ++i) {
    if (exe_phdr[i].p_type == PT_PHDR) {
      bias = exe_phdr_addr - exe_phdr[i].p_vaddr;
    }
  }
  for (i = 0; i < exe_count; ++i) {
    if (exe_phdr[i].p_type == PT_DYNAMIC) {
      dyn = (const Elf64_Dyn*)(bias + exe_phdr[i].p_vaddr);
    }
  }
  for (; dyn != 0 && dyn->d_tag != DT_NULL; ++dyn) {
    if (dyn->d_tag == DT_DEBUG) {
      return (const struct crt_r_debug*)(uintptr_t)dyn->d_un.d_ptr;
    }
  }
  return 0;
}

static int add_module(const Elf64_Phdr* tls, uintptr_t bias, uintptr_t tp, const crt_dtv_t* dtv) {
  struct crt_tls_module* module;
  uintptr_t block;

  if (tls_module_count >= CRT_TLS_MAX_MODULES || tls_module_count + 1 > tls_dtv_slots) {
    return -1;
  }
  block = (uintptr_t)dtv[tls_module_count + 1].pointer.val;
#if defined(__aarch64__)
  /* Variant I: a static-TLS block lies ABOVE the 16-byte header at the thread pointer,
   * inside the area the loader reserved, and is aligned as its segment asks. */
  if (block == 0 || block == (uintptr_t)-1L || block < tp + CRT_TLS_TCB_SIZE ||
      (block - tp) > (1UL << 24) || (tls->p_align != 0 && (block % tls->p_align) != 0) ||
      tls->p_memsz > (1UL << 24)) {
    return -1;
  }
  module = &tls_modules[tls_module_count++];
  module->image = (const unsigned char*)(bias + tls->p_vaddr);
  module->filesz = tls->p_filesz;
  module->memsz = tls->p_memsz;
  module->distance = block - tp;
  if (module->distance + module->memsz > tls_static_size) {
    tls_static_size = module->distance + module->memsz;
  }
  if (tls->p_align > tls_align) {
    tls_align = tls->p_align;
  }
  return 0;
#else
  /* A static-TLS block lies below the thread pointer (variant II), inside the
   * area the loader reserved, and is aligned as its segment asks. */
  if (block == 0 || block >= tp || (tp - block) > (1UL << 24) ||
      (tls->p_align != 0 && (block % tls->p_align) != 0) || (tp - block) < tls->p_memsz) {
    return -1;
  }
  module = &tls_modules[tls_module_count++];
  module->image = (const unsigned char*)(bias + tls->p_vaddr);
  module->filesz = tls->p_filesz;
  module->memsz = tls->p_memsz;
  module->distance = tp - block;
  if (module->distance > tls_static_size) {
    tls_static_size = module->distance;
  }
  return 0;
#endif
}

#if defined(__aarch64__)
/* How many bytes immediately below `tp` (up to `want`) lie in mapped pages. mincore()
 * fails with ENOMEM for an unmapped page instead of faulting, so this never touches
 * memory it has not proved mapped. */
static size_t mapped_bytes_below(uintptr_t tp, size_t want) {
  uintptr_t page = (uintptr_t)getauxval(6 /* AT_PAGESZ */);
  uintptr_t addr;
  size_t covered;
  unsigned char vec;

  if (page == 0 || (page & (page - 1)) != 0) {
    return 0;
  }
  addr = tp & ~(page - 1);
  covered = tp - addr; /* the part of tp's own page below tp */
  while (covered < want && addr >= page) {
    addr -= page;
    if (mincore((void*)addr, page, &vec) != 0) {
      break;
    }
    covered = tp - addr;
  }
  return covered < want ? covered : want;
}
#endif

static int tls_discover(void) {
  uintptr_t tp = current_thread_pointer();
  const crt_dtv_t* dtv;
  const Elf64_Phdr* exe_phdr = (const Elf64_Phdr*)getauxval(3 /* AT_PHDR */);
  size_t exe_count = (size_t)getauxval(5 /* AT_PHNUM */);
  const struct crt_r_debug* r_debug;
  const struct crt_link_map* map;
  const Elf64_Phdr* tls;
  uintptr_t exe_bias = 0;
  size_t i;

  if (tp == 0 || exe_phdr == 0 || exe_count == 0) {
    return -1;
  }
  dtv = thread_dtv(tp);
  if (dtv == 0) {
    return -1;
  }
  tls_dtv_slots = dtv[-1].counter;
  tls_dtv_generation = dtv[0].counter;
  if (tls_dtv_slots == 0 || tls_dtv_slots > 4096) {
    return -1;
  }
  r_debug = find_r_debug(exe_phdr, exe_count, (uintptr_t)exe_phdr);
  if (r_debug == 0) {
    return -1;
  }
  for (i = 0; i < exe_count; ++i) {
    if (exe_phdr[i].p_type == PT_PHDR) {
      exe_bias = (uintptr_t)exe_phdr - exe_phdr[i].p_vaddr;
    }
  }
  for (map = r_debug->r_map; map != 0; map = map->l_next) {
    const Elf64_Phdr* phdr;
    size_t count;
    uintptr_t bias;

    if (map->l_ld == 0) {
      continue;
    }
    if (map->l_name == 0 || map->l_name[0] == 0) {
      /* The main executable: its program headers come from the auxiliary vector. */
      phdr = exe_phdr;
      count = exe_count;
      bias = exe_bias;
    } else {
      const Elf64_Ehdr* header = (const Elf64_Ehdr*)map->l_addr;

      if (map->l_addr == 0 || memcmp(header->e_ident, ELFMAG, SELFMAG) != 0 ||
          header->e_phoff == 0) {
        continue; /* e.g. the vDSO, which has no TLS. */
      }
      phdr = (const Elf64_Phdr*)((const char*)header + header->e_phoff);
      count = header->e_phnum;
      bias = map->l_addr;
    }
    tls = find_tls_phdr(phdr, count);
    if (tls != 0 && add_module(tls, bias, tp, dtv) != 0) {
      return -1;
    }
  }
  if (tls_module_count == 0) {
    return -1;
  }
  /* Every slot the loader filled for a static block must have been accounted for. */
  for (i = tls_module_count + 1; i <= tls_dtv_slots && i <= tls_module_count + 4; ++i) {
    if (dtv[i].pointer.val != 0 && dtv[i].pointer.val != (void*)-1L) {
      return -1; /* a block we did not account for. */
    }
  }
#if defined(__aarch64__)
  /* The thread pointer of every new thread must be aligned for the strictest module. */
  if (tls_align > 4096 || (tls_align & (tls_align - 1)) != 0 || (tp & (tls_align - 1)) != 0) {
    return -1;
  }
  tls_pre_size = mapped_bytes_below(tp, CRT_TLS_PRE_TCB_MAX);
  if (tls_pre_size < CRT_TLS_PRE_TCB_MIN) {
    return -1;
  }
  tls_pre_size &= ~(size_t)15;
#endif
  return 0;
}

static int tls_ready(void) {
  crt_spin_lock(&tls_lock);
  if (tls_state == 0) {
    tls_state = tls_discover() == 0 ? 1 : -1;
  }
  crt_spin_unlock(&tls_lock);
  return tls_state == 1;
}

#if defined(__aarch64__)

size_t __crt_linux_thread_tls_size(void) {
  if (!tls_ready()) {
    return 0;
  }
  /* descriptor window + thread-pointer alignment slack + static blocks + dtv with its
   * length slot, plus the 64-byte rounding of the stack end pthread_create() applies. */
  return tls_pre_size + tls_align + round_up(tls_static_size, 16) +
         (tls_dtv_slots + 2) * sizeof(crt_dtv_t) + CRT_TLS_ALIGN;
}

void* __crt_linux_thread_tls_setup(void* region_end) {
  uintptr_t tp0;
  uintptr_t start;
  uintptr_t tp;
  size_t size = __crt_linux_thread_tls_size();
  crt_dtv_t* dtv;
  size_t i;

  if (size == 0) {
    return 0;
  }
  /* `start` is the same address pthread_create() uses as the top of the new stack, so
   * nothing below is ever shared with the stack. */
  start = ((uintptr_t)region_end - size) & ~(uintptr_t)(CRT_TLS_ALIGN - 1);
  tp = round_up(start + tls_pre_size, tls_align);
  tp0 = current_thread_pointer();
  memset((void*)start, 0, (size_t)((uintptr_t)region_end - start));
  /* The loader's thread descriptor, at the same offsets from the thread pointer. */
  memcpy((void*)(tp - tls_pre_size), (const void*)(tp0 - tls_pre_size), tls_pre_size);
  /* Header (dtv, private): copy, then re-aim the dtv. */
  memcpy((void*)tp, (const void*)tp0, CRT_TLS_TCB_SIZE);
  dtv = (crt_dtv_t*)(tp + round_up(tls_static_size, 16) + sizeof(crt_dtv_t));
  dtv[-1].counter = tls_dtv_slots;
  dtv[0].counter = tls_dtv_generation;
  for (i = 0; i < tls_module_count; ++i) {
    unsigned char* block = (unsigned char*)(tp + tls_modules[i].distance);

    memcpy(block, tls_modules[i].image, tls_modules[i].filesz);
    /* The remainder of the block (.tbss) is already zero from the memset. */
    dtv[i + 1].pointer.val = block;
    dtv[i + 1].pointer.to_free = 0;
  }
  *(void**)tp = (void*)dtv; /* this thread's own dtv. */
  return (void*)tp;
}

#else

size_t __crt_linux_thread_tls_size(void) {
  if (!tls_ready()) {
    return 0;
  }
  return round_up(tls_static_size, CRT_TLS_ALIGN) + CRT_TLS_TCB_COPY +
         (tls_dtv_slots + 2) * sizeof(crt_dtv_t) + CRT_TLS_ALIGN;
}

void* __crt_linux_thread_tls_setup(void* region_end) {
  uintptr_t tp0;
  uintptr_t tp;
  uintptr_t base;
  size_t size = __crt_linux_thread_tls_size();
  const crt_dtv_t* dtv0;
  crt_dtv_t* dtv;
  size_t i;

  if (size == 0) {
    return 0;
  }
  base = ((uintptr_t)region_end - size) & ~(uintptr_t)(CRT_TLS_ALIGN - 1);
  tp = base + round_up(tls_static_size, CRT_TLS_ALIGN);
  tp0 = current_thread_pointer();
  dtv0 = thread_dtv(tp0);
  /* Control block header, then the dtv after it. */
  memset((void*)base, 0, size);
  memcpy((void*)tp, (const void*)tp0, CRT_TLS_TCB_COPY);
  dtv = (crt_dtv_t*)(tp + CRT_TLS_TCB_COPY + sizeof(crt_dtv_t));
  dtv[-1].counter = tls_dtv_slots;
  dtv[0].counter = tls_dtv_generation;
  for (i = 0; i < tls_module_count; ++i) {
    unsigned char* block = (unsigned char*)(tp - tls_modules[i].distance);

    memcpy(block, tls_modules[i].image, tls_modules[i].filesz);
    /* The remainder of the block (.tbss) is already zero from the memset. */
    dtv[i + 1].pointer.val = block;
    dtv[i + 1].pointer.to_free = 0;
  }
  (void)dtv0;
  *(uintptr_t*)tp = tp;              /* tcb: the thread pointer points at itself. */
  *(void**)(tp + 8) = (void*)dtv;    /* this thread's own dtv. */
  *(uintptr_t*)(tp + 16) = tp;       /* self pointer. */
  return (void*)tp;
}

#endif

#else

size_t __crt_linux_thread_tls_size(void) {
  return 0;
}

void* __crt_linux_thread_tls_setup(void* region_end) {
  (void)region_end;
  return 0;
}

#endif
