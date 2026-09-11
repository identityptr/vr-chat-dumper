#include "il2cpp.h"

#include "memfind.h"

#include <stdint.h>
#include <string.h>

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

typedef struct {
    const uint8_t *bytes;
    const char *mask;
    size_t size;
    size_t anchor;
    size_t anchor_size;
} Signature;


static const uint8_t registration_bytes[] = {
    0x48, 0x8d, 0x0d, 0, 0, 0, 0,
    0x48, 0x89, 0x0d, 0, 0, 0, 0,
    0x48, 0x8d, 0x05, 0, 0, 0, 0,
    0x48, 0x89, 0x05, 0, 0, 0, 0,
    0x48, 0x89, 0x0d
};

static const Signature registration_signature = {
    registration_bytes,
    "xxx????xxx????xxx????xxx????xxx",
    sizeof(registration_bytes),
    0,
    3
};

static const uint8_t metadata_call_bytes[] = {
    0xe8, 0, 0, 0, 0,
    0x48, 0x89, 0x05, 0, 0, 0, 0,
    0x48, 0x85, 0xc0, 0x75
};

static const Signature metadata_call_signature = {
    metadata_call_bytes,
    "x????xxx????xxxx",
    sizeof(metadata_call_bytes),
    5,
    3
};

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 |
           (uint32_t)p[3] << 24;
}

static int32_t get_i32(const uint8_t *p)
{
    return (int32_t)get_u32(p);
}

static const PeSection *section_by_name(const PeImage *image, const char *name)
{
    size_t i;

    for (i = 0; i < image->section_count; ++i) {
        if (strcmp(image->sections[i].name, name) == 0)
            return &image->sections[i];
    }
    return NULL;
}

static int section_bytes(const uint8_t *file, size_t file_size,
                         const PeSection *section, const uint8_t **data,
                         size_t *size)
{
    if (section == NULL || section->raw_offset > file_size ||
        section->raw_size > file_size - section->raw_offset)
        return -1;

    *data = file + section->raw_offset;
    *size = section->raw_size;
    return 0;
}

static int signature_matches(const uint8_t *p, const Signature *signature)
{
    size_t i;

    for (i = 0; i < signature->size; ++i) {
        if (signature->mask[i] == 'x' && p[i] != signature->bytes[i])
            return 0;
    }
    return 1;
}

static size_t signature_find(const uint8_t *data, size_t size,
                             const Signature *signature)
{
    size_t cursor = 0;

    while (cursor + signature->anchor_size <= size) {
        size_t found = memfind(data + cursor, size - cursor,
                               signature->bytes + signature->anchor,
                               signature->anchor_size);
        size_t anchor;
        size_t start;

        if (found == SIZE_MAX)
            break;

        anchor = cursor + found;
        cursor = anchor + 1;
        if (anchor < signature->anchor)
            continue;

        start = anchor - signature->anchor;
        if (start + signature->size <= size &&
            signature_matches(data + start, signature))
            return start;
    }
    return SIZE_MAX;
}

static uint32_t relative_target(uint32_t instruction_rva,
                                uint32_t instruction_size,
                                const uint8_t *displacement)
{
    int64_t target = instruction_rva + instruction_size;

    target += get_i32(displacement);
    return (uint32_t)target;
}

static uint32_t function_start(const uint8_t *pdata, size_t size,
                               uint32_t instruction_rva)
{
    size_t offset;

    for (offset = 0; offset + 12 <= size; offset += 12) {
        uint32_t begin = get_u32(pdata + offset);
        uint32_t end = get_u32(pdata + offset + 4);

        if (instruction_rva >= begin && instruction_rva < end)
            return begin;
    }
    return 0;
}

static int resolve_registrations(const uint8_t *text, size_t text_size,
                                 const PeSection *section,
                                 const PeImage *image,
                                 Il2CppSymbols *symbols)
{
    size_t offset = signature_find(text, text_size, &registration_signature);
    uint32_t instruction_rva;
    uint32_t code_rva;
    uint32_t metadata_rva;

    if (offset == SIZE_MAX)
        return -1;

    instruction_rva = section->virtual_address + (uint32_t)offset;
    code_rva = relative_target(instruction_rva, 7, text + offset + 3);
    metadata_rva = relative_target(instruction_rva + 14, 7,
                                   text + offset + 17);

    symbols->code_registration = image->image_base + code_rva;
    symbols->metadata_registration = image->image_base + metadata_rva;
    return 0;
}

static int resolve_metadata(const uint8_t *text, size_t text_size,
                            const PeSection *text_section,
                            const uint8_t *pdata, size_t pdata_size,
                            const PeImage *image, Il2CppSymbols *symbols)
{
    size_t offset = signature_find(text, text_size, &metadata_call_signature);
    uint32_t call_rva;
    uint32_t load_rva;
    uint32_t initialize_rva;

    if (offset == SIZE_MAX)
        return -1;

    call_rva = text_section->virtual_address + (uint32_t)offset;
    load_rva = relative_target(call_rva, 5, text + offset + 1);
    initialize_rva = function_start(pdata, pdata_size, call_rva);
    if (initialize_rva == 0)
        return -1;

    symbols->metadata_load = image->image_base + load_rva;
    symbols->metadata_initialize = image->image_base + initialize_rva;
    return 0;
}

int il2cpp_resolve(const uint8_t *file, size_t file_size,
                   const PeImage *image, Il2CppSymbols *symbols)
{
    const PeSection *text_section = section_by_name(image, ".text");
    const PeSection *pdata_section = section_by_name(image, ".pdata");
    const uint8_t *text;
    const uint8_t *pdata;
    size_t text_size;
    size_t pdata_size;

    memset(symbols, 0, sizeof(*symbols));
    if (section_bytes(file, file_size, text_section, &text, &text_size) != 0 ||
        section_bytes(file, file_size, pdata_section, &pdata, &pdata_size) != 0)
        return -1;

    if (resolve_registrations(text, text_size, text_section,
                              image, symbols) != 0)
        return -1;
    if (resolve_metadata(text, text_size, text_section,
                         pdata, pdata_size, image, symbols) != 0)
        return -1;
    return 0;
}
