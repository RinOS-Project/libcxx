/*
 * RinOS C++ <queue> ✿
 * キューアダプタ
 */

#ifndef RINCXX_QUEUE_H
#define RINCXX_QUEUE_H

#include "rincxx.h"
#include "version.h"
#include "deque.h"
#include "vector.h"
#include "functional.h"
#include "memory.h"
#if __cplusplus > 202002L
#include "ranges.h"
#endif
#if __cplusplus >= 202002L
#include "compare.h"
#endif

namespace std {

#if __cplusplus > 202002L
#define RIN_QUEUE_CONSTEXPR constexpr
#else
#define RIN_QUEUE_CONSTEXPR
#endif

/* ═══════════════════════════════════════════════════════════════
 * queue - FIFOキュー
 * ═══════════════════════════════════════════════════════════════*/

template<class T, class Container = deque<T>>
class queue {
public:
    using container_type = Container;
    using value_type = typename Container::value_type;
    using size_type = typename Container::size_type;
    using reference = typename Container::reference;
    using const_reference = typename Container::const_reference;
    
protected:
    Container c;
    template<class V>
    RIN_QUEUE_CONSTEXPR void push_impl(V&& value, true_type) {
        Container candidate(c);
        candidate.push_back(std::forward<V>(value));
        using std::swap;
        swap(c, candidate);
    }

    template<class V>
    RIN_QUEUE_CONSTEXPR void push_impl(V&& value, false_type) {
        c.push_back(std::forward<V>(value));
    }

    template<class Result>
    static RIN_QUEUE_CONSTEXPR Result emplace_result(Container& container, false_type) {
        return container.back();
    }

    template<class Result>
    static RIN_QUEUE_CONSTEXPR void emplace_result(Container&, true_type) {}

    template<class C, class... Args>
    RIN_QUEUE_CONSTEXPR auto emplace_impl(true_type, Args&&... args)
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
    RIN_QUEUE_CONSTEXPR auto emplace_impl(false_type, Args&&... args)
        -> decltype(std::declval<C&>().emplace_back(
            std::forward<Args>(args)...)) {
        return c.emplace_back(std::forward<Args>(args)...);
    }

    void assign_copy_impl(const queue& other, true_type) {
        Container candidate(c);
        candidate = other.c;
        using std::swap;
        swap(c, candidate);
    }

    void assign_copy_impl(const queue& other, false_type) {
        c = other.c;
    }
    
public:
    /* コンストラクタ */
    RIN_QUEUE_CONSTEXPR queue() : c() {}
    
    RIN_QUEUE_CONSTEXPR explicit queue(const Container& cont) : c(cont) {}
    RIN_QUEUE_CONSTEXPR explicit queue(Container&& cont) : c(std::move(cont)) {}
    
    RIN_QUEUE_CONSTEXPR queue(const queue& other) : c(other.c) {}
    RIN_QUEUE_CONSTEXPR queue(queue&& other)
        noexcept(is_nothrow_move_constructible<Container>::value)
        : c(std::move(other.c)) {}
    
#if __cplusplus > 202002L
    template<class InputIt,
             typename enable_if<!is_integral<remove_cv_t<InputIt>>::value,
                                int>::type = 0>
    RIN_QUEUE_CONSTEXPR queue(InputIt first, InputIt last) : c(first, last) {}

    template<class InputIt, class Alloc,
             typename enable_if<
                 !is_integral<remove_cv_t<InputIt>>::value &&
                     uses_allocator<Container, Alloc>::value,
                                int>::type = 0>
    RIN_QUEUE_CONSTEXPR queue(InputIt first, InputIt last, const Alloc& alloc)
        : c(first, last, alloc) {}

    template<class R>
        requires detail::container_compatible_range<R, T>
    RIN_QUEUE_CONSTEXPR queue(from_range_t, R&& range)
        : c(detail::make_container_from_range<Container>(
              std::forward<R>(range))) {}

    template<class R, class Alloc>
        requires detail::container_compatible_range<R, T> &&
                 uses_allocator<Container, Alloc>::value
    RIN_QUEUE_CONSTEXPR queue(from_range_t, R&& range, const Alloc& alloc)
        : c(detail::make_container_from_range<Container>(
              std::forward<R>(range), alloc)) {}
#endif

    template<class Alloc,
             typename enable_if<uses_allocator<Container, Alloc>::value,
                                int>::type = 0>
    RIN_QUEUE_CONSTEXPR explicit queue(const Alloc& alloc) : c(alloc) {}

    template<class Alloc,
             typename enable_if<uses_allocator<Container, Alloc>::value,
                                int>::type = 0>
    RIN_QUEUE_CONSTEXPR queue(const Container& cont, const Alloc& alloc) : c(cont, alloc) {}

