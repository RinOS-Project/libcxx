/*
 * RinOS C++ <generator>
 * C++23 coroutine-backed input ranges.
 */

#ifndef RINCXX_GENERATOR_H
#define RINCXX_GENERATOR_H

#include "rincxx.h"
#include "version.h"

#if defined(RIN_LIBCXX_CXX23_DIALECT) && \
    ((defined(__cpp_impl_coroutine) && __cpp_impl_coroutine >= 201902L) || \
     (defined(__cpp_coroutines) && __cpp_coroutines >= 201703L))

#include "coroutine.h"
#include "exception.h"
#include "memory.h"
#include "new.h"
#include "ranges.h"
#include "type_traits.h"
#include "utility.h"

namespace std {

template<class Ref, class Val = void, class Allocator = void>
class generator;

namespace detail {

struct generator_state;

struct generator_node {
    generator_state* state;
    generator_node* previous;
    coroutine_handle<> coroutine;
    void* value;
};

struct generator_state {
    generator_node* top;
};

template<class T>
struct is_generator : false_type {};

template<class Ref, class Val, class Allocator>
struct is_generator<generator<Ref, Val, Allocator>> : true_type {};

struct generator_allocation_record {
    void (*release)(generator_allocation_record*) noexcept;
};

struct generator_allocation_prefix {
    generator_allocation_record* record;
};

inline size_t generator_frame_overhead(size_t header_size,
                                       size_t alignment) noexcept {
    if (alignment < alignof(max_align_t)) alignment = alignof(max_align_t);
    const size_t maximum = static_cast<size_t>(-1);
    if (header_size > maximum - sizeof(generator_allocation_prefix) ||
        alignment - 1 > maximum - header_size -
                            sizeof(generator_allocation_prefix))
        return maximum;
    return header_size + sizeof(generator_allocation_prefix) + alignment - 1;
}

inline void* finish_generator_frame_allocation(
    void* allocation, size_t header_size, size_t alignment,
    generator_allocation_record* record) noexcept {
    if (alignment < alignof(max_align_t)) alignment = alignof(max_align_t);
    auto* first = static_cast<unsigned char*>(allocation) + header_size +
                  sizeof(generator_allocation_prefix);
    const uintptr_t address = reinterpret_cast<uintptr_t>(first);
    const uintptr_t frame_address =
        (address + alignment - 1) / alignment * alignment;
    auto* frame = reinterpret_cast<unsigned char*>(frame_address);
    auto* prefix = reinterpret_cast<generator_allocation_prefix*>(
        frame - sizeof(generator_allocation_prefix));
    prefix->record = record;
    return frame;
}

struct generator_rin_allocation_record : generator_allocation_record {
    void* allocation;
};

inline void release_generator_rin_allocation(
    generator_allocation_record* base) noexcept {
    auto* record = static_cast<generator_rin_allocation_record*>(base);
    void* allocation = record->allocation;
    record->~generator_rin_allocation_record();
    rin_free(allocation);
}

inline void* allocate_generator_frame(
    size_t size, size_t alignment = alignof(max_align_t)) noexcept {
    const size_t overhead = generator_frame_overhead(
        sizeof(generator_rin_allocation_record), alignment);
    if (overhead == static_cast<size_t>(-1)) return nullptr;
    if (size > static_cast<size_t>(-1) - overhead) return nullptr;
    void* allocation = rin_malloc(overhead + size);
    if (!allocation) return nullptr;
    auto* record = ::new (allocation) generator_rin_allocation_record{
        {&release_generator_rin_allocation}, allocation};
    return finish_generator_frame_allocation(allocation,
        sizeof(generator_rin_allocation_record), alignment, record);
}

template<class Allocator>
struct generator_allocator_record : generator_allocation_record {
    using unit_type = max_align_t;
    using unit_allocator = typename allocator_traits<Allocator>::template
        rebind_alloc<unit_type>;
    using traits = allocator_traits<unit_allocator>;
    using pointer = typename traits::pointer;

    unit_allocator allocator;
    pointer allocation;
    size_t count;

    generator_allocator_record(const unit_allocator& alloc, pointer storage,
                               size_t units)
        : generator_allocation_record{&release}, allocator(alloc),
          allocation(storage), count(units) {}

