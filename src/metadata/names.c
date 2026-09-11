#define _POSIX_C_SOURCE 200809L
#include "names.h"

#include <stdio.h>
#include <string.h>

static uint32_t read_u32(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static int obfuscated_pair(const uint8_t *text, size_t remaining)
{
    return remaining >= 2 &&
           text[0] == 0xc3 &&
           text[1] >= 0x8c &&
           text[1] <= 0x8f;
}

static size_t normalize_text(uint8_t *text, size_t length,
                             size_t identity, size_t *renamed)
{
    size_t read_offset = 0;
    size_t write_offset = 0;

    while (read_offset < length) {
        if (obfuscated_pair(text + read_offset, length - read_offset)) {
            size_t run_end = read_offset;
            while (obfuscated_pair(text + run_end, length - run_end))
                run_end += 2;
            if (run_end - read_offset >= 8) {
                char alias[16];
                int alias_size = snprintf(alias, sizeof(alias), "x%06zx",
                                          identity + read_offset);
                if (alias_size > 0 &&
                    (size_t)alias_size <= run_end - read_offset) {
                    memcpy(text + write_offset, alias, (size_t)alias_size);
                    write_offset += (size_t)alias_size;
                    read_offset = run_end;
                    (*renamed)++;
                    continue;
                }
            }
        }
        text[write_offset++] = text[read_offset++];
    }
    if (write_offset < length)
        memset(text + write_offset, 0, length - write_offset);
    return write_offset;
}

static void write_u32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

static size_t normalize_literals(uint8_t *data, size_t data_size)
{
    uint32_t records_offset = read_u32(data + 8);
    uint32_t records_size = read_u32(data + 12);
    uint32_t strings_offset = read_u32(data + 16);
    uint32_t strings_size = read_u32(data + 20);
    size_t record;
    size_t renamed = 0;

    if ((size_t)records_offset + records_size > data_size ||
        (size_t)strings_offset + strings_size > data_size)
        return 0;
    for (record = 0; record + 8 <= records_size; record += 8) {
        uint8_t *entry = data + records_offset + record;
        uint32_t length = read_u32(entry);
        uint32_t offset = read_u32(entry + 4);
        size_t new_length;
        if ((size_t)offset + length > strings_size)
            continue;
        new_length = normalize_text(data + strings_offset + offset,
                                    length, offset, &renamed);
        if (new_length != length)
            write_u32(entry, (uint32_t)new_length);
    }
    return renamed;
}

size_t metadata_normalize_names(uint8_t *data, size_t data_size)
{
    uint32_t table_offset;
    uint32_t table_size;
    size_t cursor = 0;
    size_t renamed = 0;
    uint8_t *table;

    if (data_size < 32)
        return 0;
    table_offset = read_u32(data + 24);
    table_size = read_u32(data + 28);
    if ((size_t)table_offset + table_size > data_size)
        return 0;
    table = data + table_offset;

    while (cursor < table_size) {
        size_t length = strnlen((const char *)table + cursor,
                                table_size - cursor);
        if (length == table_size - cursor)
            break;
        normalize_text(table + cursor, length, cursor, &renamed);
        cursor += length + 1;
    }
    return renamed + normalize_literals(data, data_size);
}
