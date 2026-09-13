/*
 * RinOS C++ <utility> ✿
 * ユーティリティライブラリ
 */

#ifndef RINCXX_UTILITY_H
#define RINCXX_UTILITY_H

#include "rincxx.h"
#include "type_traits.h"
#include "limits.h"
#include "__tuple_fwd.h"
#if __cplusplus >= 202002L
#include "compare.h"
#endif

#ifdef __cplusplus

namespace std {

/* ═══════════════════════════════════════════════════════════════
 * move, forward (type_traits.hから再エクスポート)
 * ═══════════════════════════════════════════════════════════════*/

/* すでにtype_traits.hで定義済み */

/* ═══════════════════════════════════════════════════════════════
 * swap - rincxx.hで既に定義済み。配列版のみ追加
 * ═══════════════════════════════════════════════════════════════*/

/* 基本swap は rincxx.h で定義済み */

template<typename T, size_t N>
#if __cplusplus >= 201402L
constexpr
#else
inline
#endif
void swap(T (&a)[N], T (&b)[N])
    noexcept(noexcept(swap(a[0], b[0]))) {
    for (size_t i = 0; i < N; ++i) {
        swap(a[i], b[i]);
    }
}

/* ═══════════════════════════════════════════════════════════════
 * exchange (C++14)
 * ═══════════════════════════════════════════════════════════════*/

template<typename T, typename U = T>
T exchange(T& obj, U&& new_value) {
    T old_value = std::move(obj);
    obj = std::forward<U>(new_value);
    return old_value;
}

/* ═══════════════════════════════════════════════════════════════
 * piecewise_construct (pairより前に定義)
 * ═══════════════════════════════════════════════════════════════*/

struct piecewise_construct_t {
    explicit piecewise_construct_t() = default;
};

#if __cplusplus >= 201703L
inline constexpr piecewise_construct_t piecewise_construct{};
#else
constexpr piecewise_construct_t piecewise_construct{};
#endif

/* tuple の前方宣言 */
template<typename...> class tuple;

/* integer_sequence / index_sequence / make_index_sequence - pair より前に必要 */
template<typename T, T... Ints>
struct integer_sequence {
    using value_type = T;
    static constexpr size_t size() noexcept { return sizeof...(Ints); }
};

template<size_t... Ints>
using index_sequence = integer_sequence<size_t, Ints...>;

/* make_integer_sequence実装 */
#if __has_builtin(__make_integer_seq)
    template<typename T, T N>
    using make_integer_sequence = __make_integer_seq<integer_sequence, T, N>;
#elif defined(__GNUC__) && __GNUC__ >= 8
    template<typename T, T N>
    using make_integer_sequence = integer_sequence<T, __integer_pack(N)...>;
#else
    namespace detail {
        template<typename T, T N, T... Is>
        struct make_seq_impl {
            using type = typename make_seq_impl<T, N - 1, N - 1, Is...>::type;
        };
        template<typename T, T... Is>
        struct make_seq_impl<T, T(0), Is...> {
            using type = integer_sequence<T, Is...>;
        };
    }
    template<typename T, T N>
    using make_integer_sequence = typename detail::make_seq_impl<T, N>::type;
#endif

template<size_t N>
using make_index_sequence = make_integer_sequence<size_t, N>;

template<typename... T>
using index_sequence_for = make_index_sequence<sizeof...(T)>;

/* get<I>(tuple) の前方宣言 - 定義は tuple.h */
template<size_t I, typename... Types>
constexpr tuple_element_t<I, tuple<Types...>>&
get(tuple<Types...>& t) noexcept;

template<size_t I, typename... Types>
constexpr const tuple_element_t<I, tuple<Types...>>&
get(const tuple<Types...>& t) noexcept;

template<size_t I, typename... Types>
constexpr tuple_element_t<I, tuple<Types...>>&&
get(tuple<Types...>&& t) noexcept;

template<size_t I, typename... Types>
constexpr const tuple_element_t<I, tuple<Types...>>&&
get(const tuple<Types...>&& t) noexcept;

/* ═══════════════════════════════════════════════════════════════
 * pair - ガード付き（map.hで定義されている可能性がある）
 * ═══════════════════════════════════════════════════════════════*/

#ifndef RINCXX_PAIR_DEFINED
#define RINCXX_PAIR_DEFINED

template<typename T1, typename T2>
struct pair {
    using first_type = T1;
    using second_type = T2;
    
