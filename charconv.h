/*
 * RinOS C++ <charconv>
 * Elementary string conversions (C++17/20)
 */

#ifndef RINCXX_CHARCONV_H
#define RINCXX_CHARCONV_H

#include "rincxx.h"

/* std::to_chars and std::from_chars are C++17 facilities. */
#if defined(__cplusplus) && __cplusplus >= 201703L

#include "cstddef.h"
#include "type_traits.h"
#include "cstdint.h"
#include "limits.h"
#include "system_error.h"

/* P0067R5 defines the C++17 charconv surface.  P2497R0 adds the explicit
 * result conversion in C++26 and bumps the representative feature-test
 * macro.  GCC 13's C++2b mode still reports __cplusplus == 202100L, so the
 * latter is an explicit RinOS preview until a final C++26 dialect exists. */
#if __cplusplus > 202302L || defined(RIN_ENABLE_CXX26_CHARCONV)
#define __cpp_lib_to_chars 202306L
#define RIN_LIBCXX_CHARCONV_HAS_RESULT_BOOL 1
#else
#define __cpp_lib_to_chars 201611L
#endif

namespace std {

/* ===================================================================
 * chars_format - floating-point formatting
 * ===================================================================*/

enum class chars_format {
    scientific = 1,
    fixed = 2,
    hex = 4,
    general = fixed | scientific
};

constexpr chars_format operator|(chars_format lhs, chars_format rhs) noexcept {
    return static_cast<chars_format>(
        static_cast<int>(lhs) | static_cast<int>(rhs)
    );
}

constexpr chars_format operator&(chars_format lhs, chars_format rhs) noexcept {
    return static_cast<chars_format>(
        static_cast<int>(lhs) & static_cast<int>(rhs)
    );
}

constexpr chars_format operator^(chars_format lhs, chars_format rhs) noexcept {
    return static_cast<chars_format>(
        static_cast<int>(lhs) ^ static_cast<int>(rhs)
    );
}

constexpr chars_format operator~(chars_format fmt) noexcept {
    return static_cast<chars_format>(~static_cast<int>(fmt));
}

constexpr chars_format& operator|=(chars_format& lhs, chars_format rhs) noexcept {
    return lhs = lhs | rhs;
}

constexpr chars_format& operator&=(chars_format& lhs, chars_format rhs) noexcept {
    return lhs = lhs & rhs;
}

constexpr chars_format& operator^=(chars_format& lhs, chars_format rhs) noexcept {
    return lhs = lhs ^ rhs;
}

/* ===================================================================
 * to_chars_result / from_chars_result
 * ===================================================================*/

struct to_chars_result {
    char* ptr;
    errc ec;

    friend constexpr bool operator==(const to_chars_result& lhs,
                                      const to_chars_result& rhs) noexcept {
        return lhs.ptr == rhs.ptr && lhs.ec == rhs.ec;
    }

    friend constexpr bool operator!=(const to_chars_result& lhs,
                                      const to_chars_result& rhs) noexcept {
        return !(lhs == rhs);
    }

#if defined(RIN_LIBCXX_CHARCONV_HAS_RESULT_BOOL)
    /* C++26: operator bool */
    constexpr explicit operator bool() const noexcept {
        return ec == errc{};
    }
#endif
};

struct from_chars_result {
    const char* ptr;
    errc ec;

    friend constexpr bool operator==(const from_chars_result& lhs,
                                      const from_chars_result& rhs) noexcept {
        return lhs.ptr == rhs.ptr && lhs.ec == rhs.ec;
    }

