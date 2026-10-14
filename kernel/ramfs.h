#ifndef TESTAROSSA_RAMFS_H
#define TESTAROSSA_RAMFS_H

/* A small read-only archive built into the kernel. It lives in memory and
 * resets on every boot. Nothing is written to disk. */

int ramfs_count(void);                       /* number of built-in files */
const char *ramfs_name(int index);           /* path of file number index, or NULL */

/* Find a file by its full path, for example "/readme.txt".
 * Returns its NUL-terminated contents, or NULL if there is no such file. */
const char *ramfs_read(const char *path);

#endif
