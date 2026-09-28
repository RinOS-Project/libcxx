/*
 * RinOS C++ Runtime ✿
 * 基本的なC++ランタイムサポート
 */

#ifndef RINCXX_H
#define RINCXX_H

#include "new_runtime.h"

/* Keep the C++ carrier in lockstep with the freestanding C ABI. */
#if defined(_MSC_VER)
#if defined(_WIN64)
typedef unsigned __int64 size_t;
typedef __int64          ptrdiff_t;
#else
typedef unsigned int size_t;
typedef int          ptrdiff_t;
#endif
#else
typedef __SIZE_TYPE__ size_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;
#endif

/* nullptr_t for C++ */
#ifdef __cplusplus
using nullptr_t = decltype(nullptr);
#endif

/* NULL定義 */
#ifndef NULL
#ifdef __cplusplus
#define NULL nullptr
#else
#define NULL ((void*)0)
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * メモリ管理フック (カーネルまたはアプリで実装)
 * ═══════════════════════════════════════════════════════════════*/

/* これらはリンク時に提供される */
void* rin_malloc(size_t size);
void  rin_free(void* ptr);
void* rin_realloc(void* ptr, size_t size);
void* rin_calloc(size_t num, size_t size);
void  rin_log(const char* msg);
#if defined(_MSC_VER)
__declspec(noreturn) void rin_panic(const char* msg);
#else
[[noreturn]] void rin_panic(const char* msg);
#endif

#ifdef __cplusplus
}

#if defined(RIN_CXX_NEW_EXCEPTION_BRIDGE)
namespace rin_cxx_detail {
/*
 * <new> defines this bridge after it has made std::bad_alloc complete.  The
 * declaration lives here so the global operator-new definitions can remain in
 * the foundational header without creating an exception/string include cycle.
 */
void* throwing_new(size_t size);
}
#endif

/* ═══════════════════════════════════════════════════════════════
 * C++ new/delete 演算子
 * ═══════════════════════════════════════════════════════════════*/

#if !defined(_MSC_VER)
inline void* operator new(size_t size) {
#if defined(RIN_CXX_NEW_EXCEPTION_BRIDGE)
    return rin_cxx_detail::throwing_new(size);
#elif defined(RIN_CXX_USE_PRODUCT_NEW_OWNER)
    return rin_cxx_operator_new(size);
#else
    /* Debug: detect corrupted size before syscall */
    if (size > 0x100000) {
        rin_log("[NEW] bad allocation size\n");
    }
    return rin_malloc(size);
#endif
}

inline void* operator new[](size_t size) {
#if defined(RIN_CXX_NEW_EXCEPTION_BRIDGE)
    return rin_cxx_detail::throwing_new(size);
#elif defined(RIN_CXX_USE_PRODUCT_NEW_OWNER)
    return rin_cxx_operator_new(size);
#else
    return rin_malloc(size);
#endif
}

inline void operator delete(void* ptr) noexcept {
    rin_free(ptr);
}

inline void operator delete[](void* ptr) noexcept {
    rin_free(ptr);
}

inline void operator delete(void* ptr, size_t) noexcept {
    rin_free(ptr);
}

inline void operator delete[](void* ptr, size_t) noexcept {
    rin_free(ptr);
}
#endif

/* placement new */
inline void* operator new(size_t, void* ptr) noexcept {
    return ptr;
}

inline void* operator new[](size_t, void* ptr) noexcept {
    return ptr;
}

/* The matching placement-delete functions are part of the global allocation
 * ABI.  They are selected when a placement-new constructor throws; omitting
 * them leaves an otherwise valid placement expression with an unresolved
 * cleanup call.  Placement storage remains caller-owned, so these functions
 * deliberately do not release the pointer. */
inline void operator delete(void* ptr, void* placement) noexcept {
    (void)ptr;
    (void)placement;
}

inline void operator delete[](void* ptr, void* placement) noexcept {
    (void)ptr;
    (void)placement;
}

/* ═══════════════════════════════════════════════════════════════
 * 基本型定義
 * ═══════════════════════════════════════════════════════════════*/

namespace std {
    typedef decltype(nullptr) nullptr_t;
    
    /* move/forward */
    template<typename T>
    struct remove_reference { typedef T type; };
    
    template<typename T>
    struct remove_reference<T&> { typedef T type; };
    
    template<typename T>
    struct remove_reference<T&&> { typedef T type; };
    
    template<typename T>
    constexpr typename remove_reference<T>::type&& move(T&& arg) noexcept {
        return static_cast<typename remove_reference<T>::type&&>(arg);
    }
    
    template<typename T>
    constexpr T&& forward(typename remove_reference<T>::type& arg) noexcept {
        return static_cast<T&&>(arg);
    }
    
    template<typename T>
    constexpr T&& forward(typename remove_reference<T>::type&& arg) noexcept {
        return static_cast<T&&>(arg);
    }
    
    /* The body needs C++14 constexpr rules; C++11 still exposes the same
     * runtime swap operation without making an invalid constexpr function. */
    template<typename T>
#if __cplusplus >= 201402L
    constexpr
#else
    inline
#endif
    auto swap(T& a, T& b)
        noexcept(noexcept(T(std::move(a))) &&
                 noexcept(a = std::move(b)))
        -> decltype(T(std::move(a)), a = std::move(b), void()) {
        T tmp = std::move(a);
        a = std::move(b);
        b = std::move(tmp);
    }
    
    /* min/max - defined in algorithm.h */
}

#endif /* __cplusplus */
#endif /* RINCXX_H */
