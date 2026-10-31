/*
 * RinOS C++ Expected ✿
 * C++23 expected / unexpected value-or-error transport.
 */

#ifndef RINCXX_EXPECTED_H
#define RINCXX_EXPECTED_H

#include "rincxx.h"
#include "version.h"

#if defined(RIN_LIBCXX_CXX23_DIALECT)

#include "compare.h"
#include "concepts.h"
#include "exception.h"
#include "functional.h"
#include "initializer_list.h"
#include "memory.h"
#include "type_traits.h"
#include "utility.h"

namespace std {

template<class T, class E>
class expected;

/* ═══════════════════════════════════════════════════════════════
 * unexpected and unexpect
 * ═══════════════════════════════════════════════════════════════*/

template<class E>
class unexpected {
    static_assert(is_object<E>::value && !is_array<E>::value &&
                      !is_function<E>::value &&
                      is_nothrow_destructible<E>::value,
                  "unexpected requires a nothrow-destructible object error");

    E error_;

public:
    using error_type = E;

    constexpr explicit unexpected(const E& error)
        requires (is_copy_constructible<E>::value)
        : error_(error) {}

    constexpr explicit unexpected(E&& error)
        requires (is_move_constructible<E>::value)
        : error_(std::move(error)) {}

    template<class... Args>
    constexpr explicit unexpected(in_place_t, Args&&... args)
        requires (is_constructible<E, Args...>::value)
        : error_(std::forward<Args>(args)...) {}

    template<class U, class... Args>
    constexpr explicit unexpected(in_place_t, initializer_list<U> values,
                                  Args&&... args)
        requires (is_constructible<E, initializer_list<U>&, Args...>::value)
        : error_(values, std::forward<Args>(args)...) {}

    constexpr unexpected(const unexpected&) = default;
    constexpr unexpected(unexpected&&) = default;
    constexpr unexpected& operator=(const unexpected&) = default;
    constexpr unexpected& operator=(unexpected&&) = default;

    template<class G>
    constexpr explicit(!is_convertible<const G&, E>::value)
    unexpected(const unexpected<G>& other)
        requires (!is_same<E, G>::value &&
                  is_constructible<E, const G&>::value)
        : error_(other.error()) {}

    template<class G>
    constexpr explicit(!is_convertible<G&&, E>::value)
    unexpected(unexpected<G>&& other)
        requires (!is_same<E, G>::value && is_constructible<E, G&&>::value)
        : error_(std::move(other).error()) {}

    constexpr const E& error() const & noexcept { return error_; }
    constexpr E& error() & noexcept { return error_; }
    constexpr const E&& error() const && noexcept {
        return std::move(error_);
    }
    constexpr E&& error() && noexcept { return std::move(error_); }

    constexpr void swap(unexpected& other)
        noexcept(is_nothrow_swappable<E>::value)
        requires (is_swappable<E>::value) {
        using std::swap;
        swap(error_, other.error_);
    }

    friend constexpr void swap(unexpected& lhs, unexpected& rhs)
        noexcept(noexcept(lhs.swap(rhs)))
        requires requires { lhs.swap(rhs); }
    {
        lhs.swap(rhs);
    }

    template<class G>
    friend constexpr bool operator==(const unexpected& lhs,
                                     const unexpected<G>& rhs)
        requires requires(const E& left, const G& right) {
            { left == right } -> convertible_to<bool>;
        }
    {
        return lhs.error_ == rhs.error();
    }
};

template<class E>
unexpected(E) -> unexpected<E>;

struct unexpect_t {
    explicit constexpr unexpect_t() = default;
};

inline constexpr unexpect_t unexpect{};

template<class E>
class bad_expected_access;

template<>
class bad_expected_access<void> : public exception {
public:
    bad_expected_access() noexcept = default;
    const char* what() const noexcept override {
        return "bad expected access";
    }
};

template<class E>
class bad_expected_access : public bad_expected_access<void> {
    E error_;

public:
    explicit bad_expected_access(E error) : error_(std::move(error)) {}

    E& error() & noexcept { return error_; }
    const E& error() const & noexcept { return error_; }
    E&& error() && noexcept { return std::move(error_); }
    const E&& error() const && noexcept { return std::move(error_); }
};

namespace detail {

template<class T>
struct expected_stored_value { using type = remove_cv_t<T>; };

template<>
struct expected_stored_value<void> { using type = unsigned char; };

template<class T>
using expected_stored_value_t = typename expected_stored_value<T>::type;

template<class T, class E, bool =
    is_trivially_destructible<expected_stored_value_t<T>>::value &&
    is_trivially_destructible<E>::value>
union expected_union;

template<class T, class E>
union expected_union<T, E, true> {
    unsigned char empty_;
    expected_stored_value_t<T> value_;
    E error_;

    constexpr expected_union() noexcept : empty_(0) {}
    constexpr ~expected_union() = default;
};

template<class T, class E>
union expected_union<T, E, false> {
    unsigned char empty_;
    expected_stored_value_t<T> value_;
    E error_;

    constexpr expected_union() noexcept : empty_(0) {}
    constexpr ~expected_union() {}
};

template<class T, class E, bool =
    is_trivially_destructible<expected_stored_value_t<T>>::value &&
    is_trivially_destructible<E>::value>
struct expected_storage;

template<class T, class E>
struct expected_storage<T, E, true> {
    expected_union<T, E, true> data_;
    bool has_value_;
    bool active_;

