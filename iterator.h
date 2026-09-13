/*
 * RinOS libcxx - iterator.h
 * イテレータユーティリティ
 */
#ifndef RINCXX_ITERATOR_H
#define RINCXX_ITERATOR_H

#include "type_traits.h"
#include "concepts.h"  /* C++20 iterator concepts */
#if __cplusplus >= 202002L
#include "compare.h"
#endif

#ifdef __cplusplus

namespace std {

/* `contiguous_iterator` is specified in terms of to_address.  Keep this
 * foundation in <iterator> so a direct iterator user does not depend on a
 * circular include through <memory>. */
namespace _ptr_traits_detail {
template<typename...> using void_t = void;

template<typename Ptr, typename = void>
struct _get_element_type {};

template<typename Ptr>
struct _get_element_type<Ptr, void_t<typename Ptr::element_type>> {
    using type = typename Ptr::element_type;
};

template<typename Ptr>
struct _get_first_arg {};

template<template<typename, typename...> class Template, typename T,
         typename... Args>
struct _get_first_arg<Template<T, Args...>> {
    using type = T;
};

template<typename Ptr, typename = void>
struct _get_value_type : _get_first_arg<Ptr> {};

template<typename Ptr>
struct _get_value_type<Ptr, void_t<typename Ptr::value_type>> {
    using type = typename Ptr::value_type;
};

template<typename Ptr, typename = void>
struct _element_type_or_first_arg : _get_value_type<Ptr> {};

template<typename Ptr>
struct _element_type_or_first_arg<Ptr, void_t<typename Ptr::element_type>> {
    using type = typename Ptr::element_type;
};

template<typename Ptr, typename = void>
struct _get_difference_type { using type = ptrdiff_t; };

template<typename Ptr>
struct _get_difference_type<Ptr, void_t<typename Ptr::difference_type>> {
    using type = typename Ptr::difference_type;
};

template<typename Ptr, typename U>
struct _replace_first_arg {};

template<template<typename, typename...> class Template, typename T,
         typename... Args, typename U>
struct _replace_first_arg<Template<T, Args...>, U> {
    using type = Template<U, Args...>;
};

template<typename Ptr, typename U, typename = void>
struct _rebind_or_replace : _replace_first_arg<Ptr, U> {};

template<typename Ptr, typename U>
struct _rebind_or_replace<Ptr, U,
    void_t<typename Ptr::template rebind<U>>> {
    using type = typename Ptr::template rebind<U>;
};
} /* namespace _ptr_traits_detail */

template<typename Ptr>
struct pointer_traits {
    using pointer = Ptr;
    using element_type =
        typename _ptr_traits_detail::_element_type_or_first_arg<Ptr>::type;
    using difference_type =
        typename _ptr_traits_detail::_get_difference_type<Ptr>::type;

    template<typename U>
    using rebind = typename _ptr_traits_detail::_rebind_or_replace<Ptr, U>::type;

    static pointer pointer_to(element_type& reference) {
        return Ptr::pointer_to(reference);
    }
};

template<typename T>
struct pointer_traits<T*> {
    using pointer = T*;
    using element_type = T;
    using difference_type = ptrdiff_t;

    template<typename U>
    using rebind = U*;

    static constexpr pointer pointer_to(element_type& reference) noexcept {
        return __builtin_addressof(reference);
    }
};

#if __cplusplus >= 202002L
template<typename T>
constexpr T* to_address(T* pointer) noexcept {
    static_assert(!std::is_function<T>::value, "T cannot be a function type");
    return pointer;
}

namespace _detail {
template<typename T, typename = void>
struct _has_to_address : false_type {};

template<typename T>
struct _has_to_address<T, void_t<
    decltype(pointer_traits<T>::to_address(declval<const T&>()))>> : true_type {};
} /* namespace _detail */

template<typename T>
constexpr auto to_address(const T& pointer) noexcept {
    if constexpr (_detail::_has_to_address<T>::value) {
        return pointer_traits<T>::to_address(pointer);
    } else {
        return to_address(pointer.operator->());
    }
}
#endif

/* イテレータタグ */
struct input_iterator_tag {};
struct output_iterator_tag {};
struct forward_iterator_tag : public input_iterator_tag {};
struct bidirectional_iterator_tag : public forward_iterator_tag {};
struct random_access_iterator_tag : public bidirectional_iterator_tag {};
struct contiguous_iterator_tag : public random_access_iterator_tag {};  /* C++20 */

/* unreachable_sentinel (C++20) - 無限範囲用 */
struct unreachable_sentinel_t {
    template<typename T>
    friend constexpr bool operator==(unreachable_sentinel_t, const T&) noexcept { return false; }
    template<typename T>
    friend constexpr bool operator==(const T&, unreachable_sentinel_t) noexcept { return false; }
    template<typename T>
    friend constexpr bool operator!=(unreachable_sentinel_t, const T&) noexcept { return true; }
    template<typename T>
    friend constexpr bool operator!=(const T&, unreachable_sentinel_t) noexcept { return true; }
};
#if __cplusplus >= 202002L
inline constexpr unreachable_sentinel_t unreachable_sentinel{};
#endif

/* C++20 sentinel used by single-pass stream iterators. */
struct default_sentinel_t {};
#if __cplusplus >= 202002L
inline constexpr default_sentinel_t default_sentinel{};
#endif

/* イテレータ traits - デフォルトは空 (SFINAE対応) */
template<typename Iterator, typename = void>
struct iterator_traits {};

namespace detail {

/* C++20 iterator concepts use the nested iterator_concept when it is
 * supplied.  Falling back to iterator_category preserves legacy iterators. */
template<typename Iterator, typename = void>
struct iterator_concept_or_category {
    using type = typename Iterator::iterator_category;
};

template<typename Iterator>
struct iterator_concept_or_category<
    Iterator, void_t<typename Iterator::iterator_concept>> {
    using type = typename Iterator::iterator_concept;
};

} /* namespace detail */

/* C++20 iterators may publish iterator_concept without the legacy
 * iterator_category.  Keep the associated types usable by ranges while
 * retaining iterator_category when a legacy iterator supplies it. */
namespace detail {
template<typename Iterator, typename = void>
struct iterator_category_or_concept {
    using type = typename Iterator::iterator_concept;
};

template<typename Iterator>
struct iterator_category_or_concept<
    Iterator, void_t<typename Iterator::iterator_category>> {
    using type = typename Iterator::iterator_category;
};
} /* namespace detail */

template<typename Iterator>
struct iterator_traits<Iterator, void_t<
    typename Iterator::difference_type,
    typename Iterator::value_type,
    typename Iterator::pointer,
    typename Iterator::reference,
    typename detail::iterator_category_or_concept<Iterator>::type>> {
    typedef typename Iterator::difference_type difference_type;
    typedef typename Iterator::value_type value_type;
    typedef typename Iterator::pointer pointer;
    typedef typename Iterator::reference reference;
    typedef typename detail::iterator_category_or_concept<Iterator>::type
        iterator_category;
    typedef typename detail::iterator_concept_or_category<Iterator>::type
        iterator_concept;
};

/* ポインタ特殊化 */
template<typename T>
struct iterator_traits<T*> {
    typedef ptrdiff_t difference_type;
    typedef T value_type;
    typedef T* pointer;
    typedef T& reference;
    typedef random_access_iterator_tag iterator_category;
    /* C++20: pointers are contiguous iterators */
    typedef contiguous_iterator_tag iterator_concept;
};

template<typename T>
struct iterator_traits<const T*> {
    typedef ptrdiff_t difference_type;
    typedef T value_type;
    typedef const T* pointer;
    typedef const T& reference;
    typedef random_access_iterator_tag iterator_category;
    /* C++20: pointers are contiguous iterators */
    typedef contiguous_iterator_tag iterator_concept;
};

#if __cplusplus >= 202002L
/* C++20 range adaptors depend on these associated iterator types and
 * category concepts. Keep them here, after iterator_traits, so direct
 * <iterator> users and <ranges> share one definition. */
namespace detail {
template<typename I, typename = void>
struct iter_difference_has_nested : false_type {};

template<typename I>
struct iter_difference_has_nested<I, void_t<
    typename remove_cvref_t<I>::difference_type>> : true_type {};

template<typename I, typename = void>
struct iter_difference_has_traits : false_type {};

template<typename I>
struct iter_difference_has_traits<I, void_t<
    typename iterator_traits<remove_cvref_t<I>>::difference_type>>
    : true_type {};

template<typename I, bool HasNested, bool IsIntegral, bool HasTraits>
struct iter_difference_select {};

template<typename I, bool IsIntegral, bool HasTraits>
struct iter_difference_select<I, true, IsIntegral, HasTraits> {
    using type = typename remove_cvref_t<I>::difference_type;
};

/* Ranges values such as int are weakly incrementable even though they are
 * not iterators.  The standard associated difference for these scalar
 * values is ptrdiff_t; bool is intentionally excluded because it cannot be
 * incremented as a range value. */
template<typename I, bool HasTraits>
struct iter_difference_select<I, false, true, HasTraits> {
    using type = ptrdiff_t;
};

template<typename I>
struct iter_difference_select<I, false, false, true> {
    using type = typename iterator_traits<remove_cvref_t<I>>::difference_type;
};

template<typename I, typename = void>
struct iter_difference_impl
    : iter_difference_select<
          I,
          iter_difference_has_nested<I>::value,
          is_integral<remove_cvref_t<I>>::value &&
              !is_same<remove_cvref_t<I>, bool>::value,
          iter_difference_has_traits<I>::value> {};
} /* namespace detail */

template<typename I>
using iter_difference_t = typename detail::iter_difference_impl<I>::type;

template<typename I>
using iter_value_t = typename iterator_traits<remove_cvref_t<I>>::value_type;

template<typename I>
using iter_reference_t = decltype(*declval<I&>());

/* iter_move belongs with the iterator-associated aliases so direct
 * <iterator> users and <ranges> consumers observe one ADL-first owner. */
namespace ranges {
namespace detail {
void iter_move() = delete;

template<typename I>
constexpr auto adl_iter_move(I&& value)
    noexcept(noexcept(iter_move(static_cast<I&&>(value))))
    -> decltype(iter_move(static_cast<I&&>(value)))
{
    return iter_move(static_cast<I&&>(value));
}

template<typename I, typename = void>
struct has_adl_iter_move : false_type {};

template<typename I>
struct has_adl_iter_move<I, void_t<
    decltype(adl_iter_move(declval<I&>()))>> : true_type {};
} /* namespace detail */

namespace __cpo {
struct iter_move_fn {
    template<typename I,
             enable_if_t<detail::has_adl_iter_move<I>::value, int> = 0>
    constexpr auto operator()(I&& value) const
        noexcept(noexcept(detail::adl_iter_move(static_cast<I&&>(value))))
        -> decltype(detail::adl_iter_move(static_cast<I&&>(value)))
    {
        return detail::adl_iter_move(static_cast<I&&>(value));
    }

