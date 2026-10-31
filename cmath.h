/*
 * RinOS C++ <cmath> ✿
 * 数学関数 - libc/math.hのラッパー
 */

#ifndef RINCXX_CMATH_H
#define RINCXX_CMATH_H

#include "rincxx.h"
#include "type_traits.h"
#include "__nextafter.h"

/* libc/math.hの関数を使用 */
#include "../libc/math.h"
#include "ctime.h"           /* retained for C++ consumer include compatibility */

/* ═══════════════════════════════════════════════════════════════
 * 追加の数学定数
 * ═══════════════════════════════════════════════════════════════*/

#ifndef M_LOG2E
#define M_LOG2E    1.44269504088896340736
#endif

#ifndef M_LOG10E
#define M_LOG10E   0.43429448190325182765
#endif

#ifndef M_LN2
#define M_LN2      0.69314718055994530942
#endif

#ifndef M_LN10
#define M_LN10     2.30258509299404568402
#endif

#ifndef M_PI_2
#define M_PI_2     1.57079632679489661923
#endif

#ifndef M_PI_4
#define M_PI_4     0.78539816339744830962
#endif

#ifndef M_1_PI
#define M_1_PI     0.31830988618379067154
#endif

#ifndef M_2_PI
#define M_2_PI     0.63661977236758134308
#endif

#ifndef M_2_SQRTPI
#define M_2_SQRTPI 1.12837916709551257390
#endif

#ifndef M_SQRT2
#define M_SQRT2    1.41421356237309504880
#endif

#ifndef M_SQRT1_2
#define M_SQRT1_2  0.70710678118654752440
#endif

#ifdef __cplusplus

/* ═══════════════════════════════════════════════════════════════
 * C++名前空間版 - libc/math.hの関数をstd::に取り込む
 * ═══════════════════════════════════════════════════════════════*/