    constexpr expected_storage() noexcept
        : data_(), has_value_(false), active_(false) {}
    constexpr ~expected_storage() = default;
};

template<class T, class E>
struct expected_storage<T, E, false> {
    expected_union<T, E, false> data_;
    bool has_value_;
    bool active_;

    constexpr expected_storage() noexcept
        : data_(), has_value_(false), active_(false) {}

    constexpr ~expected_storage() {
        if (!active_) return;
        if (has_value_) {
            destroy_at(addressof(data_.value_));
        } else {
            destroy_at(addressof(data_.error_));
        }
    }
};

template<class T, class E>
struct is_expected : false_type {};

template<class T, class E>
struct is_expected<expected<T, E>, void> : true_type {};

template<class T>
struct is_unexpected : false_type {};

template<class E>
struct is_unexpected<unexpected<E>> : true_type {};

template<class T>
struct expected_traits;

template<class T, class E>
struct expected_traits<expected<T, E>> {
    using value_type = T;
    using error_type = E;
};

template<class Self, class F, bool = is_void<
    typename expected_traits<remove_cvref_t<Self>>::value_type>::value>
struct expected_invoke_result;

template<class Self, class F>
struct expected_invoke_result<Self, F, true> {
    using type = invoke_result_t<F>;
};

template<class Self, class F>
struct expected_invoke_result<Self, F, false> {
    using type = invoke_result_t<
        F, decltype(*declval<Self>())>;
};

template<class New, class Old, class... Args>
constexpr void expected_reinit(New* new_value, Old* old_value,
                               Args&&... args) {
    if constexpr (is_nothrow_constructible<New, Args...>::value) {
        destroy_at(old_value);
        construct_at(new_value, std::forward<Args>(args)...);
    } else if constexpr (is_nothrow_move_constructible<New>::value) {
        New temporary(std::forward<Args>(args)...);
        destroy_at(old_value);
        construct_at(new_value, std::move(temporary));
    } else {
        static_assert(is_nothrow_move_constructible<Old>::value,
                      "expected alternative transition needs a nothrow move");
        Old temporary(std::move(*old_value));
        destroy_at(old_value);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            construct_at(new_value, std::forward<Args>(args)...);
        } catch (...) {
            construct_at(old_value, std::move(temporary));
            throw;
        }
#else
        construct_at(new_value, std::forward<Args>(args)...);
#endif
    }
}

template<class E>
[[noreturn]] inline void expected_access_fail(const E& error) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw bad_expected_access<E>(error);
#else
    (void)error;
    __builtin_trap();
#endif
}

template<class E>
[[noreturn]] inline void expected_access_fail(E&& error) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw bad_expected_access<E>(std::forward<E>(error));
#else
    (void)error;
    __builtin_trap();
#endif
}

template<class Self, class F>
constexpr auto expected_and_then(Self&& self, F&& function) {
    using self_type = remove_cvref_t<Self>;
    using value_type = typename expected_traits<self_type>::value_type;
    using result_type = remove_cvref_t<
        typename expected_invoke_result<Self, F>::type>;
    static_assert(is_expected<result_type, void>::value,
                  "expected::and_then callable must return expected");
    static_assert(is_same<typename expected_traits<result_type>::error_type,
                          typename expected_traits<self_type>::error_type>::value,
                  "expected::and_then must preserve the error type");
    if (self.has_value()) {
        if constexpr (is_void<value_type>::value) {
            return std::invoke(std::forward<F>(function));
        } else {
            return std::invoke(std::forward<F>(function),
                               *std::forward<Self>(self));
        }
    }
    return result_type(unexpect,
                       std::forward<Self>(self).error());
}

template<class Self, class F>
constexpr auto expected_or_else(Self&& self, F&& function) {
    using self_type = remove_cvref_t<Self>;
    using value_type = typename expected_traits<self_type>::value_type;
    using result_type = remove_cvref_t<invoke_result_t<
        F, decltype(std::forward<Self>(self).error())>>;
    static_assert(is_expected<result_type, void>::value,
                  "expected::or_else callable must return expected");
    static_assert(is_same<typename expected_traits<result_type>::value_type,
                          value_type>::value,
                  "expected::or_else must preserve the value type");
    if (self.has_value()) {
        if constexpr (is_void<value_type>::value) {
            return result_type(in_place);
        } else {
            return result_type(in_place, *std::forward<Self>(self));
        }
    }
    return std::invoke(std::forward<F>(function),
                       std::forward<Self>(self).error());
}

template<class Self, class F>
constexpr auto expected_transform(Self&& self, F&& function) {
    using self_type = remove_cvref_t<Self>;
    using value_type = typename expected_traits<self_type>::value_type;
    using mapped_type = remove_cv_t<
        typename expected_invoke_result<Self, F>::type>;
    using error_type = typename expected_traits<self_type>::error_type;
    using result_type = expected<mapped_type, error_type>;
    if (!self.has_value()) {
        return result_type(unexpect,
                           std::forward<Self>(self).error());
    }
    if constexpr (is_void<value_type>::value) {
        if constexpr (is_void<mapped_type>::value) {
            std::invoke(std::forward<F>(function));
            return result_type();
        } else {
            return result_type(in_place,
                               std::invoke(std::forward<F>(function)));
        }
    } else if constexpr (is_void<mapped_type>::value) {
        std::invoke(std::forward<F>(function),
                    *std::forward<Self>(self));
        return result_type();
    } else {
        return result_type(in_place,
            std::invoke(std::forward<F>(function),
                        *std::forward<Self>(self)));
    }
}

