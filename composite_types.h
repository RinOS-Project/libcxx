/* C++26 value-semantic wrappers for dynamically allocated objects. */
#ifndef RINCXX_COMPOSITE_TYPES_H
#define RINCXX_COMPOSITE_TYPES_H

#include "initializer_list.h"
#include "composite_types_version.h"
#include "concepts.h"

#if defined(__cpp_lib_indirect)
namespace std {

template<class T, class Allocator>
class indirect;

namespace detail {
template<class T> struct is_indirect_wrapper : false_type {};
template<class T, class Allocator>
struct is_indirect_wrapper<indirect<T, Allocator>> : true_type {};
} // namespace detail

template<class T, class Allocator = allocator<T>>
class indirect {
    static_assert(is_same<typename allocator_traits<Allocator>::value_type,
                          T>::value,
                  "indirect allocator value_type must match T");

public:
    using value_type = T;
    using allocator_type = Allocator;
    using pointer = typename allocator_traits<Allocator>::pointer;
    using const_pointer = typename allocator_traits<Allocator>::const_pointer;

private:
    Allocator alloc_;
    pointer pointer_{};
    bool engaged_ = false;

    template<class... Args>
    static pointer create(Allocator& alloc, Args&&... args) {
        pointer storage = allocator_traits<Allocator>::allocate(alloc, 1);
        T* object = std::to_address(storage);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            allocator_traits<Allocator>::construct(
                alloc, object, std::forward<Args>(args)...);
        } catch (...) {
            allocator_traits<Allocator>::deallocate(alloc, storage, 1);
            throw;
        }
#else
        allocator_traits<Allocator>::construct(
            alloc, object, std::forward<Args>(args)...);
#endif
        return storage;
    }

    static void destroy(Allocator& alloc, pointer storage) noexcept {
        T* object = std::to_address(storage);
        allocator_traits<Allocator>::destroy(alloc, object);
        allocator_traits<Allocator>::deallocate(alloc, storage, 1);
    }

    void reset() noexcept {
        if (engaged_) destroy(alloc_, pointer_);
        pointer_ = pointer{};
        engaged_ = false;
    }

    template<class U, class OtherAllocator>
    friend class indirect;

public:
    explicit indirect()
        requires (is_default_constructible<Allocator>::value &&
                  is_default_constructible<T>::value)
        : alloc_(), pointer_(create(alloc_)), engaged_(true) {}

    explicit indirect(allocator_arg_t, const Allocator& alloc)
        requires is_default_constructible<T>::value
        : alloc_(alloc), pointer_(create(alloc_)), engaged_(true) {}

    indirect(const indirect& other)
        requires is_copy_constructible<T>::value
        : alloc_(allocator_traits<Allocator>::
                     select_on_container_copy_construction(other.alloc_)),
          pointer_(), engaged_(false) {
        if (other.engaged_) {
            pointer_ = create(alloc_, *std::to_address(other.pointer_));
            engaged_ = true;
        }
    }

    indirect(allocator_arg_t, const Allocator& alloc, const indirect& other)
        requires is_copy_constructible<T>::value
        : alloc_(alloc), pointer_(), engaged_(false) {
        if (other.engaged_) {
            pointer_ = create(alloc_, *std::to_address(other.pointer_));
            engaged_ = true;
        }
    }

    indirect(indirect&& other) noexcept
        : alloc_(std::move(other.alloc_)),
          pointer_(std::move(other.pointer_)), engaged_(other.engaged_) {
        other.pointer_ = pointer{};
        other.engaged_ = false;
    }

    indirect(allocator_arg_t, const Allocator& alloc, indirect&& other)
        noexcept(allocator_traits<Allocator>::is_always_equal::value)
        : alloc_(alloc), pointer_(), engaged_(false) {
        if (!other.engaged_) return;
        if (allocator_traits<Allocator>::is_always_equal::value ||
            alloc_ == other.alloc_) {
            pointer_ = std::move(other.pointer_);
            engaged_ = true;
            other.pointer_ = pointer{};
            other.engaged_ = false;
        } else {
            pointer_ = create(alloc_, std::move(*std::to_address(other.pointer_)));
            engaged_ = true;
            other.reset();
        }
    }

    template<class U = T>
    explicit indirect(U&& value)
        requires (!is_same<remove_cvref_t<U>, indirect>::value &&
                  !is_same<remove_cvref_t<U>, in_place_t>::value &&
                  is_constructible<T, U>::value &&
                  is_default_constructible<Allocator>::value)
        : alloc_(), pointer_(create(alloc_, std::forward<U>(value))),
          engaged_(true) {}