    friend constexpr bool operator!=(const from_chars_result& lhs,
                                      const from_chars_result& rhs) noexcept {
        return !(lhs == rhs);
    }

#if defined(RIN_LIBCXX_CHARCONV_HAS_RESULT_BOOL)
    /* C++26: operator bool */
    constexpr explicit operator bool() const noexcept {
        return ec == errc{};
    }
#endif
};

/* ===================================================================
 * to_chars - integer to string conversion
 * ===================================================================*/

namespace detail {

/* Digit tables for fast conversion */
inline constexpr char digit_pairs[] =
    "00010203040506070809"
    "10111213141516171819"
    "20212223242526272829"
    "30313233343536373839"
    "40414243444546474849"
    "50515253545556575859"
    "60616263646566676869"
    "70717273747576777879"
    "80818283848586878889"
    "90919293949596979899";

inline constexpr char digit_chars[] = "0123456789abcdefghijklmnopqrstuvwxyz";

template<typename T>
inline to_chars_result to_chars_unsigned(char* first, char* last, T value, int base) {
    if (base < 2 || base > 36) {
        return {first, errc::invalid_argument};
    }
    if (first >= last) {
        return {last, errc::value_too_large};
    }

    char buffer[65]; // Maximum for 64-bit binary
    char* p = buffer + sizeof(buffer);

    if (base == 10) {
        // Optimized decimal conversion
        while (value >= 100) {
            auto const idx = (value % 100) * 2;
            value /= 100;
            *--p = digit_pairs[idx + 1];
            *--p = digit_pairs[idx];
        }
        if (value >= 10) {
            auto const idx = value * 2;
            *--p = digit_pairs[idx + 1];
            *--p = digit_pairs[idx];
        } else {
            *--p = static_cast<char>('0' + value);
        }
    } else {
        // General base conversion
        while (value >= static_cast<T>(base)) {
            *--p = digit_chars[value % base];
            value /= base;
        }
        *--p = digit_chars[value];
    }

    size_t len = static_cast<size_t>(buffer + sizeof(buffer) - p);
    if (len > static_cast<size_t>(last - first)) {
        return {last, errc::value_too_large};
    }

    for (size_t i = 0; i < len; ++i) {
        first[i] = p[i];
    }
    return {first + len, errc{}};
}

template<typename T>
inline to_chars_result to_chars_signed(char* first, char* last, T value, int base) {
    using U = make_unsigned_t<T>;

    /* Validate the base before touching caller storage.  In particular, a
     * negative value must not publish its '-' prefix when the base is invalid
     * or the destination is too small: callers observe either a complete
     * conversion or the original buffer. */
    if (base < 2 || base > 36) {
        return {first, errc::invalid_argument};
    }

    if (value < 0) {
        char magnitude[65];
        const U unsigned_value = static_cast<U>(0) - static_cast<U>(value);
        const to_chars_result converted =
            to_chars_unsigned(magnitude, magnitude + sizeof(magnitude),
                              unsigned_value, base);
        if (converted.ec != errc{}) {
            return {first, converted.ec};
        }
        const size_t digits = static_cast<size_t>(converted.ptr - magnitude);
        if (first >= last ||
            digits == static_cast<size_t>(-1) ||
            digits + 1u > static_cast<size_t>(last - first)) {
            return {last, errc::value_too_large};
        }
        *first++ = '-';
        for (size_t index = 0; index < digits; ++index) {
            first[index] = magnitude[index];
        }
        return {first + digits, errc{}};
    }
    return to_chars_unsigned(first, last, static_cast<U>(value), base);
}

} // namespace detail

/* Integer to_chars overloads */
inline to_chars_result to_chars(char* first, char* last, char value, int base = 10) {
    return detail::to_chars_signed(first, last, value, base);
}

inline to_chars_result to_chars(char* first, char* last, signed char value, int base = 10) {
    return detail::to_chars_signed(first, last, value, base);
}

inline to_chars_result to_chars(char* first, char* last, unsigned char value, int base = 10) {
    return detail::to_chars_unsigned(first, last, value, base);
}

inline to_chars_result to_chars(char* first, char* last, short value, int base = 10) {
    return detail::to_chars_signed(first, last, value, base);
}

inline to_chars_result to_chars(char* first, char* last, unsigned short value, int base = 10) {
    return detail::to_chars_unsigned(first, last, value, base);
}

inline to_chars_result to_chars(char* first, char* last, int value, int base = 10) {
    return detail::to_chars_signed(first, last, value, base);
}

inline to_chars_result to_chars(char* first, char* last, unsigned int value, int base = 10) {
    return detail::to_chars_unsigned(first, last, value, base);
}

inline to_chars_result to_chars(char* first, char* last, long value, int base = 10) {
    return detail::to_chars_signed(first, last, value, base);
}

inline to_chars_result to_chars(char* first, char* last, unsigned long value, int base = 10) {
    return detail::to_chars_unsigned(first, last, value, base);
}

inline to_chars_result to_chars(char* first, char* last, long long value, int base = 10) {
    return detail::to_chars_signed(first, last, value, base);
}

inline to_chars_result to_chars(char* first, char* last, unsigned long long value, int base = 10) {
    return detail::to_chars_unsigned(first, last, value, base);
}

/* ===================================================================
 * from_chars - string to integer conversion
 * ===================================================================*/

namespace detail {

inline constexpr int char_to_digit(char c, int base) {
    int digit = 0;
    if (c >= '0' && c <= '9') {
        digit = c - '0';
    } else if (c >= 'a' && c <= 'z') {
        digit = c - 'a' + 10;
    } else if (c >= 'A' && c <= 'Z') {
        digit = c - 'A' + 10;
    } else {
        return -1;
    }
    return digit < base ? digit : -1;
}

template<typename T>
inline from_chars_result from_chars_unsigned(const char* first, const char* last, T& value, int base) {
    if (base < 2 || base > 36 || first == last) {
        return {first, errc::invalid_argument};
    }

    const char* p = first;
    T result = 0;
    bool has_digit = false;
    bool overflow = false;

    while (p != last) {
        int digit = char_to_digit(*p, base);
        if (digit < 0) break;

        if (!overflow) {
            T converted_digit = static_cast<T>(digit);
            T converted_base = static_cast<T>(base);
            T maximum = numeric_limits<T>::max();
            if (result > (maximum - converted_digit) / converted_base) {
                overflow = true;
            } else {
                result = result * converted_base + converted_digit;
            }
        }
        has_digit = true;
        ++p;
    }

    if (!has_digit) {
        return {first, errc::invalid_argument};
    }
    if (overflow) return {p, errc::result_out_of_range};

    value = result;
    return {p, errc{}};
}

template<typename T>
inline from_chars_result from_chars_signed(const char* first, const char* last, T& value, int base) {
    if (first == last) {
        return {first, errc::invalid_argument};
    }

    const char* p = first;
    bool negative = false;

    if (*p == '-') {
        negative = true;
        ++p;
    } else if (*p == '+') {
        return {first, errc::invalid_argument};
    }

    using U = make_unsigned_t<T>;
    U unsigned_value;
    auto result = from_chars_unsigned(p, last, unsigned_value, base);

    if (result.ec != errc{}) {
        return result.ec == errc::invalid_argument
            ? from_chars_result{first, result.ec} : result;
    }

    if (negative) {
        // Check for underflow
        constexpr U max_negative = static_cast<U>(-(static_cast<T>(1) +
            numeric_limits<T>::min())) + 1;
        if (unsigned_value > max_negative) {
            return {result.ptr, errc::result_out_of_range};
        }
        value = unsigned_value == max_negative
            ? numeric_limits<T>::min()
            : static_cast<T>(-static_cast<T>(unsigned_value));
    } else {
        // Check for overflow
        if (unsigned_value > static_cast<U>(numeric_limits<T>::max())) {
            return {result.ptr, errc::result_out_of_range};
        }
        value = static_cast<T>(unsigned_value);
    }

    return result;
}

} // namespace detail

/* Integer from_chars overloads */
inline from_chars_result from_chars(const char* first, const char* last, char& value, int base = 10) {
    return detail::from_chars_signed(first, last, value, base);
}

inline from_chars_result from_chars(const char* first, const char* last, signed char& value, int base = 10) {
    return detail::from_chars_signed(first, last, value, base);
}

inline from_chars_result from_chars(const char* first, const char* last, unsigned char& value, int base = 10) {
    return detail::from_chars_unsigned(first, last, value, base);
}

inline from_chars_result from_chars(const char* first, const char* last, short& value, int base = 10) {
    return detail::from_chars_signed(first, last, value, base);
}

inline from_chars_result from_chars(const char* first, const char* last, unsigned short& value, int base = 10) {
    return detail::from_chars_unsigned(first, last, value, base);
}

inline from_chars_result from_chars(const char* first, const char* last, int& value, int base = 10) {
    return detail::from_chars_signed(first, last, value, base);
}

inline from_chars_result from_chars(const char* first, const char* last, unsigned int& value, int base = 10) {
    return detail::from_chars_unsigned(first, last, value, base);
}

inline from_chars_result from_chars(const char* first, const char* last, long& value, int base = 10) {
    return detail::from_chars_signed(first, last, value, base);
}

inline from_chars_result from_chars(const char* first, const char* last, unsigned long& value, int base = 10) {
    return detail::from_chars_unsigned(first, last, value, base);
}

inline from_chars_result from_chars(const char* first, const char* last, long long& value, int base = 10) {
    return detail::from_chars_signed(first, last, value, base);
}

inline from_chars_result from_chars(const char* first, const char* last, unsigned long long& value, int base = 10) {
    return detail::from_chars_unsigned(first, last, value, base);
}

/* ===================================================================
 * Floating-point to_chars
 * ===================================================================*/

namespace detail {

/* Decimal parsing keeps 1,100 significant input digits.  The exponent search
 * compares those digits across the complete x87 range and can require about
 * 37,000 bits even before it narrows to the adjacent binary80 boundary.
 * Formatting stages every possibly non-zero binary80 fractional digit; larger
 * requested precisions can then be published by inserting trailing zeroes. */
inline constexpr unsigned fp_big_limbs = 1216;
inline constexpr unsigned fp_binary64_staged_precision = 1100;
inline constexpr unsigned fp_binary80_staged_precision = 16500;
inline constexpr unsigned fp_digit_capacity = 21500;
inline constexpr unsigned fp_text_capacity = 21516;

struct fp_big_uint {
    uint32_t limb[fp_big_limbs];
    unsigned size;
};

struct fp_binary {
    uint64_t significand;
    uint64_t fraction;
    int exponent;
    int hex_exponent;
    unsigned fraction_bits;
    unsigned exact_hex_digits;
    unsigned staged_precision;
    unsigned hex_leading;
    bool negative;
    bool zero;
    bool infinity;
    bool nan;
    bool lower_boundary_is_closer;
    bool explicit_hex_leading;
};

inline void fp_big_zero(fp_big_uint& value) noexcept {
    value.size = 0;
}

inline void fp_big_set(fp_big_uint& value, uint64_t source) noexcept {
    fp_big_zero(value);
    if (source == 0) return;
    value.limb[0] = static_cast<uint32_t>(source);
    value.limb[1] = static_cast<uint32_t>(source >> 32);
    value.size = value.limb[1] == 0 ? 1u : 2u;
}

inline void fp_big_normalize(fp_big_uint& value) noexcept {
    while (value.size != 0 && value.limb[value.size - 1] == 0) --value.size;
}

inline int fp_big_compare(const fp_big_uint& lhs,
                          const fp_big_uint& rhs) noexcept {
    if (lhs.size != rhs.size) return lhs.size < rhs.size ? -1 : 1;
    for (unsigned i = lhs.size; i != 0; --i) {
        if (lhs.limb[i - 1] != rhs.limb[i - 1]) {
            return lhs.limb[i - 1] < rhs.limb[i - 1] ? -1 : 1;
        }
    }
    return 0;
}

inline bool fp_big_shift_left(fp_big_uint& value, unsigned bits) noexcept {
    if (value.size == 0 || bits == 0) return true;
    unsigned words = bits / 32u;
    unsigned shift = bits % 32u;
    unsigned extra = shift != 0 &&
                     (value.limb[value.size - 1] >> (32u - shift)) != 0;
    if (value.size + words + extra > fp_big_limbs) return false;
    for (unsigned i = value.size; i != 0; --i) {
        value.limb[i - 1 + words] = value.limb[i - 1];
    }
    for (unsigned i = 0; i < words; ++i) value.limb[i] = 0;
    value.size += words;
    if (shift != 0) {
        uint32_t carry = 0;
        for (unsigned i = words; i < value.size; ++i) {
            uint64_t next = (static_cast<uint64_t>(value.limb[i]) << shift) |
                            carry;
            value.limb[i] = static_cast<uint32_t>(next);
            carry = static_cast<uint32_t>(next >> 32);
        }
        if (carry != 0) value.limb[value.size++] = carry;
    }
    return true;
}

inline bool fp_big_multiply_small(fp_big_uint& value,
                                  uint32_t multiplier) noexcept {
    if (value.size == 0 || multiplier == 1) return true;
    if (multiplier == 0) {
        fp_big_zero(value);
        return true;
    }
    uint64_t carry = 0;
    for (unsigned i = 0; i < value.size; ++i) {
        uint64_t product = static_cast<uint64_t>(value.limb[i]) * multiplier +
                           carry;
        value.limb[i] = static_cast<uint32_t>(product);
        carry = product >> 32;
    }
    if (carry != 0) {
        if (value.size == fp_big_limbs) return false;
        value.limb[value.size++] = static_cast<uint32_t>(carry);
    }
    return true;
}

inline bool fp_big_add_small(fp_big_uint& value, uint32_t addend) noexcept {
    if (addend == 0u) return true;
    if (value.size == 0u) {
        value.limb[0] = addend;
        value.size = 1u;
        return true;
    }
    uint64_t carry = addend;
    unsigned index = 0;
    while (carry != 0u && index < value.size) {
        uint64_t sum = static_cast<uint64_t>(value.limb[index]) + carry;
        value.limb[index++] = static_cast<uint32_t>(sum);
        carry = sum >> 32;
    }
    if (carry != 0u) {
        if (value.size == fp_big_limbs) return false;
        value.limb[value.size++] = static_cast<uint32_t>(carry);
    }
    return true;
}

inline bool fp_big_multiply_power5(fp_big_uint& value,
                                   unsigned exponent) noexcept {
    for (unsigned index = 0; index < exponent; ++index) {
        if (!fp_big_multiply_small(value, 5u)) return false;
    }
    return true;
}

inline bool fp_big_add(fp_big_uint& value,
                       const fp_big_uint& addend) noexcept {
    unsigned limit = value.size > addend.size ? value.size : addend.size;
    if (limit > fp_big_limbs) return false;
    uint64_t carry = 0;
    for (unsigned i = 0; i < limit; ++i) {
        uint64_t sum = carry;
        if (i < value.size) sum += value.limb[i];
        if (i < addend.size) sum += addend.limb[i];
        value.limb[i] = static_cast<uint32_t>(sum);
        carry = sum >> 32;
    }
    value.size = limit;
    if (carry != 0) {
        if (value.size == fp_big_limbs) return false;
        value.limb[value.size++] = static_cast<uint32_t>(carry);
    }
    return true;
}

inline void fp_big_subtract(fp_big_uint& value,
                            const fp_big_uint& subtrahend) noexcept {
    uint64_t borrow = 0;
    for (unsigned i = 0; i < value.size; ++i) {
        uint64_t left = value.limb[i];
        uint64_t right = (i < subtrahend.size ? subtrahend.limb[i] : 0u) +
                         borrow;
        value.limb[i] = static_cast<uint32_t>(left - right);
        borrow = left < right ? 1u : 0u;
    }
    fp_big_normalize(value);
}

inline unsigned fp_big_digit(fp_big_uint& remainder,
                             const fp_big_uint& divisor) noexcept {
    unsigned digit = 0;
    while (fp_big_compare(remainder, divisor) >= 0) {
        fp_big_subtract(remainder, divisor);
        ++digit;
    }
    return digit;
}

inline fp_binary fp_decode(float value) noexcept {
    union { float value; uint32_t bits; } bits = {value};
    uint32_t exponent = (bits.bits >> 23) & 0xffu;
    uint32_t fraction = bits.bits & 0x7fffffu;
    fp_binary result{};
    result.fraction = fraction;
    result.fraction_bits = 23;
    result.exact_hex_digits = 6;
    result.staged_precision = fp_binary64_staged_precision;
    result.negative = (bits.bits >> 31) != 0;
    result.nan = exponent == 0xffu && fraction != 0;
    result.infinity = exponent == 0xffu && fraction == 0;
    result.zero = exponent == 0 && fraction == 0;
    if (exponent == 0) {
        result.significand = fraction;
        result.exponent = -149;
        result.hex_exponent = -126;
    } else if (exponent != 0xffu) {
        result.significand = (uint64_t{1} << 23) | fraction;
        result.exponent = static_cast<int>(exponent) - 127 - 23;
        result.hex_exponent = static_cast<int>(exponent) - 127;
        result.lower_boundary_is_closer = fraction == 0 && exponent > 1;
    }
    return result;
}

inline fp_binary fp_decode(double value) noexcept {
    union { double value; uint64_t bits; } bits = {value};
    unsigned exponent = static_cast<unsigned>((bits.bits >> 52) & 0x7ffu);
    uint64_t fraction = bits.bits & 0xfffffffffffffu;
    fp_binary result{};
    result.fraction = fraction;
    result.fraction_bits = 52;
    result.exact_hex_digits = 13;
    result.staged_precision = fp_binary64_staged_precision;
    result.negative = (bits.bits >> 63) != 0;
    result.nan = exponent == 0x7ffu && fraction != 0;
    result.infinity = exponent == 0x7ffu && fraction == 0;
    result.zero = exponent == 0 && fraction == 0;
    if (exponent == 0) {
        result.significand = fraction;
        result.exponent = -1074;
        result.hex_exponent = -1022;
    } else if (exponent != 0x7ffu) {
        result.significand = (uint64_t{1} << 52) | fraction;
        result.exponent = static_cast<int>(exponent) - 1023 - 52;
        result.hex_exponent = static_cast<int>(exponent) - 1023;
        result.lower_boundary_is_closer = fraction == 0 && exponent > 1;
    }
    return result;
}

#if defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
    __LDBL_MANT_DIG__ == 64 && __LDBL_MAX_EXP__ == 16384 && \
    defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
    defined(__ORDER_BIG_ENDIAN__) && \
    (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ || \
     __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
inline void fp_binary80_parts(long double value, uint64_t& significand,
                              uint16_t& exponent_sign) noexcept {
    union {
        long double value;
        unsigned char bytes[sizeof(long double)];
    } raw{};
    raw.value = value;
    significand = 0;
    if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) {
        for (unsigned i = 0; i < 8u; ++i) {
            significand |= static_cast<uint64_t>(raw.bytes[i]) << (i * 8u);
        }
        exponent_sign = static_cast<uint16_t>(raw.bytes[8]) |
            static_cast<uint16_t>(static_cast<uint16_t>(raw.bytes[9]) << 8u);
    } else {
        for (unsigned i = 0; i < 8u; ++i) {
            significand = (significand << 8u) | raw.bytes[2u + i];
        }
        exponent_sign = static_cast<uint16_t>(raw.bytes[1]) |
            static_cast<uint16_t>(static_cast<uint16_t>(raw.bytes[0]) << 8u);
    }
}

inline long double fp_binary80_value(uint64_t significand,
                                     uint16_t encoded_exponent,
                                     bool negative) noexcept {
    union {
        long double value;
        unsigned char bytes[sizeof(long double)];
    } raw{};
    uint16_t exponent_sign = static_cast<uint16_t>(encoded_exponent |
        (negative ? 0x8000u : 0u));
    if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) {
        for (unsigned i = 0; i < 8u; ++i) {
            raw.bytes[i] = static_cast<unsigned char>(significand >> (i * 8u));
        }
        raw.bytes[8] = static_cast<unsigned char>(exponent_sign);
        raw.bytes[9] = static_cast<unsigned char>(exponent_sign >> 8u);
    } else {
        raw.bytes[0] = static_cast<unsigned char>(exponent_sign >> 8u);
        raw.bytes[1] = static_cast<unsigned char>(exponent_sign);
        for (unsigned i = 0; i < 8u; ++i) {
            raw.bytes[2u + i] = static_cast<unsigned char>(
                significand >> (8u * (7u - i)));
        }
    }
    return raw.value;
}

inline fp_binary fp_decode(long double value) noexcept {
    uint64_t significand;
    uint16_t exponent_sign;
    fp_binary80_parts(value, significand, exponent_sign);
    unsigned encoded_exponent = exponent_sign & 0x7fffu;
    bool integer_bit = (significand >> 63u) != 0u;
    fp_binary result{};
    result.staged_precision = fp_binary80_staged_precision;
    result.explicit_hex_leading = true;
    result.negative = (exponent_sign & 0x8000u) != 0u;
    result.nan = encoded_exponent == 0x7fffu &&
                 significand != 0x8000000000000000u;
    result.infinity = encoded_exponent == 0x7fffu &&
                      significand == 0x8000000000000000u;
    result.zero = encoded_exponent == 0u && significand == 0u;
    result.fraction = significand & 0x0fffffffffffffffu;
    result.fraction_bits = 60u;
    result.exact_hex_digits = 15u;
    result.hex_leading = static_cast<unsigned>(significand >> 60u);
    if (encoded_exponent == 0u) {
        result.significand = significand;
        result.exponent = -16445;
        result.hex_exponent = -16385;
        if (integer_bit) {
            /* Canonicalize an x87 pseudo-denormal to the equivalent minimum
             * normal value before decimal boundary generation. */
            result.exponent = -16445;
        }
    } else if (encoded_exponent != 0x7fffu && integer_bit) {
        result.significand = significand;
        result.exponent = static_cast<int>(encoded_exponent) - 16383 - 63;
        result.hex_exponent = static_cast<int>(encoded_exponent) - 16383 - 3;
        result.lower_boundary_is_closer = significand ==
                                          (uint64_t{1} << 63u) &&
                                          encoded_exponent > 1u;
    } else if (encoded_exponent != 0x7fffu) {
        /* Unsupported x87 unnormal encodings are not finite C++ values. */
        result.nan = true;
    }
    return result;
}
#elif defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
      __LDBL_MANT_DIG__ == 53 && __LDBL_MAX_EXP__ == 1024
inline fp_binary fp_decode(long double value) noexcept {
    return fp_decode(static_cast<double>(value));
}
#endif

inline bool fp_value_is_subnormal(float value) noexcept {
    union { float value; uint32_t bits; } raw = {value};
    return (raw.bits & 0x7f800000u) == 0u &&
           (raw.bits & 0x007fffffu) != 0u;
}

inline bool fp_value_is_subnormal(double value) noexcept {
    union { double value; uint64_t bits; } raw = {value};
    return (raw.bits & (uint64_t{0x7ff} << 52u)) == 0u &&
           (raw.bits & ((uint64_t{1} << 52u) - 1u)) != 0u;
}

#if defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
    __LDBL_MANT_DIG__ == 64 && __LDBL_MAX_EXP__ == 16384 && \
    defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
    defined(__ORDER_BIG_ENDIAN__) && \
    (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ || \
     __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
inline bool fp_value_is_subnormal(long double value) noexcept {
    uint64_t significand;
    uint16_t exponent_sign;
    fp_binary80_parts(value, significand, exponent_sign);
    return (exponent_sign & 0x7fffu) == 0u && significand != 0u;
}
#elif defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
      __LDBL_MANT_DIG__ == 53 && __LDBL_MAX_EXP__ == 1024
inline bool fp_value_is_subnormal(long double value) noexcept {
    return fp_value_is_subnormal(static_cast<double>(value));
}
#endif

inline bool fp_decimal_ratio(const fp_binary& binary, fp_big_uint& remainder,
                             fp_big_uint& divisor, fp_big_uint& lower_margin,
                             fp_big_uint& upper_margin) noexcept {
    int common_exponent;
    if (binary.lower_boundary_is_closer) {
        fp_big_set(remainder, binary.significand);
        if (!fp_big_shift_left(remainder, 2u)) return false;
        fp_big_set(lower_margin, 1u);
        fp_big_set(upper_margin, 2u);
        common_exponent = binary.exponent - 2;
    } else {
        fp_big_set(remainder, binary.significand);
        if (!fp_big_shift_left(remainder, 1u)) return false;
        fp_big_set(lower_margin, 1u);
        fp_big_set(upper_margin, 1u);
        common_exponent = binary.exponent - 1;
    }
    fp_big_set(divisor, 1u);
    if (common_exponent >= 0) {
        unsigned shift = static_cast<unsigned>(common_exponent);
        return fp_big_shift_left(remainder, shift) &&
               fp_big_shift_left(lower_margin, shift) &&
               fp_big_shift_left(upper_margin, shift);
    }
    return fp_big_shift_left(divisor,
                             static_cast<unsigned>(-common_exponent));
}

inline bool fp_decimal_normalize(fp_big_uint& remainder, fp_big_uint& divisor,
                                 fp_big_uint& lower_margin,
                                 fp_big_uint& upper_margin,
                                 int& decimal_exponent) noexcept {
    decimal_exponent = 0;
    while (fp_big_compare(remainder, divisor) < 0) {
        if (!fp_big_multiply_small(remainder, 10u) ||
            !fp_big_multiply_small(lower_margin, 10u) ||
            !fp_big_multiply_small(upper_margin, 10u)) return false;
        --decimal_exponent;
    }
    for (;;) {
        fp_big_uint ten_divisor = divisor;
        if (!fp_big_multiply_small(ten_divisor, 10u)) return false;
        if (fp_big_compare(remainder, ten_divisor) < 0) break;
        divisor = ten_divisor;
        ++decimal_exponent;
    }
    return true;
}

inline void fp_round_digits(char* digits, unsigned count,
                            int& decimal_exponent) noexcept {
    for (unsigned i = count; i != 0; --i) {
        if (digits[i - 1] != '9') {
            ++digits[i - 1];
            return;
        }
        digits[i - 1] = '0';
    }
    digits[0] = '1';
    ++decimal_exponent;
}

inline bool fp_shortest_digits(const fp_binary& binary, char* digits,
                               unsigned& count,
                               int& decimal_exponent) noexcept {
    fp_big_uint remainder, divisor, lower_margin, upper_margin;
    if (!fp_decimal_ratio(binary, remainder, divisor, lower_margin,
                          upper_margin) ||
        !fp_decimal_normalize(remainder, divisor, lower_margin, upper_margin,
                              decimal_exponent)) return false;
    bool inclusive = (binary.significand & 1u) == 0;
    count = 0;
    for (;;) {
        if (count == 32u) return false;
        unsigned digit = fp_big_digit(remainder, divisor);
        if (digit > 9u) return false;
        digits[count++] = static_cast<char>('0' + digit);

        int lower_compare = fp_big_compare(remainder, lower_margin);
        bool low = inclusive ? lower_compare <= 0 : lower_compare < 0;
        fp_big_uint upper = remainder;
        if (!fp_big_add(upper, upper_margin)) return false;
        int upper_compare = fp_big_compare(upper, divisor);
        bool high = inclusive ? upper_compare >= 0 : upper_compare > 0;
        if (low || high) {
            bool round_up = high && !low;
            if (low && high) {
                fp_big_uint twice = remainder;
                if (!fp_big_multiply_small(twice, 2u)) return false;
                int halfway = fp_big_compare(twice, divisor);
                round_up = halfway > 0 ||
                           (halfway == 0 && ((digits[count - 1] - '0') & 1));
            }
            if (round_up) fp_round_digits(digits, count, decimal_exponent);
            while (count > 1u && digits[count - 1] == '0') --count;
            return true;
        }
        if (!fp_big_multiply_small(remainder, 10u) ||
            !fp_big_multiply_small(lower_margin, 10u) ||
            !fp_big_multiply_small(upper_margin, 10u)) return false;
    }
}

inline bool fp_significant_digits(const fp_binary& binary, unsigned requested,
                                  char* digits, int& decimal_exponent) noexcept {
    fp_big_uint remainder, divisor, lower_margin, upper_margin;
    if (requested == 0 || requested > fp_digit_capacity ||
        !fp_decimal_ratio(binary, remainder, divisor, lower_margin,
                          upper_margin) ||
        !fp_decimal_normalize(remainder, divisor, lower_margin, upper_margin,
                              decimal_exponent)) return false;
    for (unsigned i = 0; i < requested; ++i) {
        unsigned digit = fp_big_digit(remainder, divisor);
        if (digit > 9u) return false;
        digits[i] = static_cast<char>('0' + digit);
        if (i + 1u != requested &&
            !fp_big_multiply_small(remainder, 10u)) return false;
    }
    fp_big_uint twice = remainder;
    if (!fp_big_multiply_small(twice, 2u)) return false;
    int halfway = fp_big_compare(twice, divisor);
    if (halfway > 0 ||
        (halfway == 0 && ((digits[requested - 1] - '0') & 1))) {
        fp_round_digits(digits, requested, decimal_exponent);
    }
    return true;
}

inline bool fp_append(char* output, unsigned capacity, unsigned& length,
                      char value) noexcept {
    if (length == capacity) return false;
    output[length++] = value;
    return true;
}

inline bool fp_append_exponent(char* output, unsigned capacity,
                               unsigned& length, int exponent,
                               char marker, unsigned minimum_digits) noexcept {
    if (!fp_append(output, capacity, length, marker) ||
        !fp_append(output, capacity, length, exponent < 0 ? '-' : '+')) {
        return false;
    }
    unsigned magnitude = static_cast<unsigned>(exponent < 0 ? -exponent : exponent);
    char reversed[12];
    unsigned count = 0;
    do {
        reversed[count++] = static_cast<char>('0' + magnitude % 10u);
        magnitude /= 10u;
    } while (magnitude != 0);
    while (count < minimum_digits) reversed[count++] = '0';
    while (count != 0) {
        if (!fp_append(output, capacity, length, reversed[--count])) return false;
    }
    return true;
}

inline bool fp_render_fixed_shortest(const char* digits, unsigned count,
                                     int exponent, char* output,
                                     unsigned capacity,
                                     unsigned& length) noexcept {
    length = 0;
    if (exponent < 0) {
        if (!fp_append(output, capacity, length, '0') ||
            !fp_append(output, capacity, length, '.')) return false;
        for (int zero = -1; zero > exponent; --zero) {
            if (!fp_append(output, capacity, length, '0')) return false;
        }
        for (unsigned i = 0; i < count; ++i) {
            if (!fp_append(output, capacity, length, digits[i])) return false;
        }
        return true;
    }
    unsigned integer_digits = static_cast<unsigned>(exponent) + 1u;
    for (unsigned i = 0; i < integer_digits; ++i) {
        char digit = i < count ? digits[i] : '0';
        if (!fp_append(output, capacity, length, digit)) return false;
    }
    if (count > integer_digits) {
        if (!fp_append(output, capacity, length, '.')) return false;
        for (unsigned i = integer_digits; i < count; ++i) {
            if (!fp_append(output, capacity, length, digits[i])) return false;
        }
    }
    return true;
}

inline bool fp_render_fixed_precision(const char* digits, unsigned count,
                                      int exponent, unsigned precision,
                                      char* output, unsigned capacity,
                                      unsigned& length) noexcept {
    length = 0;
    if (exponent < 0) {
        if (!fp_append(output, capacity, length, '0')) return false;
    } else {
        for (int position = exponent; position >= 0; --position) {
            int index = exponent - position;
            char digit = index >= 0 && static_cast<unsigned>(index) < count
                ? digits[index] : '0';
            if (!fp_append(output, capacity, length, digit)) return false;
        }
    }
    if (precision != 0) {
        if (!fp_append(output, capacity, length, '.')) return false;
        for (unsigned place = 1; place <= precision; ++place) {
            int index = exponent + static_cast<int>(place);
            char digit = index >= 0 && static_cast<unsigned>(index) < count
                ? digits[index] : '0';
            if (!fp_append(output, capacity, length, digit)) return false;
        }
    }
    return true;
}

inline bool fp_render_scientific(const char* digits, unsigned count,
                                 int exponent, char* output,
                                 unsigned capacity,
                                 unsigned& length) noexcept {
    length = 0;
    if (!fp_append(output, capacity, length, digits[0])) return false;
    if (count > 1u) {
        if (!fp_append(output, capacity, length, '.')) return false;
        for (unsigned i = 1; i < count; ++i) {
            if (!fp_append(output, capacity, length, digits[i])) return false;
        }
    }
    return fp_append_exponent(output, capacity, length, exponent, 'e', 2u);
}

inline bool fp_format_decimal_shortest(const fp_binary& binary,
                                       chars_format format,
                                       bool shortest_style, char* output,
                                       unsigned capacity,
                                       unsigned& length) noexcept {
    char digits[fp_digit_capacity];
    unsigned count;
    int exponent;
    if (!fp_shortest_digits(binary, digits, count, exponent)) return false;
    if (format == chars_format::fixed) {
        unsigned integer_digits = exponent >= 0
            ? static_cast<unsigned>(exponent) + 1u : 0u;
        if (count < integer_digits) {
            count = integer_digits;
            if (!fp_significant_digits(binary, count, digits, exponent)) {
                return false;
            }
        }
        return fp_render_fixed_shortest(digits, count, exponent, output,
                                        capacity, length);
    }
    if (format == chars_format::scientific) {
        return fp_render_scientific(digits, count, exponent, output,
                                    capacity, length);
    }
    if (!shortest_style) {
        if (exponent < -4 || exponent >= 6) {
            return fp_render_scientific(digits, count, exponent, output,
                                        capacity, length);
        }
        unsigned integer_digits = exponent >= 0
            ? static_cast<unsigned>(exponent) + 1u : 0u;
        if (count < integer_digits) {
            count = integer_digits;
            if (!fp_significant_digits(binary, count, digits, exponent)) {
                return false;
            }
        }
        return fp_render_fixed_shortest(digits, count, exponent, output,
                                        capacity, length);
    }
    char alternative[fp_text_capacity];
    unsigned alternative_length;
    if (!fp_render_scientific(digits, count, exponent, output, capacity,
                              length)) {
        return false;
    }
    char fixed_digits[fp_digit_capacity];
    for (unsigned i = 0; i < count; ++i) fixed_digits[i] = digits[i];
    unsigned fixed_count = count;
    int fixed_exponent = exponent;
    unsigned integer_digits = fixed_exponent >= 0
        ? static_cast<unsigned>(fixed_exponent) + 1u : 0u;
    if (fixed_count < integer_digits) {
        fixed_count = integer_digits;
        if (!fp_significant_digits(binary, fixed_count, fixed_digits,
                                   fixed_exponent)) return false;
    }
    if (!fp_render_fixed_shortest(fixed_digits, fixed_count, fixed_exponent,
                                  alternative, sizeof(alternative),
                                  alternative_length)) return false;
    if (alternative_length <= length) {
        if (alternative_length > capacity) return false;
        for (unsigned i = 0; i < alternative_length; ++i) {
            output[i] = alternative[i];
        }
        length = alternative_length;
    }
    return true;
}

inline bool fp_format_decimal_precision(const fp_binary& binary,
                                        chars_format format,
                                        unsigned precision, char* output,
                                        unsigned capacity,
                                        unsigned& length) noexcept {
    if (precision > binary.staged_precision) return false;
    char digits[fp_digit_capacity];
    int exponent = 0;
    unsigned count;
    if (format == chars_format::scientific) {
        count = precision + 1u;
        if (!fp_significant_digits(binary, count, digits, exponent)) return false;
        return fp_render_scientific(digits, count, exponent, output,
                                    capacity, length);
    }
    if (format == chars_format::general) {
        unsigned significant = precision == 0 ? 1u : precision;
        count = significant;
        if (!fp_significant_digits(binary, count, digits, exponent)) return false;
        while (count > 1u && digits[count - 1] == '0') --count;
        if (exponent < -4 || exponent >= static_cast<int>(significant)) {
            return fp_render_scientific(digits, count, exponent, output,
                                        capacity, length);
        }
        return fp_render_fixed_shortest(digits, count, exponent, output,
                                        capacity, length);
    }

    fp_big_uint remainder, divisor, lower_margin, upper_margin;
    if (!fp_decimal_ratio(binary, remainder, divisor, lower_margin,
                          upper_margin) ||
        !fp_decimal_normalize(remainder, divisor, lower_margin, upper_margin,
                              exponent)) return false;
    int requested = exponent + 1 + static_cast<int>(precision);
    if (requested <= 0) {
        bool round_up = false;
        if (requested == 0) {
            fp_big_uint five = divisor;
            if (!fp_big_multiply_small(five, 5u)) return false;
            round_up = fp_big_compare(remainder, five) > 0;
        }
        digits[0] = round_up ? '1' : '0';
        count = 1;
        exponent = round_up ? -static_cast<int>(precision) : 0;
    } else {
        count = static_cast<unsigned>(requested);
        if (!fp_significant_digits(binary, count, digits, exponent)) return false;
    }
    return fp_render_fixed_precision(digits, count, exponent, precision,
                                     output, capacity, length);
}

inline char fp_hex_digit(unsigned digit) noexcept {
    return "0123456789abcdef"[digit & 0xfu];
}

inline bool fp_format_hex(const fp_binary& binary, bool has_precision,
                          unsigned precision, char* output,
                          unsigned capacity, unsigned& length) noexcept {
    if (has_precision && precision > binary.staged_precision) return false;
    length = 0;
    unsigned exact_digits = binary.exact_hex_digits;
    unsigned padding = exact_digits * 4u - binary.fraction_bits;
    uint64_t aligned_fraction = binary.fraction << padding;
    unsigned emitted = exact_digits;
    uint64_t kept = 0;
    unsigned leading = binary.explicit_hex_leading ? binary.hex_leading :
        binary.significand == 0 ? 0u :
        (binary.fraction == binary.significand ? 0u : 1u);
    int exponent = binary.zero ? 0 : binary.hex_exponent;

    if (binary.explicit_hex_leading && !has_precision && leading == 0u &&
        binary.significand != 0u) {
        unsigned highest = 63u - static_cast<unsigned>(
            __builtin_clzll(binary.significand));
        unsigned alignment = 3u - highest % 4u;
        uint64_t aligned = binary.significand << alignment;
        exact_digits = highest / 4u;
        unsigned fraction_bits = exact_digits * 4u;
        uint64_t mask = fraction_bits == 0u ? 0u :
            (uint64_t{1} << fraction_bits) - 1u;
        aligned_fraction = aligned & mask;
        emitted = exact_digits;
        leading = static_cast<unsigned>(aligned >> fraction_bits);
        exponent = binary.exponent + static_cast<int>(highest) - 3;
    } else if (!binary.explicit_hex_leading && !has_precision &&
               leading == 0u && binary.fraction != 0u) {
        unsigned highest = 0;
        uint64_t scan = binary.fraction;
        while ((scan >>= 1u) != 0u) ++highest;
        exponent = binary.exponent + static_cast<int>(highest);
        uint64_t lower = binary.fraction - (uint64_t{1} << highest);
        exact_digits = (highest + 3u) / 4u;
        padding = exact_digits * 4u - highest;
        aligned_fraction = lower << padding;
        emitted = exact_digits;
        leading = 1u;
    }

    if (has_precision && precision < exact_digits) {
        unsigned shift = binary.fraction_bits - precision * 4u;
        kept = binary.significand >> shift;
        uint64_t mask = (uint64_t{1} << shift) - 1u;
        uint64_t discarded = binary.significand & mask;
        uint64_t half = uint64_t{1} << (shift - 1u);
        if (discarded > half || (discarded == half && (kept & 1u))) ++kept;
        leading = static_cast<unsigned>(kept >> (precision * 4u));
        if (leading > 0xfu) {
            leading >>= 4u;
            exponent += 4;
        }
        emitted = precision;
    } else if (!has_precision) {
        while (emitted != 0 &&
               ((aligned_fraction >> ((exact_digits - emitted) * 4u)) & 0xfu) == 0) {
            --emitted;
        }
    } else {
        emitted = precision;
    }

    if (!fp_append(output, capacity, length, fp_hex_digit(leading))) return false;
    if (emitted != 0) {
        if (!fp_append(output, capacity, length, '.')) return false;
        for (unsigned i = 0; i < emitted; ++i) {
            unsigned digit = 0;
            if (has_precision && precision < exact_digits) {
                unsigned shift = (emitted - i - 1u) * 4u;
                digit = static_cast<unsigned>((kept >> shift) & 0xfu);
            } else if (i < exact_digits) {
                unsigned shift = (exact_digits - i - 1u) * 4u;
                digit = static_cast<unsigned>((aligned_fraction >> shift) & 0xfu);
            }
            if (!fp_append(output, capacity, length, fp_hex_digit(digit))) return false;
        }
    }
    return fp_append_exponent(output, capacity, length, exponent, 'p', 1u);
}

inline bool fp_copy_literal(char* output, unsigned capacity, unsigned& length,
                            const char* literal) noexcept {
    length = 0;
    while (*literal != '\0') {
        if (!fp_append(output, capacity, length, *literal++)) return false;
    }
    return true;
}

inline to_chars_result fp_publish(char* first, char* last,
                                  const char* candidate,
                                  unsigned length) noexcept {
    if (static_cast<size_t>(last - first) < length) {
        return {last, errc::value_too_large};
    }
    for (unsigned i = 0; i < length; ++i) first[i] = candidate[i];
    return {first + length, errc{}};
}

inline to_chars_result fp_publish_extended_precision(
    char* first, char* last, const char* candidate, unsigned length,
    chars_format format, unsigned precision,
    unsigned staged_precision) noexcept {
    /* Once the representation-specific staging boundary is reached, any
     * further fixed, scientific, or hexadecimal digits are zero; general
     * removes those zeroes. */
    unsigned extra = precision - staged_precision;
    unsigned insertion = length;
    if (format == chars_format::scientific || format == chars_format::hex) {
        char marker = format == chars_format::hex ? 'p' : 'e';
        while (insertion != 0u && candidate[insertion - 1u] != marker) {
            --insertion;
        }
        if (insertion == 0u) return {first, errc::invalid_argument};
        --insertion;
    } else if (format == chars_format::general) {
        extra = 0u;
    }
    size_t total = static_cast<size_t>(length) + extra;
    if (static_cast<size_t>(last - first) < total) {
        return {last, errc::value_too_large};
    }
    unsigned published = 0;
    while (published < insertion) {
        first[published] = candidate[published];
        ++published;
    }
    for (unsigned zero = 0; zero < extra; ++zero) {
        first[published++] = '0';
    }
    for (unsigned source = insertion; source < length; ++source) {
        first[published++] = candidate[source];
    }
    return {first + total, errc{}};
}

inline bool fp_valid_format(chars_format format) noexcept {
    return format == chars_format::scientific || format == chars_format::fixed ||
           format == chars_format::hex || format == chars_format::general;
}

inline to_chars_result fp_to_chars(char* first, char* last,
                                   const fp_binary& binary,
                                   chars_format format, bool has_precision,
                                   int precision,
                                   bool shortest_style = false) noexcept {
    if (!fp_valid_format(format) || (has_precision && precision < 0)) {
        return {first, errc::invalid_argument};
    }
    char candidate[fp_text_capacity];
    unsigned length = 0;
    if (binary.nan) {
        if (!fp_copy_literal(candidate, sizeof(candidate), length, "nan")) {
            return {last, errc::value_too_large};
        }
        return fp_publish(first, last, candidate, length);
    }
    if (binary.negative && !fp_append(candidate, sizeof(candidate), length, '-')) {
        return {last, errc::value_too_large};
    }
    if (binary.infinity) {
        unsigned literal_length;
        if (!fp_copy_literal(candidate + length,
                             sizeof(candidate) - length, literal_length,
                             "inf")) return {last, errc::value_too_large};
        length += literal_length;
        return fp_publish(first, last, candidate, length);
    }

    fp_binary magnitude = binary;
    magnitude.negative = false;
    unsigned requested_precision = has_precision
        ? static_cast<unsigned>(precision) : 0u;
    unsigned bounded_precision = requested_precision > binary.staged_precision
        ? binary.staged_precision : requested_precision;
    char* body = candidate + length;
    unsigned body_capacity = sizeof(candidate) - length;
    unsigned body_length = 0;
    bool formatted;
    if (magnitude.zero) {
        if (format == chars_format::hex) {
            formatted = fp_format_hex(magnitude, has_precision,
                                      bounded_precision,
                                      body, body_capacity, body_length);
        } else if (has_precision && format == chars_format::scientific) {
            char digits[fp_binary80_staged_precision + 1u] = {'0'};
            unsigned count = bounded_precision + 1u;
            for (unsigned i = 0; i < count; ++i) digits[i] = '0';
            formatted = fp_render_scientific(digits, count, 0, body,
                                              body_capacity, body_length);
        } else if (has_precision && format == chars_format::fixed) {
            char zero = '0';
            formatted = fp_render_fixed_precision(&zero, 1u, 0,
                                                   bounded_precision, body,
                                                   body_capacity, body_length);
        } else {
            formatted = fp_copy_literal(body, body_capacity, body_length, "0");
        }
    } else if (format == chars_format::hex) {
        formatted = fp_format_hex(magnitude, has_precision,
                                  bounded_precision,
                                  body, body_capacity, body_length);
    } else if (has_precision) {
        formatted = fp_format_decimal_precision(
            magnitude, format, bounded_precision, body,
            body_capacity, body_length);
    } else {
        formatted = fp_format_decimal_shortest(magnitude, format,
                                               shortest_style, body,
                                               body_capacity, body_length);
    }
    if (!formatted) return {last, errc::value_too_large};
    length += body_length;
    if (has_precision && requested_precision > binary.staged_precision) {
        return fp_publish_extended_precision(first, last, candidate, length,
                                              format, requested_precision,
                                              binary.staged_precision);
    }
    return fp_publish(first, last, candidate, length);
}

inline int fp_hex_digit(char value) noexcept {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

inline bool fp_ascii_case_equal(char value, char lower) noexcept {
    return value == lower || value == static_cast<char>(lower - ('a' - 'A'));
}

inline bool fp_match_word(const char* first, const char* last,
                          const char* word, const char*& end) noexcept {
    const char* cursor = first;
    while (*word != '\0') {
        if (cursor == last || !fp_ascii_case_equal(*cursor, *word++)) return false;
        ++cursor;
    }
    end = cursor;
    return true;
}

inline bool fp_round_right(uint64_t source, unsigned shift, bool sticky,
                           uint64_t& rounded) noexcept {
    if (shift == 0u) { rounded = source; return true; }
    if (shift > 64u) { rounded = 0; return true; }
    if (shift == 64u) {
        uint64_t halfway = uint64_t{1} << 63;
        rounded = source > halfway || (source == halfway && sticky) ? 1u : 0u;
        return true;
    }
    uint64_t quotient = source >> shift;
    uint64_t remainder = source & ((uint64_t{1} << shift) - 1u);
    uint64_t halfway = uint64_t{1} << (shift - 1u);
    if (remainder > halfway ||
        (remainder == halfway && (sticky || (quotient & 1u) != 0u))) {
        ++quotient;
    }
    rounded = quotient;
    return true;
}

template<typename T> struct fp_parse_traits;

template<> struct fp_parse_traits<float> {
    using bits_type = uint32_t;
    static constexpr unsigned fraction_bits = 23u;
    static constexpr int exponent_bias = 127;
    static constexpr int minimum_normal = -126;
    static constexpr int minimum_subnormal = -149;
    static constexpr int maximum_exponent = 127;
    static constexpr bits_type sign = 0x80000000u;
    static constexpr bits_type infinity = 0x7f800000u;
    static constexpr bits_type nan = 0x7fc00001u;
};

template<> struct fp_parse_traits<double> {
    using bits_type = uint64_t;
    static constexpr unsigned fraction_bits = 52u;
    static constexpr int exponent_bias = 1023;
    static constexpr int minimum_normal = -1022;
    static constexpr int minimum_subnormal = -1074;
    static constexpr int maximum_exponent = 1023;
    static constexpr bits_type sign = uint64_t{1} << 63;
    static constexpr bits_type infinity = uint64_t{0x7ff} << 52;
    static constexpr bits_type nan = (uint64_t{0x7ff} << 52) |
                                     (uint64_t{1} << 51) | 1u;
};

template<typename T>
inline T fp_value_from_bits(typename fp_parse_traits<T>::bits_type bits) noexcept {
    union { typename fp_parse_traits<T>::bits_type bits; T value; } converted{};
    converted.bits = bits;
    return converted.value;
}

template<typename T>
inline T fp_zero_value(bool negative) noexcept {
    using traits = fp_parse_traits<T>;
    return fp_value_from_bits<T>(negative ? traits::sign : 0);
}

template<typename T>
inline T fp_infinity_value(bool negative) noexcept {
    using traits = fp_parse_traits<T>;
    return fp_value_from_bits<T>(traits::infinity |
                                 (negative ? traits::sign : 0));
}

template<typename T>
inline T fp_nan_value(bool negative) noexcept {
    using traits = fp_parse_traits<T>;
    return fp_value_from_bits<T>(traits::nan | (negative ? traits::sign : 0));
}

#if defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
    __LDBL_MANT_DIG__ == 64 && __LDBL_MAX_EXP__ == 16384 && \
    defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
    defined(__ORDER_BIG_ENDIAN__) && \
    (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ || \
     __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
template<> inline long double fp_zero_value<long double>(bool negative) noexcept {
    return fp_binary80_value(0u, 0u, negative);
}

template<> inline long double fp_infinity_value<long double>(bool negative) noexcept {
    return fp_binary80_value(0x8000000000000000u, 0x7fffu, negative);
}

template<> inline long double fp_nan_value<long double>(bool negative) noexcept {
    return fp_binary80_value(0xc000000000000001u, 0x7fffu, negative);
}
#elif defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
      __LDBL_MANT_DIG__ == 53 && __LDBL_MAX_EXP__ == 1024
template<> inline long double fp_zero_value<long double>(bool negative) noexcept {
    return static_cast<long double>(fp_zero_value<double>(negative));
}

template<> inline long double fp_infinity_value<long double>(bool negative) noexcept {
    return static_cast<long double>(fp_infinity_value<double>(negative));
}

template<> inline long double fp_nan_value<long double>(bool negative) noexcept {
    return static_cast<long double>(fp_nan_value<double>(negative));
}
#endif

template<typename T>
inline bool fp_binary_to_value(uint64_t significand, int scale,
                               bool round_bit, bool sticky, bool negative,
                               T& output) noexcept {
    using traits = fp_parse_traits<T>;
    using bits_type = typename traits::bits_type;
    if (significand == 0u) {
        output = fp_value_from_bits<T>(negative ? traits::sign : bits_type{0});
        return true;
    }
    unsigned highest = 63u - static_cast<unsigned>(__builtin_clzll(significand));
    long long exponent_wide = static_cast<long long>(scale) + highest;
    if (exponent_wide > traits::maximum_exponent) return false;
    uint64_t rounded = 0;
    if (exponent_wide >= traits::minimum_normal) {
        int shift = static_cast<int>(highest) -
                    static_cast<int>(traits::fraction_bits);
        if (shift > 0) {
            fp_round_right(significand, static_cast<unsigned>(shift),
                           round_bit || sticky, rounded);
        } else {
            rounded = significand << static_cast<unsigned>(-shift);
        }
        uint64_t precision_limit = uint64_t{1} << (traits::fraction_bits + 1u);
        if (rounded == precision_limit) {
            rounded >>= 1;
            if (++exponent_wide > traits::maximum_exponent) return false;
        }
        bits_type exponent_bits = static_cast<bits_type>(
            exponent_wide + traits::exponent_bias);
        bits_type fraction_mask = static_cast<bits_type>(
            (uint64_t{1} << traits::fraction_bits) - 1u);
        bits_type bits = static_cast<bits_type>(rounded) & fraction_mask;
        bits |= exponent_bits << traits::fraction_bits;
        if (negative) bits |= traits::sign;
        output = fp_value_from_bits<T>(bits);
        return true;
    }
    long long shift_wide = static_cast<long long>(traits::minimum_subnormal) -
                           static_cast<long long>(scale);
    if (shift_wide <= 0) rounded = significand << static_cast<unsigned>(-shift_wide);
    else fp_round_right(significand,
                        shift_wide > 65 ? 65u : static_cast<unsigned>(shift_wide),
                        round_bit || sticky, rounded);
    if (rounded == 0u) return false;
    uint64_t normal_threshold = uint64_t{1} << traits::fraction_bits;
    bits_type bits;
    if (rounded >= normal_threshold) {
        bits = static_cast<bits_type>(1) << traits::fraction_bits;
    } else {
        bits = static_cast<bits_type>(rounded);
    }
    if (negative) bits |= traits::sign;
    output = fp_value_from_bits<T>(bits);
    return true;
}

#if defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
    __LDBL_MANT_DIG__ == 64 && __LDBL_MAX_EXP__ == 16384 && \
    defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
    defined(__ORDER_BIG_ENDIAN__) && \
    (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ || \
     __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
inline bool fp_binary_to_value(uint64_t significand, int scale,
                               bool round_bit, bool sticky, bool negative,
                               long double& output) noexcept {
    if (significand == 0u) {
        output = fp_zero_value<long double>(negative);
        return true;
    }
    unsigned highest = 63u - static_cast<unsigned>(__builtin_clzll(significand));
    long long exponent_wide = static_cast<long long>(scale) + highest;
    if (exponent_wide > 16383) return false;
    uint64_t rounded = 0;
    if (exponent_wide >= -16382) {
        int shift = static_cast<int>(highest) - 63;
        if (shift > 0) {
            fp_round_right(significand, static_cast<unsigned>(shift),
                           round_bit || sticky, rounded);
        } else {
            rounded = significand << static_cast<unsigned>(-shift);
            if (shift == 0 && round_bit &&
                (sticky || (rounded & 1u) != 0u)) {
                if (rounded == ~uint64_t{0}) {
                    rounded = uint64_t{1} << 63u;
                    if (++exponent_wide > 16383) return false;
                } else {
                    ++rounded;
                }
            }
        }
        uint16_t encoded_exponent = static_cast<uint16_t>(exponent_wide + 16383);
        output = fp_binary80_value(rounded, encoded_exponent, negative);
        return true;
    }
    long long shift_wide = -16445LL - static_cast<long long>(scale);
    if (shift_wide <= 0) {
        rounded = significand << static_cast<unsigned>(-shift_wide);
    } else {
        fp_round_right(significand,
                       shift_wide > 65 ? 65u : static_cast<unsigned>(shift_wide),
                       round_bit || sticky, rounded);
    }
    if (rounded == 0u) return false;
    if (rounded >= (uint64_t{1} << 63u)) {
        output = fp_binary80_value(uint64_t{1} << 63u, 1u, negative);
    } else {
        output = fp_binary80_value(rounded, 0u, negative);
    }
    return true;
}
#elif defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
      __LDBL_MANT_DIG__ == 53 && __LDBL_MAX_EXP__ == 1024
inline bool fp_binary_to_value(uint64_t significand, int scale,
                               bool round_bit, bool sticky, bool negative,
                               long double& output) noexcept {
    double converted;
    if (!fp_binary_to_value(significand, scale, round_bit, sticky, negative,
                            converted)) return false;
    output = static_cast<long double>(converted);
    return true;
}
#endif

template<typename T>
inline from_chars_result fp_from_chars_hex(const char* first, const char* last,
                                           T& value,
                                           int* range_direction = nullptr) noexcept {
    if (range_direction) *range_direction = 0;
    const char* cursor = first;
    bool negative = false;
    if (cursor != last && *cursor == '-') { negative = true; ++cursor; }
    else if (cursor != last && *cursor == '+') return {first, errc::invalid_argument};

    const char* special_end = cursor;
    if (fp_match_word(cursor, last, "inf", special_end)) {
        const char* infinity_end;
        if (fp_match_word(special_end, last, "inity", infinity_end))
            special_end = infinity_end;
        value = fp_infinity_value<T>(negative);
        return {special_end, errc{}};
    }
    if (fp_match_word(cursor, last, "nan", special_end)) {
        if (special_end != last && *special_end == '(') {
            const char* payload = special_end + 1;
            const char* scan = payload;
            while (scan != last && ((*scan >= '0' && *scan <= '9') ||
                   (*scan >= 'a' && *scan <= 'z') ||
                   (*scan >= 'A' && *scan <= 'Z') || *scan == '_')) ++scan;
            if (scan != last && *scan == ')') special_end = scan + 1;
        }
        value = fp_nan_value<T>(negative);
        return {special_end, errc{}};
    }

    uint64_t significand = 0;
    unsigned kept_bits = 0;
    long long total_bits = 0;
    long long fractional_nibbles = 0;
    bool any_digit = false;
    bool seen_point = false;
    bool seen_nonzero = false;
    bool round_bit = false;
    bool sticky = false;
    bool tail_started = false;
    for (; cursor != last; ++cursor) {
        if (*cursor == '.' && !seen_point) { seen_point = true; continue; }
        int digit = fp_hex_digit(*cursor);
        if (digit < 0) break;
        any_digit = true;
        if (seen_point) ++fractional_nibbles;
        if (!seen_nonzero && digit == 0) continue;
        unsigned digit_bits = 4u;
        if (!seen_nonzero) {
            seen_nonzero = true;
            digit_bits = digit >= 8 ? 4u : digit >= 4 ? 3u :
                         digit >= 2 ? 2u : 1u;
        }
        total_bits += digit_bits;
        unsigned available = 64u - kept_bits;
        if (digit_bits <= available) {
            significand = (significand << digit_bits) |
                          static_cast<unsigned>(digit);
            kept_bits += digit_bits;
        } else if (available != 0u) {
            unsigned discarded = digit_bits - available;
            significand = (significand << available) |
                (static_cast<unsigned>(digit) >> discarded);
            kept_bits = 64u;
            unsigned tail = static_cast<unsigned>(digit) &
                ((1u << discarded) - 1u);
            round_bit = (tail & (1u << (discarded - 1u))) != 0u;
            sticky = (tail & ((1u << (discarded - 1u)) - 1u)) != 0u;
            tail_started = true;
        } else if (!tail_started) {
            round_bit = (static_cast<unsigned>(digit) & 8u) != 0u;
            sticky = (static_cast<unsigned>(digit) & 7u) != 0u;
            tail_started = true;
        } else if (digit != 0) {
            sticky = true;
        }
    }
    if (!any_digit) return {first, errc::invalid_argument};
    long long dropped_bits = total_bits - static_cast<long long>(kept_bits);
    long long exponent = 0;
    const char* exponent_marker = cursor;
    if (cursor != last && (*cursor == 'p' || *cursor == 'P')) {
        const char* exponent_cursor = cursor + 1;
        bool exponent_negative = false;
        if (exponent_cursor != last && (*exponent_cursor == '+' ||
            *exponent_cursor == '-')) {
            exponent_negative = *exponent_cursor == '-';
            ++exponent_cursor;
        }
        const char* digits_begin = exponent_cursor;
        while (exponent_cursor != last && *exponent_cursor >= '0' &&
               *exponent_cursor <= '9') {
            if (exponent < 100000) exponent = exponent * 10 + (*exponent_cursor - '0');
            ++exponent_cursor;
        }
        if (exponent_cursor != digits_begin) {
            cursor = exponent_cursor;
            if (exponent_negative) exponent = -exponent;
        } else cursor = exponent_marker;
    }
    if (!seen_nonzero) {
        value = fp_zero_value<T>(negative);
        return {cursor, errc{}};
    }
    long long scale_wide = exponent - fractional_nibbles * 4 + dropped_bits;
    if (scale_wide < -100000 || scale_wide > 100000) {
        if (range_direction) *range_direction = scale_wide < 0 ? -1 : 1;
        return {cursor, errc::result_out_of_range};
    }
    T converted;
    if (!fp_binary_to_value(significand, static_cast<int>(scale_wide),
                            round_bit, sticky, negative, converted)) {
        if (range_direction) *range_direction = scale_wide < 0 ? -1 : 1;
        return {cursor, errc::result_out_of_range};
    }
    value = converted;
    return {cursor, errc{}};
}

template<typename T>
inline void fp_finite_components(typename fp_parse_traits<T>::bits_type bits,
                                 uint64_t& significand,
                                 int& exponent) noexcept {
    using traits = fp_parse_traits<T>;
    using bits_type = typename traits::bits_type;
    bits_type fraction_mask = static_cast<bits_type>(
        (uint64_t{1} << traits::fraction_bits) - 1u);
    bits_type fraction = bits & fraction_mask;
    bits_type encoded_exponent = (bits >> traits::fraction_bits) &
        static_cast<bits_type>((uint64_t{1} <<
            (sizeof(bits_type) == 4 ? 8u : 11u)) - 1u);
    if (encoded_exponent == 0) {
        significand = fraction;
        exponent = traits::minimum_subnormal;
    } else {
        significand = (uint64_t{1} << traits::fraction_bits) | fraction;
        exponent = static_cast<int>(encoded_exponent) - traits::exponent_bias -
                   static_cast<int>(traits::fraction_bits);
    }
}

inline int fp_compare_decimal_big_binary(const fp_big_uint& decimal,
                                         int decimal_exponent, bool sticky,
                                         fp_big_uint right,
                                         int binary_exponent,
                                         bool& okay) noexcept {
    if (right.size == 0u) return decimal.size == 0u ? 0 : 1;
    fp_big_uint left = decimal;
    int left_exponent = 0;
    int right_exponent = binary_exponent;
    if (decimal_exponent >= 0) {
        okay = fp_big_multiply_power5(left,
                                      static_cast<unsigned>(decimal_exponent));
        left_exponent = decimal_exponent;
    } else {
        unsigned power = static_cast<unsigned>(-decimal_exponent);
        okay = fp_big_multiply_power5(right, power);
        right_exponent += static_cast<int>(power);
    }
    if (!okay) return 0;
    if (left_exponent > right_exponent) {
        okay = fp_big_shift_left(left,
            static_cast<unsigned>(left_exponent - right_exponent));
    } else if (right_exponent > left_exponent) {
        okay = fp_big_shift_left(right,
            static_cast<unsigned>(right_exponent - left_exponent));
    }
    if (!okay) return 0;
    int comparison = fp_big_compare(left, right);
    return comparison == 0 && sticky ? 1 : comparison;
}

inline int fp_compare_decimal_binary(const fp_big_uint& decimal,
                                     int decimal_exponent, bool sticky,
                                     uint64_t binary_significand,
                                     int binary_exponent,
                                     bool& okay) noexcept {
    fp_big_uint right;
    fp_big_set(right, binary_significand);
    return fp_compare_decimal_big_binary(decimal, decimal_exponent, sticky,
                                         right, binary_exponent, okay);
}

inline int fp_compare_decimal_midpoint(const fp_big_uint& decimal,
                                       int decimal_exponent, bool sticky,
                                       uint64_t lower_significand,
                                       int lower_exponent,
                                       uint64_t upper_significand,
                                       int upper_exponent,
                                       bool& okay) noexcept {
    int common_exponent = lower_exponent < upper_exponent
        ? lower_exponent : upper_exponent;
    fp_big_uint boundary, upper;
    fp_big_set(boundary, lower_significand);
    fp_big_set(upper, upper_significand);
    okay = fp_big_shift_left(boundary,
        static_cast<unsigned>(lower_exponent - common_exponent)) &&
        fp_big_shift_left(upper,
        static_cast<unsigned>(upper_exponent - common_exponent)) &&
        fp_big_add(boundary, upper);
    if (!okay) return 0;
    return fp_compare_decimal_big_binary(decimal, decimal_exponent, sticky,
                                         boundary, common_exponent - 1,
                                         okay);
}

template<typename T>
inline bool fp_decimal_to_value(const fp_big_uint& decimal,
                                int decimal_exponent, bool sticky,
                                bool negative, T& output) noexcept {
    using traits = fp_parse_traits<T>;
    using bits_type = typename traits::bits_type;
    constexpr bits_type maximum_finite = sizeof(bits_type) == 4
        ? static_cast<bits_type>(0x7f7fffffu)
        : static_cast<bits_type>(uint64_t{0x7fefffffffffffff});
    bits_type low = 0;
    bits_type high = maximum_finite;
    bool okay = true;
    while (low < high) {
        bits_type middle = low + static_cast<bits_type>((high - low + 1) / 2);
        uint64_t significand;
        int exponent;
        fp_finite_components<T>(middle, significand, exponent);
        int comparison = fp_compare_decimal_binary(
            decimal, decimal_exponent, sticky, significand, exponent, okay);
        if (!okay) return false;
        if (comparison >= 0) low = middle;
        else high = middle - 1;
    }
    uint64_t lower_significand;
    int lower_exponent;
    fp_finite_components<T>(low, lower_significand, lower_exponent);
    int exact = fp_compare_decimal_binary(decimal, decimal_exponent, sticky,
                                          lower_significand, lower_exponent,
                                          okay);
    if (!okay) return false;
    bits_type selected = low;
    if (exact != 0) {
        uint64_t boundary_significand;
        int boundary_exponent;
        if (low == maximum_finite) {
            boundary_significand = lower_significand * 2u + 1u;
            boundary_exponent = lower_exponent - 1;
            int boundary = fp_compare_decimal_binary(
                decimal, decimal_exponent, sticky, boundary_significand,
                boundary_exponent, okay);
            if (!okay || boundary >= 0) return false;
        } else {
            uint64_t upper_significand;
            int upper_exponent;
            fp_finite_components<T>(static_cast<bits_type>(low + 1u),
                                    upper_significand, upper_exponent);
            int common_exponent = lower_exponent < upper_exponent
                ? lower_exponent : upper_exponent;
            uint64_t lower_scaled = lower_significand <<
                static_cast<unsigned>(lower_exponent - common_exponent);
            uint64_t upper_scaled = upper_significand <<
                static_cast<unsigned>(upper_exponent - common_exponent);
            boundary_significand = lower_scaled + upper_scaled;
            boundary_exponent = common_exponent - 1;
            int boundary = fp_compare_decimal_binary(
                decimal, decimal_exponent, sticky, boundary_significand,
                boundary_exponent, okay);
            if (!okay) return false;
            if (boundary > 0 || (boundary == 0 && (low & 1u) != 0u)) {
                selected = static_cast<bits_type>(low + 1u);
            }
        }
    }
    if (selected == 0) return false;
    if (negative) selected |= traits::sign;
    output = fp_value_from_bits<T>(selected);
    return true;
}

#if defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
    __LDBL_MANT_DIG__ == 64 && __LDBL_MAX_EXP__ == 16384 && \
    defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
    defined(__ORDER_BIG_ENDIAN__) && \
    (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ || \
     __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
inline bool fp_decimal_to_value(const fp_big_uint& decimal,
                                int decimal_exponent, bool sticky,
                                bool negative, long double& output) noexcept {
    bool okay = true;
    uint16_t selected_exponent = 0u;
    int minimum_comparison = fp_compare_decimal_binary(
        decimal, decimal_exponent, sticky, uint64_t{1} << 63u, -16445,
        okay);
    if (!okay) return false;
    if (minimum_comparison >= 0) {
        unsigned low_exponent = 1u;
        unsigned high_exponent = 0x7ffeu;
        while (low_exponent < high_exponent) {
            unsigned distance = high_exponent - low_exponent;
            unsigned middle = low_exponent + distance / 2u + (distance & 1u);
            int exponent = static_cast<int>(middle) - 16383 - 63;
            int comparison = fp_compare_decimal_binary(
                decimal, decimal_exponent, sticky, uint64_t{1} << 63u,
                exponent, okay);
            if (!okay) return false;
            if (comparison >= 0) low_exponent = middle;
            else high_exponent = middle - 1u;
        }
        selected_exponent = static_cast<uint16_t>(low_exponent);
    }

    uint64_t low = selected_exponent == 0u ? 0u : uint64_t{1} << 63u;
    uint64_t high = selected_exponent == 0u
        ? (uint64_t{1} << 63u) - 1u : ~uint64_t{0};
    int candidate_exponent = selected_exponent == 0u
        ? -16445 : static_cast<int>(selected_exponent) - 16383 - 63;
    while (low < high) {
        uint64_t distance = high - low;
        uint64_t middle = low + distance / 2u + (distance & 1u);
        int comparison = fp_compare_decimal_binary(
            decimal, decimal_exponent, sticky, middle, candidate_exponent,
            okay);
        if (!okay) return false;
        if (comparison >= 0) low = middle;
        else high = middle - 1u;
    }


    int exact = fp_compare_decimal_binary(decimal, decimal_exponent, sticky,
                                          low, candidate_exponent, okay);
    if (!okay) return false;
    uint16_t result_exponent = selected_exponent;
    uint64_t result_significand = low;
    if (exact != 0) {
        uint16_t upper_exponent = selected_exponent;
        uint64_t upper_significand;
        if (selected_exponent == 0u &&
            low == (uint64_t{1} << 63u) - 1u) {
            upper_significand = uint64_t{1} << 63u;
            upper_exponent = 1u;
        } else if (low != ~uint64_t{0}) {
            upper_significand = low + 1u;
        } else {
            upper_significand = uint64_t{1} << 63u;
            ++upper_exponent;
        }
        int upper_binary_exponent = upper_exponent == 0u
            ? -16445 : static_cast<int>(upper_exponent) - 16383 - 63;
        int boundary = fp_compare_decimal_midpoint(
            decimal, decimal_exponent, sticky, low, candidate_exponent,
            upper_significand, upper_binary_exponent, okay);
        if (!okay) return false;
        if (boundary > 0 || (boundary == 0 && (low & 1u) != 0u)) {
            result_exponent = upper_exponent;
            result_significand = upper_significand;
        }
    }
    if (result_significand == 0u || result_exponent == 0x7fffu) return false;
    output = fp_binary80_value(result_significand, result_exponent, negative);
    return true;
}
#elif defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
      __LDBL_MANT_DIG__ == 53 && __LDBL_MAX_EXP__ == 1024
inline bool fp_decimal_to_value(const fp_big_uint& decimal,
                                int decimal_exponent, bool sticky,
                                bool negative, long double& output) noexcept {
    double converted;
    if (!fp_decimal_to_value(decimal, decimal_exponent, sticky, negative,
                             converted)) return false;
    output = static_cast<long double>(converted);
    return true;
}
#endif

template<typename T>
inline bool fp_decimal_range(long long order,
                             long long decimal_exponent) noexcept {
    return order <= 400 && order >= -400 && decimal_exponent <= 2000 &&
           decimal_exponent >= -2000;
}

#if defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
    __LDBL_MANT_DIG__ == 64 && __LDBL_MAX_EXP__ == 16384 && \
    defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
    defined(__ORDER_BIG_ENDIAN__) && \
    (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ || \
     __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
template<> inline bool fp_decimal_range<long double>(
    long long order, long long decimal_exponent) noexcept {
    return order <= 5000 && order >= -5000 && decimal_exponent <= 5100 &&
           decimal_exponent >= -6200;
}
#elif defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
      __LDBL_MANT_DIG__ == 53 && __LDBL_MAX_EXP__ == 1024
template<> inline bool fp_decimal_range<long double>(
    long long order, long long decimal_exponent) noexcept {
    return fp_decimal_range<double>(order, decimal_exponent);
}
#endif

template<typename T>
inline from_chars_result fp_from_chars_decimal(const char* first,
                                               const char* last, T& value,
                                               chars_format format,
                                               int* range_direction = nullptr) noexcept {
    if (range_direction) *range_direction = 0;
    if (format != chars_format::general && format != chars_format::fixed &&
        format != chars_format::scientific) {
        return {first, errc::invalid_argument};
    }
    const char* cursor = first;
    bool negative = false;
    if (cursor != last && *cursor == '-') { negative = true; ++cursor; }
    else if (cursor != last && *cursor == '+') return {first, errc::invalid_argument};

    const char* special_end = cursor;
    if (fp_match_word(cursor, last, "inf", special_end)) {
        const char* infinity_end;
        if (fp_match_word(special_end, last, "inity", infinity_end))
            special_end = infinity_end;
        value = fp_infinity_value<T>(negative);
        return {special_end, errc{}};
    }
    if (fp_match_word(cursor, last, "nan", special_end)) {
        if (special_end != last && *special_end == '(') {
            const char* scan = special_end + 1;
            while (scan != last && ((*scan >= '0' && *scan <= '9') ||
                   (*scan >= 'a' && *scan <= 'z') ||
                   (*scan >= 'A' && *scan <= 'Z') || *scan == '_')) ++scan;
            if (scan != last && *scan == ')') special_end = scan + 1;
        }
        value = fp_nan_value<T>(negative);
        return {special_end, errc{}};
    }

    fp_big_uint decimal;
    fp_big_zero(decimal);
    bool any_digit = false;
    bool significant = false;
    bool sticky = false;
    unsigned kept = 0;
    long long significant_count = 0;
    long long fractional_digits = 0;
    bool point = false;
    for (; cursor != last; ++cursor) {
        if (*cursor == '.' && !point) { point = true; continue; }
        if (*cursor < '0' || *cursor > '9') break;
        any_digit = true;
        if (point) ++fractional_digits;
        unsigned digit = static_cast<unsigned>(*cursor - '0');
        if (!significant && digit == 0u) continue;
        significant = true;
        ++significant_count;
        if (kept < 1100u) {
            if (!fp_big_multiply_small(decimal, 10u) ||
                !fp_big_add_small(decimal, digit)) {
                if (range_direction) *range_direction = 1;
                return {cursor, errc::result_out_of_range};
            }
            ++kept;
        } else if (digit != 0u) sticky = true;
    }
    if (!any_digit) return {first, errc::invalid_argument};

    long long explicit_exponent = 0;
    bool exponent_present = false;
    if (format != chars_format::fixed && cursor != last &&
        (*cursor == 'e' || *cursor == 'E')) {
        const char* marker = cursor;
        const char* scan = cursor + 1;
        bool exponent_negative = false;
        if (scan != last && (*scan == '+' || *scan == '-')) {
            exponent_negative = *scan == '-';
            ++scan;
        }
        const char* exponent_digits = scan;
        while (scan != last && *scan >= '0' && *scan <= '9') {
            if (explicit_exponent < 100000)
                explicit_exponent = explicit_exponent * 10 + (*scan - '0');
            ++scan;
        }
        if (scan != exponent_digits) {
            exponent_present = true;
            cursor = scan;
            if (exponent_negative) explicit_exponent = -explicit_exponent;
        } else cursor = marker;
    }
    if (format == chars_format::scientific && !exponent_present)
        return {first, errc::invalid_argument};
    if (!significant) {
        value = fp_zero_value<T>(negative);
        return {cursor, errc{}};
    }
    long long dropped = significant_count - static_cast<long long>(kept);
    long long decimal_exponent = explicit_exponent - fractional_digits + dropped;
    long long order = significant_count + explicit_exponent - fractional_digits - 1;
    if (!fp_decimal_range<T>(order, decimal_exponent)) {
        if (range_direction) *range_direction = order < 0 ? -1 : 1;
        return {cursor, errc::result_out_of_range};
    }
    T converted;
    if (!fp_decimal_to_value(decimal, static_cast<int>(decimal_exponent),
                             sticky, negative, converted)) {
        if (range_direction) *range_direction = order < 0 ? -1 : 1;
        return {cursor, errc::result_out_of_range};
    }
    value = converted;
    return {cursor, errc{}};
}

} // namespace detail

inline to_chars_result to_chars(char* first, char* last, float value) {
    return detail::fp_to_chars(first, last, detail::fp_decode(value),
                               chars_format::general, false, 0, true);
}

inline to_chars_result to_chars(char* first, char* last, double value) {
    return detail::fp_to_chars(first, last, detail::fp_decode(value),
                               chars_format::general, false, 0, true);
}

#if defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
    ((__LDBL_MANT_DIG__ == 64 && __LDBL_MAX_EXP__ == 16384 && \
      defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
      defined(__ORDER_BIG_ENDIAN__) && \
      (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ || \
       __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)) || \
     (__LDBL_MANT_DIG__ == 53 && __LDBL_MAX_EXP__ == 1024))
inline to_chars_result to_chars(char* first, char* last, long double value) {
    return detail::fp_to_chars(first, last, detail::fp_decode(value),
                               chars_format::general, false, 0, true);
}
#endif

inline to_chars_result to_chars(char* first, char* last, float value,
                                chars_format format) {
    return detail::fp_to_chars(first, last, detail::fp_decode(value), format,
                               false, 0);
}

inline to_chars_result to_chars(char* first, char* last, double value,
                                 chars_format format) {
    return detail::fp_to_chars(first, last, detail::fp_decode(value), format,
                               false, 0);
}

#if defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
    ((__LDBL_MANT_DIG__ == 64 && __LDBL_MAX_EXP__ == 16384 && \
      defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
      defined(__ORDER_BIG_ENDIAN__) && \
      (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ || \
       __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)) || \
     (__LDBL_MANT_DIG__ == 53 && __LDBL_MAX_EXP__ == 1024))
inline to_chars_result to_chars(char* first, char* last, long double value,
                                chars_format format) {
    return detail::fp_to_chars(first, last, detail::fp_decode(value), format,
                               false, 0);
}
#endif

inline to_chars_result to_chars(char* first, char* last, float value,
                                 chars_format format, int precision) {
    return detail::fp_to_chars(first, last, detail::fp_decode(value), format,
                               true, precision);
}

#if defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
    ((__LDBL_MANT_DIG__ == 64 && __LDBL_MAX_EXP__ == 16384 && \
      defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
      defined(__ORDER_BIG_ENDIAN__) && \
      (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ || \
       __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)) || \
     (__LDBL_MANT_DIG__ == 53 && __LDBL_MAX_EXP__ == 1024))
inline to_chars_result to_chars(char* first, char* last, long double value,
                                chars_format format, int precision) {
    return detail::fp_to_chars(first, last, detail::fp_decode(value), format,
                               true, precision);
}
#endif

inline from_chars_result from_chars(const char* first, const char* last,
                                    float& value, chars_format format) {
    return format == chars_format::hex
        ? detail::fp_from_chars_hex(first, last, value)
        : detail::fp_from_chars_decimal(first, last, value, format);
}

inline from_chars_result from_chars(const char* first, const char* last,
                                     double& value, chars_format format) {
    return format == chars_format::hex
        ? detail::fp_from_chars_hex(first, last, value)
        : detail::fp_from_chars_decimal(first, last, value, format);
}

#if defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
    ((__LDBL_MANT_DIG__ == 64 && __LDBL_MAX_EXP__ == 16384 && \
      defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
      defined(__ORDER_BIG_ENDIAN__) && \
      (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ || \
       __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)) || \
     (__LDBL_MANT_DIG__ == 53 && __LDBL_MAX_EXP__ == 1024))
inline from_chars_result from_chars(const char* first, const char* last,
                                    long double& value, chars_format format) {
    return format == chars_format::hex
        ? detail::fp_from_chars_hex(first, last, value)
        : detail::fp_from_chars_decimal(first, last, value, format);
}
#endif

inline from_chars_result from_chars(const char* first, const char* last,
                                    float& value) {
    return detail::fp_from_chars_decimal(first, last, value,
                                         chars_format::general);
}

inline from_chars_result from_chars(const char* first, const char* last,
                                     double& value) {
    return detail::fp_from_chars_decimal(first, last, value,
                                         chars_format::general);
}

#if defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
    ((__LDBL_MANT_DIG__ == 64 && __LDBL_MAX_EXP__ == 16384 && \
      defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
      defined(__ORDER_BIG_ENDIAN__) && \
      (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ || \
       __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)) || \
     (__LDBL_MANT_DIG__ == 53 && __LDBL_MAX_EXP__ == 1024))
inline from_chars_result from_chars(const char* first, const char* last,
                                    long double& value) {
    return detail::fp_from_chars_decimal(first, last, value,
                                         chars_format::general);
}
#endif

inline to_chars_result to_chars(char* first, char* last, double value,
                                chars_format format, int precision) {
    return detail::fp_to_chars(first, last, detail::fp_decode(value), format,
                               true, precision);
}

} /* namespace std */

/* string.h reaches charconv through system_error/cstdlib.  Keep the standard
 * string overload here, after all charconv declarations are complete, so that
 * that include cycle cannot observe an incomplete to_chars_result. */
#if defined(RINCXX_STRING_H)
namespace std {

/* `long double` is a distinct standard overload even when the target uses a
 * wider x87 representation.  The representation-aware charconv owner keeps
 * six fixed fractional digits without a narrowing conversion. */
inline string to_string(long double value) {
    char buffer[16384];
#if defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
    ((__LDBL_MANT_DIG__ == 64 && __LDBL_MAX_EXP__ == 16384 && \
      defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
      defined(__ORDER_BIG_ENDIAN__) && \
      (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ || \
       __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)) || \
     (__LDBL_MANT_DIG__ == 53 && __LDBL_MAX_EXP__ == 1024))
    /* The representation-aware overload is present only for ABIs whose
     * payload decoder is implemented above. */
    const to_chars_result converted =
        std::to_chars(buffer, buffer + sizeof(buffer), value,
                      chars_format::fixed, 6);
#else
    /* Keep <string> well-formed on an unknown long-double ABI.  This is a
     * bounded compatibility fallback, not an implementation of that ABI's
     * long-double charconv contract; the value is deliberately narrowed and
     * the unsupported ABI remains tracked in TODO.md. */
    const to_chars_result converted =
        std::to_chars(buffer, buffer + sizeof(buffer),
                      static_cast<double>(value), chars_format::fixed, 6);
#endif
    if (converted.ec != errc{}) return string();
    return string(buffer, static_cast<size_t>(converted.ptr - buffer));
}

inline wstring to_wstring(long double value) {
    return __detail::narrow_to_wstring(to_string(value));
}

} /* namespace std */
#endif

#endif /* defined(__cplusplus) && __cplusplus >= 201703L */
#endif /* RINCXX_CHARCONV_H */