template<class Self, class F>
constexpr auto expected_transform_error(Self&& self, F&& function) {
    using self_type = remove_cvref_t<Self>;
    using value_type = typename expected_traits<self_type>::value_type;
    using error_type = remove_cv_t<invoke_result_t<
        F, decltype(std::forward<Self>(self).error())>>;
    using result_type = expected<value_type, error_type>;
    if (self.has_value()) {
        if constexpr (is_void<value_type>::value) {
            return result_type(in_place);
        } else {
            return result_type(in_place, *std::forward<Self>(self));
        }
    }
    return result_type(unexpect,
        std::invoke(std::forward<F>(function),
                    std::forward<Self>(self).error()));
}

} /* namespace detail */

/* ═══════════════════════════════════════════════════════════════
 * expected<T, E>
 * ═══════════════════════════════════════════════════════════════*/

template<class T, class E>
class expected : private detail::expected_storage<T, E> {
    static_assert(!is_void<T>::value && is_object<T>::value &&
                      !is_array<T>::value && !is_function<T>::value &&
                      is_nothrow_destructible<T>::value &&
                      !is_same<remove_cv_t<T>, in_place_t>::value &&
                      !is_same<remove_cv_t<T>, unexpect_t>::value,
                  "expected value type must be a nothrow-destructible object");
    static_assert(is_object<E>::value && !is_array<E>::value &&
                      !is_function<E>::value &&
                      is_nothrow_destructible<E>::value,
                  "expected error type must be a nothrow-destructible object");

    using base = detail::expected_storage<T, E>;
    using stored_value_type = detail::expected_stored_value_t<T>;

    template<class U, class G>
    friend class expected;

    template<class Source>
    constexpr void construct_from(Source&& source) {
        if (source.has_value()) {
            construct_at(addressof(this->data_.value_),
                         *std::forward<Source>(source));
            this->has_value_ = true;
        } else {
            construct_at(addressof(this->data_.error_),
                         std::forward<Source>(source).error());
            this->has_value_ = false;
        }
        this->active_ = true;
    }

    template<class Source>
    constexpr void assign_from(Source&& source) {
        if (this->has_value_ == source.has_value()) {
            if (this->has_value_) {
                this->data_.value_ = *std::forward<Source>(source);
            } else {
                this->data_.error_ = std::forward<Source>(source).error();
            }
        } else if (source.has_value()) {
            detail::expected_reinit(
                addressof(this->data_.value_),
                addressof(this->data_.error_),
                *std::forward<Source>(source));
            this->has_value_ = true;
        } else {
            detail::expected_reinit(
                addressof(this->data_.error_),
                addressof(this->data_.value_),
                std::forward<Source>(source).error());
            this->has_value_ = false;
        }
    }

public:
    using value_type = T;
    using error_type = E;
    using unexpected_type = unexpected<E>;
    template<class U> using rebind = expected<U, E>;

    constexpr expected()
        requires (is_default_constructible<T>::value)
        : base() {
        construct_at(addressof(this->data_.value_));
        this->has_value_ = true;
        this->active_ = true;
    }

    constexpr expected(const expected& other)
        requires (is_copy_constructible<T>::value &&
                  is_copy_constructible<E>::value)
        : base() {
        construct_from(other);
    }

    constexpr expected(const expected&)
        requires (!(is_copy_constructible<T>::value &&
                    is_copy_constructible<E>::value)) = delete;

    constexpr expected(expected&& other)
        noexcept(is_nothrow_move_constructible<T>::value &&
                 is_nothrow_move_constructible<E>::value)
        requires (is_move_constructible<T>::value &&
                  is_move_constructible<E>::value)
        : base() {
        construct_from(std::move(other));
    }

    constexpr expected(expected&&)
        requires (!(is_move_constructible<T>::value &&
                    is_move_constructible<E>::value)) = delete;

    template<class U, class G>
    constexpr explicit(!(is_convertible<const U&, T>::value &&
                         is_convertible<const G&, E>::value))
    expected(const expected<U, G>& other)
        requires (!is_same<expected, expected<U, G>>::value &&
                  !is_void<U>::value &&
                  is_constructible<T, const U&>::value &&
                  is_constructible<E, const G&>::value)
        : base() {
        construct_from(other);
    }

    template<class U, class G>
    constexpr explicit(!(is_convertible<U&&, T>::value &&
                         is_convertible<G&&, E>::value))
    expected(expected<U, G>&& other)
        requires (!is_same<expected, expected<U, G>>::value &&
                  !is_void<U>::value &&
                  is_constructible<T, U&&>::value &&
                  is_constructible<E, G&&>::value)
        : base() {
        construct_from(std::move(other));
    }

    template<class U = remove_cv_t<T>>
    constexpr explicit(!is_convertible<U&&, T>::value)
    expected(U&& value)
        requires (!detail::is_expected<remove_cvref_t<U>, void>::value &&
                  !detail::is_unexpected<remove_cvref_t<U>>::value &&
                  !is_same<remove_cvref_t<U>, in_place_t>::value &&
                  !is_same<remove_cvref_t<U>, unexpect_t>::value &&
                  is_constructible<T, U&&>::value)
        : base() {
        construct_at(addressof(this->data_.value_),
                     std::forward<U>(value));
        this->has_value_ = true;
        this->active_ = true;
    }