    template<class U = T>
    explicit indirect(allocator_arg_t, const Allocator& alloc, U&& value)
        requires (!is_same<remove_cvref_t<U>, indirect>::value &&
                  !is_same<remove_cvref_t<U>, in_place_t>::value &&
                  is_constructible<T, U>::value)
        : alloc_(alloc), pointer_(create(alloc_, std::forward<U>(value))),
          engaged_(true) {}

    template<class... Args>
    explicit indirect(in_place_t, Args&&... args)
        requires (is_constructible<T, Args...>::value &&
                  is_default_constructible<Allocator>::value)
        : alloc_(), pointer_(create(alloc_, std::forward<Args>(args)...)),
          engaged_(true) {}

    template<class... Args>
    explicit indirect(allocator_arg_t, const Allocator& alloc, in_place_t,
                       Args&&... args)
        requires is_constructible<T, Args...>::value
        : alloc_(alloc), pointer_(create(alloc_, std::forward<Args>(args)...)),
          engaged_(true) {}

    template<class I, class... Args>
    explicit indirect(in_place_t, initializer_list<I> values, Args&&... args)
        requires (is_constructible<T, initializer_list<I>&, Args...>::value &&
                  is_default_constructible<Allocator>::value)
        : alloc_(), pointer_(create(alloc_, values, std::forward<Args>(args)...)),
          engaged_(true) {}

    template<class I, class... Args>
    explicit indirect(allocator_arg_t, const Allocator& alloc, in_place_t,
                       initializer_list<I> values, Args&&... args)
        requires is_constructible<T, initializer_list<I>&, Args...>::value
        : alloc_(alloc),
          pointer_(create(alloc_, values, std::forward<Args>(args)...)),
          engaged_(true) {}

    ~indirect() {
        reset();
    }

    indirect& operator=(const indirect& other)
        requires (is_copy_constructible<T>::value &&
                  is_copy_assignable<T>::value) {
        if (this == &other) return *this;

        constexpr bool propagate =
            allocator_traits<Allocator>::
                propagate_on_container_copy_assignment::value;
        Allocator target_alloc = propagate ? other.alloc_ : alloc_;
        if (!other.engaged_) {
            reset();
            if constexpr (propagate) alloc_ = other.alloc_;
            return *this;
        }
        if (!propagate && engaged_ && alloc_ == other.alloc_) {
            *std::to_address(pointer_) = *std::to_address(other.pointer_);
            return *this;
        }

        pointer replacement = create(
            target_alloc, *std::to_address(other.pointer_));
        reset();
        if constexpr (propagate) alloc_ = other.alloc_;
        pointer_ = std::move(replacement);
        engaged_ = true;
        return *this;
    }

    indirect& operator=(indirect&& other)
        noexcept(allocator_traits<Allocator>::
                     propagate_on_container_move_assignment::value ||
                 allocator_traits<Allocator>::is_always_equal::value) {
        if (this == &other) return *this;
        constexpr bool propagate =
            allocator_traits<Allocator>::
                propagate_on_container_move_assignment::value;
        if constexpr (propagate) {
            reset();
            alloc_ = std::move(other.alloc_);
            pointer_ = std::move(other.pointer_);
            engaged_ = other.engaged_;
            other.pointer_ = pointer{};
            other.engaged_ = false;
        } else if (allocator_traits<Allocator>::is_always_equal::value ||
                   alloc_ == other.alloc_) {
            reset();
            pointer_ = std::move(other.pointer_);
            engaged_ = other.engaged_;
            other.pointer_ = pointer{};
            other.engaged_ = false;
        } else if (!other.engaged_) {
            reset();
        } else {
            pointer replacement = create(
                alloc_, std::move(*std::to_address(other.pointer_)));
            reset();
            pointer_ = std::move(replacement);
            engaged_ = true;
            other.reset();
        }
        return *this;
    }

    template<class U = T>
    indirect& operator=(U&& value)
        requires (!is_same<remove_cvref_t<U>, indirect>::value &&
                  is_constructible<T, U>::value &&
                  is_assignable<T&, U>::value) {
        if (engaged_) {
            *std::to_address(pointer_) = std::forward<U>(value);
        } else {
            pointer_ = create(alloc_, std::forward<U>(value));
            engaged_ = true;
        }
        return *this;
    }