    template<typename I,
             enable_if_t<!detail::has_adl_iter_move<I>::value, int> = 0>
    constexpr auto operator()(I&& value) const
        noexcept(noexcept(static_cast<remove_reference_t<decltype(
            *static_cast<I&&>(value))>&&>(*static_cast<I&&>(value))))
        -> decltype(static_cast<remove_reference_t<decltype(
            *static_cast<I&&>(value))>&&>(*static_cast<I&&>(value)))
    {
        return static_cast<remove_reference_t<decltype(
            *static_cast<I&&>(value))>&&>(*static_cast<I&&>(value));
    }
};
} /* namespace __cpo */

inline constexpr __cpo::iter_move_fn iter_move{};

namespace detail {
void iter_swap() = delete;

template<typename I, typename J>
constexpr auto adl_iter_swap(I&& left, J&& right)
    noexcept(noexcept(iter_swap(static_cast<I&&>(left), static_cast<J&&>(right))))
    -> decltype(iter_swap(static_cast<I&&>(left), static_cast<J&&>(right)))
{
    return iter_swap(static_cast<I&&>(left), static_cast<J&&>(right));
}

template<typename I, typename J, typename = void>
struct has_adl_iter_swap : false_type {};

template<typename I, typename J>
struct has_adl_iter_swap<I, J, void_t<decltype(
    adl_iter_swap(declval<I&>(), declval<J&>()))>> : true_type {};

/* Standard fallback for proxy references without ADL/direct swap. */
template<typename I, typename J, typename = void>
struct has_iter_swap_move : false_type {};

template<typename I, typename J>
struct has_iter_swap_move<I, J, void_t<
    decltype(iter_value_t<I>(ranges::iter_move(declval<I&>()))),
    decltype(*declval<I&>() = ranges::iter_move(declval<J&>())),
    decltype(*declval<J&>() = declval<iter_value_t<I>&&>())>>
    : true_type {};
} /* namespace detail */

namespace __cpo {
struct iter_swap_fn {
    template<typename I, typename J,
             enable_if_t<detail::has_adl_iter_swap<I, J>::value, int> = 0>
    constexpr auto operator()(I&& left, J&& right) const
        noexcept(noexcept(detail::adl_iter_swap(static_cast<I&&>(left),
                                                static_cast<J&&>(right))))
        -> decltype(detail::adl_iter_swap(static_cast<I&&>(left),
                                          static_cast<J&&>(right)))
    {
        return detail::adl_iter_swap(static_cast<I&&>(left),
                                     static_cast<J&&>(right));
    }

    template<typename I, typename J,
             enable_if_t<!detail::has_adl_iter_swap<I, J>::value &&
                         is_swappable_with_v<iter_reference_t<I>,
                                              iter_reference_t<J>>, int> = 0>
    constexpr void operator()(I&& left, J&& right) const
        noexcept(noexcept(swap(*static_cast<I&&>(left),
                               *static_cast<J&&>(right))))
    {
        swap(*static_cast<I&&>(left), *static_cast<J&&>(right));
    }

    template<typename I, typename J,
             enable_if_t<!detail::has_adl_iter_swap<I, J>::value &&
                         !is_swappable_with_v<iter_reference_t<I>,
                                               iter_reference_t<J>> &&
                         detail::has_iter_swap_move<I, J>::value, int> = 0>
    constexpr void operator()(I&& left, J&& right) const
        noexcept(noexcept(iter_value_t<I>(ranges::iter_move(
                         static_cast<I&&>(left)))) &&
                 noexcept(*static_cast<I&&>(left) = ranges::iter_move(
                         static_cast<J&&>(right))) &&
                 noexcept(*static_cast<J&&>(right) =
                          static_cast<iter_value_t<I>&&>(
                              declval<iter_value_t<I>&>())))
    {
        using value_type = iter_value_t<I>;
        value_type temporary(ranges::iter_move(static_cast<I&&>(left)));
        *static_cast<I&&>(left) =
            ranges::iter_move(static_cast<J&&>(right));
        *static_cast<J&&>(right) = static_cast<value_type&&>(temporary);
    }
};
} /* namespace __cpo */

inline constexpr __cpo::iter_swap_fn iter_swap{};
} /* namespace ranges */

template<typename I>
using iter_rvalue_reference_t = decltype(ranges::iter_move(declval<I&>()));

namespace detail {
template<typename I, typename Category, typename = void>
struct iterator_concept_at_least : false_type {};

template<typename I, typename Category>
struct iterator_concept_at_least<
    I, Category, void_t<typename iterator_traits<I>::iterator_concept>>
    : integral_constant<bool,
        is_base_of<Category,
                   typename iterator_traits<I>::iterator_concept>::value> {};

} /* namespace detail */

template<typename I>
concept weakly_incrementable = movable<I> && requires(I value) {
    typename iter_difference_t<I>;
    requires signed_integral<iter_difference_t<I>>;
    { ++value } -> same_as<I&>;
    value++;
};

template<typename I>
concept input_or_output_iterator = weakly_incrementable<I> &&
    requires(I value) {
        *value;
    };

template<typename I>
concept indirectly_readable = requires {
    typename iter_value_t<I>;
    typename iter_reference_t<I>;
    typename iter_rvalue_reference_t<I>;
} && common_reference_with<iter_reference_t<I>&&, iter_value_t<I>&> &&
    common_reference_with<iter_reference_t<I>&&,
                          iter_rvalue_reference_t<I>&&> &&
    common_reference_with<iter_rvalue_reference_t<I>&&,
                          const iter_value_t<I>&>;

template<typename Out, typename T>
concept indirectly_writable = requires(Out&& output, T&& value) {
    *output = static_cast<T&&>(value);
    *static_cast<Out&&>(output) = static_cast<T&&>(value);
    const_cast<const iter_reference_t<Out>&&>(*output) =
        static_cast<T&&>(value);
    const_cast<const iter_reference_t<Out>&&>(
        *static_cast<Out&&>(output)) = static_cast<T&&>(value);
};

template<typename S, typename I>
concept sentinel_for = semiregular<S> && input_or_output_iterator<I> &&
    requires(const I& iterator, const S& sentinel) {
        { iterator == sentinel } -> detail::boolean_testable;
        { sentinel == iterator } -> detail::boolean_testable;
        { iterator != sentinel } -> detail::boolean_testable;
        { sentinel != iterator } -> detail::boolean_testable;
    };

template<typename S, typename I>
inline constexpr bool disable_sized_sentinel_for = false;

template<typename S, typename I>
concept sized_sentinel_for = sentinel_for<S, I> &&
    !disable_sized_sentinel_for<remove_cvref_t<S>, remove_cvref_t<I>> &&
    requires(const I& iterator, const S& sentinel) {
        { sentinel - iterator } -> same_as<iter_difference_t<I>>;
        { iterator - sentinel } -> same_as<iter_difference_t<I>>;
    };

template<typename I>
concept input_iterator = input_or_output_iterator<I> &&
    indirectly_readable<I> && sentinel_for<I, I> &&
    detail::iterator_concept_at_least<I, input_iterator_tag>::value;

template<typename Out, typename T>
concept output_iterator = input_or_output_iterator<Out> &&
    indirectly_writable<Out, T> &&
    requires(Out output, T value) {
        *output++ = static_cast<T&&>(value);
    };

template<typename I>
concept incrementable = regular<I> && weakly_incrementable<I> &&
    requires(I value) {
        { value++ } -> same_as<I>;
    };

template<typename I>
concept forward_iterator = input_iterator<I> && incrementable<I> &&
    detail::iterator_concept_at_least<I, forward_iterator_tag>::value;

template<typename I>
concept bidirectional_iterator = forward_iterator<I> &&
    detail::iterator_concept_at_least<I, bidirectional_iterator_tag>::value &&
    requires(I value) {
        { --value } -> same_as<I&>;
        { value-- } -> same_as<I>;
    };

template<typename I>
concept random_access_iterator = bidirectional_iterator<I> &&
    detail::iterator_concept_at_least<I, random_access_iterator_tag>::value &&
    requires(I value, I other, iter_difference_t<I> count) {
        { value += count } -> same_as<I&>;
        { value -= count } -> same_as<I&>;
        { value + count } -> same_as<I>;
        { count + value } -> same_as<I>;
        { value - count } -> same_as<I>;
        { value - other } -> same_as<iter_difference_t<I>>;
        value[count];
    };

template<typename I>
concept contiguous_iterator = random_access_iterator<I> &&
    detail::iterator_concept_at_least<I, contiguous_iterator_tag>::value &&
    is_lvalue_reference<iter_reference_t<I>>::value &&
    same_as<iter_value_t<I>, remove_cvref_t<iter_reference_t<I>>> &&
    requires(const I& value) {
        { std::to_address(value) } ->
            same_as<add_pointer_t<iter_reference_t<I>>>;
    };
#if __cplusplus >= 202002L
namespace detail {

/* counted_iterator keeps the end boundary as a signed remaining count.  The
 * category base is intentionally empty when an output-only iterator does not
 * publish the legacy associated value types. */
template<typename I, typename = void>
struct counted_iterator_traits {};

template<typename I>
struct counted_iterator_traits<I, void_t<
    typename iterator_traits<remove_cvref_t<I>>::value_type,
    typename iterator_traits<remove_cvref_t<I>>::pointer,
    typename iterator_traits<remove_cvref_t<I>>::reference,
    typename iterator_traits<remove_cvref_t<I>>::iterator_category>> {
    using value_type = typename iterator_traits<remove_cvref_t<I>>::value_type;
    using pointer = typename iterator_traits<remove_cvref_t<I>>::pointer;
    using reference = typename iterator_traits<remove_cvref_t<I>>::reference;
    using iterator_category =
        typename iterator_traits<remove_cvref_t<I>>::iterator_category;
};

} /* namespace detail */

template<input_or_output_iterator I>
class counted_iterator : public detail::counted_iterator_traits<I> {
    I current_;
    iter_difference_t<I> count_;

public:
    using iterator_type = I;
    using difference_type = iter_difference_t<I>;
    using iterator_concept = conditional_t<
        random_access_iterator<I>, random_access_iterator_tag,
        conditional_t<forward_iterator<I>, forward_iterator_tag,
                      input_iterator_tag>>;
    using reference = iter_reference_t<I>;

