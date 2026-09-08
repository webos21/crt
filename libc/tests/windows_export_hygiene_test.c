#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  uint32_t virtual_address;
  uint32_t virtual_size;
  uint32_t raw_offset;
  uint32_t raw_size;
} section_info;

static uint16_t read_u16(const unsigned char* data, size_t offset) {
  return (uint16_t)data[offset] | ((uint16_t)data[offset + 1] << 8);
}

static uint32_t read_u32(const unsigned char* data, size_t offset) {
  return (uint32_t)data[offset] | ((uint32_t)data[offset + 1] << 8) |
         ((uint32_t)data[offset + 2] << 16) | ((uint32_t)data[offset + 3] << 24);
}

static int rva_to_offset(uint32_t rva, const section_info* sections, int count,
                         uint32_t* offset) {
  int i;
  for (i = 0; i < count; ++i) {
    uint32_t span = sections[i].virtual_size > sections[i].raw_size
                        ? sections[i].virtual_size
                        : sections[i].raw_size;
    if (rva >= sections[i].virtual_address &&
        rva < sections[i].virtual_address + span) {
      *offset = sections[i].raw_offset + (rva - sections[i].virtual_address);
      return 0;
    }
  }
  return -1;
}

static int load_file(const char* path, unsigned char** out_data, size_t* out_size) {
  FILE* file = fopen(path, "rb");
  long size;
  unsigned char* data;
  if (!file) return -1;
  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return -1;
  }
  size = ftell(file);
  if (size <= 0 || fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return -1;
  }
  data = (unsigned char*)malloc((size_t)size);
  if (!data) {
    fclose(file);
    return -1;
  }
  if (fread(data, 1, (size_t)size, file) != (size_t)size) {
    free(data);
    fclose(file);
    return -1;
  }
  fclose(file);
  *out_data = data;
  *out_size = (size_t)size;
  return 0;
}

static int dll_exports_name(const char* path, const char* denied_name) {
  unsigned char* data;
  size_t size;
  uint32_t pe_offset;
  uint32_t optional_offset;
  uint16_t section_count;
  uint16_t optional_size;
  uint16_t magic;
  uint32_t data_directory_offset;
  uint32_t export_rva;
  uint32_t section_offset;
  section_info sections[96];
  uint32_t export_offset;
  uint32_t name_count;
  uint32_t names_rva;
  uint32_t names_offset;
  uint32_t offset;
  uint32_t i;
  int result = 0;

  if (load_file(path, &data, &size) != 0) {
    fprintf(stderr, "windows_export_hygiene_test: could not read %s\n", path);
    return -1;
  }
  if (size < 0x40 || data[0] != 'M' || data[1] != 'Z') {
    result = -1;
    goto done;
  }
  pe_offset = read_u32(data, 0x3c);
  if (pe_offset + 24 >= size || memcmp(data + pe_offset, "PE\0\0", 4) != 0) {
    result = -1;
    goto done;
  }
  section_count = read_u16(data, pe_offset + 6);
  optional_size = read_u16(data, pe_offset + 20);
  if (section_count > 96) {
    result = -1;
    goto done;
  }
  optional_offset = pe_offset + 24;
  magic = read_u16(data, optional_offset);
  if (magic == 0x10b) {
    data_directory_offset = optional_offset + 96;
  } else if (magic == 0x20b) {
    data_directory_offset = optional_offset + 112;
  } else {
    result = -1;
    goto done;
  }
  export_rva = read_u32(data, data_directory_offset);
  if (export_rva == 0) goto done;

  section_offset = optional_offset + optional_size;
  for (i = 0; i < section_count; ++i) {
    uint32_t base = section_offset + i * 40;
    if (base + 40 > size) {
      result = -1;
      goto done;
    }
    sections[i].virtual_size = read_u32(data, base + 8);
    sections[i].virtual_address = read_u32(data, base + 12);
    sections[i].raw_size = read_u32(data, base + 16);
    sections[i].raw_offset = read_u32(data, base + 20);
  }

  if (rva_to_offset(export_rva, sections, section_count, &export_offset) != 0 ||
      export_offset + 40 > size) {
    result = -1;
    goto done;
  }
  name_count = read_u32(data, export_offset + 24);
  names_rva = read_u32(data, export_offset + 32);
  if (rva_to_offset(names_rva, sections, section_count, &names_offset) != 0) {
    result = -1;
    goto done;
  }
  for (i = 0; i < name_count; ++i) {
    uint32_t name_rva;
    const char* name;
    if (names_offset + i * 4 + 4 > size) {
      result = -1;
      goto done;
    }
    name_rva = read_u32(data, names_offset + i * 4);
    if (rva_to_offset(name_rva, sections, section_count, &offset) != 0 ||
        offset >= size) {
      result = -1;
      goto done;
    }
    name = (const char*)data + offset;
    if (strcmp(name, denied_name) == 0) {
      fprintf(stderr, "windows_export_hygiene_test: %s exports %s\n", path,
              denied_name);
      result = 1;
      goto done;
    }
  }

done:
  free(data);
  return result;
}

int main(int argc, char** argv) {
  int i;
  if (argc < 2) {
    fprintf(stderr, "windows_export_hygiene_test: missing dll arguments\n");
    return 1;
  }
  for (i = 1; i < argc; ++i) {
    if (dll_exports_name(argv[i], "_fltused") != 0) return 1;
  }
  printf("windows_export_hygiene_test: ok\n");
  return 0;
}
