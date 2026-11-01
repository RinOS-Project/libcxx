/*
 * RinOS C++ <stacktrace> ✿
 * Allocator-backed stacktrace entry and capture surface (C++23)
 *
 * `current()` uses the compiler unwind ABI when it is available and allocates
 * frame storage through the selected allocator.  Toolchains without that ABI
 * keep the safe one-frame return-address fallback.  Symbol and DSO lookup
 * remain explicitly unavailable.
 */

#ifndef RINCXX_STACKTRACE_H
#define RINCXX_STACKTRACE_H

#include "rincxx.h"
#include "compare.h"
#include "cstdint.h"
#include "exception.h"
#include "memory.h"
#include "string.h"

#include "version.h"

#if defined(RIN_LIBCXX_CXX23_DIALECT)
#if defined(__has_include)
#if __has_include(<unwind.h>)
#include <unwind.h>
#define RIN_LIBCXX_HAS_UNWIND_BACKTRACE 1
#endif
#elif defined(__GNUC__) || defined(__clang__)
#include <unwind.h>
#define RIN_LIBCXX_HAS_UNWIND_BACKTRACE 1
#endif
#endif

#if defined(RIN_LIBCXX_CXX23_DIALECT)

namespace std {

/* <stacktrace> owns the entry carrier, while <functional> owns the primary
 * hash template.  A forward declaration keeps this standalone header usable
 * without forcing the entire callable surface into every diagnostic build. */
template<class T>
struct hash;

class stacktrace_entry {
public:
    using native_handle_type = uintptr_t;

private:
    native_handle_type native_;

public:
    constexpr stacktrace_entry() noexcept : native_(0) {}

    /* Public construction keeps diagnostics testable without requiring a
     * target symbolization service. */
    constexpr explicit stacktrace_entry(native_handle_type native) noexcept
        : native_(native) {}

    constexpr native_handle_type native_handle() const noexcept {
        return native_;
    }

    constexpr explicit operator bool() const noexcept {
        return native_ != 0;
    }

    constexpr bool operator==(const stacktrace_entry& other) const noexcept {
        return native_ == other.native_;
    }

    constexpr bool operator!=(const stacktrace_entry& other) const noexcept {
        return !(*this == other);
    }

    constexpr strong_ordering operator<=>(
        const stacktrace_entry& other) const noexcept {
        if (native_ < other.native_) return strong_ordering::less;
        if (native_ > other.native_) return strong_ordering::greater;
        return strong_ordering::equal;
    }

    /* Symbol lookup is intentionally not synthesized from an address. */
    string source_file() const {
        return string();
    }

    uint_least32_t source_line() const noexcept {
        return 0;
    }