    constexpr counted_iterator() requires default_initializable<I>
        : current_(), count_() {}

    constexpr counted_iterator(I current, difference_type count)
        : current_(static_cast<I&&>(current)), count_(count) {}

    template<typename U>
        requires convertible_to<const U&, I>
    constexpr counted_iterator(const counted_iterator<U>& other)
        noexcept(is_nothrow_constructible<I, const U&>::value)
        : current_(other.base()),
          count_(static_cast<difference_type>(other.count())) {}

    template<typename U>
        requires assignable_from<I&, const U&>
    constexpr counted_iterator& operator=(const counted_iterator<U>& other)
        noexcept(is_nothrow_assignable<I&, const U&>::value) {
        current_ = other.base();
        count_ = static_cast<difference_type>(other.count());
        return *this;
    }

    constexpr I base() const & { return current_; }
    constexpr I base() && { return static_cast<I&&>(current_); }
    constexpr difference_type count() const noexcept { return count_; }

    constexpr reference operator*() const { return *current_; }

    constexpr auto operator->() const
        requires requires(const I& value) { value.operator->(); }
    {
        return current_.operator->();
    }

    constexpr auto operator->() const requires is_pointer<I>::value {
        return current_;
    }

    constexpr counted_iterator& operator++() {
        ++current_;
        --count_;
        return *this;
    }

    constexpr counted_iterator operator++(int) requires forward_iterator<I> {
        counted_iterator before(*this);
        ++(*this);
        return before;
    }

    constexpr void operator++(int) requires (!forward_iterator<I>) {
        ++(*this);
    }

    constexpr counted_iterator& operator--()
        requires bidirectional_iterator<I>
    {
        --current_;
        ++count_;
        return *this;
    }

    constexpr counted_iterator operator--(int)
        requires bidirectional_iterator<I>
    {
        counted_iterator before(*this);
        --(*this);
        return before;
    }

    constexpr counted_iterator& operator+=(difference_type offset)
        requires random_access_iterator<I>
    {
        current_ += offset;
        count_ -= offset;
        return *this;
    }

    constexpr counted_iterator& operator-=(difference_type offset)
        requires random_access_iterator<I>
    {
        current_ -= offset;
        count_ += offset;
        return *this;
    }

    constexpr counted_iterator operator+(difference_type offset) const
        requires random_access_iterator<I>
    {
        counted_iterator result(*this);
        result += offset;
        return result;
    }

    friend constexpr counted_iterator operator+(
        difference_type offset, const counted_iterator& value)
        requires random_access_iterator<I>
    {
        return value + offset;
    }

    constexpr counted_iterator operator-(difference_type offset) const
        requires random_access_iterator<I>
    {
        counted_iterator result(*this);
        result -= offset;
        return result;
    }

    constexpr reference operator[](difference_type offset) const
        requires random_access_iterator<I>
    {
        return current_[offset];
    }

    friend constexpr bool operator==(const counted_iterator& left,
                                     const counted_iterator& right)
        noexcept(noexcept(left.current_ == right.current_)) {
        return left.current_ == right.current_;
    }

    friend constexpr bool operator!=(const counted_iterator& left,
                                     const counted_iterator& right)
        noexcept(noexcept(left.current_ != right.current_)) {
        return left.current_ != right.current_;
    }

    friend constexpr bool operator==(const counted_iterator& value,
                                     default_sentinel_t) noexcept {
        return value.count_ == 0;
    }

    friend constexpr bool operator==(default_sentinel_t sentinel,
                                     const counted_iterator& value) noexcept {
        return value == sentinel;
    }

    friend constexpr bool operator!=(const counted_iterator& value,
                                     default_sentinel_t sentinel) noexcept {
        return !(value == sentinel);
    }

    friend constexpr bool operator!=(default_sentinel_t sentinel,
                                     const counted_iterator& value) noexcept {
        return !(sentinel == value);
    }

    friend constexpr difference_type operator-(
        const counted_iterator& left, const counted_iterator& right) noexcept {
        return right.count_ - left.count_;
    }

    friend constexpr difference_type operator-(
        const counted_iterator& value, default_sentinel_t) noexcept {
        return -value.count_;
    }

    friend constexpr difference_type operator-(
        default_sentinel_t, const counted_iterator& value) noexcept {
        return value.count_;
    }

    friend constexpr bool operator<(const counted_iterator& left,
                                    const counted_iterator& right)
        requires random_access_iterator<I>
    { return left.current_ < right.current_; }

    friend constexpr bool operator>(const counted_iterator& left,
                                    const counted_iterator& right)
        requires random_access_iterator<I>
    { return right < left; }

    friend constexpr bool operator<=(const counted_iterator& left,
                                     const counted_iterator& right)
        requires random_access_iterator<I>
    { return !(right < left); }

    friend constexpr bool operator>=(const counted_iterator& left,
                                     const counted_iterator& right)
        requires random_access_iterator<I>
    { return !(left < right); }

