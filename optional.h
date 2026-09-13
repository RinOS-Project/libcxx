/*
 * RinOS C++ Optional ✿
 * std::optional 互換実装 (constexpr対応)
 */

#ifndef RINCXX_OPTIONAL_H
#define RINCXX_OPTIONAL_H

#include "rincxx.h"
#include "version.h"

/* std::optional is a C++17 library facility.  Keep the declaration surface
 * absent in earlier language modes so merely including this header remains
 * well-formed for C++11/14 consumers. */
#if defined(__cplusplus) && __cplusplus >= 201703L

#include "exception.h"
#include "functional.h"
#if __cplusplus >= 202002L
#include "compare.h"
#endif
#include "initializer_list.h"
#include "memory.h"
#include "new.h"
#include "type_traits.h"
#include "utility.h"

namespace std {

/* ═══════════════════════════════════════════════════════════════
 * nullopt_t
 * ═══════════════════════════════════════════════════════════════*/

struct nullopt_t {
    explicit constexpr nullopt_t(int) noexcept {}
};

inline constexpr nullopt_t nullopt{0};

/* ═══════════════════════════════════════════════════════════════
 * bad_optional_access
 * ═══════════════════════════════════════════════════════════════*/

class bad_optional_access : public exception {
public:
    const char* what() const noexcept override {
        return "bad optional access";
    }
};

template<typename T>
class optional;

namespace detail {

[[noreturn]] inline void optional_access_fail() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw bad_optional_access();
#else
    __builtin_trap();
#endif
}

template<typename T>
struct is_optional : false_type {};

template<typename T>
struct is_optional<optional<T>> : true_type {};

template<typename T, typename U>
struct optional_converts_from_any_cvref : disjunction<
    is_constructible<T, optional<U>&>,
    is_convertible<optional<U>&, T>,
    is_constructible<T, optional<U>&&>,
    is_convertible<optional<U>&&, T>,
    is_constructible<T, const optional<U>&>,
    is_convertible<const optional<U>&, T>,
    is_constructible<T, const optional<U>&&>,
    is_convertible<const optional<U>&&, T>> {};

template<typename T, typename U>
struct optional_assigns_from_any_cvref : disjunction<
    is_assignable<T&, optional<U>&>,
    is_assignable<T&, optional<U>&&>,
    is_assignable<T&, const optional<U>&>,
    is_assignable<T&, const optional<U>&&>> {};

template<typename T, typename U>
using optional_eq_result = enable_if_t<is_convertible<
    decltype(declval<const T&>() == declval<const U&>()), bool>::value, bool>;

template<typename T, typename U>
using optional_ne_result = enable_if_t<is_convertible<
    decltype(declval<const T&>() != declval<const U&>()), bool>::value, bool>;

template<typename T, typename U>
using optional_lt_result = enable_if_t<is_convertible<
    decltype(declval<const T&>() < declval<const U&>()), bool>::value, bool>;

template<typename T, typename U>
using optional_gt_result = enable_if_t<is_convertible<
    decltype(declval<const T&>() > declval<const U&>()), bool>::value, bool>;

template<typename T, typename U>
using optional_le_result = enable_if_t<is_convertible<
    decltype(declval<const T&>() <= declval<const U&>()), bool>::value, bool>;

template<typename T, typename U>
using optional_ge_result = enable_if_t<is_convertible<
    decltype(declval<const T&>() >= declval<const U&>()), bool>::value, bool>;

#if __cplusplus > 202002L
struct optional_transform_tag {};
#endif

} /* namespace detail */

/* ═══════════════════════════════════════════════════════════════
 * optional storage (union-based for constexpr)
 * ═══════════════════════════════════════════════════════════════*/

template<typename T, bool = is_trivially_destructible<T>::value>
struct optional_storage {
    union {
        char dummy_;
        T value_;
    };
    bool has_value_;

    constexpr optional_storage() noexcept : dummy_{}, has_value_(false) {}

    optional_storage(const optional_storage&) = default;
    optional_storage(optional_storage&&) = default;
    optional_storage& operator=(const optional_storage&) = default;
    optional_storage& operator=(optional_storage&&) = default;

    template<typename... Args>
    constexpr optional_storage(Args&&... args)
        : value_(std::forward<Args>(args)...), has_value_(true) {}

#if __cplusplus > 202002L
    template<typename F, typename U>
    constexpr optional_storage(detail::optional_transform_tag,
                               F&& function, U&& value)
        : value_(std::invoke(std::forward<F>(function),
                             std::forward<U>(value))),
          has_value_(true) {}
#endif

#if __cplusplus >= 202002L
    constexpr
#endif
    ~optional_storage() {
        if (has_value_) {
            value_.~T();
        }
    }
};

/* Trivially destructibleの特殊化 */
template<typename T>
struct optional_storage<T, true> {
    union {
        char dummy_;
        T value_;
    };
    bool has_value_;

    constexpr optional_storage() noexcept : dummy_{}, has_value_(false) {}

    optional_storage(const optional_storage&) = default;
    optional_storage(optional_storage&&) = default;
    optional_storage& operator=(const optional_storage&) = default;
    optional_storage& operator=(optional_storage&&) = default;

    template<typename... Args>
    constexpr optional_storage(Args&&... args)
        : value_(std::forward<Args>(args)...), has_value_(true) {}

#if __cplusplus > 202002L
    template<typename F, typename U>
    constexpr optional_storage(detail::optional_transform_tag,
                               F&& function, U&& value)
        : value_(std::invoke(std::forward<F>(function),
                             std::forward<U>(value))),
          has_value_(true) {}
#endif

    ~optional_storage() = default;
};

#if __cplusplus < 202002L
/*
 * C++17 has no constrained non-template special members.  Carry the
 * availability and triviality rules in storage bases so optional<T> can
 * default its own special members without advertising operations that T
 * does not support.
 */
template<typename T, bool = is_copy_constructible<T>::value,
         bool = is_trivially_copy_constructible<T>::value>