    static void release(generator_allocation_record* base) noexcept {
        auto* record = static_cast<generator_allocator_record*>(base);
        unit_allocator allocator_copy(record->allocator);
        pointer allocation = record->allocation;
        const size_t count = record->count;
        record->~generator_allocator_record();
        traits::deallocate(allocator_copy, allocation, count);
    }
};

template<class Allocator>
void* allocate_generator_frame(
    size_t size, const Allocator& source,
    size_t alignment = alignof(max_align_t)) noexcept {
    using record_type = generator_allocator_record<Allocator>;
    using unit_allocator = typename record_type::unit_allocator;
    using traits = typename record_type::traits;
    using unit_type = typename record_type::unit_type;
    static_assert(is_pointer_v<typename traits::pointer>,
                  "generator allocators must use raw pointers");

    const size_t overhead = generator_frame_overhead(sizeof(record_type), alignment);
    if (overhead == static_cast<size_t>(-1)) return nullptr;
    if (size > static_cast<size_t>(-1) - overhead) return nullptr;
    const size_t bytes = overhead + size;
    if (bytes > static_cast<size_t>(-1) - (sizeof(unit_type) - 1))
        return nullptr;
    const size_t count = (bytes + sizeof(unit_type) - 1) / sizeof(unit_type);
    typename traits::pointer storage = nullptr;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    try {
        unit_allocator allocator(source);
        storage = traits::allocate(allocator, count);
        if (!storage) return nullptr;
        auto* record = ::new (static_cast<void*>(storage)) record_type(
            allocator, storage, count);
        return finish_generator_frame_allocation(storage, sizeof(record_type),
                                                  alignment, record);
    } catch (...) {
        if (storage) {
            try {
                unit_allocator cleanup_allocator(source);
                traits::deallocate(cleanup_allocator, storage, count);
            } catch (...) {
            }
        }
        return nullptr;
    }
#else
    unit_allocator allocator(source);
    storage = traits::allocate(allocator, count);
    if (!storage) return nullptr;
    auto* record = ::new (static_cast<void*>(storage)) record_type(
        allocator, storage, count);
    return finish_generator_frame_allocation(storage, sizeof(record_type),
                                              alignment, record);
#endif
}

inline void deallocate_generator_frame(void* frame) noexcept {
    if (!frame) return;
    auto* prefix = reinterpret_cast<generator_allocation_prefix*>(
        static_cast<unsigned char*>(frame) -
        sizeof(generator_allocation_prefix));
    prefix->record->release(prefix->record);
}

struct generator_final_awaiter {
    generator_node* node;

    bool await_ready() const noexcept { return false; }

    coroutine_handle<> await_suspend(coroutine_handle<>) const noexcept {
        generator_state* state = node->state;
        if (node->previous) {
            state->top = node->previous;
            return state->top->coroutine;
        } else {
            state->top = nullptr;
            return noop_coroutine();
        }
    }

    void await_resume() const noexcept {}
};

} // namespace detail

template<class Ref, class Val, class Allocator>
class generator : public ranges::view_interface<generator<Ref, Val, Allocator>> {
    using value = conditional_t<is_void_v<Val>, remove_cvref_t<Ref>, Val>;
    using reference = conditional_t<is_void_v<Val>, Ref&&, Ref>;

public:
    using yielded = conditional_t<is_reference_v<reference>, reference,
                                  const reference&>;

    class promise_type {
        friend class generator;

    public:
        detail::generator_state state_{nullptr};
        detail::generator_node node_{&state_, nullptr, {}, nullptr};
        exception_ptr exception_{};

        promise_type() = default;

        static generator get_return_object_on_allocation_failure() noexcept {
            return generator{coroutine_handle<promise_type>{}};
        }

        template<class A, class... Args,
                 enable_if_t<is_void_v<Allocator> ||
                                 is_convertible_v<const A&, Allocator>,
                             int> = 0>
        promise_type(allocator_arg_t, const A&, Args&&...) noexcept {}

        template<class... Args>
        explicit promise_type(Args&&...) noexcept {}

        generator get_return_object() noexcept {
            node_.coroutine = coroutine_handle<promise_type>::from_promise(*this);
            return generator{coroutine_handle<promise_type>::from_promise(*this)};
        }

        suspend_always initial_suspend() const noexcept { return {}; }

        detail::generator_final_awaiter final_suspend() noexcept {
            return {&node_};
        }

        suspend_always yield_value(yielded value) noexcept {
            node_.value = const_cast<void*>(static_cast<const void*>(
                addressof(value)));
            return {};
        }

