#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <private/crt_macho_symbol.h>

/* See libc/include/private/crt_macho_symbol.h for why this engine lives
 * here and is shared between libdl and libc's own macOS signal backend.
 * Only 64-bit Mach-O is handled, matching this project's x86_64/aarch64-only
 * scope. */

struct crt_mach_header_64 {
  uint32_t magic;
  int32_t cputype;
  int32_t cpusubtype;
  uint32_t filetype;
  uint32_t ncmds;
  uint32_t sizeofcmds;
  uint32_t flags;
  uint32_t reserved;
};

#define CRT_MH_MAGIC_64 0xfeedfacfU

struct crt_load_command {
  uint32_t cmd;
  uint32_t cmdsize;
};

struct crt_segment_command_64 {
  uint32_t cmd;
  uint32_t cmdsize;
  char segname[16];
  uint64_t vmaddr;
  uint64_t vmsize;
  uint64_t fileoff;
  uint64_t filesize;
  int32_t maxprot;
  int32_t initprot;
  uint32_t nsects;
  uint32_t flags;
};

struct crt_linkedit_data_command {
  uint32_t cmd;
  uint32_t cmdsize;
  uint32_t dataoff;
  uint32_t datasize;
};

struct crt_dyld_info_command {
  uint32_t cmd;
  uint32_t cmdsize;
  uint32_t rebase_off;
  uint32_t rebase_size;
  uint32_t bind_off;
  uint32_t bind_size;
  uint32_t weak_bind_off;
  uint32_t weak_bind_size;
  uint32_t lazy_bind_off;
  uint32_t lazy_bind_size;
  uint32_t export_off;
  uint32_t export_size;
};

struct crt_dylib {
  uint32_t name_offset;
  uint32_t timestamp;
  uint32_t current_version;
  uint32_t compatibility_version;
};

struct crt_dylib_command {
  uint32_t cmd;
  uint32_t cmdsize;
  struct crt_dylib dylib;
};

#define CRT_LC_SEGMENT_64 0x19U
#define CRT_LC_LOAD_DYLIB 0x0CU
#define CRT_LC_LOAD_WEAK_DYLIB 0x80000018U
#define CRT_LC_REEXPORT_DYLIB 0x8000001FU
#define CRT_LC_LOAD_UPWARD_DYLIB 0x80000023U
#define CRT_LC_DYLD_INFO 0x22U
#define CRT_LC_DYLD_INFO_ONLY 0x80000022U
#define CRT_LC_DYLD_EXPORTS_TRIE 0x80000033U

#define CRT_EXPORT_SYMBOL_FLAGS_KIND_MASK 0x03U
#define CRT_EXPORT_SYMBOL_FLAGS_KIND_REGULAR 0x00U
#define CRT_EXPORT_SYMBOL_FLAGS_KIND_ABSOLUTE 0x02U
#define CRT_EXPORT_SYMBOL_FLAGS_REEXPORT 0x08U
#define CRT_EXPORT_SYMBOL_FLAGS_STUB_AND_RESOLVER 0x10U

#define CRT_DL_REEXPORT_MAX_DEPTH 8
#define CRT_TRIE_MAX_VISITED 128

/* dyld image introspection: public, non-deprecated, and shared-cache-aware
 * (unlike the legacy NSAddImage/NSLookupSymbolInImage API). */
extern uint32_t _dyld_image_count(void);
extern const struct crt_mach_header_64* _dyld_get_image_header(uint32_t image_index);
extern const char* _dyld_get_image_name(uint32_t image_index);
extern intptr_t _dyld_get_image_vmaddr_slide(uint32_t image_index);

static void make_mach_symbol(const char* symbol, char* buffer, size_t size) {
  if (size == 0) {
    return;
  }
  buffer[0] = '_';
  if (size > 1) {
    size_t i;
    for (i = 1; i + 1 < size && symbol[i - 1] != '\0'; ++i) {
      buffer[i] = symbol[i - 1];
    }
    buffer[i] = '\0';
  }
}

static uint64_t read_uleb128(const uint8_t** pp, const uint8_t* end, int* ok) {
  uint64_t result = 0;
  unsigned int shift = 0;
  const uint8_t* p = *pp;

  while (p < end) {
    uint8_t byte = *p++;

    result |= (uint64_t)(byte & 0x7f) << shift;
    if ((byte & 0x80) == 0) {
      *pp = p;
      *ok = 1;
      return result;
    }
    shift += 7;
    if (shift > 63) {
      break;
    }
  }
  *ok = 0;
  return 0;
}

struct crt_macho_export_info {
  const uint8_t* trie_start;
  const uint8_t* trie_end;
};

