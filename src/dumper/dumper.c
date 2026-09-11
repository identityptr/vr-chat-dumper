#include "dumper.h"

#include "payload.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int resource_path(char *path, size_t size, const Dumper *dumper,
                         const EmbeddedFile *resource)
{
    int length = snprintf(path, size, "%s/%s", dumper->path, resource->name);

    if (length < 0 || (size_t)length >= size) {
        errno = ENAMETOOLONG;
        return -1;
    }
    return 0;
}

static int unpack(const Dumper *dumper, const EmbeddedFile *resource)
{
    char path[PATH_MAX];
    const unsigned char *cursor = resource->data;
    size_t remaining = resource->size;
    int fd;

    if (resource_path(path, sizeof(path), dumper, resource) != 0)
        return -1;

    fd = open(path, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (fd == -1)
        return -1;

    while (remaining != 0) {
        ssize_t written = write(fd, cursor, remaining);

        if (written < 0 && errno == EINTR)
            continue;
        if (written <= 0) {
            int error = errno;
            close(fd);
            unlink(path);
            errno = error;
            return -1;
        }
        cursor += written;
        remaining -= (size_t)written;
    }

    if (close(fd) == -1) {
        unlink(path);
        return -1;
    }
    return 0;
}

int dumper_open(Dumper *dumper)
{
    char temporary[] = "/tmp/vrchat-il2cpp.XXXXXX";
    size_t i;

    dumper->path[0] = '\0';
    if (mkdtemp(temporary) == NULL)
        return -1;

    snprintf(dumper->path, sizeof(dumper->path), "%s", temporary);
    for (i = 0; i < embedded_file_count; ++i) {
        if (unpack(dumper, &embedded_files[i]) != 0) {
            int error = errno;
            dumper_close(dumper);
            errno = error;
            return -1;
        }
    }
    return 0;
}

void dumper_close(Dumper *dumper)
{
    size_t i;

    if (dumper->path[0] == '\0')
        return;

    for (i = 0; i < embedded_file_count; ++i) {
        char path[PATH_MAX];

        if (resource_path(path, sizeof(path), dumper, &embedded_files[i]) == 0)
            unlink(path);
    }
    rmdir(dumper->path);
    dumper->path[0] = '\0';
}