    /* Return a stable, allocation-owning hexadecimal diagnostic even when
     * symbol services are absent.  Empty entries have the standard empty
     * description rather than exposing a fabricated address. */
    string description() const {
        if (native_ == 0) return string();

        char buffer[2 + sizeof(native_handle_type) * 2 + 1];
        buffer[0] = '0';
        buffer[1] = 'x';
        const char* digits = "0123456789abcdef";
        size_t write = 2;
        bool started = false;
        for (size_t bit = sizeof(native_handle_type) * 8; bit != 0; bit -= 4) {
            const unsigned digit = static_cast<unsigned>((native_ >> (bit - 4)) & 0xfu);
            if (digit != 0 || started || bit == 4) {
                buffer[write++] = digits[digit];
                started = true;
            }
        }
        buffer[write] = '\0';
        return string(buffer);
    }
};

/* C++23 requires stacktrace_entry to be hashable.  Hash only the opaque
 * native handle; symbolization is deliberately outside this bounded owner. */
template<>
struct hash<stacktrace_entry> {
    size_t operator()(const stacktrace_entry& entry) const noexcept {
        return static_cast<size_t>(entry.native_handle());
    }
};

#if defined(RIN_LIBCXX_HAS_UNWIND_BACKTRACE)
namespace __stacktrace_detail {

struct capture_state {
    stacktrace_entry* entries;
    size_t capacity;
    size_t skip;
    size_t count;
    bool full;
};

struct depth_state {
    size_t skip;
    size_t limit;
    size_t count;
    bool full;
};

static _Unwind_Reason_Code capture_frame(
    _Unwind_Context* frame, void* opaque) {
    capture_state* state = static_cast<capture_state*>(opaque);
    uintptr_t instruction;

    if (state == nullptr || frame == nullptr) return _URC_FATAL_PHASE1_ERROR;
    if (state->skip != 0u) {
        --state->skip;
        return _URC_NO_REASON;
    }
    instruction = static_cast<uintptr_t>(_Unwind_GetIP(frame));
    if (instruction == 0u) return _URC_NO_REASON;
    if (state->count >= state->capacity) {
        state->full = true;
        return _URC_NORMAL_STOP;
    }
    state->entries[state->count++] =
        stacktrace_entry(static_cast<stacktrace_entry::native_handle_type>(
            instruction));
    if (state->count == state->capacity) {
        state->full = true;
        return _URC_NORMAL_STOP;
    }
    return _URC_NO_REASON;
}

static _Unwind_Reason_Code count_frame(_Unwind_Context* frame, void* opaque) {
    depth_state* state = static_cast<depth_state*>(opaque);
    if (state == nullptr || frame == nullptr) return _URC_FATAL_PHASE1_ERROR;
    if (state->skip != 0u) {
        --state->skip;
        return _URC_NO_REASON;
    }
    if (static_cast<uintptr_t>(_Unwind_GetIP(frame)) == 0u)
        return _URC_NO_REASON;
    if (state->count == state->limit) {
        state->full = true;
        return _URC_NORMAL_STOP;
    }
    ++state->count;
    if (state->count == state->limit) {
        state->full = true;
        return _URC_NORMAL_STOP;
    }
    return _URC_NO_REASON;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static size_t count_frames(size_t skip, size_t limit) {
    if (limit == 0u) return 0u;
    depth_state state = { skip, limit, 0u, false };
    const _Unwind_Reason_Code reason = _Unwind_Backtrace(count_frame, &state);
    if (state.full || reason == _URC_END_OF_STACK) return state.count;
    return 0u;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static size_t capture(stacktrace_entry* entries, size_t capacity,
                      size_t skip) {
    capture_state state = { entries, capacity, skip, 0u, false };
    const _Unwind_Reason_Code reason =
        _Unwind_Backtrace(capture_frame, &state);

    /* A partial trace after a malformed/missing unwind record is misleading.
     * Only natural end-of-stack or our bounded-capacity stop is publishable. */
    if (state.full || reason == _URC_END_OF_STACK) return state.count;
    return 0u;
}

} /* namespace __stacktrace_detail */
#endif

template<class Allocator = allocator<stacktrace_entry>>
class basic_stacktrace {
public:
    using value_type = stacktrace_entry;
    using reference = value_type&;
    using const_reference = const value_type&;
    using const_iterator = const value_type*;
    using iterator = const_iterator;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using allocator_type = Allocator;
    using allocator_traits_type = allocator_traits<allocator_type>;
    using allocator_pointer = typename allocator_traits_type::pointer;

private:
    allocator_type allocator_;
    allocator_pointer storage_{};
    size_type size_;
    size_type capacity_;

    value_type* raw_data() noexcept {
        return capacity_ == 0u ? nullptr : std::to_address(storage_);
    }

    const value_type* raw_data() const noexcept {
        return capacity_ == 0u ? nullptr : std::to_address(storage_);
    }

    void release_storage() noexcept {
        if (capacity_ != 0u) {
            value_type* values = raw_data();
            for (size_type index = 0; index < size_; ++index)
                allocator_traits_type::destroy(allocator_, values + index);
            allocator_traits_type::deallocate(
                allocator_, storage_, capacity_);
        }
        storage_ = allocator_pointer{};
        size_ = 0u;
        capacity_ = 0u;
    }

    void destroy_suffix(size_type new_size) noexcept {
        value_type* values = raw_data();
        while (size_ > new_size) {
            --size_;
            allocator_traits_type::destroy(allocator_, values + size_);
        }
    }

    bool allocate_storage(size_type requested) {
        if (requested == 0u) return true;
        if (requested > max_size()) return false;
        allocator_pointer allocation =
            allocator_traits_type::allocate(allocator_, requested);
        if (allocation == allocator_pointer{}) return false;
        storage_ = std::move(allocation);
        capacity_ = requested;
        return true;
    }

    [[noreturn]] static void allocation_failed() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        throw bad_alloc();
#else
        rin_panic("basic_stacktrace allocation failed");
#endif
    }

    void copy_storage_from(const basic_stacktrace& other) {
        if (other.size_ == 0u) return;
        if (!allocate_storage(other.size_)) allocation_failed();
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            const value_type* source = other.raw_data();
            value_type* destination = raw_data();
            for (size_type index = 0; index < other.size_; ++index) {
                allocator_traits_type::construct(
                    allocator_, destination + index, source[index]);
                ++size_;
            }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            release_storage();
            throw;
        }
#endif
    }

