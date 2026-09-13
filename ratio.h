/*
 * RinOS C++ <ratio> ✿
 * コンパイル時有理数演算
 */

#ifndef RINCXX_RATIO_H
#define RINCXX_RATIO_H

#include "rincxx.h"
#include "type_traits.h"
#include "cstdint.h"

/* P2734R0 adds the 2022 SI prefixes in C++26.  GCC 13 uses 202100L for its
 * C++2b dialect, so the preview define is explicit rather than inferred from
 * the raw __cplusplus value.  Non-representable aliases remain conditionally
 * absent on the current 64-bit intmax_t ABI as permitted by the paper. */
#if defined(__cplusplus) && \
    (__cplusplus > 202302L || defined(RIN_ENABLE_CXX26_RATIO))
#ifndef __cpp_lib_ratio
#define __cpp_lib_ratio 202306L
#endif
#endif

namespace std {

/* ═══════════════════════════════════════════════════════════════
 * コンパイル時数学ヘルパー
 * ═══════════════════════════════════════════════════════════════*/

namespace detail {

/* 絶対値 */
template<intmax_t X>
struct ratio_abs {
    static_assert(X != INTMAX_MIN,
                  "ratio absolute value is not representable");
    static constexpr intmax_t value = X < 0 ? -X : X;
};

/* 符号 */
template<intmax_t X>
struct ratio_sign {
    static constexpr intmax_t value = X < 0 ? -1 : (X > 0 ? 1 : 0);
};

/* 最大公約数 (GCD) */
template<intmax_t A, intmax_t B>
struct ratio_gcd {
    static constexpr intmax_t value = ratio_gcd<B, A % B>::value;
};

template<intmax_t A>
struct ratio_gcd<A, 0> {
    static constexpr intmax_t value = ratio_abs<A>::value;
};

template<intmax_t B>
struct ratio_gcd<0, B> {
    static constexpr intmax_t value = ratio_abs<B>::value;
};

template<>
struct ratio_gcd<0, 0> {
    static constexpr intmax_t value = 1;
};

/* 最小公倍数 (LCM) */
template<uintmax_t Left, uintmax_t Right,
         bool Overflow = (Right != 0u &&
             Left > static_cast<uintmax_t>(INTMAX_MAX) / Right)>
struct ratio_lcm_product;

template<uintmax_t Left, uintmax_t Right>
struct ratio_lcm_product<Left, Right, false> {
    static constexpr intmax_t value = static_cast<intmax_t>(Left * Right);
};

template<uintmax_t Left, uintmax_t Right>
struct ratio_lcm_product<Left, Right, true> {
    /* Keep an unrepresentable LCM ill-formed, but avoid evaluating a signed
     * overflowing product before the required diagnostic is instantiated. */
    static_assert(Left <= static_cast<uintmax_t>(INTMAX_MAX) / Right,
                  "ratio lcm is not representable");
};

template<intmax_t A, intmax_t B>
struct ratio_lcm {
    static constexpr intmax_t value =
        ratio_lcm_product<
            static_cast<uintmax_t>(ratio_abs<A>::value /
                                   ratio_gcd<A, B>::value),
            static_cast<uintmax_t>(ratio_abs<B>::value)>::value;
};

/* Compare non-negative fractions without forming cross-products.  Equal
 * integer quotients reduce the comparison to their reciprocal fractional
 * parts, reversing the ordering at each Euclidean step. */
#if __cplusplus >= 201402L
constexpr
#else
inline
#endif
bool ratio_unsigned_less(uintmax_t left_num, uintmax_t left_den,
                         uintmax_t right_num, uintmax_t right_den) {
    bool reversed = false;
    for (;;) {
        const uintmax_t left_quotient = left_num / left_den;
        const uintmax_t right_quotient = right_num / right_den;
        if (left_quotient != right_quotient) {
            return reversed ? left_quotient > right_quotient
                            : left_quotient < right_quotient;
        }

        const uintmax_t left_remainder = left_num % left_den;
        const uintmax_t right_remainder = right_num % right_den;
        if (left_remainder == 0 || right_remainder == 0) {
            if (left_remainder == right_remainder) return false;
            return reversed ? right_remainder == 0
                            : left_remainder == 0;
        }

        left_num = left_den;
        left_den = left_remainder;
        right_num = right_den;
        right_den = right_remainder;
        reversed = !reversed;
    }
}

#if __cplusplus >= 201402L
constexpr
#else
inline
#endif
bool ratio_signed_less(intmax_t left_num, intmax_t left_den,
                       intmax_t right_num, intmax_t right_den) {
    if (left_num < 0 && right_num >= 0) return true;
    if (left_num >= 0 && right_num < 0) return false;

    if (left_num < 0) {
        return ratio_unsigned_less(
            static_cast<uintmax_t>(-right_num),
            static_cast<uintmax_t>(right_den),
            static_cast<uintmax_t>(-left_num),
            static_cast<uintmax_t>(left_den));
    }
    return ratio_unsigned_less(
        static_cast<uintmax_t>(left_num),
        static_cast<uintmax_t>(left_den),
        static_cast<uintmax_t>(right_num),
        static_cast<uintmax_t>(right_den));
}

/* C++11 constexpr functions cannot express the loop above.  Keep ratio
 * comparison usable in that language mode with the same Euclidean algorithm
 * as a template recursion, so 32-bit targets do not need a non-standard
 * __int128 cross product. */
template<uintmax_t LeftNum, uintmax_t LeftDen,
         uintmax_t RightNum, uintmax_t RightDen,
         bool Reversed,
         bool DifferentQuotient = (LeftNum / LeftDen != RightNum / RightDen),
         bool HasZeroRemainder = (LeftNum % LeftDen == 0 ||
                                  RightNum % RightDen == 0)>
struct ratio_unsigned_less_cxx11;

template<uintmax_t LeftNum, uintmax_t LeftDen,
         uintmax_t RightNum, uintmax_t RightDen, bool Reversed,
         bool HasZeroRemainder>
struct ratio_unsigned_less_cxx11<LeftNum, LeftDen, RightNum, RightDen,
                                  Reversed, true, HasZeroRemainder>
    : integral_constant<bool,
        Reversed ? (LeftNum / LeftDen > RightNum / RightDen)
                 : (LeftNum / LeftDen < RightNum / RightDen)> {};

template<uintmax_t LeftNum, uintmax_t LeftDen,
         uintmax_t RightNum, uintmax_t RightDen, bool Reversed>
struct ratio_unsigned_less_cxx11<LeftNum, LeftDen, RightNum, RightDen,
                                  Reversed, false, true>
    : integral_constant<bool,
        (LeftNum % LeftDen == RightNum % RightDen)
            ? false
            : (Reversed ? (RightNum % RightDen == 0)
                        : (LeftNum % LeftDen == 0))> {};

template<uintmax_t LeftNum, uintmax_t LeftDen,
         uintmax_t RightNum, uintmax_t RightDen, bool Reversed>
struct ratio_unsigned_less_cxx11<LeftNum, LeftDen, RightNum, RightDen,
                                  Reversed, false, false>
    : ratio_unsigned_less_cxx11<LeftDen, LeftNum % LeftDen,
                                 RightDen, RightNum % RightDen,
                                 !Reversed> {};

template<typename Left, typename Right,
         bool LeftNegative = (Left::num < 0),
         bool RightNegative = (Right::num < 0)>
struct ratio_less_cxx11;

template<typename Left, typename Right>
struct ratio_less_cxx11<Left, Right, true, false> : true_type {};

template<typename Left, typename Right>
struct ratio_less_cxx11<Left, Right, false, true> : false_type {};

template<typename Left, typename Right>
struct ratio_less_cxx11<Left, Right, false, false>
    : ratio_unsigned_less_cxx11<
        static_cast<uintmax_t>(Left::num),
        static_cast<uintmax_t>(Left::den),
        static_cast<uintmax_t>(Right::num),
        static_cast<uintmax_t>(Right::den), false> {};

template<typename Left, typename Right>
struct ratio_less_cxx11<Left, Right, true, true>
    : ratio_unsigned_less_cxx11<
        static_cast<uintmax_t>(-Right::num),
        static_cast<uintmax_t>(Right::den),
        static_cast<uintmax_t>(-Left::num),
        static_cast<uintmax_t>(Left::den), false> {};

} /* namespace detail */

/* ═══════════════════════════════════════════════════════════════
 * ratio クラステンプレート
 * ═══════════════════════════════════════════════════════════════*/

template<intmax_t Num, intmax_t Denom = 1>
class ratio {
    static_assert(Denom != 0, "ratio denominator cannot be zero");
    static_assert(Num != INTMAX_MIN,
                  "ratio numerator magnitude is not representable");
    static_assert(Denom != INTMAX_MIN,
                  "ratio denominator magnitude is not representable");

