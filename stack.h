/*
 * RinOS C++ <stack> ✿
 * スタックアダプタ
 */

#ifndef RINCXX_STACK_H
#define RINCXX_STACK_H

#include "rincxx.h"
#include "version.h"
#include "deque.h"
#include "memory.h"
#if __cplusplus > 202002L
#include "ranges.h"
#endif
#if __cplusplus >= 202002L
#include "compare.h"
#endif

namespace std {

#if __cplusplus > 202002L
#define RIN_STACK_CONSTEXPR constexpr
#else
#define RIN_STACK_CONSTEXPR
#endif

/* ═══════════════════════════════════════════════════════════════
 * stack - LIFOスタック
 * ═══════════════════════════════════════════════════════════════*/

template<class T, class Container = deque<T>>
class stack {
public:
    using container_type = Container;
    using value_type = typename Container::value_type;
    using size_type = typename Container::size_type;
    using reference = typename Container::reference;
    using const_reference = typename Container::const_reference;
    
protected:
    Container c;

    template<class V>
    RIN_STACK_CONSTEXPR void push_impl(V&& value, true_type) {
        Container candidate(c);
        candidate.push_back(std::forward<V>(value));
        using std::swap;
        swap(c, candidate);
    }

    template<class V>
    RIN_STACK_CONSTEXPR void push_impl(V&& value, false_type) {
        c.push_back(std::forward<V>(value));
    }

    template<class Result>
    static RIN_STACK_CONSTEXPR Result emplace_result(Container& container, false_type) {
        return container.back();
    }

    template<class Result>
    static RIN_STACK_CONSTEXPR void emplace_result(Container&, true_type) {}

    template<class C, class... Args>
    RIN_STACK_CONSTEXPR auto emplace_impl(true_type, Args&&... args)
        -> decltype(std::declval<C&>().emplace_back(
            std::forward<Args>(args)...)) {
        using result_type = decltype(std::declval<C&>().emplace_back(
            std::forward<Args>(args)...));
        Container candidate(c);
        candidate.emplace_back(std::forward<Args>(args)...);
        using std::swap;
        swap(c, candidate);
        return emplace_result<result_type>(c,
            integral_constant<bool, is_void<result_type>::value>());
    }

    template<class C, class... Args>
    RIN_STACK_CONSTEXPR auto emplace_impl(false_type, Args&&... args)
        -> decltype(std::declval<C&>().emplace_back(
            std::forward<Args>(args)...)) {
        return c.emplace_back(std::forward<Args>(args)...);
    }

    void assign_copy_impl(const stack& other, true_type) {
        Container candidate(c);
        candidate = other.c;
        using std::swap;
        swap(c, candidate);
    }

    void assign_copy_impl(const stack& other, false_type) {
        c = other.c;
    }
    
public:
    /* ═══════════════════════════════════════════════════════════
     * コンストラクタ
     * ═══════════════════════════════════════════════════════════*/
    
    RIN_STACK_CONSTEXPR stack() : c() {}
    
    RIN_STACK_CONSTEXPR explicit stack(const Container& cont) : c(cont) {}
    RIN_STACK_CONSTEXPR explicit stack(Container&& cont) : c(std::move(cont)) {}
    
    RIN_STACK_CONSTEXPR stack(const stack& other) : c(other.c) {}
    RIN_STACK_CONSTEXPR stack(stack&& other)
        noexcept(is_nothrow_move_constructible<Container>::value)
        : c(std::move(other.c)) {}
    
#if __cplusplus > 202002L
    template<class InputIt,
             typename enable_if<!is_integral<remove_cv_t<InputIt>>::value,
                                int>::type = 0>
    RIN_STACK_CONSTEXPR stack(InputIt first, InputIt last) : c(first, last) {}

    template<class InputIt, class Alloc,
             typename enable_if<
                 !is_integral<remove_cv_t<InputIt>>::value &&
                     uses_allocator<Container, Alloc>::value,
                                int>::type = 0>
    RIN_STACK_CONSTEXPR stack(InputIt first, InputIt last, const Alloc& alloc)
        : c(first, last, alloc) {}

    template<class R>
        requires detail::container_compatible_range<R, T>
    RIN_STACK_CONSTEXPR stack(from_range_t, R&& range)
        : c(detail::make_container_from_range<Container>(
              std::forward<R>(range))) {}

    template<class R, class Alloc>
        requires detail::container_compatible_range<R, T> &&
                 uses_allocator<Container, Alloc>::value
    RIN_STACK_CONSTEXPR stack(from_range_t, R&& range, const Alloc& alloc)
        : c(detail::make_container_from_range<Container>(
              std::forward<R>(range), alloc)) {}
#endif

    template<class Alloc,
             typename enable_if<uses_allocator<Container, Alloc>::value,
                                int>::type = 0>
    RIN_STACK_CONSTEXPR explicit stack(const Alloc& alloc) : c(alloc) {}