namespace std {

/* 絶対値 */
using ::fabs;
using ::fabsf;
inline long double fabs(long double x) {
    return __builtin_fabsl(x);
}

/* 丸め関数 */
using ::floor;
using ::floorf;
inline long double floor(long double x) { return ::floorl(x); }
using ::ceil;
using ::ceilf;
inline long double ceil(long double x) { return ::ceill(x); }
using ::round;
using ::roundf;
inline long double round(long double x) { return ::roundl(x); }
using ::trunc;
using ::truncf;
inline long double trunc(long double x) { return ::truncl(x); }
using ::rint;
using ::rintf;
using ::rintl;
using ::nearbyint;
using ::nearbyintf;
using ::nearbyintl;
inline long double rint(long double x) { return __builtin_rintl(x); }
inline long double nearbyint(long double x) { return __builtin_nearbyintl(x); }

/* 剰余 */
using ::fmod;
using ::fmodf;
inline long double fmod(long double x, long double y) {
    return __builtin_fmodl(x, y);
}

/* 平方根・累乗 */
using ::sqrt;
using ::sqrtf;
inline long double sqrt(long double x) {
    return __builtin_sqrtl(x);
}
using ::cbrt;
using ::cbrtf;
inline long double cbrt(long double x) {
    return __builtin_cbrtl(x);
}
using ::pow;
using ::powf;
inline long double pow(long double base, long double exponent) {
    return __builtin_powl(base, exponent);
}

/* 対数・指数 */
using ::log;
using ::logf;
using ::log10;
using ::log10f;
using ::log2;
using ::log2f;
using ::exp;
using ::expf;
using ::exp2;
using ::exp2f;
using ::expm1;
using ::expm1f;
using ::log1p;
using ::log1pf;
inline long double exp2(long double x) { return __builtin_exp2l(x); }
inline long double expm1(long double x) { return __builtin_expm1l(x); }
inline long double log1p(long double x) { return __builtin_log1pl(x); }
inline long double log(long double x) { return __builtin_logl(x); }
inline long double log10(long double x) { return __builtin_log10l(x); }
inline long double log2(long double x) { return __builtin_log2l(x); }
inline long double exp(long double x) { return __builtin_expl(x); }

/* 三角関数 */
using ::sin;
using ::sinf;
using ::cos;
using ::cosf;
using ::tan;
using ::tanf;
using ::asin;
using ::asinf;
using ::acos;
using ::acosf;
using ::atan;
using ::atanf;
using ::atan2;
using ::atan2f;
inline long double atan(long double x) { return __builtin_atanl(x); }
inline long double atan2(long double y, long double x) {
    return __builtin_atan2l(y, x);
}
inline long double sin(long double x) { return __builtin_sinl(x); }
inline long double cos(long double x) { return __builtin_cosl(x); }
inline long double tan(long double x) { return __builtin_tanl(x); }
inline long double asin(long double x) { return __builtin_asinl(x); }
inline long double acos(long double x) { return __builtin_acosl(x); }

/* 双曲線関数 */
using ::sinh;
using ::sinhf;
using ::cosh;
using ::coshf;
using ::tanh;
using ::tanhf;
using ::asinh;
using ::asinhf;
using ::acosh;
using ::acoshf;
using ::atanh;
using ::atanhf;
inline long double asinh(long double x) { return __builtin_asinhl(x); }
inline long double acosh(long double x) { return __builtin_acoshl(x); }
inline long double atanh(long double x) { return __builtin_atanhl(x); }
inline long double sinh(long double x) { return __builtin_sinhl(x); }
inline long double cosh(long double x) { return __builtin_coshl(x); }
inline long double tanh(long double x) { return __builtin_tanhl(x); }

/* その他 */
using ::hypot;
using ::hypotf;
using ::copysign;
using ::copysignf;
inline long double copysign(long double magnitude, long double sign) {
    return __builtin_copysignl(magnitude, sign);
}
using ::fmax;
using ::fmaxf;
inline long double fmax(long double x, long double y) {
    return __builtin_fmaxl(x, y);
}
using ::fmin;
using ::fminf;
inline long double fmin(long double x, long double y) {
    return __builtin_fminl(x, y);
}
using ::fdim;
using ::fdimf;
using ::remainder;
using ::remainderf;
using ::fma;
using ::fmaf;
using ::lround;
using ::lroundf;
using ::llround;
using ::llroundf;
using ::lrint;
using ::lrintf;
using ::llrint;
inline long double hypot(long double x, long double y) {
    return __builtin_hypotl(x, y);
}
inline long double fdim(long double x, long double y) {
    return __builtin_fdiml(x, y);
}
inline long double remainder(long double x, long double y) {
    return __builtin_remainderl(x, y);
}
inline long double fma(long double x, long double y, long double z) {
    return __builtin_fmal(x, y, z);
}

/* Error/gamma functions.  The C math header intentionally keeps the
 * freestanding surface small; expose the standard C++ overload set directly
 * through compiler builtins so binary32/binary64 callers do not narrow via a
 * double temporary and binary80 callers retain their active ABI. */
inline float erf(float x) { return __builtin_erff(x); }
inline double erf(double x) { return __builtin_erf(x); }
inline long double erf(long double x) { return __builtin_erfl(x); }
inline float erfc(float x) { return __builtin_erfcf(x); }
inline double erfc(double x) { return __builtin_erfc(x); }
inline long double erfc(long double x) { return __builtin_erfcl(x); }
inline float lgamma(float x) { return __builtin_lgammaf(x); }
inline double lgamma(double x) { return __builtin_lgamma(x); }
inline long double lgamma(long double x) { return __builtin_lgammal(x); }
inline float tgamma(float x) { return __builtin_tgammaf(x); }
inline double tgamma(double x) { return __builtin_tgamma(x); }
inline long double tgamma(long double x) { return __builtin_tgammal(x); }

/* Exponent decomposition and scaled exponentiation preserve the standard
 * return type/argument width.  `ilogb` returns the implementation-defined
 * FP_ILOGB* sentinel for zero/NaN, exactly as the compiler builtin does. */
inline int ilogb(float x) { return __builtin_ilogbf(x); }
inline int ilogb(double x) { return __builtin_ilogb(x); }
inline int ilogb(long double x) { return __builtin_ilogbl(x); }
inline float logb(float x) { return __builtin_logbf(x); }
inline double logb(double x) { return __builtin_logb(x); }
inline long double logb(long double x) { return __builtin_logbl(x); }
inline float scalbln(float x, long n) { return __builtin_scalblnf(x, n); }
inline double scalbln(double x, long n) { return __builtin_scalbln(x, n); }
inline long double scalbln(long double x, long n) {
    return __builtin_scalblnl(x, n);
}

/* `remquo` writes only the low implementation-defined quotient bits.  The
 * destination pointer is owned by the caller, matching the C contract; the
 * builtin performs the required zero-divisor/NaN handling. */
inline float remquo(float x, float y, int* quo) {
    return __builtin_remquof(x, y, quo);
}
inline double remquo(double x, double y, int* quo) {
    return __builtin_remquo(x, y, quo);
}
inline long double remquo(long double x, long double y, int* quo) {
    return __builtin_remquol(x, y, quo);
}

/* C++ exposes the C NaN factories in std as overloads.  The payload string is
 * interpreted by the active compiler/runtime and is never copied by this
 * allocation-free wrapper. */
inline float nanf(const char* tag) { return __builtin_nanf(tag); }
inline double nan(const char* tag) { return __builtin_nan(tag); }
inline long double nanl(const char* tag) { return __builtin_nanl(tag); }
using ::frexp;
using ::ldexp;
using ::scalbn;
using ::scalbnf;

inline long double frexp(long double x, int* exp) {
    return __builtin_frexpl(x, exp);
}
inline long double ldexp(long double x, int exp) {
    return __builtin_ldexpl(x, exp);
}
inline long double scalbn(long double x, int exp) {
    return __builtin_scalbnl(x, exp);
}

/* modf - C++オーバーロード */
using ::modf;    /* double版 */
using ::modff;   /* float版 */

inline float modf(float x, float* iptr) { return modff(x, iptr); }
inline long double modf(long double x, long double* iptr) {
    return __builtin_modfl(x, iptr);
}

/* 浮動小数点分類関数 - IEEE 754 完全実装 */

/* isnan - NaN判定 */
inline bool isnan(float x) {
    union { float f; unsigned int i; } u;
    u.f = x;
    unsigned int exp = (u.i >> 23) & 0xFF;
    unsigned int frac = u.i & 0x7FFFFF;
    return (exp == 0xFF) && (frac != 0);
}

inline bool isnan(double x) {
    union { double d; unsigned long long i; } u;
    u.d = x;
    unsigned long long exp = (u.i >> 52) & 0x7FF;
    unsigned long long frac = u.i & 0xFFFFFFFFFFFFFULL;
    return (exp == 0x7FF) && (frac != 0);
}

inline bool isnan(long double x) {
    /* A binary80/binary128 value may be finite while its double conversion
     * overflows to infinity.  Do not classify long double through a narrower
     * type; the compiler builtin inspects the active ABI directly. */
    return __builtin_isnan(x) != 0;
}

/* isinf - 無限大判定 */
inline bool isinf(float x) {
    union { float f; unsigned int i; } u;
    u.f = x;
    unsigned int exp = (u.i >> 23) & 0xFF;
    unsigned int frac = u.i & 0x7FFFFF;
    return (exp == 0xFF) && (frac == 0);
}

inline bool isinf(double x) {
    union { double d; unsigned long long i; } u;
    u.d = x;
    unsigned long long exp = (u.i >> 52) & 0x7FF;
    unsigned long long frac = u.i & 0xFFFFFFFFFFFFFULL;
    return (exp == 0x7FF) && (frac == 0);
}

inline bool isinf(long double x) {
    return __builtin_isinf(x) != 0;
}

/* isfinite - 有限数判定 */
inline bool isfinite(float x) {
    union { float f; unsigned int i; } u;
    u.f = x;
    unsigned int exp = (u.i >> 23) & 0xFF;
    return exp != 0xFF;
}

inline bool isfinite(double x) {
    union { double d; unsigned long long i; } u;
    u.d = x;
    unsigned long long exp = (u.i >> 52) & 0x7FF;
    return exp != 0x7FF;
}

inline bool isfinite(long double x) {
    return __builtin_isfinite(x) != 0;
}

/* isnormal - 正規化数判定 */
inline bool isnormal(float x) {
    union { float f; unsigned int i; } u;
    u.f = x;
    unsigned int exp = (u.i >> 23) & 0xFF;
    return (exp != 0) && (exp != 0xFF);
}

inline bool isnormal(double x) {
    union { double d; unsigned long long i; } u;
    u.d = x;
    unsigned long long exp = (u.i >> 52) & 0x7FF;
    return (exp != 0) && (exp != 0x7FF);
}

inline bool isnormal(long double x) {
    return __builtin_isnormal(x) != 0;
}

/* signbit - ビット演算による実装 */
inline bool signbit(float x) {
    union { float f; unsigned int i; } u;
    u.f = x;
    return (u.i >> 31) != 0;
}

inline bool signbit(double x) {
    union { double d; unsigned long long i; } u;
    u.d = x;
    return (u.i >> 63) != 0;
}

inline bool signbit(long double x) {
    return __builtin_signbit(x) != 0;
}

/* C99 floating-point relational macros are C++ overloads in <cmath>.  Keep
 * NaN handling explicit so the predicates never accidentally report an
 * ordering for unordered operands, while integral arguments remain valid
 * arithmetic inputs and are always ordered. */
namespace cmath_detail {

template<typename T>
inline bool relational_isnan(const T&, false_type) noexcept {
    return false;
}

template<typename T>
inline bool relational_isnan(const T& value, true_type) noexcept {
    return std::isnan(value);
}

template<typename T>
inline bool relational_isnan(const T& value) noexcept {
    return relational_isnan(value,
        integral_constant<bool, is_floating_point<T>::value>());
}

template<typename T, typename U>
using relational_common_t = typename common_type<T, U>::type;

} /* namespace cmath_detail */

template<typename T, typename U>
inline bool isunordered(T left, U right) noexcept {
    using Common = cmath_detail::relational_common_t<T, U>;
    const Common lhs = static_cast<Common>(left);
    const Common rhs = static_cast<Common>(right);
    return cmath_detail::relational_isnan(lhs) ||
           cmath_detail::relational_isnan(rhs);
}

template<typename T, typename U>
inline bool isgreater(T left, U right) noexcept {
    using Common = cmath_detail::relational_common_t<T, U>;
    const Common lhs = static_cast<Common>(left);
    const Common rhs = static_cast<Common>(right);
    return !isunordered(lhs, rhs) && lhs > rhs;
}

template<typename T, typename U>
inline bool isgreaterequal(T left, U right) noexcept {
    using Common = cmath_detail::relational_common_t<T, U>;
    const Common lhs = static_cast<Common>(left);
    const Common rhs = static_cast<Common>(right);
    return !isunordered(lhs, rhs) && lhs >= rhs;
}

template<typename T, typename U>
inline bool isless(T left, U right) noexcept {
    using Common = cmath_detail::relational_common_t<T, U>;
    const Common lhs = static_cast<Common>(left);
    const Common rhs = static_cast<Common>(right);
    return !isunordered(lhs, rhs) && lhs < rhs;
}

template<typename T, typename U>
inline bool islessequal(T left, U right) noexcept {
    using Common = cmath_detail::relational_common_t<T, U>;
    const Common lhs = static_cast<Common>(left);
    const Common rhs = static_cast<Common>(right);
    return !isunordered(lhs, rhs) && lhs <= rhs;
}

template<typename T, typename U>
inline bool islessgreater(T left, U right) noexcept {
    using Common = cmath_detail::relational_common_t<T, U>;
    const Common lhs = static_cast<Common>(left);
    const Common rhs = static_cast<Common>(right);
    return !isunordered(lhs, rhs) && (lhs < rhs || lhs > rhs);
}

/* fpclassify - 浮動小数点数の分類 (IEEE 754 完全実装) */
#ifndef FP_NAN
#define FP_NAN       0
#endif
#ifndef FP_INFINITE
#define FP_INFINITE  1
#endif
#ifndef FP_ZERO
#define FP_ZERO      2
#endif
#ifndef FP_SUBNORMAL
#define FP_SUBNORMAL 3
#endif
#ifndef FP_NORMAL
#define FP_NORMAL    4
#endif

inline int fpclassify(float x) {
    union { float f; unsigned int i; } u;
    u.f = x;
    unsigned int exp = (u.i >> 23) & 0xFF;
    unsigned int frac = u.i & 0x7FFFFF;

    if (exp == 0xFF) {
        return (frac == 0) ? FP_INFINITE : FP_NAN;
    } else if (exp == 0) {
        return (frac == 0) ? FP_ZERO : FP_SUBNORMAL;
    }
    return FP_NORMAL;
}

inline int fpclassify(double x) {
    union { double d; unsigned long long i; } u;
    u.d = x;
    unsigned long long exp = (u.i >> 52) & 0x7FF;
    unsigned long long frac = u.i & 0xFFFFFFFFFFFFFULL;

    if (exp == 0x7FF) {
        return (frac == 0) ? FP_INFINITE : FP_NAN;
    } else if (exp == 0) {
        return (frac == 0) ? FP_ZERO : FP_SUBNORMAL;
    }
    return FP_NORMAL;
}

inline int fpclassify(long double x) {
    return __builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL,
                                FP_SUBNORMAL, FP_ZERO, x);
}

