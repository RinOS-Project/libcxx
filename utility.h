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
#if __cplusplus > 202302L || defined(RIN_ENABLE_CXX26_FUNCTION_WRAPPERS)
#include "invoke.h"
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
                 is_constructible<T1, U1&>::value &&
                 is_constructible<T2, U2&>::value
             >::type,
             typename = void>
    constexpr pair(pair<U1, U2>& p) : first(p.first), second(p.second) {}

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

#if __cplusplus >= 201703L
template<typename T1,typename T2>
pair(T1,T2)->pair<typename decay<T1>::type,typename decay<T2>::type>;
#endif

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

#if __cplusplus > 202002L
namespace detail {
template<typename Left, typename Right,
         template<typename> class LeftQual,
         template<typename> class RightQual, typename = void>
struct pair_basic_common_reference {};

template<typename L1, typename L2, typename R1, typename R2,
         template<typename> class LeftQual,
         template<typename> class RightQual>
struct pair_basic_common_reference<
    pair<L1, L2>, pair<R1, R2>, LeftQual, RightQual,
    void_t<common_reference_t<LeftQual<L1>, RightQual<R1>>,
           common_reference_t<LeftQual<L2>, RightQual<R2>>>> {
    using type = pair<common_reference_t<LeftQual<L1>, RightQual<R1>>,
                      common_reference_t<LeftQual<L2>, RightQual<R2>>>;
};
} // namespace detail

template<typename L1, typename L2, typename R1, typename R2,
         template<typename> class LeftQual,
         template<typename> class RightQual>
struct basic_common_reference<pair<L1, L2>, pair<R1, R2>,
                              LeftQual, RightQual>
    : detail::pair_basic_common_reference<
          pair<L1, L2>, pair<R1, R2>, LeftQual, RightQual> {};
#endif

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

#if __cplusplus > 202302L || defined(RIN_ENABLE_CXX26_FUNCTION_WRAPPERS)
template<auto X, class T = decltype(X)>
struct constant_wrapper;

namespace detail {

template<class T>
concept __constant_wrapper_parameter = requires {
    typename constant_wrapper<T::value>;
};

template<auto Value, class... Args>
inline constexpr bool __constant_wrapper_call_is_constant = [] {
    if constexpr ((__constant_wrapper_parameter<remove_cvref_t<Args>> && ...)) {
        return requires {
            typename constant_wrapper<std::invoke(
                Value, remove_cvref_t<Args>::value...)>;
        };
    } else {
        return false;
    }
}();

template<auto Value, class... Args>
concept __constant_wrapper_callable =
    __constant_wrapper_call_is_constant<Value, Args...> || requires {
        std::invoke(Value, declval<Args>()...);
    };

template<auto Value, class... Args>
consteval bool __constant_wrapper_call_noexcept() {
    if constexpr (__constant_wrapper_call_is_constant<Value, Args...>) {
        return true;
    } else if constexpr (requires { std::invoke(Value, declval<Args>()...); }) {
        return noexcept(std::invoke(Value, declval<Args>()...));
    } else {
        return false;
    }
}

template<auto Value, class... Args>
inline constexpr bool __constant_wrapper_subscript_is_constant = [] {
    if constexpr (sizeof...(Args) != 0 &&
                  (__constant_wrapper_parameter<remove_cvref_t<Args>> && ...)) {
        return requires {
            typename constant_wrapper<
                (Value[remove_cvref_t<Args>::value...])>;
        };
    } else {
        return false;
    }
}();

template<auto Value, class... Args>
concept __constant_wrapper_subscriptable =
    __constant_wrapper_subscript_is_constant<Value, Args...> ||
    (sizeof...(Args) != 0 && requires {
        Value[declval<Args>()...];
    });

template<auto Value, class... Args>
consteval bool __constant_wrapper_subscript_noexcept() {
    if constexpr (__constant_wrapper_subscript_is_constant<Value, Args...>) {
        return true;
    } else if constexpr (sizeof...(Args) != 0 && requires {
                             Value[declval<Args>()...];
                         }) {
        return noexcept(Value[declval<Args>()...]);
    } else {
        return false;
    }
}

} // namespace detail

/* These hidden friends participate through constant_wrapper's base class and
 * only accept arguments whose static value is itself a valid NTTP. */
struct __constant_wrapper_operators {
#define __RIN_CW_UNARY(OP)                                                   \
    template<detail::__constant_wrapper_parameter T>                        \
    friend constexpr auto operator OP(T) noexcept                           \
        -> constant_wrapper<(OP T::value)> { return {}; }

    __RIN_CW_UNARY(+)
    __RIN_CW_UNARY(-)
    __RIN_CW_UNARY(~)
    __RIN_CW_UNARY(!)
    __RIN_CW_UNARY(&)
    __RIN_CW_UNARY(*)
#undef __RIN_CW_UNARY

#define __RIN_CW_BINARY(OP)                                                  \
    template<detail::__constant_wrapper_parameter L,                        \
             detail::__constant_wrapper_parameter R>                        \
    friend constexpr auto operator OP(L, R) noexcept                        \
        -> constant_wrapper<(L::value OP R::value)> { return {}; }

    __RIN_CW_BINARY(+)
    __RIN_CW_BINARY(-)
    __RIN_CW_BINARY(*)
    __RIN_CW_BINARY(/)
    __RIN_CW_BINARY(%)
    __RIN_CW_BINARY(<<)
    __RIN_CW_BINARY(>>)
    __RIN_CW_BINARY(&)
    __RIN_CW_BINARY(|)
    __RIN_CW_BINARY(^)
#undef __RIN_CW_BINARY