    template<class Alloc,
             typename enable_if<uses_allocator<Container, Alloc>::value,
                                int>::type = 0>
    RIN_STACK_CONSTEXPR stack(const Container& cont, const Alloc& alloc) : c(cont, alloc) {}

    template<class Alloc,
             typename enable_if<uses_allocator<Container, Alloc>::value,
                                int>::type = 0>
    RIN_STACK_CONSTEXPR stack(Container&& cont, const Alloc& alloc)
        : c(std::move(cont), alloc) {}

    template<class Alloc,
             typename enable_if<uses_allocator<Container, Alloc>::value,
                                int>::type = 0>
    RIN_STACK_CONSTEXPR stack(const stack& other, const Alloc& alloc) : c(other.c, alloc) {}

    template<class Alloc,
             typename enable_if<uses_allocator<Container, Alloc>::value,
                                int>::type = 0>
    RIN_STACK_CONSTEXPR stack(stack&& other, const Alloc& alloc)
        : c(std::move(other.c), alloc) {}
    
    /* ═══════════════════════════════════════════════════════════
     * 代入
     * ═══════════════════════════════════════════════════════════*/
    
    RIN_STACK_CONSTEXPR stack& operator=(const stack& other) {
        if (this == &other) return *this;
        assign_copy_impl(other, integral_constant<bool,
            is_copy_constructible<Container>::value &&
            is_nothrow_swappable<Container>::value>());
        return *this;
    }
    
    RIN_STACK_CONSTEXPR stack& operator=(stack&& other)
        noexcept(is_nothrow_move_assignable<Container>::value) {
        c = std::move(other.c);
        return *this;
    }
    
    /* ═══════════════════════════════════════════════════════════
     * 要素アクセス
     * ═══════════════════════════════════════════════════════════*/
    
    RIN_STACK_CONSTEXPR reference top() noexcept(noexcept(c.back())) { return c.back(); }
    RIN_STACK_CONSTEXPR const_reference top() const noexcept(noexcept(c.back())) { return c.back(); }
    
    /* ═══════════════════════════════════════════════════════════
     * 容量
     * ═══════════════════════════════════════════════════════════*/
    
    RIN_STACK_CONSTEXPR bool empty() const noexcept(noexcept(c.empty())) { return c.empty(); }
    RIN_STACK_CONSTEXPR size_type size() const noexcept(noexcept(c.size())) { return c.size(); }

    /* The adaptor exposes the allocator owned by its underlying sequence.
     * Keep the return type and noexcept contract dependent on the sequence so
     * custom allocator-aware containers retain their own allocator identity. */
    template<class C = Container>
    RIN_STACK_CONSTEXPR auto get_allocator() const
        noexcept(noexcept(std::declval<const C&>().get_allocator()))
        -> decltype(std::declval<const C&>().get_allocator()) {
        return c.get_allocator();
    }
    
    /* ═══════════════════════════════════════════════════════════
     * 変更
     * ═══════════════════════════════════════════════════════════*/
    
    RIN_STACK_CONSTEXPR void push(const value_type& value) {
        push_impl(value, integral_constant<bool,
            is_copy_constructible<value_type>::value &&
            is_copy_constructible<Container>::value &&
            is_nothrow_swappable<Container>::value>());
    }
    RIN_STACK_CONSTEXPR void push(value_type&& value) {
        push_impl(std::move(value), integral_constant<bool,
            is_copy_constructible<value_type>::value &&
            is_copy_constructible<Container>::value &&
            is_nothrow_swappable<Container>::value>());
    }

#if __cplusplus > 202002L
    template<class R>
        requires detail::container_compatible_range<R, T>
    RIN_STACK_CONSTEXPR void push_range(R&& range) {
        if (detail::container_range_known_empty(range)) return;
        if constexpr (is_copy_constructible<value_type>::value &&
                      is_copy_constructible<Container>::value &&
                      is_nothrow_swappable<Container>::value) {
            Container candidate(c);
            detail::append_container_range(candidate, std::forward<R>(range));
            using std::swap;
            swap(c, candidate);
        } else {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            const size_type original_size = c.size();
            try {
                detail::append_container_range(c, std::forward<R>(range));
            } catch (...) {
                while (c.size() > original_size) c.pop_back();
                throw;
            }
#else
            detail::append_container_range(c, std::forward<R>(range));
#endif
        }
    }
#endif
    
    template<class C = Container, class... Args>
    RIN_STACK_CONSTEXPR auto emplace(Args&&... args)
        -> decltype(std::declval<C&>().emplace_back(
            std::forward<Args>(args)...)) {
        return emplace_impl<C>(integral_constant<bool,
            is_copy_constructible<value_type>::value &&
            is_copy_constructible<Container>::value &&
            is_nothrow_swappable<Container>::value>(),
            std::forward<Args>(args)...);
    }
    
    RIN_STACK_CONSTEXPR void pop() noexcept(noexcept(c.pop_back())) { c.pop_back(); }
    