/* nextafter - 次の表現可能な浮動小数点値 */
inline float nextafter(float from, float to) {
    if (from == to) return to;
    if (isnan(from) || isnan(to)) return from + to; // NaN propagation

    union { float f; unsigned int i; } uf;
    uf.f = from;

    if (from == 0.0f) {
        // from is zero, return smallest denormal toward to
        uf.i = 1;
        return (to > 0.0f) ? uf.f : -uf.f;
    }

    if ((from > 0.0f) == (to > from)) {
        uf.i++;
    } else {
        uf.i--;
    }
    return uf.f;
}

inline double nextafter(double from, double to) {
    if (from == to) return to;
    if (isnan(from) || isnan(to)) return from + to; // NaN propagation

    union { double d; unsigned long long i; } uf;
    uf.d = from;

    if (from == 0.0) {
        // from is zero, return smallest denormal toward to
        uf.i = 1;
        return (to > 0.0) ? uf.d : -uf.d;
    }

    if ((from > 0.0) == (to > from)) {
        uf.i++;
    } else {
        uf.i--;
    }
    return uf.d;
}

inline long double nextafter(long double from, long double to) {
    return __detail::__nextafter(from, to);
}

/* nexttoward keeps the direction in the wider long-double domain.  A plain
 * cast to float/double can round an in-between target back to the source and
 * would therefore incorrectly report no movement. */
