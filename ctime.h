/*
 * RinOS libcxx - ctime.h
 * C++ wrapper for time.h
 */
#ifndef RINCXX_CTIME_H
#define RINCXX_CTIME_H

/* A hosted consumer's C runtime owns time_t, tm and the time functions.
 * Rin's time.h describes the target ABI (including GNU tm fields), which is
 * not interchangeable with the compiler-provided C ABI. */
#if defined(__cplusplus) && !defined(RIN_FREESTANDING) && \
    defined(__STDC_HOSTED__) && __STDC_HOSTED__
#include <time.h>
#else
#include "../libc/time.h"
#endif

#ifdef __cplusplus

namespace std {
    /* Types */
    using ::time_t;
    using ::clock_t;
    using ::tm;       /* struct tm を std::tm として使用可能に */
    using ::timespec;
    using ::clockid_t;

    /* Functions */
    using ::time;
    using ::localtime;
    using ::gmtime;
#if !defined(_WIN32)
    using ::localtime_r;
    using ::gmtime_r;
    using ::mktime;
#endif
    using ::strftime;
    using ::difftime;
    using ::asctime;
    using ::ctime;
    using ::clock;
    using ::clock_gettime;
    using ::clock_getres;
    using ::nanosleep;
}

#endif /* __cplusplus */

#endif /* RINCXX_CTIME_H */