    T1 first;
    T2 second;
    
    /* デフォルトコンストラクタ */
    constexpr pair() : first(), second() {}

    /* 値コンストラクタ - const ref版 (コピー可能な型のみ) */
    template<typename V1 = T1, typename V2 = T2,
             typename = typename enable_if<
                 is_copy_constructible<V1>::value &&
                 is_copy_constructible<V2>::value
             >::type>
    constexpr pair(const T1& x, const T2& y) : first(x), second(y) {}

    /* 変換コンストラクタ - forwarding (ムーブのみの型もサポート) */
    template<typename U1, typename U2,
             typename = typename enable_if<
                 is_constructible<T1, U1&&>::value &&
                 is_constructible<T2, U2&&>::value
             >::type,
             typename = void>
    constexpr pair(U1&& x, U2&& y)
        : first(std::forward<U1>(x)), second(std::forward<U2>(y)) {}

    /* Key-only constructor for map::operator[] - constructs second with default */
    template<typename U1,
             typename = typename enable_if<
                 is_constructible<T1, U1>::value &&
                 is_default_constructible<T2>::value
             >::type>
    explicit constexpr pair(U1&& x)
        : first(std::forward<U1>(x)), second() {}
    
    /* コピーコンストラクタ */
    pair(const pair&) = default;
    pair(pair&&) = default;
    
    /* 変換コピーコンストラクタ - U1,U2がT1,T2に変換可能な場合のみ */
    template<typename U1, typename U2,
             typename = typename enable_if<
                 is_constructible<T1, const U1&>::value &&
                 is_constructible<T2, const U2&>::value
             >::type>
    constexpr pair(const pair<U1, U2>& p) : first(p.first), second(p.second) {}

    template<typename U1, typename U2,
             typename = typename enable_if<
                 is_constructible<T1, U1&&>::value &&
                 is_constructible<T2, U2&&>::value
             >::type>
    constexpr pair(pair<U1, U2>&& p)
        : first(std::forward<U1>(p.first)), second(std::forward<U2>(p.second)) {}

    /* piecewise_construct コンストラクタ */
private:
    template<typename Tuple1, typename Tuple2, size_t... I1, size_t... I2>
    constexpr pair(piecewise_construct_t, Tuple1&& t1, Tuple2&& t2,
                   index_sequence<I1...>, index_sequence<I2...>)
        : first(get<I1>(std::forward<Tuple1>(t1))...),
          second(get<I2>(std::forward<Tuple2>(t2))...) {}
public:
    template<typename... Args1, typename... Args2>
    constexpr pair(piecewise_construct_t pc, tuple<Args1...> first_args, tuple<Args2...> second_args)
        : pair(pc, std::move(first_args), std::move(second_args),
               make_index_sequence<sizeof...(Args1)>{},
               make_index_sequence<sizeof...(Args2)>{}) {}

    /* 代入 */
    pair& operator=(const pair& other) {
        first = other.first;
        second = other.second;
        return *this;
    }
    
    pair& operator=(pair&& other) noexcept {
        first = std::move(other.first);
        second = std::move(other.second);
        return *this;
    }
    
    template<typename U1, typename U2>
    pair& operator=(const pair<U1, U2>& other) {
        first = other.first;
        second = other.second;
        return *this;
    }
    
    template<typename U1, typename U2>
    pair& operator=(pair<U1, U2>&& other) {
        first = std::forward<U1>(other.first);
        second = std::forward<U2>(other.second);
        return *this;
    }
    