    const T& operator*() const & noexcept { return *std::to_address(pointer_); }
    T& operator*() & noexcept { return *std::to_address(pointer_); }
    const T&& operator*() const && noexcept {
        return std::move(*std::to_address(pointer_));
    }
    T&& operator*() && noexcept { return std::move(*std::to_address(pointer_)); }
    const_pointer operator->() const noexcept {
        return pointer_traits<const_pointer>::pointer_to(*std::to_address(pointer_));
    }
    pointer operator->() noexcept { return pointer_; }
    bool valueless_after_move() const noexcept { return !engaged_; }
    allocator_type get_allocator() const noexcept { return alloc_; }

    void swap(indirect& other)
        noexcept(allocator_traits<Allocator>::
                     propagate_on_container_swap::value ||
                 allocator_traits<Allocator>::is_always_equal::value) {
        if (this == &other) return;
        if constexpr (allocator_traits<Allocator>::
                          propagate_on_container_swap::value) {
            using std::swap;
            swap(alloc_, other.alloc_);
        }
        using std::swap;
        swap(pointer_, other.pointer_);
        swap(engaged_, other.engaged_);
    }

    friend void swap(indirect& left, indirect& right)
        noexcept(noexcept(left.swap(right))) { left.swap(right); }

    template<class U, class OtherAllocator>
    friend constexpr bool operator==(const indirect& left,
                                     const indirect<U, OtherAllocator>& right)
        requires requires { { *left == *right } -> convertible_to<bool>; } {
        if (left.valueless_after_move() || right.valueless_after_move())
            return left.valueless_after_move() == right.valueless_after_move();
        return *left == *right;
    }

    template<class U, class OtherAllocator>
    friend constexpr auto operator<=>(const indirect& left,
                                      const indirect<U, OtherAllocator>& right)
        -> detail::synth_three_way_result_t<T, U>
        requires requires { detail::synth_three_way(*left, *right); } {
        if (left.valueless_after_move() || right.valueless_after_move())
            return !left.valueless_after_move() <=>
                   !right.valueless_after_move();
        return detail::synth_three_way(*left, *right);
    }

    template<class U,
             enable_if_t<!detail::is_indirect_wrapper<
                 remove_cvref_t<U>>::value, int> = 0>
    friend constexpr bool operator==(const indirect& left, const U& right)
        requires requires { { *left == right } -> convertible_to<bool>; } {
        return !left.valueless_after_move() && *left == right;
    }

    template<class U,
             enable_if_t<!detail::is_indirect_wrapper<
                 remove_cvref_t<U>>::value, int> = 0>
    friend constexpr auto operator<=>(const indirect& left, const U& right)
        -> detail::synth_three_way_result_t<T, U>
        requires requires { detail::synth_three_way(*left, right); } {
        if (left.valueless_after_move()) return strong_ordering::less;
        return detail::synth_three_way(*left, right);
    }
};

template<class Value>
indirect(Value) -> indirect<Value>;
template<class Allocator, class Value>
indirect(allocator_arg_t, Allocator, Value)
    -> indirect<Value, typename allocator_traits<Allocator>::template
                          rebind_alloc<Value>>;

template<class T, class Allocator = allocator<T>>
class polymorphic {
    static_assert(is_same<typename allocator_traits<Allocator>::value_type,
                          T>::value,
                  "polymorphic allocator value_type must match T");

public:
    using value_type = T;
    using allocator_type = Allocator;
    using pointer = typename allocator_traits<Allocator>::pointer;
    using const_pointer = typename allocator_traits<Allocator>::const_pointer;

private:
    struct operations {
        void* (*copy)(const Allocator&, const void*);
        void* (*move)(const Allocator&, void*);
        void (*destroy)(Allocator&, void*) noexcept;
    };

    Allocator alloc_;
    void* object_ = nullptr;
    const operations* operations_ = nullptr;