    void initialize_for_capture(size_type count) {
        if (!allocate_storage(count)) return;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            value_type* destination = raw_data();
            for (; size_ < count; ++size_)
                allocator_traits_type::construct(
                    allocator_, destination + size_);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            release_storage();
            throw;
        }
#endif
    }

    void take_storage(basic_stacktrace& other) noexcept {
        storage_ = std::move(other.storage_);
        size_ = other.size_;
        capacity_ = other.capacity_;
        other.storage_ = allocator_pointer{};
        other.size_ = 0u;
        other.capacity_ = 0u;
    }

    void clear_entries() noexcept {
        destroy_suffix(0u);
    }

#if defined(__GNUC__) || defined(__clang__)
    __attribute__((noinline))
#endif
    static basic_stacktrace capture_current(size_type skip,
                                            size_type max_depth,
                                            const allocator_type& alloc,
                                            uintptr_t fallback_address) noexcept {
        basic_stacktrace result(alloc);
        if (max_depth == 0u) return result;

#if defined(RIN_LIBCXX_HAS_UNWIND_BACKTRACE)
        if (skip > static_cast<size_type>(-1) - 2u) return result;
        const size_type internal_skip = skip + 2u;
        const size_type allocator_limit = result.max_size();
        size_type count_limit = max_depth;
        if (count_limit > allocator_limit &&
            allocator_limit != static_cast<size_type>(-1))
            count_limit = allocator_limit + 1u;
        const size_type depth = __stacktrace_detail::count_frames(
            internal_skip, count_limit);
        if (depth == 0u || depth > allocator_limit) return result;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            result.initialize_for_capture(depth);
            if (result.capacity_ != depth) return result;
            const size_type captured = __stacktrace_detail::capture(
                result.raw_data(), depth, internal_skip);
            result.destroy_suffix(captured);
            return result;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            result.release_storage();
            return result;
        }
#endif
#elif defined(__GNUC__) || defined(__clang__)
        if (skip != 0u || fallback_address == 0u ||
            result.max_size() == 0u) return result;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            result.initialize_for_capture(1u);
            if (result.capacity_ == 1u) {
                result.raw_data()[0] = stacktrace_entry(
                    static_cast<stacktrace_entry::native_handle_type>(
                        fallback_address));
            }
            return result;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            result.release_storage();
            return result;
        }
#endif
#else
        (void)skip;
        (void)fallback_address;
        return result;
#endif
    }

