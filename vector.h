/*
 * RinOS C++ Vector ✿
 * std::vector 互換実装
 */

#ifndef RINCXX_VECTOR_H
#define RINCXX_VECTOR_H

#include "rincxx.h"
#include "initializer_list.h"
#include "iterator.h"
#include "type_traits.h"
#include "memory.h"  /* std::allocator */
#include "functional.h"
#include "exception.h"
#include "cstdint.h"
#if __cplusplus >= 202002L
#include "compare.h"
#endif
#if __cplusplus > 202002L
#include "ranges.h"
#endif

#ifdef __cplusplus

namespace std {

namespace detail {
template<typename T>
inline void vector_exchange(T& left, T& right) noexcept(
    noexcept(T(std::move(left))) &&
    noexcept(left = std::move(right)) &&
    noexcept(right = std::move(left))) {
    if (&left == &right) return;
    T temporary(std::move(left));
    left = std::move(right);
    right = std::move(temporary);
}

template<typename T>
constexpr T* vector_pointer_address(T* pointer) noexcept { return pointer; }

#if __cplusplus >= 202002L
/* C++20 allocators may expose a fancy pointer whose only address
 * customization is pointer_traits::to_address.  Keep the allocation owner
 * as the allocator pointer, but derive the raw contiguous view through the
 * standard customization point. */
template<typename Pointer>
constexpr auto vector_pointer_address(const Pointer& pointer) noexcept
    -> decltype(std::to_address(pointer)) {
    return std::to_address(pointer);
}
#else
template<typename Pointer>
constexpr auto vector_pointer_address(const Pointer& pointer) noexcept
    -> decltype(pointer_traits<Pointer>::to_address(pointer)) {
    return pointer_traits<Pointer>::to_address(pointer);
}

template<typename Pointer>
constexpr auto vector_pointer_address(const Pointer& pointer) noexcept
    -> decltype(vector_pointer_address(pointer.operator->())) {
    return vector_pointer_address(pointer.operator->());
}
#endif

[[noreturn]] inline void vector_allocation_failure() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw bad_alloc();
#else
    rin_panic("[VEC] allocation failed");
#endif
}

[[noreturn]] inline void vector_length_failure() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw length_error("vector too long");
#else
    rin_panic("[VEC] size overflow");
#endif
}

[[noreturn]] inline void vector_at_contract_fail() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw out_of_range("vector::at position is out of range");
#else
    rin_panic("[VEC] at() out of range");
#endif
}

[[noreturn]] inline void vector_iterator_contract_fail() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw out_of_range("vector iterator does not belong to this vector");
#else
    rin_panic("[VEC] invalid iterator");
#endif
}
} /* namespace detail */

/* ═══════════════════════════════════════════════════════════════
 * vector
 * 標準互換の実装
 * ═══════════════════════════════════════════════════════════════*/

template<typename T, typename Allocator = allocator<T>>
class vector {
public:
    using value_type = T;
    using allocator_type = Allocator;
    using alloc_traits = allocator_traits<Allocator>;
    using size_type = size_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = typename alloc_traits::pointer;
    using const_pointer = typename alloc_traits::const_pointer;

    /* Standard approach: iterator is just T* */
    using iterator = T*;
    using const_iterator = const T*;
    using difference_type = ptrdiff_t;

private:
    pointer m_allocation = pointer();
    T* m_data;
    size_type m_size;
    size_type m_capacity;
#if __cplusplus >= 202002L
    [[no_unique_address]]
#endif
    Allocator m_alloc;

    /* An empty vector has no allocated pointer.  Keep end/iterator results
     * null instead of forming `nullptr + 0`, which is undefined pointer
     * arithmetic even though the resulting logical position is valid. */
    iterator iterator_at(size_type index) noexcept {
        return m_data ? m_data + index : nullptr;
    }

    const_iterator iterator_at(size_type index) const noexcept {
        return m_data ? m_data + index : nullptr;
    }

    size_type iterator_index(const_iterator position) const {
        size_type index = 0;
        if (pointer_index(position, index)) return index;
        detail::vector_iterator_contract_fail();
    }

    bool pointer_index(const_iterator position, size_type& index) const
        noexcept {
        if (!m_data) {
            if (position == nullptr && m_size == 0u) {
                index = 0u;
                return true;
            }
            return false;
        }
        const uintptr_t base = reinterpret_cast<uintptr_t>(m_data);
        const uintptr_t address = reinterpret_cast<uintptr_t>(position);
        if (address < base) return false;
        const uintptr_t distance = address - base;
        if (distance % sizeof(T) != 0u) return false;
        const uintptr_t raw_index = distance / sizeof(T);
        if (raw_index > static_cast<uintptr_t>(m_size)) return false;
        index = static_cast<size_type>(raw_index);
        return true;
    }

    size_type checked_size_add(size_type extra) const {
        const size_type limit = max_size();
        if (extra > limit - m_size)
            detail::vector_length_failure();
        return m_size + extra;
    }

