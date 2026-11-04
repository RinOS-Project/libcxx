/*
 * RinOS C++ <inplace_vector>
 * Fixed-capacity contiguous storage with no dynamic allocation.
 */

#ifndef RINCXX_INPLACE_VECTOR_H
#define RINCXX_INPLACE_VECTOR_H

#include "rincxx.h"

#if defined(__cplusplus) && __cplusplus > 202002L

#include "algorithm.h"
#include "compare.h"
#include "cstdint.h"
#include "cstdint.h"
#include "exception.h"
#include "initializer_list.h"
#include "iterator.h"
#include "memory.h"
#include "optional.h"
#include "ranges.h"
#include "type_traits.h"
#include "utility.h"

namespace std {

/* C++26 optional<T&> is the result type used by the non-throwing append
 * operations. The existing optional<T> implementation predates that
 * specialization, so provide the reference representation here without
 * changing optional's value-storage implementation. */
template<typename T>
class optional<T&> {
    T* value_;

public:
    using value_type = T;

    constexpr optional() noexcept : value_(nullptr) {}
    constexpr optional(nullopt_t) noexcept : value_(nullptr) {}
    constexpr explicit optional(in_place_t, T& value) noexcept
        : value_(__builtin_addressof(value)) {}
    constexpr optional(T& value) noexcept
        : value_(__builtin_addressof(value)) {}

    constexpr optional& operator=(nullopt_t) noexcept {
        value_ = nullptr;
        return *this;
    }

    constexpr T* operator->() const noexcept { return value_; }
    constexpr T& operator*() const noexcept { return *value_; }
    constexpr explicit operator bool() const noexcept {
        return value_ != nullptr;
    }
    constexpr bool has_value() const noexcept { return value_ != nullptr; }
    constexpr T& value() const {
        if (!value_) detail::optional_access_fail();
        return *value_;
    }
    constexpr void reset() noexcept { value_ = nullptr; }
};

namespace detail {

[[noreturn]] inline void inplace_vector_capacity_failure() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw bad_alloc();
#else
    rin_panic("[INPLACE_VECTOR] capacity exceeded");
#endif
}

[[noreturn]] inline void inplace_vector_access_failure() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw out_of_range("inplace_vector index is out of range");
#else
    rin_panic("[INPLACE_VECTOR] index out of range");
#endif
}

[[noreturn]] inline void inplace_vector_iterator_failure() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw out_of_range("inplace_vector iterator does not belong to this object");
#else
    rin_panic("[INPLACE_VECTOR] invalid iterator");
#endif
}

} /* namespace detail */

template<typename T, size_t N>
class inplace_vector {
public:
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

private:
    static_assert(N > 0, "the zero-capacity specialization is separate");
    static_assert(is_object<T>::value && !is_array<T>::value,
                  "inplace_vector requires a non-array object type");

    union storage_type {
        unsigned char inactive;
        T elements[N];

        constexpr storage_type() noexcept : inactive(0) {}
        storage_type(const storage_type&) = default;
        storage_type(storage_type&&) = default;
        storage_type& operator=(const storage_type&) = default;
        storage_type& operator=(storage_type&&) = default;
        constexpr ~storage_type()
            requires is_trivially_destructible<T>::value = default;
        ~storage_type()
            requires (!is_trivially_destructible<T>::value) {}
    };

    storage_type storage_;
    size_t size_;

    constexpr T* slot(size_t index) noexcept {
        return __builtin_addressof(storage_.elements[index]);
    }

    constexpr const T* slot(size_t index) const noexcept {
        return __builtin_addressof(storage_.elements[index]);
    }

    constexpr iterator iterator_at(size_t index) noexcept {
        return data() + index;
    }

    constexpr const_iterator iterator_at(size_t index) const noexcept {
        return data() + index;
    }

    constexpr size_t iterator_index(const_iterator position) const {
        const uintptr_t first = reinterpret_cast<uintptr_t>(data());
        const uintptr_t address = reinterpret_cast<uintptr_t>(position);
        const uintptr_t bytes = size_ * sizeof(T);
        if (address < first || address - first > bytes ||
            (address - first) % sizeof(T) != 0)
            detail::inplace_vector_iterator_failure();
        return static_cast<size_t>((address - first) / sizeof(T));
    }

