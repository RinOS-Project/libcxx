/*
 * RinOS C++ Array ✿
 * std::array 互換実装
 */

#ifndef RINCXX_ARRAY_H
#define RINCXX_ARRAY_H

#include "rincxx.h"
#include "cstddef.h"
#include "algorithm.h"
#include "exception.h"
#if __cplusplus >= 202002L
#include "compare.h"
#endif
#include "__tuple_fwd.h"

#ifdef __cplusplus

namespace std {

#if __cplusplus >= 201402L
#define RIN_ARRAY_CONSTEXPR14 constexpr
#else
#define RIN_ARRAY_CONSTEXPR14 inline
#endif

namespace detail {

[[noreturn]] inline void array_contract_fail() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw out_of_range("array::at position is out of range");
#else
    __builtin_trap();
#endif
}

} /* namespace detail */

/* ═══════════════════════════════════════════════════════════════
 * array<T, N>
 * ═══════════════════════════════════════════════════════════════*/

template<typename T, size_t N>
struct array {
    /* 型定義 */
    using value_type = T;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = T*;
    using const_iterator = const T*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    /* データ */
    T data_[N];
    
    /* 要素アクセス */
    RIN_ARRAY_CONSTEXPR14 reference at(size_type pos) {
        if (pos >= N) detail::array_contract_fail();
        return data_[pos];
    }

    RIN_ARRAY_CONSTEXPR14 const_reference at(size_type pos) const {
        if (pos >= N) detail::array_contract_fail();
        return data_[pos];
    }
    
    RIN_ARRAY_CONSTEXPR14 reference operator[](size_type pos) noexcept { return data_[pos]; }
    constexpr const_reference operator[](size_type pos) const noexcept { return data_[pos]; }

    RIN_ARRAY_CONSTEXPR14 reference front() noexcept { return data_[0]; }
    constexpr const_reference front() const noexcept { return data_[0]; }

    RIN_ARRAY_CONSTEXPR14 reference back() noexcept { return data_[N - 1]; }
    constexpr const_reference back() const noexcept { return data_[N - 1]; }

    RIN_ARRAY_CONSTEXPR14 pointer data() noexcept { return data_; }
    constexpr const_pointer data() const noexcept { return data_; }

    /* イテレータ */
    RIN_ARRAY_CONSTEXPR14 iterator begin() noexcept { return data_; }
    constexpr const_iterator begin() const noexcept { return data_; }
    constexpr const_iterator cbegin() const noexcept { return data_; }

    RIN_ARRAY_CONSTEXPR14 iterator end() noexcept { return data_ + N; }
    constexpr const_iterator end() const noexcept { return data_ + N; }
    constexpr const_iterator cend() const noexcept { return data_ + N; }

    RIN_ARRAY_CONSTEXPR14 reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    constexpr const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }

    RIN_ARRAY_CONSTEXPR14 reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    constexpr const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    constexpr const_reverse_iterator crend() const noexcept { return const_reverse_iterator(cbegin()); }

    /* 容量 */
    constexpr bool empty() const noexcept { return N == 0; }
    constexpr size_type size() const noexcept { return N; }
    constexpr size_type max_size() const noexcept { return N; }
    
    /* 操作 */
    RIN_ARRAY_CONSTEXPR14 void fill(const T& value) {
        for (size_type i = 0; i < N; ++i) {
            data_[i] = value;
        }
    }

    RIN_ARRAY_CONSTEXPR14 void swap(array& other)
        noexcept(is_nothrow_swappable<T>::value) {
        using std::swap;
        for (size_type i = 0; i < N; ++i) {
            swap(data_[i], other.data_[i]);
        }
    }
};

/* N == 0 の特殊化 */
template<typename T>
struct array<T, 0> {
    using value_type = T;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = T*;
    using const_iterator = const T*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    /* 空の構造体でもaggregate初期化をサポートするためのダミー
     * 標準では T[0] は不正だが、空のstructを置くことで {{}} 初期化可能にする */
    struct __empty_t {};
    __empty_t __elems_;

    RIN_ARRAY_CONSTEXPR14 reference at(size_type) {
        detail::array_contract_fail();
    }
    
    RIN_ARRAY_CONSTEXPR14 const_reference at(size_type) const {
        detail::array_contract_fail();
    }
    
