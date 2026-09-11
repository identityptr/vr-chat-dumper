#include "pe.h"

#include <string.h>

static uint16_t get_u16(const uint8_t *p)
{
    return (uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8);
}

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 |
           (uint32_t)p[3] << 24;
}

static uint64_t get_u64(const uint8_t *p)
{
    return (uint64_t)get_u32(p) | (uint64_t)get_u32(p + 4) << 32;
}

static int contains(size_t file_size, size_t offset, size_t length)
{
    return offset <= file_size && length <= file_size - offset;
}

static void read_section(PeSection *section, const uint8_t *header)
{
    memcpy(section->name, header, 8);
    section->name[8] = '\0';
    section->virtual_size = get_u32(header + 8);
    section->virtual_address = get_u32(header + 12);
    section->raw_size = get_u32(header + 16);
    section->raw_offset = get_u32(header + 20);
    section->characteristics = get_u32(header + 36);
}

int pe_parse(const uint8_t *data, size_t size, PeImage *image)
{
    const uint8_t *coff;
    const uint8_t *optional;
    size_t pe_offset;
    size_t section_offset;
    uint16_t optional_size;
    size_t i;

    memset(image, 0, sizeof(*image));
    if (!contains(size, 0, 0x40) || memcmp(data, "MZ", 2) != 0)
        return -1;

    pe_offset = get_u32(data + 0x3c);
    if (!contains(size, pe_offset, 24) ||
        memcmp(data + pe_offset, "PE\0\0", 4) != 0)
        return -1;

    coff = data + pe_offset + 4;
    image->machine = get_u16(coff);
    image->section_count = get_u16(coff + 2);
    image->timestamp = get_u32(coff + 4);
    optional_size = get_u16(coff + 16);
    section_offset = pe_offset + 24 + optional_size;

    if (image->section_count > PE_SECTION_LIMIT ||
        !contains(size, pe_offset + 24, optional_size) ||
        optional_size < 64 ||
        !contains(size, section_offset, (size_t)image->section_count * 40))
        return -1;

    optional = coff + 20;
    image->optional_magic = get_u16(optional);
    image->entry_rva = get_u32(optional + 16);

    if (image->optional_magic == 0x20b)
        image->image_base = get_u64(optional + 24);
    else if (image->optional_magic == 0x10b)
        image->image_base = get_u32(optional + 28);
    else
        return -1;

    image->section_alignment = get_u32(optional + 32);
    image->file_alignment = get_u32(optional + 36);
    image->image_size = get_u32(optional + 56);
    image->header_size = get_u32(optional + 60);

    for (i = 0; i < image->section_count; ++i)
        read_section(&image->sections[i], data + section_offset + i * 40);

    return 0;
}

const char *pe_machine_name(uint16_t machine)
{
    switch (machine) {
    case 0x014c:
        return "x86";
    case 0x8664:
        return "x86-64";
    case 0xaa64:
        return "arm64";
    default:
        return "unknown";
    }
}
