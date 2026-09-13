/*
 * RinOS C++ <variant> ✿
 * バリアント型 - active-alternative lifetime subset
 */

#ifndef RINCXX_VARIANT_H
#define RINCXX_VARIANT_H

#include "rincxx.h"
#include "version.h"

/* std::variant is a C++17 facility. */
#if defined(__cplusplus) && __cplusplus >= 201703L

#include "compare.h"
#include "exception.h"
#include "functional.h"
#include "initializer_list.h"
#include "memory.h"
#include "new.h"
#include "type_traits.h"
#include "utility.h"

namespace std {

/*
 * C++20 permits active-union-member lifetime changes during constant
 * evaluation when they go through construct_at.  Keep the C++17 surface
 * available, but do not promise constexpr evaluation before that model.
 */
#if __cplusplus >= 202002L
#define RIN_VARIANT_CONSTEXPR20 constexpr
#else
#define RIN_VARIANT_CONSTEXPR20 inline
#endif

inline constexpr size_t variant_npos = static_cast<size_t>(-1);

struct monostate {};

constexpr bool operator==(monostate, monostate) noexcept { return true; }
constexpr bool operator!=(monostate, monostate) noexcept { return false; }
constexpr bool operator<(monostate, monostate) noexcept { return false; }
constexpr bool operator>(monostate, monostate) noexcept { return false; }
constexpr bool operator<=(monostate, monostate) noexcept { return true; }
constexpr bool operator>=(monostate, monostate) noexcept { return true; }
#if __cplusplus >= 202002L
constexpr strong_ordering operator<=>(monostate, monostate) noexcept {
    return strong_ordering::equal;
}
#endif

class bad_variant_access : public exception {
public:
    const char* what() const noexcept override {
        return "bad variant access";
    }
};

namespace detail {

[[noreturn]] inline void variant_access_fail() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw bad_variant_access();
#else
    __builtin_trap();
#endif
}

template<class T, class... Types>
struct variant_type_index;

template<class T>
struct variant_type_index<T> {
    static constexpr size_t value = variant_npos;
};

template<class T, class Head, class... Tail>
struct variant_type_index<T, Head, Tail...> {
private:
    static constexpr size_t tail = variant_type_index<T, Tail...>::value;

public:
    static constexpr size_t value = is_same<T, Head>::value
        ? 0
        : (tail == variant_npos ? variant_npos : tail + 1);
};

template<class T, class... Types>
inline constexpr size_t variant_type_index_v =
    variant_type_index<T, Types...>::value;

template<class T, class... Types>
inline constexpr size_t variant_type_count_v =
    (static_cast<size_t>(is_same<T, Types>::value) + ... + 0u);

template<class From, size_t I, class To, class = void>
struct variant_overload_candidate {
    static void select() = delete;
};

template<class From, size_t I, class To>
struct variant_overload_candidate<
    From, I, To, void_t<decltype(To{declval<From>()})>> {
    static integral_constant<size_t, I> select(To);
};

template<class From, class Indices, class... Types>
struct variant_overload_set;

template<class From, size_t... I, class... Types>
struct variant_overload_set<From, index_sequence<I...>, Types...>
    : variant_overload_candidate<From, I, Types>... {
    using variant_overload_candidate<From, I, Types>::select...;
};

template<class From, class Enable, class... Types>
struct variant_selected_index_impl {
    static constexpr bool valid = false;
};

template<class From, class... Types>
struct variant_selected_index_impl<
    From,
    void_t<decltype(variant_overload_set<
        From, index_sequence_for<Types...>, Types...>::select(declval<From>()))>,
    Types...>
    : decltype(variant_overload_set<
        From, index_sequence_for<Types...>, Types...>::select(declval<From>())) {
    static constexpr bool valid = true;
};

template<class From, class... Types>
using variant_selected_index =
    variant_selected_index_impl<From, void, Types...>;

/* Avoid probing converting alternatives for a same-variant source.  This is
 * important for recursive alternatives such as map<string, Value>: clang
 * otherwise instantiates the map's pair<Value> while Value is incomplete,
 * even though the converting constructor is disabled below. */
template<bool Enabled, class From, class... Types>
struct variant_selected_index_if {
    static constexpr bool valid = false;
};

template<class From, class... Types>
struct variant_selected_index_if<true, From, Types...>
    : variant_selected_index<From, Types...> {};

template<size_t N, class... Types>
struct variant_type_at;

template<size_t N, class Head, class... Tail>
struct variant_type_at<N, Head, Tail...> {
    using type = typename variant_type_at<N - 1, Tail...>::type;
};

template<class Head, class... Tail>
struct variant_type_at<0, Head, Tail...> {
    using type = Head;
};

template<size_t N, class... Types>
using variant_type_at_t = typename variant_type_at<N, Types...>::type;

/* Converting assignment has to remain a well-formed substitution even when
 * no alternative is viable.  Keeping the selected alternative behind this
 * boolean specialization avoids instantiating variant_type_at_t with the
 * sentinel index while SFINAE is deciding whether operator= participates. */
template<class Selected, class Value, bool Valid, class... Types>
struct variant_conversion_assignment_traits {
    static constexpr bool constructible = false;
};

template<class Selected, class Value, class... Types>
struct variant_conversion_assignment_traits<Selected, Value, true, Types...> {
    using target = variant_type_at_t<Selected::value, Types...>;
    static constexpr bool constructible =
        is_constructible<target, Value>::value;
};

template<size_t... Values>
struct variant_max;

template<size_t Value>
struct variant_max<Value> {
    static constexpr size_t value = Value;
};

template<size_t First, size_t Second, size_t... Rest>
struct variant_max<First, Second, Rest...> {
    static constexpr size_t value =
        variant_max<(First > Second ? First : Second), Rest...>::value;
};

/*
 * A recursive union is deliberately used instead of an aligned byte buffer.
 * It gives a selected alternative a real subobject, which is required for
 * C++20 constant evaluation.  When an alternative needs destruction the
 * union destructor is empty and variant owns the indexed active lifetime;
 * otherwise it is defaulted to preserve trivial special members.
 */
template<size_t I, bool TrivialDestructor, class... Types>
union variant_storage_union;

template<size_t I, class Head>
union variant_storage_union<I, true, Head> {
    char dummy_;
    Head head_;

    constexpr variant_storage_union() noexcept : dummy_{} {}

    template<class... Args>
    constexpr variant_storage_union(in_place_index_t<I>, Args&&... args)
        : head_(std::forward<Args>(args)...) {}

    variant_storage_union(const variant_storage_union&) = default;
    variant_storage_union(variant_storage_union&&) = default;
    variant_storage_union& operator=(const variant_storage_union&) = default;
    variant_storage_union& operator=(variant_storage_union&&) = default;
    ~variant_storage_union() = default;
};

template<size_t I, class Head, class... Tail>
union variant_storage_union<I, true, Head, Tail...> {
    char dummy_;
    Head head_;
    variant_storage_union<I + 1, true, Tail...> tail_;

    constexpr variant_storage_union() noexcept : dummy_{} {}

    template<class... Args>
    constexpr variant_storage_union(in_place_index_t<I>, Args&&... args)
        : head_(std::forward<Args>(args)...) {}

    template<size_t Target, class... Args,
             typename enable_if<(Target != I), int>::type = 0>
    constexpr variant_storage_union(in_place_index_t<Target>, Args&&... args)
        : tail_(in_place_index<Target>, std::forward<Args>(args)...) {}

    variant_storage_union(const variant_storage_union&) = default;
    variant_storage_union(variant_storage_union&&) = default;
    variant_storage_union& operator=(const variant_storage_union&) = default;
    variant_storage_union& operator=(variant_storage_union&&) = default;
    ~variant_storage_union() = default;
};

template<size_t I, class Head>
union variant_storage_union<I, false, Head> {
    char dummy_;
    Head head_;

    constexpr variant_storage_union() noexcept : dummy_{} {}

    template<class... Args>
    constexpr variant_storage_union(in_place_index_t<I>, Args&&... args)
        : head_(std::forward<Args>(args)...) {}

    variant_storage_union(const variant_storage_union&) = default;
    variant_storage_union(variant_storage_union&&) = default;
    variant_storage_union& operator=(const variant_storage_union&) = default;
    variant_storage_union& operator=(variant_storage_union&&) = default;
    RIN_VARIANT_CONSTEXPR20 ~variant_storage_union() {}
};

template<size_t I, class Head, class... Tail>
union variant_storage_union<I, false, Head, Tail...> {
    char dummy_;
    Head head_;
    variant_storage_union<I + 1, false, Tail...> tail_;

    constexpr variant_storage_union() noexcept : dummy_{} {}

    template<class... Args>
    constexpr variant_storage_union(in_place_index_t<I>, Args&&... args)
        : head_(std::forward<Args>(args)...) {}

    template<size_t Target, class... Args,
             typename enable_if<(Target != I), int>::type = 0>
    constexpr variant_storage_union(in_place_index_t<Target>, Args&&... args)
        : tail_(in_place_index<Target>, std::forward<Args>(args)...) {}

    variant_storage_union(const variant_storage_union&) = default;
    variant_storage_union(variant_storage_union&&) = default;
    variant_storage_union& operator=(const variant_storage_union&) = default;
    variant_storage_union& operator=(variant_storage_union&&) = default;
    RIN_VARIANT_CONSTEXPR20 ~variant_storage_union() {}
};

template<size_t Target, size_t I, bool TrivialDestructor, class Head,
         class... Tail>
constexpr decltype(auto) variant_storage_get(
    variant_storage_union<I, TrivialDestructor, Head, Tail...>& storage) noexcept {
    if constexpr (Target == I) {
        return (storage.head_);
    } else {
        return variant_storage_get<Target>(storage.tail_);
    }
}

template<size_t Target, size_t I, bool TrivialDestructor, class Head,
         class... Tail>
constexpr decltype(auto) variant_storage_get(
    const variant_storage_union<I, TrivialDestructor, Head, Tail...>& storage) noexcept {
    if constexpr (Target == I) {
        return (storage.head_);
    } else {
        return variant_storage_get<Target>(storage.tail_);
    }
}

template<class... Types>
struct variant_storage {
    using union_type = variant_storage_union<
        0, conjunction<is_trivially_destructible<Types>...>::value, Types...>;

    union_type data_;

    constexpr variant_storage() noexcept : data_() {}

    template<size_t I, class... Args>
    constexpr variant_storage(in_place_index_t<I>, Args&&... args)
        : data_(in_place_index<I>, std::forward<Args>(args)...) {}

    template<size_t I>
    constexpr variant_type_at_t<I, Types...>& get() noexcept {
        return variant_storage_get<I>(data_);
    }

    template<size_t I>
    constexpr const variant_type_at_t<I, Types...>& get() const noexcept {
        return variant_storage_get<I>(data_);
    }
};

/* The common owner contains all active-member lifetime work. */
template<class... Types>
struct variant_lifetime_base {
    variant_storage<Types...> storage_;
    size_t index_ = variant_npos;

    constexpr variant_lifetime_base() noexcept = default;

    template<size_t I, class... Args>
    constexpr variant_lifetime_base(in_place_index_t<I>, Args&&... args)
        : storage_(in_place_index<I>, std::forward<Args>(args)...), index_(I) {}

    template<size_t I = 0>
    RIN_VARIANT_CONSTEXPR20 void destroy_at_index() noexcept {
        if constexpr (I < sizeof...(Types)) {
            if (index_ == I) {
                using T = variant_type_at_t<I, Types...>;
                storage_.template get<I>().~T();
            } else {
                destroy_at_index<I + 1>();
            }
        }
    }

    RIN_VARIANT_CONSTEXPR20 void destroy_active() noexcept {
        if (index_ != variant_npos) {
            destroy_at_index();
            index_ = variant_npos;
        }
    }

    template<size_t I, class... Args>
    RIN_VARIANT_CONSTEXPR20 variant_type_at_t<I, Types...>*
    construct_at_index(Args&&... args) {
#if __cplusplus >= 202002L
        return std::construct_at(&storage_.template get<I>(),
                                 std::forward<Args>(args)...);
#else
        using T = variant_type_at_t<I, Types...>;
        return ::new (static_cast<void*>(&storage_.template get<I>()))
            T(std::forward<Args>(args)...);
#endif
    }

    template<size_t I = 0>
    RIN_VARIANT_CONSTEXPR20 void copy_construct_active(
        const variant_lifetime_base& other) {
        if constexpr (I < sizeof...(Types)) {
            if (other.index_ == I) {
                construct_at_index<I>(other.storage_.template get<I>());
                index_ = I;
            } else {
                copy_construct_active<I + 1>(other);
            }
        }
    }

    template<size_t I = 0>
    RIN_VARIANT_CONSTEXPR20 void move_construct_active(
        variant_lifetime_base& other) {
        if constexpr (I < sizeof...(Types)) {
            if (other.index_ == I) {
                construct_at_index<I>(
                    std::move(other.storage_.template get<I>()));
                index_ = I;
            } else {
                move_construct_active<I + 1>(other);
            }
        }
    }

    template<size_t I = 0>
    RIN_VARIANT_CONSTEXPR20 void copy_assign_active(
        const variant_lifetime_base& other) {
        if constexpr (I < sizeof...(Types)) {
            if (index_ == I) {
                storage_.template get<I>() = other.storage_.template get<I>();
            } else {
                copy_assign_active<I + 1>(other);
            }
        }
    }

    template<size_t I = 0>
    RIN_VARIANT_CONSTEXPR20 void move_assign_active(
        variant_lifetime_base& other) {
        if constexpr (I < sizeof...(Types)) {
            if (index_ == I) {
                storage_.template get<I>() =
                    std::move(other.storage_.template get<I>());
            } else {
                move_assign_active<I + 1>(other);
            }
        }
    }

