#ifndef MEMFIND_H
#define MEMFIND_H

#include <stddef.h>
#include <stdint.h>

size_t memfind(const uint8_t *data, size_t data_size,
                   const uint8_t *pattern, size_t pattern_size);

#endif
