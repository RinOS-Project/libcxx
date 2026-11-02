/*
 * RinOS C++ <memory_resource>
 * Polymorphic allocators and allocation resources.
 */

#ifndef RINCXX_MEMORY_RESOURCE_H
#define RINCXX_MEMORY_RESOURCE_H

#if __cplusplus >= 201703L

/* string.h can be pulled in by exception.h below.  Forward-declare the
 * allocator first so its pmr string aliases can be declared during that
 * include cycle. */
namespace std { namespace pmr {
template<class T> class polymorphic_allocator;
} }

#include "rincxx.h"
#include "cstddef.h"
#include "cstdint.h"
#include "type_traits.h"
#include "memory.h"
#include "new.h"
#include "exception.h"
#include "atomic.h"
#include "mutex.h"
#include "tuple.h"
#if __cplusplus >= 202002L
#include "uses_allocator_construction.h"
#endif

namespace std {
namespace pmr {

namespace __detail {

class new_delete_resource_impl;
class null_memory_resource_impl;

[[noreturn]] inline void allocation_failure() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw bad_alloc();
#else
    terminate();
#endif
}

inline bool valid_alignment(size_t alignment) noexcept {
    return alignment != 0 && (alignment & (alignment - 1)) == 0;
}

inline size_t maximum_size() noexcept {
    return static_cast<size_t>(-1);
}

inline size_t checked_add(size_t left, size_t right) {
    if (right > maximum_size() - left) allocation_failure();
    return left + right;
}

inline size_t checked_multiply(size_t left, size_t right) {
    if (left != 0 && right > maximum_size() / left)
        allocation_failure();
    return left * right;
}

template<class T> struct is_pair : false_type {};
template<class First, class Second>
struct is_pair<pair<First, Second>> : true_type {};

} // namespace __detail

class memory_resource {
public:
    memory_resource() = default;
    memory_resource(const memory_resource&) = delete;
    memory_resource& operator=(const memory_resource&) = delete;
    virtual ~memory_resource() = default;

    void* allocate(size_t bytes, size_t alignment = alignof(max_align_t)) {
        if (!__detail::valid_alignment(alignment))
            __detail::allocation_failure();
        void* result = do_allocate(bytes, alignment);
        if (!result) __detail::allocation_failure();
        return result;
    }

    void deallocate(void* pointer, size_t bytes,
                    size_t alignment = alignof(max_align_t)) noexcept {
        if (!pointer) return;
        do_deallocate(pointer, bytes, alignment);
    }

    bool is_equal(const memory_resource& other) const noexcept {
        return this == &other || do_is_equal(other);
    }

protected:
    virtual void* do_allocate(size_t bytes, size_t alignment) = 0;
    virtual void do_deallocate(void* pointer, size_t bytes,
                               size_t alignment) noexcept = 0;
    virtual bool do_is_equal(const memory_resource& other) const noexcept = 0;
};

namespace __detail {

class new_delete_resource_impl final : public memory_resource {
protected:
    void* do_allocate(size_t bytes, size_t alignment) override {
        return ::operator new(bytes, align_val_t(alignment));
    }

    void do_deallocate(void* pointer, size_t, size_t alignment) noexcept override {
        ::operator delete(pointer, align_val_t(alignment));
    }

    bool do_is_equal(const memory_resource& other) const noexcept override {
        return this == &other;
    }
};

class null_memory_resource_impl final : public memory_resource {
protected:
    void* do_allocate(size_t, size_t) override {
        allocation_failure();
    }

    void do_deallocate(void*, size_t, size_t) noexcept override {}

