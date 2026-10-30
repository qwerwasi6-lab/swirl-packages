#ifndef _SWIRL_SYS_STAT_H
#define _SWIRL_SYS_STAT_H
#include <time.h>
/* same layout as the x86-64 glibc struct the libc shim was built against */
struct stat {
    dev_t st_dev; ino_t st_ino; unsigned long st_nlink;
    mode_t st_mode; uid_t st_uid; gid_t st_gid; int __pad0;
    dev_t st_rdev; off_t st_size; long st_blksize; long st_blocks;
    struct timespec st_atim, st_mtim, st_ctim; long __unused[3];
};
#define S_IFMT   0170000
#define S_IFDIR  0040000
#define S_IFREG  0100000
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
int stat(const char *path, struct stat *out);
int mkdir(const char *path, mode_t mode);
#endif
