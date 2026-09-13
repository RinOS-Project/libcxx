/*
 * RinOS C++ <cfenv>
 * C++ wrapper for fenv.h
 */

#ifndef RINCXX_CFENV_H
#define RINCXX_CFENV_H

#include <fenv.h>

#ifdef __cplusplus

namespace std {
    using ::fenv_t;
    using ::fexcept_t;

    /* Floating-point exception functions */
    using ::feclearexcept;
    using ::fegetexceptflag;
    using ::feraiseexcept;
    using ::fesetexceptflag;
    using ::fetestexcept;

    /* Rounding functions */
    using ::fegetround;
    using ::fesetround;

    /* Environment functions */
    using ::fegetenv;
    using ::feholdexcept;
    using ::fesetenv;
    using ::feupdateenv;
}

#endif /* __cplusplus */
#endif /* RINCXX_CFENV_H */