    template<class Alloc,
             typename enable_if<uses_allocator<Container, Alloc>::value,
                                int>::type = 0>
    RIN_QUEUE_CONSTEXPR queue(Container&& cont, const Alloc& alloc)
        : c(std::move(cont), alloc) {}

    template<class Alloc,
             typename enable_if<uses_allocator<Container, Alloc>::value,
                                int>::type = 0>
    RIN_QUEUE_CONSTEXPR queue(const queue& other, const Alloc& alloc) : c(other.c, alloc) {}

    template<class Alloc,
             typename enable_if<uses_allocator<Container, Alloc>::value,
                                int>::type = 0>
    RIN_QUEUE_CONSTEXPR queue(queue&& other, const Alloc& alloc)
        : c(std::move(other.c), alloc) {}
    
    /* 代入 */
    RIN_QUEUE_CONSTEXPR queue& operator=(const queue& other) {
        if (this == &other) return *this;
        assign_copy_impl(other, integral_constant<bool,
            is_copy_constructible<Container>::value &&
            is_nothrow_swappable<Container>::value>());
        return *this;
    }
    
    RIN_QUEUE_CONSTEXPR queue& operator=(queue&& other)
        noexcept(is_nothrow_move_assignable<Container>::value) {
        c = std::move(other.c);
        return *this;
    }
    
    /* 要素アクセス */
    RIN_QUEUE_CONSTEXPR reference front() noexcept(noexcept(c.front())) { return c.front(); }
    RIN_QUEUE_CONSTEXPR const_reference front() const noexcept(noexcept(c.front())) { return c.front(); }
    
    RIN_QUEUE_CONSTEXPR reference back() noexcept(noexcept(c.back())) { return c.back(); }
    RIN_QUEUE_CONSTEXPR const_reference back() const noexcept(noexcept(c.back())) { return c.back(); }
    
    /* 容量 */
    RIN_QUEUE_CONSTEXPR bool empty() const noexcept(noexcept(c.empty())) { return c.empty(); }
    RIN_QUEUE_CONSTEXPR size_type size() const noexcept(noexcept(c.size())) { return c.size(); }

    /* The adaptor exposes the allocator owned by its underlying sequence.
     * Keep the return type and noexcept contract dependent on the sequence so
     * custom allocator-aware containers retain their own allocator identity. */
    template<class C = Container>
    RIN_QUEUE_CONSTEXPR auto get_allocator() const
        noexcept(noexcept(std::declval<const C&>().get_allocator()))
        -> decltype(std::declval<const C&>().get_allocator()) {
        return c.get_allocator();
    }
    
    /* 変更 */
    RIN_QUEUE_CONSTEXPR void push(const value_type& value) {
        push_impl(value, integral_constant<bool,
            is_copy_constructible<value_type>::value &&
            is_copy_constructible<Container>::value &&
            is_nothrow_swappable<Container>::value>());
    }
    RIN_QUEUE_CONSTEXPR void push(value_type&& value) {
        push_impl(std::move(value), integral_constant<bool,
            is_copy_constructible<value_type>::value &&
            is_copy_constructible<Container>::value &&
            is_nothrow_swappable<Container>::value>());
    }

#if __cplusplus > 202002L
    template<class R>
        requires detail::container_compatible_range<R, T>
    RIN_QUEUE_CONSTEXPR void push_range(R&& range) {
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
                /* Prefer removing the appended suffix one element at a time
                 * when the sequence exposes pop_back().  The C++ queue
                 * SequenceContainer contract does not require resize(), and
                 * a custom container may provide both operations with
                 * different exception guarantees.  Resize-only containers
                 * remain supported as a fallback; a container with neither
                 * operation cannot offer rollback but its original FIFO head
                 * is still left intact. */
                if constexpr (requires(Container& container) {
                                         container.pop_back();
                                     }) {
                    while (c.size() > original_size) c.pop_back();
                } else if constexpr (requires(Container& container, size_type count) {
                                  container.resize(count);
                              }) {
                    c.resize(original_size);
                }
                throw;
            }
#else
            detail::append_container_range(c, std::forward<R>(range));
#endif
        }
    }
#endif
    
    template<class C = Container, class... Args>
    RIN_QUEUE_CONSTEXPR auto emplace(Args&&... args)
        -> decltype(std::declval<C&>().emplace_back(
            std::forward<Args>(args)...)) {
        return emplace_impl<C>(integral_constant<bool,
            is_copy_constructible<value_type>::value &&
            is_copy_constructible<Container>::value &&
            is_nothrow_swappable<Container>::value>(),
            std::forward<Args>(args)...);
    }
    
    RIN_QUEUE_CONSTEXPR void pop() noexcept(noexcept(c.pop_front())) { c.pop_front(); }
    
    RIN_QUEUE_CONSTEXPR void swap(queue& other)
        noexcept(is_nothrow_swappable<Container>::value) {
        using std::swap;
        swap(c, other.c);
    }
    
    /* 比較用のfriend */
    template<class T2, class C2>
    friend RIN_QUEUE_CONSTEXPR bool operator==(const queue<T2, C2>& lhs, const queue<T2, C2>& rhs);
    
    template<class T2, class C2>
    friend RIN_QUEUE_CONSTEXPR bool operator<(const queue<T2, C2>& lhs, const queue<T2, C2>& rhs);

