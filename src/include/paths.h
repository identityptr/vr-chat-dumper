#ifndef VRCHAT_PATHS_H
#define VRCHAT_PATHS_H

#include <stddef.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct {
    char project[PATH_MAX];
    char game[PATH_MAX];
    char assembly[PATH_MAX];
    char metadata[PATH_MAX];
    char output[PATH_MAX];
    char dump_output[PATH_MAX];
    char metadata_output[PATH_MAX];
    char assembly_output[PATH_MAX];
    char dotnet[PATH_MAX];
} Paths;

int paths_resolve(Paths *paths, const char *game_override,
                  const char *output_override);
int path_join(char *output, size_t output_size,
              const char *left, const char *right);
int directory_create(const char *path);

#endif