    constexpr void destroy_from(size_t first) noexcept {
        while (size_ > first) {
            --size_;
            destroy_at(slot(size_));
        }
    }

    template<typename U>
    constexpr iterator insert_one(size_t index, U&& value) {
        if (size_ == N) detail::inplace_vector_capacity_failure();
        if (index == size_) {
            construct_at(slot(size_), std::forward<U>(value));
            ++size_;
            return iterator_at(index);
        }

        const size_t old_size = size_;
        construct_at(slot(old_size), std::move(*slot(old_size - 1)));
        ++size_;
        for (size_t i = old_size - 1; i > index; --i)
            *slot(i) = std::move(*slot(i - 1));
        *slot(index) = std::forward<U>(value);
        return iterator_at(index);
    }

    constexpr iterator insert_staged(size_t index,
                                     inplace_vector& staged) {
        const size_t count = staged.size_;
        if (count > N - size_)
            detail::inplace_vector_capacity_failure();
        for (size_t i = 0; i < count; ++i)
            insert_one(index + i, std::move(*staged.slot(i)));
        return iterator_at(index);
    }

    template<typename InputIt, typename Sentinel>
    constexpr void construct_range(InputIt first, Sentinel last) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            for (; first != last; ++first)
                emplace_back(*first);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            clear();
            throw;
        }
#endif
    }

public:
    constexpr inplace_vector() noexcept : storage_(), size_(0) {}

    constexpr explicit inplace_vector(size_type count) : inplace_vector() {
        if (count > N) detail::inplace_vector_capacity_failure();
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            while (size_ < count) emplace_back();
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            clear();
            throw;
        }
#endif
    }

    constexpr inplace_vector(size_type count, const T& value)
        : inplace_vector() {
        if (count > N) detail::inplace_vector_capacity_failure();
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            while (size_ < count) emplace_back(value);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            clear();
            throw;
        }
#endif
    }

    template<typename InputIt,
             typename = enable_if_t<!is_integral<InputIt>::value>>
    constexpr inplace_vector(InputIt first, InputIt last) : inplace_vector() {
        construct_range(first, last);
    }

    template<typename R>
        requires detail::container_compatible_range<R, T>
    constexpr inplace_vector(from_range_t, R&& range) : inplace_vector() {
        if constexpr (ranges::sized_range<R>) {
            if (ranges::size(range) > N)
                detail::inplace_vector_capacity_failure();
        }
        construct_range(ranges::begin(range), ranges::end(range));
    }

    constexpr inplace_vector(const inplace_vector&)
        requires is_trivially_copy_constructible<T>::value = default;

    constexpr inplace_vector(const inplace_vector& other)
        requires (!is_trivially_copy_constructible<T>::value)
        : inplace_vector() {
        construct_range(other.begin(), other.end());
    }

    constexpr inplace_vector(inplace_vector&&)
        noexcept(is_nothrow_move_constructible<T>::value)
        requires is_trivially_move_constructible<T>::value = default;

    constexpr inplace_vector(inplace_vector&& other)
        noexcept(is_nothrow_move_constructible<T>::value)
        requires (!is_trivially_move_constructible<T>::value)
        : inplace_vector() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            while (size_ < other.size_) {
                emplace_back(std::move(*other.slot(size_)));
            }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            clear();
            throw;
        }