        auto yield_value(const remove_reference_t<yielded>& value)
            requires (is_rvalue_reference_v<yielded> &&
                      is_constructible_v<remove_cvref_t<yielded>,
                                         const remove_reference_t<yielded>&>) {
            struct copy_awaiter {
                promise_type* promise;
                remove_cvref_t<yielded> value;

                bool await_ready() const noexcept { return false; }
                void await_suspend(coroutine_handle<promise_type>) noexcept {
                    promise->node_.value = addressof(value);
                }
                void await_resume() const noexcept {}
            };
            return copy_awaiter{this, value};
        }

        template<class R2, class V2, class A2, class Unused>
            requires same_as<typename generator<R2, V2, A2>::yielded, yielded>
        auto yield_value(ranges::elements_of<generator<R2, V2, A2>&&, Unused> g) noexcept {
            return make_generator_awaiter(std::move(g.range));
        }

        template<class R2, class V2, class A2, class Unused>
            requires same_as<typename generator<R2, V2, A2>::yielded, yielded>
        auto yield_value(ranges::elements_of<generator<R2, V2, A2>&, Unused> g) noexcept {
            return make_generator_awaiter(std::move(g.range));
        }

        template<ranges::input_range R, class A>
            requires (!detail::is_generator<remove_cvref_t<R>>::value &&
                      convertible_to<ranges::range_reference_t<R>, yielded>)
        auto yield_value(ranges::elements_of<R, A> range) {
            auto child = make_range_generator(
                allocator_arg, std::move(range.allocator),
                ranges::begin(range.range), ranges::end(range.range));
            using child_type = decltype(child);
            return yield_value(
                ranges::elements_of<child_type&&>{std::move(child)});
        }

        void await_transform() = delete;
        void return_void() const noexcept {}

        void unhandled_exception() noexcept {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            exception_ = current_exception();
#else
            __builtin_trap();
#endif
        }

        static void* operator new(size_t size) noexcept
            requires (is_void_v<Allocator> || default_initializable<Allocator>) {
            return allocate_with_default_allocator(size,
                                                    alignof(max_align_t));
        }

        static void* operator new(size_t size, align_val_t alignment) noexcept
            requires (is_void_v<Allocator> || default_initializable<Allocator>) {
            return allocate_with_default_allocator(
                size, static_cast<size_t>(alignment));
        }

        template<class A>
        static void* allocate_using_allocator(size_t size,
                                              const A& allocator,
                                              size_t alignment) noexcept {
            if constexpr (is_void_v<Allocator>) {
                return detail::allocate_generator_frame(size, allocator,
                                                        alignment);
            } else {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
                try {
                    Allocator selected(allocator);
                    return detail::allocate_generator_frame(size, selected,
                                                            alignment);
                } catch (...) {
                    return nullptr;
                }
#else
                Allocator selected(allocator);
                return detail::allocate_generator_frame(size, selected,
                                                        alignment);
#endif
            }
        }

        static void* allocate_with_default_allocator(
            size_t size, size_t alignment) noexcept {
            if constexpr (is_void_v<Allocator>) {
                return detail::allocate_generator_frame(size, alignment);
            } else {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
                try {
                    Allocator allocator{};
                    return allocate_using_allocator(size, allocator, alignment);
                } catch (...) {
                    return nullptr;
                }
#else
                Allocator allocator{};
                return allocate_using_allocator(size, allocator, alignment);
#endif
            }
        }

        template<class A, class... Args>
            requires (is_void_v<Allocator> ||
                      is_convertible_v<const A&, Allocator>)
        static void* operator new(size_t size, allocator_arg_t,
                                  const A& allocator, Args&&...) noexcept {
            return allocate_using_allocator(size, allocator,
                                            alignof(max_align_t));
        }

        template<class A, class... Args>
            requires (is_void_v<Allocator> ||
                      is_convertible_v<const A&, Allocator>)
        static void* operator new(size_t size, align_val_t alignment,
                                  allocator_arg_t, const A& allocator,
                                  Args&&...) noexcept {
            return allocate_using_allocator(
                size, allocator, static_cast<size_t>(alignment));
        }

        template<class This, class A, class... Args>
            requires (is_void_v<Allocator> ||
                      is_convertible_v<const A&, Allocator>)
        static void* operator new(size_t size, const This&, allocator_arg_t,
                                  const A& allocator, Args&&...) noexcept {
            return allocate_using_allocator(size, allocator,
                                            alignof(max_align_t));
        }


