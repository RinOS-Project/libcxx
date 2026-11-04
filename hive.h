/*
 * RinOS C++26 <hive>
 * Block-based sequence container with stable element addresses.
 */

#ifndef RINCXX_HIVE_H
#define RINCXX_HIVE_H

#include "rincxx.h"

#if defined(__cplusplus) && __cplusplus > 202002L && \
    (__cplusplus > 202302L || defined(RIN_ENABLE_CXX26_HIVE))

#include "algorithm.h"
#include "compare.h"
#include "exception.h"
#include "functional.h"
#include "initializer_list.h"
#include "iterator.h"
#include "memory.h"
#include "ranges.h"
#include "type_traits.h"
#include "utility.h"

#ifndef __cpp_lib_hive
#define __cpp_lib_hive 202502L
#endif

namespace std {

struct hive_limits {
    size_t min;
    size_t max;
};

namespace hive_detail {

[[noreturn]] inline void length_failure() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw length_error("hive exceeds max_size or block limits");
#else
    rin_panic("[HIVE] length or block limit exceeded");
#endif
}

[[noreturn]] inline void allocation_failure() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw bad_alloc();
#else
    rin_panic("[HIVE] allocation failed");
#endif
}

template<class Pointer>
constexpr auto raw_address(const Pointer& pointer) noexcept
#if __cplusplus >= 202002L
    -> decltype(std::to_address(pointer))
#else
    -> decltype(pointer_traits<Pointer>::to_address(pointer))
#endif
{
#if __cplusplus >= 202002L
    return std::to_address(pointer);
#else
    return pointer_traits<Pointer>::to_address(pointer);
#endif
}

template<class T>
constexpr T* raw_address(T* pointer) noexcept { return pointer; }

#if __cplusplus < 202002L
template<class Pointer>
constexpr auto raw_address(const Pointer& pointer) noexcept
    -> decltype(raw_address(pointer.operator->())) {
    return raw_address(pointer.operator->());
}
#endif

} // namespace hive_detail

template<class T, class Allocator = allocator<T>>
class hive {
    static_assert(is_object<T>::value && !is_array<T>::value,
                  "hive requires a non-array object type");

    static constexpr size_t no_slot = static_cast<size_t>(-1);

    struct Slot {
        union {
            T value;
            size_t next_free;
        };
        bool occupied;
        Slot() noexcept : next_free(no_slot), occupied(false) {}
        ~Slot() {}
    };

    struct Block {
        Block* prev;
        Block* next;
        Block* free_prev;
        Block* free_next;
        Slot* slots;
        size_t capacity;
        size_t live;
        size_t free_head;
        hive* owner;
        bool on_free_list;

        Block(size_t count, Slot* storage, hive* parent) noexcept
            : prev(nullptr), next(nullptr), free_prev(nullptr), free_next(nullptr),
              slots(storage), capacity(count), live(0), free_head(0),
              owner(parent), on_free_list(false) {}
    };

    using block_allocator = typename allocator_traits<Allocator>::template
        rebind_alloc<Block>;
    using block_traits = allocator_traits<block_allocator>;
    using block_pointer = typename block_traits::pointer;
    using slot_allocator = typename allocator_traits<Allocator>::template
        rebind_alloc<Slot>;
    using slot_traits = allocator_traits<slot_allocator>;
    using slot_pointer = typename slot_traits::pointer;

public:
    using value_type = T;
    using allocator_type = Allocator;
    using pointer = typename allocator_traits<Allocator>::pointer;
    using const_pointer = typename allocator_traits<Allocator>::const_pointer;
    using reference = T&;
    using const_reference = const T&;
    using size_type = size_t;
    using difference_type = ptrdiff_t;

    template<bool Const>
    class basic_iterator;
    using iterator = basic_iterator<false>;
    using const_iterator = basic_iterator<true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

private:
    allocator_type alloc_;
    Block* head_;
    Block* tail_;
    Block* free_head_;
    size_type size_;
    size_type capacity_;
    size_type next_block_capacity_;
    hive_limits limits_;

    static constexpr hive_limits default_limits_value() noexcept {
        return {8u, 256u};
    }

    static constexpr hive_limits hard_limits_value() noexcept {
        return {1u, 65536u};
    }

    static constexpr size_type min_size(size_type left, size_type right) noexcept {
        return left < right ? left : right;
    }

    void validate_limits(hive_limits limits) const {
        if (!is_within_hard_limits(limits)) hive_detail::length_failure();
    }

    void ensure_additional(size_type count) const {
        const size_type limit = max_size();
        if (size_ > limit || count > limit - size_)
            hive_detail::length_failure();
    }

    T* value_pointer(Block* block, size_type index) noexcept {
        return __builtin_addressof(block->slots[index].value);
    }