#endif
        other.clear();
    }

    constexpr inplace_vector(initializer_list<T> values) : inplace_vector() {
        construct_range(values.begin(), values.end());
    }

    constexpr ~inplace_vector()
        requires is_trivially_destructible<T>::value = default;

    constexpr ~inplace_vector()
        requires (!is_trivially_destructible<T>::value) { clear(); }

    constexpr inplace_vector& operator=(const inplace_vector&)
        requires (is_trivially_destructible<T>::value &&
                  is_trivially_copy_constructible<T>::value &&
                  is_trivially_copy_assignable<T>::value) = default;

    constexpr inplace_vector& operator=(const inplace_vector& other)
        requires (!(is_trivially_destructible<T>::value &&
                    is_trivially_copy_constructible<T>::value &&
                    is_trivially_copy_assignable<T>::value)) {
        if (this != &other) assign(other.begin(), other.end());
        return *this;
    }

    constexpr inplace_vector& operator=(inplace_vector&& other)
        noexcept(is_nothrow_move_assignable<T>::value &&
                 is_nothrow_move_constructible<T>::value)
        requires (is_trivially_destructible<T>::value &&
                  is_trivially_move_constructible<T>::value &&
                  is_trivially_move_assignable<T>::value) = default;

    constexpr inplace_vector& operator=(inplace_vector&& other)
        noexcept(is_nothrow_move_assignable<T>::value &&
                 is_nothrow_move_constructible<T>::value)
        requires (!(is_trivially_destructible<T>::value &&
                    is_trivially_move_constructible<T>::value &&
                    is_trivially_move_assignable<T>::value)) {
        if (this != &other) {
            clear();
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            try {
#endif
                while (size_ < other.size_)
                    emplace_back(std::move(*other.slot(size_)));
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            } catch (...) {
                clear();
                throw;
            }
#endif
            other.clear();
        }
        return *this;
    }

    constexpr inplace_vector& operator=(initializer_list<T> values) {
        assign(values.begin(), values.end());
        return *this;
    }

    template<typename InputIt, typename Sentinel,
             typename = enable_if_t<!is_integral<InputIt>::value>>
    constexpr void assign(InputIt first, Sentinel last) {
        inplace_vector staged(first, last);
        clear();
        for (size_type i = 0; i < staged.size_; ++i)
            emplace_back(std::move(*staged.slot(i)));
    }

    constexpr void assign(size_type count, const T& value) {
        if (count > N) detail::inplace_vector_capacity_failure();
        if (count == 0) {
            clear();
            return;
        }
        T stable_value(value);
        clear();
        while (size_ < count) emplace_back(stable_value);
    }

    constexpr void assign(initializer_list<T> values) {
        assign(values.begin(), values.end());
    }

    template<typename R>
        requires detail::container_compatible_range<R, T>
    constexpr void assign_range(R&& range) {
        inplace_vector staged(from_range, std::forward<R>(range));
        clear();
        for (size_type i = 0; i < staged.size_; ++i)
            emplace_back(std::move(*staged.slot(i)));
    }

    constexpr iterator begin() noexcept { return data(); }
    constexpr const_iterator begin() const noexcept { return data(); }
    constexpr const_iterator cbegin() const noexcept { return begin(); }
    constexpr iterator end() noexcept { return iterator_at(size_); }
    constexpr const_iterator end() const noexcept { return iterator_at(size_); }
    constexpr const_iterator cend() const noexcept { return end(); }
    constexpr reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    constexpr const_reverse_iterator rbegin() const noexcept {
        return const_reverse_iterator(end());
    }
    constexpr const_reverse_iterator crbegin() const noexcept { return rbegin(); }
    constexpr reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    constexpr const_reverse_iterator rend() const noexcept {
        return const_reverse_iterator(begin());
    }
    constexpr const_reverse_iterator crend() const noexcept { return rend(); }

    constexpr bool empty() const noexcept { return size_ == 0; }
    constexpr size_type size() const noexcept { return size_; }
    static constexpr size_type max_size() noexcept { return N; }
    static constexpr size_type capacity() noexcept { return N; }

    static constexpr void reserve(size_type requested) {
        if (requested > N) detail::inplace_vector_capacity_failure();
    }
    static constexpr void shrink_to_fit() noexcept {}

    constexpr void resize(size_type requested) {
        if (requested > N) detail::inplace_vector_capacity_failure();
        if (requested < size_) {
            destroy_from(requested);
            return;
        }
        const size_type old_size = size_;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            while (size_ < requested) emplace_back();
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            destroy_from(old_size);
            throw;
        }