    static constexpr intmax_t na = detail::ratio_abs<Num>::value;
    static constexpr intmax_t da = detail::ratio_abs<Denom>::value;
    static constexpr intmax_t g = detail::ratio_gcd<na, da>::value;
    static constexpr intmax_t s = detail::ratio_sign<Num>::value * detail::ratio_sign<Denom>::value;

public:
    static constexpr intmax_t num = s * na / g;
    static constexpr intmax_t den = da / g;

    using type = ratio<num, den>;
};

/* C++11/14 constexpr static data members still require an out-of-class
 * definition when a caller odr-uses their address (or binds a reference).
 * C++17 makes these members implicitly inline, so keep the definitions out of
 * newer modes to avoid a second definition on older toolchains. */
#if __cplusplus < 201703L
template<intmax_t Num, intmax_t Denom>
constexpr intmax_t ratio<Num, Denom>::num;

template<intmax_t Num, intmax_t Denom>
constexpr intmax_t ratio<Num, Denom>::den;
#endif

#if defined(__SIZEOF_INT128__) && !defined(RIN_RATIO_FORCE_PORTABLE_WIDE)
namespace detail {

/* The current RinOS ABI has a 64-bit intmax_t.  A signed 128-bit temporary
 * holds the unreduced numerator and denominator of any sum or difference of
 * two normalized intmax_t ratios; only the reduced result is converted back
 * to intmax_t. */
using ratio_wide_int = __int128;

constexpr ratio_wide_int ratio_wide_abs(ratio_wide_int value) {
    return value < 0 ? -value : value;
}

template<ratio_wide_int Left, ratio_wide_int Right>
struct ratio_wide_gcd {
    static constexpr ratio_wide_int value =
        ratio_wide_gcd<Right, Left % Right>::value;
};

template<ratio_wide_int Left>
struct ratio_wide_gcd<Left, 0> {
    static constexpr ratio_wide_int value = ratio_wide_abs(Left);
};

template<ratio_wide_int Numerator, ratio_wide_int Denominator>
struct ratio_from_wide {
    static_assert(Denominator > 0, "ratio denominator must be positive");

private:
    static constexpr ratio_wide_int divisor =
        ratio_wide_gcd<ratio_wide_abs(Numerator), Denominator>::value;
    static constexpr ratio_wide_int normalized_numerator = Numerator / divisor;
    static constexpr ratio_wide_int normalized_denominator = Denominator / divisor;

