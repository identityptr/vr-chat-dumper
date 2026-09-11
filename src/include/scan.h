#ifndef SCAN_H
#define SCAN_H

#include "il2cpp.h"
#include "pe.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *name;
    uint64_t *offsets;
    size_t count;
} ScanResult;

int scan_write(const char *path, const char *input, size_t file_size,
               const PeImage *image, const Il2CppSymbols *symbols,
               const ScanResult *results, size_t result_count);

#endif