#endif
    }

    constexpr void resize(size_type requested, const T& value) {
        if (requested > N) detail::inplace_vector_capacity_failure();
        if (requested <= size_) {
            destroy_from(requested);
            return;
        }
        T stable_value(value);
        const size_type old_size = size_;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            while (size_ < requested) emplace_back(stable_value);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            destroy_from(old_size);
            throw;
        }
#endif
    }

    constexpr reference operator[](size_type index) noexcept {
        return *slot(index);
    }
    constexpr const_reference operator[](size_type index) const noexcept {
        return *slot(index);
    }
    constexpr reference at(size_type index) {
        if (index >= size_) detail::inplace_vector_access_failure();
        return *slot(index);
    }
    constexpr const_reference at(size_type index) const {
        if (index >= size_) detail::inplace_vector_access_failure();
        return *slot(index);
    }
    constexpr reference front() noexcept { return *slot(0); }
    constexpr const_reference front() const noexcept { return *slot(0); }
    constexpr reference back() noexcept { return *slot(size_ - 1); }
    constexpr const_reference back() const noexcept { return *slot(size_ - 1); }
    constexpr pointer data() noexcept { return slot(0); }
    constexpr const_pointer data() const noexcept { return slot(0); }

    template<typename... Args>
    constexpr reference emplace_back(Args&&... args) {
        if (size_ == N) detail::inplace_vector_capacity_failure();
        construct_at(slot(size_), std::forward<Args>(args)...);
        ++size_;
        return back();
    }

    constexpr reference push_back(const T& value) { return emplace_back(value); }
    constexpr reference push_back(T&& value) {
        return emplace_back(std::move(value));
    }

    template<typename R>
        requires detail::container_compatible_range<R, T>
    constexpr void append_range(R&& range) {
        inplace_vector staged(from_range, std::forward<R>(range));
        insert_staged(size_, staged);
    }

    constexpr void pop_back() noexcept {
        --size_;
        destroy_at(slot(size_));
    }

    template<typename... Args>
    constexpr optional<reference> try_emplace_back(Args&&... args) {
        if (size_ == N) return nullopt;
        construct_at(slot(size_), std::forward<Args>(args)...);
        ++size_;
        return optional<reference>(in_place, back());
    }

    constexpr optional<reference> try_push_back(const T& value) {
        return try_emplace_back(value);
    }
    constexpr optional<reference> try_push_back(T&& value) {
        return try_emplace_back(std::move(value));
    }

    template<typename... Args>
    constexpr reference unchecked_emplace_back(Args&&... args) {
        return *try_emplace_back(std::forward<Args>(args)...);
    }
    constexpr reference unchecked_push_back(const T& value) {
        return *try_push_back(value);
    }
    constexpr reference unchecked_push_back(T&& value) {
        return *try_push_back(std::move(value));
    }

    template<typename... Args>
    constexpr iterator emplace(const_iterator position, Args&&... args) {
        const size_type index = iterator_index(position);
        if (index == size_) {
            emplace_back(std::forward<Args>(args)...);
            return iterator_at(index);
        }
        if (size_ == N) detail::inplace_vector_capacity_failure();
        T staged(std::forward<Args>(args)...);
        return insert_one(index, std::move(staged));
    }

    constexpr iterator insert(const_iterator position, const T& value) {
        const size_type index = iterator_index(position);
        if (size_ == N) detail::inplace_vector_capacity_failure();
        T staged(value);
        return insert_one(index, std::move(staged));
    }
    constexpr iterator insert(const_iterator position, T&& value) {
        const size_type index = iterator_index(position);
        if (size_ == N) detail::inplace_vector_capacity_failure();
        T staged(std::move(value));
        return insert_one(index, std::move(staged));
    }

    constexpr iterator insert(const_iterator position, size_type count,
                              const T& value) {
        const size_type index = iterator_index(position);
        if (count > N - size_)
            detail::inplace_vector_capacity_failure();
        inplace_vector staged(count, value);
        return insert_staged(index, staged);
    }

    template<typename InputIt,
             typename = enable_if_t<!is_integral<InputIt>::value>>
    constexpr iterator insert(const_iterator position, InputIt first,
                              InputIt last) {
        const size_type index = iterator_index(position);
        inplace_vector staged(first, last);
        return insert_staged(index, staged);
    }

    template<typename R>
        requires detail::container_compatible_range<R, T>
    constexpr iterator insert_range(const_iterator position, R&& range) {
        const size_type index = iterator_index(position);
        inplace_vector staged(from_range, std::forward<R>(range));
        return insert_staged(index, staged);
    }

    constexpr iterator insert(const_iterator position,
                              initializer_list<T> values) {
        return insert(position, values.begin(), values.end());
    }

    constexpr iterator erase(const_iterator position) {
        const size_type index = iterator_index(position);
        if (index == size_) detail::inplace_vector_iterator_failure();
        return erase(position, position + 1);
    }

    constexpr iterator erase(const_iterator first, const_iterator last) {
        const size_type begin_index = iterator_index(first);
        const size_type end_index = iterator_index(last);
        if (end_index < begin_index)
            detail::inplace_vector_iterator_failure();
        const size_type count = end_index - begin_index;
        for (size_type i = end_index; i < size_; ++i)
            *slot(i - count) = std::move(*slot(i));
        destroy_from(size_ - count);
        return iterator_at(begin_index);
    }

    constexpr void swap(inplace_vector& other)
        noexcept(is_nothrow_swappable<T>::value &&
                 is_nothrow_move_constructible<T>::value) {
        if (this == &other) return;
        const size_type shared = size_ < other.size_ ? size_ : other.size_;
        for (size_type i = 0; i < shared; ++i)
            std::swap(*slot(i), *other.slot(i));
        if (size_ < other.size_) {
            const size_type old_other_size = other.size_;
            while (size_ < old_other_size)
                emplace_back(std::move(*other.slot(size_)));
            other.destroy_from(shared);
        } else if (other.size_ < size_) {
            const size_type old_size = size_;
            while (other.size_ < old_size)
                other.emplace_back(std::move(*slot(other.size_)));
            destroy_from(shared);
        }
    }

    constexpr void clear() noexcept { destroy_from(0); }

    friend constexpr bool operator==(const inplace_vector& left,
                                     const inplace_vector& right) {
        if (left.size_ != right.size_) return false;
        for (size_type i = 0; i < left.size_; ++i)
            if (!(left[i] == right[i])) return false;
        return true;
    }

    friend constexpr auto operator<=>(const inplace_vector& left,
                                      const inplace_vector& right)
        requires requires(const T& a, const T& b) {
            detail::synth_three_way(a, b);
        }
    {
        using element_order = detail::synth_three_way_result_t<T>;
        using result_order = common_comparison_category_t<
            element_order, strong_ordering>;
        const size_type shared = left.size_ < right.size_
            ? left.size_ : right.size_;
        for (size_type i = 0; i < shared; ++i) {
            auto result = detail::synth_three_way(left[i], right[i]);
            if (result != 0) return static_cast<result_order>(result);
        }
        return static_cast<result_order>(left.size_ <=> right.size_);
    }

    friend constexpr void swap(inplace_vector& left, inplace_vector& right)
        noexcept(noexcept(left.swap(right))) {
        left.swap(right);
    }
};

