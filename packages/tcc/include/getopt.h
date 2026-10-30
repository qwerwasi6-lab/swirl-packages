/* getopt.h — libc shim: command-line option parsing.
 *
 * NetSurf's process_cmdline() (upstream, unmodified) parses -f/-b/-w/-h and
 * the start URL through getopt_long()/optind. This header + the matcher in
 * shim/swirl_libc.c provide the POSIX subset it needs (no argv permutation:
 * parsing stops at the first non-option, which is exactly the URL slot). */
#ifndef SWIRL_SHIM_GETOPT_H
#define SWIRL_SHIM_GETOPT_H

#ifdef __cplusplus
extern "C" {
#endif

#define no_argument         0
#define required_argument   1
#define optional_argument   2

struct option {
    const char *name;
    int         has_arg;
    int        *flag;
    int         val;
};

extern char *optarg;    /* argument of the current option          */
extern int   optind;    /* index of the next argv element to parse */
extern int   opterr;    /* if nonzero, print errors to stderr      */
extern int   optopt;    /* offending option character              */

int getopt(int nargc, char *const argv[], const char *ostr);
int getopt_long(int nargc, char *const argv[], const char *ostr,
                const struct option *longopts, int *longindex);
int getopt_long_only(int nargc, char *const argv[], const char *ostr,
                     const struct option *longopts, int *longindex);

#ifdef __cplusplus
}
#endif

#endif /* SWIRL_SHIM_GETOPT_H */
