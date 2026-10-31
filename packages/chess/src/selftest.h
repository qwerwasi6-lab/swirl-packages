#ifndef SWIRLCHESS_SELFTEST_H
#define SWIRLCHESS_SELFTEST_H
#include "engine.h"

#define SELFTEST_STAGES 12

/* Runs one stage (1..SELFTEST_STAGES). Returns 0 = pass, otherwise an error code. */
int selftest_run(int stage, TimeFn now);

#endif