/* Finds `header` among dyld's currently loaded images and returns its real
 * ASLR/shared-cache slide, or 0 if `header` is not a currently loaded image
 * (a defensive fallback; every header this file passes in came from dyld's
 * own image list or from an NSAddImage() result, so this should always
 * succeed in practice). */
static intptr_t slide_for_header(const struct crt_mach_header_64* header) {
  uint32_t count = _dyld_image_count();
  uint32_t i;

  for (i = 0; i < count; ++i) {
    if (_dyld_get_image_header(i) == header) {
      return _dyld_get_image_vmaddr_slide(i);
    }
  }
  return 0;
}

/* Locates __LINKEDIT and the export trie (LC_DYLD_EXPORTS_TRIE, falling back
 * to the older LC_DYLD_INFO[_ONLY] export_off/export_size) for a loaded
 * 64-bit Mach-O image. Returns 0 if this image has none (not a 64-bit
 * Mach-O, or predates both dyld export mechanisms). */
static int describe_exports(const struct crt_mach_header_64* header, struct crt_macho_export_info* out) {
  const uint8_t* cursor;
  uint32_t i;
  uint64_t linkedit_vmaddr = 0;
  uint64_t linkedit_fileoff = 0;
  int have_linkedit = 0;
  uint32_t trie_dataoff = 0;
  uint32_t trie_datasize = 0;
  int have_trie_cmd = 0;
  uint32_t info_export_off = 0;
  uint32_t info_export_size = 0;
  const uint8_t* linkedit_base;
  intptr_t slide;

  if (header == 0 || header->magic != CRT_MH_MAGIC_64) {
    return 0;
  }
  slide = slide_for_header(header);

  cursor = (const uint8_t*)header + sizeof(struct crt_mach_header_64);
  for (i = 0; i < header->ncmds; ++i) {
    const struct crt_load_command* lc = (const struct crt_load_command*)cursor;

    if (lc->cmdsize < sizeof(struct crt_load_command)) {
      return 0;
    }
    if (lc->cmd == CRT_LC_SEGMENT_64) {
      const struct crt_segment_command_64* seg = (const struct crt_segment_command_64*)cursor;

      if (memcmp(seg->segname, "__LINKEDIT", 11) == 0) {
        linkedit_vmaddr = seg->vmaddr;
        linkedit_fileoff = seg->fileoff;
        have_linkedit = 1;
      }
    } else if (lc->cmd == CRT_LC_DYLD_EXPORTS_TRIE) {
      const struct crt_linkedit_data_command* led = (const struct crt_linkedit_data_command*)cursor;

      trie_dataoff = led->dataoff;
      trie_datasize = led->datasize;
      have_trie_cmd = 1;
    } else if (lc->cmd == CRT_LC_DYLD_INFO || lc->cmd == CRT_LC_DYLD_INFO_ONLY) {
      const struct crt_dyld_info_command* dic = (const struct crt_dyld_info_command*)cursor;

      info_export_off = dic->export_off;
      info_export_size = dic->export_size;
    }
    cursor += lc->cmdsize;
  }

  if (!have_linkedit) {
    return 0;
  }
  if (have_trie_cmd && trie_datasize != 0) {
    /* keep dataoff/datasize as-is */
  } else if (info_export_size != 0) {
    trie_dataoff = info_export_off;
    trie_datasize = info_export_size;
  } else {
    return 0;
  }

  /* runtime_address(file_offset X) = __LINKEDIT.vmaddr + slide -
   * __LINKEDIT.fileoff + X. This needs the image's real ASLR/shared-cache
   * slide from dyld; it is NOT simply "header's own runtime address", which
   * only happens to work when __TEXT.vmaddr is 0 (true for a traditional
   * standalone PIE dylib, but false for a dylib living in the dyld shared
   * cache, where __TEXT.vmaddr is some large cache-relative base instead). */
  linkedit_base = (const uint8_t*)(linkedit_vmaddr + (uint64_t)slide - linkedit_fileoff);
  out->trie_start = linkedit_base + trie_dataoff;
  out->trie_end = out->trie_start + trie_datasize;
  return 1;
}

/* Reimplementation of dyld's MachOLoaded::trieWalk(): descends the export
 * trie matching `symbol` one edge at a time, returning a pointer to the
 * start of the matching terminal node's data, or 0 if not found or if the
 * trie data looks malformed. */
