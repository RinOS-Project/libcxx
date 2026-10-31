/*
 * RinOS C++ <cstddef> header
 * C++ wrapper for stddef.h types
 */

#ifndef RINCXX_CSTDDEF_H
#define RINCXX_CSTDDEF_H

/* A host <cstddef> (libstdc++) may share a TU with this header when
 * product code mixes RinOS libcxx with host-STL-based runtime headers.
 * Whichever runs first wins: preclaim the libstdc++ guard so a later
 * host <cstddef> skips its body (cf. cstdlib.h), and skip ours when
 * the host already ran. */
#if defined(_GLIBCXX_CSTDDEF)
#define RINCXX_CSTDDEF_HOST_NAMESPACE 1
#else
#define _GLIBCXX_CSTDDEF 1
#endif

#ifndef RINCXX_CSTDDEF_HOST_NAMESPACE

#include "../libc/stddef.h"

namespace std {
    using ::size_t;
    using ::ptrdiff_t;

    // nullptr_t - defined here since not in C stddef.h
    using nullptr_t = decltype(nullptr);

    // max_align_t - use the target ABI's complete object type when available.
#if defined(__RINOS__)
    using max_align_t = ::max_align_t;
#elif defined(__BIGGEST_ALIGNMENT__)
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

#endif /* !RINCXX_CSTDDEF_HOST_NAMESPACE */

#endif /* RINCXX_CSTDDEF_H */
