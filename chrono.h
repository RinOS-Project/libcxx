/*
 * RinOS C++ <chrono> ✿
 * 完全な時間ライブラリ実装
 */

#ifndef RINCXX_CHRONO_H
#define RINCXX_CHRONO_H

#include "rincxx.h"
#include "version.h"
#include "type_traits.h"
#include "ratio.h"
#include "limits.h"
#include "ctime.h"
#include "chrono_clock.h"
#if __cplusplus >= 202002L
#include "compare.h"
#endif

#ifdef __cplusplus

namespace std {
namespace chrono {

#if __cplusplus >= 201402L
#define RIN_CHRONO_CONSTEXPR14 constexpr
#else
#define RIN_CHRONO_CONSTEXPR14 inline
#endif

/* ═══════════════════════════════════════════════════════════════
 * ratio - std::ratio を chrono 名前空間にインポート
 * ═══════════════════════════════════════════════════════════════*/

/* std::ratio をそのまま使用 */
template<intmax_t Num, intmax_t Den = 1>
using ratio = std::ratio<Num, Den>;

/* 標準的なratio (std::から継承) */
using nano  = std::nano;
using micro = std::micro;
using milli = std::milli;
using centi = std::centi;
using deci  = std::deci;
using deca  = std::deca;
using hecto = std::hecto;
using kilo  = std::kilo;
using mega  = std::mega;
using giga  = std::giga;
using tera  = std::tera;
using peta  = std::peta;
using exa   = std::exa;

/* ratio_divide / ratio_multiply を std:: から継承 */
template<typename R1, typename R2>
using ratio_divide = std::ratio_divide<R1, R2>;

template<typename R1, typename R2>
using ratio_multiply = std::ratio_multiply<R1, R2>;

/* ═══════════════════════════════════════════════════════════════
 * duration - 時間間隔
 * ═══════════════════════════════════════════════════════════════*/

template<typename Rep>
struct treat_as_floating_point : is_floating_point<Rep> {};

#if __cplusplus >= 201703L
template<typename Rep>
inline constexpr bool treat_as_floating_point_v =
    treat_as_floating_point<Rep>::value;
#endif

template<typename Rep>
struct duration_values {
    static constexpr Rep zero() noexcept { return Rep(0); }
    static constexpr Rep min() noexcept { return numeric_limits<Rep>::lowest(); }
    static constexpr Rep max() noexcept { return numeric_limits<Rep>::max(); }
};

namespace chrono_detail {

/* Signed integer duration arithmetic is exposed by a freestanding header, so
 * allowing the host compiler's undefined overflow to escape would make the
 * same expression behave differently across targets.  Keep the policy local
 * to the representation: signed integers saturate, while unsigned and
 * non-integral/custom representations retain their native operators. */
template<typename Rep>
constexpr Rep add_rep(const Rep& lhs, const Rep& rhs, true_type) {
    return rhs > 0 && lhs > numeric_limits<Rep>::max() - rhs
        ? numeric_limits<Rep>::max()
        : rhs < 0 && lhs < numeric_limits<Rep>::lowest() - rhs
            ? numeric_limits<Rep>::lowest()
            : static_cast<Rep>(lhs + rhs);
}

template<typename Rep>
constexpr Rep add_rep(const Rep& lhs, const Rep& rhs, false_type) {
    return static_cast<Rep>(lhs + rhs);
}

template<typename Rep>
constexpr Rep add_rep(const Rep& lhs, const Rep& rhs) {
    return add_rep(lhs, rhs, integral_constant<bool,
        is_integral<Rep>::value && is_signed<Rep>::value>());
}

template<typename Rep>
constexpr Rep subtract_rep(const Rep& lhs, const Rep& rhs, true_type) {
    return rhs < 0 && lhs > numeric_limits<Rep>::max() + rhs
        ? numeric_limits<Rep>::max()
        : rhs > 0 && lhs < numeric_limits<Rep>::lowest() + rhs
            ? numeric_limits<Rep>::lowest()
            : static_cast<Rep>(lhs - rhs);
}

template<typename Rep>
constexpr Rep subtract_rep(const Rep& lhs, const Rep& rhs, false_type) {
    return static_cast<Rep>(lhs - rhs);
}

template<typename Rep>
constexpr Rep subtract_rep(const Rep& lhs, const Rep& rhs) {
    return subtract_rep(lhs, rhs, integral_constant<bool,
        is_integral<Rep>::value && is_signed<Rep>::value>());
}

template<typename Rep>
constexpr Rep multiply_rep(const Rep& lhs, const Rep& rhs, true_type) {
    return lhs == 0 || rhs == 0
        ? Rep(0)
        : lhs == -1 && rhs == numeric_limits<Rep>::lowest()
            ? numeric_limits<Rep>::max()
            : rhs == -1 && lhs == numeric_limits<Rep>::lowest()
                ? numeric_limits<Rep>::max()
                : lhs > 0 && rhs > 0 &&
                          lhs > numeric_limits<Rep>::max() / rhs
                    ? numeric_limits<Rep>::max()
                    : lhs > 0 && rhs < 0 &&
                              rhs < numeric_limits<Rep>::lowest() / lhs
                        ? numeric_limits<Rep>::lowest()
                        : lhs < 0 && rhs > 0 &&
                                  lhs < numeric_limits<Rep>::lowest() / rhs
                            ? numeric_limits<Rep>::lowest()
                            : lhs < 0 && rhs < 0 &&
                                      rhs < numeric_limits<Rep>::max() / lhs
                                ? numeric_limits<Rep>::lowest()
                                : static_cast<Rep>(lhs * rhs);
}

template<typename Rep>
constexpr Rep multiply_rep(const Rep& lhs, const Rep& rhs, false_type) {
    return static_cast<Rep>(lhs * rhs);
}

template<typename Rep>
constexpr Rep multiply_rep(const Rep& lhs, const Rep& rhs) {
    return multiply_rep(lhs, rhs, integral_constant<bool,
        is_integral<Rep>::value && is_signed<Rep>::value>());
}

template<typename Rep>
constexpr Rep divide_rep(const Rep& lhs, const Rep& rhs, true_type) {
    return lhs == numeric_limits<Rep>::lowest() && rhs == Rep(-1)
        ? numeric_limits<Rep>::max()
        : static_cast<Rep>(lhs / rhs);
}

template<typename Rep>
constexpr Rep divide_rep(const Rep& lhs, const Rep& rhs, false_type) {
    return static_cast<Rep>(lhs / rhs);
}

template<typename Rep>
constexpr Rep divide_rep(const Rep& lhs, const Rep& rhs) {
    return divide_rep(lhs, rhs, integral_constant<bool,
        is_integral<Rep>::value && is_signed<Rep>::value>());
}

template<typename Rep>
constexpr Rep remainder_rep(const Rep& lhs, const Rep& rhs, true_type) {
    return lhs == numeric_limits<Rep>::lowest() && rhs == Rep(-1)
        ? Rep(0)
        : static_cast<Rep>(lhs % rhs);
}

template<typename Rep>
constexpr Rep remainder_rep(const Rep& lhs, const Rep& rhs, false_type) {
    return static_cast<Rep>(lhs % rhs);
}

template<typename Rep>
constexpr Rep remainder_rep(const Rep& lhs, const Rep& rhs) {
    return remainder_rep(lhs, rhs, integral_constant<bool,
        is_integral<Rep>::value && is_signed<Rep>::value>());
}

/* Integer duration conversions can overflow before the target
 * representation is reached (for example seconds -> a sub-nanosecond
 * period).  Keep the C++11 constexpr form as one return expression and use
 * the compiler's 128-bit integer only for integral source/target reps when
 * the target provides it. */
#if defined(__SIZEOF_INT128__) && !defined(RIN_CHRONO_DISABLE_INT128)
template<typename ToRep, typename Rep, intmax_t Numerator,
         intmax_t Denominator>
constexpr ToRep duration_cast_value(const Rep& value, true_type) {
    return static_cast<ToRep>(
        ((static_cast<__int128>(value) * static_cast<__int128>(Numerator)) /
         static_cast<__int128>(Denominator)) >
                static_cast<__int128>(numeric_limits<ToRep>::max())
            ? numeric_limits<ToRep>::max()
            : ((static_cast<__int128>(value) *
                static_cast<__int128>(Numerator)) /
               static_cast<__int128>(Denominator)) <
                    static_cast<__int128>(numeric_limits<ToRep>::lowest())
                ? numeric_limits<ToRep>::lowest()
                : static_cast<ToRep>(
                      (static_cast<__int128>(value) *
                       static_cast<__int128>(Numerator)) /
                      static_cast<__int128>(Denominator)));
}
#else
/* The freestanding/MSVC profile has no __int128 carrier.  A direct
 * `value * Numerator` in that profile can overflow before the division even
 * when the final duration fits.  Keep the conversion constexpr in C++11 by
 * carrying quotient/remainder state through a bounded binary long division.
 * Ratio denominators are positive intmax_t values, so doubling a remainder
 * cannot overflow unsigned long long. */
struct duration_cast_unsigned_result {
    unsigned long long quotient;
    unsigned long long remainder;

