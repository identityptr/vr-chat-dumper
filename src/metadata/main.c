#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "names.h"

#define HEADER_SIZE 0x188u
#define CANONICAL_HEADER_SIZE 0x100u
#define IL2CPP_METADATA_SANITY 0xfab11bafu
#define IL2CPP_METADATA_VERSION 30u

struct section_recipe {
    const char *name;
    uint32_t offset_field;
    int32_t source_adjust;
    uint32_t size_field;
    int32_t offset_byte_sign;
    int32_t key_bias;
};

static const struct section_recipe recipes[] = {
    {"literals",  28,  32, 220,  1,  60},
    {"literal_data", 176, -28, 184,  1,   0},
    {"strings", 296,  48, 308, -1, -76},
    {"properties", 264,  64, 108, -1, -92},
    {"methods", 120, -20, 372,  1,   8},
    {"fields", 136, -36,  48, -1,   8},
    {"assemblies", 216,  40,  84,  1,  68},
};

struct canonical_section {
    const char *name;
    uint32_t offset_field;
    int32_t offset_adjust;
    uint32_t size_field;
};


static const struct canonical_section canonical_sections[] = {
    {"string literals",                    28,  32, 220},
    {"string literal data",               176, -28, 184},
    {"metadata strings",                  296,  48, 308},
    {"events",                            188,  32, 320},
    {"properties",                        264,  64, 108},
    {"methods",                           120, -20, 372},
    {"parameter default values",          340, -20, 212},
    {"field default values",              200,  32, 248},
    {"default value data",                336, -24,   4},
    {"field marshaled sizes",             156,  68, 368},
    {"parameters",                        148,  24, 244},
    {"fields",                            136, -36,  48},
    {"generic parameters",                 16, -48, 168},
    {"generic parameter constraints",     388,  44, 284},
    {"generic containers",                364, -20, 360},
    {"nested types",                      316,  28,  68},
    {"interfaces",                        180, -48, 128},
    {"vtable methods",                    164,  52,  76},
    {"interface offsets",                  80,  36, 356},
    {"type definitions",                  256, -52, 324},
    {"images",                            224,  16, 232},
    {"assemblies",                        216,  40,  84},
    {"field references",                   88,  40, 240},
    {"referenced assemblies",             280,  24, 312},
    {"attribute data",                     32,  60, 304},
    {"attribute data ranges",              92, -52, 292},
    {"unresolved virtual call types", UINT32_MAX,   0, UINT32_MAX},
    {"unresolved virtual call ranges",UINT32_MAX,   0, UINT32_MAX},
    {"Windows Runtime type names",    UINT32_MAX,   0, UINT32_MAX},
    {"Windows Runtime strings",       UINT32_MAX,   0, UINT32_MAX},
    {"exported type definitions",     UINT32_MAX,   0, UINT32_MAX},
};

_Static_assert(sizeof(canonical_sections) / sizeof(canonical_sections[0]) == 31,
               "metadata v31 must contain 31 section pairs");

static uint32_t read_u32le(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static void write_u32le(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static int read_entire_file(const char *path, uint8_t **data, size_t *size)
{
    FILE *fp;
    long length;
    uint8_t *buffer;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "error: cannot open '%s': %s\n", path, strerror(errno));
        return 0;
    }
    if (fseek(fp, 0, SEEK_END) != 0 || (length = ftell(fp)) < 0 ||
        fseek(fp, 0, SEEK_SET) != 0) {
        fprintf(stderr, "error: cannot determine size of '%s'\n", path);
        fclose(fp);
        return 0;
    }
    if ((unsigned long)length > SIZE_MAX) {
        fprintf(stderr, "error: '%s' is too large for this build\n", path);
        fclose(fp);
        return 0;
    }

    buffer = malloc((size_t)length ? (size_t)length : 1u);
    if (buffer == NULL) {
        fprintf(stderr, "error: allocation failed for %ld bytes\n", length);
        fclose(fp);
        return 0;
    }
    if (fread(buffer, 1, (size_t)length, fp) != (size_t)length) {
        fprintf(stderr, "error: short read from '%s'\n", path);
        free(buffer);
        fclose(fp);
        return 0;
    }
    fclose(fp);
    *data = buffer;
    *size = (size_t)length;
    return 1;
}

static int write_entire_file(const char *path, const uint8_t *data, size_t size)
{
    FILE *fp = fopen(path, "wb");

    if (fp == NULL) {
        fprintf(stderr, "error: cannot create '%s': %s\n", path, strerror(errno));
        return 0;
    }
    if (fwrite(data, 1, size, fp) != size || fclose(fp) != 0) {
        fprintf(stderr, "error: failed writing '%s'\n", path);
        return 0;
    }
    return 1;
}

static void decrypt_header(uint8_t *output, const uint8_t *input)
{
    size_t i;

    for (i = 0; i < HEADER_SIZE; ++i)
        output[i] = input[i] ^ (uint8_t)(i + 0x2b);
}