    template<class U, class... Args>
    static void* create(const Allocator& owner, Args&&... args) {
        using object_allocator = typename allocator_traits<Allocator>::
            template rebind_alloc<U>;
        using object_traits = allocator_traits<object_allocator>;
        using object_pointer = typename object_traits::pointer;
        Allocator construction_allocator(owner);
        object_allocator storage_allocator(owner);
        object_pointer storage = object_traits::allocate(storage_allocator, 1);
        U* raw = std::to_address(storage);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            allocator_traits<Allocator>::construct(
                construction_allocator, raw, std::forward<Args>(args)...);
        } catch (...) {
            object_traits::deallocate(storage_allocator, storage, 1);
            throw;
        }
#else
        allocator_traits<Allocator>::construct(
            construction_allocator, raw, std::forward<Args>(args)...);
#endif
        return raw;
    }

    template<class U>
    static void* copy_object(const Allocator& owner, const void* object) {
        return create<U>(owner, *static_cast<const U*>(object));
    }

    template<class U>
    static void* move_object(const Allocator& owner, void* object) {
        return create<U>(owner, std::move(*static_cast<U*>(object)));
    }

    template<class U>
    static void destroy_object(Allocator& owner, void* object) noexcept {
        using object_allocator = typename allocator_traits<Allocator>::
            template rebind_alloc<U>;
        using object_traits = allocator_traits<object_allocator>;
        using object_pointer = typename object_traits::pointer;
        U* raw = static_cast<U*>(object);
        object_allocator storage_allocator(owner);
        object_pointer storage = pointer_traits<object_pointer>::pointer_to(*raw);
        allocator_traits<Allocator>::destroy(owner, raw);
        object_traits::deallocate(storage_allocator, storage, 1);
    }

    template<class U>
    static const operations* table() noexcept {
        static const operations value = {
            &copy_object<U>, &move_object<U>, &destroy_object<U>};
        return &value;
    }

    template<class U, class... Args>
    void emplace(Args&&... args) {
        object_ = create<U>(alloc_, std::forward<Args>(args)...);
        operations_ = table<U>();
    }

    void reset() noexcept {
        if (object_) operations_->destroy(alloc_, object_);
        object_ = nullptr;
        operations_ = nullptr;
    }

    template<class U>
    static constexpr bool is_valid_object =
        is_base_of<T, U>::value && is_convertible<U*, T*>::value &&
        is_copy_constructible<U>::value;