    /* swap */
    void swap(pair& other) noexcept {
        using std::swap;
        swap(first, other.first);
        swap(second, other.second);
    }
};

/* pair比較 */
template<typename T1, typename T2>
constexpr bool operator==(const pair<T1, T2>& lhs, const pair<T1, T2>& rhs) {
    return lhs.first == rhs.first && lhs.second == rhs.second;
}

template<typename T1, typename T2>
constexpr bool operator!=(const pair<T1, T2>& lhs, const pair<T1, T2>& rhs) {
    return !(lhs == rhs);
}

template<typename T1, typename T2>
constexpr bool operator<(const pair<T1, T2>& lhs, const pair<T1, T2>& rhs) {
    return lhs.first < rhs.first || (!(rhs.first < lhs.first) && lhs.second < rhs.second);
}

template<typename T1, typename T2>
constexpr bool operator<=(const pair<T1, T2>& lhs, const pair<T1, T2>& rhs) {
    return !(rhs < lhs);
}

template<typename T1, typename T2>
constexpr bool operator>(const pair<T1, T2>& lhs, const pair<T1, T2>& rhs) {
    return rhs < lhs;
}

template<typename T1, typename T2>
constexpr bool operator>=(const pair<T1, T2>& lhs, const pair<T1, T2>& rhs) {
    return !(lhs < rhs);
}

#if __cplusplus >= 202002L
template<typename T1, typename T2, typename U1, typename U2>
constexpr auto operator<=>(const pair<T1, T2>& lhs,
                           const pair<U1, U2>& rhs)
    -> common_comparison_category_t<
        detail::synth_three_way_result_t<T1, U1>,
        detail::synth_three_way_result_t<T2, U2>> {
    using category = common_comparison_category_t<
        detail::synth_three_way_result_t<T1, U1>,
        detail::synth_three_way_result_t<T2, U2>>;
    const auto first = detail::synth_three_way(lhs.first, rhs.first);
    if (first != 0) return static_cast<category>(first);
    return static_cast<category>(detail::synth_three_way(lhs.second,
                                                          rhs.second));
}
#endif

/* make_pair */
template<typename T1, typename T2>
constexpr pair<typename decay<T1>::type, typename decay<T2>::type>
make_pair(T1&& x, T2&& y) {
    return pair<typename decay<T1>::type, typename decay<T2>::type>(
        std::forward<T1>(x), std::forward<T2>(y));
}

/* pair swap */
template<typename T1, typename T2>
void swap(pair<T1, T2>& lhs, pair<T1, T2>& rhs) noexcept {
    lhs.swap(rhs);
}

#endif /* RINCXX_PAIR_DEFINED */

/* ═══════════════════════════════════════════════════════════════
 * get for pair (C++11) - tuple interface
 * ═══════════════════════════════════════════════════════════════*/

template<size_t I> struct _pair_get;

template<>
struct _pair_get<0> {
    template<typename T1, typename T2>
    static constexpr T1& get(pair<T1, T2>& p) noexcept { return p.first; }

    template<typename T1, typename T2>
    static constexpr const T1& get(const pair<T1, T2>& p) noexcept { return p.first; }

    template<typename T1, typename T2>
    static constexpr T1&& get(pair<T1, T2>&& p) noexcept { return std::forward<T1>(p.first); }

    template<typename T1, typename T2>
    static constexpr const T1&& get(const pair<T1, T2>&& p) noexcept { return std::forward<const T1>(p.first); }
};

template<>
struct _pair_get<1> {
    template<typename T1, typename T2>
    static constexpr T2& get(pair<T1, T2>& p) noexcept { return p.second; }

    template<typename T1, typename T2>
    static constexpr const T2& get(const pair<T1, T2>& p) noexcept { return p.second; }

    template<typename T1, typename T2>
    static constexpr T2&& get(pair<T1, T2>&& p) noexcept { return std::forward<T2>(p.second); }