struct optional_cxx17_copy_ctor_base;

template<typename T>
struct optional_cxx17_copy_ctor_base<T, true, true>
    : optional_storage<T> {
    using base = optional_storage<T>;
    using base::base;
    optional_cxx17_copy_ctor_base() = default;
    optional_cxx17_copy_ctor_base(const optional_cxx17_copy_ctor_base&) = default;
    optional_cxx17_copy_ctor_base(optional_cxx17_copy_ctor_base&&) = default;
    optional_cxx17_copy_ctor_base& operator=(const optional_cxx17_copy_ctor_base&) = default;
    optional_cxx17_copy_ctor_base& operator=(optional_cxx17_copy_ctor_base&&) = default;
};

template<typename T>
struct optional_cxx17_copy_ctor_base<T, true, false>
    : optional_storage<T> {
    using base = optional_storage<T>;
    using base::base;
    optional_cxx17_copy_ctor_base() = default;
    constexpr optional_cxx17_copy_ctor_base(
        const optional_cxx17_copy_ctor_base& other) : base() {
        if (other.has_value_) {
            ::new (static_cast<void*>(&(this->value_))) T(other.value_);
            this->has_value_ = true;
        }
    }
    optional_cxx17_copy_ctor_base(optional_cxx17_copy_ctor_base&&) = default;
    optional_cxx17_copy_ctor_base& operator=(const optional_cxx17_copy_ctor_base&) = default;
    optional_cxx17_copy_ctor_base& operator=(optional_cxx17_copy_ctor_base&&) = default;
};

template<typename T, bool Trivial>
struct optional_cxx17_copy_ctor_base<T, false, Trivial>
    : optional_storage<T> {
    using base = optional_storage<T>;
    using base::base;
    optional_cxx17_copy_ctor_base() = default;
    optional_cxx17_copy_ctor_base(const optional_cxx17_copy_ctor_base&) = delete;
    optional_cxx17_copy_ctor_base(optional_cxx17_copy_ctor_base&&) = default;
    optional_cxx17_copy_ctor_base& operator=(const optional_cxx17_copy_ctor_base&) = default;
    optional_cxx17_copy_ctor_base& operator=(optional_cxx17_copy_ctor_base&&) = default;
};

template<typename T, bool = is_move_constructible<T>::value,
         bool = is_trivially_move_constructible<T>::value>
struct optional_cxx17_move_ctor_base;

template<typename T>
struct optional_cxx17_move_ctor_base<T, true, true>
    : optional_cxx17_copy_ctor_base<T> {
    using base = optional_cxx17_copy_ctor_base<T>;
    using base::base;
    optional_cxx17_move_ctor_base() = default;
    optional_cxx17_move_ctor_base(const optional_cxx17_move_ctor_base&) = default;
    optional_cxx17_move_ctor_base(optional_cxx17_move_ctor_base&&) = default;
    optional_cxx17_move_ctor_base& operator=(const optional_cxx17_move_ctor_base&) = default;
    optional_cxx17_move_ctor_base& operator=(optional_cxx17_move_ctor_base&&) = default;
};

template<typename T>
struct optional_cxx17_move_ctor_base<T, true, false>
    : optional_cxx17_copy_ctor_base<T> {
    using base = optional_cxx17_copy_ctor_base<T>;
    using base::base;
    optional_cxx17_move_ctor_base() = default;
    optional_cxx17_move_ctor_base(const optional_cxx17_move_ctor_base&) = default;
    constexpr optional_cxx17_move_ctor_base(
        optional_cxx17_move_ctor_base&& other) noexcept(
            is_nothrow_move_constructible<T>::value) : base() {
        if (other.has_value_) {
            ::new (static_cast<void*>(&(this->value_)))
                T(std::move(other.value_));
            this->has_value_ = true;
        }
    }
    optional_cxx17_move_ctor_base& operator=(const optional_cxx17_move_ctor_base&) = default;
    optional_cxx17_move_ctor_base& operator=(optional_cxx17_move_ctor_base&&) = default;
};

template<typename T, bool Trivial>
struct optional_cxx17_move_ctor_base<T, false, Trivial>
    : optional_cxx17_copy_ctor_base<T> {
    using base = optional_cxx17_copy_ctor_base<T>;
    using base::base;
    optional_cxx17_move_ctor_base() = default;
    optional_cxx17_move_ctor_base(const optional_cxx17_move_ctor_base&) = default;
    optional_cxx17_move_ctor_base(optional_cxx17_move_ctor_base&&) = delete;
    optional_cxx17_move_ctor_base& operator=(const optional_cxx17_move_ctor_base&) = default;
    optional_cxx17_move_ctor_base& operator=(optional_cxx17_move_ctor_base&&) = default;
};

template<typename T,
         bool = is_copy_constructible<T>::value && is_copy_assignable<T>::value,
         bool = is_trivially_copy_constructible<T>::value &&
                is_trivially_copy_assignable<T>::value &&
                is_trivially_destructible<T>::value>
struct optional_cxx17_copy_assign_base;

template<typename T>
struct optional_cxx17_copy_assign_base<T, true, true>
    : optional_cxx17_move_ctor_base<T> {
    using base = optional_cxx17_move_ctor_base<T>;
    using base::base;
    optional_cxx17_copy_assign_base() = default;
    optional_cxx17_copy_assign_base(const optional_cxx17_copy_assign_base&) = default;
    optional_cxx17_copy_assign_base(optional_cxx17_copy_assign_base&&) = default;
    optional_cxx17_copy_assign_base& operator=(const optional_cxx17_copy_assign_base&) = default;
    optional_cxx17_copy_assign_base& operator=(optional_cxx17_copy_assign_base&&) = default;
};