    const T* value_pointer(const Block* block, size_type index) const noexcept {
        return __builtin_addressof(block->slots[index].value);
    }

    Block* make_block(size_type count) {
        if (count < limits_.min || count > limits_.max ||
            count > max_size() || count > static_cast<size_type>(-1) / sizeof(Slot))
            hive_detail::length_failure();

        block_allocator block_alloc(alloc_);
        block_pointer block_storage = block_traits::allocate(block_alloc, 1);
        Block* block = hive_detail::raw_address(block_storage);
        if (!block) {
            if (!(block_storage == block_pointer()))
                block_traits::deallocate(block_alloc, block_storage, 1);
            hive_detail::allocation_failure();
        }

        slot_allocator slots_alloc(alloc_);
        slot_pointer slots_storage{};
        Slot* slots = nullptr;
        size_type constructed = 0;
        bool slots_allocated = false;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            slots_storage = slot_traits::allocate(slots_alloc, count);
            slots_allocated = !(slots_storage == slot_pointer());
            slots = hive_detail::raw_address(slots_storage);
            if (!slots) hive_detail::allocation_failure();
            for (; constructed < count; ++constructed) {
                slot_traits::construct(slots_alloc, slots + constructed);
                slots[constructed].next_free =
                    constructed + 1u < count ? constructed + 1u : no_slot;
            }
            block_traits::construct(block_alloc, block, count, slots, this);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            while (constructed) {
                --constructed;
                slot_traits::destroy(slots_alloc, slots + constructed);
            }
            if (slots_allocated)
                slot_traits::deallocate(slots_alloc, slots_storage, count);
            block_traits::deallocate(block_alloc, block_storage, 1);
            throw;
        }
#endif
        return block;
    }

    void destroy_block(Block* block) noexcept {
        if (!block) return;
        slot_allocator slots_alloc(alloc_);
        slot_pointer slots_storage =
            pointer_traits<slot_pointer>::pointer_to(*block->slots);
        for (size_type index = 0; index < block->capacity; ++index) {
            if (block->slots[index].occupied) {
                allocator_traits<Allocator>::destroy(alloc_, value_pointer(block, index));
            }
        }
        for (size_type index = 0; index < block->capacity; ++index)
            slot_traits::destroy(slots_alloc, block->slots + index);
        slot_traits::deallocate(slots_alloc, slots_storage, block->capacity);

        block_allocator block_alloc(alloc_);
        block_pointer block_storage = pointer_traits<block_pointer>::pointer_to(*block);
        block_traits::destroy(block_alloc, block);
        block_traits::deallocate(block_alloc, block_storage, 1);
    }

    void append_all(Block* block) noexcept {
        block->prev = tail_;
        block->next = nullptr;
        if (tail_) tail_->next = block;
        else head_ = block;
        tail_ = block;
        block->owner = this;
        capacity_ += block->capacity;
    }

    void unlink_all(Block* block) noexcept {
        if (block->prev) block->prev->next = block->next;
        else head_ = block->next;
        if (block->next) block->next->prev = block->prev;
        else tail_ = block->prev;
        block->prev = block->next = nullptr;
        capacity_ -= block->capacity;
    }

    void add_free_block(Block* block) noexcept {
        if (block->on_free_list || block->free_head == no_slot) return;
        block->free_prev = nullptr;
        block->free_next = free_head_;
        if (free_head_) free_head_->free_prev = block;
        free_head_ = block;
        block->on_free_list = true;
    }

    void remove_free_block(Block* block) noexcept {
        if (!block->on_free_list) return;
        if (block->free_prev) block->free_prev->free_next = block->free_next;
        else free_head_ = block->free_next;
        if (block->free_next) block->free_next->free_prev = block->free_prev;
        block->free_prev = block->free_next = nullptr;
        block->on_free_list = false;
    }

    size_type choose_block_capacity(size_type requested = 0) const noexcept {
        size_type count = requested ? requested : next_block_capacity_;
        if (count < limits_.min) count = limits_.min;
        if (count > limits_.max) count = limits_.max;
        return count;
    }

    void update_growth(size_type used) noexcept {
        const size_type max = limits_.max;
        next_block_capacity_ = used > max / 2u ? max : used * 2u;
        if (next_block_capacity_ < limits_.min) next_block_capacity_ = limits_.min;
    }

    template<class... Args>
    iterator emplace_impl(Args&&... args) {
        ensure_additional(1);
        Block* block = free_head_;
        bool fresh = false;
        if (!block) {
            const size_type available = max_size() - capacity_;
            size_type count = choose_block_capacity();
            if (count > available) count = available;
            if (count < limits_.min) hive_detail::length_failure();
            block = make_block(count);
            fresh = true;
        }
        const size_type index = block->free_head;
        const size_type following = block->slots[index].next_free;
 #if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            allocator_traits<Allocator>::construct(
                alloc_, value_pointer(block, index), std::forward<Args>(args)...);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            if (fresh) destroy_block(block);
            throw;
        }