    template<size_t I = 0>
    RIN_VARIANT_CONSTEXPR20 void swap_same_active(
        variant_lifetime_base& other) {
        if constexpr (I < sizeof...(Types)) {
            if (index_ == I) {
                using std::swap;
                swap(storage_.template get<I>(),
                     other.storage_.template get<I>());
            } else {
                swap_same_active<I + 1>(other);
            }
        }
    }
};

#if __cplusplus < 202002L
/*
 * C++17 cannot constrain non-template special members.  These bases carry
 * the standard availability rules so the derived variant has deleted copy or
 * move operations when an alternative does, rather than failing only after a
 * caller instantiates a function body.
 */
template<bool Trivial, class... Types>
struct variant_cxx17_destroy_base;

template<class... Types>
struct variant_cxx17_destroy_base<false, Types...>
    : variant_lifetime_base<Types...> {
    using base = variant_lifetime_base<Types...>;
    using base::base;
    variant_cxx17_destroy_base() = default;
    variant_cxx17_destroy_base(const variant_cxx17_destroy_base&) = default;
    variant_cxx17_destroy_base(variant_cxx17_destroy_base&&) = default;
    variant_cxx17_destroy_base& operator=(const variant_cxx17_destroy_base&) =
        default;
    variant_cxx17_destroy_base& operator=(variant_cxx17_destroy_base&&) =
        default;
    ~variant_cxx17_destroy_base() { this->destroy_active(); }
};

template<class... Types>
struct variant_cxx17_destroy_base<true, Types...>
    : variant_lifetime_base<Types...> {
    using base = variant_lifetime_base<Types...>;
    using base::base;
    variant_cxx17_destroy_base() = default;
    variant_cxx17_destroy_base(const variant_cxx17_destroy_base&) = default;
    variant_cxx17_destroy_base(variant_cxx17_destroy_base&&) = default;
    variant_cxx17_destroy_base& operator=(const variant_cxx17_destroy_base&) =
        default;
    variant_cxx17_destroy_base& operator=(variant_cxx17_destroy_base&&) =
        default;
    ~variant_cxx17_destroy_base() = default;
};

template<bool Copyable, bool Trivial, class... Types>
struct variant_cxx17_copy_ctor_base;

template<class... Types>
struct variant_cxx17_copy_ctor_base<true, true, Types...>
    : variant_cxx17_destroy_base<
          conjunction<is_trivially_destructible<Types>...>::value, Types...> {
    using base = variant_cxx17_destroy_base<
        conjunction<is_trivially_destructible<Types>...>::value, Types...>;
    using base::base;
    variant_cxx17_copy_ctor_base() = default;
    variant_cxx17_copy_ctor_base(const variant_cxx17_copy_ctor_base&) = default;
    variant_cxx17_copy_ctor_base(variant_cxx17_copy_ctor_base&&) = default;
    variant_cxx17_copy_ctor_base& operator=(
        const variant_cxx17_copy_ctor_base&) = default;
    variant_cxx17_copy_ctor_base& operator=(
        variant_cxx17_copy_ctor_base&&) = default;
};

template<class... Types>
struct variant_cxx17_copy_ctor_base<true, false, Types...>
    : variant_cxx17_destroy_base<
          conjunction<is_trivially_destructible<Types>...>::value, Types...> {
    using base = variant_cxx17_destroy_base<
        conjunction<is_trivially_destructible<Types>...>::value, Types...>;
    using base::base;
    variant_cxx17_copy_ctor_base() = default;
    variant_cxx17_copy_ctor_base(const variant_cxx17_copy_ctor_base& other) noexcept(
        conjunction<is_nothrow_copy_constructible<Types>...>::value)
        : base() {
        if (other.index_ != variant_npos) {
            this->copy_construct_active(other);
        }
    }
    variant_cxx17_copy_ctor_base(variant_cxx17_copy_ctor_base&&) = default;
    variant_cxx17_copy_ctor_base& operator=(
        const variant_cxx17_copy_ctor_base&) = default;
    variant_cxx17_copy_ctor_base& operator=(
        variant_cxx17_copy_ctor_base&&) = default;
};

template<bool Trivial, class... Types>
struct variant_cxx17_copy_ctor_base<false, Trivial, Types...>
    : variant_cxx17_destroy_base<
          conjunction<is_trivially_destructible<Types>...>::value, Types...> {
    using base = variant_cxx17_destroy_base<
        conjunction<is_trivially_destructible<Types>...>::value, Types...>;
    using base::base;
    variant_cxx17_copy_ctor_base() = default;
    variant_cxx17_copy_ctor_base(const variant_cxx17_copy_ctor_base&) = delete;
    variant_cxx17_copy_ctor_base(variant_cxx17_copy_ctor_base&&) = default;
    variant_cxx17_copy_ctor_base& operator=(
        const variant_cxx17_copy_ctor_base&) = default;
    variant_cxx17_copy_ctor_base& operator=(
        variant_cxx17_copy_ctor_base&&) = default;
};

template<bool Moveable, bool Trivial, class... Types>
struct variant_cxx17_move_ctor_base;

template<class... Types>
struct variant_cxx17_move_ctor_base<true, true, Types...>
    : variant_cxx17_copy_ctor_base<
          conjunction<is_copy_constructible<Types>...>::value,
          conjunction<is_trivially_copy_constructible<Types>...>::value,
          Types...> {
    using base = variant_cxx17_copy_ctor_base<
        conjunction<is_copy_constructible<Types>...>::value,
        conjunction<is_trivially_copy_constructible<Types>...>::value,
        Types...>;
    using base::base;
    variant_cxx17_move_ctor_base() = default;
    variant_cxx17_move_ctor_base(variant_cxx17_move_ctor_base&&) = default;
    variant_cxx17_move_ctor_base(const variant_cxx17_move_ctor_base&) = default;
    variant_cxx17_move_ctor_base& operator=(
        const variant_cxx17_move_ctor_base&) = default;
    variant_cxx17_move_ctor_base& operator=(
        variant_cxx17_move_ctor_base&&) = default;
};

template<class... Types>
struct variant_cxx17_move_ctor_base<true, false, Types...>
    : variant_cxx17_copy_ctor_base<
          conjunction<is_copy_constructible<Types>...>::value,
          conjunction<is_trivially_copy_constructible<Types>...>::value,
          Types...> {
    using base = variant_cxx17_copy_ctor_base<
        conjunction<is_copy_constructible<Types>...>::value,
        conjunction<is_trivially_copy_constructible<Types>...>::value,
        Types...>;
    using base::base;
    variant_cxx17_move_ctor_base() = default;
    variant_cxx17_move_ctor_base(variant_cxx17_move_ctor_base&& other) noexcept(
        conjunction<is_nothrow_move_constructible<Types>...>::value)
        : base() {
        if (other.index_ != variant_npos) {
            this->move_construct_active(other);
        }
    }
    variant_cxx17_move_ctor_base(const variant_cxx17_move_ctor_base&) = default;
    variant_cxx17_move_ctor_base& operator=(
        const variant_cxx17_move_ctor_base&) = default;
    variant_cxx17_move_ctor_base& operator=(
        variant_cxx17_move_ctor_base&&) = default;
};

template<bool Trivial, class... Types>
struct variant_cxx17_move_ctor_base<false, Trivial, Types...>
    : variant_cxx17_copy_ctor_base<
          conjunction<is_copy_constructible<Types>...>::value,
          conjunction<is_trivially_copy_constructible<Types>...>::value,
          Types...> {
    using base = variant_cxx17_copy_ctor_base<
        conjunction<is_copy_constructible<Types>...>::value,
        conjunction<is_trivially_copy_constructible<Types>...>::value,
        Types...>;
    using base::base;
    variant_cxx17_move_ctor_base() = default;
    variant_cxx17_move_ctor_base(variant_cxx17_move_ctor_base&&) = delete;
    variant_cxx17_move_ctor_base(const variant_cxx17_move_ctor_base&) = default;
    variant_cxx17_move_ctor_base& operator=(
        const variant_cxx17_move_ctor_base&) = default;
    variant_cxx17_move_ctor_base& operator=(
        variant_cxx17_move_ctor_base&&) = default;
};

template<class... Types>
inline constexpr bool variant_cxx17_trivially_copy_assignable_v =
    conjunction<is_trivially_copy_constructible<Types>...,
                is_trivially_copy_assignable<Types>...,
                is_trivially_destructible<Types>...>::value;

template<class... Types>
inline constexpr bool variant_cxx17_trivially_move_assignable_v =
    conjunction<is_trivially_move_constructible<Types>...,
                is_trivially_move_assignable<Types>...,
                is_trivially_destructible<Types>...>::value;

template<bool CopyAssignable, bool Trivial, class... Types>
struct variant_cxx17_copy_assign_base;

template<class... Types>
struct variant_cxx17_copy_assign_base<true, true, Types...>
    : variant_cxx17_move_ctor_base<
          conjunction<is_move_constructible<Types>...>::value,
          conjunction<is_trivially_move_constructible<Types>...>::value,
          Types...> {
    using base = variant_cxx17_move_ctor_base<
        conjunction<is_move_constructible<Types>...>::value,
        conjunction<is_trivially_move_constructible<Types>...>::value,
        Types...>;
    using base::base;
    variant_cxx17_copy_assign_base() = default;
    variant_cxx17_copy_assign_base& operator=(
        const variant_cxx17_copy_assign_base&) = default;
    variant_cxx17_copy_assign_base(const variant_cxx17_copy_assign_base&) =
        default;
    variant_cxx17_copy_assign_base(variant_cxx17_copy_assign_base&&) = default;
    variant_cxx17_copy_assign_base& operator=(
        variant_cxx17_copy_assign_base&&) = default;
};

template<class... Types>
struct variant_cxx17_copy_assign_base<true, false, Types...>
    : variant_cxx17_move_ctor_base<
          conjunction<is_move_constructible<Types>...>::value,
          conjunction<is_trivially_move_constructible<Types>...>::value,
          Types...> {
    using base = variant_cxx17_move_ctor_base<
        conjunction<is_move_constructible<Types>...>::value,
        conjunction<is_trivially_move_constructible<Types>...>::value,
        Types...>;
    using base::base;
    variant_cxx17_copy_assign_base() = default;
    variant_cxx17_copy_assign_base& operator=(
        const variant_cxx17_copy_assign_base& other) {
        if (this == &other) return *this;
        if (other.index_ == variant_npos) {
            this->destroy_active();
        } else if (this->index_ == other.index_) {
            this->copy_assign_active(other);
        } else {
            this->destroy_active();
            this->copy_construct_active(other);
        }
        return *this;
    }
    variant_cxx17_copy_assign_base(const variant_cxx17_copy_assign_base&) = default;
    variant_cxx17_copy_assign_base(variant_cxx17_copy_assign_base&&) = default;
    variant_cxx17_copy_assign_base& operator=(
        variant_cxx17_copy_assign_base&&) = default;
};

template<bool Trivial, class... Types>
struct variant_cxx17_copy_assign_base<false, Trivial, Types...>
    : variant_cxx17_move_ctor_base<
          conjunction<is_move_constructible<Types>...>::value,
          conjunction<is_trivially_move_constructible<Types>...>::value,
          Types...> {
    using base = variant_cxx17_move_ctor_base<
        conjunction<is_move_constructible<Types>...>::value,
        conjunction<is_trivially_move_constructible<Types>...>::value,
        Types...>;
    using base::base;
    variant_cxx17_copy_assign_base() = default;
    variant_cxx17_copy_assign_base(const variant_cxx17_copy_assign_base&) = default;
    variant_cxx17_copy_assign_base(variant_cxx17_copy_assign_base&&) = default;
    variant_cxx17_copy_assign_base& operator=(
        const variant_cxx17_copy_assign_base&) = delete;
    variant_cxx17_copy_assign_base& operator=(
        variant_cxx17_copy_assign_base&&) = default;
};

template<bool MoveAssignable, bool Trivial, class... Types>
struct variant_cxx17_move_assign_base;

template<class... Types>
struct variant_cxx17_move_assign_base<true, true, Types...>
    : variant_cxx17_copy_assign_base<
          conjunction<is_copy_constructible<Types>...,
                      is_copy_assignable<Types>...>::value,
          variant_cxx17_trivially_copy_assignable_v<Types...>, Types...> {
    using base = variant_cxx17_copy_assign_base<
        conjunction<is_copy_constructible<Types>...,
                    is_copy_assignable<Types>...>::value,
        variant_cxx17_trivially_copy_assignable_v<Types...>, Types...>;
    using base::base;
    variant_cxx17_move_assign_base() = default;
    variant_cxx17_move_assign_base& operator=(
        variant_cxx17_move_assign_base&&) = default;
    variant_cxx17_move_assign_base(const variant_cxx17_move_assign_base&) =
        default;
    variant_cxx17_move_assign_base(variant_cxx17_move_assign_base&&) = default;
    variant_cxx17_move_assign_base& operator=(
        const variant_cxx17_move_assign_base&) = default;
};

template<class... Types>
struct variant_cxx17_move_assign_base<true, false, Types...>
    : variant_cxx17_copy_assign_base<
          conjunction<is_copy_constructible<Types>...,
                      is_copy_assignable<Types>...>::value,
          variant_cxx17_trivially_copy_assignable_v<Types...>, Types...> {
    using base = variant_cxx17_copy_assign_base<
        conjunction<is_copy_constructible<Types>...,
                    is_copy_assignable<Types>...>::value,
        variant_cxx17_trivially_copy_assignable_v<Types...>, Types...>;
    using base::base;
    variant_cxx17_move_assign_base() = default;
    variant_cxx17_move_assign_base& operator=(
        variant_cxx17_move_assign_base&& other) noexcept(
            conjunction<is_nothrow_move_constructible<Types>...,
                        is_nothrow_move_assignable<Types>...>::value) {
        if (this == &other) return *this;
        if (other.index_ == variant_npos) {
            this->destroy_active();
        } else if (this->index_ == other.index_) {
            this->move_assign_active(other);
        } else {
            this->destroy_active();
            this->move_construct_active(other);
        }
        return *this;
    }
    variant_cxx17_move_assign_base(const variant_cxx17_move_assign_base&) = default;
    variant_cxx17_move_assign_base(variant_cxx17_move_assign_base&&) = default;
    variant_cxx17_move_assign_base& operator=(
        const variant_cxx17_move_assign_base&) = default;
};

template<bool Trivial, class... Types>
struct variant_cxx17_move_assign_base<false, Trivial, Types...>
    : variant_cxx17_copy_assign_base<
          conjunction<is_copy_constructible<Types>...,
                      is_copy_assignable<Types>...>::value,
          variant_cxx17_trivially_copy_assignable_v<Types...>, Types...> {
    using base = variant_cxx17_copy_assign_base<
        conjunction<is_copy_constructible<Types>...,
                    is_copy_assignable<Types>...>::value,
        variant_cxx17_trivially_copy_assignable_v<Types...>, Types...>;
    using base::base;
    variant_cxx17_move_assign_base() = default;
    variant_cxx17_move_assign_base(const variant_cxx17_move_assign_base&) = default;
    variant_cxx17_move_assign_base(variant_cxx17_move_assign_base&&) = default;
    variant_cxx17_move_assign_base& operator=(
        const variant_cxx17_move_assign_base&) = default;
    variant_cxx17_move_assign_base& operator=(
        variant_cxx17_move_assign_base&&) = delete;
};

template<class... Types>
using variant_cxx17_base = variant_cxx17_move_assign_base<
    conjunction<is_move_constructible<Types>...,
                is_move_assignable<Types>...>::value,
    variant_cxx17_trivially_move_assignable_v<Types...>, Types...>;
#endif

#if __cplusplus >= 202002L
template<class... Types>
using variant_base = variant_lifetime_base<Types...>;
#else
template<class... Types>
using variant_base = variant_cxx17_base<Types...>;
#endif

template<class T, class = void>
struct variant_equality_traits {
    static constexpr bool valid = false;
    static constexpr bool nothrow = false;
};

template<class T>
struct variant_equality_traits<
    T, void_t<decltype(declval<const T&>() == declval<const T&>())>> {
    using result_type =
        decltype(declval<const T&>() == declval<const T&>());
    static constexpr bool valid = is_convertible<result_type, bool>::value;
    static constexpr bool nothrow =
        valid && noexcept(declval<const T&>() == declval<const T&>());
};

template<class T, class = void>
struct variant_less_traits {
    static constexpr bool valid = false;
    static constexpr bool nothrow = false;
};

template<class T>
struct variant_less_traits<
    T, void_t<decltype(declval<const T&>() < declval<const T&>())>> {
    using result_type =
        decltype(declval<const T&>() < declval<const T&>());
    static constexpr bool valid = is_convertible<result_type, bool>::value;
    static constexpr bool nothrow =
        valid && noexcept(declval<const T&>() < declval<const T&>());
};

#if __cplusplus >= 202002L
template<class T, class = void>
struct variant_three_way_traits {
    static constexpr bool valid = false;
    static constexpr bool nothrow = false;
};

template<class T>
struct variant_three_way_traits<T, void_t<compare_three_way_result_t<T>>> {
    static constexpr bool valid = three_way_comparable<T>;
    static constexpr bool nothrow =
        valid && noexcept(declval<const T&>() <=> declval<const T&>());
};
#endif

} /* namespace detail */