    void relocate_move(T* new_data) {
        size_type constructed = 0;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            for (; constructed < m_size; ++constructed) {
                alloc_traits::construct(
                    m_alloc, new_data + constructed,
                    std::move(m_data[constructed]));
            }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            while (constructed != 0) {
                --constructed;
                alloc_traits::destroy(m_alloc, new_data + constructed);
            }
            throw;
        }
#endif
        for (size_type i = 0; i < m_size; ++i)
            alloc_traits::destroy(m_alloc, m_data + i);
    }

    void relocate_copy(T* new_data) {
        size_type constructed = 0;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            for (; constructed < m_size; ++constructed) {
                alloc_traits::construct(
                    m_alloc, new_data + constructed, m_data[constructed]);
            }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            while (constructed != 0) {
                --constructed;
                alloc_traits::destroy(m_alloc, new_data + constructed);
            }
            throw;
        }
#endif
        for (size_type i = 0; i < m_size; ++i)
            alloc_traits::destroy(m_alloc, m_data + i);
    }

    void relocate_noncopyable(T* new_data, true_type) {
        relocate_move(new_data);
    }

    void relocate_noncopyable(T* new_data, false_type) {
        if (m_size > 0) {
            rin_panic("[VEC] type cannot be moved or copied");
        }
    }

    void relocate_after_throwing_move(T* new_data, true_type) {
        relocate_copy(new_data);
    }

    void relocate_after_throwing_move(T* new_data, false_type) {
        relocate_noncopyable(new_data,
            integral_constant<bool, is_move_constructible<T>::value>());
    }

    void relocate_nontrivial(T* new_data, true_type) {
        relocate_move(new_data);
    }

    void relocate_nontrivial(T* new_data, false_type) {
        relocate_after_throwing_move(new_data,
            integral_constant<bool, is_copy_constructible<T>::value>());
    }

    void relocate_elements(T* new_data, true_type) {
        if (m_size > 0) {
            __builtin_memcpy(new_data, m_data, m_size * sizeof(T));
        }
    }

    void relocate_elements(T* new_data, false_type) {
        relocate_nontrivial(new_data,
            integral_constant<bool, is_nothrow_move_constructible<T>::value>());
    }

    void grow(size_type new_cap) {
        if (new_cap <= m_capacity) return;
        const size_type limit = max_size();
        if (new_cap > limit)
            detail::vector_length_failure();

        size_type cap = 4;
        if (m_capacity != 0) {
            cap = m_capacity > limit / 2 ? limit : m_capacity * 2;
        }
        if (cap < new_cap) cap = new_cap;

        pointer new_allocation = alloc_traits::allocate(m_alloc, cap);
        T* new_data = detail::vector_pointer_address(new_allocation);
        if (!new_data) {
            if (new_allocation != pointer()) {
                alloc_traits::deallocate(m_alloc, new_allocation, cap);
            }
            detail::vector_allocation_failure();
        }

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
        relocate_elements(new_data,
            integral_constant<bool, is_trivially_copyable<T>::value>());
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            alloc_traits::deallocate(m_alloc, new_allocation, cap);
            throw;
        }
