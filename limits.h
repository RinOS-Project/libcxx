/*
 * RinOS C++ <limits> ✿
 * 数値型の限界値
 */

#ifndef RINCXX_LIMITS_H
#define RINCXX_LIMITS_H

#include "rincxx.h"
#include "climits.h"

#ifdef __cplusplus

namespace std {

/* 浮動小数点分類 */
enum float_round_style {
    round_indeterminate       = -1,
    round_toward_zero         =  0,
    round_to_nearest          =  1,
    round_toward_infinity     =  2,
    round_toward_neg_infinity =  3
};

enum float_denorm_style {
    denorm_indeterminate = -1,
    denorm_absent        =  0,
    denorm_present       =  1
};

/* 基本テンプレート */
template<typename T>
class numeric_limits {
public:
    static constexpr bool is_specialized = false;
    static constexpr T min() noexcept { return T(); }
    static constexpr T max() noexcept { return T(); }
    static constexpr T lowest() noexcept { return T(); }
    static constexpr int digits = 0;
    static constexpr int digits10 = 0;
    static constexpr int max_digits10 = 0;
    static constexpr bool is_signed = false;
    static constexpr bool is_integer = false;
    static constexpr bool is_exact = false;
    static constexpr int radix = 0;
    static constexpr T epsilon() noexcept { return T(); }
    static constexpr T round_error() noexcept { return T(); }
    static constexpr int min_exponent = 0;
    static constexpr int min_exponent10 = 0;
    static constexpr int max_exponent = 0;
    static constexpr int max_exponent10 = 0;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr float_denorm_style has_denorm = denorm_absent;
    static constexpr bool has_denorm_loss = false;
    static constexpr T infinity() noexcept { return T(); }
    static constexpr T quiet_NaN() noexcept { return T(); }
    static constexpr T signaling_NaN() noexcept { return T(); }
    static constexpr T denorm_min() noexcept { return T(); }
    static constexpr bool is_iec559 = false;
    static constexpr bool is_bounded = false;
    static constexpr bool is_modulo = false;
    static constexpr bool traps = false;
    static constexpr bool tinyness_before = false;
    static constexpr float_round_style round_style = round_toward_zero;
};

/* bool特殊化 */
template<>
class numeric_limits<bool> {
public:
    static constexpr bool is_specialized = true;
    static constexpr bool min() noexcept { return false; }
    static constexpr bool max() noexcept { return true; }
    static constexpr bool lowest() noexcept { return false; }
    static constexpr int digits = 1;
    static constexpr int digits10 = 0;
    static constexpr int max_digits10 = 0;
    static constexpr bool is_signed = false;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr int radix = 2;
    static constexpr bool epsilon() noexcept { return false; }
    static constexpr bool round_error() noexcept { return false; }
    static constexpr int min_exponent = 0;
    static constexpr int min_exponent10 = 0;
    static constexpr int max_exponent = 0;
    static constexpr int max_exponent10 = 0;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr float_denorm_style has_denorm = denorm_absent;
    static constexpr bool has_denorm_loss = false;
    static constexpr bool infinity() noexcept { return false; }
    static constexpr bool quiet_NaN() noexcept { return false; }
    static constexpr bool signaling_NaN() noexcept { return false; }
    static constexpr bool denorm_min() noexcept { return false; }
    static constexpr bool is_iec559 = false;
    static constexpr bool is_bounded = true;
    static constexpr bool is_modulo = false;
    static constexpr bool traps = true;
    static constexpr bool tinyness_before = false;
    static constexpr float_round_style round_style = round_toward_zero;
};

/* char特殊化 */
template<>
class numeric_limits<char> {
public:
    static constexpr bool is_specialized = true;
    static constexpr char min() noexcept { return CHAR_MIN; }
    static constexpr char max() noexcept { return CHAR_MAX; }
    static constexpr char lowest() noexcept { return CHAR_MIN; }
    static constexpr int digits = CHAR_BIT - 1;
    static constexpr int digits10 = 2;
    static constexpr int max_digits10 = 0;
    static constexpr bool is_signed = (CHAR_MIN < 0);
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr int radix = 2;
    static constexpr char epsilon() noexcept { return 0; }
    static constexpr char round_error() noexcept { return 0; }
    static constexpr int min_exponent = 0;
    static constexpr int min_exponent10 = 0;
    static constexpr int max_exponent = 0;
    static constexpr int max_exponent10 = 0;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr float_denorm_style has_denorm = denorm_absent;
    static constexpr bool has_denorm_loss = false;
    static constexpr char infinity() noexcept { return 0; }
    static constexpr char quiet_NaN() noexcept { return 0; }
    static constexpr char signaling_NaN() noexcept { return 0; }
    static constexpr char denorm_min() noexcept { return 0; }
    static constexpr bool is_iec559 = false;
    static constexpr bool is_bounded = true;
    static constexpr bool is_modulo = !is_signed;
    static constexpr bool traps = true;
    static constexpr bool tinyness_before = false;
    static constexpr float_round_style round_style = round_toward_zero;
};

/* signed char特殊化 */
template<>
class numeric_limits<signed char> {
public:
    static constexpr bool is_specialized = true;
    static constexpr signed char min() noexcept { return SCHAR_MIN; }
    static constexpr signed char max() noexcept { return SCHAR_MAX; }
    static constexpr signed char lowest() noexcept { return SCHAR_MIN; }
    static constexpr int digits = CHAR_BIT - 1;
    static constexpr int digits10 = 2;
    static constexpr int max_digits10 = 0;
    static constexpr bool is_signed = true;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr int radix = 2;
    static constexpr signed char epsilon() noexcept { return 0; }
    static constexpr signed char round_error() noexcept { return 0; }
    static constexpr int min_exponent = 0;
    static constexpr int min_exponent10 = 0;
    static constexpr int max_exponent = 0;
    static constexpr int max_exponent10 = 0;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr float_denorm_style has_denorm = denorm_absent;
    static constexpr bool has_denorm_loss = false;
    static constexpr signed char infinity() noexcept { return 0; }
    static constexpr signed char quiet_NaN() noexcept { return 0; }
    static constexpr signed char signaling_NaN() noexcept { return 0; }
    static constexpr signed char denorm_min() noexcept { return 0; }
    static constexpr bool is_iec559 = false;
    static constexpr bool is_bounded = true;
    static constexpr bool is_modulo = false;
    static constexpr bool traps = true;
    static constexpr bool tinyness_before = false;
    static constexpr float_round_style round_style = round_toward_zero;
};

/* unsigned char特殊化 */
template<>
class numeric_limits<unsigned char> {
public:
    static constexpr bool is_specialized = true;
    static constexpr unsigned char min() noexcept { return 0; }
    static constexpr unsigned char max() noexcept { return UCHAR_MAX; }
    static constexpr unsigned char lowest() noexcept { return 0; }
    static constexpr int digits = CHAR_BIT;
    static constexpr int digits10 = 2;
    static constexpr int max_digits10 = 0;
    static constexpr bool is_signed = false;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr int radix = 2;
    static constexpr unsigned char epsilon() noexcept { return 0; }
    static constexpr unsigned char round_error() noexcept { return 0; }
    static constexpr int min_exponent = 0;
    static constexpr int min_exponent10 = 0;
    static constexpr int max_exponent = 0;
    static constexpr int max_exponent10 = 0;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr float_denorm_style has_denorm = denorm_absent;
    static constexpr bool has_denorm_loss = false;
    static constexpr unsigned char infinity() noexcept { return 0; }
    static constexpr unsigned char quiet_NaN() noexcept { return 0; }
    static constexpr unsigned char signaling_NaN() noexcept { return 0; }
    static constexpr unsigned char denorm_min() noexcept { return 0; }
    static constexpr bool is_iec559 = false;
    static constexpr bool is_bounded = true;
    static constexpr bool is_modulo = true;
    static constexpr bool traps = true;
    static constexpr bool tinyness_before = false;
    static constexpr float_round_style round_style = round_toward_zero;
};

/* short特殊化 */
template<>
class numeric_limits<short> {
public:
    static constexpr bool is_specialized = true;
    static constexpr short min() noexcept { return SHRT_MIN; }
    static constexpr short max() noexcept { return SHRT_MAX; }
    static constexpr short lowest() noexcept { return SHRT_MIN; }
    static constexpr int digits = 15;
    static constexpr int digits10 = 4;
    static constexpr int max_digits10 = 0;
    static constexpr bool is_signed = true;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr int radix = 2;
    static constexpr short epsilon() noexcept { return 0; }
    static constexpr short round_error() noexcept { return 0; }
    static constexpr int min_exponent = 0;
    static constexpr int min_exponent10 = 0;
    static constexpr int max_exponent = 0;
    static constexpr int max_exponent10 = 0;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr float_denorm_style has_denorm = denorm_absent;
    static constexpr bool has_denorm_loss = false;
    static constexpr short infinity() noexcept { return 0; }
    static constexpr short quiet_NaN() noexcept { return 0; }
    static constexpr short signaling_NaN() noexcept { return 0; }
    static constexpr short denorm_min() noexcept { return 0; }
    static constexpr bool is_iec559 = false;
    static constexpr bool is_bounded = true;
    static constexpr bool is_modulo = false;
    static constexpr bool traps = true;
    static constexpr bool tinyness_before = false;
    static constexpr float_round_style round_style = round_toward_zero;
};

/* unsigned short特殊化 */
template<>
class numeric_limits<unsigned short> {
public:
    static constexpr bool is_specialized = true;
    static constexpr unsigned short min() noexcept { return 0; }
    static constexpr unsigned short max() noexcept { return USHRT_MAX; }
    static constexpr unsigned short lowest() noexcept { return 0; }
    static constexpr int digits = 16;
    static constexpr int digits10 = 4;
    static constexpr int max_digits10 = 0;
    static constexpr bool is_signed = false;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr int radix = 2;
    static constexpr unsigned short epsilon() noexcept { return 0; }
    static constexpr unsigned short round_error() noexcept { return 0; }
    static constexpr int min_exponent = 0;
    static constexpr int min_exponent10 = 0;
    static constexpr int max_exponent = 0;
    static constexpr int max_exponent10 = 0;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr float_denorm_style has_denorm = denorm_absent;
    static constexpr bool has_denorm_loss = false;
    static constexpr unsigned short infinity() noexcept { return 0; }
    static constexpr unsigned short quiet_NaN() noexcept { return 0; }
    static constexpr unsigned short signaling_NaN() noexcept { return 0; }
    static constexpr unsigned short denorm_min() noexcept { return 0; }
    static constexpr bool is_iec559 = false;
    static constexpr bool is_bounded = true;
    static constexpr bool is_modulo = true;
    static constexpr bool traps = true;
    static constexpr bool tinyness_before = false;
    static constexpr float_round_style round_style = round_toward_zero;
};

/* int特殊化 */
template<>
class numeric_limits<int> {
public:
    static constexpr bool is_specialized = true;
    static constexpr int min() noexcept { return INT_MIN; }
    static constexpr int max() noexcept { return INT_MAX; }
    static constexpr int lowest() noexcept { return INT_MIN; }
    static constexpr int digits = 31;
    static constexpr int digits10 = 9;
    static constexpr int max_digits10 = 0;
    static constexpr bool is_signed = true;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr int radix = 2;
    static constexpr int epsilon() noexcept { return 0; }
    static constexpr int round_error() noexcept { return 0; }
    static constexpr int min_exponent = 0;
    static constexpr int min_exponent10 = 0;
    static constexpr int max_exponent = 0;
    static constexpr int max_exponent10 = 0;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr float_denorm_style has_denorm = denorm_absent;
    static constexpr bool has_denorm_loss = false;
    static constexpr int infinity() noexcept { return 0; }
    static constexpr int quiet_NaN() noexcept { return 0; }
    static constexpr int signaling_NaN() noexcept { return 0; }
    static constexpr int denorm_min() noexcept { return 0; }
    static constexpr bool is_iec559 = false;
    static constexpr bool is_bounded = true;
    static constexpr bool is_modulo = false;
    static constexpr bool traps = true;
    static constexpr bool tinyness_before = false;
    static constexpr float_round_style round_style = round_toward_zero;
};

/* unsigned int特殊化 */
template<>
class numeric_limits<unsigned int> {
public:
    static constexpr bool is_specialized = true;
    static constexpr unsigned int min() noexcept { return 0; }
    static constexpr unsigned int max() noexcept { return UINT_MAX; }
    static constexpr unsigned int lowest() noexcept { return 0; }
    static constexpr int digits = 32;
    static constexpr int digits10 = 9;
    static constexpr int max_digits10 = 0;
    static constexpr bool is_signed = false;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr int radix = 2;
    static constexpr unsigned int epsilon() noexcept { return 0; }
    static constexpr unsigned int round_error() noexcept { return 0; }
    static constexpr int min_exponent = 0;
    static constexpr int min_exponent10 = 0;
    static constexpr int max_exponent = 0;
    static constexpr int max_exponent10 = 0;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr float_denorm_style has_denorm = denorm_absent;
    static constexpr bool has_denorm_loss = false;
    static constexpr unsigned int infinity() noexcept { return 0; }
    static constexpr unsigned int quiet_NaN() noexcept { return 0; }
    static constexpr unsigned int signaling_NaN() noexcept { return 0; }
    static constexpr unsigned int denorm_min() noexcept { return 0; }
    static constexpr bool is_iec559 = false;
    static constexpr bool is_bounded = true;
    static constexpr bool is_modulo = true;
    static constexpr bool traps = true;
    static constexpr bool tinyness_before = false;
    static constexpr float_round_style round_style = round_toward_zero;
};

/* long特殊化 */
template<>
class numeric_limits<long> {
public:
    static constexpr bool is_specialized = true;
    static constexpr long min() noexcept { return LONG_MIN; }
    static constexpr long max() noexcept { return LONG_MAX; }
    static constexpr long lowest() noexcept { return LONG_MIN; }
    /* `long` follows the data model, not the CPU family.  In particular,
     * Windows x86-64 is LLP64 (32-bit long), while Linux x86-64 is LP64
     * (64-bit long).  Derive the width from the actual C++ type so hosted
     * consumers and freestanding targets observe the same ABI. */
    static constexpr int digits =
        static_cast<int>(sizeof(long) * CHAR_BIT) - 1;
    static constexpr int digits10 = (digits * 301) / 1000;
    static constexpr int max_digits10 = 0;
    static constexpr bool is_signed = true;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr int radix = 2;
    static constexpr long epsilon() noexcept { return 0; }
    static constexpr long round_error() noexcept { return 0; }
    static constexpr int min_exponent = 0;
    static constexpr int min_exponent10 = 0;
    static constexpr int max_exponent = 0;
    static constexpr int max_exponent10 = 0;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr float_denorm_style has_denorm = denorm_absent;
    static constexpr bool has_denorm_loss = false;
    static constexpr long infinity() noexcept { return 0; }
    static constexpr long quiet_NaN() noexcept { return 0; }
    static constexpr long signaling_NaN() noexcept { return 0; }
    static constexpr long denorm_min() noexcept { return 0; }
    static constexpr bool is_iec559 = false;
    static constexpr bool is_bounded = true;
    static constexpr bool is_modulo = false;
    static constexpr bool traps = true;
    static constexpr bool tinyness_before = false;
    static constexpr float_round_style round_style = round_toward_zero;
};

/* unsigned long特殊化 */
template<>
class numeric_limits<unsigned long> {
public:
    static constexpr bool is_specialized = true;
    static constexpr unsigned long min() noexcept { return 0; }
    static constexpr unsigned long max() noexcept { return ULONG_MAX; }
    static constexpr unsigned long lowest() noexcept { return 0; }
    /* Keep the unsigned partner tied to the actual LLP64/LP64 width too. */
    static constexpr int digits =
        static_cast<int>(sizeof(unsigned long) * CHAR_BIT);
    static constexpr int digits10 = (digits * 301) / 1000;
    static constexpr int max_digits10 = 0;
    static constexpr bool is_signed = false;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr int radix = 2;
    static constexpr unsigned long epsilon() noexcept { return 0; }
    static constexpr unsigned long round_error() noexcept { return 0; }
    static constexpr int min_exponent = 0;
    static constexpr int min_exponent10 = 0;
    static constexpr int max_exponent = 0;
    static constexpr int max_exponent10 = 0;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr float_denorm_style has_denorm = denorm_absent;
    static constexpr bool has_denorm_loss = false;
    static constexpr unsigned long infinity() noexcept { return 0; }
    static constexpr unsigned long quiet_NaN() noexcept { return 0; }
    static constexpr unsigned long signaling_NaN() noexcept { return 0; }
    static constexpr unsigned long denorm_min() noexcept { return 0; }
    static constexpr bool is_iec559 = false;
    static constexpr bool is_bounded = true;
    static constexpr bool is_modulo = true;
    static constexpr bool traps = true;
    static constexpr bool tinyness_before = false;
    static constexpr float_round_style round_style = round_toward_zero;
};

/* long long特殊化 */
template<>
class numeric_limits<long long> {
public:
    static constexpr bool is_specialized = true;
    static constexpr long long min() noexcept { return LLONG_MIN; }
    static constexpr long long max() noexcept { return LLONG_MAX; }
    static constexpr long long lowest() noexcept { return LLONG_MIN; }
    static constexpr int digits = 63;
    static constexpr int digits10 = 18;
    static constexpr int max_digits10 = 0;
    static constexpr bool is_signed = true;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr int radix = 2;
    static constexpr long long epsilon() noexcept { return 0; }
    static constexpr long long round_error() noexcept { return 0; }
    static constexpr int min_exponent = 0;
    static constexpr int min_exponent10 = 0;
    static constexpr int max_exponent = 0;
    static constexpr int max_exponent10 = 0;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr float_denorm_style has_denorm = denorm_absent;
    static constexpr bool has_denorm_loss = false;
    static constexpr long long infinity() noexcept { return 0; }
    static constexpr long long quiet_NaN() noexcept { return 0; }
    static constexpr long long signaling_NaN() noexcept { return 0; }
    static constexpr long long denorm_min() noexcept { return 0; }
    static constexpr bool is_iec559 = false;
    static constexpr bool is_bounded = true;
    static constexpr bool is_modulo = false;
    static constexpr bool traps = true;
    static constexpr bool tinyness_before = false;
    static constexpr float_round_style round_style = round_toward_zero;
};

/* unsigned long long特殊化 */
template<>
class numeric_limits<unsigned long long> {
public:
    static constexpr bool is_specialized = true;
    static constexpr unsigned long long min() noexcept { return 0; }
    static constexpr unsigned long long max() noexcept { return ULLONG_MAX; }
    static constexpr unsigned long long lowest() noexcept { return 0; }
    static constexpr int digits = 64;
    static constexpr int digits10 = 19;
    static constexpr int max_digits10 = 0;
    static constexpr bool is_signed = false;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr int radix = 2;
    static constexpr unsigned long long epsilon() noexcept { return 0; }
    static constexpr unsigned long long round_error() noexcept { return 0; }
    static constexpr int min_exponent = 0;
    static constexpr int min_exponent10 = 0;
    static constexpr int max_exponent = 0;
    static constexpr int max_exponent10 = 0;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr float_denorm_style has_denorm = denorm_absent;
    static constexpr bool has_denorm_loss = false;
    static constexpr unsigned long long infinity() noexcept { return 0; }
    static constexpr unsigned long long quiet_NaN() noexcept { return 0; }
    static constexpr unsigned long long signaling_NaN() noexcept { return 0; }
    static constexpr unsigned long long denorm_min() noexcept { return 0; }
    static constexpr bool is_iec559 = false;
    static constexpr bool is_bounded = true;
    static constexpr bool is_modulo = true;
    static constexpr bool traps = true;
    static constexpr bool tinyness_before = false;
    static constexpr float_round_style round_style = round_toward_zero;
};

/* float特殊化 */
template<>
class numeric_limits<float> {
public:
    static constexpr bool is_specialized = true;
    static constexpr float min() noexcept { return 1.17549435e-38f; }
    static constexpr float max() noexcept { return 3.40282347e+38f; }
    static constexpr float lowest() noexcept { return -3.40282347e+38f; }
    static constexpr int digits = 24;
    static constexpr int digits10 = 6;
    static constexpr int max_digits10 = 9;
    static constexpr bool is_signed = true;
    static constexpr bool is_integer = false;
    static constexpr bool is_exact = false;
    static constexpr int radix = 2;
    static constexpr float epsilon() noexcept { return 1.19209290e-07f; }
    static constexpr float round_error() noexcept { return 0.5f; }
    static constexpr int min_exponent = -125;
    static constexpr int min_exponent10 = -37;
    static constexpr int max_exponent = 128;
    static constexpr int max_exponent10 = 38;
    static constexpr bool has_infinity = true;
    static constexpr bool has_quiet_NaN = true;
    static constexpr bool has_signaling_NaN = true;
    static constexpr float_denorm_style has_denorm = denorm_present;
    static constexpr bool has_denorm_loss = false;
    static constexpr float infinity() noexcept { return __builtin_huge_valf(); }
    static constexpr float quiet_NaN() noexcept { return __builtin_nanf(""); }
    static constexpr float signaling_NaN() noexcept { return __builtin_nansf(""); }
    static constexpr float denorm_min() noexcept { return 1.40129846e-45f; }
    static constexpr bool is_iec559 = true;
    static constexpr bool is_bounded = true;
    static constexpr bool is_modulo = false;
    static constexpr bool traps = false;
    static constexpr bool tinyness_before = false;
    static constexpr float_round_style round_style = round_to_nearest;
};

/* double特殊化 */
template<>
class numeric_limits<double> {
public:
    static constexpr bool is_specialized = true;
    static constexpr double min() noexcept { return 2.2250738585072014e-308; }
    static constexpr double max() noexcept { return 1.7976931348623157e+308; }
    static constexpr double lowest() noexcept { return -1.7976931348623157e+308; }
    static constexpr int digits = 53;
    static constexpr int digits10 = 15;
    static constexpr int max_digits10 = 17;
    static constexpr bool is_signed = true;
    static constexpr bool is_integer = false;
    static constexpr bool is_exact = false;
    static constexpr int radix = 2;
    static constexpr double epsilon() noexcept { return 2.2204460492503131e-16; }
    static constexpr double round_error() noexcept { return 0.5; }
    static constexpr int min_exponent = -1021;
    static constexpr int min_exponent10 = -307;
    static constexpr int max_exponent = 1024;
    static constexpr int max_exponent10 = 308;
    static constexpr bool has_infinity = true;
    static constexpr bool has_quiet_NaN = true;
    static constexpr bool has_signaling_NaN = true;
    static constexpr float_denorm_style has_denorm = denorm_present;
    static constexpr bool has_denorm_loss = false;
    static constexpr double infinity() noexcept { return __builtin_huge_val(); }
    static constexpr double quiet_NaN() noexcept { return __builtin_nan(""); }
    static constexpr double signaling_NaN() noexcept { return __builtin_nans(""); }
    static constexpr double denorm_min() noexcept { return 4.9406564584124654e-324; }
    static constexpr bool is_iec559 = true;
    static constexpr bool is_bounded = true;
    static constexpr bool is_modulo = false;
    static constexpr bool traps = false;
    static constexpr bool tinyness_before = false;
    static constexpr float_round_style round_style = round_to_nearest;
};

/* long double特殊化 */
template<>
class numeric_limits<long double> {
public:
    static constexpr bool is_specialized = true;
    static constexpr long double min() noexcept { return __LDBL_MIN__; }
    static constexpr long double max() noexcept { return __LDBL_MAX__; }
    static constexpr long double lowest() noexcept { return -__LDBL_MAX__; }
    static constexpr int digits = __LDBL_MANT_DIG__;
    static constexpr int digits10 = __LDBL_DIG__;
    static constexpr int max_digits10 =
        2 + (__LDBL_MANT_DIG__ * 301) / 1000;
    static constexpr bool is_signed = true;
    static constexpr bool is_integer = false;
    static constexpr bool is_exact = false;
    static constexpr int radix = 2;
    static constexpr long double epsilon() noexcept { return __LDBL_EPSILON__; }
    static constexpr long double round_error() noexcept { return 0.5L; }
    static constexpr int min_exponent = __LDBL_MIN_EXP__;
    static constexpr int min_exponent10 = __LDBL_MIN_10_EXP__;
    static constexpr int max_exponent = __LDBL_MAX_EXP__;
    static constexpr int max_exponent10 = __LDBL_MAX_10_EXP__;
    static constexpr bool has_infinity = true;
    static constexpr bool has_quiet_NaN = true;
    static constexpr bool has_signaling_NaN = true;
    static constexpr float_denorm_style has_denorm = denorm_present;
    static constexpr bool has_denorm_loss = false;
    static constexpr long double infinity() noexcept { return __builtin_huge_vall(); }
    static constexpr long double quiet_NaN() noexcept { return __builtin_nanl(""); }
    static constexpr long double signaling_NaN() noexcept { return __builtin_nansl(""); }
    static constexpr long double denorm_min() noexcept { return __LDBL_DENORM_MIN__; }
    static constexpr bool is_iec559 = true;
    static constexpr bool is_bounded = true;
    static constexpr bool is_modulo = false;
    static constexpr bool traps = false;
    static constexpr bool tinyness_before = false;
    static constexpr float_round_style round_style = round_to_nearest;
};

template<typename Character, typename Underlying>
class __character_numeric_limits : public numeric_limits<Underlying> {
public:
    static constexpr Character min() noexcept {
        return static_cast<Character>(numeric_limits<Underlying>::min());
    }
    static constexpr Character max() noexcept {
        return static_cast<Character>(numeric_limits<Underlying>::max());
    }
    static constexpr Character lowest() noexcept {
        return static_cast<Character>(numeric_limits<Underlying>::lowest());
    }
    static constexpr Character epsilon() noexcept { return Character(); }
    static constexpr Character round_error() noexcept { return Character(); }
    static constexpr Character infinity() noexcept { return Character(); }
    static constexpr Character quiet_NaN() noexcept { return Character(); }
    static constexpr Character signaling_NaN() noexcept { return Character(); }
    static constexpr Character denorm_min() noexcept { return Character(); }
};

template<>
class numeric_limits<wchar_t>
    : public __character_numeric_limits<wchar_t, __WCHAR_TYPE__> {};

#if defined(__cpp_char8_t)
template<>
class numeric_limits<char8_t>
    : public __character_numeric_limits<char8_t, unsigned char> {};
#endif

template<>
class numeric_limits<char16_t>
    : public __character_numeric_limits<char16_t, __CHAR16_TYPE__> {};

template<>
class numeric_limits<char32_t>
    : public __character_numeric_limits<char32_t, __CHAR32_TYPE__> {};

template<typename T>
class numeric_limits<const T> : public numeric_limits<T> {};

template<typename T>
class numeric_limits<volatile T> : public numeric_limits<T> {};

template<typename T>
class numeric_limits<const volatile T> : public numeric_limits<T> {};

/* C++11/14 require an out-of-class definition when a static constexpr
 * numeric_limits data member is odr-used (for example, by taking its
 * address).  Keep these definitions external and header-safe until C++17's
 * inline-variable rule takes over. */
#if __cplusplus < 201703L
template<typename T> constexpr bool numeric_limits<T>::is_specialized;
template<typename T> constexpr int numeric_limits<T>::digits;
template<typename T> constexpr int numeric_limits<T>::digits10;
template<typename T> constexpr int numeric_limits<T>::max_digits10;
template<typename T> constexpr bool numeric_limits<T>::is_signed;
template<typename T> constexpr bool numeric_limits<T>::is_integer;
template<typename T> constexpr bool numeric_limits<T>::is_exact;
template<typename T> constexpr int numeric_limits<T>::radix;
template<typename T> constexpr int numeric_limits<T>::min_exponent;
template<typename T> constexpr int numeric_limits<T>::min_exponent10;
template<typename T> constexpr int numeric_limits<T>::max_exponent;
template<typename T> constexpr int numeric_limits<T>::max_exponent10;
template<typename T> constexpr bool numeric_limits<T>::has_infinity;
template<typename T> constexpr bool numeric_limits<T>::has_quiet_NaN;
template<typename T> constexpr bool numeric_limits<T>::has_signaling_NaN;
template<typename T> constexpr float_denorm_style numeric_limits<T>::has_denorm;
template<typename T> constexpr bool numeric_limits<T>::has_denorm_loss;
template<typename T> constexpr bool numeric_limits<T>::is_iec559;
template<typename T> constexpr bool numeric_limits<T>::is_bounded;
template<typename T> constexpr bool numeric_limits<T>::is_modulo;
template<typename T> constexpr bool numeric_limits<T>::traps;
template<typename T> constexpr bool numeric_limits<T>::tinyness_before;
template<typename T> constexpr float_round_style numeric_limits<T>::round_style;

#if defined(_MSC_VER)
#define RIN_CXX11_NUMERIC_LIMITS_SPECIALIZATION_DEF __declspec(selectany)
#elif defined(__GNUC__) || defined(__clang__)
#if defined(__MINGW32__) || defined(_WIN32)
#define RIN_CXX11_NUMERIC_LIMITS_SPECIALIZATION_DEF __attribute__((selectany))
#else
#define RIN_CXX11_NUMERIC_LIMITS_SPECIALIZATION_DEF __attribute__((weak))
#endif
#else
#define RIN_CXX11_NUMERIC_LIMITS_SPECIALIZATION_DEF
#endif
#define RIN_DEFINE_NL_MEMBER(TYPE, MEMBER) \
    constexpr decltype(numeric_limits<TYPE>::MEMBER) \
    numeric_limits<TYPE>::MEMBER RIN_CXX11_NUMERIC_LIMITS_SPECIALIZATION_DEF;
#define RIN_DEFINE_NL_ARITHMETIC(TYPE) \
    RIN_DEFINE_NL_MEMBER(TYPE, is_specialized) \
    RIN_DEFINE_NL_MEMBER(TYPE, digits) \
    RIN_DEFINE_NL_MEMBER(TYPE, digits10) \
    RIN_DEFINE_NL_MEMBER(TYPE, max_digits10) \
    RIN_DEFINE_NL_MEMBER(TYPE, is_signed) \
    RIN_DEFINE_NL_MEMBER(TYPE, is_integer) \
    RIN_DEFINE_NL_MEMBER(TYPE, is_exact) \
    RIN_DEFINE_NL_MEMBER(TYPE, radix) \
    RIN_DEFINE_NL_MEMBER(TYPE, min_exponent) \
    RIN_DEFINE_NL_MEMBER(TYPE, min_exponent10) \
    RIN_DEFINE_NL_MEMBER(TYPE, max_exponent) \
    RIN_DEFINE_NL_MEMBER(TYPE, max_exponent10) \
    RIN_DEFINE_NL_MEMBER(TYPE, has_infinity) \
    RIN_DEFINE_NL_MEMBER(TYPE, has_quiet_NaN) \
    RIN_DEFINE_NL_MEMBER(TYPE, has_signaling_NaN) \
    RIN_DEFINE_NL_MEMBER(TYPE, has_denorm) \
    RIN_DEFINE_NL_MEMBER(TYPE, has_denorm_loss) \
    RIN_DEFINE_NL_MEMBER(TYPE, is_iec559) \
    RIN_DEFINE_NL_MEMBER(TYPE, is_bounded) \
    RIN_DEFINE_NL_MEMBER(TYPE, is_modulo) \
    RIN_DEFINE_NL_MEMBER(TYPE, traps) \
    RIN_DEFINE_NL_MEMBER(TYPE, tinyness_before) \
    RIN_DEFINE_NL_MEMBER(TYPE, round_style)
RIN_DEFINE_NL_ARITHMETIC(bool)
RIN_DEFINE_NL_ARITHMETIC(char)
RIN_DEFINE_NL_ARITHMETIC(signed char)
RIN_DEFINE_NL_ARITHMETIC(unsigned char)
RIN_DEFINE_NL_ARITHMETIC(short)
RIN_DEFINE_NL_ARITHMETIC(unsigned short)
RIN_DEFINE_NL_ARITHMETIC(int)
RIN_DEFINE_NL_ARITHMETIC(unsigned int)
RIN_DEFINE_NL_ARITHMETIC(long)
RIN_DEFINE_NL_ARITHMETIC(unsigned long)
RIN_DEFINE_NL_ARITHMETIC(long long)
RIN_DEFINE_NL_ARITHMETIC(unsigned long long)
RIN_DEFINE_NL_ARITHMETIC(float)
RIN_DEFINE_NL_ARITHMETIC(double)
RIN_DEFINE_NL_ARITHMETIC(long double)
#undef RIN_DEFINE_NL_ARITHMETIC
#undef RIN_DEFINE_NL_MEMBER
#undef RIN_CXX11_NUMERIC_LIMITS_SPECIALIZATION_DEF
#endif

} /* namespace std */

#endif /* __cplusplus */
#endif /* RINCXX_LIMITS_H */
