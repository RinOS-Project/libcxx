/* SPDX-License-Identifier: MIT */
#ifndef RINCXX_NEXTAFTER_H
#define RINCXX_NEXTAFTER_H

#include "rincxx.h"

#ifdef __cplusplus

namespace std {
namespace __detail {

inline void __nextafter_step_ieee_encoding(unsigned char* bytes,
                                           unsigned int byte_count,
                                           bool little_endian,
                                           bool increase)
{
    for (unsigned int offset = 0; offset < byte_count; ++offset) {
        const unsigned int index = little_endian
                                 ? offset : byte_count - 1u - offset;
        if (increase) {
            ++bytes[index];
            if (bytes[index] != 0u) return;
        } else {
            const unsigned char previous = bytes[index];
            --bytes[index];
            if (previous != 0u) return;
        }
    }
}

inline void __nextafter_initialize_ieee_minimum(unsigned char* bytes,
                                                unsigned int byte_count,
                                                bool little_endian,
                                                bool negative)
{
    for (unsigned int i = 0; i < byte_count; ++i) bytes[i] = 0u;
    bytes[little_endian ? 0u : byte_count - 1u] = 1u;
    if (negative) bytes[little_endian ? byte_count - 1u : 0u] = 0x80u;
}

/* x87 extended precision stores a 64-bit explicit-integer-bit significand
 * and a 16-bit sign/exponent.  Its ten-byte payload can be padded by the ABI,
 * so the caller supplies the full object but this owner only changes bytes
 * zero through nine. */
inline void __nextafter_x87_initialize_minimum(unsigned char* bytes,
                                               bool little_endian,
                                               bool negative)
{
    for (unsigned int i = 0; i < 10u; ++i) bytes[i] = 0u;
    if (little_endian) {
        bytes[0] = 1u;
        bytes[9] = negative ? 0x80u : 0u;
    } else {
        bytes[9] = 1u;
        bytes[0] = negative ? 0x80u : 0u;
    }
}

inline void __nextafter_x87_step(unsigned char* bytes, bool little_endian,
                                 bool increase_magnitude)
{
    unsigned long long significand = 0;
    unsigned int sign_exponent;
    if (little_endian) {
        for (unsigned int i = 0; i < 8u; ++i) {
            significand |= static_cast<unsigned long long>(bytes[i]) <<
                           (8u * i);
        }
        sign_exponent = static_cast<unsigned int>(bytes[8]) |
                        (static_cast<unsigned int>(bytes[9]) << 8u);
    } else {
        for (unsigned int i = 0; i < 8u; ++i) {
            significand = (significand << 8u) | bytes[2u + i];
        }
        sign_exponent = (static_cast<unsigned int>(bytes[0]) << 8u) |
                        static_cast<unsigned int>(bytes[1]);
    }

    const bool negative = (sign_exponent & 0x8000u) != 0u;
    unsigned int exponent = sign_exponent & 0x7fffu;
    constexpr unsigned long long integer_bit = 1ull << 63u;
    constexpr unsigned long long maximum_significand = ~0ull;
    if (increase_magnitude) {
        if (exponent == 0u) {
            if (significand < integer_bit - 1u) {
                ++significand;
            } else {
                exponent = 1u;
                significand = integer_bit;
            }
        } else if (significand != maximum_significand) {
            ++significand;
        } else {
            ++exponent;
            significand = integer_bit;
        }
    } else if (exponent == 0u) {
        --significand;
    } else if (significand == integer_bit) {
        --exponent;
        significand = exponent == 0u ? integer_bit - 1u
                                     : maximum_significand;
    } else {
        --significand;
    }
    sign_exponent = (negative ? 0x8000u : 0u) | exponent;

    if (little_endian) {
        for (unsigned int i = 0; i < 8u; ++i) {
            bytes[i] = static_cast<unsigned char>(significand >> (8u * i));
        }
        bytes[8] = static_cast<unsigned char>(sign_exponent);
        bytes[9] = static_cast<unsigned char>(sign_exponent >> 8u);
    } else {
        bytes[0] = static_cast<unsigned char>(sign_exponent >> 8u);
        bytes[1] = static_cast<unsigned char>(sign_exponent);
        for (unsigned int i = 0; i < 8u; ++i) {
            bytes[2u + i] = static_cast<unsigned char>(
                significand >> (8u * (7u - i)));
        }
    }
}

inline unsigned long long __nextafter_double_encoding(double value)
{
    unsigned long long encoded;
    __builtin_memcpy(&encoded, &value, sizeof(encoded));
    return encoded;
}

inline double __nextafter_double_from_encoding(unsigned long long encoded)
{
    double value;
    __builtin_memcpy(&value, &encoded, sizeof(value));
    return value;
}

inline void __nextafter_initialize_double_double_minimum(
    bool negative, double* result_high, double* result_low)
{
    *result_high = __nextafter_double_from_encoding(
        (negative ? 1ull << 63u : 0ull) | 1ull);
    *result_low = 0.0;
}

inline void __nextafter_step_double_double(double high, double low,
                                           bool toward_greater,
                                           double* result_high,
                                           double* result_low)
{
    constexpr unsigned long long sign_bit = 1ull << 63u;
    constexpr unsigned long long magnitude_mask = sign_bit - 1ull;
    constexpr unsigned long long infinity = 0x7ff0000000000000ull;
    constexpr unsigned long long maximum_high = 0x7fefffffffffffffull;
    constexpr unsigned long long maximum_low = 0x7c8ffffffffffffeull;
    const unsigned long long high_encoding =
        __nextafter_double_encoding(high);
    const unsigned long long low_encoding =
        __nextafter_double_encoding(low);
    const unsigned long long high_magnitude =
        high_encoding & magnitude_mask;

    if (high_magnitude == infinity) {
        const bool negative = (high_encoding & sign_bit) != 0ull;
        *result_high = __nextafter_double_from_encoding(
            (negative ? sign_bit : 0ull) | maximum_high);
        *result_low = __nextafter_double_from_encoding(
            (negative ? sign_bit : 0ull) | maximum_low);
        return;
    }

    if (high_magnitude == maximum_high &&
        (low_encoding & magnitude_mask) == maximum_low &&
        (((high_encoding & sign_bit) == 0ull && toward_greater) ||
         ((high_encoding & sign_bit) != 0ull && !toward_greater))) {
        const unsigned long long sign = high_encoding & sign_bit;
        *result_high = __nextafter_double_from_encoding(sign | infinity);
        *result_low = __nextafter_double_from_encoding(sign);
        return;
    }

    unsigned int exponent_bits =
        static_cast<unsigned int>(high_magnitude >> 52u);
    int exponent = exponent_bits == 0u
                 ? -1022 : static_cast<int>(exponent_bits) - 1023;
    const bool exact_high_power =
        exponent_bits != 0u &&
        (high_magnitude & 0x000fffffffffffffull) == 0ull;
    const bool low_nonzero = (low_encoding & magnitude_mask) != 0ull;
    const bool low_opposes_high = low_nonzero &&
        ((low_encoding ^ high_encoding) & sign_bit) != 0ull;
    const bool reducing_magnitude =
        (high_encoding & sign_bit) == 0ull
            ? !toward_greater : toward_greater;
    if (exact_high_power &&
        (low_opposes_high || (!low_nonzero && reducing_magnitude))) {
        --exponent;
    }

    int step_exponent = exponent - 105;
    unsigned long long step_encoding;
    if (step_exponent <= -1074) {
        step_encoding = 1ull;
    } else if (step_exponent < -1022) {
        step_encoding =
            1ull << static_cast<unsigned int>(step_exponent + 1074);
    } else {
        step_encoding = static_cast<unsigned long long>(step_exponent + 1023)
                      << 52u;
    }
    if (!toward_greater) step_encoding |= sign_bit;
    const double step = __nextafter_double_from_encoding(step_encoding);

    const double sum = high + step;
    const double sum_delta = sum - high;
    double error = (high - (sum - sum_delta)) + (step - sum_delta);
    error += low;
    double next_high = sum + error;
    double next_low = error - (next_high - sum);

    if ((__nextafter_double_encoding(next_high) & magnitude_mask) == 0ull &&
        (__nextafter_double_encoding(next_low) & magnitude_mask) == 0ull &&
        (high_encoding & sign_bit) != 0ull) {
        next_high = __nextafter_double_from_encoding(sign_bit);
        next_low = 0.0;
    }
    *result_high = next_high;
    *result_low = next_low;
}

inline float __nextafter(float from, float to)
{
    if (from == to) return to;
    if (from != from || to != to) return from + to;
    unsigned int encoded;
    __builtin_memcpy(&encoded, &from, sizeof(encoded));
    if (from == 0.0f) {
        encoded = to < 0.0f ? (1u << 31u) | 1u : 1u;
    } else if ((from > 0.0f) == (to > from)) {
        ++encoded;
    } else {
        --encoded;
    }
    float result;
    __builtin_memcpy(&result, &encoded, sizeof(result));
    return result;
}

inline double __nextafter(double from, double to)
{
    if (from == to) return to;
    if (from != from || to != to) return from + to;
    unsigned long long encoded;
    __builtin_memcpy(&encoded, &from, sizeof(encoded));
    if (from == 0.0) {
        encoded = to < 0.0 ? (1ull << 63u) | 1u : 1u;
    } else if ((from > 0.0) == (to > from)) {
        ++encoded;
    } else {
        --encoded;
    }
    double result;
    __builtin_memcpy(&result, &encoded, sizeof(result));
    return result;
}

inline long double __nextafter(long double from, long double to)
{
#if defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
    __LDBL_MANT_DIG__ == 113 && __LDBL_MAX_EXP__ == 16384 && \
    defined(__SIZEOF_LONG_DOUBLE__) && __SIZEOF_LONG_DOUBLE__ == 16 && \
    defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
    defined(__ORDER_BIG_ENDIAN__) && \
    (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ || \
     __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    if (from == to) return to;
    if (from != from || to != to) return from + to;

    constexpr bool little_endian =
        __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__;
    constexpr unsigned int byte_count = 16u;
    unsigned char bytes[byte_count];
    __builtin_memcpy(bytes, &from, byte_count);

    if (from == 0.0L) {
        __nextafter_initialize_ieee_minimum(bytes, byte_count, little_endian,
                                            to < 0.0L);
    } else {
        const bool increase_encoding =
            (from > 0.0L) == (to > from);
        __nextafter_step_ieee_encoding(bytes, byte_count, little_endian,
                                       increase_encoding);
    }

    long double result;
    __builtin_memcpy(&result, bytes, byte_count);
    return result;
#elif defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
      __LDBL_MANT_DIG__ == 106 && __LDBL_MAX_EXP__ == 1024 && \
      defined(__SIZEOF_LONG_DOUBLE__) && __SIZEOF_LONG_DOUBLE__ == 16
    if (from != from || to != to) return from + to;
    if (from == to) return to;

    unsigned char from_bytes[16];
    unsigned char to_bytes[16];
    double high;
    double low;
    double to_high;
    __builtin_memcpy(from_bytes, &from, sizeof(from_bytes));
    __builtin_memcpy(to_bytes, &to, sizeof(to_bytes));
    __builtin_memcpy(&high, from_bytes, sizeof(high));
    __builtin_memcpy(&low, from_bytes + sizeof(high), sizeof(low));
    __builtin_memcpy(&to_high, to_bytes, sizeof(to_high));

    double result_high;
    double result_low;
    if (from == 0.0L) {
        __nextafter_initialize_double_double_minimum(
            (__nextafter_double_encoding(to_high) & (1ull << 63u)) != 0ull,
            &result_high, &result_low);
    } else {
        __nextafter_step_double_double(high, low, from < to,
                                       &result_high, &result_low);
    }

    unsigned char result_bytes[16];
    __builtin_memcpy(result_bytes, &result_high, sizeof(result_high));
    __builtin_memcpy(result_bytes + sizeof(result_high), &result_low,
                     sizeof(result_low));
    long double result;
    __builtin_memcpy(&result, result_bytes, sizeof(result));
    return result;
#elif defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
      __LDBL_MANT_DIG__ == 64 && __LDBL_MAX_EXP__ == 16384 && \
      defined(__SIZEOF_LONG_DOUBLE__) && __SIZEOF_LONG_DOUBLE__ >= 10 && \
      defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
      __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    if (from == to) return to;
    if (from != from || to != to) return from + to;

    unsigned char bytes[sizeof(long double)];
    __builtin_memcpy(bytes, &from, sizeof(long double));
    if (from == 0.0L) {
        __nextafter_x87_initialize_minimum(bytes, true, to < 0.0L);
    } else {
        const bool negative = (bytes[9] & 0x80u) != 0u;
        __nextafter_x87_step(bytes, true,
                             negative ? to < from : to > from);
    }
    long double result;
    __builtin_memcpy(&result, bytes, sizeof(long double));
    return result;
#elif defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
      __LDBL_MANT_DIG__ == 64 && __LDBL_MAX_EXP__ == 16384 && \
      defined(__SIZEOF_LONG_DOUBLE__) && __SIZEOF_LONG_DOUBLE__ >= 10 && \
      defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
      __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    if (from == to) return to;
    if (from != from || to != to) return from + to;

    unsigned char bytes[sizeof(long double)];
    __builtin_memcpy(bytes, &from, sizeof(long double));
    if (from == 0.0L) {
        __nextafter_x87_initialize_minimum(bytes, false, to < 0.0L);
    } else {
        const bool negative = (bytes[0] & 0x80u) != 0u;
        __nextafter_x87_step(bytes, false,
                             negative ? to < from : to > from);
    }
    long double result;
    __builtin_memcpy(&result, bytes, sizeof(long double));
    return result;
#elif defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
      __LDBL_MANT_DIG__ == 53 && __LDBL_MAX_EXP__ == 1024
    return static_cast<long double>(
        __nextafter(static_cast<double>(from), static_cast<double>(to)));
#elif !defined(RIN_CXX_NEXTAFTER_FORCE_UNKNOWN_FALLBACK) && \
      (defined(__GNUC__) || defined(__clang__))
    /* For an ABI not covered by the raw encodings above, keep the compiler's
     * long-double representation intact instead of silently narrowing or
     * trapping.  GCC and Clang lower this builtin to the active ABI's
     * nextafterl implementation. */
    return __builtin_nextafterl(from, to);
#else
    /* A non-GCC/Clang compiler may expose a long-double ABI for which Rin's
     * raw encodings are not known and provide no nextafterl builtin.  Do not
     * turn that public operation into a successful-looking trap.  Preserve
     * the ordered/NaN/equality contract and return the closest value visible
     * through the supported binary64 conversion.  This is intentionally a
     * bounded compatibility fallback: adjacent values that only exist in the
     * wider representation still require an ABI-specific implementation and
     * remain tracked by the cmath TODO. */
    if (from == to) return to;
    if (from != from || to != to) return from + to;
    return static_cast<long double>(
        __nextafter(static_cast<double>(from), static_cast<double>(to)));
#endif
}

} /* namespace __detail */
} /* namespace std */

#endif /* __cplusplus */
#endif /* RINCXX_NEXTAFTER_H */