template<typename T>
struct optional_cxx17_copy_assign_base<T, true, false>
    : optional_cxx17_move_ctor_base<T> {
    using base = optional_cxx17_move_ctor_base<T>;
    using base::base;
    optional_cxx17_copy_assign_base() = default;
    optional_cxx17_copy_assign_base(const optional_cxx17_copy_assign_base&) = default;
    optional_cxx17_copy_assign_base(optional_cxx17_copy_assign_base&&) = default;
    constexpr optional_cxx17_copy_assign_base& operator=(
        const optional_cxx17_copy_assign_base& other) {
        if (other.has_value_) {
            if (this->has_value_) {
                this->value_ = other.value_;
            } else {
                ::new (static_cast<void*>(&(this->value_))) T(other.value_);
                this->has_value_ = true;
            }
        } else if (this->has_value_) {
            this->value_.~T();
            this->has_value_ = false;
        }
        return *this;
    }
    optional_cxx17_copy_assign_base& operator=(optional_cxx17_copy_assign_base&&) = default;
};

template<typename T, bool Trivial>
struct optional_cxx17_copy_assign_base<T, false, Trivial>
    : optional_cxx17_move_ctor_base<T> {
    using base = optional_cxx17_move_ctor_base<T>;
    using base::base;
    optional_cxx17_copy_assign_base() = default;
    optional_cxx17_copy_assign_base(const optional_cxx17_copy_assign_base&) = default;
    optional_cxx17_copy_assign_base(optional_cxx17_copy_assign_base&&) = default;
    optional_cxx17_copy_assign_base& operator=(const optional_cxx17_copy_assign_base&) = delete;
    optional_cxx17_copy_assign_base& operator=(optional_cxx17_copy_assign_base&&) = default;
};

template<typename T,
         bool = is_move_constructible<T>::value && is_move_assignable<T>::value,
         bool = is_trivially_move_constructible<T>::value &&
                is_trivially_move_assignable<T>::value &&
                is_trivially_destructible<T>::value>
struct optional_cxx17_move_assign_base;

template<typename T>
struct optional_cxx17_move_assign_base<T, true, true>
    : optional_cxx17_copy_assign_base<T> {
    using base = optional_cxx17_copy_assign_base<T>;
    using base::base;
    optional_cxx17_move_assign_base() = default;
    optional_cxx17_move_assign_base(const optional_cxx17_move_assign_base&) = default;
    optional_cxx17_move_assign_base(optional_cxx17_move_assign_base&&) = default;
    optional_cxx17_move_assign_base& operator=(const optional_cxx17_move_assign_base&) = default;
    optional_cxx17_move_assign_base& operator=(optional_cxx17_move_assign_base&&) = default;
};

template<typename T>
struct optional_cxx17_move_assign_base<T, true, false>
    : optional_cxx17_copy_assign_base<T> {
    using base = optional_cxx17_copy_assign_base<T>;
    using base::base;
    optional_cxx17_move_assign_base() = default;
    optional_cxx17_move_assign_base(const optional_cxx17_move_assign_base&) = default;
    optional_cxx17_move_assign_base(optional_cxx17_move_assign_base&&) = default;
    optional_cxx17_move_assign_base& operator=(const optional_cxx17_move_assign_base&) = default;
    constexpr optional_cxx17_move_assign_base& operator=(
        optional_cxx17_move_assign_base&& other) noexcept(
            is_nothrow_move_constructible<T>::value &&
            is_nothrow_move_assignable<T>::value) {
        if (other.has_value_) {
            if (this->has_value_) {
                this->value_ = std::move(other.value_);
            } else {
                ::new (static_cast<void*>(&(this->value_)))
                    T(std::move(other.value_));
                this->has_value_ = true;
            }
        } else if (this->has_value_) {
            this->value_.~T();
            this->has_value_ = false;
        }
        return *this;
    }
};

template<typename T, bool Trivial>
struct optional_cxx17_move_assign_base<T, false, Trivial>
    : optional_cxx17_copy_assign_base<T> {
    using base = optional_cxx17_copy_assign_base<T>;
    using base::base;
    optional_cxx17_move_assign_base() = default;
    optional_cxx17_move_assign_base(const optional_cxx17_move_assign_base&) = default;
    optional_cxx17_move_assign_base(optional_cxx17_move_assign_base&&) = default;
    optional_cxx17_move_assign_base& operator=(const optional_cxx17_move_assign_base&) = default;
    optional_cxx17_move_assign_base& operator=(optional_cxx17_move_assign_base&&) = delete;
};
#endif

/* ═══════════════════════════════════════════════════════════════
 * optional<T>
 * ═══════════════════════════════════════════════════════════════*/

template<typename T>
class optional : private
#if __cplusplus >= 202002L
                 optional_storage<T>
#else
                 optional_cxx17_move_assign_base<T>
