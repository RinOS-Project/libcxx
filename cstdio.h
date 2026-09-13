/*
 * RinOS C++ <cstdio>
 * C++ wrapper for stdio.h
 */

#ifndef RINCXX_CSTDIO_H
#define RINCXX_CSTDIO_H

#if defined(__cplusplus) && !defined(RIN_FREESTANDING) && \
    defined(__STDC_HOSTED__) && __STDC_HOSTED__
#include <stdio.h>
#else
#include "../libc/stdio.h"
#endif
#include "cstdarg.h"

#ifdef __cplusplus

namespace std {
    using ::size_t;
    using ::FILE;
    using ::fpos_t;

    /* File operations */
    using ::fopen;
    using ::freopen;
    using ::fclose;
    using ::fflush;
    using ::setbuf;
    using ::setvbuf;

    /* Character I/O */
    using ::fgetc;
    using ::fgets;
    using ::fputc;
    using ::fputs;
    using ::getc;
    using ::getchar;
    using ::putc;
    using ::putchar;
    using ::puts;
    using ::ungetc;

    /* Direct I/O */
    using ::fread;
    using ::fwrite;

    /* File positioning */
    using ::fgetpos;
    using ::fseek;
    using ::fsetpos;
    using ::ftell;
    using ::rewind;

    /* Error handling */
    using ::clearerr;
    using ::feof;
    using ::ferror;
    using ::perror;

    /* Formatted I/O - inline wrappers to avoid linkage issues */
    inline int printf(const char* fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        int r = vprintf(fmt, ap);
        va_end(ap);
        return r;
    }

    inline int fprintf(FILE* f, const char* fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        int r = vfprintf(f, fmt, ap);
        va_end(ap);
        return r;
    }

    inline int sprintf(char* buf, const char* fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        int r = vsprintf(buf, fmt, ap);
        va_end(ap);
        return r;
    }

    inline int snprintf(char* buf, size_t size, const char* fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        int r = vsnprintf(buf, size, fmt, ap);
        va_end(ap);
        return r;
    }

    inline int vprintf(const char* fmt, va_list ap) {
        return ::vprintf(fmt, ap);
    }

    inline int vfprintf(FILE* f, const char* fmt, va_list ap) {
        return ::vfprintf(f, fmt, ap);
    }

    inline int vsprintf(char* buf, const char* fmt, va_list ap) {
        return ::vsprintf(buf, fmt, ap);
    }

    inline int vsnprintf(char* buf, size_t size, const char* fmt, va_list ap) {
        return ::vsnprintf(buf, size, fmt, ap);
    }

    inline int scanf(const char* fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        int r = vscanf(fmt, ap);
        va_end(ap);
        return r;
    }

    inline int fscanf(FILE* f, const char* fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        int r = vfscanf(f, fmt, ap);
        va_end(ap);
        return r;
    }

    inline int sscanf(const char* s, const char* fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        int r = vsscanf(s, fmt, ap);
        va_end(ap);
        return r;
    }

    inline int vscanf(const char* fmt, va_list ap) {
        return ::vscanf(fmt, ap);
    }

    inline int vfscanf(FILE* f, const char* fmt, va_list ap) {
        return ::vfscanf(f, fmt, ap);
    }

    inline int vsscanf(const char* s, const char* fmt, va_list ap) {
        return ::vsscanf(s, fmt, ap);
    }

    /* File management */
    using ::remove;
    using ::rename;
    using ::tmpfile;
    using ::tmpnam;
}

#endif /* __cplusplus */
#endif /* RINCXX_CSTDIO_H */
