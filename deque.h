/*
 * RinOS C++ <deque> ✿
 * 両端キュー
 */

#ifndef RINCXX_DEQUE_H
#define RINCXX_DEQUE_H

#include "rincxx.h"
#include "type_traits.h"
#include "iterator.h"
#include "algorithm.h"
#include "initializer_list.h"
#include "memory.h"
#include "exception.h"
#if __cplusplus >= 202002L
#include "compare.h"
#endif
#if __cplusplus > 202002L
#include "ranges.h"
#endif

namespace std {

namespace detail {

[[noreturn]] inline void deque_at_contract_fail() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw out_of_range("deque::at position is out of range");
#else
    __builtin_trap();
#endif
}

[[noreturn]] inline void deque_iterator_contract_fail() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw out_of_range("deque iterator position is invalid");
#else
    __builtin_trap();
#endif
}

[[noreturn]] inline void deque_allocation_failure() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw bad_alloc();
#else
    rin_panic("[DEQUE] allocation failed");
#endif
}

[[noreturn]] inline void deque_length_failure() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw length_error("deque::resize exceeds max_size");
#else
    rin_panic("[DEQUE] resize exceeds max_size");
#endif
}

[[noreturn]] inline void deque_insert_length_failure() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw length_error("deque insertion exceeds max_size");
#else
    rin_panic("[DEQUE] insertion exceeds max_size");
#endif
}

/* deque storage may be owned by an allocator fancy pointer.  The map itself
 * is a rebound allocation of pointer objects, so neither map access nor
 * element construction may require pointer arithmetic/operator[] on either
 * pointer type. */
template<class T>
constexpr T* deque_pointer_address(T* pointer) noexcept {
    return pointer;
}

/* Fancy allocators are allowed to expose an address only through their
 * pointer_traits customization; requiring operator-> made the otherwise
 * valid address-only pointer fail in C++11--17.  Keep the raw-pointer
 * overload above, then prefer pointer_traits::to_address when the mode does
 * not provide std::to_address and fall back to the legacy operator-> path. */
#if __cplusplus >= 202002L
template<class Pointer>
constexpr auto deque_pointer_address(const Pointer& pointer) noexcept
    -> decltype(std::to_address(pointer)) {
    return std::to_address(pointer);
}
#else
template<class Pointer>
constexpr auto deque_pointer_address(const Pointer& pointer) noexcept
    -> decltype(pointer_traits<Pointer>::to_address(pointer)) {
    return pointer_traits<Pointer>::to_address(pointer);
}

template<class Pointer>
constexpr auto deque_pointer_address(const Pointer& pointer) noexcept
    -> decltype(deque_pointer_address(pointer.operator->())) {
    return deque_pointer_address(pointer.operator->());
}
#endif

/* Middle insert/erase rotates existing elements.  An otherwise perfectly
 * usable value type may provide a throwing ADL swap even though its move
 * construction and assignment are noexcept.  Calling that swap would make
 * a structural operation fail after the deque has already moved elements.
 * Prefer the standard three-move exchange whenever it is non-throwing; keep
 * ADL swap as the fallback for types whose move operations can throw. */
template<class T>
inline void deque_exchange_impl(T& left, T& right, true_type) noexcept {
    T temporary(std::move(left));
    left = std::move(right);
    right = std::move(temporary);
}

template<class T>
inline void deque_exchange_impl(T& left, T& right, false_type) {
    using std::swap;
    swap(left, right);
}

template<class T>
inline void deque_exchange(T& left, T& right) noexcept(
    is_nothrow_move_constructible<T>::value &&
    is_nothrow_move_assignable<T>::value) {
    deque_exchange_impl(
        left, right,
        integral_constant<bool,
            is_nothrow_move_constructible<T>::value &&
            is_nothrow_move_assignable<T>::value>());
}

} /* namespace detail */

/* ═══════════════════════════════════════════════════════════════
 * deque クラス - ブロックベース実装
 * ═══════════════════════════════════════════════════════════════*/

template<class T, class Allocator = allocator<T>>
class deque {
public:
    using value_type = T;
    using allocator_type = Allocator;
    using alloc_traits = allocator_traits<allocator_type>;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = typename alloc_traits::pointer;
    using const_pointer = typename alloc_traits::const_pointer;
    
private:
    /* ブロックサイズ (バイト単位で512、最低8要素) */
    static constexpr size_type block_size = 
        sizeof(T) < 512 ? 512 / sizeof(T) : 1;
    static constexpr size_type min_map_size = 8;
    
    using map_allocator_type =
        typename alloc_traits::template rebind_alloc<pointer>;
    using map_alloc_traits = allocator_traits<map_allocator_type>;
    using map_pointer = typename map_alloc_traits::pointer;

    [[no_unique_address]] allocator_type alloc_;
    map_pointer map_;   /* ブロックへのポインタ配列 */
    size_type map_size_;/* マップのサイズ */
    size_type start_;   /* 開始位置 (要素インデックス) */
    size_type size_;    /* 要素数 */
    size_type generation_ = 1; /* iterator snapshot generation */

    void ensure_additional_capacity(size_type additional) const {
        const size_type limit = max_size();
        if (size_ > limit || additional > limit - size_)
            detail::deque_insert_length_failure();
    }

    void ensure_replacement_capacity(size_type requested) const {
        if (requested > max_size())
            detail::deque_insert_length_failure();
    }

    void invalidate_iterators() noexcept {
        /* Keep zero reserved for a default-constructed iterator. */
        ++generation_;
        if (generation_ == 0) ++generation_;
    }

    static bool has_pointer(const pointer& value) {
        return value != pointer();
    }

    static bool has_map(const map_pointer& value) {
        return value != map_pointer();
    }

    static pointer* map_data(map_pointer& value) noexcept {
        return detail::deque_pointer_address(value);
    }

    static const pointer* map_data(const map_pointer& value) noexcept {
        return detail::deque_pointer_address(value);
    }

    static pointer& map_slot(map_pointer& value, size_type index) noexcept {
        return map_data(value)[index];
    }

    static const pointer& map_slot(const map_pointer& value,
                                   size_type index) noexcept {
        return map_data(value)[index];
    }

    static T* element_address(pointer& block, size_type offset) noexcept {
        return detail::deque_pointer_address(block) + offset;
    }

    static const T* element_address(const pointer& block,
                                    size_type offset) noexcept {
        return detail::deque_pointer_address(block) + offset;
    }

