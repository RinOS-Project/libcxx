/*
 * RinOS C++ <stacktrace> ✿
 * Bounded stacktrace entry and capture surface (C++23)
 *
 * `current()` uses the compiler unwind ABI when it is available, with a
 * fixed-capacity result and no allocation.  Toolchains without that ABI keep
 * the safe one-frame return-address fallback.  Symbol and DSO lookup remain
 * explicitly unavailable.
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
    using const_reference = const value_type&;
    using const_iterator = const value_type*;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using allocator_type = Allocator;
    using allocator_traits_type = allocator_traits<allocator_type>;

private:
    /* Keep capture bounded and deterministic.  The capacity is part of this
     * implementation's ABI. */
    static constexpr size_type capacity = 16;
    value_type entries_[capacity];
    size_type size_;
    allocator_type allocator_;

    constexpr void copy_entries(const basic_stacktrace& other) noexcept {
        for (size_type index = 0; index < other.size_; ++index)
            entries_[index] = other.entries_[index];
    }

public:
    constexpr basic_stacktrace() noexcept
        : entries_{}, size_(0), allocator_() {}

    explicit basic_stacktrace(const allocator_type& alloc) noexcept
        : entries_{}, size_(0), allocator_(alloc) {}

    basic_stacktrace(const basic_stacktrace& other)
        : entries_{}, size_(other.size_),
          allocator_(allocator_traits_type::select_on_container_copy_construction(
              other.allocator_)) {
        copy_entries(other);
    }

    basic_stacktrace(const basic_stacktrace& other,
                     const allocator_type& alloc)
        : entries_{}, size_(other.size_), allocator_(alloc) {
        copy_entries(other);
    }

    basic_stacktrace(basic_stacktrace&& other) noexcept(
        is_nothrow_move_constructible<allocator_type>::value)
        : entries_{}, size_(other.size_),
          allocator_(std::move(other.allocator_)) {
        copy_entries(other);
        other.size_ = 0;
    }

    basic_stacktrace(basic_stacktrace&& other,
                     const allocator_type& alloc) noexcept(
        is_nothrow_copy_constructible<allocator_type>::value)
        : entries_{}, size_(other.size_), allocator_(alloc) {
        copy_entries(other);
        other.size_ = 0;
    }

    basic_stacktrace& operator=(const basic_stacktrace& other) {
        if (this == &other) return *this;
        if constexpr (allocator_traits_type::
                          propagate_on_container_copy_assignment::value) {
            allocator_ = other.allocator_;
        }
        size_ = other.size_;
        copy_entries(other);
        return *this;
    }

    basic_stacktrace& operator=(basic_stacktrace&& other) noexcept(
        allocator_traits_type::propagate_on_container_move_assignment::value
            ? is_nothrow_move_assignable<allocator_type>::value
            : true) {
        if (this == &other) return *this;
        if constexpr (allocator_traits_type::
                          propagate_on_container_move_assignment::value) {
            allocator_ = std::move(other.allocator_);
        }
        size_ = other.size_;
        copy_entries(other);
        other.size_ = 0;
        return *this;
    }

    constexpr size_type size() const noexcept { return size_; }
    constexpr bool empty() const noexcept { return size_ == 0; }
    constexpr size_type max_size() const noexcept { return capacity; }
    constexpr const_iterator begin() const noexcept { return entries_; }
    constexpr const_iterator end() const noexcept { return entries_ + size_; }

    constexpr const_reference front() const noexcept { return entries_[0]; }
    constexpr const_reference back() const noexcept { return entries_[size_ - 1]; }

    allocator_type get_allocator() const { return allocator_; }

    constexpr const_reference operator[](size_type index) const noexcept {
        return entries_[index];
    }

    constexpr const_reference at(size_type index) const {
        if (index < size_) return entries_[index];
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        throw out_of_range("basic_stacktrace::at");
#else
        rin_panic("basic_stacktrace::at");
#endif
    }

    void swap(basic_stacktrace& other) noexcept(
        !allocator_traits_type::propagate_on_container_swap::value ||
        is_nothrow_swappable<allocator_type>::value) {
        if (this == &other) return;
        for (size_type index = 0; index < capacity; ++index) {
            value_type entry = entries_[index];
            entries_[index] = other.entries_[index];
            other.entries_[index] = entry;
        }
        const size_type size = size_;
        size_ = other.size_;
        other.size_ = size;
        if constexpr (allocator_traits_type::propagate_on_container_swap::value) {
            using std::swap;
            swap(allocator_, other.allocator_);
        }
    }

    friend constexpr bool operator==(const basic_stacktrace& left,
                                     const basic_stacktrace& right) noexcept {
        if (left.size_ != right.size_) return false;
        for (size_type index = 0; index < left.size_; ++index) {
            if (left.entries_[index] != right.entries_[index]) return false;
        }
        return true;
    }

    friend constexpr bool operator!=(const basic_stacktrace& left,
                                     const basic_stacktrace& right) noexcept {
        return !(left == right);
    }

    friend constexpr strong_ordering operator<=>
        (const basic_stacktrace& left,
         const basic_stacktrace& right) noexcept {
        if (left.size_ < right.size_) return strong_ordering::less;
        if (left.size_ > right.size_) return strong_ordering::greater;
        for (size_type index = 0; index < left.size_; ++index) {
            const strong_ordering order = left.entries_[index] <=>
                                          right.entries_[index];
            if (order != strong_ordering::equal) return order;
        }
        return strong_ordering::equal;
    }

#if defined(__GNUC__) || defined(__clang__)
    __attribute__((noinline))
#endif
    static basic_stacktrace current(size_type skip = 0) noexcept {
        basic_stacktrace result;
#if defined(RIN_LIBCXX_HAS_UNWIND_BACKTRACE)
        if (skip <= static_cast<size_type>(-1) - 2u)
            result.size_ = __stacktrace_detail::capture(
                result.entries_, capacity, skip + 2u);
#elif defined(__GNUC__) || defined(__clang__)
        if (skip == 0u) {
            const void* return_address = __builtin_return_address(0);
            if (return_address != nullptr) {
                result.entries_[0] = stacktrace_entry(
                    reinterpret_cast<stacktrace_entry::native_handle_type>(
                        return_address));
                result.size_ = 1u;
            }
        }
#else
        (void)skip;
#endif
        return result;
    }

#if defined(__GNUC__) || defined(__clang__)
    __attribute__((noinline))
#endif
    static basic_stacktrace current(const allocator_type& alloc,
                                    size_type skip = 0) noexcept {
        basic_stacktrace result(alloc);
#if defined(RIN_LIBCXX_HAS_UNWIND_BACKTRACE)
        if (skip <= static_cast<size_type>(-1) - 2u)
            result.size_ = __stacktrace_detail::capture(
                result.entries_, capacity, skip + 2u);
#elif defined(__GNUC__) || defined(__clang__)
        if (skip == 0u) {
            const void* return_address = __builtin_return_address(0);
            if (return_address != nullptr) {
                result.entries_[0] = stacktrace_entry(
                    reinterpret_cast<stacktrace_entry::native_handle_type>(
                        return_address));
                result.size_ = 1u;
            }
        }
#else
        (void)skip;
#endif
        return result;
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
