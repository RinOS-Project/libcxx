/*
 * No-exception C ABI allocation owner for the CoreCLR freestanding profile.
 * Product app images use the profile-aware C++ owner in new_operators.cpp.
 */

#include "new_runtime.h"

extern void rin_free(void* pointer);
extern void rin_aligned_free(void* pointer);

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

void* _ZnwjRKSt9nothrow_t(unsigned int size, const void* nothrow) {
    (void)nothrow;
    return rin_cxx_operator_new_nothrow((size_t)size);
}

void* _ZnajRKSt9nothrow_t(unsigned int size, const void* nothrow) {
    (void)nothrow;
    return rin_cxx_operator_new_nothrow((size_t)size);
}

/* CoreCLR can include the shared C runtime object, so all ordinary and sized
 * delete symbols live here with the rest of this profile's allocation ABI. */
void _ZdlPv(void* pointer) { rin_free(pointer); }
void _ZdaPv(void* pointer) { rin_free(pointer); }
void _ZdlPvj(void* pointer, unsigned int size) {
    (void)size;
    rin_free(pointer);
}
void _ZdaPvj(void* pointer, unsigned int size) {
    (void)size;
    rin_free(pointer);
}
void _ZdlPvm(void* pointer, size_t size) {
    (void)size;
    rin_free(pointer);
}
void _ZdaPvm(void* pointer, size_t size) {
    (void)size;
    rin_free(pointer);
}

void _ZdlPvRKSt9nothrow_t(void* pointer, const void* nothrow) {
    (void)nothrow;
    rin_free(pointer);
}

/* C++17 aligned allocation ABI spellings for the same no-exception profile. */
void* _ZnwjSt11align_val_t(size_t size, size_t alignment) {
    return rin_cxx_operator_new_aligned(size, alignment);
}
void* _ZnajSt11align_val_t(size_t size, size_t alignment) {
    return rin_cxx_operator_new_aligned(size, alignment);
}
void* _ZnwmSt11align_val_t(size_t size, size_t alignment) {
    return rin_cxx_operator_new_aligned(size, alignment);
}
void* _ZnamSt11align_val_t(size_t size, size_t alignment) {
    return rin_cxx_operator_new_aligned(size, alignment);
}
void* _ZnwjSt11align_val_tRKSt9nothrow_t(
    size_t size, size_t alignment, const void* nothrow) {
    (void)nothrow;
    return rin_cxx_operator_new_aligned_nothrow(size, alignment);
}
void* _ZnajSt11align_val_tRKSt9nothrow_t(
    size_t size, size_t alignment, const void* nothrow) {
    (void)nothrow;
    return rin_cxx_operator_new_aligned_nothrow(size, alignment);
}
void* _ZnwmSt11align_val_tRKSt9nothrow_t(
    size_t size, size_t alignment, const void* nothrow) {
    (void)nothrow;
    return rin_cxx_operator_new_aligned_nothrow(size, alignment);
}
void* _ZnamSt11align_val_tRKSt9nothrow_t(
    size_t size, size_t alignment, const void* nothrow) {
    (void)nothrow;
    return rin_cxx_operator_new_aligned_nothrow(size, alignment);
}
void _ZdlPvSt11align_val_t(void* pointer, size_t alignment) {
    (void)alignment;
    rin_aligned_free(pointer);
}
void _ZdaPvSt11align_val_t(void* pointer, size_t alignment) {
    (void)alignment;
    rin_aligned_free(pointer);
}
void _ZdlPvmSt11align_val_t(
    void* pointer, size_t size, size_t alignment) {
    (void)size;
    (void)alignment;
    rin_aligned_free(pointer);
}
void _ZdaPvmSt11align_val_t(
    void* pointer, size_t size, size_t alignment) {
    (void)size;
    (void)alignment;
    rin_aligned_free(pointer);
}
void _ZdlPvjSt11align_val_t(
    void* pointer, unsigned int size, size_t alignment) {
    (void)size;
    (void)alignment;
    rin_aligned_free(pointer);
}
void _ZdaPvjSt11align_val_t(
    void* pointer, unsigned int size, size_t alignment) {
    (void)size;
    (void)alignment;
    rin_aligned_free(pointer);
}
void _ZdlPvSt11align_val_tRKSt9nothrow_t(
    void* pointer, size_t alignment, const void* nothrow) {
    (void)alignment;
    (void)nothrow;
    rin_aligned_free(pointer);
}
void _ZdaPvSt11align_val_tRKSt9nothrow_t(
    void* pointer, size_t alignment, const void* nothrow) {
    (void)alignment;
    (void)nothrow;
    rin_aligned_free(pointer);
}

void _ZdaPvRKSt9nothrow_t(void* pointer, const void* nothrow) {
    (void)nothrow;
    rin_free(pointer);
}