        template<class This, class A, class... Args>
            requires (is_void_v<Allocator> ||
                      is_convertible_v<const A&, Allocator>)
        static void* operator new(size_t size, const This&, align_val_t alignment,
                                  allocator_arg_t, const A& allocator,
                                  Args&&...) noexcept {
            return allocate_using_allocator(
                size, allocator, static_cast<size_t>(alignment));
        }

        static void operator delete(void* frame, size_t) noexcept {
            detail::deallocate_generator_frame(frame);
        }

        static void operator delete(void* frame) noexcept {
            detail::deallocate_generator_frame(frame);
        }

        static void operator delete(void* frame, align_val_t) noexcept {
            detail::deallocate_generator_frame(frame);
        }

        static void operator delete(void* frame, size_t, align_val_t) noexcept {
            detail::deallocate_generator_frame(frame);
        }

    private:
        template<class Child>
        struct generator_awaiter {
            promise_type* parent;
            Child child;

            bool await_ready() const noexcept { return !child.coroutine_; }

            coroutine_handle<> await_suspend(
                coroutine_handle<promise_type>) noexcept {
                auto& promise = child.coroutine_.promise();
                promise.node_.state = parent->node_.state;
                promise.node_.previous = &parent->node_;
                promise.node_.coroutine = child.coroutine_;
                promise.node_.state->top = &promise.node_;
                return child.coroutine_;
            }

            void await_resume() {
                if (child.coroutine_ && child.coroutine_.promise().exception_)
                    rethrow_exception(child.coroutine_.promise().exception_);
            }
        };

        template<class Child>
        generator_awaiter<Child> make_generator_awaiter(Child&& child) noexcept {
            return {this, std::forward<Child>(child)};
        }

        template<class A, class I, class S>
        static generator<yielded, void, A> make_range_generator(
            allocator_arg_t, A allocator, I first, S last) {
            (void)allocator;
            for (; first != last; ++first)
                co_yield static_cast<yielded>(*first);
        }
    };

    class iterator {
    public:
        using iterator_concept = input_iterator_tag;
        using iterator_category = input_iterator_tag;
        using value_type = value;
        using difference_type = ptrdiff_t;
        using reference = generator::reference;
        using pointer = add_pointer_t<yielded>;

        iterator() noexcept = default;
        explicit iterator(coroutine_handle<promise_type> coroutine) noexcept
            : coroutine_(coroutine) {}

        iterator(iterator&& other) noexcept
            : coroutine_(exchange(other.coroutine_, {})) {}

        iterator& operator=(iterator&& other) noexcept {
            coroutine_ = exchange(other.coroutine_, {});
            return *this;
        }

        reference operator*() const noexcept(is_nothrow_copy_constructible_v<reference>) {
            auto* node = coroutine_.promise().state_.top;
            return static_cast<reference>(
                *static_cast<add_pointer_t<yielded>>(node->value));
        }

        iterator& operator++() {
            auto& state = coroutine_.promise().state_;
            if (state.top) state.top->coroutine.resume();
            if (coroutine_.promise().exception_)
                rethrow_exception(coroutine_.promise().exception_);
            return *this;
        }

        void operator++(int) { ++*this; }

        friend bool operator==(const iterator& it, default_sentinel_t) noexcept {
            return !it.coroutine_ || it.coroutine_.done();
        }

        friend bool operator==(default_sentinel_t sentinel,
                               const iterator& it) noexcept {
            return it == sentinel;
        }

    private:
        coroutine_handle<promise_type> coroutine_{};
    };

    generator(const generator&) = delete;
    generator(generator&& other) noexcept
        : coroutine_(exchange(other.coroutine_, {})) {}

    ~generator() {
        if (coroutine_) coroutine_.destroy();
    }

    generator& operator=(generator other) noexcept {
        swap(coroutine_, other.coroutine_);
        return *this;
    }

    iterator begin() {
        if (!coroutine_) return iterator{};
        auto& promise = coroutine_.promise();
        promise.state_.top = &promise.node_;
        promise.node_.state = &promise.state_;
        promise.node_.previous = nullptr;
        coroutine_.resume();
        if (promise.exception_) rethrow_exception(promise.exception_);
        return iterator{coroutine_};
    }

    default_sentinel_t end() const noexcept { return {}; }

private:
    friend class promise_type;
    template<class, class, class> friend class generator;

    explicit generator(coroutine_handle<promise_type> coroutine) noexcept
        : coroutine_(coroutine) {}

    coroutine_handle<promise_type> coroutine_{};
};

} // namespace std

#endif // C++23 coroutine support

#endif // RINCXX_GENERATOR_H