template<class T, class Enable = void>
struct variant_size;

template<class... Types>
class variant;

template<class... Types>
struct variant_size<variant<Types...>>
    : integral_constant<size_t, sizeof...(Types)> {};

template<class T>
struct variant_size<const T> : variant_size<T> {};

template<class T>
struct variant_size<volatile T> : variant_size<T> {};

template<class T>
struct variant_size<const volatile T> : variant_size<T> {};

#if __cplusplus > 202002L
template<class T>
struct variant_size<T, void_t<typename T::__rincxx_variant_tag>>
    : variant_size<typename T::__rincxx_variant_tag> {};
#endif

template<class T>
inline constexpr size_t variant_size_v = variant_size<T>::value;

template<size_t I, class T, class Enable = void>
struct variant_alternative;

template<size_t I, class... Types>
struct variant_alternative<I, variant<Types...>> {
    using type = detail::variant_type_at_t<I, Types...>;
};

template<size_t I, class T>
struct variant_alternative<I, const T> {
    using type = add_const_t<typename variant_alternative<I, T>::type>;
};

template<size_t I, class T>
struct variant_alternative<I, volatile T> {
    using type = add_volatile_t<typename variant_alternative<I, T>::type>;
};

template<size_t I, class T>
struct variant_alternative<I, const volatile T> {
    using type = add_cv_t<typename variant_alternative<I, T>::type>;
};

#if __cplusplus > 202002L
template<size_t I, class T>
struct variant_alternative<I, T, void_t<typename T::__rincxx_variant_tag>>
    : variant_alternative<I, typename T::__rincxx_variant_tag> {};
#endif

template<size_t I, class T>
using variant_alternative_t = typename variant_alternative<I, T>::type;

template<class... Types>
class variant : private detail::variant_base<Types...> {
    using base = detail::variant_base<Types...>;
    using base::storage_;
    using base::index_;
    using base::destroy_active;
    using base::copy_construct_active;
    using base::move_construct_active;
    using base::copy_assign_active;
    using base::move_assign_active;
    using base::swap_same_active;
    using base::construct_at_index;

    static_assert(sizeof...(Types) > 0,
                  "variant must have at least one alternative");
    static_assert((!is_void<Types>::value && ...),
                  "variant alternatives cannot be void");
    static_assert((!is_reference<Types>::value && ...),
                  "variant alternatives cannot be references");
    static_assert((!is_array<Types>::value && ...),
                  "variant alternatives cannot be arrays");
    static_assert((is_destructible<Types>::value && ...),
                  "variant alternatives must be destructible");

    template<size_t I, class... Ts>
    friend constexpr variant_alternative_t<I, variant<Ts...>>&
    get(variant<Ts...>& value);

    template<size_t I, class... Ts>
    friend constexpr const variant_alternative_t<I, variant<Ts...>>&
    get(const variant<Ts...>& value);

    template<size_t I, class... Ts>
    friend constexpr variant_alternative_t<I, variant<Ts...>>*
    get_if(variant<Ts...>* value) noexcept;

    template<size_t I, class... Ts>
    friend constexpr const variant_alternative_t<I, variant<Ts...>>*
    get_if(const variant<Ts...>* value) noexcept;

public:
#if __cplusplus > 202002L
    using __rincxx_variant_tag = variant;
#endif

    RIN_VARIANT_CONSTEXPR20 variant() noexcept(
        is_nothrow_default_constructible<
            detail::variant_type_at_t<0, Types...>>::value)
#if __cplusplus >= 202002L
        requires is_default_constructible<
            detail::variant_type_at_t<0, Types...>>::value
#endif
        : base(in_place_index<0>) {}

#if __cplusplus >= 202002L
    variant()
        requires (!is_default_constructible<
                  detail::variant_type_at_t<0, Types...>>::value) = delete;
#endif

#if __cplusplus >= 202002L
    RIN_VARIANT_CONSTEXPR20 variant(const variant& other) noexcept(
        conjunction<is_nothrow_copy_constructible<Types>...>::value)
        requires (conjunction<is_copy_constructible<Types>...>::value &&
                  conjunction<is_trivially_copy_constructible<Types>...>::value) = default;

    RIN_VARIANT_CONSTEXPR20 variant(const variant& other) noexcept(
        conjunction<is_nothrow_copy_constructible<Types>...>::value)
        requires (conjunction<is_copy_constructible<Types>...>::value &&
                  !conjunction<is_trivially_copy_constructible<Types>...>::value)
        : base()
    {
        if (!other.valueless_by_exception()) {
            copy_construct_active(other);
        }
    }

    variant(const variant&)
        requires (!conjunction<is_copy_constructible<Types>...>::value) = delete;

    RIN_VARIANT_CONSTEXPR20 variant(variant&& other) noexcept(
        conjunction<is_nothrow_move_constructible<Types>...>::value)
        requires (conjunction<is_move_constructible<Types>...>::value &&
                  conjunction<is_trivially_move_constructible<Types>...>::value) = default;

    RIN_VARIANT_CONSTEXPR20 variant(variant&& other) noexcept(
        conjunction<is_nothrow_move_constructible<Types>...>::value)
        requires (conjunction<is_move_constructible<Types>...>::value &&
                  !conjunction<is_trivially_move_constructible<Types>...>::value)
        : base()
    {
        if (!other.valueless_by_exception()) {
            move_construct_active(other);
        }
    }

    variant(variant&&)
        requires (!conjunction<is_move_constructible<Types>...>::value) = delete;
#endif

#if __cplusplus >= 202002L
    template<class T,
             class Selected = detail::variant_selected_index_if<
                 !is_same<typename decay<T>::type, variant>::value,
                 T&&, Types...>,
             typename enable_if<
                 !is_same<typename decay<T>::type, variant>::value &&
                 Selected::valid,
                 int
             >::type = 0>
    explicit(!is_convertible<
                 T&&,
                 detail::variant_type_at_t<Selected::value, Types...>>::value)
    RIN_VARIANT_CONSTEXPR20 variant(T&& value) noexcept(
        is_nothrow_constructible<detail::variant_type_at_t<Selected::value,
                                                           Types...>, T&&>::value)
        : base(in_place_index<Selected::value>, std::forward<T>(value)) {}
#else
    template<class T,
             class Selected = detail::variant_selected_index_if<
                 !is_same<typename decay<T>::type, variant>::value,
                 T&&, Types...>,
             typename enable_if<
                 !is_same<typename decay<T>::type, variant>::value &&
                 Selected::valid &&
                 is_convertible<
                     T&&,
                     detail::variant_type_at_t<Selected::value,
                                                Types...>>::value,
                 int
             >::type = 0>
    RIN_VARIANT_CONSTEXPR20 variant(T&& value) noexcept(
        is_nothrow_constructible<detail::variant_type_at_t<Selected::value,
                                                           Types...>, T&&>::value)
        : base(in_place_index<Selected::value>, std::forward<T>(value)) {}

    template<class T,
             class Selected = detail::variant_selected_index_if<
                 !is_same<typename decay<T>::type, variant>::value,
                 T&&, Types...>,
             typename enable_if<
                 !is_same<typename decay<T>::type, variant>::value &&
                 Selected::valid &&
                 !is_convertible<
                     T&&,
                     detail::variant_type_at_t<Selected::value,
                                                Types...>>::value,
                 int
             >::type = 0>
    explicit RIN_VARIANT_CONSTEXPR20 variant(T&& value) noexcept(
        is_nothrow_constructible<detail::variant_type_at_t<Selected::value,
                                                           Types...>, T&&>::value)
        : base(in_place_index<Selected::value>, std::forward<T>(value)) {}
#endif

    template<size_t I, class... Args,
             typename enable_if<
                 I < sizeof...(Types) &&
                 is_constructible<detail::variant_type_at_t<I, Types...>,
                                  Args&&...>::value,
                 int
             >::type = 0>
    RIN_VARIANT_CONSTEXPR20 explicit variant(in_place_index_t<I>, Args&&... args) noexcept(
        is_nothrow_constructible<detail::variant_type_at_t<I, Types...>,
                                 Args&&...>::value)
        : base(in_place_index<I>, std::forward<Args>(args)...) {}

    template<size_t I, class U, class... Args,
             typename enable_if<
                 I < sizeof...(Types) &&
                 is_constructible<detail::variant_type_at_t<I, Types...>,
                                  initializer_list<U>&, Args&&...>::value,
                 int
             >::type = 0>
    RIN_VARIANT_CONSTEXPR20 explicit variant(in_place_index_t<I>,
                                             initializer_list<U> values,
                                             Args&&... args) noexcept(
        is_nothrow_constructible<detail::variant_type_at_t<I, Types...>,
                                 initializer_list<U>&, Args&&...>::value)
        : base(in_place_index<I>, values, std::forward<Args>(args)...) {}

    template<class T, class... Args,
             typename enable_if<
                 detail::variant_type_count_v<T, Types...> == 1 &&
                 is_constructible<T, Args&&...>::value,
                 int
             >::type = 0>
    RIN_VARIANT_CONSTEXPR20 explicit variant(in_place_type_t<T>, Args&&... args) noexcept(
        is_nothrow_constructible<T, Args&&...>::value)
        : variant(in_place_index<detail::variant_type_index_v<T, Types...>>,
                  std::forward<Args>(args)...) {}

    template<class T, class U, class... Args,
             typename enable_if<
                 detail::variant_type_count_v<T, Types...> == 1 &&
                 is_constructible<T, initializer_list<U>&, Args&&...>::value,
                 int
             >::type = 0>
    RIN_VARIANT_CONSTEXPR20 explicit variant(in_place_type_t<T>,
                                             initializer_list<U> values,
                                             Args&&... args) noexcept(
        is_nothrow_constructible<T, initializer_list<U>&, Args&&...>::value)
        : variant(in_place_index<detail::variant_type_index_v<T, Types...>>,
                  values, std::forward<Args>(args)...) {}

#if __cplusplus >= 202002L
    RIN_VARIANT_CONSTEXPR20 ~variant()
        requires conjunction<is_trivially_destructible<Types>...>::value = default;

    RIN_VARIANT_CONSTEXPR20 ~variant()
        requires (!conjunction<is_trivially_destructible<Types>...>::value) {
        destroy_active();
    }

    RIN_VARIANT_CONSTEXPR20 variant& operator=(const variant& other)
        requires conjunction<
            is_copy_constructible<Types>...,
            is_copy_assignable<Types>...,
            is_trivially_copy_constructible<Types>...,
            is_trivially_copy_assignable<Types>...,
            is_trivially_destructible<Types>...>::value = default;

    RIN_VARIANT_CONSTEXPR20 variant& operator=(const variant& other)
        requires (conjunction<
                      is_copy_constructible<Types>...,
                      is_copy_assignable<Types>...>::value &&
                  !conjunction<
                      is_trivially_copy_constructible<Types>...,
                      is_trivially_copy_assignable<Types>...,
                      is_trivially_destructible<Types>...>::value)
    {
        if (this == &other) return *this;
        if (other.valueless_by_exception()) {
            destroy_active();
        } else if (index_ == other.index_) {
            copy_assign_active(other);
        } else {
            destroy_active();
            copy_construct_active(other);
        }
        return *this;
    }

    variant& operator=(const variant&)
        requires (!conjunction<
                  is_copy_constructible<Types>...,
                  is_copy_assignable<Types>...>::value) = delete;

    RIN_VARIANT_CONSTEXPR20 variant& operator=(variant&& other) noexcept(
        conjunction<
            is_nothrow_move_constructible<Types>...,
            is_nothrow_move_assignable<Types>...>::value)
        requires conjunction<
            is_move_constructible<Types>...,
            is_move_assignable<Types>...,
            is_trivially_move_constructible<Types>...,
            is_trivially_move_assignable<Types>...,
            is_trivially_destructible<Types>...>::value = default;

    RIN_VARIANT_CONSTEXPR20 variant& operator=(variant&& other) noexcept(
        conjunction<
            is_nothrow_move_constructible<Types>...,
            is_nothrow_move_assignable<Types>...>::value)
        requires (conjunction<
                      is_move_constructible<Types>...,
                      is_move_assignable<Types>...>::value &&
                  !conjunction<
                      is_trivially_move_constructible<Types>...,
                      is_trivially_move_assignable<Types>...,
                      is_trivially_destructible<Types>...>::value)
    {
        if (this == &other) return *this;
        if (other.valueless_by_exception()) {
            destroy_active();
        } else if (index_ == other.index_) {
            move_assign_active(other);
        } else {
            destroy_active();
            move_construct_active(other);
        }
        return *this;
    }

    variant& operator=(variant&&)
        requires (!conjunction<
                  is_move_constructible<Types>...,
                  is_move_assignable<Types>...>::value) = delete;
#endif