static const uint8_t* trie_walk(const uint8_t* start, const uint8_t* end, const char* symbol) {
  const uint8_t* p = start;
  uint32_t visited[CRT_TRIE_MAX_VISITED];
  int visited_count = 0;

  visited[visited_count++] = 0;
  while (p < end) {
    uint64_t terminal_size;
    const uint8_t* children;
    uint8_t children_remaining;
    uint64_t node_offset = 0;
    int ok;

    terminal_size = *p;
    if (terminal_size > 127) {
      terminal_size = read_uleb128(&p, end, &ok);
      if (!ok) {
        return 0;
      }
    } else {
      ++p;
    }

    if (*symbol == '\0' && terminal_size != 0) {
      return p;
    }

    children = p + terminal_size;
    if (children >= end) {
      return 0;
    }
    children_remaining = *children++;
    p = children;

    for (; children_remaining > 0; --children_remaining) {
      const char* s = symbol;
      int wrong_edge = 0;
      uint8_t c;

      if (p >= end) {
        return 0;
      }
      c = *p;
      while (c != '\0') {
        if (!wrong_edge) {
          if (c != (uint8_t)*s) {
            wrong_edge = 1;
          } else {
            ++s;
          }
        }
        ++p;
        if (p >= end) {
          return 0;
        }
        c = *p;
      }
      ++p; /* skip the edge label's terminating NUL */

      if (wrong_edge) {
        while (p < end && (*p & 0x80) != 0) {
          ++p;
        }
        if (p >= end) {
          return 0;
        }
        ++p; /* skip the final (non-continuation) uleb128 byte */
      } else {
        node_offset = read_uleb128(&p, end, &ok);
        if (!ok || node_offset == 0 || start + node_offset >= end) {
          return 0;
        }
        symbol = s;
        break;
      }
    }

    if (node_offset != 0) {
      int i;

      for (i = 0; i < visited_count; ++i) {
        if (visited[i] == node_offset) {
          return 0; /* cycle in malformed/hostile trie data */
        }
      }
      if (visited_count >= CRT_TRIE_MAX_VISITED) {
        return 0;
      }
      visited[visited_count++] = (uint32_t)node_offset;
      p = start + node_offset;
    } else {
      p = end;
    }
  }
  return 0;
}

static const char* dylib_name_at_ordinal(const struct crt_mach_header_64* header, uint32_t ordinal) {
  const uint8_t* cursor = (const uint8_t*)header + sizeof(struct crt_mach_header_64);
  uint32_t i;
  uint32_t seen = 0;

  for (i = 0; i < header->ncmds; ++i) {
    const struct crt_load_command* lc = (const struct crt_load_command*)cursor;

    if (lc->cmd == CRT_LC_LOAD_DYLIB || lc->cmd == CRT_LC_LOAD_WEAK_DYLIB ||
        lc->cmd == CRT_LC_REEXPORT_DYLIB || lc->cmd == CRT_LC_LOAD_UPWARD_DYLIB) {
      ++seen;
      if (seen == ordinal) {
        const struct crt_dylib_command* dc = (const struct crt_dylib_command*)cursor;

        return (const char*)cursor + dc->dylib.name_offset;
      }
    }
    cursor += lc->cmdsize;
  }
  return 0;
}

static const struct crt_mach_header_64* find_loaded_image_by_name(const char* name) {
  uint32_t count = _dyld_image_count();
  uint32_t i;
  size_t name_len;

  if (name == 0) {
    return 0;
  }
  name_len = strlen(name);
  for (i = 0; i < count; ++i) {
    const char* loaded_name = _dyld_get_image_name(i);
    size_t loaded_len;

    if (loaded_name == 0) {
      continue;
    }
    if (strcmp(loaded_name, name) == 0) {
      return _dyld_get_image_header(i);
    }
    /* Fall back to a basename match: install names and dlopen()-requested
     * paths do not always agree on the leading directory. */
    loaded_len = strlen(loaded_name);
    if (loaded_len >= name_len &&
        strcmp(loaded_name + (loaded_len - name_len), name) == 0 &&
        (loaded_len == name_len || loaded_name[loaded_len - name_len - 1] == '/')) {
      return _dyld_get_image_header(i);
    }
  }
  return 0;
}

static void* find_exported_symbol_in_image(const struct crt_mach_header_64* header, const char* mach_symbol, int depth);

/* Umbrella-style libraries such as libSystem.B.dylib re-export virtually
 * everything through LC_REEXPORT_DYLIB dependencies (libsystem_kernel.dylib,
 * libsystem_c.dylib, libsystem_pthread.dylib, ...) rather than listing every
 * symbol individually in their own trie -- libSystem.B.dylib's own export
 * trie is only ~120 bytes despite exposing thousands of symbols. This is a
 * *different* mechanism from an individual trie node carrying
 * CRT_EXPORT_SYMBOL_FLAGS_REEXPORT (handled separately below): here the
 * symbol is not present in this image's trie at all, so every re-exported
 * dependency's own trie must be searched in turn, matching dyld's own
 * MachOLoaded::findExportedSymbol() fallback. */