    friend constexpr auto iter_move(const counted_iterator& value)
        noexcept(noexcept(ranges::iter_move(value.current_)))
        -> decltype(ranges::iter_move(value.current_)) {
        return ranges::iter_move(value.current_);
    }
};

template<typename I>
counted_iterator(I, iter_difference_t<I>) -> counted_iterator<I>;

/* P2259-style cross-base interoperability.  The class friends above cover
 * the common same-base case; these constrained non-members let a mutable
 * iterator compare with its const/base-compatible counterpart without
 * manufacturing an iterator of either concrete type.  Difference uses the
 * signed common difference type while preserving counted_iterator's
 * remaining-count convention. */
template<input_or_output_iterator I1, input_or_output_iterator I2>
    requires (!same_as<I1, I2>) && requires(
        const counted_iterator<I1>& left,
        const counted_iterator<I2>& right) {
            { left.base() == right.base() } -> convertible_to<bool>;
        }
constexpr bool operator==(const counted_iterator<I1>& left,
                          const counted_iterator<I2>& right)
    noexcept(noexcept(left.base() == right.base())) {
    return left.base() == right.base();
}

template<input_or_output_iterator I1, input_or_output_iterator I2>
    requires (!same_as<I1, I2>) && requires(
        const counted_iterator<I1>& left,
        const counted_iterator<I2>& right) {
            { left.base() != right.base() } -> convertible_to<bool>;
        }
constexpr bool operator!=(const counted_iterator<I1>& left,
                          const counted_iterator<I2>& right)
    noexcept(noexcept(left.base() != right.base())) {
    return left.base() != right.base();
}

template<input_or_output_iterator I1, input_or_output_iterator I2>
    requires random_access_iterator<I1> && random_access_iterator<I2> &&
             (!same_as<I1, I2>) && requires(
                 const counted_iterator<I1>& left,
                 const counted_iterator<I2>& right) {
                     { left.base() < right.base() } -> convertible_to<bool>;
                 }
constexpr bool operator<(const counted_iterator<I1>& left,
                         const counted_iterator<I2>& right)
    noexcept(noexcept(left.base() < right.base())) {
    return left.base() < right.base();
}

template<input_or_output_iterator I1, input_or_output_iterator I2>
    requires random_access_iterator<I1> && random_access_iterator<I2> &&
             (!same_as<I1, I2>)
constexpr bool operator>(const counted_iterator<I1>& left,
                         const counted_iterator<I2>& right)
    noexcept(noexcept(right < left)) {
    return right < left;
}

template<input_or_output_iterator I1, input_or_output_iterator I2>
    requires random_access_iterator<I1> && random_access_iterator<I2> &&
             (!same_as<I1, I2>)
constexpr bool operator<=(const counted_iterator<I1>& left,
                          const counted_iterator<I2>& right)
    noexcept(noexcept(right < left)) {
    return !(right < left);
}

template<input_or_output_iterator I1, input_or_output_iterator I2>
    requires random_access_iterator<I1> && random_access_iterator<I2> &&
             (!same_as<I1, I2>)
constexpr bool operator>=(const counted_iterator<I1>& left,
                          const counted_iterator<I2>& right)
    noexcept(noexcept(left < right)) {
    return !(left < right);
}

#if __cplusplus >= 202002L
template<input_or_output_iterator I1, input_or_output_iterator I2>
    requires random_access_iterator<I1> && random_access_iterator<I2> &&
             requires(const counted_iterator<I1>& left,
                      const counted_iterator<I2>& right) {
                 left.base() <=> right.base();
             }
constexpr auto operator<=>(const counted_iterator<I1>& left,
                           const counted_iterator<I2>& right)
    -> decltype(left.base() <=> right.base()) {
    return left.base() <=> right.base();
}
#endif

template<input_or_output_iterator I1, input_or_output_iterator I2>
    requires random_access_iterator<I1> && random_access_iterator<I2> &&
             (!same_as<I1, I2>) && requires(
                 const counted_iterator<I1>& left,
                 const counted_iterator<I2>& right) {
                     left.count();
                     right.count();
                 }
constexpr common_type_t<iter_difference_t<I1>, iter_difference_t<I2>>
operator-(const counted_iterator<I1>& left,
          const counted_iterator<I2>& right) noexcept {
    using common_difference =
        common_type_t<iter_difference_t<I1>, iter_difference_t<I2>>;
    return static_cast<common_difference>(right.count()) -
           static_cast<common_difference>(left.count());
}

template<typename I>
constexpr counted_iterator<I> make_counted_iterator(
    I current, iter_difference_t<I> count) {
    return counted_iterator<I>(static_cast<I&&>(current), count);
}

template<input_or_output_iterator I1, input_or_output_iterator I2>
    requires requires { typename common_type_t<I1, I2>; } &&
             input_or_output_iterator<common_type_t<I1, I2>>
struct common_type<counted_iterator<I1>, counted_iterator<I2>> {
    using type = counted_iterator<common_type_t<I1, I2>>;
};
#endif

#if __cplusplus >= 202002L
template<input_or_output_iterator I, typename S>
    requires sentinel_for<S, I>
class common_iterator : public detail::counted_iterator_traits<I> {
    struct iterator_state_tag {};
    struct sentinel_state_tag {};

    union storage_t {
        I current;
        S sentinel;

        constexpr storage_t() {}
        constexpr storage_t(iterator_state_tag, I value)
            : current(static_cast<I&&>(value)) {}
        constexpr storage_t(sentinel_state_tag, S value)
            : sentinel(static_cast<S&&>(value)) {}
        constexpr ~storage_t() {}
    } storage_;
    bool is_iterator_;

    constexpr void destroy_active() noexcept {
        if (is_iterator_)
            storage_.current.~I();
        else
            storage_.sentinel.~S();
    }

    void copy_construct(const common_iterator& other) {
        if (is_iterator_)
            ::new (static_cast<void*>(&storage_.current)) I(other.storage_.current);
        else
            ::new (static_cast<void*>(&storage_.sentinel)) S(other.storage_.sentinel);
    }

    void move_construct(common_iterator&& other) {
        if (is_iterator_)
            ::new (static_cast<void*>(&storage_.current))
                I(static_cast<I&&>(other.storage_.current));
        else
            ::new (static_cast<void*>(&storage_.sentinel))
                S(static_cast<S&&>(other.storage_.sentinel));
    }

public:
    using iterator_type = I;
    using sentinel_type = S;
    using difference_type = iter_difference_t<I>;
    using iterator_concept = conditional_t<
        random_access_iterator<I>, random_access_iterator_tag,
        conditional_t<forward_iterator<I>, forward_iterator_tag,
                      input_iterator_tag>>;
    using reference = iter_reference_t<I>;

    constexpr common_iterator()
        requires default_initializable<I>
        : storage_(iterator_state_tag{}, I()), is_iterator_(true) {}

    constexpr common_iterator(I current)
        : storage_(iterator_state_tag{}, static_cast<I&&>(current)),
          is_iterator_(true) {}

    template<typename T = S>
        requires (!same_as<I, T>)
    constexpr common_iterator(S sentinel)
        : storage_(sentinel_state_tag{}, static_cast<S&&>(sentinel)),
          is_iterator_(false) {}

    common_iterator(const common_iterator& other)
        : storage_(), is_iterator_(other.is_iterator_) {
        copy_construct(other);
    }

    common_iterator(common_iterator&& other)
        noexcept(is_nothrow_move_constructible<I>::value &&
                 is_nothrow_move_constructible<S>::value)
        : storage_(), is_iterator_(other.is_iterator_) {
        move_construct(static_cast<common_iterator&&>(other));
    }

    ~common_iterator() { destroy_active(); }

    common_iterator& operator=(const common_iterator& other)
        requires is_constructible<I, const I&>::value &&
                 is_constructible<S, const S&>::value {
        if (this == &other)
            return *this;
        destroy_active();
        is_iterator_ = other.is_iterator_;
        copy_construct(other);
        return *this;
    }

    common_iterator& operator=(common_iterator&& other)
        noexcept(is_nothrow_move_constructible<I>::value &&
                 is_nothrow_move_constructible<S>::value)
        requires is_constructible<I, I&&>::value &&
                 is_constructible<S, S&&>::value {
        if (this == &other)
            return *this;
        destroy_active();
        is_iterator_ = other.is_iterator_;
        move_construct(static_cast<common_iterator&&>(other));
        return *this;
    }

    constexpr bool holds_iterator() const noexcept { return is_iterator_; }

    constexpr reference operator*() const { return *storage_.current; }

    constexpr auto operator->() const
        requires requires(const I& value) { value.operator->(); }
    { return storage_.current.operator->(); }

    constexpr auto operator->() const requires is_pointer<I>::value {
        return storage_.current;
    }

    constexpr common_iterator& operator++() {
        ++storage_.current;
        return *this;
    }

    constexpr common_iterator operator++(int) requires forward_iterator<I> {
        common_iterator before(*this);
        ++(*this);
        return before;
    }

    constexpr void operator++(int) requires (!forward_iterator<I>) {
        ++(*this);
    }

    constexpr common_iterator& operator--()
        requires bidirectional_iterator<I>
    { --storage_.current; return *this; }