    template<class G>
    constexpr explicit(!is_convertible<const G&, E>::value)
    expected(const unexpected<G>& error)
        requires (is_constructible<E, const G&>::value)
        : base() {
        construct_at(addressof(this->data_.error_), error.error());
        this->has_value_ = false;
        this->active_ = true;
    }

    template<class G>
    constexpr explicit(!is_convertible<G&&, E>::value)
    expected(unexpected<G>&& error)
        requires (is_constructible<E, G&&>::value)
        : base() {
        construct_at(addressof(this->data_.error_), std::move(error).error());
        this->has_value_ = false;
        this->active_ = true;
    }

    template<class... Args>
    constexpr explicit expected(in_place_t, Args&&... args)
        requires (is_constructible<T, Args...>::value)
        : base() {
        construct_at(addressof(this->data_.value_),
                     std::forward<Args>(args)...);
        this->has_value_ = true;
        this->active_ = true;
    }

    template<class U, class... Args>
    constexpr explicit expected(in_place_t, initializer_list<U> values,
                                Args&&... args)
        requires (is_constructible<T,
                                   initializer_list<U>&, Args...>::value)
        : base() {
        construct_at(addressof(this->data_.value_), values,
                     std::forward<Args>(args)...);
        this->has_value_ = true;
        this->active_ = true;
    }

    template<class... Args>
    constexpr explicit expected(unexpect_t, Args&&... args)
        requires (is_constructible<E, Args...>::value)
        : base() {
        construct_at(addressof(this->data_.error_),
                     std::forward<Args>(args)...);
        this->has_value_ = false;
        this->active_ = true;
    }

    template<class U, class... Args>
    constexpr explicit expected(unexpect_t, initializer_list<U> values,
                                 Args&&... args)
        requires (is_constructible<E, initializer_list<U>&, Args...>::value)
        : base() {
        construct_at(addressof(this->data_.error_), values,
                     std::forward<Args>(args)...);
        this->has_value_ = false;
        this->active_ = true;
    }

    constexpr ~expected() = default;

    constexpr expected& operator=(const expected& other)
        requires (is_copy_constructible<T>::value &&
                  is_copy_assignable<T>::value &&
                  is_copy_constructible<E>::value &&
                  is_copy_assignable<E>::value &&
                  (is_nothrow_move_constructible<T>::value ||
                   is_nothrow_move_constructible<E>::value)) {
        assign_from(other);
        return *this;
    }

    constexpr expected& operator=(const expected&)
        requires (!(is_copy_constructible<T>::value &&
                    is_copy_assignable<T>::value &&
                    is_copy_constructible<E>::value &&
                    is_copy_assignable<E>::value &&
                    (is_nothrow_move_constructible<T>::value ||
                     is_nothrow_move_constructible<E>::value))) = delete;

    constexpr expected& operator=(expected&& other)
        noexcept(is_nothrow_move_constructible<T>::value &&
                 is_nothrow_move_assignable<T>::value &&
                 is_nothrow_move_constructible<E>::value &&
                 is_nothrow_move_assignable<E>::value)
        requires (is_move_constructible<T>::value &&
                  is_move_assignable<T>::value &&
                  is_move_constructible<E>::value &&
                  is_move_assignable<E>::value &&
                  (is_nothrow_move_constructible<T>::value ||
                   is_nothrow_move_constructible<E>::value)) {
        assign_from(std::move(other));
        return *this;
    }

    constexpr expected& operator=(expected&&)
        requires (!(is_move_constructible<T>::value &&
                    is_move_assignable<T>::value &&
                    is_move_constructible<E>::value &&
                    is_move_assignable<E>::value &&
                    (is_nothrow_move_constructible<T>::value ||
                     is_nothrow_move_constructible<E>::value))) = delete;

    template<class U = remove_cv_t<T>>
    constexpr expected& operator=(U&& value)
        requires (!detail::is_expected<remove_cvref_t<U>, void>::value &&
                  !detail::is_unexpected<remove_cvref_t<U>>::value &&
                  is_constructible<T, U&&>::value &&
                  is_assignable<T&, U&&>::value &&
                  (is_nothrow_constructible<T, U&&>::value ||
                   is_nothrow_move_constructible<stored_value_type>::value ||
                   is_nothrow_move_constructible<E>::value)) {
        if (this->has_value_) {
            this->data_.value_ = std::forward<U>(value);
        } else {
            detail::expected_reinit(
                addressof(this->data_.value_),
                addressof(this->data_.error_), std::forward<U>(value));
            this->has_value_ = true;
        }
        return *this;
    }

    template<class G>
    constexpr expected& operator=(const unexpected<G>& error)
        requires (is_constructible<E, const G&>::value &&
                  is_assignable<E&, const G&>::value &&
                  (is_nothrow_constructible<E, const G&>::value ||
                   is_nothrow_move_constructible<stored_value_type>::value ||
                   is_nothrow_move_constructible<E>::value)) {
        if (this->has_value_) {
            detail::expected_reinit(
                addressof(this->data_.error_),
                addressof(this->data_.value_), error.error());
            this->has_value_ = false;
        } else {
            this->data_.error_ = error.error();
        }
        return *this;
    }

