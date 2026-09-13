/*
 * RinOS C++ <compare>
 * Three-way comparison (C++20)
 */

#ifndef RINCXX_COMPARE_H
#define RINCXX_COMPARE_H

#include "rincxx.h"
#include "version.h"
#include "type_traits.h"
#include "pointer_order.h"
#include "concepts.h"

#ifdef __cplusplus

#if __cplusplus >= 202002L
namespace std {

namespace detail {

using comparison_value = signed char;

enum class ordered_value : comparison_value {
    less = -1,
    equivalent = 0,
    greater = 1
};

enum class unordered_value : comparison_value {
    unordered = 2
};

/* Only a null pointer constant can form this exposition-only zero type. */
struct comparison_zero {
    consteval comparison_zero(comparison_zero*) noexcept {}
};

} /* namespace detail */

class weak_ordering;
class strong_ordering;

class partial_ordering {
    detail::comparison_value value_;

    constexpr explicit partial_ordering(detail::ordered_value value) noexcept
        : value_(static_cast<detail::comparison_value>(value)) {}
    constexpr explicit partial_ordering(detail::unordered_value value) noexcept
        : value_(static_cast<detail::comparison_value>(value)) {}

    friend class weak_ordering;
    friend class strong_ordering;

public:
    static const partial_ordering less;
    static const partial_ordering equivalent;
    static const partial_ordering greater;
    static const partial_ordering unordered;

    [[nodiscard]] friend constexpr bool
    operator==(partial_ordering value, detail::comparison_zero) noexcept {
        return value.value_ == 0;
    }

    [[nodiscard]] friend constexpr bool
    operator==(partial_ordering, partial_ordering) noexcept = default;

    [[nodiscard]] friend constexpr bool
    operator<(partial_ordering value, detail::comparison_zero) noexcept {
        return value.value_ == -1;
    }

    [[nodiscard]] friend constexpr bool
    operator>(partial_ordering value, detail::comparison_zero) noexcept {
        return value.value_ == 1;
    }

    [[nodiscard]] friend constexpr bool
    operator<=(partial_ordering value, detail::comparison_zero) noexcept {
        return value.value_ == -1 || value.value_ == 0;
    }

    [[nodiscard]] friend constexpr bool
    operator>=(partial_ordering value, detail::comparison_zero) noexcept {
        return value.value_ == 0 || value.value_ == 1;
    }

    [[nodiscard]] friend constexpr bool
    operator<(detail::comparison_zero, partial_ordering value) noexcept {
        return value.value_ == 1;
    }

    [[nodiscard]] friend constexpr bool
    operator>(detail::comparison_zero, partial_ordering value) noexcept {
        return value.value_ == -1;
    }

    [[nodiscard]] friend constexpr bool
    operator<=(detail::comparison_zero, partial_ordering value) noexcept {
        return value.value_ == 0 || value.value_ == 1;
    }

    [[nodiscard]] friend constexpr bool
    operator>=(detail::comparison_zero, partial_ordering value) noexcept {
        return value.value_ == -1 || value.value_ == 0;
    }

    [[nodiscard]] friend constexpr partial_ordering
    operator<=>(partial_ordering value, detail::comparison_zero) noexcept {
        return value;
    }

    [[nodiscard]] friend constexpr partial_ordering
    operator<=>(detail::comparison_zero, partial_ordering value) noexcept {
        if (value.value_ == -1)
            return partial_ordering::greater;
        if (value.value_ == 1)
            return partial_ordering::less;
        return value;
    }
};

inline constexpr partial_ordering partial_ordering::less{
    detail::ordered_value::less};
inline constexpr partial_ordering partial_ordering::equivalent{
    detail::ordered_value::equivalent};
inline constexpr partial_ordering partial_ordering::greater{
    detail::ordered_value::greater};
inline constexpr partial_ordering partial_ordering::unordered{
    detail::unordered_value::unordered};

class weak_ordering {
    detail::comparison_value value_;

    constexpr explicit weak_ordering(detail::ordered_value value) noexcept
        : value_(static_cast<detail::comparison_value>(value)) {}

    friend class strong_ordering;

public:
    static const weak_ordering less;
    static const weak_ordering equivalent;
    static const weak_ordering greater;

    [[nodiscard]] constexpr operator partial_ordering() const noexcept {
        return partial_ordering(static_cast<detail::ordered_value>(value_));
    }

    [[nodiscard]] friend constexpr bool
    operator==(weak_ordering value, detail::comparison_zero) noexcept {
        return value.value_ == 0;
    }

    [[nodiscard]] friend constexpr bool
    operator==(weak_ordering, weak_ordering) noexcept = default;

    [[nodiscard]] friend constexpr bool
    operator<(weak_ordering value, detail::comparison_zero) noexcept {
        return value.value_ < 0;
    }