    constexpr common_iterator operator--(int)
        requires bidirectional_iterator<I>
    { common_iterator before(*this); --(*this); return before; }

    constexpr common_iterator& operator+=(difference_type offset)
        requires random_access_iterator<I>
    { storage_.current += offset; return *this; }

    constexpr common_iterator& operator-=(difference_type offset)
        requires random_access_iterator<I>
    { storage_.current -= offset; return *this; }

    constexpr common_iterator operator+(difference_type offset) const
        requires random_access_iterator<I>
    { common_iterator result(*this); result += offset; return result; }

    friend constexpr common_iterator operator+(
        difference_type offset, const common_iterator& value)
        requires random_access_iterator<I>
    { return value + offset; }

    constexpr common_iterator operator-(difference_type offset) const
        requires random_access_iterator<I>
    { common_iterator result(*this); result -= offset; return result; }

    constexpr reference operator[](difference_type offset) const
        requires random_access_iterator<I>
    { return storage_.current[offset]; }

    friend constexpr bool operator==(const common_iterator& left,
                                     const common_iterator& right) {
        if (left.is_iterator_ != right.is_iterator_)
            return left.is_iterator_ ? left.storage_.current == right.storage_.sentinel
                                     : right.storage_.current == left.storage_.sentinel;
        return left.is_iterator_ ? left.storage_.current == right.storage_.current : true;
    }

    friend constexpr bool operator!=(const common_iterator& left,
                                     const common_iterator& right) {
        return !(left == right);
    }

    friend constexpr difference_type operator-(
        const common_iterator& left, const common_iterator& right)
        requires sized_sentinel_for<S, I>
    {
        if (left.is_iterator_ && right.is_iterator_)
            return left.storage_.current - right.storage_.current;
        if (left.is_iterator_)
            return left.storage_.current - right.storage_.sentinel;
        if (right.is_iterator_)
            return left.storage_.sentinel - right.storage_.current;
        return 0;
    }

    template<typename X = I>
        requires requires(X& left, X& right) {
            ranges::iter_swap(left, right);
        }
    friend constexpr void iter_swap(common_iterator& left,
                                    common_iterator& right)
        noexcept(noexcept(ranges::iter_swap(left.storage_.current,
                                            right.storage_.current))) {
        if (!left.is_iterator_ || !right.is_iterator_) {
            __builtin_trap();
        }
        ranges::iter_swap(left.storage_.current, right.storage_.current);
    }

    friend constexpr auto iter_move(const common_iterator& value)
        noexcept(noexcept(ranges::iter_move(value.storage_.current)))
        -> decltype(ranges::iter_move(value.storage_.current)) {
        return ranges::iter_move(value.storage_.current);
    }
};

#endif

#endif

/* distance - ランダムアクセスイテレータ用 */
template<typename InputIterator>
inline typename iterator_traits<InputIterator>::difference_type
_distance_impl(InputIterator first, InputIterator last, random_access_iterator_tag) {
    return last - first;
}

/* distance - その他のイテレータ用 */
template<typename InputIterator>
inline typename iterator_traits<InputIterator>::difference_type
_distance_impl(InputIterator first, InputIterator last, input_iterator_tag) {
    typename iterator_traits<InputIterator>::difference_type n = 0;
    while (first != last) {
        ++first;
        ++n;
    }
    return n;
}

/* distance 本体 */
template<typename InputIterator>
inline typename iterator_traits<InputIterator>::difference_type
distance(InputIterator first, InputIterator last) {
    return _distance_impl(first, last, 
        typename iterator_traits<InputIterator>::iterator_category());
}

/* ポインタ用distance特殊化 */
template<typename T>
inline ptrdiff_t distance(T* first, T* last) {
    return last - first;
}

template<typename T>
inline ptrdiff_t distance(const T* first, const T* last) {
    return last - first;
}

/* advance - ランダムアクセスイテレータ用 */
template<typename InputIterator, typename Distance>
inline void _advance_impl(InputIterator& it, Distance n, random_access_iterator_tag) {
    it += n;
}

/* advance - 双方向イテレータ用 */
template<typename InputIterator, typename Distance>
inline void _advance_impl(InputIterator& it, Distance n, bidirectional_iterator_tag) {
    if (n >= 0) {
        while (n--) ++it;
    } else {
        while (n++) --it;
    }
}

/* advance - 入力イテレータ用 */
template<typename InputIterator, typename Distance>
inline void _advance_impl(InputIterator& it, Distance n, input_iterator_tag) {
    while (n-- > 0) ++it;
}

/* advance - 前方イテレータ用 (input_iterator_tagから派生) */
template<typename InputIterator, typename Distance>
inline void _advance_impl(InputIterator& it, Distance n, forward_iterator_tag) {
    while (n-- > 0) ++it;
}

/* advance 本体 */
template<typename InputIterator, typename Distance>
inline void advance(InputIterator& it, Distance n) {
    _advance_impl(it, n, typename iterator_traits<InputIterator>::iterator_category());
}

/* ポインタ用advance特殊化 */
template<typename T>
inline void advance(T*& it, long n) {
    it += n;
}

/* next/prev */
template<typename InputIterator>
inline InputIterator next(InputIterator it, 
    typename iterator_traits<InputIterator>::difference_type n = 1) {
    advance(it, n);
    return it;
}

template<typename BidirectionalIterator>
inline BidirectionalIterator prev(BidirectionalIterator it,
    typename iterator_traits<BidirectionalIterator>::difference_type n = 1) {
    advance(it, -n);
    return it;
}

/* begin/end for C配列 */
template<typename T, unsigned long N>
inline T* begin(T (&arr)[N]) { return arr; }

template<typename T, unsigned long N>
inline T* end(T (&arr)[N]) { return arr + N; }

template<typename T, unsigned long N>
inline const T* cbegin(const T (&arr)[N]) { return arr; }

template<typename T, unsigned long N>
inline const T* cend(const T (&arr)[N]) { return arr + N; }

/* begin/end for containers with .begin()/.end() methods */
template<typename Container>
inline auto begin(Container& c) -> decltype(c.begin()) { return c.begin(); }

template<typename Container>
inline auto begin(const Container& c) -> decltype(c.begin()) { return c.begin(); }

template<typename Container>
inline auto end(Container& c) -> decltype(c.end()) { return c.end(); }

template<typename Container>
inline auto end(const Container& c) -> decltype(c.end()) { return c.end(); }

template<typename Container>
inline auto cbegin(const Container& c) -> decltype(c.begin()) { return c.begin(); }

template<typename Container>
inline auto cend(const Container& c) -> decltype(c.end()) { return c.end(); }

/* forward declare reverse_iterator for rbegin/rend */
template<typename Iterator>
class reverse_iterator;

/* rbegin/rend for C配列 */
template<typename T, unsigned long N>
inline reverse_iterator<T*> rbegin(T (&arr)[N]) {
    return reverse_iterator<T*>(arr + N);
}

template<typename T, unsigned long N>
inline reverse_iterator<T*> rend(T (&arr)[N]) {
    return reverse_iterator<T*>(arr);
}

template<typename T, unsigned long N>
inline reverse_iterator<const T*> rbegin(const T (&arr)[N]) {
    return reverse_iterator<const T*>(arr + N);
}

template<typename T, unsigned long N>
inline reverse_iterator<const T*> rend(const T (&arr)[N]) {
    return reverse_iterator<const T*>(arr);
}

template<typename T, unsigned long N>
inline reverse_iterator<const T*> crbegin(const T (&arr)[N]) {
    return reverse_iterator<const T*>(arr + N);
}

template<typename T, unsigned long N>
inline reverse_iterator<const T*> crend(const T (&arr)[N]) {
    return reverse_iterator<const T*>(arr);
}

/* rbegin/rend for containers */
template<typename Container>
inline auto rbegin(Container& c) -> decltype(c.rbegin()) {
    return c.rbegin();
}

template<typename Container>
inline auto rbegin(const Container& c) -> decltype(c.rbegin()) {
    return c.rbegin();
}

template<typename Container>
inline auto rend(Container& c) -> decltype(c.rend()) {
    return c.rend();
}

template<typename Container>
inline auto rend(const Container& c) -> decltype(c.rend()) {
    return c.rend();
}

template<typename Container>
inline auto crbegin(const Container& c) -> decltype(c.rbegin()) {
    return c.rbegin();
}

template<typename Container>
inline auto crend(const Container& c) -> decltype(c.rend()) {
    return c.rend();
}

/* reverse_iterator */
template<typename Iterator>
class reverse_iterator {
public:
    typedef Iterator iterator_type;
    typedef typename iterator_traits<Iterator>::difference_type difference_type;
    typedef typename iterator_traits<Iterator>::value_type value_type;
    typedef typename iterator_traits<Iterator>::pointer pointer;
    typedef typename iterator_traits<Iterator>::reference reference;
    typedef typename iterator_traits<Iterator>::iterator_category iterator_category;
#if __cplusplus >= 202002L
    using iterator_concept = conditional_t<
        random_access_iterator<Iterator>, random_access_iterator_tag,
        conditional_t<bidirectional_iterator<Iterator>, bidirectional_iterator_tag,
                      conditional_t<forward_iterator<Iterator>, forward_iterator_tag,
                                    input_iterator_tag>>>;
#endif

protected:
    Iterator current;

public:
    reverse_iterator() : current() {}
    explicit reverse_iterator(Iterator x) : current(x) {}
    
