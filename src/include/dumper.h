#ifndef RUNTIME_H
#define RUNTIME_H

#include "paths.h"

typedef struct {
    char path[PATH_MAX];
} Dumper;

int dumper_open(Dumper *dumper);
void dumper_close(Dumper *dumper);

#endif