#endif
{
#if __cplusplus >= 202002L
    using storage = optional_storage<T>;
#else
    using storage = optional_cxx17_move_assign_base<T>;
#endif

    static_assert(!is_void<T>::value,
                  "optional value type cannot be void");
    static_assert(!is_reference<T>::value,
                  "optional value type cannot be a reference");
    static_assert(!is_array<T>::value,
                  "optional value type cannot be an array");
    static_assert(!is_same<typename remove_cv<T>::type, nullopt_t>::value,
                  "optional value type cannot be nullopt_t");
    static_assert(!is_same<typename remove_cv<T>::type, in_place_t>::value,
                  "optional value type cannot be in_place_t");
    static_assert(is_destructible<T>::value,
                  "optional value type must be destructible");

public:
    using value_type = T;

    /* デフォルトコンストラクタ */
    constexpr optional() noexcept : storage() {}

    /* nullopt コンストラクタ */
    constexpr optional(nullopt_t) noexcept : storage() {}

    /* Value constructor: U -> optional<T> where T is constructible from U */
#if __cplusplus >= 202002L
    template<typename U = T,
             typename = typename enable_if<
                 is_constructible<T, U&&>::value &&
                 !is_same<remove_cvref_t<U>, optional>::value &&
                 !is_same<remove_cvref_t<U>, in_place_t>::value &&
                 !(is_same<typename remove_cv<T>::type, bool>::value &&
                   detail::is_optional<remove_cvref_t<U>>::value)
             >::type>
    constexpr explicit(!is_convertible<U&&, T>::value) optional(U&& value)
        noexcept(is_nothrow_constructible<T, U&&>::value)
        : storage(std::forward<U>(value)) {}
#else
    template<typename U = T, typename enable_if<
                 is_constructible<T, U&&>::value &&
                 is_convertible<U&&, T>::value &&
                 !is_same<remove_cvref_t<U>, optional>::value &&
                 !is_same<remove_cvref_t<U>, in_place_t>::value &&
                 !(is_same<typename remove_cv<T>::type, bool>::value &&
                   detail::is_optional<remove_cvref_t<U>>::value), int>::type = 0>
    constexpr optional(U&& value)
        noexcept(is_nothrow_constructible<T, U&&>::value)
        : storage(std::forward<U>(value)) {}

    template<typename U = T, typename enable_if<
                 is_constructible<T, U&&>::value &&
                 !is_convertible<U&&, T>::value &&
                 !is_same<remove_cvref_t<U>, optional>::value &&
                 !is_same<remove_cvref_t<U>, in_place_t>::value &&
                 !(is_same<typename remove_cv<T>::type, bool>::value &&
                   detail::is_optional<remove_cvref_t<U>>::value), bool>::type = true>
    constexpr explicit optional(U&& value)
        noexcept(is_nothrow_constructible<T, U&&>::value)
        : storage(std::forward<U>(value)) {}
#endif

    /* in_place コンストラクタ */
    template<typename... Args,
             typename = typename enable_if<
                 is_constructible<T, Args&&...>::value>::type>
    constexpr explicit optional(in_place_t, Args&&... args)
        noexcept(is_nothrow_constructible<T, Args&&...>::value)
        : storage(std::forward<Args>(args)...) {}

    template<typename U, typename... Args,
             typename = typename enable_if<
                 is_constructible<T, initializer_list<U>&, Args&&...>::value
             >::type>
    constexpr explicit optional(in_place_t, initializer_list<U> values,
                                Args&&... args)
        noexcept(is_nothrow_constructible<
                 T, initializer_list<U>&, Args&&...>::value)
        : storage(values, std::forward<Args>(args)...) {}

    /* Trivial special members remain trivial when T permits it. */
#if __cplusplus >= 202002L
    constexpr optional(const optional&)
        requires (is_trivially_copy_constructible<T>::value) = default;

    /* コピーコンストラクタ */
    constexpr optional(const optional& other)
        requires (is_copy_constructible<T>::value &&
                  !is_trivially_copy_constructible<T>::value)
        : storage() {
        if (other.has_value()) {
            construct(*other);
        }
    }

    optional(const optional&)
        requires (!is_copy_constructible<T>::value) = delete;

    constexpr optional(optional&&)
        noexcept(is_nothrow_move_constructible<T>::value)
        requires (is_trivially_move_constructible<T>::value) = default;

    /* ムーブコンストラクタ */
    constexpr optional(optional&& other)
        noexcept(is_nothrow_move_constructible<T>::value)
        requires (is_move_constructible<T>::value &&
                  !is_trivially_move_constructible<T>::value)
        : storage() {
        if (other.has_value()) {
            construct(std::move(*other));
        }
    }

    optional(optional&&)
        requires (!is_move_constructible<T>::value) = delete;
#else
    optional(const optional&) = default;
    optional(optional&&) = default;
#endif

    /* Converting constructor: optional<U> -> optional<T> where U is convertible to T */
#if __cplusplus >= 202002L
    template<typename U>
    constexpr explicit(!is_convertible<const U&, T>::value)
    optional(const optional<U>& other)
        noexcept(is_nothrow_constructible<T, const U&>::value)
        requires (!is_same<T, U>::value &&
                  is_constructible<T, const U&>::value &&
                  (is_same<typename remove_cv<T>::type, bool>::value ||
                   !detail::optional_converts_from_any_cvref<T, U>::value))
        : storage() {
        if (other.has_value()) {
            construct(*other);
        }
    }

    template<typename U>
    constexpr explicit(!is_convertible<U&&, T>::value)
    optional(optional<U>&& other)
        noexcept(is_nothrow_constructible<T, U&&>::value)
        requires (!is_same<T, U>::value &&
                  is_constructible<T, U&&>::value &&
                  (is_same<typename remove_cv<T>::type, bool>::value ||
                   !detail::optional_converts_from_any_cvref<T, U>::value))
        : storage() {
        if (other.has_value()) {
            construct(std::move(*other));
        }
    }
#else
    template<typename U, typename enable_if<
                 !is_same<T, U>::value &&
                 is_constructible<T, const U&>::value &&
                 is_convertible<const U&, T>::value &&
                 (is_same<typename remove_cv<T>::type, bool>::value ||
                  !detail::optional_converts_from_any_cvref<T, U>::value), int>::type = 0>
    constexpr optional(const optional<U>& other)
        noexcept(is_nothrow_constructible<T, const U&>::value)
        : storage() {
        if (other.has_value()) {
            construct(*other);
        }
    }

    template<typename U, typename enable_if<
                 !is_same<T, U>::value &&
                 is_constructible<T, const U&>::value &&
                 !is_convertible<const U&, T>::value &&
                 (is_same<typename remove_cv<T>::type, bool>::value ||
                  !detail::optional_converts_from_any_cvref<T, U>::value), bool>::type = true>
    constexpr explicit optional(const optional<U>& other)
        noexcept(is_nothrow_constructible<T, const U&>::value)
        : storage() {
        if (other.has_value()) {
            construct(*other);
        }
    }

    template<typename U, typename enable_if<
                 !is_same<T, U>::value &&
                 is_constructible<T, U&&>::value &&
                 is_convertible<U&&, T>::value &&
                 (is_same<typename remove_cv<T>::type, bool>::value ||
                  !detail::optional_converts_from_any_cvref<T, U>::value), int>::type = 0>
    constexpr optional(optional<U>&& other)
        noexcept(is_nothrow_constructible<T, U&&>::value)
        : storage() {
        if (other.has_value()) {
            construct(std::move(*other));
        }
    }

    template<typename U, typename enable_if<
                 !is_same<T, U>::value &&
                 is_constructible<T, U&&>::value &&
                 !is_convertible<U&&, T>::value &&
                 (is_same<typename remove_cv<T>::type, bool>::value ||
                  !detail::optional_converts_from_any_cvref<T, U>::value), bool>::type = true>
    constexpr explicit optional(optional<U>&& other)
        noexcept(is_nothrow_constructible<T, U&&>::value)
        : storage() {
        if (other.has_value()) {
            construct(std::move(*other));
        }
    }
#endif

    /* デストラクタはoptional_storageが処理 */

    /* 代入演算子 */
    constexpr optional& operator=(nullopt_t) noexcept {
        reset();
        return *this;
    }

#if __cplusplus >= 202002L
    constexpr optional& operator=(const optional&)
        requires (is_trivially_copy_constructible<T>::value &&
                  is_trivially_copy_assignable<T>::value &&
                  is_trivially_destructible<T>::value) = default;

    constexpr optional& operator=(const optional& other)
        requires (is_copy_constructible<T>::value &&
                  is_copy_assignable<T>::value &&
                  !(is_trivially_copy_constructible<T>::value &&
                    is_trivially_copy_assignable<T>::value &&
                    is_trivially_destructible<T>::value)) {
        if (this != &other) {
            if (other.has_value()) {
                if (this->has_value_) {
                    this->value_ = *other;
                } else {
                    construct(*other);
                }
            } else {
                reset();
            }
        }
        return *this;
    }

    optional& operator=(const optional&)
        requires (!(is_copy_constructible<T>::value &&
                    is_copy_assignable<T>::value)) = delete;

    constexpr optional& operator=(optional&&)
        noexcept(is_nothrow_move_constructible<T>::value &&
                 is_nothrow_move_assignable<T>::value)
        requires (is_trivially_move_constructible<T>::value &&
                  is_trivially_move_assignable<T>::value &&
                  is_trivially_destructible<T>::value) = default;

    constexpr optional& operator=(optional&& other)
        noexcept(is_nothrow_move_constructible<T>::value &&
                 is_nothrow_move_assignable<T>::value)
        requires (is_move_constructible<T>::value &&
                  is_move_assignable<T>::value &&
                  !(is_trivially_move_constructible<T>::value &&
                    is_trivially_move_assignable<T>::value &&
                    is_trivially_destructible<T>::value)) {
        if (this != &other) {
            if (other.has_value()) {
                if (this->has_value_) {
                    this->value_ = std::move(*other);
                } else {
                    construct(std::move(*other));
                }
            } else {
                reset();
            }
        }
        return *this;
    }

    optional& operator=(optional&&)
        requires (!(is_move_constructible<T>::value &&
                    is_move_assignable<T>::value)) = delete;
#else
    optional& operator=(const optional&) = default;
    optional& operator=(optional&&) = default;
#endif

    template<typename U, typename = typename enable_if<
                 !is_same<T, U>::value &&
                 is_constructible<T, const U&>::value &&
                 is_assignable<T&, const U&>::value &&
                 !detail::optional_converts_from_any_cvref<T, U>::value &&
                 !detail::optional_assigns_from_any_cvref<T, U>::value>::type>
    constexpr optional& operator=(const optional<U>& other)
        noexcept(is_nothrow_constructible<T, const U&>::value &&
                 is_nothrow_assignable<T&, const U&>::value)
    {
        if (other.has_value()) {
            if (this->has_value_) {
                this->value_ = *other;
            } else {
                construct(*other);
            }
        } else {
            reset();
        }
        return *this;
    }

    template<typename U, typename = typename enable_if<
                 !is_same<T, U>::value &&
                 is_constructible<T, U&&>::value &&
                 is_assignable<T&, U&&>::value &&
                 !detail::optional_converts_from_any_cvref<T, U>::value &&
                 !detail::optional_assigns_from_any_cvref<T, U>::value>::type>
    constexpr optional& operator=(optional<U>&& other)
        noexcept(is_nothrow_constructible<T, U&&>::value &&
                 is_nothrow_assignable<T&, U&&>::value)
    {
        if (other.has_value()) {
            if (this->has_value_) {
                this->value_ = std::move(*other);
            } else {
                construct(std::move(*other));
            }
        } else {
            reset();
        }
        return *this;
    }

    /* Value assignment: optional<U> remains valid when it is a value for T. */
    template<typename U = T,
             typename = typename enable_if<
                 !is_same<remove_cvref_t<U>, optional>::value &&
                 !(is_scalar<T>::value &&
                   is_same<T, typename decay<U>::type>::value) &&
                 is_constructible<T, U&&>::value &&
                 is_assignable<T&, U&&>::value
             >::type>
    constexpr optional& operator=(U&& val)
        noexcept(is_nothrow_constructible<T, U&&>::value &&
                 is_nothrow_assignable<T&, U&&>::value) {
        if (this->has_value_) {
            this->value_ = std::forward<U>(val);
        } else {
            construct(std::forward<U>(val));
        }
        return *this;
    }

    /* アクセサ */
    constexpr bool has_value() const noexcept { return this->has_value_; }
    constexpr explicit operator bool() const noexcept { return this->has_value_; }

    constexpr T& value() & {
        if (!this->has_value_) {
            detail::optional_access_fail();
        }
        return this->value_;
    }

    constexpr const T& value() const & {
        if (!this->has_value_) {
            detail::optional_access_fail();
        }
        return this->value_;
    }

    constexpr T&& value() && {
        if (!this->has_value_) {
            detail::optional_access_fail();
        }
        return std::move(this->value_);
    }

    constexpr const T&& value() const && {
        if (!this->has_value_) {
            detail::optional_access_fail();
        }
        return std::move(this->value_);
    }

    constexpr T& operator*() & noexcept { return this->value_; }
    constexpr const T& operator*() const & noexcept { return this->value_; }
    constexpr T&& operator*() && noexcept { return std::move(this->value_); }
    constexpr const T&& operator*() const && noexcept {
        return std::move(this->value_);
    }

    constexpr T* operator->() noexcept { return &(this->value_); }
    constexpr const T* operator->() const noexcept { return &(this->value_); }

    template<typename U>
    constexpr T value_or(U&& default_val) const & {
        static_assert(is_copy_constructible<T>::value,
                      "optional::value_or const& requires copy construction");
        static_assert(is_convertible<U&&, T>::value,
                      "optional::value_or requires an implicitly convertible default");
        return this->has_value_ ? this->value_ : static_cast<T>(std::forward<U>(default_val));
    }

    template<typename U>
    constexpr T value_or(U&& default_val) && {
        static_assert(is_move_constructible<T>::value,
                      "optional::value_or && requires move construction");
        static_assert(is_convertible<U&&, T>::value,
                      "optional::value_or requires an implicitly convertible default");
        return this->has_value_ ? std::move(this->value_) : static_cast<T>(std::forward<U>(default_val));
    }

#if __cplusplus > 202002L
    template<typename F>
    constexpr auto and_then(F&& function) & {
        using result_type =
            remove_cvref_t<invoke_result_t<F, T&>>;
        static_assert(detail::is_optional<result_type>::value,
                      "optional::and_then callable must return optional");
        if (this->has_value_) {
            return std::invoke(std::forward<F>(function), this->value_);
        }
        return result_type();
    }

    template<typename F>
    constexpr auto and_then(F&& function) const & {
        using result_type =
            remove_cvref_t<invoke_result_t<F, const T&>>;
        static_assert(detail::is_optional<result_type>::value,
                      "optional::and_then callable must return optional");
        if (this->has_value_) {
            return std::invoke(std::forward<F>(function), this->value_);
        }
        return result_type();
    }

    template<typename F>
    constexpr auto and_then(F&& function) && {
        using result_type =
            remove_cvref_t<invoke_result_t<F, T>>;
        static_assert(detail::is_optional<result_type>::value,
                      "optional::and_then callable must return optional");
        if (this->has_value_) {
            return std::invoke(std::forward<F>(function),
                               std::move(this->value_));
        }
        return result_type();
    }

    template<typename F>
    constexpr auto and_then(F&& function) const && {
        using result_type =
            remove_cvref_t<invoke_result_t<F, const T>>;
        static_assert(detail::is_optional<result_type>::value,
                      "optional::and_then callable must return optional");
        if (this->has_value_) {
            return std::invoke(std::forward<F>(function),
                               std::move(this->value_));
        }
        return result_type();
    }

    template<typename F>
    constexpr auto transform(F&& function) & {
        using result_type = remove_cv_t<invoke_result_t<F, T&>>;
        if (this->has_value_) {
            return optional<result_type>(
                detail::optional_transform_tag{},
                std::forward<F>(function), this->value_);
        }
        return optional<result_type>();
    }

    template<typename F>
    constexpr auto transform(F&& function) const & {
        using result_type = remove_cv_t<invoke_result_t<F, const T&>>;
        if (this->has_value_) {
            return optional<result_type>(
                detail::optional_transform_tag{},
                std::forward<F>(function), this->value_);
        }
        return optional<result_type>();
    }

    template<typename F>
    constexpr auto transform(F&& function) && {
        using result_type = remove_cv_t<invoke_result_t<F, T>>;
        if (this->has_value_) {
            return optional<result_type>(
                detail::optional_transform_tag{},
                std::forward<F>(function), std::move(this->value_));
        }
        return optional<result_type>();
    }

    template<typename F>
    constexpr auto transform(F&& function) const && {
        using result_type = remove_cv_t<invoke_result_t<F, const T>>;
        if (this->has_value_) {
            return optional<result_type>(
                detail::optional_transform_tag{},
                std::forward<F>(function), std::move(this->value_));
        }
        return optional<result_type>();
    }

    template<typename F>
    constexpr optional or_else(F&& function) const &
        requires (is_invocable<F>::value && is_copy_constructible<T>::value) {
        using result_type = invoke_result_t<F>;
        static_assert(is_same<remove_cvref_t<result_type>, optional>::value,
                      "optional::or_else callable must return optional<T>");
        if (this->has_value_) {
            return *this;
        }
        return std::invoke(std::forward<F>(function));
    }

    template<typename F>
    constexpr optional or_else(F&& function) &&
        requires (is_invocable<F>::value && is_move_constructible<T>::value) {
        using result_type = invoke_result_t<F>;
        static_assert(is_same<remove_cvref_t<result_type>, optional>::value,
                      "optional::or_else callable must return optional<T>");
        if (this->has_value_) {
            return std::move(*this);
        }
        return std::invoke(std::forward<F>(function));
    }
#endif

    /* 変更 */
    constexpr void reset() noexcept {
        if (this->has_value_) {
            if constexpr (!is_trivially_destructible<T>::value) {
                this->value_.~T();
            }
            this->has_value_ = false;
        }
    }

    template<typename... Args>
    constexpr enable_if_t<is_constructible<T, Args&&...>::value, T&>
    emplace(Args&&... args)
        noexcept(is_nothrow_constructible<T, Args&&...>::value) {
        reset();
        construct(std::forward<Args>(args)...);
        return this->value_;
    }

    template<typename U, typename... Args>
    constexpr typename enable_if<
        is_constructible<T, initializer_list<U>&, Args&&...>::value,
        T&
    >::type emplace(initializer_list<U> values, Args&&... args)
        noexcept(is_nothrow_constructible<
                 T, initializer_list<U>&, Args&&...>::value) {
        reset();
        construct(values, std::forward<Args>(args)...);
        return this->value_;
    }

#if __cplusplus >= 202002L
    constexpr void swap(optional& other)
        noexcept(is_nothrow_move_constructible<T>::value &&
                 is_nothrow_swappable<T>::value)
        requires (is_move_constructible<T>::value &&
                  is_swappable<T>::value) {
        if (this->has_value_ && other.has_value()) {
            using std::swap;
            swap(this->value_, *other);
        } else if (this->has_value_) {
            other.construct(std::move(this->value_));
            reset();
        } else if (other.has_value()) {
            construct(std::move(*other));
            other.reset();
        }
    }
#else
    template<typename U = T, typename enable_if<
                 is_same<U, T>::value &&
                 is_move_constructible<U>::value &&
                 is_swappable<U>::value, int>::type = 0>
    constexpr void swap(optional& other)
        noexcept(is_nothrow_move_constructible<T>::value &&
                 is_nothrow_swappable<T>::value) {
        if (this->has_value_ && other.has_value()) {
            using std::swap;
            swap(this->value_, *other);
        } else if (this->has_value_) {
            other.construct(std::move(this->value_));
            reset();
        } else if (other.has_value()) {
            construct(std::move(*other));
            other.reset();
        }
    }
#endif

private:
#if __cplusplus > 202002L
    template<typename>
    friend class optional;

    template<typename F, typename U>
    constexpr explicit optional(detail::optional_transform_tag tag,
                                F&& function, U&& value)
        : storage(tag, std::forward<F>(function),
                  std::forward<U>(value)) {}
#endif

    template<typename... Args>
    constexpr void construct(Args&&... args) {
#if __cplusplus >= 202002L
        std::construct_at(&(this->value_), std::forward<Args>(args)...);
#else
        ::new (static_cast<void*>(&(this->value_))) T(std::forward<Args>(args)...);
#endif
        this->has_value_ = true;
    }
};

