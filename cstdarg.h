/*
 * RinOS C++ <cstdarg>
 * C++ wrapper for stdarg.h
 */

#ifndef RINCXX_CSTDARG_H
#define RINCXX_CSTDARG_H

#include <stdarg.h>

#ifdef __cplusplus

namespace std {
    using ::va_list;
    /* va_start, va_end, va_arg, va_copy are macros */
}

#endif /* __cplusplus */
#endif /* RINCXX_CSTDARG_H */