#endif
        block->slots[index].occupied = true;
        block->free_head = following;
        ++block->live;
        ++size_;
        if (fresh) {
            append_all(block);
            update_growth(block->capacity);
            add_free_block(block);
        } else if (block->free_head == no_slot) {
            remove_free_block(block);
        }
        return iterator(block, index, this);
    }

    Block* first_live(Block* block) const noexcept {
        while (block && !block->live) block = block->next;
        return block;
    }

    Block* last_live(Block* block) const noexcept {
        while (block && !block->live) block = block->prev;
        return block;
    }

    size_type first_live_slot(const Block* block) const noexcept {
        for (size_type i = 0; i < block->capacity; ++i)
            if (block->slots[i].occupied) return i;
        return no_slot;
    }

    size_type next_live_slot(const Block* block, size_type from) const noexcept {
        for (size_type i = from + 1u; i < block->capacity; ++i)
            if (block->slots[i].occupied) return i;
        return no_slot;
    }

    size_type previous_live_slot(const Block* block, size_type from) const noexcept {
        while (from) {
            --from;
            if (block->slots[from].occupied) return from;
        }
        return no_slot;
    }

    void clear_blocks() noexcept {
        Block* block = head_;
        while (block) {
            Block* next = block->next;
            destroy_block(block);
            block = next;
        }
        head_ = tail_ = free_head_ = nullptr;
        size_ = capacity_ = 0;
    }

    void steal_blocks(hive& source) noexcept {
        head_ = source.head_;
        tail_ = source.tail_;
        free_head_ = source.free_head_;
        size_ = source.size_;
        capacity_ = source.capacity_;
        next_block_capacity_ = source.next_block_capacity_;
        limits_ = source.limits_;
        for (Block* block = head_; block; block = block->next) block->owner = this;
        source.head_ = source.tail_ = source.free_head_ = nullptr;
        source.size_ = source.capacity_ = 0;
        source.next_block_capacity_ = source.limits_.min;
    }

    void replace_blocks(hive& replacement) noexcept {
        clear_blocks();
        steal_blocks(replacement);
    }

    template<class InputIt>
    void append_range(InputIt first, InputIt last) {
        for (; first != last; ++first) emplace(*first);
    }

    template<class Range>
    void append_range(Range&& range) {
        auto first = ranges::begin(range);
        auto last = ranges::end(range);
        for (; first != last; ++first) emplace(*first);
    }

