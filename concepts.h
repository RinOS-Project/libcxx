/*
 * RinOS C++ <concepts> ✿
 * コンセプト (C++20)
 */

#ifndef RINCXX_CONCEPTS_H
#define RINCXX_CONCEPTS_H

/* C++20以上でのみconcepts機能を有効化 */
#if __cplusplus < 202002L && !defined(__cpp_concepts)

/* C++20未満では空のヘッダー */

#else /* C++20以上 */

#include "rincxx.h"
#include "type_traits.h"
#include "cstddef.h"  /* ptrdiff_t */

namespace std {

/* ═══════════════════════════════════════════════════════════════
 * 追加の型特性 (concepts用)
 * ═══════════════════════════════════════════════════════════════*/

namespace detail {

/* is_referenceable */
template<typename T, typename = void>
struct is_referenceable : false_type {};

template<typename T>
struct is_referenceable<T, void_t<T&>> : true_type {};

/* swappable検出 */
template<typename T, typename U, typename = void>
struct is_swappable_with_impl : false_type {};

/* common_reference participates only when the trait exposes a type. */
template<typename T, typename U, typename = void>
struct has_common_reference : false_type {};

template<typename T, typename U>
struct has_common_reference<T, U, void_t<
    typename common_reference<T, U>::type>> : true_type {};

} /* namespace detail */

/* ═══════════════════════════════════════════════════════════════
 * 基本コンセプト (コンセプトをクラステンプレートとして実装)
 *
 * 注: 真のC++20 conceptsはrequires節を使うが、
 *     このコンパイラではサポートされない可能性があるため
 *     constexpr bool + SFINAEで実装
 * ═══════════════════════════════════════════════════════════════*/

/* ───────────────────────────────────────────────────────────────
 * same_as (C++20 concept)
 * ───────────────────────────────────────────────────────────────*/

template<typename T, typename U>
concept same_as = is_same_v<T, U> && is_same_v<U, T>;

/* ───────────────────────────────────────────────────────────────
 * derived_from
 * ───────────────────────────────────────────────────────────────*/

template<typename Derived, typename Base>
inline constexpr bool derived_from =
    is_base_of_v<Base, Derived> &&
    is_convertible<const volatile Derived*, const volatile Base*>::value;

/* ───────────────────────────────────────────────────────────────
 * convertible_to
 * ───────────────────────────────────────────────────────────────*/

namespace detail {
    template<typename From, typename To, typename = void>
    struct is_convertible_to_impl : false_type {};

    template<typename From, typename To>
    struct is_convertible_to_impl<From, To,
        void_t<decltype(static_cast<To>(declval<From>()))>> : true_type {};

