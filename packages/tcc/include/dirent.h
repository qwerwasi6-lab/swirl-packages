/* dirent.h — POSIX directory shim over SwirlFS s_dir_open/s_dir_read. */
#ifndef _DIRENT_H_SWIRL
#define _DIRENT_H_SWIRL

#include "swirl_compat.h"

#define DT_UNKNOWN 0
#define DT_FIFO    1
#define DT_CHR     2
#define DT_DIR     4
#define DT_BLK     6
#define DT_REG     8
#define DT_LNK     10
#define DT_SOCK    12

typedef struct dirent {
    long           d_ino;
    long           d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char           d_name[64];
} dirent_t;

typedef struct _DIR DIR;

DIR *opendir(const char *path);
int  closedir(DIR *d);
struct dirent *readdir(DIR *d);
int  dirfd(DIR *d);
void rewinddir(DIR *d);

/* scandir with the comparator the file fetcher uses */
typedef int (*scandir_cmp_t)(const struct dirent **, const struct dirent **);
typedef int (*scandir_filter_t)(const struct dirent *);
int scandir(const char *path, struct dirent ***out, scandir_filter_t filter,
            scandir_cmp_t cmp);
int alphasort(const struct dirent **a, const struct dirent **b);

#endif