    template<class G>
    constexpr expected& operator=(unexpected<G>&& error)
        requires (is_constructible<E, G&&>::value &&
                  is_assignable<E&, G&&>::value &&
                  (is_nothrow_constructible<E, G&&>::value ||
                   is_nothrow_move_constructible<stored_value_type>::value ||
                   is_nothrow_move_constructible<E>::value)) {
        if (this->has_value_) {
            detail::expected_reinit(
                addressof(this->data_.error_),
                addressof(this->data_.value_), std::move(error).error());
            this->has_value_ = false;
        } else {
            this->data_.error_ = std::move(error).error();
        }
        return *this;
    }

    template<class... Args>
    constexpr T& emplace(Args&&... args) noexcept
        requires (is_nothrow_constructible<T, Args...>::value) {
        if (this->has_value_) {
            destroy_at(addressof(this->data_.value_));
        } else {
            destroy_at(addressof(this->data_.error_));
        }
        construct_at(addressof(this->data_.value_),
                     std::forward<Args>(args)...);
        this->has_value_ = true;
        return this->data_.value_;
    }

    template<class U, class... Args>
    constexpr T& emplace(initializer_list<U> values, Args&&... args) noexcept
        requires (is_nothrow_constructible<T,
                                           initializer_list<U>&,
                                           Args...>::value) {
        if (this->has_value_) {
            destroy_at(addressof(this->data_.value_));
        } else {
            destroy_at(addressof(this->data_.error_));
        }
        construct_at(addressof(this->data_.value_), values,
                     std::forward<Args>(args)...);
        this->has_value_ = true;
        return this->data_.value_;
    }

    constexpr void swap(expected& other)
        noexcept(is_nothrow_move_constructible<T>::value &&
                 is_nothrow_swappable<T>::value &&
                 is_nothrow_move_constructible<E>::value &&
                 is_nothrow_swappable<E>::value)
        requires (is_swappable<T>::value &&
                  is_swappable<E>::value &&
                  is_move_constructible<T>::value &&
                  is_move_constructible<E>::value &&
                  (is_nothrow_move_constructible<T>::value ||
                   is_nothrow_move_constructible<E>::value)) {
        if (this->has_value_ == other.has_value_) {
            if (this->has_value_) {
                using std::swap;
                swap(this->data_.value_, other.data_.value_);
            } else {
                using std::swap;
                swap(this->data_.error_, other.data_.error_);
            }
            return;
        }
        if (this->has_value_) {
            other.swap(*this);
            return;
        }

        if constexpr (is_nothrow_move_constructible<E>::value) {
            E temporary(std::move(this->data_.error_));
            destroy_at(addressof(this->data_.error_));
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            if constexpr (is_nothrow_move_constructible<T>::value) {
                construct_at(addressof(this->data_.value_),
                             std::move(other.data_.value_));
                destroy_at(addressof(other.data_.value_));
                construct_at(addressof(other.data_.error_),
                             std::move(temporary));
            } else {
                try {
                    construct_at(addressof(this->data_.value_),
                                 std::move(other.data_.value_));
                    destroy_at(addressof(other.data_.value_));
                    construct_at(addressof(other.data_.error_),
                                 std::move(temporary));
                } catch (...) {
                    construct_at(addressof(this->data_.error_),
                                 std::move(temporary));
                    throw;
                }
            }
#else
            construct_at(addressof(this->data_.value_),
                         std::move(other.data_.value_));
            destroy_at(addressof(other.data_.value_));
            construct_at(addressof(other.data_.error_),
                         std::move(temporary));
#endif
        } else {
            stored_value_type temporary(std::move(other.data_.value_));
            destroy_at(addressof(other.data_.value_));
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            try {
#endif
                construct_at(addressof(other.data_.error_),
                             std::move(this->data_.error_));
                destroy_at(addressof(this->data_.error_));
                construct_at(addressof(this->data_.value_),
                             std::move(temporary));
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            } catch (...) {
                construct_at(addressof(other.data_.value_),
                             std::move(temporary));
                throw;
            }
#endif
        }
        this->has_value_ = true;
        other.has_value_ = false;
    }

    friend constexpr void swap(expected& lhs, expected& rhs)
        noexcept(noexcept(lhs.swap(rhs)))
        requires requires { lhs.swap(rhs); }
    {
        lhs.swap(rhs);
    }

    constexpr T* operator->() noexcept {
        return addressof(this->data_.value_);
    }
    constexpr const T* operator->() const noexcept {
        return addressof(this->data_.value_);
    }
    constexpr T& operator*() & noexcept { return this->data_.value_; }
    constexpr const T& operator*() const & noexcept {
        return this->data_.value_;
    }
    constexpr T&& operator*() && noexcept {
        return std::move(this->data_.value_);
    }
    constexpr const T&& operator*() const && noexcept {
        return std::move(this->data_.value_);
    }

    constexpr explicit operator bool() const noexcept {
        return this->has_value_;
    }
    constexpr bool has_value() const noexcept { return this->has_value_; }

    constexpr T& value() & {
        if (!this->has_value_)
            detail::expected_access_fail<E>(this->data_.error_);
        return this->data_.value_;
    }
    constexpr const T& value() const & {
        if (!this->has_value_)
            detail::expected_access_fail<E>(this->data_.error_);
        return this->data_.value_;
    }
    constexpr T&& value() && {
        if (!this->has_value_)
            detail::expected_access_fail<E>(std::move(this->data_.error_));
        return std::move(this->data_.value_);
    }
    constexpr const T&& value() const && {
        if (!this->has_value_)
            detail::expected_access_fail<E>(std::move(this->data_.error_));
        return std::move(this->data_.value_);
    }

