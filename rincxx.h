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

/* ═══════════════════════════════════════════════════════════════
 * C++ new/delete 演算子
 * ═══════════════════════════════════════════════════════════════*/

#if !defined(_MSC_VER)
/* Replaceable allocation functions have one out-of-line owner per image. */
void* operator new(size_t size);
void* operator new[](size_t size);
void operator delete(void* ptr) noexcept;
void operator delete[](void* ptr) noexcept;
void operator delete(void* ptr, size_t size) noexcept;
void operator delete[](void* ptr, size_t size) noexcept;
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
    void swap(T& a, T& b)
        noexcept(noexcept(T(static_cast<T&&>(a))) &&
                 noexcept(a = static_cast<T&&>(b))) {
        T tmp = static_cast<T&&>(a);
        a = static_cast<T&&>(b);
        b = static_cast<T&&>(tmp);
    }
    
    /* min/max - defined in algorithm.h */
}

#endif /* __cplusplus */
#endif /* RINCXX_H */