    template<detail::__constant_wrapper_parameter L,
             detail::__constant_wrapper_parameter R>
        requires (!is_constructible_v<bool, decltype(L::value)> ||
                  !is_constructible_v<bool, decltype(R::value)>)
    friend constexpr auto operator&&(L, R) noexcept
        -> constant_wrapper<(L::value && R::value)> { return {}; }

    template<detail::__constant_wrapper_parameter L,
             detail::__constant_wrapper_parameter R>
        requires (!is_constructible_v<bool, decltype(L::value)> ||
                  !is_constructible_v<bool, decltype(R::value)>)
    friend constexpr auto operator||(L, R) noexcept
        -> constant_wrapper<(L::value || R::value)> { return {}; }

#define __RIN_CW_COMPARE(OP)                                                 \
    template<detail::__constant_wrapper_parameter L,                        \
             detail::__constant_wrapper_parameter R>                        \
    friend constexpr auto operator OP(L, R) noexcept                        \
        -> constant_wrapper<(L::value OP R::value)> { return {}; }

    __RIN_CW_COMPARE(<=>)
    __RIN_CW_COMPARE(<)
    __RIN_CW_COMPARE(<=)
    __RIN_CW_COMPARE(==)
    __RIN_CW_COMPARE(!=)
    __RIN_CW_COMPARE(>)
    __RIN_CW_COMPARE(>=)
#undef __RIN_CW_COMPARE

    template<detail::__constant_wrapper_parameter L,
             detail::__constant_wrapper_parameter R>
    friend constexpr auto operator,(L, R) noexcept = delete;

    template<detail::__constant_wrapper_parameter L,
             detail::__constant_wrapper_parameter R>
    friend constexpr auto operator->*(L, R) noexcept
        -> constant_wrapper<(L::value ->* R::value)> { return {}; }
};

template<auto X, class T>
struct constant_wrapper : __constant_wrapper_operators {
    static_assert(is_same<T, decltype(X)>::value,
                  "constant_wrapper's value type is fixed by its argument");
    static constexpr decltype(auto) value = (X);
    using type = constant_wrapper;
    using value_type = decltype(X);

    template<detail::__constant_wrapper_parameter R>
    constexpr auto operator=(R) const noexcept
        -> constant_wrapper<(value = R::value)> { return {}; }

    constexpr operator decltype(value)() const noexcept { return value; }

    template<class Self = constant_wrapper>
        requires is_same<Self, constant_wrapper>::value
    constexpr auto operator++() const noexcept
        -> constant_wrapper<(++Self::value)> { return {}; }
    template<class Self = constant_wrapper>
        requires is_same<Self, constant_wrapper>::value
    constexpr auto operator++(int) const noexcept
        -> constant_wrapper<(Self::value++)> { return {}; }
    template<class Self = constant_wrapper>
        requires is_same<Self, constant_wrapper>::value
    constexpr auto operator--() const noexcept
        -> constant_wrapper<(--Self::value)> { return {}; }
    template<class Self = constant_wrapper>
        requires is_same<Self, constant_wrapper>::value
    constexpr auto operator--(int) const noexcept
        -> constant_wrapper<(Self::value--)> { return {}; }

#define __RIN_CW_COMPOUND(OP)                                                \
    template<detail::__constant_wrapper_parameter R>                        \
    constexpr auto operator OP(R) const noexcept                            \
        -> constant_wrapper<(value OP R::value)> { return {}; }

    __RIN_CW_COMPOUND(+=)
    __RIN_CW_COMPOUND(-=)
    __RIN_CW_COMPOUND(*=)
    __RIN_CW_COMPOUND(/=)
    __RIN_CW_COMPOUND(%=)
    __RIN_CW_COMPOUND(&=)
    __RIN_CW_COMPOUND(|=)
    __RIN_CW_COMPOUND(^=)
    __RIN_CW_COMPOUND(<<=)
    __RIN_CW_COMPOUND(>>=)
#undef __RIN_CW_COMPOUND

    template<class... Args>
        requires detail::__constant_wrapper_callable<value, Args...>
    static constexpr decltype(auto) operator()(Args&&... args)
        noexcept(detail::__constant_wrapper_call_noexcept<value, Args...>()) {
        if constexpr (detail::__constant_wrapper_call_is_constant<value,
                                                                  Args...>) {
            return constant_wrapper<std::invoke(
                value, remove_cvref_t<Args>::value...)>{};
        } else {
            return std::invoke(value, std::forward<Args>(args)...);
        }
    }

    template<class... Args>
        requires detail::__constant_wrapper_subscriptable<value, Args...>
    static constexpr decltype(auto) operator[](Args&&... args)
        noexcept(detail::__constant_wrapper_subscript_noexcept<value,
                                                               Args...>()) {
        if constexpr (detail::__constant_wrapper_subscript_is_constant<
                          value, Args...>) {
            return constant_wrapper<
                (value[remove_cvref_t<Args>::value...])>{};
        } else {
            return value[std::forward<Args>(args)...];
        }
    }
};

template<auto X>
inline constexpr constant_wrapper<X> cw{};
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

namespace detail {
inline void unreachable_constant_evaluation() noexcept {}
}

[[noreturn]] constexpr void unreachable() noexcept {
    // A non-constexpr call makes constant evaluation fail without returning
    // from a function declared [[noreturn]]. Runtime always takes the builtin.
    if (is_constant_evaluated()) detail::unreachable_constant_evaluation();
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