    constexpr duration_cast_unsigned_result(unsigned long long q,
                                             unsigned long long r)
        : quotient(q), remainder(r) {}
};

constexpr bool duration_cast_step_carries(unsigned long long remainder,
                                          unsigned long long addend,
                                          unsigned long long denominator,
                                          unsigned long long bit) {
    return bit != 0 && remainder + remainder >= denominator - addend;
}

constexpr unsigned long long duration_cast_step_remainder(
    unsigned long long remainder, unsigned long long addend,
    unsigned long long denominator, unsigned long long bit) {
    return duration_cast_step_carries(remainder, addend, denominator, bit)
        ? remainder + remainder - (denominator - addend)
        : remainder + remainder + (bit != 0 ? addend : 0);
}

constexpr unsigned long long duration_cast_step_bit(
    unsigned long long multiplier, unsigned remaining) {
    return remaining == 0 ? 0 : ((multiplier >> (remaining - 1)) & 1u);
}

constexpr unsigned long long duration_cast_step_addend(
    unsigned long long dividend_quotient, unsigned long long dividend_remainder,
    unsigned long long denominator, unsigned long long remainder,
    unsigned long long bit) {
    return bit * dividend_quotient +
        (duration_cast_step_carries(remainder, dividend_remainder,
                                    denominator, bit) ? 1u : 0u);
}

constexpr bool duration_cast_step_overflows(unsigned long long quotient,
                                            unsigned long long addend,
                                            unsigned long long limit) {
    return addend > limit || quotient > (limit - addend) / 2;
}

constexpr duration_cast_unsigned_result duration_cast_unsigned_step(
    unsigned long long dividend_quotient, unsigned long long dividend_remainder,
    unsigned long long multiplier, unsigned long long denominator,
    unsigned long long limit, unsigned long long quotient,
    unsigned long long remainder, unsigned remaining) {
    /* Keep this as one return expression so C++11 can use duration_cast in a
     * constant expression; the C++14 path may still fold the recursion. */
    return remaining == 0
        ? duration_cast_unsigned_result(quotient, remainder)
        : duration_cast_step_overflows(
              quotient,
              duration_cast_step_addend(
                  dividend_quotient, dividend_remainder, denominator,
                  remainder, duration_cast_step_bit(multiplier, remaining)),
              limit)
            ? duration_cast_unsigned_result(limit, 0)
            : duration_cast_unsigned_step(
                  dividend_quotient, dividend_remainder, multiplier,
                  denominator, limit,
                  quotient * 2 + duration_cast_step_addend(
                      dividend_quotient, dividend_remainder, denominator,
                      remainder, duration_cast_step_bit(multiplier, remaining)),
                  duration_cast_step_remainder(remainder,
                                               dividend_remainder,
                                               denominator,
                                               duration_cast_step_bit(
                                                   multiplier, remaining)),
                  remaining - 1);
}

constexpr unsigned long long duration_cast_unsigned_mul_div(
    unsigned long long value, unsigned long long multiplier,
    unsigned long long denominator, unsigned long long limit) {
    return duration_cast_unsigned_step(
               value / denominator, value % denominator, multiplier,
               denominator, limit, 0, 0,
               static_cast<unsigned>(numeric_limits<unsigned long long>::digits))
        .quotient;
}

template<typename Rep>
constexpr bool duration_cast_negative(const Rep& value, true_type) {
    return value < Rep(0);
}

template<typename Rep>
constexpr bool duration_cast_negative(const Rep&, false_type) {
    return false;
}

template<typename Rep>
constexpr bool duration_cast_negative(const Rep& value) {
    return duration_cast_negative(value,
        integral_constant<bool, is_signed<Rep>::value>());
}

template<typename Rep>
constexpr unsigned long long duration_cast_magnitude(const Rep& value,
                                                      true_type) {
    typedef typename make_unsigned<Rep>::type UnsignedRep;
    return value < Rep(0)
        ? static_cast<unsigned long long>(
              UnsignedRep(0) - static_cast<UnsignedRep>(value))
        : static_cast<unsigned long long>(value);
}

template<typename Rep>
constexpr unsigned long long duration_cast_magnitude(const Rep& value,
                                                      false_type) {
    return static_cast<unsigned long long>(value);
}

template<typename Rep>
constexpr unsigned long long duration_cast_magnitude(const Rep& value) {
    return duration_cast_magnitude(value,
        integral_constant<bool, is_signed<Rep>::value>());
}

template<typename ToRep>
constexpr unsigned long long duration_cast_negative_limit(true_type) {
    return static_cast<unsigned long long>(0) - static_cast<unsigned long long>(
        numeric_limits<ToRep>::lowest());
}

template<typename ToRep>
constexpr unsigned long long duration_cast_negative_limit(false_type) {
    return 0;
}

template<typename ToRep>
constexpr unsigned long long duration_cast_negative_limit() {
    return duration_cast_negative_limit<ToRep>(
        integral_constant<bool, is_signed<ToRep>::value>());
}

template<typename ToRep>
constexpr ToRep duration_cast_apply_sign(unsigned long long converted,
                                          bool negative, true_type) {
    return !negative
        ? static_cast<ToRep>(converted)
        : converted == 0
            ? ToRep(0)
            : converted >= duration_cast_negative_limit<ToRep>()
                ? numeric_limits<ToRep>::lowest()
                : static_cast<ToRep>(-static_cast<ToRep>(converted));
}

template<typename ToRep>
constexpr ToRep duration_cast_apply_sign(unsigned long long converted,
                                          bool negative, false_type) {
    return negative ? ToRep(0) : static_cast<ToRep>(converted);
}

template<typename ToRep>
constexpr ToRep duration_cast_apply_sign(unsigned long long converted,
                                          bool negative) {
    return duration_cast_apply_sign<ToRep>(converted, negative,
        integral_constant<bool, is_signed<ToRep>::value>());
}

template<typename ToRep, typename Rep, intmax_t Numerator,
         intmax_t Denominator>
constexpr ToRep duration_cast_value(const Rep& value, true_type) {
    return duration_cast_apply_sign<ToRep>(
        duration_cast_unsigned_mul_div(
            duration_cast_magnitude(value),
            static_cast<unsigned long long>(Numerator),
            static_cast<unsigned long long>(Denominator),
            duration_cast_negative(value)
                ? duration_cast_negative_limit<ToRep>()
                : static_cast<unsigned long long>(numeric_limits<ToRep>::max())),
        duration_cast_negative(value));
}
#endif

template<typename ToRep, typename Rep, intmax_t Numerator,
         intmax_t Denominator>
RIN_CHRONO_CONSTEXPR14 ToRep duration_cast_floating_value(
    const Rep& value, true_type) {
    const long double scaled =
        static_cast<long double>(value) *
        static_cast<long double>(Numerator) /
        static_cast<long double>(Denominator);
    return __builtin_isnan(scaled)
        ? ToRep(0)
        : is_signed<ToRep>::value
            ? (scaled >= static_cast<long double>(
                           numeric_limits<ToRep>::max())
                ? numeric_limits<ToRep>::max()
                : scaled <= static_cast<long double>(
                                 numeric_limits<ToRep>::lowest())
                    ? numeric_limits<ToRep>::lowest()
                    : static_cast<ToRep>(scaled))
            : (!(scaled > 0.0L)
                ? ToRep(0)
                : scaled >= static_cast<long double>(
                               numeric_limits<ToRep>::max())
                    ? numeric_limits<ToRep>::max()
                    : static_cast<ToRep>(scaled));
}

template<typename ToRep, typename Rep, intmax_t Numerator,
         intmax_t Denominator>
RIN_CHRONO_CONSTEXPR14 ToRep duration_cast_floating_value(
    const Rep& value, false_type) {
    return static_cast<ToRep>(
        (static_cast<common_type_t<Rep, ToRep, intmax_t>>(value) *
         static_cast<common_type_t<Rep, ToRep, intmax_t>>(Numerator)) /
        static_cast<common_type_t<Rep, ToRep, intmax_t>>(Denominator));
}

template<typename ToRep, typename Rep, intmax_t Numerator,
         intmax_t Denominator>
RIN_CHRONO_CONSTEXPR14 ToRep duration_cast_value(const Rep& value, false_type) {
    return duration_cast_floating_value<ToRep, Rep, Numerator, Denominator>(
        value, integral_constant<bool,
            is_floating_point<Rep>::value && is_integral<ToRep>::value>());
}

template<typename ToRep, typename Rep, intmax_t Numerator,
         intmax_t Denominator>
constexpr ToRep duration_cast_value(const Rep& value) {
    return duration_cast_value<ToRep, Rep, Numerator, Denominator>(
        value, integral_constant<bool,
            is_integral<Rep>::value && is_integral<ToRep>::value>());
}

/* Negating the minimum value of a signed integer is undefined in C++.
 * duration's unary minus is specified for every representation, so the
 * freestanding implementation uses a deterministic saturating result at
 * that one boundary. Unsigned and non-integral representations retain their
 * native arithmetic semantics. */
template<typename Rep>
constexpr Rep negate_rep(const Rep& value, true_type) {
    return value == numeric_limits<Rep>::lowest()
        ? numeric_limits<Rep>::max()
        : static_cast<Rep>(-value);
}

template<typename Rep>
constexpr Rep negate_rep(const Rep& value, false_type) {
    return static_cast<Rep>(-value);
}

template<typename Rep>
constexpr Rep negate_rep(const Rep& value) {
    return negate_rep(value, integral_constant<bool,
        is_integral<Rep>::value && is_signed<Rep>::value>());
}

} /* namespace chrono_detail */

template<typename Rep, typename Period = ratio<1>>
class duration;

template<typename ToDuration, typename Rep, typename Period>
constexpr ToDuration duration_cast(const duration<Rep, Period>& d);

template<typename Rep, typename Period>
class duration {
public:
    using rep = Rep;
    using period = Period;
    
private:
    rep rep_;
    
public:
    /* コンストラクタ */
    constexpr duration() = default;

    static_assert(period::num > 0 && period::den > 0,
                  "duration period must be positive");
    
    template<typename Rep2,
             typename enable_if<
                 is_convertible<const Rep2&, rep>::value &&
                 (treat_as_floating_point<rep>::value ||
                  !treat_as_floating_point<Rep2>::value),
                 int>::type = 0>
    constexpr explicit duration(const Rep2& r) : rep_(static_cast<rep>(r)) {}

    template<typename Rep2, typename Period2,
             typename enable_if<
                 is_convertible<const Rep2&, rep>::value &&
                 (treat_as_floating_point<rep>::value ||
                  (!treat_as_floating_point<Rep2>::value &&
                   ratio_divide<Period2, Period>::den == 1)),
                 int>::type = 0>
    constexpr duration(const duration<Rep2, Period2>& d)
        : rep_(duration_cast<duration>(d).count()) {}
    
    /* count() - 内部表現を返す */
    constexpr rep count() const { return rep_; }
    
    /* 単項演算子 */
    constexpr duration operator+() const { return *this; }
    constexpr duration operator-() const {
        return duration(chrono_detail::negate_rep(rep_));
    }
    
    /* 複合代入演算子 */
    RIN_CHRONO_CONSTEXPR14 duration& operator++() {
        rep_ = chrono_detail::add_rep(rep_, rep(1));
        return *this;
    }
    RIN_CHRONO_CONSTEXPR14 duration operator++(int) {
        duration old(rep_);
        ++*this;
        return old;
    }
    RIN_CHRONO_CONSTEXPR14 duration& operator--() {
        rep_ = chrono_detail::subtract_rep(rep_, rep(1));
        return *this;
    }
    RIN_CHRONO_CONSTEXPR14 duration operator--(int) {
        duration old(rep_);
        --*this;
        return old;
    }
    
    RIN_CHRONO_CONSTEXPR14 duration& operator+=(const duration& d) {
        rep_ = chrono_detail::add_rep(rep_, d.count());
        return *this;
    }
    RIN_CHRONO_CONSTEXPR14 duration& operator-=(const duration& d) {
        rep_ = chrono_detail::subtract_rep(rep_, d.count());
        return *this;
    }
    RIN_CHRONO_CONSTEXPR14 duration& operator*=(const rep& rhs) {
        rep_ = chrono_detail::multiply_rep(rep_, rhs);
        return *this;
    }
    RIN_CHRONO_CONSTEXPR14 duration& operator/=(const rep& rhs) {
        rep_ = chrono_detail::divide_rep(rep_, rhs);
        return *this;
    }
    RIN_CHRONO_CONSTEXPR14 duration& operator%=(const rep& rhs) {
        rep_ = chrono_detail::remainder_rep(rep_, rhs);
        return *this;
    }
    RIN_CHRONO_CONSTEXPR14 duration& operator%=(const duration& d) {
        rep_ = chrono_detail::remainder_rep(rep_, d.count());
        return *this;
    }
    
