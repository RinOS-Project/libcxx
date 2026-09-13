/*
 * RinOS C++ <inttypes.h> wrapper
 * Ensures std::snprintf is available when inttypes.h is included
 */

#ifndef RINCXX_INTTYPES_H_WRAPPER
#define RINCXX_INTTYPES_H_WRAPPER

#ifdef __cplusplus
#include "cstdio.h"
#endif

#include "../libc/inttypes.h"

#endif /* RINCXX_INTTYPES_H_WRAPPER */
