/* time.h - SwirlOS TinyCC. Time comes from the RTC; there is no timezone (UTC). */
#ifndef _SWIRL_TIME_H
#define _SWIRL_TIME_H
#include <sys/types.h>
struct tm {
    int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst;
};
struct timespec { time_t tv_sec; long tv_nsec; };
#define CLOCKS_PER_SEC 1000000L
time_t time(time_t *t);
clock_t clock(void);
time_t mktime(struct tm *tm);
time_t timegm(struct tm *tm);
struct tm *gmtime(const time_t *t);
struct tm *gmtime_r(const time_t *t, struct tm *out);
struct tm *localtime(const time_t *t);
struct tm *localtime_r(const time_t *t, struct tm *out);
#endif