public:
    explicit polymorphic()
        requires (is_default_constructible<Allocator>::value &&
                  is_default_constructible<T>::value &&
                  is_copy_constructible<T>::value)
        : alloc_() { emplace<T>(); }

    explicit polymorphic(allocator_arg_t, const Allocator& alloc)
        requires (is_default_constructible<T>::value &&
                  is_copy_constructible<T>::value)
        : alloc_(alloc) { emplace<T>(); }

    polymorphic(const polymorphic& other)
        : alloc_(allocator_traits<Allocator>::
                     select_on_container_copy_construction(other.alloc_)) {
        if (other.object_) {
            object_ = other.operations_->copy(alloc_, other.object_);
            operations_ = other.operations_;
        }
    }

    polymorphic(allocator_arg_t, const Allocator& alloc,
                const polymorphic& other)
        : alloc_(alloc) {
        if (other.object_) {
            object_ = other.operations_->copy(alloc_, other.object_);
            operations_ = other.operations_;
        }
    }

    polymorphic(polymorphic&& other) noexcept
        : alloc_(std::move(other.alloc_)), object_(other.object_),
          operations_(other.operations_) {
        other.object_ = nullptr;
        other.operations_ = nullptr;
    }

    polymorphic(allocator_arg_t, const Allocator& alloc,
                polymorphic&& other)
        noexcept(allocator_traits<Allocator>::is_always_equal::value)
        : alloc_(alloc) {
        if (!other.object_) return;
        if (allocator_traits<Allocator>::is_always_equal::value ||
            alloc_ == other.alloc_) {
            object_ = other.object_;
            operations_ = other.operations_;
            other.object_ = nullptr;
            other.operations_ = nullptr;
        } else {
            object_ = other.operations_->move(alloc_, other.object_);
            operations_ = other.operations_;
            other.reset();
        }
    }

    template<class U = T>
    explicit polymorphic(U&& value)
        requires (!is_same<remove_cvref_t<U>, polymorphic>::value &&
                  is_valid_object<remove_cvref_t<U>> &&
                  is_constructible<remove_cvref_t<U>, U>::value &&
                  is_default_constructible<Allocator>::value)
        : alloc_() { emplace<remove_cvref_t<U>>(std::forward<U>(value)); }

    template<class U = T>
    explicit polymorphic(allocator_arg_t, const Allocator& alloc, U&& value)
        requires (!is_same<remove_cvref_t<U>, polymorphic>::value &&
                  is_valid_object<remove_cvref_t<U>> &&
                  is_constructible<remove_cvref_t<U>, U>::value)
        : alloc_(alloc) { emplace<remove_cvref_t<U>>(std::forward<U>(value)); }

    template<class U, class... Args>
    explicit polymorphic(in_place_type_t<U>, Args&&... args)
        requires (is_same<remove_cvref_t<U>, U>::value &&
                  is_valid_object<U> && is_constructible<U, Args...>::value &&
                  is_default_constructible<Allocator>::value)
        : alloc_() { emplace<U>(std::forward<Args>(args)...); }

    template<class U, class... Args>
    explicit polymorphic(allocator_arg_t, const Allocator& alloc,
                         in_place_type_t<U>, Args&&... args)
        requires (is_same<remove_cvref_t<U>, U>::value &&
                  is_valid_object<U> && is_constructible<U, Args...>::value)
        : alloc_(alloc) { emplace<U>(std::forward<Args>(args)...); }

    template<class U, class I, class... Args>
    explicit polymorphic(in_place_type_t<U>, initializer_list<I> values,
                         Args&&... args)
        requires (is_same<remove_cvref_t<U>, U>::value &&
                  is_valid_object<U> &&
                  is_constructible<U, initializer_list<I>&, Args...>::value &&
                  is_default_constructible<Allocator>::value)
        : alloc_() { emplace<U>(values, std::forward<Args>(args)...); }

    template<class U, class I, class... Args>
    explicit polymorphic(allocator_arg_t, const Allocator& alloc,
                         in_place_type_t<U>, initializer_list<I> values,
                         Args&&... args)
        requires (is_same<remove_cvref_t<U>, U>::value &&
                  is_valid_object<U> &&
                  is_constructible<U, initializer_list<I>&, Args...>::value)
        : alloc_(alloc) { emplace<U>(values, std::forward<Args>(args)...); }

    ~polymorphic() { reset(); }

    polymorphic& operator=(const polymorphic& other) {
        if (this == &other) return *this;
        constexpr bool propagate =
            allocator_traits<Allocator>::
                propagate_on_container_copy_assignment::value;
        Allocator target_alloc = propagate ? other.alloc_ : alloc_;
        void* replacement = other.object_
            ? other.operations_->copy(target_alloc, other.object_) : nullptr;
        reset();
        if constexpr (propagate) alloc_ = other.alloc_;
        object_ = replacement;
        operations_ = other.object_ ? other.operations_ : nullptr;
        return *this;
    }

    polymorphic& operator=(polymorphic&& other)
        noexcept(allocator_traits<Allocator>::
                     propagate_on_container_move_assignment::value ||
                 allocator_traits<Allocator>::is_always_equal::value) {
        if (this == &other) return *this;
        constexpr bool propagate =
            allocator_traits<Allocator>::
                propagate_on_container_move_assignment::value;
        if constexpr (propagate) {
            reset();
            alloc_ = std::move(other.alloc_);
            object_ = other.object_;
            operations_ = other.operations_;
            other.object_ = nullptr;
            other.operations_ = nullptr;
        } else if (allocator_traits<Allocator>::is_always_equal::value ||
                   alloc_ == other.alloc_) {
            reset();
            object_ = other.object_;
            operations_ = other.operations_;
            other.object_ = nullptr;
            other.operations_ = nullptr;
        } else if (!other.object_) {
            reset();
        } else {
            void* replacement = other.operations_->move(alloc_, other.object_);
            reset();
            object_ = replacement;
            operations_ = other.operations_;
            other.reset();
        }
        return *this;
    }

    const T& operator*() const noexcept { return *static_cast<const T*>(object_); }
    T& operator*() noexcept { return *static_cast<T*>(object_); }
    const_pointer operator->() const noexcept {
        return pointer_traits<const_pointer>::pointer_to(
            *static_cast<const T*>(object_));
    }
    pointer operator->() noexcept {
        return pointer_traits<pointer>::pointer_to(*static_cast<T*>(object_));
    }
    bool valueless_after_move() const noexcept { return object_ == nullptr; }
    allocator_type get_allocator() const noexcept { return alloc_; }

    void swap(polymorphic& other)
        noexcept(allocator_traits<Allocator>::
                     propagate_on_container_swap::value ||
                 allocator_traits<Allocator>::is_always_equal::value) {
        if (this == &other) return;
        if constexpr (allocator_traits<Allocator>::
                          propagate_on_container_swap::value) {
            using std::swap;
            swap(alloc_, other.alloc_);
        }
        using std::swap;
        swap(object_, other.object_);
        swap(operations_, other.operations_);
    }

    friend void swap(polymorphic& left, polymorphic& right)
        noexcept(noexcept(left.swap(right))) { left.swap(right); }
};

} // namespace std
#endif // __cpp_lib_indirect

#endif // RINCXX_COMPOSITE_TYPES_H