    [[nodiscard]] friend constexpr bool
    operator>(weak_ordering value, detail::comparison_zero) noexcept {
        return value.value_ > 0;
    }

    [[nodiscard]] friend constexpr bool
    operator<=(weak_ordering value, detail::comparison_zero) noexcept {
        return value.value_ <= 0;
    }

    [[nodiscard]] friend constexpr bool
    operator>=(weak_ordering value, detail::comparison_zero) noexcept {
        return value.value_ >= 0;
    }

    [[nodiscard]] friend constexpr bool
    operator<(detail::comparison_zero, weak_ordering value) noexcept {
        return value.value_ > 0;
    }

    [[nodiscard]] friend constexpr bool
    operator>(detail::comparison_zero, weak_ordering value) noexcept {
        return value.value_ < 0;
    }

    [[nodiscard]] friend constexpr bool
    operator<=(detail::comparison_zero, weak_ordering value) noexcept {
        return value.value_ >= 0;
    }

    [[nodiscard]] friend constexpr bool
    operator>=(detail::comparison_zero, weak_ordering value) noexcept {
        return value.value_ <= 0;
    }

    [[nodiscard]] friend constexpr weak_ordering
    operator<=>(weak_ordering value, detail::comparison_zero) noexcept {
        return value;
    }

    [[nodiscard]] friend constexpr weak_ordering
    operator<=>(detail::comparison_zero, weak_ordering value) noexcept {
        return value.value_ < 0 ? weak_ordering::greater
             : value.value_ > 0 ? weak_ordering::less
                                : weak_ordering::equivalent;
    }
};

inline constexpr weak_ordering weak_ordering::less{
    detail::ordered_value::less};
inline constexpr weak_ordering weak_ordering::equivalent{
    detail::ordered_value::equivalent};
inline constexpr weak_ordering weak_ordering::greater{
    detail::ordered_value::greater};

class strong_ordering {
    detail::comparison_value value_;

    constexpr explicit strong_ordering(detail::ordered_value value) noexcept
        : value_(static_cast<detail::comparison_value>(value)) {}

public:
    static const strong_ordering less;
    static const strong_ordering equal;
    static const strong_ordering equivalent;
    static const strong_ordering greater;

    [[nodiscard]] constexpr operator partial_ordering() const noexcept {
        return partial_ordering(static_cast<detail::ordered_value>(value_));
    }

    [[nodiscard]] constexpr operator weak_ordering() const noexcept {
        return weak_ordering(static_cast<detail::ordered_value>(value_));
    }

    [[nodiscard]] friend constexpr bool
    operator==(strong_ordering value, detail::comparison_zero) noexcept {
        return value.value_ == 0;
    }

    [[nodiscard]] friend constexpr bool
    operator==(strong_ordering, strong_ordering) noexcept = default;

    [[nodiscard]] friend constexpr bool
    operator<(strong_ordering value, detail::comparison_zero) noexcept {
        return value.value_ < 0;
    }

    [[nodiscard]] friend constexpr bool
    operator>(strong_ordering value, detail::comparison_zero) noexcept {
        return value.value_ > 0;
    }

    [[nodiscard]] friend constexpr bool
    operator<=(strong_ordering value, detail::comparison_zero) noexcept {
        return value.value_ <= 0;
    }

    [[nodiscard]] friend constexpr bool
    operator>=(strong_ordering value, detail::comparison_zero) noexcept {
        return value.value_ >= 0;
    }

    [[nodiscard]] friend constexpr bool
    operator<(detail::comparison_zero, strong_ordering value) noexcept {
        return value.value_ > 0;
    }

    [[nodiscard]] friend constexpr bool
    operator>(detail::comparison_zero, strong_ordering value) noexcept {
        return value.value_ < 0;
    }

    [[nodiscard]] friend constexpr bool
    operator<=(detail::comparison_zero, strong_ordering value) noexcept {
        return value.value_ >= 0;
    }

    [[nodiscard]] friend constexpr bool
    operator>=(detail::comparison_zero, strong_ordering value) noexcept {
        return value.value_ <= 0;
    }

    [[nodiscard]] friend constexpr strong_ordering
    operator<=>(strong_ordering value, detail::comparison_zero) noexcept {
        return value;
    }