    bool do_is_equal(const memory_resource& other) const noexcept override {
        return this == &other;
    }
};

} // namespace __detail

inline bool operator==(const memory_resource& left,
                       const memory_resource& right) noexcept {
    return left.is_equal(right);
}

inline bool operator!=(const memory_resource& left,
                       const memory_resource& right) noexcept {
    return !(left == right);
}

inline memory_resource* new_delete_resource() noexcept {
    static __detail::new_delete_resource_impl resource;
    return &resource;
}

inline memory_resource* null_memory_resource() noexcept {
    static __detail::null_memory_resource_impl resource;
    return &resource;
}

inline atomic<memory_resource*>& __default_resource_slot() noexcept {
    static atomic<memory_resource*> resource(nullptr);
    return resource;
}

inline memory_resource* get_default_resource() noexcept {
    memory_resource* resource =
        __default_resource_slot().load(memory_order_acquire);
    return resource ? resource : new_delete_resource();
}

inline memory_resource* set_default_resource(memory_resource* resource) noexcept {
    if (!resource) resource = new_delete_resource();
    memory_resource* previous = __default_resource_slot().exchange(
        resource, memory_order_acq_rel);
    return previous ? previous : new_delete_resource();
}

template<class T>
class polymorphic_allocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using void_pointer = void*;
    using const_void_pointer = const void*;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using propagate_on_container_copy_assignment = false_type;
    using propagate_on_container_move_assignment = false_type;
    using propagate_on_container_swap = false_type;
    using is_always_equal = false_type;

    template<class U>
    struct rebind { using other = polymorphic_allocator<U>; };

    polymorphic_allocator() noexcept : resource_(get_default_resource()) {}

    polymorphic_allocator(const polymorphic_allocator&) noexcept = default;

    template<class U>
    polymorphic_allocator(const polymorphic_allocator<U>& other) noexcept
        : resource_(other.resource()) {}

#if __cplusplus >= 202002L
    polymorphic_allocator(const polymorphic_allocator&,
                          memory_resource* resource) noexcept
        : resource_(resource ? resource : get_default_resource()) {}
#endif

    polymorphic_allocator& operator=(const polymorphic_allocator&) = delete;

    polymorphic_allocator(memory_resource* resource) noexcept
        : resource_(resource ? resource : get_default_resource()) {}

    [[nodiscard]] T* allocate(size_t count) {
        if (count > max_size()) __detail::allocation_failure();
        return static_cast<T*>(resource_->allocate(count * sizeof(T), alignof(T)));
    }

    void deallocate(T* pointer, size_t count) noexcept {
        resource_->deallocate(pointer, count * sizeof(T), alignof(T));
    }

    size_t max_size() const noexcept {
        return __detail::maximum_size() / sizeof(T);
    }

    memory_resource* resource() const noexcept { return resource_; }

    polymorphic_allocator select_on_container_copy_construction() const noexcept {
        return polymorphic_allocator();
    }

#if __cplusplus >= 202002L
    void* allocate_bytes(size_t bytes,
                         size_t alignment = alignof(max_align_t)) {
        return resource_->allocate(bytes, alignment);
    }

    void deallocate_bytes(void* pointer, size_t bytes,
                          size_t alignment = alignof(max_align_t)) noexcept {
        resource_->deallocate(pointer, bytes, alignment);
    }

    template<class U>
    U* allocate_object(size_t count = 1) {
        if (count > __detail::maximum_size() / sizeof(U))
            __detail::allocation_failure();
        return static_cast<U*>(resource_->allocate(count * sizeof(U), alignof(U)));
    }

    template<class U>
    void deallocate_object(U* pointer, size_t count = 1) noexcept {
        resource_->deallocate(pointer, count * sizeof(U), alignof(U));
    }

    template<class U, class... Args>
    U* new_object(Args&&... args) {
        U* result = allocate_object<U>();
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            construct(result, std::forward<Args>(args)...);
        } catch (...) {
            deallocate_object(result);
            throw;
        }
#else
        construct(result, std::forward<Args>(args)...);
#endif
        return result;
    }

    template<class U>
    void delete_object(U* pointer) {
        if (!pointer) return;
        destroy(pointer);
        deallocate_object(pointer);
    }
#endif

    template<class U, class... Args>
    void construct(U* pointer, Args&&... args) {
#if __cplusplus >= 202002L
        auto construction_args = uses_allocator_construction_args<U>(
            *this, std::forward<Args>(args)...);
        std::apply([pointer](auto&&... values) {
            ::new (static_cast<void*>(pointer)) U(
                std::forward<decltype(values)>(values)...);
        }, std::move(construction_args));
#else
        if constexpr (__detail::is_pair<remove_cv_t<U>>::value) {
            construct_pair(pointer, std::forward<Args>(args)...);
        } else {
            construct_regular(pointer, std::forward<Args>(args)...);
        }
#endif
    }

    template<class U>
    void destroy(U* pointer) {
        pointer->~U();
    }

