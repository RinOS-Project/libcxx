/*
 * RinOS C++20 <bit> header
 * Bit manipulation functions
 */

#ifndef RINCXX_BIT_H
#define RINCXX_BIT_H

#include "version.h"
#include "type_traits.h"
#include "cstdint.h"

#if __cplusplus >= 202002L
namespace std {

namespace __bit_detail {

/* All currently supported Rin targets use 8-bit bytes.  MSVC does not
 * publish the GNU spelling used by the other frontends. */
#if defined(_MSC_VER)
inline constexpr int bits_per_byte = 8;
#else
inline constexpr int bits_per_byte = __CHAR_BIT__;
#endif

template<typename T>
inline constexpr bool is_unsigned_integer_v =
    is_same_v<remove_cv_t<T>, unsigned char>
    || is_same_v<remove_cv_t<T>, unsigned short>
    || is_same_v<remove_cv_t<T>, unsigned int>
    || is_same_v<remove_cv_t<T>, unsigned long>
    || is_same_v<remove_cv_t<T>, unsigned long long>
#if defined(__SIZEOF_INT128__)
    || is_same_v<remove_cv_t<T>, unsigned __int128>
#endif
    ;

} // namespace __bit_detail

// Bit cast (C++20)
template<typename To, typename From>
requires (sizeof(To) == sizeof(From)
          && is_trivially_copyable_v<To>
          && is_trivially_copyable_v<From>)
constexpr To bit_cast(const From& from) noexcept {
#if defined(__clang__) && defined(__has_builtin)
#if __has_builtin(__builtin_bit_cast)
    return __builtin_bit_cast(To, from);
#else
#error "RinOS bit_cast requires Clang's constexpr compiler builtin"
#endif
#elif defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 11
    return __builtin_bit_cast(To, from);
#elif defined(_MSC_VER) && _MSC_VER >= 1928 && \
    (defined(_M_IX86) || defined(_M_X64) || defined(_M_AMD64))
    return __builtin_bit_cast(To, from);
#else
#error "RinOS bit_cast has no verified constexpr backend for this compiler"
#endif
}

// Reverse object-representation bytes (C++23).
#if defined(__cplusplus) && __cplusplus > 202002L
template<typename T>
requires is_integral_v<T>
[[nodiscard]] constexpr T byteswap(T value) noexcept {
    /* A one-byte integral value is already byte-reversed.  Keep this before
     * make_unsigned so bool participates exactly as the standard integral
     * overload does without instantiating an invalid unsigned counterpart. */
    if constexpr (sizeof(T) == 1) {
        return value;
    } else {
        using U = make_unsigned_t<T>;
        constexpr int byte_width = __bit_detail::bits_per_byte;
        const U byte_mask = static_cast<U>(~U(0)) >>
            ((sizeof(U) - 1) * byte_width);
        U input = static_cast<U>(value);
        U output = 0;

        for (size_t index = 0; index < sizeof(U); ++index) {
            output = static_cast<U>((output << byte_width) |
                                    (input & byte_mask));
            input = static_cast<U>(input >> byte_width);
        }
        /* Preserve the reversed object representation for signed integral
         * types instead of relying on an out-of-range unsigned conversion. */
        return bit_cast<T>(output);
    }
}
#endif

// Count trailing zeros
template<typename T>
requires __bit_detail::is_unsigned_integer_v<T>
constexpr int countr_zero(T x) noexcept {
    constexpr int width = sizeof(T) * __bit_detail::bits_per_byte;
    if (x == 0) return width;

    int count = 0;
    while ((x & T(1)) == 0) {
        ++count;
        x = static_cast<T>(x >> 1);
    }
    return count;
}

// Count leading zeros
template<typename T>
requires __bit_detail::is_unsigned_integer_v<T>
constexpr int countl_zero(T x) noexcept {
    constexpr int width = sizeof(T) * __bit_detail::bits_per_byte;
    if (x == 0) return width;

    int count = 0;
    T bit = static_cast<T>(T(1) << (width - 1));
    while ((x & bit) == 0) {
        ++count;
        bit = static_cast<T>(bit >> 1);
    }
    return count;
}

// Count trailing ones
template<typename T>
requires __bit_detail::is_unsigned_integer_v<T>
constexpr int countr_one(T x) noexcept {
    return countr_zero(static_cast<T>(~x));
}

// Count leading ones
template<typename T>
requires __bit_detail::is_unsigned_integer_v<T>
constexpr int countl_one(T x) noexcept {
    return countl_zero(static_cast<T>(~x));
}

// Population count (count set bits)
template<typename T>
requires __bit_detail::is_unsigned_integer_v<T>
constexpr int popcount(T x) noexcept {
    int count = 0;
    while (x != 0) {
        ++count;
        x = static_cast<T>(x & static_cast<T>(x - 1));
    }
    return count;
}

// Check if power of 2
template<typename T>
requires __bit_detail::is_unsigned_integer_v<T>
constexpr bool has_single_bit(T x) noexcept {
    return x != 0 && (x & (x - 1)) == 0;
}

// Bit width
template<typename T>
requires __bit_detail::is_unsigned_integer_v<T>
constexpr int bit_width(T x) noexcept {
    return sizeof(T) * __bit_detail::bits_per_byte - countl_zero(x);
}

// Bit ceil (round up to power of 2)
template<typename T>
requires __bit_detail::is_unsigned_integer_v<T>
constexpr T bit_ceil(T x) noexcept {
    if (x <= 1) return 1;
    return T(1) << bit_width(T(x - 1));
}

// Bit floor (round down to power of 2)
template<typename T>
requires __bit_detail::is_unsigned_integer_v<T>
constexpr T bit_floor(T x) noexcept {
    if (x == 0) return 0;
    return T(1) << (bit_width(x) - 1);
}

// Rotate left
template<typename T>
requires __bit_detail::is_unsigned_integer_v<T>
constexpr T rotl(T x, int s) noexcept {
    constexpr int N = sizeof(T) * __bit_detail::bits_per_byte;
    int r = s % N;
    if (r == 0) return x;
    if (r < 0) r += N;
    return (x << r) | (x >> (N - r));
}

// Rotate right
template<typename T>
requires __bit_detail::is_unsigned_integer_v<T>
constexpr T rotr(T x, int s) noexcept {
    constexpr int N = sizeof(T) * __bit_detail::bits_per_byte;
    int r = s % N;
    if (r == 0) return x;
    if (r < 0) r += N;
    return (x >> r) | (x << (N - r));
}

// Endian detection
#if defined(_MSC_VER)
/* The verified MSVC x86/x64 targets are little-endian and do not publish the
 * GNU byte-order macros. */
enum class endian {
    little = 0,
    big    = 1,
    native = little
};
#else
enum class endian {
    little = __ORDER_LITTLE_ENDIAN__,
    big    = __ORDER_BIG_ENDIAN__,
    native = __BYTE_ORDER__
};
#endif

} // namespace std
#endif /* __cplusplus >= 202002L */

#endif /* RINCXX_BIT_H */