public:
    template<bool Const>
    class basic_iterator {
        friend class hive;
        template<bool> friend class basic_iterator;
        Block* block_;
        size_type index_;
        hive* owner_;

        basic_iterator(Block* block, size_type index, hive* owner) noexcept
            : block_(block), index_(index), owner_(owner) {}

        void seek_forward() noexcept {
            if (!block_) return;
            size_type next = owner_->next_live_slot(block_, index_);
            while (next == no_slot) {
                block_ = owner_->first_live(block_->next);
                if (!block_) {
                    index_ = 0;
                    return;
                }
                next = owner_->first_live_slot(block_);
            }
            index_ = next;
        }

        void seek_backward() noexcept {
            if (!block_) {
                if (!owner_) return;
                block_ = owner_->last_live(owner_->tail_);
                if (!block_) return;
                index_ = block_->capacity;
            } else {
                owner_ = block_->owner;
            }
            size_type previous = owner_->previous_live_slot(block_, index_);
            while (previous == no_slot) {
                block_ = owner_->last_live(block_->prev);
                if (!block_) return;
                index_ = block_->capacity;
                previous = owner_->previous_live_slot(block_, index_);
            }
            index_ = previous;
            owner_ = block_->owner;
        }

        static difference_type distance_to(const basic_iterator& from,
                                           const basic_iterator& to) {
            if (from == to) return 0;
            basic_iterator cursor = from;
            difference_type distance = 0;
            while (cursor != to && cursor.block_) {
                ++cursor;
                ++distance;
            }
            if (cursor == to) return distance;
            cursor = from;
            distance = 0;
            while (cursor != to && cursor.block_) {
                --cursor;
                --distance;
            }
            return cursor == to ? distance : 0;
        }

    public:
        using iterator_category = bidirectional_iterator_tag;
        using iterator_concept = bidirectional_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = typename conditional<Const, const T*, T*>::type;
        using reference = typename conditional<Const, const T&, T&>::type;

        basic_iterator() noexcept : block_(nullptr), index_(0), owner_(nullptr) {}

        template<bool Other, typename = typename enable_if<Const && !Other>::type>
        basic_iterator(const basic_iterator<Other>& other) noexcept
            : block_(other.block_), index_(other.index_), owner_(other.owner_) {}

        reference operator*() const {
            return *reinterpret_cast<pointer>(
                __builtin_addressof(block_->slots[index_].value));
        }
        pointer operator->() const { return __builtin_addressof(operator*()); }

        basic_iterator& operator++() {
            if (block_) {
                owner_ = block_->owner;
                seek_forward();
            }
            return *this;
        }
        basic_iterator operator++(int) { basic_iterator copy = *this; ++*this; return copy; }
        basic_iterator& operator--() { seek_backward(); return *this; }
        basic_iterator operator--(int) { basic_iterator copy = *this; --*this; return copy; }

        template<bool Other>
        bool operator==(const basic_iterator<Other>& other) const noexcept {
            return block_ == other.block_ && (!block_ || index_ == other.index_);
        }
        template<bool Other>
        bool operator!=(const basic_iterator<Other>& other) const noexcept {
            return !(*this == other);
        }

        template<bool Other>
        difference_type operator-(const basic_iterator<Other>& other) const {
            basic_iterator<Const> converted(other.block_, other.index_, other.owner_);
            return distance_to(converted, *this);
        }
        basic_iterator& operator+=(difference_type amount) {
            while (amount > 0) { ++*this; --amount; }
            while (amount < 0) { --*this; ++amount; }
            return *this;
        }
        basic_iterator& operator-=(difference_type amount) { return *this += -amount; }
        basic_iterator operator+(difference_type amount) const {
            basic_iterator copy = *this; copy += amount; return copy;
        }
        basic_iterator operator-(difference_type amount) const {
            basic_iterator copy = *this; copy -= amount; return copy;
        }
        friend basic_iterator operator+(difference_type amount, basic_iterator value) {
            value += amount; return value;
        }

        template<bool Other>
        strong_ordering operator<=>(const basic_iterator<Other>& other) const {
            if (*this == other) return strong_ordering::equal;
            if (block_ == other.block_)
                return index_ < other.index_ ? strong_ordering::less
                                             : strong_ordering::greater;
            hive* left_owner = block_ ? block_->owner : owner_;
            hive* right_owner = other.block_ ? other.block_->owner : other.owner_;
            if (left_owner && left_owner == right_owner) {
                for (Block* block = left_owner->head_; block; block = block->next) {
                    if (block == block_) return strong_ordering::less;
                    if (block == other.block_) return strong_ordering::greater;
                }
                if (!block_ && other.block_) return strong_ordering::greater;
                if (block_ && !other.block_) return strong_ordering::less;
            }
            const uintptr_t left = reinterpret_cast<uintptr_t>(block_);
            const uintptr_t right = reinterpret_cast<uintptr_t>(other.block_);
            return left < right ? strong_ordering::less : strong_ordering::greater;
        }
    };

    hive() noexcept(noexcept(Allocator()))
        : hive(Allocator()) {}

    explicit hive(const Allocator& alloc) noexcept
        : alloc_(alloc), head_(nullptr), tail_(nullptr), free_head_(nullptr),
          size_(0), capacity_(0), next_block_capacity_(default_limits_value().min),
          limits_(default_limits_value()) {}

    explicit hive(hive_limits block_limits)
        : hive(block_limits, Allocator()) {}

    hive(hive_limits block_limits, const Allocator& alloc)
        : hive(alloc) {
        validate_limits(block_limits);
        limits_ = block_limits;
        next_block_capacity_ = block_limits.min;
    }

    explicit hive(size_type count, const Allocator& alloc = Allocator())
        : hive(alloc) { resize(count); }

    hive(size_type count, hive_limits block_limits,
         const Allocator& alloc = Allocator())
        : hive(block_limits, alloc) { resize(count); }

    hive(size_type count, const T& value, const Allocator& alloc = Allocator())
        : hive(alloc) { insert(count, value); }

    hive(size_type count, const T& value, hive_limits block_limits,
         const Allocator& alloc = Allocator())
        : hive(block_limits, alloc) { insert(count, value); }

    template<class InputIt, typename enable_if<!is_integral<InputIt>::value, int>::type = 0>
    hive(InputIt first, InputIt last, const Allocator& alloc = Allocator())
        : hive(alloc) { append_range(first, last); }

    template<class InputIt, typename enable_if<!is_integral<InputIt>::value, int>::type = 0>
    hive(InputIt first, InputIt last, hive_limits block_limits,
         const Allocator& alloc = Allocator())
        : hive(block_limits, alloc) { append_range(first, last); }

    template<class Range>
    hive(from_range_t, Range&& range, const Allocator& alloc = Allocator())
        : hive(alloc) { append_range(std::forward<Range>(range)); }

    template<class Range>
    hive(from_range_t, Range&& range, hive_limits block_limits,
         const Allocator& alloc = Allocator())
        : hive(block_limits, alloc) { append_range(std::forward<Range>(range)); }

    hive(const hive& other)
        : hive(allocator_traits<Allocator>::select_on_container_copy_construction(
                   other.alloc_)) {
        limits_ = other.limits_;
        next_block_capacity_ = other.next_block_capacity_;
        for (const T& value : other) emplace(value);
    }

    hive(const hive& other, const type_identity_t<Allocator>& alloc)
        : hive(alloc) {
        limits_ = other.limits_;
        next_block_capacity_ = other.next_block_capacity_;
        for (const T& value : other) emplace(value);
    }

    hive(hive&& other) noexcept
        : alloc_(std::move(other.alloc_)), head_(nullptr), tail_(nullptr),
          free_head_(nullptr), size_(0), capacity_(0),
          next_block_capacity_(default_limits_value().min),
          limits_(default_limits_value()) {
        steal_blocks(other);
    }

    hive(hive&& other, const type_identity_t<Allocator>& alloc)
        : hive(alloc) {
        if (alloc_ == other.alloc_) {
            steal_blocks(other);
        } else {
            limits_ = other.limits_;
            next_block_capacity_ = other.next_block_capacity_;
            for (T& value : other) emplace(std::move(value));
            other.clear();
        }
    }

    hive(initializer_list<T> values, const Allocator& alloc = Allocator())
        : hive(values.begin(), values.end(), alloc) {}

    hive(initializer_list<T> values, hive_limits block_limits,
         const Allocator& alloc = Allocator())
        : hive(values.begin(), values.end(), block_limits, alloc) {}

    ~hive() { clear_blocks(); }

    hive& operator=(const hive& other) {
        if (this == &other) return *this;
        if (allocator_traits<Allocator>::propagate_on_container_copy_assignment::value) {
            hive replacement(limits_, other.alloc_);
            for (const T& value : other) replacement.emplace(value);
            clear_blocks();
            alloc_ = other.alloc_;
            steal_blocks(replacement);
        } else {
            hive replacement(limits_, alloc_);
            for (const T& value : other) replacement.emplace(value);
            replace_blocks(replacement);
        }
        return *this;
    }

    hive& operator=(hive&& other) noexcept(
        allocator_traits<Allocator>::propagate_on_container_move_assignment::value ||
        allocator_traits<Allocator>::is_always_equal::value) {
        if (this == &other) return *this;
        if (allocator_traits<Allocator>::propagate_on_container_move_assignment::value) {
            clear_blocks();
            alloc_ = std::move(other.alloc_);
            steal_blocks(other);
        } else if (alloc_ == other.alloc_) {
            clear_blocks();
            steal_blocks(other);
        } else {
            hive replacement(limits_, alloc_);
            for (T& value : other) replacement.emplace(std::move(value));
            replace_blocks(replacement);
            other.clear();
        }
        return *this;
    }

    hive& operator=(initializer_list<T> values) {
        assign(values.begin(), values.end());
        return *this;
    }

    template<class InputIt, typename enable_if<!is_integral<InputIt>::value, int>::type = 0>
    void assign(InputIt first, InputIt last) {
        hive replacement(limits_, alloc_);
        replacement.append_range(first, last);
        replace_blocks(replacement);
        limits_ = replacement.limits_;
    }

    template<class Range>
    void assign_range(Range&& range) {
        hive replacement(limits_, alloc_);
        replacement.append_range(std::forward<Range>(range));
        replace_blocks(replacement);
        limits_ = replacement.limits_;
    }

    void assign(size_type count, const T& value) {
        hive replacement(limits_, alloc_);
        replacement.insert(count, value);
        replace_blocks(replacement);
        limits_ = replacement.limits_;
    }

    void assign(initializer_list<T> values) { assign(values.begin(), values.end()); }

    allocator_type get_allocator() const noexcept { return alloc_; }

    iterator begin() noexcept {
        Block* block = first_live(head_);
        return block ? iterator(block, first_live_slot(block), this) : end();
    }
    const_iterator begin() const noexcept {
        Block* block = first_live(head_);
        return block ? const_iterator(block, first_live_slot(block),
                                      const_cast<hive*>(this)) : end();
    }
    const_iterator cbegin() const noexcept { return begin(); }
    iterator end() noexcept { return iterator(nullptr, 0, this); }
    const_iterator end() const noexcept {
        return const_iterator(nullptr, 0, const_cast<hive*>(this));
    }
    const_iterator cend() const noexcept { return end(); }
    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const noexcept { return const_reverse_iterator(cbegin()); }

    bool empty() const noexcept { return size_ == 0; }
    size_type size() const noexcept { return size_; }
    size_type capacity() const noexcept { return capacity_; }
    size_type max_size() const noexcept {
        slot_allocator slots_alloc(alloc_);
        const size_type slot_limit = slot_traits::max_size(slots_alloc);
        const size_type value_limit = allocator_traits<Allocator>::max_size(alloc_);
        return min_size(slot_limit, value_limit);
    }

    void reserve(size_type requested) {
        if (requested <= capacity_) return;
        if (requested > max_size() || capacity_ > max_size())
            hive_detail::length_failure();
        size_type pending_capacity = 0;
        Block* pending_head = nullptr;
        Block* pending_tail = nullptr;
        size_type growth = next_block_capacity_;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            while (capacity_ + pending_capacity < requested) {
                size_type count = growth;
                if (count < limits_.min) count = limits_.min;
                if (count > limits_.max) count = limits_.max;
                const size_type remaining = requested - capacity_ - pending_capacity;
                if (count > remaining && remaining >= limits_.min) count = remaining;
                if (count < limits_.min) count = limits_.min;
                const size_type available = max_size() - capacity_ - pending_capacity;
                if (count > available) count = available;
                if (count < limits_.min) hive_detail::length_failure();
                Block* block = make_block(count);
                block->prev = pending_tail;
                if (pending_tail) pending_tail->next = block;
                else pending_head = block;
                pending_tail = block;
                pending_capacity += count;
                growth = count > limits_.max / 2u ? limits_.max : count * 2u;
            }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            Block* block = pending_head;
            while (block) {
                Block* next = block->next;
                destroy_block(block);
                block = next;
            }
            throw;
        }