static int decrypt_section(uint8_t *output, const uint8_t *input,
                           size_t file_size, const uint8_t *header,
                           const struct section_recipe *recipe)
{
    uint32_t encoded_offset = read_u32le(header + recipe->offset_field);
    uint32_t section_size = read_u32le(header + recipe->size_field);
    int64_t source_signed = (int64_t)encoded_offset + recipe->source_adjust;
    size_t source;
    size_t i;

    if (source_signed < 0) {
        fprintf(stderr, "error: %s has a negative source offset\n", recipe->name);
        return 0;
    }
    source = (size_t)source_signed;
    if (source > file_size || (size_t)section_size > file_size - source) {
        fprintf(stderr,
                "error: %s range 0x%zx..0x%zx is outside the 0x%zx-byte file\n",
                recipe->name, source, source + (size_t)section_size, file_size);
        return 0;
    }

    printf("%-10s source=0x%08zx size=0x%08" PRIx32
           " key=(i %c header[0x%03" PRIx32 "] %+d)\n",
           recipe->name, source, section_size,
           recipe->offset_byte_sign > 0 ? '+' : '-',
           recipe->offset_field, recipe->key_bias);

    for (i = 0; i < section_size; ++i) {
        int64_t key = (int64_t)i
                    + recipe->offset_byte_sign * header[recipe->offset_field]
                    + recipe->key_bias;
        output[source + i] = input[source + i] ^ (uint8_t)key;
    }
    return 1;
}

static int reconstruct_canonical_header(uint8_t *output,
                                        const uint8_t *shuffled_header,
                                        size_t file_size)
{
    size_t i;

    memset(output, 0, CANONICAL_HEADER_SIZE);
    write_u32le(output, IL2CPP_METADATA_SANITY);
    write_u32le(output + 4, IL2CPP_METADATA_VERSION);

    for (i = 0; i < sizeof(canonical_sections) / sizeof(canonical_sections[0]); ++i) {
        const struct canonical_section *section = &canonical_sections[i];
        uint32_t offset = 0;
        uint32_t size = 0;
        size_t destination = 8 + i * 8;

        if (section->offset_field != UINT32_MAX) {
            int64_t signed_offset =
                (int64_t)read_u32le(shuffled_header + section->offset_field)
                + section->offset_adjust;

            if (signed_offset < 0 || (uint64_t)signed_offset > file_size) {
                fprintf(stderr, "error: canonical %s offset is outside the file\n",
                        section->name);
                return 0;
            }
            offset = (uint32_t)signed_offset;
            size = read_u32le(shuffled_header + section->size_field);
            if ((size_t)offset > file_size || (size_t)size > file_size - offset) {
                fprintf(stderr,
                        "error: canonical %s range 0x%08" PRIx32
                        "..0x%08" PRIx32 " is outside the file\n",
                        section->name, offset, offset + size);
                return 0;
            }
        }

        write_u32le(output + destination, offset);
        write_u32le(output + destination + 4, size);
        printf("canonical  %-31s offset=0x%08" PRIx32
               " size=0x%08" PRIx32 "\n",
               section->name, offset, size);
    }
    return 1;
}

int main(int argc, char **argv)
{
    uint8_t *input = NULL;
    uint8_t *output = NULL;
    uint8_t shuffled_header[HEADER_SIZE];
    size_t file_size = 0;
    size_t i;
    int ok = EXIT_FAILURE;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <global-metadata.dat> <output.dat>\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (!read_entire_file(argv[1], &input, &file_size))
        goto done;
    if (file_size < HEADER_SIZE) {
        fprintf(stderr, "error: input is smaller than the 0x%x-byte header\n",
                HEADER_SIZE);
        goto done;
    }

    output = malloc(file_size);
    if (output == NULL) {
        fprintf(stderr, "error: allocation failed for %zu bytes\n", file_size);
        goto done;
    }
    memcpy(output, input, file_size);
    decrypt_header(output, input);
    memcpy(shuffled_header, output, HEADER_SIZE);

    printf("header     size=0x%08x key=(i + 0x2b)\n", HEADER_SIZE);
    printf("header[0] = 0x%08" PRIx32 " (expected shuffled value 0x00002278)\n",
           read_u32le(output));

    for (i = 0; i < sizeof(recipes) / sizeof(recipes[0]); ++i) {
        if (!decrypt_section(output, input, file_size, shuffled_header, &recipes[i]))
            goto done;
    }
    if (!reconstruct_canonical_header(output, shuffled_header, file_size))
        goto done;
    printf("renamed    %zu obfuscated metadata identifiers\n",
           metadata_normalize_names(output, file_size));
    if (!write_entire_file(argv[2], output, file_size))
        goto done;

    printf("wrote %zu bytes to %s\n", file_size, argv[2]);
    ok = EXIT_SUCCESS;

done:
    free(output);
    free(input);
    return ok;
}