    map_pointer allocate_map(size_type count) {
        map_allocator_type map_alloc(alloc_);
        map_pointer result = map_alloc_traits::allocate(map_alloc, count);
        pointer* raw_result = detail::deque_pointer_address(result);
        if (!has_map(result) || !raw_result) {
            if (has_map(result)) {
                map_alloc_traits::deallocate(map_alloc, result, count);
            }
            detail::deque_allocation_failure();
        }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        size_type constructed = 0;
        try {
            for (; constructed < count; ++constructed) {
                map_alloc_traits::construct(
                    map_alloc, std::addressof(map_slot(result, constructed)),
                    pointer());
            }
        } catch (...) {
            for (size_type i = 0; i < constructed; ++i) {
                map_alloc_traits::destroy(
                    map_alloc, std::addressof(map_slot(result, i)));
            }
            map_alloc_traits::deallocate(map_alloc, result, count);
            throw;
        }
#else
        for (size_type i = 0; i < count; ++i) {
            map_alloc_traits::construct(
                map_alloc, std::addressof(map_slot(result, i)), pointer());
        }
#endif
        return result;
    }

    void deallocate_map(map_pointer value, size_type count) {
        if (!has_map(value)) return;
        map_allocator_type map_alloc(alloc_);
        for (size_type i = 0; i < count; ++i)
            map_alloc_traits::destroy(map_alloc,
                                      std::addressof(map_slot(value, i)));
        map_alloc_traits::deallocate(map_alloc, value, count);
    }

    void initialize_map() {
        map_size_ = min_map_size;
        map_ = allocate_map(map_size_);
        start_ = (map_size_ / 2) * block_size;
        size_ = 0;
    }
    
    /* ブロックとオフセットを計算 */
    size_type block_index(size_type pos) const {
        return (start_ + pos) / block_size;
    }
    
    size_type block_offset(size_type pos) const {
        return (start_ + pos) % block_size;
    }
    
    /* ブロックを確保 */
    pointer allocate_block() {
        pointer result = alloc_traits::allocate(alloc_, block_size);
        if (!has_pointer(result) || !detail::deque_pointer_address(result)) {
            if (has_pointer(result)) {
                alloc_traits::deallocate(alloc_, result, block_size);
            }
            detail::deque_allocation_failure();
        }
        return result;
    }
    
    void deallocate_block(pointer block) {
        alloc_traits::deallocate(alloc_, block, block_size);
    }

    void release_storage() {
        clear();
        if (has_map(map_)) {
            for (size_type i = 0; i < map_size_; ++i) {
                if (has_pointer(map_slot(map_, i))) {
                    deallocate_block(map_slot(map_, i));
                    map_slot(map_, i) = pointer();
                }
            }
            deallocate_map(map_, map_size_);
        }
        map_ = map_pointer();
        map_size_ = 0;
        start_ = 0;
        size_ = 0;
        invalidate_iterators();
    }

    void steal_storage(deque& other) {
        map_ = other.map_;
        map_size_ = other.map_size_;
        start_ = other.start_;
        size_ = other.size_;
        other.map_ = map_pointer();
        other.map_size_ = 0;
        other.start_ = 0;
        other.size_ = 0;
        other.invalidate_iterators();
    }

    void swap_storage(deque& other) noexcept {
        detail::deque_exchange(map_, other.map_);
        detail::deque_exchange(map_size_, other.map_size_);
        detail::deque_exchange(start_, other.start_);
        detail::deque_exchange(size_, other.size_);
    }
    
    /* マップを再配置 */
    void reallocate_map(size_type blocks_to_add, bool add_at_front) {
        const size_type old_start_block = start_ / block_size;
        const size_type start_offset = start_ % block_size;
        const size_type old_num_blocks = size_ == 0
            ? 0
            : (start_offset + size_ + block_size - 1) / block_size;
        size_type new_num_blocks = old_num_blocks + blocks_to_add;

        size_type growth = max(map_size_, blocks_to_add);
        size_type new_map_size = map_size_ + growth + 2;
        if (new_map_size < min_map_size) new_map_size = min_map_size;
        map_pointer new_map = allocate_map(new_map_size);
        const size_type new_start_block =
            (new_map_size - new_num_blocks) / 2
            + (add_at_front ? blocks_to_add : 0);

        for (size_type i = 0; i < old_num_blocks; ++i) {
            map_slot(new_map, new_start_block + i) =
                map_slot(map_, old_start_block + i);
            map_slot(map_, old_start_block + i) = pointer();
        }

        if (has_map(map_)) {
            for (size_type i = 0; i < map_size_; ++i) {
                if (has_pointer(map_slot(map_, i)))
                    deallocate_block(map_slot(map_, i));
            }
            deallocate_map(map_, map_size_);
        }
        map_ = new_map;
        map_size_ = new_map_size;
        start_ = new_start_block * block_size + start_offset;
    }
    
    /* 要素にアクセス */
    T& element_at(size_type pos) {
        size_type bi = block_index(pos);
        size_type bo = block_offset(pos);
        return *element_address(map_slot(map_, bi), bo);
    }
    
    const T& element_at(size_type pos) const {
        size_type bi = block_index(pos);
        size_type bo = block_offset(pos);
        return *element_address(map_slot(map_, bi), bo);
    }

    void compact_map() {
        const size_type old_start_block = start_ / block_size;
        const size_type start_offset = start_ % block_size;
        const size_type active_blocks = size_ == 0
            ? 0
            : (start_offset + size_ + block_size - 1) / block_size;
        size_type new_map_size = active_blocks + 2;
        if (new_map_size < min_map_size) new_map_size = min_map_size;
        if (new_map_size >= map_size_) return;

        map_pointer new_map = allocate_map(new_map_size);
        const size_type new_start_block =
            (new_map_size - active_blocks) / 2;
        for (size_type i = 0; i < active_blocks; ++i) {
            map_slot(new_map, new_start_block + i) =
                map_slot(map_, old_start_block + i);
            map_slot(map_, old_start_block + i) = pointer();
        }
        for (size_type i = 0; i < map_size_; ++i) {
            if (has_pointer(map_slot(map_, i)))
                deallocate_block(map_slot(map_, i));
        }
        deallocate_map(map_, map_size_);
        map_ = new_map;
        map_size_ = new_map_size;
        start_ = new_start_block * block_size + start_offset;
    }

    void reverse_elements(size_type first, size_type last) {
        while (first < last) {
            --last;
            if (first == last) break;
            detail::deque_exchange(element_at(first), element_at(last));
            ++first;
        }
    }

    void rotate_elements(size_type first, size_type middle, size_type last) {
        if (first == middle || middle == last) return;
        reverse_elements(first, middle);
        reverse_elements(middle, last);
        reverse_elements(first, last);
    }