static void* find_in_reexported_dylibs(const struct crt_mach_header_64* header, const char* mach_symbol, int depth) {
  const uint8_t* cursor = (const uint8_t*)header + sizeof(struct crt_mach_header_64);
  uint32_t i;

  if (depth >= CRT_DL_REEXPORT_MAX_DEPTH) {
    return 0;
  }
  for (i = 0; i < header->ncmds; ++i) {
    const struct crt_load_command* lc = (const struct crt_load_command*)cursor;

    if (lc->cmd == CRT_LC_REEXPORT_DYLIB) {
      const struct crt_dylib_command* dc = (const struct crt_dylib_command*)cursor;
      const char* dep_name = (const char*)cursor + dc->dylib.name_offset;
      const struct crt_mach_header_64* dep_header = find_loaded_image_by_name(dep_name);

      if (dep_header != 0 && dep_header != header) {
        void* result = find_exported_symbol_in_image(dep_header, mach_symbol, depth + 1);

        if (result != 0) {
          return result;
        }
      }
    }
    cursor += lc->cmdsize;
  }
  return 0;
}

static void* find_exported_symbol_in_image(const struct crt_mach_header_64* header, const char* mach_symbol, int depth) {
  struct crt_macho_export_info info;
  const uint8_t* node;
  const uint8_t* p;
  uint64_t flags;
  int ok;

  if (header == 0 || depth > CRT_DL_REEXPORT_MAX_DEPTH || !describe_exports(header, &info)) {
    return 0;
  }
  node = trie_walk(info.trie_start, info.trie_end, mach_symbol);
  if (node == 0) {
    return find_in_reexported_dylibs(header, mach_symbol, depth);
  }
  p = node;
  flags = read_uleb128(&p, info.trie_end, &ok);
  if (!ok) {
    return 0;
  }

  if ((flags & CRT_EXPORT_SYMBOL_FLAGS_REEXPORT) != 0) {
    uint64_t ordinal;
    const char* imported_name;
    const char* dep_name;
    const struct crt_mach_header_64* dep_header;

    ordinal = read_uleb128(&p, info.trie_end, &ok);
    if (!ok || p >= info.trie_end) {
      return 0;
    }
    imported_name = (const char*)p;
    if (imported_name[0] == '\0') {
      imported_name = mach_symbol;
    }
    dep_name = dylib_name_at_ordinal(header, (uint32_t)ordinal);
    dep_header = dep_name != 0 ? find_loaded_image_by_name(dep_name) : 0;
    if (dep_header == 0 || dep_header == header) {
      return 0;
    }
    return find_exported_symbol_in_image(dep_header, imported_name, depth + 1);
  }

  switch (flags & CRT_EXPORT_SYMBOL_FLAGS_KIND_MASK) {
    case CRT_EXPORT_SYMBOL_FLAGS_KIND_REGULAR: {
      uint64_t value;

      /* If stub-and-resolver, the first uleb128 is the stub offset (a
       * normal callable address) and a resolver offset follows; the
       * resolver is only relevant to lazy-binding machinery this CRT does
       * not implement, so the stub is what we want either way. */
      value = read_uleb128(&p, info.trie_end, &ok);
      if (!ok) {
        return 0;
      }
      return (void*)((const uint8_t*)header + value);
    }
    case CRT_EXPORT_SYMBOL_FLAGS_KIND_ABSOLUTE: {
      uint64_t value = read_uleb128(&p, info.trie_end, &ok);

      if (!ok) {
        return 0;
      }
      return (void*)(uintptr_t)value;
    }
    default:
      /* Thread-local or an unrecognized kind: not a plain data/code address. */
      return 0;
  }
}

const void* __crt_macho_find_loaded_image(const char* image_name) {
  return (const void*)find_loaded_image_by_name(image_name);
}

void* __crt_macho_find_symbol_in_image(const void* handle, const char* symbol) {
  char mach_symbol[256];

  make_mach_symbol(symbol, mach_symbol, sizeof(mach_symbol));
  return find_exported_symbol_in_image((const struct crt_mach_header_64*)handle, mach_symbol, 0);
}

void* __crt_macho_find_symbol_in_loaded_image(const char* image_name, const char* symbol) {
  const void* handle = __crt_macho_find_loaded_image(image_name);

  if (handle == 0) {
    return 0;
  }
  return __crt_macho_find_symbol_in_image(handle, symbol);
}

void* __crt_macho_find_symbol_in_any_loaded_image(const char* symbol) {
  char mach_symbol[256];
  uint32_t count = _dyld_image_count();
  uint32_t i;

  make_mach_symbol(symbol, mach_symbol, sizeof(mach_symbol));
  for (i = 0; i < count; ++i) {
    void* address = find_exported_symbol_in_image(_dyld_get_image_header(i), mach_symbol, 0);

    if (address != 0) {
      return address;
    }
  }
  return 0;
}