    RIN_ARRAY_CONSTEXPR14 reference operator[](size_type) noexcept {
        detail::array_contract_fail();
    }
    RIN_ARRAY_CONSTEXPR14 const_reference operator[](size_type) const noexcept {
        detail::array_contract_fail();
    }
    
    RIN_ARRAY_CONSTEXPR14 reference front() noexcept { detail::array_contract_fail(); }
    RIN_ARRAY_CONSTEXPR14 const_reference front() const noexcept {
        detail::array_contract_fail();
    }
    
    RIN_ARRAY_CONSTEXPR14 reference back() noexcept { detail::array_contract_fail(); }
    RIN_ARRAY_CONSTEXPR14 const_reference back() const noexcept {
        detail::array_contract_fail();
    }
    
    RIN_ARRAY_CONSTEXPR14 pointer data() noexcept { return nullptr; }
    constexpr const_pointer data() const noexcept { return nullptr; }
    
    RIN_ARRAY_CONSTEXPR14 iterator begin() noexcept { return nullptr; }
    constexpr const_iterator begin() const noexcept { return nullptr; }
    constexpr const_iterator cbegin() const noexcept { return nullptr; }
    
    RIN_ARRAY_CONSTEXPR14 iterator end() noexcept { return nullptr; }
    constexpr const_iterator end() const noexcept { return nullptr; }
    constexpr const_iterator cend() const noexcept { return nullptr; }

    RIN_ARRAY_CONSTEXPR14 reverse_iterator rbegin() noexcept {
        return reverse_iterator(end());
    }
    RIN_ARRAY_CONSTEXPR14 const_reverse_iterator rbegin() const noexcept {
        return const_reverse_iterator(end());
    }
    RIN_ARRAY_CONSTEXPR14 const_reverse_iterator crbegin() const noexcept {
        return const_reverse_iterator(cend());
    }

    RIN_ARRAY_CONSTEXPR14 reverse_iterator rend() noexcept {
        return reverse_iterator(begin());
    }
    RIN_ARRAY_CONSTEXPR14 const_reverse_iterator rend() const noexcept {
        return const_reverse_iterator(begin());
    }
    RIN_ARRAY_CONSTEXPR14 const_reverse_iterator crend() const noexcept {
        return const_reverse_iterator(cbegin());
    }
    
    constexpr bool empty() const noexcept { return true; }
    constexpr size_type size() const noexcept { return 0; }
    constexpr size_type max_size() const noexcept { return 0; }
    
    RIN_ARRAY_CONSTEXPR14 void fill(const T&) {}
    RIN_ARRAY_CONSTEXPR14 void swap(array&) noexcept {}
};

/* ═══════════════════════════════════════════════════════════════
 * 比較演算子
 * ═══════════════════════════════════════════════════════════════*/

template<typename T, size_t N>
RIN_ARRAY_CONSTEXPR14 bool operator==(const array<T, N>& lhs, const array<T, N>& rhs) {
    for (size_t i = 0; i < N; ++i) {
        if (!(lhs[i] == rhs[i])) return false;
    }
    return true;
}

template<typename T, size_t N>
constexpr bool operator!=(const array<T, N>& lhs, const array<T, N>& rhs) {
    return !(lhs == rhs);
}

#if __cplusplus < 202002L
template<typename T, size_t N>
RIN_ARRAY_CONSTEXPR14 bool operator<(const array<T, N>& lhs, const array<T, N>& rhs) {
    for (size_t i = 0; i < N; ++i) {
        if (lhs[i] < rhs[i]) return true;
        if (rhs[i] < lhs[i]) return false;
    }
    return false;
}

template<typename T, size_t N>
constexpr bool operator<=(const array<T, N>& lhs, const array<T, N>& rhs) {
    return !(rhs < lhs);
}

template<typename T, size_t N>
constexpr bool operator>(const array<T, N>& lhs, const array<T, N>& rhs) {
    return rhs < lhs;
}

template<typename T, size_t N>
constexpr bool operator>=(const array<T, N>& lhs, const array<T, N>& rhs) {
    return !(lhs < rhs);
}
#endif

#if __cplusplus >= 202002L
template<typename T, size_t N>
constexpr auto operator<=>(const array<T, N>& lhs, const array<T, N>& rhs)
    -> detail::synth_three_way_result_t<T> {
    using result_type = detail::synth_three_way_result_t<T>;
    for (size_t i = 0; i < N; ++i) {
        const auto result = detail::synth_three_way(lhs[i], rhs[i]);
        if (result != 0) return result;
    }
    return result_type::equivalent;
}
#endif