template<typename T>
#if __cplusplus >= 202002L
constexpr void swap(optional<T>& lhs, optional<T>& rhs)
    noexcept(noexcept(lhs.swap(rhs)))
    requires (is_move_constructible<T>::value && is_swappable<T>::value) {
    lhs.swap(rhs);
}
#else
constexpr typename enable_if<
    is_move_constructible<T>::value && is_swappable<T>::value, void>::type
swap(optional<T>& lhs, optional<T>& rhs)
    noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}
#endif

template<typename T>
optional(T) -> optional<T>;

/* ═══════════════════════════════════════════════════════════════
 * 比較演算子
 * ═══════════════════════════════════════════════════════════════*/

template<typename T, typename U>
constexpr auto operator==(const optional<T>& lhs, const optional<U>& rhs)
    -> detail::optional_eq_result<T, U> {
    if (lhs.has_value() != rhs.has_value()) return false;
    if (!lhs.has_value()) return true;
    return *lhs == *rhs;
}

template<typename T, typename U>
constexpr auto operator!=(const optional<T>& lhs, const optional<U>& rhs)
    -> detail::optional_ne_result<T, U> {
    if (lhs.has_value() != rhs.has_value()) return true;
    return lhs.has_value() && *lhs != *rhs;
}

#if __cplusplus >= 202002L
template<typename T, typename U>
    requires three_way_comparable_with<T, U>