/* The empty specialization has no data member, preserving the standard's
 * zero-size triviality guarantees while keeping operations safe at N == 0. */
template<typename T>
class inplace_vector<T, 0> {
    static constexpr void fail_if_nonempty(size_t requested) {
        if (requested != 0) detail::inplace_vector_capacity_failure();
    }

public:
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

    constexpr inplace_vector() noexcept = default;
    constexpr explicit inplace_vector(size_type count) { fail_if_nonempty(count); }
    constexpr inplace_vector(size_type count, const T&) { fail_if_nonempty(count); }

    template<typename InputIt,
             typename = enable_if_t<!is_integral<InputIt>::value>>
    constexpr inplace_vector(InputIt first, InputIt last) {
        if (first != last) detail::inplace_vector_capacity_failure();
    }

    template<typename R>
        requires detail::container_compatible_range<R, T>
    constexpr inplace_vector(from_range_t, R&& range) {
        auto first = ranges::begin(range);
        auto last = ranges::end(range);
        if (first != last)
            detail::inplace_vector_capacity_failure();
    }

    constexpr inplace_vector(const inplace_vector&) noexcept = default;
    constexpr inplace_vector(inplace_vector&&) noexcept = default;
    constexpr inplace_vector(initializer_list<T> values) {
        if (values.size()) detail::inplace_vector_capacity_failure();
    }
    constexpr inplace_vector& operator=(const inplace_vector&) noexcept = default;
    constexpr inplace_vector& operator=(inplace_vector&&) noexcept = default;
    constexpr inplace_vector& operator=(initializer_list<T> values) {
        if (values.size()) detail::inplace_vector_capacity_failure();
        return *this;
    }