#if __cplusplus >= 202002L
    template<class T2, class C2>
    friend RIN_QUEUE_CONSTEXPR auto operator<=>(const queue<T2, C2>& lhs,
                            const queue<T2, C2>& rhs)
        -> compare_three_way_result_t<C2>;
#endif
};

template<class T, class Container, class Alloc>
struct uses_allocator<queue<T, Container>, Alloc>
    : uses_allocator<Container, Alloc> {};

/* 比較演算子 */
template<class T, class Container>
RIN_QUEUE_CONSTEXPR bool operator==(const queue<T, Container>& lhs, const queue<T, Container>& rhs) {
    return lhs.c == rhs.c;
}

template<class T, class Container>
RIN_QUEUE_CONSTEXPR bool operator!=(const queue<T, Container>& lhs, const queue<T, Container>& rhs) {
    return !(lhs == rhs);
}

template<class T, class Container>
RIN_QUEUE_CONSTEXPR bool operator<(const queue<T, Container>& lhs, const queue<T, Container>& rhs) {
    return lhs.c < rhs.c;
}

template<class T, class Container>
RIN_QUEUE_CONSTEXPR bool operator<=(const queue<T, Container>& lhs, const queue<T, Container>& rhs) {
    return !(rhs < lhs);
}

template<class T, class Container>
RIN_QUEUE_CONSTEXPR bool operator>(const queue<T, Container>& lhs, const queue<T, Container>& rhs) {
    return rhs < lhs;
}

template<class T, class Container>
RIN_QUEUE_CONSTEXPR bool operator>=(const queue<T, Container>& lhs, const queue<T, Container>& rhs) {
    return !(lhs < rhs);
}

#if __cplusplus >= 202002L
template<class T, class Container>
RIN_QUEUE_CONSTEXPR auto operator<=>(const queue<T, Container>& lhs,
                 const queue<T, Container>& rhs)
    -> compare_three_way_result_t<Container> {
    return lhs.c <=> rhs.c;
}
#endif

template<class T, class Container>
RIN_QUEUE_CONSTEXPR void swap(queue<T, Container>& lhs, queue<T, Container>& rhs)
    noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

#if __cplusplus >= 201703L
template<class Container,
         typename enable_if<
             !_uses_allocator_detail::qualifies_as_allocator<Container>::value,
             int>::type = 0>
queue(Container) -> queue<typename Container::value_type, Container>;

#if __cplusplus > 202002L
template<class InputIt,
         typename enable_if<!is_integral<remove_cv_t<InputIt>>::value,
                            int>::type = 0>
queue(InputIt, InputIt)
    -> queue<typename iterator_traits<InputIt>::value_type>;

template<ranges::input_range R>
queue(from_range_t, R&&) -> queue<ranges::range_value_t<R>>;
#endif

template<class Container, class Alloc,
         typename enable_if<
             !_uses_allocator_detail::qualifies_as_allocator<Container>::value &&
             _uses_allocator_detail::qualifies_as_allocator<Alloc>::value &&
             uses_allocator<Container, Alloc>::value,
             int>::type = 0>
queue(Container, Alloc)
    -> queue<typename Container::value_type, Container>;

#if __cplusplus > 202002L
template<class InputIt, class Alloc,
         typename enable_if<
             !is_integral<remove_cv_t<InputIt>>::value &&
             _uses_allocator_detail::qualifies_as_allocator<Alloc>::value,
             int>::type = 0>
queue(InputIt, InputIt, Alloc)
    -> queue<typename iterator_traits<InputIt>::value_type,
             deque<typename iterator_traits<InputIt>::value_type, Alloc>>;

template<ranges::input_range R, class Alloc,
         typename enable_if<
             _uses_allocator_detail::qualifies_as_allocator<Alloc>::value,
             int>::type = 0>
queue(from_range_t, R&&, Alloc)
    -> queue<ranges::range_value_t<R>,
             deque<ranges::range_value_t<R>, Alloc>>;
#endif
#endif

/* ═══════════════════════════════════════════════════════════════
 * priority_queue - 優先度付きキュー（ヒープベース）
 * ═══════════════════════════════════════════════════════════════*/