constexpr compare_three_way_result_t<T, U>
operator<=>(const optional<T>& lhs, const optional<U>& rhs) {
    if (lhs.has_value() && rhs.has_value()) {
        return *lhs <=> *rhs;
    }
    return static_cast<bool>(lhs) <=> static_cast<bool>(rhs);
}
#endif

template<typename T>
constexpr bool operator==(const optional<T>& opt, nullopt_t) noexcept {
    return !opt.has_value();
}

template<typename T>
constexpr bool operator==(nullopt_t, const optional<T>& opt) noexcept {
    return !opt.has_value();
}

#if __cplusplus >= 202002L
template<typename T>
constexpr strong_ordering operator<=>(const optional<T>& opt,
                                      nullopt_t) noexcept {
    return static_cast<bool>(opt) <=> false;
}
#endif

template<typename T, typename U>
constexpr auto operator==(const optional<T>& opt, const U& val)
    -> detail::optional_eq_result<T, U> {
    return opt.has_value() && *opt == val;
}

template<typename T, typename U>
constexpr auto operator==(const U& val, const optional<T>& opt)
    -> detail::optional_eq_result<U, T> {
    return opt.has_value() && val == *opt;
}

template<typename T, typename U>
constexpr auto operator!=(const optional<T>& opt, const U& val)
    -> detail::optional_ne_result<T, U> {
    return !opt.has_value() || *opt != val;
}