    template<typename InputIt,
             typename = enable_if_t<!is_integral<InputIt>::value>>
    constexpr void assign(InputIt first, InputIt last) {
        if (first != last) detail::inplace_vector_capacity_failure();
    }
    constexpr void assign(size_type count, const T&) { fail_if_nonempty(count); }
    constexpr void assign(initializer_list<T> values) {
        if (values.size()) detail::inplace_vector_capacity_failure();
    }
    template<typename R>
        requires detail::container_compatible_range<R, T>
    constexpr void assign_range(R&& range) {
        auto first = ranges::begin(range);
        auto last = ranges::end(range);
        if (first != last)
            detail::inplace_vector_capacity_failure();
    }

    constexpr iterator begin() noexcept { return nullptr; }
    constexpr const_iterator begin() const noexcept { return nullptr; }
    constexpr const_iterator cbegin() const noexcept { return nullptr; }
    constexpr iterator end() noexcept { return nullptr; }
    constexpr const_iterator end() const noexcept { return nullptr; }
    constexpr const_iterator cend() const noexcept { return nullptr; }
    constexpr reverse_iterator rbegin() noexcept { return reverse_iterator(nullptr); }
    constexpr const_reverse_iterator rbegin() const noexcept {
        return const_reverse_iterator(nullptr);
    }
    constexpr const_reverse_iterator crbegin() const noexcept { return rbegin(); }
    constexpr reverse_iterator rend() noexcept { return reverse_iterator(nullptr); }
    constexpr const_reverse_iterator rend() const noexcept {
        return const_reverse_iterator(nullptr);
    }
    constexpr const_reverse_iterator crend() const noexcept { return rend(); }

    constexpr bool empty() const noexcept { return true; }
    constexpr size_type size() const noexcept { return 0; }
    static constexpr size_type max_size() noexcept { return 0; }
    static constexpr size_type capacity() noexcept { return 0; }
    static constexpr void reserve(size_type requested) { fail_if_nonempty(requested); }
    static constexpr void shrink_to_fit() noexcept {}
    constexpr void resize(size_type requested) { fail_if_nonempty(requested); }
    constexpr void resize(size_type requested, const T&) { fail_if_nonempty(requested); }

    constexpr reference operator[](size_type) noexcept { __builtin_trap(); }
    constexpr const_reference operator[](size_type) const noexcept { __builtin_trap(); }
    constexpr reference at(size_type) { detail::inplace_vector_access_failure(); }
    constexpr const_reference at(size_type) const { detail::inplace_vector_access_failure(); }
    constexpr reference front() noexcept { __builtin_trap(); }
    constexpr const_reference front() const noexcept { __builtin_trap(); }
    constexpr reference back() noexcept { __builtin_trap(); }
    constexpr const_reference back() const noexcept { __builtin_trap(); }
    constexpr pointer data() noexcept { return nullptr; }
    constexpr const_pointer data() const noexcept { return nullptr; }

