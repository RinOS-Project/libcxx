/*
 * RinOS C++ <cwctype>
 * C++ wrapper for wctype.h
 */

#ifndef RINCXX_CWCTYPE_H
#define RINCXX_CWCTYPE_H

#include <wctype.h>

#ifdef __cplusplus

namespace std {
    using ::wint_t;
    using ::wctrans_t;
    using ::wctype_t;

    /* Wide character classification */
    using ::iswalnum;
    using ::iswalpha;
    using ::iswblank;
    using ::iswcntrl;
    using ::iswdigit;
    using ::iswgraph;
    using ::iswlower;
    using ::iswprint;
    using ::iswpunct;
    using ::iswspace;
    using ::iswupper;
    using ::iswxdigit;
    using ::iswctype;
    using ::wctype;

    /* Wide character conversion */
    using ::towlower;
    using ::towupper;
    using ::towctrans;
    using ::wctrans;
}

#endif /* __cplusplus */
#endif /* RINCXX_CWCTYPE_H */
