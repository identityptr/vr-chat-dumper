#ifndef IL2CPP_H
#define IL2CPP_H

#include "pe.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t code_registration;
    uint64_t metadata_registration;
    uint64_t metadata_initialize;
    uint64_t metadata_load;
} Il2CppSymbols;

int il2cpp_resolve(const uint8_t *file, size_t file_size,
                   const PeImage *image, Il2CppSymbols *symbols);

#endif