template<typename T, size_t N>
RIN_ARRAY_CONSTEXPR14 void swap(array<T, N>& lhs, array<T, N>& rhs)
    noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

/* ═══════════════════════════════════════════════════════════════
 * std::get for array
 * ═══════════════════════════════════════════════════════════════*/

template<size_t I, typename T, size_t N>
constexpr T& get(array<T, N>& arr) noexcept {
    static_assert(I < N, "Index out of bounds");
    return arr[I];
}

template<size_t I, typename T, size_t N>
constexpr const T& get(const array<T, N>& arr) noexcept {
    static_assert(I < N, "Index out of bounds");
    return arr[I];
}

template<size_t I, typename T, size_t N>
constexpr T&& get(array<T, N>&& arr) noexcept {
    static_assert(I < N, "Index out of bounds");
    return std::move(arr[I]);
}

template<size_t I, typename T, size_t N>
constexpr const T&& get(const array<T, N>&& arr) noexcept {
    static_assert(I < N, "Index out of bounds");
    return std::move(arr[I]);
}

/* tuple_size, tuple_element for array */
template<typename T, size_t N>
struct tuple_size<array<T, N>> : integral_constant<size_t, N> {};

template<typename T, size_t N>
struct tuple_size<const array<T, N>> : integral_constant<size_t, N> {};

template<typename T, size_t N>
struct tuple_size<volatile array<T, N>> : integral_constant<size_t, N> {};

template<typename T, size_t N>
struct tuple_size<const volatile array<T, N>> : integral_constant<size_t, N> {};

template<size_t I, typename T, size_t N>
struct tuple_element<I, array<T, N>> {
    static_assert(I < N, "Index out of bounds");
    using type = T;
};

template<size_t I, typename T, size_t N>
struct tuple_element<I, const array<T, N>> {
    static_assert(I < N, "Index out of bounds");
    using type = const T;
};

template<size_t I, typename T, size_t N>
struct tuple_element<I, volatile array<T, N>> {
    static_assert(I < N, "Index out of bounds");
    using type = volatile T;
};

template<size_t I, typename T, size_t N>
struct tuple_element<I, const volatile array<T, N>> {
    static_assert(I < N, "Index out of bounds");
    using type = const volatile T;
};

#if __cplusplus >= 202002L
/* ═══════════════════════════════════════════════════════════════
 * to_array (C++20)
 * ═══════════════════════════════════════════════════════════════*/

namespace detail {
    template<typename T, size_t N, size_t... I>
    constexpr array<typename remove_cv<T>::type, N>
    to_array_impl(T (&a)[N], index_sequence<I...>) {
        return {{a[I]...}};
    }

    template<typename T, size_t N, size_t... I>
    constexpr array<typename remove_cv<T>::type, N>
    to_array_impl(T (&&a)[N], index_sequence<I...>) {
        return {{std::move(a[I])...}};
    }
} /* namespace detail */

/* Lvalue reference overload */
template<typename T, size_t N,
         typename enable_if<
             !is_array<T>::value &&
             is_constructible<typename remove_cv<T>::type, T&>::value,
             int
         >::type = 0>
constexpr array<typename remove_cv<T>::type, N> to_array(T (&a)[N]) {
    return detail::to_array_impl(a, make_index_sequence<N>{});
}

/* Rvalue reference overload */
template<typename T, size_t N,
         typename enable_if<
             !is_array<T>::value &&
             is_move_constructible<typename remove_cv<T>::type>::value,
             int
         >::type = 0>
constexpr array<typename remove_cv<T>::type, N> to_array(T (&&a)[N]) {
    return detail::to_array_impl(std::move(a), make_index_sequence<N>{});
}
#endif

/* ═══════════════════════════════════════════════════════════════
 * Deduction guide (CTAD) for array
 * ═══════════════════════════════════════════════════════════════*/

#if __cplusplus >= 201703L
template<typename T, typename... U,
         typename enable_if<conjunction<is_same<T, U>...>::value, int>::type = 0>
array(T, U...) -> array<T, 1 + sizeof...(U)>;
#endif

#undef RIN_ARRAY_CONSTEXPR14

} /* namespace std */

#endif /* __cplusplus */
#endif /* RINCXX_ARRAY_H */