    /* 静的メンバ */
    static constexpr duration zero() noexcept {
        return duration(duration_values<rep>::zero());
    }
    static constexpr duration min() noexcept {
        return duration(duration_values<rep>::min());
    }
    static constexpr duration max() noexcept {
        return duration(duration_values<rep>::max());
    }
};

/* 二項演算子 */
template<typename Rep1, typename Period1, typename Rep2, typename Period2>
constexpr auto operator+(const duration<Rep1, Period1>& lhs, const duration<Rep2, Period2>& rhs)
    -> common_type_t<duration<Rep1, Period1>, duration<Rep2, Period2>> {
    using CD = common_type_t<duration<Rep1, Period1>,
                             duration<Rep2, Period2>>;
    return CD(chrono_detail::add_rep(CD(lhs).count(), CD(rhs).count()));
}

template<typename Rep1, typename Period1, typename Rep2, typename Period2>
constexpr auto operator-(const duration<Rep1, Period1>& lhs, const duration<Rep2, Period2>& rhs)
    -> common_type_t<duration<Rep1, Period1>, duration<Rep2, Period2>> {
    using CD = common_type_t<duration<Rep1, Period1>,
                             duration<Rep2, Period2>>;
    return CD(chrono_detail::subtract_rep(CD(lhs).count(), CD(rhs).count()));
}
} // namespace chrono

/* The common representation retains both operands' arithmetic type, while
 * the common period is the greatest period that divides both source periods.
 * This is the standard unit used for duration arithmetic and comparison. */
template<typename Rep1, typename Period1, typename Rep2, typename Period2>
struct common_type<chrono::duration<Rep1, Period1>,
                   chrono::duration<Rep2, Period2>> {
    using type = chrono::duration<
        common_type_t<Rep1, Rep2>,
        ratio<detail::ratio_gcd<Period1::num, Period2::num>::value,
              detail::ratio_lcm<Period1::den, Period2::den>::value>>;
};

namespace chrono {

template<typename Rep1, typename Period, typename Rep2>
constexpr duration<decltype(Rep1() * Rep2()), Period>
operator*(const duration<Rep1, Period>& d, const Rep2& s) {
    using Result = decltype(Rep1() * Rep2());
    return duration<Result, Period>(chrono_detail::multiply_rep(
        static_cast<Result>(d.count()), static_cast<Result>(s)));
}

template<typename Rep1, typename Rep2, typename Period>
constexpr duration<decltype(Rep1() * Rep2()), Period>
operator*(const Rep1& s, const duration<Rep2, Period>& d) {
    return d * s;
}

template<typename Rep1, typename Period, typename Rep2>
constexpr duration<decltype(Rep1() / Rep2()), Period>
operator/(const duration<Rep1, Period>& d, const Rep2& s) {
    using Result = decltype(Rep1() / Rep2());
    return duration<Result, Period>(chrono_detail::divide_rep(
        static_cast<Result>(d.count()), static_cast<Result>(s)));
}

template<typename Rep1, typename Period, typename Rep2>
constexpr duration<decltype(Rep1() % Rep2()), Period>
operator%(const duration<Rep1, Period>& d, const Rep2& s) {
    using Result = decltype(Rep1() % Rep2());
    return duration<Result, Period>(chrono_detail::remainder_rep(
        static_cast<Result>(d.count()), static_cast<Result>(s)));
}

template<typename Rep1, typename Period1, typename Rep2, typename Period2>
constexpr decltype(Rep1() / Rep2())
operator/(const duration<Rep1, Period1>& lhs, const duration<Rep2, Period2>& rhs) {
    using CD = common_type_t<duration<Rep1, Period1>,
                             duration<Rep2, Period2>>;
    return chrono_detail::divide_rep(CD(lhs).count(), CD(rhs).count());
}

template<typename Rep1, typename Period1, typename Rep2, typename Period2>
constexpr common_type_t<duration<Rep1, Period1>, duration<Rep2, Period2>>
operator%(const duration<Rep1, Period1>& lhs,
          const duration<Rep2, Period2>& rhs) {
    using CD = common_type_t<duration<Rep1, Period1>,
                             duration<Rep2, Period2>>;
    return CD(chrono_detail::remainder_rep(CD(lhs).count(), CD(rhs).count()));
}

/* 比較演算子 */
template<typename Rep1, typename Period1, typename Rep2, typename Period2>
constexpr bool operator==(const duration<Rep1, Period1>& lhs, const duration<Rep2, Period2>& rhs) {
    using CT = common_type_t<duration<Rep1, Period1>,
                             duration<Rep2, Period2>>;
    return CT(lhs).count() == CT(rhs).count();
}

template<typename Rep1, typename Period1, typename Rep2, typename Period2>
constexpr bool operator!=(const duration<Rep1, Period1>& lhs, const duration<Rep2, Period2>& rhs) {
    return !(lhs == rhs);
}

template<typename Rep1, typename Period1, typename Rep2, typename Period2>
constexpr bool operator<(const duration<Rep1, Period1>& lhs, const duration<Rep2, Period2>& rhs) {
    using CT = common_type_t<duration<Rep1, Period1>,
                             duration<Rep2, Period2>>;
    return CT(lhs).count() < CT(rhs).count();
}

template<typename Rep1, typename Period1, typename Rep2, typename Period2>
constexpr bool operator<=(const duration<Rep1, Period1>& lhs, const duration<Rep2, Period2>& rhs) {
    return !(rhs < lhs);
}

template<typename Rep1, typename Period1, typename Rep2, typename Period2>
constexpr bool operator>(const duration<Rep1, Period1>& lhs, const duration<Rep2, Period2>& rhs) {
    return rhs < lhs;
}

template<typename Rep1, typename Period1, typename Rep2, typename Period2>
constexpr bool operator>=(const duration<Rep1, Period1>& lhs, const duration<Rep2, Period2>& rhs) {
    return !(lhs < rhs);
}

#if __cplusplus >= 202002L
template<typename Rep1, typename Period1, typename Rep2, typename Period2>
constexpr auto operator<=>(const duration<Rep1, Period1>& lhs,
                           const duration<Rep2, Period2>& rhs)
    -> std::detail::synth_three_way_result_t<
        typename common_type_t<duration<Rep1, Period1>,
                               duration<Rep2, Period2>>::rep> {
    using common_duration =
        common_type_t<duration<Rep1, Period1>, duration<Rep2, Period2>>;
    return std::detail::synth_three_way(common_duration(lhs).count(),
                                        common_duration(rhs).count());
}
#endif

/* 標準的なduration型 */
using nanoseconds  = duration<long long, nano>;
using microseconds = duration<long long, micro>;
using milliseconds = duration<long long, milli>;
using seconds      = duration<long long>;
using minutes      = duration<long long, ratio<60>>;
using hours        = duration<long long, ratio<3600>>;

/* C++20 追加 */
#if __cplusplus >= 202002L
using days   = duration<long long, ratio<86400>>;
using weeks  = duration<long long, ratio<604800>>;
using months = duration<long long, ratio<2629746>>;
using years  = duration<long long, ratio<31556952>>;
#endif

/* ═══════════════════════════════════════════════════════════════
 * duration_cast
 * ═══════════════════════════════════════════════════════════════*/

namespace chrono_detail {

template<typename ToDuration, typename Rep, typename Period,
         bool NumeratorIsOne, bool DenominatorIsOne>
struct duration_cast_impl;

template<typename ToDuration, typename Rep, typename Period>
struct duration_cast_impl<ToDuration, Rep, Period, true, true> {
    static constexpr ToDuration cast(const duration<Rep, Period>& d) {
        typedef typename ToDuration::rep ToRep;
        return ToDuration(
            chrono_detail::duration_cast_value<ToRep, Rep, 1, 1>(
                d.count()));
    }
};

template<typename ToDuration, typename Rep, typename Period>
struct duration_cast_impl<ToDuration, Rep, Period, false, true> {
    static constexpr ToDuration cast(const duration<Rep, Period>& d) {
        typedef typename ToDuration::rep ToRep;
        typedef typename ToDuration::period ToPeriod;
        typedef ratio_divide<Period, ToPeriod> Conversion;
        return ToDuration(
            chrono_detail::duration_cast_value<ToRep, Rep, Conversion::num, 1>(
                d.count()));
    }
};

template<typename ToDuration, typename Rep, typename Period>
struct duration_cast_impl<ToDuration, Rep, Period, true, false> {
    static constexpr ToDuration cast(const duration<Rep, Period>& d) {
        typedef typename ToDuration::rep ToRep;
        typedef typename ToDuration::period ToPeriod;
        typedef ratio_divide<Period, ToPeriod> Conversion;
        return ToDuration(
            chrono_detail::duration_cast_value<ToRep, Rep, 1, Conversion::den>(
                d.count()));
    }
};

template<typename ToDuration, typename Rep, typename Period>
struct duration_cast_impl<ToDuration, Rep, Period, false, false> {
    static constexpr ToDuration cast(const duration<Rep, Period>& d) {
        typedef typename ToDuration::rep ToRep;
        typedef typename ToDuration::period ToPeriod;
        typedef ratio_divide<Period, ToPeriod> Conversion;
        return ToDuration(
            chrono_detail::duration_cast_value<ToRep, Rep,
                                                Conversion::num,
                                                Conversion::den>(d.count()));
    }
};

} /* namespace chrono_detail */

template<typename ToDuration, typename Rep, typename Period>
constexpr ToDuration duration_cast(const duration<Rep, Period>& d) {
    return chrono_detail::duration_cast_impl<
        ToDuration, Rep, Period,
        ratio_divide<Period, typename ToDuration::period>::num == 1,
        ratio_divide<Period, typename ToDuration::period>::den == 1>::cast(d);
}

/* ═══════════════════════════════════════════════════════════════
 * time_point
 * ═══════════════════════════════════════════════════════════════*/

template<typename Clock, typename Duration = typename Clock::duration>
class time_point {
public:
    using clock = Clock;
    using duration = Duration;
    using rep = typename duration::rep;
    using period = typename duration::period;
    
private:
    duration d_;
    
public:
    /* コンストラクタ */
    constexpr time_point() : d_(duration::zero()) {}
    constexpr explicit time_point(const duration& d) : d_(d) {}
    
    template<typename Duration2,
             typename enable_if<
                 is_convertible<Duration2, duration>::value,
                 int>::type = 0>
    constexpr time_point(const time_point<clock, Duration2>& t)
        : d_(t.time_since_epoch()) {}
    
    /* time_since_epoch */
    constexpr duration time_since_epoch() const { return d_; }
    
    /* 算術演算 */
    RIN_CHRONO_CONSTEXPR14 time_point& operator+=(const duration& d) {
        d_ += d;
        return *this;
    }
    RIN_CHRONO_CONSTEXPR14 time_point& operator-=(const duration& d) {
        d_ -= d;
        return *this;
    }
    
    /* 静的メンバ */
    static constexpr time_point min() noexcept { return time_point(duration::min()); }
    static constexpr time_point max() noexcept { return time_point(duration::max()); }
};

/* time_point演算子 */
template<typename Clock, typename Dur1, typename Rep2, typename Period2>
constexpr time_point<Clock,
                     common_type_t<Dur1, duration<Rep2, Period2>>>
operator+(const time_point<Clock, Dur1>& lhs, const duration<Rep2, Period2>& rhs) {
    using CommonDuration = common_type_t<Dur1, duration<Rep2, Period2>>;
    return time_point<Clock, CommonDuration>(
        CommonDuration(lhs.time_since_epoch()) + CommonDuration(rhs));
}

template<typename Rep1, typename Period1, typename Clock, typename Dur2>
constexpr time_point<Clock,
                     common_type_t<Dur2, duration<Rep1, Period1>>>
operator+(const duration<Rep1, Period1>& lhs, const time_point<Clock, Dur2>& rhs) {
    using CommonDuration = common_type_t<Dur2, duration<Rep1, Period1>>;
    return time_point<Clock, CommonDuration>(
        CommonDuration(rhs.time_since_epoch()) + CommonDuration(lhs));
}

template<typename Clock, typename Dur1, typename Rep2, typename Period2>
constexpr time_point<Clock,
                     common_type_t<Dur1, duration<Rep2, Period2>>>
operator-(const time_point<Clock, Dur1>& lhs, const duration<Rep2, Period2>& rhs) {
    using CommonDuration = common_type_t<Dur1, duration<Rep2, Period2>>;
    return time_point<Clock, CommonDuration>(
        CommonDuration(lhs.time_since_epoch()) - CommonDuration(rhs));
}

template<typename Clock, typename Dur1, typename Dur2>
constexpr auto operator-(const time_point<Clock, Dur1>& lhs, const time_point<Clock, Dur2>& rhs)
    -> decltype(lhs.time_since_epoch() - rhs.time_since_epoch()) {
    return lhs.time_since_epoch() - rhs.time_since_epoch();
}

/* time_point比較 */
template<typename Clock, typename Dur1, typename Dur2>
constexpr bool operator==(const time_point<Clock, Dur1>& lhs, const time_point<Clock, Dur2>& rhs) {
    return lhs.time_since_epoch() == rhs.time_since_epoch();
}

template<typename Clock, typename Dur1, typename Dur2>
constexpr bool operator!=(const time_point<Clock, Dur1>& lhs, const time_point<Clock, Dur2>& rhs) {
    return !(lhs == rhs);
}

template<typename Clock, typename Dur1, typename Dur2>
constexpr bool operator<(const time_point<Clock, Dur1>& lhs, const time_point<Clock, Dur2>& rhs) {
    return lhs.time_since_epoch() < rhs.time_since_epoch();
}

template<typename Clock, typename Dur1, typename Dur2>
constexpr bool operator<=(const time_point<Clock, Dur1>& lhs, const time_point<Clock, Dur2>& rhs) {
    return !(rhs < lhs);
}

template<typename Clock, typename Dur1, typename Dur2>
constexpr bool operator>(const time_point<Clock, Dur1>& lhs, const time_point<Clock, Dur2>& rhs) {
    return rhs < lhs;
}

template<typename Clock, typename Dur1, typename Dur2>
constexpr bool operator>=(const time_point<Clock, Dur1>& lhs, const time_point<Clock, Dur2>& rhs) {
    return !(lhs < rhs);
}

#if __cplusplus >= 202002L
template<typename Clock, typename Dur1, typename Dur2>
constexpr auto operator<=>(const time_point<Clock, Dur1>& lhs,
                           const time_point<Clock, Dur2>& rhs)
    -> decltype(lhs.time_since_epoch() <=> rhs.time_since_epoch()) {
    return lhs.time_since_epoch() <=> rhs.time_since_epoch();
}
#endif

/* time_point_cast */
template<typename ToDuration, typename Clock, typename Duration>
constexpr time_point<Clock, ToDuration>
time_point_cast(const time_point<Clock, Duration>& tp) {
    return time_point<Clock, ToDuration>(duration_cast<ToDuration>(tp.time_since_epoch()));
}

/* ═══════════════════════════════════════════════════════════════
 * クロック
 * ═══════════════════════════════════════════════════════════════*/

} // namespace chrono
} // namespace std

extern "C" {
    /* カーネル提供のティック取得関数 */
    unsigned long long rin_get_ticks(void);
    unsigned long long rin_get_ticks_per_second(void);

    /* TSC関連 (カーネル提供) */
    unsigned long long platform_get_tsc_frequency(void);
    int platform_has_hpet(void);
}

namespace std {
namespace chrono {

/* system_clock - システム時間（壁時計時間） */
struct system_clock {
    using rep        = long long;
    using period     = micro;
    using duration   = std::chrono::duration<rep, period>;
    using time_point = std::chrono::time_point<system_clock>;

    static constexpr bool is_steady = false;

    static time_point now() noexcept {
        const rep microseconds = static_cast<rep>(
            detail::chrono_realtime_now_nanoseconds() / 1000LL);
        return time_point(duration(microseconds));
    }

    /* time_tとの相互変換 */
    static time_t to_time_t(const time_point& tp) noexcept {
        return static_cast<time_t>(duration_cast<seconds>(tp.time_since_epoch()).count());
    }

    static time_point from_time_t(time_t t) noexcept {
        return time_point(seconds(t));
    }
};

/* steady_clock - 単調増加クロック (TSCベース) */
struct steady_clock {
    using rep        = long long;
    using period     = nano;
    using duration   = std::chrono::duration<rep, period>;
    using time_point = std::chrono::time_point<steady_clock>;