    size_type integrate_staged_in_place(size_type index, deque& pending) {
        const size_type original_size = size_;
        const size_type inserted_size = pending.size_;
        if (inserted_size == 0) return index;

        if (index < original_size - index) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            size_type added = 0;
            try {
                while (!pending.empty()) {
                    push_front(std::move(pending.back()));
                    pending.pop_back();
                    ++added;
                }
            } catch (...) {
                while (added != 0) {
                    pop_front();
                    --added;
                }
                throw;
            }
#else
            while (!pending.empty()) {
                push_front(std::move(pending.back()));
                pending.pop_back();
            }
#endif
            rotate_elements(0, inserted_size, inserted_size + index);
        } else {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            size_type added = 0;
            try {
                while (!pending.empty()) {
                    push_back(std::move(pending.front()));
                    pending.pop_front();
                    ++added;
                }
            } catch (...) {
                while (added != 0) {
                    pop_back();
                    --added;
                }
                throw;
            }
#else
            while (!pending.empty()) {
                push_back(std::move(pending.front()));
                pending.pop_front();
            }
#endif
            rotate_elements(index, original_size,
                            original_size + inserted_size);
        }
        return index;
    }

    size_type integrate_staged(size_type index, deque& pending) {
        ensure_additional_capacity(pending.size_);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        return integrate_staged_transaction(
            index, pending,
            integral_constant<bool, is_copy_constructible<T>::value>());
#else
        return integrate_staged_in_place(index, pending);
#endif
    }

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    size_type integrate_staged_transaction(size_type index, deque& pending,
                                           true_type) {
        /* Middle rotation can throw from move assignment.  Build a complete
         * copy first so an exception never exposes a partially shifted
         * original deque. */
        deque candidate(*this, alloc_);
        candidate.integrate_staged_in_place(index, pending);
        swap(candidate);
        return index;
    }

    size_type integrate_staged_transaction(size_type index, deque& pending,
                                           false_type) {
        /* A throwing move-only value cannot in general offer the strong
         * guarantee: copying the original storage is not available. */
        return integrate_staged_in_place(index, pending);
    }
#endif

    void integrate_copies_in_place(size_type index, size_type count,
                                   const T& value) {
        if (count == 0) return;
        const size_type original_size = size_;

        if (original_size == 0) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            size_type added = 0;
            try {
                while (added != count) {
                    push_back(value);
                    ++added;
                }
            } catch (...) {
                while (added != 0) {
                    pop_back();
                    --added;
                }
                throw;
            }
#else
            for (size_type added = 0; added != count; ++added)
                push_back(value);
#endif
            return;
        }

        if (index < original_size - index) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            size_type added = 0;
            try {
                while (added != count) {
                    push_front(front());
                    ++added;
                }
            } catch (...) {
                while (added != 0) {
                    pop_front();
                    --added;
                }
                throw;
            }
#else
            for (size_type added = 0; added != count; ++added)
                push_front(front());
#endif
            for (size_type i = 0; i < index; ++i)
                (*this)[i] = (*this)[count + i];
        } else {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            size_type added = 0;
            try {
                while (added != count) {
                    push_back(back());
                    ++added;
                }
            } catch (...) {
                while (added != 0) {
                    pop_back();
                    --added;
                }
                throw;
            }
#else
            for (size_type added = 0; added != count; ++added)
                push_back(back());
#endif
            for (size_type i = original_size; i > index; --i)
                (*this)[i - 1 + count] = (*this)[i - 1];
        }

        for (size_type i = 0; i < count; ++i)
            (*this)[index + i] = value;
    }

    void integrate_copies(size_type index, size_type count, const T& value) {
        ensure_additional_capacity(count);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        /* Every valid const-value insertion is CopyInsertable.  Keep copy
         * assignment and rotation failures private to the candidate. */
        deque candidate(*this, alloc_);
        candidate.integrate_copies_in_place(index, count, value);
        swap(candidate);
#else
        integrate_copies_in_place(index, count, value);
#endif
    }

    template<class... Args>
    void emplace_middle_in_place(size_type index, Args&&... args) {
        deque pending(alloc_);
        pending.emplace_back(std::forward<Args>(args)...);
        if (index < size_ / 2) {
            push_front(std::move(front()));
            for (size_type i = 1; i < index + 1; ++i) {
                (*this)[i] = std::move((*this)[i + 1]);
            }
            (*this)[index] = std::move(pending.front());
        } else {
            push_back(std::move(back()));
            for (size_type i = size_ - 2; i > index; --i) {
                (*this)[i] = std::move((*this)[i - 1]);
            }
            (*this)[index] = std::move(pending.front());
        }
    }

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    template<class... Args>
    void emplace_middle_transaction(size_type index, true_type,
                                    Args&&... args) {
        deque candidate(*this, alloc_);
        candidate.emplace_middle_in_place(index, std::forward<Args>(args)...);
        swap(candidate);
    }

    template<class... Args>
    void emplace_middle_transaction(size_type index, false_type,
                                    Args&&... args) {
        /* A move-only element cannot be copied into a private candidate, but
         * the structural part of the operation is still rollback-safe.  The
         * in-place path extends the nearer end before shifting existing
         * elements; if a later move assignment throws, remove that extension
         * before rethrowing so size/start/map ownership remain valid.  Values
         * already moved from are intentionally left with the standard
         * move-only exception guarantee (their value is unspecified), while
         * no duplicate live element or leaked block is exposed. */
        const size_type original_size = size_;
        try {
            emplace_middle_in_place(index, std::forward<Args>(args)...);
        } catch (...) {
            if (size_ > original_size) {
                if (index < original_size / 2)
                    pop_front();
                else
                    pop_back();
            }
            throw;
        }
    }
#endif

    void erase_one_in_place(size_type index) {
        if (index < size_ / 2) {
            for (size_type i = index; i > 0; --i) {
                (*this)[i] = std::move((*this)[i - 1]);
            }
            pop_front();
        } else {
            for (size_type i = index; i < size_ - 1; ++i) {
                (*this)[i] = std::move((*this)[i + 1]);
            }
            pop_back();
        }
    }

    void erase_range_in_place(size_type index, size_type count) {
        while (count != 0) {
            erase_one_in_place(index);
            --count;
        }
    }

    template<class U>
    static void relocate_values(const deque& source, deque& destination,
                                U) {
        for (size_type i = 0; i < source.size_; ++i)
            destination.push_back(source[i]);
    }

    static void relocate_values(deque& source, deque& destination,
                                false_type) {
        for (size_type i = 0; i < source.size_; ++i)
            destination.push_back(std::move(source[i]));
    }

    void swap_unequal(deque& other) {
        /* A raw storage swap would make each deque destroy blocks allocated
         * by the other allocator.  Rebuild both sequences first, then publish
         * the completed allocator-owned storage with same-allocator swaps. */
        ensure_replacement_capacity(other.size_);
        other.ensure_replacement_capacity(size_);
        deque left(alloc_);
        deque right(other.alloc_);
        relocate_values(other, left,
                        integral_constant<bool, is_copy_constructible<T>::value>());
        relocate_values(*this, right,
                        integral_constant<bool, is_copy_constructible<T>::value>());
        swap_storage(left);
        other.swap_storage(right);
    }

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    void erase_range_transaction(size_type index, size_type count, true_type) {
        deque candidate(*this, alloc_);
        candidate.erase_range_in_place(index, count);
        swap(candidate);
    }

    void erase_range_transaction(size_type index, size_type count, false_type) {
        erase_range_in_place(index, count);
    }
