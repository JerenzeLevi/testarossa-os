#include "ramfs.h"

/* Built-in files. Original text written for this project. */
struct ramfs_file {
    const char *path;
    const char *data;
};

static const struct ramfs_file files[] = {
    {
        "/readme.txt",
        "Testarossa OS keeps a small archive in memory.\n"
        "Nothing here is written to disk, so it resets on every boot.\n"
        "\n"
        "Try:  ls\n"
        "      cat /archive/entry01.txt\n"
    },
    {
        "/archive/entry01.txt",
        "ENTRY 01\n"
        "The machine was left running in a back room.\n"
        "The screen still shows the last thing someone typed.\n"
        "Nobody has come back to turn it off.\n"
    },
    {
        "/system/about.txt",
        "TESTAROSSA OS 0.1.0\n"
        "Built from scratch for a computer science exam.\n"
        "This file is built into the kernel. It is read-only.\n"
    },
};

#define FILE_COUNT ((int)(sizeof(files) / sizeof(files[0])))

static int streq(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

int ramfs_count(void) {
    return FILE_COUNT;
}

const char *ramfs_name(int index) {
    if (index < 0 || index >= FILE_COUNT)
        return 0;
    return files[index].path;
}

const char *ramfs_read(const char *path) {
    for (int i = 0; i < FILE_COUNT; i++) {
        if (streq(files[i].path, path))
            return files[i].data;
    }
    return 0;
}