    constexpr E& error() & noexcept { return this->data_.error_; }
    constexpr const E& error() const & noexcept { return this->data_.error_; }
    constexpr E&& error() && noexcept {
        return std::move(this->data_.error_);
    }
    constexpr const E&& error() const && noexcept {
        return std::move(this->data_.error_);
    }

    template<class U = remove_cv_t<T>>
    constexpr T value_or(U&& fallback) const &
        requires (is_copy_constructible<T>::value &&
                  is_convertible<U&&, T>::value) {
        return this->has_value_ ? this->data_.value_
                                : static_cast<T>(std::forward<U>(fallback));
    }

    template<class U = remove_cv_t<T>>
    constexpr T value_or(U&& fallback) &&
        requires (is_move_constructible<T>::value &&
                  is_convertible<U&&, T>::value) {
        return this->has_value_ ? std::move(this->data_.value_)
                                : static_cast<T>(std::forward<U>(fallback));
    }

    template<class F> constexpr auto and_then(F&& function) & {
        return detail::expected_and_then(*this, std::forward<F>(function));
    }
    template<class F> constexpr auto and_then(F&& function) const & {
        return detail::expected_and_then(*this, std::forward<F>(function));
    }
    template<class F> constexpr auto and_then(F&& function) && {
        return detail::expected_and_then(std::move(*this),
                                         std::forward<F>(function));
    }
    template<class F> constexpr auto and_then(F&& function) const && {
        return detail::expected_and_then(std::move(*this),
                                         std::forward<F>(function));
    }
    template<class F> constexpr auto or_else(F&& function) & {
        return detail::expected_or_else(*this, std::forward<F>(function));
    }
    template<class F> constexpr auto or_else(F&& function) const & {
        return detail::expected_or_else(*this, std::forward<F>(function));
    }
    template<class F> constexpr auto or_else(F&& function) && {
        return detail::expected_or_else(std::move(*this),
                                        std::forward<F>(function));
    }
    template<class F> constexpr auto or_else(F&& function) const && {
        return detail::expected_or_else(std::move(*this),
                                        std::forward<F>(function));
    }
    template<class F> constexpr auto transform(F&& function) & {
        return detail::expected_transform(*this, std::forward<F>(function));
    }
    template<class F> constexpr auto transform(F&& function) const & {
        return detail::expected_transform(*this, std::forward<F>(function));
    }
    template<class F> constexpr auto transform(F&& function) && {
        return detail::expected_transform(std::move(*this),
                                          std::forward<F>(function));
    }
    template<class F> constexpr auto transform(F&& function) const && {
        return detail::expected_transform(std::move(*this),
                                          std::forward<F>(function));
    }
    template<class F> constexpr auto transform_error(F&& function) & {
        return detail::expected_transform_error(*this,
                                                std::forward<F>(function));
    }
    template<class F> constexpr auto transform_error(F&& function) const & {
        return detail::expected_transform_error(*this,
                                                std::forward<F>(function));
    }
    template<class F> constexpr auto transform_error(F&& function) && {
        return detail::expected_transform_error(std::move(*this),
                                                std::forward<F>(function));
    }
    template<class F> constexpr auto transform_error(F&& function) const && {
        return detail::expected_transform_error(std::move(*this),
                                                std::forward<F>(function));
    }
};

/* ═══════════════════════════════════════════════════════════════
 * expected<void, E>
 * ═══════════════════════════════════════════════════════════════*/

template<class E>
class expected<void, E> : private detail::expected_storage<void, E> {
    static_assert(is_object<E>::value && !is_array<E>::value &&
                      !is_function<E>::value &&
                      is_nothrow_destructible<E>::value,
                  "expected error type must be a nothrow-destructible object");

    using base = detail::expected_storage<void, E>;
    template<class, class> friend class expected;

    constexpr void construct_value() noexcept {
        construct_at(addressof(this->data_.value_));
        this->has_value_ = true;
        this->active_ = true;
    }

    template<class Source>
    constexpr void construct_from(Source&& source) {
        if (source.has_value()) {
            construct_value();
        } else {
            construct_at(addressof(this->data_.error_),
                         std::forward<Source>(source).error());
            this->has_value_ = false;
            this->active_ = true;
        }
    }

    template<class Source>
    constexpr void assign_from(Source&& source) {
        if (this->has_value_ == source.has_value()) {
            if (!this->has_value_)
                this->data_.error_ = std::forward<Source>(source).error();
            return;
        }
        if (source.has_value()) {
            destroy_at(addressof(this->data_.error_));
            construct_at(addressof(this->data_.value_));
            this->has_value_ = true;
        } else {
            detail::expected_reinit(
                addressof(this->data_.error_),
                addressof(this->data_.value_),
                std::forward<Source>(source).error());
            this->has_value_ = false;
        }
    }

public:
    using value_type = void;
    using error_type = E;
    using unexpected_type = unexpected<E>;
    template<class U> using rebind = expected<U, E>;

    constexpr expected() noexcept : base() { construct_value(); }
    constexpr expected(in_place_t) noexcept : expected() {}

