/*
 * RinOS C++ <cstddef> header
 * C++ wrapper for stddef.h types
 */

#ifndef RINCXX_CSTDDEF_H
#define RINCXX_CSTDDEF_H

#include "../libc/stddef.h"

namespace std {
    using ::size_t;
    using ::ptrdiff_t;

    // nullptr_t - defined here since not in C stddef.h
    using nullptr_t = decltype(nullptr);

    // max_align_t - use compiler builtin or define
#ifdef __BIGGEST_ALIGNMENT__
    struct max_align_t {
        alignas(__BIGGEST_ALIGNMENT__) char _[__BIGGEST_ALIGNMENT__];
    };
#else
    using max_align_t = long double;
#endif

    // byte type and operators are C++17 additions.  Earlier language modes
    // must not expose them: in particular, mutating constexpr functions are
    // not valid C++11 declarations.
#if __cplusplus >= 201703L
    enum class byte : unsigned char {};

    // byte operations
    template<class IntegerType>
    constexpr byte& operator<<=(byte& b, IntegerType shift) noexcept {
        return b = byte(static_cast<unsigned char>(b) << shift);
    }

    template<class IntegerType>
    constexpr byte operator<<(byte b, IntegerType shift) noexcept {
        return byte(static_cast<unsigned char>(b) << shift);
    }

    template<class IntegerType>
    constexpr byte& operator>>=(byte& b, IntegerType shift) noexcept {
        return b = byte(static_cast<unsigned char>(b) >> shift);
    }

    template<class IntegerType>
    constexpr byte operator>>(byte b, IntegerType shift) noexcept {
        return byte(static_cast<unsigned char>(b) >> shift);
    }

    constexpr byte& operator|=(byte& l, byte r) noexcept {
        return l = byte(static_cast<unsigned char>(l) | static_cast<unsigned char>(r));
    }

    constexpr byte operator|(byte l, byte r) noexcept {
        return byte(static_cast<unsigned char>(l) | static_cast<unsigned char>(r));
    }

    constexpr byte& operator&=(byte& l, byte r) noexcept {
        return l = byte(static_cast<unsigned char>(l) & static_cast<unsigned char>(r));
    }

    constexpr byte operator&(byte l, byte r) noexcept {
        return byte(static_cast<unsigned char>(l) & static_cast<unsigned char>(r));
    }

    constexpr byte& operator^=(byte& l, byte r) noexcept {
        return l = byte(static_cast<unsigned char>(l) ^ static_cast<unsigned char>(r));
    }

    constexpr byte operator^(byte l, byte r) noexcept {
        return byte(static_cast<unsigned char>(l) ^ static_cast<unsigned char>(r));
    }

    constexpr byte operator~(byte b) noexcept {
        return byte(~static_cast<unsigned char>(b));
    }

    template<class IntegerType>
    constexpr IntegerType to_integer(byte b) noexcept {
        return static_cast<IntegerType>(b);
    }
#endif
}

#endif /* RINCXX_CSTDDEF_H */