    static constexpr bool is_steady = true;

    static time_point now() noexcept {
        return time_point(duration(
            detail::chrono_steady_now_nanoseconds()));
    }
};

/* high_resolution_clock - 高精度クロック (TSCベース、ナノ秒精度) */
struct high_resolution_clock {
    using rep        = long long;
    using period     = nano;
    using duration   = std::chrono::duration<rep, period>;
    using time_point = std::chrono::time_point<high_resolution_clock>;

    static constexpr bool is_steady = true;

    static time_point now() noexcept {
        return time_point(duration(
            detail::chrono_steady_now_nanoseconds()));
    }

    /* TSC周波数取得 (プロファイリング用) */
    static unsigned long long tsc_frequency() noexcept {
#if defined(__STDC_HOSTED__) && __STDC_HOSTED__ != 0
        return 0;
#else
        return platform_get_tsc_frequency();
#endif
    }

    /* HPET利用可否 */
    static bool has_hpet() noexcept {
#if defined(__STDC_HOSTED__) && __STDC_HOSTED__ != 0
        return false;
#else
        return platform_has_hpet() != 0;
#endif
    }
};

/* C++20 hh_mm_ss.  Keep the decomposition in a decimal precision so a
 * non-decimal source period (for example 1/3 second) has a stable, bounded
 * subsecond representation instead of exposing a fractional duration. */
#if __cplusplus >= 202002L
namespace chrono_detail {

constexpr unsigned long long decimal_power10(unsigned width) noexcept {
    unsigned long long value = 1u;
    for (unsigned index = 0u; index != width; ++index) value *= 10u;
    return value;
}

template<typename Period>
constexpr int hh_mm_ss_fractional_width() noexcept {
    if (Period::den == 1) return 0;
    unsigned long long value = static_cast<unsigned long long>(Period::den);
    int width = 0;
    while (value >= 10u && value % 10u == 0u) {
        value /= 10u;
        ++width;
    }
    return value == 1u && width <= 18 ? width : 6;
}

template<typename Duration>
struct hh_mm_ss_traits {
    static constexpr int fractional_width =
        hh_mm_ss_fractional_width<typename Duration::period>();
    using rep = common_type_t<typename Duration::rep, long long>;
    using precision = duration<rep, ratio<
        1, static_cast<intmax_t>(decimal_power10(
            static_cast<unsigned>(fractional_width)))>>;
};

} /* namespace chrono_detail */

template<typename Duration>
class hh_mm_ss {
    using traits = chrono_detail::hh_mm_ss_traits<Duration>;

    bool negative_;
    bool minimum_;
    chrono::hours hours_;
    chrono::minutes minutes_;
    chrono::seconds seconds_;
    typename traits::precision subseconds_;

public:
    using precision = typename traits::precision;
    static constexpr unsigned fractional_width =
        static_cast<unsigned>(traits::fractional_width);

    constexpr explicit hh_mm_ss(Duration value) noexcept
        : negative_(value.count() < typename Duration::rep(0)),
          minimum_(false), hours_(0), minutes_(0), seconds_(0), subseconds_(0) {
        const precision total = duration_cast<precision>(value);
        typename precision::rep magnitude = total.count();
        minimum_ = negative_ && magnitude ==
            numeric_limits<typename precision::rep>::lowest();
        if (negative_)
            magnitude = chrono_detail::negate_rep(magnitude);
        const typename precision::rep hour_units =
            duration_cast<precision>(chrono::hours(1)).count();
        const typename precision::rep minute_units =
            duration_cast<precision>(chrono::minutes(1)).count();
        const typename precision::rep second_units =
            duration_cast<precision>(chrono::seconds(1)).count();
        const typename precision::rep hour_count = magnitude / hour_units;
        magnitude %= hour_units;
        const typename precision::rep minute_count = magnitude / minute_units;
        magnitude %= minute_units;
        const typename precision::rep second_count = magnitude / second_units;
        magnitude %= second_units;
        hours_ = chrono::hours(static_cast<chrono::hours::rep>(hour_count));
        minutes_ = chrono::minutes(static_cast<chrono::minutes::rep>(minute_count));
        seconds_ = chrono::seconds(static_cast<chrono::seconds::rep>(second_count));
        subseconds_ = precision(magnitude);
    }