    RIN_STACK_CONSTEXPR void swap(stack& other)
        noexcept(is_nothrow_swappable<Container>::value) {
        using std::swap;
        swap(c, other.c);
    }
    
    /* ═══════════════════════════════════════════════════════════
     * friend宣言（比較用）
     * ═══════════════════════════════════════════════════════════*/
    
    template<class T2, class C2>
    friend RIN_STACK_CONSTEXPR bool operator==(const stack<T2, C2>& lhs, const stack<T2, C2>& rhs);
    
    template<class T2, class C2>
    friend RIN_STACK_CONSTEXPR bool operator<(const stack<T2, C2>& lhs, const stack<T2, C2>& rhs);

#if __cplusplus >= 202002L
    template<class T2, class C2>
    friend RIN_STACK_CONSTEXPR auto operator<=>(const stack<T2, C2>& lhs,
                            const stack<T2, C2>& rhs)
        -> compare_three_way_result_t<C2>;
#endif
};

template<class T, class Container, class Alloc>
struct uses_allocator<stack<T, Container>, Alloc>
    : uses_allocator<Container, Alloc> {};

/* ═══════════════════════════════════════════════════════════════
 * 比較演算子
 * ═══════════════════════════════════════════════════════════════*/

template<class T, class Container>
RIN_STACK_CONSTEXPR bool operator==(const stack<T, Container>& lhs, const stack<T, Container>& rhs) {
    return lhs.c == rhs.c;
}

template<class T, class Container>
RIN_STACK_CONSTEXPR bool operator!=(const stack<T, Container>& lhs, const stack<T, Container>& rhs) {
    return !(lhs == rhs);
}

template<class T, class Container>
RIN_STACK_CONSTEXPR bool operator<(const stack<T, Container>& lhs, const stack<T, Container>& rhs) {
    return lhs.c < rhs.c;
}

template<class T, class Container>
RIN_STACK_CONSTEXPR bool operator<=(const stack<T, Container>& lhs, const stack<T, Container>& rhs) {
    return !(rhs < lhs);
}

template<class T, class Container>
RIN_STACK_CONSTEXPR bool operator>(const stack<T, Container>& lhs, const stack<T, Container>& rhs) {
    return rhs < lhs;
}

template<class T, class Container>
RIN_STACK_CONSTEXPR bool operator>=(const stack<T, Container>& lhs, const stack<T, Container>& rhs) {
    return !(lhs < rhs);
}

#if __cplusplus >= 202002L
template<class T, class Container>
RIN_STACK_CONSTEXPR auto operator<=>(const stack<T, Container>& lhs,
                 const stack<T, Container>& rhs)
    -> compare_three_way_result_t<Container> {
    return lhs.c <=> rhs.c;
}
#endif

/* ═══════════════════════════════════════════════════════════════
 * 特殊関数
 * ═══════════════════════════════════════════════════════════════*/

template<class T, class Container>
RIN_STACK_CONSTEXPR void swap(stack<T, Container>& lhs, stack<T, Container>& rhs)
    noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

#if __cplusplus >= 201703L
template<class Container,
         typename enable_if<
             !_uses_allocator_detail::qualifies_as_allocator<Container>::value,
             int>::type = 0>
stack(Container) -> stack<typename Container::value_type, Container>;

#if __cplusplus > 202002L
template<class InputIt,
         typename enable_if<!is_integral<remove_cv_t<InputIt>>::value,
                            int>::type = 0>
stack(InputIt, InputIt)
    -> stack<typename iterator_traits<InputIt>::value_type>;

template<ranges::input_range R>
stack(from_range_t, R&&) -> stack<ranges::range_value_t<R>>;
#endif

template<class Container, class Alloc,
         typename enable_if<
             !_uses_allocator_detail::qualifies_as_allocator<Container>::value &&
             _uses_allocator_detail::qualifies_as_allocator<Alloc>::value &&
             uses_allocator<Container, Alloc>::value,
             int>::type = 0>
stack(Container, Alloc)
    -> stack<typename Container::value_type, Container>;

#if __cplusplus > 202002L
template<class InputIt, class Alloc,
         typename enable_if<
             !is_integral<remove_cv_t<InputIt>>::value &&
             _uses_allocator_detail::qualifies_as_allocator<Alloc>::value,
             int>::type = 0>
stack(InputIt, InputIt, Alloc)
    -> stack<typename iterator_traits<InputIt>::value_type,
             deque<typename iterator_traits<InputIt>::value_type, Alloc>>;

template<ranges::input_range R, class Alloc,
         typename enable_if<
             _uses_allocator_detail::qualifies_as_allocator<Alloc>::value,
             int>::type = 0>
stack(from_range_t, R&&, Alloc)
    -> stack<ranges::range_value_t<R>,
             deque<ranges::range_value_t<R>, Alloc>>;
#endif
#endif

} /* namespace std */

#undef RIN_STACK_CONSTEXPR

#endif /* RINCXX_STACK_H */