private:
    template<class U, class... Args>
    auto allocator_arguments(Args&&... args) const {
        using allocator_type = polymorphic_allocator<T>;
        if constexpr (!uses_allocator<remove_cv_t<U>, allocator_type>::value) {
            return forward_as_tuple(std::forward<Args>(args)...);
        } else if constexpr (is_constructible<
                                 U, allocator_arg_t, const allocator_type&,
                                 Args&&...>::value) {
            return tuple<allocator_arg_t, const allocator_type&, Args&&...>(
                allocator_arg, *this, std::forward<Args>(args)...);
        } else if constexpr (is_constructible<
                                 U, Args&&..., const allocator_type&>::value) {
            return tuple_cat(forward_as_tuple(std::forward<Args>(args)...),
                             forward_as_tuple(*this));
        } else {
            static_assert(!uses_allocator<remove_cv_t<U>, allocator_type>::value,
                          "allocator-aware type has no compatible constructor");
            return tuple<>();
        }
    }

    template<class U, class... Args>
    void construct_regular(U* pointer, Args&&... args) const {
        auto values = allocator_arguments<U>(std::forward<Args>(args)...);
        std::apply([pointer](auto&&... values_to_construct) {
            ::new (static_cast<void*>(pointer)) U(
                std::forward<decltype(values_to_construct)>(values_to_construct)...);
        }, std::move(values));
    }

    template<class Pair, class Tuple1, class Tuple2>
    void construct_pair_piecewise(Pair* pointer, Tuple1&& first_args,
                                  Tuple2&& second_args) const {
        auto first = std::apply([this](auto&&... values) {
            return allocator_arguments<typename Pair::first_type>(
                std::forward<decltype(values)>(values)...);
        }, std::forward<Tuple1>(first_args));
        auto second = std::apply([this](auto&&... values) {
            return allocator_arguments<typename Pair::second_type>(
                std::forward<decltype(values)>(values)...);
        }, std::forward<Tuple2>(second_args));
        ::new (static_cast<void*>(pointer)) Pair(
            piecewise_construct, std::move(first), std::move(second));
    }

    template<class Pair>
    void construct_pair(Pair* pointer) const {
        construct_pair_piecewise(pointer, tuple<>(), tuple<>());
    }

    template<class Pair, class Tuple1, class Tuple2>
    void construct_pair(Pair* pointer, piecewise_construct_t,
                        Tuple1&& first_args, Tuple2&& second_args) const {
        construct_pair_piecewise(pointer, std::forward<Tuple1>(first_args),
                                 std::forward<Tuple2>(second_args));
    }

    template<class Pair, class First, class Second>
    void construct_pair(Pair* pointer, First&& first, Second&& second) const {
        construct_pair_piecewise(
            pointer,
            forward_as_tuple(std::forward<First>(first)),
            forward_as_tuple(std::forward<Second>(second)));
    }

    template<class Pair, class Source>
    void construct_pair(Pair* pointer, Source&& source) const {
        using source_type = remove_cv_t<remove_reference_t<Source>>;
        static_assert(__detail::is_pair<source_type>::value,
                      "pair construction requires a pair source");
        construct_pair_piecewise(
            pointer,
            forward_as_tuple(std::forward<Source>(source).first),
            forward_as_tuple(std::forward<Source>(source).second));
    }

    memory_resource* resource_;
};

template<class T1, class T2>
inline bool operator==(const polymorphic_allocator<T1>& left,
                       const polymorphic_allocator<T2>& right) noexcept {
    return *left.resource() == *right.resource();
}

template<class T1, class T2>
inline bool operator!=(const polymorphic_allocator<T1>& left,
                       const polymorphic_allocator<T2>& right) noexcept {
    return !(left == right);
}

struct pool_options {
    size_t max_blocks_per_chunk = 0;
    size_t largest_required_pool_block = 0;
};