    constexpr bool is_negative() const noexcept { return negative_; }
    constexpr chrono::hours hours() const noexcept { return hours_; }
    constexpr chrono::minutes minutes() const noexcept { return minutes_; }
    constexpr chrono::seconds seconds() const noexcept { return seconds_; }
    constexpr precision subseconds() const noexcept { return subseconds_; }
    constexpr precision to_duration() const noexcept {
        if (minimum_)
            return precision(numeric_limits<typename precision::rep>::lowest());
        const precision total = duration_cast<precision>(hours_) +
            duration_cast<precision>(minutes_) +
            duration_cast<precision>(seconds_) + subseconds_;
        return negative_ ? precision(-total.count()) : total;
    }
    constexpr explicit operator bool() const noexcept {
        return to_duration().count() != 0;
    }
};

template<typename Duration>
constexpr bool operator==(const hh_mm_ss<Duration>& left,
                          const hh_mm_ss<Duration>& right) noexcept {
    return left.to_duration() == right.to_duration();
}

#endif /* C++20 hh_mm_ss */

/* ═══════════════════════════════════════════════════════════════
 * C++20 calendar
 * ═══════════════════════════════════════════════════════════════*/

#if __cplusplus >= 202002L

using sys_days = time_point<system_clock, days>;

struct last_spec {
    explicit constexpr last_spec() noexcept = default;
};

inline constexpr last_spec last{};

namespace chrono_detail {

constexpr long long floor_div(long long value, long long divisor) noexcept {
    const long long quotient = value / divisor;
    const long long remainder = value % divisor;
    return remainder < 0 ? quotient - 1 : quotient;
}

constexpr unsigned days_in_month(int year_value,
                                 unsigned month_value) noexcept {
    if (month_value == 2u) {
        const bool leap = (year_value % 4 == 0) &&
                          (year_value % 100 != 0 || year_value % 400 == 0);
        return leap ? 29u : 28u;
    }
    return month_value == 4u || month_value == 6u ||
                   month_value == 9u || month_value == 11u
               ? 30u
               : 31u;
}

constexpr int saturating_int(long long value) noexcept {
    return value > static_cast<long long>(numeric_limits<int>::max())
        ? numeric_limits<int>::max()
        : value < static_cast<long long>(numeric_limits<int>::lowest())
            ? numeric_limits<int>::lowest()
            : static_cast<int>(value);
}

/* Howard Hinnant's proleptic Gregorian civil calendar conversion.  The
 * epoch offset makes day zero 1970-01-01, matching system_clock. */
constexpr long long days_from_civil(int year_value, unsigned month_value,
                                    unsigned day_value) noexcept {
    const long long adjusted_year = static_cast<long long>(year_value) -
                                    (month_value <= 2u ? 1LL : 0LL);
    const long long era = floor_div(adjusted_year, 400);
    const unsigned year_of_era = static_cast<unsigned>(
        adjusted_year - era * 400LL);
    const unsigned adjusted_month =
        month_value > 2u ? month_value - 3u : month_value + 9u;
    const unsigned day_of_year =
        (153u * adjusted_month + 2u) / 5u + day_value - 1u;
    const unsigned day_of_era = year_of_era * 365u + year_of_era / 4u -
                                year_of_era / 100u + day_of_year;
    return era * 146097LL + static_cast<long long>(day_of_era) - 719468LL;
}

struct civil_date {
    int year;
    unsigned month;
    unsigned day;
};

constexpr civil_date civil_from_days(long long serial_day) noexcept {
    const long long shifted_serial =
        serial_day > numeric_limits<long long>::max() - 719468LL
            ? numeric_limits<long long>::max()
            : serial_day < numeric_limits<long long>::lowest() + 719468LL
                ? numeric_limits<long long>::lowest()
                : serial_day + 719468LL;
    const long long quotient = shifted_serial / 146097LL;
    const long long remainder = shifted_serial % 146097LL;
    const long long era = remainder < 0 ? quotient - 1LL : quotient;
    const unsigned day_of_era = static_cast<unsigned>(
        remainder < 0 ? remainder + 146097LL : remainder);
    const unsigned year_of_era =
        (day_of_era - day_of_era / 1460u + day_of_era / 36524u -
         day_of_era / 146096u) /
        365u;
    long long year_value = static_cast<long long>(year_of_era) + era * 400LL;
    const unsigned day_of_year =
        day_of_era - (365u * year_of_era + year_of_era / 4u -
                      year_of_era / 100u);
    const unsigned month_part = (5u * day_of_year + 2u) / 153u;
    const unsigned day_value =
        day_of_year - (153u * month_part + 2u) / 5u + 1u;
    const unsigned month_value = static_cast<unsigned>(
        static_cast<int>(month_part) + (month_part < 10u ? 3 : -9));
    year_value += month_value <= 2u ? 1LL : 0LL;
    return {saturating_int(year_value), month_value, day_value};
}

constexpr unsigned positive_mod(long long value, unsigned modulus) noexcept {
    const long long remainder = value % static_cast<long long>(modulus);
    return static_cast<unsigned>(remainder < 0 ? remainder + modulus
                                               : remainder);
}

/* Calendar values are noexcept and may be formed from invalid values before
 * the caller checks `ok()`.  Keep extreme arithmetic deterministic rather
 * than letting signed overflow or implementation-defined narrowing escape. */
constexpr long long saturating_add(long long lhs, long long rhs) noexcept {
    return rhs > 0 && lhs > numeric_limits<long long>::max() - rhs
        ? numeric_limits<long long>::max()
        : rhs < 0 && lhs < numeric_limits<long long>::lowest() - rhs
            ? numeric_limits<long long>::lowest()
            : lhs + rhs;
}

constexpr long long saturating_sub(long long lhs, long long rhs) noexcept {
    return rhs < 0 && lhs > numeric_limits<long long>::max() + rhs
        ? numeric_limits<long long>::max()
        : rhs > 0 && lhs < numeric_limits<long long>::lowest() + rhs
            ? numeric_limits<long long>::lowest()
            : lhs - rhs;
}

struct month_shift_result {
    int year;
    unsigned month;
};

constexpr month_shift_result shift_months(int year_value,
                                           unsigned month_value,
                                           long long amount,
                                           bool subtract) noexcept {
    long long whole = amount / 12LL;
    long long remainder = amount % 12LL;
    /* Negating the quotient/remainder is safe even when amount is LLONG_MIN:
     * division by twelve keeps the quotient away from the signed minimum. */
    if (subtract) {
        whole = -whole;
        remainder = -remainder;
    }
    const long long month_index =
        static_cast<long long>(month_value == 0u ? 0u : month_value - 1u) +
        remainder;
    const long long carry = floor_div(month_index, 12LL);
    const long long shifted_year = saturating_add(
        saturating_add(static_cast<long long>(year_value), whole), carry);
    return {saturating_int(shifted_year),
            positive_mod(month_index, 12u) + 1u};
}

} /* namespace chrono_detail */

class year {
public:
    using rep = int;

private:
    rep value_;

public:
    constexpr explicit year(rep value) noexcept : value_(value) {}
    constexpr year() noexcept : value_(0) {}
    constexpr rep value() const noexcept { return value_; }
    constexpr explicit operator int() const noexcept { return value_; }
    constexpr bool ok() const noexcept { return value_ >= -32767 && value_ <= 32767; }
    constexpr bool is_leap() const noexcept {
        return value_ % 4 == 0 && (value_ % 100 != 0 || value_ % 400 == 0);
    }
    constexpr year operator+() const noexcept { return *this; }
    constexpr year operator-() const noexcept {
        return year(chrono_detail::negate_rep(value_));
    }
    constexpr year& operator++() noexcept {
        value_ = chrono_detail::saturating_int(
            chrono_detail::saturating_add(static_cast<long long>(value_), 1LL));
        return *this;
    }
    constexpr year operator++(int) noexcept { year old(*this); ++*this; return old; }
    constexpr year& operator--() noexcept {
        value_ = chrono_detail::saturating_int(
            chrono_detail::saturating_sub(static_cast<long long>(value_), 1LL));
        return *this;
    }
    constexpr year operator--(int) noexcept { year old(*this); --*this; return old; }
    constexpr year& operator+=(const years& amount) noexcept {
        value_ = chrono_detail::saturating_int(
            chrono_detail::saturating_add(static_cast<long long>(value_),
                                          amount.count()));
        return *this;
    }
    constexpr year& operator-=(const years& amount) noexcept {
        value_ = chrono_detail::saturating_int(
            chrono_detail::saturating_sub(static_cast<long long>(value_),
                                          amount.count()));
        return *this;
    }
};

constexpr bool operator==(year left, year right) noexcept {
    return left.value() == right.value();
}
constexpr bool operator!=(year left, year right) noexcept { return !(left == right); }
constexpr bool operator<(year left, year right) noexcept { return left.value() < right.value(); }
constexpr bool operator>(year left, year right) noexcept { return right < left; }
constexpr bool operator<=(year left, year right) noexcept { return !(right < left); }
constexpr bool operator>=(year left, year right) noexcept { return !(left < right); }
#if __cplusplus >= 202002L
constexpr strong_ordering operator<=>(year left, year right) noexcept {
    return left.value() < right.value() ? strong_ordering::less
         : left.value() > right.value() ? strong_ordering::greater
                                        : strong_ordering::equivalent;
}
#endif
constexpr year operator+(year value, const years& amount) noexcept {
    value += amount; return value;
}
constexpr year operator+(const years& amount, year value) noexcept { return value + amount; }
constexpr year operator-(year value, const years& amount) noexcept {
    value -= amount; return value;
}
constexpr years operator-(year left, year right) noexcept {
    return years(static_cast<years::rep>(
        static_cast<long long>(left.value()) -
        static_cast<long long>(right.value())));
}

class month {
    unsigned value_;

public:
    constexpr explicit month(unsigned value) noexcept : value_(value) {}
    constexpr month() noexcept : value_(0) {}
    constexpr unsigned value() const noexcept { return value_; }
    constexpr explicit operator unsigned() const noexcept { return value_; }
    constexpr bool ok() const noexcept { return value_ >= 1u && value_ <= 12u; }
    constexpr month& operator++() noexcept { value_ = value_ == 12u ? 1u : value_ + 1u; return *this; }
    constexpr month operator++(int) noexcept { month old(*this); ++*this; return old; }
    constexpr month& operator--() noexcept { value_ = value_ == 1u ? 12u : value_ - 1u; return *this; }
    constexpr month operator--(int) noexcept { month old(*this); --*this; return old; }
    constexpr month& operator+=(const months& amount) noexcept {
        value_ = static_cast<unsigned>(chrono_detail::positive_mod(
            static_cast<long long>(value_ == 0u ? 0u : value_ - 1u) +
                amount.count() % 12LL, 12u) + 1u);
        return *this;
    }
    constexpr month& operator-=(const months& amount) noexcept {
        value_ = static_cast<unsigned>(chrono_detail::positive_mod(
            static_cast<long long>(value_ == 0u ? 0u : value_ - 1u) -
                amount.count() % 12LL, 12u) + 1u);
        return *this;
    }
};

constexpr bool operator==(month left, month right) noexcept { return left.value() == right.value(); }
constexpr bool operator!=(month left, month right) noexcept { return !(left == right); }
constexpr bool operator<(month left, month right) noexcept { return left.value() < right.value(); }
constexpr bool operator>(month left, month right) noexcept { return right < left; }
constexpr bool operator<=(month left, month right) noexcept { return !(right < left); }
constexpr bool operator>=(month left, month right) noexcept { return !(left < right); }
#if __cplusplus >= 202002L
constexpr strong_ordering operator<=>(month left, month right) noexcept {
    return left.value() < right.value() ? strong_ordering::less
         : left.value() > right.value() ? strong_ordering::greater
                                        : strong_ordering::equivalent;
}
#endif
constexpr month operator+(month value, const months& amount) noexcept { value += amount; return value; }
constexpr month operator+(const months& amount, month value) noexcept { return value + amount; }
constexpr month operator-(month value, const months& amount) noexcept { value -= amount; return value; }
constexpr months operator-(month left, month right) noexcept {
    return months(static_cast<months::rep>(left.value()) -
                  static_cast<months::rep>(right.value()));
}

class day {
    unsigned value_;

public:
    constexpr explicit day(unsigned value) noexcept : value_(value % 256u) {}
    constexpr day() noexcept : value_(0) {}
    constexpr unsigned value() const noexcept { return value_; }
    constexpr explicit operator unsigned() const noexcept { return value_; }
    constexpr bool ok() const noexcept { return value_ >= 1u && value_ <= 31u; }
    constexpr day& operator++() noexcept { return *this += days{1}; }
    constexpr day operator++(int) noexcept { day old(*this); ++*this; return old; }
    constexpr day& operator--() noexcept { return *this -= days{1}; }
    constexpr day operator--(int) noexcept { day old(*this); --*this; return old; }
    constexpr day& operator+=(const days& amount) noexcept {
        value_ = chrono_detail::positive_mod(
            static_cast<long long>(value_) + amount.count(), 256u);
        return *this;
    }
    constexpr day& operator-=(const days& amount) noexcept {
        value_ = chrono_detail::positive_mod(
            static_cast<long long>(value_) - amount.count(), 256u);
        return *this;
    }
};

constexpr bool operator==(day left, day right) noexcept { return left.value() == right.value(); }
constexpr bool operator!=(day left, day right) noexcept { return !(left == right); }
constexpr bool operator<(day left, day right) noexcept { return left.value() < right.value(); }
constexpr bool operator>(day left, day right) noexcept { return right < left; }
constexpr bool operator<=(day left, day right) noexcept { return !(right < left); }
#if __cplusplus >= 202002L
constexpr strong_ordering operator<=>(day left, day right) noexcept {
    return left.value() < right.value() ? strong_ordering::less
         : left.value() > right.value() ? strong_ordering::greater
                                        : strong_ordering::equivalent;
}
#endif
constexpr bool operator>=(day left, day right) noexcept { return !(left < right); }
constexpr day operator+(day value, const days& amount) noexcept {
    value += amount;
    return value;
}
constexpr day operator+(const days& amount, day value) noexcept {
    return value + amount;
}
constexpr day operator-(day value, const days& amount) noexcept {
    value -= amount;
    return value;
}
constexpr days operator-(day left, day right) noexcept {
    return days(static_cast<long long>(left.value()) -
                static_cast<long long>(right.value()));
}

inline constexpr month January{1}, February{2}, March{3}, April{4}, May{5},
    June{6}, July{7}, August{8}, September{9}, October{10}, November{11},
    December{12};

class weekday;
class weekday_indexed;
class weekday_last;

class weekday {
    unsigned value_;

public:
    constexpr explicit weekday(unsigned value) noexcept : value_(value % 7u) {}
    constexpr weekday() noexcept : value_(0) {}
    constexpr explicit weekday(const sys_days& value) noexcept
        : value_(chrono_detail::positive_mod(
              static_cast<long long>(chrono_detail::positive_mod(
                  value.time_since_epoch().count(), 7u)) + 4LL, 7u)) {}
    constexpr unsigned c_encoding() const noexcept { return value_; }
    constexpr unsigned iso_encoding() const noexcept { return value_ == 0u ? 7u : value_; }
    constexpr bool ok() const noexcept { return value_ <= 6u; }
    constexpr weekday& operator++() noexcept { value_ = (value_ + 1u) % 7u; return *this; }
    constexpr weekday operator++(int) noexcept { weekday old(*this); ++*this; return old; }
    constexpr weekday& operator--() noexcept { value_ = value_ == 0u ? 6u : value_ - 1u; return *this; }
    constexpr weekday operator--(int) noexcept { weekday old(*this); --*this; return old; }
    constexpr weekday& operator+=(const days& amount) noexcept {
        value_ = chrono_detail::positive_mod(
            static_cast<long long>(value_) + amount.count() % 7LL, 7u); return *this;
    }
    constexpr weekday& operator-=(const days& amount) noexcept {
        value_ = chrono_detail::positive_mod(
            static_cast<long long>(value_) - amount.count() % 7LL, 7u);
        return *this;
    }
    constexpr weekday_indexed operator[](unsigned index) const noexcept;
    constexpr weekday_last operator[](last_spec) const noexcept;
};

constexpr bool operator==(weekday left, weekday right) noexcept { return left.c_encoding() == right.c_encoding(); }
constexpr bool operator!=(weekday left, weekday right) noexcept { return !(left == right); }
constexpr bool operator<(weekday left, weekday right) noexcept { return left.c_encoding() < right.c_encoding(); }
constexpr weekday operator+(weekday value, const days& amount) noexcept { value += amount; return value; }
constexpr weekday operator+(const days& amount, weekday value) noexcept { return value + amount; }
#if __cplusplus >= 202002L
constexpr strong_ordering operator<=>(weekday left, weekday right) noexcept {
    return left.c_encoding() < right.c_encoding() ? strong_ordering::less
         : left.c_encoding() > right.c_encoding() ? strong_ordering::greater
                                                   : strong_ordering::equivalent;
}
#endif
constexpr weekday operator-(weekday value, const days& amount) noexcept { value -= amount; return value; }
constexpr days operator-(weekday left, weekday right) noexcept {
    return days(static_cast<long long>(chrono_detail::positive_mod(
        static_cast<long long>(left.c_encoding()) - right.c_encoding(), 7u)));
}

inline constexpr weekday Sunday{0}, Monday{1}, Tuesday{2}, Wednesday{3},
    Thursday{4}, Friday{5}, Saturday{6};

#if defined(__GNUC__) && !defined(__clang__)
/* GCC diagnoses chrono accessors whose names match their return types even
 * when the return type is qualified or named through an alias. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wchanges-meaning"
#endif

class weekday_indexed {
    weekday weekday_;
    unsigned index_;

public:
    constexpr weekday_indexed() noexcept : weekday_(), index_(0u) {}
    constexpr weekday_indexed(const chrono::weekday& value, unsigned index) noexcept
        : weekday_(value), index_(index) {}
    constexpr weekday weekday() const noexcept { return weekday_; }
    constexpr unsigned index() const noexcept { return index_; }
    constexpr bool ok() const noexcept { return weekday_.ok() && index_ >= 1u && index_ <= 5u; }
};

constexpr bool operator==(weekday_indexed left,
                          weekday_indexed right) noexcept {
    return left.weekday() == right.weekday() &&
           left.index() == right.index();
}
constexpr bool operator!=(weekday_indexed left,
                          weekday_indexed right) noexcept {
    return !(left == right);
}
constexpr bool operator<(weekday_indexed left,
                         weekday_indexed right) noexcept {
    return left.weekday() < right.weekday() ||
           (left.weekday() == right.weekday() &&
            left.index() < right.index());
}
constexpr bool operator>(weekday_indexed left,
                         weekday_indexed right) noexcept {
    return right < left;
}
constexpr bool operator<=(weekday_indexed left,
                          weekday_indexed right) noexcept {
    return !(right < left);
}
constexpr bool operator>=(weekday_indexed left,
                          weekday_indexed right) noexcept {
    return !(left < right);
}
#if __cplusplus >= 202002L
constexpr strong_ordering operator<=>(weekday_indexed left,
                                      weekday_indexed right) noexcept {
    if (left.weekday() < right.weekday()) return strong_ordering::less;
    if (right.weekday() < left.weekday()) return strong_ordering::greater;
    return left.index() < right.index() ? strong_ordering::less
         : left.index() > right.index() ? strong_ordering::greater
                                        : strong_ordering::equivalent;
}
#endif

class weekday_last {
    weekday weekday_;

public:
    constexpr weekday_last() noexcept : weekday_() {}
    constexpr explicit weekday_last(const chrono::weekday& value) noexcept : weekday_(value) {}
    constexpr weekday weekday() const noexcept { return weekday_; }
    constexpr bool ok() const noexcept { return weekday_.ok(); }
};

constexpr bool operator==(weekday_last left, weekday_last right) noexcept {
    return left.weekday() == right.weekday();
}
constexpr bool operator!=(weekday_last left, weekday_last right) noexcept {
    return !(left == right);
}
constexpr bool operator<(weekday_last left, weekday_last right) noexcept {
    return left.weekday() < right.weekday();
}
constexpr bool operator>(weekday_last left, weekday_last right) noexcept {
    return right < left;
}
constexpr bool operator<=(weekday_last left, weekday_last right) noexcept {
    return !(right < left);
}
constexpr bool operator>=(weekday_last left, weekday_last right) noexcept {
    return !(left < right);
}
#if __cplusplus >= 202002L
constexpr strong_ordering operator<=>(weekday_last left,
                                      weekday_last right) noexcept {
    return left.weekday() < right.weekday() ? strong_ordering::less
         : right.weekday() < left.weekday() ? strong_ordering::greater
                                            : strong_ordering::equivalent;
}
#endif

constexpr weekday_indexed weekday::operator[](unsigned index) const noexcept {
    return weekday_indexed(*this, index);
}
constexpr weekday_last weekday::operator[](last_spec) const noexcept {
    return weekday_last(*this);
}

class month_day {
    month month_;
    day day_;

public:
    constexpr month_day() noexcept : month_(), day_() {}
    constexpr month_day(const chrono::month& month_value,
                        const chrono::day& day_value) noexcept
        : month_(month_value), day_(day_value) {}
    constexpr month month() const noexcept { return month_; }
    constexpr day day() const noexcept { return day_; }
    constexpr bool ok() const noexcept {
        return month_.ok() && day_.ok() &&
               day_.value() <= chrono_detail::days_in_month(2020, month_.value());
    }
};

constexpr bool operator==(month_day left, month_day right) noexcept {
    return left.month() == right.month() && left.day() == right.day();
}
constexpr bool operator!=(month_day left, month_day right) noexcept { return !(left == right); }
constexpr bool operator<(month_day left, month_day right) noexcept {
    return left.month() < right.month() ||
           (left.month() == right.month() && left.day() < right.day());
}
constexpr bool operator>(month_day left, month_day right) noexcept {
    return right < left;
}
constexpr bool operator<=(month_day left, month_day right) noexcept {
    return !(right < left);
}
constexpr bool operator>=(month_day left, month_day right) noexcept {
    return !(left < right);
}
#if __cplusplus >= 202002L
constexpr strong_ordering operator<=>(month_day left,
                                      month_day right) noexcept {
    if (left.month() < right.month()) return strong_ordering::less;
    if (right.month() < left.month()) return strong_ordering::greater;
    return left.day() < right.day() ? strong_ordering::less
         : right.day() < left.day() ? strong_ordering::greater
                                    : strong_ordering::equivalent;
}
#endif

class month_day_last {
    month month_;

public:
    constexpr month_day_last() noexcept : month_() {}
    constexpr explicit month_day_last(const chrono::month& value) noexcept : month_(value) {}
    constexpr month month() const noexcept { return month_; }
    constexpr chrono::day day() const noexcept {
        return chrono::day{chrono_detail::days_in_month(2020, month_.value())};
    }
    constexpr bool ok() const noexcept { return month_.ok(); }
};

constexpr bool operator==(month_day_last left,
                          month_day_last right) noexcept {
    return left.month() == right.month();
}
constexpr bool operator!=(month_day_last left,
                          month_day_last right) noexcept {
    return !(left == right);
}
constexpr bool operator<(month_day_last left,
                         month_day_last right) noexcept {
    return left.month() < right.month();
}
constexpr bool operator>(month_day_last left,
                         month_day_last right) noexcept {
    return right < left;
}
constexpr bool operator<=(month_day_last left,
                          month_day_last right) noexcept {
    return !(right < left);
}
constexpr bool operator>=(month_day_last left,
                          month_day_last right) noexcept {
    return !(left < right);
}
#if __cplusplus >= 202002L
constexpr strong_ordering operator<=>(month_day_last left,
                                      month_day_last right) noexcept {
    return left.month() < right.month() ? strong_ordering::less
         : right.month() < left.month() ? strong_ordering::greater
                                        : strong_ordering::equivalent;
}
#endif

class year_month {
    year year_;
    month month_;

public:
    constexpr year_month() noexcept : year_(), month_() {}
    constexpr year_month(const chrono::year& year_value,
                         const chrono::month& month_value) noexcept
        : year_(year_value), month_(month_value) {}
    constexpr year year() const noexcept { return year_; }
    constexpr month month() const noexcept { return month_; }
    constexpr bool ok() const noexcept { return year_.ok() && month_.ok(); }
    constexpr year_month& operator+=(const months& amount) noexcept {
        const chrono_detail::month_shift_result shifted =
            chrono_detail::shift_months(year_.value(), month_.value(),
                                        amount.count(), false);
        year_ = chrono::year(shifted.year);
        month_ = chrono::month(shifted.month);
        return *this;
    }
    constexpr year_month& operator-=(const months& amount) noexcept {
        const chrono_detail::month_shift_result shifted =
            chrono_detail::shift_months(year_.value(), month_.value(),
                                        amount.count(), true);
        year_ = chrono::year(shifted.year);
        month_ = chrono::month(shifted.month);
        return *this;
    }
    constexpr year_month& operator+=(const years& amount) noexcept { year_ += amount; return *this; }
    constexpr year_month& operator-=(const years& amount) noexcept { year_ -= amount; return *this; }
};

constexpr bool operator==(year_month left, year_month right) noexcept {
    return left.year() == right.year() && left.month() == right.month();
}
constexpr bool operator!=(year_month left, year_month right) noexcept { return !(left == right); }
constexpr bool operator<(year_month left, year_month right) noexcept {
    return left.year() < right.year() ||
           (left.year() == right.year() && left.month() < right.month());
}
constexpr bool operator>(year_month left, year_month right) noexcept { return right < left; }
#if __cplusplus >= 202002L
constexpr strong_ordering operator<=>(year_month left,
                                      year_month right) noexcept {
    if (left.year() < right.year()) return strong_ordering::less;
    if (right.year() < left.year()) return strong_ordering::greater;
    return left.month() < right.month() ? strong_ordering::less
         : right.month() < left.month() ? strong_ordering::greater
                                        : strong_ordering::equivalent;
}
#endif
constexpr bool operator<=(year_month left, year_month right) noexcept { return !(right < left); }
constexpr bool operator>=(year_month left, year_month right) noexcept { return !(left < right); }
constexpr year_month operator+(year_month value, const months& amount) noexcept { value += amount; return value; }
constexpr year_month operator+(const months& amount, year_month value) noexcept { return value + amount; }
constexpr year_month operator-(year_month value, const months& amount) noexcept { value -= amount; return value; }
constexpr months operator-(year_month left, year_month right) noexcept {
    return months((static_cast<long long>(left.year().value()) -
                   static_cast<long long>(right.year().value())) * 12LL +
                  static_cast<long long>(left.month().value()) - right.month().value());
}
constexpr year_month operator+(year_month value, const years& amount) noexcept { value += amount; return value; }
constexpr year_month operator+(const years& amount, year_month value) noexcept { return value + amount; }
constexpr year_month operator-(year_month value, const years& amount) noexcept { value -= amount; return value; }

class year_month_day_last;

class year_month_day {
    year year_;
    month month_;
    day day_;

public:
    constexpr year_month_day() noexcept : year_(), month_(), day_() {}
    constexpr year_month_day(const chrono::year& year_value,
                             const chrono::month& month_value,
                             const chrono::day& day_value) noexcept
        : year_(year_value), month_(month_value), day_(day_value) {}
    constexpr explicit year_month_day(const sys_days& value) noexcept
        : year_(chrono_detail::civil_from_days(value.time_since_epoch().count()).year),
          month_(chrono_detail::civil_from_days(value.time_since_epoch().count()).month),
          day_(chrono_detail::civil_from_days(value.time_since_epoch().count()).day) {}
    constexpr year_month_day(const year_month_day_last& value) noexcept;
    constexpr year year() const noexcept { return year_; }
    constexpr month month() const noexcept { return month_; }
    constexpr day day() const noexcept { return day_; }
    constexpr bool ok() const noexcept {
        return year_.ok() && month_.ok() && day_.ok() &&
               day_.value() <= chrono_detail::days_in_month(year_.value(), month_.value());
    }
    constexpr year_month_day& operator+=(const months& amount) noexcept {
        const year_month shifted = year_month(year_, month_) + amount;
        year_ = shifted.year();
        month_ = shifted.month();
        return *this;
    }
    constexpr year_month_day& operator-=(const months& amount) noexcept {
        /* Route subtraction through year_month's checked month-shift owner.
         * Negating a duration count here is undefined for LLONG_MIN. */
        const year_month shifted = year_month(year_, month_) - amount;
        year_ = shifted.year();
        month_ = shifted.month();
        return *this;
    }
    constexpr year_month_day& operator+=(const years& amount) noexcept {
        year_ += amount;
        return *this;
    }
    constexpr year_month_day& operator-=(const years& amount) noexcept {
        year_ -= amount;
        return *this;
    }
    constexpr explicit operator sys_days() const noexcept {
        return sys_days(days(chrono_detail::days_from_civil(
            year_.value(), month_.value(), day_.value())));
    }
};