    template<typename U,
             typename enable_if<is_convertible<const U&, Iterator>::value,
                                 int>::type = 0>
    reverse_iterator(const reverse_iterator<U>& u) : current(u.base()) {}

    template<typename U,
             typename enable_if<is_convertible<const U&, Iterator>::value &&
                                 is_assignable<Iterator&, const U&>::value,
                                 int>::type = 0>
    typename enable_if<is_convertible<const U&, Iterator>::value &&
                       is_assignable<Iterator&, const U&>::value,
                       reverse_iterator&>::type
    operator=(const reverse_iterator<U>& u) {
        current = u.base();
        return *this;
    }

    /* base() returns reference to allow --(it.base()) pattern */
    Iterator& base() { return current; }
    Iterator base() const { return current; }
    
    reference operator*() const {
        Iterator tmp = current;
        return *--tmp;
    }
    
    pointer operator->() const {
        return &(operator*());
    }
    
    reverse_iterator& operator++() { --current; return *this; }
    reverse_iterator operator++(int) { reverse_iterator tmp = *this; --current; return tmp; }
    reverse_iterator& operator--() { ++current; return *this; }
    reverse_iterator operator--(int) { reverse_iterator tmp = *this; ++current; return tmp; }
    
    reverse_iterator operator+(difference_type n) const { return reverse_iterator(current - n); }
    reverse_iterator& operator+=(difference_type n) { current -= n; return *this; }
    reverse_iterator operator-(difference_type n) const { return reverse_iterator(current + n); }
    reverse_iterator& operator-=(difference_type n) { current += n; return *this; }
    
    reference operator[](difference_type n) const { return *(*this + n); }
    
    bool operator==(const reverse_iterator& other) const { return current == other.current; }
    bool operator!=(const reverse_iterator& other) const { return current != other.current; }
    bool operator<(const reverse_iterator& other) const { return current > other.current; }
    bool operator>(const reverse_iterator& other) const { return current < other.current; }
    bool operator<=(const reverse_iterator& other) const { return current >= other.current; }
    bool operator>=(const reverse_iterator& other) const { return current <= other.current; }

    /* Difference between two reverse_iterators */
    difference_type operator-(const reverse_iterator& other) const {
        return other.current - current;
    }

#if __cplusplus >= 202002L
    friend constexpr auto iter_move(const reverse_iterator& value)
        noexcept(noexcept(ranges::iter_move(declval<Iterator&>())))
        -> decltype(ranges::iter_move(declval<Iterator&>())) {
        Iterator previous = value.current;
        --previous;
        return ranges::iter_move(previous);
    }

    friend constexpr void iter_swap(reverse_iterator& left,
                                    reverse_iterator& right)
        noexcept(noexcept(ranges::iter_swap(declval<Iterator&>(),
                                            declval<Iterator&>()))) {
        Iterator left_previous = left.current;
        Iterator right_previous = right.current;
        --left_previous;
        --right_previous;
        ranges::iter_swap(left_previous, right_previous);
    }
#endif
};

/* Reverse iterators need the symmetric offset operation to satisfy the
 * random_access_iterator algebra (and therefore to remain valid ranges). */
template<typename Iterator>
inline reverse_iterator<Iterator>
operator+(typename reverse_iterator<Iterator>::difference_type count,
          const reverse_iterator<Iterator>& value) {
    return value + count;
}

/* C++11/17 require the relational operators to work across the converting
 * iterator boundary too (for example iterator<const T*> versus iterator<T*>).
 * The member operators above cover the exact same-type case; these mixed-only
 * overloads avoid ambiguity there while preserving reverse ordering. */
#if __cplusplus < 202002L
template<typename Iterator1, typename Iterator2>
inline typename enable_if<!is_same<Iterator1, Iterator2>::value, bool>::type
operator==(const reverse_iterator<Iterator1>& left,
           const reverse_iterator<Iterator2>& right) {
    return left.base() == right.base();
}

template<typename Iterator1, typename Iterator2>
inline typename enable_if<!is_same<Iterator1, Iterator2>::value, bool>::type
operator!=(const reverse_iterator<Iterator1>& left,
           const reverse_iterator<Iterator2>& right) {
    return !(left == right);
}

template<typename Iterator1, typename Iterator2>
inline typename enable_if<!is_same<Iterator1, Iterator2>::value, bool>::type
operator<(const reverse_iterator<Iterator1>& left,
          const reverse_iterator<Iterator2>& right) {
    return right.base() < left.base();
}

template<typename Iterator1, typename Iterator2>
inline typename enable_if<!is_same<Iterator1, Iterator2>::value, bool>::type
operator>(const reverse_iterator<Iterator1>& left,
          const reverse_iterator<Iterator2>& right) {
    return right < left;
}

template<typename Iterator1, typename Iterator2>
inline typename enable_if<!is_same<Iterator1, Iterator2>::value, bool>::type
operator<=(const reverse_iterator<Iterator1>& left,
           const reverse_iterator<Iterator2>& right) {
    return !(right < left);
}

template<typename Iterator1, typename Iterator2>
inline typename enable_if<!is_same<Iterator1, Iterator2>::value, bool>::type
operator>=(const reverse_iterator<Iterator1>& left,
           const reverse_iterator<Iterator2>& right) {
    return !(left < right);
}
#endif

#if __cplusplus >= 202002L
template<typename Iterator1, typename Iterator2>
constexpr auto operator<=>(const reverse_iterator<Iterator1>& left,
                           const reverse_iterator<Iterator2>& right)
    -> decltype(right.base() <=> left.base()) {
    return right.base() <=> left.base();
}
#endif

/* Non-member operator- for reverse_iterator */
template<typename Iterator1, typename Iterator2>
inline auto operator-(const reverse_iterator<Iterator1>& lhs, const reverse_iterator<Iterator2>& rhs)
    -> decltype(rhs.base() - lhs.base()) {
    return rhs.base() - lhs.base();
}

template<typename Iterator>
inline reverse_iterator<Iterator> make_reverse_iterator(Iterator i) {
    return reverse_iterator<Iterator>(i);
}

namespace detail {
template<typename I1, typename I2, typename = void>
struct reverse_iterator_common_type {};

template<typename I1, typename I2>
struct reverse_iterator_common_type<I1, I2,
    void_t<common_type_t<I1, I2>>> {
    using type = reverse_iterator<common_type_t<I1, I2>>;
};
} /* namespace detail */

template<typename I1, typename I2>
struct common_type<reverse_iterator<I1>, reverse_iterator<I2>>
    : detail::reverse_iterator_common_type<I1, I2> {};

/* size - for C arrays */
template<typename T, size_t N>
constexpr size_t size(const T (&)[N]) noexcept {
    return N;
}

/* size - for containers with .size() */
template<typename Container>
constexpr auto size(const Container& c) -> decltype(c.size()) {
    return c.size();
}

/* ssize - signed size (C++20) */
template<typename T, size_t N>
constexpr ptrdiff_t ssize(const T (&)[N]) noexcept {
    return static_cast<ptrdiff_t>(N);
}

template<typename Container>
constexpr auto ssize(const Container& c) -> decltype(static_cast<ptrdiff_t>(c.size())) {
    return static_cast<ptrdiff_t>(c.size());
}

/* empty - for C arrays */
template<typename T, size_t N>
constexpr bool empty(const T (&)[N]) noexcept {
    return N == 0;
}

/* empty - for containers */
template<typename Container>
constexpr auto empty(const Container& c) -> decltype(c.empty()) {
    return c.empty();
}

/* data - for C arrays */
template<typename T, size_t N>
constexpr T* data(T (&arr)[N]) noexcept {
    return arr;
}

/* data - for containers */
template<typename Container>
constexpr auto data(Container& c) -> decltype(c.data()) {
    return c.data();
}

template<typename Container>
constexpr auto data(const Container& c) -> decltype(c.data()) {
    return c.data();
}

/* ═══════════════════════════════════════════════════════════════
 * back_insert_iterator / back_inserter
 * ═══════════════════════════════════════════════════════════════*/

template<typename Container>
class back_insert_iterator {
protected:
    Container* container;

public:
    using iterator_category = output_iterator_tag;
    using value_type = void;
    using difference_type = void;
    using pointer = void;
    using reference = void;
    using container_type = Container;