template<typename T, typename U>
constexpr auto operator!=(const U& val, const optional<T>& opt)
    -> detail::optional_ne_result<U, T> {
    return !opt.has_value() || val != *opt;
}

/* optional<T> vs optional<U> ordering */
template<typename T, typename U>
constexpr auto operator<(const optional<T>& lhs, const optional<U>& rhs)
    -> detail::optional_lt_result<T, U> {
    if (!rhs.has_value()) return false;
    if (!lhs.has_value()) return true;
    return *lhs < *rhs;
}

template<typename T, typename U>
constexpr auto operator>(const optional<T>& lhs, const optional<U>& rhs)
    -> detail::optional_gt_result<T, U> {
    if (!lhs.has_value()) return false;
    if (!rhs.has_value()) return true;
    return *lhs > *rhs;
}

template<typename T, typename U>
constexpr auto operator<=(const optional<T>& lhs, const optional<U>& rhs)
    -> detail::optional_le_result<T, U> {
    if (!lhs.has_value()) return true;
    if (!rhs.has_value()) return false;
    return *lhs <= *rhs;
}

template<typename T, typename U>
constexpr auto operator>=(const optional<T>& lhs, const optional<U>& rhs)
    -> detail::optional_ge_result<T, U> {
    if (!rhs.has_value()) return true;
    if (!lhs.has_value()) return false;
    return *lhs >= *rhs;
}