    [[nodiscard]] friend constexpr strong_ordering
    operator<=>(detail::comparison_zero, strong_ordering value) noexcept {
        return value.value_ < 0 ? strong_ordering::greater
             : value.value_ > 0 ? strong_ordering::less
                                : strong_ordering::equal;
    }
};

inline constexpr strong_ordering strong_ordering::less{
    detail::ordered_value::less};
inline constexpr strong_ordering strong_ordering::equal{
    detail::ordered_value::equivalent};
inline constexpr strong_ordering strong_ordering::equivalent{
    detail::ordered_value::equivalent};
inline constexpr strong_ordering strong_ordering::greater{
    detail::ordered_value::greater};

[[nodiscard]] constexpr bool is_eq(partial_ordering value) noexcept {
    return value == 0;
}
[[nodiscard]] constexpr bool is_neq(partial_ordering value) noexcept {
    return value != 0;
}
[[nodiscard]] constexpr bool is_lt(partial_ordering value) noexcept {
    return value < 0;
}
[[nodiscard]] constexpr bool is_lteq(partial_ordering value) noexcept {
    return value <= 0;
}
[[nodiscard]] constexpr bool is_gt(partial_ordering value) noexcept {
    return value > 0;
}
[[nodiscard]] constexpr bool is_gteq(partial_ordering value) noexcept {
    return value >= 0;
}

namespace detail {

template<typename T>
inline constexpr bool is_comparison_category =
    is_same_v<T, partial_ordering> ||
    is_same_v<T, weak_ordering> ||
    is_same_v<T, strong_ordering>;

template<typename... Types>
struct common_comparison_category_impl {
    static constexpr bool all_categories =
        (is_comparison_category<Types> && ...);
    static constexpr bool has_partial =
        (is_same_v<Types, partial_ordering> || ...);
    static constexpr bool has_weak =
        (is_same_v<Types, weak_ordering> || ...);

    using type = conditional_t<!all_categories, void,
                 conditional_t<has_partial, partial_ordering,
                 conditional_t<has_weak, weak_ordering,
                               strong_ordering>>>;
};

} /* namespace detail */

template<typename... Types>
struct common_comparison_category
    : detail::common_comparison_category_impl<Types...> {};

template<typename... Types>
using common_comparison_category_t =
    typename common_comparison_category<Types...>::type;

namespace detail {

template<typename Result, typename Category>
concept compares_as =
    same_as<common_comparison_category_t<Result, Category>, Category>;

template<typename T, typename U>
concept weakly_equality_comparable_with =
    requires(const remove_reference_t<T>& left,
             const remove_reference_t<U>& right) {
        { left == right } -> convertible_to<bool>;
        { left != right } -> convertible_to<bool>;
        { right == left } -> convertible_to<bool>;
        { right != left } -> convertible_to<bool>;
    };

template<typename T, typename U>
concept partially_ordered_with =
    requires(const remove_reference_t<T>& left,
             const remove_reference_t<U>& right) {
        { left < right } -> convertible_to<bool>;
        { left > right } -> convertible_to<bool>;
        { left <= right } -> convertible_to<bool>;
        { left >= right } -> convertible_to<bool>;
        { right < left } -> convertible_to<bool>;
        { right > left } -> convertible_to<bool>;
        { right <= left } -> convertible_to<bool>;
        { right >= left } -> convertible_to<bool>;
    };

} /* namespace detail */

template<typename T, typename Category = partial_ordering>
concept three_way_comparable =
    detail::weakly_equality_comparable_with<T, T> &&
    detail::partially_ordered_with<T, T> &&
    requires(const remove_reference_t<T>& left,
             const remove_reference_t<T>& right) {
        { left <=> right } -> detail::compares_as<Category>;
    };

template<typename T, typename U, typename Category = partial_ordering>
concept three_way_comparable_with =
    three_way_comparable<T, Category> &&
    three_way_comparable<U, Category> &&
    common_reference_with<const remove_reference_t<T>&,
                          const remove_reference_t<U>&> &&
    three_way_comparable<
        common_reference_t<const remove_reference_t<T>&,
                           const remove_reference_t<U>&>, Category> &&
    detail::weakly_equality_comparable_with<T, U> &&
    detail::partially_ordered_with<T, U> &&
    requires(const remove_reference_t<T>& left,
             const remove_reference_t<U>& right) {
        { left <=> right } -> detail::compares_as<Category>;
        { right <=> left } -> detail::compares_as<Category>;
    };

namespace detail {

template<typename T, typename U, typename = void>
struct compare_three_way_result_impl {};

template<typename T, typename U>
struct compare_three_way_result_impl<T, U, void_t<
    decltype(declval<const remove_reference_t<T>&>() <=>
             declval<const remove_reference_t<U>&>())>> {
    using type = decltype(declval<const remove_reference_t<T>&>() <=>
                          declval<const remove_reference_t<U>&>());
};

} /* namespace detail */

template<typename T, typename U = T>
struct compare_three_way_result
    : detail::compare_three_way_result_impl<T, U> {};

template<typename T, typename U = T>
using compare_three_way_result_t =
    typename detail::compare_three_way_result_impl<T, U>::type;

struct compare_three_way {
    template<typename T, typename U>
        requires three_way_comparable_with<T, U>
    [[nodiscard]] constexpr auto operator()(T&& left, U&& right) const
        noexcept(noexcept(static_cast<T&&>(left) <=> static_cast<U&&>(right))) {
        return static_cast<T&&>(left) <=> static_cast<U&&>(right);
    }