#endif

    template<class... Args>
    T& append_element(Args&&... args) {
        if (size_ >= max_size())
            detail::deque_length_failure();
        size_type back_pos = start_ + size_;
        size_type back_block = back_pos / block_size;
        size_type back_offset = back_pos % block_size;

        if (back_block >= map_size_) {
            reallocate_map(1, false);
            back_pos = start_ + size_;
            back_block = back_pos / block_size;
            back_offset = back_pos % block_size;
        }

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        bool allocated_block = false;
#endif
        if (!has_pointer(map_slot(map_, back_block))) {
            map_slot(map_, back_block) = allocate_block();
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            allocated_block = true;
#endif
        }

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            alloc_traits::construct(
                alloc_, element_address(map_slot(map_, back_block),
                                        back_offset),
                std::forward<Args>(args)...);
        } catch (...) {
            if (allocated_block) {
                deallocate_block(map_slot(map_, back_block));
                map_slot(map_, back_block) = pointer();
            }
            throw;
        }
#else
        alloc_traits::construct(
            alloc_, element_address(map_slot(map_, back_block), back_offset),
            std::forward<Args>(args)...);
#endif
        ++size_;
        invalidate_iterators();
        return element_at(size_ - 1);
    }

    template<class... Args>
    T& prepend_element(Args&&... args) {
        if (size_ >= max_size())
            detail::deque_length_failure();
        if (start_ == 0) reallocate_map(1, true);

        const size_type new_start = start_ - 1;
        const size_type front_block = new_start / block_size;
        const size_type front_offset = new_start % block_size;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        bool allocated_block = false;
#endif
        if (!has_pointer(map_slot(map_, front_block))) {
            map_slot(map_, front_block) = allocate_block();
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            allocated_block = true;
#endif
        }

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            alloc_traits::construct(
                alloc_, element_address(map_slot(map_, front_block),
                                        front_offset),
                std::forward<Args>(args)...);
        } catch (...) {
            if (allocated_block) {
                deallocate_block(map_slot(map_, front_block));
                map_slot(map_, front_block) = pointer();
            }
            throw;
        }
#else
        alloc_traits::construct(
            alloc_, element_address(map_slot(map_, front_block), front_offset),
            std::forward<Args>(args)...);
#endif
        start_ = new_start;
        ++size_;
        invalidate_iterators();
        return element_at(0);
    }
    
