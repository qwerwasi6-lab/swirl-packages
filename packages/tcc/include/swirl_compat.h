/* libc compat shim for the NetSurf port (SwirlOS ring-3).
 * One header per standard name; implementations live in shim/swirl_libc.c.
 * Freestanding target: compiled with -nostdinc -I<gcc-include> -I<here>. */
#ifndef SWIRL_LIBC_COMPAT_H
#define SWIRL_LIBC_COMPAT_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <time.h>
#ifndef __bool_true_false_are_defined
#define bool  _Bool
#define true  1
#define false 0
#endif

/* POSIX types + time declarations come from the platform headers (the
 * host headers are reachable; our libc implementation supplies the
 * actual symbols). */
#include <sys/types.h>

#define EOF (-1)
#define BUFSIZ 4096
#define FILENAME_MAX 256
#define PATH_MAX 256
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define stdin  ((FILE *)0)
#define stdout ((FILE *)1)
#define stderr ((FILE *)2)

/* ---- stdlib ---- */
void  *malloc(size_t n);
void  *calloc(size_t a, size_t b);
void  *realloc(void *p, size_t n);
void   free(void *p);
void   exit(int) __attribute__((noreturn));
void   abort(void) __attribute__((noreturn));
int    abs(int v);
long   labs(long v);
char  *getenv(const char *name);
int    atoi(const char *s);
long   strtol(const char *s, char **end, int base);
unsigned long strtoul(const char *s, char **end, int base);
double strtod(const char *s, char **end);
int    rand(void);
void   srand(unsigned seed);
void  *bsearch(const void *key, const void *base, size_t nm, size_t sz,
               int (*cmp)(const void *, const void *));
void   qsort(void *base, size_t nm, size_t sz,
             int (*cmp)(const void *, const void *));
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 0x7fffffff

/* ---- stdio ---- */
typedef struct _FILE FILE;
FILE      *fopen(const char *path, const char *mode);
int        fclose(FILE *f);
size_t     fread(void *p, size_t sz, size_t n, FILE *f);
size_t     fwrite(const void *p, size_t sz, size_t n, FILE *f);
int        fseek(FILE *f, long off, int whence);
long       ftell(FILE *f);
void       rewind(FILE *f);
int        feof(FILE *f);
int        ferror(FILE *f);
int        fflush(FILE *f);
int        setvbuf(FILE *f, char *b, int m, size_t s);
void       setbuf(FILE *f, char *b);
char      *fgets(char *buf, int n, FILE *f);
int        fputs(const char *s, FILE *f);
int        fputc(int c, FILE *f);
int        fgetc(FILE *f);
int        ungetc(int c, FILE *f);
int        fprintf(FILE *f, const char *fmt, ...);
int        vfprintf(FILE *f, const char *fmt, va_list ap);
int        printf(const char *fmt, ...);
int        snprintf(char *b, size_t n, const char *fmt, ...);
int        vsnprintf(char *b, size_t n, const char *fmt, va_list ap);
int        sprintf(char *b, const char *fmt, ...);
int        sscanf(const char *s, const char *fmt, ...);
FILE      *tmpfile(void);
int        remove(const char *path);
int        rename(const char *a, const char *b);
int        setvbuf(FILE *f, char *b, int m, size_t s);
#define _IONBF 2
#define _IOLBF 1
#define _IOFBF 0

/* ---- string (declared here so transitive includes always provide them) ---- */
void  *memcpy(void *d, const void *s, size_t n);
void  *memmove(void *d, const void *s, size_t n);
void  *memset(void *d, int c, size_t n);
size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strcpy(char *d, const char *s);
char  *strcat(char *d, const char *s);
char  *strrchr(const char *s, int c);
void  *memchr(const void *s, int c, size_t n);
int    memcmp(const void *a, const void *b, size_t n);
char  *strchr(const char *s, int c);
char  *strncpy(char *d, const char *s, size_t n);
char  *strncat(char *d, const char *s, size_t n);
char  *strdup(const char *s);
char  *strndup(const char *s, size_t n);
size_t strcspn(const char *s, const char *rej);
size_t strspn(const char *s, const char *acc);
char  *strpbrk(const char *s, const char *acc);
char  *strstr(const char *h, const char *n);
char  *strtok_r(char *s, const char *delim, char **save);
char  *strtok(char *s, const char *delim);
int    strcasecmp(const char *a, const char *b);
int    strncasecmp(const char *a, const char *b, size_t n);
char  *strcasestr(const char *h, const char *n);
size_t strnlen(const char *s, size_t n);

