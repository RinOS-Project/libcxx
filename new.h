/*
 * RinOS C++ <new> ✿
 * 動的メモリ管理
 */

#ifndef RINCXX_NEW_H
#define RINCXX_NEW_H

#define RIN_CXX_NEW_EXCEPTION_BRIDGE 1
#include "rincxx.h"
#undef RIN_CXX_NEW_EXCEPTION_BRIDGE
#include "exception.h"

/* These facilities are fully declared below, so publish their standard
 * feature-test values from <new> itself (not only from <version>). */
#if __cplusplus >= 201703L
#ifndef __cpp_lib_aligned_new
#define __cpp_lib_aligned_new 201606L
#endif
#ifndef __cpp_lib_hardware_interference_size
#define __cpp_lib_hardware_interference_size 201703L
#endif
#ifndef __cpp_lib_launder
#define __cpp_lib_launder 201606L
#endif
#endif
#if __cplusplus >= 202002L
#ifndef __cpp_lib_destroying_delete
#define __cpp_lib_destroying_delete 201806L
#endif
#endif

namespace rin_cxx_detail {

/*
 * The C runtime owns retry/new-handler policy and exposes a non-throwing
 * allocation primitive.  The C++ <new> boundary is responsible only for the
 * language-level failure result: exception-enabled callers receive the
 * standard bad_alloc object, while freestanding no-exception products retain
 * the existing terminal operator-new path.
 */
inline void* throwing_new(size_t size) {
    void* allocation = rin_cxx_operator_new_nothrow(size);
    if (allocation) return allocation;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw std::bad_alloc();
#else
    return rin_cxx_operator_new(size);
#endif
}

inline void* throwing_aligned_new(size_t size, size_t alignment) {
    void* allocation = rin_cxx_operator_new_aligned_nothrow(size, alignment);
    if (allocation) return allocation;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw std::bad_alloc();
#else
    return rin_cxx_operator_new_aligned(size, alignment);
#endif
}

} /* namespace rin_cxx_detail */

#if __cplusplus >= 201703L
#define RIN_NEW_NODISCARD [[nodiscard]]
#else
#define RIN_NEW_NODISCARD
#endif

/* rin_malloc/rin_free は rincxx.h で宣言済み */

/* アライメント付きメモリ割り当て */
extern "C" {
    void* rin_aligned_alloc(size_t size, size_t alignment);
    void  rin_aligned_free(void* ptr);
}

namespace std {

/* ═══════════════════════════════════════════════════════════════
 * nothrow_t
 * ═══════════════════════════════════════════════════════════════*/

struct nothrow_t {
    explicit nothrow_t() = default;
};

/* This is a standard-library object, not a header-local convenience value.
 * Keep one external definition for every language mode so C++11 clients and
 * a C++20 runtime observe the same address. */
extern const nothrow_t nothrow;

/* ═══════════════════════════════════════════════════════════════
 * align_val_t (C++17)
 * ═══════════════════════════════════════════════════════════════*/

#if __cplusplus >= 201703L
enum class align_val_t : size_t {};
#endif

/* ═══════════════════════════════════════════════════════════════
 * destroying_delete_t (C++20)
 * ═══════════════════════════════════════════════════════════════*/

#if __cplusplus >= 202002L
struct destroying_delete_t {
    explicit destroying_delete_t() = default;
};

inline constexpr destroying_delete_t destroying_delete{};
#endif

/* ═══════════════════════════════════════════════════════════════
 * new_handler
 * ═══════════════════════════════════════════════════════════════*/

using new_handler = rin_cxx_new_handler_fn;

inline new_handler get_new_handler() noexcept {
    return rin_cxx_get_new_handler();
}

inline new_handler set_new_handler(new_handler handler) noexcept {
    return rin_cxx_set_new_handler(handler);
}

/* ═══════════════════════════════════════════════════════════════
 * hardware_destructive_interference_size (C++17)
 * ═══════════════════════════════════════════════════════════════*/

#if __cplusplus >= 201703L
inline constexpr size_t hardware_destructive_interference_size = 64;
inline constexpr size_t hardware_constructive_interference_size = 64;
#endif

/* ═══════════════════════════════════════════════════════════════
 * launder (C++17)
 * ═══════════════════════════════════════════════════════════════*/

#if __cplusplus >= 201703L
template<typename T>
RIN_NEW_NODISCARD constexpr T* launder(T* p) noexcept {
    return __builtin_launder(p);
}
#endif

} /* namespace std */