public:
    /* ═══════════════════════════════════════════════════════════
     * イテレータ
     * ═══════════════════════════════════════════════════════════*/
    
    class iterator {
        friend class const_iterator;
        friend class deque;

        deque* deque_;
        size_type index_;
        size_type generation_;

        void require_live() const {
            if (!deque_ || generation_ != deque_->generation_)
                detail::deque_iterator_contract_fail();
        }

        size_type shifted(difference_type amount, bool subtract) const {
            require_live();
            if (index_ > deque_->size_)
                detail::deque_iterator_contract_fail();

            const bool negative = amount < 0;
            const bool add = subtract ? negative : !negative;
            const size_type magnitude = negative
                ? static_cast<size_type>(-(amount + 1)) + 1u
                : static_cast<size_type>(amount);
            if (add) {
                if (magnitude > deque_->size_ - index_)
                    detail::deque_iterator_contract_fail();
                return index_ + magnitude;
            }
            if (magnitude > index_)
                detail::deque_iterator_contract_fail();
            return index_ - magnitude;
        }

        size_type element_index(difference_type amount = 0) const {
            const size_type result = shifted(amount, false);
            if (result >= deque_->size_)
                detail::deque_iterator_contract_fail();
            return result;
        }

        void require_same_owner(const iterator& other) const {
            if (deque_ != other.deque_)
                detail::deque_iterator_contract_fail();
            require_live();
            other.require_live();
        }

    public:
        using iterator_category = random_access_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = typename deque::pointer;
        using reference = T&;
        
        iterator() : deque_(nullptr), index_(0), generation_(0) {}
        iterator(deque* d, size_type i)
            : deque_(d), index_(i), generation_(d ? d->generation_ : 0) {}
        iterator(deque* d, size_type i, size_type generation)
            : deque_(d), index_(i), generation_(generation) {}
        
        reference operator*() const { return deque_->element_at(element_index()); }
        pointer operator->() const {
            return pointer_traits<pointer>::pointer_to(
                deque_->element_at(element_index()));
        }
        
        reference operator[](difference_type n) const { 
            return deque_->element_at(element_index(n));
        }
        
        iterator& operator++() { index_ = shifted(1, false); return *this; }
        iterator operator++(int) { iterator t = *this; ++*this; return t; }
        iterator& operator--() { index_ = shifted(1, true); return *this; }
        iterator operator--(int) { iterator t = *this; --*this; return t; }
        
        iterator& operator+=(difference_type n) {
            index_ = shifted(n, false);
            return *this;
        }
        iterator& operator-=(difference_type n) {
            index_ = shifted(n, true);
            return *this;
        }
        
        iterator operator+(difference_type n) const { 
            return iterator(deque_, shifted(n, false));
        }
        iterator operator-(difference_type n) const { 
            return iterator(deque_, shifted(n, true));
        }
        
        difference_type operator-(const iterator& other) const {
            require_same_owner(other);
            if (index_ >= other.index_)
                return static_cast<difference_type>(index_ - other.index_);
            return -static_cast<difference_type>(other.index_ - index_);
        }

        bool operator==(const iterator& other) const {
            if (deque_) {
                require_live();
            }
            if (other.deque_) {
                other.require_live();
            }
            return deque_ == other.deque_ && index_ == other.index_;
        }
        bool operator!=(const iterator& other) const { return !(*this == other); }
        bool operator<(const iterator& other) const {
            require_same_owner(other);
            return index_ < other.index_;
        }
        bool operator<=(const iterator& other) const {
            require_same_owner(other);
            return index_ <= other.index_;
        }
        bool operator>(const iterator& other) const {
            require_same_owner(other);
            return index_ > other.index_;
        }
        bool operator>=(const iterator& other) const {
            require_same_owner(other);
            return index_ >= other.index_;
        }

        friend iterator operator+(difference_type amount,
                                  const iterator& value) {
            return value + amount;
        }
    };
    
    class const_iterator {
        friend class deque;
        friend class iterator;

        const deque* deque_;
        size_type index_;
        size_type generation_;

        void require_live() const {
            if (!deque_ || generation_ != deque_->generation_)
                detail::deque_iterator_contract_fail();
        }

        size_type shifted(difference_type amount, bool subtract) const {
            require_live();
            if (index_ > deque_->size_)
                detail::deque_iterator_contract_fail();

            const bool negative = amount < 0;
            const bool add = subtract ? negative : !negative;
            const size_type magnitude = negative
                ? static_cast<size_type>(-(amount + 1)) + 1u
                : static_cast<size_type>(amount);
            if (add) {
                if (magnitude > deque_->size_ - index_)
                    detail::deque_iterator_contract_fail();
                return index_ + magnitude;
            }
            if (magnitude > index_)
                detail::deque_iterator_contract_fail();
            return index_ - magnitude;
        }

        size_type element_index(difference_type amount = 0) const {
            const size_type result = shifted(amount, false);
            if (result >= deque_->size_)
                detail::deque_iterator_contract_fail();
            return result;
        }

        void require_same_owner(const const_iterator& other) const {
            if (deque_ != other.deque_)
                detail::deque_iterator_contract_fail();
            require_live();
            other.require_live();
        }

    public:
        using iterator_category = random_access_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = typename deque::const_pointer;
        using reference = const T&;
        
        const_iterator() : deque_(nullptr), index_(0), generation_(0) {}
        const_iterator(const deque* d, size_type i)
            : deque_(d), index_(i), generation_(d ? d->generation_ : 0) {}
        const_iterator(const deque* d, size_type i, size_type generation)
            : deque_(d), index_(i), generation_(generation) {}
        const_iterator(const iterator& it)
            : deque_(it.deque_), index_(it.index_), generation_(it.generation_) {}
        
        reference operator*() const { return deque_->element_at(element_index()); }
        pointer operator->() const {
            return pointer_traits<pointer>::pointer_to(
                deque_->element_at(element_index()));
        }
        
        reference operator[](difference_type n) const { 
            return deque_->element_at(element_index(n));
        }
        
        const_iterator& operator++() { index_ = shifted(1, false); return *this; }
        const_iterator operator++(int) { const_iterator t = *this; ++*this; return t; }
        const_iterator& operator--() { index_ = shifted(1, true); return *this; }
        const_iterator operator--(int) { const_iterator t = *this; --*this; return t; }
        
        const_iterator& operator+=(difference_type n) {
            index_ = shifted(n, false);
            return *this;
        }
        const_iterator& operator-=(difference_type n) {
            index_ = shifted(n, true);
            return *this;
        }
        
        const_iterator operator+(difference_type n) const { 
            return const_iterator(deque_, shifted(n, false));
        }
        const_iterator operator-(difference_type n) const { 
            return const_iterator(deque_, shifted(n, true));
        }
        
        difference_type operator-(const const_iterator& other) const {
            require_same_owner(other);
            if (index_ >= other.index_)
                return static_cast<difference_type>(index_ - other.index_);
            return -static_cast<difference_type>(other.index_ - index_);
        }

        difference_type operator-(const iterator& other) const {
            if (deque_ != other.deque_)
                detail::deque_iterator_contract_fail();
            require_live();
            other.require_live();
            if (index_ >= other.index_)
                return static_cast<difference_type>(index_ - other.index_);
            return -static_cast<difference_type>(other.index_ - index_);
        }
        
        bool operator==(const const_iterator& other) const {
            if (deque_) {
                require_live();
            }
            if (other.deque_) {
                other.require_live();
            }
            return deque_ == other.deque_ && index_ == other.index_;
        }
        bool operator==(const iterator& other) const {
            if (deque_ != other.deque_)
                return false;
            require_live();
            other.require_live();
            return index_ == other.index_;
        }
        bool operator!=(const const_iterator& other) const { return !(*this == other); }
        bool operator!=(const iterator& other) const { return !(*this == other); }
        bool operator<(const const_iterator& other) const {
            require_same_owner(other);
            return index_ < other.index_;
        }
        bool operator<(const iterator& other) const {
            if (deque_ != other.deque_)
                detail::deque_iterator_contract_fail();
            require_live();
            other.require_live();
            return index_ < other.index_;
        }
        bool operator<=(const const_iterator& other) const {
            require_same_owner(other);
            return index_ <= other.index_;
        }
        bool operator<=(const iterator& other) const {
            if (deque_ != other.deque_)
                detail::deque_iterator_contract_fail();
            require_live();
            other.require_live();
            return index_ <= other.index_;
        }
        bool operator>(const const_iterator& other) const {
            require_same_owner(other);
            return index_ > other.index_;
        }
        bool operator>(const iterator& other) const {
            if (deque_ != other.deque_)
                detail::deque_iterator_contract_fail();
            require_live();
            other.require_live();
            return index_ > other.index_;
        }
        bool operator>=(const const_iterator& other) const {
            require_same_owner(other);
            return index_ >= other.index_;
        }
        bool operator>=(const iterator& other) const {
            if (deque_ != other.deque_)
                detail::deque_iterator_contract_fail();
            require_live();
            other.require_live();
            return index_ >= other.index_;
        }

        friend difference_type operator-(const iterator& left,
                                         const const_iterator& right) {
            return -(right - left);
        }
        friend bool operator==(const iterator& left,
                               const const_iterator& right) {
            return right == left;
        }
        friend bool operator!=(const iterator& left,
                               const const_iterator& right) {
            return !(left == right);
        }
        friend bool operator<(const iterator& left,
                              const const_iterator& right) {
            return right > left;
        }
        friend bool operator<=(const iterator& left,
                               const const_iterator& right) {
            return right >= left;
        }
        friend bool operator>(const iterator& left,
                              const const_iterator& right) {
            return right < left;
        }
        friend bool operator>=(const iterator& left,
                               const const_iterator& right) {
            return right <= left;
        }
        friend const_iterator operator+(difference_type amount,
                                        const const_iterator& value) {
            return value + amount;
        }
    };
    
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    
    /* ═══════════════════════════════════════════════════════════
     * コンストラクタ・デストラクタ
     * ═══════════════════════════════════════════════════════════*/
    
    deque()
        : alloc_(), map_(), map_size_(0), start_(0), size_(0) {
        initialize_map();
    }

    explicit deque(const allocator_type& alloc)
        : alloc_(alloc), map_(), map_size_(0), start_(0), size_(0) {
        initialize_map();
    }

    explicit deque(size_type count) : deque() {
        ensure_additional_capacity(count);
        for (size_type i = 0; i < count; ++i)
            emplace_back();
    }

    deque(size_type count, const allocator_type& alloc) : deque(alloc) {
        ensure_additional_capacity(count);
        for (size_type i = 0; i < count; ++i)
            emplace_back();
    }

    deque(size_type count, const T& value) : deque() {
        ensure_additional_capacity(count);
        for (size_type i = 0; i < count; ++i)
            push_back(value);
    }

    deque(size_type count, const T& value, const allocator_type& alloc)
        : deque(alloc) {
        ensure_additional_capacity(count);
        for (size_type i = 0; i < count; ++i)
            push_back(value);
    }

    template<class InputIt,
             typename = typename enable_if<!is_integral<InputIt>::value>::type>
    deque(InputIt first, InputIt last) : deque() {
        for (; first != last; ++first)
            push_back(*first);
    }

    template<class InputIt,
             typename = typename enable_if<!is_integral<InputIt>::value>::type>
    deque(InputIt first, InputIt last, const allocator_type& alloc)
        : deque(alloc) {
        for (; first != last; ++first)
            push_back(*first);
    }

