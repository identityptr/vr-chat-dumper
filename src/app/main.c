#include "paths.h"
#include "process.h"
#include "dumper.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    char metadata_tool[PATH_MAX];
    char scan_tool[PATH_MAX];
    char dumper[PATH_MAX];
    char canonical_metadata[PATH_MAX];
    char scan_file[PATH_MAX];
} PipelineFiles;

static int make_path(char *target, size_t size, const char *directory,
                     const char *name)
{
    if (path_join(target, size, directory, name) != 0)
        return 0;
    errno = ENAMETOOLONG;
    return -1;
}

static int prepare_files(PipelineFiles *files, const Paths *paths,
                         const Dumper *dumper)
{
    if (make_path(files->metadata_tool, sizeof(files->metadata_tool),
                  paths->project, "bin/metadata") != 0)
        return -1;
    if (make_path(files->scan_tool, sizeof(files->scan_tool),
                  paths->project, "bin/scan") != 0)
        return -1;
    if (make_path(files->dumper, sizeof(files->dumper), dumper->path,
                  "Il2CppDumper.dll") != 0)
        return -1;
    if (make_path(files->canonical_metadata, sizeof(files->canonical_metadata),
                  paths->metadata_output, "global-metadata.dat") != 0)
        return -1;
    return make_path(files->scan_file, sizeof(files->scan_file), paths->assembly_output,
                     "scan.json");
}

static int decrypt_metadata(const Paths *paths, const PipelineFiles *files)
{
    const char *arguments[] = {
        files->metadata_tool,
        paths->metadata,
        files->canonical_metadata,
        NULL
    };

    return process_run(paths->project, arguments);
}

static int dump_il2cpp(const Paths *paths, const PipelineFiles *files,
                       const Dumper *dumper)
{
    const char *arguments[] = {
        paths->dotnet,
        files->dumper,
        paths->assembly,
        files->canonical_metadata,
        paths->dump_output,
        NULL
    };

    return process_run(dumper->path, arguments);
}

static int scan_gameassembly(const Paths *paths, const PipelineFiles *files)
{
    const char *arguments[] = {
        files->scan_tool,
        paths->assembly,
        files->scan_file,
        NULL
    };

    return process_run(paths->project, arguments);
}

static int pipeline(const Paths *paths)
{
    Dumper dumper = {{0}};
    PipelineFiles files;
    int code = EXIT_FAILURE;

    if (dumper_open(&dumper) != 0) {
        perror("embedded dumper");
        return EXIT_FAILURE;
    }
    if (prepare_files(&files, paths, &dumper) != 0) {
        perror("path");
        goto finished;
    }

    code = decrypt_metadata(paths, &files);
    if (code != 0) {
        fprintf(stderr, "metadata tool exited with status %d\n", code);
        code = EXIT_FAILURE;
        goto finished;
    }

    code = dump_il2cpp(paths, &files, &dumper);
    if (code != 0) {
        fprintf(stderr, "Il2CppDumper exited with status %d\n", code);
        code = EXIT_FAILURE;
        goto finished;
    }

    code = scan_gameassembly(paths, &files);
    if (code != 0) {
        fprintf(stderr, "GameAssembly scan exited with status %d\n", code);
        code = EXIT_FAILURE;
        goto finished;
    }

    code = EXIT_SUCCESS;

finished:
    dumper_close(&dumper);
    return code;
}

int main(int argc, char **argv)
{
    const char *game = argc > 1 ? argv[1] : NULL;
    const char *output = argc > 2 ? argv[2] : NULL;
    Paths paths;
    int code;

    if (argc > 3) {
        fprintf(stderr, "usage: %s [vrchat-directory] [output-directory]\n",
                argv[0]);
        return EXIT_FAILURE;
    }
    if (paths_resolve(&paths, game, output) == 0) {
        fputs("VRChat installation not found\n", stderr);
        return EXIT_FAILURE;
    }
    if (directory_create(paths.dump_output) == 0 ||
        directory_create(paths.metadata_output) == 0 ||
        directory_create(paths.assembly_output) == 0) {
        perror("output directory");
        return EXIT_FAILURE;
    }

    printf("Game    %s\nOutput  %s\n\n", paths.game, paths.output);
    code = pipeline(&paths);
    if (code == EXIT_SUCCESS) {
        printf("\nDump      %s\nMetadata  %s\nScan      %s\n",
               paths.dump_output, paths.metadata_output, paths.assembly_output);
    }
    return code;
}