    static_assert(normalized_numerator >= -static_cast<ratio_wide_int>(INTMAX_MAX) &&
                  normalized_numerator <= static_cast<ratio_wide_int>(INTMAX_MAX),
                  "ratio numerator is not representable");
    static_assert(normalized_denominator <= static_cast<ratio_wide_int>(INTMAX_MAX),
                  "ratio denominator is not representable");

public:
    using type = typename ratio<static_cast<intmax_t>(normalized_numerator),
                                static_cast<intmax_t>(normalized_denominator)>::type;
};

} /* namespace detail */
#else
namespace detail {

/* A ratio sum/difference can have a 127-bit unreduced numerator even when
 * its reduced numerator and denominator fit in intmax_t.  Some i686 and
 * non-GNU targets provide no __int128, so keep this fixed two-word unsigned
 * representation in the type system instead of evaluating an overflowing
 * intmax_t intermediate.  Ratio inputs exclude INTMAX_MIN, therefore their
 * magnitudes and all relevant products fit in these 128 bits. */
static_assert(sizeof(uintmax_t) == 8u,
              "portable ratio wide arithmetic requires the 64-bit intmax ABI");
template<uintmax_t High, uintmax_t Low>
struct ratio_portable_wide {
    static constexpr uintmax_t high = High;
    static constexpr uintmax_t low = Low;
};

template<typename Left, typename Right>
struct ratio_portable_wide_less : integral_constant<bool,
    (Left::high < Right::high) ||
    (Left::high == Right::high && Left::low < Right::low)> {};

template<typename Value>
struct ratio_portable_wide_zero : integral_constant<bool,
    Value::high == 0u && Value::low == 0u> {};

template<typename Value>
struct ratio_portable_wide_even : integral_constant<bool,
    (Value::low & 1u) == 0u> {};

template<typename Left, typename Right>
struct ratio_portable_wide_add {
private:
    static constexpr uintmax_t low = Left::low + Right::low;
    static constexpr uintmax_t carry = low < Left::low ? 1u : 0u;

public:
    using type = ratio_portable_wide<Left::high + Right::high + carry, low>;
};

template<typename Left, typename Right>
struct ratio_portable_wide_subtract {
    static_assert(!ratio_portable_wide_less<Left, Right>::value,
                  "portable wide subtraction underflow");

private:
    static constexpr uintmax_t borrow = Left::low < Right::low ? 1u : 0u;

public:
    using type = ratio_portable_wide<
        Left::high - Right::high - borrow, Left::low - Right::low>;
};

template<typename Value>
struct ratio_portable_wide_shift_right {
    using type = ratio_portable_wide<
        (Value::high >> 1u),
        ((Value::low >> 1u) | (Value::high << 63u))>;
};

template<typename Value>
struct ratio_portable_wide_shift_left {
    using type = ratio_portable_wide<
        ((Value::high << 1u) | (Value::low >> 63u)), (Value::low << 1u)>;
};

template<typename Value>
struct ratio_portable_wide_increment {
private:
    static constexpr uintmax_t low = Value::low + 1u;

public:
    using type = ratio_portable_wide<
        Value::high + (low == 0u ? 1u : 0u), low>;
};

template<uintmax_t Left, uintmax_t Right>
struct ratio_portable_wide_multiply {
private:
    static constexpr uintmax_t limb_mask = 0xffffffffu;
    static constexpr uintmax_t left_low = Left & limb_mask;
    static constexpr uintmax_t left_high = Left >> 32u;
    static constexpr uintmax_t right_low = Right & limb_mask;
    static constexpr uintmax_t right_high = Right >> 32u;
    static constexpr uintmax_t low_low = left_low * right_low;
    static constexpr uintmax_t low_high = left_low * right_high;
    static constexpr uintmax_t high_low = left_high * right_low;
    static constexpr uintmax_t high_high = left_high * right_high;
    static constexpr uintmax_t after_low_high =
        low_low + (low_high << 32u);
    static constexpr uintmax_t carry_low_high =
        after_low_high < low_low ? 1u : 0u;
    static constexpr uintmax_t after_high_low =
        after_low_high + (high_low << 32u);
    static constexpr uintmax_t carry_high_low =
        after_high_low < after_low_high ? 1u : 0u;

public:
    using type = ratio_portable_wide<
        high_high + (low_high >> 32u) + carry_low_high +
            (high_low >> 32u) + carry_high_low,
        after_high_low>;
};

template<bool Negative, typename Magnitude>
struct ratio_portable_wide_signed {
    static constexpr bool negative = Negative && !ratio_portable_wide_zero<Magnitude>::value;
    using magnitude = Magnitude;
};

template<intmax_t Value, uintmax_t Factor>
struct ratio_portable_wide_product {
    static_assert(Value != INTMAX_MIN,
                  "ratio portable magnitude is not representable");
    static constexpr bool negative = Value < 0;
    using type = ratio_portable_wide_signed<negative,
        typename ratio_portable_wide_multiply<
            static_cast<uintmax_t>(negative ? -Value : Value), Factor>::type>;
};

template<typename Left, typename Right, bool SameSign,
         bool LeftLess = ratio_portable_wide_less<typename Left::magnitude,
                                                  typename Right::magnitude>::value>
struct ratio_portable_wide_signed_add_impl;

template<typename Left, typename Right, bool LeftLess>
struct ratio_portable_wide_signed_add_impl<Left, Right, true, LeftLess> {
    using type = ratio_portable_wide_signed<Left::negative,
        typename ratio_portable_wide_add<typename Left::magnitude,
                                         typename Right::magnitude>::type>;
};

template<typename Left, typename Right>
struct ratio_portable_wide_signed_add_impl<Left, Right, false, true> {
    using type = ratio_portable_wide_signed<Right::negative,
        typename ratio_portable_wide_subtract<typename Right::magnitude,
                                              typename Left::magnitude>::type>;
};

template<typename Left, typename Right>
struct ratio_portable_wide_signed_add_impl<Left, Right, false, false> {
    using type = ratio_portable_wide_signed<Left::negative,
        typename ratio_portable_wide_subtract<typename Left::magnitude,
                                              typename Right::magnitude>::type>;
};

template<typename Left, typename Right>
struct ratio_portable_wide_signed_add
    : ratio_portable_wide_signed_add_impl<
          Left, Right, Left::negative == Right::negative> {};

template<typename Value>
struct ratio_portable_wide_signed_negate {
    using type = ratio_portable_wide_signed<!Value::negative,
                                             typename Value::magnitude>;
};

/* Product of two signed intmax values.  The cross-cancelled factors used by
 * ratio_multiply/ratio_divide can still multiply to 126 bits, so do not form
 * the product in the signed host type (which would be undefined before the
 * ratio's required representability diagnostic is instantiated). */
template<intmax_t Left, intmax_t Right>
struct ratio_portable_wide_signed_product {
    static_assert(Left != INTMAX_MIN && Right != INTMAX_MIN,
                  "ratio portable signed magnitude is not representable");
    static constexpr bool negative = (Left < 0) != (Right < 0);
    using type = ratio_portable_wide_signed<negative,
        typename ratio_portable_wide_multiply<
            static_cast<uintmax_t>(Left < 0 ? -Left : Left),
            static_cast<uintmax_t>(Right < 0 ? -Right : Right)>::type>;
};

template<typename Left, typename Right, bool LeftZero,
         bool RightZero, bool LeftEven, bool RightEven>
struct ratio_portable_wide_gcd_impl;

template<typename Left, typename Right>
struct ratio_portable_wide_gcd;

template<typename Left, typename Right, bool LeftEven, bool RightEven>
struct ratio_portable_wide_gcd_impl<Left, Right, true, false,
                                    LeftEven, RightEven> {
    using type = Right;
};

template<typename Left, typename Right, bool LeftEven, bool RightEven>
struct ratio_portable_wide_gcd_impl<Left, Right, false, true,
                                    LeftEven, RightEven> {
    using type = Left;
};

template<typename Left, typename Right, bool LeftEven, bool RightEven>
struct ratio_portable_wide_gcd_impl<Left, Right, true, true,
                                    LeftEven, RightEven> {
    using type = ratio_portable_wide<0u, 1u>;
};

template<typename Left, typename Right>
struct ratio_portable_wide_gcd_impl<Left, Right, false, false, true, true> {
    using left_half = typename ratio_portable_wide_shift_right<Left>::type;
    using right_half = typename ratio_portable_wide_shift_right<Right>::type;
    using type = typename ratio_portable_wide_shift_left<typename
        ratio_portable_wide_gcd<left_half, right_half>::type>::type;
};

template<typename Left, typename Right>
struct ratio_portable_wide_gcd_impl<Left, Right, false, false, true, false> {
    using half = typename ratio_portable_wide_shift_right<Left>::type;
    using type = typename ratio_portable_wide_gcd<half, Right>::type;
};

template<typename Left, typename Right>
struct ratio_portable_wide_gcd_impl<Left, Right, false, false, false, true> {
    using half = typename ratio_portable_wide_shift_right<Right>::type;
    using type = typename ratio_portable_wide_gcd<Left, half>::type;
};

template<typename Left, typename Right,
         bool LeftLess = ratio_portable_wide_less<Left, Right>::value>
struct ratio_portable_wide_gcd_odd_step;

template<typename Left, typename Right>
struct ratio_portable_wide_gcd_odd_step<Left, Right, true> {
    using difference = typename ratio_portable_wide_subtract<Right, Left>::type;
    using half = typename ratio_portable_wide_shift_right<difference>::type;
    using type = typename ratio_portable_wide_gcd<Left, half>::type;
};

template<typename Left, typename Right>
struct ratio_portable_wide_gcd_odd_step<Left, Right, false> {
    using difference = typename ratio_portable_wide_subtract<Left, Right>::type;
    using half = typename ratio_portable_wide_shift_right<difference>::type;
    using type = typename ratio_portable_wide_gcd<half, Right>::type;
};

template<typename Left, typename Right>
struct ratio_portable_wide_gcd_impl<Left, Right, false, false, false, false>
    : ratio_portable_wide_gcd_odd_step<Left, Right> {};

template<typename Left, typename Right>
struct ratio_portable_wide_gcd {
    using type = typename ratio_portable_wide_gcd_impl<
        Left, Right, ratio_portable_wide_zero<Left>::value,
        ratio_portable_wide_zero<Right>::value,
        ratio_portable_wide_even<Left>::value,
        ratio_portable_wide_even<Right>::value>::type;
};

template<typename Value, int Index, bool Upper = (Index >= 64)>
struct ratio_portable_wide_bit;

template<typename Value, int Index>
struct ratio_portable_wide_bit<Value, Index, true>
    : integral_constant<unsigned,
        static_cast<unsigned>((Value::high >> (Index - 64)) & 1u)> {};

template<typename Value, int Index>
struct ratio_portable_wide_bit<Value, Index, false>
    : integral_constant<unsigned,
        static_cast<unsigned>((Value::low >> Index) & 1u)> {};

template<typename Remainder, typename Quotient, typename Divisor,
         unsigned Bit, bool Subtract>
struct ratio_portable_wide_divide_step_impl;

template<typename Remainder, typename Quotient, typename Divisor,
         unsigned Bit>
struct ratio_portable_wide_divide_step_impl<Remainder, Quotient, Divisor,
                                            Bit, false> {
private:
    using shifted = typename ratio_portable_wide_shift_left<Remainder>::type;
    using with_bit = typename ratio_portable_wide_add<
        shifted, ratio_portable_wide<0u, Bit>>::type;

public:
    using remainder = with_bit;
    using shifted_quotient = typename ratio_portable_wide_shift_left<Quotient>::type;
    using quotient = shifted_quotient;
};

template<typename Remainder, typename Quotient, typename Divisor,
         unsigned Bit>
struct ratio_portable_wide_divide_step_impl<Remainder, Quotient, Divisor,
                                            Bit, true> {
private:
    using shifted = typename ratio_portable_wide_shift_left<Remainder>::type;
    using with_bit = typename ratio_portable_wide_add<
        shifted, ratio_portable_wide<0u, Bit>>::type;

public:
    using remainder = typename ratio_portable_wide_subtract<with_bit, Divisor>::type;
    using shifted_quotient = typename ratio_portable_wide_shift_left<Quotient>::type;
    using quotient = typename ratio_portable_wide_increment<shifted_quotient>::type;
};

template<typename Remainder, typename Quotient, typename Divisor,
         unsigned Bit>
struct ratio_portable_wide_divide_step {
private:
    using shifted = typename ratio_portable_wide_shift_left<Remainder>::type;
    using with_bit = typename ratio_portable_wide_add<
        shifted, ratio_portable_wide<0u, Bit>>::type;

public:
    using type = ratio_portable_wide_divide_step_impl<
        Remainder, Quotient, Divisor, Bit,
        !ratio_portable_wide_less<with_bit, Divisor>::value>;
};

template<typename Dividend, typename Divisor, int Index,
         typename Remainder, typename Quotient>
struct ratio_portable_wide_divide_bits {
private:
    using step = typename ratio_portable_wide_divide_step<
        Remainder, Quotient, Divisor,
        ratio_portable_wide_bit<Dividend, Index>::value>::type;

public:
    using type = typename ratio_portable_wide_divide_bits<
        Dividend, Divisor, Index - 1, typename step::remainder,
        typename step::quotient>::type;
};

template<typename Dividend, typename Divisor, typename Remainder,
         typename Quotient>
struct ratio_portable_wide_divide_bits<Dividend, Divisor, -1,
                                       Remainder, Quotient> {
    struct type {
        using quotient = Quotient;
        using remainder = Remainder;
    };
};

template<typename Dividend, typename Divisor>
struct ratio_portable_wide_divide {
    static_assert(!ratio_portable_wide_zero<Divisor>::value,
                  "portable wide division by zero");
    using type = typename ratio_portable_wide_divide_bits<
        Dividend, Divisor, 127, ratio_portable_wide<0u, 0u>,
        ratio_portable_wide<0u, 0u>>::type;
};

template<typename Numerator, typename Denominator>
struct ratio_from_portable_wide {
    static_assert(!ratio_portable_wide_zero<Denominator>::value,
                  "ratio denominator must be positive");

private:
    using divisor = typename ratio_portable_wide_gcd<
        typename Numerator::magnitude, Denominator>::type;
    using numerator_division = typename ratio_portable_wide_divide<
        typename Numerator::magnitude, divisor>::type;
    using denominator_division = typename ratio_portable_wide_divide<
        Denominator, divisor>::type;
    using normalized_numerator = typename numerator_division::quotient;
    using normalized_denominator = typename denominator_division::quotient;