    template<class T,
             class Selected = detail::variant_selected_index_if<
                 !is_same<typename decay<T>::type, variant>::value,
                 T&&, Types...>,
             class Conversion = detail::variant_conversion_assignment_traits<
                 Selected, T&&, Selected::valid, Types...>,
             typename enable_if<
                 !is_same<typename decay<T>::type, variant>::value &&
                 Selected::valid &&
                 Conversion::constructible,
                 int
             >::type = 0>
    RIN_VARIANT_CONSTEXPR20 variant& operator=(T&& value) noexcept(
        is_nothrow_assignable<detail::variant_type_at_t<Selected::value,
                                                        Types...>&,
                              T&&>::value &&
        is_nothrow_constructible<detail::variant_type_at_t<Selected::value,
                                                            Types...>,
                                 T&&>::value) {
        constexpr size_t I = Selected::value;
        using Target = detail::variant_type_at_t<I, Types...>;
        if (index_ == I) {
            if constexpr (is_assignable<Target&, T&&>::value) {
                storage_.template get<I>() = std::forward<T>(value);
            } else if constexpr (is_nothrow_constructible<
                                     Target, T&&>::value &&
                                 is_nothrow_move_constructible<
                                     Target>::value) {
                Target temporary(std::forward<T>(value));
                destroy_active();
                this->template construct_at_index<I>(std::move(temporary));
                index_ = I;
            } else {
                destroy_active();
                this->template construct_at_index<I>(
                    std::forward<T>(value));
                index_ = I;
            }
        } else {
            if constexpr (!is_nothrow_constructible<Target, T&&>::value &&
                          is_nothrow_move_constructible<Target>::value) {
                Target temporary(std::forward<T>(value));
                destroy_active();
                this->template construct_at_index<I>(std::move(temporary));
            } else {
                destroy_active();
                this->template construct_at_index<I>(std::forward<T>(value));
            }
            index_ = I;
        }
        return *this;
    }

    constexpr size_t index() const noexcept { return index_; }

    constexpr bool valueless_by_exception() const noexcept {
        return index_ == variant_npos;
    }

    template<size_t I, class... Args>
    RIN_VARIANT_CONSTEXPR20 typename enable_if<
        I < sizeof...(Types) &&
        is_constructible<detail::variant_type_at_t<I, Types...>,
                         Args&&...>::value,
        detail::variant_type_at_t<I, Types...>&
    >::type emplace(Args&&... args) {
        destroy_active();
        auto* result = this->template construct_at_index<I>(
            std::forward<Args>(args)...);
        index_ = I;
        return *result;
    }

    template<size_t I, class U, class... Args>
    RIN_VARIANT_CONSTEXPR20 typename enable_if<
        I < sizeof...(Types) &&
        is_constructible<detail::variant_type_at_t<I, Types...>,
                         initializer_list<U>&, Args&&...>::value,
        detail::variant_type_at_t<I, Types...>&
    >::type emplace(initializer_list<U> values, Args&&... args) {
        destroy_active();
        auto* result = this->template construct_at_index<I>(
            values, std::forward<Args>(args)...);
        index_ = I;
        return *result;
    }

    template<class T, class... Args>
    RIN_VARIANT_CONSTEXPR20 typename enable_if<
        detail::variant_type_count_v<T, Types...> == 1 &&
        is_constructible<T, Args&&...>::value,
        T&
    >::type emplace(Args&&... args) {
        return emplace<detail::variant_type_index_v<T, Types...>>(
            std::forward<Args>(args)...);
    }

    template<class T, class U, class... Args>
    RIN_VARIANT_CONSTEXPR20 typename enable_if<
        detail::variant_type_count_v<T, Types...> == 1 &&
        is_constructible<T, initializer_list<U>&, Args&&...>::value,
        T&
    >::type emplace(initializer_list<U> values, Args&&... args) {
        return emplace<detail::variant_type_index_v<T, Types...>>(
            values, std::forward<Args>(args)...);
    }

#if __cplusplus < 202002L
    template<class Dummy = void,
             typename enable_if<
                 conjunction<is_move_constructible<Types>...,
                             is_swappable<Types>...>::value &&
                 is_same<Dummy, Dummy>::value,
                 int>::type = 0>
#endif
    RIN_VARIANT_CONSTEXPR20 void swap(variant& other) noexcept(
        conjunction<is_nothrow_move_constructible<Types>...,
                    is_nothrow_swappable<Types>...>::value)
#if __cplusplus >= 202002L
        requires conjunction<
            is_move_constructible<Types>...,
            is_swappable<Types>...>::value
#endif
    {
        if (this == &other) return;
        if (index_ == other.index_) {
            if (!valueless_by_exception()) swap_same_active(other);
            return;
        }

        if (valueless_by_exception()) {
            move_construct_active(other);
            other.destroy_active();
            return;
        }
        if (other.valueless_by_exception()) {
            other.move_construct_active(*this);
            destroy_active();
            return;
        }

        /* The standard participation rule requires move construction and
         * swappability, not move assignment.  Exchange the two active
         * lifetimes explicitly so a move-construct-only alternative can use
         * variant::swap across different indexes. */
        variant saved(std::move(other));
        other.destroy_active();
        other.move_construct_active(*this);
        destroy_active();
        move_construct_active(saved);
    }
};

template<size_t I, class... Types>
constexpr variant_alternative_t<I, variant<Types...>>&
get(variant<Types...>& value) {
    static_assert(I < sizeof...(Types), "variant index out of bounds");
    if (value.index_ != I) detail::variant_access_fail();
    return value.storage_.template get<I>();
}

template<size_t I, class... Types>
constexpr const variant_alternative_t<I, variant<Types...>>&
get(const variant<Types...>& value) {
    static_assert(I < sizeof...(Types), "variant index out of bounds");
    if (value.index_ != I) detail::variant_access_fail();
    return value.storage_.template get<I>();
}

template<size_t I, class... Types>
constexpr variant_alternative_t<I, variant<Types...>>&&
get(variant<Types...>&& value) {
    return std::move(get<I>(value));
}

template<size_t I, class... Types>
constexpr const variant_alternative_t<I, variant<Types...>>&&
get(const variant<Types...>&& value) {
    return std::move(get<I>(value));
}

template<class T, class... Types>
constexpr T& get(variant<Types...>& value) {
    static_assert(detail::variant_type_count_v<T, Types...> == 1,
                  "get<T> requires exactly one matching alternative");
    return get<detail::variant_type_index_v<T, Types...>>(value);
}

template<class T, class... Types>
constexpr const T& get(const variant<Types...>& value) {
    static_assert(detail::variant_type_count_v<T, Types...> == 1,
                  "get<T> requires exactly one matching alternative");
    return get<detail::variant_type_index_v<T, Types...>>(value);
}

template<class T, class... Types>
constexpr T&& get(variant<Types...>&& value) {
    return std::move(get<T>(value));
}

template<class T, class... Types>
constexpr const T&& get(const variant<Types...>&& value) {
    return std::move(get<T>(value));
}

template<size_t I, class... Types>
constexpr variant_alternative_t<I, variant<Types...>>*
get_if(variant<Types...>* value) noexcept {
    static_assert(I < sizeof...(Types), "variant index out of bounds");
    if (!value || value->index_ != I) return nullptr;
    return &value->storage_.template get<I>();
}

template<size_t I, class... Types>
constexpr const variant_alternative_t<I, variant<Types...>>*
get_if(const variant<Types...>* value) noexcept {
    static_assert(I < sizeof...(Types), "variant index out of bounds");
    if (!value || value->index_ != I) return nullptr;
    return &value->storage_.template get<I>();
}

template<class T, class... Types>
constexpr T* get_if(variant<Types...>* value) noexcept {
    static_assert(detail::variant_type_count_v<T, Types...> == 1,
                  "get_if<T> requires exactly one matching alternative");
    return get_if<detail::variant_type_index_v<T, Types...>>(value);
}

template<class T, class... Types>
constexpr const T* get_if(const variant<Types...>* value) noexcept {
    static_assert(detail::variant_type_count_v<T, Types...> == 1,
                  "get_if<T> requires exactly one matching alternative");
    return get_if<detail::variant_type_index_v<T, Types...>>(value);
}

template<class T, class... Types>
constexpr bool holds_alternative(const variant<Types...>& value) noexcept {
    static_assert(detail::variant_type_count_v<T, Types...> == 1,
                  "holds_alternative<T> requires one matching alternative");
    return value.index() == detail::variant_type_index_v<T, Types...>;
}

namespace detail {

template<class T>
struct is_variant : false_type {};

template<class... Types>
struct is_variant<variant<Types...>> : true_type {};

template<class T, class = void>
struct is_variant_like : is_variant<remove_cvref_t<T>> {};

#if __cplusplus > 202002L
template<class T>
struct is_variant_like<
    T, void_t<typename remove_cvref_t<T>::__rincxx_variant_tag>>
    : is_variant<typename remove_cvref_t<T>::__rincxx_variant_tag> {};
#endif

template<class T>
inline constexpr bool is_variant_v = is_variant_like<T>::value;

template<class R, class Visitor, class Variant, size_t I,
         bool IsVariant = is_variant_v<Variant>>
struct variant_visit_one_as_traits {
    static constexpr bool valid = false;
};

template<class R, class Visitor, class Variant, size_t I>
struct variant_visit_one_as_traits<R, Visitor, Variant, I, true> {
    using argument_type = decltype(get<I>(declval<Variant>()));
    static constexpr bool current =
        is_invocable_r<R, Visitor, argument_type>::value;
    static constexpr size_t count =
        variant_size_v<remove_cvref_t<Variant>>;

    static constexpr bool valid = [] {
        if constexpr (I + 1 == count) return current;
        else return current &&
            variant_visit_one_as_traits<R, Visitor, Variant, I + 1>::valid;
    }();
};

template<class Visitor, class Variant, size_t I,
         bool IsVariant = is_variant_v<Variant>>
struct variant_visit_one_traits {
    static constexpr bool valid = false;
};

template<class Visitor, class Variant, size_t I, class Argument,
         bool IsInvocable = is_invocable<Visitor, Argument>::value>
struct variant_visit_one_result_traits {
    static constexpr bool valid = false;
    using result_type = void;
};

template<class Visitor, class Variant, size_t I, class Argument>
struct variant_visit_one_result_traits<Visitor, Variant, I, Argument, true> {
    using result_type = invoke_result_t<Visitor, Argument>;
    static constexpr size_t count =
        variant_size_v<remove_cvref_t<Variant>>;

    static constexpr bool valid = [] {
        if constexpr (I + 1 == count) {
            return true;
        } else {
            using next = variant_visit_one_traits<Visitor, Variant, I + 1>;
            if constexpr (!next::valid) {
                return false;
            } else {
                return is_same<result_type, typename next::result_type>::value;
            }
        }
    }();
};

template<class Visitor, class Variant, size_t I>
struct variant_visit_one_traits<Visitor, Variant, I, true> {
    using argument_type = decltype(get<I>(declval<Variant>()));
    using result_traits = variant_visit_one_result_traits<
        Visitor, Variant, I, argument_type>;

    static constexpr bool valid = result_traits::valid;
    using result_type = typename result_traits::result_type;
};

template<class Visitor, class FirstVariant, class SecondVariant,
         size_t FirstIndex, size_t SecondIndex, class FirstArgument,
         class SecondArgument,
         bool IsInvocable = is_invocable<Visitor, FirstArgument,
                                         SecondArgument>::value>
struct variant_visit_two_candidate_traits {
    static constexpr bool valid = false;
    using result_type = void;
};

template<class Visitor, class FirstVariant, class SecondVariant,
         size_t FirstIndex, size_t SecondIndex, class FirstArgument,
         class SecondArgument>
struct variant_visit_two_candidate_traits<
    Visitor, FirstVariant, SecondVariant, FirstIndex, SecondIndex,
    FirstArgument, SecondArgument, true> {
    static constexpr bool valid = true;
    using result_type = invoke_result_t<Visitor, FirstArgument, SecondArgument>;
};

template<class Visitor, class FirstVariant, class SecondVariant,
         size_t FirstIndex, size_t SecondIndex>
using variant_visit_two_candidate = variant_visit_two_candidate_traits<
    Visitor, FirstVariant, SecondVariant, FirstIndex, SecondIndex,
    decltype(get<FirstIndex>(declval<FirstVariant>())),
    decltype(get<SecondIndex>(declval<SecondVariant>()))>;

template<class Visitor, class FirstVariant, class SecondVariant,
         class Result, size_t FirstIndex, size_t SecondIndex>
struct variant_visit_two_all_traits {
    using candidate = variant_visit_two_candidate<
        Visitor, FirstVariant, SecondVariant, FirstIndex, SecondIndex>;
    static constexpr size_t first_count =
        variant_size_v<remove_cvref_t<FirstVariant>>;
    static constexpr size_t second_count =
        variant_size_v<remove_cvref_t<SecondVariant>>;

    static constexpr bool valid = candidate::valid &&
        is_same<Result, typename candidate::result_type>::value && [] {
            if constexpr (SecondIndex + 1 < second_count) {
                return variant_visit_two_all_traits<
                    Visitor, FirstVariant, SecondVariant, Result,
                    FirstIndex, SecondIndex + 1>::valid;
            } else if constexpr (FirstIndex + 1 < first_count) {
                return variant_visit_two_all_traits<
                    Visitor, FirstVariant, SecondVariant, Result,
                    FirstIndex + 1, 0>::valid;
            } else {
                return true;
            }
        }();
};

template<class Visitor, class FirstVariant, class SecondVariant,
         bool IsFirstVariant = is_variant_v<FirstVariant>,
         bool IsSecondVariant = is_variant_v<SecondVariant>>
struct variant_visit_two_traits {
    static constexpr bool valid = false;
};

template<class Visitor, class FirstVariant, class SecondVariant>
struct variant_visit_two_traits<Visitor, FirstVariant, SecondVariant,
                                true, true> {
    using first = variant_visit_two_candidate<
        Visitor, FirstVariant, SecondVariant, 0, 0>;
    using result_type = typename first::result_type;

    static constexpr bool valid = first::valid &&
        variant_visit_two_all_traits<Visitor, FirstVariant, SecondVariant,
                                     result_type, 0, 0>::valid;
};

template<class R, class Visitor, class FirstVariant, class SecondVariant,
         size_t FirstIndex, size_t SecondIndex, class FirstArgument,
         class SecondArgument,
         bool IsInvocable = is_invocable_r<R, Visitor, FirstArgument,
                                            SecondArgument>::value>
struct variant_visit_two_as_candidate_traits {
    static constexpr bool valid = false;
};

template<class R, class Visitor, class FirstVariant, class SecondVariant,
         size_t FirstIndex, size_t SecondIndex, class FirstArgument,
         class SecondArgument>
struct variant_visit_two_as_candidate_traits<
    R, Visitor, FirstVariant, SecondVariant, FirstIndex, SecondIndex,
    FirstArgument, SecondArgument, true> {
    static constexpr bool valid = true;
};

template<class R, class Visitor, class FirstVariant, class SecondVariant,
         size_t FirstIndex, size_t SecondIndex>
using variant_visit_two_as_candidate = variant_visit_two_as_candidate_traits<
    R, Visitor, FirstVariant, SecondVariant, FirstIndex, SecondIndex,
    decltype(get<FirstIndex>(declval<FirstVariant>())),
    decltype(get<SecondIndex>(declval<SecondVariant>()))>;

template<class R, class Visitor, class FirstVariant, class SecondVariant,
         size_t FirstIndex, size_t SecondIndex>
struct variant_visit_two_as_all_traits {
    using candidate = variant_visit_two_as_candidate<
        R, Visitor, FirstVariant, SecondVariant, FirstIndex, SecondIndex>;
    static constexpr size_t first_count =
        variant_size_v<remove_cvref_t<FirstVariant>>;
    static constexpr size_t second_count =
        variant_size_v<remove_cvref_t<SecondVariant>>;