namespace __detail {

class monotonic_resource_core {
    struct block {
        block* previous;
        size_t bytes;
        size_t alignment;
    };

public:
    explicit monotonic_resource_core(size_t initial_size,
                                     memory_resource* upstream) noexcept
        : initial_buffer_(nullptr), initial_size_(0), current_(nullptr),
          remaining_(0), blocks_(nullptr), upstream_(upstream),
          next_size_(initial_size ? initial_size : 1),
          initial_next_size_(initial_size ? initial_size : 1) {}

    monotonic_resource_core(size_t initial_size, void* buffer,
                            size_t buffer_size, memory_resource* upstream) noexcept
        : initial_buffer_(buffer), initial_size_(buffer ? buffer_size : 0),
          current_(static_cast<unsigned char*>(buffer)),
          remaining_(buffer ? buffer_size : 0), blocks_(nullptr),
          upstream_(upstream), next_size_(initial_size ? initial_size : 1),
          initial_next_size_(initial_size ? initial_size : 1) {}

    ~monotonic_resource_core() { release(); }

    void* allocate(size_t bytes, size_t alignment) {
        if (bytes == 0) bytes = 1;
        if (void* result = try_current(bytes, alignment)) return result;

        const size_t capacity = bytes > next_size_ ? bytes : next_size_;
        size_t total = __detail::checked_add(sizeof(block), alignment - 1);
        total = __detail::checked_add(total, capacity);
        const size_t block_alignment = alignof(block);
        void* raw = upstream_->allocate(total, block_alignment);
        block* allocation = ::new (raw) block{blocks_, total, block_alignment};
        blocks_ = allocation;
        current_ = static_cast<unsigned char*>(raw) + sizeof(block);
        remaining_ = total - sizeof(block);

        size_t doubled = capacity > __detail::maximum_size() / 2
            ? __detail::maximum_size() : capacity * 2;
        next_size_ = doubled > capacity ? doubled : capacity;
        if (void* result = try_current(bytes, alignment)) return result;

        __detail::allocation_failure();
    }

    void release() noexcept {
        block* current_block = blocks_;
        blocks_ = nullptr;
        while (current_block) {
            block* previous = current_block->previous;
            size_t bytes = current_block->bytes;
            size_t alignment = current_block->alignment;
            upstream_->deallocate(current_block, bytes, alignment);
            current_block = previous;
        }
        current_ = static_cast<unsigned char*>(initial_buffer_);
        remaining_ = initial_size_;
        next_size_ = initial_next_size_;
    }

private:
    void* try_current(size_t bytes, size_t alignment) noexcept {
        if (!current_ || bytes > remaining_) return nullptr;
        void* candidate = current_;
        size_t available = remaining_;
        void* result = std::align(alignment, bytes, candidate, available);
        if (!result) return nullptr;
        const size_t consumed = static_cast<size_t>(
            static_cast<unsigned char*>(result) - current_) + bytes;
        current_ = static_cast<unsigned char*>(result) + bytes;
        remaining_ -= consumed;
        return result;
    }

    void* initial_buffer_;
    size_t initial_size_;
    unsigned char* current_;
    size_t remaining_;
    block* blocks_;
    memory_resource* upstream_;
    size_t next_size_;
    size_t initial_next_size_;
};

class pool_resource_core {
    struct free_node { free_node* next; };
    struct pool_chunk {
        pool_chunk* previous;
        size_t bytes;
        size_t alignment;
    };
    struct pool_bin { free_node* free; };

    static constexpr size_t bin_count = 21;
    static constexpr size_t default_chunk_blocks = 32;

public:
    pool_resource_core(const pool_options& options, memory_resource* upstream)
        : upstream_(upstream), chunks_(nullptr), bins_{},
          blocks_per_chunk_(options.max_blocks_per_chunk
                                ? options.max_blocks_per_chunk
                                : default_chunk_blocks),
          largest_block_(options.largest_required_pool_block
                             ? options.largest_required_pool_block
                             : static_cast<size_t>(4096)) {
        const size_t supported_limit = block_size(bin_count - 1);
        if (largest_block_ > supported_limit) largest_block_ = supported_limit;
        for (size_t i = 0; i < bin_count; ++i) bins_[i].free = nullptr;
        effective_options_.max_blocks_per_chunk = blocks_per_chunk_;
        effective_options_.largest_required_pool_block = largest_block_;
    }