constexpr bool operator==(year_month_day left, year_month_day right) noexcept {
    return left.year() == right.year() && left.month() == right.month() &&
           left.day() == right.day();
}
constexpr bool operator!=(year_month_day left, year_month_day right) noexcept { return !(left == right); }
constexpr bool operator<(year_month_day left, year_month_day right) noexcept {
    return left.year() < right.year() ||
           (left.year() == right.year() &&
            (left.month() < right.month() ||
             (left.month() == right.month() && left.day() < right.day())));
}
constexpr bool operator>(year_month_day left, year_month_day right) noexcept { return right < left; }
#if __cplusplus >= 202002L
constexpr strong_ordering operator<=>(year_month_day left,
                                      year_month_day right) noexcept {
    if (left.year() < right.year()) return strong_ordering::less;
    if (right.year() < left.year()) return strong_ordering::greater;
    if (left.month() < right.month()) return strong_ordering::less;
    if (right.month() < left.month()) return strong_ordering::greater;
    return left.day() < right.day() ? strong_ordering::less
         : right.day() < left.day() ? strong_ordering::greater
                                     : strong_ordering::equivalent;
}
#endif
constexpr bool operator<=(year_month_day left, year_month_day right) noexcept { return !(right < left); }
constexpr bool operator>=(year_month_day left, year_month_day right) noexcept { return !(left < right); }
constexpr year_month_day operator+(year_month_day value, const years& amount) noexcept {
    return year_month_day(value.year() + amount, value.month(), value.day());
}
constexpr year_month_day operator+(const years& amount,
                                   year_month_day value) noexcept {
    return value + amount;
}
constexpr year_month_day operator-(year_month_day value, const years& amount) noexcept {
    value -= amount;
    return value;
}
constexpr year_month_day operator+(year_month_day value,
                                   const months& amount) noexcept {
    value += amount;
    return value;
}
constexpr year_month_day operator+(const months& amount,
                                   year_month_day value) noexcept {
    return value + amount;
}
constexpr year_month_day operator-(year_month_day value,
                                   const months& amount) noexcept {
    value -= amount;
    return value;
}

class year_month_day_last {
    year year_;
    month_day_last month_day_last_;

public:
    constexpr year_month_day_last() noexcept : year_(), month_day_last_() {}
    constexpr year_month_day_last(const chrono::year& year_value,
                                  const chrono::month_day_last& value) noexcept
        : year_(year_value), month_day_last_(value) {}
    constexpr year year() const noexcept { return year_; }
    constexpr month month() const noexcept { return month_day_last_.month(); }
    constexpr month_day_last month_day_last() const noexcept { return month_day_last_; }
    constexpr day day() const noexcept {
        return chrono::day{chrono_detail::days_in_month(
            year_.value(), month().value())};
    }
    constexpr bool ok() const noexcept { return year_.ok() && month().ok(); }
    constexpr year_month_day_last& operator+=(const months& amount) noexcept {
        const year_month shifted = year_month(year_, month()) + amount;
        year_ = shifted.year();
        month_day_last_ = chrono::month_day_last(shifted.month());
        return *this;
    }
    constexpr year_month_day_last& operator-=(const months& amount) noexcept {
        const year_month shifted = year_month(year_, month()) - amount;
        year_ = shifted.year();
        month_day_last_ = chrono::month_day_last(shifted.month());
        return *this;
    }
    constexpr year_month_day_last& operator+=(const years& amount) noexcept {
        year_ += amount;
        return *this;
    }
    constexpr year_month_day_last& operator-=(const years& amount) noexcept {
        year_ -= amount;
        return *this;
    }
    constexpr explicit operator sys_days() const noexcept {
        return sys_days(days(chrono_detail::days_from_civil(
            year_.value(), month().value(), day().value())));
    }
};

constexpr year_month_day::year_month_day(
    const year_month_day_last& value) noexcept
    : year_(value.year()), month_(value.month()), day_(value.day()) {}

constexpr bool operator==(year_month_day_last left,
                          year_month_day_last right) noexcept {
    return left.year() == right.year() && left.month() == right.month();
}
constexpr bool operator!=(year_month_day_last left,
                          year_month_day_last right) noexcept { return !(left == right); }