    static constexpr bool valid = candidate::valid && [] {
        if constexpr (SecondIndex + 1 < second_count) {
            return variant_visit_two_as_all_traits<
                R, Visitor, FirstVariant, SecondVariant, FirstIndex,
                SecondIndex + 1>::valid;
        } else if constexpr (FirstIndex + 1 < first_count) {
            return variant_visit_two_as_all_traits<
                R, Visitor, FirstVariant, SecondVariant, FirstIndex + 1,
                0>::valid;
        } else {
            return true;
        }
    }();
};

template<class R, class Visitor, class FirstVariant, class SecondVariant,
         bool IsFirstVariant = is_variant_v<FirstVariant>,
         bool IsSecondVariant = is_variant_v<SecondVariant>>
struct variant_visit_two_as_traits {
    static constexpr bool valid = false;
};

template<class R, class Visitor, class FirstVariant, class SecondVariant>
struct variant_visit_two_as_traits<R, Visitor, FirstVariant, SecondVariant,
                                   true, true> {
    static constexpr bool valid = variant_visit_two_as_all_traits<
        R, Visitor, FirstVariant, SecondVariant, 0, 0>::valid;
};

template<class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, size_t FirstIndex, size_t SecondIndex,
         size_t ThirdIndex, class FirstArgument, class SecondArgument,
         class ThirdArgument,
         bool IsInvocable = is_invocable<Visitor, FirstArgument,
                                         SecondArgument, ThirdArgument>::value>
struct variant_visit_three_candidate_traits {
    static constexpr bool valid = false;
    using result_type = void;
};

template<class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, size_t FirstIndex, size_t SecondIndex,
         size_t ThirdIndex, class FirstArgument, class SecondArgument,
         class ThirdArgument>
struct variant_visit_three_candidate_traits<
    Visitor, FirstVariant, SecondVariant, ThirdVariant, FirstIndex,
    SecondIndex, ThirdIndex, FirstArgument, SecondArgument, ThirdArgument,
    true> {
    static constexpr bool valid = true;
    using result_type = invoke_result_t<Visitor, FirstArgument,
                                        SecondArgument, ThirdArgument>;
};

template<class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, size_t FirstIndex, size_t SecondIndex,
         size_t ThirdIndex>
using variant_visit_three_candidate = variant_visit_three_candidate_traits<
    Visitor, FirstVariant, SecondVariant, ThirdVariant, FirstIndex,
    SecondIndex, ThirdIndex,
    decltype(get<FirstIndex>(declval<FirstVariant>())),
    decltype(get<SecondIndex>(declval<SecondVariant>())),
    decltype(get<ThirdIndex>(declval<ThirdVariant>()))>;

template<class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, class Result, size_t FirstIndex,
         size_t SecondIndex, size_t ThirdIndex>
struct variant_visit_three_all_traits {
    using candidate = variant_visit_three_candidate<
        Visitor, FirstVariant, SecondVariant, ThirdVariant, FirstIndex,
        SecondIndex, ThirdIndex>;
    static constexpr size_t first_count =
        variant_size_v<remove_cvref_t<FirstVariant>>;
    static constexpr size_t second_count =
        variant_size_v<remove_cvref_t<SecondVariant>>;
    static constexpr size_t third_count =
        variant_size_v<remove_cvref_t<ThirdVariant>>;

    static constexpr bool valid = candidate::valid &&
        is_same<Result, typename candidate::result_type>::value && [] {
            if constexpr (ThirdIndex + 1 < third_count) {
                return variant_visit_three_all_traits<
                    Visitor, FirstVariant, SecondVariant, ThirdVariant,
                    Result, FirstIndex, SecondIndex, ThirdIndex + 1>::valid;
            } else if constexpr (SecondIndex + 1 < second_count) {
                return variant_visit_three_all_traits<
                    Visitor, FirstVariant, SecondVariant, ThirdVariant,
                    Result, FirstIndex, SecondIndex + 1, 0>::valid;
            } else if constexpr (FirstIndex + 1 < first_count) {
                return variant_visit_three_all_traits<
                    Visitor, FirstVariant, SecondVariant, ThirdVariant,
                    Result, FirstIndex + 1, 0, 0>::valid;
            } else {
                return true;
            }
        }();
};

template<class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant,
         bool IsFirstVariant = is_variant_v<FirstVariant>,
         bool IsSecondVariant = is_variant_v<SecondVariant>,
         bool IsThirdVariant = is_variant_v<ThirdVariant>>
struct variant_visit_three_traits {
    static constexpr bool valid = false;
    using result_type = void;
};

template<class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant>
struct variant_visit_three_traits<Visitor, FirstVariant, SecondVariant,
                                  ThirdVariant, true, true, true> {
    using first = variant_visit_three_candidate<
        Visitor, FirstVariant, SecondVariant, ThirdVariant, 0, 0, 0>;
    using result_type = typename first::result_type;

    static constexpr bool valid = first::valid &&
        variant_visit_three_all_traits<
            Visitor, FirstVariant, SecondVariant, ThirdVariant, result_type,
            0, 0, 0>::valid;
};

template<class R, class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, size_t FirstIndex, size_t SecondIndex,
         size_t ThirdIndex, class FirstArgument, class SecondArgument,
         class ThirdArgument,
         bool IsInvocable = is_invocable_r<R, Visitor, FirstArgument,
                                            SecondArgument, ThirdArgument>::value>
struct variant_visit_three_as_candidate_traits {
    static constexpr bool valid = false;
};

template<class R, class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, size_t FirstIndex, size_t SecondIndex,
         size_t ThirdIndex, class FirstArgument, class SecondArgument,
         class ThirdArgument>
struct variant_visit_three_as_candidate_traits<
    R, Visitor, FirstVariant, SecondVariant, ThirdVariant, FirstIndex,
    SecondIndex, ThirdIndex, FirstArgument, SecondArgument, ThirdArgument,
    true> {
    static constexpr bool valid = true;
};

template<class R, class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, size_t FirstIndex, size_t SecondIndex,
         size_t ThirdIndex>
using variant_visit_three_as_candidate = variant_visit_three_as_candidate_traits<
    R, Visitor, FirstVariant, SecondVariant, ThirdVariant, FirstIndex,
    SecondIndex, ThirdIndex,
    decltype(get<FirstIndex>(declval<FirstVariant>())),
    decltype(get<SecondIndex>(declval<SecondVariant>())),
    decltype(get<ThirdIndex>(declval<ThirdVariant>()))>;

template<class R, class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, size_t FirstIndex, size_t SecondIndex,
         size_t ThirdIndex>
struct variant_visit_three_as_all_traits {
    using candidate = variant_visit_three_as_candidate<
        R, Visitor, FirstVariant, SecondVariant, ThirdVariant, FirstIndex,
        SecondIndex, ThirdIndex>;
    static constexpr size_t first_count =
        variant_size_v<remove_cvref_t<FirstVariant>>;
    static constexpr size_t second_count =
        variant_size_v<remove_cvref_t<SecondVariant>>;
    static constexpr size_t third_count =
        variant_size_v<remove_cvref_t<ThirdVariant>>;