/* ═══════════════════════════════════════════════════════════════
 * operator new / delete (nothrowバリアント)
 * 注: 基本的なnew/deleteは rincxx.h で定義済み
 * ═══════════════════════════════════════════════════════════════*/

/* nothrow new */
RIN_NEW_NODISCARD inline void* operator new(size_t size, const std::nothrow_t&) noexcept {
    return rin_cxx_operator_new_nothrow(size);
}

RIN_NEW_NODISCARD inline void* operator new[](size_t size, const std::nothrow_t&) noexcept {
    return rin_cxx_operator_new_nothrow(size);
}

/* nothrow delete */
inline void operator delete(void* ptr, const std::nothrow_t&) noexcept {
    rin_free(ptr);
}

inline void operator delete[](void* ptr, const std::nothrow_t&) noexcept {
    rin_free(ptr);
}

#if __cplusplus >= 201703L
/* C++17 over-aligned placement forms.  The supplied storage is owned by the
 * caller just like ordinary placement new, and the matching delete is a
 * no-op cleanup hook used only when construction throws. */
inline void* operator new(size_t, std::align_val_t, void* ptr) noexcept {
    return ptr;
}

inline void* operator new[](size_t, std::align_val_t, void* ptr) noexcept {
    return ptr;
}

inline void operator delete(void* ptr, std::align_val_t, void* placement) noexcept {
    (void)ptr;
    (void)placement;
}

inline void operator delete[](void* ptr, std::align_val_t, void* placement) noexcept {
    (void)ptr;
    (void)placement;
}

inline void operator delete(void* ptr, std::align_val_t,
                            const std::nothrow_t&) noexcept {
    rin_aligned_free(ptr);
}

inline void operator delete[](void* ptr, std::align_val_t,
                              const std::nothrow_t&) noexcept {
    rin_aligned_free(ptr);
}

/* アライメント指定new (C++17) - rin_aligned_alloc を使用 */
RIN_NEW_NODISCARD inline void* operator new(size_t size, std::align_val_t alignment) {
    return rin_cxx_detail::throwing_aligned_new(
        size, static_cast<size_t>(alignment));
}

RIN_NEW_NODISCARD inline void* operator new[](size_t size, std::align_val_t alignment) {
    return rin_cxx_detail::throwing_aligned_new(
        size, static_cast<size_t>(alignment));
}

RIN_NEW_NODISCARD inline void* operator new(size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept {
    return rin_cxx_operator_new_aligned_nothrow(
        size, static_cast<size_t>(alignment));
}

[[nodiscard]] inline void* operator new[](size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept {
    return rin_cxx_operator_new_aligned_nothrow(
        size, static_cast<size_t>(alignment));
}

/* アライメント指定delete - rin_aligned_free を使用 */
inline void operator delete(void* ptr, std::align_val_t) noexcept {
    rin_aligned_free(ptr);
}

inline void operator delete[](void* ptr, std::align_val_t) noexcept {
    rin_aligned_free(ptr);
}

inline void operator delete(void* ptr, size_t, std::align_val_t) noexcept {
    rin_aligned_free(ptr);
}

inline void operator delete[](void* ptr, size_t, std::align_val_t) noexcept {
    rin_aligned_free(ptr);
}
#endif

#undef RIN_NEW_NODISCARD

#endif /* RINCXX_NEW_H */
