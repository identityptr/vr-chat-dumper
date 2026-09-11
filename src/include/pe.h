#ifndef PE_H
#define PE_H

#include <stddef.h>
#include <stdint.h>

#define PE_SECTION_LIMIT 96

typedef struct {
    char name[9];
    uint32_t virtual_size;
    uint32_t virtual_address;
    uint32_t raw_size;
    uint32_t raw_offset;
    uint32_t characteristics;
} PeSection;

typedef struct {
    uint16_t machine;
    uint16_t section_count;
    uint16_t optional_magic;
    uint32_t timestamp;
    uint32_t entry_rva;
    uint32_t section_alignment;
    uint32_t file_alignment;
    uint32_t image_size;
    uint32_t header_size;
    uint64_t image_base;
    PeSection sections[PE_SECTION_LIMIT];
} PeImage;

int pe_parse(const unsigned char *data, size_t size, PeImage *image);
const char *pe_machine_name(uint16_t machine);

#endif
