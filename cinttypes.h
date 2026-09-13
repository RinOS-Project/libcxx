/*
 * RinOS C++ <cinttypes>
 * C++ wrapper for inttypes.h
 */

#ifndef RINCXX_CINTTYPES_H
#define RINCXX_CINTTYPES_H

#include "cstdint.h"
#include "cstdio.h"
#include <inttypes.h>

#ifdef __cplusplus

namespace std {
    using ::imaxdiv_t;
    using ::imaxabs;
    using ::imaxdiv;
    using ::strtoimax;
    using ::strtoumax;
    using ::wcstoimax;
    using ::wcstoumax;
}

#endif /* __cplusplus */
#endif /* RINCXX_CINTTYPES_H */