#endif

        if (m_data) {
            alloc_traits::deallocate(m_alloc, m_allocation, m_capacity);
        }
        m_allocation = new_allocation;
        m_data = new_data;
        m_capacity = cap;
    }

    void destroy_all() {
        for (size_type i = 0; i < m_size; i++) {
            alloc_traits::destroy(m_alloc, m_data + i);
        }
    }

    template<typename... Args>
    iterator emplace_central(const_iterator pos, true_type, Args&&... args) {
        const size_type idx = iterator_index(pos);
        const size_type required = checked_size_add(1);
        vector candidate(m_alloc);
        candidate.reserve(required);
        for (size_type i = 0; i < idx; ++i)
            candidate.push_back(m_data[i]);
        candidate.emplace_back(std::forward<Args>(args)...);
        for (size_type i = idx; i < m_size; ++i)
            candidate.push_back(m_data[i]);
        swap(candidate);
        return iterator_at(idx);
    }

    template<typename... Args>
    iterator emplace_central(const_iterator pos, false_type, Args&&... args) {
        const size_type idx = iterator_index(pos);
        if (m_size >= m_capacity) grow(checked_size_add(1));

        for (size_type i = m_size; i > idx; --i) {
            alloc_traits::construct(m_alloc, m_data + i,
                                    std::move(m_data[i - 1]));
            alloc_traits::destroy(m_alloc, m_data + i - 1);
        }
        alloc_traits::construct(m_alloc, m_data + idx,
                                std::forward<Args>(args)...);
        ++m_size;
        return iterator_at(idx);
    }

    template<typename Value>
    iterator insert_central(const_iterator pos, Value&& value, true_type) {
        const size_type idx = iterator_index(pos);
        const size_type required = checked_size_add(1);
        vector candidate(m_alloc);
        candidate.reserve(required);
        for (size_type i = 0; i < idx; ++i)
            candidate.push_back(m_data[i]);
        candidate.emplace_back(std::forward<Value>(value));
        for (size_type i = idx; i < m_size; ++i)
            candidate.push_back(m_data[i]);
        swap(candidate);
        return iterator_at(idx);
    }

    template<typename Value>
    iterator insert_central(const_iterator pos, Value&& value, false_type) {
        const size_type idx = iterator_index(pos);
        if (m_size >= m_capacity) grow(checked_size_add(1));

        for (size_type i = m_size; i > idx; --i) {
            alloc_traits::construct(m_alloc, m_data + i,
                                    std::move(m_data[i - 1]));
            alloc_traits::destroy(m_alloc, m_data + i - 1);
        }
        alloc_traits::construct(m_alloc, m_data + idx,
                                std::forward<Value>(value));
        ++m_size;
        return iterator_at(idx);
    }

    iterator insert_count_central(const_iterator pos, size_type count,
                                  const T& value, true_type) {
        const size_type idx = iterator_index(pos);
        if (count == 0) return iterator_at(idx);

        T value_copy = value;
        const size_type required = checked_size_add(count);
        vector candidate(m_alloc);
        candidate.reserve(required);
        for (size_type i = 0; i < idx; ++i)
            candidate.push_back(m_data[i]);
        for (size_type i = 0; i < count; ++i)
            candidate.push_back(value_copy);
        for (size_type i = idx; i < m_size; ++i)
            candidate.push_back(m_data[i]);
        swap(candidate);
        return iterator_at(idx);
    }

    iterator insert_count_central(const_iterator pos, size_type count,
                                  const T& value, false_type) {
        const size_type idx = iterator_index(pos);
        if (count == 0) return iterator_at(idx);

        T value_copy = value;
        const size_type required = checked_size_add(count);
        if (required > m_capacity) grow(required);
        for (size_type i = m_size; i > idx; --i) {
            alloc_traits::construct(m_alloc,
                                    m_data + i + count - 1,
                                    std::move(m_data[i - 1]));
            alloc_traits::destroy(m_alloc, m_data + i - 1);
        }
        for (size_type i = 0; i < count; ++i)
            alloc_traits::construct(m_alloc, m_data + idx + i, value_copy);
        m_size += count;
        return iterator_at(idx);
    }

    template<typename InputIt>
    iterator insert_range_central(const_iterator pos, InputIt first,
                                  InputIt last, size_type count, true_type) {
        const size_type idx = iterator_index(pos);
        if (count == 0) return iterator_at(idx);

        const size_type required = checked_size_add(count);
        vector candidate(m_alloc);
        candidate.reserve(required);
        for (size_type i = 0; i < idx; ++i)
            candidate.push_back(m_data[i]);
        for (InputIt it = first; it != last; ++it)
            candidate.push_back(*it);
        for (size_type i = idx; i < m_size; ++i)
            candidate.push_back(m_data[i]);
        swap(candidate);
        return iterator_at(idx);
    }

    template<typename InputIt>
    iterator insert_range_central(const_iterator pos, InputIt first,
                                  InputIt last, size_type count, false_type) {
        const size_type idx = iterator_index(pos);
        if (count == 0) return iterator_at(idx);
        const size_type required = checked_size_add(count);
        if (required > m_capacity) grow(required);

        for (size_type i = m_size; i > idx; --i) {
            alloc_traits::construct(m_alloc,
                                    m_data + i + count - 1,
                                    std::move(m_data[i - 1]));
            alloc_traits::destroy(m_alloc, m_data + i - 1);
        }
        size_type i = 0;
        for (InputIt it = first; it != last; ++it, ++i)
            alloc_traits::construct(m_alloc, m_data + idx + i, *it);
        m_size += count;
        return iterator_at(idx);
    }

    template<typename InputIt>
    static auto range_is_empty(InputIt first, InputIt last, int)
        -> decltype(first == last, bool()) {
        return first == last;
    }

    template<typename InputIt>
    static bool range_is_empty(InputIt, InputIt, long) {
        /* InputIterator only requires !=; retain the candidate path when a
         * producer does not expose an equality operator. */
        return false;
    }

    template<typename InputIt>
    iterator insert_range_dispatch(const_iterator pos, InputIt first,
                                   InputIt last, true_type) {
        const size_type idx = iterator_index(pos);
        /* A valid empty range has no observable effect.  Return before
         * creating the copy-on-write candidate so an empty insertion cannot
         * spuriously fail its reserve allocation. */
        if (range_is_empty(first, last, 0)) return iterator_at(idx);
        vector candidate(m_alloc);
        candidate.reserve(m_size);
        for (size_type i = 0; i < idx; ++i)
            candidate.push_back(m_data[i]);
        for (InputIt it = first; it != last; ++it)
            candidate.push_back(*it);
        for (size_type i = idx; i < m_size; ++i)
            candidate.push_back(m_data[i]);
        swap(candidate);
        return iterator_at(idx);
    }

    template<typename InputIt>
    iterator insert_range_dispatch(const_iterator pos, InputIt first,
                                   InputIt last, false_type) {
        size_type count = 0;
        const size_type limit = max_size();
        for (InputIt it = first; it != last; ++it) {
            if (count == limit)
                detail::vector_length_failure();
            ++count;
        }
        return insert_range_central(pos, first, last, count, false_type());
    }

    iterator erase_central(const_iterator pos, true_type) {
        const size_type idx = iterator_index(pos);
        vector candidate(m_alloc);
        candidate.reserve(m_size);
        for (size_type i = 0; i < m_size; ++i) {
            if (i != idx)
                candidate.push_back(m_data[i]);
        }
        swap(candidate);
        return iterator_at(idx);
    }

    iterator erase_central(const_iterator pos, false_type) {
        const size_type idx = iterator_index(pos);
        alloc_traits::destroy(m_alloc, m_data + idx);
        for (size_type i = idx; i < m_size - 1; ++i) {
            alloc_traits::construct(m_alloc, m_data + i,
                                    std::move(m_data[i + 1]));
            alloc_traits::destroy(m_alloc, m_data + i + 1);
        }
        --m_size;
        return iterator_at(idx);
    }

    iterator erase_range_central(const_iterator first, const_iterator last,
                                 true_type) {
        const size_type idx_first = iterator_index(first);
        const size_type idx_last = iterator_index(last);
        const size_type count = idx_last - idx_first;
        if (count == 0) return iterator_at(idx_first);

        vector candidate(m_alloc);
        candidate.reserve(m_size);
        for (size_type i = 0; i < m_size; ++i) {
            if (i < idx_first || i >= idx_last)
                candidate.push_back(m_data[i]);
        }
        swap(candidate);
        return iterator_at(idx_first);
    }

    iterator erase_range_central(const_iterator first, const_iterator last,
                                 false_type) {
        const size_type idx_first = iterator_index(first);
        const size_type idx_last = iterator_index(last);
        const size_type count = idx_last - idx_first;
        if (count == 0) return iterator_at(idx_first);

        for (size_type i = idx_first; i < idx_last; ++i)
            alloc_traits::destroy(m_alloc, m_data + i);
        for (size_type i = idx_last; i < m_size; ++i) {
            alloc_traits::construct(m_alloc, m_data + i - count,
                                    std::move(m_data[i]));
            alloc_traits::destroy(m_alloc, m_data + i);
        }
        m_size -= count;
        return iterator_at(idx_first);
    }

    void copy_assign_candidate(const vector& other, false_type) {
        vector candidate(m_alloc);
        candidate.reserve(other.m_size);
        for (size_type i = 0; i < other.m_size; ++i)
            candidate.push_back(other.m_data[i]);
        swap(candidate);
    }

    void copy_assign_candidate(const vector& other, true_type) {
        vector candidate(other.m_alloc);
        candidate.reserve(other.m_size);
        for (size_type i = 0; i < other.m_size; ++i)
            candidate.push_back(other.m_data[i]);

        Allocator old_alloc(m_alloc);
        m_alloc = other.m_alloc;
        pointer old_allocation = m_allocation;
        T* old_data = m_data;
        const size_type old_size = m_size;
        const size_type old_capacity = m_capacity;
        for (size_type i = 0; i < old_size; ++i)
            alloc_traits::destroy(old_alloc, old_data + i);
        if (old_data)
            alloc_traits::deallocate(old_alloc, old_allocation, old_capacity);

        m_allocation = candidate.m_allocation;
        m_data = candidate.m_data;
        m_size = candidate.m_size;
        m_capacity = candidate.m_capacity;
        candidate.m_allocation = pointer();
        candidate.m_data = nullptr;
        candidate.m_size = 0;
        candidate.m_capacity = 0;
    }

    void move_assign_unequal(vector& other) {
        vector candidate(m_alloc);
        candidate.reserve(other.m_size);
        for (size_type i = 0; i < other.m_size; ++i)
            candidate.push_back(std::move(other.m_data[i]));
        swap(candidate);
        other.clear();
    }