/* optional<T> vs nullopt_t ordering */
template<typename T>
constexpr bool operator<(const optional<T>&, nullopt_t) noexcept {
    return false;
}

template<typename T>
constexpr bool operator<(nullopt_t, const optional<T>& opt) noexcept {
    return opt.has_value();
}

template<typename T>
constexpr bool operator>(const optional<T>& opt, nullopt_t) noexcept {
    return opt.has_value();
}

template<typename T>
constexpr bool operator>(nullopt_t, const optional<T>&) noexcept {
    return false;
}

template<typename T>
constexpr bool operator<=(const optional<T>& opt, nullopt_t) noexcept {
    return !opt.has_value();
}

template<typename T>
constexpr bool operator<=(nullopt_t, const optional<T>&) noexcept {
    return true;
}

template<typename T>
constexpr bool operator>=(const optional<T>&, nullopt_t) noexcept {
    return true;
}

template<typename T>
constexpr bool operator>=(nullopt_t, const optional<T>& opt) noexcept {
    return !opt.has_value();
}

/* optional<T> vs U ordering */
template<typename T, typename U>
constexpr auto operator<(const optional<T>& opt, const U& val)
    -> detail::optional_lt_result<T, U> {
    return opt.has_value() ? *opt < val : true;
}

template<typename T, typename U>
constexpr auto operator<(const U& val, const optional<T>& opt)
    -> detail::optional_lt_result<U, T> {
    return opt.has_value() ? val < *opt : false;
}

template<typename T, typename U>
constexpr auto operator>(const optional<T>& opt, const U& val)
    -> detail::optional_gt_result<T, U> {
    return opt.has_value() ? *opt > val : false;
}

template<typename T, typename U>
constexpr auto operator>(const U& val, const optional<T>& opt)
    -> detail::optional_gt_result<U, T> {
    return opt.has_value() ? val > *opt : true;
}

template<typename T, typename U>
constexpr auto operator<=(const optional<T>& opt, const U& val)
    -> detail::optional_le_result<T, U> {
    return opt.has_value() ? *opt <= val : true;
}

template<typename T, typename U>
constexpr auto operator<=(const U& val, const optional<T>& opt)
    -> detail::optional_le_result<U, T> {
    return opt.has_value() ? val <= *opt : false;
}

template<typename T, typename U>
constexpr auto operator>=(const optional<T>& opt, const U& val)
    -> detail::optional_ge_result<T, U> {
    return opt.has_value() ? *opt >= val : false;
}

template<typename T, typename U>
constexpr auto operator>=(const U& val, const optional<T>& opt)
    -> detail::optional_ge_result<U, T> {
    return opt.has_value() ? val >= *opt : true;
}

#if __cplusplus >= 202002L
template<typename T, typename U>
    requires (!detail::is_optional<remove_cvref_t<U>>::value &&
              three_way_comparable_with<T, U>)
constexpr compare_three_way_result_t<T, U>
operator<=>(const optional<T>& opt, const U& value) {
    if (opt.has_value()) {
        return *opt <=> value;
    }
    return strong_ordering::less;
}
#endif

/* ═══════════════════════════════════════════════════════════════
 * make_optional
 * ═══════════════════════════════════════════════════════════════*/

template<typename T>
constexpr typename enable_if<
    is_constructible<decay_t<T>, T&&>::value,
    optional<decay_t<T>>>::type make_optional(T&& val)
    noexcept(is_nothrow_constructible<decay_t<T>, T&&>::value)
{
    return optional<decay_t<T>>(std::forward<T>(val));
}

template<typename T, typename... Args>
constexpr typename enable_if<
    is_constructible<T, Args&&...>::value,
    optional<T>>::type make_optional(Args&&... args)
    noexcept(is_nothrow_constructible<T, Args&&...>::value)
{
    return optional<T>(in_place, std::forward<Args>(args)...);
}

template<typename T, typename U, typename... Args>
constexpr typename enable_if<
    is_constructible<T, initializer_list<U>&, Args&&...>::value,
    optional<T>>::type make_optional(initializer_list<U> values,
                                     Args&&... args)
    noexcept(is_nothrow_constructible<
             T, initializer_list<U>&, Args&&...>::value)
{
    return optional<T>(in_place, values, std::forward<Args>(args)...);
}

/* ═══════════════════════════════════════════════════════════════
 * common_type specializations for optional
 * ═══════════════════════════════════════════════════════════════*/

/* optional<T> and optional<U> */
template<typename T, typename U>
struct common_type<optional<T>, optional<U>> {
    using type = optional<typename common_type<T, U>::type>;
};

/* optional<T> and T */
template<typename T>
struct common_type<optional<T>, T> {
    using type = optional<T>;
};

template<typename T>
struct common_type<T, optional<T>> {
    using type = optional<T>;
};

/* optional<T> and nullopt_t */
template<typename T>
struct common_type<optional<T>, nullopt_t> {
    using type = optional<T>;
};

template<typename T>
struct common_type<nullopt_t, optional<T>> {
    using type = optional<T>;
};

namespace detail {

template<typename T, typename = void>
struct optional_hash_base {
    optional_hash_base() = delete;
};

template<typename T>
struct optional_hash_base<T, void_t<decltype(
    hash<remove_const_t<T>>{}(declval<const remove_const_t<T>&>()))>> {
    using value_type = remove_const_t<T>;

    size_t operator()(const optional<T>& opt) const
        noexcept(noexcept(hash<value_type>{}(*opt)))
    {
        constexpr size_t empty_hash = static_cast<size_t>(-3333);
        return opt.has_value() ? hash<value_type>{}(*opt) : empty_hash;
    }
};

} /* namespace detail */

template<typename T>
struct hash<optional<T>> : detail::optional_hash_base<T> {};

} /* namespace std */

#endif /* defined(__cplusplus) && __cplusplus >= 201703L */
#endif /* RINCXX_OPTIONAL_H */
