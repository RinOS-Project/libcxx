/*
 * RinOS C++ <cwchar>
 * C++ wrapper for wchar.h
 */

#ifndef RINCXX_CWCHAR_H
#define RINCXX_CWCHAR_H

#include <wchar.h>

#ifdef __cplusplus

namespace std {
    /* Note: wchar_t is a built-in type in C++, not using */
    using ::wint_t;
    using ::mbstate_t;
    using ::size_t;

    /* Wide string manipulation */
    using ::wcscpy;
    using ::wcsncpy;
    using ::wcscat;
    using ::wcsncat;
    using ::wcscmp;
    using ::wcsncmp;
    using ::wcscoll;
    using ::wcsxfrm;
    using ::wcschr;
    using ::wcsrchr;
    using ::wcsspn;
    using ::wcscspn;
    using ::wcspbrk;
    using ::wcsstr;
    using ::wcstok;
    using ::wcslen;

    /* Wide memory manipulation */
    using ::wmemcpy;
    using ::wmemmove;
    using ::wmemcmp;
    using ::wmemchr;
    using ::wmemset;

    /* Wide character I/O */
    using ::fgetwc;
    using ::fgetws;
    using ::fputwc;
    using ::fputws;
    using ::getwc;
    using ::getwchar;
    using ::putwc;
    using ::putwchar;
    using ::ungetwc;

    /* Wide formatted I/O */
    using ::fwprintf;
    using ::fwscanf;
    using ::swprintf;
    using ::swscanf;
    using ::vfwprintf;
    using ::vfwscanf;
    using ::vswprintf;
    using ::vswscanf;
    using ::vwprintf;
    using ::vwscanf;
    using ::wprintf;
    using ::wscanf;

    /* Wide character conversion */
    using ::wcstod;
    using ::wcstof;
    using ::wcstold;
    using ::wcstol;
    using ::wcstoll;
    using ::wcstoul;
    using ::wcstoull;

    /* Multibyte/wide character conversion */
    using ::btowc;
    using ::wctob;
    using ::mbsinit;
    using ::mbrlen;
    using ::mbrtowc;
    using ::wcrtomb;
    using ::mbsrtowcs;
    using ::wcsrtombs;

    /* Time */
    using ::wcsftime;
}

#endif /* __cplusplus */
#endif /* RINCXX_CWCHAR_H */
