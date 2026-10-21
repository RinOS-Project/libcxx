/*
 * libcxx-side operator new/new[] entry points for RinOS apps.
 * The app runtime (rin_runtime.c) must not define these symbols.
 */

#include "new_runtime.h"

extern void rin_free(void* pointer);

/* 32-bit ABI symbols */
void* _Znwj(unsigned int size) { return rin_cxx_operator_new((size_t)size); }
void* _Znaj(unsigned int size) { return rin_cxx_operator_new((size_t)size); }

/* 64-bit ABI symbols */
void* _Znwm(size_t size) { return rin_cxx_operator_new(size); }
void* _Znam(size_t size) { return rin_cxx_operator_new(size); }

/* Itanium ABI nothrow overloads used by CoreCLR's freestanding C++ PAL.  The
 * second parameter is the address of std::nothrow; it is opaque here so this
 * C owner does not create a second C++ standard-library type. */
void* _ZnwmRKSt9nothrow_t(size_t size, const void* nothrow) {
    (void)nothrow;
    return rin_cxx_operator_new_nothrow(size);
}

void* _ZnamRKSt9nothrow_t(size_t size, const void* nothrow) {
    (void)nothrow;
    return rin_cxx_operator_new_nothrow(size);
}

void _ZdlPvRKSt9nothrow_t(void* pointer, const void* nothrow) {
    (void)nothrow;
    rin_free(pointer);
}

void _ZdaPvRKSt9nothrow_t(void* pointer, const void* nothrow) {
    (void)nothrow;
    rin_free(pointer);
}
