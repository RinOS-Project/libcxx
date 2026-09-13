/*
 * RinOS C++ <clocale>
 * C++ wrapper for locale.h
 */

#ifndef RINCXX_CLOCALE_H
#define RINCXX_CLOCALE_H

#include "../libc/locale.h"

#ifdef __cplusplus

namespace std {
    using ::lconv;
    using ::setlocale;
    using ::localeconv;
}

#endif /* __cplusplus */
#endif /* RINCXX_CLOCALE_H */