    static constexpr bool valid = candidate::valid && [] {
        if constexpr (ThirdIndex + 1 < third_count) {
            return variant_visit_three_as_all_traits<
                R, Visitor, FirstVariant, SecondVariant, ThirdVariant,
                FirstIndex, SecondIndex, ThirdIndex + 1>::valid;
        } else if constexpr (SecondIndex + 1 < second_count) {
            return variant_visit_three_as_all_traits<
                R, Visitor, FirstVariant, SecondVariant, ThirdVariant,
                FirstIndex, SecondIndex + 1, 0>::valid;
        } else if constexpr (FirstIndex + 1 < first_count) {
            return variant_visit_three_as_all_traits<
                R, Visitor, FirstVariant, SecondVariant, ThirdVariant,
                FirstIndex + 1, 0, 0>::valid;
        } else {
            return true;
        }
    }();
};

template<class R, class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant,
         bool IsFirstVariant = is_variant_v<FirstVariant>,
         bool IsSecondVariant = is_variant_v<SecondVariant>,
         bool IsThirdVariant = is_variant_v<ThirdVariant>>
struct variant_visit_three_as_traits {
    static constexpr bool valid = false;
};

template<class R, class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant>
struct variant_visit_three_as_traits<R, Visitor, FirstVariant, SecondVariant,
                                     ThirdVariant, true, true, true> {
    static constexpr bool valid = variant_visit_three_as_all_traits<
        R, Visitor, FirstVariant, SecondVariant, ThirdVariant, 0, 0, 0>::valid;
};

template<class Visitor, class FirstArgument>
struct variant_visit_bind_first {
    template<class... RemainingArguments>
    auto operator()(RemainingArguments&&...) const
        -> invoke_result_t<Visitor, FirstArgument, RemainingArguments&&...>;
};

template<class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, class FourthVariant, size_t FirstIndex,
         bool IsFirstVariant = is_variant_v<FirstVariant>>
struct variant_visit_four_first_traits {
    static constexpr bool valid = false;
    using result_type = void;
};

template<class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, class FourthVariant, size_t FirstIndex>
struct variant_visit_four_first_traits<Visitor, FirstVariant, SecondVariant,
                                       ThirdVariant, FourthVariant, FirstIndex,
                                       true> {
    using bound = variant_visit_bind_first<
        Visitor, decltype(get<FirstIndex>(declval<FirstVariant>()))>;
    using remaining = variant_visit_three_traits<
        bound, SecondVariant, ThirdVariant, FourthVariant>;
    static constexpr bool valid = remaining::valid;
    using result_type = typename remaining::result_type;
};

template<class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, class FourthVariant, class Result, size_t I>
struct variant_visit_four_all_traits {
    using current = variant_visit_four_first_traits<
        Visitor, FirstVariant, SecondVariant, ThirdVariant, FourthVariant, I>;
    static constexpr bool valid = current::valid &&
        is_same<Result, typename current::result_type>::value && [] {
            if constexpr (I + 1 < variant_size_v<remove_cvref_t<FirstVariant>>) {
                return variant_visit_four_all_traits<
                    Visitor, FirstVariant, SecondVariant, ThirdVariant,
                    FourthVariant, Result, I + 1>::valid;
            }
            return true;
        }();
};

template<class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, class FourthVariant,
         bool IsFirst = is_variant_v<FirstVariant>,
         bool IsSecond = is_variant_v<SecondVariant>,
         bool IsThird = is_variant_v<ThirdVariant>,
         bool IsFourth = is_variant_v<FourthVariant>>
struct variant_visit_four_traits {
    static constexpr bool valid = false;
    using result_type = void;
};

template<class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, class FourthVariant>
struct variant_visit_four_traits<Visitor, FirstVariant, SecondVariant,
                                 ThirdVariant, FourthVariant,
                                 true, true, true, true> {
    using first = variant_visit_four_first_traits<
        Visitor, FirstVariant, SecondVariant, ThirdVariant, FourthVariant, 0>;
    using result_type = typename first::result_type;
    static constexpr bool valid = first::valid && variant_visit_four_all_traits<
        Visitor, FirstVariant, SecondVariant, ThirdVariant, FourthVariant,
        result_type, 0>::valid;
};

template<class R, class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, class FourthVariant, size_t FirstIndex,
         bool IsFirstVariant = is_variant_v<FirstVariant>>
struct variant_visit_four_as_first_traits {
    static constexpr bool valid = false;
};

template<class R, class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, class FourthVariant, size_t FirstIndex>
struct variant_visit_four_as_first_traits<
    R, Visitor, FirstVariant, SecondVariant, ThirdVariant, FourthVariant,
    FirstIndex, true> {
    using bound = variant_visit_bind_first<
        Visitor, decltype(get<FirstIndex>(declval<FirstVariant>()))>;
    using remaining = variant_visit_three_as_traits<
        R, bound, SecondVariant, ThirdVariant, FourthVariant>;
    static constexpr bool valid = remaining::valid;
};

template<class R, class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, class FourthVariant, size_t I>
struct variant_visit_four_as_all_traits {
    using current = variant_visit_four_as_first_traits<
        R, Visitor, FirstVariant, SecondVariant, ThirdVariant, FourthVariant,
        I>;
    static constexpr bool valid = current::valid && [] {
        if constexpr (I + 1 < variant_size_v<remove_cvref_t<FirstVariant>>) {
            return variant_visit_four_as_all_traits<
                R, Visitor, FirstVariant, SecondVariant, ThirdVariant,
                FourthVariant, I + 1>::valid;
        }
        return true;
    }();
};

template<class R, class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, class FourthVariant,
         bool IsFirst = is_variant_v<FirstVariant>,
         bool IsSecond = is_variant_v<SecondVariant>,
         bool IsThird = is_variant_v<ThirdVariant>,
         bool IsFourth = is_variant_v<FourthVariant>>
struct variant_visit_four_as_traits {
    static constexpr bool valid = false;
};

template<class R, class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, class FourthVariant>
struct variant_visit_four_as_traits<R, Visitor, FirstVariant, SecondVariant,
                                    ThirdVariant, FourthVariant,
                                    true, true, true, true> {
    static constexpr bool valid = variant_visit_four_as_all_traits<
        R, Visitor, FirstVariant, SecondVariant, ThirdVariant, FourthVariant,
        0>::valid;
};

template<class Traits, bool Valid = Traits::valid>
struct variant_visit_traits_result {
    using type = void;
};

template<class Traits>
struct variant_visit_traits_result<Traits, true> {
    using type = typename Traits::result_type;
};

template<class Visitor, class... Variants>
struct variant_visit_general_traits;

template<class R, class Visitor, class... Variants>
struct variant_visit_general_as_traits;

template<class Visitor, class Variant>
struct variant_visit_general_traits<Visitor, Variant> {
    using traits = variant_visit_one_traits<Visitor, Variant, 0>;
    static constexpr bool valid = traits::valid;
    using result_type = typename variant_visit_traits_result<traits>::type;
};

template<class Visitor, class FirstVariant, class SecondVariant>
struct variant_visit_general_traits<Visitor, FirstVariant, SecondVariant> {
    using traits = variant_visit_two_traits<Visitor, FirstVariant, SecondVariant>;
    static constexpr bool valid = traits::valid;
    using result_type = typename variant_visit_traits_result<traits>::type;
};

template<class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant>
struct variant_visit_general_traits<Visitor, FirstVariant, SecondVariant,
                                    ThirdVariant> {
    using traits = variant_visit_three_traits<
        Visitor, FirstVariant, SecondVariant, ThirdVariant>;
    static constexpr bool valid = traits::valid;
    using result_type = typename variant_visit_traits_result<traits>::type;
};

template<class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, class FourthVariant>
struct variant_visit_general_traits<Visitor, FirstVariant, SecondVariant,
                                    ThirdVariant, FourthVariant> {
    using traits = variant_visit_four_traits<
        Visitor, FirstVariant, SecondVariant, ThirdVariant, FourthVariant>;
    static constexpr bool valid = traits::valid;
    using result_type = typename variant_visit_traits_result<traits>::type;
};

template<class R, class Visitor, class Variant>
struct variant_visit_general_as_traits<R, Visitor, Variant> {
    static constexpr bool valid =
        variant_visit_one_as_traits<R, Visitor, Variant, 0>::valid;
};

template<class R, class Visitor, class FirstVariant, class SecondVariant>
struct variant_visit_general_as_traits<R, Visitor, FirstVariant, SecondVariant> {
    static constexpr bool valid = variant_visit_two_as_traits<
        R, Visitor, FirstVariant, SecondVariant>::valid;
};

template<class R, class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant>
struct variant_visit_general_as_traits<R, Visitor, FirstVariant, SecondVariant,
                                       ThirdVariant> {
    static constexpr bool valid = variant_visit_three_as_traits<
        R, Visitor, FirstVariant, SecondVariant, ThirdVariant>::valid;
};

template<class R, class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, class FourthVariant>
struct variant_visit_general_as_traits<R, Visitor, FirstVariant, SecondVariant,
                                       ThirdVariant, FourthVariant> {
    static constexpr bool valid = variant_visit_four_as_traits<
        R, Visitor, FirstVariant, SecondVariant, ThirdVariant,
        FourthVariant>::valid;
};

template<class Visitor, class FirstVariant, size_t FirstIndex,
         bool IsFirstVariant, class... RemainingVariants>
struct variant_visit_many_first_traits {
    static constexpr bool valid = false;
    using result_type = void;
};

template<class Visitor, class FirstVariant, size_t FirstIndex,
         class... RemainingVariants>
struct variant_visit_many_first_traits<Visitor, FirstVariant, FirstIndex, true,
                                       RemainingVariants...> {
    using bound = variant_visit_bind_first<
        Visitor, decltype(get<FirstIndex>(declval<FirstVariant>()))>;
    using remaining = variant_visit_general_traits<bound, RemainingVariants...>;
    static constexpr bool valid = remaining::valid;
    using result_type = typename remaining::result_type;
};

template<class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, class FourthVariant, class FifthVariant,
         class Result, size_t FirstIndex, class... RemainingVariants>
struct variant_visit_many_all_traits {
    using current = variant_visit_many_first_traits<
        Visitor, FirstVariant, FirstIndex, true, SecondVariant, ThirdVariant,
        FourthVariant, FifthVariant, RemainingVariants...>;
    static constexpr bool valid = current::valid &&
        is_same<Result, typename current::result_type>::value && [] {
            if constexpr (FirstIndex + 1 <
                          variant_size_v<remove_cvref_t<FirstVariant>>) {
                return variant_visit_many_all_traits<
                    Visitor, FirstVariant, SecondVariant, ThirdVariant,
                    FourthVariant, FifthVariant, Result, FirstIndex + 1,
                    RemainingVariants...>::valid;
            }
            return true;
        }();
};

template<bool AreVariants, class Visitor, class FirstVariant,
         class SecondVariant, class ThirdVariant, class FourthVariant,
         class FifthVariant, class... RemainingVariants>
struct variant_visit_many_traits_impl {
    static constexpr bool valid = false;
    using result_type = void;
};

template<class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, class FourthVariant, class FifthVariant,
         class... RemainingVariants>
struct variant_visit_many_traits_impl<
    true, Visitor, FirstVariant, SecondVariant, ThirdVariant, FourthVariant,
    FifthVariant, RemainingVariants...> {
    using first = variant_visit_many_first_traits<
        Visitor, FirstVariant, 0, true, SecondVariant, ThirdVariant,
        FourthVariant, FifthVariant, RemainingVariants...>;
    using result_type = typename first::result_type;
    static constexpr bool valid = first::valid && variant_visit_many_all_traits<
        Visitor, FirstVariant, SecondVariant, ThirdVariant, FourthVariant,
        FifthVariant, result_type, 0, RemainingVariants...>::valid;
};

template<class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, class FourthVariant, class FifthVariant,
         class... RemainingVariants>
struct variant_visit_general_traits<Visitor, FirstVariant, SecondVariant,
                                    ThirdVariant, FourthVariant, FifthVariant,
                                    RemainingVariants...>
    : variant_visit_many_traits_impl<
          is_variant_v<FirstVariant> && is_variant_v<SecondVariant> &&
              is_variant_v<ThirdVariant> && is_variant_v<FourthVariant> &&
              is_variant_v<FifthVariant> &&
              (is_variant_v<RemainingVariants> && ...),
          Visitor, FirstVariant, SecondVariant, ThirdVariant, FourthVariant,
          FifthVariant, RemainingVariants...> {};

template<class R, class Visitor, class FirstVariant, size_t FirstIndex,
         bool IsFirstVariant, class... RemainingVariants>
struct variant_visit_many_as_first_traits {
    static constexpr bool valid = false;
};

template<class R, class Visitor, class FirstVariant, size_t FirstIndex,
         class... RemainingVariants>
struct variant_visit_many_as_first_traits<
    R, Visitor, FirstVariant, FirstIndex, true, RemainingVariants...> {
    using bound = variant_visit_bind_first<
        Visitor, decltype(get<FirstIndex>(declval<FirstVariant>()))>;
    static constexpr bool valid = variant_visit_general_as_traits<
        R, bound, RemainingVariants...>::valid;
};

template<class R, class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, class FourthVariant, class FifthVariant,
         size_t FirstIndex, class... RemainingVariants>
struct variant_visit_many_as_all_traits {
    using current = variant_visit_many_as_first_traits<
        R, Visitor, FirstVariant, FirstIndex, true, SecondVariant,
        ThirdVariant, FourthVariant, FifthVariant, RemainingVariants...>;
    static constexpr bool valid = current::valid && [] {
        if constexpr (FirstIndex + 1 <
                      variant_size_v<remove_cvref_t<FirstVariant>>) {
            return variant_visit_many_as_all_traits<
                R, Visitor, FirstVariant, SecondVariant, ThirdVariant,
                FourthVariant, FifthVariant, FirstIndex + 1,
                RemainingVariants...>::valid;
        }
        return true;
    }();
};

template<bool AreVariants, class R, class Visitor, class FirstVariant,
         class SecondVariant, class ThirdVariant, class FourthVariant,
         class FifthVariant, class... RemainingVariants>
struct variant_visit_many_as_traits_impl {
    static constexpr bool valid = false;
};

template<class R, class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, class FourthVariant, class FifthVariant,
         class... RemainingVariants>
struct variant_visit_many_as_traits_impl<
    true, R, Visitor, FirstVariant, SecondVariant, ThirdVariant, FourthVariant,
    FifthVariant, RemainingVariants...> {
    static constexpr bool valid = variant_visit_many_as_all_traits<
        R, Visitor, FirstVariant, SecondVariant, ThirdVariant, FourthVariant,
        FifthVariant, 0, RemainingVariants...>::valid;
};

template<class R, class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, class FourthVariant, class FifthVariant,
         class... RemainingVariants>
struct variant_visit_general_as_traits<
    R, Visitor, FirstVariant, SecondVariant, ThirdVariant, FourthVariant,
    FifthVariant, RemainingVariants...>
    : variant_visit_many_as_traits_impl<
          is_variant_v<FirstVariant> && is_variant_v<SecondVariant> &&
              is_variant_v<ThirdVariant> && is_variant_v<FourthVariant> &&
              is_variant_v<FifthVariant> &&
              (is_variant_v<RemainingVariants> && ...),
          R, Visitor, FirstVariant, SecondVariant, ThirdVariant, FourthVariant,
          FifthVariant, RemainingVariants...> {};

template<class Visitor, class Variant, size_t I>
constexpr decltype(auto) variant_visit_one(Visitor&& visitor,
                                           Variant&& value) {
    if constexpr (I == 0) {
        return std::forward<Visitor>(visitor)(
            get<0>(std::forward<Variant>(value)));
    } else {
        if (value.index() == I) {
            return std::forward<Visitor>(visitor)(
                get<I>(std::forward<Variant>(value)));
        }
        return variant_visit_one<Visitor, Variant, I - 1>(
            std::forward<Visitor>(visitor),
            std::forward<Variant>(value));
    }
}

template<class Visitor, class FirstVariant, class SecondVariant,
         size_t FirstIndex, size_t SecondIndex>
constexpr decltype(auto) variant_visit_two_second(Visitor&& visitor,
                                                   FirstVariant&& first,
                                                   SecondVariant&& second) {
    if constexpr (SecondIndex == 0) {
        return std::forward<Visitor>(visitor)(
            get<FirstIndex>(std::forward<FirstVariant>(first)),
            get<0>(std::forward<SecondVariant>(second)));
    } else {
        if (second.index() == SecondIndex) {
            return std::forward<Visitor>(visitor)(
                get<FirstIndex>(std::forward<FirstVariant>(first)),
                get<SecondIndex>(std::forward<SecondVariant>(second)));
        }
        return variant_visit_two_second<Visitor, FirstVariant, SecondVariant,
                                        FirstIndex, SecondIndex - 1>(
            std::forward<Visitor>(visitor),
            std::forward<FirstVariant>(first),
            std::forward<SecondVariant>(second));
    }
}

template<class Visitor, class FirstVariant, class SecondVariant,
         size_t FirstIndex, size_t SecondIndex>
constexpr decltype(auto) variant_visit_two_first(Visitor&& visitor,
                                                  FirstVariant&& first,
                                                  SecondVariant&& second) {
    if constexpr (FirstIndex == 0) {
        return variant_visit_two_second<Visitor, FirstVariant, SecondVariant,
                                        0, SecondIndex>(
            std::forward<Visitor>(visitor),
            std::forward<FirstVariant>(first),
            std::forward<SecondVariant>(second));
    } else {
        if (first.index() == FirstIndex) {
            return variant_visit_two_second<Visitor, FirstVariant,
                                            SecondVariant, FirstIndex,
                                            SecondIndex>(
                std::forward<Visitor>(visitor),
                std::forward<FirstVariant>(first),
                std::forward<SecondVariant>(second));
        }
        return variant_visit_two_first<Visitor, FirstVariant, SecondVariant,
                                       FirstIndex - 1, SecondIndex>(
            std::forward<Visitor>(visitor),
            std::forward<FirstVariant>(first),
            std::forward<SecondVariant>(second));
    }
}

template<class Visitor, class FirstVariant, class SecondVariant>
constexpr decltype(auto) variant_visit_two(Visitor&& visitor,
                                            FirstVariant&& first,
                                            SecondVariant&& second) {
    if (first.valueless_by_exception() || second.valueless_by_exception()) {
        variant_access_fail();
    }
    return variant_visit_two_first<
        Visitor, FirstVariant, SecondVariant,
        variant_size_v<remove_cvref_t<FirstVariant>> - 1,
        variant_size_v<remove_cvref_t<SecondVariant>> - 1>(
        std::forward<Visitor>(visitor), std::forward<FirstVariant>(first),
        std::forward<SecondVariant>(second));
}

template<class R, class Visitor, class... Args>
constexpr R variant_visit_invoke_as(Visitor&& visitor, Args&&... args) {
    if constexpr (is_void_v<R>) {
        std::forward<Visitor>(visitor)(std::forward<Args>(args)...);
    } else {
        return static_cast<R>(
            std::forward<Visitor>(visitor)(std::forward<Args>(args)...));
    }
}

template<class R, class Visitor, class Variant, size_t I>
constexpr R variant_visit_one_as(Visitor&& visitor, Variant&& value) {
    if constexpr (I == 0) {
        return variant_visit_invoke_as<R>(
            std::forward<Visitor>(visitor),
            get<0>(std::forward<Variant>(value)));
    } else {
        if (value.index() == I) {
            return variant_visit_invoke_as<R>(
                std::forward<Visitor>(visitor),
                get<I>(std::forward<Variant>(value)));
        }
        return variant_visit_one_as<R, Visitor, Variant, I - 1>(
            std::forward<Visitor>(visitor), std::forward<Variant>(value));
    }
}

template<class R, class Visitor, class FirstVariant, class SecondVariant,
         size_t FirstIndex, size_t SecondIndex>
constexpr R variant_visit_two_second_as(Visitor&& visitor,
                                         FirstVariant&& first,
                                         SecondVariant&& second) {
    if constexpr (SecondIndex == 0) {
        return variant_visit_invoke_as<R>(
            std::forward<Visitor>(visitor),
            get<FirstIndex>(std::forward<FirstVariant>(first)),
            get<0>(std::forward<SecondVariant>(second)));
    } else {
        if (second.index() == SecondIndex) {
            return variant_visit_invoke_as<R>(
                std::forward<Visitor>(visitor),
                get<FirstIndex>(std::forward<FirstVariant>(first)),
                get<SecondIndex>(std::forward<SecondVariant>(second)));
        }
        return variant_visit_two_second_as<R, Visitor, FirstVariant,
                                           SecondVariant, FirstIndex,
                                           SecondIndex - 1>(
            std::forward<Visitor>(visitor),
            std::forward<FirstVariant>(first),
            std::forward<SecondVariant>(second));
    }
}

template<class R, class Visitor, class FirstVariant, class SecondVariant,
         size_t FirstIndex, size_t SecondIndex>
constexpr R variant_visit_two_first_as(Visitor&& visitor,
                                        FirstVariant&& first,
                                        SecondVariant&& second) {
    if constexpr (FirstIndex == 0) {
        return variant_visit_two_second_as<R, Visitor, FirstVariant,
                                           SecondVariant, 0, SecondIndex>(
            std::forward<Visitor>(visitor),
            std::forward<FirstVariant>(first),
            std::forward<SecondVariant>(second));
    } else {
        if (first.index() == FirstIndex) {
            return variant_visit_two_second_as<R, Visitor, FirstVariant,
                                               SecondVariant, FirstIndex,
                                               SecondIndex>(
                std::forward<Visitor>(visitor),
                std::forward<FirstVariant>(first),
                std::forward<SecondVariant>(second));
        }
        return variant_visit_two_first_as<R, Visitor, FirstVariant,
                                          SecondVariant, FirstIndex - 1,
                                          SecondIndex>(
            std::forward<Visitor>(visitor),
            std::forward<FirstVariant>(first),
            std::forward<SecondVariant>(second));
    }
}

template<class R, class Visitor, class FirstVariant, class SecondVariant>
constexpr R variant_visit_two_as(Visitor&& visitor, FirstVariant&& first,
                                 SecondVariant&& second) {
    if (first.valueless_by_exception() || second.valueless_by_exception()) {
        variant_access_fail();
    }
    return variant_visit_two_first_as<
        R, Visitor, FirstVariant, SecondVariant,
        variant_size_v<remove_cvref_t<FirstVariant>> - 1,
        variant_size_v<remove_cvref_t<SecondVariant>> - 1>(
        std::forward<Visitor>(visitor), std::forward<FirstVariant>(first),
        std::forward<SecondVariant>(second));
}

template<class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, size_t FirstIndex>
constexpr decltype(auto) variant_visit_three_first(Visitor&& visitor,
                                                    FirstVariant&& first,
                                                    SecondVariant&& second,
                                                    ThirdVariant&& third) {
    const auto invoke_second = [&](auto&& first_value) -> decltype(auto) {
            return variant_visit_two(
                [&](auto&& second_value, auto&& third_value) -> decltype(auto) {
                    return std::forward<Visitor>(visitor)(
                        std::forward<decltype(first_value)>(first_value),
                        std::forward<decltype(second_value)>(second_value),
                        std::forward<decltype(third_value)>(third_value));
                },
                std::forward<SecondVariant>(second),
                std::forward<ThirdVariant>(third));
    };
    if constexpr (FirstIndex == 0) {
        return invoke_second(get<0>(std::forward<FirstVariant>(first)));
    } else {
        if (first.index() == FirstIndex) {
            return invoke_second(
                get<FirstIndex>(std::forward<FirstVariant>(first)));
        }
        return variant_visit_three_first<Visitor, FirstVariant, SecondVariant,
                                         ThirdVariant, FirstIndex - 1>(
            std::forward<Visitor>(visitor), std::forward<FirstVariant>(first),
            std::forward<SecondVariant>(second),
            std::forward<ThirdVariant>(third));
    }
}

template<class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant>
constexpr decltype(auto) variant_visit_three(Visitor&& visitor,
                                              FirstVariant&& first,
                                              SecondVariant&& second,
                                              ThirdVariant&& third) {
    if (first.valueless_by_exception() || second.valueless_by_exception() ||
        third.valueless_by_exception()) variant_access_fail();
    return variant_visit_three_first<
        Visitor, FirstVariant, SecondVariant, ThirdVariant,
        variant_size_v<remove_cvref_t<FirstVariant>> - 1>(
        std::forward<Visitor>(visitor), std::forward<FirstVariant>(first),
        std::forward<SecondVariant>(second), std::forward<ThirdVariant>(third));
}

template<class R, class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, size_t FirstIndex>
constexpr R variant_visit_three_first_as(Visitor&& visitor, FirstVariant&& first,
                                         SecondVariant&& second,
                                         ThirdVariant&& third) {
    const auto invoke_second = [&](auto&& first_value) -> R {
            return variant_visit_two_as<R>(
                [&](auto&& second_value, auto&& third_value) -> R {
                    return variant_visit_invoke_as<R>(
                        std::forward<Visitor>(visitor),
                        std::forward<decltype(first_value)>(first_value),
                        std::forward<decltype(second_value)>(second_value),
                        std::forward<decltype(third_value)>(third_value));
                },
                std::forward<SecondVariant>(second),
                std::forward<ThirdVariant>(third));
    };
    if constexpr (FirstIndex == 0) {
        return invoke_second(get<0>(std::forward<FirstVariant>(first)));
    } else {
        if (first.index() == FirstIndex) {
            return invoke_second(
                get<FirstIndex>(std::forward<FirstVariant>(first)));
        }
        return variant_visit_three_first_as<R, Visitor, FirstVariant,
                                            SecondVariant, ThirdVariant,
                                            FirstIndex - 1>(
            std::forward<Visitor>(visitor), std::forward<FirstVariant>(first),
            std::forward<SecondVariant>(second),
            std::forward<ThirdVariant>(third));
    }
}

template<class R, class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant>
constexpr R variant_visit_three_as(Visitor&& visitor, FirstVariant&& first,
                                   SecondVariant&& second,
                                   ThirdVariant&& third) {
    if (first.valueless_by_exception() || second.valueless_by_exception() ||
        third.valueless_by_exception()) variant_access_fail();
    return variant_visit_three_first_as<
        R, Visitor, FirstVariant, SecondVariant, ThirdVariant,
        variant_size_v<remove_cvref_t<FirstVariant>> - 1>(
        std::forward<Visitor>(visitor), std::forward<FirstVariant>(first),
        std::forward<SecondVariant>(second), std::forward<ThirdVariant>(third));
}

template<class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, class FourthVariant, size_t FirstIndex>
constexpr decltype(auto) variant_visit_four_first(Visitor&& visitor,
                                                   FirstVariant&& first,
                                                   SecondVariant&& second,
                                                   ThirdVariant&& third,
                                                   FourthVariant&& fourth) {
    const auto invoke_second = [&](auto&& first_value) -> decltype(auto) {
            return variant_visit_three(
                [&](auto&& second_value, auto&& third_value,
                    auto&& fourth_value) -> decltype(auto) {
                    return std::forward<Visitor>(visitor)(
                        std::forward<decltype(first_value)>(first_value),
                        std::forward<decltype(second_value)>(second_value),
                        std::forward<decltype(third_value)>(third_value),
                        std::forward<decltype(fourth_value)>(fourth_value));
                },
                std::forward<SecondVariant>(second),
                std::forward<ThirdVariant>(third),
                std::forward<FourthVariant>(fourth));
    };
    if constexpr (FirstIndex == 0) {
        return invoke_second(get<0>(std::forward<FirstVariant>(first)));
    } else {
        if (first.index() == FirstIndex) {
            return invoke_second(
                get<FirstIndex>(std::forward<FirstVariant>(first)));
        }
        return variant_visit_four_first<Visitor, FirstVariant, SecondVariant,
                                        ThirdVariant, FourthVariant,
                                        FirstIndex - 1>(
            std::forward<Visitor>(visitor), std::forward<FirstVariant>(first),
            std::forward<SecondVariant>(second),
            std::forward<ThirdVariant>(third),
            std::forward<FourthVariant>(fourth));
    }
}

template<class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, class FourthVariant>
constexpr decltype(auto) variant_visit_four(Visitor&& visitor,
                                             FirstVariant&& first,
                                             SecondVariant&& second,
                                             ThirdVariant&& third,
                                             FourthVariant&& fourth) {
    if (first.valueless_by_exception() || second.valueless_by_exception() ||
        third.valueless_by_exception() || fourth.valueless_by_exception()) {
        variant_access_fail();
    }
    return variant_visit_four_first<
        Visitor, FirstVariant, SecondVariant, ThirdVariant, FourthVariant,
        variant_size_v<remove_cvref_t<FirstVariant>> - 1>(
        std::forward<Visitor>(visitor), std::forward<FirstVariant>(first),
        std::forward<SecondVariant>(second), std::forward<ThirdVariant>(third),
        std::forward<FourthVariant>(fourth));
}

template<class R, class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, class FourthVariant, size_t FirstIndex>
constexpr R variant_visit_four_first_as(Visitor&& visitor, FirstVariant&& first,
                                        SecondVariant&& second,
                                        ThirdVariant&& third,
                                        FourthVariant&& fourth) {
    const auto invoke_second = [&](auto&& first_value) -> R {
            return variant_visit_three_as<R>(
                [&](auto&& second_value, auto&& third_value,
                    auto&& fourth_value) -> R {
                    return variant_visit_invoke_as<R>(
                        std::forward<Visitor>(visitor),
                        std::forward<decltype(first_value)>(first_value),
                        std::forward<decltype(second_value)>(second_value),
                        std::forward<decltype(third_value)>(third_value),
                        std::forward<decltype(fourth_value)>(fourth_value));
                },
                std::forward<SecondVariant>(second),
                std::forward<ThirdVariant>(third),
                std::forward<FourthVariant>(fourth));
    };
    if constexpr (FirstIndex == 0) {
        return invoke_second(get<0>(std::forward<FirstVariant>(first)));
    } else {
        if (first.index() == FirstIndex) {
            return invoke_second(
                get<FirstIndex>(std::forward<FirstVariant>(first)));
        }
        return variant_visit_four_first_as<R, Visitor, FirstVariant,
                                           SecondVariant, ThirdVariant,
                                           FourthVariant, FirstIndex - 1>(
            std::forward<Visitor>(visitor), std::forward<FirstVariant>(first),
            std::forward<SecondVariant>(second),
            std::forward<ThirdVariant>(third),
            std::forward<FourthVariant>(fourth));
    }
}

template<class R, class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, class FourthVariant>
constexpr R variant_visit_four_as(Visitor&& visitor, FirstVariant&& first,
                                  SecondVariant&& second,
                                  ThirdVariant&& third,
                                  FourthVariant&& fourth) {
    if (first.valueless_by_exception() || second.valueless_by_exception() ||
        third.valueless_by_exception() || fourth.valueless_by_exception()) {
        variant_access_fail();
    }
    return variant_visit_four_first_as<
        R, Visitor, FirstVariant, SecondVariant, ThirdVariant, FourthVariant,
        variant_size_v<remove_cvref_t<FirstVariant>> - 1>(
        std::forward<Visitor>(visitor), std::forward<FirstVariant>(first),
        std::forward<SecondVariant>(second), std::forward<ThirdVariant>(third),
        std::forward<FourthVariant>(fourth));
}

template<class Visitor, class FirstVariant, class... RemainingVariants>
constexpr decltype(auto) variant_visit_many_unchecked(
    Visitor&& visitor, FirstVariant&& first, RemainingVariants&&... remaining) {
    auto invoke_first = [&](auto&& first_value) -> decltype(auto) {
        if constexpr (sizeof...(RemainingVariants) == 0) {
            return std::forward<Visitor>(visitor)(
                std::forward<decltype(first_value)>(first_value));
        } else {
            return variant_visit_many_unchecked(
                [&](auto&&... remaining_values) -> decltype(auto) {
                    return std::forward<Visitor>(visitor)(
                        std::forward<decltype(first_value)>(first_value),
                        std::forward<decltype(remaining_values)>(
                            remaining_values)...);
                },
                std::forward<RemainingVariants>(remaining)...);
        }
    };
    return variant_visit_one<
        decltype(invoke_first), FirstVariant,
        variant_size_v<remove_cvref_t<FirstVariant>> - 1>(
        std::move(invoke_first), std::forward<FirstVariant>(first));
}

template<class R, class Visitor, class FirstVariant,
         class... RemainingVariants>
constexpr R variant_visit_many_unchecked_as(
    Visitor&& visitor, FirstVariant&& first, RemainingVariants&&... remaining) {
    auto invoke_first = [&](auto&& first_value) -> R {
        if constexpr (sizeof...(RemainingVariants) == 0) {
            return variant_visit_invoke_as<R>(
                std::forward<Visitor>(visitor),
                std::forward<decltype(first_value)>(first_value));
        } else {
            return variant_visit_many_unchecked_as<R>(
                [&](auto&&... remaining_values) -> R {
                    return variant_visit_invoke_as<R>(
                        std::forward<Visitor>(visitor),
                        std::forward<decltype(first_value)>(first_value),
                        std::forward<decltype(remaining_values)>(
                            remaining_values)...);
                },
                std::forward<RemainingVariants>(remaining)...);
        }
    };
    return variant_visit_one<
        decltype(invoke_first), FirstVariant,
        variant_size_v<remove_cvref_t<FirstVariant>> - 1>(
        std::move(invoke_first), std::forward<FirstVariant>(first));
}

template<size_t I, class... Types>
constexpr bool variant_equal_at(const variant<Types...>& lhs,
                                const variant<Types...>& rhs) {
    if constexpr (I == sizeof...(Types)) {
        return true;
    } else {
        if (lhs.index() == I) return get<I>(lhs) == get<I>(rhs);
        return variant_equal_at<I + 1>(lhs, rhs);
    }
}

template<size_t I, class... Types>
constexpr bool variant_less_at(const variant<Types...>& lhs,
                               const variant<Types...>& rhs) {
    if constexpr (I == sizeof...(Types)) {
        return false;
    } else {
        if (lhs.index() == I) return get<I>(lhs) < get<I>(rhs);
        return variant_less_at<I + 1>(lhs, rhs);
    }
}

#if __cplusplus >= 202002L
template<class Category, size_t I, class... Types>
constexpr Category variant_compare_at(const variant<Types...>& lhs,
                                      const variant<Types...>& rhs) {
    if constexpr (I == sizeof...(Types)) {
        return Category::equivalent;
    } else {
        if (lhs.index() == I) {
            return static_cast<Category>(get<I>(lhs) <=> get<I>(rhs));
        }
        return variant_compare_at<Category, I + 1>(lhs, rhs);
    }
}
#endif

} /* namespace detail */