public:
    constexpr basic_stacktrace() noexcept(
        is_nothrow_default_constructible<allocator_type>::value)
        : allocator_(), storage_{}, size_(0), capacity_(0) {}

    explicit basic_stacktrace(const allocator_type& alloc) noexcept(
        is_nothrow_copy_constructible<allocator_type>::value)
        : allocator_(alloc), storage_{}, size_(0), capacity_(0) {}

    basic_stacktrace(const basic_stacktrace& other)
        : allocator_(allocator_traits_type::select_on_container_copy_construction(
              other.allocator_)), storage_{}, size_(0), capacity_(0) {
        copy_storage_from(other);
    }

    basic_stacktrace(const basic_stacktrace& other,
                     const allocator_type& alloc)
        : allocator_(alloc), storage_{}, size_(0), capacity_(0) {
        copy_storage_from(other);
    }

    basic_stacktrace(basic_stacktrace&& other) noexcept
        : allocator_(std::move(other.allocator_)),
          storage_(std::move(other.storage_)), size_(other.size_),
          capacity_(other.capacity_) {
        other.storage_ = allocator_pointer{};
        other.size_ = 0u;
        other.capacity_ = 0u;
    }

    basic_stacktrace(basic_stacktrace&& other,
                     const allocator_type& alloc)
        : allocator_(alloc), storage_{}, size_(0), capacity_(0) {
        if (allocator_ == other.allocator_) take_storage(other);
        else {
            copy_storage_from(other);
            other.clear_entries();
        }
    }

    ~basic_stacktrace() noexcept { release_storage(); }

    basic_stacktrace& operator=(const basic_stacktrace& other) {
        if (this == &other) return *this;
        if constexpr (allocator_traits_type::
                          propagate_on_container_copy_assignment::value) {
            basic_stacktrace replacement(other, other.allocator_);
            release_storage();
            allocator_ = other.allocator_;
            take_storage(replacement);
        } else {
            basic_stacktrace replacement(other, allocator_);
            release_storage();
            take_storage(replacement);
        }
        return *this;
    }

    basic_stacktrace& operator=(basic_stacktrace&& other) noexcept(
        allocator_traits_type::propagate_on_container_move_assignment::value ||
        allocator_traits_type::is_always_equal::value) {
        if (this == &other) return *this;
        if constexpr (allocator_traits_type::
                          propagate_on_container_move_assignment::value) {
            release_storage();
            allocator_ = std::move(other.allocator_);
            take_storage(other);
        } else if (allocator_ == other.allocator_) {
            release_storage();
            take_storage(other);
        } else {
            basic_stacktrace replacement(other, allocator_);
            release_storage();
            take_storage(replacement);
            other.clear_entries();
        }
        return *this;
    }

    size_type size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }
    size_type max_size() const noexcept {
        return allocator_traits_type::max_size(allocator_);
    }
    const_iterator begin() const noexcept { return raw_data(); }
    const_iterator end() const noexcept {
        const_iterator first = raw_data();
        return size_ == 0u ? first : first + size_;
    }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }
    const_reverse_iterator rbegin() const noexcept {
        return const_reverse_iterator(cend());
    }
    const_reverse_iterator rend() const noexcept {
        return const_reverse_iterator(cbegin());
    }
    const_reverse_iterator crbegin() const noexcept { return rbegin(); }
    const_reverse_iterator crend() const noexcept { return rend(); }

    const_reference front() const noexcept { return raw_data()[0]; }
    const_reference back() const noexcept { return raw_data()[size_ - 1]; }

    allocator_type get_allocator() const noexcept { return allocator_; }

    const_reference operator[](size_type index) const noexcept {
        return raw_data()[index];
    }

    const_reference at(size_type index) const {
        if (index < size_) return raw_data()[index];
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        throw out_of_range("basic_stacktrace::at");
#else
        rin_panic("basic_stacktrace::at");
#endif
    }

    void swap(basic_stacktrace& other) noexcept(
        allocator_traits_type::propagate_on_container_swap::value ||
        allocator_traits_type::is_always_equal::value) {
        if (this == &other) return;
        if constexpr (allocator_traits_type::propagate_on_container_swap::value) {
            using std::swap;
            swap(allocator_, other.allocator_);
        } else if (!(allocator_ == other.allocator_)) {
            __builtin_trap();
        }
        using std::swap;
        swap(storage_, other.storage_);
        swap(size_, other.size_);
        swap(capacity_, other.capacity_);
    }

    template<class Allocator2>
    friend bool operator==(
        const basic_stacktrace& left,
        const basic_stacktrace<Allocator2>& right) noexcept {
        if (left.size() != right.size()) return false;
        const_iterator left_entry = left.begin();
        typename basic_stacktrace<Allocator2>::const_iterator right_entry = right.begin();
        for (size_type index = 0; index < left.size(); ++index) {
            if (left_entry[index] != right_entry[index]) return false;
        }
        return true;
    }

    template<class Allocator2>
    friend bool operator!=(
        const basic_stacktrace& left,
        const basic_stacktrace<Allocator2>& right) noexcept {
        return !(left == right);
    }

    template<class Allocator2>
    friend strong_ordering operator<=>
        (const basic_stacktrace& left,
         const basic_stacktrace<Allocator2>& right) noexcept {
        if (left.size() < right.size()) return strong_ordering::less;
        if (left.size() > right.size()) return strong_ordering::greater;
        const_iterator left_entry = left.begin();
        typename basic_stacktrace<Allocator2>::const_iterator right_entry =
            right.begin();
        for (size_type index = 0; index < left.size(); ++index) {
            const strong_ordering order = left_entry[index] <=>
                                          right_entry[index];
            if (order != strong_ordering::equal) return order;
        }
        return strong_ordering::equal;
    }