    static_assert(ratio_portable_wide_zero<
                      typename numerator_division::remainder>::value &&
                  ratio_portable_wide_zero<
                      typename denominator_division::remainder>::value,
                  "portable wide ratio reduction must be exact");
    static_assert(normalized_numerator::high == 0u &&
                  normalized_numerator::low <=
                      static_cast<uintmax_t>(INTMAX_MAX),
                  "ratio numerator is not representable");
    static_assert(normalized_denominator::high == 0u &&
                  normalized_denominator::low != 0u &&
                  normalized_denominator::low <=
                      static_cast<uintmax_t>(INTMAX_MAX),
                  "ratio denominator is not representable");
    static constexpr intmax_t numerator = Numerator::negative
        ? -static_cast<intmax_t>(normalized_numerator::low)
        : static_cast<intmax_t>(normalized_numerator::low);

public:
    using type = typename ratio<numerator,
        static_cast<intmax_t>(normalized_denominator::low)>::type;
};

} /* namespace detail */
#endif

/* ═══════════════════════════════════════════════════════════════
 * ratio 演算
 * ═══════════════════════════════════════════════════════════════*/

/* 加算 */
template<typename R1, typename R2>
struct ratio_add_impl {
private:
#if defined(__SIZEOF_INT128__) && !defined(RIN_RATIO_FORCE_PORTABLE_WIDE)
    static constexpr detail::ratio_wide_int g =
        static_cast<detail::ratio_wide_int>(detail::ratio_gcd<
            R1::den, R2::den>::value);
    static constexpr detail::ratio_wide_int n =
        static_cast<detail::ratio_wide_int>(R1::num) * (R2::den / g) +
        static_cast<detail::ratio_wide_int>(R2::num) * (R1::den / g);
    static constexpr detail::ratio_wide_int d =
        (static_cast<detail::ratio_wide_int>(R1::den) / g) * R2::den;
#else
    static constexpr intmax_t g = detail::ratio_gcd<R1::den, R2::den>::value;
    using left = typename detail::ratio_portable_wide_product<
        R1::num, static_cast<uintmax_t>(R2::den / g)>::type;
    using right = typename detail::ratio_portable_wide_product<
        R2::num, static_cast<uintmax_t>(R1::den / g)>::type;
    using numerator = typename detail::ratio_portable_wide_signed_add<
        left, right>::type;
    using denominator = typename detail::ratio_portable_wide_multiply<
        static_cast<uintmax_t>(R1::den / g),
        static_cast<uintmax_t>(R2::den)>::type;
#endif

public:
#if defined(__SIZEOF_INT128__) && !defined(RIN_RATIO_FORCE_PORTABLE_WIDE)
    using type = typename detail::ratio_from_wide<n, d>::type;
#else
    using type = typename detail::ratio_from_portable_wide<
        numerator, denominator>::type;
#endif
    static constexpr intmax_t num = type::num;
    static constexpr intmax_t den = type::den;
};

template<typename R1, typename R2>
using ratio_add = typename ratio_add_impl<R1, R2>::type;

template<typename R1, typename R2>
using ratio_add_t = ratio_add<R1, R2>;

/* 減算 */
template<typename R1, typename R2>
struct ratio_subtract_impl {
private:
#if defined(__SIZEOF_INT128__) && !defined(RIN_RATIO_FORCE_PORTABLE_WIDE)
    static constexpr detail::ratio_wide_int g =
        static_cast<detail::ratio_wide_int>(detail::ratio_gcd<
            R1::den, R2::den>::value);
    static constexpr detail::ratio_wide_int n =
        static_cast<detail::ratio_wide_int>(R1::num) * (R2::den / g) -
        static_cast<detail::ratio_wide_int>(R2::num) * (R1::den / g);
    static constexpr detail::ratio_wide_int d =
        (static_cast<detail::ratio_wide_int>(R1::den) / g) * R2::den;
#else
    static constexpr intmax_t g = detail::ratio_gcd<R1::den, R2::den>::value;
    using left = typename detail::ratio_portable_wide_product<
        R1::num, static_cast<uintmax_t>(R2::den / g)>::type;
    using right = typename detail::ratio_portable_wide_product<
        R2::num, static_cast<uintmax_t>(R1::den / g)>::type;
    using numerator = typename detail::ratio_portable_wide_signed_add<
        left, typename detail::ratio_portable_wide_signed_negate<right>::type>::type;
    using denominator = typename detail::ratio_portable_wide_multiply<
        static_cast<uintmax_t>(R1::den / g),
        static_cast<uintmax_t>(R2::den)>::type;
#endif

public:
#if defined(__SIZEOF_INT128__) && !defined(RIN_RATIO_FORCE_PORTABLE_WIDE)
    using type = typename detail::ratio_from_wide<n, d>::type;
#else
    using type = typename detail::ratio_from_portable_wide<
        numerator, denominator>::type;
#endif
    static constexpr intmax_t num = type::num;
    static constexpr intmax_t den = type::den;
};

template<typename R1, typename R2>
using ratio_subtract = typename ratio_subtract_impl<R1, R2>::type;

template<typename R1, typename R2>
using ratio_subtract_t = ratio_subtract<R1, R2>;

/* 乗算 */
template<typename R1, typename R2>
struct ratio_multiply_impl {
private:
    static constexpr intmax_t g1 = detail::ratio_gcd<R1::num, R2::den>::value;
    static constexpr intmax_t g2 = detail::ratio_gcd<R2::num, R1::den>::value;
#if defined(__SIZEOF_INT128__) && !defined(RIN_RATIO_FORCE_PORTABLE_WIDE)
    /* Cross-cancellation keeps the operands minimal, but their product is
     * still allowed to exceed intmax_t before ratio's representability check.
     * Evaluate in signed 128-bit and let ratio_from_wide issue the required
     * diagnostic only after exact reduction. */
    static constexpr detail::ratio_wide_int n =
        static_cast<detail::ratio_wide_int>(R1::num / g1) *
        static_cast<detail::ratio_wide_int>(R2::num / g2);
    static constexpr detail::ratio_wide_int d =
        static_cast<detail::ratio_wide_int>(R1::den / g2) *
        static_cast<detail::ratio_wide_int>(R2::den / g1);
#else
    using numerator = typename detail::ratio_portable_wide_signed_product<
        R1::num / g1, R2::num / g2>::type;
    using denominator = typename detail::ratio_portable_wide_multiply<
        static_cast<uintmax_t>(R1::den / g2),
        static_cast<uintmax_t>(R2::den / g1)>::type;
#endif

public:
#if defined(__SIZEOF_INT128__) && !defined(RIN_RATIO_FORCE_PORTABLE_WIDE)
    using type = typename detail::ratio_from_wide<n, d>::type;
#else
    using type = typename detail::ratio_from_portable_wide<
        numerator, denominator>::type;
#endif
    static constexpr intmax_t num = type::num;
    static constexpr intmax_t den = type::den;
};

template<typename R1, typename R2>
using ratio_multiply = typename ratio_multiply_impl<R1, R2>::type;

template<typename R1, typename R2>
using ratio_multiply_t = ratio_multiply<R1, R2>;

/* 除算 */
template<typename R1, typename R2>
struct ratio_divide_impl {
    static_assert(R2::num != 0, "ratio_divide: division by zero");

private:
    static constexpr intmax_t g1 = detail::ratio_gcd<R1::num, R2::num>::value;
    static constexpr intmax_t g2 = detail::ratio_gcd<R2::den, R1::den>::value;
#if defined(__SIZEOF_INT128__) && !defined(RIN_RATIO_FORCE_PORTABLE_WIDE)
    static constexpr detail::ratio_wide_int raw_n =
        static_cast<detail::ratio_wide_int>(R1::num / g1) *
        static_cast<detail::ratio_wide_int>(R2::den / g2);
    static constexpr detail::ratio_wide_int raw_d =
        static_cast<detail::ratio_wide_int>(R1::den / g2) *
        static_cast<detail::ratio_wide_int>(R2::num / g1);
    /* ratio requires a positive denominator; normalize the divisor sign in
     * the wide domain before reducing, avoiding signed intmax overflow. */
    static constexpr bool denominator_negative = raw_d < 0;
    static constexpr detail::ratio_wide_int n =
        denominator_negative ? -raw_n : raw_n;
    static constexpr detail::ratio_wide_int d =
        denominator_negative ? -raw_d : raw_d;
#else
    using raw_numerator = typename detail::ratio_portable_wide_signed_product<
        R1::num / g1, R2::den / g2>::type;
    using raw_denominator = typename detail::ratio_portable_wide_signed_product<
        R1::den / g2, R2::num / g1>::type;
    using numerator = typename std::conditional<
        raw_denominator::negative,
        typename detail::ratio_portable_wide_signed_negate<raw_numerator>::type,
        raw_numerator>::type;
    using denominator = typename raw_denominator::magnitude;
#endif

public:
#if defined(__SIZEOF_INT128__) && !defined(RIN_RATIO_FORCE_PORTABLE_WIDE)
    using type = typename detail::ratio_from_wide<n, d>::type;
#else
    using type = typename detail::ratio_from_portable_wide<
        numerator, denominator>::type;
#endif
    static constexpr intmax_t num = type::num;
    static constexpr intmax_t den = type::den;
};

template<typename R1, typename R2>
using ratio_divide = typename ratio_divide_impl<R1, R2>::type;

template<typename R1, typename R2>
using ratio_divide_t = ratio_divide<R1, R2>;

/* ═══════════════════════════════════════════════════════════════
 * ratio 比較
 * ═══════════════════════════════════════════════════════════════*/

/* 等価 */
template<typename R1, typename R2>
struct ratio_equal : integral_constant<bool, R1::num == R2::num && R1::den == R2::den> {};

#if __cplusplus >= 201703L
template<typename R1, typename R2>
inline constexpr bool ratio_equal_v = ratio_equal<R1, R2>::value;
#endif

/* 非等価 */
template<typename R1, typename R2>
struct ratio_not_equal : integral_constant<bool, !ratio_equal<R1, R2>::value> {};

#if __cplusplus >= 201703L
template<typename R1, typename R2>
inline constexpr bool ratio_not_equal_v = ratio_not_equal<R1, R2>::value;
#endif

/* 小なり */
template<typename R1, typename R2>
#if __cplusplus >= 201402L
struct ratio_less {
public:
    static constexpr bool value = detail::ratio_signed_less(
        R1::num, R1::den, R2::num, R2::den);
};
#else
struct ratio_less : detail::ratio_less_cxx11<R1, R2> {};
#endif

#if __cplusplus >= 201703L
template<typename R1, typename R2>
inline constexpr bool ratio_less_v = ratio_less<R1, R2>::value;
#endif

/* 小なりイコール */
template<typename R1, typename R2>
struct ratio_less_equal : integral_constant<bool,
    ratio_equal<R1, R2>::value || ratio_less<R1, R2>::value> {};

#if __cplusplus >= 201703L
template<typename R1, typename R2>
inline constexpr bool ratio_less_equal_v = ratio_less_equal<R1, R2>::value;
#endif

/* 大なり */
template<typename R1, typename R2>
struct ratio_greater : integral_constant<bool, ratio_less<R2, R1>::value> {};

#if __cplusplus >= 201703L
template<typename R1, typename R2>
inline constexpr bool ratio_greater_v = ratio_greater<R1, R2>::value;
#endif

/* 大なりイコール */
template<typename R1, typename R2>
struct ratio_greater_equal : integral_constant<bool,
    !ratio_less<R1, R2>::value> {};

#if __cplusplus >= 201703L
template<typename R1, typename R2>
inline constexpr bool ratio_greater_equal_v = ratio_greater_equal<R1, R2>::value;
#endif

/* ═══════════════════════════════════════════════════════════════
 * SI接頭辞 typedef
 * ═══════════════════════════════════════════════════════════════*/

/* Standard prefixes are provided only when their exact power of ten is
 * representable by intmax_t.  RinOS currently has a 64-bit intmax_t, so the
 * standard conditional yocto/zepto/zetta/yotta aliases and the C++26
 * quecto/ronto/ronna/quetta aliases are intentionally absent. */
using atto   = ratio<1, 1000000000000000000LL>;  /* 10^-18 */
using femto  = ratio<1, 1000000000000000LL>;     /* 10^-15 */
using pico   = ratio<1, 1000000000000LL>;        /* 10^-12 */
using nano   = ratio<1, 1000000000LL>;           /* 10^-9 */
using micro  = ratio<1, 1000000LL>;              /* 10^-6 */
using milli  = ratio<1, 1000LL>;                 /* 10^-3 */
using centi  = ratio<1, 100LL>;                  /* 10^-2 */
using deci   = ratio<1, 10LL>;                   /* 10^-1 */

using deca   = ratio<10LL, 1>;                   /* 10^1 */
using hecto  = ratio<100LL, 1>;                  /* 10^2 */
using kilo   = ratio<1000LL, 1>;                 /* 10^3 */
using mega   = ratio<1000000LL, 1>;              /* 10^6 */
using giga   = ratio<1000000000LL, 1>;           /* 10^9 */
using tera   = ratio<1000000000000LL, 1>;        /* 10^12 */
using peta   = ratio<1000000000000000LL, 1>;     /* 10^15 */
using exa    = ratio<1000000000000000000LL, 1>;  /* 10^18 */

/* The newer SI prefixes are conditional on the actual width of intmax_t.
 * Do not spell an unrepresentable non-type argument on the current 64-bit
 * ABI; a future wider implementation gets the aliases automatically. */
#if defined(__INTMAX_WIDTH__) && __INTMAX_WIDTH__ >= 81
using zepto  = ratio<1, static_cast<intmax_t>(1000000000000000000000)>; /* 10^-21 */
using yocto  = ratio<1, static_cast<intmax_t>(1000000000000000000000000)>; /* 10^-24 */
using zetta  = ratio<static_cast<intmax_t>(1000000000000000000000), 1>; /* 10^21 */
using yotta  = ratio<static_cast<intmax_t>(1000000000000000000000000), 1>; /* 10^24 */
#endif

#if defined(__INTMAX_WIDTH__) && __INTMAX_WIDTH__ >= 91
using ronto  = ratio<1, static_cast<intmax_t>(1000000000000000000000000000)>; /* 10^-27 */
using ronna  = ratio<static_cast<intmax_t>(1000000000000000000000000000), 1>; /* 10^27 */
#endif

#if defined(__INTMAX_WIDTH__) && __INTMAX_WIDTH__ >= 101
using quecto = ratio<1, static_cast<intmax_t>(1000000000000000000000000000000)>; /* 10^-30 */
using quetta = ratio<static_cast<intmax_t>(1000000000000000000000000000000), 1>; /* 10^30 */
#endif

/* IEC バイナリ接頭辞 (非標準だが便利) */
using kibi = ratio<1024LL, 1>;                          /* 2^10 */
using mebi = ratio<1048576LL, 1>;                       /* 2^20 */
using gibi = ratio<1073741824LL, 1>;                    /* 2^30 */
using tebi = ratio<1099511627776LL, 1>;                 /* 2^40 */
using pebi = ratio<1125899906842624LL, 1>;              /* 2^50 */
using exbi = ratio<1152921504606846976LL, 1>;           /* 2^60 */

} /* namespace std */

#endif /* RINCXX_RATIO_H */