/* ---- math ---- */
double floor(double x);
double ceil(double x);
double fabs(double x);
double fmod(double x, double y);
double sqrt(double x);
float  sqrtf(float x);
double pow(double x, double y);
double sin(double x);
double cos(double x);
double atan2(double y, double x);
double atan(double x);
double log(double x);
double exp(double x);
double log10(double x);
double ldexp(double x, int e);
double frexp(double x, int *e);
double modf(double x, double *ip);
double copysign(double x, double y);
double nan(const char *tag);
#define isnan(x) __builtin_isnan(x)
#define isinf(x) __builtin_isinf(x)
#define isfinite(x) __builtin_isfinite(x)
#define signbit(x) __builtin_signbit(x)
#define M_PI 3.14159265358979323846
#define M_PI_2 1.57079632679489661923
#define M_PI_4 0.78539816339744830962
#define M_E 2.7182818284590452354


/* ---- ctype ---- */
int   isalpha(int c);
int   isalnum(int c);
int   isdigit(int c);
int   isspace(int c);
int   isupper(int c);
int   islower(int c);
int   iscntrl(int c);
int   ispunct(int c);
int   isxdigit(int c);
int   isprint(int c);
int   isgraph(int c);
int   toupper(int c);
int   tolower(int c);

/* ---- errno ---- */
int *__errno_location(void);
#define errno (*__errno_location())
#define EINVAL 22
#define ENOMEM 12
#define ENOENT 2
#define EACCES 13
#define EEXIST 17
#define ENOTDIR 20
#define EISDIR 21
#define EBADF 9
#define EAGAIN 11
#define EWOULDBLOCK EAGAIN
#define EINTR 4
#define EIO 5
#define ENOSPC 28
#define EPIPE 32
#define ECONNREFUSED 111
#define ECONNRESET 104
#define ECONNABORTED 103
#define ENETUNREACH 101
#define ETIMEDOUT 110
#define EHOSTUNREACH 113
#define EINPROGRESS 115
#define EALREADY 114
#define ENOTCONN 107
#define EAFNOSUPPORT 97
#define EADDRINUSE 98
#define EMSGSIZE 90
#define E2BIG 7
#define EILSEQ 84
#define ESRCH 3
#define EFAULT 14
#define ENOTEMPTY 39
#define ERANGE 34
#define EDOM 33
#define ELOOP 40
#define ENAMETOOLONG 36

/* ---- time: glibc <time.h> supplies the declarations; the shim
 * (shim/swirl_libc.c) supplies the implementations on SwirlOS syscalls. ---- */

/* ---- unistd / fcntl / stat ---- */
int    unlink(const char *path);
int    rmdir(const char *path);
int    access(const char *path, int mode);
char  *getcwd(char *buf, size_t n);
int    chdir(const char *path);
int    fsync(int fd);
int    isatty(int fd);
ssize_t read(int fd, void *b, size_t n);
ssize_t write(int fd, const void *b, size_t n);
int    close(int fd);
int    unlink_recursive(const char *path);
#define R_OK 4
#define W_OK 2
#define X_OK 1
#define F_OK 0
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#define O_RDONLY 1
#define O_WRONLY 2
#define O_RDWR 3
#define O_CREAT 4
#define O_TRUNC 8
#define O_APPEND 0

#define O_ACCMODE 3

/* fstatat family (netsurf utils/file.c): only the no-follow flag is
 * referenced; the shim's fstatat ignores it (SwirlFS has no symlinks) */
#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_FDCWD -100
/* fstatat itself is declared by the platform <sys/stat.h> */

/* stat: the platform <sys/stat.h> supplies the declarations (reachable
 * host headers); shim/swirl_libc.c implements stat() over SwirlFS. */
#include <sys/stat.h>

/* ---- limits ---- */
#define INT_MAX  0x7fffffff
#define INT_MIN  (-0x7fffffff - 1)
#define LONG_MAX 0x7fffffffffffffffL
#define LONG_MIN (-LONG_MAX - 1L)
#define ULONG_MAX 0xffffffffffffffffUL
#define UINT_MAX 0xffffffffU
#define SHRT_MAX 0x7fff
#define SHRT_MIN (-SHRT_MAX - 1)
#define USHRT_MAX 0xffff
#define UCHAR_MAX 0xff
#define SCHAR_MAX 0x7f
#define SCHAR_MIN (-SCHAR_MAX - 1)
#define CHAR_BIT 8
#define PATH_MAX2 256
#undef PATH_MAX
#define PATH_MAX 256

/* ---- assert ---- */
void __assert_fail(const char *expr, const char *file, int line, const char *fn);
#define assert(e) ((e) ? (void)0 : __assert_fail(#e, __FILE__, __LINE__, __func__))

/* ---- locale / signal stubs ---- */
#define setlocale(a, b) ((char *)0)
#define LC_ALL 0
#define LC_CTYPE 1
#define localeconv() ((void *)0)
typedef int sig_atomic_t;
#define SIGPIPE 13
#define SIGINT  2
#define SIGTERM 15
#define SIG_DFL ((void (*)(int))0)
#define SIG_IGN ((void (*)(int))1)
typedef void (*sig_handler_t)(int);
sig_handler_t signal(int sig, sig_handler_t handler);

/* struct lconv referenced by some headers */
struct lconv { char *decimal_point; };

#endif