    ~pool_resource_core() { release(); }

    void* allocate(size_t bytes, size_t alignment) {
        if (bytes == 0) bytes = 1;
        size_t index = 0;
        if (!find_bin(bytes, alignment, index))
            return upstream_->allocate(bytes, alignment);
        if (!bins_[index].free) replenish(index);
        free_node* result = bins_[index].free;
        bins_[index].free = result->next;
        return result;
    }

    void deallocate(void* pointer, size_t bytes, size_t alignment) noexcept {
        if (!pointer) return;
        if (bytes == 0) bytes = 1;
        size_t index = 0;
        if (!find_bin(bytes, alignment, index)) {
            upstream_->deallocate(pointer, bytes, alignment);
            return;
        }
        free_node* result = ::new (pointer) free_node;
        result->next = bins_[index].free;
        bins_[index].free = result;
    }

    void release() noexcept {
        for (size_t i = 0; i < bin_count; ++i) bins_[i].free = nullptr;
        pool_chunk* current = chunks_;
        chunks_ = nullptr;
        while (current) {
            pool_chunk* previous = current->previous;
            size_t bytes = current->bytes;
            size_t alignment = current->alignment;
            upstream_->deallocate(current, bytes, alignment);
            current = previous;
        }
    }

    pool_options options() const noexcept { return effective_options_; }
    memory_resource* upstream_resource() const noexcept { return upstream_; }

private:
    static size_t block_size(size_t index) noexcept {
        const size_t unit = alignof(max_align_t);
        if (index < 16) return unit * (index + 1);
        return unit * (static_cast<size_t>(1) << (index - 11));
    }

    bool find_bin(size_t bytes, size_t alignment, size_t& result) const noexcept {
        if (alignment > alignof(max_align_t)) return false;
        const size_t required = bytes > sizeof(free_node) ? bytes : sizeof(free_node);
        if (required > largest_block_) return false;
        for (size_t index = 0; index < bin_count; ++index) {
            if (block_size(index) >= required) {
                result = index;
                return true;
            }
        }
        return false;
    }

    void replenish(size_t index) {
        const size_t stride = block_size(index);
        if (blocks_per_chunk_ >
            (__detail::maximum_size() - sizeof(pool_chunk) -
             alignof(max_align_t)) / stride) {
            __detail::allocation_failure();
        }
        const size_t bytes = sizeof(pool_chunk) + alignof(max_align_t) - 1 +
                             stride * blocks_per_chunk_;
        const size_t alignment = alignof(pool_chunk) > alignof(max_align_t)
            ? alignof(pool_chunk) : alignof(max_align_t);
        void* raw = upstream_->allocate(bytes, alignment);
        pool_chunk* chunk = ::new (raw) pool_chunk{chunks_, bytes, alignment};

        void* first = static_cast<unsigned char*>(raw) + sizeof(pool_chunk);
        size_t available = bytes - sizeof(pool_chunk);
        first = std::align(alignof(max_align_t), stride * blocks_per_chunk_,
                           first, available);
        if (!first) {
            upstream_->deallocate(raw, bytes, alignment);
            __detail::allocation_failure();
        }

        unsigned char* current = static_cast<unsigned char*>(first);
        for (size_t block = 0; block < blocks_per_chunk_; ++block) {
            free_node* node = ::new (static_cast<void*>(current)) free_node;
            node->next = bins_[index].free;
            bins_[index].free = node;
            current += stride;
        }
        chunks_ = chunk;
    }

    memory_resource* upstream_;
    pool_chunk* chunks_;
    pool_bin bins_[bin_count];
    size_t blocks_per_chunk_;
    size_t largest_block_;
    pool_options effective_options_;
};

} // namespace __detail

class monotonic_buffer_resource : public memory_resource {
public:
    explicit monotonic_buffer_resource(
        memory_resource* upstream = get_default_resource())
        : monotonic_buffer_resource(0, upstream) {}

    explicit monotonic_buffer_resource(
        size_t initial_size, memory_resource* upstream = get_default_resource())
        : upstream_(upstream ? upstream : get_default_resource()),
          core_(initial_size, upstream_) {}