#if __cplusplus > 202002L
    template<class R>
    vector stage_compatible_range(R&& range) {
        vector staged(m_alloc);
        auto first = ranges::begin(range);
        auto last = ranges::end(range);
        for (; first != last; ++first)
            staged.emplace_back(*first);
        return staged;
    }

    void append_staged_range(vector& staged, true_type) {
        if (staged.empty()) return;

        if (staged.size() > max_size() - m_size)
            detail::vector_length_failure();
        vector candidate(m_alloc);
        candidate.reserve(m_size + staged.size());
        for (size_type i = 0; i < m_size; ++i)
            candidate.push_back(m_data[i]);
        for (size_type i = 0; i < staged.size(); ++i)
            candidate.emplace_back(staged.m_data[i]);
        swap(candidate);
    }

    void append_staged_range(vector& staged, false_type) {
        if (staged.empty()) return;

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        const size_type original_size = m_size;
        try {
#endif
            for (size_type i = 0; i < staged.size(); ++i)
                emplace_back(std::move(staged.m_data[i]));
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            while (m_size != original_size) {
                --m_size;
                alloc_traits::destroy(m_alloc, m_data + m_size);
            }
            throw;
        }
#endif
    }

    iterator insert_staged_range(const_iterator pos, vector& staged,
                                 true_type) {
        return insert_range_dispatch(pos, staged.begin(), staged.end(),
                                     true_type());
    }

    iterator insert_staged_range(const_iterator pos, vector& staged,
                                 false_type) {
        return insert_range_dispatch(
            pos, make_move_iterator(staged.begin()),
            make_move_iterator(staged.end()), false_type());
    }
#endif

public:
    /* コンストラクタ */
    vector() : m_data(nullptr), m_size(0), m_capacity(0), m_alloc() {}

    /* Allocator-only constructor */
    explicit vector(const Allocator& alloc) : m_data(nullptr), m_size(0), m_capacity(0), m_alloc(alloc) {}

    explicit vector(size_type count) : m_data(nullptr), m_size(0), m_capacity(0), m_alloc() {
        resize(count);
    }

    /* Size + allocator constructor (default-initialize n elements) */
    vector(size_type count, const Allocator& alloc) : m_data(nullptr), m_size(0), m_capacity(0), m_alloc(alloc) {
        resize(count);
    }

    vector(size_type count, const T& value) : m_data(nullptr), m_size(0), m_capacity(0), m_alloc() {
        resize(count, value);
    }

    vector(size_type count, const T& value, const Allocator& alloc)
        : m_data(nullptr), m_size(0), m_capacity(0), m_alloc(alloc) {
        resize(count, value);
    }

    vector(const vector& other)
        : m_data(nullptr), m_size(0), m_capacity(0),
          m_alloc(alloc_traits::select_on_container_copy_construction(other.m_alloc)) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            reserve(other.m_size);
            for (size_type i = 0; i < other.m_size; i++) {
                alloc_traits::construct(m_alloc, m_data + m_size, other.m_data[i]);
                ++m_size;
            }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            destroy_all();
            if (m_data) {
                alloc_traits::deallocate(m_alloc, m_allocation, m_capacity);
            }
            m_allocation = pointer();
            m_data = nullptr;
            m_size = 0;
            m_capacity = 0;
            throw;
        }