#endif
        Block* block = pending_head;
        while (block) {
            Block* next = block->next;
            block->prev = block->next = nullptr;
            append_all(block);
            add_free_block(block);
            block = next;
        }
        if (pending_tail) next_block_capacity_ = growth;
    }

    void trim_capacity() noexcept { trim_capacity(size_); }

    void trim_capacity(size_type requested) noexcept {
        if (requested >= capacity_) return;
        Block* block = head_;
        while (block && capacity_ > requested) {
            Block* next = block->next;
            if (!block->live && capacity_ - block->capacity >= requested) {
                remove_free_block(block);
                unlink_all(block);
                destroy_block(block);
            }
            block = next;
        }
    }

    void shrink_to_fit() { trim_capacity(size_); }

    constexpr hive_limits block_capacity_limits() const noexcept { return limits_; }
    static constexpr hive_limits block_capacity_default_limits() noexcept {
        return default_limits_value();
    }
    static constexpr hive_limits block_capacity_hard_limits() noexcept {
        return hard_limits_value();
    }
    static constexpr bool is_within_hard_limits(hive_limits limits) noexcept {
        const hive_limits hard = hard_limits_value();
        return hard.min <= limits.min && limits.min <= limits.max &&
               limits.max <= hard.max;
    }

    void reshape(hive_limits block_limits) {
        validate_limits(block_limits);
        bool needs_reallocation = false;
        for (Block* block = head_; block; block = block->next) {
            if (block->capacity < block_limits.min ||
                block->capacity > block_limits.max) {
                if (block->live) needs_reallocation = true;
                else {
                    remove_free_block(block);
                    unlink_all(block);
                    destroy_block(block);
                }
            }
        }
        if (!needs_reallocation) {
            limits_ = block_limits;
            if (next_block_capacity_ < limits_.min) next_block_capacity_ = limits_.min;
            if (next_block_capacity_ > limits_.max) next_block_capacity_ = limits_.max;
            return;
        }

        hive replacement(block_limits, alloc_);
        replacement.reserve(size_);
        for (T& value : *this) {
            if constexpr (is_copy_constructible<T>::value)
                replacement.emplace(value);
            else
                replacement.emplace(std::move(value));
        }
        replace_blocks(replacement);
        limits_ = block_limits;
        next_block_capacity_ = replacement.next_block_capacity_;
    }

    template<class... Args>
    iterator emplace(Args&&... args) { return emplace_impl(std::forward<Args>(args)...); }
    template<class... Args>
    iterator emplace_hint(const_iterator, Args&&... args) {
        return emplace_impl(std::forward<Args>(args)...);
    }
    iterator insert(const T& value) { return emplace(value); }
    iterator insert(T&& value) { return emplace(std::move(value)); }
    iterator insert(const_iterator, const T& value) { return emplace(value); }
    iterator insert(const_iterator, T&& value) { return emplace(std::move(value)); }

    void insert(initializer_list<T> values) { insert_range(values); }

    template<class Range>
    void insert_range(Range&& range) { append_range(std::forward<Range>(range)); }

    template<class InputIt, typename enable_if<!is_integral<InputIt>::value, int>::type = 0>
    void insert(InputIt first, InputIt last) { append_range(first, last); }

    void insert(size_type count, const T& value) {
        ensure_additional(count);
        for (size_type i = 0; i < count; ++i) emplace(value);
    }

    void resize(size_type count) {
        if (count < size_) {
            iterator it = begin();
            for (size_type i = 0; i < count; ++i) ++it;
            erase(it, end());
        } else {
            ensure_additional(count - size_);
            while (size_ < count) emplace();
        }
    }

    void resize(size_type count, const T& value) {
        if (count < size_) {
            iterator it = begin();
            for (size_type i = 0; i < count; ++i) ++it;
            erase(it, end());
        } else {
            ensure_additional(count - size_);
            while (size_ < count) emplace(value);
        }
    }

    iterator erase(const_iterator position) {
        if (!position.block_) return end();
        Block* block = position.block_;
        const size_type index = position.index_;
        iterator following(block, index, this);
        ++following;
        const bool was_full = block->free_head == no_slot;
        allocator_traits<Allocator>::destroy(alloc_, value_pointer(block, index));
        block->slots[index].occupied = false;
        block->slots[index].next_free = block->free_head;
        block->free_head = index;
        --block->live;
        --size_;
        if (was_full) add_free_block(block);
        return following;
    }

    iterator erase(const_iterator first, const_iterator last) {
        iterator it(first.block_, first.index_, this);
        iterator stop(last.block_, last.index_, this);
        while (it != stop) it = erase(const_iterator(it));
        return it;
    }

    void clear() noexcept {
        for (Block* block = head_; block; block = block->next) {
            size_type free_head = no_slot;
            for (size_type i = block->capacity; i; --i) {
                const size_type index = i - 1u;
                if (block->slots[index].occupied) {
                    allocator_traits<Allocator>::destroy(alloc_, value_pointer(block, index));
                    block->slots[index].occupied = false;
                }
                block->slots[index].next_free = free_head;
                free_head = index;
            }
            block->live = 0;
            block->free_head = free_head;
            add_free_block(block);
        }
        size_ = 0;
    }

    void swap(hive& other) noexcept(
        allocator_traits<Allocator>::propagate_on_container_swap::value ||
        allocator_traits<Allocator>::is_always_equal::value) {
        if (this == &other) return;
        if (allocator_traits<Allocator>::propagate_on_container_swap::value)
            std::swap(alloc_, other.alloc_);
        std::swap(head_, other.head_);
        std::swap(tail_, other.tail_);
        std::swap(free_head_, other.free_head_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
        std::swap(next_block_capacity_, other.next_block_capacity_);
        std::swap(limits_, other.limits_);
        for (Block* block = head_; block; block = block->next) block->owner = this;
        for (Block* block = other.head_; block; block = block->next) block->owner = &other;
    }

    void splice(hive& other) {
        if (this == &other || other.empty()) return;
        if (!(alloc_ == other.alloc_)) hive_detail::length_failure();
        ensure_additional(other.size_);
        if (capacity_ > max_size()) hive_detail::length_failure();
        size_type transferred_capacity = 0;
        for (Block* block = other.head_; block; block = block->next)
            if (block->live) {
                if (block->capacity < limits_.min || block->capacity > limits_.max ||
                    block->capacity > max_size() - capacity_ - transferred_capacity)
                    hive_detail::length_failure();
                transferred_capacity += block->capacity;
            }

        Block* block = other.head_;
        while (block) {
            Block* next = block->next;
            if (block->live) {
                other.remove_free_block(block);
                other.unlink_all(block);
                append_all(block);
                add_free_block(block);
            }
            block = next;
        }
        size_ += other.size_;
        other.size_ = 0;
    }
    void splice(hive&& other) { splice(other); }

    template<class BinaryPredicate = equal_to<T>>
    size_type unique(BinaryPredicate predicate = BinaryPredicate()) {
        if (size_ < 2) return 0;
        size_type removed = 0;
        iterator previous = begin();
        iterator current = previous;
        ++current;
        while (current != end()) {
            if (predicate(*previous, *current)) {
                current = erase(current);
                ++removed;
            } else {
                previous = current++;
            }
        }
        return removed;
    }

    template<class Compare = less<T>>
    void sort(Compare compare = Compare()) {
        if (size_ < 2) return;
        struct sort_entry {
            T* value;
            size_type source;
        };
        using entry_allocator = typename allocator_traits<Allocator>::template
            rebind_alloc<sort_entry>;
        using entry_traits = allocator_traits<entry_allocator>;
        using entry_pointer = typename entry_traits::pointer;
        if (size_ > static_cast<size_type>(-1) / 2u)
            hive_detail::length_failure();
        const size_type allocation_count = size_ * 2u;
        entry_allocator entries_alloc(alloc_);
        entry_pointer storage = entry_traits::allocate(entries_alloc, allocation_count);
        sort_entry* entries = hive_detail::raw_address(storage);
        if (!entries) hive_detail::allocation_failure();
        size_type constructed = 0;
        auto release = [&]() noexcept {
            while (constructed) {
                --constructed;
                entry_traits::destroy(entries_alloc, entries + constructed);
            }
            entry_traits::deallocate(entries_alloc, storage, allocation_count);
        };
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            size_type index = 0;
            for (iterator it = begin(); it != end(); ++it, ++index) {
                const sort_entry entry{__builtin_addressof(*it), index};
                entry_traits::construct(entries_alloc, entries + index, entry);
                ++constructed;
            }
            for (index = 0; index < size_; ++index) {
                entry_traits::construct(entries_alloc, entries + size_ + index,
                                        entries[index]);
                ++constructed;
            }

            sort_entry* order = entries + size_;
            auto sift_down = [&](size_type root, size_type length) {
                for (;;) {
                    const size_type child = root * 2u + 1u;
                    if (child >= length) return;
                    size_type largest = child;
                    if (child + 1u < length &&
                        compare(*order[child].value, *order[child + 1u].value))
                        largest = child + 1u;
                    if (!compare(*order[root].value, *order[largest].value)) return;
                    const sort_entry temporary = order[root];
                    order[root] = order[largest];
                    order[largest] = temporary;
                    root = largest;
                }
            };
            for (size_type start = size_ / 2u; start; --start)
                sift_down(start - 1u, size_);
            for (size_type end_index = size_; end_index > 1u; --end_index) {
                const sort_entry temporary = order[0];
                order[0] = order[end_index - 1u];
                order[end_index - 1u] = temporary;
                sift_down(0, end_index - 1u);
            }

            /* Apply the sorted permutation to the existing slots.  The values
             * move between slots, so element addresses and slot iterators stay
             * valid while iteration observes the sorted sequence. */
            for (size_type start = 0; start < size_; ++start) {
                if (order[start].source == no_slot) continue;
                if (order[start].source == start) {
                    order[start].source = no_slot;
                    continue;
                }
                T temporary(std::move(*entries[start].value));
                size_type destination = start;
                for (;;) {
                    const size_type source = order[destination].source;
                    order[destination].source = no_slot;
                    if (source == start) {
                        *entries[destination].value = std::move(temporary);
                        break;
                    }
                    *entries[destination].value = std::move(*entries[source].value);
                    destination = source;
                }
            }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            release();
            throw;
        }
#endif
        release();
    }

    iterator get_iterator(const_pointer pointer_value) noexcept {
        const T* address = hive_detail::raw_address(pointer_value);
        for (Block* block = head_; block; block = block->next) {
            for (size_type i = 0; i < block->capacity; ++i)
                if (block->slots[i].occupied &&
                    value_pointer(block, i) == address)
                    return iterator(block, i, this);
        }
        return end();
    }

    const_iterator get_iterator(const_pointer pointer_value) const noexcept {
        const T* address = hive_detail::raw_address(pointer_value);
        for (Block* block = head_; block; block = block->next) {
            for (size_type i = 0; i < block->capacity; ++i)
                if (block->slots[i].occupied &&
                    value_pointer(block, i) == address)
                    return const_iterator(block, i, const_cast<hive*>(this));
        }
        return end();
    }
};