    using is_transparent = void;
};

namespace detail {

struct synth_three_way_fn {
    template<typename T, typename U>
        requires three_way_comparable_with<T, U>
    [[nodiscard]] constexpr auto operator()(const T& left,
                                             const U& right) const
        noexcept(noexcept(left <=> right)) {
        return left <=> right;
    }

    template<typename T, typename U>
        requires (!three_way_comparable_with<T, U>) &&
                 requires(const T& left, const U& right) {
                     { left < right } -> convertible_to<bool>;
                     { right < left } -> convertible_to<bool>;
                 }
    [[nodiscard]] constexpr weak_ordering operator()(const T& left,
                                                       const U& right) const
        noexcept(noexcept(left < right) && noexcept(right < left)) {
        if (left < right) return weak_ordering::less;
        if (right < left) return weak_ordering::greater;
        return weak_ordering::equivalent;
    }
};

inline constexpr synth_three_way_fn synth_three_way{};

template<typename T, typename U = T>
using synth_three_way_result_t =
    decltype(synth_three_way(declval<const T&>(), declval<const U&>()));

} /* namespace detail */

/* ADL poison pills keep the public CPO objects out of ordinary lookup. */
namespace detail {

void strong_order() = delete;
void weak_order() = delete;
void partial_order() = delete;

/* GCC 13 and Clang expose a constexpr bit-cast builtin.  It is sufficient for
 * the exact binary32/binary64 IEEE totalOrder slice below.  The x87 path is
 * guarded separately because its 80-bit payload is not a constexpr bit-cast
 * surface on every supported compiler. */
#if defined(__clang__) && defined(__has_builtin)
#if __has_builtin(__builtin_bit_cast)
#define RINCXX_COMPARE_HAS_IEEE_BINARY_TOTAL_ORDER 1
#endif
#elif defined(__GNUC__) && !defined(__clang__) && __GNUC__ == 13
#define RINCXX_COMPARE_HAS_IEEE_BINARY_TOTAL_ORDER 1
#endif

#if defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
    __LDBL_MANT_DIG__ == 64 && __LDBL_MAX_EXP__ == 16384 && \
    defined(__SIZEOF_LONG_DOUBLE__) && __SIZEOF_LONG_DOUBLE__ >= 10 && \
    defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
    defined(__ORDER_BIG_ENDIAN__) && \
    (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ || \
     __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define RINCXX_COMPARE_HAS_X87_TOTAL_ORDER 1
#endif

#if defined(RINCXX_COMPARE_HAS_IEEE_BINARY_TOTAL_ORDER)
template<typename T>
inline constexpr bool ieee_binary_total_order_type =
    is_same<remove_cv_t<T>, float>::value ||
    is_same<remove_cv_t<T>, double>::value;

template<typename T, typename U>
inline constexpr bool ieee_binary_total_order_pair =
    is_same<remove_cvref_t<T>, remove_cvref_t<U>>::value &&
    ieee_binary_total_order_type<remove_cvref_t<T>>;

template<typename T>
struct ieee_binary_total_order_bits;

template<>
struct ieee_binary_total_order_bits<float> {
    using type = unsigned int;
    static constexpr type sign = 0x80000000u;
};

template<>
struct ieee_binary_total_order_bits<double> {
    using type = unsigned long long;
    static constexpr type sign = 0x8000000000000000ULL;
};

template<typename T>
constexpr typename ieee_binary_total_order_bits<remove_cv_t<T>>::type
ieee_binary_bits(T value) noexcept {
    using value_type = remove_cv_t<T>;
    using bits_type = typename ieee_binary_total_order_bits<value_type>::type;
    return __builtin_bit_cast(bits_type, value);
}

template<typename T>
constexpr typename ieee_binary_total_order_bits<remove_cv_t<T>>::type
ieee_binary_total_key(T value) noexcept {
    using value_type = remove_cv_t<T>;
    using bits = ieee_binary_total_order_bits<value_type>;
    const typename bits::type representation = ieee_binary_bits(value);
    return (representation & bits::sign) != 0
        ? ~representation
        : representation | bits::sign;
}

template<typename T>
constexpr strong_ordering ieee_binary_strong_order(T left, T right) noexcept {
    const auto left_key = ieee_binary_total_key(left);
    const auto right_key = ieee_binary_total_key(right);
    return left_key < right_key ? strong_ordering::less
         : left_key > right_key ? strong_ordering::greater
                                : strong_ordering::equal;
}

template<typename T>
constexpr weak_ordering ieee_binary_weak_order(T left, T right) noexcept {
    const bool left_nan = left != left;
    const bool right_nan = right != right;
    if (left_nan || right_nan) {
        const bool left_negative =
            (ieee_binary_bits(left) & ieee_binary_total_order_bits<
                remove_cv_t<T>>::sign) != 0;
        const bool right_negative =
            (ieee_binary_bits(right) & ieee_binary_total_order_bits<
                remove_cv_t<T>>::sign) != 0;
        if (left_nan && right_nan && left_negative == right_negative) {
            return weak_ordering::equivalent;
        }
        if (left_nan) {
            return left_negative ? weak_ordering::less
                                 : weak_ordering::greater;
        }
        return right_negative ? weak_ordering::greater
                              : weak_ordering::less;
    }
    if (left == right) return weak_ordering::equivalent;
    return left < right ? weak_ordering::less : weak_ordering::greater;
}
#endif

#if defined(RINCXX_COMPARE_HAS_X87_TOTAL_ORDER)
struct x87_long_double_bits {
    unsigned long long significand;
    unsigned int sign_exponent;
};

inline x87_long_double_bits
x87_long_double_extract(long double value) noexcept {
    unsigned char bytes[sizeof(long double)] = {};
    __builtin_memcpy(bytes, &value, sizeof(value));
    x87_long_double_bits result{};
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    for (unsigned int index = 0u; index < 8u; ++index) {
        result.significand |=
            static_cast<unsigned long long>(bytes[index]) << (8u * index);
    }
    result.sign_exponent = static_cast<unsigned int>(bytes[8]) |
                           (static_cast<unsigned int>(bytes[9]) << 8u);
#else
    result.sign_exponent = (static_cast<unsigned int>(bytes[0]) << 8u) |
                           static_cast<unsigned int>(bytes[1]);
    for (unsigned int index = 0u; index < 8u; ++index) {
        result.significand = (result.significand << 8u) |
                             static_cast<unsigned long long>(bytes[2u + index]);
    }
#endif
    return result;
}

struct x87_long_double_key {
    unsigned int high;
    unsigned long long low;
};

inline x87_long_double_key
x87_long_double_total_key(x87_long_double_bits bits) noexcept {
    const bool negative = (bits.sign_exponent & 0x8000u) != 0u;
    x87_long_double_key key{
        bits.sign_exponent, bits.significand};
    if (negative) {
        key.high = (~key.high) & 0xffffu;
        key.low = ~key.low;
    } else {
        key.high |= 0x8000u;
    }
    return key;
}

inline bool x87_long_double_key_less(x87_long_double_key left,
                                     x87_long_double_key right) noexcept {
    return left.high < right.high ||
           (left.high == right.high && left.low < right.low);
}

inline bool x87_long_double_is_nan(x87_long_double_bits bits) noexcept {
    return (bits.sign_exponent & 0x7fffu) == 0x7fffu &&
           bits.significand != (1ull << 63u);
}

inline strong_ordering x87_long_double_strong_order(
    long double left, long double right) noexcept {
    const x87_long_double_key left_key =
        x87_long_double_total_key(x87_long_double_extract(left));
    const x87_long_double_key right_key =
        x87_long_double_total_key(x87_long_double_extract(right));
    return x87_long_double_key_less(left_key, right_key)
        ? strong_ordering::less
        : x87_long_double_key_less(right_key, left_key)
            ? strong_ordering::greater
            : strong_ordering::equal;
}

inline weak_ordering x87_long_double_weak_order(
    long double left, long double right) noexcept {
    const x87_long_double_bits left_bits = x87_long_double_extract(left);
    const x87_long_double_bits right_bits = x87_long_double_extract(right);
    const bool left_nan = x87_long_double_is_nan(left_bits);
    const bool right_nan = x87_long_double_is_nan(right_bits);
    if (left_nan || right_nan) {
        const bool left_negative = (left_bits.sign_exponent & 0x8000u) != 0u;
        const bool right_negative = (right_bits.sign_exponent & 0x8000u) != 0u;
        if (left_nan && right_nan && left_negative == right_negative)
            return weak_ordering::equivalent;
        if (left_nan)
            return left_negative ? weak_ordering::less
                                 : weak_ordering::greater;
        return right_negative ? weak_ordering::greater
                              : weak_ordering::less;
    }
    if (left == right) return weak_ordering::equivalent;
    return left < right ? weak_ordering::less : weak_ordering::greater;
}
#endif

template<typename T, typename U>
concept has_adl_strong_order =
    requires(T&& left, U&& right) {
        { strong_order(static_cast<T&&>(left), static_cast<U&&>(right)) }
            -> same_as<strong_ordering>;
    };

template<typename T, typename U>
concept has_adl_weak_order =
    requires(T&& left, U&& right) {
        { weak_order(static_cast<T&&>(left), static_cast<U&&>(right)) }
            -> same_as<weak_ordering>;
    };

template<typename T, typename U>
concept has_adl_partial_order =
    requires(T&& left, U&& right) {
        { partial_order(static_cast<T&&>(left), static_cast<U&&>(right)) }
            -> same_as<partial_ordering>;
    };

template<typename T, typename U>
    requires has_adl_strong_order<T, U>
[[nodiscard]] constexpr strong_ordering
call_adl_strong_order(T&& left, U&& right)
    noexcept(noexcept(strong_order(static_cast<T&&>(left),
                                   static_cast<U&&>(right)))) {
    return strong_order(static_cast<T&&>(left), static_cast<U&&>(right));
}

template<typename T, typename U>
    requires has_adl_weak_order<T, U>
[[nodiscard]] constexpr weak_ordering
call_adl_weak_order(T&& left, U&& right)
    noexcept(noexcept(weak_order(static_cast<T&&>(left),
                                 static_cast<U&&>(right)))) {
    return weak_order(static_cast<T&&>(left), static_cast<U&&>(right));
}

template<typename T, typename U>
    requires has_adl_partial_order<T, U>
[[nodiscard]] constexpr partial_ordering
call_adl_partial_order(T&& left, U&& right)
    noexcept(noexcept(partial_order(static_cast<T&&>(left),
                                    static_cast<U&&>(right)))) {
    return partial_order(static_cast<T&&>(left), static_cast<U&&>(right));
}

struct strong_order_fn {
    template<typename T, typename U>
        requires has_adl_strong_order<T, U>
    [[nodiscard]] constexpr strong_ordering
    operator()(T&& left, U&& right) const
        noexcept(noexcept(call_adl_strong_order(
            static_cast<T&&>(left), static_cast<U&&>(right)))) {
        return call_adl_strong_order(static_cast<T&&>(left),
                                     static_cast<U&&>(right));
    }

#if defined(RINCXX_COMPARE_HAS_IEEE_BINARY_TOTAL_ORDER)
    template<typename T, typename U>
        requires (!has_adl_strong_order<T, U>) &&
                 ieee_binary_total_order_pair<T, U>
    [[nodiscard]] constexpr strong_ordering
    operator()(T&& left, U&& right) const noexcept {
        return ieee_binary_strong_order(static_cast<T&&>(left),
                                        static_cast<U&&>(right));
    }
#endif

#if defined(RINCXX_COMPARE_HAS_X87_TOTAL_ORDER)
    template<typename T, typename U>
        requires (!has_adl_strong_order<T, U>) &&
                 is_same<remove_cvref_t<T>, long double>::value &&
                 is_same<remove_cvref_t<U>, long double>::value
    [[nodiscard]] constexpr strong_ordering
    operator()(T&& left, U&& right) const noexcept {
        return x87_long_double_strong_order(static_cast<long double>(left),
                                            static_cast<long double>(right));
    }
#endif

    template<typename T, typename U>
        requires (!has_adl_strong_order<T, U>) &&
                 is_object_pointer<remove_cvref_t<T>>::value &&
                 is_object_pointer<remove_cvref_t<U>>::value
    [[nodiscard]] constexpr strong_ordering
    operator()(T&& left, U&& right) const noexcept {
        return object_pointer_equal(static_cast<T&&>(left),
                                    static_cast<U&&>(right))
            ? strong_ordering::equal
            : object_pointer_total_less(static_cast<T&&>(left),
                                        static_cast<U&&>(right))
                ? strong_ordering::less
                : strong_ordering::greater;
    }

    template<typename T, typename U>
        requires (!has_adl_strong_order<T, U>) &&
                 is_function_pointer<remove_cvref_t<T>>::value &&
                 is_function_pointer<remove_cvref_t<U>>::value
    [[nodiscard]] constexpr strong_ordering
    operator()(T&& left, U&& right) const noexcept {
        return function_pointer_equal(static_cast<T&&>(left),
                                      static_cast<U&&>(right))
            ? strong_ordering::equal
            : function_pointer_total_less(static_cast<T&&>(left),
                                          static_cast<U&&>(right))
                ? strong_ordering::less
                : strong_ordering::greater;
    }

    template<typename T, typename U>
        requires (!has_adl_strong_order<T, U>) &&
                 (!is_object_pointer<remove_cvref_t<T>>::value ||
                  !is_object_pointer<remove_cvref_t<U>>::value) &&
                 (!is_function_pointer<remove_cvref_t<T>>::value ||
                  !is_function_pointer<remove_cvref_t<U>>::value) &&
#if defined(RINCXX_COMPARE_HAS_X87_TOTAL_ORDER)
                 (!is_same<remove_cvref_t<T>, long double>::value ||
                  !is_same<remove_cvref_t<U>, long double>::value) &&
#endif
                 requires(T&& left, U&& right) {
                     strong_ordering(static_cast<T&&>(left) <=>
                                     static_cast<U&&>(right));
                 }
    [[nodiscard]] constexpr strong_ordering
    operator()(T&& left, U&& right) const
        noexcept(noexcept(strong_ordering(
            static_cast<T&&>(left) <=> static_cast<U&&>(right)))) {
        return strong_ordering(static_cast<T&&>(left) <=>
                               static_cast<U&&>(right));
    }
};

struct weak_order_fn {
    template<typename T, typename U>
        requires has_adl_weak_order<T, U>
    [[nodiscard]] constexpr weak_ordering
    operator()(T&& left, U&& right) const
        noexcept(noexcept(call_adl_weak_order(
            static_cast<T&&>(left), static_cast<U&&>(right)))) {
        return call_adl_weak_order(static_cast<T&&>(left),
                                   static_cast<U&&>(right));
    }

#if defined(RINCXX_COMPARE_HAS_IEEE_BINARY_TOTAL_ORDER)
    template<typename T, typename U>
        requires (!has_adl_weak_order<T, U>) &&
                 ieee_binary_total_order_pair<T, U>
    [[nodiscard]] constexpr weak_ordering
    operator()(T&& left, U&& right) const noexcept {
        return ieee_binary_weak_order(static_cast<T&&>(left),
                                      static_cast<U&&>(right));
    }
#endif

#if defined(RINCXX_COMPARE_HAS_X87_TOTAL_ORDER)
    template<typename T, typename U>
        requires (!has_adl_weak_order<T, U>) &&
                 is_same<remove_cvref_t<T>, long double>::value &&
                 is_same<remove_cvref_t<U>, long double>::value
    [[nodiscard]] constexpr weak_ordering
    operator()(T&& left, U&& right) const noexcept {
        return x87_long_double_weak_order(static_cast<long double>(left),
                                          static_cast<long double>(right));
    }
#endif

    template<typename T, typename U>
        requires (!has_adl_weak_order<T, U>) &&
#if defined(RINCXX_COMPARE_HAS_X87_TOTAL_ORDER)
                 (!is_same<remove_cvref_t<T>, long double>::value ||
                  !is_same<remove_cvref_t<U>, long double>::value) &&
#endif
                 requires(T&& left, U&& right) {
                     weak_ordering(static_cast<T&&>(left) <=>
                                   static_cast<U&&>(right));
                 }
    [[nodiscard]] constexpr weak_ordering
    operator()(T&& left, U&& right) const
        noexcept(noexcept(weak_ordering(
            static_cast<T&&>(left) <=> static_cast<U&&>(right)))) {
        return weak_ordering(static_cast<T&&>(left) <=>
                             static_cast<U&&>(right));
    }

    template<typename T, typename U>
        requires (!has_adl_weak_order<T, U>) &&
                 (!requires(T&& left, U&& right) {
                     weak_ordering(static_cast<T&&>(left) <=>
                                   static_cast<U&&>(right));
                 }) &&
#if defined(RINCXX_COMPARE_HAS_IEEE_BINARY_TOTAL_ORDER)
                 (!ieee_binary_total_order_pair<T, U>) &&
#endif
#if defined(RINCXX_COMPARE_HAS_X87_TOTAL_ORDER)
                 (!is_same<remove_cvref_t<T>, long double>::value ||
                  !is_same<remove_cvref_t<U>, long double>::value) &&
#endif
                 requires(T&& left, U&& right) {
                     strong_order_fn{}(static_cast<T&&>(left),
                                       static_cast<U&&>(right));
                 }
    [[nodiscard]] constexpr weak_ordering
    operator()(T&& left, U&& right) const
        noexcept(noexcept(strong_order_fn{}(
            static_cast<T&&>(left), static_cast<U&&>(right)))) {
        return strong_order_fn{}(static_cast<T&&>(left),
                                 static_cast<U&&>(right));
    }
};

struct partial_order_fn {
    template<typename T, typename U>
        requires has_adl_partial_order<T, U>
    [[nodiscard]] constexpr partial_ordering
    operator()(T&& left, U&& right) const
        noexcept(noexcept(call_adl_partial_order(
            static_cast<T&&>(left), static_cast<U&&>(right)))) {
        return call_adl_partial_order(static_cast<T&&>(left),
                                      static_cast<U&&>(right));
    }

    template<typename T, typename U>
        requires (!has_adl_partial_order<T, U>) &&
                 requires(T&& left, U&& right) {
                     partial_ordering(static_cast<T&&>(left) <=>
                                      static_cast<U&&>(right));
                 }
    [[nodiscard]] constexpr partial_ordering
    operator()(T&& left, U&& right) const
        noexcept(noexcept(partial_ordering(
            static_cast<T&&>(left) <=> static_cast<U&&>(right)))) {
        return partial_ordering(static_cast<T&&>(left) <=>
                                static_cast<U&&>(right));
    }

    template<typename T, typename U>
        requires (!has_adl_partial_order<T, U>) &&
                 (!requires(T&& left, U&& right) {
                     partial_ordering(static_cast<T&&>(left) <=>
                                      static_cast<U&&>(right));
                 }) &&
                 requires(T&& left, U&& right) {
                     weak_order_fn{}(static_cast<T&&>(left),
                                     static_cast<U&&>(right));
                 }
    [[nodiscard]] constexpr partial_ordering
    operator()(T&& left, U&& right) const
        noexcept(noexcept(weak_order_fn{}(
            static_cast<T&&>(left), static_cast<U&&>(right)))) {
        return weak_order_fn{}(static_cast<T&&>(left),
                               static_cast<U&&>(right));
    }
};

} /* namespace detail */

inline constexpr detail::strong_order_fn strong_order{};
inline constexpr detail::weak_order_fn weak_order{};
inline constexpr detail::partial_order_fn partial_order{};

namespace detail {

struct compare_strong_order_fallback_fn {
    template<typename T, typename U>
        requires requires(const T& left, const U& right) {
            strong_order(left, right);
        }
    [[nodiscard]] constexpr strong_ordering
    operator()(const T& left, const U& right) const
        noexcept(noexcept(strong_order(left, right))) {
        return strong_order(left, right);
    }