template<class T, class Container = vector<T>, class Compare = less<typename Container::value_type>>
class priority_queue {
public:
    using container_type = Container;
    using value_compare = Compare;
    using value_type = typename Container::value_type;
    using size_type = typename Container::size_type;
    using reference = typename Container::reference;
    using const_reference = typename Container::const_reference;
    
protected:
    Container c;
    Compare comp;

    void assign_copy_impl(const priority_queue& other, true_type) {
        Container candidate_container(c);
        Compare candidate_compare(comp);
        candidate_container = other.c;
        candidate_compare = other.comp;
        using std::swap;
        swap(c, candidate_container);
        swap(comp, candidate_compare);
    }

    void assign_copy_impl(const priority_queue& other, false_type) {
        c = other.c;
        comp = other.comp;
    }

    /* A value type may provide a throwing ADL swap even though its move
     * operations are non-throwing.  Heap transactions cannot reliably undo
     * a swap which throws after partially changing its operands.  For the
     * ordinary T& reference used by vector/deque, use a three-move exchange
     * in that case; proxy references keep the normal swap path. */
    RIN_QUEUE_CONSTEXPR void exchange_values_impl(
        reference left, reference right, true_type) {
        value_type temporary(std::move(left));
        left = std::move(right);
        right = std::move(temporary);
    }

    RIN_QUEUE_CONSTEXPR void exchange_values_impl(
        reference left, reference right, false_type) {
        using std::swap;
        swap(left, right);
    }

    RIN_QUEUE_CONSTEXPR void exchange_values(reference left,
                                             reference right) {
        exchange_values_impl(
            left, right,
            integral_constant<bool,
                is_same<reference, value_type&>::value &&
                is_nothrow_move_constructible<value_type>::value &&
                is_nothrow_move_assignable<value_type>::value>());
    }
    
    /* ヒープ操作 */
    RIN_QUEUE_CONSTEXPR void sift_up(size_type index) {
        while (index > 0) {
            size_type parent = (index - 1) / 2;
            if (!comp(c[parent], c[index])) break;
            exchange_values(c[parent], c[index]);
            index = parent;
        }
    }

    /* Appending a move-only value directly to the live heap cannot use the
     * copy-and-swap candidate path.  Keep the existing heap untouched when a
     * comparator throws after one or more swaps by retracing the parent chain
     * and undoing exactly the swaps made for the new value. */
    RIN_QUEUE_CONSTEXPR void sift_up_transactional(size_type index) {
        const size_type original = index;
        (void)original;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            while (index > 0) {
                const size_type parent = (index - 1) / 2;
                if (!comp(c[parent], c[index])) break;
                exchange_values(c[parent], c[index]);
                index = parent;
            }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            while (index != original) {
                /* `index` is an ancestor of the original slot.  Recover the
                 * child on that exact ancestor path, then undo the last swap
                 * toward the original position. */
                size_type child = original;
                while ((child - 1) / 2 != index)
                    child = (child - 1) / 2;
                exchange_values(c[index], c[child]);
                index = child;
            }
            throw;
        }
#endif
    }

    struct heap_swap_record {
        size_type parent;
        size_type child;
    };

    /* Record every promotion swap in a range transaction.  A later inserted
     * value can move an earlier value again, so rolling back only the current
     * sift is insufficient; the caller reverses this complete log. */
    RIN_QUEUE_CONSTEXPR void sift_up_recorded(
        size_type index, vector<heap_swap_record>& swaps) {
        while (index > 0) {
            const size_type parent = (index - 1) / 2;
            if (!comp(c[parent], c[index])) break;
            swaps.push_back({parent, index});
            exchange_values(c[parent], c[index]);
            index = parent;
        }
    }

    RIN_QUEUE_CONSTEXPR void rollback_heap_swaps(
        vector<heap_swap_record>& swaps) {
        while (!swaps.empty()) {
            const heap_swap_record record = swaps.back();
            swaps.pop_back();
            exchange_values(c[record.parent], c[record.child]);
        }
    }
    
    RIN_QUEUE_CONSTEXPR void sift_down(size_type index) {
        const size_type size = c.size();
        if (size < 2) return;
        while (index <= (size - 2) / 2) {
            size_type largest = index;
            size_type left = 2 * index + 1;
            size_type right = 2 * index + 2;
            
            if (left < size && comp(c[largest], c[left]))
                largest = left;
            if (right < size && comp(c[largest], c[right]))
                largest = right;
            
            if (largest == index) break;
            
            exchange_values(c[index], c[largest]);
            index = largest;
        }
    }

    /* Sift down a root while the last slot is still retained as the
     * candidate for the removed top element.  Every exchange is recorded so
     * a throwing comparator can restore the exact pre-pop heap before the
     * candidate slot is destroyed. */
    RIN_QUEUE_CONSTEXPR void sift_down_recorded(
        size_type index, size_type limit, vector<heap_swap_record>& swaps) {
        while (limit >= 2 && index <= (limit - 2) / 2) {
            size_type largest = index;
            const size_type left = 2 * index + 1;
            const size_type right = 2 * index + 2;

            if (left < limit && comp(c[largest], c[left]))
                largest = left;
            if (right < limit && comp(c[largest], c[right]))
                largest = right;
            if (largest == index) break;

            swaps.push_back({index, largest});
            exchange_values(c[index], c[largest]);
            index = largest;
        }
    }

    RIN_QUEUE_CONSTEXPR void pop_in_place_fast() {
        exchange_values(c.front(), c.back());
        c.pop_back();
        if (!c.empty()) sift_down(0);
    }

    RIN_QUEUE_CONSTEXPR void pop_in_place_transactional() {
        const size_type original_size = c.size();
        if (original_size <= 1) {
            if (original_size != 0) c.pop_back();
            return;
        }

        /* Reserve before touching the live heap.  A record is trivial, so no
         * later push_back can allocate while comparisons or swaps run. */
        vector<heap_swap_record> swaps;
        swaps.reserve(original_size);

        exchange_values(c.front(), c.back());
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            sift_down_recorded(0, original_size - 1, swaps);
        } catch (...) {
            rollback_heap_swaps(swaps);
            exchange_values(c.front(), c.back());
            throw;
        }