    /* C++20's exposition-only boolean-testable accepts comparison proxies
     * only when both the result and its negation are implicitly bool-like. */
    template<typename T>
    concept boolean_testable =
        is_convertible<T, bool>::value &&
        requires(T&& value) {
            requires is_convertible<decltype(!std::forward<T>(value)), bool>::value;
        };
}

template<typename From, typename To>
concept convertible_to = is_convertible<From, To>::value &&
    detail::is_convertible_to_impl<From, To>::value;

/* common_reference_with must be available to the swappable and comparison
 * concepts below.  The aliases are only formed after both trait lookups have
 * proved that a common-reference type exists. */
template<typename T, typename U>
concept common_reference_with = detail::has_common_reference<T, U>::value &&
    detail::has_common_reference<U, T>::value &&
    same_as<common_reference_t<T, U>, common_reference_t<U, T>> &&
    convertible_to<T, common_reference_t<T, U>> &&
    convertible_to<U, common_reference_t<T, U>>;

/* ───────────────────────────────────────────────────────────────
 * integral
 * ───────────────────────────────────────────────────────────────*/

template<typename T>
inline constexpr bool integral = is_integral_v<T>;

/* ───────────────────────────────────────────────────────────────
 * signed_integral
 * ───────────────────────────────────────────────────────────────*/

template<typename T>
inline constexpr bool signed_integral = integral<T> && is_signed_v<T>;

/* ───────────────────────────────────────────────────────────────
 * unsigned_integral
 * ───────────────────────────────────────────────────────────────*/

template<typename T>
inline constexpr bool unsigned_integral = integral<T> && is_unsigned_v<T>;

/* ───────────────────────────────────────────────────────────────
 * floating_point
 * ───────────────────────────────────────────────────────────────*/

template<typename T>
inline constexpr bool floating_point = is_floating_point_v<T>;

/* ───────────────────────────────────────────────────────────────
 * destructible
 * ───────────────────────────────────────────────────────────────*/

template<typename T>
concept destructible = is_nothrow_destructible_v<T>;

/* ───────────────────────────────────────────────────────────────
 * constructible_from
 * ───────────────────────────────────────────────────────────────*/

template<typename T, typename... Args>
concept constructible_from =
    destructible<T> && is_constructible_v<T, Args...>;

/* ───────────────────────────────────────────────────────────────
 * default_initializable
 * ───────────────────────────────────────────────────────────────*/

template<typename T>
concept default_initializable =
    constructible_from<T> && is_default_constructible_v<T>;

/* ───────────────────────────────────────────────────────────────
 * move_constructible
 * ───────────────────────────────────────────────────────────────*/

template<typename T>
concept move_constructible =
    constructible_from<T, T> && convertible_to<T, T>;

/* ───────────────────────────────────────────────────────────────
 * copy_constructible
 * ───────────────────────────────────────────────────────────────*/

template<typename T>
concept copy_constructible =
    move_constructible<T> &&
    constructible_from<T, T&> && convertible_to<T&, T> &&
    constructible_from<T, const T&> && convertible_to<const T&, T> &&
    constructible_from<T, const T> && convertible_to<const T, T>;

/* ───────────────────────────────────────────────────────────────
 * assignable_from
 * ───────────────────────────────────────────────────────────────*/

template<typename LHS, typename RHS>
concept assignable_from =
    is_lvalue_reference_v<LHS> &&
    is_assignable_v<LHS, RHS> &&
    requires(LHS lhs, RHS&& rhs) {
        { lhs = std::forward<RHS>(rhs) } -> same_as<LHS>;
    };

/* ───────────────────────────────────────────────────────────────
 * swappable (簡易版)
 * is_swappable_impl is defined in type_traits.h
 * ───────────────────────────────────────────────────────────────*/

template<typename T>
concept swappable = requires(T& left, T& right) {
    swap(left, right);
};

template<typename T, typename U>
concept swappable_with = common_reference_with<
    add_lvalue_reference_t<const remove_reference_t<T>>,
    add_lvalue_reference_t<const remove_reference_t<U>>> &&
    requires(T& left, T& right, U& other_left, U& other_right) {
        swap(left, right);
        swap(other_left, other_right);
        swap(left, other_left);
        swap(other_left, left);
    };

/* ───────────────────────────────────────────────────────────────
 * movable
 * ───────────────────────────────────────────────────────────────*/

template<typename T>
concept movable =
    is_object<T>::value &&
    move_constructible<T> &&
    assignable_from<T&, T> &&
    swappable<T>;

/* ───────────────────────────────────────────────────────────────
 * copyable
 * ───────────────────────────────────────────────────────────────*/

template<typename T>
concept copyable =
    copy_constructible<T> &&
    movable<T> &&
    assignable_from<T&, T&> &&
    assignable_from<T&, const T&> &&
    assignable_from<T&, const T>;

/* ───────────────────────────────────────────────────────────────
 * semiregular
 * ───────────────────────────────────────────────────────────────*/

template<typename T>
concept semiregular =
    copyable<T> && default_initializable<T>;

/* ───────────────────────────────────────────────────────────────
 * equality_comparable (簡易版)
 * ───────────────────────────────────────────────────────────────*/

namespace detail {
    template<typename T, typename U = T, typename = void>
    struct is_equality_comparable_impl : false_type {};

template<typename T, typename U>
struct is_equality_comparable_impl<T, U, void_t<
        decltype(declval<T>() == declval<U>()),
        decltype(declval<T>() != declval<U>())>>
    : integral_constant<bool,
        boolean_testable<decltype(declval<T>() == declval<U>())> &&
        boolean_testable<decltype(declval<T>() != declval<U>())>> {};
}

template<typename T>
concept equality_comparable = detail::is_equality_comparable_impl<T>::value;

template<typename T, typename U>
concept equality_comparable_with =
    equality_comparable<T> && equality_comparable<U> &&
    common_reference_with<
        add_lvalue_reference_t<const remove_reference_t<T>>,
        add_lvalue_reference_t<const remove_reference_t<U>>> &&
    detail::is_equality_comparable_impl<T, U>::value &&
    detail::is_equality_comparable_impl<U, T>::value;

/* ───────────────────────────────────────────────────────────────
 * regular
 * ───────────────────────────────────────────────────────────────*/

template<typename T>
inline constexpr bool regular = semiregular<T> && equality_comparable<T>;

/* ───────────────────────────────────────────────────────────────
 * totally_ordered (簡易版)
 * ───────────────────────────────────────────────────────────────*/

namespace detail {
    template<typename T, typename U = T, typename = void>
    struct is_totally_ordered_impl : false_type {};

template<typename T, typename U>
struct is_totally_ordered_impl<T, U, void_t<
        decltype(declval<T>() < declval<U>()),
        decltype(declval<T>() > declval<U>()),
        decltype(declval<T>() <= declval<U>()),
        decltype(declval<T>() >= declval<U>())>>
    : integral_constant<bool,
        boolean_testable<decltype(declval<T>() < declval<U>())> &&
        boolean_testable<decltype(declval<T>() > declval<U>())> &&
        boolean_testable<decltype(declval<T>() <= declval<U>())> &&
        boolean_testable<decltype(declval<T>() >= declval<U>())>> {};
}

template<typename T>
concept totally_ordered =
    equality_comparable<T> && detail::is_totally_ordered_impl<T>::value;

template<typename T, typename U>
concept totally_ordered_with =
    totally_ordered<T> && totally_ordered<U> &&
    equality_comparable_with<T, U> &&
    detail::is_totally_ordered_impl<T, U>::value &&
    detail::is_totally_ordered_impl<U, T>::value;

/* ═══════════════════════════════════════════════════════════════
 * Callable コンセプト
 * ═══════════════════════════════════════════════════════════════*/

/* ───────────────────────────────────────────────────────────────
 * invocable
 * is_invocable_impl is defined in type_traits.h
 * ───────────────────────────────────────────────────────────────*/

template<typename F, typename... Args>
inline constexpr bool invocable = is_invocable<F, Args...>::value;

/* ───────────────────────────────────────────────────────────────
 * regular_invocable
 * (意味論的にはequality-preservingを要求するが、構文上は同じ)
 * ───────────────────────────────────────────────────────────────*/

template<typename F, typename... Args>
inline constexpr bool regular_invocable = invocable<F, Args...>;

/* ───────────────────────────────────────────────────────────────
 * predicate
 * ───────────────────────────────────────────────────────────────*/

namespace detail {
    template<typename F, typename... Args>
    struct is_predicate_impl {
    private:
        template<typename Fn, typename... As>
        static auto test(int) -> decltype(
            static_cast<bool>(declval<Fn>()(declval<As>()...)), true_type{});

