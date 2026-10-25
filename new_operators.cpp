// SPDX-License-Identifier: MIT
// Per-image replaceable C++ allocation/deallocation operator owner.

#include "new.h"

namespace rin_cxx_detail {

void* throwing_new(size_t size) {
    void* allocation = rin_cxx_operator_new_nothrow(size);
    if (allocation) return allocation;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw std::bad_alloc();
#else
    return rin_cxx_operator_new(size);
#endif
}

void* throwing_aligned_new(size_t size, size_t alignment) {
    void* allocation = rin_cxx_operator_new_aligned_nothrow(size, alignment);
    if (allocation) return allocation;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw std::bad_alloc();
#else
    return rin_cxx_operator_new_aligned(size, alignment);
#endif
}

} // namespace rin_cxx_detail

void* operator new(size_t size) {
    return rin_cxx_detail::throwing_new(size);
}

void* operator new[](size_t size) {
    return rin_cxx_detail::throwing_new(size);
}

void operator delete(void* pointer) noexcept { rin_free(pointer); }
void operator delete[](void* pointer) noexcept { rin_free(pointer); }
void operator delete(void* pointer, size_t) noexcept { rin_free(pointer); }
void operator delete[](void* pointer, size_t) noexcept { rin_free(pointer); }

void* operator new(size_t size, const std::nothrow_t&) noexcept {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    try {
        return rin_cxx_operator_new_nothrow(size);
    } catch (...) {
        return nullptr;
    }
#else
    return rin_cxx_operator_new_nothrow(size);
#endif
}

void* operator new[](size_t size, const std::nothrow_t&) noexcept {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    try {
        return rin_cxx_operator_new_nothrow(size);
    } catch (...) {
        return nullptr;
    }
#else
    return rin_cxx_operator_new_nothrow(size);
#endif
}

void operator delete(void* pointer, const std::nothrow_t&) noexcept {
    rin_free(pointer);
}

void operator delete[](void* pointer, const std::nothrow_t&) noexcept {
    rin_free(pointer);
}

#if __cplusplus >= 201703L
void* operator new(size_t size, std::align_val_t alignment) {
    return rin_cxx_detail::throwing_aligned_new(
        size, static_cast<size_t>(alignment));
}

void* operator new[](size_t size, std::align_val_t alignment) {
    return rin_cxx_detail::throwing_aligned_new(
        size, static_cast<size_t>(alignment));
}

void* operator new(size_t size, std::align_val_t alignment,
                   const std::nothrow_t&) noexcept {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    try {
        return rin_cxx_operator_new_aligned_nothrow(
            size, static_cast<size_t>(alignment));
    } catch (...) {
        return nullptr;
    }
#else
    return rin_cxx_operator_new_aligned_nothrow(
        size, static_cast<size_t>(alignment));
#endif
}

void* operator new[](size_t size, std::align_val_t alignment,
                     const std::nothrow_t&) noexcept {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    try {
        return rin_cxx_operator_new_aligned_nothrow(
            size, static_cast<size_t>(alignment));
    } catch (...) {
        return nullptr;
    }
#else
    return rin_cxx_operator_new_aligned_nothrow(
        size, static_cast<size_t>(alignment));
#endif
}

void operator delete(void* pointer, std::align_val_t) noexcept {
    rin_aligned_free(pointer);
}

void operator delete[](void* pointer, std::align_val_t) noexcept {
    rin_aligned_free(pointer);
}

void operator delete(void* pointer, size_t, std::align_val_t) noexcept {
    rin_aligned_free(pointer);
}

void operator delete[](void* pointer, size_t, std::align_val_t) noexcept {
    rin_aligned_free(pointer);
}

void operator delete(void* pointer, std::align_val_t,
                     const std::nothrow_t&) noexcept {
    rin_aligned_free(pointer);
}

void operator delete[](void* pointer, std::align_val_t,
                       const std::nothrow_t&) noexcept {
    rin_aligned_free(pointer);
}
#endif