#else
        sift_down_recorded(0, original_size - 1, swaps);
#endif
        c.pop_back();
    }

    RIN_QUEUE_CONSTEXPR void pop_dispatch(std::true_type) {
        priority_queue candidate(*this);
        /* The original is untouched while candidate's comparator runs, so
         * the fast in-place path is enough here and remains constexpr for
         * C++23 containers whose vector cannot allocate during evaluation. */
        candidate.pop_in_place_fast();
        using std::swap;
        swap(c, candidate.c);
        swap(comp, candidate.comp);
    }

    RIN_QUEUE_CONSTEXPR void pop_dispatch(std::false_type) {
        pop_in_place_transactional();
    }
    
    RIN_QUEUE_CONSTEXPR void make_heap() {
        if (c.size() <= 1) return;
        for (size_type i = c.size() / 2; i > 0; --i) {
            sift_down(i - 1);
        }
        sift_down(0);
    }
    
public:
    /* コンストラクタ */
    RIN_QUEUE_CONSTEXPR priority_queue() : c(), comp() {}
    
    RIN_QUEUE_CONSTEXPR explicit priority_queue(const Compare& compare) : c(), comp(compare) {}
    
    RIN_QUEUE_CONSTEXPR priority_queue(const Compare& compare, const Container& cont)
        : c(cont), comp(compare) {
        make_heap();
    }
    
    RIN_QUEUE_CONSTEXPR priority_queue(const Compare& compare, Container&& cont)
        : c(std::move(cont)), comp(compare) {
        make_heap();
    }
    
    RIN_QUEUE_CONSTEXPR priority_queue(const priority_queue& other) : c(other.c), comp(other.comp) {}
    RIN_QUEUE_CONSTEXPR priority_queue(priority_queue&& other)
        noexcept(is_nothrow_move_constructible<Container>::value &&
                 is_nothrow_move_constructible<Compare>::value)
        : c(std::move(other.c)), comp(std::move(other.comp)) {}
    
    template<class InputIt,
             typename enable_if<!is_integral<remove_cv_t<InputIt>>::value,
                                int>::type = 0>
    RIN_QUEUE_CONSTEXPR priority_queue(InputIt first, InputIt last, const Compare& compare = Compare())
        : c(first, last), comp(compare) {
        make_heap();
    }
    
    template<class InputIt,
             typename enable_if<!is_integral<remove_cv_t<InputIt>>::value,
                                int>::type = 0>
    RIN_QUEUE_CONSTEXPR priority_queue(InputIt first, InputIt last, const Compare& compare, 
                   const Container& cont)
        : c(cont), comp(compare) {
        c.insert(c.end(), first, last);
        make_heap();
    }

    template<class InputIt,
             typename enable_if<!is_integral<remove_cv_t<InputIt>>::value,
                                int>::type = 0>
    priority_queue(InputIt first, InputIt last, const Compare& compare,
                   Container&& cont)
        : c(std::move(cont)), comp(compare) {
        c.insert(c.end(), first, last);
        make_heap();
    }

#if __cplusplus > 202002L
    template<class R>
        requires detail::container_compatible_range<R, T>
    RIN_QUEUE_CONSTEXPR priority_queue(from_range_t, R&& range,
                   const Compare& compare = Compare())
        : c(detail::make_container_from_range<Container>(
              std::forward<R>(range))), comp(compare) {
        make_heap();
    }