#endif
    }

    vector(const vector& other, const Allocator& alloc)
        : m_data(nullptr), m_size(0), m_capacity(0), m_alloc(alloc) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            reserve(other.m_size);
            for (size_type i = 0; i < other.m_size; i++) {
                alloc_traits::construct(m_alloc, m_data + m_size, other.m_data[i]);
                ++m_size;
            }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            destroy_all();
            if (m_data) {
                alloc_traits::deallocate(m_alloc, m_allocation, m_capacity);
            }
            m_allocation = pointer();
            m_data = nullptr;
            m_size = 0;
            m_capacity = 0;
            throw;
        }
#endif
    }

    vector(vector&& other) noexcept
        : m_allocation(other.m_allocation), m_data(other.m_data), m_size(other.m_size), m_capacity(other.m_capacity),
          m_alloc(std::move(other.m_alloc)) {
        other.m_allocation = pointer();
        other.m_data = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
    }

    vector(vector&& other, const Allocator& alloc)
        : m_data(nullptr), m_size(0), m_capacity(0), m_alloc(alloc) {
        if (alloc == other.m_alloc) {
            m_allocation = other.m_allocation;
            m_data = other.m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            other.m_allocation = pointer();
            other.m_data = nullptr;
            other.m_size = 0;
            other.m_capacity = 0;
        } else {
            reserve(other.m_size);
            for (size_type i = 0; i < other.m_size; i++) {
                alloc_traits::construct(m_alloc, m_data + m_size, std::move(other.m_data[i]));
                ++m_size;
            }
        }
    }

    vector(initializer_list<T> init) : m_data(nullptr), m_size(0), m_capacity(0), m_alloc() {
        reserve(init.size());
        for (const auto& item : init) {
            alloc_traits::construct(m_alloc, m_data + m_size, item);
            ++m_size;
        }
    }

    vector(initializer_list<T> init, const Allocator& alloc)
        : m_data(nullptr), m_size(0), m_capacity(0), m_alloc(alloc) {
        reserve(init.size());
        for (const auto& item : init) {
            alloc_traits::construct(m_alloc, m_data + m_size, item);
            ++m_size;
        }
    }

    /* イテレータ範囲からのコンストラクタ (整数型でない場合のみ有効) */
    template<typename InputIt,
             typename = typename enable_if<!is_integral<InputIt>::value>::type>
    vector(InputIt first, InputIt last) : m_data(nullptr), m_size(0), m_capacity(0), m_alloc() {
        for (InputIt it = first; it != last; ++it) {
            push_back(*it);
        }
    }

    template<typename InputIt,
             typename = typename enable_if<!is_integral<InputIt>::value>::type>
    vector(InputIt first, InputIt last, const Allocator& alloc)
        : m_data(nullptr), m_size(0), m_capacity(0), m_alloc(alloc) {
        for (InputIt it = first; it != last; ++it) {
            push_back(*it);
        }
    }

#if __cplusplus > 202002L
    template<class R>
        requires detail::container_compatible_range<R, T>
    vector(from_range_t, R&& range,
           const Allocator& alloc = Allocator())
        : vector(alloc) {
        vector staged = stage_compatible_range(std::forward<R>(range));
        append_staged_range(staged,
            integral_constant<bool, is_copy_constructible<T>::value>());
    }