    constexpr expected(const expected& other)
        requires (is_copy_constructible<E>::value)
        : base() { construct_from(other); }
    constexpr expected(const expected&)
        requires (!is_copy_constructible<E>::value) = delete;

    constexpr expected(expected&& other)
        noexcept(is_nothrow_move_constructible<E>::value)
        requires (is_move_constructible<E>::value)
        : base() { construct_from(std::move(other)); }
    constexpr expected(expected&&)
        requires (!is_move_constructible<E>::value) = delete;

    template<class U, class G>
    constexpr explicit(!is_convertible<const G&, E>::value)
    expected(const expected<U, G>& other)
        requires (!is_same<expected, expected<U, G>>::value &&
                  is_constructible<E, const G&>::value)
        : base() { construct_from(other); }

    template<class U, class G>
    constexpr explicit(!is_convertible<G&&, E>::value)
    expected(expected<U, G>&& other)
        requires (!is_same<expected, expected<U, G>>::value &&
                  is_constructible<E, G&&>::value)
        : base() { construct_from(std::move(other)); }

    template<class G>
    constexpr explicit(!is_convertible<const G&, E>::value)
    expected(const unexpected<G>& error)
        requires (is_constructible<E, const G&>::value)
        : base() {
        construct_at(addressof(this->data_.error_), error.error());
        this->has_value_ = false;
        this->active_ = true;
    }

    template<class G>
    constexpr explicit(!is_convertible<G&&, E>::value)
    expected(unexpected<G>&& error)
        requires (is_constructible<E, G&&>::value)
        : base() {
        construct_at(addressof(this->data_.error_), std::move(error).error());
        this->has_value_ = false;
        this->active_ = true;
    }

    template<class... Args>
    constexpr explicit expected(unexpect_t, Args&&... args)
        requires (is_constructible<E, Args...>::value)
        : base() {
        construct_at(addressof(this->data_.error_),
                     std::forward<Args>(args)...);
        this->has_value_ = false;
        this->active_ = true;
    }

    template<class U, class... Args>
    constexpr explicit expected(unexpect_t, initializer_list<U> values,
                                 Args&&... args)
        requires (is_constructible<E, initializer_list<U>&, Args...>::value)
        : base() {
        construct_at(addressof(this->data_.error_), values,
                     std::forward<Args>(args)...);
        this->has_value_ = false;
        this->active_ = true;
    }

    constexpr ~expected() = default;

    constexpr expected& operator=(const expected& other)
        requires (is_copy_constructible<E>::value &&
                  is_copy_assignable<E>::value) {
        assign_from(other);
        return *this;
    }
    constexpr expected& operator=(const expected&)
        requires (!(is_copy_constructible<E>::value &&
                    is_copy_assignable<E>::value)) = delete;

    constexpr expected& operator=(expected&& other)
        noexcept(is_nothrow_move_constructible<E>::value &&
                 is_nothrow_move_assignable<E>::value)
        requires (is_move_constructible<E>::value &&
                  is_move_assignable<E>::value) {
        assign_from(std::move(other));
        return *this;
    }
    constexpr expected& operator=(expected&&)
        requires (!(is_move_constructible<E>::value &&
                    is_move_assignable<E>::value)) = delete;

    template<class G>
    constexpr expected& operator=(const unexpected<G>& error)
        requires (is_constructible<E, const G&>::value &&
                  is_assignable<E&, const G&>::value) {
        if (this->has_value_) {
            detail::expected_reinit(
                addressof(this->data_.error_),
                addressof(this->data_.value_), error.error());
            this->has_value_ = false;
        } else {
            this->data_.error_ = error.error();
        }
        return *this;
    }

    template<class G>
    constexpr expected& operator=(unexpected<G>&& error)
        requires (is_constructible<E, G&&>::value &&
                  is_assignable<E&, G&&>::value) {
        if (this->has_value_) {
            detail::expected_reinit(
                addressof(this->data_.error_),
                addressof(this->data_.value_), std::move(error).error());
            this->has_value_ = false;
        } else {
            this->data_.error_ = std::move(error).error();
        }
        return *this;
    }

    constexpr void emplace() noexcept {
        if (!this->has_value_)
            destroy_at(addressof(this->data_.error_));
        construct_at(addressof(this->data_.value_));
        this->has_value_ = true;
    }

    constexpr void swap(expected& other)
        noexcept(is_nothrow_move_constructible<E>::value &&
                 is_nothrow_swappable<E>::value)
        requires (is_move_constructible<E>::value && is_swappable<E>::value) {
        if (this->has_value_ == other.has_value_) {
            if (!this->has_value_) {
                using std::swap;
                swap(this->data_.error_, other.data_.error_);
            }
            return;
        }
        if (this->has_value_) {
            other.swap(*this);
            return;
        }
        destroy_at(addressof(other.data_.value_));
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        if constexpr (is_nothrow_move_constructible<E>::value) {
            construct_at(addressof(other.data_.error_),
                         std::move(this->data_.error_));
        } else {
            try {
                construct_at(addressof(other.data_.error_),
                             std::move(this->data_.error_));
            } catch (...) {
                construct_at(addressof(other.data_.value_));
                throw;
            }
        }
#else
        construct_at(addressof(other.data_.error_),
                     std::move(this->data_.error_));
#endif
        destroy_at(addressof(this->data_.error_));
        this->has_value_ = true;
        other.has_value_ = false;
    }