#if __cplusplus > 202002L
    template<detail::container_compatible_range<T> R>
    deque(from_range_t, R&& range,
          const allocator_type& alloc = allocator_type())
        : deque(alloc) {
        append_range(std::forward<R>(range));
    }
#endif

    deque(initializer_list<T> init) : deque() {
        ensure_additional_capacity(init.size());
        for (const auto& v : init)
            push_back(v);
    }

    deque(initializer_list<T> init, const allocator_type& alloc)
        : deque(alloc) {
        ensure_additional_capacity(init.size());
        for (const auto& v : init)
            push_back(v);
    }

    deque(const deque& other)
        : deque(alloc_traits::select_on_container_copy_construction(
              other.alloc_)) {
        ensure_additional_capacity(other.size_);
        for (size_type i = 0; i < other.size_; ++i)
            push_back(other[i]);
    }

    deque(const deque& other, const allocator_type& alloc) : deque(alloc) {
        ensure_additional_capacity(other.size_);
        for (size_type i = 0; i < other.size_; ++i)
            push_back(other[i]);
    }

    deque(deque&& other)
        noexcept(is_nothrow_move_constructible<allocator_type>::value)
        : alloc_(std::move(other.alloc_)), map_(), map_size_(0),
          start_(0), size_(0) {
        steal_storage(other);
    }

    deque(deque&& other, const allocator_type& alloc) : deque(alloc) {
        if (alloc_ == other.alloc_) {
            release_storage();
            steal_storage(other);
        } else {
            ensure_additional_capacity(other.size_);
            for (size_type i = 0; i < other.size_; ++i)
                push_back(std::move(other[i]));
        }
    }

    ~deque() {
        release_storage();
    }
    
    /* ═══════════════════════════════════════════════════════════
     * 代入
     * ═══════════════════════════════════════════════════════════*/
    
    deque& operator=(const deque& other) {
        if (this != &other) {
            constexpr bool propagate =
                alloc_traits::propagate_on_container_copy_assignment::value;
            if (propagate) {
                deque replacement(other, other.alloc_);
                using std::swap;
                swap(alloc_, replacement.alloc_);
                swap_storage(replacement);
            } else {
                deque replacement(other, alloc_);
                swap_storage(replacement);
            }
            invalidate_iterators();
        }
        return *this;
    }

    deque& operator=(deque&& other) noexcept(
        alloc_traits::propagate_on_container_move_assignment::value ||
        alloc_traits::is_always_equal::value) {
        if (this != &other) {
            constexpr bool propagate =
                alloc_traits::propagate_on_container_move_assignment::value;
            constexpr bool always_equal = alloc_traits::is_always_equal::value;
            if (propagate) {
                release_storage();
                alloc_ = std::move(other.alloc_);
                steal_storage(other);
            } else if (always_equal) {
                release_storage();
                steal_storage(other);
            } else if (alloc_ == other.alloc_) {
                release_storage();
                steal_storage(other);
            } else {
                deque replacement(std::move(other), alloc_);
                swap_storage(replacement);
            }
            invalidate_iterators();
            other.invalidate_iterators();
        }
        return *this;
    }
    
    deque& operator=(initializer_list<T> init) {
        assign(init);
        return *this;
    }
    
    void assign(size_type count, const T& value) {
        ensure_replacement_capacity(count);
        deque replacement(count, value, alloc_);
        swap(replacement);
    }
    
    template<class InputIt,
             typename = typename enable_if<!is_integral<InputIt>::value>::type>
    void assign(InputIt first, InputIt last) {
        deque replacement(first, last, alloc_);
        swap(replacement);
    }

#if __cplusplus > 202002L
    template<detail::container_compatible_range<T> R>
    void assign_range(R&& range) {
        static_assert(assignable_from<T&, ranges::range_reference_t<R>>,
                      "deque::assign_range requires assignable elements");
        if constexpr (ranges::sized_range<R>) {
            ensure_replacement_capacity(
                static_cast<size_type>(ranges::size(range)));
        }
        deque replacement(from_range, std::forward<R>(range), alloc_);
        swap(replacement);
    }
#endif
    
    void assign(initializer_list<T> init) {
        ensure_replacement_capacity(init.size());
        deque replacement(init, alloc_);
        swap(replacement);
    }
    
    /* ═══════════════════════════════════════════════════════════
     * 要素アクセス
     * ═══════════════════════════════════════════════════════════*/
    
    reference at(size_type pos) {
        if (pos >= size_) detail::deque_at_contract_fail();
        return element_at(pos);
    }
    
    const_reference at(size_type pos) const {
        if (pos >= size_) detail::deque_at_contract_fail();
        return element_at(pos);
    }
    
    reference operator[](size_type pos) noexcept { return element_at(pos); }
    const_reference operator[](size_type pos) const noexcept { return element_at(pos); }
    
    reference front() noexcept { return element_at(0); }
    const_reference front() const noexcept { return element_at(0); }
    
    reference back() noexcept { return element_at(size_ - 1); }
    const_reference back() const noexcept { return element_at(size_ - 1); }
    
    /* ═══════════════════════════════════════════════════════════
     * イテレータ
     * ═══════════════════════════════════════════════════════════*/
    
    iterator begin() noexcept { return iterator(this, 0); }
    const_iterator begin() const noexcept { return const_iterator(this, 0); }
    const_iterator cbegin() const noexcept { return const_iterator(this, 0); }
    
    iterator end() noexcept { return iterator(this, size_); }
    const_iterator end() const noexcept { return const_iterator(this, size_); }
    const_iterator cend() const noexcept { return const_iterator(this, size_); }
    
    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }
    
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const noexcept { return const_reverse_iterator(cbegin()); }
    
    /* ═══════════════════════════════════════════════════════════
     * 容量
     * ═══════════════════════════════════════════════════════════*/
    
    bool empty() const noexcept { return size_ == 0; }
    size_type size() const noexcept { return size_; }
    size_type max_size() const noexcept {
        return alloc_traits::max_size(alloc_);
    }

    allocator_type get_allocator() const noexcept { return alloc_; }
    
    void shrink_to_fit() {
        compact_map();
        invalidate_iterators();
    }
    
    /* ═══════════════════════════════════════════════════════════
     * 変更
     * ═══════════════════════════════════════════════════════════*/
    
    void clear() noexcept {
        while (size_ > 0)
            pop_back();
        invalidate_iterators();
    }
    
    void push_back(const T& value) {
        (void)append_element(value);
    }
    
    void push_back(T&& value) {
        (void)append_element(std::move(value));
    }