template<class InputIt, class Allocator = allocator<typename iterator_traits<InputIt>::value_type>>
hive(InputIt, InputIt, Allocator = Allocator())
    -> hive<typename iterator_traits<InputIt>::value_type, Allocator>;

template<class InputIt, class Allocator = allocator<typename iterator_traits<InputIt>::value_type>>
hive(InputIt, InputIt, hive_limits, Allocator = Allocator())
    -> hive<typename iterator_traits<InputIt>::value_type, Allocator>;

template<class Range,
         class Allocator = allocator<ranges::range_value_t<Range>>>
hive(from_range_t, Range&&, Allocator = Allocator())
    -> hive<ranges::range_value_t<Range>, Allocator>;

template<class Range,
         class Allocator = allocator<ranges::range_value_t<Range>>>
hive(from_range_t, Range&&, hive_limits, Allocator = Allocator())
    -> hive<ranges::range_value_t<Range>, Allocator>;

template<class T, class Allocator, class U>
typename hive<T, Allocator>::size_type erase(hive<T, Allocator>& container,
                                             const U& value) {
    const auto original_size = container.size();
    for (auto it = container.begin(); it != container.end();) {
        if (*it == value) it = container.erase(it);
        else ++it;
    }
    return original_size - container.size();
}

template<class T, class Allocator, class Predicate>
typename hive<T, Allocator>::size_type erase_if(hive<T, Allocator>& container,
                                                 Predicate predicate) {
    const auto original_size = container.size();
    for (auto it = container.begin(); it != container.end();) {
        if (predicate(*it)) it = container.erase(it);
        else ++it;
    }
    return original_size - container.size();
}

} // namespace std

#endif /* C++26 hive */
#endif /* RINCXX_HIVE_H */