#if defined(__GNUC__) || defined(__clang__)
    __attribute__((noinline))
#endif
    static basic_stacktrace current(
        const allocator_type& alloc = allocator_type()) noexcept {
        uintptr_t fallback_address = 0u;
#if !defined(RIN_LIBCXX_HAS_UNWIND_BACKTRACE) && \
    (defined(__GNUC__) || defined(__clang__))
        fallback_address = reinterpret_cast<uintptr_t>(
            __builtin_return_address(0));
#endif
        return capture_current(0u, static_cast<size_type>(-1), alloc,
                               fallback_address);
    }

#if defined(__GNUC__) || defined(__clang__)
    __attribute__((noinline))
#endif
    static basic_stacktrace current(size_type skip,
        const allocator_type& alloc = allocator_type()) noexcept {
        uintptr_t fallback_address = 0u;
#if !defined(RIN_LIBCXX_HAS_UNWIND_BACKTRACE) && \
    (defined(__GNUC__) || defined(__clang__))
        fallback_address = reinterpret_cast<uintptr_t>(
            __builtin_return_address(0));
#endif
        return capture_current(skip, static_cast<size_type>(-1), alloc,
                               fallback_address);
    }

#if defined(__GNUC__) || defined(__clang__)
    __attribute__((noinline))
#endif
    static basic_stacktrace current(
        size_type skip, size_type max_depth,
        const allocator_type& alloc = allocator_type()) noexcept {
        uintptr_t fallback_address = 0u;
#if !defined(RIN_LIBCXX_HAS_UNWIND_BACKTRACE) && \
    (defined(__GNUC__) || defined(__clang__))
        fallback_address = reinterpret_cast<uintptr_t>(
            __builtin_return_address(0));
#endif
        return capture_current(skip, max_depth, alloc, fallback_address);
    }
};

/* Standard non-member swap overload.  Keep the allocator propagation and
 * conditional noexcept policy in the member owner so ADL and qualified
 * std::swap observe exactly the same transaction. */
template<class Allocator>
void swap(basic_stacktrace<Allocator>& left,
          basic_stacktrace<Allocator>& right)
    noexcept(noexcept(left.swap(right))) {
    left.swap(right);
}

using stacktrace = basic_stacktrace<>;

} /* namespace std */

#endif /* C++23 */
#if defined(RIN_LIBCXX_HAS_UNWIND_BACKTRACE)
#undef RIN_LIBCXX_HAS_UNWIND_BACKTRACE
#endif
#endif /* RINCXX_STACKTRACE_H */