inline float nexttoward(float from, long double to) {
    if (from == to) return static_cast<float>(to);
    if (isnan(from) || isnan(to)) return from + static_cast<float>(to);
    return nextafter(from, to > static_cast<long double>(from)
                              ? __builtin_huge_valf()
                              : -__builtin_huge_valf());
}

inline double nexttoward(double from, long double to) {
    if (from == to) return static_cast<double>(to);
    if (isnan(from) || isnan(to)) return from + static_cast<double>(to);
    return nextafter(double(from), to > static_cast<long double>(from)
                                  ? __builtin_huge_val()
                                  : -__builtin_huge_val());
}

inline long double nexttoward(long double from, long double to) {
    return nextafter(from, to);
}

/* Also expose the C spelling in the std namespace.  The C header provides
 * `nextafterf`; the long-double and nexttoward spellings are routed through
 * the same ABI-aware overloads so callers do not accidentally narrow them. */
inline float nextafterf(float from, float to) {
    return nextafter(from, to);
}

inline long double nextafterl(long double from, long double to) {
    return nextafter(from, to);
}

inline float nexttowardf(float from, long double to) {
    return nexttoward(from, to);
}

inline long double nexttowardl(long double from, long double to) {
    return nexttoward(from, to);
}

/* abs - all overloads.  A hosted C++ runtime may have populated these
 * overloads through <assert.h> -> <cstdlib> before Rin <cmath> is included.
 * Defining a second std::abs set is ill-formed, so reuse that host owner in
 * the same include-order case while keeping the independent freestanding
 * implementation below. */