        template<typename, typename...>
        static auto test(...) -> false_type;

    public:
        static constexpr bool value = decltype(test<F, Args...>(0))::value;
    };
}

template<typename F, typename... Args>
inline constexpr bool predicate =
    regular_invocable<F, Args...> && detail::is_predicate_impl<F, Args...>::value;

/* ───────────────────────────────────────────────────────────────
 * relation
 * ───────────────────────────────────────────────────────────────*/

template<typename R, typename T, typename U>
inline constexpr bool relation =
    predicate<R, T, T> && predicate<R, U, U> &&
    predicate<R, T, U> && predicate<R, U, T>;

/* ───────────────────────────────────────────────────────────────
 * equivalence_relation
 * ───────────────────────────────────────────────────────────────*/

template<typename R, typename T, typename U>
inline constexpr bool equivalence_relation = relation<R, T, U>;

/* ───────────────────────────────────────────────────────────────
 * strict_weak_order
 * ───────────────────────────────────────────────────────────────*/

template<typename R, typename T, typename U>
inline constexpr bool strict_weak_order = relation<R, T, U>;

/* is_object, is_function are defined in type_traits.h */
/* common_reference, common_reference_t are defined in type_traits.h */

} /* namespace std */

/* ═══════════════════════════════════════════════════════════════
 * C++20 concept キーワードエミュレーション (マクロ)
 *
 * 注: 真のrequires式は使えないが、
 *     SFINAEで似たパターンを実現できる
 * ═══════════════════════════════════════════════════════════════*/

/* コンセプト制約のenable_if風マクロ */
#define RIN_REQUIRES(...) \
    std::enable_if_t<(__VA_ARGS__), int> = 0

#define RIN_REQUIRES_T(...) \
    typename = std::enable_if_t<(__VA_ARGS__)>

#endif /* C++20以上 */

#endif /* RINCXX_CONCEPTS_H */