    template<typename T, typename U>
        requires (!requires(const T& left, const U& right) {
                      strong_order(left, right);
                  }) &&
                 requires(const T& left, const U& right) {
            { left == right } -> convertible_to<bool>;
            { left < right } -> convertible_to<bool>;
        }
    [[nodiscard]] constexpr strong_ordering
    operator()(const T& left, const U& right) const
        noexcept(noexcept(static_cast<bool>(left == right)) &&
                 noexcept(static_cast<bool>(left < right))) {
        if (left == right)
            return strong_ordering::equal;
        if (left < right)
            return strong_ordering::less;
        return strong_ordering::greater;
    }
};

struct compare_weak_order_fallback_fn {
    template<typename T, typename U>
        requires requires(const T& left, const U& right) {
            weak_order(left, right);
        }
    [[nodiscard]] constexpr weak_ordering
    operator()(const T& left, const U& right) const
        noexcept(noexcept(weak_order(left, right))) {
        return weak_order(left, right);
    }

    template<typename T, typename U>
        requires (!requires(const T& left, const U& right) {
                      weak_order(left, right);
                  }) &&
                 requires(const T& left, const U& right) {
            { left == right } -> convertible_to<bool>;
            { left < right } -> convertible_to<bool>;
        }
    [[nodiscard]] constexpr weak_ordering
    operator()(const T& left, const U& right) const
        noexcept(noexcept(static_cast<bool>(left == right)) &&
                 noexcept(static_cast<bool>(left < right))) {
        if (left == right)
            return weak_ordering::equivalent;
        if (left < right)
            return weak_ordering::less;
        return weak_ordering::greater;
    }
};

struct compare_partial_order_fallback_fn {
    template<typename T, typename U>
        requires requires(const T& left, const U& right) {
            partial_order(left, right);
        }
    [[nodiscard]] constexpr partial_ordering
    operator()(const T& left, const U& right) const
        noexcept(noexcept(partial_order(left, right))) {
        return partial_order(left, right);
    }

