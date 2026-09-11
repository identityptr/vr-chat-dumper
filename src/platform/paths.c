#define _POSIX_C_SOURCE 200809L
#include "paths.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int copy_path(char *output, size_t size, const char *value)
{
    int length = snprintf(output, size, "%s", value);
    return length >= 0 && (size_t)length < size;
}

int path_join(char *output, size_t size, const char *left, const char *right)
{
    int length = snprintf(output, size, "%s/%s", left, right);
    return length >= 0 && (size_t)length < size;
}

static int exists(const char *path)
{
    return access(path, F_OK) == 0;
}

static void parent(char *path)
{
    char *slash = strrchr(path, '/');
    if (slash && slash != path) *slash = '\0';
}

static int project_path(char *output, size_t size)
{
    char executable[PATH_MAX];
    ssize_t length = readlink("/proc/self/exe", executable,
                              sizeof(executable) - 1);
    if (length < 0) return 0;
    executable[length] = '\0';
    parent(executable);
    parent(executable);
    return copy_path(output, size, executable);
}

static int valid_game(const char *game)
{
    char assembly[PATH_MAX];
    char metadata[PATH_MAX];
    return path_join(assembly, sizeof(assembly), game, "GameAssembly.dll") &&
           path_join(metadata, sizeof(metadata), game,
                     "VRChat_Data/il2cpp_data/Metadata/global-metadata.dat") &&
           exists(assembly) && exists(metadata);
}

static int known_steam_path(char *output, size_t size)
{
    const char *home = getenv("HOME");
    char candidate[PATH_MAX];
    static const char *suffixes[] = {
        ".local/share/Steam/steamapps/common/VRChat",
        ".steam/steam/steamapps/common/VRChat",
        ".steam/root/steamapps/common/VRChat"
    };
    size_t i;
    if (!home) return 0;
    for (i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); i++) {
        if (path_join(candidate, sizeof(candidate), home, suffixes[i]) &&
            valid_game(candidate))
            return copy_path(output, size, candidate);
    }
    return 0;
}

static int quoted_value(const char *line, const char *key,
                        char *output, size_t size)
{
    const char *hit = strstr(line, key);
    const char *begin;
    const char *end;
    size_t length;
    if (!hit) return 0;
    begin = strchr(hit + strlen(key), '"');
    if (!begin) return 0;
    begin++;
    end = strchr(begin, '"');
    if (!end) return 0;
    length = (size_t)(end - begin);
    if (length >= size) return 0;
    memcpy(output, begin, length);
    output[length] = '\0';
    return 1;
}

static int search_library_file(char *output, size_t size, const char *path)
{
    FILE *file = fopen(path, "rb");
    char *line = NULL;
    size_t capacity = 0;
    int found = 0;
    if (!file) return 0;
    while (getline(&line, &capacity, file) >= 0) {
        char library[PATH_MAX];
        char candidate[PATH_MAX];
        char *cursor;
        if (!quoted_value(line, "\"path\"", library, sizeof(library)))
            continue;
        for (cursor = library; *cursor; cursor++) {
            if (*cursor == '\\' && cursor[1] == '\\')
                memmove(cursor, cursor + 1, strlen(cursor));
        }
        if (path_join(candidate, sizeof(candidate), library,
                      "steamapps/common/VRChat") && valid_game(candidate)) {
            found = copy_path(output, size, candidate);
            break;
        }
    }
    free(line);
    fclose(file);
    return found;
}

static int steam_library_path(char *output, size_t size)
{
    const char *home = getenv("HOME");
    char path[PATH_MAX];
    static const char *files[] = {
        ".local/share/Steam/steamapps/libraryfolders.vdf",
        ".steam/steam/steamapps/libraryfolders.vdf"
    };
    size_t i;
    if (!home) return 0;
    for (i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        if (path_join(path, sizeof(path), home, files[i]) &&
            search_library_file(output, size, path))
            return 1;
    }
    return 0;
}

static int dotnet_path(Paths *paths)
{
    const char *root = getenv("DOTNET_ROOT");
    char candidate[PATH_MAX];
    if (root && path_join(candidate, sizeof(candidate), root, "dotnet") &&
        access(candidate, X_OK) == 0)
        return copy_path(paths->dotnet, sizeof(paths->dotnet), candidate);
    if (snprintf(candidate, sizeof(candidate), "%s/../.dotnet/dotnet",
                 paths->project) > 0 && access(candidate, X_OK) == 0)
        return copy_path(paths->dotnet, sizeof(paths->dotnet), candidate);
    return copy_path(paths->dotnet, sizeof(paths->dotnet), "dotnet");
}

int directory_create(const char *path)
{
    char current[PATH_MAX];
    char *cursor;
    if (!copy_path(current, sizeof(current), path)) return 0;
    for (cursor = current + 1; *cursor; cursor++) {
        if (*cursor != '/') continue;
        *cursor = '\0';
        if (mkdir(current, 0755) != 0 && errno != EEXIST) return 0;
        *cursor = '/';
    }
    return mkdir(current, 0755) == 0 || errno == EEXIST;
}

int paths_resolve(Paths *paths, const char *game_override,
                  const char *output_override)
{
    const char *environment_game = getenv("VRCHAT_HOME");
    const char *environment_output = getenv("VRCHAT_OUT");
    memset(paths, 0, sizeof(*paths));
    if (!project_path(paths->project, sizeof(paths->project))) return 0;
    if (game_override) {
        if (!copy_path(paths->game, sizeof(paths->game), game_override)) return 0;
    } else if (environment_game) {
        if (!copy_path(paths->game, sizeof(paths->game), environment_game)) return 0;
    } else if (!known_steam_path(paths->game, sizeof(paths->game)) &&
               !steam_library_path(paths->game, sizeof(paths->game))) {
        return 0;
    }
    if (!valid_game(paths->game)) return 0;
    if (!path_join(paths->assembly, sizeof(paths->assembly),
                   paths->game, "GameAssembly.dll") ||
        !path_join(paths->metadata, sizeof(paths->metadata), paths->game,
                   "VRChat_Data/il2cpp_data/Metadata/global-metadata.dat"))
        return 0;
    if (output_override) {
        if (!copy_path(paths->output, sizeof(paths->output), output_override)) return 0;
    } else if (environment_output) {
        if (!copy_path(paths->output, sizeof(paths->output), environment_output)) return 0;
    } else if (!path_join(paths->output, sizeof(paths->output),
                          paths->project, "out")) {
        return 0;
    }
    if (!path_join(paths->dump_output, sizeof(paths->dump_output),
                   paths->output, "dump") ||
        !path_join(paths->metadata_output, sizeof(paths->metadata_output),
                   paths->output, "metadata") ||
        !path_join(paths->assembly_output, sizeof(paths->assembly_output),
                   paths->output, "gameassembly"))
        return 0;
    return dotnet_path(paths);
}