#endif

    ~vector() {
        destroy_all();
        if (m_data) {
            alloc_traits::deallocate(m_alloc, m_allocation, m_capacity);
        }
    }
    
    /* 代入 */
    vector& operator=(const vector& other) {
        if (this != &other)
            copy_assign_candidate(other,
                integral_constant<bool,
                    alloc_traits::propagate_on_container_copy_assignment::value>());
        return *this;
    }

    vector& operator=(vector&& other) noexcept(
        alloc_traits::propagate_on_container_move_assignment::value ||
        alloc_traits::is_always_equal::value) {
        if (this != &other) {
            constexpr bool propagate = alloc_traits::propagate_on_container_move_assignment::value;
            constexpr bool always_equal = alloc_traits::is_always_equal::value;

            if (propagate || always_equal) {
                /* Allocators propagate or are always equal - can steal the buffer */
                destroy_all();
                if (m_data) {
                    alloc_traits::deallocate(m_alloc, m_allocation, m_capacity);
                }
                m_allocation = other.m_allocation;
                m_data = other.m_data;
                m_size = other.m_size;
                m_capacity = other.m_capacity;
                if (propagate) {
                    m_alloc = std::move(other.m_alloc);
                }
                other.m_allocation = pointer();
                other.m_data = nullptr;
                other.m_size = 0;
                other.m_capacity = 0;
            } else {
                /* Allocators might differ - need runtime check */
                if (m_alloc == other.m_alloc) {
                    /* Can steal the buffer */
                    destroy_all();
                    if (m_data) {
                        alloc_traits::deallocate(m_alloc, m_allocation, m_capacity);
                    }
                    m_allocation = other.m_allocation;
                    m_data = other.m_data;
                    m_size = other.m_size;
                    m_capacity = other.m_capacity;
                    other.m_allocation = pointer();
                    other.m_data = nullptr;
                    other.m_size = 0;
                    other.m_capacity = 0;
                } else {
                    /* Allocators differ - must move elements (requires MoveInsertable T) */
                    move_assign_unequal(other);
                }
            }
        }
        return *this;
    }

    vector& operator=(initializer_list<T> ilist) {
        clear();
        reserve(ilist.size());
        for (const auto& item : ilist) {
            alloc_traits::construct(m_alloc, m_data + m_size, item);
            ++m_size;
        }
        return *this;
    }
    
    /* アクセス */
    reference operator[](size_type pos) noexcept { return m_data[pos]; }
    const_reference operator[](size_type pos) const noexcept { return m_data[pos]; }

    reference at(size_type pos) {
        if (pos >= m_size) {
            detail::vector_at_contract_fail();
        }
        return m_data[pos];
    }
    const_reference at(size_type pos) const {
        if (pos >= m_size) {
            detail::vector_at_contract_fail();
        }
        return m_data[pos];
    }

    reference front() noexcept { return m_data[0]; }
    const_reference front() const noexcept { return m_data[0]; }

    reference back() noexcept { return m_data[m_size - 1]; }
    const_reference back() const noexcept { return m_data[m_size - 1]; }

    T* data() noexcept { return m_data; }
    const T* data() const noexcept { return m_data; }

    /* アロケータ */
    allocator_type get_allocator() const noexcept { return m_alloc; }

    /* イテレータ */
    iterator begin() noexcept { return m_data; }
    const_iterator begin() const noexcept { return m_data; }
    const_iterator cbegin() const noexcept { return m_data; }
    
    iterator end() noexcept { return iterator_at(m_size); }
    const_iterator end() const noexcept { return iterator_at(m_size); }
    const_iterator cend() const noexcept { return iterator_at(m_size); }
    
    /* 逆イテレータ - 標準の std::reverse_iterator を使用 */
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    
    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    reverse_iterator rend() noexcept { return reverse_iterator(m_data); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(m_data); }
    
    /* 容量 */
    bool empty() const noexcept { return m_size == 0; }
    size_type size() const noexcept { return m_size; }
    size_type capacity() const noexcept { return m_capacity; }
    size_type max_size() const noexcept {
        const size_type object_limit = static_cast<size_type>(-1) / sizeof(T);
        const size_type allocator_limit = alloc_traits::max_size(m_alloc);
        return allocator_limit < object_limit ? allocator_limit : object_limit;
    }
    
    void reserve(size_type new_cap) {
        if (new_cap > m_capacity) grow(new_cap);
    }
    
    void shrink_to_fit() {
        if (m_size < m_capacity) {
            if (m_size == 0) {
                if (m_data) {
                    alloc_traits::deallocate(m_alloc, m_allocation, m_capacity);
                }
                m_allocation = pointer();
                m_data = nullptr;
                m_capacity = 0;
            } else {
                /* Try to allocate smaller buffer - on failure, just keep current */
                pointer new_allocation = alloc_traits::allocate(m_alloc, m_size);
                T* new_data = detail::vector_pointer_address(new_allocation);
                if (!new_data) {
                    if (new_allocation != pointer()) {
                        alloc_traits::deallocate(m_alloc, new_allocation, m_size);
                    }
                    return; /* Allocation failed, keep current capacity */
                }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
                try {
#endif
                relocate_elements(new_data,
                    integral_constant<bool, is_trivially_copyable<T>::value>());
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
                } catch (...) {
                    alloc_traits::deallocate(m_alloc, new_allocation, m_size);
                    throw;
                }
#endif
                alloc_traits::deallocate(m_alloc, m_allocation, m_capacity);
                m_allocation = new_allocation;
                m_data = new_data;
                m_capacity = m_size;
            }
        }
    }
    
    /* 変更 */
    void clear() noexcept {
        destroy_all();
        m_size = 0;
    }
    
    void push_back(const T& value) {
        if (m_size >= m_capacity) grow(checked_size_add(1));
        alloc_traits::construct(m_alloc, m_data + m_size, value);
        ++m_size;
    }

    void push_back(T&& value) {
        if (m_size >= m_capacity) grow(checked_size_add(1));
        alloc_traits::construct(m_alloc, m_data + m_size, std::move(value));
        ++m_size;
    }

#if __cplusplus > 202002L
    template<class R>
        requires detail::container_compatible_range<R, T>
    void append_range(R&& range) {
        /* Stage first so self-ranges and single-pass ranges are both
         * consumed exactly once.  The copyable path commits through a
         * candidate; move-only elements append in place and remove only the
         * suffix created by this call if construction fails. */
        vector staged = stage_compatible_range(std::forward<R>(range));
        append_staged_range(staged,
            integral_constant<bool, is_copy_constructible<T>::value>());
    }
