/*
 * RinOS C++ <cerrno>
 * C++ wrapper for errno.h
 */

#ifndef RINCXX_CERRNO_H
#define RINCXX_CERRNO_H

#if defined(RINCXX_CSTDLIB_HOSTED_C) || defined(_INC_ERRNO) || \
    (!defined(RIN_FREESTANDING) && defined(__STDC_HOSTED__) && __STDC_HOSTED__)
#include <errno.h>
#else
#include "../libc/errno.h"
#endif

/* errno is a macro, not brought into std namespace */

#endif /* RINCXX_CERRNO_H */