#if !defined(RIN_FREESTANDING) && defined(__STDC_HOSTED__) && \
    __STDC_HOSTED__ && defined(_GLIBCXX_CSTDLIB)
#define RINCXX_CMATH_HOST_ABS 1
#endif
#if !defined(RINCXX_CMATH_HOST_ABS) && !defined(RINCXX_ABS_DEFINED)
#define RINCXX_ABS_DEFINED 1
inline int abs(int x) {
    if (x >= 0) return x;
    return static_cast<int>(0u - static_cast<unsigned int>(x));
}
inline long abs(long x) {
    if (x >= 0) return x;
    return static_cast<long>(0UL - static_cast<unsigned long>(x));
}
inline long long abs(long long x) {
    if (x >= 0) return x;
    return static_cast<long long>(0ULL - static_cast<unsigned long long>(x));
}
inline float abs(float x) { return ::fabsf(x); }
inline double abs(double x) { return ::fabs(x); }
inline long double fabsl(long double x) { return x < 0 ? -x : x; }
inline long double abs(long double x) { return fabsl(x); }
#endif

/* 線形補間 (C++20) */
#if __cplusplus >= 202002L
namespace cmath_detail {

template<typename Float>
constexpr Float lerp_impl(Float a, Float b, Float t) noexcept {
    if ((a <= Float(0) && b >= Float(0)) ||
        (a >= Float(0) && b <= Float(0))) {
        return t * b + (Float(1) - t) * a;
    }
    if (t == Float(1)) return b;
    const Float candidate = a + t * (b - a);
    if ((t > Float(1)) == (b > a))
        return candidate < b ? b : candidate;
    return candidate > b ? b : candidate;
}

} /* namespace cmath_detail */

constexpr float lerp(float a, float b, float t) noexcept {
    return cmath_detail::lerp_impl(a, b, t);
}

constexpr double lerp(double a, double b, double t) noexcept {
    return cmath_detail::lerp_impl(a, b, t);
}

constexpr long double lerp(long double a, long double b,
                           long double t) noexcept {
    return cmath_detail::lerp_impl(a, b, t);
}
#endif

} /* namespace std */

#endif /* __cplusplus */
#endif /* RINCXX_CMATH_H */