#endif

    template<class Alloc,
             typename enable_if<uses_allocator<Container, Alloc>::value,
                                int>::type = 0>
    RIN_QUEUE_CONSTEXPR explicit priority_queue(const Alloc& alloc) : c(alloc), comp() {}

    template<class Alloc,
             typename enable_if<uses_allocator<Container, Alloc>::value,
                                int>::type = 0>
    RIN_QUEUE_CONSTEXPR priority_queue(const Compare& compare, const Alloc& alloc)
        : c(alloc), comp(compare) {}

    template<class Alloc,
             typename enable_if<uses_allocator<Container, Alloc>::value,
                                int>::type = 0>
    RIN_QUEUE_CONSTEXPR priority_queue(const Compare& compare, const Container& cont,
                   const Alloc& alloc)
        : c(cont, alloc), comp(compare) {
        make_heap();
    }

    template<class Alloc,
             typename enable_if<uses_allocator<Container, Alloc>::value,
                                int>::type = 0>
    RIN_QUEUE_CONSTEXPR priority_queue(const Compare& compare, Container&& cont,
                   const Alloc& alloc)
        : c(std::move(cont), alloc), comp(compare) {
        make_heap();
    }

    template<class Alloc,
             typename enable_if<uses_allocator<Container, Alloc>::value,
                                int>::type = 0>
    RIN_QUEUE_CONSTEXPR priority_queue(const priority_queue& other, const Alloc& alloc)
        : c(other.c, alloc), comp(other.comp) {}

    template<class Alloc,
             typename enable_if<uses_allocator<Container, Alloc>::value,
                                int>::type = 0>
    RIN_QUEUE_CONSTEXPR priority_queue(priority_queue&& other, const Alloc& alloc)
        : c(std::move(other.c), alloc), comp(std::move(other.comp)) {}

    template<class InputIt, class Alloc,
             typename enable_if<
                 !is_integral<remove_cv_t<InputIt>>::value &&
                     uses_allocator<Container, Alloc>::value,
                                int>::type = 0>
    RIN_QUEUE_CONSTEXPR priority_queue(InputIt first, InputIt last, const Alloc& alloc)
        : c(first, last, alloc), comp() {
        make_heap();
    }

    template<class InputIt, class Alloc,
             typename enable_if<
                 !is_integral<remove_cv_t<InputIt>>::value &&
                     uses_allocator<Container, Alloc>::value,
                                int>::type = 0>
    RIN_QUEUE_CONSTEXPR priority_queue(InputIt first, InputIt last, const Compare& compare,
                   const Alloc& alloc)
        : c(first, last, alloc), comp(compare) {
        make_heap();
    }

    template<class InputIt, class Alloc,
             typename enable_if<
                 !is_integral<remove_cv_t<InputIt>>::value &&
                     uses_allocator<Container, Alloc>::value,
                                int>::type = 0>
    RIN_QUEUE_CONSTEXPR priority_queue(InputIt first, InputIt last, const Compare& compare,
                   const Container& cont, const Alloc& alloc)
        : c(cont, alloc), comp(compare) {
        c.insert(c.end(), first, last);
        make_heap();
    }

    template<class InputIt, class Alloc,
             typename enable_if<
                 !is_integral<remove_cv_t<InputIt>>::value &&
                     uses_allocator<Container, Alloc>::value,
                                int>::type = 0>
    RIN_QUEUE_CONSTEXPR priority_queue(InputIt first, InputIt last, const Compare& compare,
                   Container&& cont, const Alloc& alloc)
        : c(std::move(cont), alloc), comp(compare) {
        c.insert(c.end(), first, last);
        make_heap();
    }

#if __cplusplus > 202002L
    template<class R, class Alloc>
        requires detail::container_compatible_range<R, T> &&
                 uses_allocator<Container, Alloc>::value
    RIN_QUEUE_CONSTEXPR priority_queue(from_range_t, R&& range, const Compare& compare,
                   const Alloc& alloc)
        : c(detail::make_container_from_range<Container>(
              std::forward<R>(range), alloc)), comp(compare) {
        make_heap();
    }

    template<class R, class Alloc>
        requires detail::container_compatible_range<R, T> &&
                 uses_allocator<Container, Alloc>::value
    RIN_QUEUE_CONSTEXPR priority_queue(from_range_t, R&& range, const Alloc& alloc)
        : c(detail::make_container_from_range<Container>(
              std::forward<R>(range), alloc)), comp() {
        make_heap();
    }