#if __cplusplus > 202002L
    template<detail::container_compatible_range<T> R>
    void append_range(R&& range) {
        if constexpr (ranges::sized_range<R>) {
            ensure_additional_capacity(
                static_cast<size_type>(ranges::size(range)));
        }
        auto first = ranges::begin(range);
        auto last = ranges::end(range);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        const size_type original_size = size_;
        try {
            for (; first != last; ++first)
                emplace_back(*first);
        } catch (...) {
            while (size_ > original_size) pop_back();
            throw;
        }
#else
        for (; first != last; ++first)
            emplace_back(*first);
#endif
    }
#endif
    
    template<class... Args>
    reference emplace_back(Args&&... args) {
        return append_element(std::forward<Args>(args)...);
    }
    
    void pop_back() noexcept {
        if (size_ == 0) return;
        
        --size_;
        size_type back_pos = start_ + size_;
        size_type back_block = back_pos / block_size;
        size_type back_offset = back_pos % block_size;
        
        alloc_traits::destroy(
            alloc_, element_address(map_slot(map_, back_block), back_offset));
        if (size_ == 0 ||
            (start_ + size_ - 1) / block_size != back_block) {
            deallocate_block(map_slot(map_, back_block));
            map_slot(map_, back_block) = pointer();
        }
        invalidate_iterators();
    }
    
    void push_front(const T& value) {
        (void)prepend_element(value);
    }
    
    void push_front(T&& value) {
        (void)prepend_element(std::move(value));
    }

#if __cplusplus > 202002L
    template<detail::container_compatible_range<T> R>
    void prepend_range(R&& range) {
        insert_range(cbegin(), std::forward<R>(range));
    }
#endif
    
    template<class... Args>
    reference emplace_front(Args&&... args) {
        return prepend_element(std::forward<Args>(args)...);
    }
    
    void pop_front() noexcept {
        if (size_ == 0) return;
        
        size_type front_block = start_ / block_size;
        size_type front_offset = start_ % block_size;
        
        alloc_traits::destroy(
            alloc_, element_address(map_slot(map_, front_block), front_offset));
        ++start_;
        --size_;
        if (size_ == 0 || start_ / block_size != front_block) {
            deallocate_block(map_slot(map_, front_block));
            map_slot(map_, front_block) = pointer();
        }
        invalidate_iterators();
    }
    
    template<class... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        if (pos.deque_ != this || pos.index_ > size_)
            detail::deque_iterator_contract_fail();
        pos.require_live();
        ensure_additional_capacity(1);
        const size_type index = pos.index_;
        if (index == 0) {
            emplace_front(std::forward<Args>(args)...);
            return begin();
        }
        if (index == size_) {
            emplace_back(std::forward<Args>(args)...);
            return iterator(this, size_ - 1);
        }

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        emplace_middle_transaction(
            index, integral_constant<bool, is_copy_constructible<T>::value>(),
            std::forward<Args>(args)...);
#else
        emplace_middle_in_place(index, std::forward<Args>(args)...);
#endif

        return iterator(this, index);
    }

    iterator insert(const_iterator pos, const T& value) {
        if (pos.deque_ != this || pos.index_ > size_)
            detail::deque_iterator_contract_fail();
        pos.require_live();
        const size_type index = pos.index_;
        if (index == 0) {
            push_front(value);
            return begin();
        }
        if (index == size_) {
            push_back(value);
            return iterator(this, size_ - 1);
        }

        deque pending(alloc_);
        pending.push_back(value);
        integrate_copies(index, 1, pending.front());
        return iterator(this, index);
    }

    iterator insert(const_iterator pos, T&& value) {
        if (pos.deque_ != this || pos.index_ > size_)
            detail::deque_iterator_contract_fail();
        pos.require_live();
        return emplace(pos, std::move(value));
    }

    iterator insert(const_iterator pos, size_type count, const T& value) {
        if (pos.deque_ != this || pos.index_ > size_)
            detail::deque_iterator_contract_fail();
        pos.require_live();
        const size_type index = pos.index_;
        if (count == 0) return iterator(this, index);
        ensure_additional_capacity(count);
        deque pending(alloc_);
        pending.push_back(value);
        if (!pending.empty()) integrate_copies(index, count, pending.front());
        return iterator(this, index);
    }

    template<class InputIt,
             typename = typename enable_if<!is_integral<InputIt>::value>::type>
    iterator insert(const_iterator pos, InputIt first, InputIt last) {
        if (pos.deque_ != this || pos.index_ > size_)
            detail::deque_iterator_contract_fail();
        pos.require_live();
        const size_type index = pos.index_;
        /* An empty input range is a true no-op.  Avoid constructing the
         * staging deque (which owns an allocator-backed map) so a valid
         * empty insertion cannot spuriously fail allocation. */
        if (first == last) return iterator(this, index);
        deque pending(alloc_);
        for (; first != last; ++first)
            pending.emplace_back(*first);
        integrate_staged(index, pending);
        return iterator(this, index);
    }

    iterator insert(const_iterator pos, initializer_list<T> init) {
        if (pos.deque_ != this || pos.index_ > size_)
            detail::deque_iterator_contract_fail();
        pos.require_live();
        return insert(pos, init.begin(), init.end());
    }

#if __cplusplus > 202002L
    template<detail::container_compatible_range<T> R>
    iterator insert_range(const_iterator pos, R&& range) {
        if (pos.deque_ != this || pos.index_ > size_)
            detail::deque_iterator_contract_fail();
        pos.require_live();
        const size_type index = pos.index_;
        deque pending(from_range, std::forward<R>(range), alloc_);
        integrate_staged(index, pending);
        return iterator(this, index);
    }