    template<typename... Args>
    constexpr reference emplace_back(Args&&...) { detail::inplace_vector_capacity_failure(); }
    constexpr reference push_back(const T&) { detail::inplace_vector_capacity_failure(); }
    constexpr reference push_back(T&&) { detail::inplace_vector_capacity_failure(); }
    template<typename R>
        requires detail::container_compatible_range<R, T>
    constexpr void append_range(R&& range) {
        auto first = ranges::begin(range);
        auto last = ranges::end(range);
        if (first != last)
            detail::inplace_vector_capacity_failure();
    }
    constexpr void pop_back() noexcept {}

    template<typename... Args>
    constexpr optional<reference> try_emplace_back(Args&&...) noexcept {
        return nullopt;
    }
    constexpr optional<reference> try_push_back(const T&) noexcept { return nullopt; }
    constexpr optional<reference> try_push_back(T&&) noexcept { return nullopt; }
    template<typename... Args>
    constexpr reference unchecked_emplace_back(Args&&...) { __builtin_trap(); }
    constexpr reference unchecked_push_back(const T&) { __builtin_trap(); }
    constexpr reference unchecked_push_back(T&&) { __builtin_trap(); }

    template<typename... Args>
    constexpr iterator emplace(const_iterator, Args&&...) {
        detail::inplace_vector_capacity_failure();
    }
    constexpr iterator insert(const_iterator, const T&) {
        detail::inplace_vector_capacity_failure();
    }
    constexpr iterator insert(const_iterator, T&&) {
        detail::inplace_vector_capacity_failure();
    }
    constexpr iterator insert(const_iterator, size_type count, const T&) {
        fail_if_nonempty(count);
        return nullptr;
    }
    template<typename InputIt,
             typename = enable_if_t<!is_integral<InputIt>::value>>
    constexpr iterator insert(const_iterator, InputIt first, InputIt last) {
        if (first != last) detail::inplace_vector_capacity_failure();
        return nullptr;
    }
    template<typename R>
        requires detail::container_compatible_range<R, T>
    constexpr iterator insert_range(const_iterator, R&& range) {
        auto first = ranges::begin(range);
        auto last = ranges::end(range);
        if (first != last)
            detail::inplace_vector_capacity_failure();
        return nullptr;
    }
    constexpr iterator insert(const_iterator position, initializer_list<T> values) {
        if (values.size()) detail::inplace_vector_capacity_failure();
        return const_cast<iterator>(position);
    }
    constexpr iterator erase(const_iterator) { return nullptr; }
    constexpr iterator erase(const_iterator, const_iterator) { return nullptr; }
    constexpr void swap(inplace_vector&) noexcept {}
    constexpr void clear() noexcept {}

    friend constexpr bool operator==(inplace_vector, inplace_vector) noexcept {
        return true;
    }
    friend constexpr auto operator<=>(inplace_vector,
                                      inplace_vector) noexcept
        requires requires(const T& left, const T& right) {
            detail::synth_three_way(left, right);
        }
    {
        using result_order = common_comparison_category_t<
            detail::synth_three_way_result_t<T>, strong_ordering>;
        return static_cast<result_order>(strong_ordering::equal);
    }
    friend constexpr void swap(inplace_vector&, inplace_vector&) noexcept {}
};

template<typename T, size_t N, typename U = T>
constexpr size_t erase(inplace_vector<T, N>& container, const U& value) {
    auto first = container.begin();
    if (first == container.end()) return 0;
    auto last = std::remove(first, container.end(), value);
    const size_t removed = static_cast<size_t>(container.end() - last);
    container.erase(last, container.end());
    return removed;
}

template<typename T, size_t N, typename Predicate>
constexpr size_t erase_if(inplace_vector<T, N>& container, Predicate predicate) {
    auto first = container.begin();
    if (first == container.end()) return 0;
    auto last = std::remove_if(first, container.end(), std::move(predicate));
    const size_t removed = static_cast<size_t>(container.end() - last);
    container.erase(last, container.end());
    return removed;
}

} /* namespace std */

#endif /* C++23 and later */
#endif /* RINCXX_INPLACE_VECTOR_H */