#endif
    
    /* 代入 */
    RIN_QUEUE_CONSTEXPR priority_queue& operator=(const priority_queue& other) {
        if (this == &other) return *this;
        assign_copy_impl(other, integral_constant<bool,
            is_copy_constructible<Container>::value &&
            is_copy_constructible<Compare>::value &&
            is_nothrow_swappable<Container>::value &&
            is_nothrow_swappable<Compare>::value>());
        return *this;
    }
    
    RIN_QUEUE_CONSTEXPR priority_queue& operator=(priority_queue&& other)
        noexcept(is_nothrow_move_assignable<Container>::value &&
                 is_nothrow_move_assignable<Compare>::value) {
        c = std::move(other.c);
        comp = std::move(other.comp);
        return *this;
    }
    
    /* 要素アクセス */
    RIN_QUEUE_CONSTEXPR const_reference top() const noexcept(noexcept(c.front())) { return c.front(); }
    
    /* 容量 */
    RIN_QUEUE_CONSTEXPR bool empty() const noexcept(noexcept(c.empty())) { return c.empty(); }
    RIN_QUEUE_CONSTEXPR size_type size() const noexcept(noexcept(c.size())) { return c.size(); }

    /* priority_queue has the same allocator observer as the sequence-based
     * adaptors; do not manufacture a default allocator or lose its state. */
    template<class C = Container>
    RIN_QUEUE_CONSTEXPR auto get_allocator() const
        noexcept(noexcept(std::declval<const C&>().get_allocator()))
        -> decltype(std::declval<const C&>().get_allocator()) {
        return c.get_allocator();
    }

    /* [priqueue.cons] requires a value_compare observer.  Returning a copy
     * preserves the comparator's value semantics and keeps the protected
     * comparator state from leaking through a mutable reference. */
    value_compare value_comp() const { return comp; }
    
    /* 変更 */
    RIN_QUEUE_CONSTEXPR void push(const value_type& value) {
        c.push_back(value);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            sift_up_transactional(c.size() - 1);
        } catch (...) {
            c.pop_back();
            throw;
        }
#else
        sift_up_transactional(c.size() - 1);
#endif
    }
    
    RIN_QUEUE_CONSTEXPR void push(value_type&& value) {
        c.push_back(std::move(value));
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            sift_up_transactional(c.size() - 1);
        } catch (...) {
            c.pop_back();
            throw;
        }
#else
        sift_up_transactional(c.size() - 1);
#endif
    }

#if __cplusplus > 202002L
    template<class R>
        requires detail::container_compatible_range<R, T>
    RIN_QUEUE_CONSTEXPR void push_range(R&& range) {
        if (detail::container_range_known_empty(range)) return;
        if constexpr (is_copy_constructible<value_type>::value &&
                      is_copy_constructible<Container>::value &&
                      is_copy_constructible<Compare>::value &&
                      is_nothrow_swappable<Container>::value &&
                      is_nothrow_swappable<Compare>::value) {
            priority_queue candidate(*this);
            detail::append_container_range(candidate.c,
                                           std::forward<R>(range));
            candidate.make_heap();
            using std::swap;
            swap(c, candidate.c);
            swap(comp, candidate.comp);
        } else {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            const size_type original_size = c.size();
            vector<heap_swap_record> swaps;
            try {
                detail::append_container_range(c, std::forward<R>(range));
                for (size_type index = original_size; index < c.size(); ++index) {
                    /* Reserve enough records for this entire sift before any
                     * comparison or element swap can mutate the live heap. */
                    swaps.reserve(swaps.size() + index);
                    sift_up_recorded(index, swaps);
                }
            } catch (...) {
                /* A move-only adaptor cannot stage a copy of the existing
                 * heap.  Reverse every promotion swap first, then remove the
                 * appended suffix, so a failed append is never published as a
                 * successful mutation. */
                rollback_heap_swaps(swaps);
                while (c.size() > original_size) c.pop_back();
                throw;
            }
#else
            detail::append_container_range(c, std::forward<R>(range));
            make_heap();
#endif
        }
    }
#endif
    
    template<class... Args>
    RIN_QUEUE_CONSTEXPR void emplace(Args&&... args) {
        c.emplace_back(std::forward<Args>(args)...);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            sift_up_transactional(c.size() - 1);
        } catch (...) {
            c.pop_back();
            throw;
        }
#else
        sift_up_transactional(c.size() - 1);
#endif
    }
    
    RIN_QUEUE_CONSTEXPR void pop() {
        if (c.empty()) return;
        pop_dispatch(integral_constant<bool,
            is_copy_constructible<value_type>::value &&
            is_copy_constructible<Container>::value &&
            is_copy_constructible<Compare>::value &&
            is_nothrow_swappable<Container>::value &&
            is_nothrow_swappable<Compare>::value>());
    }
    
    RIN_QUEUE_CONSTEXPR void swap(priority_queue& other)
        noexcept(is_nothrow_swappable<Container>::value &&
                 is_nothrow_swappable<Compare>::value) {
        using std::swap;
        swap(c, other.c);
        swap(comp, other.comp);
    }
};

template<class T, class Container, class Compare, class Alloc>
struct uses_allocator<priority_queue<T, Container, Compare>, Alloc>
    : uses_allocator<Container, Alloc> {};