constexpr bool operator<(year_month_day_last left,
                         year_month_day_last right) noexcept {
    return left.year() < right.year() ||
           (left.year() == right.year() && left.month() < right.month());
}
constexpr bool operator>(year_month_day_last left,
                         year_month_day_last right) noexcept {
    return right < left;
}
constexpr bool operator<=(year_month_day_last left,
                          year_month_day_last right) noexcept {
    return !(right < left);
}
constexpr bool operator>=(year_month_day_last left,
                          year_month_day_last right) noexcept {
    return !(left < right);
}
#if __cplusplus >= 202002L
constexpr strong_ordering operator<=>(year_month_day_last left,
                                      year_month_day_last right) noexcept {
    if (left.year() < right.year()) return strong_ordering::less;
    if (right.year() < left.year()) return strong_ordering::greater;
    return left.month() < right.month() ? strong_ordering::less
         : right.month() < left.month() ? strong_ordering::greater
                                        : strong_ordering::equivalent;
}
#endif
constexpr year_month_day_last operator+(year_month_day_last value,
                                         const years& amount) noexcept {
    return year_month_day_last(value.year() + amount, value.month_day_last());
}
constexpr year_month_day_last operator+(const years& amount,
                                         year_month_day_last value) noexcept {
    return value + amount;
}
constexpr year_month_day_last operator-(year_month_day_last value,
                                         const years& amount) noexcept {
    value -= amount;
    return value;
}
constexpr year_month_day_last operator+(year_month_day_last value,
                                         const months& amount) noexcept {
    value += amount;
    return value;
}
constexpr year_month_day_last operator+(const months& amount,
                                         year_month_day_last value) noexcept {
    return value + amount;
}
constexpr year_month_day_last operator-(year_month_day_last value,
                                         const months& amount) noexcept {
    value -= amount;
    return value;
}

class month_weekday {
    month month_;
    weekday_indexed weekday_indexed_;

public:
    constexpr month_weekday() noexcept : month_(), weekday_indexed_() {}
    constexpr month_weekday(const chrono::month& month_value,
                            const chrono::weekday_indexed& value) noexcept
        : month_(month_value), weekday_indexed_(value) {}
    constexpr month month() const noexcept { return month_; }
    constexpr weekday_indexed weekday_indexed() const noexcept { return weekday_indexed_; }
    constexpr bool ok() const noexcept { return month_.ok() && weekday_indexed_.ok(); }
};

constexpr bool operator==(month_weekday left,
                          month_weekday right) noexcept {
    return left.month() == right.month() &&
           left.weekday_indexed() == right.weekday_indexed();
}
constexpr bool operator!=(month_weekday left,
                          month_weekday right) noexcept {
    return !(left == right);
}
constexpr bool operator<(month_weekday left,
                         month_weekday right) noexcept {
    return left.month() < right.month() ||
           (left.month() == right.month() &&
            left.weekday_indexed() < right.weekday_indexed());
}
constexpr bool operator>(month_weekday left,
                         month_weekday right) noexcept {
    return right < left;
}
constexpr bool operator<=(month_weekday left,
                          month_weekday right) noexcept {
    return !(right < left);
}
constexpr bool operator>=(month_weekday left,
                          month_weekday right) noexcept {
    return !(left < right);
}
#if __cplusplus >= 202002L
constexpr strong_ordering operator<=>(month_weekday left,
                                      month_weekday right) noexcept {
    if (left.month() < right.month()) return strong_ordering::less;
    if (right.month() < left.month()) return strong_ordering::greater;
    return left.weekday_indexed() <=> right.weekday_indexed();
}
#endif

class month_weekday_last {
    month month_;
    weekday_last weekday_last_;

public:
    constexpr month_weekday_last() noexcept : month_(), weekday_last_() {}
    constexpr month_weekday_last(const chrono::month& month_value,
                                 const chrono::weekday_last& value) noexcept
        : month_(month_value), weekday_last_(value) {}
    constexpr month month() const noexcept { return month_; }
    constexpr weekday_last weekday_last() const noexcept { return weekday_last_; }
    constexpr bool ok() const noexcept { return month_.ok() && weekday_last_.ok(); }
};

constexpr bool operator==(month_weekday_last left,
                          month_weekday_last right) noexcept {
    return left.month() == right.month() &&
           left.weekday_last() == right.weekday_last();
}
constexpr bool operator!=(month_weekday_last left,
                          month_weekday_last right) noexcept {
    return !(left == right);
}
constexpr bool operator<(month_weekday_last left,
                         month_weekday_last right) noexcept {
    return left.month() < right.month() ||
           (left.month() == right.month() &&
            left.weekday_last() < right.weekday_last());
}
constexpr bool operator>(month_weekday_last left,
                         month_weekday_last right) noexcept {
    return right < left;
}
constexpr bool operator<=(month_weekday_last left,
                          month_weekday_last right) noexcept {
    return !(right < left);
}
constexpr bool operator>=(month_weekday_last left,
                          month_weekday_last right) noexcept {
    return !(left < right);
}
#if __cplusplus >= 202002L
constexpr strong_ordering operator<=>(month_weekday_last left,
                                      month_weekday_last right) noexcept {
    if (left.month() < right.month()) return strong_ordering::less;
    if (right.month() < left.month()) return strong_ordering::greater;
    return left.weekday_last() <=> right.weekday_last();
}
#endif

class year_month_weekday {
    year year_;
    month month_;
    weekday_indexed weekday_indexed_;

public:
    constexpr year_month_weekday() noexcept
        : year_(), month_(), weekday_indexed_() {}
    constexpr year_month_weekday(const chrono::year& year_value,
                                 const chrono::month& month_value,
                                 const chrono::weekday_indexed& value) noexcept
        : year_(year_value), month_(month_value), weekday_indexed_(value) {}
    constexpr explicit year_month_weekday(const sys_days& value) noexcept
        : year_month_weekday(year_month_day(value).year(),
                             year_month_day(value).month(),
                             chrono::weekday_indexed(weekday(value),
                                 static_cast<unsigned>(
                                     (year_month_day(value).day().value() - 1u) /
                                     7u + 1u))) {}
    constexpr year year() const noexcept { return year_; }
    constexpr month month() const noexcept { return month_; }
    constexpr weekday_indexed weekday_indexed() const noexcept { return weekday_indexed_; }
    constexpr bool ok() const noexcept {
        if (!year_.ok() || !month_.ok() || !weekday_indexed_.ok()) return false;
        const unsigned first = weekday(sys_days(days(chrono_detail::days_from_civil(
            year_.value(), month_.value(), 1u)))).c_encoding();
        const unsigned date = 1u + chrono_detail::positive_mod(
            weekday_indexed_.weekday().c_encoding() - first, 7u) +
            7u * (weekday_indexed_.index() - 1u);
        return date <= chrono_detail::days_in_month(year_.value(), month_.value());
    }
    constexpr year_month_weekday& operator+=(const months& amount) noexcept {
        const year_month shifted = year_month(year_, month_) + amount;
        year_ = shifted.year();
        month_ = shifted.month();
        return *this;
    }
    constexpr year_month_weekday& operator-=(const months& amount) noexcept {
        const year_month shifted = year_month(year_, month_) - amount;
        year_ = shifted.year();
        month_ = shifted.month();
        return *this;
    }
    constexpr year_month_weekday& operator+=(const years& amount) noexcept {
        year_ += amount;
        return *this;
    }
    constexpr year_month_weekday& operator-=(const years& amount) noexcept {
        year_ -= amount;
        return *this;
    }
    constexpr explicit operator sys_days() const noexcept {
        const unsigned first = weekday(sys_days(days(chrono_detail::days_from_civil(
            year_.value(), month_.value(), 1u)))).c_encoding();
        const unsigned date = 1u + chrono_detail::positive_mod(
            weekday_indexed_.weekday().c_encoding() - first, 7u) +
            7u * (weekday_indexed_.index() - 1u);
        return sys_days(days(chrono_detail::days_from_civil(
            year_.value(), month_.value(), date)));
    }
};

constexpr bool operator==(year_month_weekday left,
                          year_month_weekday right) noexcept {
    return left.year() == right.year() && left.month() == right.month() &&
           left.weekday_indexed().weekday() == right.weekday_indexed().weekday() &&
           left.weekday_indexed().index() == right.weekday_indexed().index();
}
constexpr bool operator!=(year_month_weekday left,
                          year_month_weekday right) noexcept { return !(left == right); }
constexpr bool operator<(year_month_weekday left,
                         year_month_weekday right) noexcept {
    return left.year() < right.year() ||
           (left.year() == right.year() &&
            (left.month() < right.month() ||
             (left.month() == right.month() &&
              left.weekday_indexed() < right.weekday_indexed())));
}
constexpr bool operator>(year_month_weekday left,
                         year_month_weekday right) noexcept {
    return right < left;
}
constexpr bool operator<=(year_month_weekday left,
                          year_month_weekday right) noexcept {
    return !(right < left);
}
constexpr bool operator>=(year_month_weekday left,
                          year_month_weekday right) noexcept {
    return !(left < right);
}
#if __cplusplus >= 202002L
constexpr strong_ordering operator<=>(year_month_weekday left,
                                      year_month_weekday right) noexcept {
    if (left.year() < right.year()) return strong_ordering::less;
    if (right.year() < left.year()) return strong_ordering::greater;
    if (left.month() < right.month()) return strong_ordering::less;
    if (right.month() < left.month()) return strong_ordering::greater;
    return left.weekday_indexed() <=> right.weekday_indexed();
}
#endif
constexpr year_month_weekday operator+(year_month_weekday value,
                                        const months& amount) noexcept {
    value += amount;
    return value;
}
constexpr year_month_weekday operator+(const months& amount,
                                        year_month_weekday value) noexcept {
    return value + amount;
}
constexpr year_month_weekday operator-(year_month_weekday value,
                                        const months& amount) noexcept {
    value -= amount;
    return value;
}
constexpr year_month_weekday operator+(year_month_weekday value,
                                        const years& amount) noexcept {
    value += amount;
    return value;
}
constexpr year_month_weekday operator+(const years& amount,
                                        year_month_weekday value) noexcept {
    return value + amount;
}
constexpr year_month_weekday operator-(year_month_weekday value,
                                        const years& amount) noexcept {
    value -= amount;
    return value;
}

class year_month_weekday_last {
    year year_;
    month month_;
    weekday_last weekday_last_;

public:
    constexpr year_month_weekday_last() noexcept
        : year_(), month_(), weekday_last_() {}
    constexpr year_month_weekday_last(const chrono::year& year_value,
                                      const chrono::month& month_value,
                                      const chrono::weekday_last& value) noexcept
        : year_(year_value), month_(month_value), weekday_last_(value) {}
    constexpr year year() const noexcept { return year_; }
    constexpr month month() const noexcept { return month_; }
    constexpr weekday_last weekday_last() const noexcept { return weekday_last_; }
    constexpr bool ok() const noexcept { return year_.ok() && month_.ok() && weekday_last_.ok(); }
    constexpr year_month_weekday_last& operator+=(const months& amount) noexcept {
        const year_month shifted = year_month(year_, month_) + amount;
        year_ = shifted.year();
        month_ = shifted.month();
        return *this;
    }
    constexpr year_month_weekday_last& operator-=(const months& amount) noexcept {
        const year_month shifted = year_month(year_, month_) - amount;
        year_ = shifted.year();
        month_ = shifted.month();
        return *this;
    }
    constexpr year_month_weekday_last& operator+=(const years& amount) noexcept {
        year_ += amount;
        return *this;
    }
    constexpr year_month_weekday_last& operator-=(const years& amount) noexcept {
        year_ -= amount;
        return *this;
    }
    constexpr explicit operator sys_days() const noexcept {
        const unsigned final_day = chrono_detail::days_in_month(
            year_.value(), month_.value());
        const sys_days end(days(chrono_detail::days_from_civil(
            year_.value(), month_.value(), final_day)));
        const unsigned end_weekday = weekday(end).c_encoding();
        const unsigned offset = chrono_detail::positive_mod(
            static_cast<long long>(end_weekday) -
                weekday_last_.weekday().c_encoding(), 7u);
        return sys_days(end.time_since_epoch() - days(offset));
    }
};

constexpr bool operator==(year_month_weekday_last left,
                          year_month_weekday_last right) noexcept {
    return left.year() == right.year() && left.month() == right.month() &&
           left.weekday_last().weekday() == right.weekday_last().weekday();
}
constexpr bool operator!=(year_month_weekday_last left,
                          year_month_weekday_last right) noexcept { return !(left == right); }