    template<typename T1, typename T2>
    static constexpr const T2&& get(const pair<T1, T2>&& p) noexcept { return std::forward<const T2>(p.second); }
};

/* get<I>(pair) */
template<size_t I, typename T1, typename T2>
constexpr auto get(pair<T1, T2>& p) noexcept
    -> decltype(_pair_get<I>::get(p)) {
    return _pair_get<I>::get(p);
}

template<size_t I, typename T1, typename T2>
constexpr auto get(const pair<T1, T2>& p) noexcept
    -> decltype(_pair_get<I>::get(p)) {
    return _pair_get<I>::get(p);
}

template<size_t I, typename T1, typename T2>
constexpr auto get(pair<T1, T2>&& p) noexcept
    -> decltype(_pair_get<I>::get(std::move(p))) {
    return _pair_get<I>::get(std::move(p));
}

template<size_t I, typename T1, typename T2>
constexpr auto get(const pair<T1, T2>&& p) noexcept
    -> decltype(_pair_get<I>::get(std::move(p))) {
    return _pair_get<I>::get(std::move(p));
}

/* get<T>(pair) - get by type */
template<typename T, typename U>
constexpr T& get(pair<T, U>& p) noexcept { return p.first; }

template<typename T, typename U>
constexpr const T& get(const pair<T, U>& p) noexcept { return p.first; }

template<typename T, typename U>
constexpr T&& get(pair<T, U>&& p) noexcept { return std::forward<T>(p.first); }

template<typename T, typename U>
constexpr T& get(pair<U, T>& p) noexcept { return p.second; }

template<typename T, typename U>
constexpr const T& get(const pair<U, T>& p) noexcept { return p.second; }

template<typename T, typename U>
constexpr T&& get(pair<U, T>&& p) noexcept { return std::forward<T>(p.second); }

/* ═══════════════════════════════════════════════════════════════
 * tuple_size, tuple_element for pair
 * cv-qualified 版もここで明示的に定義（aggregate 型との干渉を避けるため）
 * ═══════════════════════════════════════════════════════════════*/

template<typename T1, typename T2>
struct tuple_size<pair<T1, T2>> : integral_constant<size_t, 2> {};

template<typename T1, typename T2>
struct tuple_size<const pair<T1, T2>> : integral_constant<size_t, 2> {};

template<typename T1, typename T2>
struct tuple_size<volatile pair<T1, T2>> : integral_constant<size_t, 2> {};

template<typename T1, typename T2>
struct tuple_size<const volatile pair<T1, T2>> : integral_constant<size_t, 2> {};

template<typename T1, typename T2>
struct tuple_element<0, pair<T1, T2>> { using type = T1; };

template<typename T1, typename T2>
struct tuple_element<1, pair<T1, T2>> { using type = T2; };

template<typename T1, typename T2>
struct tuple_element<0, const pair<T1, T2>> { using type = const T1; };

template<typename T1, typename T2>
struct tuple_element<1, const pair<T1, T2>> { using type = const T2; };

template<typename T1, typename T2>
struct tuple_element<0, volatile pair<T1, T2>> { using type = volatile T1; };

template<typename T1, typename T2>
struct tuple_element<1, volatile pair<T1, T2>> { using type = volatile T2; };

template<typename T1, typename T2>
struct tuple_element<0, const volatile pair<T1, T2>> { using type = const volatile T1; };

template<typename T1, typename T2>
struct tuple_element<1, const volatile pair<T1, T2>> { using type = const volatile T2; };

/* piecewise_construct_t は pair の前で定義済み */

/* ═══════════════════════════════════════════════════════════════
 * in_place (C++17)
 * ═══════════════════════════════════════════════════════════════*/

struct in_place_t {
    explicit in_place_t() = default;
};

#if __cplusplus >= 201703L
inline constexpr in_place_t in_place{};
#endif

template<typename T>
struct in_place_type_t {
    explicit in_place_type_t() = default;
};

#if __cplusplus >= 201703L
template<typename T>
inline constexpr in_place_type_t<T> in_place_type{};
#endif

template<size_t I>
struct in_place_index_t {
    explicit in_place_index_t() = default;
};

#if __cplusplus >= 201703L
template<size_t I>
inline constexpr in_place_index_t<I> in_place_index{};
#endif

/* integer_sequence/make_index_sequence は pair より前で定義済み */

/* ═══════════════════════════════════════════════════════════════
 * as_const (C++17)
 * ═══════════════════════════════════════════════════════════════*/

template<typename T>
constexpr add_const_t<T>& as_const(T& t) noexcept {
    return t;
}

template<typename T>
void as_const(const T&&) = delete;

/* ═══════════════════════════════════════════════════════════════
 * move_if_noexcept (C++11)
 * ═══════════════════════════════════════════════════════════════*/

template<typename T>
constexpr conditional_t<!is_nothrow_move_constructible<T>::value &&
                            is_copy_constructible<T>::value,
                        const T&, T&&>
move_if_noexcept(T& value) noexcept {
    return std::move(value);
}

/* ═══════════════════════════════════════════════════════════════
 * to_underlying (C++23)
 * ═══════════════════════════════════════════════════════════════*/

#if __cplusplus > 202002L
template<typename E>
constexpr typename underlying_type<E>::type to_underlying(E e) noexcept {
    return static_cast<typename underlying_type<E>::type>(e);
}
#endif

/* ═══════════════════════════════════════════════════════════════
 * forward_like (C++23)
 * ═══════════════════════════════════════════════════════════════*/

#if __cplusplus > 202002L
template<typename T, typename U>
constexpr detail::copy_cvref_t<T, remove_reference_t<U>> forward_like(U&& value)
    noexcept(noexcept(static_cast<detail::copy_cvref_t<
        T, remove_reference_t<U>>>(value))) {
    using result_type = detail::copy_cvref_t<T, remove_reference_t<U>>;
    return static_cast<result_type>(value);
}

[[noreturn]] constexpr void unreachable() noexcept {
    __builtin_unreachable();
}
#endif

/* ═══════════════════════════════════════════════════════════════
 * cmp_equal, cmp_less, etc. (C++20)
 * ═══════════════════════════════════════════════════════════════*/

#if __cplusplus >= 202002L
template<typename T, typename U>
constexpr bool cmp_equal(T t, U u) noexcept {
    if constexpr (is_signed<T>::value == is_signed<U>::value) {
        return t == u;
    } else if constexpr (is_signed<T>::value) {
        return t >= 0 && static_cast<make_unsigned_t<T>>(t) == u;
    } else {
        return u >= 0 && t == static_cast<make_unsigned_t<U>>(u);
    }
}

template<typename T, typename U>
constexpr bool cmp_not_equal(T t, U u) noexcept {
    return !cmp_equal(t, u);
}

template<typename T, typename U>
constexpr bool cmp_less(T t, U u) noexcept {
    if constexpr (is_signed<T>::value == is_signed<U>::value) {
        return t < u;
    } else if constexpr (is_signed<T>::value) {
        return t < 0 || static_cast<make_unsigned_t<T>>(t) < u;
    } else {
        return u >= 0 && t < static_cast<make_unsigned_t<U>>(u);
    }
}

template<typename T, typename U>
constexpr bool cmp_greater(T t, U u) noexcept {
    return cmp_less(u, t);
}

template<typename T, typename U>
constexpr bool cmp_less_equal(T t, U u) noexcept {
    return !cmp_greater(t, u);
}

template<typename T, typename U>
constexpr bool cmp_greater_equal(T t, U u) noexcept {
    return !cmp_less(t, u);
}

/* P1959 integer range conversion helper (C++20).  Keep all comparisons in
 * the source domain so converting a wider destination bound cannot narrow or
 * wrap before the range check. */
template<typename R, typename T,
         enable_if_t<is_integral<R>::value && is_integral<T>::value, int> = 0>
constexpr bool in_range(T value) noexcept {
    if constexpr (is_signed<T>::value == is_signed<R>::value) {
        return value >= numeric_limits<R>::min()
            && value <= numeric_limits<R>::max();
    } else if constexpr (is_signed<T>::value) {
        if (value < 0) return false;
        using unsigned_source = make_unsigned_t<T>;
        return static_cast<unsigned_source>(value)
            <= numeric_limits<R>::max();
    } else {
        using unsigned_destination = make_unsigned_t<R>;
        return value <= static_cast<unsigned_destination>(
            numeric_limits<R>::max());
    }
}
#endif

/* ═══════════════════════════════════════════════════════════════
 * rel_ops (deprecated but sometimes needed)
 * ═══════════════════════════════════════════════════════════════*/

namespace rel_ops {
    template<typename T>
    bool operator!=(const T& x, const T& y) { return !(x == y); }
    
    template<typename T>
    bool operator>(const T& x, const T& y) { return y < x; }
    
    template<typename T>
    bool operator<=(const T& x, const T& y) { return !(y < x); }
    
    template<typename T>
    bool operator>=(const T& x, const T& y) { return !(x < y); }
}

} /* namespace std */

#endif /* __cplusplus */
#endif /* RINCXX_UTILITY_H */