    explicit back_insert_iterator(Container& c) : container(&c) {}

    back_insert_iterator& operator=(const typename Container::value_type& value) {
        container->push_back(value);
        return *this;
    }

    back_insert_iterator& operator=(typename Container::value_type&& value) {
        container->push_back(std::move(value));
        return *this;
    }

    back_insert_iterator& operator*() { return *this; }
    back_insert_iterator& operator++() { return *this; }
    back_insert_iterator operator++(int) { return *this; }
};

template<typename Container>
inline back_insert_iterator<Container> back_inserter(Container& c) {
    return back_insert_iterator<Container>(c);
}

/* ═══════════════════════════════════════════════════════════════
 * front_insert_iterator / front_inserter
 * ═══════════════════════════════════════════════════════════════*/

template<typename Container>
class front_insert_iterator {
protected:
    Container* container;

public:
    using iterator_category = output_iterator_tag;
    using value_type = void;
    using difference_type = void;
    using pointer = void;
    using reference = void;
    using container_type = Container;

    explicit front_insert_iterator(Container& c) : container(&c) {}

    front_insert_iterator& operator=(const typename Container::value_type& value) {
        container->push_front(value);
        return *this;
    }

    front_insert_iterator& operator=(typename Container::value_type&& value) {
        container->push_front(move(value));
        return *this;
    }

    front_insert_iterator& operator*() { return *this; }
    front_insert_iterator& operator++() { return *this; }
    front_insert_iterator operator++(int) { return *this; }
};

template<typename Container>
inline front_insert_iterator<Container> front_inserter(Container& c) {
    return front_insert_iterator<Container>(c);
}

/* ═══════════════════════════════════════════════════════════════
 * insert_iterator / inserter
 * ═══════════════════════════════════════════════════════════════*/

template<typename Container>
class insert_iterator {
protected:
    Container* container;
    typename Container::iterator iter;

public:
    using iterator_category = output_iterator_tag;
    using value_type = void;
    using difference_type = void;
    using pointer = void;
    using reference = void;
    using container_type = Container;

    insert_iterator(Container& c, typename Container::iterator i)
        : container(&c), iter(i) {}

    insert_iterator& operator=(const typename Container::value_type& value) {
        iter = container->insert(iter, value);
        ++iter;
        return *this;
    }

    insert_iterator& operator=(typename Container::value_type&& value) {
        iter = container->insert(iter, move(value));
        ++iter;
        return *this;
    }

    insert_iterator& operator*() { return *this; }
    insert_iterator& operator++() { return *this; }
    insert_iterator operator++(int) { return *this; }
};

template<typename Container>
inline insert_iterator<Container> inserter(Container& c, typename Container::iterator i) {
    return insert_iterator<Container>(c, i);
}

/* ═══════════════════════════════════════════════════════════════
 * move_iterator
 * ═══════════════════════════════════════════════════════════════*/

template<typename Iterator>
class move_iterator {
public:
    using iterator_type = Iterator;
    using iterator_category = typename iterator_traits<Iterator>::iterator_category;
    using value_type = typename iterator_traits<Iterator>::value_type;
    using difference_type = typename iterator_traits<Iterator>::difference_type;
    using pointer = Iterator;
#if __cplusplus >= 202002L
    using iterator_concept = conditional_t<
        random_access_iterator<Iterator>, random_access_iterator_tag,
        conditional_t<bidirectional_iterator<Iterator>, bidirectional_iterator_tag,
                      conditional_t<forward_iterator<Iterator>, forward_iterator_tag,
                                    input_iterator_tag>>>;
    using reference = iter_rvalue_reference_t<Iterator>;
#else
    using reference = value_type&&;
#endif

private:
    Iterator current;

public:
    constexpr move_iterator() : current() {}
    constexpr explicit move_iterator(Iterator it) : current(it) {}

    template<typename U>
    constexpr move_iterator(const move_iterator<U>& other) : current(other.base()) {}

    constexpr Iterator base() const { return current; }

    constexpr reference operator*() const {
#if __cplusplus >= 202002L
        return ranges::iter_move(current);
#else
        return static_cast<reference>(*current);
#endif
    }

    constexpr pointer operator->() const { return current; }

#if __cplusplus >= 201402L
    constexpr
#else
    inline
#endif
    move_iterator& operator++() { ++current; return *this; }
#if __cplusplus >= 201402L
    constexpr
#else
    inline
#endif
    move_iterator operator++(int) { move_iterator tmp = *this; ++current; return tmp; }
#if __cplusplus >= 201402L
    constexpr
#else
    inline
#endif
    move_iterator& operator--() { --current; return *this; }
#if __cplusplus >= 201402L
    constexpr
#else
    inline
#endif
    move_iterator operator--(int) { move_iterator tmp = *this; --current; return tmp; }

    constexpr move_iterator operator+(difference_type n) const {
        return move_iterator(current + n);
    }
#if __cplusplus >= 201402L
    constexpr
#else
    inline
#endif
    move_iterator& operator+=(difference_type n) {
        current += n; return *this;
    }
    constexpr move_iterator operator-(difference_type n) const {
        return move_iterator(current - n);
    }
#if __cplusplus >= 201402L
    constexpr
#else
    inline
#endif
    move_iterator& operator-=(difference_type n) {
        current -= n; return *this;
    }

    constexpr reference operator[](difference_type n) const {
        return static_cast<reference>(current[n]);
    }

#if __cplusplus >= 202002L
    friend constexpr auto iter_move(const move_iterator& value)
        noexcept(noexcept(ranges::iter_move(value.current)))
        -> decltype(ranges::iter_move(value.current)) {
        return ranges::iter_move(value.current);
    }

    friend constexpr void iter_swap(move_iterator& left,
                                    move_iterator& right)
        noexcept(noexcept(ranges::iter_swap(left.current, right.current))) {
        ranges::iter_swap(left.current, right.current);
    }
#endif
};

template<typename Iterator1, typename Iterator2>
constexpr bool operator==(const move_iterator<Iterator1>& a, const move_iterator<Iterator2>& b) {
    return a.base() == b.base();
}

template<typename Iterator1, typename Iterator2>
constexpr bool operator!=(const move_iterator<Iterator1>& a, const move_iterator<Iterator2>& b) {
    return a.base() != b.base();
}

template<typename Iterator1, typename Iterator2>
constexpr bool operator<(const move_iterator<Iterator1>& a, const move_iterator<Iterator2>& b) {
    return a.base() < b.base();
}

template<typename Iterator1, typename Iterator2>
constexpr bool operator<=(const move_iterator<Iterator1>& a, const move_iterator<Iterator2>& b) {
    return a.base() <= b.base();
}

template<typename Iterator1, typename Iterator2>
constexpr bool operator>(const move_iterator<Iterator1>& a, const move_iterator<Iterator2>& b) {
    return a.base() > b.base();
}

template<typename Iterator1, typename Iterator2>
constexpr bool operator>=(const move_iterator<Iterator1>& a, const move_iterator<Iterator2>& b) {
    return a.base() >= b.base();
}

template<typename Iterator>
constexpr move_iterator<Iterator> operator+(
    typename move_iterator<Iterator>::difference_type offset,
    const move_iterator<Iterator>& value) {
    return value + offset;
}

template<typename Iterator1, typename Iterator2>
constexpr auto operator-(const move_iterator<Iterator1>& left,
                         const move_iterator<Iterator2>& right)
    -> decltype(left.base() - right.base()) {
    return left.base() - right.base();
}

#if __cplusplus >= 202002L
template<typename Iterator1, typename Iterator2>
constexpr auto operator<=>(const move_iterator<Iterator1>& left,
                           const move_iterator<Iterator2>& right)
    -> decltype(left.base() <=> right.base()) {
    return left.base() <=> right.base();
}
#endif

template<typename Iterator>
constexpr move_iterator<Iterator> make_move_iterator(Iterator i) {
    return move_iterator<Iterator>(i);
}


#if __cplusplus >= 202002L
/* move_sentinel adapts a sentinel for move_iterator without copying the
 * underlying range or manufacturing an iterator from the sentinel. */
template<typename S>
class move_sentinel {
    S end_;

public:
    using sentinel_type = S;

    constexpr move_sentinel() requires default_initializable<S> : end_() {}

    constexpr explicit move_sentinel(S end)
        : end_(static_cast<S&&>(end)) {}

    template<typename U>
        requires convertible_to<const U&, S>
    constexpr move_sentinel(const move_sentinel<U>& other)
        : end_(other.base()) {}

