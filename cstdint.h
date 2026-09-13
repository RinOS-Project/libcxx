/*
 * RinOS C++ <cstdint> header
 * C++ wrapper for stdint.h
 */

#ifndef RINCXX_CSTDINT_H
#define RINCXX_CSTDINT_H

/* The freestanding Rin ABI intentionally fixes 64-bit integer aliases to
 * long long.  Importing Clang's hosted <stdint.h> afterwards can select long
 * on x86_64 and redeclare every 64-bit/fast/intmax type incompatibly. */
#if defined(RIN_FREESTANDING)
#include "../libc/stdint.h"
#else
#include <stdint.h>
#endif

namespace std {
    using ::int8_t;
    using ::int16_t;
    using ::int32_t;
    using ::int64_t;
    using ::uint8_t;
    using ::uint16_t;
    using ::uint32_t;
    using ::uint64_t;

    using ::int_least8_t;
    using ::int_least16_t;
    using ::int_least32_t;
    using ::int_least64_t;
    using ::uint_least8_t;
    using ::uint_least16_t;
    using ::uint_least32_t;
    using ::uint_least64_t;

    using ::int_fast8_t;
    using ::int_fast16_t;
    using ::int_fast32_t;
    using ::int_fast64_t;
    using ::uint_fast8_t;
    using ::uint_fast16_t;
    using ::uint_fast32_t;
    using ::uint_fast64_t;

    using ::intmax_t;
    using ::uintmax_t;
    using ::intptr_t;
    using ::uintptr_t;
}

#endif /* RINCXX_CSTDINT_H */