template<class T, class Container, class Compare>
RIN_QUEUE_CONSTEXPR void swap(priority_queue<T, Container, Compare>& lhs, 
          priority_queue<T, Container, Compare>& rhs)
    noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

#if __cplusplus >= 201703L
template<class Compare, class Container,
         typename enable_if<
             !_uses_allocator_detail::qualifies_as_allocator<Compare>::value &&
             !_uses_allocator_detail::qualifies_as_allocator<Container>::value,
             int>::type = 0>
priority_queue(Compare, Container)
    -> priority_queue<typename Container::value_type, Container, Compare>;

template<class InputIt,
         class Compare = less<typename iterator_traits<InputIt>::value_type>,
         class Container = vector<typename iterator_traits<InputIt>::value_type>,
         typename enable_if<
             !is_integral<remove_cv_t<InputIt>>::value &&
             !_uses_allocator_detail::qualifies_as_allocator<Compare>::value &&
             !_uses_allocator_detail::qualifies_as_allocator<Container>::value,
             int>::type = 0>
priority_queue(InputIt, InputIt, Compare = Compare(),
               Container = Container())
    -> priority_queue<typename iterator_traits<InputIt>::value_type,
                      Container, Compare>;

#if __cplusplus > 202002L
template<ranges::input_range R,
         class Compare = less<ranges::range_value_t<R>>,
         typename enable_if<
             !_uses_allocator_detail::qualifies_as_allocator<Compare>::value,
             int>::type = 0>
priority_queue(from_range_t, R&&, Compare = Compare())
    -> priority_queue<ranges::range_value_t<R>,
                      vector<ranges::range_value_t<R>>, Compare>;

template<ranges::input_range R, class Compare, class Alloc,
         typename enable_if<
             !_uses_allocator_detail::qualifies_as_allocator<Compare>::value &&
             _uses_allocator_detail::qualifies_as_allocator<Alloc>::value,
             int>::type = 0>
priority_queue(from_range_t, R&&, Compare, Alloc)
    -> priority_queue<ranges::range_value_t<R>,
                      vector<ranges::range_value_t<R>, Alloc>, Compare>;

template<ranges::input_range R, class Alloc,
         typename enable_if<
             _uses_allocator_detail::qualifies_as_allocator<Alloc>::value,
             int>::type = 0>
priority_queue(from_range_t, R&&, Alloc)
    -> priority_queue<ranges::range_value_t<R>,
                      vector<ranges::range_value_t<R>, Alloc>>;
#endif

template<class Compare, class Container, class Alloc,
         typename enable_if<
             !_uses_allocator_detail::qualifies_as_allocator<Compare>::value &&
             !_uses_allocator_detail::qualifies_as_allocator<Container>::value &&
             _uses_allocator_detail::qualifies_as_allocator<Alloc>::value &&
             uses_allocator<Container, Alloc>::value,
             int>::type = 0>
priority_queue(Compare, Container, Alloc)
    -> priority_queue<typename Container::value_type, Container, Compare>;

template<class InputIt, class Alloc,
         typename enable_if<
             !is_integral<remove_cv_t<InputIt>>::value &&
             _uses_allocator_detail::qualifies_as_allocator<Alloc>::value,
             int>::type = 0>
priority_queue(InputIt, InputIt, Alloc)
    -> priority_queue<
        typename iterator_traits<InputIt>::value_type,
        vector<typename iterator_traits<InputIt>::value_type, Alloc>,
        less<typename iterator_traits<InputIt>::value_type>>;

template<class InputIt, class Compare, class Alloc,
         typename enable_if<
             !is_integral<remove_cv_t<InputIt>>::value &&
             !_uses_allocator_detail::qualifies_as_allocator<Compare>::value &&
             _uses_allocator_detail::qualifies_as_allocator<Alloc>::value,
             int>::type = 0>
priority_queue(InputIt, InputIt, Compare, Alloc)
    -> priority_queue<
        typename iterator_traits<InputIt>::value_type,
        vector<typename iterator_traits<InputIt>::value_type, Alloc>, Compare>;

template<class InputIt, class Compare, class Container, class Alloc,
         typename enable_if<
             !is_integral<remove_cv_t<InputIt>>::value &&
             !_uses_allocator_detail::qualifies_as_allocator<Compare>::value &&
             !_uses_allocator_detail::qualifies_as_allocator<Container>::value &&
             _uses_allocator_detail::qualifies_as_allocator<Alloc>::value &&
             uses_allocator<Container, Alloc>::value,
             int>::type = 0>
priority_queue(InputIt, InputIt, Compare, Container, Alloc)
    -> priority_queue<typename Container::value_type, Container, Compare>;
#endif

} /* namespace std */

#undef RIN_QUEUE_CONSTEXPR

#endif /* RINCXX_QUEUE_H */