    constexpr S base() const { return end_; }
};

template<typename S>
move_sentinel(S) -> move_sentinel<S>;

template<typename I, typename S>
constexpr bool operator==(const move_iterator<I>& iterator,
                          const move_sentinel<S>& sentinel) {
    return iterator.base() == sentinel.base();
}

template<typename I, typename S>
constexpr bool operator==(const move_sentinel<S>& sentinel,
                          const move_iterator<I>& iterator) {
    return iterator == sentinel;
}

template<typename I, typename S>
constexpr bool operator!=(const move_iterator<I>& iterator,
                          const move_sentinel<S>& sentinel) {
    return !(iterator == sentinel);
}

template<typename I, typename S>
constexpr bool operator!=(const move_sentinel<S>& sentinel,
                          const move_iterator<I>& iterator) {
    return !(sentinel == iterator);
}

template<typename I, typename S>
    requires sized_sentinel_for<S, I>
constexpr auto operator-(const move_sentinel<S>& sentinel,
                         const move_iterator<I>& iterator)
    -> decltype(sentinel.base() - iterator.base()) {
    return sentinel.base() - iterator.base();
}

template<typename I, typename S>
    requires sized_sentinel_for<S, I>
constexpr auto operator-(const move_iterator<I>& iterator,
                         const move_sentinel<S>& sentinel)
    -> decltype(iterator.base() - sentinel.base()) {
    return iterator.base() - sentinel.base();
}
#endif

/* Forward declarations - definitions are provided by istream.h/ostream.h. */
template<typename CharT, typename Traits>
class basic_istream;

template<typename CharT, typename Traits>
class basic_ostream;

template<typename CharT>
struct char_traits;

/* ═══════════════════════════════════════════════════════════════
 * istream_iterator - formatted single-pass extraction
 * ═══════════════════════════════════════════════════════════════*/

template<typename T, typename CharT = char,
         typename Traits = char_traits<CharT>, typename Distance = ptrdiff_t>
class istream_iterator {
public:
    using iterator_category = input_iterator_tag;
    using value_type = T;
    using difference_type = Distance;
    using pointer = const T*;
    using reference = const T&;
    using char_type = CharT;
    using traits_type = Traits;
    using istream_type = basic_istream<CharT, Traits>;

private:
    istream_type* stream_;
    T value_;

    void read_value() {
        if (!stream_) return;
        *stream_ >> value_;
        if (stream_->fail()) stream_ = nullptr;
    }

public:
    constexpr istream_iterator() : stream_(nullptr), value_() {}

    istream_iterator(istream_type& stream) : stream_(&stream), value_() {
        read_value();
    }

    reference operator*() const { return value_; }
    pointer operator->() const { return &value_; }

    istream_iterator& operator++() {
        read_value();
        return *this;
    }

    istream_iterator operator++(int) {
        istream_iterator before(*this);
        read_value();
        return before;
    }

    friend bool operator==(const istream_iterator& left,
                           const istream_iterator& right) {
        return left.stream_ == right.stream_;
    }

    friend bool operator!=(const istream_iterator& left,
                           const istream_iterator& right) {
        return !(left == right);
    }

    friend bool operator==(const istream_iterator& iterator,
                           default_sentinel_t) {
        return iterator.stream_ == nullptr;
    }

    friend bool operator==(default_sentinel_t sentinel,
                           const istream_iterator& iterator) {
        return iterator == sentinel;
    }

    friend bool operator!=(const istream_iterator& iterator,
                           default_sentinel_t sentinel) {
        return !(iterator == sentinel);
    }

    friend bool operator!=(default_sentinel_t sentinel,
                           const istream_iterator& iterator) {
        return !(sentinel == iterator);
    }
};

/* ═══════════════════════════════════════════════════════════════
 * ostream_iterator - formatted output with an optional delimiter
 * ═══════════════════════════════════════════════════════════════*/

template<typename T, typename CharT = char,
         typename Traits = char_traits<CharT>>
class ostream_iterator {
public:
    using iterator_category = output_iterator_tag;
    using value_type = void;
    using difference_type = ptrdiff_t;
    using pointer = void;
    using reference = void;
    using char_type = CharT;
    using traits_type = Traits;
    using ostream_type = basic_ostream<CharT, Traits>;

private:
    ostream_type* stream_;
    const CharT* delimiter_;

public:
    ostream_iterator(ostream_type& stream) noexcept
        : stream_(&stream), delimiter_(nullptr) {}

    ostream_iterator(ostream_type& stream, const CharT* delimiter) noexcept
        : stream_(&stream), delimiter_(delimiter) {}

    ostream_iterator& operator=(const T& value) {
        *stream_ << value;
        if (delimiter_) *stream_ << delimiter_;
        return *this;
    }

    ostream_iterator& operator*() noexcept { return *this; }
    ostream_iterator& operator++() noexcept { return *this; }
    ostream_iterator& operator++(int) noexcept { return *this; }
};

/* ═══════════════════════════════════════════════════════════════
 * istreambuf_iterator - reads characters from a streambuf
 * ═══════════════════════════════════════════════════════════════*/

/* Forward declaration - actual streambuf is in streambuf.h */
template<typename CharT, typename Traits>
class basic_streambuf;

template<typename CharT, typename Traits = char_traits<CharT>>
class istreambuf_iterator {
public:
    using iterator_category = input_iterator_tag;
    using value_type = CharT;
    using difference_type = typename Traits::off_type;
    using pointer = const CharT*;
    using reference = CharT;
    using char_type = CharT;
    using traits_type = Traits;
    using int_type = typename Traits::int_type;
    using streambuf_type = basic_streambuf<CharT, Traits>;
    using istream_type = basic_istream<CharT, Traits>;

private:
    streambuf_type* sbuf_;
    int_type current_;

    void read_char();

public:
    /* Default constructor - creates end-of-stream iterator */
    constexpr istreambuf_iterator() noexcept : sbuf_(nullptr), current_(Traits::eof()) {}

    /* Constructor from streambuf pointer */
    istreambuf_iterator(streambuf_type* sb) noexcept : sbuf_(sb), current_(Traits::eof()) {
        read_char();
    }

    /* Constructor from the matching input stream. */
    istreambuf_iterator(istream_type& is) noexcept
        : sbuf_(is.rdbuf()), current_(Traits::eof()) {
        read_char();
    }

    /* Proxy class for post-increment */
    class proxy {
        CharT value_;
        streambuf_type* sbuf_;
    public:
        proxy(CharT c, streambuf_type* sb) : value_(c), sbuf_(sb) {}
        CharT operator*() const { return value_; }
    };

    /* Dereference */
    CharT operator*() const {
        return Traits::to_char_type(current_);
    }

    /* Pre-increment */
    istreambuf_iterator& operator++();

    /* Post-increment */
    proxy operator++(int) {
        proxy p(Traits::to_char_type(current_), sbuf_);
        ++(*this);
        return p;
    }

    /* Equality comparison */
    bool equal(const istreambuf_iterator& other) const {
        return (sbuf_ == nullptr) == (other.sbuf_ == nullptr);
    }

    friend bool operator==(const istreambuf_iterator& a, const istreambuf_iterator& b) {
        return a.equal(b);
    }

    friend bool operator!=(const istreambuf_iterator& a, const istreambuf_iterator& b) {
        return !a.equal(b);
    }

    friend bool operator==(const istreambuf_iterator& iterator,
                           default_sentinel_t) {
        return iterator.sbuf_ == nullptr;
    }

    friend bool operator==(default_sentinel_t sentinel,
                           const istreambuf_iterator& iterator) {
        return iterator == sentinel;
    }

    friend bool operator!=(const istreambuf_iterator& iterator,
                           default_sentinel_t sentinel) {
        return !(iterator == sentinel);
    }

    friend bool operator!=(default_sentinel_t sentinel,
                           const istreambuf_iterator& iterator) {
        return !(sentinel == iterator);
    }
};

/* ═══════════════════════════════════════════════════════════════
 * ostreambuf_iterator - writes characters to a streambuf
 * ═══════════════════════════════════════════════════════════════*/

template<typename CharT, typename Traits = char_traits<CharT>>
class ostreambuf_iterator {
public:
    using iterator_category = output_iterator_tag;
    using value_type = void;
    using difference_type = ptrdiff_t;
    using pointer = void;
    using reference = void;
    using char_type = CharT;
    using traits_type = Traits;
    using streambuf_type = basic_streambuf<CharT, Traits>;
    using ostream_type = basic_ostream<CharT, Traits>;

private:
    streambuf_type* sbuf_;
    bool failed_;

public:
    ostreambuf_iterator(streambuf_type* sb) noexcept
        : sbuf_(sb), failed_(sb == nullptr) {}

    ostreambuf_iterator(ostream_type& stream) noexcept
        : ostreambuf_iterator(stream.rdbuf()) {}

    ostreambuf_iterator& operator=(CharT c);

    ostreambuf_iterator& operator*() { return *this; }
    ostreambuf_iterator& operator++() { return *this; }
    ostreambuf_iterator& operator++(int) { return *this; }

    bool failed() const noexcept { return failed_; }
};

} /* namespace std */

#endif /* __cplusplus */
#endif /* RINCXX_ITERATOR_H */
