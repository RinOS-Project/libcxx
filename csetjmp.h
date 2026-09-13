/*
 * RinOS C++ <csetjmp>
 * C++ wrapper for setjmp.h
 */

#ifndef RINCXX_CSETJMP_H
#define RINCXX_CSETJMP_H

#include <setjmp.h>

#ifdef __cplusplus

namespace std {
    using ::jmp_buf;
    using ::longjmp;
    /* setjmp is a macro */
}

#endif /* __cplusplus */
#endif /* RINCXX_CSETJMP_H */