    monotonic_buffer_resource(void* buffer, size_t buffer_size,
                              memory_resource* upstream = get_default_resource())
        : upstream_(upstream ? upstream : get_default_resource()),
          core_(0, buffer, buffer_size, upstream_) {}

    monotonic_buffer_resource(size_t initial_size, void* buffer,
                              size_t buffer_size,
                              memory_resource* upstream = get_default_resource())
        : upstream_(upstream ? upstream : get_default_resource()),
          core_(initial_size, buffer, buffer_size, upstream_) {}

    ~monotonic_buffer_resource() override = default;

    monotonic_buffer_resource(const monotonic_buffer_resource&) = delete;
    monotonic_buffer_resource& operator=(const monotonic_buffer_resource&) = delete;

    void release() noexcept { core_.release(); }
    memory_resource* upstream_resource() const noexcept { return upstream_; }

protected:
    void* do_allocate(size_t bytes, size_t alignment) override {
        return core_.allocate(bytes, alignment);
    }
    void do_deallocate(void*, size_t, size_t) noexcept override {}
    bool do_is_equal(const memory_resource& other) const noexcept override {
        return this == &other;
    }

private:
    memory_resource* upstream_;
    __detail::monotonic_resource_core core_;
};

class unsynchronized_pool_resource : public memory_resource {
public:
    explicit unsynchronized_pool_resource(
        const pool_options& options = pool_options(),
        memory_resource* upstream = get_default_resource())
        : core_(options, upstream ? upstream : get_default_resource()) {}

    explicit unsynchronized_pool_resource(memory_resource* upstream)
        : unsynchronized_pool_resource(pool_options(), upstream) {}

    ~unsynchronized_pool_resource() override = default;

    unsynchronized_pool_resource(const unsynchronized_pool_resource&) = delete;
    unsynchronized_pool_resource& operator=(
        const unsynchronized_pool_resource&) = delete;

    void release() noexcept { core_.release(); }
    pool_options options() const noexcept { return core_.options(); }
    memory_resource* upstream_resource() const noexcept {
        return core_.upstream_resource();
    }

protected:
    void* do_allocate(size_t bytes, size_t alignment) override {
        return core_.allocate(bytes, alignment);
    }
    void do_deallocate(void* pointer, size_t bytes,
                       size_t alignment) noexcept override {
        core_.deallocate(pointer, bytes, alignment);
    }
    bool do_is_equal(const memory_resource& other) const noexcept override {
        return this == &other;
    }

private:
    __detail::pool_resource_core core_;
};

class synchronized_pool_resource : public memory_resource {
public:
    explicit synchronized_pool_resource(
        const pool_options& options = pool_options(),
        memory_resource* upstream = get_default_resource())
        : core_(options, upstream ? upstream : get_default_resource()) {}

    explicit synchronized_pool_resource(memory_resource* upstream)
        : synchronized_pool_resource(pool_options(), upstream) {}

    ~synchronized_pool_resource() override = default;

    synchronized_pool_resource(const synchronized_pool_resource&) = delete;
    synchronized_pool_resource& operator=(
        const synchronized_pool_resource&) = delete;

    void release() noexcept {
        lock_guard<mutex> guard(mutex_);
        core_.release();
    }
    pool_options options() const noexcept { return core_.options(); }
    memory_resource* upstream_resource() const noexcept {
        return core_.upstream_resource();
    }

protected:
    void* do_allocate(size_t bytes, size_t alignment) override {
        lock_guard<mutex> guard(mutex_);
        return core_.allocate(bytes, alignment);
    }
    void do_deallocate(void* pointer, size_t bytes,
                       size_t alignment) noexcept override {
        lock_guard<mutex> guard(mutex_);
        core_.deallocate(pointer, bytes, alignment);
    }
    bool do_is_equal(const memory_resource& other) const noexcept override {
        return this == &other;
    }

private:
    mutable mutex mutex_;
    __detail::pool_resource_core core_;
};

} // namespace pmr
} // namespace std

#endif // __cplusplus >= 201703L

#endif // RINCXX_MEMORY_RESOURCE_H
