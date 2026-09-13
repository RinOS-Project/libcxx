/*
 * RinOS C++ Type Traits ✿
 * std::type_traits 互換実装
 */

#ifndef RINCXX_TYPE_TRAITS_H
#define RINCXX_TYPE_TRAITS_H

#include "rincxx.h"
#include "cstddef.h"
/* Note: <compare> should be included separately when needed */

#ifdef __cplusplus

namespace std {

template<typename T>
class reference_wrapper;

/* ═══════════════════════════════════════════════════════════════
 * 基本型特性
 * ═══════════════════════════════════════════════════════════════*/

template<typename T, T v>
struct integral_constant {
    static constexpr T value = v;
    using value_type = T;
    using type = integral_constant;
    constexpr operator value_type() const noexcept { return value; }
    constexpr value_type operator()() const noexcept { return value; }
};

/* In C++11/14 an odr-use of the static constexpr member (for example taking
 * `&true_type::value`) still needs one out-of-class definition.  C++17 makes
 * the in-class declaration implicitly inline. */
#if __cplusplus < 201703L
template<typename T, T v>
constexpr T integral_constant<T, v>::value;
#endif

using true_type = integral_constant<bool, true>;
using false_type = integral_constant<bool, false>;

/* bool_constant (C++17) */
template<bool B>
using bool_constant = integral_constant<bool, B>;

/* is_constant_evaluated (C++20) */
constexpr bool is_constant_evaluated() noexcept {
    return __builtin_is_constant_evaluated();
}

/* ═══════════════════════════════════════════════════════════════
 * 型変換 (remove_referenceはrincxx.hで定義済み)
 * ═══════════════════════════════════════════════════════════════*/

template<typename T> struct remove_const { using type = T; };
template<typename T> struct remove_const<const T> { using type = T; };

template<typename T> struct remove_volatile { using type = T; };
template<typename T> struct remove_volatile<volatile T> { using type = T; };

template<typename T>
struct remove_cv {
    using type = typename remove_volatile<typename remove_const<T>::type>::type;
};

/* remove_reference はrincxx.hで定義済み - スキップ */

template<typename T> struct remove_pointer { using type = T; };
template<typename T> struct remove_pointer<T*> { using type = T; };
template<typename T> struct remove_pointer<T* const> { using type = T; };
template<typename T> struct remove_pointer<T* volatile> { using type = T; };
template<typename T> struct remove_pointer<T* const volatile> { using type = T; };

template<typename T> struct add_const { using type = const T; };
template<typename T> struct add_volatile { using type = volatile T; };
template<typename T> struct add_cv { using type = const volatile T; };

template<typename T> struct add_pointer { using type = typename remove_reference<T>::type*; };

template<typename T> struct add_lvalue_reference { using type = T&; };
template<> struct add_lvalue_reference<void> { using type = void; };
template<> struct add_lvalue_reference<const void> { using type = void; };
template<> struct add_lvalue_reference<volatile void> { using type = void; };
template<> struct add_lvalue_reference<const volatile void> { using type = void; };

template<typename T> struct add_rvalue_reference { using type = T&&; };
template<> struct add_rvalue_reference<void> { using type = void; };
template<> struct add_rvalue_reference<const void> { using type = void; };
template<> struct add_rvalue_reference<volatile void> { using type = void; };
template<> struct add_rvalue_reference<const volatile void> { using type = void; };

/* remove_extent - 配列から要素型を取得 */
template<typename T> struct remove_extent { using type = T; };
template<typename T> struct remove_extent<T[]> { using type = T; };
template<typename T, size_t N> struct remove_extent<T[N]> { using type = T; };
template<typename T> using remove_extent_t = typename remove_extent<T>::type;

/* remove_all_extents */
template<typename T> struct remove_all_extents { using type = T; };
template<typename T> struct remove_all_extents<T[]> { using type = typename remove_all_extents<T>::type; };
template<typename T, size_t N> struct remove_all_extents<T[N]> { using type = typename remove_all_extents<T>::type; };
template<typename T> using remove_all_extents_t = typename remove_all_extents<T>::type;

/* void_t (C++17) - 早期定義 */
template<typename...>
using void_t = void;

/* C++14 エイリアス */
template<typename T> using remove_const_t = typename remove_const<T>::type;
template<typename T> using remove_volatile_t = typename remove_volatile<T>::type;
template<typename T> using remove_cv_t = typename remove_cv<T>::type;
template<typename T> using remove_reference_t = typename remove_reference<T>::type;
template<typename T> using remove_pointer_t = typename remove_pointer<T>::type;
template<typename T> using add_const_t = typename add_const<T>::type;
template<typename T> using add_volatile_t = typename add_volatile<T>::type;
template<typename T> using add_cv_t = typename add_cv<T>::type;
template<typename T> using add_pointer_t = typename add_pointer<T>::type;
template<typename T> using add_lvalue_reference_t = typename add_lvalue_reference<T>::type;
template<typename T> using add_rvalue_reference_t = typename add_rvalue_reference<T>::type;

/* ═══════════════════════════════════════════════════════════════
 * 型判定
 * ═══════════════════════════════════════════════════════════════*/

template<typename T, typename U> struct is_same : false_type {};
template<typename T> struct is_same<T, T> : true_type {};

template<typename T> struct is_void : is_same<void, typename remove_cv<T>::type> {};
template<typename T> struct is_null_pointer : is_same<nullptr_t, typename remove_cv<T>::type> {};

template<typename T> struct is_integral : false_type {};
template<> struct is_integral<bool> : true_type {};
template<> struct is_integral<char> : true_type {};
template<> struct is_integral<signed char> : true_type {};
template<> struct is_integral<unsigned char> : true_type {};
template<> struct is_integral<wchar_t> : true_type {};
#if defined(__cpp_char8_t)
template<> struct is_integral<char8_t> : true_type {};
#endif
template<> struct is_integral<char16_t> : true_type {};
template<> struct is_integral<char32_t> : true_type {};
template<> struct is_integral<short> : true_type {};
template<> struct is_integral<unsigned short> : true_type {};
template<> struct is_integral<int> : true_type {};
template<> struct is_integral<unsigned int> : true_type {};
template<> struct is_integral<long> : true_type {};
template<> struct is_integral<unsigned long> : true_type {};
template<> struct is_integral<long long> : true_type {};
template<> struct is_integral<unsigned long long> : true_type {};
#ifdef __SIZEOF_INT128__
template<> struct is_integral<__int128> : true_type {};
template<> struct is_integral<unsigned __int128> : true_type {};
#endif
template<typename T> struct is_integral<const T> : is_integral<T> {};
template<typename T> struct is_integral<volatile T> : is_integral<T> {};
template<typename T> struct is_integral<const volatile T> : is_integral<T> {};

template<typename T> struct is_floating_point : false_type {};
template<> struct is_floating_point<float> : true_type {};
template<> struct is_floating_point<double> : true_type {};
template<> struct is_floating_point<long double> : true_type {};
template<typename T>
struct is_floating_point<const T> : is_floating_point<T> {};
template<typename T>
struct is_floating_point<volatile T> : is_floating_point<T> {};
template<typename T>
struct is_floating_point<const volatile T> : is_floating_point<T> {};

template<typename T> struct is_arithmetic : 
    integral_constant<bool, is_integral<T>::value || is_floating_point<T>::value> {};

template<typename T> struct is_pointer : false_type {};
template<typename T> struct is_pointer<T*> : true_type {};
template<typename T> struct is_pointer<T* const> : true_type {};
template<typename T> struct is_pointer<T* volatile> : true_type {};
template<typename T> struct is_pointer<T* const volatile> : true_type {};

template<typename T> struct is_reference : false_type {};
template<typename T> struct is_reference<T&> : true_type {};
template<typename T> struct is_reference<T&&> : true_type {};

template<typename T> struct is_lvalue_reference : false_type {};
template<typename T> struct is_lvalue_reference<T&> : true_type {};

template<typename T> struct is_rvalue_reference : false_type {};
template<typename T> struct is_rvalue_reference<T&&> : true_type {};

template<typename T> struct is_const : false_type {};
template<typename T> struct is_const<const T> : true_type {};

template<typename T> struct is_volatile : false_type {};
template<typename T> struct is_volatile<volatile T> : true_type {};

template<typename T> struct is_array : false_type {};
template<typename T> struct is_array<T[]> : true_type {};
template<typename T, size_t N> struct is_array<T[N]> : true_type {};

/* ═══════════════════════════════════════════════════════════════
 * conditional
 * ═══════════════════════════════════════════════════════════════*/

template<bool B, typename T, typename F>
struct conditional { using type = T; };

template<typename T, typename F>
struct conditional<false, T, F> { using type = F; };

template<bool B, typename T, typename F>
using conditional_t = typename conditional<B, T, F>::type;

/* ═══════════════════════════════════════════════════════════════
 * enable_if
 * ═══════════════════════════════════════════════════════════════*/

template<bool B, typename T = void>
struct enable_if {};

template<typename T>
struct enable_if<true, T> { using type = T; };

template<bool B, typename T = void>
using enable_if_t = typename enable_if<B, T>::type;

/* ═══════════════════════════════════════════════════════════════
 * is_function (decayより前に定義が必要)
 * ═══════════════════════════════════════════════════════════════*/

/* is_function - 関数型の検出。関数型に const を付けても変化しない性質を利用 */
template<typename T>
struct is_function : integral_constant<bool,
    !is_const<const T>::value && !is_reference<T>::value
> {};

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_function_v = is_function<T>::value;
#endif

/* ═══════════════════════════════════════════════════════════════
 * decay
 * ═══════════════════════════════════════════════════════════════*/

template<typename T>
struct decay {
private:
    using U = typename remove_reference<T>::type;
public:
    using type = typename conditional<
        is_array<U>::value,
        typename remove_extent<U>::type*,
        typename conditional<
            is_function<U>::value,
            typename add_pointer<U>::type,
            typename remove_cv<U>::type
        >::type
    >::type;
};

template<typename T>
using decay_t = typename decay<T>::type;

/* unwrap_reference / unwrap_ref_decay (C++20) */
template<typename T>
struct unwrap_reference { using type = T; };

template<typename T>
struct unwrap_reference<reference_wrapper<T>> { using type = T&; };

template<typename T>
using unwrap_reference_t = typename unwrap_reference<T>::type;

template<typename T>
struct unwrap_ref_decay : unwrap_reference<decay_t<T>> {};

template<typename T>
using unwrap_ref_decay_t = typename unwrap_ref_decay<T>::type;

/* is_trivial - trivially copyable and trivially default constructible */
template<typename T>
struct is_trivial : integral_constant<bool, __is_trivial(T)> {};

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_trivial_v = is_trivial<T>::value;
#endif

/* ═══════════════════════════════════════════════════════════════
 * declval (common_typeより前に定義する必要がある)
 * ═══════════════════════════════════════════════════════════════*/

template<typename T>
typename add_rvalue_reference<T>::type declval() noexcept;

/* ═══════════════════════════════════════════════════════════════
 * is_convertible (declvalの後に定義)
 * ═══════════════════════════════════════════════════════════════*/

namespace detail {

template<typename To>
static void test_convertible(To);

template<typename From, typename To, typename = void>
struct is_convertible_impl : false_type {};

template<typename From, typename To>
struct is_convertible_impl<From, To,
    decltype(detail::test_convertible<To>(declval<From>()))> : true_type {};

} /* namespace detail */

template<typename From, typename To>
struct is_convertible : detail::is_convertible_impl<From, To> {};

/* void 特殊化 */
template<> struct is_convertible<void, void> : true_type {};
template<> struct is_convertible<const void, void> : true_type {};
template<> struct is_convertible<volatile void, void> : true_type {};
template<> struct is_convertible<const volatile void, void> : true_type {};
template<> struct is_convertible<void, const void> : true_type {};
template<> struct is_convertible<void, volatile void> : true_type {};
template<> struct is_convertible<void, const volatile void> : true_type {};

#if __cplusplus >= 201703L
template<typename From, typename To>
inline constexpr bool is_convertible_v = is_convertible<From, To>::value;
#endif

/* is_nothrow_convertible (C++17).  The conversion is performed as the
 * argument conversion to a noexcept probe function, so user-defined and
 * built-in conversions are both accounted for without forming a temporary
 * with an accidentally-explicit constructor.  As with the standard trait,
 * conversion to void is only valid for the void-to-void case. */
#if __cplusplus >= 201703L
namespace detail {

template<typename To>
static void test_nothrow_convertible(To) noexcept;

template<typename From, typename To, typename = void>
struct is_nothrow_convertible_impl : false_type {};

template<typename From, typename To>
struct is_nothrow_convertible_impl<From, To,
    decltype(detail::test_convertible<To>(declval<From>()))>
    : integral_constant<bool,
        noexcept(detail::test_nothrow_convertible<To>(declval<From>()))> {};

} /* namespace detail */

template<typename From, typename To>
struct is_nothrow_convertible
    : detail::is_nothrow_convertible_impl<From, To> {};

template<typename From>
struct is_nothrow_convertible<From, void> : false_type {};

template<>
struct is_nothrow_convertible<void, void> : true_type {};

template<typename From, typename To>
inline constexpr bool is_nothrow_convertible_v =
    is_nothrow_convertible<From, To>::value;
#endif

/* C++20 layout-property traits.  GCC and Clang expose the standard
 * predicates as compiler builtins; a compiler without either builtin keeps
 * the declarations available with the conservative standard-false value. */
#if __cplusplus >= 202002L
template<typename T, typename U>
struct is_layout_compatible : integral_constant<bool,
#if (defined(__clang__) && __has_builtin(__is_layout_compatible)) || \
    (defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 12)
    __is_layout_compatible(T, U)
#else
    false
#endif
> {};

template<typename Base, typename Derived>
struct is_pointer_interconvertible_base_of : integral_constant<bool,
#if (defined(__clang__) && __has_builtin(__is_pointer_interconvertible_base_of)) || \
    (defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 12)
    __is_pointer_interconvertible_base_of(Base, Derived)
#else
    false
#endif
> {};

/* The companion C++20 member-pointer predicates use the same compiler
 * layout model.  Keep the function declarations available even on a
 * compiler without the builtins, where the conservative result is false. */
template<typename S1, typename S2, typename M1, typename M2>
constexpr bool is_corresponding_member(M1 S1::*m1, M2 S2::*m2) noexcept
{
#if (defined(__clang__) && __has_builtin(__builtin_is_corresponding_member)) || \
    (defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 12)
    return __builtin_is_corresponding_member(m1, m2);
#else
    return false;
#endif
}

template<typename T, typename M>
constexpr bool is_pointer_interconvertible_with_class(M T::*mp) noexcept
{
#if (defined(__clang__) && __has_builtin(__builtin_is_pointer_interconvertible_with_class)) || \
    (defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 12)
    return __builtin_is_pointer_interconvertible_with_class(mp);
#else
    return false;
#endif
}

template<typename T, typename U>
inline constexpr bool is_layout_compatible_v =
    is_layout_compatible<T, U>::value;

template<typename Base, typename Derived>
inline constexpr bool is_pointer_interconvertible_base_of_v =
    is_pointer_interconvertible_base_of<Base, Derived>::value;
#endif

/* ═══════════════════════════════════════════════════════════════
 * common_type
 * ═══════════════════════════════════════════════════════════════*/

template<typename... T>
struct common_type;

namespace detail {

/* SFINAE helper for common_type */
template<typename T, typename U, typename = void>
struct common_type_impl {};

template<typename T, typename U>
struct common_type_impl<T, U,
    void_t<decltype(true ? declval<T>() : declval<U>())>> {
    using type = typename decay<decltype(true ? declval<T>() : declval<U>())>::type;
};

/* Fallback: try with decayed types */
template<typename T, typename U, typename = void>
struct common_type_decay_impl : common_type_impl<T, U> {};

template<typename T, typename U>
struct common_type_decay_impl<T, U,
    enable_if_t<!is_same<T, typename decay<T>::type>::value ||
                !is_same<U, typename decay<U>::type>::value>>
    : common_type<typename decay<T>::type, typename decay<U>::type> {};

} /* namespace detail */

/* Base case: single type */
template<typename T>
struct common_type<T> {
    using type = typename decay<T>::type;
};

/* Two types */
template<typename T, typename U>
struct common_type<T, U> : detail::common_type_decay_impl<T, U> {};

/* Recursive case: 3+ types */
template<typename T, typename U, typename... Rest>
struct common_type<T, U, Rest...>
    : common_type<typename common_type<T, U>::type, Rest...> {};

template<typename... T>
using common_type_t = typename common_type<T...>::type;

/* ═══════════════════════════════════════════════════════════════
 * is_trivially_constructible
 * ═══════════════════════════════════════════════════════════════*/

template<typename T, typename... Args>
struct is_trivially_constructible : integral_constant<bool, __is_trivially_constructible(T, Args...)> {};

template<typename T>
struct is_trivially_default_constructible : is_trivially_constructible<T> {};

template<typename T>
struct is_trivially_copy_constructible : is_trivially_constructible<T, const T&> {};

template<typename T>
struct is_trivially_move_constructible : is_trivially_constructible<T, T&&> {};

#if __cplusplus >= 201703L
template<typename T, typename... Args>
inline constexpr bool is_trivially_constructible_v = is_trivially_constructible<T, Args...>::value;
#endif

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_trivially_default_constructible_v = is_trivially_default_constructible<T>::value;
#endif

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_trivially_copy_constructible_v = is_trivially_copy_constructible<T>::value;
#endif

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_trivially_move_constructible_v = is_trivially_move_constructible<T>::value;
#endif

/* ═══════════════════════════════════════════════════════════════
 * is_signed / is_unsigned
 * ═══════════════════════════════════════════════════════════════*/

template<typename T, bool = is_arithmetic<T>::value>
struct is_signed_impl : integral_constant<bool, T(-1) < T(0)> {};

template<typename T>
struct is_signed_impl<T, false> : false_type {};

template<typename T>
struct is_signed : is_signed_impl<T> {};

template<typename T, bool = is_arithmetic<T>::value>
struct is_unsigned_impl : integral_constant<bool, T(0) < T(-1)> {};

template<typename T>
struct is_unsigned_impl<T, false> : false_type {};

template<typename T>
struct is_unsigned : is_unsigned_impl<T> {};

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_signed_v = is_signed<T>::value;
#endif

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_unsigned_v = is_unsigned<T>::value;
#endif

/* ═══════════════════════════════════════════════════════════════
 * make_signed / make_unsigned
 * ═══════════════════════════════════════════════════════════════*/

template<typename T> struct make_signed { using type = T; };
template<> struct make_signed<char> { using type = signed char; };
template<> struct make_signed<unsigned char> { using type = signed char; };
template<> struct make_signed<unsigned short> { using type = short; };
template<> struct make_signed<unsigned int> { using type = int; };
template<> struct make_signed<unsigned long> { using type = long; };
template<> struct make_signed<unsigned long long> { using type = long long; };
/* 既にsignedの型 */
template<> struct make_signed<signed char> { using type = signed char; };
template<> struct make_signed<short> { using type = short; };
template<> struct make_signed<int> { using type = int; };
template<> struct make_signed<long> { using type = long; };
template<> struct make_signed<long long> { using type = long long; };
/* wchar_t is 32-bit signed on Linux */
template<> struct make_signed<wchar_t> { using type = int; };
/* char16_t, char32_t */
#if defined(__cpp_char8_t)
template<> struct make_signed<char8_t> { using type = signed char; };
#endif
template<> struct make_signed<char16_t> { using type = short; };
template<> struct make_signed<char32_t> { using type = int; };
#ifdef __SIZEOF_INT128__
template<> struct make_signed<__int128> { using type = __int128; };
template<> struct make_signed<unsigned __int128> { using type = __int128; };
#endif

namespace __detail {
    /* Use __is_enum builtin directly since is_enum_v isn't defined yet */
    template<typename T, bool = __is_enum(T)> struct make_unsigned_helper { using type = T; };
    template<> struct make_unsigned_helper<char, false> { using type = unsigned char; };
    template<> struct make_unsigned_helper<signed char, false> { using type = unsigned char; };
    template<> struct make_unsigned_helper<short, false> { using type = unsigned short; };
    template<> struct make_unsigned_helper<int, false> { using type = unsigned int; };
    template<> struct make_unsigned_helper<long, false> { using type = unsigned long; };
    template<> struct make_unsigned_helper<long long, false> { using type = unsigned long long; };
    template<> struct make_unsigned_helper<unsigned char, false> { using type = unsigned char; };
    template<> struct make_unsigned_helper<unsigned short, false> { using type = unsigned short; };
    template<> struct make_unsigned_helper<unsigned int, false> { using type = unsigned int; };
    template<> struct make_unsigned_helper<unsigned long, false> { using type = unsigned long; };
    template<> struct make_unsigned_helper<unsigned long long, false> { using type = unsigned long long; };
    /* wchar_t is 32-bit signed on Linux, maps to unsigned int */
    template<> struct make_unsigned_helper<wchar_t, false> { using type = unsigned int; };
    /* char16_t, char32_t (already unsigned but need mapping) */
#if defined(__cpp_char8_t)
    template<> struct make_unsigned_helper<char8_t, false> { using type = unsigned char; };
#endif
    template<> struct make_unsigned_helper<char16_t, false> { using type = char16_t; };
    template<> struct make_unsigned_helper<char32_t, false> { using type = char32_t; };
#ifdef __SIZEOF_INT128__
    template<> struct make_unsigned_helper<__int128, false> { using type = unsigned __int128; };
    template<> struct make_unsigned_helper<unsigned __int128, false> { using type = unsigned __int128; };
#endif
    /* Enum type: get underlying type and make it unsigned */
    template<typename T> struct make_unsigned_helper<T, true> {
        using type = typename make_unsigned_helper<__underlying_type(T), false>::type;
    };
}

template<typename T> struct make_unsigned {
    using type = typename __detail::make_unsigned_helper<T>::type;
};

template<typename T>
using make_signed_t = typename make_signed<T>::type;

template<typename T>
using make_unsigned_t = typename make_unsigned<T>::type;

/* ═══════════════════════════════════════════════════════════════
 * underlying_type (for enums) - GCC拡張使用
 * ═══════════════════════════════════════════════════════════════*/

template<typename T>
struct underlying_type {
    using type = __underlying_type(T);
};

template<typename T>
using underlying_type_t = typename underlying_type<T>::type;

/* ═══════════════════════════════════════════════════════════════
 * is_enum
 * ═══════════════════════════════════════════════════════════════*/

template<typename T>
struct is_enum : integral_constant<bool, __is_enum(T)> {};

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_enum_v = is_enum<T>::value;
#endif

/* ═══════════════════════════════════════════════════════════════
 * is_class / is_union
 * ═══════════════════════════════════════════════════════════════*/

template<typename T>
struct is_class : integral_constant<bool, __is_class(T)> {};

template<typename T>
struct is_union : integral_constant<bool, __is_union(T)> {};

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_class_v = is_class<T>::value;
#endif

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_union_v = is_union<T>::value;
#endif

/* has_virtual_destructor */
template<typename T>
struct has_virtual_destructor : integral_constant<bool, __has_virtual_destructor(T)> {};

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool has_virtual_destructor_v = has_virtual_destructor<T>::value;
#endif

/* ═══════════════════════════════════════════════════════════════
 * is_trivially_copyable / is_trivially_destructible
 * ═══════════════════════════════════════════════════════════════*/

template<typename T>
struct is_trivially_copyable : integral_constant<bool, __is_trivially_copyable(T)> {};

namespace detail {
#if defined(__clang__)
template<typename T>
struct is_trivially_destructible_builtin
    : integral_constant<bool, __is_trivially_destructible(T)> {};
#else
template<typename T>
struct is_trivially_destructible_builtin
    : integral_constant<bool, __has_trivial_destructor(T)> {};
#endif
} /* namespace detail */

template<typename T>
struct is_trivially_destructible : detail::is_trivially_destructible_builtin<T> {};

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_trivially_copyable_v = is_trivially_copyable<T>::value;
#endif

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_trivially_destructible_v = is_trivially_destructible<T>::value;
#endif

/* is_trivially_assignable */
template<typename T, typename U>
struct is_trivially_assignable : integral_constant<bool, __is_trivially_assignable(T, U)> {};

#if __cplusplus >= 201703L
template<typename T, typename U>
inline constexpr bool is_trivially_assignable_v = is_trivially_assignable<T, U>::value;
#endif

/* is_trivially_copy_assignable */
template<typename T>
struct is_trivially_copy_assignable : is_trivially_assignable<T&, const T&> {};

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_trivially_copy_assignable_v = is_trivially_copy_assignable<T>::value;
#endif

/* is_trivially_move_assignable */
template<typename T>
struct is_trivially_move_assignable : is_trivially_assignable<T&, T&&> {};

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_trivially_move_assignable_v = is_trivially_move_assignable<T>::value;
#endif

/* ═══════════════════════════════════════════════════════════════
 * is_standard_layout / is_pod
 * ═══════════════════════════════════════════════════════════════*/

template<typename T>
struct is_standard_layout : integral_constant<bool, __is_standard_layout(T)> {};

template<typename T>
struct is_pod : integral_constant<bool, __is_pod(T)> {};

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_standard_layout_v = is_standard_layout<T>::value;
#endif

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_pod_v = is_pod<T>::value;
#endif

/* ═══════════════════════════════════════════════════════════════
 * is_empty / is_abstract / is_final / is_polymorphic
 * ═══════════════════════════════════════════════════════════════*/

template<typename T>
struct is_empty : integral_constant<bool, __is_empty(T)> {};

template<typename T>
struct is_abstract : integral_constant<bool, __is_abstract(T)> {};

template<typename T>
struct is_final : integral_constant<bool, __is_final(T)> {};

template<typename T>
struct is_polymorphic : integral_constant<bool, __is_polymorphic(T)> {};

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_empty_v = is_empty<T>::value;
#endif

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_abstract_v = is_abstract<T>::value;
#endif

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_final_v = is_final<T>::value;
#endif

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_polymorphic_v = is_polymorphic<T>::value;
#endif

/* ═══════════════════════════════════════════════════════════════
 * is_constructible / is_default_constructible
 * ═══════════════════════════════════════════════════════════════*/

template<typename T, typename... Args>
struct is_constructible : integral_constant<bool, __is_constructible(T, Args...)> {};

template<typename T>
struct is_default_constructible : is_constructible<T> {};

template<typename T>
struct is_copy_constructible : is_constructible<T, const T&> {};

template<typename T>
struct is_move_constructible : is_constructible<T, T&&> {};

#if __cplusplus >= 201703L
template<typename T, typename... Args>
inline constexpr bool is_constructible_v = is_constructible<T, Args...>::value;
#endif

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_default_constructible_v = is_default_constructible<T>::value;
#endif

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_copy_constructible_v = is_copy_constructible<T>::value;
#endif

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_move_constructible_v = is_move_constructible<T>::value;
#endif

/* ═══════════════════════════════════════════════════════════════
 * is_assignable
 * ═══════════════════════════════════════════════════════════════*/

template<typename T, typename U>
struct is_assignable : integral_constant<bool, __is_assignable(T, U)> {};

template<typename T>
struct is_copy_assignable : is_assignable<T&, const T&> {};

template<typename T>
struct is_move_assignable : is_assignable<T&, T&&> {};

#if __cplusplus >= 201703L
template<typename T, typename U>
inline constexpr bool is_assignable_v = is_assignable<T, U>::value;
#endif

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_copy_assignable_v = is_copy_assignable<T>::value;
#endif

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_move_assignable_v = is_move_assignable<T>::value;
#endif

/* ═══════════════════════════════════════════════════════════════
 * is_destructible
 * ═══════════════════════════════════════════════════════════════*/

namespace detail {

template<typename T, typename = void>
struct destructible_expression : false_type {};

template<typename T>
struct destructible_expression<
    T, void_t<decltype(declval<T&>().~T())>> : true_type {};

template<typename T>
struct is_destructible_impl : destructible_expression<T> {};

template<typename T>
struct is_destructible_impl<T&> : true_type {};

template<typename T>
struct is_destructible_impl<T&&> : true_type {};

template<typename T, size_t N>
struct is_destructible_impl<T[N]> : is_destructible_impl<T> {};

template<typename T>
struct is_destructible_impl<T[]> : false_type {};

template<typename T, typename = void>
struct nothrow_destructible_expression : false_type {};

template<typename T>
struct nothrow_destructible_expression<
    T, void_t<decltype(declval<T&>().~T())>>
    : integral_constant<bool, noexcept(declval<T&>().~T())> {};

template<typename T>
struct is_nothrow_destructible_impl
    : nothrow_destructible_expression<T> {};

template<typename T>
struct is_nothrow_destructible_impl<T&> : true_type {};

template<typename T>
struct is_nothrow_destructible_impl<T&&> : true_type {};

template<typename T, size_t N>
struct is_nothrow_destructible_impl<T[N]>
    : is_nothrow_destructible_impl<T> {};

template<typename T>
struct is_nothrow_destructible_impl<T[]> : false_type {};

} /* namespace detail */

template<typename T>
struct is_destructible : detail::is_destructible_impl<T> {};

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_destructible_v = is_destructible<T>::value;
#endif

/* ═══════════════════════════════════════════════════════════════
 * is_base_of
 * ═══════════════════════════════════════════════════════════════*/

template<typename Base, typename Derived>
struct is_base_of : integral_constant<bool, __is_base_of(Base, Derived)> {};

#if __cplusplus >= 201703L
template<typename Base, typename Derived>
inline constexpr bool is_base_of_v = is_base_of<Base, Derived>::value;
#endif

/* ═══════════════════════════════════════════════════════════════
 * void_t (C++17)
 * ═══════════════════════════════════════════════════════════════*/

template<typename...>
using void_t = void;

/* ═══════════════════════════════════════════════════════════════
 * conjunction / disjunction / negation (C++17)
 * ═══════════════════════════════════════════════════════════════*/

template<typename...> struct conjunction : true_type {};
template<typename B1> struct conjunction<B1> : B1 {};
template<typename B1, typename... Bn>
struct conjunction<B1, Bn...> 
    : conditional_t<bool(B1::value), conjunction<Bn...>, B1> {};

template<typename...> struct disjunction : false_type {};
template<typename B1> struct disjunction<B1> : B1 {};
template<typename B1, typename... Bn>
struct disjunction<B1, Bn...> 
    : conditional_t<bool(B1::value), B1, disjunction<Bn...>> {};

template<typename B>
struct negation : integral_constant<bool, !bool(B::value)> {};

#if __cplusplus >= 201703L
template<typename... B>
inline constexpr bool conjunction_v = conjunction<B...>::value;
#endif

#if __cplusplus >= 201703L
template<typename... B>
inline constexpr bool disjunction_v = disjunction<B...>::value;
#endif

#if __cplusplus >= 201703L
template<typename B>
inline constexpr bool negation_v = negation<B>::value;
#endif

/* ═══════════════════════════════════════════════════════════════
 * type_identity (C++20)
 * ═══════════════════════════════════════════════════════════════*/

template<typename T>
struct type_identity { using type = T; };

template<typename T>
using type_identity_t = typename type_identity<T>::type;

/* ═══════════════════════════════════════════════════════════════
 * basic_common_reference / common_reference (C++20)
 * ═══════════════════════════════════════════════════════════════*/

/* basic_common_reference - カスタマイゼーションポイント */
template<typename T, typename U, template<typename> class TQual, template<typename> class UQual>
struct basic_common_reference {};

namespace detail {

template<typename From, typename To>
struct copy_cv {
private:
    using with_const = conditional_t<is_const<From>::value,
                                     add_const_t<To>, To>;
public:
    using type = conditional_t<is_volatile<From>::value,
                               add_volatile_t<with_const>, with_const>;
};

template<typename From, typename To>
using copy_cv_t = typename copy_cv<From, To>::type;

template<typename From, typename To>
struct copy_cvref {
private:
    using cv_type = copy_cv_t<remove_reference_t<From>, To>;
public:
    using type = conditional_t<is_lvalue_reference<From>::value,
        add_lvalue_reference_t<cv_type>,
        conditional_t<is_rvalue_reference<From>::value,
            add_rvalue_reference_t<cv_type>, cv_type>>;
};

template<typename From, typename To>
using copy_cvref_t = typename copy_cvref<From, To>::type;

template<typename From>
struct xref {
    template<typename To>
    using apply = copy_cvref_t<From, To>;
};

template<typename X, typename Y, typename = void>
struct conditional_result {};

template<typename X, typename Y>
struct conditional_result<X, Y, void_t<
    decltype(false ? declval<X(&)()>()() : declval<Y(&)()>()())>> {
    using type = decltype(false ? declval<X(&)()>()() :
                                declval<Y(&)()>()());
};

template<typename Candidate, bool = is_reference<Candidate>::value>
struct reference_candidate {};

template<typename Candidate>
struct reference_candidate<Candidate, true> { using type = Candidate; };

template<typename T, typename U, typename = void>
struct simple_common_reference {};

template<typename T, typename U>
struct simple_common_reference<T, U, void_t<
    typename conditional_result<
        add_lvalue_reference_t<copy_cv_t<remove_reference_t<T>,
                                         remove_reference_t<U>>>,
        add_lvalue_reference_t<copy_cv_t<remove_reference_t<U>,
                                         remove_reference_t<T>>>>::type>>
    : reference_candidate<typename conditional_result<
          add_lvalue_reference_t<copy_cv_t<remove_reference_t<T>,
                                           remove_reference_t<U>>>,
          add_lvalue_reference_t<copy_cv_t<remove_reference_t<U>,
                                           remove_reference_t<T>>>>::type> {};

template<typename Candidate, bool>
struct convertible_reference_candidate {};

template<typename Candidate>
struct convertible_reference_candidate<Candidate, true> {
    using type = Candidate;
};

template<typename T, typename U, typename = void>
struct rvalue_common_reference {};

template<typename T, typename U>
struct rvalue_common_reference<T, U, void_t<
    typename simple_common_reference<remove_reference_t<T>&,
                                     remove_reference_t<U>&>::type>> {
private:
    using lvalue_common = typename simple_common_reference<
        remove_reference_t<T>&, remove_reference_t<U>&>::type;
    using candidate = add_rvalue_reference_t<remove_reference_t<lvalue_common>>;
public:
    using base = convertible_reference_candidate<candidate,
        is_convertible<T, candidate>::value &&
        is_convertible<U, candidate>::value>;
    using type = typename base::type;
};

template<typename T, typename U, typename = void>
struct rvalue_lvalue_common_reference {};

template<typename T, typename U>
struct rvalue_lvalue_common_reference<T, U, void_t<
    typename simple_common_reference<const remove_reference_t<T>&,
                                     remove_reference_t<U>&>::type>> {
private:
    using candidate = typename simple_common_reference<
        const remove_reference_t<T>&, remove_reference_t<U>&>::type;
public:
    using base = convertible_reference_candidate<candidate,
        is_convertible<T, candidate>::value>;
    using type = typename base::type;
};

template<typename T, typename U,
         bool BothLvalue = is_lvalue_reference<T>::value &&
                           is_lvalue_reference<U>::value,
         bool BothRvalue = is_rvalue_reference<T>::value &&
                           is_rvalue_reference<U>::value,
         bool RvalueLvalue = is_rvalue_reference<T>::value &&
                            is_lvalue_reference<U>::value,
         bool LvalueRvalue = is_lvalue_reference<T>::value &&
                            is_rvalue_reference<U>::value>
struct common_reference_from_references {};

template<typename T, typename U>
struct common_reference_from_references<T, U, true, false, false, false>
    : simple_common_reference<T, U> {};

template<typename T, typename U>
struct common_reference_from_references<T, U, false, true, false, false>
    : rvalue_common_reference<T, U> {};

template<typename T, typename U>
struct common_reference_from_references<T, U, false, false, true, false>
    : rvalue_lvalue_common_reference<T, U> {};

template<typename T, typename U>
struct common_reference_from_references<T, U, false, false, false, true>
    : common_reference_from_references<U, T> {};

template<int Priority>
struct common_reference_priority
    : common_reference_priority<Priority - 1> {};

template<>
struct common_reference_priority<0> {};

template<typename T, typename U,
         typename R = typename common_reference_from_references<T, U>::type,
         enable_if_t<is_reference<T>::value && is_reference<U>::value &&
                     is_convertible<add_pointer_t<T>, add_pointer_t<R>>::value &&
                     is_convertible<add_pointer_t<U>, add_pointer_t<R>>::value,
                     int> = 0>
type_identity<R> common_reference_select(common_reference_priority<3>);

template<typename T, typename U,
         typename R = typename basic_common_reference<
             remove_cv_t<remove_reference_t<T>>,
             remove_cv_t<remove_reference_t<U>>,
             xref<T>::template apply,
             xref<U>::template apply>::type>
type_identity<R> common_reference_select(common_reference_priority<2>);

template<typename T, typename U,
         typename R = typename conditional_result<T, U>::type>
type_identity<R> common_reference_select(common_reference_priority<1>);

template<typename T, typename U, typename R = typename common_type<T, U>::type>
type_identity<R> common_reference_select(common_reference_priority<0>);

template<typename T, typename U, typename = void>
struct common_reference_two {};

template<typename T, typename U>
struct common_reference_two<T, U, void_t<typename decltype(
    common_reference_select<T, U>(common_reference_priority<3>{}))::type>> {
    using type = typename decltype(
        common_reference_select<T, U>(common_reference_priority<3>{}))::type;
};

} /* namespace detail */

/* common_reference follows the C++20 reference, customization, conditional,
 * then common_type priority order. */
template<typename... Ts>
struct common_reference;

template<>
struct common_reference<> {};

template<typename T>
struct common_reference<T> { using type = T; };

template<typename T, typename U>
struct common_reference<T, U> : detail::common_reference_two<T, U> {};

namespace detail {

template<typename T, typename U, typename Enable, typename... Rest>
struct common_reference_many {};

template<typename T, typename U, typename... Rest>
struct common_reference_many<T, U, void_t<
    typename common_reference<T, U>::type>, Rest...>
    : common_reference<typename common_reference<T, U>::type, Rest...> {};

} /* namespace detail */

template<typename T, typename U, typename... Rest>
struct common_reference<T, U, Rest...>
    : detail::common_reference_many<T, U, void, Rest...> {};

template<typename... Ts>
using common_reference_t = typename common_reference<Ts...>::type;

/* ═══════════════════════════════════════════════════════════════
 * remove_cvref (C++20)
 * ═══════════════════════════════════════════════════════════════*/

template<typename T>
struct remove_cvref {
    using type = typename remove_cv<typename remove_reference<T>::type>::type;
};

template<typename T>
using remove_cvref_t = typename remove_cvref<T>::type;

/* ═══════════════════════════════════════════════════════════════
 * is_same_v and other _v helpers (C++17)
 * ═══════════════════════════════════════════════════════════════*/

#if __cplusplus >= 201703L
template<typename T, typename U>
inline constexpr bool is_same_v = is_same<T, U>::value;
#endif

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_void_v = is_void<T>::value;
#endif

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_null_pointer_v = is_null_pointer<T>::value;
#endif

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_integral_v = is_integral<T>::value;
#endif

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_floating_point_v = is_floating_point<T>::value;
#endif

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_arithmetic_v = is_arithmetic<T>::value;
#endif

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_pointer_v = is_pointer<T>::value;
#endif

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_reference_v = is_reference<T>::value;
#endif

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_lvalue_reference_v = is_lvalue_reference<T>::value;
#endif

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_rvalue_reference_v = is_rvalue_reference<T>::value;
#endif

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_const_v = is_const<T>::value;
#endif

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_volatile_v = is_volatile<T>::value;
#endif

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_array_v = is_array<T>::value;
#endif

/* is_member_pointer */
template<typename T> struct is_member_pointer : false_type {};
template<typename T, typename C> struct is_member_pointer<T C::*> : true_type {};
template<typename T> struct is_member_pointer<const T> : is_member_pointer<T> {};
template<typename T> struct is_member_pointer<volatile T> : is_member_pointer<T> {};
template<typename T>
struct is_member_pointer<const volatile T> : is_member_pointer<T> {};

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_member_pointer_v = is_member_pointer<T>::value;
#endif

/* is_scalar - arithmetic, enum, pointer, member pointer, null pointer */
template<typename T>
struct is_scalar : integral_constant<bool,
    is_arithmetic<T>::value ||
    is_enum<T>::value ||
    is_pointer<T>::value ||
    is_member_pointer<T>::value ||
    is_null_pointer<T>::value
> {};

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_scalar_v = is_scalar<T>::value;
#endif

/* is_object */
template<typename T>
struct is_object : integral_constant<bool,
    is_scalar<T>::value ||
    is_array<T>::value ||
    is_class<T>::value
> {};

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_object_v = is_object<T>::value;
#endif

/* is_fundamental */
template<typename T>
struct is_fundamental : integral_constant<bool,
    is_arithmetic<T>::value ||
    is_void<T>::value ||
    is_null_pointer<T>::value
> {};

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_fundamental_v = is_fundamental<T>::value;
#endif

/* is_compound */
template<typename T>
struct is_compound : integral_constant<bool, !is_fundamental<T>::value> {};

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_compound_v = is_compound<T>::value;
#endif

/* is_function は decay の前で定義済み */

/* is_member_function_pointer */
template<typename T>
struct is_member_function_pointer : false_type {};

template<typename T, typename C>
struct is_member_function_pointer<T C::*> : is_function<T> {};
template<typename T>
struct is_member_function_pointer<const T>
    : is_member_function_pointer<T> {};
template<typename T>
struct is_member_function_pointer<volatile T>
    : is_member_function_pointer<T> {};
template<typename T>
struct is_member_function_pointer<const volatile T>
    : is_member_function_pointer<T> {};

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_member_function_pointer_v = is_member_function_pointer<T>::value;
#endif

/* is_member_object_pointer */
template<typename T>
struct is_member_object_pointer : integral_constant<bool,
    is_member_pointer<T>::value && !is_member_function_pointer<T>::value
> {};

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_member_object_pointer_v = is_member_object_pointer<T>::value;
#endif

/* ═══════════════════════════════════════════════════════════════
 * invoke_result (C++17)
 * ═══════════════════════════════════════════════════════════════*/

namespace detail {

template<typename T>
struct is_reference_wrapper : false_type {};

template<typename T>
struct is_reference_wrapper<reference_wrapper<T>> : true_type {};

template<typename T>
struct member_pointer_class;

template<typename Member, typename Class>
struct member_pointer_class<Member Class::*> { using type = Class; };

template<typename Class, typename T,
         enable_if_t<is_base_of<Class, remove_cvref_t<T>>::value, int> = 0>
T&& invoke_target(T&& value) noexcept;

template<typename Class, typename T>
T& invoke_target(reference_wrapper<T> value) noexcept;

template<typename Class, typename T,
         enable_if_t<
             !is_base_of<Class, remove_cvref_t<T>>::value &&
             !is_reference_wrapper<remove_cvref_t<T>>::value,
             int> = 0>
auto invoke_target(T&& value)
    noexcept(noexcept(*declval<T>())) -> decltype(*declval<T>());

template<typename F, typename T, typename... Args,
         enable_if_t<
             is_member_function_pointer<remove_cvref_t<F>>::value,
             int> = 0>
auto invoke_expression(F&& function, T&& target, Args&&... args)
    noexcept(noexcept(
        (invoke_target<
            typename member_pointer_class<remove_cvref_t<F>>::type>(
                declval<T>()).*declval<F>())(declval<Args>()...)))
    -> decltype(
        (invoke_target<
            typename member_pointer_class<remove_cvref_t<F>>::type>(
                declval<T>()).*declval<F>())(declval<Args>()...));

template<typename F, typename T,
         enable_if_t<
             is_member_object_pointer<remove_cvref_t<F>>::value,
             int> = 0>
auto invoke_expression(F&& function, T&& target)
    noexcept(noexcept(
        invoke_target<
            typename member_pointer_class<remove_cvref_t<F>>::type>(
                declval<T>()).*declval<F>()))
    -> decltype(
        invoke_target<
            typename member_pointer_class<remove_cvref_t<F>>::type>(
                declval<T>()).*declval<F>());

template<typename F, typename... Args,
         enable_if_t<!is_member_pointer<remove_cvref_t<F>>::value, int> = 0>
auto invoke_expression(F&& function, Args&&... args)
    noexcept(noexcept(declval<F>()(declval<Args>()...)))
    -> decltype(declval<F>()(declval<Args>()...));

template<typename Void, typename F, typename... Args>
struct invoke_result_impl {};

template<typename F, typename... Args>
struct invoke_result_impl<
    void_t<decltype(invoke_expression(declval<F>(), declval<Args>()...))>,
    F, Args...> {
    using type = decltype(invoke_expression(declval<F>(), declval<Args>()...));
};

} /* namespace detail */

/* invoke_result - 呼び出し結果型 */
template<typename F, typename... Args>
struct invoke_result : detail::invoke_result_impl<void, F, Args...> {};

template<typename F, typename... Args>
using invoke_result_t = typename invoke_result<F, Args...>::type;

/* ═══════════════════════════════════════════════════════════════
 * is_invocable (C++17)
 * ═══════════════════════════════════════════════════════════════*/

namespace detail {

template<typename, typename F, typename... Args>
struct is_invocable_impl : false_type {};

template<typename F, typename... Args>
struct is_invocable_impl<void_t<invoke_result_t<F, Args...>>, F, Args...> : true_type {};

template<typename, typename R, typename F, typename... Args>
struct is_invocable_r_impl : false_type {};

template<typename R, typename F, typename... Args>
struct is_invocable_r_impl<void_t<invoke_result_t<F, Args...>>, R, F, Args...>
    : conditional_t<is_void<R>::value, true_type,
                    is_convertible<invoke_result_t<F, Args...>, R>> {};

} /* namespace detail */

template<typename F, typename... Args>
struct is_invocable : detail::is_invocable_impl<void, F, Args...> {};

template<typename R, typename F, typename... Args>
struct is_invocable_r : detail::is_invocable_r_impl<void, R, F, Args...> {};

#if __cplusplus >= 201703L
template<typename F, typename... Args>
inline constexpr bool is_invocable_v = is_invocable<F, Args...>::value;

template<typename R, typename F, typename... Args>
inline constexpr bool is_invocable_r_v = is_invocable_r<R, F, Args...>::value;
#endif

/* ═══════════════════════════════════════════════════════════════
 * is_nothrow_invocable (C++17)
 * ═══════════════════════════════════════════════════════════════*/

namespace detail {

template<typename, typename F, typename... Args>
struct is_nothrow_invocable_impl : false_type {};

template<typename F, typename... Args>
struct is_nothrow_invocable_impl<
    enable_if_t<is_invocable<F, Args...>::value>,
    F, Args...
> : integral_constant<bool,
        noexcept(invoke_expression(declval<F>(), declval<Args>()...))> {};

template<typename R>
void invoke_r_accept(R) noexcept;

template<typename R, typename Result, bool = is_void<R>::value>
struct is_nothrow_invocable_r_conversion
    : integral_constant<bool,
          noexcept(invoke_r_accept<R>(declval<Result>()))> {};

template<typename R, typename Result>
struct is_nothrow_invocable_r_conversion<R, Result, true> : true_type {};

template<typename, typename R, typename F, typename... Args>
struct is_nothrow_invocable_r_impl : false_type {};

template<typename R, typename F, typename... Args>
struct is_nothrow_invocable_r_impl<
    enable_if_t<is_invocable_r<R, F, Args...>::value>, R, F, Args...>
    : integral_constant<bool,
          noexcept(invoke_expression(declval<F>(), declval<Args>()...)) &&
          is_nothrow_invocable_r_conversion<
              R, invoke_result_t<F, Args...>>::value> {};

} /* namespace detail */

template<typename F, typename... Args>
struct is_nothrow_invocable : detail::is_nothrow_invocable_impl<void, F, Args...> {};

template<typename R, typename F, typename... Args>
struct is_nothrow_invocable_r
    : detail::is_nothrow_invocable_r_impl<void, R, F, Args...> {};

#if __cplusplus >= 201703L
template<typename F, typename... Args>
inline constexpr bool is_nothrow_invocable_v = is_nothrow_invocable<F, Args...>::value;

template<typename R, typename F, typename... Args>
inline constexpr bool is_nothrow_invocable_r_v = is_nothrow_invocable_r<R, F, Args...>::value;
#endif

/* ═══════════════════════════════════════════════════════════════
 * is_swappable (C++17)
 * ═══════════════════════════════════════════════════════════════*/

namespace detail {

template<typename T, typename = void>
struct is_swappable_impl : false_type {};

template<typename T>
struct is_swappable_impl<T, void_t<decltype(swap(declval<T&>(), declval<T&>()))>>
    : true_type {};

template<typename T, typename = void>
struct is_nothrow_swappable_impl : false_type {};

template<typename T>
struct is_nothrow_swappable_impl<T, void_t<decltype(swap(declval<T&>(), declval<T&>()))>>
    : integral_constant<bool, noexcept(swap(declval<T&>(), declval<T&>()))> {};

} /* namespace detail */

template<typename T>
struct is_swappable : detail::is_swappable_impl<T> {};

template<typename T>
struct is_nothrow_swappable : detail::is_nothrow_swappable_impl<T> {};

template<typename T, typename U, typename = void>
struct is_swappable_with_impl : false_type {};

template<typename T, typename U>
struct is_swappable_with_impl<T, U,
    void_t<decltype(swap(declval<T>(), declval<U>())),
           decltype(swap(declval<U>(), declval<T>()))>> : true_type {};

template<typename T, typename U, typename = void>
struct is_nothrow_swappable_with_impl : false_type {};

template<typename T, typename U>
struct is_nothrow_swappable_with_impl<T, U,
    void_t<decltype(swap(declval<T>(), declval<U>())),
           decltype(swap(declval<U>(), declval<T>()))>>
    : integral_constant<bool,
          noexcept(swap(declval<T>(), declval<U>())) &&
          noexcept(swap(declval<U>(), declval<T>()))> {};

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_swappable_v = is_swappable<T>::value;

template<typename T>
inline constexpr bool is_nothrow_swappable_v = is_nothrow_swappable<T>::value;

template<typename T, typename U>
inline constexpr bool is_swappable_with_v =
    is_swappable_with_impl<T, U>::value;

template<typename T, typename U>
inline constexpr bool is_nothrow_swappable_with_v =
    is_nothrow_swappable_with_impl<T, U>::value;
#endif

template<typename T, typename U>
struct is_swappable_with : is_swappable_with_impl<T, U> {};

template<typename T, typename U>
struct is_nothrow_swappable_with : is_nothrow_swappable_with_impl<T, U> {};

/* ═══════════════════════════════════════════════════════════════
 * is_bounded_array / is_unbounded_array (C++20)
 * ═══════════════════════════════════════════════════════════════*/

template<typename T>
struct is_bounded_array : false_type {};

template<typename T, size_t N>
struct is_bounded_array<T[N]> : true_type {};

template<typename T>
struct is_unbounded_array : false_type {};

template<typename T>
struct is_unbounded_array<T[]> : true_type {};

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_bounded_array_v = is_bounded_array<T>::value;

template<typename T>
inline constexpr bool is_unbounded_array_v = is_unbounded_array<T>::value;
#endif

/* ═══════════════════════════════════════════════════════════════
 * is_scoped_enum (C++23)
 * ═══════════════════════════════════════════════════════════════*/

template<typename T>
struct is_scoped_enum : integral_constant<bool,
    is_enum<T>::value && !is_convertible<T, int>::value
> {};

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_scoped_enum_v = is_scoped_enum<T>::value;
#endif

/* ═══════════════════════════════════════════════════════════════
 * is_aggregate (C++17)
 * ═══════════════════════════════════════════════════════════════*/

template<typename T>
struct is_aggregate : integral_constant<bool, __is_aggregate(T)> {};

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_aggregate_v = is_aggregate<T>::value;
#endif

/* ═══════════════════════════════════════════════════════════════
 * has_unique_object_representations (C++17)
 * ═══════════════════════════════════════════════════════════════*/

template<typename T>
struct has_unique_object_representations
    : integral_constant<bool, __has_unique_object_representations(T)> {};

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool has_unique_object_representations_v =
    has_unique_object_representations<T>::value;
#endif

/* ═══════════════════════════════════════════════════════════════
 * is_nothrow_constructible etc.
 * ═══════════════════════════════════════════════════════════════*/

template<typename T, typename... Args>
struct is_nothrow_constructible
    : integral_constant<bool, __is_nothrow_constructible(T, Args...)> {};

template<typename T>
struct is_nothrow_default_constructible : is_nothrow_constructible<T> {};

template<typename T>
struct is_nothrow_copy_constructible : is_nothrow_constructible<T, const T&> {};

template<typename T>
struct is_nothrow_move_constructible : is_nothrow_constructible<T, T&&> {};

#if __cplusplus >= 201703L
template<typename T, typename... Args>
inline constexpr bool is_nothrow_constructible_v = is_nothrow_constructible<T, Args...>::value;

template<typename T>
inline constexpr bool is_nothrow_default_constructible_v = is_nothrow_default_constructible<T>::value;

template<typename T>
inline constexpr bool is_nothrow_copy_constructible_v = is_nothrow_copy_constructible<T>::value;

template<typename T>
inline constexpr bool is_nothrow_move_constructible_v = is_nothrow_move_constructible<T>::value;
#endif

/* is_nothrow_assignable */
template<typename T, typename U>
struct is_nothrow_assignable : integral_constant<bool, __is_nothrow_assignable(T, U)> {};

template<typename T>
struct is_nothrow_copy_assignable : is_nothrow_assignable<T&, const T&> {};

template<typename T>
struct is_nothrow_move_assignable : is_nothrow_assignable<T&, T&&> {};

#if __cplusplus >= 201703L
template<typename T, typename U>
inline constexpr bool is_nothrow_assignable_v = is_nothrow_assignable<T, U>::value;

template<typename T>
inline constexpr bool is_nothrow_copy_assignable_v = is_nothrow_copy_assignable<T>::value;

template<typename T>
inline constexpr bool is_nothrow_move_assignable_v = is_nothrow_move_assignable<T>::value;
#endif

/* is_nothrow_destructible */
template<typename T>
struct is_nothrow_destructible : detail::is_nothrow_destructible_impl<T> {};

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_nothrow_destructible_v = is_nothrow_destructible<T>::value;
#endif

/* ═══════════════════════════════════════════════════════════════
 * aligned_storage / aligned_union (C++11, deprecated in C++23)
 * ═══════════════════════════════════════════════════════════════*/

template<size_t Len, size_t Align = alignof(max_align_t)>
struct aligned_storage {
    struct type {
        alignas(Align) unsigned char data[Len];
    };
};

template<size_t Len, size_t Align = alignof(max_align_t)>
using aligned_storage_t = typename aligned_storage<Len, Align>::type;

/* ═══════════════════════════════════════════════════════════════
 * extent - 配列の次元サイズ
 * ═══════════════════════════════════════════════════════════════*/

template<typename T, unsigned N = 0>
struct extent : integral_constant<size_t, 0> {};

template<typename T>
struct extent<T[], 0> : integral_constant<size_t, 0> {};

template<typename T, unsigned N>
struct extent<T[], N> : extent<T, N-1> {};

template<typename T, size_t I>
struct extent<T[I], 0> : integral_constant<size_t, I> {};

template<typename T, size_t I, unsigned N>
struct extent<T[I], N> : extent<T, N-1> {};

#if __cplusplus >= 201703L
template<typename T, unsigned N = 0>
inline constexpr size_t extent_v = extent<T, N>::value;
#endif

/* ═══════════════════════════════════════════════════════════════
 * rank - 配列の次元数
 * ═══════════════════════════════════════════════════════════════*/

template<typename T>
struct rank : integral_constant<size_t, 0> {};

template<typename T>
struct rank<T[]> : integral_constant<size_t, rank<T>::value + 1> {};

template<typename T, size_t N>
struct rank<T[N]> : integral_constant<size_t, rank<T>::value + 1> {};

#if __cplusplus >= 201703L
template<typename T>
inline constexpr size_t rank_v = rank<T>::value;
#endif

} /* namespace std */

#endif /* __cplusplus */
#endif /* RINCXX_TYPE_TRAITS_H */