template<class Visitor, class Variant,
         enable_if_t<detail::variant_visit_one_traits<
                         Visitor&&, Variant&&, 0>::valid,
                     int> = 0>
constexpr decltype(auto) visit(Visitor&& visitor, Variant&& value) {
    if (value.valueless_by_exception()) detail::variant_access_fail();
    return detail::variant_visit_one<
        Visitor, Variant,
        variant_size_v<remove_cvref_t<Variant>> - 1>(
        std::forward<Visitor>(visitor), std::forward<Variant>(value));
}

template<class Visitor, class FirstVariant, class SecondVariant,
         enable_if_t<detail::is_variant_v<FirstVariant> &&
                         detail::is_variant_v<SecondVariant> &&
                         detail::variant_visit_two_traits<
                             Visitor&&, FirstVariant&&, SecondVariant&&>::valid,
                     int> = 0>
constexpr decltype(auto) visit(Visitor&& visitor, FirstVariant&& first,
                               SecondVariant&& second) {
    return detail::variant_visit_two(
        std::forward<Visitor>(visitor), std::forward<FirstVariant>(first),
        std::forward<SecondVariant>(second));
}

template<class R, class Visitor, class Variant,
         enable_if_t<detail::is_variant_v<Variant> &&
                         detail::variant_visit_one_as_traits<
                             R, Visitor&&, Variant&&, 0>::valid,
                     int> = 0>