#endif
    
    iterator erase(const_iterator pos) {
        if (pos.deque_ != this || pos.index_ >= size_)
            detail::deque_iterator_contract_fail();
        pos.require_live();
        size_type index = pos.index_;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        erase_range_transaction(
            index, 1, integral_constant<bool, is_copy_constructible<T>::value>());
#else
        erase_one_in_place(index);
#endif
        
        return iterator(this, index);
    }
    
    iterator erase(const_iterator first, const_iterator last) {
        if (first.deque_ != this || last.deque_ != this ||
            first.index_ > last.index_ || last.index_ > size_)
            detail::deque_iterator_contract_fail();
        first.require_live();
        last.require_live();
        size_type count = last - first;
        size_type index = first.index_;
        /* Erasing an empty range must not allocate/copy or invalidate the
         * sequence.  The transactional copy path below would otherwise turn
         * this no-op into a throwing operation. */
        if (count == 0) return iterator(this, index);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        erase_range_transaction(
            index, count,
            integral_constant<bool, is_copy_constructible<T>::value>());
#else
        erase_range_in_place(index, count);
#endif
        
        return iterator(this, index);
    }
    
    void resize(size_type count) {
        if (count > max_size())
            detail::deque_length_failure();
        while (size_ > count)
            pop_back();
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        const size_type original_size = size_;
        try {
            while (size_ < count)
                emplace_back();
        } catch (...) {
            while (size_ > original_size) pop_back();
            throw;
        }
#else
        while (size_ < count)
            emplace_back();
#endif
    }
    
    void resize(size_type count, const T& value) {
        if (count > max_size())
            detail::deque_length_failure();
        while (size_ > count)
            pop_back();
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        const size_type original_size = size_;
        try {
            while (size_ < count)
                push_back(value);
        } catch (...) {
            while (size_ > original_size) pop_back();
            throw;
        }
#else
        while (size_ < count)
            push_back(value);
#endif
    }
    
    void swap(deque& other) noexcept(
        alloc_traits::propagate_on_container_swap::value ||
        alloc_traits::is_always_equal::value) {
        if (this == &other) return;
        if (alloc_traits::propagate_on_container_swap::value) {
            detail::deque_exchange(alloc_, other.alloc_);
            swap_storage(other);
        } else if (alloc_traits::is_always_equal::value ||
                   alloc_ == other.alloc_) {
            swap_storage(other);
        } else {
            swap_unequal(other);
        }
        invalidate_iterators();
        other.invalidate_iterators();
    }
};

#if __cplusplus >= 201703L
template<class InputIt,
         class Allocator =
             allocator<typename iterator_traits<InputIt>::value_type>,
         typename = enable_if_t<
             !is_integral<InputIt>::value &&
             _uses_allocator_detail::qualifies_as_allocator<Allocator>::value>>
deque(InputIt, InputIt, Allocator = Allocator())
    -> deque<typename iterator_traits<InputIt>::value_type, Allocator>;
#endif

#if __cplusplus > 202002L
template<ranges::input_range R,
         class Allocator = allocator<ranges::range_value_t<R>>>
requires _uses_allocator_detail::qualifies_as_allocator<Allocator>::value
deque(from_range_t, R&&, Allocator = Allocator())
    -> deque<ranges::range_value_t<R>, Allocator>;
#endif

/* ═══════════════════════════════════════════════════════════════
 * 比較演算子
 * ═══════════════════════════════════════════════════════════════*/

template<class T, class Allocator>
bool operator==(const deque<T, Allocator>& lhs,
                const deque<T, Allocator>& rhs) {
    if (lhs.size() != rhs.size()) return false;
    for (size_t i = 0; i < lhs.size(); ++i) {
        if (!(lhs[i] == rhs[i])) return false;
    }
    return true;
}

template<class T, class Allocator>
bool operator!=(const deque<T, Allocator>& lhs,
                const deque<T, Allocator>& rhs) {
    return !(lhs == rhs);
}

template<class T, class Allocator>
bool operator<(const deque<T, Allocator>& lhs,
               const deque<T, Allocator>& rhs) {
    return lexicographical_compare(lhs.begin(), lhs.end(), 
                                    rhs.begin(), rhs.end());
}

template<class T, class Allocator>
bool operator<=(const deque<T, Allocator>& lhs,
                const deque<T, Allocator>& rhs) {
    return !(rhs < lhs);
}

template<class T, class Allocator>
bool operator>(const deque<T, Allocator>& lhs,
               const deque<T, Allocator>& rhs) {
    return rhs < lhs;
}

template<class T, class Allocator>
bool operator>=(const deque<T, Allocator>& lhs,
                const deque<T, Allocator>& rhs) {
    return !(lhs < rhs);
}

#if __cplusplus >= 202002L
template<class T, class Allocator>
constexpr auto operator<=>(const deque<T, Allocator>& lhs,
                           const deque<T, Allocator>& rhs)
    -> detail::synth_three_way_result_t<T> {
    using result_type = detail::synth_three_way_result_t<T>;
    const typename deque<T, Allocator>::size_type common_size =
        lhs.size() < rhs.size() ? lhs.size() : rhs.size();
    for (typename deque<T, Allocator>::size_type i = 0;
         i < common_size; ++i) {
        const auto result = detail::synth_three_way(lhs[i], rhs[i]);
        if (result != 0) return result;
    }
    if (lhs.size() < rhs.size()) return result_type::less;
    if (rhs.size() < lhs.size()) return result_type::greater;
    return result_type::equivalent;
}
#endif

template<class T, class Allocator>
void swap(deque<T, Allocator>& lhs, deque<T, Allocator>& rhs)
    noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

/* ═══════════════════════════════════════════════════════════════
 * erase / erase_if (C++20)
 * ═══════════════════════════════════════════════════════════════*/

#if __cplusplus >= 202002L
template<typename T, typename Allocator, typename U>
typename deque<T, Allocator>::size_type
erase(deque<T, Allocator>& c, const U& value) {
    auto old_size = c.size();
    auto it = c.begin();
    while (it != c.end()) {
        if (*it == value) {
            it = c.erase(it);
        } else {
            ++it;
        }
    }
    return old_size - c.size();
}

template<typename T, typename Allocator, typename Pred>
typename deque<T, Allocator>::size_type
erase_if(deque<T, Allocator>& c, Pred pred) {
    auto old_size = c.size();
    auto it = c.begin();
    while (it != c.end()) {
        if (pred(*it)) {
            it = c.erase(it);
        } else {
            ++it;
        }
    }
    return old_size - c.size();
}
#endif

} /* namespace std */

#endif /* RINCXX_DEQUE_H */