#endif

    template<typename... Args>
    reference emplace_back(Args&&... args) {
        if (m_size >= m_capacity) grow(checked_size_add(1));
        alloc_traits::construct(m_alloc, m_data + m_size, std::forward<Args>(args)...);
        return m_data[m_size++];
    }

    template<typename... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        return emplace_central(pos,
            integral_constant<bool, is_copy_constructible<T>::value>(),
            std::forward<Args>(args)...);
    }

    void pop_back() noexcept {
        if (m_size > 0) {
            --m_size;
            alloc_traits::destroy(m_alloc, m_data + m_size);
        }
    }

    void resize(size_type count) {
        if (count > m_size) {
            reserve(count);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            const size_type original_size = m_size;
            try {
#endif
                for (; m_size < count; ++m_size)
                    alloc_traits::construct(m_alloc, m_data + m_size);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            } catch (...) {
                while (m_size != original_size) {
                    --m_size;
                    alloc_traits::destroy(m_alloc, m_data + m_size);
                }
                throw;
            }
#endif
        } else {
            for (size_type i = count; i < m_size; i++) {
                alloc_traits::destroy(m_alloc, m_data + i);
            }
            m_size = count;
        }
    }

    void resize(size_type count, const T& value) {
        if (count > m_size) {
            reserve(count);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            const size_type original_size = m_size;
            try {
#endif
                for (; m_size < count; ++m_size)
                    alloc_traits::construct(m_alloc, m_data + m_size, value);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            } catch (...) {
                while (m_size != original_size) {
                    --m_size;
                    alloc_traits::destroy(m_alloc, m_data + m_size);
                }
                throw;
            }
#endif
        } else {
            for (size_type i = count; i < m_size; i++) {
                alloc_traits::destroy(m_alloc, m_data + i);
            }
            m_size = count;
        }
    }
    
    iterator erase(const_iterator pos) {
        size_type index = 0;
        if (!pointer_index(pos, index) || index >= m_size) return end();
        return erase_central(pos,
            integral_constant<bool, is_copy_constructible<T>::value>());
    }

    iterator erase(const_iterator first, const_iterator last) {
        size_type first_index = 0;
        size_type last_index = 0;
        if (!pointer_index(first, first_index) ||
            !pointer_index(last, last_index) || first_index > last_index) {
            return end();
        }
        return erase_range_central(first, last,
            integral_constant<bool, is_copy_constructible<T>::value>());
    }

    iterator insert(const_iterator pos, const T& value) {
        return insert_central(pos, value,
            integral_constant<bool, is_copy_constructible<T>::value>());
    }

    iterator insert(const_iterator pos, T&& value) {
        return insert_central(pos, std::move(value),
            integral_constant<bool, is_copy_constructible<T>::value>());
    }

    iterator insert(const_iterator pos, size_type count, const T& value) {
        return insert_count_central(pos, count, value,
            integral_constant<bool, is_copy_constructible<T>::value>());
    }

    template<typename InputIt,
             typename = typename enable_if<!is_integral<InputIt>::value>::type>
    iterator insert(const_iterator pos, InputIt first, InputIt last) {
        return insert_range_dispatch(pos, first, last,
            integral_constant<bool, is_copy_constructible<T>::value>());
    }

    iterator insert(const_iterator pos, initializer_list<T> ilist) {
        return insert(pos, ilist.begin(), ilist.end());
    }

#if __cplusplus > 202002L
    template<class R>
        requires detail::container_compatible_range<R, T>
    iterator insert_range(const_iterator pos, R&& range) {
        const size_type index = iterator_index(pos);
        if (index > m_size)
            detail::vector_at_contract_fail();

        /* A range may alias this vector or expose only a single-pass
         * iterator.  Owning the source before mutation avoids both a second
         * traversal and invalidated source iterators. */
        vector staged = stage_compatible_range(std::forward<R>(range));
        return insert_staged_range(iterator_at(index), staged,
            integral_constant<bool, is_copy_constructible<T>::value>());
    }
#endif

    /* assign */
    template<typename InputIt,
             typename = typename enable_if<!is_integral<InputIt>::value>::type>
    void assign(InputIt first, InputIt last) {
        vector candidate(m_alloc);
        for (InputIt it = first; it != last; ++it)
            candidate.push_back(*it);
        swap(candidate);
    }

    void assign(size_type count, const T& value) {
        vector candidate(m_alloc);
        candidate.reserve(count);
        for (size_type i = 0; i < count; ++i)
            candidate.push_back(value);
        swap(candidate);
    }

    void assign(initializer_list<T> ilist) {
        assign(ilist.begin(), ilist.end());
    }

#if __cplusplus > 202002L
    template<class R>
        requires detail::container_compatible_range<R, T>
    void assign_range(R&& range) {
        static_assert(assignable_from<T&, ranges::range_reference_t<R>>,
                      "vector::assign_range requires assignable elements");
        vector staged = stage_compatible_range(std::forward<R>(range));
        swap(staged);
    }
#endif

    /* swap */
    void swap(vector& other) noexcept(
        alloc_traits::propagate_on_container_swap::value ||
        alloc_traits::is_always_equal::value) {
        detail::vector_exchange(m_allocation, other.m_allocation);
        detail::vector_exchange(m_data, other.m_data);
        detail::vector_exchange(m_size, other.m_size);
        detail::vector_exchange(m_capacity, other.m_capacity);
        if (alloc_traits::propagate_on_container_swap::value) {
            detail::vector_exchange(m_alloc, other.m_alloc);
        }
        /* Note: if !propagate && allocators differ, this is UB per standard */
    }
};