    friend constexpr void swap(expected& lhs, expected& rhs)
        noexcept(noexcept(lhs.swap(rhs)))
        requires requires { lhs.swap(rhs); }
    { lhs.swap(rhs); }

    constexpr explicit operator bool() const noexcept {
        return this->has_value_;
    }
    constexpr bool has_value() const noexcept { return this->has_value_; }
    constexpr void operator*() const noexcept {}

    constexpr void value() const & {
        if (!this->has_value_)
            detail::expected_access_fail<E>(this->data_.error_);
    }
    constexpr void value() && {
        if (!this->has_value_)
            detail::expected_access_fail<E>(std::move(this->data_.error_));
    }

    constexpr E& error() & noexcept { return this->data_.error_; }
    constexpr const E& error() const & noexcept { return this->data_.error_; }
    constexpr E&& error() && noexcept {
        return std::move(this->data_.error_);
    }
    constexpr const E&& error() const && noexcept {
        return std::move(this->data_.error_);
    }

    template<class F> constexpr auto and_then(F&& function) & {
        return detail::expected_and_then(*this, std::forward<F>(function));
    }
    template<class F> constexpr auto and_then(F&& function) const & {
        return detail::expected_and_then(*this, std::forward<F>(function));
    }
    template<class F> constexpr auto and_then(F&& function) && {
        return detail::expected_and_then(std::move(*this),
                                         std::forward<F>(function));
    }
    template<class F> constexpr auto and_then(F&& function) const && {
        return detail::expected_and_then(std::move(*this),
                                         std::forward<F>(function));
    }
    template<class F> constexpr auto or_else(F&& function) & {
        return detail::expected_or_else(*this, std::forward<F>(function));
    }
    template<class F> constexpr auto or_else(F&& function) const & {
        return detail::expected_or_else(*this, std::forward<F>(function));
    }
    template<class F> constexpr auto or_else(F&& function) && {
        return detail::expected_or_else(std::move(*this),
                                        std::forward<F>(function));
    }
    template<class F> constexpr auto or_else(F&& function) const && {
        return detail::expected_or_else(std::move(*this),
                                        std::forward<F>(function));
    }
    template<class F> constexpr auto transform(F&& function) & {
        return detail::expected_transform(*this, std::forward<F>(function));
    }
    template<class F> constexpr auto transform(F&& function) const & {
        return detail::expected_transform(*this, std::forward<F>(function));
    }
    template<class F> constexpr auto transform(F&& function) && {
        return detail::expected_transform(std::move(*this),
                                          std::forward<F>(function));
    }
    template<class F> constexpr auto transform(F&& function) const && {
        return detail::expected_transform(std::move(*this),
                                          std::forward<F>(function));
    }
    template<class F> constexpr auto transform_error(F&& function) & {
        return detail::expected_transform_error(*this,
                                                std::forward<F>(function));
    }
    template<class F> constexpr auto transform_error(F&& function) const & {
        return detail::expected_transform_error(*this,
                                                std::forward<F>(function));
    }
    template<class F> constexpr auto transform_error(F&& function) && {
        return detail::expected_transform_error(std::move(*this),
                                                std::forward<F>(function));
    }
    template<class F> constexpr auto transform_error(F&& function) const && {
        return detail::expected_transform_error(std::move(*this),
                                                std::forward<F>(function));
    }
};

template<class T, class E, class U, class G>
constexpr bool operator==(const expected<T, E>& lhs,
                          const expected<U, G>& rhs)
    requires (!is_void<T>::value && !is_void<U>::value &&
              requires(const T& left_value, const U& right_value,
                       const E& left_error, const G& right_error) {
                  { left_value == right_value } -> convertible_to<bool>;
                  { left_error == right_error } -> convertible_to<bool>;
              }) {
    if (lhs.has_value() != rhs.has_value()) return false;
    return lhs.has_value() ? static_cast<bool>(*lhs == *rhs)
                           : static_cast<bool>(lhs.error() == rhs.error());
}

template<class E, class G>
constexpr bool operator==(const expected<void, E>& lhs,
                          const expected<void, G>& rhs)
    requires requires(const E& left, const G& right) {
        { left == right } -> convertible_to<bool>;
    }
{
    if (lhs.has_value() != rhs.has_value()) return false;
    return lhs.has_value() ||
           static_cast<bool>(lhs.error() == rhs.error());
}

template<class T, class E, class U>
constexpr bool operator==(const expected<T, E>& lhs, const U& rhs)
    requires (!is_void<T>::value &&
              !detail::is_expected<remove_cvref_t<U>, void>::value &&
              !detail::is_unexpected<remove_cvref_t<U>>::value &&
              requires(const T& value, const U& other) {
                  { value == other } -> convertible_to<bool>;
              }) {
    return lhs.has_value() && static_cast<bool>(*lhs == rhs);
}

template<class T, class E, class G>
constexpr bool operator==(const expected<T, E>& lhs,
                          const unexpected<G>& rhs)
    requires requires(const E& left, const G& right) {
        { left == right } -> convertible_to<bool>;
    }
{
    return !lhs.has_value() &&
           static_cast<bool>(lhs.error() == rhs.error());
}

} /* namespace std */

#endif /* defined(RIN_LIBCXX_CXX23_DIALECT) */

#endif /* RINCXX_EXPECTED_H */
