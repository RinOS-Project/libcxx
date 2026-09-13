/*
 * libcxx-side operator new/new[] entry points for RinOS apps.
 * The app runtime (rin_runtime.c) must not define these symbols.
 */

#include "new_runtime.h"

/* 32-bit ABI symbols */
void* _Znwj(unsigned int size) { return rin_cxx_operator_new((size_t)size); }
void* _Znaj(unsigned int size) { return rin_cxx_operator_new((size_t)size); }

/* 64-bit ABI symbols */
void* _Znwm(size_t size) { return rin_cxx_operator_new(size); }
void* _Znam(size_t size) { return rin_cxx_operator_new(size); }
