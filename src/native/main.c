#include "memfind.h"
#include "pe.h"
#include "scan.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

typedef struct {
    uint8_t *bytes;
    size_t size;
} Buffer;

typedef struct {
    const char *name;
    const uint8_t *bytes;
    size_t size;
} Pattern;

static const uint8_t metadata_name[] = "global-metadata.dat";
static const uint8_t metadata_directory[] = "Metadata";
static const uint8_t metadata_word[] = "metadata";
static const uint8_t il2cpp_word[] = "il2cpp";
static const uint8_t metadata_magic[] = {0xaf, 0x1b, 0xb1, 0xfa};

static const Pattern patterns[] = {
    {"global-metadata.dat", metadata_name, sizeof(metadata_name) - 1},
    {"Metadata", metadata_directory, sizeof(metadata_directory) - 1},
    {"metadata", metadata_word, sizeof(metadata_word) - 1},
    {"il2cpp", il2cpp_word, sizeof(il2cpp_word) - 1},
    {"metadata-sanity", metadata_magic, sizeof(metadata_magic)}
};

static int buffer_read(Buffer *buffer, const char *path)
{
    FILE *file;
    long length;

    file = fopen(path, "rb");
    if (file == NULL)
        return -1;

    if (fseek(file, 0, SEEK_END) != 0 ||
        (length = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }

    buffer->size = (size_t)length;
    buffer->bytes = malloc(buffer->size);
    if (buffer->bytes == NULL) {
        fclose(file);
        return -1;
    }

    if (fread(buffer->bytes, 1, buffer->size, file) != buffer->size) {
        free(buffer->bytes);
        buffer->bytes = NULL;
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}

static int match_append(ScanResult *matches, size_t *capacity, size_t offset)
{
    uint64_t *expanded;

    if (matches->count == *capacity) {
        *capacity = *capacity == 0 ? 16 : *capacity * 2;
        expanded = realloc(matches->offsets,
                           *capacity * sizeof(*matches->offsets));
        if (expanded == NULL)
            return -1;
        matches->offsets = expanded;
    }

    matches->offsets[matches->count++] = offset;
    return 0;
}

static int pattern_scan(const Buffer *buffer, const Pattern *pattern,
                        ScanResult *matches)
{
    size_t capacity = 0;
    size_t cursor = 0;

    matches->name = pattern->name;
    while (cursor + pattern->size <= buffer->size) {
        size_t offset = memfind(buffer->bytes + cursor,
                                    buffer->size - cursor,
                                    pattern->bytes,
                                    pattern->size);

        if (offset == SIZE_MAX)
            return 0;

        cursor += offset;
        if (match_append(matches, &capacity, cursor) != 0)
            return -1;
        ++cursor;
    }
    return 0;
}

static int patterns_scan(const Buffer *buffer, ScanResult *results)
{
    size_t i;

    for (i = 0; i < ARRAY_COUNT(patterns); ++i) {
        if (pattern_scan(buffer, &patterns[i], &results[i]) != 0)
            return -1;
    }
    return 0;
}

static void results_free(ScanResult *results)
{
    size_t i;

    for (i = 0; i < ARRAY_COUNT(patterns); ++i)
        free(results[i].offsets);
}

static int inspect(const Buffer *buffer, const char *input, const char *output)
{
    ScanResult results[ARRAY_COUNT(patterns)] = {{0}};
    Il2CppSymbols symbols;
    PeImage image;
    int status;

    if (pe_parse(buffer->bytes, buffer->size, &image) != 0) {
        fprintf(stderr, "%s: invalid PE image\n", input);
        return EXIT_FAILURE;
    }

    if (il2cpp_resolve(buffer->bytes, buffer->size, &image, &symbols) != 0) {
        fprintf(stderr, "%s: IL2CPP symbols not found\n", input);
        return EXIT_FAILURE;
    }

    if (patterns_scan(buffer, results) != 0) {
        perror("pattern scan");
        results_free(results);
        return EXIT_FAILURE;
    }

    status = scan_write(output, input, buffer->size, &image, &symbols,
                          results, ARRAY_COUNT(results));
    results_free(results);

    if (status != 0) {
        perror(output);
        return EXIT_FAILURE;
    }

    printf("report   %s\n", output);
    return EXIT_SUCCESS;
}

static int run(const char *input, const char *output)
{
    Buffer buffer = {0};
    int status;

    if (buffer_read(&buffer, input) != 0) {
        perror(input);
        return EXIT_FAILURE;
    }

    status = inspect(&buffer, input, output);
    free(buffer.bytes);
    return status;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s GameAssembly.dll report.json\n", argv[0]);
        return EXIT_FAILURE;
    }

    return run(argv[1], argv[2]);
}