constexpr R visit(Visitor&& visitor, Variant&& value) {
    if (value.valueless_by_exception()) detail::variant_access_fail();
    return detail::variant_visit_one_as<
        R, Visitor, Variant,
        variant_size_v<remove_cvref_t<Variant>> - 1>(
        std::forward<Visitor>(visitor), std::forward<Variant>(value));
}

template<class R, class Visitor, class FirstVariant, class SecondVariant,
         enable_if_t<detail::is_variant_v<FirstVariant> &&
                         detail::is_variant_v<SecondVariant> &&
                         detail::variant_visit_two_as_traits<
                             R, Visitor&&, FirstVariant&&,
                             SecondVariant&&>::valid,
                     int> = 0>
constexpr R visit(Visitor&& visitor, FirstVariant&& first,
                  SecondVariant&& second) {
    return detail::variant_visit_two_as<R>(
        std::forward<Visitor>(visitor), std::forward<FirstVariant>(first),
        std::forward<SecondVariant>(second));
}

template<class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant,
         enable_if_t<detail::is_variant_v<FirstVariant> &&
                         detail::is_variant_v<SecondVariant> &&
                         detail::is_variant_v<ThirdVariant> &&
                         detail::variant_visit_three_traits<
                             Visitor&&, FirstVariant&&, SecondVariant&&,
                             ThirdVariant&&>::valid,
                     int> = 0>
constexpr decltype(auto) visit(Visitor&& visitor, FirstVariant&& first,
                               SecondVariant&& second, ThirdVariant&& third) {
    return detail::variant_visit_three(
        std::forward<Visitor>(visitor), std::forward<FirstVariant>(first),
        std::forward<SecondVariant>(second), std::forward<ThirdVariant>(third));
}

template<class R, class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant,
         enable_if_t<detail::is_variant_v<FirstVariant> &&
                         detail::is_variant_v<SecondVariant> &&
                         detail::is_variant_v<ThirdVariant> &&
                         detail::variant_visit_three_as_traits<
                             R, Visitor&&, FirstVariant&&, SecondVariant&&,
                             ThirdVariant&&>::valid,
                     int> = 0>
constexpr R visit(Visitor&& visitor, FirstVariant&& first,
                  SecondVariant&& second, ThirdVariant&& third) {
    return detail::variant_visit_three_as<R>(
        std::forward<Visitor>(visitor), std::forward<FirstVariant>(first),
        std::forward<SecondVariant>(second), std::forward<ThirdVariant>(third));
}

template<class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, class FourthVariant,
         enable_if_t<detail::is_variant_v<FirstVariant> &&
                         detail::is_variant_v<SecondVariant> &&
                         detail::is_variant_v<ThirdVariant> &&
                         detail::is_variant_v<FourthVariant> &&
                         detail::variant_visit_four_traits<
                             Visitor&&, FirstVariant&&, SecondVariant&&,
                             ThirdVariant&&, FourthVariant&&>::valid,
                     int> = 0>
constexpr decltype(auto) visit(Visitor&& visitor, FirstVariant&& first,
                               SecondVariant&& second, ThirdVariant&& third,
                               FourthVariant&& fourth) {
    return detail::variant_visit_four(
        std::forward<Visitor>(visitor), std::forward<FirstVariant>(first),
        std::forward<SecondVariant>(second), std::forward<ThirdVariant>(third),
        std::forward<FourthVariant>(fourth));
}

template<class R, class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, class FourthVariant,
         enable_if_t<detail::is_variant_v<FirstVariant> &&
                         detail::is_variant_v<SecondVariant> &&
                         detail::is_variant_v<ThirdVariant> &&
                         detail::is_variant_v<FourthVariant> &&
                         detail::variant_visit_four_as_traits<
                             R, Visitor&&, FirstVariant&&, SecondVariant&&,
                             ThirdVariant&&, FourthVariant&&>::valid,
                     int> = 0>
constexpr R visit(Visitor&& visitor, FirstVariant&& first,
                  SecondVariant&& second, ThirdVariant&& third,
                  FourthVariant&& fourth) {
    return detail::variant_visit_four_as<R>(
        std::forward<Visitor>(visitor), std::forward<FirstVariant>(first),
        std::forward<SecondVariant>(second), std::forward<ThirdVariant>(third),
        std::forward<FourthVariant>(fourth));
}

template<class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, class FourthVariant, class FifthVariant,
         class... RemainingVariants,
         enable_if_t<detail::is_variant_v<FirstVariant> &&
                         detail::is_variant_v<SecondVariant> &&
                         detail::is_variant_v<ThirdVariant> &&
                         detail::is_variant_v<FourthVariant> &&
                         detail::is_variant_v<FifthVariant> &&
                         (detail::is_variant_v<RemainingVariants> && ...) &&
                         detail::variant_visit_general_traits<
                             Visitor&&, FirstVariant&&, SecondVariant&&,
                             ThirdVariant&&, FourthVariant&&, FifthVariant&&,
                             RemainingVariants&&...>::valid,
                     int> = 0>
constexpr decltype(auto) visit(Visitor&& visitor, FirstVariant&& first,
                               SecondVariant&& second, ThirdVariant&& third,
                               FourthVariant&& fourth, FifthVariant&& fifth,
                               RemainingVariants&&... remaining) {
    if (first.valueless_by_exception() || second.valueless_by_exception() ||
        third.valueless_by_exception() || fourth.valueless_by_exception() ||
        fifth.valueless_by_exception() ||
        (remaining.valueless_by_exception() || ...)) {
        detail::variant_access_fail();
    }
    return detail::variant_visit_many_unchecked(
        std::forward<Visitor>(visitor), std::forward<FirstVariant>(first),
        std::forward<SecondVariant>(second), std::forward<ThirdVariant>(third),
        std::forward<FourthVariant>(fourth), std::forward<FifthVariant>(fifth),
        std::forward<RemainingVariants>(remaining)...);
}

template<class R, class Visitor, class FirstVariant, class SecondVariant,
         class ThirdVariant, class FourthVariant, class FifthVariant,
         class... RemainingVariants,
         enable_if_t<detail::is_variant_v<FirstVariant> &&
                         detail::is_variant_v<SecondVariant> &&
                         detail::is_variant_v<ThirdVariant> &&
                         detail::is_variant_v<FourthVariant> &&
                         detail::is_variant_v<FifthVariant> &&
                         (detail::is_variant_v<RemainingVariants> && ...) &&
                         detail::variant_visit_general_as_traits<
                             R, Visitor&&, FirstVariant&&, SecondVariant&&,
                             ThirdVariant&&, FourthVariant&&, FifthVariant&&,
                             RemainingVariants&&...>::valid,
                     int> = 0>
constexpr R visit(Visitor&& visitor, FirstVariant&& first,
                  SecondVariant&& second, ThirdVariant&& third,
                  FourthVariant&& fourth, FifthVariant&& fifth,
                  RemainingVariants&&... remaining) {
    if (first.valueless_by_exception() || second.valueless_by_exception() ||
        third.valueless_by_exception() || fourth.valueless_by_exception() ||
        fifth.valueless_by_exception() ||
        (remaining.valueless_by_exception() || ...)) {
        detail::variant_access_fail();
    }
    return detail::variant_visit_many_unchecked_as<R>(
        std::forward<Visitor>(visitor), std::forward<FirstVariant>(first),
        std::forward<SecondVariant>(second), std::forward<ThirdVariant>(third),
        std::forward<FourthVariant>(fourth), std::forward<FifthVariant>(fifth),
        std::forward<RemainingVariants>(remaining)...);
}

template<class... Types
#if __cplusplus < 202002L
         , enable_if_t<
               (detail::variant_equality_traits<Types>::valid && ...), int> =
               0
#endif
         >
#if __cplusplus >= 202002L
    requires (detail::variant_equality_traits<Types>::valid && ...)
#endif
constexpr bool operator==(const variant<Types...>& lhs,
                          const variant<Types...>& rhs) noexcept(
    (detail::variant_equality_traits<Types>::nothrow && ...)) {
    if (lhs.index() != rhs.index()) return false;
    if (lhs.valueless_by_exception()) return true;
    return detail::variant_equal_at<0>(lhs, rhs);
}

template<class... Types
#if __cplusplus < 202002L
         , enable_if_t<
               (detail::variant_equality_traits<Types>::valid && ...), int> =
               0
#endif
         >
#if __cplusplus >= 202002L
    requires (detail::variant_equality_traits<Types>::valid && ...)
#endif
constexpr bool operator!=(const variant<Types...>& lhs,
                          const variant<Types...>& rhs) noexcept(
    (detail::variant_equality_traits<Types>::nothrow && ...)) {
    return !(lhs == rhs);
}

template<class... Types
#if __cplusplus < 202002L
         , enable_if_t<
               (detail::variant_less_traits<Types>::valid && ...), int> = 0
#endif
         >
#if __cplusplus >= 202002L
    requires (detail::variant_less_traits<Types>::valid && ...)
#endif
constexpr bool operator<(const variant<Types...>& lhs,
                         const variant<Types...>& rhs) noexcept(
    (detail::variant_less_traits<Types>::nothrow && ...)) {
    if (lhs.valueless_by_exception()) return !rhs.valueless_by_exception();
    if (rhs.valueless_by_exception()) return false;
    if (lhs.index() != rhs.index()) return lhs.index() < rhs.index();
    return detail::variant_less_at<0>(lhs, rhs);
}

template<class... Types
#if __cplusplus < 202002L
         , enable_if_t<
               (detail::variant_less_traits<Types>::valid && ...), int> = 0
#endif
         >
#if __cplusplus >= 202002L
    requires (detail::variant_less_traits<Types>::valid && ...)
#endif
constexpr bool operator>(const variant<Types...>& lhs,
                         const variant<Types...>& rhs) noexcept(
    (detail::variant_less_traits<Types>::nothrow && ...)) {
    return rhs < lhs;
}

template<class... Types
#if __cplusplus < 202002L
         , enable_if_t<
               (detail::variant_less_traits<Types>::valid && ...), int> = 0
#endif
         >
#if __cplusplus >= 202002L
    requires (detail::variant_less_traits<Types>::valid && ...)
#endif
constexpr bool operator<=(const variant<Types...>& lhs,
                          const variant<Types...>& rhs) noexcept(
    (detail::variant_less_traits<Types>::nothrow && ...)) {
    return !(rhs < lhs);
}

template<class... Types
#if __cplusplus < 202002L
         , enable_if_t<
               (detail::variant_less_traits<Types>::valid && ...), int> = 0
#endif
         >
#if __cplusplus >= 202002L
    requires (detail::variant_less_traits<Types>::valid && ...)
#endif
constexpr bool operator>=(const variant<Types...>& lhs,
                          const variant<Types...>& rhs) noexcept(
    (detail::variant_less_traits<Types>::nothrow && ...)) {
    return !(lhs < rhs);
}

#if __cplusplus >= 202002L
template<class... Types>
    requires (detail::variant_three_way_traits<Types>::valid && ...)
constexpr auto operator<=>(const variant<Types...>& lhs,
                           const variant<Types...>& rhs) noexcept(
    (detail::variant_three_way_traits<Types>::nothrow && ...)) {
    using category = common_comparison_category_t<
        compare_three_way_result_t<Types>...>;
    if (lhs.valueless_by_exception()) {
        return rhs.valueless_by_exception() ? category::equivalent
                                             : category::less;
    }
    if (rhs.valueless_by_exception()) return category::greater;
    if (lhs.index() != rhs.index()) {
        return lhs.index() < rhs.index() ? category::less : category::greater;
    }
    return detail::variant_compare_at<category, 0>(lhs, rhs);
}
#endif

template<class... Types
#if __cplusplus < 202002L
/* Keep non-member swap participation identical to variant::swap. */
         , enable_if_t<
               conjunction<is_move_constructible<Types>...,
                           is_swappable<Types>...>::value, int> = 0
#endif
         >
RIN_VARIANT_CONSTEXPR20 void swap(variant<Types...>& lhs,
                                  variant<Types...>& rhs)
    noexcept(conjunction<is_nothrow_move_constructible<Types>...,
                         is_nothrow_swappable<Types>...>::value)
#if __cplusplus >= 202002L
    requires conjunction<is_move_constructible<Types>...,
                         is_swappable<Types>...>::value
#endif
{
    lhs.swap(rhs);
}

namespace detail {

template<class T>
struct variant_hash_enabled
    : conjunction<
          is_default_constructible<hash<remove_const_t<T>>>,
          is_copy_constructible<hash<remove_const_t<T>>>,
          is_copy_assignable<hash<remove_const_t<T>>>,
          is_destructible<hash<remove_const_t<T>>>,
          is_swappable<hash<remove_const_t<T>>>,
          is_invocable_r<size_t, const hash<remove_const_t<T>>&,
                         const remove_const_t<T>&>> {};

template<class... Types>
inline constexpr bool variant_hash_enabled_v =
    (variant_hash_enabled<Types>::value && ...);

template<class... Types>
inline constexpr bool variant_hash_nothrow_v =
    (is_nothrow_invocable<hash<remove_const_t<Types>>,
                          const remove_const_t<Types>&>::value && ...);

template<size_t I, class... Types>
size_t variant_hash_active(const variant<Types...>& value) noexcept(
    variant_hash_nothrow_v<Types...>) {
    if (value.index() == I) {
        using active_type = remove_const_t<variant_alternative_t<
            I, variant<Types...>>>;
        return hash<size_t>{}(I) + hash<active_type>{}(get<I>(value));
    }
    if constexpr (I + 1 < sizeof...(Types)) {
        return variant_hash_active<I + 1>(value);
    }
    return hash<size_t>{}(variant_npos);
}

template<class Variant, bool Enabled>
struct variant_hash_base : functional_hash_base<Variant, false> {};

template<class... Types>
struct variant_hash_base<variant<Types...>, true> {
    size_t operator()(const variant<Types...>& value) const noexcept(
        variant_hash_nothrow_v<Types...>) {
        if (value.valueless_by_exception()) {
            return hash<size_t>{}(variant_npos);
        }
        return variant_hash_active<0>(value);
    }
};

} /* namespace detail */

template<>
struct hash<monostate> {
    size_t operator()(const monostate&) const noexcept {
        return static_cast<size_t>(-7777);
    }
};

template<class... Types>
struct hash<variant<Types...>>
    : detail::variant_hash_base<variant<Types...>,
                                detail::variant_hash_enabled_v<Types...>> {};

} /* namespace std */

#undef RIN_VARIANT_CONSTEXPR20

#endif /* defined(__cplusplus) && __cplusplus >= 201703L */
#endif /* RINCXX_VARIANT_H */