    template<typename T, typename U>
        requires (!requires(const T& left, const U& right) {
                      partial_order(left, right);
                  }) &&
                 requires(const T& left, const U& right) {
            { left == right } -> convertible_to<bool>;
            { left < right } -> convertible_to<bool>;
            { right < left } -> convertible_to<bool>;
        }
    [[nodiscard]] constexpr partial_ordering
    operator()(const T& left, const U& right) const
        noexcept(noexcept(static_cast<bool>(left == right)) &&
                 noexcept(static_cast<bool>(left < right)) &&
                 noexcept(static_cast<bool>(right < left))) {
        if (left == right)
            return partial_ordering::equivalent;
        if (left < right)
            return partial_ordering::less;
        if (right < left)
            return partial_ordering::greater;
        return partial_ordering::unordered;
    }
};

} /* namespace detail */

inline constexpr detail::compare_strong_order_fallback_fn
    compare_strong_order_fallback{};
inline constexpr detail::compare_weak_order_fallback_fn
    compare_weak_order_fallback{};
inline constexpr detail::compare_partial_order_fallback_fn
    compare_partial_order_fallback{};

#if defined(RINCXX_COMPARE_HAS_IEEE_BINARY_TOTAL_ORDER)
#undef RINCXX_COMPARE_HAS_IEEE_BINARY_TOTAL_ORDER
#endif

} /* namespace std */

#endif /* __cplusplus >= 202002L */

#endif /* __cplusplus */
#endif /* RINCXX_COMPARE_H */