#if __cplusplus > 202002L
template<ranges::input_range R,
         class Allocator = allocator<ranges::range_value_t<R>>>
requires _uses_allocator_detail::qualifies_as_allocator<Allocator>::value
vector(from_range_t, R&&, Allocator = Allocator())
    -> vector<ranges::range_value_t<R>, Allocator>;
#endif

/* vector比較演算子 */
template<typename T, typename Alloc>
inline bool operator==(const vector<T, Alloc>& lhs, const vector<T, Alloc>& rhs) {
    if (lhs.size() != rhs.size()) return false;
    for (typename vector<T, Alloc>::size_type i = 0; i < lhs.size(); ++i) {
        if (!(lhs[i] == rhs[i])) return false;
    }
    return true;
}

template<typename T, typename Alloc>
inline bool operator!=(const vector<T, Alloc>& lhs, const vector<T, Alloc>& rhs) {
    return !(lhs == rhs);
}

template<typename T, typename Alloc>
inline bool operator<(const vector<T, Alloc>& lhs, const vector<T, Alloc>& rhs) {
    typename vector<T, Alloc>::size_type minSize = lhs.size() < rhs.size() ? lhs.size() : rhs.size();
    for (typename vector<T, Alloc>::size_type i = 0; i < minSize; ++i) {
        if (lhs[i] < rhs[i]) return true;
        if (rhs[i] < lhs[i]) return false;
    }
    return lhs.size() < rhs.size();
}

template<typename T, typename Alloc>
inline bool operator<=(const vector<T, Alloc>& lhs, const vector<T, Alloc>& rhs) {
    return !(rhs < lhs);
}

template<typename T, typename Alloc>
inline bool operator>(const vector<T, Alloc>& lhs, const vector<T, Alloc>& rhs) {
    return rhs < lhs;
}

template<typename T, typename Alloc>
inline bool operator>=(const vector<T, Alloc>& lhs, const vector<T, Alloc>& rhs) {
    return !(lhs < rhs);
}

#if __cplusplus >= 202002L
template<typename T, typename Alloc>
constexpr auto operator<=>(const vector<T, Alloc>& lhs,
                           const vector<T, Alloc>& rhs)
    -> detail::synth_three_way_result_t<T> {
    using result_type = detail::synth_three_way_result_t<T>;
    const typename vector<T, Alloc>::size_type common_size =
        lhs.size() < rhs.size() ? lhs.size() : rhs.size();
    for (typename vector<T, Alloc>::size_type i = 0; i < common_size; ++i) {
        const auto result = detail::synth_three_way(lhs[i], rhs[i]);
        if (result != 0) return result;
    }
    if (lhs.size() < rhs.size()) return result_type::less;
    if (rhs.size() < lhs.size()) return result_type::greater;
    return result_type::equivalent;
}
#endif

/* 非メンバー swap */
template<typename T, typename Alloc>
inline void swap(vector<T, Alloc>& lhs, vector<T, Alloc>& rhs) noexcept {
    lhs.swap(rhs);
}

/* ═══════════════════════════════════════════════════════════════
 * erase / erase_if (C++20)
 * Note: Requires move-assignable T (standard behavior)
 * ═══════════════════════════════════════════════════════════════*/

#if __cplusplus >= 202002L
template<typename T, typename Alloc, typename U>
typename vector<T, Alloc>::size_type
erase(vector<T, Alloc>& c, const U& value) {
    static_assert(is_move_assignable<T>::value,
                  "std::erase requires move-assignable T");
    auto it = c.begin();
    auto end = c.end();
    auto write = it;

    while (it != end) {
        if (!(*it == value)) {
            if (write != it) *write = std::move(*it);
            ++write;
        }
        ++it;
    }

    auto erased = c.end() - write;
    c.erase(write, c.end());
    return static_cast<typename vector<T, Alloc>::size_type>(erased);
}

template<typename T, typename Alloc, typename Pred>
typename vector<T, Alloc>::size_type
erase_if(vector<T, Alloc>& c, Pred pred) {
    static_assert(is_move_assignable<T>::value,
                  "std::erase_if requires move-assignable T");
    auto it = c.begin();
    auto end = c.end();
    auto write = it;

    while (it != end) {
        if (!pred(*it)) {
            if (write != it) *write = std::move(*it);
            ++write;
        }
        ++it;
    }

    auto erased = c.end() - write;
    c.erase(write, c.end());
    return static_cast<typename vector<T, Alloc>::size_type>(erased);
}
#endif

template<typename Alloc>
struct hash<vector<bool, Alloc>> {
    size_t operator()(const vector<bool, Alloc>& value) const noexcept {
        size_t result = sizeof(size_t) == 8
            ? static_cast<size_t>(14695981039346656037ull)
            : static_cast<size_t>(2166136261u);
        const size_t prime = sizeof(size_t) == 8
            ? static_cast<size_t>(1099511628211ull)
            : static_cast<size_t>(16777619u);
        for (size_t index = 0; index < value.size(); ++index) {
            result ^= static_cast<size_t>(value[index]);
            result *= prime;
        }
        return result;
    }
};

} /* namespace std */

#endif /* __cplusplus */
#endif /* RINCXX_VECTOR_H */