constexpr bool operator<(year_month_weekday_last left,
                         year_month_weekday_last right) noexcept {
    return left.year() < right.year() ||
           (left.year() == right.year() &&
            (left.month() < right.month() ||
             (left.month() == right.month() &&
              left.weekday_last() < right.weekday_last())));
}
constexpr bool operator>(year_month_weekday_last left,
                         year_month_weekday_last right) noexcept {
    return right < left;
}
constexpr bool operator<=(year_month_weekday_last left,
                          year_month_weekday_last right) noexcept {
    return !(right < left);
}
constexpr bool operator>=(year_month_weekday_last left,
                          year_month_weekday_last right) noexcept {
    return !(left < right);
}
#if __cplusplus >= 202002L
constexpr strong_ordering operator<=>(year_month_weekday_last left,
                                      year_month_weekday_last right) noexcept {
    if (left.year() < right.year()) return strong_ordering::less;
    if (right.year() < left.year()) return strong_ordering::greater;
    if (left.month() < right.month()) return strong_ordering::less;
    if (right.month() < left.month()) return strong_ordering::greater;
    return left.weekday_last() <=> right.weekday_last();
}
#endif
constexpr year_month_weekday_last operator+(year_month_weekday_last value,
                                             const years& amount) noexcept {
    value += amount;
    return value;
}
constexpr year_month_weekday_last operator+(
    const years& amount, year_month_weekday_last value) noexcept {
    return value + amount;
}
constexpr year_month_weekday_last operator-(year_month_weekday_last value,
                                             const years& amount) noexcept {
    value -= amount;
    return value;
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

constexpr year_month operator/(year value, month value2) noexcept { return year_month(value, value2); }
constexpr year_month operator/(year value, int month_value) noexcept {
    return year_month(value, month(static_cast<unsigned>(month_value)));
}
constexpr month_day operator/(month value, day value2) noexcept { return month_day(value, value2); }
constexpr month_day operator/(month value, int day_value) noexcept {
    return month_day(value, day(static_cast<unsigned>(day_value)));
}
constexpr month_day operator/(int month_value, day value) noexcept {
    return month_day(month(static_cast<unsigned>(month_value)), value);
}
constexpr month_day operator/(day value, month month_value) noexcept {
    return month_day(month_value, value);
}
constexpr month_day operator/(day value, int month_value) noexcept {
    return month_day(month(static_cast<unsigned>(month_value)), value);
}
constexpr month_day_last operator/(month value, last_spec) noexcept { return month_day_last(value); }
constexpr month_day_last operator/(int month_value, last_spec value) noexcept {
    (void)value;
    return month_day_last(month(static_cast<unsigned>(month_value)));
}
constexpr month_day_last operator/(last_spec value, month month_value) noexcept {
    (void)value;
    return month_day_last(month_value);
}
constexpr month_day_last operator/(last_spec value, int month_value) noexcept {
    (void)value;
    return month_day_last(month(static_cast<unsigned>(month_value)));
}
constexpr year_month_day operator/(year_month value, day value2) noexcept {
    return year_month_day(value.year(), value.month(), value2);
}
constexpr year_month_day operator/(year value, month_day value2) noexcept {
    return year_month_day(value, value2.month(), value2.day());
}
constexpr year_month_day operator/(int year_value, month_day value) noexcept {
    return year_month_day(year(static_cast<int>(year_value)),
                          value.month(), value.day());
}
constexpr year_month_day operator/(month_day value, int year_value) noexcept {
    return year_month_day(year(static_cast<int>(year_value)),
                          value.month(), value.day());
}
/* The calendar grammar is symmetric at the final composition step.  Keep
 * the reverse forms allocation-free so `May/31/2024` has the same checked
 * value construction as `2024/May/31`. */
constexpr year_month_day operator/(month_day value, year year_value) noexcept {
    return year_month_day(year_value, value.month(), value.day());
}
constexpr year_month_day_last operator/(year value, month_day_last value2) noexcept {
    return year_month_day_last(value, value2);
}
constexpr year_month_day_last operator/(int year_value,
                                        month_day_last value) noexcept {
    return year_month_day_last(year(static_cast<int>(year_value)), value);
}
constexpr year_month_day_last operator/(month_day_last value,
                                        int year_value) noexcept {
    return year_month_day_last(year(static_cast<int>(year_value)), value);
}
constexpr year_month_day_last operator/(month_day_last value,
                                        year year_value) noexcept {
    return year_month_day_last(year_value, value);
}
constexpr year_month_day_last operator/(year_month value, last_spec) noexcept {
    return year_month_day_last(value.year(), month_day_last(value.month()));
}
constexpr year_month_day operator/(year_month value, int day_value) noexcept {
    return year_month_day(value.year(), value.month(),
                          day(static_cast<unsigned>(day_value)));
}
constexpr month_weekday operator/(month value, weekday_indexed value2) noexcept {
    return month_weekday(value, value2);
}
constexpr month_weekday operator/(int month_value,
                                  weekday_indexed value) noexcept {
    return month_weekday(month(static_cast<unsigned>(month_value)), value);
}
constexpr month_weekday operator/(weekday_indexed value,
                                  month month_value) noexcept {
    return month_weekday(month_value, value);
}
constexpr month_weekday operator/(weekday_indexed value,
                                  int month_value) noexcept {
    return month_weekday(month(static_cast<unsigned>(month_value)), value);
}
constexpr month_weekday_last operator/(month value, weekday_last value2) noexcept {
    return month_weekday_last(value, value2);
}
constexpr month_weekday_last operator/(int month_value,
                                       weekday_last value) noexcept {
    return month_weekday_last(month(static_cast<unsigned>(month_value)), value);
}
constexpr month_weekday_last operator/(weekday_last value,
                                       month month_value) noexcept {
    return month_weekday_last(month_value, value);
}
constexpr month_weekday_last operator/(weekday_last value,
                                       int month_value) noexcept {
    return month_weekday_last(month(static_cast<unsigned>(month_value)), value);
}
constexpr year_month_weekday operator/(year_month value,
                                       weekday_indexed value2) noexcept {
    return year_month_weekday(value.year(), value.month(), value2);
}
constexpr year_month_weekday operator/(year value,
                                       month_weekday value2) noexcept {
    return year_month_weekday(value, value2.month(),
                              value2.weekday_indexed());
}
constexpr year_month_weekday operator/(int year_value,
                                       month_weekday value) noexcept {
    return year_month_weekday(year(static_cast<int>(year_value)),
                              value.month(), value.weekday_indexed());
}
constexpr year_month_weekday operator/(month_weekday value,
                                       year year_value) noexcept {
    return year_month_weekday(year_value, value.month(),
                              value.weekday_indexed());
}
constexpr year_month_weekday operator/(month_weekday value,
                                       int year_value) noexcept {
    return year_month_weekday(year(static_cast<int>(year_value)),
                              value.month(), value.weekday_indexed());
}
constexpr year_month_weekday_last operator/(year_month value,
                                            weekday_last value2) noexcept {
    return year_month_weekday_last(value.year(), value.month(), value2);
}
constexpr year_month_weekday_last operator/(year value,
                                            month_weekday_last value2) noexcept {
    return year_month_weekday_last(value, value2.month(),
                                   value2.weekday_last());
}
constexpr year_month_weekday_last operator/(int year_value,
                                            month_weekday_last value) noexcept {
    return year_month_weekday_last(year(static_cast<int>(year_value)),
                                   value.month(), value.weekday_last());
}
constexpr year_month_weekday_last operator/(month_weekday_last value,
                                            year year_value) noexcept {
    return year_month_weekday_last(year_value, value.month(),
                                   value.weekday_last());
}
constexpr year_month_weekday_last operator/(month_weekday_last value,
                                            int year_value) noexcept {
    return year_month_weekday_last(year(static_cast<int>(year_value)),
                                   value.month(), value.weekday_last());
}

constexpr year_month_weekday_last operator+(
    year_month_weekday_last value, const months& amount) noexcept {
    const year_month shifted = year_month(value.year(), value.month()) + amount;
    return year_month_weekday_last(shifted.year(), shifted.month(),
                                   value.weekday_last());
}
constexpr year_month_weekday_last operator+(
    const months& amount, year_month_weekday_last value) noexcept {
    return value + amount;
}
constexpr year_month_weekday_last operator-(
    year_month_weekday_last value, const months& amount) noexcept {
    value -= amount;
    return value;
}

#endif /* C++20 calendar */

/* ═══════════════════════════════════════════════════════════════
 * floor, ceil, round, abs (C++17)
 * ═══════════════════════════════════════════════════════════════*/

#if __cplusplus >= 201703L
namespace chrono_detail {

template<typename Rep>
constexpr bool round_tie_is_odd(const Rep& value, true_type) {
    return (value % Rep(2)) != Rep(0);
}

template<typename Rep>
constexpr bool round_tie_is_odd(const Rep&, false_type) {
    /* A fractional representation has no integral parity; ties therefore
     * retain the lower value, which is already the exact converted value. */
    return false;
}

template<typename Rep>
constexpr bool round_tie_is_odd(const Rep& value) {
    return round_tie_is_odd(value, integral_constant<bool,
        is_integral<Rep>::value>());
}

} /* namespace chrono_detail */

template<typename ToDuration, typename Rep, typename Period>
constexpr ToDuration floor(const duration<Rep, Period>& d) {
    ToDuration t = duration_cast<ToDuration>(d);
    if (t > d) {
        t = ToDuration(chrono_detail::subtract_rep(
            t.count(), typename ToDuration::rep(1)));
    }
    return t;
}

template<typename ToDuration, typename Rep, typename Period>
constexpr ToDuration ceil(const duration<Rep, Period>& d) {
    ToDuration t = duration_cast<ToDuration>(d);
    if (t < d) {
        t = ToDuration(chrono_detail::add_rep(
            t.count(), typename ToDuration::rep(1)));
    }
    return t;
}

template<typename ToDuration, typename Rep, typename Period,
         typename enable_if<
             !treat_as_floating_point<typename ToDuration::rep>::value,
             int>::type = 0>
constexpr ToDuration round(const duration<Rep, Period>& d) {
    ToDuration t0 = floor<ToDuration>(d);
    ToDuration t1(chrono_detail::add_rep(
        t0.count(), typename ToDuration::rep(1)));
    if (t1 == t0) return t0;
    auto d0 = d - t0;
    auto d1 = t1 - d;
    if (d0 == d1) {
        if (chrono_detail::round_tie_is_odd(t0.count())) return t1;
        return t0;
    } else if (d0 < d1) {
        return t0;
    }
    return t1;
}

template<typename Rep, typename Period>
constexpr duration<Rep, Period> abs(duration<Rep, Period> d) {
    return d >= d.zero() ? d : -d;
}
#endif

} /* namespace chrono */

/* ═══════════════════════════════════════════════════════════════
 * chrono_literals (C++14)
 * ═══════════════════════════════════════════════════════════════*/

#if __cplusplus >= 201402L
inline namespace literals {
inline namespace chrono_literals {

constexpr chrono::hours operator""_h(unsigned long long h) {
    return chrono::hours(static_cast<chrono::hours::rep>(h));
}

constexpr chrono::minutes operator""_min(unsigned long long m) {
    return chrono::minutes(static_cast<chrono::minutes::rep>(m));
}

constexpr chrono::seconds operator""_s(unsigned long long s) {
    return chrono::seconds(static_cast<chrono::seconds::rep>(s));
}

constexpr chrono::milliseconds operator""_ms(unsigned long long ms) {
    return chrono::milliseconds(static_cast<chrono::milliseconds::rep>(ms));
}

constexpr chrono::microseconds operator""_us(unsigned long long us) {
    return chrono::microseconds(static_cast<chrono::microseconds::rep>(us));
}

constexpr chrono::nanoseconds operator""_ns(unsigned long long ns) {
    return chrono::nanoseconds(static_cast<chrono::nanoseconds::rep>(ns));
}

#if __cplusplus >= 202002L
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wliteral-suffix"
#endif
constexpr chrono::year operator""y(unsigned long long value) {
    return chrono::year(static_cast<int>(value));
}

constexpr chrono::day operator""d(unsigned long long value) {
    return chrono::day(static_cast<unsigned>(value));
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
#endif

} /* namespace chrono_literals */
} /* namespace literals */
#endif

namespace chrono {
#if __cplusplus >= 201703L
template<typename ToDuration, typename Clock, typename Duration>
constexpr time_point<Clock, ToDuration>
floor(const time_point<Clock, Duration>& tp) {
    return time_point<Clock, ToDuration>(
        floor<ToDuration>(tp.time_since_epoch()));
}

template<typename ToDuration, typename Clock, typename Duration>
constexpr time_point<Clock, ToDuration>
ceil(const time_point<Clock, Duration>& tp) {
    return time_point<Clock, ToDuration>(
        ceil<ToDuration>(tp.time_since_epoch()));
}

template<typename ToDuration, typename Clock, typename Duration,
         typename enable_if<
             !treat_as_floating_point<typename ToDuration::rep>::value,
             int>::type = 0>
constexpr time_point<Clock, ToDuration>
round(const time_point<Clock, Duration>& tp) {
    return time_point<Clock, ToDuration>(
        round<ToDuration>(tp.time_since_epoch()));
}
#endif
} /* namespace chrono */

} /* namespace std */

#undef RIN_CHRONO_CONSTEXPR14

#endif /* __cplusplus */
#endif /* RINCXX_CHRONO_H */
