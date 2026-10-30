/* iconv.h — libc shim: charset conversion descriptor API (subset).
 * Implementation: shim/swirl_iconv.c (web-common charsets to/from UTF-8). */
#ifndef SWIRL_SHIM_ICONV_H
#define SWIRL_SHIM_ICONV_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *iconv_t;

iconv_t iconv_open(const char *tocode, const char *fromcode);
size_t  iconv(iconv_t cd, char **inbuf, size_t *inbytesleft,
              char **outbuf, size_t *outbytesleft);
int     iconv_close(iconv_t cd);

#ifdef __cplusplus
}
#endif

#endif /* SWIRL_SHIM_ICONV_H */
