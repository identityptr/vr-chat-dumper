#include "scan.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define JSON_MAX_DEPTH 8

typedef struct {
    FILE *file;
    unsigned depth;
    bool first[JSON_MAX_DEPTH];
} Json;

typedef struct {
    const char *name;
    uint64_t value;
} Address;

static const Address il2cpp_addresses[] = {
    {"code_registration", 0x18ae163c0ULL},
    {"metadata_registration", 0x18c02f8f0ULL},
    {"metadata_initialize", 0x180b448a0ULL},
    {"metadata_load", 0x180ab93e0ULL}
};

static const char *const metadata_tables[] = {
    "literals",
    "literal_data",
    "strings",
    "properties",
    "methods",
    "fields",
    "assemblies"
};

static void json_quote(FILE *file, const char *text)
{
    const unsigned char *p;

    fputc('"', file);
    for (p = (const unsigned char *)text; *p != '\0'; ++p) {
        switch (*p) {
        case '"':
            fputs("\\\"", file);
            break;
        case '\\':
            fputs("\\\\", file);
            break;
        case '\b':
            fputs("\\b", file);
            break;
        case '\f':
            fputs("\\f", file);
            break;
        case '\n':
            fputs("\\n", file);
            break;
        case '\r':
            fputs("\\r", file);
            break;
        case '\t':
            fputs("\\t", file);
            break;
        default:
            if (*p < 0x20)
                fprintf(file, "\\u%04x", *p);
            else
                fputc(*p, file);
        }
    }
    fputc('"', file);
}

static void json_indent(Json *json)
{
    unsigned i;

    for (i = 0; i < json->depth; ++i)
        fputs("  ", json->file);
}

static void json_value(Json *json, const char *name)
{
    if (json->depth == 0)
        return;

    if (!json->first[json->depth])
        fputc(',', json->file);
    fputc('\n', json->file);
    json_indent(json);
    json->first[json->depth] = false;

    if (name != NULL) {
        json_quote(json->file, name);
        fputs(": ", json->file);
    }
}

static void json_open(Json *json, const char *name, int delimiter)
{
    json_value(json, name);
    fputc(delimiter, json->file);
    ++json->depth;
    json->first[json->depth] = true;
}

static void json_close(Json *json, int delimiter)
{
    bool empty = json->first[json->depth];

    --json->depth;
    if (!empty) {
        fputc('\n', json->file);
        json_indent(json);
    }
    fputc(delimiter, json->file);
}

static void json_object(Json *json, const char *name)
{
    json_open(json, name, '{');
}

static void json_object_end(Json *json)
{
    json_close(json, '}');
}

static void json_array(Json *json, const char *name)
{
    json_open(json, name, '[');
}

static void json_array_end(Json *json)
{
    json_close(json, ']');
}

static void json_string(Json *json, const char *name, const char *value)
{
    json_value(json, name);
    json_quote(json->file, value);
}

static void json_uint(Json *json, const char *name, uint64_t value)
{
    json_value(json, name);
    fprintf(json->file, "%llu", (unsigned long long)value);
}

static void json_bool(Json *json, const char *name, bool value)
{
    json_value(json, name);
    fputs(value ? "true" : "false", json->file);
}

static void json_hex(Json *json, const char *name, uint64_t value, int width)
{
    char text[32];

    snprintf(text, sizeof(text), "0x%0*llx", width,
             (unsigned long long)value);
    json_string(json, name, text);
}

static void write_sections(Json *json, const PeImage *image)
{
    size_t i;

    json_array(json, "sections");
    for (i = 0; i < image->section_count; ++i) {
        const PeSection *section = &image->sections[i];

        json_object(json, NULL);
        json_string(json, "name", section->name);
        json_uint(json, "rva", section->virtual_address);
        json_uint(json, "virtual_size", section->virtual_size);
        json_uint(json, "raw_offset", section->raw_offset);
        json_uint(json, "raw_size", section->raw_size);
        json_hex(json, "characteristics", section->characteristics, 8);
        json_object_end(json);
    }
    json_array_end(json);
}

static void write_image(Json *json, const PeImage *image)
{
    json_object(json, "image");
    json_string(json, "format", "PE");
    json_string(json, "machine", pe_machine_name(image->machine));
    json_hex(json, "machine_id", image->machine, 4);
    json_uint(json, "timestamp", image->timestamp);
    json_hex(json, "optional_header", image->optional_magic, 4);
    json_hex(json, "image_base", image->image_base, 0);
    json_hex(json, "entry_rva", image->entry_rva, 0);
    json_uint(json, "image_size", image->image_size);
    json_uint(json, "header_size", image->header_size);
    json_uint(json, "section_alignment", image->section_alignment);
    json_uint(json, "file_alignment", image->file_alignment);
    write_sections(json, image);
    json_object_end(json);
}

static void write_il2cpp(Json *json, const Il2CppSymbols *symbols)
{
    json_object(json, "il2cpp");
    json_uint(json, "metadata_schema", 30);
    json_uint(json, "native_layout", 31);
    json_uint(json, "method_definition_size", 32);
    json_string(json, "address_source", "PE structure and instruction references");
    json_object(json, "addresses");
    json_hex(json, "code_registration", symbols->code_registration, 0);
    json_hex(json, "metadata_registration", symbols->metadata_registration, 0);
    json_hex(json, "metadata_initialize", symbols->metadata_initialize, 0);
    json_hex(json, "metadata_load", symbols->metadata_load, 0);
    json_object_end(json);
    json_object_end(json);
}

static void write_metadata(Json *json)
{
    size_t i;

    json_object(json, "metadata");
    json_uint(json, "header_size", 392);
    json_string(json, "header_transform",
                "byte ^ ((index + 0x2b) & 0xff)");
    json_bool(json, "identifier_normalization", true);

    json_array(json, "obfuscated_alphabet");
    json_string(json, NULL, "U+00CC");
    json_string(json, NULL, "U+00CD");
    json_string(json, NULL, "U+00CE");
    json_string(json, NULL, "U+00CF");
    json_array_end(json);

    json_array(json, "tables");
    for (i = 0; i < ARRAY_COUNT(metadata_tables); ++i)
        json_string(json, NULL, metadata_tables[i]);
    json_array_end(json);
    json_object_end(json);
}

static void write_matches(Json *json, const ScanResult *results, size_t count)
{
    size_t i;

    json_array(json, "matches");
    for (i = 0; i < count; ++i) {
        size_t j;

        json_object(json, NULL);
        json_string(json, "name", results[i].name);
        json_uint(json, "count", results[i].count);
        json_array(json, "offsets");
        for (j = 0; j < results[i].count; ++j)
            json_uint(json, NULL, results[i].offsets[j]);
        json_array_end(json);
        json_object_end(json);
    }
    json_array_end(json);
}

int scan_write(const char *path, const char *input, size_t file_size,
               const PeImage *image, const Il2CppSymbols *symbols,
               const ScanResult *results,
                 size_t result_count)
{
    FILE *file;
    Json json = {0};
    int error;

    file = fopen(path, "wb");
    if (file == NULL)
        return -1;

    json.file = file;
    json_object(&json, NULL);
    json_string(&json, "input", input);
    json_uint(&json, "file_size", file_size);
    write_image(&json, image);
    write_il2cpp(&json, symbols);
    write_metadata(&json);
    write_matches(&json, results, result_count);
    json_object_end(&json);
    fputc('\n', file);

    if (!ferror(file))
        return fclose(file);

    error = errno == 0 ? EIO : errno;
    fclose(file);
    errno = error;
    return -1;
}
